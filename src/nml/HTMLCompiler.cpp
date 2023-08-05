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

#include "HTMLCompiler.hpp"
#include "Syntax.hpp"
#include "Highlight.hpp"
#include "Cache.hpp"

#include <set>
#include <fmt/format.h>

#include <iostream>

using namespace std::literals;

[[nodiscard]] std::string HTMLCompiler::get_name() const
{
	return "HTML";
}

[[nodiscard]] bool HTMLCompiler::var_reserved(const std::string& name) const
{
	const static std::set<std::string> reserved{
		"SectionLink", "SectionLinkPos", "OrderedSectionFormatter", "UnorderedSectionFormatter",
	};
	return reserved.find(name) != reserved.end();
}

template <class T, std::size_t N>
[[nodiscard]] static std::pair<std::string, T> parseVar(
	const Document& doc,
	const std::string_view& var_name,
	const std::string& default_value,
	const std::array<std::pair<std::string_view, T>, N>& possible)
{
	const std::string var_value = doc.var_get_default(std::string(var_name), default_value);
	
	for (std::size_t i = 0; i < N; ++i)
		if (possible[i].first == var_value)
			return {""s, possible[i].second};

	return {fmt::format(
			"Cannot understand value `{}` for '{}', possible values: {}"sv,
			var_value, var_name, [&]
			{
				std::string s("{");

				std::for_each_n(possible.cbegin(), N-1, [&s](const std::pair<std::string_view, T>& p)
					{ s.append(p.first).append(", "); });
				s.append(possible.back().first);
				s.push_back('}');

				return s;
			}()), possible.front().second};
}

[[nodiscard]] std::string HTMLCompiler::var_check(const std::string& name, const std::string& value) const
{
	// TODO:
	if (name == "SectionLinkPos")
	{
		if (value != "before" && value != "after" && value != "none")
			return "";
	}

	return "";
}

MAKE_CENUM_Q(SectionLinkPos, std::uint8_t,
	NONE, 0,
	AFTER, 1,
	BEFORE, 2,
);

struct HTMLData
{
	SectionLinkPos section_link_pos;
	std::string section_link;

	std::optional<Lisp::Proc> orderedSectionFormatter;
	std::optional<Lisp::Proc> unorderedSectionFormatter;
};

[[nodiscard]] std::string HTMLCompiler::get_anchor(const std::string_view& name)
{
	std::string formatted(name.size(), '\0');
	for (std::size_t i{0}; i < name.size(); ++i)
	{
		if (   (name[i] >= '0' && name[i] <= '9')
			|| (name[i] >= 'A' && name[i] <= 'Z')
			|| (name[i] >= 'a' && name[i] <= 'z')
			||  name[i] == '.' || name[i] == '_' 
			||  name[i] == ':' || name[i] == '-')
			formatted[i] = name[i];
		else
			formatted[i] = '_';
	}

	return formatted;
}

[[nodiscard]] std::string HTMLCompiler::formatHTML(const std::string_view& s)
{
	return replace_each(s,
			make_array<std::pair<char, std::string_view>>(
				std::make_pair('<', "&lt;"sv),
				std::make_pair('>', "&gt;"sv),
				std::make_pair('&', "&amp;"sv),
				std::make_pair('"', "&quot;"sv)
			));
}

[[nodiscard]] static std::string getSectionFullName(const Syntax::Section& sec, const std::deque<std::size_t>& secStack)
{
	std::string number{""};
	bool first{true};

	std::size_t level{0};
	for (auto n : secStack)
	{
		++level;
		if (!first)
			number.append(" ");
		number.append(std::to_string(n));
		first = false;
	}

	return fmt::format("{}{}{}", number, ".", sec.get<"title">());
}

MAKE_CENUM_Q(FigureType, std::uint8_t,
	PICTURE, 0,
	AUDIO, 1,
	VIDEO, 2,
);

