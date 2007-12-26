/* $Id$ */

/** @file tile_map.h Map writing/reading functions for tiles. */

#ifndef TILE_MAP_H
#define TILE_MAP_H

#include "tile_type.h"
#include "slope_type.h"
#include "map_func.h"
#include "core/bitmath_func.hpp"

/**
 * Returns the height of a tile
 *
 * This function returns the height of the northern corner of a tile.
 * This is saved in the global map-array. It does not take affect by
 * any slope-data of the tile.
 *
 * @param tile The tile to get the height from
 * @return the height of the tile
 * @pre tile < MapSize()
 */
static inline uint TileHeight(TileIndex tile)
{
	assert(tile < MapSize());
	return GB(_m[tile].type_height, 0, 4);
}

/**
 * Sets the height of a tile.
 *
 * This function sets the height of the northern corner of a tile.
 *
 * @param tile The tile to change the height
 * @param height The new height value of the tile
 * @pre tile < MapSize()
 * @pre heigth <= MAX_TILE_HEIGHT
 */
static inline void SetTileHeight(TileIndex tile, uint height)
{
	assert(tile < MapSize());
	assert(height <= MAX_TILE_HEIGHT);
	SB(_m[tile].type_height, 0, 4, height);
}

/**
 * Returns the height of a tile in pixels.
 *
 * This function returns the height of the northern corner of a tile in pixels.
 *
 * @param tile The tile to get the height
 * @return The height of the tile in pixel
 */
static inline uint TilePixelHeight(TileIndex tile)
{
	return TileHeight(tile) * TILE_HEIGHT;
}

/**
 * Get the tiletype of a given tile.
 *
 * @param tile The tile to get the TileType
 * @return The tiletype of the tile
 * @pre tile < MapSize()
 */
static inline TileType GetTileType(TileIndex tile)
{
	assert(tile < MapSize());
	return (TileType)GB(_m[tile].type_height, 4, 4);
}

/**
 * Set the type of a tile
 *
 * This functions sets the type of a tile. If the type
 * MP_VOID is selected the tile must be at the south-west or
 * south-east edges of the map and vice versa.
 *
 * @param tile The tile to save the new type
 * @param type The type to save
 * @pre tile < MapSize()
 * @pre type MP_VOID <=> tile is on the south-east or south-west edge.
 */
static inline void SetTileType(TileIndex tile, TileType type)
{
	assert(tile < MapSize());
	/* VOID tiles (and no others) are exactly allowed at the lower left and right
	 * edges of the map */
	assert((TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY()) == (type == MP_VOID));
	SB(_m[tile].type_height, 4, 4, type);
}

/**
 * Checks if a tile is a give tiletype.
 *
 * This function checks if a tile got the given tiletype.
 *
 * @param tile The tile to check
 * @param type The type to check agains
 * @return true If the type matches agains the type of the tile
 */
static inline bool IsTileType(TileIndex tile, TileType type)
{
	return GetTileType(tile) == type;
}

/**
 * Returns the owner of a tile
 *
 * This function returns the owner of a tile. This cannot used
 * for tiles which type is one of MP_HOUSE, MP_VOID and MP_INDUSTRY
 * as no player owned any of these buildings.
 *
 * @param tile The tile to check
 * @return The owner of the tile
 * @pre tile < MapSize()
 * @pre The type of the tile must not be MP_HOUSE, MP_VOID and MP_INDUSTRY
 */
static inline Owner GetTileOwner(TileIndex tile)
{
	assert(tile < MapSize());
	assert(!IsTileType(tile, MP_HOUSE));
	assert(!IsTileType(tile, MP_VOID));
	assert(!IsTileType(tile, MP_INDUSTRY));

	return (Owner)_m[tile].m1;
}

/**
 * Sets the owner of a tile
 *
 * This function sets the owner status of a tile. Note that you cannot
 * set a owner for tiles of type MP_HOUSE, MP_VOID and MP_INDUSTRY.
 *
 * @param tile The tile to change the owner status.
 * @param owner The new owner.
 * @pre tile < MapSize()
 * @pre The type of the tile must not be MP_HOUSE, MP_VOID and MP_INDUSTRY
 */
static inline void SetTileOwner(TileIndex tile, Owner owner)
{
	assert(tile < MapSize());
	assert(!IsTileType(tile, MP_HOUSE));
	assert(!IsTileType(tile, MP_VOID));
	assert(!IsTileType(tile, MP_INDUSTRY));

	_m[tile].m1 = owner;
}

/**
 * Checks if a tile belongs to the given owner
 *
 * @param tile The tile to check
 * @param owner The owner to check agains
 * @return True if a tile belongs the the given owner
 */
static inline bool IsTileOwner(TileIndex tile, Owner owner)
{
	return GetTileOwner(tile) == owner;
}

/**
 * Set the tropic zone
 * @param tile the tile to set the zone of
 * @param type the new type
 * @pre assert(tile < MapSize());
 */
static inline void SetTropicZone(TileIndex tile, TropicZone type)
{
	assert(tile < MapSize());
	SB(_m[tile].m6, 0, 2, type);
}

/**
 * Get the tropic zone
 * @param tile the tile to get the zone of
 * @pre assert(tile < MapSize());
 * @return the zone type
 */
static inline TropicZone GetTropicZone(TileIndex tile)
{
	assert(tile < MapSize());
	return (TropicZone)GB(_m[tile].m6, 0, 2);
}

Slope GetTileSlope(TileIndex tile, uint *h);
uint GetTileZ(TileIndex tile);
uint GetTileMaxZ(TileIndex tile);

#endif /* TILE_TYPE_H */
