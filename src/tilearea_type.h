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

#include "map_func.h"

/** Represents the covered area of e.g. a rail station */
struct TileArea {
	TileIndex tile; ///< The base tile of the area
	uint16 w;       ///< The width of the area
	uint16 h;       ///< The height of the area

	/** Just construct this tile area */
	TileArea() {}

	/**
	 * Construct this tile area with some set values
	 * @param tile the base tile
	 * @param w the width
	 * @param h the height
	 */
	TileArea(TileIndex tile, uint8 w, uint8 h) : tile(tile), w(w), h(h) {}

	TileArea(TileIndex start, TileIndex end);


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

	bool Intersects(const TileArea &ta) const;

	void ClampToMap();

	/**
	 * Get the center tile.
	 * @return The tile at the center, or just north of it.
	 */
	TileIndex GetCenterTile() const
	{
		return TILE_ADDXY(this->tile, this->w / 2, this->h / 2);
	}
};

/** Base class for tile iterators. */
class TileIterator {
protected:
	TileIndex tile; ///< The current tile we are at.

	/**
	 * Initialise the iterator starting at this tile.
	 * @param tile The tile we start iterating from.
	 */
	TileIterator(TileIndex tile) : tile(tile)
	{
	}

public:
	/** Some compilers really like this. */
	virtual ~TileIterator()
	{
	}

	/**
	 * Get the tile we are currently at.
	 * @return The tile we are at, or INVALID_TILE when we're done.
	 */
	FORCEINLINE operator TileIndex () const
	{
		return this->tile;
	}

	/**
	 * Move ourselves to the next tile in the rectange on the map.
	 */
	virtual TileIterator& operator ++() = 0;
};

/** Iterator to iterate over a tile area (rectangle) of the map. */
class OrthogonalTileIterator : public TileIterator {
private:
	int w;          ///< The width of the iterated area.
	int x;          ///< The current 'x' position in the rectangle.
	int y;          ///< The current 'y' position in the rectangle.

public:
	/**
	 * Construct the iterator.
	 * @param ta Area, i.e. begin point and width/height of to-be-iterated area.
	 */
	OrthogonalTileIterator(const TileArea &ta) : TileIterator(ta.w == 0 || ta.h == 0 ? INVALID_TILE : ta.tile), w(ta.w), x(ta.w), y(ta.h)
	{
	}

	/**
	 * Move ourselves to the next tile in the rectange on the map.
	 */
	FORCEINLINE TileIterator& operator ++()
	{
		assert(this->tile != INVALID_TILE);

		if (--this->x > 0) {
			this->tile++;
		} else if (--this->y > 0) {
			this->x = this->w;
			this->tile += TileDiffXY(1, 1) - this->w;
		} else {
			this->tile = INVALID_TILE;
		}
		return *this;
	}
};

/** Iterator to iterate over a diagonal area of the map. */
class DiagonalTileIterator : public TileIterator {
private:
	uint base_x, base_y; ///< The base tile x and y coordinates from where the iterating happens.
	int a_cur, b_cur;    ///< The current (rotated) x and y coordinates of the iteration.
	int a_max, b_max;    ///< The (rotated) x and y coordinates of the end of the iteration.

public:
	/**
	 * Construct the iterator.
	 * @param begin Tile from where to begin iterating.
	 * @param end   Tile where to end the iterating.
	 */
	DiagonalTileIterator(TileIndex begin, TileIndex end);

	/**
	 * Move ourselves to the next tile in the rectange on the map.
	 */
	TileIterator& operator ++();
};

/**
 * A loop which iterates over the tiles of a TileArea.
 * @param var The name of the variable which contains the current tile.
 *            This variable will be allocated in this \c for of this loop.
 * @param ta  The tile area to search over.
 */
#define TILE_AREA_LOOP(var, ta) for (OrthogonalTileIterator var(ta); var != INVALID_TILE; ++var)

#endif /* TILEAREA_TYPE_H */
