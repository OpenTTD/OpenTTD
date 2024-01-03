/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilematrix_type.hpp Template for storing a value per area of the map. */

#ifndef TILEMATRIX_TYPE_HPP
#define TILEMATRIX_TYPE_HPP

#include "core/alloc_func.hpp"
#include "tilearea_type.h"

/**
 * A simple matrix that stores one value per N*N square of the map.
 * Storage is only allocated for the part of the map that has values
 * assigned.
 *
 * @note No constructor is called for newly allocated values, you
 *       have to do this yourself if needed.
 * @tparam T The type of the stored items.
 * @tparam N Grid size.
 */
template <typename T, uint N>
class TileMatrix {

	/** Allocates space for a new tile in the matrix.
	 * @param tile Tile to add.
	 */
	void AllocateStorage(TileIndex tile)
	{
		uint old_left = TileX(this->area.tile) / N;
		uint old_top  = TileY(this->area.tile) / N;
		uint old_w    = this->area.w / N;
		uint old_h    = this->area.h / N;

		/* Add the square the tile is in to the tile area. We do this
		 * by adding top-left and bottom-right of the square. */
		uint grid_x = (TileX(tile) / N) * N;
		uint grid_y = (TileY(tile) / N) * N;
		this->area.Add(TileXY(grid_x, grid_y));
		this->area.Add(TileXY(grid_x + N - 1, grid_y + N - 1));

		/* Allocate new storage. */
		T *new_data = CallocT<T>(this->area.w / N * this->area.h / N);

		if (old_w > 0) {
			/* Copy old data if present. */
			uint offs_x = old_left - TileX(this->area.tile) / N;
			uint offs_y = old_top  - TileY(this->area.tile) / N;

			for (uint row = 0; row < old_h; row++) {
				MemCpyT(&new_data[(row + offs_y) * this->area.w / N + offs_x], &this->data[row * old_w], old_w);
			}
		}

		free(this->data);
		this->data = new_data;
	}

public:
	static const uint GRID = N;

	TileArea area; ///< Area covered by the matrix.

	T *data; ///< Pointer to data array.

	TileMatrix() : area(INVALID_TILE, 0, 0), data(nullptr) {}

	~TileMatrix()
	{
		free(this->data);
	}

	/**
	 * Get the total covered area.
	 * @return The area covered by the matrix.
	 */
	const TileArea &GetArea() const
	{
		return this->area;
	}

	/**
	 * Get the area of the matrix square that contains a specific tile.
	 * @param tile The tile to get the map area for.
	 * @param extend Extend the area by this many squares on all sides.
	 * @return Tile area containing the tile.
	 */
	static TileArea GetAreaForTile(TileIndex tile, uint extend = 0)
	{
		uint tile_x = (TileX(tile) / N) * N;
		uint tile_y = (TileY(tile) / N) * N;
		uint w = N, h = N;

		w += std::min(extend * N, tile_x);
		h += std::min(extend * N, tile_y);

		tile_x -= std::min(extend * N, tile_x);
		tile_y -= std::min(extend * N, tile_y);

		w += std::min(extend * N, Map::SizeX() - tile_x - w);
		h += std::min(extend * N, Map::SizeY() - tile_y - h);

		return TileArea(TileXY(tile_x, tile_y), w, h);
	}

	/**
	 * Extend the coverage area to include a tile.
	 * @param tile The tile to include.
	 */
	void Add(TileIndex tile)
	{
		if (!this->area.Contains(tile)) {
			this->AllocateStorage(tile);
		}
	}

	/**
	 * Get the value associated to a tile index.
	 * @param tile The tile to get the value for.
	 * @return Pointer to the value.
	 */
	T *Get(TileIndex tile)
	{
		this->Add(tile);

		tile -= this->area.tile;
		uint x = TileX(tile) / N;
		uint y = TileY(tile) / N;

		return &this->data[y * this->area.w / N + x];
	}

	/** Array access operator, see #Get. */
	inline T &operator[](TileIndex tile)
	{
		return *this->Get(tile);
	}
};

#endif /* TILEMATRIX_TYPE_HPP */
