#include "Parser.hpp"
#include "Syntax.hpp"
#include "Compiler.hpp"
#include "Lisp.hpp"

#include <numeric>
#include <optional>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <fmt/format.h>

#include <iostream>

using namespace std::literals;

void ParserData::emplace_after(const std::string_view& regex, Match&& m)
{
	const auto it = std::find_if(matches.cbegin(), matches.cend(), [&](const Match& m) { return m.original == regex; });
	if (it == matches.cend()) [[unlikely]]
		throw Error(fmt::format("Cannot emplace_after `{}`, not found", regex));
	match_point.insert(match_point.cbegin() + std::distance(matches.cbegin(), it), 0);
	match_length.insert(match_length.cbegin() + std::distance(matches.cbegin(), it), 0);
	match_str.insert(match_str.cbegin() + std::distance(matches.cbegin(), it), "");
	matches.insert(it, m);

	Document d;
	Document f(d);
}

[[nodiscard]] std::pair<std::size_t, std::size_t> getPos(const File& f, const std::size_t pos)
{
	std::size_t start = f.content.rfind('\n', (pos == f.content.size() - 1) ? pos-1 : pos );
	if (start == std::string::npos) [[unlikely]]
		start = 0;
	else [[likely]]
		++start;

	std::size_t line_number = std::count(f.content.cbegin(), f.content.cbegin()+start, '\n'); // Line number
	std::size_t line_pos = pos - start; // Position in line


	return {line_number, line_pos};
}

/**
 * @brief Get an error message
 *
 * @param f File to print the error for
 * @param catefory Error category
 * @param msg Error message
 *	addText(env, content, )
 * @param pos Error's position
 * @param count Number of characters to highlight after pos
 * @returns Error message as a string
 */
[[nodiscard]] static std::string getErrorMessage(const File& f,
		std::string&& category, std::string&& msg,
		std::size_t pos, std::size_t count = 1)
{
	std::size_t start = f.content.rfind('\n', (pos == f.content.size() - 1) ? pos-1 : pos );
	if (start == std::string::npos) [[unlikely]]
		start = 0;
	else [[likely]]
		++start;

	const auto&& [line_number, line_pos] = getPos(f, pos);
	const std::string_view line = f.get_line(start);

	constexpr std::size_t width = 70; // Line printing width
	std::string r("\n");

	// Print filenames
	if (Colors::enabled)
		r.append(Colors::bold);
	for (const File* const other : f.stack)
		r.append(other->name).append(":\n");

	r.append(fmt::format("{}:{}:{}: ", f.name, f.line + line_number + 1, line_number == 0 ? f.pos + line_pos + 1 : line_pos + 1));
	if (Colors::enabled)
		r.append(Colors::reset).append(Colors::magenta);
	r.append(std::move(category)).append(": ");
	if (Colors::enabled)
		r.append(Colors::reset);
	r.append(std::move(msg)).append("\n");

	// Print line number
	const std::size_t line_number_width = 4 * (std::log10(line_number+1+f.line)/4 + 1);
	r.append(fmt::format("{: >{}} | ", line_number + 1 + f.line, line_number_width));

	// Truncate line
	std::string_view truncated;
	std::size_t highlight_start, highlight_count;
	{
		highlight_count = std::min(count, width);
		const std::size_t start = std::max(count+line_pos, width) - width;
		highlight_start = line_pos - std::min(start, line_pos);

		truncated = line.substr(start);

	}
	if (Colors::enabled)
		r.append(truncated.substr(0, highlight_start)).append(Colors::red).append(truncated.substr(highlight_start, highlight_count)).append(Colors::reset).append(truncated.substr(std::min(truncated.size(), highlight_start + highlight_count)));
	else
		r.append(truncated);


	// Print indicator
	r.append(fmt::format("\n{: >{}} | ", "", line_number_width));
	if (Colors::enabled)
		r.append(Colors::red);
	r.append(fmt::format("{:~>{}}\n", "^", highlight_start+1));
	if (Colors::enabled)
		r.append(Colors::reset);

	return r;
}


/**
 * @brief Closes any open list(s)
 *
 * @param doc Document
 * @param f File
 * @param data Parser data
 */
static void closeList(Document& doc, const File& f, ParserData& data)
{
	while (!data.list.empty())
	{
		doc.emplace_back<Syntax::ListEnd>(data.list.back().ordered);
		data.list.pop_back();
	}
}

/**
 * @brief Adds text and attempts to perform merging if possible
 *
 * @param doc Document
 * @param f File
 * @param data Parser data
 * @param text Text to append
 *
 * @returns Constructed text
 */
static Syntax::Text* addText(Document& doc, const File& f, ParserData& data, const std::string_view& text)
{
	std::string append;
	append.reserve(text.size());
	std::copy_if(text.cbegin(), text.cend(), std::back_inserter(append), [](char c) { return c != '\n'; });

	// Nothing to add
	if (append.empty())
	{
		if (!doc.empty() && doc.back()->get_type() == Syntax::TEXT)
			return reinterpret_cast<Syntax::Text*>(doc.back());
		else
			return nullptr;
	}

	closeList(doc, f, data);

	// Merge with previous element if it's text
	if (doc.empty()) [[unlikely]]
	{
		doc.emplace_back<Syntax::Text>(append);
	}
	else if (doc.back()->get_type() == Syntax::TEXT)
	{
		auto& elem = *reinterpret_cast<Syntax::Text*>(doc.back());
		//if (elem.style == data.style)
			elem.content.append(append);
		//else
			//doc.emplace_back<Syntax::Text>(append);
	}
	else
	{
		doc.emplace_back<Syntax::Text>(append);
	}

	return reinterpret_cast<Syntax::Text*>(doc.back());
}

/**
 * @brief Adds text and performs escaping
 * @note Call this instead of `addText()` when the regex can start in the middle of a line
 *
 * @param doc Document
 * @param f File
 * @param data Parser data
 * @param prev_pos Previous position
 * @param match_pos Position of match
 * @param insert Text to insert if match is escaped
 */
[[nodiscard]] static std::pair<bool, std::size_t> escapeAddText(Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos, const std::string& insert)
{
	if (match_pos == 0 || f.content[match_pos-1] != '\\') // Not escaped
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		return {false, match_pos};
	}

	std::size_t escape_len = 0;
	while (match_pos-1-escape_len >= 0)
	{
		if (f.content[match_pos-1-escape_len] == '\\')
			++escape_len;
		else
			break;
	}

	if (escape_len % 2) // Escaped
	{
		if (auto ptr = addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos - escape_len/2 - 1)); ptr)
		{
			//if (ptr->style == data.style)
				ptr->content.append(insert);
			//else
				//addText(doc, f, data, insert);
		}
		else
			addText(doc, f, data, insert);

		return {true, match_pos+insert.size()};
	}
	else // Not escaped
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos - escape_len/2));

		return {false, match_pos};
	}

	/*
	if (match_pos == 1 || f.content[match_pos-2] != '\\') // Escaped, add insert
	{
		if (auto ptr = addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos-1)); ptr)
		{
			if (ptr->style == data.style)
				ptr->content.append(insert);
			else
				addText(doc, f, data, insert);
		}
		else
			addText(doc, f, data, insert);


		return {true, match_pos+insert.size()};
	}
	else if (f.content[match_pos-2] == '\\') // Not escaped, add '\'
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos-1));

		return {false, match_pos};
	}
	*/
	
	__builtin_unreachable(); //TODO: use std::unreachable()
}

//{{{ String utils
[[nodiscard]] std::string_view File::get_line(std::size_t start) const noexcept
{
	std::size_t endl = content.find('\n', start);
	if (endl == std::string::npos) [[unlikely]] // Current line is the last line in file
		return content.substr(start);
	return content.substr(start, endl-start);
}

[[nodiscard]] std::pair<std::string, std::size_t> File::get_token(const std::string_view& separator, const std::string_view& escape,
		std::size_t start, std::size_t end) const noexcept
{
	while (start <= end && start < content.size())
	{
		if (content[start] != ' ' && content[start] != '\t')
			break;
		++start;
	}
	const std::string_view search_in = content.substr(start, end-start);
	std::string token;
	token.reserve(search_in.size());

	for (std::size_t pos = 0; pos < search_in.size();)
	{
		const std::string_view left = search_in.substr(pos);

		if (left.starts_with(escape)) // '\...'
		{
			if (escape.size()+separator.size() <= left.size()
				&& left.substr(escape.size()).starts_with(escape)
				&& left.substr(escape.size()*2).starts_with(separator)) // '\\sep...'
			{
				token.append(escape);
				pos += escape.size();
			}
			else if (separator.size() <= left.size() && left.substr(escape.size()).starts_with(separator)) // '\sep...'
			{
				token.append(separator);
				pos += separator.size();
			}
			else //'\...'
				token.append(escape);


			pos += escape.size();
		}
		else if (left.starts_with(separator)) // 'sep...'
		{
			return std::make_pair(token, start+pos);
		}
		else
		{
			token.append(std::string(1, left.front()));
			++pos;
		}
	}

	return std::make_pair("", std::string::npos);
}

/**
 * @brief Trims whitespaces/tabs before and after string
 *
 * @param s String to trim
 * @returns Trimmed string
 */
[[nodiscard]] static std::string_view trimIdentifier(std::string_view s)
{
	// Before
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
		s = s.substr(1);
	
	// After
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
		s = s.substr(0, s.size()-1);

	return s;
}

/**
 * @brief Splits string based on split character (can be escaped)
 * @note Returned vector is guaranted not to be empty as long as the input string is not empty
 *
 * @param s String to split
 * @param splitChar Character to use for splitting
 * @param trim Whether or not to trim the split strings
 */
