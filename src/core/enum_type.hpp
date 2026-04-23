/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file enum_type.hpp Type (helpers) for enums. */

#ifndef ENUM_TYPE_HPP
#define ENUM_TYPE_HPP

#include "base_bitset_type.hpp"

/**
 * Implementation of std::to_underlying (from C++23)
 * @param e The enum to get the value of.
 * @return The underlying value of the enum.
 */
template <typename Tenum_type>
constexpr std::underlying_type_t<Tenum_type> to_underlying(Tenum_type e) { return static_cast<std::underlying_type_t<Tenum_type>>(e); }

/** Implementation of std::is_scoped_enum_v (from C++23) */
template <class T> constexpr bool is_scoped_enum_v = std::conjunction_v<std::is_enum<T>, std::negation<std::is_convertible<T, int>>>;

/** Trait to enable prefix/postfix incrementing operators. */
template <typename Tenum_type>
struct is_enum_incrementable {
	static constexpr bool value = false;
};

template <typename Tenum_type>
constexpr bool is_enum_incrementable_v = is_enum_incrementable<Tenum_type>::value;

/**
 * Prefix increment.
 * @param e The enum to increment.
 * @return Reference to the incremented enum.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_incrementable_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type &operator ++(Tenum_type &e)
{
	e = static_cast<Tenum_type>(to_underlying(e) + 1);
	return e;
}

/**
 * Postfix increment, uses prefix increment.
 * @param e The enum to increment.
 * @return Copy of the original value.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_incrementable_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type operator ++(Tenum_type &e, int)
{
	Tenum_type e_org = e;
	++e;
	return e_org;
}

/**
 * Prefix decrement.
 * @param e The enum to decrement.
 * @return Reference to the decremented enum.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_incrementable_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type &operator --(Tenum_type &e)
{
	e = static_cast<Tenum_type>(to_underlying(e) - 1);
	return e;
}

/**
 * Postfix decrement, uses prefix decrement.
 * @param e The enum to decrement.
 * @return Copy of the original value.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_incrementable_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type operator --(Tenum_type &e, int)
{
	Tenum_type e_org = e;
	--e;
	return e_org;
}

/** For some enums it is useful to have pre/post increment/decrement operators */
#define DECLARE_INCREMENT_DECREMENT_OPERATORS(enum_type) \
	template <> struct is_enum_incrementable<enum_type> { \
		static const bool value = true; \
	};

/** Trait to enable prefix/postfix incrementing operators. */
template <typename Tenum_type>
struct is_enum_sequential {
	static constexpr bool value = false;
};

template <typename Tenum_type>
constexpr bool is_enum_sequential_v = is_enum_sequential<Tenum_type>::value;

/**
 * Add integer.
 * @param e The enum to add to.
 * @param offset The amount to add to the enum.
 * @return The new enum.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_sequential_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type operator+(Tenum_type e, int offset)
{
	return static_cast<Tenum_type>(to_underlying(e) + offset);
}

template <typename Tenum_type, std::enable_if_t<is_enum_sequential_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type &operator+=(Tenum_type &e, int offset)
{
	e = e + offset;
	return e;
}

/**
 * Subtract integer.
 * @param e The enum to subtract from.
 * @param offset The amount to subtract from the enum.
 * @return The new enum.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_sequential_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type operator-(Tenum_type e, int offset)
{
	return static_cast<Tenum_type>(to_underlying(e) - offset);
}

template <typename Tenum_type, std::enable_if_t<is_enum_sequential_v<Tenum_type>, bool> = true>
inline constexpr Tenum_type &operator-=(Tenum_type &e, int offset)
{
	e = e - offset;
	return e;
}

/**
 * Distance of two enums.
 * @param a The first enum.
 * @param b The second enum.
 * @return The value of the first enum minus the value of the second enum.
 */
template <typename Tenum_type, std::enable_if_t<is_enum_sequential_v<Tenum_type>, bool> = true>
inline constexpr auto operator-(Tenum_type a, Tenum_type b)
{
	return to_underlying(a) - to_underlying(b);
}

