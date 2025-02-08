/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file base_bitset_type.hpp Base for bitset types that accept strong types,
 * i.e. types that need some casting like StrongType and enum class.
 */

#ifndef BASE_BITSET_TYPE_HPP
#define BASE_BITSET_TYPE_HPP

/**
 * Base for bit set wrapper.
 * Allows wrapping strong type values as a bit set. Methods are loosely modelled on std::bitset.
 * @tparam Tvalue_type Type of values to wrap.
 * @tparam Tstorage Storage type required to hold values.
 */
template <typename Timpl, typename Tvalue_type, typename Tstorage>
class BaseBitSet {
public:
	using ValueType = Tvalue_type; ///< Value type of this BaseBitSet.
	using BaseType = Tstorage; ///< Storage type of this BaseBitSet, be ConvertibleThroughBase

	constexpr BaseBitSet() : data(0) {}
	explicit constexpr BaseBitSet(Tstorage data) : data(data) {}

	constexpr auto operator <=>(const BaseBitSet &) const noexcept = default;

	/**
	 * Set the value-th bit.
	 * @param value Bit to set.
	 * @returns The bit set
	 */
	inline constexpr Timpl &Set(Tvalue_type value)
	{
		this->data |= (1ULL << Timpl::DecayValueType(value));
		return static_cast<Timpl&>(*this);
	}

	/**
	 * Reset the value-th bit.
	 * @param value Bit to reset.
	 * @returns The bit set
	 */
	inline constexpr Timpl &Reset(Tvalue_type value)
	{
		this->data &= ~(1ULL << Timpl::DecayValueType(value));
		return static_cast<Timpl&>(*this);
	}

	/**
	 * Flip the value-th bit.
	 * @param value Bit to flip.
	 * @returns The bit set
	 */
	inline constexpr Timpl &Flip(Tvalue_type value)
	{
		if (this->Test(value)) {
			return this->Reset(value);
		} else {
			return this->Set(value);
		}
	}

	/**
	 * Test if the value-th bit is set.
	 * @param value Bit to check.
	 * @returns true iff the requested bit is set.
	 */
	inline constexpr bool Test(Tvalue_type value) const
	{
		return (this->data & (1ULL << Timpl::DecayValueType(value))) != 0;
	}

	/**
	 * Test if all of the values are set.
	 * @param other BitSet of values to test.
	 * @returns true iff all of the values are set.
	 */
	inline constexpr bool All(const Timpl &other) const
	{
		return (this->data & other.data) == other.data;
	}

	/**
	 * Test if any of the given values are set.
	 * @param other BitSet of values to test.
	 * @returns true iff any of the given values are set.
	 */
	inline constexpr bool Any(const Timpl &other) const
	{
		return (this->data & other.data) != 0;
	}

	/**
	 * Test if any of the values are set.
	 * @returns true iff any of the values are set.
	 */
	inline constexpr bool Any() const
	{
		return this->data != 0;
	}

	/**
	 * Test if none of the values are set.
	 * @returns true iff none of the values are set.
	 */
	inline constexpr bool None() const
	{
		return this->data == 0;
	}

	inline constexpr Timpl operator |(const Timpl &other) const
	{
		return Timpl{static_cast<Tstorage>(this->data | other.data)};
	}

	inline constexpr Timpl operator &(const Timpl &other) const
	{
		return Timpl{static_cast<Tstorage>(this->data & other.data)};
	}

	/**
	 * Retrieve the raw value behind this bit set.
	 * @returns the raw value.
	 */
	inline constexpr Tstorage base() const noexcept
	{
		return this->data;
	}

private:
	Tstorage data; ///< Bitmask of values.
};

#endif /* BASE_BITSET_TYPE_HPP */
