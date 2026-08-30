/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_airport.h Class for iterating an airport on the map. */

#ifndef TILEAREA_AIRPORT_HPP
#define TILEAREA_AIRPORT_HPP

#include "station_base.h"
#include "tilearea_orthogonal.h"

/** Iterator to iterate over all tiles belonging to an airport. */
class AirportTileIterator {
private:
	const Station *st = nullptr; ///< The station the airport is a part of.
	OrthogonalTileArea::iterator iter; ///< The underlying tile area iterator.

public:
	using value_type = TileIndex; ///< value_type iterator trait
	using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
	using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
	using pointer = void; ///< pointer iterator trait
	using reference = void; ///< reference iterator trait

	/**
	 * Construct the iterator.
	 * @param st Station the airport is part of.
	 */
	AirportTileIterator(const Station *st) : st(st), iter(st->airport)
	{
		if (*this->iter != INVALID_TILE && !st->TileBelongsToAirport(*this->iter)) ++this->iter;
	}

	/**
	 * Compare with other iterator.
	 * @param other The other iterator to compare with.
	 * @return \c true iff the other iterator matches this value.
	 */
	bool operator==(const AirportTileIterator &other) const
	{
		return this->st == other.st && this->iter == other.iter;
	};

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
		return *this->iter;
	}

	/**
	 * Increment the iterator and set it to the next position.
	 * @return This iterator after incrementing.
	 */
	inline AirportTileIterator &operator++()
	{
		do {
			++this->iter;
		} while (*this->iter != INVALID_TILE && !st->TileBelongsToAirport(*this->iter));
		return *this;
	}
};

#endif /* TILEAREA_AIRPORT_HPP */
