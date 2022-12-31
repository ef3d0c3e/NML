#include "Util.hpp"
#include "sha1.hpp"
#include "Syntax.hpp"

#include <filesystem>
#include <set>
#include <fstream>
#include <fmt/format.h>

using namespace std::literals;

bool Colors::enabled = true;
const std::string_view Colors::reset = "\033[0m"sv;
const std::string_view Colors::bold = "\033[1m"sv;
const std::string_view Colors::italic = "\033[3m"sv;
const std::string_view Colors::underline = "\033[4m"sv;
const std::string_view Colors::strike = "\033[9m"sv;
const std::string_view Colors::red = "\033[31m"sv;
const std::string_view Colors::green = "\033[32m"sv;
const std::string_view Colors::yellow = "\033[33m"sv;
const std::string_view Colors::blue = "\033[34m"sv;
const std::string_view Colors::magenta = "\033[35m"sv;
const std::string_view Colors::cyan = "\033[36m"sv;

Error::Error(const std::string& msg, const std::source_location& loc)
{
	m_msg = fmt::format("{}({}:{}) `{}` {}", loc.file_name(), loc.line(), loc.column(), loc.function_name(), msg);
}

std::string Error::what() const throw()
{
	return m_msg;
}

[[nodiscard]] std::string sha1(const std::string& s)
{
	SHA1 c;
	c.update(s);
	
	return c.final();
}

static std::set<std::string> tex_render_cache; // Already rendered files
[[nodiscard]] std::pair<std::string, std::string> Tex(const Document& doc, const std::string_view& path, const Syntax::Latex& tex)
{
	if (!std::filesystem::exists(path)) [[unlikely]]
		throw Error(fmt::format("Unable to render LaTeX : Directory '{}' could not be found", path));
	if (!std::filesystem::is_directory(path)) [[unlikely]]
		throw Error(fmt::format("Unable to render LaTeX : Path '{}' is not a directory", path));

	const std::filesystem::path dir{path};
	const std::string filename = tex.filename + ".svg"s;

	// Populate
	if (tex_render_cache.empty())
	{
		for (const auto& ent : std::filesystem::directory_iterator{dir})
		{
			if (std::filesystem::is_regular_file(ent))
				tex_render_cache.insert(ent.path().string());
		}
	}

	// Already rendered
	if (tex_render_cache.find((dir/filename).string()) != tex_render_cache.end())
	{
		std::ifstream in(dir/filename);
		if (!in.good())
			throw Error(fmt::format("Could not open svg file `{}` for `{}`", (dir/filename).string(), tex.content));
		std::string content((std::istreambuf_iterator(in)), (std::istreambuf_iterator<char>()));
		in.close();

		return {content, filename};
	}


	fmt::print(" - Processing LaTeX: \"{}\"\n", tex.content);
	{
		if (std::filesystem::exists("__temp_preamble"))
			throw Error("`__temp_preamble` already exists");
		std::ofstream out("__temp_preamble");
		if (!out.good())
			throw Error("Cannot open file `__temp_preamble`");

		out.write(tex.preamble.data(), tex.preamble.size());
		out.close();
	}

	{
		if (std::filesystem::exists("__temp_content"))
			throw Error("`__temp_content` already exists");
		std::ofstream out("__temp_content");
		if (!out.good())
			throw Error("Cannot open file `__temp_content`");

		if (tex.mode == Syntax::TexMode::MATH)
			out << tex.prepend << "$\\displaystyle " << tex.content << "$" << tex.append;
		else if (tex.mode == Syntax::TexMode::MATH_LINE)
			out << tex.prepend << "$$" << tex.content << "$$" << tex.append;
		else if (tex.mode == Syntax::TexMode::NORMAL)
			out << tex.prepend << tex.content << tex.append;
		out.close();
	}

	system(fmt::format("output=$(cat __temp_content | latex2svg --preamble __temp_preamble --fontsize {}) && echo \"$output\" > {}", tex.font_size, (dir/filename).string()).c_str());

	{
		std::error_code ec;
		std::filesystem::remove("__temp_preamble", ec);
		if (ec.value() != 0)
			throw Error(fmt::format("Cannot delete `__temp_preamble` : {}", ec.message()));
	}

	{
		std::error_code ec;
		std::filesystem::remove("__temp_content", ec);
		if (ec.value() != 0)
			throw Error(fmt::format("Cannot delete `__temp_content` : {}", ec.message()));
	}

	std::ifstream in(dir/filename);
	if (!in.good())
		throw Error(fmt::format("Could not open svg file `{}` for `{}`", (dir/filename).string(), tex.content));
	std::string content((std::istreambuf_iterator(in)), (std::istreambuf_iterator<char>()));
	in.close();

	return {content, filename};
}
