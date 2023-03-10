#include "HTMLCompiler.hpp"
#include "Syntax.hpp"
#include "Highlight.hpp"

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

[[nodiscard]] std::string HTMLCompiler::format(const std::string_view& s)
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

	return fmt::format("{}{}{}", number, ".", sec.title);
}

MAKE_CENUM_Q(FigureType, std::uint8_t,
	PICTURE, 0,
	AUDIO, 1,
	VIDEO, 2,
);

[[nodiscard]] static FigureType getFigureType(const Syntax::Figure& fig)
{
	const auto pos{fig.path.rfind('.')};
	if (pos == std::string::npos) [[unlikely]]
		throw Error(fmt::format("Cannot determine type of figure '![{}]({})', missing extension", fig.name, fig.path));
	const std::string_view ext{fig.path.substr(pos+1)};
	if (ext.empty()) [[unlikely]]
		throw Error(fmt::format("Cannot determine type of figure '![{}]({})', missing extension", fig.name, fig.path));

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

	throw Error(fmt::format("Cannot determine type of figure '![{}]({})', unknown extension '{}'", fig.name, fig.path, ext));
}

[[nodiscard]] std::string HTMLCompiler::compile(const Document& doc, const CompilerOptions& opts) const
{
	HTMLData hdata;
	//{{{ generate
	std::function<std::string(const SyntaxTree&, std::size_t)> generate = [&](const SyntaxTree& tree, std::size_t depth) -> std::string
	{
		std::string html;
		auto append = [&](std::size_t depth, const std::string& s)
		{
			html.reserve(html.size() + depth);
			for (std::size_t i{0}; i < depth; ++i)
				html.push_back('\t');

			html.append(s);
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
				html.append("</p>\n");
			if (last_type == Syntax::FIGURE && elem->get_type() != Syntax::FIGURE)
				append(depth, "</div>\n");
			if (isTextLike(elem->get_type()) &&
					((last_type == Syntax::LATEX && elem->get_type() != Syntax::LATEX)
					 || (last_type == Syntax::ANNOTATION)))
				append(depth, "");
			if (!isTextLike(last_type) && isTextLike(elem->get_type()))
				append(depth, "<p>");

			switch (elem->get_type())
			{
				case Syntax::TEXT:
				{
					const auto& text = *reinterpret_cast<const Syntax::Text*>(elem);
					html.append(format(text.content));

					break;
				}
				case Syntax::STYLEPUSH:
				{
					const auto& p = *reinterpret_cast<const Syntax::StylePush*>(elem);
					switch (p.style)
					{
						case Syntax::Style::BOLD:
							html.append("<b>"sv); break;
						case Syntax::Style::UNDERLINE:
							html.append("<u>"sv); break;
						case Syntax::Style::ITALIC:
							html.append("<i>"sv); break;
						case Syntax::Style::VERBATIM:
							html.append("<em>"sv); break;
					}

					break;
				}
				case Syntax::STYLEPOP:
				{
					const auto& p = *reinterpret_cast<const Syntax::StylePop*>(elem);
					switch (p.style)
					{
						case Syntax::Style::BOLD:
							html.append("</b>"sv); break;
						case Syntax::Style::UNDERLINE:
							html.append("</u>"sv); break;
						case Syntax::Style::ITALIC:
							html.append("</i>"sv); break;
						case Syntax::Style::VERBATIM:
							html.append("</em>"sv); break;
					}

					break;
				}
				case Syntax::BREAK:
				{
					const auto& br = *reinterpret_cast<const Syntax::Break*>(elem);
					if (br.size == 0)
						break;

					html.push_back('\n');
					append(depth, "");
					for (std::size_t i = 0; i < br.size; ++i)
						html.append("<br>");
					html.push_back('\n');
					append(depth, "");
					break;
				}
				case Syntax::SECTION:
				{
					// TODO: Custom guile formatter
					// (define (section-bullet n i)
					//	"n: Section depth, i: Section number"
					//	...)
					const auto& section = *reinterpret_cast<const Syntax::Section*>(elem);
					//if (last_type == Syntax::SECTION)
					append(depth, "<br>\n");
					if (section.numbered)
					{
						while (sections.size() > section.level)
							sections.pop_back();
						while (sections.size() < section.level)
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
						html.append(fmt::format(fmt::runtime(f),
							std::min(1+section.level, 6ul),
							format((hdata.orderedSectionFormatter.has_value())
								? hdata.orderedSectionFormatter.value().call(Lisp::to_scm(section), Lisp::to_scm(sections))
								: getSectionFullName(section, sections)),
							get_anchor(section.title),
							hdata.section_link)
						);
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
						html.append(fmt::format(fmt::runtime(f),
							std::min(1+section.level, 6ul),
							format((hdata.unorderedSectionFormatter.has_value())
								? hdata.unorderedSectionFormatter.value().call(Lisp::to_scm(section), Lisp::to_scm(sections))
								: section.title),
							get_anchor(section.title),
							hdata.section_link)
						);
					}
					break;
				}
				case Syntax::LIST_BEGIN:
				{
					list_delimiters.push(reinterpret_cast<const Syntax::ListBegin*>(elem));
					append(depth, "<ul>\n");
					++depth;

					break;
				}
				case Syntax::LIST_END:
				{
					--depth;
					if (list_delimiters.empty()) [[unlikely]] // Not possible
						throw Error("End list delimiter, without begin");
					list_delimiters.pop();
					append(depth, "</ul>\n");
					break;
				}
				case Syntax::LIST_ENTRY:
				{
					if (list_delimiters.empty()) [[unlikely]] // Not possible
						throw Error("List without delimiter");

					const Syntax::ListEntry& ent = *reinterpret_cast<const Syntax::ListEntry*>(elem);
					const Syntax::ListBegin& delim = *list_delimiters.top();

					if (delim.ordered)
					{
						const Syntax::OrderedBullet& bullet = std::get<Syntax::OrderedBullet>(delim.bullet);
						if (delim.style.empty())
							append(depth, fmt::format("<li><a class=\"bullet\">{}{}{}</a>\n",
									format(bullet.left),
									format(bullet.get(ent.counter)),
									format(bullet.right)
								));
						else
							append(depth, fmt::format("<li><a class=\"bullet\" style=\"{}\">{}{}{}</a>\n",
									format(delim.style),
									format(bullet.left),
									format(bullet.get(ent.counter)),
									format(bullet.right)
								));
						
						html.append(generate(ent.content, depth+1));
					}
					else
					{
						const Syntax::UnorderedBullet& bullet = std::get<Syntax::UnorderedBullet>(delim.bullet);
						if (delim.style.empty())
							append(depth, fmt::format("<li><a class=\"bullet\">{}</a>\n",
										format(bullet.bullet)
									));
						else
							append(depth, fmt::format("<li><a class=\"bullet\" style=\"{}\">{}</a>\n",
										format(delim.style),
										format(bullet.bullet)
									));

						html.append(generate(ent.content, depth+1));
					}
					append(depth, fmt::format("</li>\n"));

					break;
				}
				case Syntax::RULER:
				{
					append(depth, "<hr>\n");
					break;
				}
				case Syntax::FIGURE:
				{
					const Syntax::Figure& fig = *reinterpret_cast<const Syntax::Figure*>(elem);
					if (last_type != Syntax::FIGURE)
						append(depth, "<div class=\"figures\">\n");
					append(depth+1, "<div class=\"figure\">\n");
					switch (getFigureType(fig))
					{
						case FigureType::PICTURE:
							append(depth+2, fmt::format("<a href=\"{0}\"><img src=\"{0}\"></a>\n", fig.path));
							break;
						case FigureType::AUDIO: // TODO
							break;
						case FigureType::VIDEO:
							append(depth+2, fmt::format("<video src=\"{}\" controls></video>\n", fig.path));
							break;
						default: break;
					}

					// Description
					append(depth+2, fmt::format("<p><b>({})</b></p>\n", fig.id));
					html.append(generate(fig.description, depth+2));
					append(depth+1, "</div>\n");
					break;
				}
				case Syntax::CODE:
				{
					const Syntax::Code& code = *reinterpret_cast<const Syntax::Code*>(elem);

					append(depth, "<div class=\"code\">\n");
					if (!code.name.empty())
						append(depth+1, fmt::format("<div class=\"code-title\">{}</div>\n", format(code.name)));
					append(depth+1, "<div class=\"code-content\">\n");
					html.append(Highlight<HighlightTarget::HTML>(code.content, code.language, code.style_file));
					append(depth+1, "</div>\n");
					append(depth, "</div>\n");
					break;
				}
				case Syntax::QUOTE:
				{
					const Syntax::Quote& quote = *reinterpret_cast<const Syntax::Quote*>(elem);

					append(depth, "<blockquote>\n");

					html.append(generate(quote.quote, depth+1));
					if (!quote.author.empty())
						append(depth+1, fmt::format("<p class=\"quote-author\">{}</p>\n",
									replace_each(quote.author.substr(0, quote.author.size()-1),
							std::array<std::pair<char, std::string_view>, 1>{
								std::make_pair<char, std::string_view>('\n', " "sv)
							})));

					append(depth, "</blockquote>\n");

					break;
				}
				case Syntax::REFERENCE:
				{
					const auto& ref = *reinterpret_cast<const Syntax::Reference*>(elem);
					const Syntax::Figure* fig = doc.figure_get(ref.referencing);
					if (!fig) [[unlikely]]
						throw Error(fmt::format("Could not find figure with name '{}'", ref.referencing));
					switch (getFigureType(*fig))
					{
						case FigureType::PICTURE:
							html.append(fmt::format("<b class=\"figure-ref\"><a class=\"figure-ref\">({})</a><img src=\"{}\"></b>",
										fig->id,
										format(fig->path)));
							break;
						default:
							throw Error("Figure references is supported for pictures only");
							break;
					}
					break;
				}
				case Syntax::LINK:
				{
					const Syntax::Link& link = *reinterpret_cast<const Syntax::Link*>(elem);
					html.append(fmt::format("<a class=\"link\" href=\"{}\">{}</a>", format(link.path), format(link.name)));
					break;
				}
				case Syntax::LATEX:
				{
					if (!opts.tex_enabled)
						break;
					const Syntax::Latex& tex = *reinterpret_cast<const Syntax::Latex*>(elem);
					auto&& [content, filename] = Tex(doc, opts.tex_dir, tex);
					if (isTextLike(last_type))
						html.append("\n");
					append(depth, content);
					html.append("\n");
					break;
				}
				case Syntax::RAW:
				{
					const Syntax::Raw& raw = *reinterpret_cast<const Syntax::Raw*>(elem);
					std::string replace(depth+1, '\t');
					replace.front() = '\n';
					append(depth, replace_each(raw.content,
						std::array<std::pair<char, std::string_view>, 1>{std::make_pair<char, std::string_view>('\n', replace)}));
					html.append("\n");
					break;
				}
				case Syntax::RAW_INLINE:
				{
					const Syntax::RawInline& raw = *reinterpret_cast<const Syntax::RawInline*>(elem);
					std::string replace(depth+1, '\t');
					replace.front() = '\n';
					if (isTextLike(last_type))
						html.append(replace_each(raw.content,
							std::array<std::pair<char, std::string_view>, 1>{std::make_pair<char, std::string_view>('\n', replace)}));
					else
						append(depth, replace_each(raw.content,
							std::array<std::pair<char, std::string_view>, 1>{std::make_pair<char, std::string_view>('\n', replace)}));
					break;
				}
				case Syntax::EXTERNAL_REF:
				{
					const Syntax::ExternalRef& ref = *reinterpret_cast<const Syntax::ExternalRef*>(elem);
					html.append(fmt::format("<sup><a class=\"external-ref\" id=\"ref_{0}from\" href=\"#ref_{0}\" alt=\"{1}\">[{0}]</a></sup>", ref.num, format(ref.desc)));
					break;
				}
				case Syntax::PRESENTATION:
				{
					const Syntax::Presentation& pres = *reinterpret_cast<const Syntax::Presentation*>(elem);
					if (pres.type == Syntax::PresType::CENTER)
					{
						append(depth, "<center>\n");
						html.append(generate(pres.content, depth+1));
						append(depth, "</center>\n");
					}
					else if (pres.type == Syntax::PresType::BOX)
					{
						append(depth, "<div class=\"box\">\n");
						html.append(generate(pres.content, depth+1));
						append(depth, "</div>\n");
					}
					else if (pres.type == Syntax::PresType::LEFT_LINE)
					{
						append(depth, "<div class=\"left-line\">\n");
						html.append(generate(pres.content, depth+1));
						append(depth, "</div>\n");
					}
					else
						throw Error(fmt::format("Unsupported presentation type : {}", pres.type));
					break;
				}
				case Syntax::ANNOTATION:
				{
					const Syntax::Annotation& anno = *reinterpret_cast<const Syntax::Annotation*>(elem);
					if (isTextLike(last_type))
						html.append("\n");

					append(depth, "<div class=\"annotation\">\n");
					html.append(generate(anno.name, depth+1));
					append(depth, "</div>\n");
					append(depth, "<div class=\"hide\">\n");
					html.append(generate(anno.content, depth+1));
					append(depth, "</div>\n");
					break;
				}
				case Syntax::CUSTOM_STYLEPUSH:
				{
					const auto& push = *reinterpret_cast<const Syntax::CustomStylePush*>(elem);

					html.append(push.style.begin.call());
					break;
				}
				case Syntax::CUSTOM_STYLEPOP:
				{
					const auto& push = *reinterpret_cast<const Syntax::CustomStylePop*>(elem);

					html.append(push.style.end.call());
					break;
				}
				case Syntax::CUSTOM_PRESPUSH:
				{
					const auto& push = *reinterpret_cast<const Syntax::CustomPresPush*>(elem);

					append(depth, push.pres.begin.call(Lisp::TypeConverter<std::size_t>::from(push.level)));
					html.append("\n");
					++depth;
					break;
				}
				case Syntax::CUSTOM_PRESPOP:
				{
					const auto& push = *reinterpret_cast<const Syntax::CustomPresPop*>(elem);

					--depth;
					append(depth, push.pres.end.call(Lisp::TypeConverter<std::size_t>::from(push.level)));
					html.append("\n");
					break;
				}
				default: break;
			}

			last_type = elem->get_type();
		});

		if (isTextLike(last_type))
			html.append("</p>\n");
		if (last_type == Syntax::FIGURE)
			append(depth, "</div>\n");

		return html;
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


	std::string html("<html>\n");
	html.append("<head>\n")
		.append("	<meta charset=\"UTF-8\">\n");

	// Title
	const std::string title = doc.var_get_default("Title", "");
	if (const std::string page_title = doc.var_get_default("PageTitle", ""); !page_title.empty())
		html.append(fmt::format("\t<title>{}</title>\n", format(page_title)));
	else if (!title.empty())
		html.append(fmt::format("\t<title>{}</title>\n", format(title)));

	// Css
	const std::string css = doc.var_get_default("CSS", "");
	if (!css.empty())
		html.append(fmt::format("\t<link rel=\"stylesheet\" href=\"{}\">\n", format(css)));

	html.append("	</div>\n")
		.append("	</center>\n")
		.append("	<div id=\"content\">\n");

	html.append("</head>\n")
		.append("<body>\n")
		.append("	<center>\n")
		.append("	<div id=\"header\">\n");

	// Title
	if (!title.empty())
		html.append(fmt::format("\t\t<h1 class=\"title\">{}</h1>\n", format(title)));

	// Author
	const std::string author = doc.var_get_default("Author", "");
	if (!author.empty())
		html.append(fmt::format("\t\t<h1 class=\"author\">{}</h1>\n", format(author)));

	// Date
	const std::string date = doc.var_get_default("Date", "");
	if (!date.empty())
		html.append(fmt::format("\t\t<h1 class=\"date\">{}</h1>\n", format(date)));

	html.append("	</div>\n")
		.append("	</center>\n")
		.append("	<div id=\"content\">\n");

	// TOC
	const std::string toc = doc.var_get_default("TOC", "");
	if (!toc.empty() && !doc.get_header().empty())
	{
		html.append("		<nav id=\"toc\">\n")
			.append(fmt::format("\t\t\t<p class=\"toc-header\">{}</p>\n", toc));

		std::size_t depth = 0;
		for (const auto& [num, sec] : doc.get_header())
		{
			while (depth < sec->level)
				++depth, html.append("\t\t\t<ul>\n");
			while (depth > sec->level)
				--depth, html.append("\t\t\t</ul>\n");
			html.append(fmt::format(
					"\t\t\t\t<li><a href=\"#{}\">{}</a></li>\n",
					get_anchor(sec->title), format(sec->title)));
		}
		while (depth > 0)
			--depth, html.append("\t\t\t</ul>\n");
		html.append("		</nav>\n");
	}

	html.append(generate(doc.get_tree(), 2));

	html.append("	</div>\n");

	// External Refs
	if (!doc.get_external_refs().empty())
	{
		html.append("	<div id=\"references\">\n")
			.append(fmt::format("\t\t<h1 class=\"external-ref\">{}</h1>\n", doc.var_get_default("ExternalRef", "References")))
			.append("		<ul>\n");
		for (const auto ref : doc.get_external_refs())
		{
			if (ref->author.empty())
			{
				if (ref->url.empty())
					html.append(fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"ref_{0}from\">^</a> <i>{1}</i></li>\n", ref->num, ref->desc));
				else
					html.append(fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"ref_{0}from\">^</a> <i><a class=\"link\" href=\"{1}\">{2}</a></i></li>\n", ref->num, ref->url, ref->desc));
			}
			else
			{
				if (ref->url.empty())
					html.append(fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"ref_{0}from\">^</a> {1}, <i>{2}</i></li>\n", ref->num, ref->author, ref->desc));
				else
					html.append(fmt::format("\t\t\t<li id=\"ref_{0}\">{0}. <a href=\"#ref_{0}from\">^</a> {1}, <i><a class=\"link\" href=\"{2}\">{3}</a></i></li>\n", ref->num, ref->author, ref->url, ref->desc));
			}
		}
		html.append("		</ul>\n")
			.append("	</div>\n");
	}
	

	// Footer
	html.append("</body>\n")
		.append("</html>\n");

	return html;
}
