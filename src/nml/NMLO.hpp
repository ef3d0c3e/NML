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

#ifndef NML_NMLO_HPP
#define NML_NMLO_HPP

#include <algorithm>
#include <cstdint>
#include <string_view>
#include "Lisp.hpp"
#include "Cenum.hpp"
#include "sha512.hpp"

namespace Syntax
{
/**
 * @brief Type of syntax elemnts
 */
MAKE_CENUMV_Q(Type, std::uint8_t,
	TEXT, 0,
	STYLEPUSH, 1,
	STYLEPOP, 2,
	BREAK, 3,
	SECTION, 4,
	LIST_BEGIN, 5,
	LIST_END, 6,
	LIST_ENTRY, 7,
	RULER, 8,
	FIGURE, 9,
	CODE, 10,
	QUOTE, 11,
	REFERENCE, 12,
	LINK, 13,
	LATEX, 14,
	RAW, 15,
	RAW_INLINE, 16,
	EXTERNAL_REF, 17,
	PRESENTATION, 18,
	ANNOTATION, 19,

	/* User-Defined */
	CUSTOM_STYLEPUSH, 20,
	CUSTOM_STYLEPOP, 21,
	CUSTOM_PRESPUSH, 22,
	CUSTOM_PRESPOP, 23,
);

/**
 * @brief Abstract class for an element
 */
struct Element
{
	[[nodiscard]] virtual Type get_type() const noexcept = 0;
	[[nodiscard]] virtual std::string serialize() const = 0;
	[[nodiscard]] virtual std::string hash() const = 0;

	virtual ~Element() {};
};
}

MAKE_CENUM_Q(FieldAccess, std::uint8_t,
	READ, 0b1,
	WRITE, 0b10,

	NONE, 0b0,
	DEFAULT, WRITE | READ
);

namespace NMLOHelper
{
	/**
	 * @brief Gets the lisp name for function/field
	 * @param prefix Prefix
	 * @param name Name
	 *
	 * @returns "nmlo-<prefix>-<name>" (hypgen snake case)
	 */
	[[nodiscard]] std::string getLispStringName(const std::string_view prefix, const std::string_view name);

	namespace detail
	{
		template<class T, template <class...> class Tmpl, auto Check>
		struct ser_container : std::false_type {};
		template<template <class...> class Tmpl, class T, class... Ts, auto Check>
		struct ser_container<Tmpl<T, Ts...>, Tmpl, Check> : std::conditional_t<
			Check.template operator()<T>(),
			std::true_type, std::false_type> {};

		template<class T, template <class...> class Tmpl, auto Check>
		struct ser_tuple : std::false_type {};
		template<template <class...> class Tmpl, class... Ts, auto Check>
		struct ser_tuple<Tmpl<Ts...>, Tmpl, Check> : std::conditional_t<
			Check.template operator()<Ts...>(),
			std::true_type, std::false_type> {};

		template <class T>
		struct ser : std::conditional_t<
			is_one_of_v<T, std::string, std::string_view>
			|| std::is_integral_v<T>
			|| std::is_floating_point_v<T>
			|| (std::is_nothrow_convertible_v<T, std::uint8_t> && sizeof(T) == sizeof(std::uint8_t))
			|| ser_container<T, std::vector, []<class... Ts> consteval { return (ser<Ts>::value && ...); }>::value
			|| ser_tuple<T, std::tuple, []<class... Ts> consteval { return (ser<Ts>::value && ...); }>::value
			|| ser_tuple<T, std::pair, []<class... Ts> consteval { return (ser<Ts>::value && ...); }>::value,
			std::true_type, std::false_type> {};

		template <class T, template <class...> class Tmpl>
		struct is_class_template : std::false_type {};
		template <template <class...> class Tmpl, class... Ts>
		struct is_class_template<Tmpl<Ts...>, Tmpl> : std::true_type {};

	}

	template <class T>
	concept Serializable = detail::ser<T>::value;

	template <class T>
	concept SerializableContainer = Serializable<T>
		&& detail::is_class_template<T, std::vector>::value;

	template <class T>
	concept SerializableTuple = Serializable<T>
		&& (detail::is_class_template<T, std::pair>::value
		|| detail::is_class_template<T, std::tuple>::value);

