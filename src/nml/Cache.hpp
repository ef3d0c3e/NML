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

#ifndef NML_CACHE_HPP
#define NML_CACHE_HPP

#include "Syntax.hpp"
#include <filesystem>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <ostream>

/**
 * Base idea: enable caching of some objects by default (configurable)
 * 
 * Once an objet is compiled, call a method e.g cache(const Syntax::[*]&)
 * If cache is off for type -> do nothing
 * If cache is on for type -> hash element, store compiled data on disk
 * 
 * Cache format: compiled data...
 *
 * At start: load every cached objects in memory (threaded reading) also reading cache can be performed while compiling
 * And store them into some sort of "atlas" (for faster reading when needed)
 * Also set a limit to memory cache size & auto discard used elements so new ones can be loaded
 *
 * Atlas in the form: map<Hash, Cached>
 *
 * struct Cached { std::string content; std::size_t uses; }; ///< Auto discard in a thread using 'GC'
 */

/**
 * @brief Struct for cached content that is loaded in memory
 */
class Cached
{
	friend class Cache;

	std::string m_content; ///< Content
	bool m_sync;
	mutable std::size_t m_uses; ///< Number of uses (cache access)

	/**
	 * @Constructor
	 *
	 * @param content Cached content
	 * @param sync Whether the cached element is synched with it's disk version
	 * @param uses Number of uses
	 */
	Cached(std::string&& content, bool sync, std::size_t uses):
		m_content{std::move(content)}, m_sync{sync}, m_uses{uses} {}

	// TODO
	//const void append()

	/**
	 * @brief Gets content and increment uses
	 *
	 * @return Content
	 */
	const std::string& get() const noexcept;
public:
	friend std::ostream& operator<<(const Cached& c, std::ostream& s)
	{
		s << c.get();
		return s;
	}
};

/**
 * @brief Class for automatically using cached version of compiled elements
 */
class Cache
{
	std::filesystem::path m_dir; ///< Cache directory
	std::ostream& m_stream; ///< Output stream

	std::mutex m_cacheMutex; ///< Mutex for m_cache
	std::map<std::string, Cached> m_cache; ///< Objects in memory cache
	std::size_t m_cacheSize; ///< Size of cache (in bytes)

	std::mutex m_runThreadMutex;
	std::condition_variable m_runThreadCv;
	bool m_runThread;
	std::thread m_cacheThread; ///< Threaded cache loading, unloading & cleaning

	static inline constexpr std::array<Syntax::Type, 1> CACHED_ELEMS = { Syntax::CODE }; ///< Cached elements
	static inline constexpr std::size_t MAX_CACHE_SIZE = 1<<24; ///< Cached elements
	
	/**
	 * @brief Method to update the cache
	 * Discards used elements
	 */
	void updateCache();
public:
	/**
	 * @brief Constructor
	 * @param dir Directory to read/store cached elements
	 * @param stream Stream to write to
	 */
	Cache(const std::string& dir, std::ostream& stream);
	/**
	 * @brief Destructor
	 *
	 * Iterates through the disk cache and removes unused files
	 */
	~Cache();

	/**
	 * @brief Gets cached version of element
	 *
	 * @param elem Element to get cached version of
	 * @param empty Called if no cached version found
	 *
	 * @note if elem is not cached and it's set to be cached, the result of empty() will get cached
	 */
	template <NMLObject Elem>
	void operator()(const Elem& elem, std::function<void(std::ostream&)>&& empty)
	{
		if constexpr (std::find(CACHED_ELEMS.cbegin(), CACHED_ELEMS.cend(), Elem::type_index) != CACHED_ELEMS.cend())
		{
			if (m_dir.empty()) // No cache
			{
				empty(m_stream);
				return;
			}

			auto hash = elem.hash();

			std::unique_lock _{m_cacheMutex}; // Lock
			const auto it = m_cache.find(hash);
			if (it == m_cache.cend()) // Not in memory cache
			{
				// Load if exists
				if (const auto path = m_dir / hash; std::filesystem::exists(path) &&
					std::filesystem::is_regular_file(path))
				{
					std::ifstream in(path, std::ios::binary);
					if (!in.good()) [[unlikely]]
						throw Error(fmt::format("Unable to load cached file '{}'", path.native()));

					// Read file
					std::string s{(std::istreambuf_iterator<char>(in)),
							(std::istreambuf_iterator<char>())};

					// Write to stream
					m_stream << s;

					// Store to memory cache
					m_cache.insert(std::make_pair(std::move(hash), Cached{
							std::move(s), true, 1,
					}));
					m_cacheSize += s.size();
				}
				else // Call empty() and store
				{
					std::stringstream ss;
					empty(ss);

					// Write to stream
					m_stream << ss.rdbuf();

					// Store in memory cache
					auto hash = elem.hash();
					m_cache.insert(std::make_pair(hash, Cached{
						ss.str(), false, 1
					}));
				}
			}
			else // In cache
			{
				// Load
				Cached& c = it->second;

				// Write to stream
				m_stream << c.get();
			}
		}
		else
		{
			empty(m_stream);
		}
	}
};

#endif // NML_CACHE_HPP
