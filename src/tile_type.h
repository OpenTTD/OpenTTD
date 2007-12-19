/* $Id$ */

/** @file tile_type.h Types related to tiles. */

#ifndef TILE_TYPE_H
#define TILE_TYPE_H

#include "core/enum_type.hpp"

/** Maximum allowed tile height */
#define MAX_TILE_HEIGHT 15

/** Maximum allowed snowline height */
#define MAX_SNOWLINE_HEIGHT (MAX_TILE_HEIGHT - 2)

/**
 * The different type of a tile.
 *
 * Each tile belongs to one type, according whatever is build on it.
 *
 * @note A railway with a crossing street is marked as MP_ROAD.
 */
enum TileType {
	MP_CLEAR,               ///< A tile without any structures, i.e. grass, rocks, farm fields etc.
	MP_RAILWAY,             ///< A railway
	MP_ROAD,                ///< A tile with road (or tram tracks)
	MP_HOUSE,               ///< A house by a town
	MP_TREES,               ///< Tile got trees
	MP_STATION,             ///< A tile of a station
	MP_WATER,               ///< Water tile
	MP_VOID,                ///< Invisible tiles at the SW and SE border
	MP_INDUSTRY,            ///< Part of an industry
	MP_TUNNELBRIDGE,        ///< Tunnel entry/exit and bridge heads
	MP_UNMOVABLE,           ///< Contains an object with cannot be removed like transmitters
};

/**
 * Additional infos of a tile on a tropic game.
 *
 * Each non-water tile in a tropic game is either a rainforest tile or a
 * desert one.
 */
enum TropicZone {
	TROPICZONE_INVALID    = 0,      ///< Invalid tropiczone-type
	TROPICZONE_DESERT     = 1,      ///< Tile is desert
	TROPICZONE_RAINFOREST = 2,      ///< Rainforest tile
};

/**
 * The index/ID of a Tile.
 */
typedef uint32 TileIndex;

#endif /* TILE_TYPE_H */
