#ifndef NML_SYNTAX_HPP
#define NML_SYNTAX_HPP

#include <deque>
#include <map>
#include <stack>
#include <utility>
#include <variant>
#include <functional>
#include <memory>
#include <filesystem>
#include <algorithm>
#include "Lisp.hpp"
#include "Cenum.hpp"
#include "Util.hpp"

using namespace std::literals;

namespace Syntax { struct Element; struct Section; struct Figure; struct ExternalRef; }

// {{{ Lisp helpers
static std::string getLispStringName(const std::string& prefix, const std::string& name)
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

#define __DEFTYPE_MAP(__counter_base, __name, __m, __first, __second, ...) \
	__m(__COUNTER__ - __counter_base - 1, __name, __first, __second)     \
		__CENUM_IF_ELSE(__CENUM_HAS_ARGS(__VA_ARGS__))(                \
			__CENUM_DEFER2(__DEFTYPE__MAP)()(__counter_base, __name, __m, __VA_ARGS__))()
#define __DEFTYPE__MAP() __DEFTYPE_MAP
#define __DEFTYPE_INIT(__N, __name, __tp, __id) \
	scm_c_define_gsubr(getLispStringName(#__name, #__id).c_str(), 1, 0, 0, (void*)+[](SCM o) \
	{ return scm_list_ref(o, TypeConverter<std::size_t>::from(__N)); });
#define __DEFTYPE_TO(__N, __name, __tp, __id) \
	o.__id = TypeConverter<__tp>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(__N)));
#define __DEFTYPE_TO_NEW(__N, __name, __tp, __id) \
	p->__id = TypeConverter<__tp>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(__N)));
#define __DEFTYPE_FROM(__N, __name, __tp, __id) TypeConverter<__tp>::from(o.__id),
#define __DEFTYPE_DEF(__N, __name, __tp, __id) __tp __id;

#define DEFTYPE(__name, __enum, ...) \
	[[nodiscard]] static std::string get_name() {  \
		constexpr static std::string_view type_name{ #__name }; \
		std::string lower{type_name}; \
		std::transform(lower.cbegin(), lower.cend(), lower.begin(), [](unsigned char c){ return std::tolower(c); }); \
		return lower; \
	} \
	[[nodiscard]] Type get_type() const noexcept { return __enum; } \
	constexpr static Type ELEMENT_TYPE = __enum; \
private: \
	enum { __counter2 = __COUNTER__ }; \
public: \
	__CENUM_EXPAND(__CENUM_EVAL(__DEFTYPE_MAP(__counter2, __name, __DEFTYPE_DEF, __VA_ARGS__))) \
	[[nodiscard]] __name() {} \
}; \
} \
template <> \
struct Lisp::TypeConverter<Syntax::__name> \
{ \
private: \
	enum { __counter1 = __COUNTER__ }; \
public: \
	static void init() \
	{ \
		__CENUM_EXPAND(__CENUM_EVAL(__DEFTYPE_MAP(__counter1+1, __name, __DEFTYPE_INIT, __VA_ARGS__))) \
		scm_c_define_gsubr(getLispStringName("is", #__name).c_str(), 1, 0, 0, (void*)+[](SCM o) \
		{ return scm_from_bool(Syntax::__name::ELEMENT_TYPE == scm_to_uint8(scm_list_ref(o, TypeConverter<std::size_t>::from(0)))); }); \
	} \
private: \
	enum { __counter2 = __COUNTER__ }; \
public: \
	[[nodiscard]] static Syntax::__name to(SCM v) \
	{ \
		Syntax::__name o; \
		__CENUM_EXPAND(__CENUM_EVAL(__DEFTYPE_MAP(__counter2+1, __name, __DEFTYPE_TO, __VA_ARGS__))) \
		return o; \
	} \
private: \
	enum { __counter3 = __COUNTER__ }; \
public: \
	[[nodiscard]] static Syntax::__name* to_new(SCM v) \
	{ \
		Syntax::__name* p = new Syntax::__name; \
		__CENUM_EXPAND(__CENUM_EVAL(__DEFTYPE_MAP(__counter3+1, __name, __DEFTYPE_TO_NEW, __VA_ARGS__))) \
		return p; \
	} \
private: \
	enum { __counter4 = __COUNTER__ }; \
public: \
	[[nodiscard]] static SCM from(const Syntax::__name& o) \
	{ \
		return scm_list_n( \
			TypeConverter<std::uint8_t>::from(o.get_type().value), \
			__CENUM_EXPAND(__CENUM_EVAL(__DEFTYPE_MAP(__counter4+1, __name, __DEFTYPE_FROM, __VA_ARGS__))) \
			SCM_UNDEFINED); \
	} \
}; \
namespace Syntax {

#define DEFCTYPE(__name, __enum, ...) \
	[[nodiscard]] CType get_type() const noexcept { return CType::__enum; } \
private: \
	enum { __counter1 = __COUNTER__ }; \
public: \
	__CENUM_EXPAND(__CENUM_EVAL(__DEFTYPE_MAP(__counter1, __name, __DEFTYPE_DEF, __VA_ARGS__))) \
};
// }}}

/**
 * @brief Contains all the syntax elements
 */
class SyntaxTree
{
friend class Document;
friend struct Lisp::TypeConverter<SyntaxTree>;
	std::deque<std::shared_ptr<Syntax::Element>> elems; ///< Elements
public:
	/**
	 * @brief Constructor
	 */
	SyntaxTree() {}

	/**
	 * @brief Move constructor
	 *
	 * @param tree SyntaxTree to move from
	 */
	SyntaxTree(SyntaxTree&& tree):
		elems(std::move(tree.elems)) {}

	/**
	 * @brief Copy constructor
	 *
	 * @param tree SyntaxTree to copy from
	 */
	SyntaxTree(const SyntaxTree& tree)
	{
		elems = tree.elems;
	}

	/**
	 * @brief Copy operator
	 *
	 * @param tree SyntaxTree to copy from
	 */
	SyntaxTree& operator=(const SyntaxTree& tree)
	{
		elems = tree.elems;
		return *this;
	}

	/**
	 * @brief Destructor
	 */
	~SyntaxTree() {}

