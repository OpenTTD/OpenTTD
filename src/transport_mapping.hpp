/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file transport_mapping.hpp Transport mapping utilities. */

#ifndef TRANSPORT_MAPPING_HPP
#define TRANSPORT_MAPPING_HPP

#include "core/convertible_through_base.hpp"
#include "core/strong_typedef_type.hpp"

/**
 * Transport type (rail, road, etc) mapping helper.
 * @tparam TType Native type to be mapped.
 * @tparam TInvalidType Invalid value for native type.
 * @tparam TMaxSize maximum number of mapped entries.
 */
template <typename TType, TType TInvalidType, size_t TMaxSize, typename TMapTypeTag>
class TransportMapping {
public:
	static constexpr size_t MAX_SIZE = TMaxSize;
	static constexpr TType INVALID_TYPE = TInvalidType;

	/** Unique type holds a mapped transport type. */
	using MapType = StrongType::Typedef<uint8_t, TMapTypeTag, StrongType::Compare>;
	static constexpr MapType INVALID_MAP_TYPE = static_cast<MapType>(TMaxSize);

	TransportMapping() { this->Init(); }

	void Init()
	{
		std::ranges::fill(this->map, INVALID_TYPE);
	}

	/**
	 * Get the native type of a mapped type.
	 * @param mapped_type Mapped type to look up.
	 * @return native type of mapped type, or INVALID_TYPE if invalid.
	 */
	TType GetType(MapType mapped_type) const
	{
		if (mapped_type == INVALID_MAP_TYPE) return INVALID_TYPE;

		assert(mapped_type.base() < std::size(this->map));
		return this->map[mapped_type];
	}

	void Set(MapType mapped_type, TType type)
	{
		this->map[mapped_type] = type;
	}

	/**
	 * Get the mapped type of a native type.
	 * @param type Native type to look up.
	 * @return mapped type of native type, or INVALID_MAP_TYPE if invalid.
	 */
	MapType GetMappedType(TType type)
	{
		if (type == INVALID_TYPE) return INVALID_MAP_TYPE;

		auto it = std::ranges::find(this->map, type);
		if (it != std::end(this->map)) return this->Index(it);

		return INVALID_MAP_TYPE;
	}

	/**
	 * Allocate a mapped type for a native type.
	 * If the native type is already mapped then the existing allocation is used.
	 * @param type Native type to map.
	 * @param exec Whether to actually set the mapping.
	 * @return mapped type of native type, or INVALID_MAP_TYPE if allocation was not possible.
	 */
	MapType AllocateMapType(TType type, bool exec)
	{
		if (type == INVALID_TYPE) return INVALID_MAP_TYPE;

		/* Check for existing mapped type. */
		auto it = std::ranges::find(this->map, type);
		if (it != std::end(this->map)) return this->Index(it);

		/* Check for an unused entry. */
		it = std::ranges::find(this->map, INVALID_TYPE);
		if (it == std::end(this->map)) return INVALID_MAP_TYPE;

		if (exec) *it = type;

		return this->Index(it);
	}

private:
	using MapStorage = TypedIndexContainer<std::array<TType, MAX_SIZE>, MapType>;
	MapStorage map{};

	MapType Index(MapStorage::iterator iter) const
	{
		return static_cast<MapType>(iter - std::begin(this->map));
	}
};

#endif /* TRANSPORT_MAPPING_HPP */
