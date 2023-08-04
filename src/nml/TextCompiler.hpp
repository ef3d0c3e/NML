#ifndef NML_TEXTCOMPILER_HPP
#define NML_TEXTCOMPILER_HPP

#include "Compiler.hpp"

class TextCompiler : public Compiler
{
public:
	[[nodiscard]] virtual std::string get_name() const;
	[[nodiscard]] virtual bool var_reserved(const std::string& name) const;
	[[nodiscard]] virtual std::string var_check(const std::string& name, const std::string& value) const;
	[[nodiscard]] virtual std::string compile(const Document& doc, const CompilerOptions& opts) const;
};

#endif // NML_TEXTCOMPILER_HPP
