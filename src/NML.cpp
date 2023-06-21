#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>

#include "Syntax.hpp"
#include "Parser.hpp"
#include "Util.hpp"

#include "TextCompiler.hpp"
#include "HTMLCompiler.hpp"



int main(int argc, char* argv[])
{
	std::setlocale(LC_ALL, "en_US.UTF-8");

	cxxopts::Options opts("NMl", "NML is not a markup language");

	std::string tex_dir, cxx_dir, in_file, out_file, compiler;
	std::vector<std::string> printing;
	opts.add_options()
		("h,help",    "Displays help", cxxopts::value<bool>()->default_value("false"))
		("v,version", "Displays version", cxxopts::value<bool>()->default_value("false"))
		("t,tex-directory", "Sets tex output directory", cxxopts::value<std::string>(tex_dir)->default_value("tex"))
		("no-tex", "Disables tex processing", cxxopts::value<bool>()->default_value("false"))
		("x,cxx-directory", "Sets cxxabi output directory", cxxopts::value<std::string>(cxx_dir)->default_value("cxxabi"))
		("no-cxx", "Disables cxxabi processing", cxxopts::value<bool>()->default_value("false"))
		("i,input", "Sets input file", cxxopts::value<std::string>(in_file))
		("o,output", "Sets output file", cxxopts::value<std::string>(out_file))
		("c,compiler", "Sets compiler", cxxopts::value<std::string>(compiler)->default_value("text"))
		("p,print", "Prints informations", cxxopts::value<std::vector<std::string>>(printing))
		("no-colors", "Disables colors in messages", cxxopts::value<bool>()->default_value("false"));

	decltype(opts.parse(argc, argv)) result;
	try
	{
		result = opts.parse(argc, argv);
	}
	catch (cxxopts::option_not_exists_exception& e)
	{
		std::cerr << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	catch (cxxopts::option_syntax_exception& e)
	{
		std::cerr << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	catch (cxxopts::missing_argument_exception& e)
	{
		std::cerr << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}

	if (result.count("help"))
	{
		std::cout << opts.help() << std::endl;
		std::exit(EXIT_SUCCESS);
	}

	if (result.count("version"))
	{
		std::cout << "NML v0.30\n"
			<< "License: GNU Affero General Public License version 3 (AGPLv3)\n"
			<< "see <https://www.gnu.org/licenses/agpl-3.0.en.html>\n"
			<< "This is free software: you are free to change and redistribute it.\n"
			<< "There is NO WARRANTY, to the extent permitted by law.\n"
			<< "\n"
			<< "Author(s):\n"
			<< " - ef3d0c3e <ef3d0c3e@pundalik.org>\n";

		std::exit(EXIT_SUCCESS);
	}

	if (!result.count("input"))
	{
		std::cerr << "You must specify an input file." << std::endl;
		std::exit(EXIT_FAILURE);
	}

	const std::filesystem::path in_file_path{in_file};
	if (in_file_path.has_parent_path())
		std::filesystem::current_path(in_file_path.parent_path());
	
	// Parse
	try
	{
		std::ifstream in(in_file_path.filename());
		if (!in.good())
		{
			std::cerr << "Error: could not open file '" + in_file_path.filename().string() + "'." << std::endl;
			return EXIT_FAILURE;
		}
		std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
		in.close();


		Compiler* c;
		if (compiler == "text")
			c = new TextCompiler();
		else if (compiler == "html")
			c = new HTMLCompiler();
		else
		{
			std::cerr << "Unknown compiler: '" << compiler << "'." << std::endl;
			std::exit(EXIT_FAILURE);
		}
		
		const bool noTexDir = !result.count("no-tex") && (!std::filesystem::exists(tex_dir) || !std::filesystem::is_directory(tex_dir));
		const bool noCxxDir = !result.count("no-cxx") && (!std::filesystem::exists(cxx_dir) || !std::filesystem::is_directory(cxx_dir));
		CompilerOptions opts = {
			.tex_enabled = !noTexDir,
			.tex_dir = tex_dir,
			.cxx_enabled = !noCxxDir,
			.cxx_dir = cxx_dir,
		};


		Parser p(c);
		auto doc = p.parse(File(in_file_path.filename().string(), content, 0, 0)).first;

		if (!result.count("output"))
			std::cout << c->compile(doc, opts);
		else
		{
			std::ofstream out(out_file);
			if (!out.good())
			{
				std::cerr << "Unable to open output file" << std::endl;
				std::exit(EXIT_FAILURE);
			}
			const std::string res = c->compile(doc, opts);
			out.write(res.data(), res.size());
			out.close();
		}

		for (const std::string& arg : printing)
		{
			if (arg == "vars")
			{
				doc.var_for_each([&](const std::string& name, Variable* var)
				{
					std::function<void(const std::string&, const Variable*, std::size_t)> var_print = [&](const std::string& name, const Variable* var, std::size_t d)
					{
						for (std::size_t i = 0;  i < d+1; ++i)
							std::cout << " ";
						if (d != 0)
							std::cout << "`";
						switch (var->get_type())
						{
							case Variable::TEXT:
								std::cout << "- TEXT'" << name << "' : `" << var->to_string(doc) << "`\n";
								break;
							case Variable::PATH:
								std::cout << "- PATH'" << name << "' : `" << var->to_string(doc) << "`\n";
								break;
							case Variable::PROXY:
								const std::string& pname = reinterpret_cast<const ProxyVariable*>(var)->proxy_name();
								std::cout << "- PROXY{" << pname << "}'" << name << "' : `" << var->to_string(doc) << "`\n";
								var_print(pname, doc.var_get(pname), d+1);
								break;
						}
					};
					var_print(name, var, 0);
				});
			}
			else if (arg == "styles")
			{
				std::cout << "Custom styles:\n";
				doc.custom_styles_for_each([&](const std::string& name, const CustomStyle* style)
				{
					std::cout << " - '" << name << "' : `" << style->regex << "`\n";
				});
				std::cout << "==============\n";
			}
			else if (arg == "presentations")
			{
				std::cout << "Custom presentations:\n";
				doc.custom_pres_for_each([&](const std::string& name, const CustomPres* pres)
				{
					std::cout << " - '" << name << "' : `" << pres->regex_begin << " " << pres->regex_end << "`\n";
				});
				std::cout << "=====================\n";
			}
			else if (arg == "process")
			{
				std::cout << "Custom processes:\n";
				doc.custom_process_for_each([&](const std::string& name, const CustomProcess* process)
				{
					std::cout << " - '" << name << "' : `" << process->regex_begin << " " << process->token_end << "`\n";
				});
				std::cout << "=====================\n";
			}
			else [[unlikely]]
			{
				std::cerr << "Unknown printing argument : '" << arg << "'." << std::endl;
			}
		}

	}
	catch (Error& e)
	{
		std::cerr << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
