/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map_func.h Functions related to maps. */

#ifndef MAP_FUNC_H
#define MAP_FUNC_H

#include "core/math_func.hpp"
#include "tile_type.h"
#include "map_type.h"
#include "direction_func.h"

/**
 * Wrapper class to abstract away the way the tiles are stored. It is
 * intended to be used to access the "map" data of a single tile.
 *
 * The wrapper is expected to be fully optimized away by the compiler, even
 * with low optimization levels except when completely disabling it.
 */
class Tile {
private:
	friend struct Map;
	/**
	 * Data that is stored per tile. Also used TileExtended for this.
	 * Look at docs/landscape.html for the exact meaning of the members.
	 */
	struct TileBase {
		uint8_t type = 0; ///< The type (bits 4..7), bridges (2..3), rainforest/desert (0..1)
		uint8_t height = 0; ///< The height of the northern corner.
		uint16_t m2 = 0; ///< Primarily used for indices to towns, industries and stations
		uint8_t m1 = 0; ///< Primarily used for ownership information
		uint8_t m3 = 0; ///< General purpose
		uint8_t m4 = 0; ///< General purpose
		uint8_t m5 = 0; ///< General purpose
	};

	static_assert(sizeof(TileBase) == 8);

	/**
	 * Data that is stored per tile. Also used TileBase for this.
	 * Look at docs/landscape.html for the exact meaning of the members.
	 */
	struct TileExtended {
		uint8_t m6 = 0; ///< General purpose
		uint8_t m7 = 0; ///< Primarily used for newgrf support
		uint16_t m8 = 0; ///< General purpose
	};

	static std::unique_ptr<TileBase[]> base_tiles; ///< Pointer to the tile-array.
	static std::unique_ptr<TileExtended[]> extended_tiles; ///< Pointer to the extended tile-array.

	TileIndex tile; ///< The tile to access the map data for.

public:
	/**
	 * Create the tile wrapper for the given tile.
	 * @param tile The tile to access the map for.
	 */
	[[debug_inline]] inline Tile(TileIndex tile) : tile(tile) {}

	/**
	 * Create the tile wrapper for the given tile.
	 * @param tile The tile to access the map for.
	 */
	Tile(uint tile) : tile(tile) {}

	/**
	 * Implicit conversion to the TileIndex.
	 */
	[[debug_inline]] inline constexpr operator TileIndex() const { return this->tile; }

	/**
	 * Implicit conversion to the uint for bounds checking.
	 */
	[[debug_inline]] inline constexpr operator uint() const { return this->tile.base(); }

	/**
	 * The type (bits 4..7), bridges (2..3), rainforest/desert (0..1)
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &type()
	{
		return base_tiles[this->tile.base()].type;
	}

	/**
	 * The height of the northern corner
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the height for.
	 * @return reference to the byte holding the height.
	 */
	[[debug_inline]] inline uint8_t &height()
	{
		return base_tiles[this->tile.base()].height;
	}