	/**
	 * @brief Utility to serialize a type
	 *
	 * @param v Value to serialize
	 * @return Serialized value
	 */
	template <class T>
	[[nodiscard]] static std::string serialize(const T& v) noexcept
	{
		std::string data;

		if constexpr (is_one_of_v<T, std::string, std::string_view>) // Strings
		{
			data.resize(v.size());
			std::copy(v.cbegin(), v.cend(), data.begin());
		}
		else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T> ||
				(std::is_nothrow_convertible_v<T, std::uint8_t> && sizeof(T) == sizeof(std::uint8_t)) // For CENUM
			)
		{
			data.resize(sizeof(T));
			std::copy((const char*)&v, (const char*)&v+sizeof(T), data.begin());
		}
		else if constexpr (SerializableContainer<T>)
		{
			std::for_each(v.cbegin(), v.cend(), [&data](const auto& value) { data.append(serialize(value)); });
		}
		else if constexpr (SerializableTuple<T>)
		{
			[&]<std::size_t... i>(std::index_sequence<i...>)
			{
				((data.append( serialize(std::get<i>(v)) )), ...);
			}(std::make_index_sequence<std::tuple_size_v<T>>{});
		}
		else
		{
			[]<bool v = false> { static_assert(v, "Type not serializable"); }();
		}
		// TODO Also serialize custom pres, style & syntaxtree

		return data;
	}
} // NMLOHelper

template <std::size_t N>
struct Literal
{
	char data[N];

	[[nodiscard]] constexpr Literal(const char (&literal)[N])
	{
		std::copy_n(literal, N, data);
	}

	[[nodiscard]] constexpr operator std::string_view() const noexcept
	{
		return std::string_view{data, N-1};
	}

	/**
	 * Checks if two strings are equal
	 *
	 * @param s First string
	 * @param t Second string
	 * @returns true if ```s``` and ```t``` have the same content
	 * @note ```s == t``` iff ```t == s```
	 */
	template <std::size_t M>
	[[nodiscard]] constexpr friend bool operator==(const Literal<N>& s, const Literal<M>& t) noexcept
	{
		if constexpr (N != M)
			return false;

		return [&]<std::size_t... i>(std::index_sequence<i...>)
		{
			return ((s.data[i] == t.data[i]) && ...);
		}(std::make_index_sequence<N>{});
	}
};

template <class T, Literal Name, FieldAccess Access = FieldAccess::DEFAULT>
class NMLOField
{
	using type = T;
	static constinit inline Literal name = Name;
	static constinit inline FieldAccess access = Access;
};

template <Syntax::Type, Literal, class...>
class NMLO;

template <Syntax::Type Type, Literal Name, class... Ts, Literal... Ns, FieldAccess... As>
class NMLO<Type, Name, NMLOField<Ts, Ns, As>...> : public Syntax::Element
{
	template <class T>
		requires std::is_base_of_v<Syntax::Element, T>
	friend struct Lisp::TypeConverter;

	using base = NMLO<Type, Name, NMLOField<Ts, Ns, As>...>;
	static constexpr inline auto type_index = Type;
	static constexpr inline auto class_name = static_cast<std::string_view>(Name);

	std::tuple<Ts...> data;

	template <std::size_t, Literal, class...>
	struct GetImpl;

	template <std::size_t ID, Literal S,
			 class T, Literal N, FieldAccess A,
			 class... _Ts, Literal... _Ns, FieldAccess... _As>
	struct GetImpl<ID, S, NMLOField<T, N, A>, NMLOField<_Ts, _Ns, _As>...>
	{
		static consteval std::pair<std::size_t, FieldAccess> f()
		{
			if constexpr (S == N)
			{
				return std::make_pair(ID, A);
			}
			else
			{
				return GetImpl<ID+1, S, NMLOField<_Ts, _Ns, _As>...>::f();
			}
		}
	};

	[[nodiscard]] std::string serialize_impl() const noexcept
	requires (NMLOHelper::Serializable<Ts> && ...)
	{
		std::string ser;
		
		[this, &ser]<std::size_t... i>(std::index_sequence<i...>)
		{
			((ser.append( NMLOHelper::serialize(std::get<i>(data)) )), ...);
		}(std::make_index_sequence<std::tuple_size_v<decltype(data)>>{});

		return ser;
	}

	[[nodiscard]] std::string serialize_impl() const noexcept
	{
		// Not serializable
		return {};
	}

public:
	[[nodiscard]] static inline constexpr std::string_view type_name() noexcept { return Name; };
	[[nodiscard]] Syntax::Type get_type() const noexcept { return Type; };

	[[nodiscard]] std::string serialize() const noexcept { return serialize_impl(); }
	[[nodiscard]] std::string hash() const noexcept { return sw::sha512::calculate(serialize_impl()); }
	