	/**
	 * @brief Clears every element (without deallocating them)
	 */
	void clear()
	{ elems.clear(); }

	/**
	 * @brief Inserts new element
	 *
	 * @param elem Element to insert
	 */
	void insert(std::shared_ptr<Syntax::Element> elem)
	{ elems.push_back(elem); }

	/**
	  * @brief Inserts before element
	  * @note Does not update figures, external refs and sections
	  *
	  * @param before Element to insert before of
	  * @param args Arguments for newly constructed element
	  * @returns Newly inserted element
	  */
	template <class T, class... Args>
		requires std::is_base_of<Syntax::Element, T>::value
	T* insert_before(const Syntax::Element* before, Args&&... args)
	{
		T* ptr = new T(std::forward<Args>(args)...);
		const auto pos = std::find_if(elems.cbegin(), elems.cend(), [&](const auto& elem) { return before == elem.get(); });
		if (pos == elems.cend()) [[unlikely]]
			throw Error(fmt::format("Cannot insert_before `{}`, not found", (const void*)before));
		elems.insert(pos, std::shared_ptr<Syntax::Element>(ptr));

		return ptr;
	}

	void for_each_elem(std::function<void(const Syntax::Element*)>&& f) const
	{
		for (const auto& elem : elems)
			f(elem.get());
	}

	void for_each_elem(std::function<void(Syntax::Element*)>&& f)
	{
		for (auto& elem : elems)
			f(elem.get());
	}

	void pop_back()
	{
		elems.pop_back();
	}

	[[nodiscard]] auto begin() const noexcept
	{ return elems.begin(); }
	[[nodiscard]] auto cbegin() const noexcept
	{ return elems.cbegin(); }
	[[nodiscard]] auto end() const noexcept
	{ return elems.end(); }
	[[nodiscard]] auto cend() const noexcept
	{ return elems.cend(); }

	[[nodiscard]] auto rbegin() const noexcept
	{ return elems.rbegin(); }
	[[nodiscard]] auto crbegin() const noexcept
	{ return elems.crbegin(); }
	[[nodiscard]] auto rend() const noexcept
	{ return elems.rend(); }
	[[nodiscard]] auto crend() const noexcept
	{ return elems.crend(); }
};


template<>
struct Lisp::TypeConverter<SyntaxTree>
{
	using cv = TypeConverter<std::deque<std::shared_ptr<Syntax::Element>>>;

	// TODO: make a copy of every single element
	[[nodiscard]] static SyntaxTree to(SCM v);
	[[nodiscard]] static SCM from(const SyntaxTree& tree);
};

//template<class T>
//requires (std::is_same_v<T, SyntaxTree>)
//struct Lisp::TypeConverter<T>
//{
//	using cv = TypeConverter<std::deque<std::shared_ptr<Syntax::Element>>>;
//
//	[[nodiscard]] static SyntaxTree to(SCM v)
//	{
//		return SyntaxTree{};
//	}
//	[[nodiscard]] static SCM from(const SyntaxTree& tree)
//	{
//		return scm_from_int8(0);
//	}
//};

// optional<T>
template <class T>
struct Lisp::TypeConverter<std::optional<T>>
{
	using cv = TypeConverter<T>;

	[[nodiscard]] static std::optional<T> to(SCM v)
	{
		if (scm_nil_p(v))
			return std::optional<T>{};
		else
			return std::optional<T>{cv::to(scm_list_ref(v, TypeConverter<std::size_t>::from(1)))};
	}
	[[nodiscard]] static SCM from(const std::optional<T>& v)
	{
		if (v.has_value())
			return scm_list_1(cv::from(v.value()));
		else
			return SCM_EOL;
	}
};

MAKE_CENUM_Q(CType, std::uint8_t,
	STYLE, 0,
	PRES, 1,
	PROCESS, 2);
/**
 * @brief Abstract class for a custom type
 */
struct CustomType
{
	std::string_view type_name; ///< Name of custom type

	[[nodiscard]] virtual CType get_type() const noexcept = 0;


	/**
	 * @brief Move constructor
	 *
	 * @param type_name Name of custom type
	 */
	CustomType(const std::string_view& type_name):
		type_name(type_name) {}

	virtual ~CustomType() {}
};

/**
 * @brief Custom text style
 */
struct CustomStyle : public CustomType
{
	/**
	 * @brief Constructor
	 *
	 * @param name Custom style's name
	 * @param regex Custom style's regex
	 * @param begin Begin callback
	 * @param end End callback
	 */
	[[nodiscard]] CustomStyle(const std::string_view& name, std::size_t index, std::string_view&& regex, Lisp::Proc&& begin, Lisp::Proc&& end) noexcept:
		CustomType(name), index(index), regex(std::move(regex)), begin(std::move(begin)), end(std::move(end)) {}

	/**
	 * @brief Constructor
	 *
	 * @param name Custom style's name
	 * @param regex Custom style's regex
	 * @param begin Begin callback
	 * @param end End callback
	 * @param apply Apply callback
	 */
	[[nodiscard]] CustomStyle(const std::string_view& name, std::size_t index, std::string_view&& regex, Lisp::Proc&& begin, Lisp::Proc&& end, Lisp::Proc&& apply) noexcept:
		CustomType(name), index(index), regex(std::move(regex)), begin(std::move(begin)), end(std::move(end)), apply(std::move(apply)) {}

	/**
	 * @brief Copy constructor
	 *
	 * @param style Style to copy
	 */
	[[nodiscard]] CustomStyle(const CustomStyle& style) noexcept:
		CustomType(style.type_name), index(style.index), regex(style.regex), begin(style.begin), end(style.end)
	{
		if (style.apply.has_value())
			apply.emplace(style.apply.value());
	}

