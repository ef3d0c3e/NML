#include "Cache.hpp"


std::string Cache::operator()(const Syntax::Element* elem, std::function<std::string()>&& empty)
{
	std::string&& value = empty();
}
