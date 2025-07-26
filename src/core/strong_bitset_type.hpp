/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strong_bitset_type.hpp Type helper for making a BitSet out of a Strong Typedef. */

#ifndef STRONG_BITSET_TYPE_HPP
#define STRONG_BITSET_TYPE_HPP

#include "base_bitset_type.hpp"

/**
 * Strong bit set.
 * @tparam Tvalue_type Type of values to wrap.
 * @tparam Tstorage Storage type required to hold values.
 */
template <typename Tvalue_type, typename Tstorage, Tstorage Tmask = std::numeric_limits<Tstorage>::max()>
class StrongBitSet : public BaseBitSet<StrongBitSet<Tvalue_type, Tstorage, Tmask>, Tvalue_type, Tstorage> {
public:
	constexpr StrongBitSet() : BaseClass() {}
	constexpr StrongBitSet(Tvalue_type value) : BaseClass() { this->Set(value); }
	explicit constexpr StrongBitSet(Tstorage data) : BaseClass(data) {}

	constexpr StrongBitSet(std::initializer_list<const Tvalue_type> values) : BaseClass()
	{
		for (const Tvalue_type &value : values) {
			this->Set(value);
		}
	}

	static constexpr size_t DecayValueType(Tvalue_type value) { return value.base(); }

	constexpr auto operator <=>(const StrongBitSet &) const noexcept = default;

private:
	using BaseClass = BaseBitSet<StrongBitSet<Tvalue_type, Tstorage, Tmask>, Tvalue_type, Tstorage, Tmask>;
};

#endif /* STRONG_BITSET_TYPE_HPP */