[[nodiscard]] static std::vector<std::string> charSplit(const std::string_view& s, const char splitChar, bool trim = true, bool skipEmpty = false)
{
	std::vector<std::string> split;
	std::string word;
	for (std::size_t i = 0; i < s.size();)
	{
		if (s[i] == splitChar) // Add word after splitChar
		{
			if (!skipEmpty || !word.empty())
				split.push_back(std::move(word));
			word.clear();
			++i;
		}
		if (i+1 == s.size()) // Add last word
		{
			word.push_back(s[i++]);
			if (!skipEmpty || !word.empty())
				split.push_back(std::move(word));
			break;
		}
		else if (s[i] == '\\')
		{
			if (i+1 < s.size() && s[i+1] == splitChar)
			{
				word.push_back(splitChar);
				i += 2;
			}
			if (i+1 < s.size() && s[i+1] == '\\')
			{
				word.push_back('\\');
				i += 2;
			}
		}
		else
			word.push_back(s[i++]);
	}

	if (trim)
	{
		for (std::size_t i = 0; i < split.size(); ++i)
			split[i] = std::string(trimIdentifier(split[i]));
	}

	return split;
}
//}}}


[[nodiscard]] ParserData::ParserData(Parser& parser):
	parser(parser)
{
	//{{{ Lists
	matches.emplace_back("(^|\n)( |\t){1,}(\\*|\\-)"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos) -> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		// Parse list stack
		std::deque<bool> list;
		const std::size_t bullet_pos = std::min(f.content.find('-', match_pos), f.content.find('*', match_pos));
		std::size_t bullet_pos_end;
		for (bullet_pos_end = bullet_pos; bullet_pos_end < f.content.size(); ++bullet_pos_end)
		{
			if (f.content[bullet_pos_end] == '*')
				list.push_back(false);
			else if (f.content[bullet_pos_end] == '-')
				list.push_back(true);
			else
				break;
		}

		// All entries in data.list & list have the same ordering until list_index
		std::size_t list_index = 0;
		{
			auto it1 = list.cbegin();
			auto it2 = data.list.cbegin();

			while (it1 != list.cend() && it2 != data.list.cend() && *it1 == it2->ordered)
				++it1, ++it2;
			list_index = it1 - list.cbegin();
		}
		
		// Closes lists
		while (list_index < data.list.size())
		{
			doc.emplace_back<Syntax::ListEnd>(data.list.back().ordered);
			data.list.pop_back();

			if (data.list.empty()) // Clear variables
			{
				doc.var_erase("Bullet");
				doc.var_erase("BulletStyle");
				doc.var_erase("BulletCounter");
			}
		}

		// Current counter
		std::size_t bullet_counter = data.list.empty() ? 1 : data.list.back().counter;
		bool custom_counter = false;
		// Parse custom bullet counter
		if (const Variable* counterVar = doc.var_get("BulletCounter"); counterVar != nullptr)
		{
			std::string counter = counterVar->to_string(doc);
			auto [end, err] = std::from_chars(counter.data(), counter.data()+counter.size(), bullet_counter);
			if (err == std::errc{} && end == counter.data()+counter.size()) [[likely]]
			{}
			else if (err == std::errc::invalid_argument) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid List Entry", fmt::format("'Counter' `{}` is not a number", counter), bullet_pos, list.size()));
			else if (err == std::errc::result_out_of_range) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid List Entry", fmt::format("'Counter' `{}` cannot be safely represented (out of range)", counter), bullet_pos, list.size()));
			else
				throw Error(getErrorMessage(f, "Invalid List Entry", fmt::format("Could not parse 'Counter' `{}` as a number", counter), bullet_pos, list.size()));

			custom_counter = true;
			doc.var_erase("BulletCounter");
		}

		// Open lists
		for (auto it = list.cbegin() + data.list.size(); it != list.cend(); ++it)
		{
			data.list.push_back({*it, 1});
			if (!custom_counter)
				bullet_counter = 1;

			// Style
			std::string bullet = *it
				? doc.var_get_default("DefaultOrderedBullet", "1.")
				: doc.var_get_default("DefaultUnorderedBullet", "*)");
			if (Variable* bulletVar = doc.var_get("Bullet"); bulletVar != nullptr)
				bullet = bulletVar->to_string(doc);
			doc.var_erase("Bullet");
			std::string bullet_style = *it
				? doc.var_get_default("DefaultOrderedBulletStyle", "")
				: doc.var_get_default("DefaultUnorderedBulletStyle", "");
			if (Variable* bulletStyleVar = doc.var_get("BulletStyle"); bulletStyleVar != nullptr)
				bullet_style = bulletStyleVar->to_string(doc);
			doc.var_erase("BulletStyle");


			// Parse numbering
			if (*it)
			{
				if (bullet.empty()) [[unlikely]] // Normally impossible
					throw Error(getErrorMessage(f, "Invalid List Entry", "Variable `Bullet` is empty", bullet_pos, list.size()));
				
				// Attempts to parse numbering type
				static constexpr const std::array<std::pair<char, Syntax::OrderedBullet::Type>, 6> bullets{
					std::make_pair('1', Syntax::OrderedBullet::NUMBER),
					std::make_pair('a', Syntax::OrderedBullet::ALPHA),
					std::make_pair('A', Syntax::OrderedBullet::ALPHA_CAPITAL),
					std::make_pair('i', Syntax::OrderedBullet::ROMAN),
					std::make_pair('I', Syntax::OrderedBullet::ROMAN_CAPITAL),
					std::make_pair('v', Syntax::OrderedBullet::PEX)
				};

				Syntax::OrderedBullet::Type bullet_type;
				const std::size_t bullet_type_pos = std::accumulate(bullets.cbegin(), bullets.cend(),
						std::string::npos, [&bullet_type, &bullet]
						(std::size_t p, const std::pair<char, Syntax::OrderedBullet::Type>& e) mutable
				{
					if (const std::size_t pos = bullet.find(e.first); pos < p)
					{
						bullet_type = e.second;
						return pos;
					}

					return p;
				});

				if (bullet_type_pos == std::string::npos) [[unlikely]]
					throw Error(getErrorMessage(f, "Invalid List Entry", fmt::format("Unable to determine numbering type for bullet format `{}` ", bullet), bullet_pos, list.size()));

				const Syntax::ListBegin* lb = doc.emplace_back<Syntax::ListBegin>(
					std::string(bullet_style),
					bullet_type,
					std::string(bullet.substr(0, bullet_type_pos)),
					std::string(bullet.substr(bullet_type_pos+1))
				);

				// Check if representible
				if (std::string err = std::get<Syntax::OrderedBullet>(lb->bullet).is_representible(bullet_counter); !err.empty())
					throw Error(getErrorMessage(f, "Invalid List Entry", std::move(err), bullet_pos, list.size()));
			}
			else
			{
				if (bullet.empty()) [[unlikely]] // Normally impossible
					throw Error(getErrorMessage(f, "Invalid List Entry", "Variable `Bullet` is empty", bullet_pos, list.size()));

				doc.emplace_back<Syntax::ListBegin>(std::string(bullet_style), std::string(bullet));
			}
		}

		// Get spacing
		std::size_t spacing;
		for (spacing = 0; spacing+bullet_pos_end < f.content.size(); ++spacing)
			if (f.content[spacing+bullet_pos_end] != ' ' && f.content[spacing+bullet_pos_end] != '\t')
				break;

		// Get content
		std::string content;
		std::size_t i = 0;
		for (i = bullet_pos_end + spacing; i < f.content.size();)
		{
			if (f.content[i] == '\\' &&
				i+1 < f.content.size() && f.content[i+1] == '\n') // Continue
			{
				i += 2;
				while (i < f.content.size() && (f.content[i] == ' ' || f.content[i] == '\t')) // Remove spacings after continue
					++i;
			}
			else if (f.content[i] == '\\' &&
				i+2 < f.content.size() && f.content[i+1] == '\\' && f.content[i+2] == '\n') // Continue + \n
			{
				content.push_back('\n');
				i += 3;
				while (i < f.content.size() && (f.content[i] == ' ' || f.content[i] == '\t')) // Remove spacings after continue
					++i;
			}
			else if (f.content[i] == '\n') // Stop
				break;
			else
				content.push_back(f.content[i++]);
		}
		content.push_back('\n');


		if (content.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid List Entry", "List entry cannot be empty", bullet_pos, list.size()));

		File ent("[list entry]", std::string_view(content), 0, 0, f);
		Document entry = data.parser.parse(ent, &doc, &data).first;

		// Merge non elements & add listentry
		doc.merge_non_elems(entry);
		doc.emplace_back<Syntax::ListEntry>(std::move(entry.get_tree()), bullet_counter);

		data.list.back().counter = bullet_counter+1;

		return i;
	});
	//}}}
	//{{{ Sections
	// Ordered
	// NOTE: Section bullets are global, thefore should be handled during compile time
	matches.emplace_back("(^|\n)[#]{1,}((\\*){1,2}|)[ |\t]"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (f.content[match_pos] == '\n')
			++match_pos;

		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));

		const std::string_view line = f.get_line(match_pos);

		// Gets level of section
		std::size_t level = 0;
		while (level < line.size() && line[level] == '#')
			++level;

		if (level < line.size() && line[level] == '*') // Unordered
		{
			if (level+1 < line.size() && line[level+1] == '*') // Not in toc
			{
				// Get section's name
				const std::string_view section_name = trimIdentifier(line.substr(std::min(line.size(), level+2)));
				if (section_name.empty()) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Section", "Sections cannot have empty names", match_pos+level+2));

				doc.emplace_back<Syntax::Section>(static_cast<std::string>(section_name), level, false, false);
			}
			else // In toc
			{
				// Get section's name
				const std::string_view section_name = trimIdentifier(line.substr(std::min(line.size(), level+1)));
				if (section_name.empty()) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Section", "Sections cannot have empty names", match_pos+level+1));

				doc.emplace_back<Syntax::Section>(static_cast<std::string>(section_name), level, false, true);
			}
		}
		else // Ordered
		{
			// Get section's name
			const std::string_view section_name = trimIdentifier(line.substr(std::min(line.size(), level)));
			if (section_name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Section", "Sections cannot have empty names", match_pos+level));

			doc.emplace_back<Syntax::Section>(static_cast<std::string>(section_name), level, true, true);
		}

		return match_pos+line.size();
	});
	//}}}
	//{{{ Markers
	// Ruler
	matches.emplace_back("(^|\n)[=]{3,}"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);
		if (const auto it = std::find_if(line.cbegin(), line.cend(), [](char c) { return c != '='; }); it != line.cend())
			throw Error(getErrorMessage(f, "Invalid Ruler", "Line may only contain '='", match_pos + (it - line.cbegin())));

		doc.emplace_back<Syntax::Ruler>(line.size()-3);

		return match_pos+line.size();
	});
	// Figures
	matches.emplace_back("(^|\n)!\\["s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;
		
		const std::string_view line = f.get_line(match_pos);
		auto&& [name, name_end] = f.get_token("]", "\\", match_pos+2, match_pos + line.size());

		if (name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Figure", "Figures cannot have empty names", match_pos+1, 1));
		if (name_end+2 - match_pos >= line.size() || f.content[name_end+1] != '(') [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Figure", "Missing '(' + path + ')' after figure name", name_end, 1));

		if (doc.figure_exists(name)) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Figure", fmt::format("A figure named '{}' already exists", name), match_pos+2, name_end-match_pos-2));

		auto&& [path, path_end] = f.get_token(")", "\\", name_end+2, match_pos+line.size());
		if (path.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Figure", "Missing path in figure", name_end+2, 1));

		
		std::size_t description_start = path_end+1;
		while (description_start < match_pos+line.size() && (f.content[description_start] == ' ' || f.content[description_start] == '\t'))
			++description_start;

		std::string description;
		std::size_t i = 0;
		for (i = description_start; i < f.content.size();)
		{
			if (f.content[i] == '\\' &&
				i+1 < f.content.size() && f.content[i+1] == '\n') // Continue
				i += 2;
			else if (f.content[i] == '\\' &&
				i+2 < f.content.size() && f.content[i+1] == '\\' && f.content[i+2] == '\n') // Continue + \n
			{
				description.push_back('\n');
				i += 3;
			}
			else if (f.content[i] == '\n') // Stop
				break;
			else
				description.push_back(f.content[i++]);
		}
		description.push_back('\n');

		Document desc = data.parser.parse(File("[figure description]", std::string_view(description), 0, 0, f), &doc, &data).first;
		doc.emplace_back<Syntax::Figure>(std::move(path), std::move(name), std::move(desc.get_tree()));

		return i;
	});
	// Figure reference
	matches.emplace_back("§\\{"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "§{"); escaped)
			return new_pos;
		//NOTE: "§" is 2byte wide

		auto&& [ref, ref_end] = f.get_token("}", "\\", match_pos+3);
		if (ref.empty() || ref.contains('\n')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Figure Reference", "Missing terminating '}' after opening '{'", match_pos, 3));

		std::vector<std::string> args = charSplit(ref, ',', true);
		if (args.size() == 1) // Only name
		{
			if (!doc.figure_exists(args[0])) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Figure Reference", fmt::format("Trying to reference unknown figure '{}'", args[0]), match_pos, ref_end-match_pos+1));

			doc.emplace_back<Syntax::Reference>(std::move(args[0]), "", Syntax::RefType::FIGURE);
		}
		else if (args.size() == 2) // Name + custom name
		{
			if (!doc.figure_exists(args[0])) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Figure Reference", fmt::format("Trying to reference unknown figure '{}'", args[0]), match_pos, ref_end-match_pos+1));

			doc.emplace_back<Syntax::Reference>(std::move(args[0]), std::move(args[1]), Syntax::RefType::FIGURE);
		}
		else [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Figure Reference", fmt::format("Too many arguments given ({}), syntax is `§{{object name, [custom name]}}`", args.size()), match_pos, 3));

		return ref_end+1;
	});
	// Link
	matches.emplace_back("\\[(?!\\[).*\\]\\(.*\\)"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "["); escaped)
			return new_pos;

		// Note: performing extensive checks because this regex could false-positive if escaped
		auto&& [name, name_end] = f.get_token("]", "\\", match_pos+1);
		if (name_end == std::string::npos || name.contains('\n')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Link", "Missing closing ']' after opening '['", match_pos, 1));
		if (name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Link", "Link cannot have an empty name", match_pos, name_end-match_pos+1));
		if (name_end+1 == f.content.size() || f.content[name_end+1] != '(') [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Link", fmt::format("Missing '(' after '[{}]'", name), match_pos, name_end-match_pos));

		if (name_end+2 == f.content.size()) [[unlikely]] // Not possible
			throw Error(getErrorMessage(f, "Invalid Link", "Missing closing ')' after opening '('", match_pos, name_end-match_pos+1));
		
		auto&& [path, path_end] = f.get_token(")", "\\", name_end+2);
		if (path_end == std::string::npos || path.contains('\n')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Link", "Missing closing ')' after opening '('", match_pos, 1));
		if (path.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Link", "Link cannot have an empty path", match_pos, path_end-match_pos+1));
		if (f.content[path_end] != ')') [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Link", fmt::format("Missing ')' after '[{}]({}'", name, path), match_pos, path_end-match_pos));

		doc.emplace_back<Syntax::Link>(std::move(name), std::move(path));

		return path_end+1;
	});
	// Annotations
	matches.emplace_back("\\^\\{\\{"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "}}"); escaped)
			return new_pos;

		// Note: performing extensive checks because this regex could false-positive if escaped
		auto&& [anno, anno_end] = f.get_token("}}", "\\", match_pos+3);
		if (anno_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Annotation", "Missing closing '}}' after opening '{{'", match_pos, 2));
		if (anno.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Annotation", "Annotation cannot be empty", match_pos, anno_end-match_pos+2));
		anno.push_back('\n');

		std::string name = std::string(doc.var_get_default("Annotation", "[note]"));
		name.push_back('\n');

		Document doc_name = data.parser.parse(File("[annotation name]", std::string_view(name), 0, 0, f), &doc, &data).first;
		Document doc_anno = data.parser.parse(File("[annotation]", std::string_view(anno), 0, 0, f), &doc, &data).first;

		doc.emplace_back<Syntax::Annotation>(std::move(doc_name.get_tree()), std::move(doc_anno.get_tree()));

		return anno_end+2;
	});
	// External ref
	matches.emplace_back("§\\[.*\\]\\[.*\\]\\("s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "§["); escaped)
			return new_pos;

		// Note: performing extensive checks because this regex could false-positive if escaped
		auto&& [desc, desc_end] = f.get_token("]", "\\", match_pos+3);
		if (desc_end == std::string::npos || desc.contains('\n')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid External Reference", "Missing closing ']' after opening '['", match_pos, 3));
		if (desc.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid External Reference", "Description cannot be empty", match_pos, desc_end-match_pos+1));
		if (desc_end+1 == f.content.size() || f.content[desc_end+1] != '[') [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid External Reference", fmt::format("Missing '[' after '§[{}]'", desc), match_pos, desc_end-match_pos));

		if (desc_end+2 == f.content.size()) [[unlikely]] // Not possible
			throw Error(getErrorMessage(f, "Invalid External Reference", "Missing closing ']' after opening '['", match_pos, desc_end-match_pos+1));

		auto&& [author, author_end] = f.get_token("]", "\\", desc_end+2);
		if (author_end == std::string::npos || author.contains('\n')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid External Reference", "Missing closing ']' after opening '['", author_end+1, 1));
		if (author_end+1 == f.content.size() || f.content[author_end+1] != '(') [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid External Reference", fmt::format("Missing '(' after '§[{}][{}]'", desc, author), match_pos, author_end-match_pos));

		if (author_end+2 == f.content.size()) [[unlikely]] // Not possible
			throw Error(getErrorMessage(f, "Invalid External Reference", "Missing closing ')' after opening '('", match_pos, author_end-match_pos+1));

		auto&& [link, link_end] = f.get_token(")", "\\", author_end+2);
		if (link_end == std::string::npos || link.contains('\n')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid External Reference", "Missing closing ')' after opening '('", link_end+1, 1));

		doc.emplace_back<Syntax::ExternalRef>(std::move(desc), std::move(author), std::move(link));

		return link_end+1;
	});
	//}}}
	//{{{ Language
	// Definition
	matches.emplace_back("(^|\n)#\\+"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);

		std::string name(line.substr(2, line.find(' ')-2));
		if (name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Definition", "Variable has no name", match_pos, 2));
		bool is_path = false;
		bool is_proxy = false;
		bool is_call = false;
		std::string pname;
		if (name.ends_with("'"))
		{
			is_path = true;
			pname = name.substr(0, name.size()-1);
			if (pname.empty())
				throw Error(getErrorMessage(f, "Invalid Path Definition", "Variable has no name", match_pos, 2));
		}
		else if (name.ends_with("&"))
		{
			is_proxy = true;
			pname = name.substr(0, name.size()-1);
			if (pname.empty())
				throw Error(getErrorMessage(f, "Invalid Proxy Definition", "Variable has no name", match_pos, 2));
		}
		else if (name.ends_with("%"))
		{
			is_call = true;
			pname = name.substr(0, name.size()-1);
			if (pname.empty())
				throw Error(getErrorMessage(f, "Invalid Call Definition", "Variable has no name", match_pos, 2));
		}

		if (name.size()+2+1 >= line.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Definition", fmt::format("Variable '{}' has no value", name), match_pos, name.size()+2));
		if (name.contains('(') || name.contains(')')) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Definition", "Variable name cannot contain parenthesis", match_pos, name.size()+2));

		std::string value;
		std::size_t i = 0;
		for (i = match_pos + name.size() + 3; i < f.content.size();)
		{
			if (f.content[i] == '\\' &&
				i+1 < f.content.size() && f.content[i+1] == '\n') // Continue
				i += 2;
			else if (f.content[i] == '\\' &&
				i+2 < f.content.size() && f.content[i+1] == '\\' && f.content[i+2] == '\n') // Continue + \n
			{
				value.push_back('\n');
				i += 3;
			}
			else if (f.content[i] == '\n') // Stop
				break;
			else
				value.push_back(f.content[i++]);
		}

		if (value.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Definition", fmt::format("Variable '{}' has no value", name), match_pos, name.size()+2));
			
		if (is_path)
		{
			doc.var_insert(pname, std::make_unique<PathVariable>(std::filesystem::absolute(std::filesystem::path(value))));
		}
		else if (is_proxy)
		{
			
			if (Variable* var = doc.var_get(value); var == nullptr) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Proxy Definition", fmt::format("Variable '{}' does not exist!", value), match_pos+name.size()+3, value.size()));

			auto new_var = std::make_unique<ProxyVariable>(value);
			const Variable* var = new_var.get();

			std::deque<const ProxyVariable*> proxy_chain;
			while (var->get_type() == Variable::PROXY)
			{
				// Check for duplicate
				const auto it = std::find(proxy_chain.cbegin(), proxy_chain.cend(), var);
				if (it != proxy_chain.cend()) [[unlikely]]
				{
					std::string proxy{pname};
					for (const auto* v : proxy_chain)
						proxy += " -> " + v->proxy_name();
					proxy += " -> " + reinterpret_cast<const ProxyVariable*>(var)->proxy_name();
					throw Error(getErrorMessage(f, "Invalid Proxy Definition", fmt::format("Attempting to define a cyclic proxy variable! {}", proxy), match_pos, name.size()+2));
				}

				// Here perform additional check to check if it's cycling on current variable (since doc.var_get() returns the old variable)
				if (!proxy_chain.empty() && reinterpret_cast<const ProxyVariable*>(var)->proxy_name() == pname) [[unlikely]]
				{
					std::string proxy{pname};
					for (const auto* v : proxy_chain)
						proxy += " -> " + v->proxy_name();
					proxy += " -> " + reinterpret_cast<const ProxyVariable*>(var)->proxy_name();
					throw Error(getErrorMessage(f, "Invalid Proxy Definition", fmt::format("Attempting to define a cyclic proxy variable! {}", proxy), match_pos, name.size()+2));
				}

				// Next iteration
				proxy_chain.push_back(reinterpret_cast<const ProxyVariable*>(var));
				var = doc.var_get(proxy_chain.back()->proxy_name());
				if (var == nullptr) [[unlikely]]
				{
					std::string proxy{pname};
					for (const auto* v : proxy_chain)
						proxy += " -> " + v->proxy_name();
					proxy += " -> " + reinterpret_cast<const ProxyVariable*>(var)->proxy_name();
					throw Error(getErrorMessage(f, "Invalid Proxy Definition", fmt::format("Proxy chain leads to undefined variable! {} (undefined)", proxy), match_pos, name.size()+2));
				}
			}

			doc.var_insert(pname, std::move(new_var));
		}
		else if (is_call)
		{
			Variable* var = doc.var_get(value);
			if (var == nullptr) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Call Definition", fmt::format("Variable '{}' does not exist!", value), match_pos+name.size()+2, value.size()));
			doc.var_insert(pname, std::make_unique<TextVariable>(var->to_string(doc)));
		}
		else
			doc.var_insert(name, std::make_unique<TextVariable>(std::move(value)));


		return i;
	});
	// Include
	matches.emplace_back("(^|\n)#\\:Inc "s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);

		if (line.find(' ') == line.size()-1) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Include", "Include requires a filename", match_pos, 6));
		const std::string_view name(line.substr(line.find(' ')+1));
		if (name.empty()) [[unlikely]] // Should not happen
			throw Error(getErrorMessage(f, "Invalid Include", "Include requires a filename", match_pos, 6));

		const std::filesystem::path path(std::filesystem::current_path().append(name));
		std::ifstream in(path);
		if (!in.good()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Include", fmt::format("Unable to open file '{}'", name), match_pos+6, name.size()));
		std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
		in.close();

		std::filesystem::path current_path = std::filesystem::current_path();
		if (path.has_parent_path())
			std::filesystem::current_path(path.parent_path());

		File file(path.relative_path().string(), std::string_view(content), 0, 0, f);
		auto&& [inc, pdata] = data.parser.parse(file, &doc, &data);

		std::filesystem::current_path(current_path);

		for (std::size_t i = 0; i < pdata.matches.size(); ++i)
		{
			if (pdata.matches[i].original == data.matches[i].original)
				continue;

			data.matches.insert(data.matches.begin()+i, pdata.matches[i]);
		}
		inc.custom_styles_for_each([&](const std::string& name, const CustomStyle* style)
		{
			const auto it = std::find_if(data.matches.cbegin(), data.matches.cend(), [&](const Match& m) { return m.original == "="; }); // Before "="
			data.match_point.insert(data.match_point.cbegin() + std::distance(data.matches.cbegin(), it), 0);
			data.match_length.insert(data.match_length.cbegin() + std::distance(data.matches.cbegin(), it), 0);
			data.match_str.insert(data.match_str.cbegin() + std::distance(data.matches.cbegin(), it), "");
			data.customStyle.push_back(false);
		});
		inc.custom_pres_for_each([&](const std::string& name, const CustomPres* pres)
		{
			const auto it = std::find_if(data.matches.cbegin(), data.matches.cend(), [&](const Match& m) { return m.original == "\\[\\[\\|"; }) - 1; // Before "[[["
			data.match_point.insert(data.match_point.cbegin() + std::distance(data.matches.cbegin(), it), 0);
			data.match_length.insert(data.match_length.cbegin() + std::distance(data.matches.cbegin(), it), 0);
			data.match_str.insert(data.match_str.cbegin() + std::distance(data.matches.cbegin(), it), "");
			data.customPres.push_back(0);
		});
		inc.custom_process_for_each([&](const std::string& name, const CustomProcess* process)
		{
			const auto it = std::find_if(data.matches.cbegin(), data.matches.cend(), [&](const Match& m) { return m.original == "%"; }) - 1; // Before "%"
			data.match_point.insert(data.match_point.cbegin() + std::distance(data.matches.cbegin(), it), 0);
			data.match_length.insert(data.match_length.cbegin() + std::distance(data.matches.cbegin(), it), 0);
			data.match_str.insert(data.match_str.cbegin() + std::distance(data.matches.cbegin(), it), "");
		});

		// Will also merge custom types
		doc.merge(std::move(inc));

		return match_pos+line.size();
	});
	// DefStyle
	matches.emplace_back("(^|\n)#\\:DefStyle "s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);

		// Get style name
		if (line.find(' ') == line.size()-1) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefStyle", "Syntax: '#:DefStyle <Name> <Regex>'\nCustom style is missing a name!", match_pos, 10));
		const std::size_t name_pos = line.find(' ')+1;
		const std::string_view name(line.substr(name_pos, line.find(' ', name_pos)-name_pos));
		if (name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefStyle", "Syntax: '#:DefStyle <Name> <Regex>'\nCustom style name is empty!", match_pos, 10));

		// Get style regex
		if (line[name_pos+name.size()] != ' ' || name_pos+name.size()+1 >= line.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefStyle", "Syntax: '#:DefStyle <Name> <Regex>'\nCustom style is missing a regex!", match_pos, name_pos+name.size()));
		std::string regex(line.substr(name_pos+name.size()+1));
		if (regex.empty()) [[unlikely]] // Shouldn't happen
			throw Error(getErrorMessage(f, "Invalid DefStyle", "Syntax: '#:DefStyle <Name> <Regex>'\nCustom style regex is empty.", match_pos, name_pos+name.size()));

		auto begin = Lisp::get_proc(fmt::format("{}-begin", name));
		if (!begin.has_value())
			throw Error(getErrorMessage(f, "Invalid DefStyle", fmt::format("Missing guile procedure: '{}-begin'", name), match_pos, line.size()));
		auto end = Lisp::get_proc(fmt::format("{}-end", name));
		if (!end.has_value())
			throw Error(getErrorMessage(f, "Invalid DefStyle", fmt::format("Missing guile procedure: '{}-end'", name), match_pos, line.size()));

		CustomStyle* style;
		if (Lisp::symbol_exists(fmt::format("{}-apply", name)))
			style = new CustomStyle(name, data.customStyle.size(), std::move(regex), std::move(begin.value()), std::move(end.value()), std::move(Lisp::get_proc(fmt::format("{}-apply", name)).value()));
		else
			style = new CustomStyle(name, data.customStyle.size(), std::move(regex), std::move(begin.value()), std::move(end.value()));

		
		data.customStyle.push_back(false);
		doc.types_add(name, reinterpret_cast<const CustomType*>(style));

		// Add after verbatim
		data.emplace_after("=", Match(style->regex, [style](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
				-> std::size_t
		{
			if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, data.current_match_str); escaped)
				return new_pos;

			data.customStyle[style->index] = data.customStyle[style->index] ^ true;
			if (data.customStyle[style->index])
			{
				doc.emplace_back<Syntax::CustomStylePush>(*style);
			}
			else // Closing
			{
				// Apply if needed
				if (style->apply.has_value())
				{
					std::deque<Syntax::Element*> backtrack; // Elements to pass to apply
					auto it = doc.get_tree().rbegin();
					while (it != doc.get_tree().rend())
					{
						if (it->get()->get_type() == Syntax::CUSTOM_STYLEPUSH) // Stop here
						{
							const auto& push = *reinterpret_cast<const Syntax::CustomStylePush*>(it->get());
							if (push.style.index == style->index)
								break;
						}

						backtrack.push_front(it->get());
						++it;
					}

					// Convert to SCM
					SCM list = Lisp::TypeConverter<decltype(backtrack)>::from(backtrack);
					
					// Delete elements
					for (std::size_t i{0}; i < backtrack.size(); ++i)
						doc.pop_back();

					auto result = style->apply.value().call_cv<decltype(backtrack)>(list);
					for (Syntax::Element* elem : result)
						doc.push_back(elem);
				}

				doc.emplace_back<Syntax::CustomStylePop>(*style);
			}
			return match_pos+data.current_match_length;
		}));

		return match_pos+line.size();
	});
	// DefPresentation
	matches.emplace_back("(^|\n)#\\:DefPresentation "s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);

		
		// Get presentation name
		if (line.find(' ') == line.size()-1) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefPresentation", "Syntax: '#:DefPresentation <Name> <RegexBegin> <RegexEnd>'\nCustom presentation is missing a name!", match_pos, 17));
		const std::size_t name_pos = line.find(' ')+1;
		const std::string_view name(line.substr(name_pos, line.find(' ', name_pos)-name_pos));
		if (name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefPresentation", "Syntax: '#:DefPresentation <Name> <RegexBegin> <RegexEnd>'\nCustom presentation name is empty!", match_pos, 17));


		// Get regex_begin
		if (line[name_pos+name.size()] != ' ' || name_pos+name.size()+1 >= line.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefPresentation", "Syntax: '#:DefPresentation <Name> <RegexBegin> <RegexEnd>'\nCustom presentation is missing a begin regex!", match_pos, name_pos+name.size()));
		const std::size_t rb_pos = line.find(' ', name_pos+name.size())+1;
		std::string regex_begin(line.substr(rb_pos, line.find(' ', rb_pos)-rb_pos));
		if (regex_begin.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefPresentation", "Syntax: '#:DefPresentation <Name> <RegexBegin> <RegexEnd>'\nCustom presentation begin regex is empty.", match_pos, name_pos+name.size()));

		// Get regex_end
		if (line[rb_pos+regex_begin.size()] != ' ' || rb_pos+regex_begin.size()+1 >= line.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefPresentation", "Syntax: '#:DefPresentation <Name> <RegexBegin> <RegexEnd>'\nCustom presentation is missing an end regex!", match_pos, rb_pos+regex_begin.size()));
		std::string regex_end(line.substr(rb_pos+regex_begin.size()+1));
		if (regex_end.empty()) [[unlikely]] // Shouldn't happen
			throw Error(getErrorMessage(f, "Invalid DefPresentation", "Syntax: '#:DefPresentation <Name> <RegexBegin> <RegexEnd>'\nCustom presentation end regex is empty.", match_pos, name_pos+name.size()));

		auto begin = Lisp::get_proc(fmt::format("{}-begin", name));
		if (!begin.has_value())
			throw Error(getErrorMessage(f, "Invalid DefPresentation", fmt::format("Missing guile procedure: '{}-begin'", name), match_pos, line.size()));
		auto end = Lisp::get_proc(fmt::format("{}-end", name));
		if (!end.has_value())
			throw Error(getErrorMessage(f, "Invalid DefPresentation", fmt::format("Missing guile procedure: '{}-end'", name), match_pos, line.size()));
		
		const CustomPres* pres = new CustomPres(name, data.customPres.size(), std::move(regex_begin), std::move(regex_end), std::move(begin.value()), std::move(end.value()));
		data.customPres.push_back(0);
		doc.types_add(name, reinterpret_cast<const CustomPres*>(pres));

		// Add after left line
		data.emplace_after("\\[\\[\\|", Match(pres->regex_begin, [pres](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
				-> std::size_t
		{
			if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, data.current_match_str); escaped)
				return new_pos;

			++data.customPres[pres->index];
			doc.emplace_back<Syntax::CustomPresPush>(*pres, data.customPres[pres->index]-1);
			return match_pos+data.current_match_length;
		}));
		data.emplace_after("\\[\\[\\|", Match(pres->regex_end, [pres](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
				-> std::size_t
		{
			if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, data.current_match_str); escaped)
				return new_pos;

			if (data.customPres[pres->index] == 0) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Custom Presentation", "Attempting to close presentation without opening it first!", match_pos, data.current_match_length));

			--data.customPres[pres->index];
			doc.emplace_back<Syntax::CustomPresPop>(*pres, data.customPres[pres->index]);
			return match_pos+data.current_match_length;
		}));

		return match_pos+line.size();
	});
	// DefProcess
	matches.emplace_back("(^|\n)#\\:DefProcess "s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);

		// Get process name
		if (line.find(' ') == line.size()-1) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefProcess", "Syntax: '#:DefProcess <Name> <RegexBegin> <RegexEnd>'\nCustom process is missing a name!", match_pos, 17));
		const std::size_t name_pos = line.find(' ')+1;
		const std::string_view name(line.substr(name_pos, line.find(' ', name_pos)-name_pos));
		if (name.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefProcess", "Syntax: '#:DefProcess <Name> <RegexBegin> <RegexEnd>'\nCustom process name is empty!", match_pos, 17));


		// Get regex_begin
		if (line[name_pos+name.size()] != ' ' || name_pos+name.size()+1 >= line.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefProcess", "Syntax: '#:DefProcess <Name> <RegexBegin> <RegexEnd>'\nCustom process is missing a begin regex!", match_pos, name_pos+name.size()));
		const std::size_t rb_pos = line.find(' ', name_pos+name.size())+1;
		std::string regex_begin(line.substr(rb_pos, line.find(' ', rb_pos)-rb_pos));
		if (regex_begin.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefProcess", "Syntax: '#:DefProcess <Name> <RegexBegin> <RegexEnd>'\nCustom process begin regex is empty.", match_pos, name_pos+name.size()));

		// Get regex_end
		if (line[rb_pos+regex_begin.size()] != ' ' || rb_pos+regex_begin.size()+1 >= line.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid DefProcess", "Syntax: '#:DefProcess <Name> <RegexBegin> <RegexEnd>'\nCustom process is missing an end regex!", match_pos, rb_pos+regex_begin.size()));
		std::string regex_end(line.substr(rb_pos+regex_begin.size()+1));
		if (regex_end.empty()) [[unlikely]] // Shouldn't happen
			throw Error(getErrorMessage(f, "Invalid DefProcess", "Syntax: '#:DefProcess <Name> <RegexBegin> <RegexEnd>'\nCustom process end regex is empty.", match_pos, name_pos+name.size()));

		auto apply = Lisp::get_proc(fmt::format("{}-apply", name));
		if (!apply.has_value())
			throw Error(getErrorMessage(f, "Invalid DefProcess", fmt::format("Missing guile procedure: '{}-apply'", name), match_pos, line.size()));
		
		const CustomProcess* process = new CustomProcess(name, data.customPres.size(), std::move(regex_begin), std::move(regex_end), std::move(apply.value()));
		doc.types_add(name, process);

		// Add after call
		data.emplace_after("%", Match(process->regex_begin, [process](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
				-> std::size_t
		{
			if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, data.current_match_str); escaped)
				return new_pos;

			auto&& [content, content_end] = f.get_token(process->token_end, "\\", match_pos+data.current_match_length);
			if (content_end == std::string::npos) [[unlikely]]
				throw Error(getErrorMessage(f, fmt::format("Invalid Custom Process ({})", process->type_name), fmt::format("Missing terminating '{}' after initial '{}'", process->token_end, process->regex_begin), match_pos, data.current_match_length));
			if (content.empty()) [[unlikely]]
				throw Error(getErrorMessage(f, fmt::format("Invalid Custom Process ({})", process->type_name), "Empty content", match_pos, content_end-match_pos+data.current_match_length));

			// Parse tokens
			const std::string fcontent = static_cast<std::string>(content).append("\n");
			Document parsed = data.parser.parse(File(fmt::format("[{}-apply tokens]", process->type_name), fcontent, 0, 0, f), &doc, &data).first;
			std::deque<Syntax::Element*> elems;
			parsed.get_tree().for_each_elem([&elems](Syntax::Element* elem) { elems.push_back(elem); });

			// Convert to SCM
			SCM list = Lisp::TypeConverter<decltype(elems)>::from(elems);
			
			auto result = process->apply.call_cv<decltype(elems)>(list);
			for (Syntax::Element* elem : result) // Add elems to doc
				doc.push_back(elem);

			// TODO: Find a better way to do this, using a method that also merges non elems (figures, custom types, ...)

			return content_end+process->token_end.size();
		}));

		return match_pos+line.size();
	});
	// Scheme code
	matches.emplace_back("%%"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "%%"); escaped)
			return new_pos;

		auto&& [scheme, scheme_end] = f.get_token("%%", "\\", match_pos+2);
		if (scheme_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Scheme", "Missing terminating '%%' after initial '%%'", match_pos, 2));
		if (scheme.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Scheme", "Scheme cannot be empty", match_pos, scheme_end-match_pos+2));

		Lisp::eval(scheme, doc, data, data.parser);

		return scheme_end+2;
	});
	// Call
	matches.emplace_back("%"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "%"); escaped)
			return new_pos;

		auto&& [call, call_end] = f.get_token("%", "\\", match_pos+1);
		if (call_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Call", "Missing terminating '%' after initial '%'", match_pos, 1));
		if (call.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Call", "Calls cannot be empty", match_pos, call_end-match_pos+1));

		if (call.front() != '(') // Substitution
		{
			if (call.contains('\n')) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Call", "Missing terminating '%' after initial '%'", match_pos, 1));

			std::string_view sv;
			//std::cout << "-------\n in " << f.name << std::endl;
			//doc.var_for_each([](const std::string& name, const std::string& val)
			//{
			//	std::cout << name << " : " << val << std::endl;
			//});
			//std::cout << "-------\n";
			std::string s;
			if (Variable* var = doc.var_get(call); var == nullptr) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Call", fmt::format("Unknown variable '{}'", call), match_pos, call_end-match_pos+1));
			else
				s = var->to_string(doc);
			s.push_back('\n');

			Document ins = data.parser.parse(File(fmt::format("[#+{}]", call), s, 0, 0, f), &doc, &data).first;
			doc.merge(std::move(ins));
		}
		else // Lisp mode
		{
			std::string r = Lisp::eval_r(call, doc, data, data.parser);
			r.push_back('\n');
			Document ins = data.parser.parse(File(fmt::format("[lisp result]"), r, 0, 0, f), &doc, &data).first;
			doc.merge(std::move(ins));
		}

		return call_end+1;
	});
	// cxxabi definition
	matches.emplace_back("(^|\n)\\@\\@<"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		const std::string_view line = f.get_line(match_pos);

		// Get code
		auto&& [code, code_end] = f.get_token(">@@", "\\", match_pos + line.size()+1);
		if (code.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid cxxabi definition", "Missing code", match_pos, line.size()));

		// Put back the spaces before the token starts
		code = std::string(f.content.cbegin() + match_pos + line.size()+1, f.content.cbegin() + code_end);
		if (code.empty()) [[unlikely]] // Just in case
			throw Error(getErrorMessage(f, "Invalid cxxabi definition", "Missing code", match_pos, line.size()));

		return code_end+3;
	});
	// cxxabi call
	matches.emplace_back("@<"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, ">@"); escaped)
			return new_pos;

		auto&& [call, call_end] = f.get_token(">@", "\\", match_pos+2);
		if (call_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid cxxabi Call", "Missing terminating '>@' after initial '@<'", match_pos, 2));
		if (call.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid cxxabi Call", "Calls cannot be empty", match_pos, call_end-match_pos+2));

		return call_end+2;
	});
	// Raw Inline
	matches.emplace_back("\\{\\{\\{"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "{{{"); escaped)
			return new_pos;

		auto&& [raw, raw_end] = f.get_token("}}}", "\\", match_pos+3);
		if (raw_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Raw Inline", "Missing terminating '}}}' after initial '{{{'", match_pos, 3));
		if (raw.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Raw Inline", "Inline raws cannot be empty", match_pos, raw_end-match_pos+3));

		doc.emplace_back<Syntax::RawInline>(std::move(raw));

		return raw_end+3;
	});
	// Raw
	matches.emplace_back("\\{\\{"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "{{"); escaped)
			return new_pos;

		auto&& [raw, raw_end] = f.get_token("}}", "\\", match_pos+2);
		if (raw_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Raw", "Missing terminating '}}' after initial '{{'", match_pos, 2));
		if (raw.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Raw", "Raws cannot be empty", match_pos, raw_end-match_pos+2));

		doc.emplace_back<Syntax::Raw>(std::move(raw));

		return raw_end+2;
	});
	// Comment
	matches.emplace_back("::"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "::"); escaped)
			return new_pos;

		std::size_t endl = f.content.find('\n', match_pos);
		if (endl == std::string::npos)
			endl = f.content.size();

		return endl;
	});
	//}}}
	//{{{ Blocks
	// Code
	matches.emplace_back("(^|\n)```"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		const std::string_view line = f.get_line(match_pos);

		// Get language
		auto&& [language, language_end] = f.get_token(",", "\\", match_pos+3, match_pos+line.size());
		bool has_name = true;
		if (language.empty()) // In case this code statement is anonymous e.g "```cpp\n..."
		{
			language = line.substr(3);
			language.erase(std::remove(language.begin(), language.end(), '\\'), language.end()); // Removes '\'
			has_name = false;
		}
		language = trimIdentifier(language);
		if (language.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Code", "Missing language", match_pos, 3));

		// Get name
		const std::string_view name = has_name ? trimIdentifier(line.substr(language_end - match_pos + 1)) : "";

		// Get code
		auto&& [code, code_end] = f.get_token("```", "\\", match_pos + line.size()+1);
		if (code.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Code", "Missing code", match_pos, line.size()));

		// Put back the spaces before the token starts
		code = std::string(f.content.cbegin() + match_pos + line.size()+1, f.content.cbegin() + code_end);
		if (code.empty()) [[unlikely]] // Just in case
			throw Error(getErrorMessage(f, "Invalid Code", "Missing code", match_pos, line.size()));

		// Processes includes
		for (std::size_t pos = 0; pos < code.size();)
		{
			const std::string_view left(code.cbegin()+pos, code.cend());
			const std::size_t left_pos = match_pos + line.size() + 1 + pos;

			if (left.starts_with("#:Inc ")) // Include code from another file
			{
				const File line_file = File("[include]", left.substr(0, std::min(left.find('\n'), left.size())), 0, 0, f);
				if (6 >= line_file.content.size()) [[unlikely]]
					throw Error(getErrorMessage(f, "Invalid Code", "Missing filename after '#:Inc'", left_pos, line.size()));

				auto&& [filename, filename_end] = line_file.get_token(",", "\\", 6);
				if (filename.empty())
					filename = line_file.content.substr(6);
				if (filename.empty()) [[unlikely]]
					throw Error(getErrorMessage(f, "Invalid Code", "Empty filename after '#:Inc'", left_pos, line.size()));

				// Parse begin,count
				if (filename_end != std::string::npos) // e.g '#:Inc filename.ext, 12, 3'
				{
					if (filename_end+1 >= line_file.content.size()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", "Missing include line begin after '#:Inc'", left_pos, line.size()));

					auto&& [line_begin, line_begin_end] = line_file.get_token(",", "\\", filename_end+1);
					line_begin = trimIdentifier(line_begin);
					if (line_begin.empty()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", "Missing include line begin after '#:Inc'", left_pos, line.size()));

					if (line_begin_end+1 >= line_file.content.size()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", "Missing include line begin after '#:Inc'", left_pos, line.size()));
					const std::string_view line_count = trimIdentifier(line_file.content.substr(line_begin_end+1));
					if (line_count.empty()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", "Missing include line count after '#:Inc'", left_pos, line.size()));

					auto stoi = [&] (const std::string_view& sv, std::size_t& out) {
						auto [end, err] = std::from_chars(sv.cbegin(), sv.cend(), out);
						if (err == std::errc{} && end == sv.cend()) [[likely]]
						{}
						else if (err == std::errc::invalid_argument) [[unlikely]]
							throw Error(getErrorMessage(f, "Invalid Code", fmt::format("Cannot parse '{}' as a number", line_begin), left_pos, line.size()));
						else if (err == std::errc::result_out_of_range) [[unlikely]]
							throw Error(getErrorMessage(f, "Invalid Code", fmt::format("Value '{}' cannot be safely represented (out of range)", line_begin), left_pos, line.size()));
						else
							throw Error(getErrorMessage(f, "Invalid Code", fmt::format("Could not parse '{}' as a number", line_begin), left_pos, line.size()));
					};
					std::size_t line_begin_n, line_count_n;
					stoi(line_begin, line_begin_n);
					stoi(line_count, line_count_n);

					const std::filesystem::path path(std::filesystem::path(f.name).parent_path().append(filename));
					std::ifstream in(path);
					if (!in.good()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", fmt::format("Unable to open file '{}' for '#:Inc'", filename), left_pos+6, filename.size()));

					// Erase "#:Inc"
					code.erase(pos, line_file.content.size());

					std::string line;
					std::size_t line_n = 0;
					while (std::getline(in, line))
					{
						++line_n;
						if (line_n >= line_begin_n + line_count_n) [[unlikely]]
							break;
						else if (line_n >= line_begin_n)
						{
							code.insert(pos, line);
							pos += line.size();
							if (line_n+1 < line_begin_n + line_count_n)
								code.insert(pos++, "\n", 1);
						}
					}
					in.close();
				}
				else // Include entire file
				{
					const std::filesystem::path path(std::filesystem::path(f.name).parent_path().append(filename));
					std::ifstream in(path);
					if (!in.good()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", fmt::format("Unable to open file '{}' for '#:Inc'", filename), left_pos+6, filename.size()));

					// Erase "#:Inc"
					code.erase(pos, line_file.content.size());

					std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
					if (content.empty()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Code", fmt::format("Included file '{}' is empty", filename), left_pos+6, filename.size()));
					content.resize(content.size()-1);
					code.insert(pos, content);
					in.close();

					// Skip included content to avoid recursive includes
					pos += content.size();
				}
			}
			else if (left.starts_with("\\")) // Escaping includes or code fragment declaration
			{
				std::size_t level = 0;
				while (level < left.size() && left[level] == '\\')
					++level;

				const std::string_view left2 = left.substr(level);
				if (left2.starts_with("#:Inc ") || left2.starts_with("```"))
				{
					code.erase(pos, 1); // Remove first '\'
					pos = code.find('\n', pos);
				}
				else
				{
					if (left.front() != '\n')
						pos = code.find('\n', pos);
					else
						++pos;
				}
			}
			else if (left.front() != '\n')
				pos = code.find('\n', pos);
			else
				++pos;
		}

		// Determine precise line numbers
		std::vector<Syntax::code_fragment> frags;
		std::size_t line_number = 1;
		const auto add_fragment = [&](const std::string&& code)
		{
			frags.push_back({line_number, std::move(code)});
			line_number += std::count(code.cbegin(), code.cend(), '\n');
		};

		std::size_t last_pos = 0;
		const File code_file = File("[code]", code, 0, 0, f);
		for (std::size_t pos = 0; pos < code.size(); ++pos)
		{
			const std::string_view left(code.cbegin()+pos, code.cend());

			if (left.starts_with("#:Line "))
			{
				add_fragment(code.substr(last_pos, pos - last_pos - 1));

				const std::string_view line = left.substr(0, std::min(left.find('\n'), left.size()));
				if (7 >= line.size()) [[unlikely]]
					throw Error(getErrorMessage(code_file, "Invalid Code", "Missing number after '#:Line'", pos, line.size()));

				const std::string_view line_nr = line.substr(7);
				if (line_nr.empty()) [[unlikely]]
					throw Error(getErrorMessage(code_file, "Invalid Code", "Missing number after '#:Line'", pos, line.size()));

				if (const auto&& [ptr, ec] = std::from_chars(line_nr.cbegin(), line_nr.cend(), line_number); ec != std::errc{} || ptr != line_nr.cend()) [[unlikely]]
					throw Error(getErrorMessage(code_file, "Invalid Code", "Invalid number for '#:Line'", pos, line.size()));

				pos += line.size();
				last_pos = pos+1;
			}
		}

		add_fragment(code.substr(last_pos));

		// Get CodeStyle
		std::string code_style;
		if (Variable* var = doc.var_get("CodeStyle"); var == nullptr) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Code", "You must set '#+CodeStyle' before using code fragments", match_pos, line.size()));
		else
			code_style = var->to_string(doc);

		doc.emplace_back<Syntax::Code>(std::move(language), std::string{name}, std::string{code_style}, std::move(frags));
		// Replace with new system
		//doc.emplace_back<Syntax::Code>(std::move(language), std::string(name), std::move(code), std::string(code_style));

		return code_end+3;
	});
	// Quote
	matches.emplace_back("(^|\n)>"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));
		if (f.content[match_pos] == '\n')
			++match_pos;

		// Get quote content
		std::string quote;
		std::size_t quote_end;
		std::string author;
		for (quote_end = match_pos; quote_end < f.content.size();)
		{
			if (f.content[quote_end] != '>')
				break;

			const std::size_t begin = quote_end+1;
			if (begin >= f.content.size()) [[unlikely]]
				throw Error(getErrorMessage(f, "Invalid Quote", "Empty quote line at end of file", quote_end));

			quote_end = f.content.find('\n', begin);
			if (quote_end == std::string::npos) // EOF
			{
				if (f.content[begin] == '[')
				{
					if (!author.empty()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Quote", "Quote can only have one author", begin, f.content.size()-begin));
					if (f.content.find(']', begin) == std::string::npos) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Quote", "Quote author needs a closing ']'", begin, f.content.size()-begin));

					author.append(f.content.substr(begin+1, f.content.size()-begin-2)).push_back('\n');
				}
				else
					quote.append(f.content.substr(begin));
				quote_end = f.content.size();
				break;
			}
			else
			{
				if (f.content[begin] == '[')
				{
					if (!author.empty()) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Quote", "Quote can only have one author", begin, quote_end-begin));
					if (f.content.find(']', begin) != quote_end-1) [[unlikely]]
						throw Error(getErrorMessage(f, "Invalid Quote", "Quote author needs a closing ']'", begin, quote_end-begin));


					author.append(f.content.substr(begin+1, quote_end-begin-2)).push_back('\n');
				}
				else
					quote.append(f.content.substr(begin, quote_end-begin+1));
				++quote_end;
			}
		}

		Document doc_quote = data.parser.parse(File("[quote]", std::string_view(quote), 0, 0, f), &doc, &data).first;
		doc.emplace_back<Syntax::Quote>(std::move(doc_quote.get_tree()), std::move(author));

		return quote_end;
	});
	// Latex math
	matches.emplace_back("\\$(?![\\$|\\|])"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "$"); escaped)
			return new_pos;

		if (match_pos+1 == f.content.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Math", "Missing terminating '$' after initial '$'", match_pos, 1));
		auto&& [tex, tex_end] = f.get_token("$", "\\", match_pos+1);
		if (tex_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Math", "Missing terminating '$' after initial '$'", match_pos, 1));
		if (tex.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Math", "LaTeX Math cannot be empty", match_pos, tex_end-match_pos+1));

		std::string preamble = doc.var_get_default("TexPreamble", "");
		std::string prepend = doc.var_get_default("TexPrepend", "");
		std::string append = doc.var_get_default("TexAppend", "");
		std::string font_size = doc.var_get_default("TexFontSize", "12");

		std::string filename = sha1(tex).append("_m");
		doc.emplace_back<Syntax::Latex>(std::move(tex), std::move(filename), std::move(preamble), std::move(prepend), std::move(append), std::move(font_size), Syntax::TexMode::MATH);

		return tex_end+1;
	});
	// Latex line
	matches.emplace_back("\\$\\$(?!\\$)"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "$$"); escaped)
			return new_pos;

		if (match_pos+2 >= f.content.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Normal", "Missing terminating '$$' after initial '$$'", match_pos, 2));
		auto&& [tex, tex_end] = f.get_token("$$", "\\", match_pos+2);
		if (tex_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Normal", "Missing terminating '$$' after initial '$$'", match_pos, 2));
		if (tex.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Normal", "LaTeX code cannot be empty", match_pos, tex_end-match_pos+2));

		std::string preamble = std::string(doc.var_get_default("TexPreamble", ""));
		std::string prepend = doc.var_get_default("TexPrepend", "");
		std::string append = doc.var_get_default("TexAppend", "");
		std::string font_size = doc.var_get_default("TexFontSize", "12");

		std::string filename = sha1(tex).append("_l");
		doc.emplace_back<Syntax::Latex>(std::move(tex), std::move(filename), std::move(preamble), std::move(prepend), std::move(append), std::move(font_size), Syntax::TexMode::MATH_LINE);

		return tex_end+2;
	});
	// Latex normal
	matches.emplace_back("\\$\\|"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "|$"); escaped)
			return new_pos;

		if (match_pos+2 >= f.content.size()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Normal", "Missing terminating '|$' after initial '$|'", match_pos, 2));
		auto&& [tex, tex_end] = f.get_token("|$", "\\", match_pos+2);
		if (tex_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Normal", "Missing terminating '|$' after initial '$|'", match_pos, 2));
		if (tex.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid LaTeX Normal", "LaTeX code cannot be empty", match_pos, tex_end-match_pos+2));

		std::string preamble = std::string(doc.var_get_default("TexPreamble", ""));
		std::string prepend = doc.var_get_default("TexPrepend", "");
		std::string append = doc.var_get_default("TexAppend", "");
		std::string font_size = doc.var_get_default("TexFontSize", "12");

		std::string filename = sha1(tex).append("_n");
		doc.emplace_back<Syntax::Latex>(std::move(tex), std::move(filename), std::move(preamble), std::move(prepend), std::move(append), std::move(font_size), Syntax::TexMode::NORMAL);

		return tex_end+2;
	});
	//}}}
	//{{{ Presentation
	// Center
	matches.emplace_back("\\[\\[(?!(\\[|\\|))"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "[["); escaped)
			return new_pos;

		auto&& [center, center_end] = f.get_token("]]", "\\", match_pos+2);
		if (center_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Center", "Missing closing ']]' after opening '[['", match_pos, 2));
		if (center.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Center", "Center cannot be empty", match_pos, center_end-match_pos+2));

		const auto&& [line_number, line_pos] = getPos(f, match_pos+2);
		center.push_back('\n');
		Document desc = data.parser.parse(File("[center]", center, line_number, line_pos, f), &doc, &data).first;
		doc.merge_non_elems(desc);
		doc.emplace_back<Syntax::Presentation>(std::move(desc.get_tree()), Syntax::PresType::CENTER);

		return center_end+2;
	});
	// Box
	matches.emplace_back("\\[\\[\\["s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "[[["); escaped)
			return new_pos;

		auto&& [box, box_end] = f.get_token("]]]", "\\", match_pos+3);
		if (box_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Box", "Missing closing ']]]' after opening '[[['", match_pos, 3));
		if (box.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Box", "Box cannot be empty", match_pos, box_end-match_pos+3));

		const auto&& [line_number, line_pos] = getPos(f, match_pos+3);
		box.push_back('\n');
		Document desc = data.parser.parse(File("[box]", box, line_number, line_pos, f), &doc, &data).first;
		doc.merge_non_elems(desc);
		doc.emplace_back<Syntax::Presentation>(std::move(desc.get_tree()), Syntax::PresType::BOX);

		return box_end+3;
	});
	// Left line
	matches.emplace_back("\\[\\[\\|"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "[[|"); escaped)
			return new_pos;

		auto&& [left_line, left_line_end] = f.get_token("|]]", "\\", match_pos+3);
		if (left_line_end == std::string::npos) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Left Line", "Missing closing '|]]' after opening '[[|'", match_pos, 3));
		if (left_line.empty()) [[unlikely]]
			throw Error(getErrorMessage(f, "Invalid Left Line", "Left line cannot be empty", match_pos, left_line_end-match_pos+3));

		const auto&& [line_number, line_pos] = getPos(f, match_pos+3);
		left_line.push_back('\n');
		Document desc = data.parser.parse(File("[left_line]", left_line, line_number, line_pos, f), &doc, &data).first;
		doc.merge_non_elems(desc);
		doc.emplace_back<Syntax::Presentation>(std::move(desc.get_tree()), Syntax::PresType::LEFT_LINE);

		return left_line_end+3;
	});
	//}}}
	//{{{ Text Styles
	// Bold
	matches.emplace_back("\\*\\*"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "**"); escaped)
			return new_pos;

		data.style ^= Syntax::Style::BOLD;
		if (data.style & Syntax::Style::BOLD)
			doc.emplace_back<Syntax::StylePush>(Syntax::Style::BOLD);
		else
			doc.emplace_back<Syntax::StylePop>(Syntax::Style::BOLD);
		return match_pos+2;
	});
	// Underline
	matches.emplace_back("__"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "__"); escaped)
			return new_pos;

		data.style ^= Syntax::Style::UNDERLINE;
		if (data.style & Syntax::Style::UNDERLINE)
			doc.emplace_back<Syntax::StylePush>(Syntax::Style::UNDERLINE);
		else
			doc.emplace_back<Syntax::StylePop>(Syntax::Style::UNDERLINE);
		return match_pos+2;
	});
	// Italic
	matches.emplace_back("\\*(?!\\*)"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "*"); escaped)
			return new_pos;

		data.style ^= Syntax::Style::ITALIC;
		if (data.style & Syntax::Style::ITALIC)
			doc.emplace_back<Syntax::StylePush>(Syntax::Style::ITALIC);
		else
			doc.emplace_back<Syntax::StylePop>(Syntax::Style::ITALIC);
		return match_pos+1;
	});
	// Verbatim
	matches.emplace_back("="s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		if (auto&& [escaped, new_pos] = escapeAddText(doc, f, data, prev_pos, match_pos, "="); escaped)
			return new_pos;

		data.style ^= Syntax::Style::VERBATIM;
		if (data.style & Syntax::Style::VERBATIM)
			doc.emplace_back<Syntax::StylePush>(Syntax::Style::VERBATIM);
		else
			doc.emplace_back<Syntax::StylePop>(Syntax::Style::VERBATIM);
		return match_pos+1;
	});
	//}}}
	//{{{ Breaks
	// Long break
	matches.emplace_back("[\n]{2,}[^#]"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));

		// In case some style was not closed
		forEveryStyle(data.style, [&] (Syntax::Style s) {
			throw Error(getErrorMessage(f, "Unterminated Style", (std::string)Syntax::getStyleName(s), f.content.size()-1));
		});

		doc.custom_styles_for_each([&](const std::string& name, const CustomStyle* style)
		{
			if (data.customStyle[style->index])
				throw Error(getErrorMessage(f, "Unterminated Custom Style", std::string(name), f.content.size()-1));
		});

		const std::string_view left = f.content.substr(match_pos);
		std::size_t num; // Number of '\n'
		for (num = 0; num < left.size(); ++num)
			if (left[num] != '\n')
				break;

		doc.emplace_back<Syntax::Break>(num-1);

		return match_pos+num-1;
	});
	// Short break
	matches.emplace_back("\n[^\n]"s, [](Document& doc, const File& f, ParserData& data, std::size_t prev_pos, std::size_t match_pos)
			-> std::size_t
	{
		addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos));

		doc.emplace_back<Syntax::Break>(0); // Append size 0 break
		//// Append a whitespace if previous and current element are text-like
		//if (doc.empty()) [[unlikely]]
		//	return match_pos+1;
		//else if (doc.back()->get_type())
		//{
		//	auto& elem = *reinterpret_cast<Syntax::Text*>(doc.back());
		//	if (elem.style == data.style)
		//		elem.content.append(append);
		//	else
		//		doc.emplace_back<Syntax::Text>(append, data.style);
		//}
		//else
		//{
		//	doc.emplace_back<Syntax::Text>(append, data.style);
		//}
		
		//if (auto ptr = addText(doc, f, data, f.content.substr(prev_pos, match_pos-prev_pos)); ptr)
		//{
		//	addText(doc, f, data, " ");
		//}

		// In case some style was not closed
		forEveryStyle(data.style, [&] (Syntax::Style s) {
			throw Error(getErrorMessage(f, "Unterminated Style", (std::string)Syntax::getStyleName(s), match_pos-1));
		});

		doc.custom_styles_for_each([&](const std::string& name, const CustomStyle* style)
		{
			if (data.customStyle[style->index])
				throw Error(getErrorMessage(f, "Unterminated Custom Style", std::string(name), match_pos-1));
		});

		return match_pos+1;
	});
	//}}}
}

