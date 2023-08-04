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
