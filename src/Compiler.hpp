#ifndef NML_COMPILER_HPP
#define NML_COMPILER_HPP

#include "Syntax.hpp"

/**
 * @brief Options for a compiler
 */
struct CompilerOptions
{
	bool tex_enabled; ///< Whether tex processing is enabled or not
	std::string tex_dir; ///< Location of tex directory
	bool cxx_enabled; ///< Whether cxx processing is enabled or not
	std::string cxx_dir; ///< Location of cxx directory
};

/**
 * @brief Abstract class for a NML compiler
 */
class Compiler
{
public:
	/**
	 * @brief Destructor
	 */
	virtual ~Compiler() {}

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
	 * @param opts Compiler options
	 * @returns Compiled document as string
	 */
	[[nodiscard]] virtual std::string compile(const Document& doc, const CompilerOptions& opts) const = 0;
};

#endif // NML_COMPILER_HPP