[[nodiscard]] ParserData::Match::Match(const std::string& match, decltype(callback)&& callback):
	original(match), callback(callback)
{
	try
	{
		regex = std::regex(match);
	}
	catch (std::regex_error& e)
	{
		throw Error(fmt::format("regex_error() : regex `{}` failed to compile with message : {}", match, e.what()));
	}
}

[[nodiscard]] Parser::Parser(const Compiler* const _compiler):
	compiler(_compiler)
{
}

[[nodiscard]] std::pair<Document, ParserData> Parser::parse(const File& f, const Document* inherit, ParserData* inheritData)
{
	ParserData data(*this);
	if (inheritData)
	{
		data.matches = inheritData->matches;
		data.customStyle = inheritData->customStyle;
		data.customPres = inheritData->customPres;
	}
	data.match_point.resize(data.matches.size(), 0);
	data.match_length.resize(data.matches.size(), 0);
	data.match_str.resize(data.matches.size(), "");
	// Initializes match_point
	for (std::size_t i = 0; i < data.matches.size(); ++i)
	{
		if (std::match_results<std::string_view::const_iterator> m; std::regex_search(f.content.cbegin(), f.content.cend(), m, data.matches[i].regex))
		{
			data.match_point[i] = m.position(0);
			data.match_length[i] = m.length(0);
			data.match_str[i] = m.str(0);
		}
		else
		{
			data.match_point[i] =  std::string::npos; // Will never match
			data.match_length[i] = 0;
			data.match_str[i] = "";
		}
	}

	Document doc;
	if (inherit)
	{
		inherit->var_for_each([&](const std::string& name, const Variable* var)
		{ doc.vars.insert({name, std::unique_ptr<Variable>(var->clone())}); });
		doc.figures = inherit->figures;
		doc.custom_types = inherit->custom_types;
	}
	static bool lisp = false;
	if (!lisp)
	{
		Lisp::init(doc, f, data, *this);
		lisp = true;
	}
	for (std::size_t cur_pos = 0; cur_pos < f.content.size();)
	{
		// Next match
		const std::size_t match_index = std::min_element(data.match_point.cbegin(), data.match_point.cend()) - data.match_point.cbegin();
		const std::size_t match_pos = data.match_point[match_index];

		if (match_pos == std::string::npos) // Last match, everything left can only be text
		{
			closeList(doc, f, data);
			addText(doc, f, data, f.content.substr(cur_pos, f.content.size() - cur_pos - 1));

			// In case some style was not closed
			forEveryStyle(data.style, [&] (Syntax::Style s) {
				throw Error(getErrorMessage(f, "Unterminated Style", (std::string)Syntax::getStyleName(s), f.content.size()-1));
			});

			doc.custom_styles_for_each([&](const std::string& name, const CustomStyle* style)
			{
				if (data.customStyle[style->index])
					throw Error(getErrorMessage(f, "Unterminated Custom Style", std::string(name), f.content.size()-1));
			});

			break;
		}

		// Close lists if not index 0 (index for list)
		if (match_index != 0)
			closeList(doc, f, data);

		data.current_match_length = data.match_length[match_index];
		data.current_match_str = data.match_str[match_index];
		cur_pos = data.matches[match_index].callback(doc, f, data, cur_pos, match_pos);
		data.last_match_index = match_index;

		//std::cout << " - " << f.name << '[' << cur_pos << "] -----\n";

		// Update data.match_point & data.match_length
		for (std::size_t i = 0; i < data.matches.size(); ++i)
		{
			//std::cout << i << " : " << data.match_point[i] << " " << matches[i].original << " -- " << data.match_str[i] << std::endl;

			if (std::match_results<std::string_view::const_iterator> m; std::regex_search(f.content.cbegin()+cur_pos, f.content.cend(), m, data.matches[i].regex, std::regex_constants::match_not_bol))
			{
				data.match_point[i] = m.position(0)+cur_pos;
				data.match_length[i] = m.length(0);
				data.match_str[i] = m.str(0);
			}
			else
			{
				data.match_point[i] =  std::string::npos; // Will never match
				data.match_length[i] = 0;
				data.match_str[i] = "";
			}
		}
	}

	closeList(doc, f, data);

	auto isTextLike = [](Syntax::Type t)
	{
		return t == Syntax::TEXT         ||
			   t == Syntax::STYLEPUSH    ||
			   t == Syntax::STYLEPOP     ||
			   t == Syntax::BREAK        ||
			   t == Syntax::REFERENCE    ||
			   t == Syntax::LINK         ||
			   t == Syntax::LATEX        ||
			   t == Syntax::RAW_INLINE   ||
			   t == Syntax::EXTERNAL_REF ||
			   t == Syntax::ANNOTATION   ||
			   t == Syntax::CUSTOM_STYLEPUSH ||
			   t == Syntax::CUSTOM_STYLEPOP;
	};

	// Process all size 0 breaks
	doc.tree.for_each_elem([&,
		e1 = (Syntax::Element*)nullptr,
		e2 = (Syntax::Element*)nullptr](Syntax::Element* e3) mutable
	{
		auto end = [&]
		{
			e1 = e2;
			e2 = e3;
		};

		if (e1 == nullptr || !isTextLike(e1->get_type()) ||
			e2 == nullptr || e2->get_type() != Syntax::BREAK ||
			!isTextLike(e3->get_type())) [[likely]]
			return end();

		const auto& br = *reinterpret_cast<const Syntax::Break*>(e2);
		if (br.size != 0)
			return end();

		// If previous & next elements are text-like,
		// add a whitespace before the break
		if (e1->get_type() == Syntax::TEXT)
		{
			auto& text = *reinterpret_cast<Syntax::Text*>(e1);
			text.content.push_back(' ');
		}
		else
			doc.tree.insert_before<Syntax::Text>(e2, " "s);
		end();
	});

	return std::make_pair(std::move(doc), std::move(data));
}
