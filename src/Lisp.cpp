#include "Lisp.hpp"
#include "Parser.hpp"
#include "Syntax.hpp"
#include "HTMLCompiler.hpp"
#include "TextCompiler.hpp"
#include <iostream>
#include <fstream>
#include <fmt/format.h>
#include <filesystem>
#include <ranges>
#include "LispIntegration.hpp"

struct Lisp::Closure
{
	Document& doc;
	const File& file;
	ParserData& data;
};

static void* init(void* data)
{
	return nullptr;
}

template <class T>
void deleteFn(void* ptr) { delete reinterpret_cast<T>(ptr); }

void Lisp::init(Document& doc, const File& f, ParserData& data, Parser& parser)
{
	Closure* cls = new Closure(doc, f, data);
	scm_with_guile(::init, cls);
	scm_c_define("nml-closure", scm_from_pointer(cls, +[](void* p) { std::cout << "called" << std::endl; delete reinterpret_cast<Closure*>(p); }));

	scm_c_define_gsubr("nml-var-defined", 1, 1, 0, (void*)+[](SCM name, SCM sdoc)
	{
		if (scm_is_eq(sdoc, SCM_UNDEFINED))
			sdoc = scm_variable_ref(scm_c_lookup("nml-current-doc"));
		if (!scm_is_string(name))
			return SCM_BOOL_F;

		Document& doc = *reinterpret_cast<Document*>(scm_to_pointer(sdoc));
		const std::string s = to_string(name);
		if (doc.var_get(s) == nullptr)
			return SCM_BOOL_F;

		return SCM_BOOL_T;
	});
	scm_c_define_gsubr("nml-var-get", 1, 1, 0, (void*)+[](SCM name, SCM sdoc)
	{
		if (scm_is_eq(sdoc, SCM_UNDEFINED))
			sdoc = scm_variable_ref(scm_c_lookup("nml-current-doc"));
		if (!scm_is_string(name))
			return SCM_EOL;

		Document& doc = *reinterpret_cast<Document*>(scm_to_pointer(sdoc));
		const std::string s = to_string(name);
		std::string var;
		if (Variable* varVar = doc.var_get(s); varVar == nullptr)
			return SCM_EOL;
		else
			var = varVar->to_string(doc);

		return scm_from_locale_stringn(var.data(), var.size());
	});
	scm_c_define_gsubr("nml-var-get-default", 2, 1, 0, (void*)+[](SCM name, SCM def, SCM sdoc)
	{
		if (scm_is_eq(sdoc, SCM_UNDEFINED))
			sdoc = scm_variable_ref(scm_c_lookup("nml-current-doc"));
		if (!scm_is_string(name))
			return SCM_EOL;

		Document& doc = *reinterpret_cast<Document*>(scm_to_pointer(sdoc));
		const std::string s = to_string(name);
		std::string var;
		if (Variable* varVar = doc.var_get(s); varVar == nullptr)
			return def;
		else
			var = varVar->to_string(doc);

		return scm_from_locale_stringn(var.data(), var.size());
	});
	scm_c_define_gsubr("nml-var-define", 2, 1, 0, (void*)+[](SCM name, SCM value, SCM sdoc)
	{
		if (scm_is_eq(sdoc, SCM_UNDEFINED))
			sdoc = scm_variable_ref(scm_c_lookup("nml-current-doc"));
		if (!scm_is_string(name) || !scm_is_string(value))
			return SCM_BOOL_F;

		Document& doc = *reinterpret_cast<Document*>(scm_to_pointer(sdoc));
		doc.var_insert(to_string(name), std::make_unique<TextVariable>(to_string(value)));

		return SCM_BOOL_T;
	});

	scm_c_define_gsubr("nml-doc-parse", 1, 2, 0, (void*)+[](SCM spath, SCM inheritDoc, SCM inheritData)
	{
		if (scm_is_null(spath))
			return SCM_EOL;

		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
			return SCM_EOL;

		Document* inheritDoc_ = nullptr;
		ParserData* inheritData_ = nullptr;
		if (!scm_is_eq(inheritDoc, SCM_UNDEFINED))
			inheritDoc_ = reinterpret_cast<Document*>(scm_to_pointer(inheritDoc));
		if (!scm_is_eq(inheritData, SCM_UNDEFINED))
			inheritData_ = reinterpret_cast<ParserData*>(scm_to_pointer(inheritData));
		Parser& parser = *reinterpret_cast<Parser*>(scm_to_pointer(scm_variable_ref(scm_c_lookup("nml-current-parser"))));

		std::ifstream in{path};
		if (!in.good())
		{
			std::cerr << "Error: could not open file '" + path.filename().string() + "'." << std::endl;
			return SCM_EOL;
		}
		std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
		in.close();

		const std::filesystem::path current_file_parent{std::filesystem::current_path()};

		if (path.has_parent_path())
			std::filesystem::current_path(path.parent_path());

		File f(fmt::format("nml_doc_parse({})", path.filename().string()), content, 0, 0);
		Document* doc = new Document(parser.parse(std::move(f), inheritDoc_, inheritData_).first);

		std::filesystem::current_path(current_file_parent);

		return scm_from_pointer(doc, deleteFn<decltype(doc)>);
	});
	scm_c_define_gsubr("nml-doc-compile", 2, 1, 0, (void*)+[](SCM sdoc, SCM compiler_name, SCM tex) //TODO: cxx
	{
		if (scm_is_null(sdoc) || !scm_is_string(compiler_name))
			return SCM_EOL;
		const std::string compiler = to_string(compiler_name);

		Compiler* c;
		if (compiler == "text")
			c = new TextCompiler();
		else if (compiler == "html")
			c = new HTMLCompiler();
		else
		{
			std::cerr << "Unknown compiler: '" << compiler << "'." << std::endl;
			return SCM_EOL;
		}
		Document& doc = *reinterpret_cast<Document*>(scm_to_pointer(sdoc));

		CompilerOptions opts;
		opts.tex_dir = "tex";
		if (scm_is_eq(tex, SCM_UNDEFINED) || scm_is_null(tex))
		{
			opts.tex_enabled = false;
		}
		else if (!scm_is_string(tex))
		{
			delete c;
			return SCM_EOL;
		}
		else
		{
			opts.tex_enabled = true;
			opts.tex_dir = to_string(tex);
		}

		const std::string content = c->compile(doc, opts);

		delete c;

		return scm_from_locale_stringn(content.data(), content.size());
	});

	scm_c_define_gsubr("nml-num-roman", 2, 0, 0, (void*)+[](SCM number, SCM roman)
	{
		if (scm_is_null(number) || scm_is_null(roman) || !scm_is_unsigned_integer(number, 0, 3999))
			return SCM_EOL;

		if (scm_ilength(roman) != 13)
			return scm_from_locale_string("<Error: roman array is invalid, must contain 13 strings>");

		static constexpr auto roman_dec = make_array<std::size_t>(1000ul, 900ul, 500ul, 400ul, 100ul, 90ul, 50ul, 40ul, 10ul, 9ul, 5ul, 4ul, 1ul);

		std::string content;
		std::size_t n{scm_to_unsigned_integer(number, 0, 3999)};
		std::size_t i{0};

		while(n)
		{
			while(n / roman_dec[i])
			{
				content.append(scm_to_locale_string(scm_list_ref(roman, scm_from_unsigned_integer(i))));
				n -= roman_dec[i];
			}
			i++;
		}

		return scm_from_locale_stringn(content.data(), content.size());
	});


	// {{{ HTML Util
	scm_c_define_gsubr("nml-html-get-anchor", 1, 0, 0, (void*)+[](SCM string)
	{
		if (scm_is_null(string) || !scm_is_string(string))
			return SCM_EOL;

		return to_scm(HTMLCompiler::get_anchor(to_string(string)));
	});
	scm_c_define_gsubr("nml-html-format", 1, 0, 0, (void*)+[](SCM string)
	{
		if (scm_is_null(string) || !scm_is_string(string))
			return SCM_EOL;

		return to_scm(HTMLCompiler::format(to_string(string)));
	});
	// }}}

	//{{{ FS Utils
	scm_c_define_gsubr("nml-fs-path", 1, 0, 0, (void*)+[](SCM pathname)
	{
		if (scm_is_null(pathname) || !scm_is_string(pathname))
			return SCM_EOL;
		
		std::filesystem::path* ppath = new std::filesystem::path(to_string(pathname));

		return scm_from_pointer(ppath, deleteFn<decltype(ppath)>);
	});
	scm_c_define_gsubr("nml-fs-exists", 1, 0, 0, (void*)+[](SCM spath)
	{
		if (scm_is_null(spath))
			return SCM_EOL;
		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		
		return scm_from_bool(std::filesystem::exists(path));
	});
	scm_c_define_gsubr("nml-fs-is-file", 1, 0, 0, (void*)+[](SCM spath)
	{
		if (scm_is_null(spath))
			return SCM_EOL;
		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		
		return scm_from_bool(std::filesystem::is_regular_file(path));
	});
	scm_c_define_gsubr("nml-fs-is-dir", 1, 0, 0, (void*)+[](SCM spath)
	{
		if (scm_is_null(spath))
			return SCM_EOL;
		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		
		return scm_from_bool(std::filesystem::is_directory(path));
	});
	scm_c_define_gsubr("nml-fs-filename", 1, 0, 0, (void*)+[](SCM spath)
	{
		if (scm_is_null(spath))
			return SCM_EOL;
		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		
		return to_scm(path.filename().string());
	});
	scm_c_define_gsubr("nml-fs-fullname", 1, 0, 0, (void*)+[](SCM spath)
	{
		if (scm_is_null(spath))
			return SCM_EOL;
		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		
		return to_scm((path.root_path() / path.relative_path()).string());
	});
	scm_c_define_gsubr("nml-fs-map", 2, 0, 0, (void*)+[](SCM proc, SCM spath)
	{
		if (scm_is_null(spath) || scm_is_null(proc))
			return SCM_EOL;
		const std::filesystem::path& path = *reinterpret_cast<const std::filesystem::path*>(scm_to_pointer(spath));
		if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
			return SCM_EOL;

		SCM list = SCM_EOL;
		for (auto const& dir_ent : std::filesystem::directory_iterator{path})
		{
			SCM l1 = scm_list_1(scm_call_1(proc, scm_from_pointer(new std::filesystem::path(dir_ent.path()), deleteFn<std::filesystem::path*>)));
			if (!scm_is_null(list))
				list = scm_reverse_x(list, l1);
			else
				std::swap(list, l1);
		}
		
		return list;
	});
	
	//}}}

	//{{{ String utils
	scm_c_define_gsubr("string-tail", 2, 0, 0, (void*)+[](SCM string, SCM start)
	{
		return scm_substring(string, to_scm(0), start);
	});
	scm_c_define_gsubr("string-ends-with", 2, 0, 0, (void*)+[](SCM string, SCM substring)
	{
		if (!scm_is_string(string) || !scm_is_string(substring))
			return SCM_EOL;

		const std::size_t sslen = scm_to_uint64(scm_string_length(substring));
		const std::size_t slen = scm_to_uint64(scm_string_length(string));
		if (sslen > slen)
			return SCM_BOOL_F;
		for (std::size_t i = 0; i < sslen; ++i)
		{
			const SCM sc = scm_c_string_ref(string, slen-sslen+i);
			const SCM ssc = scm_c_string_ref(substring, i);

			if (!scm_is_eq(sc, ssc))
				return SCM_BOOL_F;
		}

		return SCM_BOOL_T;
	});
	scm_c_define_gsubr("string-starts-with", 2, 0, 0, (void*)+[](SCM string, SCM substring)
	{
		if (!scm_is_string(string) || !scm_is_string(substring))
			return SCM_EOL;

		const std::size_t sslen = scm_to_uint64(scm_string_length(substring));
		const std::size_t slen = scm_to_uint64(scm_string_length(string));
		if (sslen > slen)
			return SCM_BOOL_F;
		for (std::size_t i = 0; i < sslen; ++i)
		{
			const SCM sc = scm_c_string_ref(string, i);
			const SCM ssc = scm_c_string_ref(substring, i);

			if (!scm_is_eq(sc, ssc))
				return SCM_BOOL_F;
		}

		return SCM_BOOL_T;
	});
	//}}}

	//{{{ Custom Types
	// Custom types getters/setters
	[]<std::size_t... i>(std::index_sequence<i...>)
	{
		((TypeConverter<std::tuple_element_t<i, Syntax::Elements>>::init()),...);
	}(std::make_index_sequence<std::tuple_size_v<Syntax::Elements>>{});

	// Get type's name
	scm_c_define_gsubr("nmlo-type-name", 1, 0, 0, (void*)+[](SCM elem)
	{
		if (scm_list_p(elem) != SCM_BOOL_T)
			return SCM_EOL;

		SCM type = scm_list_ref(elem, TypeConverter<std::size_t>::from(0));
		return scm_from_locale_string(Syntax::getTypeName(scm_to_uint8(type)).data());
	});

	// Custom methods for custom types
	[]<std::size_t... i>(std::index_sequence<i...>)
	{
		(([]
		{
			using Type = typename std::tuple_element_t<i, Syntax::Elements>;
			using Methods = LispMethods<Type>;

			// Methods
			for (const auto j : std::ranges::iota_view{0uz, Methods::methods.size()})
			{
				scm_c_define_gsubr(
					("nmlo-"s).append(Type::get_name()).append("-").append(Methods::methods[j].name).c_str(),
					Methods::methods[j].params[0], Methods::methods[j].params[1], Methods::methods[j].params[2],
					Methods::methods[j].fn);
			}
		}()), ...);
	}(std::make_index_sequence<std::tuple_size_v<Syntax::Elements>>{});
	//}}}
}