	/**
	 * Primarily used for ownership information
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &m1()
	{
		return base_tiles[this->tile.base()].m1;
	}

	/**
	 * Primarily used for indices to towns, industries and stations
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the uint16_t holding the data.
	 */
	[[debug_inline]] inline uint16_t &m2()
	{
		return base_tiles[this->tile.base()].m2;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &m3()
	{
		return base_tiles[this->tile.base()].m3;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &m4()
	{
		return base_tiles[this->tile.base()].m4;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &m5()
	{
		return base_tiles[this->tile.base()].m5;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &m6()
	{
		return extended_tiles[this->tile.base()].m6;
	}

	/**
	 * Primarily used for newgrf support
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	[[debug_inline]] inline uint8_t &m7()
	{
		return extended_tiles[this->tile.base()].m7;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the uint16_t holding the data.
	 */
	[[debug_inline]] inline uint16_t &m8()
	{
		return extended_tiles[this->tile.base()].m8;
	}
};

/**
 * Size related data of the map.
 */
struct Map {
private:
	/**
	 * Iterator to iterate all Tiles
	 */
	struct Iterator {
		typedef Tile value_type;
		typedef Tile *pointer;
		typedef Tile &reference;
		typedef size_t difference_type;
		typedef std::forward_iterator_tag iterator_category;

		explicit Iterator(TileIndex index) : index(index) {}
		bool operator==(const Iterator &other) const { return this->index == other.index; }
		Tile operator*() const { return this->index; }
		Iterator & operator++() { this->index++; return *this; }
	private:
		TileIndex index;
	};

	/*
	 * Iterable ensemble of all Tiles
	 */
	struct IterateWrapper {
		Iterator begin() { return Iterator(TileIndex{}); }
		Iterator end() { return Iterator(TileIndex{Map::Size()}); }
		bool empty() { return false; }
	};

	static uint log_x;     ///< 2^_map_log_x == _map_size_x
	static uint log_y;     ///< 2^_map_log_y == _map_size_y
	static uint size_x;    ///< Size of the map along the X
	static uint size_y;    ///< Size of the map along the Y
	static uint size;      ///< The number of tiles on the map
	static uint tile_mask; ///< _map_size - 1 (to mask the mapsize)

	static uint initial_land_count; ///< Initial number of land tiles on the map.

public:
	static void Allocate(uint size_x, uint size_y);
	static void CountLandTiles();

	/**
	 * Logarithm of the map size along the X side.
	 * @note try to avoid using this one
	 * @return 2^"return value" == Map::SizeX()
	 */
	[[debug_inline]] inline static uint LogX()
	{
		return Map::log_x;
	}

	/**
	 * Logarithm of the map size along the y side.
	 * @note try to avoid using this one
	 * @return 2^"return value" == Map::SizeY()
	 */
	static inline uint LogY()
	{
		return Map::log_y;
	}

	/**
	 * Get the size of the map along the X
	 * @return the number of tiles along the X of the map
	 */
	[[debug_inline]] inline static uint SizeX()
	{
		return Map::size_x;
	}

	/**
	 * Get the size of the map along the Y
	 * @return the number of tiles along the Y of the map
	 */
	static inline uint SizeY()
	{
		return Map::size_y;
	}

	/**
	 * Get the size of the map
	 * @return the number of tiles of the map
	 */
	[[debug_inline]] inline static uint Size()
	{
		return Map::size;
	}

	/**
	 * Gets the maximum X coordinate within the map, including MP_VOID
	 * @return the maximum X coordinate
	 */
	[[debug_inline]] inline static uint MaxX()
	{
		return Map::SizeX() - 1;
	}

	/**
	 * Gets the maximum Y coordinate within the map, including MP_VOID
	 * @return the maximum Y coordinate
	 */
	static inline uint MaxY()
	{
		return Map::SizeY() - 1;
	}

	/**
	 * Scales the given value by the number of water tiles.
	 * @param n the value to scale
	 * @return the scaled size
	 */
	static inline uint ScaleByLandProportion(uint n)
	{
		/* Use 64-bit arithmetic to avoid overflow. */
		return static_cast<uint>(static_cast<uint64_t>(n) * Map::initial_land_count / Map::size);
	}

	/**
	 * 'Wraps' the given "tile" so it is within the map.
	 * It does this by masking the 'high' bits of.
	 * @param tile the tile to 'wrap'
	 */
	static inline TileIndex WrapToMap(TileIndex tile)
	{
		return TileIndex{tile.base() & Map::tile_mask};
	}

	/**
	 * Scales the given value by the map size, where the given value is
	 * for a 256 by 256 map.
	 * @param n the value to scale
	 * @return the scaled size
	 */
	static inline uint ScaleBySize(uint n)
	{
		/* Subtract 12 from shift in order to prevent integer overflow
		 * for large values of n. It's safe since the min mapsize is 64x64. */
		return CeilDiv(n << (Map::LogX() + Map::LogY() - 12), 1 << 4);
	}

	/**
	 * Scales the given value by the maps circumference, where the given
	 * value is for a 256 by 256 map
	 * @param n the value to scale
	 * @return the scaled size
	 */
	static inline uint ScaleBySize1D(uint n)
	{
		/* Normal circumference for the X+Y is 256+256 = 1<<9
		 * Note, not actually taking the full circumference into account,
		 * just half of it. */
		return CeilDiv((n << Map::LogX()) + (n << Map::LogY()), 1 << 9);
	}

	/**
	 * Check whether the map has been initialized, as to not try to save the map
	 * during crashlog when the map is not there yet.
	 * @return true when the map has been allocated/initialized.
	 */
	static bool IsInitialized()
	{
		return Tile::base_tiles != nullptr;
	}

	/**
	 * Returns an iterable ensemble of all Tiles
	 * @return an iterable ensemble of all Tiles
	 */
	static IterateWrapper Iterate() { return IterateWrapper(); }
};

/**
 * Returns the TileIndex of a coordinate.
 *
 * @param x The x coordinate of the tile
 * @param y The y coordinate of the tile
 * @return The TileIndex calculated by the coordinate
 */
[[debug_inline]] inline static TileIndex TileXY(uint x, uint y)
{
	return TileIndex{(y << Map::LogX()) + x};
}

/**
 * Calculates an offset for the given coordinate(-offset).
 *
 * This function calculate an offset value which can be added to a
 * #TileIndex. The coordinates can be negative.
 *
 * @param x The offset in x direction
 * @param y The offset in y direction
 * @return The resulting offset value of the given coordinate
 * @see ToTileIndexDiff(TileIndexDiffC)
 */
inline TileIndexDiff TileDiffXY(int x, int y)
{
	/* Multiplication gives much better optimization on MSVC than shifting.
	 * 0 << shift isn't optimized to 0 properly.
	 * Typically x and y are constants, and then this doesn't result
	 * in any actual multiplication in the assembly code.. */
	return (y * Map::SizeX()) + x;
}

/**
 * Get a tile from the virtual XY-coordinate.
 * @param x The virtual x coordinate of the tile.
 * @param y The virtual y coordinate of the tile.
 * @return The TileIndex calculated by the coordinate.
 */
[[debug_inline]] inline static TileIndex TileVirtXY(uint x, uint y)
{
	return TileIndex{(y >> 4 << Map::LogX()) + (x >> 4)};
}


/**
 * Get the X component of a tile
 * @param tile the tile to get the X component of
 * @return the X component
 */
[[debug_inline]] inline static uint TileX(TileIndex tile)
{
	return tile.base() & Map::MaxX();
}

/**
 * Get the Y component of a tile
 * @param tile the tile to get the Y component of
 * @return the Y component
 */
[[debug_inline]] inline static uint TileY(TileIndex tile)
{
	return tile.base() >> Map::LogX();
}

/**
 * Return the offset between two tiles from a TileIndexDiffC struct.
 *
 * This function works like #TileDiffXY(int, int) and returns the
 * difference between two tiles.
 *
 * @param tidc The coordinate of the offset as TileIndexDiffC
 * @return The difference between two tiles.
 * @see TileDiffXY(int, int)
 */
inline TileIndexDiff ToTileIndexDiff(TileIndexDiffC tidc)
{
	return TileDiffXY(tidc.x, tidc.y);
}

/* Helper functions to provide explicit +=/-= operators for TileIndex and TileIndexDiff. */
constexpr TileIndex &operator+=(TileIndex &tile, TileIndexDiff offset) { tile = tile + TileIndex(offset); return tile; }
constexpr TileIndex &operator-=(TileIndex &tile, TileIndexDiff offset) { tile = tile - TileIndex(offset); return tile; }

/**
 * Adds a given offset to a tile.
 *
 * @param tile The tile to add an offset to.
 * @param offset The offset to add.
 * @return The resulting tile.
 */
#ifndef _DEBUG
	constexpr TileIndex TileAdd(TileIndex tile, TileIndexDiff offset) { return tile + offset; }
#else
	TileIndex TileAdd(TileIndex tile, TileIndexDiff offset);
#endif

/**
 * Adds a given offset to a tile.
 *
 * @param tile The tile to add an offset to.
 * @param x The x offset to add to the tile.
 * @param y The y offset to add to the tile.
 * @return The resulting tile.
 */
inline TileIndex TileAddXY(TileIndex tile, int x, int y)
{
	return TileAdd(tile, TileDiffXY(x, y));
}

TileIndex TileAddWrap(TileIndex tile, int addx, int addy);

/**
 * Returns the TileIndexDiffC offset from a DiagDirection.
 *
 * @param dir The given direction
 * @return The offset as TileIndexDiffC value
 */
inline TileIndexDiffC TileIndexDiffCByDiagDir(DiagDirection dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[DIAGDIR_END];

	assert(IsValidDiagDirection(dir));
	return _tileoffs_by_diagdir[dir];
}

/**
 * Returns the TileIndexDiffC offset from a Direction.
 *
 * @param dir The given direction
 * @return The offset as TileIndexDiffC value
 */
inline TileIndexDiffC TileIndexDiffCByDir(Direction dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[DIR_END];

	assert(IsValidDirection(dir));
	return _tileoffs_by_dir[dir];
}

/**
 * Add a TileIndexDiffC to a TileIndex and returns the new one.
 *
 * Returns tile + the diff given in diff. If the result tile would end up
 * outside of the map, INVALID_TILE is returned instead.
 *
 * @param tile The base tile to add the offset on
 * @param diff The offset to add on the tile
 * @return The resulting TileIndex
 */
inline TileIndex AddTileIndexDiffCWrap(TileIndex tile, TileIndexDiffC diff)
{
	int x = TileX(tile) + diff.x;
	int y = TileY(tile) + diff.y;
	/* Negative value will become big positive value after cast */
	if ((uint)x >= Map::SizeX() || (uint)y >= Map::SizeY()) return INVALID_TILE;
	return TileXY(x, y);
}

/**
 * Returns the diff between two tiles
 *
 * @param tile_a from tile
 * @param tile_b to tile
 * @return the difference between tila_a and tile_b
 */
inline TileIndexDiffC TileIndexToTileIndexDiffC(TileIndex tile_a, TileIndex tile_b)
{
	TileIndexDiffC difference;

	difference.x = TileX(tile_a) - TileX(tile_b);
	difference.y = TileY(tile_a) - TileY(tile_b);

	return difference;
}

/* Functions to calculate distances */
uint DistanceManhattan(TileIndex, TileIndex); ///< also known as L1-Norm. Is the shortest distance one could go over diagonal tracks (or roads)
uint DistanceSquare(TileIndex, TileIndex); ///< Euclidean- or L2-Norm squared
uint DistanceMax(TileIndex, TileIndex); ///< also known as L-Infinity-Norm
uint DistanceMaxPlusManhattan(TileIndex, TileIndex); ///< Max + Manhattan
uint DistanceFromEdge(TileIndex); ///< shortest distance from any edge of the map
uint DistanceFromEdgeDir(TileIndex, DiagDirection); ///< distance from the map edge in given direction

/**
 * Convert an Axis to a TileIndexDiff
 *
 * @param axis The Axis
 * @return The resulting TileIndexDiff in southern direction (either SW or SE).
 */
inline TileIndexDiff TileOffsByAxis(Axis axis)
{
	extern const TileIndexDiffC _tileoffs_by_axis[];

	assert(IsValidAxis(axis));
	return ToTileIndexDiff(_tileoffs_by_axis[axis]);
}

/**
 * Convert a DiagDirection to a TileIndexDiff
 *
 * @param dir The DiagDirection
 * @return The resulting TileIndexDiff
 * @see TileIndexDiffCByDiagDir
 */
inline TileIndexDiff TileOffsByDiagDir(DiagDirection dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[DIAGDIR_END];

	assert(IsValidDiagDirection(dir));
	return ToTileIndexDiff(_tileoffs_by_diagdir[dir]);
}

/**
 * Convert a Direction to a TileIndexDiff.
 *
 * @param dir The direction to convert from
 * @return The resulting TileIndexDiff
 */
inline TileIndexDiff TileOffsByDir(Direction dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[DIR_END];

	assert(IsValidDirection(dir));
	return ToTileIndexDiff(_tileoffs_by_dir[dir]);
}

/**
 * Adds a Direction to a tile.
 *
 * @param tile The current tile
 * @param dir The direction in which we want to step
 * @return the moved tile
 */
inline TileIndex TileAddByDir(TileIndex tile, Direction dir)
{
	return TileAdd(tile, TileOffsByDir(dir));
}

/**
 * Adds a DiagDir to a tile.
 *
 * @param tile The current tile
 * @param dir The direction in which we want to step
 * @return the moved tile
 */
inline TileIndex TileAddByDiagDir(TileIndex tile, DiagDirection dir)
{
	return TileAdd(tile, TileOffsByDiagDir(dir));
}

/**
 * Determines the DiagDirection to get from one tile to another.
 * The tiles do not necessarily have to be adjacent.
 * @param tile_from Origin tile
 * @param tile_to Destination tile
 * @return DiagDirection from tile_from towards tile_to, or INVALID_DIAGDIR if the tiles are not on an axis
 */
inline DiagDirection DiagdirBetweenTiles(TileIndex tile_from, TileIndex tile_to)
{
	int dx = (int)TileX(tile_to) - (int)TileX(tile_from);
	int dy = (int)TileY(tile_to) - (int)TileY(tile_from);
	if (dx == 0) {
		if (dy == 0) return INVALID_DIAGDIR;
		return (dy < 0 ? DIAGDIR_NW : DIAGDIR_SE);
	} else {
		if (dy != 0) return INVALID_DIAGDIR;
		return (dx < 0 ? DIAGDIR_NE : DIAGDIR_SW);
	}
}

/**
 * Get a random tile out of a given seed.
 * @param r the random 'seed'
 * @return a valid tile
 */
inline TileIndex RandomTileSeed(uint32_t r)
{
	return Map::WrapToMap(TileIndex{r});
}

/**
 * Get a valid random tile.
 * @note a define so 'random' gets inserted in the place where it is actually
 *       called, thus making the random traces more explicit.
 * @return a valid tile
 */
#define RandomTile() RandomTileSeed(Random())

uint GetClosestWaterDistance(TileIndex tile, bool water);

#endif /* MAP_FUNC_H */
