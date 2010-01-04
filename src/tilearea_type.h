/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilearea_type.h Type for storing the 'area' of something uses on the map. */

#ifndef TILEAREA_TYPE_H
#define TILEAREA_TYPE_H

#include "tile_type.h"

/** Represents the covered area of e.g. a rail station */
struct TileArea {
	/** Just construct this tile area */
	TileArea() {}

	/**
	 * Construct this tile area with some set values
	 * @param tile the base tile
	 * @param w the width
	 * @param h the height
	 */
	TileArea(TileIndex tile, uint8 w, uint8 h) : tile(tile), w(w), h(h) {}

	/**
	 * Construct this tile area based on two points.
	 * @param start the start of the area
	 * @param end   the end of the area
	 */
	TileArea(TileIndex start, TileIndex end);

	TileIndex tile; ///< The base tile of the area
	uint8 w;        ///< The width of the area
	uint8 h;        ///< The height of the area

	/**
	 * Add a single tile to a tile area; enlarge if needed.
	 * @param to_add The tile to add
	 */
	void Add(TileIndex to_add);

	/**
	 * Clears the 'tile area', i.e. make the tile invalid.
	 */
	void Clear()
	{
		this->tile = INVALID_TILE;
		this->w    = 0;
		this->h    = 0;
	}

	/**
	 * Does this tile area intersect with another?
	 * @param ta the other tile area to check against.
	 * @return true if they intersect.
	 */
	bool Intersects(const TileArea &ta) const;
};

#endif /* TILEAREA_TYPE_H */
