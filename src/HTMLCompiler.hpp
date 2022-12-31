#ifndef NML_HTMLCOMPILER_HPP
#define NML_HTMLCOMPILER_HPP

#include "Compiler.hpp"

class HTMLCompiler : public Compiler
{
public:
	[[nodiscard]] virtual std::string get_name() const;
	[[nodiscard]] virtual bool var_reserved(const std::string& name) const;
	[[nodiscard]] virtual std::string var_check(const std::string& name, const std::string& value) const;
	[[nodiscard]] virtual std::string compile(const Document& doc, const CompilerOptions& opts) const;

	[[nodiscard]] static std::string get_anchor(const std::string_view& name);
	[[nodiscard]] static std::string format(const std::string_view& s);
};

#endif // NML_HTMLCOMPILER_HPP