	/**
	 * @brief Default constructor
	 */
	[[nodiscard]] CustomStyle() noexcept:
		CustomType("") {}

DEFCTYPE(CustomStyle, STYLE,
	std::size_t, index,
	std::string, regex,
	Lisp::Proc, begin,
	Lisp::Proc, end,
	std::optional<Lisp::Proc>, apply)

template <>
struct Lisp::TypeConverter<CustomStyle>
{
	[[nodiscard]] static CustomStyle to(SCM v)
	{
		auto style = CustomStyle(
			TypeConverter<std::string_view>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0))),
			TypeConverter<std::size_t>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(1))),
			TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(2))),
			TypeConverter<Proc>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(3))),
			TypeConverter<Proc>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(4)))
		);
		auto apply = TypeConverter<std::optional<Proc>>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(5)));
		if (apply.has_value())
			style.apply.emplace(apply.value().proc);
		return style;
	}
	[[nodiscard]] static SCM from(const CustomStyle& style)
	{
		return scm_list_n(
			TypeConverter<std::string_view>::from(style.type_name),
			TypeConverter<std::size_t>::from(style.index),
			TypeConverter<std::string>::from(style.regex),
			TypeConverter<Proc>::from(style.begin),
			TypeConverter<Proc>::from(style.end),
			TypeConverter<std::optional<Proc>>::from(style.apply),
			SCM_UNDEFINED
		);
	}
};

/**
 * @brief Custom presentation
 */
struct CustomPres : public CustomType
{
	/**
	 * @brief Constructor
	 *
	 * @param name Custom presentation's name
	 * @param regex_begin Begin presentation regex
	 * @param regex_end End presentation regex
	 * @param begin Begin callback
	 * @param end End callback
	 */
	[[nodiscard]] CustomPres(const std::string_view& name, std::size_t index, std::string&& regex_begin, std::string&& regex_end, Lisp::Proc&& begin, Lisp::Proc&& end) noexcept:
		CustomType(name), index(index), regex_begin(std::move(regex_begin)), regex_end(std::move(regex_end)), begin(std::move(begin)), end(std::move(end)) {}

	/**
	 * @brief Default constructor
	 */
	[[nodiscard]] CustomPres() noexcept:
		CustomType("") {}

DEFCTYPE(CustomPres, PRES,
	std::size_t, index,
	std::string, regex_begin,
	std::string, regex_end,
	Lisp::Proc, begin,
	Lisp::Proc, end)

template <>
struct Lisp::TypeConverter<CustomPres>
{
	[[nodiscard]] static CustomPres to(SCM v)
	{
		return CustomPres(
			TypeConverter<std::string_view>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0))),
			TypeConverter<std::size_t>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(1))),
			TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(2))),
			TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(3))),
			TypeConverter<Proc>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(4))),
			TypeConverter<Proc>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(5)))
		);
	}
	[[nodiscard]] static SCM from(const CustomPres& style)
	{
		return scm_list_n(
			TypeConverter<std::string_view>::from(style.type_name),
			TypeConverter<std::size_t>::from(style.index),
			TypeConverter<std::string>::from(style.regex_begin),
			TypeConverter<std::string>::from(style.regex_end),
			TypeConverter<Proc>::from(style.begin),
			TypeConverter<Proc>::from(style.end),
			SCM_UNDEFINED
		);
	}
};

/**
 * @brief Custom presentation
 */
struct CustomProcess : public CustomType
{
	/**
	 * @brief Constructor
	 *
	 * @param name Custom process's name
	 * @param regex_begin Begin process regex
	 * @param token_end End process token
	 * @param apply Apply callback
	 */
	[[nodiscard]] CustomProcess(const std::string_view& name, std::size_t index, std::string&& regex_begin, std::string&& token_end, Lisp::Proc&& apply) noexcept:
		CustomType(name), index(index), regex_begin(std::move(regex_begin)), token_end(std::move(token_end)), apply(std::move(apply.proc)) {}

	/**
	 * @brief Default constructor
	 */
	[[nodiscard]] CustomProcess() noexcept:
		CustomType("") {}

DEFCTYPE(CustomProcess, PROCESS,
	std::size_t, index,
	std::string, regex_begin,
	std::string, token_end,
	Lisp::Proc, apply)

template <>
struct Lisp::TypeConverter<CustomProcess>
{
	[[nodiscard]] static CustomProcess to(SCM v)
	{
		return CustomProcess(
			TypeConverter<std::string_view>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0))),
			TypeConverter<std::size_t>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(1))),
			TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(2))),
			TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(3))),
			TypeConverter<Proc>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(4)))
		);
	}
	[[nodiscard]] static SCM from(const CustomProcess& style)
	{
		return scm_list_n(
			TypeConverter<std::string_view>::from(style.type_name),
			TypeConverter<std::size_t>::from(style.index),
			TypeConverter<std::string>::from(style.regex_begin),
			TypeConverter<std::string>::from(style.token_end),
			TypeConverter<Proc>::from(style.apply),
			SCM_UNDEFINED
		);
	}
};

/**
  * @brief Abstract variable class
  */
struct Variable
{
	MAKE_CENUMV_Q(Type, std::uint8_t,
		TEXT, 0,
		PATH, 1,
		PROXY, 2,
	);

	/**
	  * @brief Gets variable's type
	  * @returns Variable's type
	  */
	[[nodiscard]] virtual Type get_type() const noexcept = 0;

	/**
	  * @brief Gets variable as a string
	  * @param doc Document
	  * @returns Variable converted to string
	  */
	[[nodiscard]] virtual std::string to_string(const Document& doc) const = 0;

	/**
	  * @brief Clones variable
	  * @returns Clone of this
	  */
	[[nodiscard]] virtual Variable* clone() const = 0;

	virtual ~Variable() {};
};

struct TextVariable : public Variable
{
	[[nodiscard]] virtual Type get_type() const noexcept { return Variable::TEXT; };
	[[nodiscard]] virtual std::string to_string(const Document&) const noexcept { return content; };
	[[nodiscard]] virtual Variable* clone() const { return new TextVariable(content); }

	/**
	  * @brief String constructor
	  * @param content String to copy
	  */
	TextVariable(const std::string& content):
		content(content) {}

	/**
	  * @brief Move constructor
	  * @param content Rvalue string to move
	  */
	TextVariable(std::string&& content):
		content(std::move(content)) {}

	std::string content;
};

