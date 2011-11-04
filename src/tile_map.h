/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tile_map.h Map writing/reading functions for tiles. */

#ifndef TILE_MAP_H
#define TILE_MAP_H

#include "slope_type.h"
#include "map_func.h"
#include "core/bitmath_func.hpp"
#include "settings_type.h"

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
	 * edges of the map. If _settings_game.construction.freeform_edges is true,
	 * the upper edges of the map are also VOID tiles. */
	assert((TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY() || (_settings_game.construction.freeform_edges && (TileX(tile) == 0 || TileY(tile) == 0))) == (type == MP_VOID));
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
 * Checks if a tile is valid
 *
 * @param tile The tile to check
 * @return True if the tile is on the map and not one of MP_VOID.
 */
static inline bool IsValidTile(TileIndex tile)
{
	return tile < MapSize() && !IsTileType(tile, MP_VOID);
}

/**
 * Returns the owner of a tile
 *
 * This function returns the owner of a tile. This cannot used
 * for tiles which type is one of MP_HOUSE, MP_VOID and MP_INDUSTRY
 * as no company owned any of these buildings.
 *
 * @param tile The tile to check
 * @return The owner of the tile
 * @pre IsValidTile(tile)
 * @pre The type of the tile must not be MP_HOUSE and MP_INDUSTRY
 */
static inline Owner GetTileOwner(TileIndex tile)
{
	assert(IsValidTile(tile));
	assert(!IsTileType(tile, MP_HOUSE));
	assert(!IsTileType(tile, MP_INDUSTRY));

	return (Owner)GB(_m[tile].m1, 0, 5);
}

/**
 * Sets the owner of a tile
 *
 * This function sets the owner status of a tile. Note that you cannot
 * set a owner for tiles of type MP_HOUSE, MP_VOID and MP_INDUSTRY.
 *
 * @param tile The tile to change the owner status.
 * @param owner The new owner.
 * @pre IsValidTile(tile)
 * @pre The type of the tile must not be MP_HOUSE and MP_INDUSTRY
 */
static inline void SetTileOwner(TileIndex tile, Owner owner)
{
	assert(IsValidTile(tile));
	assert(!IsTileType(tile, MP_HOUSE));
	assert(!IsTileType(tile, MP_INDUSTRY));

	SB(_m[tile].m1, 0, 5, owner);
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
 * @pre tile < MapSize()
 */
static inline void SetTropicZone(TileIndex tile, TropicZone type)
{
	assert(tile < MapSize());
	assert(!IsTileType(tile, MP_VOID) || type == TROPICZONE_NORMAL);
	SB(_m[tile].m6, 0, 2, type);
}

/**
 * Get the tropic zone
 * @param tile the tile to get the zone of
 * @pre tile < MapSize()
 * @return the zone type
 */
static inline TropicZone GetTropicZone(TileIndex tile)
{
	assert(tile < MapSize());
	return (TropicZone)GB(_m[tile].m6, 0, 2);
}

/**
 * Get the current animation frame
 * @param t the tile
 * @pre IsTileType(t, MP_HOUSE) || IsTileType(t, MP_OBJECT) || IsTileType(t, MP_INDUSTRY) ||IsTileType(t, MP_STATION)
 * @return frame number
 */
static inline byte GetAnimationFrame(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_OBJECT) || IsTileType(t, MP_INDUSTRY) ||IsTileType(t, MP_STATION));
	return _me[t].m7;
}

/**
 * Set a new animation frame
 * @param t the tile
 * @param frame the new frame number
 * @pre IsTileType(t, MP_HOUSE) || IsTileType(t, MP_OBJECT) || IsTileType(t, MP_INDUSTRY) ||IsTileType(t, MP_STATION)
 */
static inline void SetAnimationFrame(TileIndex t, byte frame)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_OBJECT) || IsTileType(t, MP_INDUSTRY) ||IsTileType(t, MP_STATION));
	_me[t].m7 = frame;
}

Slope GetTilePixelSlope(TileIndex tile, uint *h);
uint GetTilePixelZ(TileIndex tile);
uint GetTileMaxPixelZ(TileIndex tile);


/**
 * Calculate a hash value from a tile position
 *
 * @param x The X coordinate
 * @param y The Y coordinate
 * @return The hash of the tile
 */
static inline uint TileHash(uint x, uint y)
{
	uint hash = x >> 4;
	hash ^= x >> 6;
	hash ^= y >> 4;
	hash -= y >> 6;
	return hash;
}

/**
 * Get the last two bits of the TileHash
 *  from a tile position.
 *
 * @see TileHash()
 * @param x The X coordinate
 * @param y The Y coordinate
 * @return The last two bits from hash of the tile
 */
static inline uint TileHash2Bit(uint x, uint y)
{
	return GB(TileHash(x, y), 0, 2);
}

#endif /* TILE_MAP_H */
