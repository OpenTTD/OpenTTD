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

class OrthogonalTileIterator;

/** Represents the covered area of e.g. a rail station */
struct OrthogonalTileArea {
	TileIndex tile; ///< The base tile of the area
	uint16_t w;       ///< The width of the area
	uint16_t h;       ///< The height of the area

	/**
	 * Construct this tile area with some set values
	 * @param tile the base tile
	 * @param w the width
	 * @param h the height
	 */
	OrthogonalTileArea(TileIndex tile = INVALID_TILE, uint8_t w = 0, uint8_t h = 0) : tile(tile), w(w), h(h)
	{
	}

	OrthogonalTileArea(TileIndex start, TileIndex end);

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

	bool Intersects(const OrthogonalTileArea &ta) const;

	bool Contains(TileIndex tile) const;

	OrthogonalTileArea &Expand(int rad);

	void ClampToMap();

	/**
	 * Get the center tile.
	 * @return The tile at the center, or just north of it.
	 */
	TileIndex GetCenterTile() const
	{
		return TILE_ADDXY(this->tile, this->w / 2, this->h / 2);
	}

	OrthogonalTileIterator begin() const;

	OrthogonalTileIterator end() const;
};

/** Represents a diagonal tile area. */
struct DiagonalTileArea {

	TileIndex tile; ///< Base tile of the area
	int16_t a;        ///< Extent in diagonal "x" direction (may be negative to signify the area stretches to the left)
	int16_t b;        ///< Extent in diagonal "y" direction (may be negative to signify the area stretches upwards)

	/**
	 * Construct this tile area with some set values.
	 * @param tile The base tile.
	 * @param a The "x" extent.
	 * @param b The "y" estent.
	 */
	DiagonalTileArea(TileIndex tile = INVALID_TILE, int8_t a = 0, int8_t b = 0) : tile(tile), a(a), b(b)
	{
	}

	DiagonalTileArea(TileIndex start, TileIndex end);

	/**
	 * Clears the TileArea by making the tile invalid and setting a and b to 0.
	 */
	void Clear()
	{
		this->tile = INVALID_TILE;
		this->a    = 0;
		this->b    = 0;
	}

	bool Contains(TileIndex tile) const;
};

/** Shorthand for the much more common orthogonal tile area. */
typedef OrthogonalTileArea TileArea;

/** Base class for tile iterators. */
class TileIterator {
protected:
	TileIndex tile; ///< The current tile we are at.

	/**
	 * Initialise the iterator starting at this tile.
	 * @param tile The tile we start iterating from.
	 */
	TileIterator(TileIndex tile = INVALID_TILE) : tile(tile)
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
	inline operator TileIndex () const
	{
		return this->tile;
	}

	/**
	 * Get the tile we are currently at.
	 * @return The tile we are at, or INVALID_TILE when we're done.
	 */
	inline TileIndex operator *() const
	{
		return this->tile;
	}

	/**
	 * Move ourselves to the next tile in the rectangle on the map.
	 */
	virtual TileIterator& operator ++() = 0;

	/**
	 * Allocate a new iterator that is a copy of this one.
	 */
	virtual std::unique_ptr<TileIterator> Clone() const = 0;

	/**
	 * Equality comparison.
	 */
	bool operator ==(const TileIterator &rhs) const
	{
		return this->tile == rhs.tile;
	}
	/**
	 * Inequality comparison.
	 */
	bool operator !=(const TileIterator &rhs) const
	{
		return this->tile != rhs.tile;
	}

	/**
	 * Equality comparison.
	 */
	bool operator ==(const TileIndex &rhs) const
	{
		return this->tile == rhs;
	}
	/**
	 * Inequality comparison.
	 */
	bool operator !=(const TileIndex &rhs) const
	{
		return this->tile != rhs;
	}

	static std::unique_ptr<TileIterator> Create(TileIndex corner1, TileIndex corner2, bool diagonal);
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
	OrthogonalTileIterator(const OrthogonalTileArea &ta) : TileIterator(ta.w == 0 || ta.h == 0 ? INVALID_TILE : ta.tile), w(ta.w), x(ta.w), y(ta.h)
	{
	}

	/**
	 * Construct the iterator.
	 * @param corner1 Tile from where to begin iterating.
	 * @param corner2 Tile where to end the iterating.
	 */
	OrthogonalTileIterator(TileIndex corner1, TileIndex corner2)
	{
		*this = OrthogonalTileIterator(OrthogonalTileArea(corner1, corner2));
	}

	/**
	 * Move ourselves to the next tile in the rectangle on the map.
	 */
	inline TileIterator& operator ++() override
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

	std::unique_ptr<TileIterator> Clone() const override
	{
		return std::make_unique<OrthogonalTileIterator>(*this);
	}
};

/** Iterator to iterate over a diagonal area of the map. */
class DiagonalTileIterator : public TileIterator {
private:
	uint base_x; ///< The base tile x coordinate from where the iterating happens.
	uint base_y; ///< The base tile y coordinate from where the iterating happens.
	int a_cur;   ///< The current (rotated) x coordinate of the iteration.
	int b_cur;   ///< The current (rotated) y coordinate of the iteration.
	int a_max;   ///< The (rotated) x coordinate of the end of the iteration.
	int b_max;   ///< The (rotated) y coordinate of the end of the iteration.

public:

	/**
	 * Construct the iterator.
	 * @param ta Area, i.e. begin point and (diagonal) width/height of to-be-iterated area.
	 */
	DiagonalTileIterator(const DiagonalTileArea &ta) :
		TileIterator(ta.tile), base_x(TileX(ta.tile)), base_y(TileY(ta.tile)), a_cur(0), b_cur(0), a_max(ta.a), b_max(ta.b)
	{
	}

	/**
	 * Construct the iterator.
	 * @param corner1 Tile from where to begin iterating.
	 * @param corner2 Tile where to end the iterating.
	 */
	DiagonalTileIterator(TileIndex corner1, TileIndex corner2)
	{
		*this = DiagonalTileIterator(DiagonalTileArea(corner1, corner2));
	}

	TileIterator& operator ++() override;

	std::unique_ptr<TileIterator> Clone() const override
	{
		return std::make_unique<DiagonalTileIterator>(*this);
	}
};

#endif /* TILEAREA_TYPE_H */