struct PathVariable : public Variable
{
	[[nodiscard]] virtual Type get_type() const noexcept { return Variable::PATH; };
	[[nodiscard]] virtual std::string to_string(const Document&) const noexcept { return static_cast<std::string>(std::filesystem::relative(path)); };
	[[nodiscard]] virtual Variable* clone() const { return new PathVariable(path); }

	/**
	  * @brief String constructor
	  * @param content String to copy
	  */
	PathVariable(const std::filesystem::path& path):
		path(path) {}

	/**
	  * @brief Move constructor
	  * @param content Rvalue string to move
	  */
	PathVariable(std::filesystem::path&& path):
		path(std::move(path)) {}

	std::filesystem::path path;
};

struct ProxyVariable : public Variable
{
	[[nodiscard]] virtual Type get_type() const noexcept { return Variable::PROXY; };
	[[nodiscard]] virtual std::string to_string(const Document&) const noexcept;
	[[nodiscard]] virtual Variable* clone() const { return new ProxyVariable(proxy); }

	/**
	  * @brief Proxy constructor
	  * @param proxy Proxy variable name
	  * @param doc Document of proxy variable
	  */
	ProxyVariable(const std::string& proxy):
		proxy(proxy) {}

	const std::string proxy;

	[[nodiscard]] const std::string& proxy_name() const noexcept { return proxy; }
};

/**
 * @brief Represents a document
 */
class Document
{
	friend struct Parser;

	SyntaxTree tree; ///< Syntax elements
	std::map<std::string, std::unique_ptr<Variable>> vars; ///< Variables

	std::deque<std::pair<std::size_t, Syntax::Section*>> header; ///< Contains all sections
	std::stack<std::size_t> numbers;

	std::map<std::string, const CustomType*> custom_types;
	std::deque<Syntax::ExternalRef*> external_refs; ///< Contains all external refs

	std::map<std::string, Syntax::Figure*> figures; ///< Map of figures
	std::size_t figure_id = 0; ///< Last figure's id
	std::size_t external_ref_id = 0; ///< Last externalref's id
public:
	/**
	  * @brief Default constructor
	  */
	Document() {}

	/**
	  * @brief Copy constructor
	  */
	Document(const Document& doc):
		tree(doc.tree)
	{
		doc.var_for_each([this](const std::string& name, const Variable* var)
		{ vars.insert({name, std::unique_ptr<Variable>(var->clone())}); });

		header = doc.header;
		numbers = doc.numbers;

		custom_types = doc.custom_types;
		external_refs = doc.external_refs;
		figures = doc.figures;
		figure_id = doc.figure_id;
		external_ref_id = doc.external_ref_id;
	}

	/**
	 * @brief Construct a new at the end
	 * @param args Arguments to constructor
	 *
	 * @returns Pointer to T constructed with args
	 */
	template <class T, class... Args>
		requires std::is_base_of<Syntax::Element, T>::value
	T* emplace_back(Args&&... args)
	{
		T* ptr;
		if constexpr (std::is_same_v<T, Syntax::Figure>)
		{
			ptr = new T(std::forward<Args>(args)..., ++figure_id);
			figures.insert({ptr->name, ptr});
		}
		else if constexpr (std::is_same_v<T, Syntax::ExternalRef>)
		{
			ptr = new T(std::forward<Args>(args)..., ++external_ref_id);
			external_refs.push_back(ptr);
		}
		else if constexpr (std::is_same_v<T, Syntax::Section>)
		{
			ptr = new T(std::forward<Args>(args)...);

			while (ptr->level > numbers.size())
				numbers.push(0);
			while (ptr->level < numbers.size())
				numbers.pop();
			++numbers.top();

			if (ptr->toc)
				header.emplace_back(numbers.top(), ptr);
		}
		else
		{
			ptr = new T(std::forward<Args>(args)...);
		}
		tree.elems.push_back(std::shared_ptr<Syntax::Element>(ptr));

		return ptr;
	}

	void push_back(Syntax::Element* ptr)
	{
		// TODO
		tree.elems.push_back(std::shared_ptr<Syntax::Element>(ptr));
	}

	void pop_back()
	{
		const auto back = tree.elems.back();


		tree.elems.pop_back();
	}

	/**
	 * @brief Merges two documents together
	 *
	 * @param doc Document to merge
	 */
	void merge(Document&& doc)
	{
		merge_non_elems(doc);

		for (auto&& elem : doc.tree)
			tree.insert(elem);
		doc.tree.clear();
	}

	/**
	 * @brief Merges another document's non elements
	 * @note This means merging
	 *  - Figures
	 *  - Definitions
	 *  - References
	 *  - Custom Types
	 *
	 * @param doc Document to merge
	 */
	void merge_non_elems(const Document& doc)
	{
		for (const auto& [key, val] : doc.vars)
			var_insert(key, std::unique_ptr<Variable>(val.get()->clone()));

		for (const auto& [key, type] : doc.custom_types)
			types_add(key, type);
	}

	/**
	 * @brief Gets whether document is empty
	 *
	 * @returns true If document is empty, false otherwise
	 */
	[[nodiscard]] inline bool empty() const noexcept
	{ return tree.elems.empty(); }

	/**
	 * @brief Gets left element
	 * @warning Must not be empty
	 *
	 * @returns Last element
	 */
	[[nodiscard]] inline const Syntax::Element* const back() const noexcept
	{ return tree.elems.back().get(); }

	/**
	 * @brief Gets left element
	 * @warning Must not be empty
	 *
	 * @returns Last element
	 */
	[[nodiscard]] inline Syntax::Element* back() noexcept
	{ return tree.elems.back().get(); }

	/**
	 * @brief Gets tree
	 *
	 * @returns (cref) Tree
	 */
	[[nodiscard]] const SyntaxTree& get_tree() const noexcept
	{ return tree; }

	/**
	 * @brief Gets tree
	 *
	 * @returns (ref) Tree
	 */
	[[nodiscard]] SyntaxTree& get_tree() noexcept
	{ return tree; }

	/**
	 * @brief Gets header
	 *
	 * @returns (cref) Header
	 */
	[[nodiscard]] const auto& get_header() const noexcept
	{ return header; }

