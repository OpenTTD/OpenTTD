/* $Id$ */

/** @file tile_type.h Types related to tiles. */

#ifndef TILE_TYPE_H
#define TILE_TYPE_H

#include "core/enum_type.hpp"

enum {
	TILE_SIZE   = 16,   ///< Tiles are 16x16 "units" in size
	TILE_PIXELS = 32,   ///< a tile is 32x32 pixels
	TILE_HEIGHT =  8,   ///< The standard height-difference between tiles on two levels is 8 (z-diff 8)

	MAX_TILE_HEIGHT     = 15,                    ///< Maximum allowed tile height
	MAX_SNOWLINE_HEIGHT = (MAX_TILE_HEIGHT - 2), ///< Maximum allowed snowline height
};


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
 * The tropiczone is not modified during gameplay. It mainly affects tree growth. (desert tiles are visible though)
 *
 * In randomly generated maps:
 *  TROPICZONE_DESERT: Generated everywhere, if there is neither water nor mountains (TileHeight >= 4) in a certain distance from the tile.
 *  TROPICZONE_RAINFOREST: Genereated everywhere, if there is no desert in a certain distance from the tile.
 *  TROPICZONE_NORMAL: Everywhere else, i.e. between desert and rainforest and on sea (if you clear the water).
 *
 * In scenarios:
 *  TROPICZONE_NORMAL: Default value.
 *  TROPICZONE_DESERT: Placed manually.
 *  TROPICZONE_RAINFOREST: Placed if you plant certain rainforest-trees.
 */
enum TropicZone {
	TROPICZONE_NORMAL     = 0,      ///< Normal tropiczone
	TROPICZONE_DESERT     = 1,      ///< Tile is desert
	TROPICZONE_RAINFOREST = 2,      ///< Rainforest tile
};

/**
 * The index/ID of a Tile.
 */
typedef uint32 TileIndex;

/**
 * The very nice invalid tile marker
 */
static const TileIndex INVALID_TILE = (TileIndex)-1;

#endif /* TILE_TYPE_H */
