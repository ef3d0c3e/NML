#ifndef NML_CACHE_HPP
#define NML_CACHE_HPP

#include "Syntax.hpp"
#include <filesystem>
#include <iostream>

class Cache
{
	std::filesystem::path m_dir;

	// TODO: Open all files first for faster access
public:
	Cache(const std::string& dir):
		m_dir{dir} {}
	~Cache() {}

	std::string get(const Syntax::Element* elem, std::function<std::string()>&& empty)
	{
		// Hash elem
		// Get from cache
		// if not found return empty()
		
		return empty();
	}
};

#endif // NML_CACHE_HPP
