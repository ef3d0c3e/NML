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

#include "Syntax.hpp"
#include <utility>
#include <ranges>
#include <fmt/format.h>

using namespace std::literals;

[[nodiscard]] bool Syntax::isCustomType(Type type) noexcept
{
	return type >= Type::CUSTOM_STYLEPUSH;
}

[[nodiscard]] std::string_view Syntax::getTypeName(Type type) noexcept
{
	static constexpr auto names = make_array<std::string_view>(
		"Text",
		"StylePush",
		"StylePop",
		"Break",
		"Section",
		"List Begin",
		"List End",
		"List Entry",
		"Ruler",
		"Figure",
		"Code",
		"Quote",
		"Reference",
		"Link",
		"Latex",
		"Raw",
		"Raw Inline",
		"External Reference",
		"Presentation",
		"Annotation",

		"Custom Style Push",
		"Custom Style Pop",
		"Custom Presentation Push",
		"Custom Presentation Pop"
	);

	return names[type];
}

[[nodiscard]] std::string_view Syntax::getStyleName(Style style) noexcept
{
	switch (style)
	{
		case Style::NONE:
			return "None"sv;
		case Style::BOLD:
			return "Bold"sv;
		case Style::UNDERLINE:
			return "Underline"sv;
		case Style::ITALIC:
			return "Italic"sv;
		case Style::VERBATIM:
			return "Verbatim"sv;
		default:
			return "Invalid Style"sv;
	}
}

void Syntax::forEveryStyle(Syntax::Style s, std::function<void(Syntax::Style)> fn, bool reverse)
{
	if (reverse)
		[&]<std::size_t... i>(std::index_sequence<i...>)
		{
			(( (s & Syntax::Style::get<Syntax::Style::size - i-1>()) ? fn(Syntax::Style::get<Syntax::Style::size - i-1>()) : void() ), ...);
		}(std::make_index_sequence<Syntax::Style::size-1>{});
	else
		[&]<std::size_t... i>(std::index_sequence<i...>)
		{
			(( (s & Syntax::Style::get<i+1>()) ? fn(Syntax::Style::get<i+1>()) : void() ), ...);
		}(std::make_index_sequence<Syntax::Style::size-1>{});
}

[[nodiscard]] std::string Syntax::OrderedBullet::is_representible(std::size_t n) const
{
	switch (bullet)
	{
		case ALPHA:
		case ALPHA_CAPITAL:
			if (n == 0)
				return "`0` cannot be represented using letters";
			else if (n > 26)
				return fmt::format("`{}` exceeds the highest representible letter (26 = 'Z')", n);
			break;
		case ROMAN:
		case ROMAN_CAPITAL:
		case PEX:
			if (n == 0)
				return "`0` cannot be represented using roman numerals";
			else if (n > 3999)
				return fmt::format("`{}` exceeds the highest representible roman numeral (3999 = 'MMMCMXCIX')", n);
			break;
		default:
			break;
	}

	return "";
}

[[nodiscard]] std::string Syntax::OrderedBullet::get(std::size_t n) const
{
	static constexpr auto roman_dec = make_array<std::size_t>(1000ul, 900ul, 500ul, 400ul, 100ul, 90ul, 50ul, 40ul, 10ul, 9ul, 5ul, 4ul, 1ul);

	switch (bullet)
	{
		case NUMBER:
			return std::to_string(n);
		case ALPHA:
			return std::string(1, 'a'+n-1);
		case ALPHA_CAPITAL:
			return std::string(1, 'A'+n-1);
		case ROMAN:
		{
			std::string ret;

			static constexpr auto roman = make_array<std::string_view>("m"sv, "cm"sv, "d"sv, "cd"sv, "c"sv, "xc"sv, "l"sv, "xl"sv, "x"sv, "ix"sv, "v"sv, "iv"sv, "i"sv);
			std::size_t i{0};

			while(n)
			{
				while(n / roman_dec[i])
				{
					ret.append(roman[i]);
					n -= roman_dec[i];
				}
				i++;
			}

			return ret;
		}
		case ROMAN_CAPITAL:
		{
			std::string ret;

			static constexpr auto roman = make_array<std::string_view>("M"sv, "CM"sv, "D"sv, "CD"sv, "C"sv, "XC"sv, "L"sv, "XL"sv, "X"sv, "IX"sv, "V"sv, "IV"sv, "I"sv);
			std::size_t i{0};

			while(n)
			{
				while(n / roman_dec[i])
				{
					ret.append(roman[i]);
					n -= roman_dec[i];
				}
				i++;
			}

			return ret;
		}
		case PEX:
		{
			std::string ret;

			static constexpr auto roman = make_array<std::string_view>("m"sv, "cm"sv, "d"sv, "cd"sv, "c"sv, "xc"sv, "l"sv, "xl"sv, "x"sv, "•x"sv, "v"sv, "•v"sv, "•"sv);
			std::size_t i{0};

			while(n)
			{
				while(n / roman_dec[i])
				{
					ret.append(roman[i]);
					n -= roman_dec[i];
				}
				i++;
			}

			return ret;
		}
		default:
			return "";
			break;
	}

	return "";
}

[[nodiscard]] std::string ProxyVariable::to_string(const Document& doc) const noexcept
{
	return doc.var_get_default(proxy, "error");
}

[[nodiscard]] SyntaxTree Lisp::TypeConverter<SyntaxTree>::to(SCM v)
{
	SyntaxTree tree;
	const std::size_t len = TypeConverter<std::size_t>::to(scm_length(v));
	for (const auto i : std::ranges::iota_view{0uz, len})
		tree.elems.push_back(std::shared_ptr<Syntax::Element>{TypeConverter<Syntax::Element*>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(i)))});

	return tree;
}

[[nodiscard]] SCM Lisp::TypeConverter<SyntaxTree>::from(const SyntaxTree& tree)
{
	if (tree.elems.empty())
		return SCM_EOL;
	SCM list = scm_make_list(TypeConverter<std::size_t>::from(tree.elems.size()), SCM_EOL);
	std::for_each(tree.elems.cbegin(), tree.elems.cend(), [&list, i = 0](const auto& spelem) mutable
	{
		scm_list_set_x(list, TypeConverter<std::size_t>::from(i++), TypeConverter<Syntax::Element*>::from(spelem.get()));
	});

	return list;
}