[[nodiscard]] static FigureType getFigureType(const Syntax::Figure& fig)
{
	const auto pos{fig.get<"path">().rfind('.')};
	if (pos == std::string::npos) [[unlikely]]
		throw Error(fmt::format("Cannot determine type of figure '![{}]({})', missing extension", fig.get<"name">(), fig.get<"path">()));
	const std::string_view ext{fig.get<"path">().substr(pos+1)};
	if (ext.empty()) [[unlikely]]
		throw Error(fmt::format("Cannot determine type of figure '![{}]({})', missing extension", fig.get<"name">(), fig.get<"path">()));

	if (ext == "png"sv  ||
		ext == "jpg"sv  ||
		ext == "jpeg"sv ||
		ext == "svg"sv  ||
		ext == "bmp"sv  ||
		ext == "gif"sv)
		return FigureType::PICTURE;

	if (ext == "mp3"sv ||
		ext == "wav"sv ||
		ext == "ogg"sv ||
		ext == "flac"sv)
		return FigureType::AUDIO;

	if (ext == "mkv"sv  ||
		ext == "mp4"sv  ||
		ext == "webm"sv)
		return FigureType::VIDEO;

	throw Error(fmt::format("Cannot determine type of figure '![{}]({})', unknown extension '{}'", fig.get<"name">(), fig.get<"path">(), ext));
}

