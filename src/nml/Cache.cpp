/* NML a simple yet powerful markup language
   Copyright © 2023 ef3d0c3e

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "Cache.hpp"
#include <fstream>


const std::string& Cached::get() const noexcept
{
	++m_uses;

	return m_content;
}

void Cache::updateCache()
{
	std::unique_lock _{m_cacheMutex};

	// Sync
	for (auto& [hash, cached] : m_cache)
	{
		if (cached.m_sync)
			continue;
		std::ofstream out(m_dir / hash, std::ios::binary);
		if (!out.good()) [[unlikely]]
			throw Error(fmt::format("Unable to open file for writing: {}", (m_dir / hash).string()));

		out.write(cached.m_content.data(), cached.m_content.size());
		cached.m_sync = true;
	}


	if (m_cacheSize < MAX_CACHE_SIZE)
		return;

	// Clear used elements
	for (auto it = m_cache.cbegin(); it != m_cache.cend();)
	{
		if (it->second.m_uses == 0)
		{
			++it;
			continue;
		}
		
		// Remove
		it = m_cache.erase(it);
	}
}

Cache::Cache(const std::string& dir, std::ostream& stream):
	m_dir{dir}, m_stream{stream}
{
	if (m_dir.empty()) // No cache
		return;

	// Build cache once
	for (const auto& ent : std::filesystem::directory_iterator{m_dir})
	{
		const auto& path = ent.path();
		const auto file_sz = std::filesystem::file_size(path);
		if (m_cacheSize + file_sz > MAX_CACHE_SIZE) // Stop
			break;
		m_cacheSize += file_sz;

		// Open
		std::ifstream in(path, std::ios::binary);
		if (!in.good()) [[unlikely]]
			throw Error(fmt::format("Unable to open file in cache: {}", path.native()));

		// Add
		std::string s{std::istreambuf_iterator<char>(in),
			std::istreambuf_iterator<char>()};
		
		m_cache.insert(std::make_pair(path.filename(), Cached{
			std::move(s), true, 0
		}));
	}

	// Start thread
	m_runThread = true;
	m_cacheThread = std::thread([this]
	{
		bool run = m_runThread;
		while (run)
		{
			updateCache();

			std::unique_lock lck{this->m_runThreadMutex};
			m_runThreadCv.wait_for(lck, std::chrono::milliseconds{500}, [this, &run]{ run = m_runThread; return m_runThread; });
		}

		// Update one last time to make elements are synched
		// with disk
		this->updateCache();
	});
}

Cache::~Cache()
{
	if (m_dir.empty()) // No cache
		return;

	{
		std::unique_lock _{m_runThreadMutex};
		m_runThread = false;
	}
	m_cacheThread.join();
}