	/**
	 * @brief Adds a custom type
	 *
	 * @param name Custom type's name (must be unique)
	 * @param type Custom type to add
	 */
	void types_add(const std::string_view& name, const CustomType* type)
	{
		custom_types.insert({std::string(std::move(name)), type});
	}

	/**
	 * @brief Gets whether a custom type with name already exists
	 *
	 * @param name Name to search for
	 * @returns true If a custom type with name exists
	 */
	[[nodiscard]] bool type_exists(const std::string& name) const
	{
		return custom_types.find(name) != custom_types.end();
	}

	/**
	 * @brief Gets a custom type by it's name
	 *
	 * @param name Custom type's name
	 * @returns A const pointer to the custom type if found, nullptr otherwise
	 */
	[[nodiscard]] const CustomType* types_get(const std::string& name) const
	{
		const auto it = custom_types.find(name);
		return it == custom_types.end() ? nullptr : it->second;
	}

	void custom_types_for_each(std::function<void(const std::string&, const CustomType*)> fn)
	{
		for (auto& [name, type] : custom_types)
		{
			fn(name, type);
		}
	}

	void custom_styles_for_each(std::function<void(const std::string&, const CustomStyle*)> fn)
	{
		for (auto& [name, style] : custom_types)
		{
			if (style->get_type() == CType::STYLE)
				fn(name, reinterpret_cast<const CustomStyle*>(style));
		}
	}

	void custom_pres_for_each(std::function<void(const std::string&, const CustomPres*)> fn)
	{
		for (auto& [name, pres] : custom_types)
		{
			if (pres->get_type() == CType::PRES)
				fn(name, reinterpret_cast<const CustomPres*>(pres));
		}
	}

	void custom_process_for_each(std::function<void(const std::string&, const CustomProcess*)> fn)
	{
		for (auto& [name, process] : custom_types)
		{
			if (process->get_type() == CType::PROCESS)
				fn(name, reinterpret_cast<const CustomProcess*>(process));
		}
	}

	/**
	 * @brief Gets external refs
	 *
	 * @returns (cref) External refs
	 */
	[[nodiscard]] const auto& get_external_refs() const noexcept
	{ return external_refs; }

	/**
	 * @brief Erases a variable
	 *
	 * @param var Variable name to erase
	 * @returns true If variable was erased
	 */
	bool var_erase(const std::string& var)
	{ return vars.erase(var) != 0; }

	/**
	 * @brief Inserts/Modifies a variable
	 *
	 * @param key Variable's key
	 * @param value Variable's value
	 */
	void var_insert(const std::string& key, std::unique_ptr<Variable>&& value)
	{
		auto it = vars.find(key);
		if (it != vars.end())
			it->second.swap(value);
		else
			vars.insert({key, std::move(value)});
	}

	/**
	 * @brief Gets if variable exists
	 *
	 * @param key Variable's name
	 * @returns Variable If a variable was found (nullptr otherwise)
	 */
	Variable* var_get(const std::string& key) const
	{
		const auto it = vars.find(key);
		if (it == vars.end())
			return nullptr;

		return it->second.get();
	}

	/**
	 * @brief Gets variable or fallback
	 *
	 * @param key Variable's name
	 * @param def Default value in case no value is found
	 * @returns Found value or def
	 */
	[[nodiscard]] std::string var_get_default(const std::string& key, const std::string& def) const
	{
		const auto it = vars.find(key);
		if (it == vars.end())
			return def;

		return it->second.get()->to_string(*this);
	}

	/**
	 * @brief Performs an action on each variable
	 *
	 * @param fn Action to perform
	 */
	void var_for_each(std::function<void(const std::string&, Variable*)> fn)
	{
		for (auto& [name, var] : vars)
			fn(name, var.get());
	}


	/**
	 * @brief Performs an action on each variable
	 *
	 * @param fn Action to perform
	 */
	void var_for_each(std::function<void(const std::string&, const Variable*)> fn) const
	{
		for (auto& [name, var] : vars)
			fn(name, var.get());
	}


	/**
	 * @brief Gets whether a figure with name already exists
	 *
	 * @param name Name to search for
	 * @returns true If a figure with name exists
	 */
	[[nodiscard]] bool figure_exists(const std::string& name) const
	{
		return figures.find(name) != figures.end();
	}

	/**
	 * @brief Gets a figure by it's name
	 *
	 * @param name Figure's name
	 * @returns A const pointer to the figure if found, nullptr otherwise
	 */
	[[nodiscard]] const Syntax::Figure* figure_get(const std::string& name) const
	{
		const auto it = figures.find(name);
		return it == figures.end() ? nullptr : it->second;
	}
};

namespace Syntax
{
/**
 * @brief Type of syntax elemnts
 */
MAKE_CENUMV_Q(Type, std::uint8_t,
	TEXT, 0,
	STYLEPUSH, 1,
	STYLEPOP, 2,
	BREAK, 3,
	SECTION, 4,
	LIST_BEGIN, 5,
	LIST_END, 6,
	LIST_ENTRY, 7,
	RULER, 8,
	FIGURE, 9,
	CODE, 10,
	QUOTE, 11,
	REFERENCE, 12,
	LINK, 13,
	LATEX, 14,
	RAW, 15,
	RAW_INLINE, 16,
	EXTERNAL_REF, 17,
	PRESENTATION, 18,
	ANNOTATION, 19,

	/* User-Defined */
	CUSTOM_STYLEPUSH, 20,
	CUSTOM_STYLEPOP, 21,
	CUSTOM_PRESPUSH, 22,
	CUSTOM_PRESPOP, 23,
);

/**
 * @brief Gets whether type is custom or not
 *
 * @param type Type
 * @returns True if type is custom
 */
[[nodiscard]] bool isCustomType(Type type) noexcept;

/**
 * @brief Gets name for type
 *
 * @param type Type
 * @returns Type's name
 */
[[nodiscard]] std::string_view getTypeName(Type type) noexcept;


/**
 * @brief Abstract class for an element
 */
struct Element
{
	[[nodiscard]] virtual Type get_type() const noexcept = 0;