void HTMLCompiler::compile(const Document& doc) const
{
	HTMLData hdata;
	Cache cache{m_opts.cache_dir, m_opts.stream};

	//{{{ generate
	std::function<void(std::ostream&, const SyntaxTree&, std::size_t)> generate = [&](std::ostream& stream, const SyntaxTree& tree, std::size_t depth)
	{
		auto format = [](std::ostream& s, std::size_t depth, const std::string& content)
		{
			std::fill_n(std::ostream_iterator<char>(s), depth, '\t');
			s << content;
		};

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

		std::deque<std::size_t> sections;
		Syntax::Type last_type{Syntax::SECTION}; // Emulate title
		std::stack<const Syntax::ListBegin*> list_delimiters;

		tree.for_each_elem([&](const Syntax::Element* elem)
		{
			if (isTextLike(last_type) && !isTextLike(elem->get_type()))
				stream << "</p\n";
			if (last_type == Syntax::FIGURE && elem->get_type() != Syntax::FIGURE)
				format(stream, depth, "</div>\n");
			if (isTextLike(elem->get_type()) &&
					((last_type == Syntax::LATEX && elem->get_type() != Syntax::LATEX)
					 || (last_type == Syntax::ANNOTATION)))
				format(stream, depth, "");
			if (!isTextLike(last_type) && isTextLike(elem->get_type()))
				format(stream, depth, "<p>");

			switch (elem->get_type())
			{
				case Syntax::TEXT:
				{
					const auto& text = *reinterpret_cast<const Syntax::Text*>(elem);
					cache(text, [&](auto& s){ s << formatHTML(text.get<"content">()); });

					break;
				}
				case Syntax::STYLEPUSH:
				{
					const auto& p = *reinterpret_cast<const Syntax::StylePush*>(elem);
					cache(p, [&](auto& s)
					{
						switch (p.get<"style">())
						{
							case Syntax::Style::BOLD:
								s << "<b>"sv; break;
							case Syntax::Style::UNDERLINE:
								s << "<u>"sv; break;
							case Syntax::Style::ITALIC:
								s << "<i>"sv; break;
							case Syntax::Style::VERBATIM:
								s << "<em>"sv; break;
						}
					});

					break;
				}
				case Syntax::STYLEPOP:
				{
					const auto& p = *reinterpret_cast<const Syntax::StylePop*>(elem);
					cache(p, [&](auto& s)
					{
						switch (p.get<"style">())
						{
							case Syntax::Style::BOLD:
								s << "</b>"sv; break;
							case Syntax::Style::UNDERLINE:
								s << "</u>"sv; break;
							case Syntax::Style::ITALIC:
								s << "</i>"sv; break;
							case Syntax::Style::VERBATIM:
								s << "</em>"sv; break;
						}
					});

					break;
				}
				case Syntax::BREAK:
				{
					const auto& br = *reinterpret_cast<const Syntax::Break*>(elem);
					if (br.get<"size">() == 0)
						break;

					cache(br, [&](auto& s)
					{
						s << '\n';
						format(s, depth, "");
						for (std::size_t i = 0; i < br.get<"size">(); ++i)
							s << "<br>";
						s << '\n';
						format(s, depth, "");
					});
					break;
				}
				case Syntax::SECTION:
				{
					const auto& section = *reinterpret_cast<const Syntax::Section*>(elem);
					//if (last_type == Syntax::SECTION)
					cache(section, [&](auto& s)
					{
						format(s, depth, "<br>\n");
						if (section.get<"numbered">())
						{
							while (sections.size() > section.get<"level">())
								sections.pop_back();
							while (sections.size() < section.get<"level">())
								sections.push_back(0);
							++sections.back();

							std::string f;
							switch (hdata.section_link_pos)
							{
								case SectionLinkPos::BEFORE:
									f = "\t\t<h{0} id=\"{2}\"><a class=\"section-link\" href=\"#{2}\">{3}</a>{1}</h{0}><br>\n";
									break;
								case SectionLinkPos::AFTER:
									f = "\t\t<h{0} id=\"{2}\">{1}<a class=\"section-link\" href=\"#{2}\">{3}</a></h{0}><br>\n";
									break;
								case SectionLinkPos::NONE:
									f = "\t\t<h{0} id=\"{2}\">{1}</h{0}><br>\n";
									break;
								default: break;
							}
							s << fmt::format(fmt::runtime(f),
										std::min(1+section.get<"level">(), 6ul),
										formatHTML((hdata.orderedSectionFormatter.has_value())
											? hdata.orderedSectionFormatter.value().call(Lisp::to_scm(section), Lisp::to_scm(sections))
											: getSectionFullName(section, sections)),
										get_anchor(section.get<"title">()),
										hdata.section_link);
						}
						else
						{
							std::string f;
							switch (hdata.section_link_pos)
							{
								case SectionLinkPos::BEFORE:
									f = "\t\t<h{0} id=\"{2}\"><a class=\"section-link\" href=\"#{2}\">{3}</a>{1}</h{0}><br>\n";
									break;
								case SectionLinkPos::AFTER:
									f = "\t\t<h{0} id=\"{2}\">{1}<a class=\"section-link\" href=\"#{2}\">{3}</a></h{0}><br>\n";
									break;
								case SectionLinkPos::NONE:
									f = "\t\t<h{0} id=\"{2}\">{1}</h{0}><br>\n";
									break;
								default: break;
							}
							s << fmt::format(fmt::runtime(f),
										std::min(1+section.get<"level">(), 6ul),
										formatHTML((hdata.unorderedSectionFormatter.has_value())
											? hdata.unorderedSectionFormatter.value().call(Lisp::to_scm(section), Lisp::to_scm(sections))
											: section.get<"title">()),
										get_anchor(section.get<"title">()),
										hdata.section_link);
						}
					});
					break;
				}
				case Syntax::LIST_BEGIN:
				{
					const auto& begin = *reinterpret_cast<const Syntax::ListBegin*>(elem);

					list_delimiters.push(&begin);
					cache(begin, [&](auto& s)
					{
						format(s, depth, "<ul>\n");
					});
					++depth;

					break;
				}
				case Syntax::LIST_END:
				{
					const auto& end = *reinterpret_cast<const Syntax::ListEnd*>(elem);

					--depth;
					if (list_delimiters.empty()) [[unlikely]] // Not possible [should have been caught in parser...]
						throw Error("End list delimiter, without begin");
						list_delimiters.pop();
					cache(end, [&](auto& s)
					{
						format(s, depth, "</ul>\n");
					});
					break;
				}
				case Syntax::LIST_ENTRY:
				{
					if (list_delimiters.empty()) [[unlikely]] // Not possible [parser shouldn't have generated entry without delimiter]
						throw Error("List without delimiter");

					const Syntax::ListEntry& ent = *reinterpret_cast<const Syntax::ListEntry*>(elem);
					const Syntax::ListBegin& delim = *list_delimiters.top();

					cache(ent, [&](auto& s)
					{
						if (delim.get<"ordered">())
						{
							const Syntax::OrderedBullet& bullet = std::get<Syntax::OrderedBullet>(delim.get<"bullet">());
							if (delim.get<"style">().empty())
								format(s, depth, fmt::format("<li><a class=\"bullet\">{}{}{}</a>\n",
											formatHTML(bullet.left),
											formatHTML(bullet.get(ent.get<"counter">())),
											formatHTML(bullet.right)
											));
							else
								format(s, depth, fmt::format("<li><a class=\"bullet\" style=\"{}\">{}{}{}</a>\n",
											formatHTML(delim.get<"style">()),
											formatHTML(bullet.left),
											formatHTML(bullet.get(ent.get<"counter">())),
											formatHTML(bullet.right)
											));

							generate(s, ent.get<"content">(), depth+1);
						}
						else
						{
							const Syntax::UnorderedBullet& bullet = std::get<Syntax::UnorderedBullet>(delim.get<"bullet">());
							if (delim.get<"style">().empty())
								format(s, depth, fmt::format("<li><a class=\"bullet\">{}</a>\n",
											formatHTML(bullet.bullet)
											));
							else
								format(s, depth, fmt::format("<li><a class=\"bullet\" style=\"{}\">{}</a>\n",
											formatHTML(delim.get<"style">()),
											formatHTML(bullet.bullet)
											));

							generate(s, ent.get<"content">(), depth+1);
						}
						format(s, depth, fmt::format("</li>\n"));
					});

					break;
				}
				case Syntax::RULER:
				{
					const Syntax::Ruler& ruler = *reinterpret_cast<const Syntax::Ruler*>(elem);

					cache(ruler, [&](auto& s)
					{
						format(s, depth, "<hr>\n");
					});
					break;
				}
				case Syntax::FIGURE:
				{
					const Syntax::Figure& fig = *reinterpret_cast<const Syntax::Figure*>(elem);

					if (last_type != Syntax::FIGURE)
						format(stream, depth, "<div class=\"figures\">\n");

					cache(fig, [&](auto& s)
					{
						format(s, depth+1, "<div class=\"figure\">\n");
						switch (getFigureType(fig))
						{
							case FigureType::PICTURE:
								format(s, depth+2, fmt::format("<a href=\"{0}\"><img src=\"{0}\"></a>\n", fig.get<"path">()));
								break;
							case FigureType::AUDIO: // TODO
								break;
							case FigureType::VIDEO:
								format(s, depth+2, fmt::format("<video src=\"{}\" controls></video>\n", fig.get<"path">()));
								break;
							default: break;
						}

						// Description
						format(s, depth+2, fmt::format("<p><b>({})</b></p>\n", fig.get<"id">()));
						generate(s, fig.get<"description">(), depth+2);
						format(s, depth+1, "</div>\n");
					});
					break;
				}
				case Syntax::CODE:
				{
					const Syntax::Code& code = *reinterpret_cast<const Syntax::Code*>(elem);

					cache(code, [&](auto& s)
					{
						format(s, depth, "<div class=\"highlight\">\n");
						if (!code.get<"name">().empty())
							format(s, depth+1, fmt::format("<div class=\"highlight-title\">{}</div>\n", formatHTML(code.get<"name">())));
						format(s, depth+1, "<div class=\"highlight-content\">\n");

						// List of all lines
						std::vector<std::pair<std::size_t, std::string>> lines;

						// Build lines vector
						for (const auto& [start_line, content] : code.get<"content">())
						{
							// Get formatted string
							std::string source = Highlight<HighlightTarget::HTML>(content, code.get<"language">(), code.get<"style_file">());
							// Remove source-highlight comment
							std::string_view formatted{std::string_view{source}.substr(source.find("-->\n") + 4)};


							for (std::size_t i = 0, start = 0, end = std::min(formatted.find('\n', start), formatted.size()); 
									start != formatted.size();
									++i, start = end+1, end = std::min(formatted.find('\n', start), formatted.size()))
							{

								std::string_view line{formatted.substr(start, end-start)};

								if (i == 0) // '<pre><tt>[content]'
								{
									lines.push_back({i+start_line, std::string{line.substr(9)}});
								}
								else if (end+1 == formatted.size()) // '[content]</tt></pre>'
								{
									if (start+12 != end) // Prevent empty last line
										lines.push_back({i+start_line, std::string{line.substr(0, line.size()-11)}});
								}
								else
									lines.push_back({i+start_line, std::string{line}});
							}
						}

						format(s, depth+2, "<table>\n");
						format(s, depth+3, "<tr>\n");

						// Output lines
						format(s, depth+4, "<td class=\"gutter\">\n");
						format(s, depth+5, "<pre>");
						for (const auto& [line, content] : lines)
							format(s, 0, fmt::format("<span>{}</span><br>", line));
						format(s, 0, "</pre>\n");
						format(s, depth+4, "</td>\n");

						// Output code
						format(s, depth+4, "<td class=\"code\">\n");
						format(s, depth+5, "<pre>");
						for (const auto& [line, content] : lines)
							format(s, 0, fmt::format("<span>{}</span><br>", content));
						format(s, 0, "</pre>\n");
						format(s, depth+4, "</td>\n");

						format(s, depth+3, "</tr>\n");
						format(s, depth+2, "</table>\n");

						format(s, depth+1, "</div>\n");
						format(s, depth, "</div>\n");
					});
					break;
				}
				case Syntax::QUOTE:
				{
					const Syntax::Quote& quote = *reinterpret_cast<const Syntax::Quote*>(elem);

					cache(quote, [&](auto& s)
					{
						format(s, depth, "<blockquote>\n");

						generate(s, quote.get<"quote">(), depth+1);
						if (!quote.get<"author">().empty())
							format(s, depth+1, fmt::format("<p class=\"quote-author\">{}</p>\n",
										replace_each(quote.get<"author">().substr(0, quote.get<"author">().size()-1),
											std::array<std::pair<char, std::string_view>, 1>{
											std::make_pair<char, std::string_view>('\n', " "sv)
											})));

						format(s, depth, "</blockquote>\n");
					});

					break;
				}
				case Syntax::REFERENCE:
				{
					const auto& ref = *reinterpret_cast<const Syntax::Reference*>(elem);
					const Syntax::Figure* fig = doc.figure_get(ref.get<"referencing">());
					if (!fig) [[unlikely]]
						throw Error(fmt::format("Could not find figure with name '{}'", ref.get<"referencing">()));

					cache(ref, [&](auto& s)
					{
						switch (getFigureType(*fig))
						{
							case FigureType::PICTURE:
								s << fmt::format("<b class=\"figure-ref\"><a class=\"figure-ref\">({})</a><img src=\"{}\"></b>",
											fig->get<"id">(),
											formatHTML(fig->get<"path">()));
								break;
							default:
								throw Error("Figure references is supported for pictures only");
								break;
						}
					});
					break;
				}
				case Syntax::LINK:
				{
					const Syntax::Link& link = *reinterpret_cast<const Syntax::Link*>(elem);

					cache(link, [&](auto& s)
					{
						s << fmt::format("<a class=\"link\" href=\"{}\">{}</a>", formatHTML(link.get<"path">()), formatHTML(link.get<"name">()));
					});
					break;
				}
				case Syntax::LATEX:
				{
					if (!m_opts.tex_enabled)
						break;
					const Syntax::Latex& tex = *reinterpret_cast<const Syntax::Latex*>(elem);
					auto&& [content, filename] = Tex(m_opts.tex_dir, tex);
					if (isTextLike(last_type))
						stream << '\n';
					cache(tex, [&](auto& s)
					{
						format(s, depth, content);
						s << '\n';
					});
					break;
				}
				case Syntax::RAW:
				{
					const Syntax::Raw& raw = *reinterpret_cast<const Syntax::Raw*>(elem);

					cache(raw, [&](auto& s)
					{
						std::string replace(depth+1, '\t');
						replace.front() = '\n';
						format(s, depth, replace_each(raw.get<"content">(),
									std::array<std::pair<char, std::string_view>, 1>{std::make_pair<char, std::string_view>('\n', replace)}));
						s << '\n';
					});
					break;
				}
				case Syntax::RAW_INLINE:
				{
					const Syntax::RawInline& raw = *reinterpret_cast<const Syntax::RawInline*>(elem);

					cache(raw, [&](auto& s)
					{
						std::string replace(depth+1, '\t');
						replace.front() = '\n';
						if (isTextLike(last_type))
							s << replace_each(raw.get<"content">(),
										std::array<std::pair<char, std::string_view>, 1>{std::make_pair<char, std::string_view>('\n', replace)});
						else
							format(s, depth, replace_each(raw.get<"content">(),
										std::array<std::pair<char, std::string_view>, 1>{std::make_pair<char, std::string_view>('\n', replace)}));
					});
					break;
				}
				case Syntax::EXTERNAL_REF:
				{
					const Syntax::ExternalRef& ref = *reinterpret_cast<const Syntax::ExternalRef*>(elem);
					
					cache(ref, [&](auto& s)
					{
						s << fmt::format("<sup><a class=\"external-ref\" id=\"ref_{0}from\" href=\"#ref_{0}\" alt=\"{1}\">[{0}]</a></sup>", ref.get<"num">(), formatHTML(ref.get<"desc">()));
					});
					break;
				}
				case Syntax::PRESENTATION:
				{
					const Syntax::Presentation& pres = *reinterpret_cast<const Syntax::Presentation*>(elem);

					cache(pres, [&](auto& s)
					{
						if (pres.get<"type">() == Syntax::PresType::CENTER)
						{
							format(s, depth, "<center>\n");
							generate(s, pres.get<"content">(), depth+1);
							format(s, depth, "</center>\n");
						}
						else if (pres.get<"type">() == Syntax::PresType::BOX)
						{
							format(s, depth, "<div class=\"box\">\n");
							generate(s, pres.get<"content">(), depth+1);
							format(s, depth, "</div>\n");
						}
						else if (pres.get<"type">() == Syntax::PresType::LEFT_LINE)
						{
							format(s, depth, "<div class=\"left-line\">\n");
							generate(s, pres.get<"content">(), depth+1);
							format(s, depth, "</div>\n");
						}
						else
							throw Error(fmt::format("Unsupported presentation type : {}", pres.get<"type">().value));
					});
					break;
				}
				case Syntax::ANNOTATION:
				{
					const Syntax::Annotation& anno = *reinterpret_cast<const Syntax::Annotation*>(elem);
					if (isTextLike(last_type))
						stream << '\n';

					cache(anno, [&](auto& s)
					{
						format(s, depth, "<div class=\"annotation\">\n");
						generate(s, anno.get<"name">(), depth+1);
						format(s, depth, "</div>\n");
						format(s, depth, "<div class=\"hide\">\n");
						generate(s, anno.get<"content">(), depth+1);
						format(s, depth, "</div>\n");
					});
					break;
				}
				case Syntax::CUSTOM_STYLEPUSH:
				{
					const auto& push = *reinterpret_cast<const Syntax::CustomStylePush*>(elem);

					cache(push, [&](auto& s)
					{
						s << push.get<"style">().begin.call();
					});
					break;
				}
				case Syntax::CUSTOM_STYLEPOP:
				{
					const auto& pop = *reinterpret_cast<const Syntax::CustomStylePop*>(elem);

					cache(pop, [&](auto& s)
					{
						s << pop.get<"style">().end.call();
					});
					break;
				}
				case Syntax::CUSTOM_PRESPUSH:
				{
					const auto& push = *reinterpret_cast<const Syntax::CustomPresPush*>(elem);

					cache(push, [&](auto& s)
					{
						format(s, depth, push.get<"pres">().begin.call(Lisp::TypeConverter<std::size_t>::from(push.get<"level">())));
						s << '\n';
					});
					++depth;
					break;
				}
				case Syntax::CUSTOM_PRESPOP:
				{
					const auto& pop = *reinterpret_cast<const Syntax::CustomPresPop*>(elem);

					--depth;
					cache(pop, [&](auto& s)
					{
						format(s, depth, pop.get<"pres">().end.call(Lisp::TypeConverter<std::size_t>::from(pop.get<"level">())));
						s << '\n';
					});
					break;
				}
				default: break;
			}

			last_type = elem->get_type();
		});

		if (isTextLike(last_type))
			stream << "</p>\n";
		if (last_type == Syntax::FIGURE)
			format(stream, depth, "</div>\n");
	};
	//}}}

	// Parse
	//#+SectionLinkPos
	if (const auto&& [err, v] = parseVar(doc, "SectionLinkPos", "after",
			make_array<std::pair<std::string_view, SectionLinkPos>>(
				std::make_pair("none"sv, SectionLinkPos::NONE),
				std::make_pair("after"sv, SectionLinkPos::AFTER),
				std::make_pair("before"sv, SectionLinkPos::BEFORE)
			)); !err.empty()) throw Error(fmt::format("\nHTML Error : {}"sv, err));
	else
		hdata.section_link_pos = v;
	//#+SectionLink
	hdata.section_link = doc.var_get_default("SectionLink", "[link]");
	//#+OrderedSectionFormatter
	if (const auto& name = doc.var_get_default("OrderedSectionFormatter", ""); !name.empty())
		hdata.orderedSectionFormatter = Lisp::get_proc(static_cast<std::string>(name));
	//#+UnorderedSectionFormatter
	if (const auto& name = doc.var_get_default("UnorderedSectionFormatter", ""); !name.empty())
		hdata.unorderedSectionFormatter = Lisp::get_proc(static_cast<std::string>(name));


	auto& stream = m_opts.stream;
	stream << "<html>\n"
		   << "<head>\n"
		   << "\t<meta charset=\"UTF-8\">\n";

	// Title
	const std::string title = doc.var_get_default("Title", "");
	if (const std::string page_title = doc.var_get_default("PageTitle", ""); !page_title.empty())
		stream << fmt::format("\t<title>{}</title>\n", formatHTML(page_title));
	else if (!title.empty())
		stream << fmt::format("\t<title>{}</title>\n", formatHTML(title));

	// Css
	const std::string css = doc.var_get_default("CSS", "");
	if (!css.empty())
		stream << fmt::format("\t<link rel=\"stylesheet\" href=\"{}\">\n", formatHTML(css));

	stream << "\t</div>\n"
		   << "\t</center>\n"
		   << "\t<div id=\"content\">\n"
		   << "</head>\n"
		   << "<body>\n"
		   << "\t<center>\n"
		   << "\t<div id=\"header\">\n";

	// Title
	if (!title.empty())
		stream << fmt::format("\t\t<h1 class=\"title\">{}</h1>\n", formatHTML(title));

	// Author
	const std::string author = doc.var_get_default("Author", "");
	if (!author.empty())
		stream << fmt::format("\t\t<h1 class=\"author\">{}</h1>\n", formatHTML(author));

	// Date
	const std::string date = doc.var_get_default("Date", "");
	if (!date.empty())
		stream << fmt::format("\t\t<h1 class=\"date\">{}</h1>\n", formatHTML(date));

	stream << "\t</div>\n"
		   << "\t</center>\n"
		   << "\t<div id=\"content\">\n";

	// TOC
	const std::string toc = doc.var_get_default("TOC", "");
	if (!toc.empty() && !doc.get_header().empty())
	{
		stream << "\t\t<nav id=\"toc\">\n"
			   << fmt::format("\t\t\t<p class=\"toc-header\">{}</p>\n", formatHTML(toc));

		std::size_t depth = 0;
		for (const auto& [num, sec] : doc.get_header())
		{
			while (depth < sec->get<"level">())
				++depth, stream << "\t\t\t<ul>\n";
			while (depth > sec->get<"level">())
				--depth, stream << "\t\t\t</ul>\n";
			stream << fmt::format(
					"\t\t\t\t<li><a href=\"#{}\">{}</a></li>\n",
					get_anchor(sec->get<"title">()), formatHTML(sec->get<"title">()));
		}
		while (depth > 0)
			--depth, stream << "\t\t\t</ul>\n";
		stream << "\t\t</nav>\n";
	}

	generate(stream, doc.get_tree(), 2);

	stream << "\t</div>\n";

	// External Refs
	if (!doc.get_external_refs().empty())
	{
		stream << "\t<div id=\"references\">\n"
			   << fmt::format("\t\t<h1 class=\"external-ref\">{}</h1>\n", doc.var_get_default("ExternalRef", "References"))
			   << "\t\t<ul>\n";
		for (const auto ref : doc.get_external_refs())
		{
			if (ref->get<"author">().empty())
			{
				if (ref->get<"url">().empty())
					stream << fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"ref_{0}from\">^</a> <i>{1}</i></li>\n", ref->get<"num">(), ref->get<"desc">());
				else
					stream << fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"ref_{0}from\">^</a> <i><a class=\"link\" href=\"{1}\">{2}</a></i></li>\n",
								ref->get<"num">(), ref->get<"url">(), ref->get<"desc">());
			}
			else
			{
				if (ref->get<"url">().empty())
					stream << fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"ref_{0}from\">^</a> {1}, <i>{2}</i></li>\n", ref->get<"num">(), ref->get<"author">(),
								ref->get<"desc">());
				else
					stream << fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"#ref_{0}from\">^</a> {1}, <i><a class=\"link\" href=\"{2}\">{3}</a></i></li>\n",
								ref->get<"num">(), ref->get<"author">(), ref->get<"url">(), ref->get<"desc">());
			}
		}
		stream << "\t\t</ul>\n"
			   << "\t</div>\n";
	}
	

	// Footer
	stream << "</body>\n"
		   << "</html>\n";
}
