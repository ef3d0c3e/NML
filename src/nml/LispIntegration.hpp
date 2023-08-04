#ifndef LISPINTEGRATION_HPP
#define LISPINTEGRATION_HPP

#include "Syntax.hpp"

/**
 * @brief Structure to hold methods for classes
 */
struct scheme_method
{
	std::array<int, 3> params; ///< {REQ, OPT, RST}
	std::string name;
	scm_t_subr fn;

	scheme_method(std::string&& name, int req, int opt, int rst, scm_t_subr fn):
		params{req, opt, rst}, name{std::move(name)}, fn{fn}
	{
	}
};

template <class T>
struct LispMethods
{
	const static inline std::array<scheme_method, 0> methods{};
};

template <>
struct LispMethods<Syntax::Text>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM content) {
			return Lisp::TypeConverter<Syntax::Text>::from(Syntax::Text(
				Lisp::TypeConverter<std::string>::to(content)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::StylePush>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM style) {
			return Lisp::TypeConverter<Syntax::StylePush>::from(Syntax::StylePush(
				Lisp::TypeConverter<Syntax::Style>::to(style)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::StylePop>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM style) {
			return Lisp::TypeConverter<Syntax::StylePop>::from(Syntax::StylePop(
				Lisp::TypeConverter<Syntax::Style>::to(style)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Section>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 2, 0, (void*)+[](SCM title, SCM level, SCM numbered, SCM toc) {
			return Lisp::TypeConverter<Syntax::Section>::from(Syntax::Section(
				Lisp::TypeConverter<std::string>::to(title),
				Lisp::TypeConverter<std::size_t>::to(level),
				!scm_is_eq(numbered, SCM_BOOL_F),
				scm_is_eq(toc, SCM_BOOL_T)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::ListBegin>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 2, 0, (void*)+[](SCM style, SCM o1, SCM o2, SCM o3) {
			if (scm_is_eq(o2, SCM_UNDEFINED)) // Unordered
				return Lisp::TypeConverter<Syntax::ListBegin>::from(Syntax::ListBegin(
					Lisp::TypeConverter<std::string>::to(style),
					Lisp::TypeConverter<std::string>::to(o1)
				));
			else // Ordered
				return Lisp::TypeConverter<Syntax::ListBegin>::from(Syntax::ListBegin(
					Lisp::TypeConverter<std::string>::to(style),
					Lisp::TypeConverter<Syntax::OrderedBullet::Type>::to(o1),
					Lisp::TypeConverter<std::string>::to(o2),
					Lisp::TypeConverter<std::string>::to(o3)
				));
		})
	};
};

template <>
struct LispMethods<Syntax::ListEnd>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM ordered) {
			return Lisp::TypeConverter<Syntax::ListEnd>::from(Syntax::ListEnd(
				Lisp::TypeConverter<bool>::to(ordered)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::ListEntry>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM tree, SCM counter) {
			return Lisp::TypeConverter<Syntax::ListEntry>::from(Syntax::ListEntry(
				Lisp::TypeConverter<SyntaxTree>::to(tree),
				Lisp::TypeConverter<std::size_t>::to(counter)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Ruler>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM length) {
			return Lisp::TypeConverter<Syntax::Ruler>::from(Syntax::Ruler(
				Lisp::TypeConverter<std::size_t>::to(length)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Figure>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 4, 0, 0, (void*)+[](SCM path, SCM name, SCM desc, SCM id) {
			return Lisp::TypeConverter<Syntax::Figure>::from(Syntax::Figure(
				Lisp::TypeConverter<std::string>::to(path),
				Lisp::TypeConverter<std::string>::to(name),
				Lisp::TypeConverter<SyntaxTree>::to(desc),
				Lisp::TypeConverter<std::size_t>::to(id)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Code>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 4, 0, 0, (void*)+[](SCM lang, SCM name, SCM style, SCM content) {
			return Lisp::TypeConverter<Syntax::Code>::from(Syntax::Code(
				Lisp::TypeConverter<std::string>::to(lang),
				Lisp::TypeConverter<std::string>::to(name),
				Lisp::TypeConverter<std::string>::to(style),
				Lisp::TypeConverter<std::vector<Syntax::code_fragment>>::to(content)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Quote>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM quote, SCM author) {
			return Lisp::TypeConverter<Syntax::Quote>::from(Syntax::Quote(
				Lisp::TypeConverter<SyntaxTree>::to(quote),
				Lisp::TypeConverter<std::string>::to(author)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Reference>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 3, 0, 0, (void*)+[](SCM ref, SCM name, SCM type) {
			return Lisp::TypeConverter<Syntax::Reference>::from(Syntax::Reference(
				Lisp::TypeConverter<std::string>::to(ref),
				Lisp::TypeConverter<std::string>::to(name),
				Lisp::TypeConverter<Syntax::RefType>::to(type)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Link>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM name, SCM path) {
			return Lisp::TypeConverter<Syntax::Link>::from(Syntax::Link(
				Lisp::TypeConverter<std::string>::to(name),
				Lisp::TypeConverter<std::string>::to(path)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Latex>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 7, 0, 0, (void*)+[](SCM content, SCM filename, SCM preamble, SCM prepend, SCM append, SCM font_size, SCM tex) {
			return Lisp::TypeConverter<Syntax::Latex>::from(Syntax::Latex(
				Lisp::TypeConverter<std::string>::to(content),
				Lisp::TypeConverter<std::string>::to(filename),
				Lisp::TypeConverter<std::string>::to(preamble),
				Lisp::TypeConverter<std::string>::to(prepend),
				Lisp::TypeConverter<std::string>::to(append),
				Lisp::TypeConverter<std::string>::to(font_size),
				Lisp::TypeConverter<Syntax::TexMode>::to(tex)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Raw>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM content) {
			return Lisp::TypeConverter<Syntax::Raw>::from(Syntax::Raw(
				Lisp::TypeConverter<std::string>::to(content)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::RawInline>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM content) {
			return Lisp::TypeConverter<Syntax::RawInline>::from(Syntax::RawInline(
				Lisp::TypeConverter<std::string>::to(content)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::ExternalRef>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 4, 0, 0, (void*)+[](SCM desc, SCM author, SCM url, SCM num) {
			return Lisp::TypeConverter<Syntax::ExternalRef>::from(Syntax::ExternalRef(
				Lisp::TypeConverter<std::string>::to(desc),
				Lisp::TypeConverter<std::string>::to(author),
				Lisp::TypeConverter<std::string>::to(url),
				Lisp::TypeConverter<std::size_t>::to(num)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Presentation>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM content, SCM type) {
			return Lisp::TypeConverter<Syntax::Presentation>::from(Syntax::Presentation(
				Lisp::TypeConverter<SyntaxTree>::to(content),
				Lisp::TypeConverter<Syntax::PresType>::to(type)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::Annotation>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM name, SCM content) {
			return Lisp::TypeConverter<Syntax::Annotation>::from(Syntax::Annotation(
				Lisp::TypeConverter<SyntaxTree>::to(name),
				Lisp::TypeConverter<SyntaxTree>::to(content)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::CustomStylePush>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM style) {
			return Lisp::TypeConverter<Syntax::CustomStylePush>::from(Syntax::CustomStylePush(
				Lisp::TypeConverter<CustomStyle>::to(style)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::CustomStylePop>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 1, 0, 0, (void*)+[](SCM style) {
			return Lisp::TypeConverter<Syntax::CustomStylePop>::from(Syntax::CustomStylePop(
				Lisp::TypeConverter<CustomStyle>::to(style)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::CustomPresPush>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM pres, SCM level) {
			return Lisp::TypeConverter<Syntax::CustomPresPush>::from(Syntax::CustomPresPush(
				Lisp::TypeConverter<CustomPres>::to(pres),
				Lisp::TypeConverter<std::size_t>::to(level)
			));
		})
	};
};

template <>
struct LispMethods<Syntax::CustomPresPop>
{
	const static inline std::array<scheme_method, 1> methods = {
		scheme_method("make", 2, 0, 0, (void*)+[](SCM pres, SCM level) {
			return Lisp::TypeConverter<Syntax::CustomPresPop>::from(Syntax::CustomPresPop(
				Lisp::TypeConverter<CustomPres>::to(pres),
				Lisp::TypeConverter<std::size_t>::to(level)
			));
		})
	};
};

#endif // LISPINTEGRATION_HPP