	virtual ~Element() {};
};


/**
 * @brief Styles for text
 */
MAKE_CENUM_Q(Style, std::uint8_t,
	NONE,      0,
	BOLD,      1<<0,
	UNDERLINE, 1<<1,
	ITALIC,    1<<2,
	VERBATIM,  1<<3,
);
/**
 * @brief Gets name for style
 *
 * @param style Style
 * @returns Style's name
 */
[[nodiscard]] std::string_view getStyleName(Style style) noexcept;
/**
 * @brief Performs an action for every (set) style
 *
 * @param s Style
 * @param fn Function to call for each set style
 * @param reverse Run through all styles in opposite order
 */
void forEveryStyle(Style s, std::function<void(Style)> fn, bool reverse = false);

//{{{ Syntax elements

} // Syntax
template <>
struct Lisp::TypeConverter<Syntax::Style>
{
	[[nodiscard]] static Syntax::Style to(SCM v)
	{
		return scm_to_uint8(v);
	}
	[[nodiscard]] static SCM from(const Syntax::Style& x)
	{
		return scm_from_uint8(x);
	}
};
namespace Syntax {

/**
 * @brief Text
 * Text can have style @see Text::Style
 */
struct Text : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _content Text's content
	 * @param _style Text's style
	 */
	[[nodiscard]] Text(std::string&& content) noexcept:
		content(std::move(content)) {}

	/**
	 * @brief String view constructor
	 *
	 * @param view View to content
	 * @param _style Text's style
	 */
	[[nodiscard]] Text(const std::string_view& view) noexcept:
		content(view) {}

DEFTYPE(Text, TEXT,
	std::string, content)

/**
 * @brief Single style being added
 */
struct StylePush : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _style Added style
	 */
	[[nodiscard]] StylePush(Style style) noexcept:
		style(style) {}
DEFTYPE(StylePush, STYLEPUSH,
	Syntax::Style, style)

/**
 * @brief Single style being removed
 */
struct StylePop : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _style Removec style
	 */
	[[nodiscard]] StylePop(Style style) noexcept:
		style(style) {}

DEFTYPE(StylePop, STYLEPOP,
	Syntax::Style, style)

/**
 * @brief Break
 * Represents spacing in the document
 */
struct Break : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _size Number of lines for break
	 */
	[[nodiscard]] Break(std::size_t size) noexcept:
		size(size) {}

DEFTYPE(Break, BREAK,
	std::size_t, size)

/**
 * @brief Section
 * Represents section in the document
 */
struct Section : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _title Section's title
	 * @param _level Section's level
	 * @param _numbered Whether section should have numbering
	 * @param _tor Whether section sohuld appear in the toc
	 */
	[[nodiscard]] Section(std::string&& _title, std::size_t _level, bool _numbered, bool _toc) noexcept:
		title(std::move(_title)), level(_level), numbered(_numbered), toc(_toc) {}

DEFTYPE(Section, SECTION,
	std::string, title,
	std::size_t, level,
	bool, numbered,
	bool, toc)

// {{{ Bullets
/**
 * @brief Bullet for an unordered list
 */
struct UnorderedBullet
{
	std::string bullet; ///< Bullet string "#+BulletN"
};

/**
 * @brief Bullet for an ordered list
 *
 * Setting '`#+BulletStypeN (i).`' Will be stored as:
 * \code{.cpp}
 * OrderedBullet{
 *	.bullet = ROMAN,
 *	.left = "(",
 *	.right = ")."
 * }
 * \endcode
 */
struct OrderedBullet
{
	MAKE_CENUMV_Q(Type, std::uint8_t,
		NUMBER,        0, // 1
		ALPHA,         1, // a
		ALPHA_CAPITAL, 2, // A
		ROMAN,         3, // i
		ROMAN_CAPITAL, 4, // I
		PEX,           5, // v
	);
	Type bullet; ///< Bullet numbering type

	std::string left; ///< Left delimiter (before bullet type)
	std::string right; ///< Right delimiter (after bullet type)

	/**
	 * @brief Checks if a number is representible using a certain bullet type
	 *
	 * @param n Number to represent
	 * @returns An error message if number is not representible, an empty string otherwise
	 */
	[[nodiscard]] std::string is_representible(std::size_t n) const;

	/**
	 * @brief Gets the representation for a number
	 *
	 * @param n Number to represent
	 * @returns Number represented using type
	 */
	[[nodiscard]] std::string get(std::size_t n) const;
};

using bullet_type = std::variant<Syntax::UnorderedBullet, Syntax::OrderedBullet>;

} // Syntax
template <>
struct Lisp::TypeConverter<Syntax::OrderedBullet::Type>
{
	[[nodiscard]] static Syntax::OrderedBullet::Type to(SCM v)
	{
		return TypeConverter<std::uint8_t>::to(v);
	}
	[[nodiscard]] static SCM from(Syntax::OrderedBullet::Type type)
	{
		return TypeConverter<std::uint8_t>::from(type);
	}
};

template<>
struct Lisp::TypeConverter<Syntax::bullet_type>
{
	[[nodiscard]] static Syntax::bullet_type to(SCM v)
	{
		if (scm_ilength(v) == 1)
			return Syntax::UnorderedBullet{
				.bullet = TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0))),
			};
		else
			return Syntax::OrderedBullet{
				.bullet = TypeConverter<std::uint8_t>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0))),
				.left = TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(1))),
				.right = TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(2)))
			};
	}
	[[nodiscard]] static SCM from(const Syntax::bullet_type& bullet)
	{
		return std::visit([]<class T>(const T& bullet)
		{
			if constexpr (std::is_same_v<T, Syntax::UnorderedBullet>)
				return scm_list_1(TypeConverter<std::string>::from(bullet.bullet));
			else if constexpr (std::is_same_v<T, Syntax::OrderedBullet>)
				return scm_list_3(
					TypeConverter<std::uint8_t>::from(bullet.bullet),
					TypeConverter<std::string>::from(bullet.left),
					TypeConverter<std::string>::from(bullet.right));
		}, bullet);
	}
};
namespace Syntax {
// }}}

/**
 * @brief Beginning of a list
 */
struct ListBegin : public Element
{
	/**
	 * @brief Unordered list constructor
	 *
	 * @param _style Additional style
	 * @param bullet List's bullet
	 */
	[[nodiscard]] ListBegin(std::string&& style, std::string&& _bullet):
		style(std::move(style)), ordered(false)
	{
		bullet.emplace<UnorderedBullet>(std::move(_bullet));
	}

