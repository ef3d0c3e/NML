#ifndef NML_CACHE_HPP
#define NML_CACHE_HPP

#include "Syntax.hpp"
#include <filesystem>
#include <iostream>

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
 * Atlas in the form: map<Hash, std::string>
 *
 * struct Cached { std::string content; std::size_t uses; }; ///< Auto discard in a thread using 'GC'
 */

class Cache
{
	std::filesystem::path m_dir;
public:
	Cache(const std::string& dir):
		m_dir{dir} {}
	~Cache() {}

	/**
	 * @brief Gets cached version of element
	 *
	 * @param elem Element to get cached version of
	 * @empty Callback if no cached version found
	 * @returns cached elem or empty()
	 *
	 * @note if elem is not cached and it's set to be cached, the result of empty() will get cached
	 */
	std::string operator()(const Syntax::Element* elem, std::function<std::string()>&& empty);
};

#endif // NML_CACHE_HPP
