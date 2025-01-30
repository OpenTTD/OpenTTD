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

/** Trait to enable prefix/postfix incrementing operators. */
template <typename enum_type>
struct is_enum_incrementable {
	static constexpr bool value = false;
};

template <typename enum_type>
constexpr bool is_enum_incrementable_v = is_enum_incrementable<enum_type>::value;

/** Prefix increment. */
template <typename enum_type, std::enable_if_t<is_enum_incrementable_v<enum_type>, bool> = true>
inline constexpr enum_type &operator ++(enum_type &e)
{
	e = static_cast<enum_type>(to_underlying(e) + 1);
	return e;
}

/** Postfix increment, uses prefix increment. */
template <typename enum_type, std::enable_if_t<is_enum_incrementable_v<enum_type>, bool> = true>
inline constexpr enum_type operator ++(enum_type &e, int)
{
	enum_type e_org = e;
	++e;
	return e_org;
}

/** Prefix decrement. */
template <typename enum_type, std::enable_if_t<is_enum_incrementable_v<enum_type>, bool> = true>
inline constexpr enum_type &operator --(enum_type &e)
{
	e = static_cast<enum_type>(to_underlying(e) - 1);
	return e;
}

/** Postfix decrement, uses prefix decrement. */
template <typename enum_type, std::enable_if_t<is_enum_incrementable_v<enum_type>, bool> = true>
inline constexpr enum_type operator --(enum_type &e, int)
{
	enum_type e_org = e;
	--e;
	return e_org;
}

/** For some enums it is useful to have pre/post increment/decrement operators */
#define DECLARE_INCREMENT_DECREMENT_OPERATORS(enum_type) \
	template <> struct is_enum_incrementable<enum_type> { \
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
template <typename T, class = typename std::enable_if_t<std::is_enum_v<T>>>
debug_inline constexpr bool HasFlag(const T x, const T y)
{
	return (x & y) == y;
}

/**
 * Toggle a value in a bitset enum.
 * @param x The value to change.
 * @param y The flag to toggle.
 */
template <typename T, class = typename std::enable_if_t<std::is_enum_v<T>>>
debug_inline constexpr void ToggleFlag(T &x, const T y)
{
	if (HasFlag(x, y)) {
		x &= ~y;
	} else {
		x |= y;
	}
}

/**
 * Enum-as-bit-set wrapper.
 * Allows wrapping enum values as a bit set. Methods are loosely modelled on std::bitset.
 * @tparam Tenum Enum values to wrap.
 * @tparam Tsorage Storage type required to hold eenum values.
 */
template <typename Tenum, typename Tstorage>
class EnumBitSet {
public:
	using enum_type = Tenum; ///< Enum type of this EnumBitSet.
	using storage_type = Tstorage; ///< Storage type of this EnumBitSet.

	constexpr EnumBitSet() : data(0) {}
	constexpr EnumBitSet(Tenum value) : data(0) { this->Set(value); }
	explicit constexpr EnumBitSet(Tstorage data) : data(data) {}

	/**
	 * Construct an EnumBitSet from a list of enum values.
	 * @param values List of enum values.
	 */
	constexpr EnumBitSet(std::initializer_list<const Tenum> values) : data(0)
	{
		for (const Tenum &value : values) {
			this->Set(value);
		}
	}

	constexpr auto operator <=>(const EnumBitSet &) const noexcept = default;

	/**
	 * Set the enum value.
	 * @param value Enum value to set.
	 * @returns The EnumBitset
	 */
	inline constexpr EnumBitSet &Set(Tenum value)
	{
		this->data |= (1U << to_underlying(value));
		return *this;
	}

	/**
	 * Reset the enum value to not set.
	 * @param value Enum value to reset.
	 * @returns The EnumBitset
	 */
	inline constexpr EnumBitSet &Reset(Tenum value)
	{
		this->data &= ~(1U << to_underlying(value));
		return *this;
	}

	/**
	 * Flip the enum value.
	 * @param value Enum value to flip.
	 * @returns The EnumBitset
	 */
	inline constexpr EnumBitSet &Flip(Tenum value)
	{
		if (this->Test(value)) {
			return this->Reset(value);
		} else {
			return this->Set(value);
		}
	}

	/**
	 * Test if the enum value is set.
	 * @param value Enum value to check.
	 * @returns true iff the requested value is set.
	 */
	inline constexpr bool Test(Tenum value) const
	{
		return (this->data & (1U << to_underlying(value))) != 0;
	}

	/**
	 * Test if all of the enum values are set.
	 * @param other BitSet of enum values to test.
	 * @returns true iff all of the enum values are set.
	 */
	inline constexpr bool All(const EnumBitSet &other) const
	{
		return (this->data & other.data) == other.data;
	}

	/**
	 * Test if any of the enum values are set.
	 * @param other BitSet of enum values to test.
	 * @returns true iff any of the enum values are set.
	 */
	inline constexpr bool Any(const EnumBitSet &other) const
	{
		return (this->data & other.data) != 0;
	}

	inline constexpr EnumBitSet operator |(const EnumBitSet &other) const
	{
		return EnumBitSet{static_cast<Tstorage>(this->data | other.data)};
	}

	inline constexpr EnumBitSet operator &(const EnumBitSet &other) const
	{
		return EnumBitSet{static_cast<Tstorage>(this->data & other.data)};
	}

	/**
	 * Retrieve the raw value behind this EnumBitSet.
	 * @returns the raw value.
	 */
	inline constexpr Tstorage base() const noexcept
	{
		return this->data;
	}

private:
	Tstorage data; ///< Bitmask of enum values.
};

#endif /* ENUM_TYPE_HPP */
