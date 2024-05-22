/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bitmap_type.hpp Bitmap functions. */

#ifndef BITMAP_TYPE_HPP
#define BITMAP_TYPE_HPP


/** Represents a tile area containing containing individually set tiles.
 * Each tile must be contained within the preallocated area.
 * A std::vector<bool> is used to mark which tiles are contained.
 */
class BitmapTileArea : public TileArea {
protected:
	std::vector<bool> data;

	inline uint Index(uint x, uint y) const { return y * this->w + x; }

	inline uint Index(TileIndex tile) const { return Index(TileX(tile) - TileX(this->tile), TileY(tile) - TileY(this->tile)); }

public:
	BitmapTileArea()
	{
		this->tile = INVALID_TILE;
		this->w = 0;
		this->h = 0;
	}

	BitmapTileArea(const TileArea &ta)
	{
		this->tile = ta.tile;
		this->w = ta.w;
		this->h = ta.h;
		this->data.resize(Index(this->w, this->h));
	}

	/**
	 * Reset and clear the BitmapTileArea.
	 */
	void Reset()
	{
		this->tile = INVALID_TILE;
		this->w = 0;
		this->h = 0;
		this->data.clear();
	}

	/**
	 * Initialize the BitmapTileArea with the specified Rect.
	 * @param rect Rect to use.
	 */
	void Initialize(const Rect &r)
	{
		this->tile = TileXY(r.left, r.top);
		this->w = r.Width();
		this->h = r.Height();
		this->data.clear();
		this->data.resize(Index(w, h));
	}

	void Initialize(const TileArea &ta)
	{
		this->tile = ta.tile;
		this->w = ta.w;
		this->h = ta.h;
		this->data.clear();
		this->data.resize(Index(w, h));
	}

	/**
	 * Add a tile as part of the tile area.
	 * @param tile Tile to add.
	 */
	inline void SetTile(TileIndex tile)
	{
		assert(this->Contains(tile));
		this->data[Index(tile)] = true;
	}

	/**
	 * Clear a tile from the tile area.
	 * @param tile Tile to clear
	 */
	inline void ClrTile(TileIndex tile)
	{
		assert(this->Contains(tile));
		this->data[Index(tile)] = false;
	}

	/**
	 * Test if a tile is part of the tile area.
	 * @param tile Tile to check
	 */
	inline bool HasTile(TileIndex tile) const
	{
		return this->Contains(tile) && this->data[Index(tile)];
	}
};

/** Iterator to iterate over all tiles belonging to a bitmaptilearea. */
class BitmapTileIterator : public OrthogonalTileIterator {
protected:
	const BitmapTileArea *bitmap;
public:
	/**
	 * Construct the iterator.
	 * @param bitmap BitmapTileArea to iterate.
	 */
	BitmapTileIterator(const BitmapTileArea &bitmap) : OrthogonalTileIterator(bitmap), bitmap(&bitmap)
	{
		if (!this->bitmap->HasTile(TileIndex(this->tile))) ++(*this);
	}

	inline TileIterator& operator ++() override
	{
		(*this).OrthogonalTileIterator::operator++();
		while (this->tile != INVALID_TILE && !this->bitmap->HasTile(TileIndex(this->tile))) {
			(*this).OrthogonalTileIterator::operator++();
		}
		return *this;
	}

	std::unique_ptr<TileIterator> Clone() const override
	{
		return std::make_unique<BitmapTileIterator>(*this);
	}
};

#endif /* BITMAP_TYPE_HPP */