void Lisp::eval(const std::string& s, Document& doc, ParserData& data, Parser& parser)
{
	scm_c_define("nml-current-doc", scm_from_pointer(&doc, NULL));
	scm_c_define("nml-current-data", scm_from_pointer(&data, NULL));
	scm_c_define("nml-current-parser", scm_from_pointer(&parser, NULL));

	scm_c_eval_string(s.c_str());
}

std::string Lisp::eval_r(const std::string& s, Document& doc, ParserData& data, Parser& parser)
{
	scm_c_define("nml-current-doc", scm_from_pointer(&doc, NULL));
	scm_c_define("nml-current-data", scm_from_pointer(&data, NULL));
	scm_c_define("nml-current-parser", scm_from_pointer(&parser, NULL));

	SCM r = scm_c_eval_string(s.c_str());
	return to_string(r);
}

[[nodiscard]] std::string Lisp::to_string(SCM x)
{
	char* buf;
	if (scm_is_eq(x, SCM_UNDEFINED))
		return "<Undefined>";
	else if (scm_is_null(x))
		return "<Nil>";
	else if (scm_is_string(x))
		buf = scm_to_locale_string(x);
	else if (scm_is_number(x))
		buf = scm_to_locale_string(scm_number_to_string(x, scm_from_unsigned_integer(10)));
	else
		return "<Error : Invalid return type>";

	std::string s{buf};
	free(buf);
	return s;
}

std::optional<Lisp::Proc> Lisp::get_proc(const std::string& name)
{
	SCM proc = scm_variable_ref(scm_c_lookup(name.c_str()));
	/* TODO: Error handling */

	if (scm_is_null(proc) || !scm_procedure_p(proc))
		return std::optional<Proc>();

	return std::optional<Proc>(Proc(std::move(proc)));
}

#include <fmt/format.h>

bool Lisp::symbol_exists(const std::string& name)
{
	return scm_is_true_assume_not_nil(scm_c_eval_string(fmt::format("(defined? '{})", name).c_str()));
}
