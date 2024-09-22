/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file enum_type.hpp Type (helpers) for enums */

#ifndef ENUM_TYPE_HPP
#define ENUM_TYPE_HPP

/** Implementation of std::to_underlying (from C++23) */
template <typename enum_type>
constexpr std::underlying_type_t<enum_type> to_underlying(enum_type e) { return static_cast<std::underlying_type_t<enum_type>>(e); }

/** Some enums need to have allowed incrementing (i.e. StationClassID) */
#define DECLARE_POSTFIX_INCREMENT(enum_type) \
	inline constexpr enum_type operator ++(enum_type& e, int) \
	{ \
		enum_type e_org = e; \
		e = static_cast<enum_type>(to_underlying(e) + 1); \
		return e_org; \
	} \
	inline constexpr enum_type operator --(enum_type& e, int) \
	{ \
		enum_type e_org = e; \
		e = static_cast<enum_type>(to_underlying(e) - 1); \
		return e_org; \
	}



/** Operators to allow to work with enum as with type safe bit set in C++ */
#define DECLARE_ENUM_AS_BIT_SET(enum_type) \
	inline constexpr enum_type operator | (enum_type m1, enum_type m2) { return static_cast<enum_type>(to_underlying(m1) | to_underlying(m2)); } \
	inline constexpr enum_type operator & (enum_type m1, enum_type m2) { return static_cast<enum_type>(to_underlying(m1) & to_underlying(m2)); } \
	inline constexpr enum_type operator ^ (enum_type m1, enum_type m2) { return static_cast<enum_type>(to_underlying(m1) ^ to_underlying(m2)); } \
	inline constexpr enum_type& operator |= (enum_type& m1, enum_type m2) { m1 = m1 | m2; return m1; } \
	inline constexpr enum_type& operator &= (enum_type& m1, enum_type m2) { m1 = m1 & m2; return m1; } \
	inline constexpr enum_type& operator ^= (enum_type& m1, enum_type m2) { m1 = m1 ^ m2; return m1; } \
	inline constexpr enum_type operator ~(enum_type m) { return static_cast<enum_type>(~to_underlying(m)); }

/** Operator that allows this enumeration to be added to any other enumeration. */
#define DECLARE_ENUM_AS_ADDABLE(EnumType) \
	template <typename OtherEnumType, typename = typename std::enable_if<std::is_enum_v<OtherEnumType>, OtherEnumType>::type> \
	constexpr OtherEnumType operator + (OtherEnumType m1, EnumType m2) { \
		return static_cast<OtherEnumType>(to_underlying(m1) + to_underlying(m2)); \
	}

/**
 * Checks if a value in a bitset enum is set.
 * @param x The value to check.
 * @param y The flag to check.
 * @return True iff the flag is set.
 */
template <typename T, class = typename std::enable_if_t<std::is_enum_v<T>>>
debug_inline constexpr bool HasFlag(const T x, const T y)
{
	return (x & y) == y;
}

#endif /* ENUM_TYPE_HPP */
