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

#ifndef NML_UTIL_HPP
#define NML_UTIL_HPP

#include <string>
#include <cstdint>
#include <source_location>
#include <array>
#include <algorithm>

namespace Colors
{
	extern bool enabled;

	 extern const std::string_view reset;
	 extern const std::string_view bold;
	 extern const std::string_view italic;
	 extern const std::string_view underline;
	 extern const std::string_view strike;
	 extern const std::string_view red;
	 extern const std::string_view green;
	 extern const std::string_view yellow;
	 extern const std::string_view blue;
	 extern const std::string_view magenta;
	 extern const std::string_view cyan;
} // Colors

/**
 * @brief Error exception
 *
 * Thrown when there is an error
 */
class Error
{
	std::string m_msg; ///< Error message

public:
	/**
	 * @brief Constructor
	 *
	 * @param msg Error message
	 * @param loc Location
	 */
	Error(const std::string& msg, const std::source_location& loc = std::source_location::current());

	/**
	 * @brief what()
	 *
	 * @returns Error message
	 */
	virtual std::string what() const throw();
};

/**
 * @brief Generates a SHA1 digest
 *
 * @param s Source string to hash
 * @returns SHA1 digest of s
 */
[[nodiscard]] std::string sha1(const std::string& s);

/**
 * @brief Constructs an array in place
 *
 * @param args Elements of the array
 * @tparam T Array's type
 * @returns An array of type T containing args
 */
template <class T, class... Args>
static constexpr auto make_array(Args&&... args) noexcept
{
	return std::array<T, sizeof...(Args)>{std::forward<Args>(args)...};
}

/**
 * @brief Replaces every matching character in string
 *
 * @param input Input string
 * @param replace Characters to replace/with
 */
template <std::size_t N>
static std::string replace_each(
		const std::string_view& input,
		const std::array<std::pair<char, std::string_view>, N>& replace)
{
	// Result string's size
	std::size_t new_size = input.size();
	for (const auto c : input)
	{
		[&]<std::size_t... i>(std::index_sequence<i...>)
		{
			((new_size += (c == replace[i].first) ? replace[i].second.size()-1 : 0), ...);
		}(std::make_index_sequence<N>{});
	}

	std::string result(new_size, '\0');
	std::size_t j = 0;
	for (const auto c : input)
	{
		auto fn = [&]<std::size_t i>() -> bool
		{
			if (c != replace[i].first)
				return false;

			for (const auto c : replace[i].second)
				result[j++] = c;

			return true;
		};

		[&]<std::size_t... i>(std::index_sequence<i...>)
		{
			if ( !((fn.template operator()<i>()) || ...) ) // Shortcut
				result[j++] = c;
		}(std::make_index_sequence<N>{});
	}

	return result;
}

class Document;
namespace Syntax { struct Latex; }
/**
 * @brief Generates svg data from Latex element
 *
 * @param doc Document where the Tex element is located
 * @param path Directory to render Tex files in
 * @param tex Tex element to generate LaTeX for
 * @returns <content, filename>
 */
[[nodiscard]] std::pair<std::string, std::string> Tex(const Document& doc, const std::string_view& path, const Syntax::Latex& tex);

template <std::size_t N>
struct StringLiteral
{
	/**
	 * Constructor
	 *
	 * @param literal String
	 */
	constexpr StringLiteral(const char (&literal)[N])
	{
		std::copy_n(literal, N, data);
	}

	/**
	 * Converts to ```std::string```
	 *
	 * @returns A ```std::string()``` with this' content
	 */
	explicit operator std::string() const
	{
		return std::string(data, N);
	}

	/**
	 * Converts to ```std::string_view```
	 *
	 * @returns A ```std::string_view()``` with this' content
	 */
	explicit constexpr operator std::string_view() const
	{
		return std::string_view(data, N);
	}

	/**
	 * Gets string's size
	 *
	 * @returns This string's size
	 */
	[[nodiscard]] static constexpr std::size_t size() noexcept
	{ return N; }

	char data[N]; ///< String's content

	/**
	 * Checks if two strings are equal
	 *
	 * @param s First string
	 * @param t Second string
	 * @returns true if ```s``` and ```t``` have the same content
	 * @note ```s == t``` iff ```t == s```
	 */
	template <std::size_t M>
	[[nodiscard]] constexpr friend bool operator==(const StringLiteral<N>& s, const StringLiteral<M>& t) noexcept
	{
		if constexpr (N != M)
			return false;

		return [&]<std::size_t... i>(std::index_sequence<i...>)
		{
			return ((s.data[i] == t.data[i]) && ...);
		}(std::make_index_sequence<N>{});
	}
};

template <class T, class... Us>
static inline constexpr bool is_one_of_v = (std::is_same_v<T, Us> || ...);

#endif // NML_UTIL_HPP
