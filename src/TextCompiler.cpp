#include "TextCompiler.hpp"
#include "Syntax.hpp"
#include <fmt/format.h>

using namespace std::literals;

[[nodiscard]] std::string TextCompiler::get_name() const
{
	return "Text";
}

[[nodiscard]] bool TextCompiler::var_reserved(const std::string& name) const
{
	return false;
}

[[nodiscard]] std::string TextCompiler::var_check(const std::string& name, const std::string& value) const
{
	return ""s;
}

static void append(std::string& s, const Syntax::Element* elem, const std::string& content, std::size_t depth)
{
	for (std::size_t i = 0; i < depth; ++i)
		s.append("  ");
	s.append(fmt::format("[{}]: {}", Syntax::getTypeName(elem->get_type()), content));
	s.append("\n");
}

[[nodiscard]] std::string TextCompiler::compile(const Document& doc, const CompilerOptions& opts) const
{
	std::function<std::string(const SyntaxTree&, std::size_t)> generate = [&](const SyntaxTree& tree, std::size_t depth) -> std::string
	{
		std::string s;

		s.append(fmt::format("{: <{}}{{\n", "", depth*2));
		tree.for_each_elem([&](const Syntax::Element* elem)
		{
			switch (elem->get_type())
			{
				case Syntax::TEXT:
				{
					const Syntax::Text& text = *reinterpret_cast<const Syntax::Text*>(elem);

					std::string style;
					append(s, elem, fmt::format("\"{}\" {}", text.get<"content">(), style), depth+1);
					break;
				}
				case Syntax::STYLEPUSH:
				{
					const Syntax::StylePush& p = *reinterpret_cast<const Syntax::StylePush*>(elem);

					append(s, elem, fmt::format("{}", Syntax::getStyleName(p.get<"style">())), depth+1);
					break;
				}
				case Syntax::STYLEPOP:
				{
					const Syntax::StylePush& p = *reinterpret_cast<const Syntax::StylePush*>(elem);

					append(s, elem, fmt::format("{}", Syntax::getStyleName(p.get<"style">())), depth+1);
					break;
				}
				case Syntax::BREAK:
				{
					const Syntax::Break& br = *reinterpret_cast<const Syntax::Break*>(elem);

					append(s, elem, fmt::format("{}", br.get<"size">()), depth+1);
					break;
				}
				case Syntax::SECTION:
				{
					const Syntax::Section& sec = *reinterpret_cast<const Syntax::Section*>(elem);
					append(s, elem, fmt::format("{}+{} - {} ", sec.get<"numbered">() ? "ord" : "unord", sec.get<"toc">() ? "toc" : "notoc", sec.get<"title">()), depth+1);
					break;
				}
				case Syntax::LIST_BEGIN:
				{
					const Syntax::ListBegin& lb = *reinterpret_cast<const Syntax::ListBegin*>(elem);
					if (lb.get<"ordered">())
					{
						const auto& ord = std::get<Syntax::OrderedBullet>(lb.get<"bullet">());
						append(s, elem, fmt::format("(ord) st=({}) bullet=({}+{}+{})", lb.get<"style">(), ord.left, ord.bullet.value, ord.right), depth+1);
					}
					else
					{
						const auto& unord = std::get<Syntax::UnorderedBullet>(lb.get<"bullet">());
						append(s, elem, fmt::format("(unord) st=({}) bullet=({})", lb.get<"style">(), unord.bullet), depth+1);
					}
					break;
				}
				case Syntax::LIST_END:
				{
					const Syntax::ListEnd& le = *reinterpret_cast<const Syntax::ListEnd*>(elem);
					append(s, elem, fmt::format("({})", le.get<"ordered">() ? "ord" : "unord"), depth+1);
					break;
				}
				case Syntax::LIST_ENTRY:
				{
					const Syntax::ListEntry& ent = *reinterpret_cast<const Syntax::ListEntry*>(elem);
					append(s, elem, fmt::format("{}", ent.get<"counter">()), depth+1);
					s.append(generate(ent.get<"content">(), depth+1));
					break;
				}
				case Syntax::RULER:
				{
					const Syntax::Ruler& ruler = *reinterpret_cast<const Syntax::Ruler*>(elem);
					append(s, elem, fmt::format("{} ", ruler.get<"length">()), depth+1);
					break;
				}
				case Syntax::FIGURE:
				{
					const Syntax::Figure& fig = *reinterpret_cast<const Syntax::Figure*>(elem);
					append(s, elem, fmt::format("name=({}) path=({}) id={}", fig.get<"name">(), fig.get<"path">(), fig.get<"id">()), depth+1);
					s.append(generate(fig.get<"description">(), depth+1));
					break;
				}
				case Syntax::CODE:
				{
					const Syntax::Code& code = *reinterpret_cast<const Syntax::Code*>(elem);
					append(s, elem, fmt::format("lang=({}) name=({}) style_file=({})", code.get<"language">(), code.get<"name">(), code.get<"style_file">()), depth+1);
					break;
				}
				case Syntax::QUOTE:
				{
					const Syntax::Quote& quote = *reinterpret_cast<const Syntax::Quote*>(elem);
					append(s, elem, fmt::format("author=({})", quote.get<"author">()), depth+1);
					s.append(generate(quote.get<"quote">(), depth+1));
					break;
				}
				case Syntax::REFERENCE:
				{
					const Syntax::Reference& ref = *reinterpret_cast<const Syntax::Reference*>(elem);
					append(s, elem, fmt::format("referencing=({}) name=({}) reftype={}", ref.get<"referencing">(), ref.get<"name">(), ref.get<"type">().value), depth+1);
					break;
				}
				case Syntax::LINK:
				{
					const Syntax::Link& link = *reinterpret_cast<const Syntax::Link*>(elem);
					append(s, elem, fmt::format("name=({}) path=({})", link.get<"name">(), link.get<"path">()), depth+1);
					break;
				}
				case Syntax::LATEX:
				{
					const Syntax::Latex& tex = *reinterpret_cast<const Syntax::Latex*>(elem);
					if (tex.get<"mode">() == Syntax::TexMode::MATH)
						append(s, elem, fmt::format("math code=({}) filename=({})", tex.get<"content">(), tex.get<"filename">()), depth+1);
					else if (tex.get<"mode">() == Syntax::TexMode::NORMAL)
						append(s, elem, fmt::format("normal code=({}) filename=({})", tex.get<"content">(), tex.get<"filename">()), depth+1);
					break;
				}
				case Syntax::RAW:
				{
					const Syntax::Raw& raw = *reinterpret_cast<const Syntax::Raw*>(elem);
					append(s, elem, fmt::format("content=({})", raw.get<"content">()), depth+1);
					break;
				}
				case Syntax::RAW_INLINE:
				{
					const Syntax::RawInline& raw = *reinterpret_cast<const Syntax::RawInline*>(elem);
					append(s, elem, fmt::format("content=({})", raw.get<"content">()), depth+1);
					break;
				}
				case Syntax::EXTERNAL_REF:
				{
					const Syntax::ExternalRef& eref = *reinterpret_cast<const Syntax::ExternalRef*>(elem);
					append(s, elem, fmt::format("[{}] desc=({}) author=({}) url=({})", eref.get<"num">(), eref.get<"desc">(), eref.get<"author">(), eref.get<"url">()), depth+1);
					break;
				}
				case Syntax::PRESENTATION:
				{
					const Syntax::Presentation& pres = *reinterpret_cast<const Syntax::Presentation*>(elem);
					append(s, elem, fmt::format("{}", pres.get<"type">().value), depth+1);
					s.append(generate(pres.get<"content">(), depth+1));
					break;
				}
				case Syntax::ANNOTATION:
				{
					const Syntax::Annotation& anno = *reinterpret_cast<const Syntax::Annotation*>(elem);
					append(s, elem, "", depth+1);
					s.append(generate(anno.get<"name">(), depth+1));
					s.append(generate(anno.get<"content">(), depth+1));
					break;
				}
				case Syntax::CUSTOM_STYLEPUSH:
				{
					const Syntax::CustomStylePush& push = *reinterpret_cast<const Syntax::CustomStylePush*>(elem);

					append(s, elem, fmt::format("({}) {}", push.get<"style">().type_name, push.get<"style">().begin.call()), depth+1);
					break;
				}
				case Syntax::CUSTOM_STYLEPOP:
				{
					const Syntax::CustomStylePop& pop = *reinterpret_cast<const Syntax::CustomStylePop*>(elem);

					append(s, elem, fmt::format("({}) {}", pop.get<"style">().type_name, pop.get<"style">().end.call()), depth+1);
					break;
				}
				case Syntax::CUSTOM_PRESPUSH:
				{
					const Syntax::CustomPresPush& push = *reinterpret_cast<const Syntax::CustomPresPush*>(elem);

					append(s, elem, fmt::format("({}) {}", push.get<"pres">().type_name, push.get<"pres">().begin.call(Lisp::TypeConverter<std::size_t>::from(push.get<"level">()))), depth+1);
					++depth;
					break;
				}
				case Syntax::CUSTOM_PRESPOP:
				{
					const Syntax::CustomPresPop& pop = *reinterpret_cast<const Syntax::CustomPresPop*>(elem);

					append(s, elem, fmt::format("({}) {}", pop.get<"pres">().type_name, pop.get<"pres">().end.call(Lisp::TypeConverter<std::size_t>::from(pop.get<"level">()))), depth);
					--depth;
					break;
				}
				default:
					append(s, elem, "", depth+1);
					break;
			}
		});
		s.append(fmt::format("{: <{}}}}\n", "", depth*2));

		return s;
	};

	return generate(doc.get_tree(), 0);
}

