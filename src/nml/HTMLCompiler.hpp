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

#ifndef NML_HTMLCOMPILER_HPP
#define NML_HTMLCOMPILER_HPP

#include "Compiler.hpp"

class HTMLCompiler : public Compiler
{
public:
	HTMLCompiler(CompilerOptions&& opts):
		Compiler(std::move(opts)) {}

	[[nodiscard]] virtual std::string get_name() const;
	[[nodiscard]] virtual bool var_reserved(const std::string& name) const;
	[[nodiscard]] virtual std::string var_check(const std::string& name, const std::string& value) const;
	virtual void compile(const Document& doc) const;

	[[nodiscard]] static std::string get_anchor(const std::string_view& name);
	[[nodiscard]] static std::string formatHTML(const std::string_view& s);
};

#endif // NML_HTMLCOMPILER_HPP