	/**
	 * @brief Ordered list constructor
	 *
	 * @param _style Additional style
	 * @param type List's bullet type
	 * @param left Lest delimiter
	 * @param right Right delimiter
	 */
	[[nodiscard]] ListBegin(std::string&& style, OrderedBullet::Type type, std::string&& left, std::string&& right):
		style(std::move(style)), ordered(true)
	{
		bullet.emplace<OrderedBullet>(type, std::move(left), std::move(right));
	}

DEFTYPE(ListBegin, LIST_BEGIN,
	std::string, style,
	bool, ordered,
	Syntax::bullet_type, bullet)

/**
 * @brief End of a list
 */
struct ListEnd : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param ordered Whether the list was ordered or not
	 */
	[[nodiscard]] ListEnd(bool ordered):
		ordered(ordered) {}

DEFTYPE(ListEnd, LIST_END,
	bool, ordered)

/**
 * @brief List entry
 */
struct ListEntry : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _content Entry's content
	 * @param _count Entry's counter
	 */
	[[nodiscard]] ListEntry(SyntaxTree&& content, std::size_t counter):
		content(std::move(content)), counter(counter) {}

DEFTYPE(ListEntry, LIST_ENTRY,
	SyntaxTree, content,
	std::size_t, counter)

/**
 * @brief A ruler (horizontal separator)
 */
struct Ruler : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _length Ruler's length
	 */
	[[nodiscard]] Ruler(std::size_t length):
		length(length) {}

DEFTYPE(Ruler, RULER,
	std::size_t, length)

/**
 * @brief A figure
 */
struct Figure : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _path Figure's path
	 * @param _name Figure's name
	 * @param _description Figure's description
	 * @param _id Figure's id
	 */
	[[nodiscard]] Figure(std::string&& path, std::string&& name, SyntaxTree&& description, std::size_t id):
		path(std::move(path)), name(std::move(name)), description(std::move(description)), id(id) {}

DEFTYPE(Figure, FIGURE,
	std::string, path,
	std::string, name,
	SyntaxTree, description,
	std::size_t, id)

/**
 * @brief Represents a code fragment
 */

using code_fragment = std::pair<std::size_t, std::string>;

} // Syntax
template<>
struct Lisp::TypeConverter<Syntax::code_fragment>
{
	[[nodiscard]] static Syntax::code_fragment to(SCM v)
	{
		return Syntax::code_fragment{
			TypeConverter<std::size_t>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0))),
			TypeConverter<std::string>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(1)))
		};
	}
	[[nodiscard]] static SCM from(const Syntax::code_fragment& code)
	{
		return scm_list_2(
			TypeConverter<std::size_t>::from(code.first),
			TypeConverter<std::string>::from(code.second)
		);
	}
};

