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

#ifndef NML_LISP_HPP
#define NML_LISP_HPP

#include <string>
#include <optional>
#include <libguile.h>
#include <fmt/format.h>
#include <vector>
#include <deque>
#include "Util.hpp"
#include <iostream>

class Document;
struct File;
struct ParserData;
struct Parser;

namespace Lisp
{
template <class, class...>
struct TypeConverter;

template <>
struct TypeConverter<SCM>
{
	[[nodiscard]] static SCM from(SCM v)
	{
		return v;
	}
};

// String
template <>
struct TypeConverter<std::string>
{
	[[nodiscard]] static std::string to(SCM v)
	{
		return std::string{scm_to_locale_string(v)};
	}
	[[nodiscard]] static SCM from(const std::string& v)
	{
		return scm_from_locale_stringn(v.data(), v.size());
	}
};

// String view
template <>
struct TypeConverter<std::string_view>
{
	[[nodiscard]] static std::string to(SCM v)
	{
		return std::string{scm_to_locale_string(v)};
	}
	[[nodiscard]] static SCM from(const std::string_view& v)
	{
		return scm_from_locale_stringn(v.data(), v.size());
	}
};

// Integers type
template <class T>
requires(std::is_integral_v<T>)
struct TypeConverter<T>
{
	[[nodiscard]] static T to(SCM v)
	{
		if constexpr (std::is_same_v<T, bool>)
			return scm_to_bool(v);
		else if constexpr (std::is_same_v<T, char>)
			return scm_to_char(v);
		else if constexpr (std::is_same_v<T, std::uint8_t>)
			return scm_to_uint8(v);
		else if constexpr (std::is_same_v<T, std::int8_t>)
			return scm_to_int8(v);
		else if constexpr (std::is_same_v<T, std::uint16_t>)
			return scm_to_uint16(v);
		else if constexpr (std::is_same_v<T, std::int16_t>)
			return scm_to_int16(v);
		else if constexpr (std::is_same_v<T, std::uint32_t>)
			return scm_to_uint32(v);
		else if constexpr (std::is_same_v<T, std::int32_t>)
			return scm_to_int32(v);
		else if constexpr (std::is_same_v<T, std::uint64_t>)
			return scm_to_uint64(v);
		else if constexpr (std::is_same_v<T, std::int64_t>)
			return scm_to_int64(v);
	}
	[[nodiscard]] static SCM from(const T& x)
	{
		if constexpr (std::is_same_v<T, bool>)
			return scm_from_bool(x);
		else if constexpr (std::is_same_v<T, char>)
			return scm_from_char(x);
		else if constexpr (std::is_same_v<T, std::uint8_t>)
			return scm_from_uint8(x);
		else if constexpr (std::is_same_v<T, std::int8_t>)
			return scm_from_int8(x);
		else if constexpr (std::is_same_v<T, std::uint16_t>)
			return scm_from_uint16(x);
		else if constexpr (std::is_same_v<T, std::int16_t>)
			return scm_from_int16(x);
		else if constexpr (std::is_same_v<T, std::uint32_t>)
			return scm_from_uint32(x);
		else if constexpr (std::is_same_v<T, std::int32_t>)
			return scm_from_int32(x);
		else if constexpr (std::is_same_v<T, std::uint64_t>)
			return scm_from_uint64(x);
		else if constexpr (std::is_same_v<T, std::int64_t>)
			return scm_from_int64(x);
	}
};

// Double
template <class T>
requires(std::is_same_v<T, double>)
struct TypeConverter<T>
{
	[[nodiscard]] static T to(SCM v)
	{
		return scm_to_double(v);
	}
	[[nodiscard]] static SCM from(const T& x)
	{
		return scm_from_double(x);
	}
};

// Containers
template <template <class...> class Container, class T, class... Ts>
requires (std::is_same_v<Container<T, Ts...>, std::deque<T, Ts...>>
		|| std::is_same_v<Container<T, Ts...>, std::vector<T, Ts...>>)
struct TypeConverter<Container<T, Ts...>>
{
	using cv = TypeConverter<T>;

	[[nodiscard]] static Container<T, Ts...> to(SCM v)
	{
		Container<T, Ts...> vec;
		const std::size_t len = scm_ilength(v);
		if constexpr (std::is_same_v<Container<T, Ts...>, std::vector<T, Ts...>>)
			vec.reserve(len);
		for (std::size_t i = 0; i < len; ++i)
			vec.push_back(cv::to(scm_list_ref(v, scm_from_unsigned_integer(i))));

		return vec;
	}

