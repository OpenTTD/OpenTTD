/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_airportlayout.h Class for iterating an airport layout on the map. */

#ifndef TILEAREA_AIRPORTLAYOUT_HPP
#define TILEAREA_AIRPORTLAYOUT_HPP

#include "newgrf_airport.h"

/** Iterator to iterate over all tiles belonging to an airport spec. */
class AirportTileTableIterator {
private:
	std::span<const AirportTileTable> att; ///< The offsets.
	std::span<const AirportTileTable>::iterator iter; ///< Underlying tile area.
	TileIndex base_tile; ///< The tile we base the offsets off.
	TileIndex tile; ///< The current file.

public:
	using value_type = TileIndex; ///< value_type iterator trait
	using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
	using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
	using pointer = void; ///< pointer iterator trait
	using reference = void; ///< reference iterator trait

	/**
	 * Construct the iterator.
	 * @param att The TileTable we want to iterate over.
	 * @param base_tile The basetile for all offsets.
	 */
	AirportTileTableIterator(std::span<const AirportTileTable> att, TileIndex base_tile) : att(att), iter(att.begin()), base_tile(base_tile), tile(base_tile + ToTileIndexDiff(att.front().ti)) {}

	/**
	 * Compare with other iterator.
	 * @param other The other iterator to compare with.
	 * @return \c true iff the other iterator matches this value.
	 */
	bool operator==(const AirportTileTableIterator &other) const
	{
		return this->base_tile == other.base_tile && this->tile == other.tile;
	}

	/**
	 * Compare with end sentinel.
	 * @return \c true iff this iterator is at the end.
	 */
	bool operator==(const std::default_sentinel_t &) const
	{
		return **this == INVALID_TILE;
	}

	/**
	 * Get the current tile.
	 * @return The current tile, or INVALID_TILE when complete.
	 */
	TileIndex operator*() const
	{
		return this->tile;
	}

	/**
	 * Increment the iterator and set it to the next position.
	 * @return This iterator after incrementing.
	 */
	inline AirportTileTableIterator &operator++()
	{
		++this->iter;
		if (this->iter == std::end(att)) {
			this->tile = INVALID_TILE;
		} else {
			this->tile = this->base_tile + ToTileIndexDiff(this->iter->ti);
		}
		return *this;
	}

	/**
	 * Get the StationGfx for the current tile.
	 * @return The identifier of the graphics for this tile.
	 */
	StationGfx GetStationGfx() const
	{
		return this->iter->gfx;
	}
};

#endif /* TILEAREA_AIRPORTLAYOUT_HPP */