	/**
	 * @brief Getter for reading field
	 *
	 * @tparam S field to access
	 * @returns const reference to field with name ```S```
	 */
	template <Literal S>
	[[nodiscard]] constexpr const auto& get() const noexcept
	{
		static constexpr auto R = GetImpl<0, S, NMLOField<Ts, Ns, As>...>::f();
		static_assert(R.second & FieldAccess::READ, "Cannot get field for reading, no access");

		return std::get<R.first>(data);
	}

	/**
	 * @brief Getter for writing to field
	 *
	 * @tparam S field to access
	 * @returns lvalue reference to field with name ```S```
	 */
	template <Literal S>
	[[nodiscard]] constexpr auto& get() noexcept
	{
		static constexpr auto R = GetImpl<0, S, NMLOField<Ts, Ns, As>...>::f();
		static_assert(R.second & FieldAccess::WRITE, "Cannot get field for writing, no access");

		return std::get<R.first>(data);
	}

	/**
	 * @brief Gets field information
	 *
	 * @tparam ID Field's id
	 */
	using Types = std::tuple<Ts...>;
	using Names = std::tuple<std::integral_constant<decltype(Ns), Ns>...>;
	using Accessors = std::tuple<std::integral_constant<FieldAccess, As>...>;
	template <std::size_t ID>
	struct Field
	{
		using type = std::tuple_element_t<ID, Types>;
		static constinit inline auto name = std::tuple_element_t<ID, Names>::value;
		static constinit inline auto access = std::tuple_element_t<ID, Accessors>::value;
	};
};

template <class T>
concept NMLObject = requires
{
	typename T::base;
	std::is_base_of_v<Syntax::Element, T>;
};

/**
 * @brief Type converter for NMLO objects
 */
template <NMLObject T>
struct Lisp::TypeConverter<T>
{
	using type = typename T::base;
	/**
	 * @brief Initializes scheme procedures related to type
	 */
	static void init()
	{
		// "is-object" procedure
		scm_c_define_gsubr(NMLOHelper::getLispStringName("is", type::type_name()).c_str(), 1, 0, 0, (void*)+[](SCM o)
		{ return scm_from_bool(type::type_index == scm_to_uint8(scm_list_ref(o, TypeConverter<std::size_t>::from(0)))); });

		// Setters/Getters
		[]<std::size_t... i>(std::index_sequence<i...>)
		{
			(([]<std::size_t ID>
			{
				using field = typename type::template Field<ID>;

				// Accessor
				scm_c_define_gsubr(NMLOHelper::getLispStringName(type::type_name(), field::name).c_str(), 1, 1, 0, (void*)+[](SCM o, SCM v)
				{
					if (v == SCM_UNDEFINED) // Get
						return scm_list_ref(o, TypeConverter<std::size_t>::from(ID+1));
					else // Set
						return scm_list_set_x(o, TypeConverter<std::size_t>::from(ID+1), v);
				});
			}.template operator()<i>()), ...);
		}(std::make_index_sequence<std::tuple_size_v<decltype(type::data)>>{});
	}

	/**
	 * @brief Converts from SCM object to NMLO
	 * @param v SCM object
	 * @return NMLO from v
	 */
	[[nodiscard]] static type to(SCM v)
	{
		type o;

		[&]<std::size_t... i>(std::index_sequence<i...>)
		{
			((std::get<i>(o.data) = scm_list_ref(v, TypeConverter<std::size_t>::from(i+1))), ...);
		}(std::make_index_sequence<std::tuple_size_v<decltype(type::data)>>{});

		return o;
	}

	/**
	 * @brief Converts from SCM object to NMLO constructed using ```new```
	 * @param v SCM object
	 * @return Pointer to NMLO from v
	 */
	[[nodiscard]] static type* to_new(SCM v)
	{
		type* o = new type;

		[&]<std::size_t... i>(std::index_sequence<i...>)
		{
			((std::get<i>(o->data) = TypeConverter<typename type::template Field<i>::type>::to(scm_list_ref(v, TypeConverter<std::size_t>::from(i+1)))), ...);
		}(std::make_index_sequence<std::tuple_size_v<decltype(type::data)>>{});

		return o;
	}

	/**
	 * @brief Converts from NMLO to SCM object
	 * @param o NMLO
	 * @return SCM object from o
	 */
	[[nodiscard]] static SCM from(const type& o)
	{
		return [&]<std::size_t... i>(std::index_sequence<i...>)
		{
			return scm_list_n(
					TypeConverter<std::uint8_t>::from(o.get_type().value), // Type id
					(TypeConverter<typename type::template Field<i>::type>::from(std::get<i>(o.data)))...,
					SCM_UNDEFINED);
		}(std::make_index_sequence<std::tuple_size_v<decltype(type::data)>>{});
	}
};

#endif // NML_NMLO_HPP