	[[nodiscard]] static SCM from(const Container<T, Ts...>& vec)
	{
		if (vec.empty())
			return SCM_EOL;
		SCM list = scm_make_list(TypeConverter<std::size_t>::from(vec.size()), SCM_EOL);
		std::for_each(vec.cbegin(), vec.cend(), [&list, i = 0](const T& t) mutable
		{
			scm_list_set_x(list, TypeConverter<std::size_t>::from(i++), cv::from(t));
		});

		return list;
	}
};

template <StringLiteral TypeName, class T>
struct TypeField
{
	inline static constexpr auto name = TypeName;
	using type = T;
};

template <class...>
struct TypeBuilder;

template <class T, StringLiteral... FNames, class... FTypes>
struct TypeBuilder<T, TypeField<FNames, FTypes>...>
{
};

struct Closure;

void init(Document& doc, const File& f, ParserData& data, Parser& parser);
void eval(const std::string& s, Document& doc, ParserData& data, Parser& parser);
std::string eval_r(const std::string& s, Document& doc, ParserData& data, Parser& parser);

/**
 * @brief Gets string representation of value
 *
 * @param x SCM to convert to string
 * @returns String representation
 */
[[nodiscard]] std::string to_string(SCM x);

/**
 * @brief Converts value to SCM
 *
 * @param v Value to convert
 * @returns SCM representation of v
 */
template <class T>
[[nodiscard]] SCM to_scm(const T& v)
{
	return TypeConverter<T>::from(v);
}

/**
 * @brief Wrapper around procedures
 */
struct Proc
{
	SCM proc;

	Proc():
		proc(SCM_UNDEFINED) {}

	Proc(Proc&& other):
		proc(std::move(other.proc)) {}

	Proc(const Proc& other):
		proc(other.proc) {}

	Proc(SCM&& proc):
		proc(std::move(proc)) {}

	Proc(const SCM& proc):
		proc(proc) {}

	Proc& operator=(const Proc& proc) noexcept
	{
		this->proc = proc.proc;
		return *this;
	}

	template <class... Args>
	std::string call(Args&&... args) const noexcept
	{
		if constexpr (sizeof...(Args) > 3)
			[]<bool v = true>(){ static_assert(v, "Invalid number of arguments for scm_call."); }();

		if constexpr (sizeof...(Args) == 0)
			return to_string(scm_call_0(proc));
		else if constexpr (sizeof...(Args) == 1)
			return to_string(scm_call_1(proc, std::forward<Args>(args)...));
		else if constexpr (sizeof...(Args) == 2)
			return to_string(scm_call_2(proc, std::forward<Args>(args)...));
		else if constexpr (sizeof...(Args) == 3)
			return to_string(scm_call_3(proc, std::forward<Args>(args)...));
	}

	template <class C, class... Args>
	auto call_cv(Args&&... args) const noexcept
	{
		if constexpr (sizeof...(Args) > 3)
			[]<bool v = true>(){ static_assert(v, "Invalid number of arguments for scm_call."); }();

		if constexpr (sizeof...(Args) == 0)
			return TypeConverter<C>::to(scm_call_0(proc));
		else if constexpr (sizeof...(Args) == 1)
			return TypeConverter<C>::to(scm_call_1(proc, std::forward<Args>(args)...));
		else if constexpr (sizeof...(Args) == 2)
			return TypeConverter<C>::to(scm_call_2(proc, std::forward<Args>(args)...));
		else if constexpr (sizeof...(Args) == 3)
			return TypeConverter<C>::to(scm_call_3(proc, std::forward<Args>(args)...));
	}
};

template <>
struct TypeConverter<Proc>
{
	[[nodiscard]] static Proc to(SCM v)
	{
		return Proc{v};
	}
	[[nodiscard]] static SCM from(const Proc& p)
	{
		return p.proc;
	}
};

/**
 * @brief Gets procedure from name
 *
 * @param name Procedure's name
 * @returns Procedure if found
 */
std::optional<Proc> get_proc(const std::string& name);
/**
 * @brief Gets whether a symbol exists
 *
 * @param name Symbol's name
 * @returns True if symbol found
 */
bool symbol_exists(const std::string& name);
} // Lisp

#endif // NML_LISP_HPP
