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

#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>

#include "nml/Syntax.hpp"
#include "nml/Parser.hpp"
#include "nml/Util.hpp"
#include "nml/Benchmark.hpp"

#include "nml/TextCompiler.hpp"
#include "nml/HTMLCompiler.hpp"



int main(int argc, char* argv[])
{
	std::setlocale(LC_ALL, "en_US.UTF-8");

	cxxopts::Options opts("NML", "NML is not a markup language");

	std::string tex_dir, cache_dir, in_file, out_file, compiler;
	std::vector<std::string> printing;
	bool cache;
	opts.add_options()
		("h,help",    "Displays help", cxxopts::value<bool>()->default_value("false"))
		("v,version", "Displays version", cxxopts::value<bool>()->default_value("false"))
		("t,tex-directory", "Sets tex output directory", cxxopts::value<std::string>(tex_dir)->default_value("tex"))
		("no-tex", "Disables tex processing", cxxopts::value<bool>()->default_value("false"))
		("cache-dir", "Sets the cache directory", cxxopts::value<std::string>(cache_dir)->default_value("cache"))
		("cache", "Enables caching", cxxopts::value<bool>(cache)->default_value("false"))
		("cxx", "Enables cxxabi processing (Requires cache)", cxxopts::value<bool>()->default_value("false"))
		("i,input", "Sets input file", cxxopts::value<std::string>(in_file))
		("o,output", "Sets output file", cxxopts::value<std::string>(out_file))
		("c,compiler", "Sets compiler", cxxopts::value<std::string>(compiler)->default_value("text"))
		("p,print", "Prints informations", cxxopts::value<std::vector<std::string>>(printing))
		("b,benchmark", "Prints benchmark result", cxxopts::value<bool>()->default_value("false"))
		("no-colors", "Disables colors in messages", cxxopts::value<bool>()->default_value("false"));

	decltype(opts.parse(argc, argv)) result;
	try
	{
		result = opts.parse(argc, argv);
	}
	catch (cxxopts::exceptions::exception& e)
	{
		std::cerr << e.what() << '\n'
			<< "Try '" << argv[0] << "' --help for more information."
			<< std::endl;
		std::exit(EXIT_FAILURE);
	}

	if (result.count("help"))
	{
		std::cout << opts.help() << std::endl;
		std::exit(EXIT_SUCCESS);
	}

	if (result.count("version"))
	{
		std::cout << "NML v0.32\n"
			<< "License: GNU Affero General Public License version 3 or later (AGPLv3+)\n"
			<< "see <https://www.gnu.org/licenses/agpl-3.0.en.html>\n"
			<< "This is free software: you are free to change and redistribute it.\n"
			<< "There is NO WARRANTY, to the extent permitted by law.\n"
			<< "\n"
			<< "Author(s):\n"
			<< " - ef3d0c3e <ef3d0c3e@pundalik.org>\n"
			<< "\n"
			<< "Third party libraries license:\n"
			<< " - guile: https://www.gnu.org/software/guile/manual/html_node/Guile-License.html"
			<< " - fmt https://github.com/fmtlib/fmt/blob/master/LICENSE.rst\n"
			<< " - cxxopts: https://github.com/jarro2783/cxxopts/blob/master/LICENSE\n"
			<< "Libraries for nml-test:\n"
			<< " - Catch2: https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt\n"
			<< " - utfcpp: https://github.com/nemtrif/utfcpp/blob/master/LICENSE\n";

		std::exit(EXIT_SUCCESS);
	}

	if (!result.count("input"))
	{
		std::cerr << "You must specify an input file." << std::endl;
		std::exit(EXIT_FAILURE);
	}

	const std::filesystem::path out_file_path{std::filesystem::current_path() / out_file};
	const std::filesystem::path in_file_path{std::filesystem::current_path() / in_file};
	if (in_file_path.has_parent_path())
		std::filesystem::current_path(in_file_path.parent_path());
	else
	{
		std::cerr << "Unable to set working directory" << std::endl;
		std::exit(EXIT_FAILURE);
	}
	
	// Parse
	try
	{
		Benchmarker.push("Reading input");
		std::ifstream in(in_file_path.filename());
		if (!in.good())
		{
			std::cerr << "Error: could not open file '" + in_file_path.filename().string() + "'." << std::endl;
			return EXIT_FAILURE;
		}
		std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
		in.close();
		Benchmarker.pop(); // Reading input


		Benchmarker.push("Fetching compiler");

		// Get options
		std::unique_ptr<std::ostream> stream;
		if (result.count("output"))
		{
			stream = std::unique_ptr<std::ostream>(new std::ofstream(out_file_path, std::ios::binary));
			if (!stream->good())
			{
				std::cerr << "Unable to open output file: " << out_file_path << std::endl;
				std::exit(EXIT_FAILURE);
			}
		}
		else
			stream = std::make_unique<std::ostream>(std::cout.rdbuf());
		CompilerOptions opts(*stream);

		const bool noTexDir = !result.count("no-tex") && (!std::filesystem::exists(tex_dir) || !std::filesystem::is_directory(tex_dir));
		const bool noCacheDir = !cache || (!std::filesystem::exists(cache_dir) || !std::filesystem::is_directory(cache_dir));
		if (result.count("cxx") && noCacheDir) [[unlikely]]
		{
			if (!cache)
				std::cerr << "Cache must be enabled for CXX processing, see `--help` for more information" << std::endl;
			else
				std::cerr << "Cache directory must be set to an existing directory for CXX processing, see `--help` for more information" << std::endl;
			std::exit(EXIT_FAILURE);
		}

		// Print warning if caching is set but no dir is found
		// TODO: Do the same thing for latex
		if (cache && noCacheDir)
			std::cout << "Warning: Caching is enabled but no cache dir is set!\nYou can set the caching directory caching by running with `--cache-dir=PATH`" << std::endl;
		
		opts.tex_enabled = !noTexDir;
		opts.tex_dir = tex_dir;
		opts.cache_enabled = !noCacheDir;
		opts.cache_dir = noCacheDir ? "" : cache_dir;
		opts.cxx_enabled = result.count("cxx") != 0;

		// Get compiler
		Compiler* c;
		if (compiler == "text")
			c = new TextCompiler(std::move(opts));
		else if (compiler == "html")
			c = new HTMLCompiler(std::move(opts));
		else
		{
			std::cerr << "Unknown compiler: '" << compiler << "'." << std::endl;
			std::exit(EXIT_FAILURE);
		}
		
		Benchmarker.pop(); // Fetching compiler


		Benchmarker.push("Parsing");
		Parser p(c);
		auto doc = p.parse(File(in_file_path.filename().string(), content, 0, 0)).first;
		Benchmarker.pop(); // Parsing

		Benchmarker.push("Compiling");
		c->compile(doc);
		delete c;
		Benchmarker.pop(); // Compiling

		Benchmarker.push("Printing");
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
		Benchmarker.pop(); // Printing

	}
	catch (Error& e)
	{
		std::cerr << e.what() << std::endl;
		std::exit(EXIT_FAILURE);
	}

	if (result.count("benchmark"))
		std::cout << Benchmarker.display();

	return EXIT_SUCCESS;
}