/** For some enums it is useful to add/sub more than 1 */
#define DECLARE_ENUM_AS_SEQUENTIAL(enum_type) \
	template <> struct is_enum_sequential<enum_type> { \
		static const bool value = true; \
	};

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
template <typename Tenum_type, class = typename std::enable_if_t<std::is_enum_v<Tenum_type>>>
[[debug_inline]] inline constexpr bool HasFlag(const Tenum_type x, const Tenum_type y)
{
	return (x & y) == y;
}

/**
 * Toggle a value in a bitset enum.
 * @param x The value to change.
 * @param y The flag to toggle.
 */
template <typename Tenum_type, class = typename std::enable_if_t<std::is_enum_v<Tenum_type>>>
[[debug_inline]] inline constexpr void ToggleFlag(Tenum_type &x, const Tenum_type y)
{
	if (HasFlag(x, y)) {
		x &= ~y;
	} else {
		x |= y;
	}
}

/** Helper template structure to get the mask for an EnumBitSet from the end enum value. */
template <typename Tstorage, typename Tenum, Tenum Tend_value>
struct EnumBitSetMask {
	static constexpr Tstorage value = std::numeric_limits<Tstorage>::max() >> (std::numeric_limits<Tstorage>::digits - to_underlying(Tend_value));
};

/**
 * Enum-as-bit-set wrapper.
 * Allows wrapping enum values as a bit set. Methods are loosely modelled on std::bitset.
 * @note Only set Tend_value if the bitset needs to be automatically masked to valid values.
 * @tparam Tenum Enum values to wrap.
 * @tparam Tstorage Storage type required to hold eenum values.
 * @tparam Tend_value Last valid value + 1.
 */
template <typename Tenum_type, typename Tstorage, Tenum_type Tend_value = Tenum_type{std::numeric_limits<Tstorage>::digits}>
class EnumBitSet : public BaseBitSet<EnumBitSet<Tenum_type, Tstorage, Tend_value>, Tenum_type, Tstorage, EnumBitSetMask<Tstorage, Tenum_type, Tend_value>::value> {
	using BaseClass = BaseBitSet<EnumBitSet<Tenum_type, Tstorage, Tend_value>, Tenum_type, Tstorage, EnumBitSetMask<Tstorage, Tenum_type, Tend_value>::value>;
public:
	using EnumType = BaseClass::ValueType;

	constexpr EnumBitSet() : BaseClass() {}
	constexpr EnumBitSet(Tenum_type value) : BaseClass() { this->Set(value); }
	explicit constexpr EnumBitSet(Tstorage data) : BaseClass(data) {}

	/**
	 * Construct an EnumBitSet from a list of enum values.
	 * @param values List of enum values.
	 */
	constexpr EnumBitSet(std::initializer_list<const Tenum_type> values) : BaseClass()
	{
		for (const Tenum_type &value : values) {
			this->Set(value);
		}
	}

	constexpr auto operator <=>(const EnumBitSet &) const noexcept = default;

	static constexpr size_t DecayValueType(const BaseClass::ValueType &value) { return to_underlying(value); }
};

/**
 * A sort-of mixin that implements 'at(pos)' and 'operator[](pos)' only for a specific enum class.
 * This to prevent having to call 'to_underlying()' for many container accesses, whilst preventing accidental use of the wrong index type.
 * @tparam Container A base container.
 * @tparam Index The enum class to use for indexing.
 */
template <typename Tcontainer, typename Tindex>
class EnumClassIndexContainer : public Tcontainer {
public:
	Tcontainer::reference at(const Tindex &pos) { return this->Tcontainer::at(to_underlying(pos)); }

	Tcontainer::const_reference at(const Tindex &pos) const { return this->Tcontainer::at(to_underlying(pos)); }

	Tcontainer::reference operator[](const Tindex &pos) { return this->Tcontainer::operator[](to_underlying(pos)); }

	Tcontainer::const_reference operator[](const Tindex &pos) const { return this->Tcontainer::operator[](to_underlying(pos)); }
};

#endif /* ENUM_TYPE_HPP */
