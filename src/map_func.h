/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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
		byte   type;   ///< The type (bits 4..7), bridges (2..3), rainforest/desert (0..1)
		byte   height; ///< The height of the northern corner.
		uint16_t m2;     ///< Primarily used for indices to towns, industries and stations
		byte   m1;     ///< Primarily used for ownership information
		byte   m3;     ///< General purpose
		byte   m4;     ///< General purpose
		byte   m5;     ///< General purpose
	};

	static_assert(sizeof(TileBase) == 8);

	/**
	 * Data that is stored per tile. Also used TileBase for this.
	 * Look at docs/landscape.html for the exact meaning of the members.
	 */
	struct TileExtended {
		byte m6;   ///< General purpose
		byte m7;   ///< Primarily used for newgrf support
		uint16_t m8; ///< General purpose
	};

	static TileBase *base_tiles;         ///< Pointer to the tile-array.
	static TileExtended *extended_tiles; ///< Pointer to the extended tile-array.

	TileIndex tile; ///< The tile to access the map data for.

public:
	/**
	 * Create the tile wrapper for the given tile.
	 * @param tile The tile to access the map for.
	 */
	debug_inline Tile(TileIndex tile) : tile(tile) {}

	/**
	 * Create the tile wrapper for the given tile.
	 * @param tile The tile to access the map for.
	 */
	Tile(uint tile) : tile(tile) {}

	/**
	 * Implicit conversion to the TileIndex.
	 */
	debug_inline constexpr operator TileIndex() const { return tile; }

	/**
	 * Implicit conversion to the uint for bounds checking.
	 */
	debug_inline constexpr operator uint() const { return tile.base(); }

	/**
	 * The type (bits 4..7), bridges (2..3), rainforest/desert (0..1)
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &type()
	{
		return base_tiles[tile.base()].type;
	}

	/**
	 * The height of the northern corner
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the height for.
	 * @return reference to the byte holding the height.
	 */
	debug_inline byte &height()
	{
		return base_tiles[tile.base()].height;
	}

	/**
	 * Primarily used for ownership information
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &m1()
	{
		return base_tiles[tile.base()].m1;
	}

	/**
	 * Primarily used for indices to towns, industries and stations
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the uint16_t holding the data.
	 */
	debug_inline uint16_t &m2()
	{
		return base_tiles[tile.base()].m2;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &m3()
	{
		return base_tiles[tile.base()].m3;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &m4()
	{
		return base_tiles[tile.base()].m4;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &m5()
	{
		return base_tiles[tile.base()].m5;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &m6()
	{
		return extended_tiles[tile.base()].m6;
	}

	/**
	 * Primarily used for newgrf support
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the byte holding the data.
	 */
	debug_inline byte &m7()
	{
		return extended_tiles[tile.base()].m7;
	}

	/**
	 * General purpose
	 *
	 * Look at docs/landscape.html for the exact meaning of the data.
	 * @param tile The tile to get the data for.
	 * @return reference to the uint16_t holding the data.
	 */
	debug_inline uint16_t &m8()
	{
		return extended_tiles[tile.base()].m8;
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
		bool operator!=(const Iterator &other) const { return !(*this == other); }
		Tile operator*() const { return this->index; }
		Iterator & operator++() { this->index++; return *this; }
	private:
		TileIndex index;
	};

	/*
	 * Iterable ensemble of all Tiles
	 */
	struct IterateWrapper {
		Iterator begin() { return Iterator(0); }
		Iterator end() { return Iterator(Map::Size()); }
		bool empty() { return false; }
	};

	static uint log_x;     ///< 2^_map_log_x == _map_size_x
	static uint log_y;     ///< 2^_map_log_y == _map_size_y
	static uint size_x;    ///< Size of the map along the X
	static uint size_y;    ///< Size of the map along the Y
	static uint size;      ///< The number of tiles on the map
	static uint tile_mask; ///< _map_size - 1 (to mask the mapsize)

public:
	static void Allocate(uint size_x, uint size_y);

	/**
	 * Logarithm of the map size along the X side.
	 * @note try to avoid using this one
	 * @return 2^"return value" == Map::SizeX()
	 */
	debug_inline static uint LogX()
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
	debug_inline static uint SizeX()
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
	debug_inline static uint Size()
	{
		return Map::size;
	}

	/**
	 * Gets the maximum X coordinate within the map, including MP_VOID
	 * @return the maximum X coordinate
	 */
	debug_inline static uint MaxX()
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
	 * 'Wraps' the given "tile" so it is within the map.
	 * It does this by masking the 'high' bits of.
	 * @param tile the tile to 'wrap'
	 */
	static inline TileIndex WrapToMap(TileIndex tile)
	{
		return tile.base() & Map::tile_mask;
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
 * An offset value between two tiles.
 *
 * This value is used for the difference between
 * two tiles. It can be added to a TileIndex to get
 * the resulting TileIndex of the start tile applied
 * with this saved difference.
 *
 * @see TileDiffXY(int, int)
 */
typedef int32_t TileIndexDiff;

/**
 * Returns the TileIndex of a coordinate.
 *
 * @param x The x coordinate of the tile
 * @param y The y coordinate of the tile
 * @return The TileIndex calculated by the coordinate
 */
debug_inline static TileIndex TileXY(uint x, uint y)
{
	return (y << Map::LogX()) + x;
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
static inline TileIndexDiff TileDiffXY(int x, int y)
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
debug_inline static TileIndex TileVirtXY(uint x, uint y)
{
	return (y >> 4 << Map::LogX()) + (x >> 4);
}


/**
 * Get the X component of a tile
 * @param tile the tile to get the X component of
 * @return the X component
 */
debug_inline static uint TileX(TileIndex tile)
{
	return tile.base() & Map::MaxX();
}

/**
 * Get the Y component of a tile
 * @param tile the tile to get the Y component of
 * @return the Y component
 */
debug_inline static uint TileY(TileIndex tile)
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
static inline TileIndexDiff ToTileIndexDiff(TileIndexDiffC tidc)
{
	return (tidc.y << Map::LogX()) + tidc.x;
}


#ifndef _DEBUG
	/**
	 * Adds two tiles together.
	 *
	 * @param x One tile
	 * @param y Another tile to add
	 * @return The resulting tile(index)
	 */
#	define TILE_ADD(x, y) ((x) + (y))
#else
	extern TileIndex TileAdd(TileIndex tile, TileIndexDiff add,
		const char *exp, const char *file, int line);
#	define TILE_ADD(x, y) (TileAdd((x), (y), #x " + " #y, __FILE__, __LINE__))
#endif

/**
 * Adds a given offset to a tile.
 *
 * @param tile The tile to add an offset on it
 * @param x The x offset to add to the tile
 * @param y The y offset to add to the tile
 */
#define TILE_ADDXY(tile, x, y) TILE_ADD(tile, TileDiffXY(x, y))

TileIndex TileAddWrap(TileIndex tile, int addx, int addy);

/**
 * Returns the TileIndexDiffC offset from a DiagDirection.
 *
 * @param dir The given direction
 * @return The offset as TileIndexDiffC value
 */
static inline TileIndexDiffC TileIndexDiffCByDiagDir(DiagDirection dir)
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
static inline TileIndexDiffC TileIndexDiffCByDir(Direction dir)
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
static inline TileIndex AddTileIndexDiffCWrap(TileIndex tile, TileIndexDiffC diff)
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
static inline TileIndexDiffC TileIndexToTileIndexDiffC(TileIndex tile_a, TileIndex tile_b)
{
	TileIndexDiffC difference;

	difference.x = TileX(tile_a) - TileX(tile_b);
	difference.y = TileY(tile_a) - TileY(tile_b);

	return difference;
}

/* Functions to calculate distances */
uint DistanceManhattan(TileIndex, TileIndex); ///< also known as L1-Norm. Is the shortest distance one could go over diagonal tracks (or roads)
uint DistanceSquare(TileIndex, TileIndex); ///< euclidian- or L2-Norm squared
uint DistanceMax(TileIndex, TileIndex); ///< also known as L-Infinity-Norm
uint DistanceMaxPlusManhattan(TileIndex, TileIndex); ///< Max + Manhattan
uint DistanceFromEdge(TileIndex); ///< shortest distance from any edge of the map
uint DistanceFromEdgeDir(TileIndex, DiagDirection); ///< distance from the map edge in given direction

/**
 * Convert a DiagDirection to a TileIndexDiff
 *
 * @param dir The DiagDirection
 * @return The resulting TileIndexDiff
 * @see TileIndexDiffCByDiagDir
 */
static inline TileIndexDiff TileOffsByDiagDir(DiagDirection dir)
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
static inline TileIndexDiff TileOffsByDir(Direction dir)
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
static inline TileIndex TileAddByDir(TileIndex tile, Direction dir)
{
	return TILE_ADD(tile, TileOffsByDir(dir));
}

/**
 * Adds a DiagDir to a tile.
 *
 * @param tile The current tile
 * @param dir The direction in which we want to step
 * @return the moved tile
 */
static inline TileIndex TileAddByDiagDir(TileIndex tile, DiagDirection dir)
{
	return TILE_ADD(tile, TileOffsByDiagDir(dir));
}

/**
 * Determines the DiagDirection to get from one tile to another.
 * The tiles do not necessarily have to be adjacent.
 * @param tile_from Origin tile
 * @param tile_to Destination tile
 * @return DiagDirection from tile_from towards tile_to, or INVALID_DIAGDIR if the tiles are not on an axis
 */
static inline DiagDirection DiagdirBetweenTiles(TileIndex tile_from, TileIndex tile_to)
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
 * A callback function type for searching tiles.
 *
 * @param tile The tile to test
 * @param user_data additional data for the callback function to use
 * @return A boolean value, depend on the definition of the function.
 */
typedef bool TestTileOnSearchProc(TileIndex tile, void *user_data);

bool CircularTileSearch(TileIndex *tile, uint size, TestTileOnSearchProc proc, void *user_data);
bool CircularTileSearch(TileIndex *tile, uint radius, uint w, uint h, TestTileOnSearchProc proc, void *user_data);

/**
 * Get a random tile out of a given seed.
 * @param r the random 'seed'
 * @return a valid tile
 */
static inline TileIndex RandomTileSeed(uint32_t r)
{
	return Map::WrapToMap(r);
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
