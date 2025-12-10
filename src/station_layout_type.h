/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_layout_type.h Functions related to station layouts. */

#ifndef STATION_LAYOUT_TYPE_H
#define STATION_LAYOUT_TYPE_H

#include "newgrf_station.h"
#include "station_map.h"

class RailStationTileLayout {
private:
	std::span<const StationGfx> layout{}; ///< Predefined tile layout.
	uint platforms; ///< Number of platforms.
	uint length; ///< Length of platforms.
public:
	RailStationTileLayout(const StationSpec *spec, uint8_t platforms, uint8_t length);

	class Iterator {
		const RailStationTileLayout &stl; ///< Station tile layout being iterated.
		uint position = 0; ///< Position within iterator.
	public:
		using value_type = StationGfx;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::forward_iterator_tag;
		using pointer = void;
		using reference = void;

		Iterator(const RailStationTileLayout &stl) : stl(stl) {}

		bool operator==(const Iterator &rhs) const { return this->position == rhs.position; }
		bool operator==(const std::default_sentinel_t &) const { return this->position == this->stl.platforms * this->stl.length; }

		StationGfx operator*() const;

		Iterator &operator++()
		{
			++this->position;
			return *this;
		}

		Iterator operator++(int)
		{
			Iterator result = *this;
			++*this;
			return result;
		}
	};

	Iterator begin() const { return Iterator(*this); }
	std::default_sentinel_t end() const { return std::default_sentinel_t(); }
};

#endif /* STATION_LAYOUT_TYPE_H */