namespace Syntax {
struct Code : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _language Fragment's language
	 * @param _name Fragment's name
	 * @param _content Fragment's content
	 * @param _style_file File containing custom style for code fragment
	 */
	[[nodiscard]] Code(std::string&& language, std::string&& name, std::string&& style_file, std::vector<code_fragment>&& content):
		language(std::move(language)), name(std::move(name)), style_file(std::move(style_file)), content(std::move(content)) {}

DEFTYPE(Code, CODE,
	std::string, language,
	std::string, name,
	std::string, style_file,
	std::vector<Syntax::code_fragment>, content)

/**
 * @brief Represents a quote
 */
struct Quote : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _quote Quote's content
	 * @param _author Quote's author(s)
	 */
	[[nodiscard]] Quote(SyntaxTree&& quote, std::string&& author):
		quote(std::move(quote)), author(std::move(author)) {}

DEFTYPE(Quote, QUOTE,
	SyntaxTree, quote,
	std::string, author)

MAKE_CENUM_Q(RefType, std::uint8_t,
	FIGURE, 0,
);
} // Syntax
template <>
struct Lisp::TypeConverter<Syntax::RefType>
{
	[[nodiscard]] static Syntax::RefType to(SCM v)
	{
		return scm_to_uint8(v);
	}
	[[nodiscard]] static SCM from(const Syntax::RefType& x)
	{
		return scm_from_uint8(x);
	}
};
namespace Syntax {

/**
 * @brief Represents an internal reference
 */
struct Reference : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _referencing Name of referenced object
	 * @param _name Custom display name
	 * @param _type Type of referenced object
	 */
	[[nodiscard]] Reference(std::string&& referencing, std::string&& name, RefType type):
		referencing(std::move(referencing)), name(std::move(name)), type(std::move(type)) {}

DEFTYPE(Reference, REFERENCE,
	std::string, referencing,
	std::string, name,
	Syntax::RefType, type)

/**
 * @brief Represents a link
 */
struct Link : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _name Link's display name
	 * @param _path Link's path
	 */
	[[nodiscard]] Link(std::string&& name, std::string&& path):
		name(std::move(name)), path(std::move(path)) {}

DEFTYPE(Link, LINK,
	std::string, name,
	std::string, path)

MAKE_CENUM_Q(TexMode, std::uint8_t,
	NORMAL, 0,
	MATH, 1,
	MATH_LINE, 2,
);
} // Syntax
template <>
struct Lisp::TypeConverter<Syntax::TexMode>
{
	[[nodiscard]] static Syntax::TexMode to(SCM v)
	{
		return scm_to_uint8(v);
	}
	[[nodiscard]] static SCM from(const Syntax::TexMode& x)
	{
		return scm_from_uint8(x);
	}
};
namespace Syntax {

/**
 * @brief Represents latex code
 */
struct Latex : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param content LaTeX Code
	 * @param filename Where to write it (if needed)
	 * @param preamble Preamble to use
	 * @param mode Latex mode
	 */
	[[nodiscard]] Latex(std::string&& content, std::string&& filename, std::string&& preamble, std::string&& prepend, std::string&& append, std::string&& font_size, TexMode mode):
		content(std::move(content)), filename(std::move(filename)), preamble(std::move(preamble)), prepend(std::move(prepend)), append(std::move(append)), font_size(std::move(font_size)), mode(mode) {}

DEFTYPE(Latex, LATEX,
	std::string, content,
	std::string, filename,
	std::string, preamble,
	std::string, prepend,
	std::string, append,
	std::string, font_size,
	Syntax::TexMode, mode)

/**
 * @brief Represents raw code fragment
 */
struct Raw : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _content Raw content
	 */
	[[nodiscard]] Raw(std::string&& content):
		content(std::move(content)) {}

DEFTYPE(Raw, RAW,
	std::string, content)

/**
 * @brief Represents inline raw code fragment
 */
struct RawInline : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _content Raw content
	 */
	[[nodiscard]] RawInline(std::string&& content):
		content(std::move(content)) {}

DEFTYPE(RawInline, RAW_INLINE,
	std::string, content)

/**
 * @brief Represents an external reference
 */
struct ExternalRef : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _desc Description
	 * @param _author Author (empty if none)
	 * @param _url Url
	 * @param _num Reference's number
	 */
	[[nodiscard]] ExternalRef(std::string&& desc, std::string&& author, std::string&& url, std::size_t num):
		desc(std::move(desc)), author(std::move(author)), url(std::move(url)), num(num) {}

DEFTYPE(ExternalRef, EXTERNAL_REF,
	std::string, desc,
	std::string, author,
	std::string, url,
	std::size_t, num)

MAKE_CENUM_Q(PresType, std::uint8_t,
	CENTER, 0,
	BOX, 1,
	LEFT_LINE, 2,
);
} // Syntax
template <>
struct Lisp::TypeConverter<Syntax::PresType>
{
	[[nodiscard]] static Syntax::PresType to(SCM v)
	{
		return scm_to_uint8(v);
	}
	[[nodiscard]] static SCM from(const Syntax::PresType& x)
	{
		return scm_from_uint8(x);
	}
};
namespace Syntax {

/**
 * @brief Represents a presentation element
 */
struct Presentation : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _content Content
	 * @param _type Presentation type
	 */
	[[nodiscard]] Presentation(SyntaxTree&& content, PresType type):
		content(std::move(content)), type(type) {}

DEFTYPE(Presentation, PRESENTATION,
	SyntaxTree, content,
	Syntax::PresType, type)

/**
 * @brief Represents an annotation
 */
struct Annotation : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param _name What to hover on
	 * @param _content Content shown when hovered
	 */
	[[nodiscard]] Annotation(SyntaxTree&& name, SyntaxTree&& content):
		name(std::move(name)), content(std::move(content)){}

DEFTYPE(Annotation, ANNOTATION,
	SyntaxTree, name,
	SyntaxTree, content)


struct CustomStylePush : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param style Style pushed
	 */
	[[nodiscard]] CustomStylePush(const CustomStyle& style) noexcept:
		style(style) {}

DEFTYPE(CustomStylePush, CUSTOM_STYLEPUSH,
	CustomStyle, style)

struct CustomStylePop : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param style Style popped
	 */
	[[nodiscard]] CustomStylePop(const CustomStyle& style) noexcept:
		style(style) {}

DEFTYPE(CustomStylePop, CUSTOM_STYLEPOP,
	CustomStyle, style)

struct CustomPresPush : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param pres Presentation pushed
	 * @param level Nest level
	 */
	[[nodiscard]] CustomPresPush(const CustomPres& pres, std::size_t level) noexcept:
		pres(pres), level(level) {}

DEFTYPE(CustomPresPush, CUSTOM_PRESPUSH,
	CustomPres, pres,
	std::size_t, level)

struct CustomPresPop : public Element
{
	/**
	 * @brief Constructor
	 *
	 * @param pres Presentation popped
	 * @param level Nest level
	 */
	[[nodiscard]] CustomPresPop(const CustomPres& pres, std::size_t level) noexcept:
		pres(pres), level(level) {}

DEFTYPE(CustomPresPop, CUSTOM_PRESPOP,
	CustomPres, pres,
	std::size_t, level)

//}}}

/**
 * @brief List of all elements
 */
using Elements = std::tuple<
	Text,
	StylePush,
	StylePop,
	Break,
	Section,
	ListBegin,
	ListEnd,
	ListEntry,
	Ruler,
	Figure,
	Code,
	Quote,
	Reference,
	Link,
	Latex,
	Raw,
	RawInline,
	ExternalRef,
	Presentation,
	Annotation,

	CustomStylePush,
	CustomStylePop,
	CustomPresPush,
	CustomPresPop
	>;

} // Syntax

// Lisp conversion
template<>
struct Lisp::TypeConverter<Syntax::Element*>
{
	[[nodiscard]] static Syntax::Element* to(SCM v)
	{
		static std::array<std::function<Syntax::Element*(SCM)>, std::tuple_size_v<Syntax::Elements>> cvs;

		[&]<std::size_t... _>(std::index_sequence<_...>)
		{
			(([&]<std::size_t i>()
			{
				using T = std::tuple_element_t<i, Syntax::Elements>;
				cvs[i] = [](SCM v)
				{
					return TypeConverter<T>::to_new(v);
				};
			}.template operator()<_>()), ...);
		}(std::make_index_sequence<std::tuple_size_v<Syntax::Elements>>{});

		// Type id
		const auto id = TypeConverter<std::uint8_t>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(0)));

		return cvs[id](v);
	}

	[[nodiscard]] static SCM from(const Syntax::Element* elem)
	{
		static std::array<std::function<SCM(const Syntax::Element*)>, std::tuple_size_v<Syntax::Elements>> cvs;

		[&]<std::size_t... _>(std::index_sequence<_...>)
		{
			(([&]<std::size_t i>()
			{
				using T = std::tuple_element_t<i, Syntax::Elements>;
				cvs[i] = [](const Syntax::Element* elem)
				{
					return TypeConverter<T>::from(*reinterpret_cast<const T*>(elem));
				};
			}.template operator()<_>()), ...);
		}(std::make_index_sequence<std::tuple_size_v<Syntax::Elements>>{});

		return cvs[elem->get_type()](elem);
	}
};

#endif // NML_SYNTAX_HPP
