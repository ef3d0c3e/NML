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

#ifndef NML_COMPILER_HPP
#define NML_COMPILER_HPP

#include "Syntax.hpp"
#include <ostream>

/**
 * @brief Options for a compiler
 */
struct CompilerOptions
{
	std::ostream& stream; ///< Output stream

	/**
	 * Constructor
	 *
	 * @param stream Output stream
	 */
	CompilerOptions(std::ostream& stream):
		stream{stream} {}

	/**
	 * Options copy constructor
	 *
	 * @param opts CompilerOptions to copy options from (not stream)
	 * @param stream Output stream
	 */
	CompilerOptions(const CompilerOptions& opts, std::ostream& stream):
		stream{stream}
	{
		tex_enabled = opts.tex_enabled;
		tex_dir = opts.tex_dir;
		cache_enabled = opts.cache_enabled;
		cache_dir = opts.cache_dir;
		cxx_enabled = opts.cxx_enabled;
	}

	bool tex_enabled = false; ///< Whether tex processing is enabled or not
	std::string tex_dir; ///< Location of tex directory
	bool cache_enabled = false; ///< Whether caching is enabled
	std::string cache_dir; ///< Location of cache directory
	bool cxx_enabled = false; ///< Whether cxx processing is enabled or not
};

/**
 * @brief Abstract class for a NML compiler
 */
class Compiler
{
protected:
	CompilerOptions m_opts;
public:
	/**
	 * @brief Constructor
	 * @param opts Options for the compiler
	 */
	Compiler(CompilerOptions&& opts):
		m_opts{std::move(opts)} {}

	/**
	 * @brief Destructor
	 */
	virtual ~Compiler() {}

	/**
	 * @brief Gets options for compiler
	 *
	 * @return Compiler options
	 */
	[[nodiscard]] const CompilerOptions& getOptions() const { return m_opts; }

	/**
	 * @param Gets compiler name
	 *
	 * @returns Compiler's name
	 */
	[[nodiscard]] virtual std::string get_name() const = 0;

	/**
	 * @brief Gets whether a variable is reserved by this compiler
	 *
	 * @param name Variable's name
	 * @returns true If the variable is reserved
	 */
	[[nodiscard]] virtual bool var_reserved(const std::string& name) const = 0;


	/**
	 * @brief Gets whether a reserved variable has correct value
	 *
	 * @param name Variable's name
	 * @param value Variable's value
	 * @returns An error message in case it fails
	 */
	[[nodiscard]] virtual std::string var_check(const std::string& name, const std::string& value) const = 0;

	/**
	 * @brief Compile a document
	 *
	 * @param doc Document to compile
	 * @param stream Stream to output to
	 * @returns stream
	 */
	virtual void compile(const Document& doc) const = 0;
};

#endif // NML_COMPILER_HPP
