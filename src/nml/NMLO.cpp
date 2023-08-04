#include "NMLO.hpp"

[[nodiscard]] std::string NMLOHelper::getLispStringName(const std::string_view prefix, const std::string_view name)
{
	std::string r = "nmlo-";
	for (char c : prefix)
	{
		if (r.back() != '-' && std::tolower(c) != c)
			r.push_back('-');
		r.push_back(static_cast<char>(std::tolower(c)));
	}
	r.push_back('-');
	for (char c : name)
	{
		if (r.back() != '-' && std::tolower(c) != c)
			r.push_back('-');
		r.push_back(static_cast<char>(std::tolower(c)));
	}
	return r;
}
