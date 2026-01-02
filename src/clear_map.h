/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file clear_map.h Map accessors for 'clear' tiles. */

#ifndef CLEAR_MAP_H
#define CLEAR_MAP_H

#include "bridge_map.h"
#include "industry_type.h"

/**
 * Ground types. Valid densities in comments after the enum.
 */
enum ClearGround : uint8_t {
	CLEAR_GRASS  = 0, ///< 0-3
	CLEAR_ROUGH  = 1, ///< 3
	CLEAR_ROCKS  = 2, ///< 3
	CLEAR_FIELDS = 3, ///< 3
	CLEAR_DESERT = 5, ///< 1,3
};


/**
 * Test if a tile is covered with snow.
 * @param t the tile to check
 * @pre IsTileType(t, TileType::Clear)
 * @return whether the tile is covered with snow.
 */
inline bool IsSnowTile(Tile t)
{
	assert(IsTileType(t, TileType::Clear));
	return t.GetTileBaseAs<TileType::Clear>().snow_presence;
}

/**
 * Get the type of clear tile.
 * @param t the tile to get the clear ground type of
 * @pre IsTileType(t, TileType::Clear)
 * @return the ground type
 */
inline ClearGround GetClearGround(Tile t)
{
	assert(IsTileType(t, TileType::Clear));
	return static_cast<ClearGround>(t.GetTileExtendedAs<TileType::Clear>().ground);
}

/**
 * Set the type of clear tile.
 * @param t  the tile to set the clear ground type of
 * @param ct the ground type
 * @pre IsTileType(t, TileType::Clear)
 */
inline bool IsClearGround(Tile t, ClearGround ct)
{
	return GetClearGround(t) == ct;
}


/**
 * Get the density of a non-field clear tile.
 * @param t the tile to get the density of
 * @pre IsTileType(t, TileType::Clear)
 * @return the density
 */
inline uint GetClearDensity(Tile t)
{
	assert(IsTileType(t, TileType::Clear));
	return t.GetTileExtendedAs<TileType::Clear>().density;
}

/**
 * Increment the density of a non-field clear tile.
 * @param t the tile to increment the density of
 * @param d the amount to increment the density with
 * @pre IsTileType(t, TileType::Clear)
 */
inline void AddClearDensity(Tile t, int d)
{
	assert(IsTileType(t, TileType::Clear)); // XXX incomplete
	t.GetTileExtendedAs<TileType::Clear>().density += d;
}

/**
 * Set the density of a non-field clear tile.
 * @param t the tile to set the density of
 * @param d the new density
 * @pre IsTileType(t, TileType::Clear)
 */
inline void SetClearDensity(Tile t, uint d)
{
	assert(IsTileType(t, TileType::Clear));
	t.GetTileExtendedAs<TileType::Clear>().density = d;
}


/**
 * Get the counter used to advance to the next clear density/field type.
 * @param t the tile to get the counter of
 * @pre IsTileType(t, TileType::Clear)
 * @return the value of the counter
 */
inline uint GetClearCounter(Tile t)
{
	assert(IsTileType(t, TileType::Clear));
	return t.GetTileExtendedAs<TileType::Clear>().update;
}

/**
 * Increments the counter used to advance to the next clear density/field type.
 * @param t the tile to increment the counter of
 * @param c the amount to increment the counter with
 * @pre IsTileType(t, TileType::Clear)
 */
inline void AddClearCounter(Tile t, int c)
{
	assert(IsTileType(t, TileType::Clear)); // XXX incomplete
	t.GetTileExtendedAs<TileType::Clear>().update += c;
}

/**
 * Sets the counter used to advance to the next clear density/field type.
 * @param t the tile to set the counter of
 * @param c the amount to set the counter to
 * @pre IsTileType(t, TileType::Clear)
 */
inline void SetClearCounter(Tile t, uint c)
{
	assert(IsTileType(t, TileType::Clear)); // XXX incomplete
	t.GetTileExtendedAs<TileType::Clear>().update = c;
}


/**
 * Sets ground type and density in one go, also sets the counter to 0
 * @param t       the tile to set the ground type and density for
 * @param type    the new ground type of the tile
 * @param density the density of the ground tile
 * @pre IsTileType(t, TileType::Clear)
 */
inline void SetClearGroundDensity(Tile t, ClearGround type, uint density)
{
	assert(IsTileType(t, TileType::Clear)); // XXX incomplete
	auto &extended = t.GetTileExtendedAs<TileType::Clear>();
	extended.density = density;
	extended.ground = type;
	extended.update = 0;
}


/**
 * Get the field type (production stage) of the field
 * @param t the field to get the type of
 * @pre GetClearGround(t) == CLEAR_FIELDS
 * @return the field type
 */
inline uint GetFieldType(Tile t)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	return t.GetTileBaseAs<TileType::Clear>().field_type;
}

/**
 * Set the field type (production stage) of the field
 * @param t the field to get the type of
 * @param f the field type
 * @pre GetClearGround(t) == CLEAR_FIELDS
 */
inline void SetFieldType(Tile t, uint f)
{
	assert(GetClearGround(t) == CLEAR_FIELDS); // XXX incomplete
	t.GetTileBaseAs<TileType::Clear>().field_type = f;
}

/**
 * Get the industry (farm) that made the field
 * @param t the field to get creating industry of
 * @pre GetClearGround(t) == CLEAR_FIELDS
 * @return the industry that made the field
 */
inline IndustryID GetIndustryIndexOfField(Tile t)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	return static_cast<IndustryID>(t.GetTileExtendedAs<TileType::Clear>().farm_index);
}

/**
 * Set the industry (farm) that made the field
 * @param t the field to get creating industry of
 * @param i the industry that made the field
 * @pre GetClearGround(t) == CLEAR_FIELDS
 */
inline void SetIndustryIndexOfField(Tile t, IndustryID i)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	t.GetTileExtendedAs<TileType::Clear>().farm_index = i.base();
}


/**
 * Is there a fence at the given border?
 * @param t the tile to check for fences
 * @param side the border to check
 * @pre IsClearGround(t, CLEAR_FIELDS)
 * @return 0 if there is no fence, otherwise the fence type
 */
inline uint GetFence(Tile t, DiagDirection side)
{
	assert(IsClearGround(t, CLEAR_FIELDS));
	switch (side) {
		default: NOT_REACHED();
		case DIAGDIR_SE: return t.GetTileBaseAs<TileType::Clear>().hedge_SE;
		case DIAGDIR_SW: return t.GetTileBaseAs<TileType::Clear>().hedge_SW;
		case DIAGDIR_NE: return t.GetTileBaseAs<TileType::Clear>().hedge_NE;
		case DIAGDIR_NW: return t.GetTileExtendedAs<TileType::Clear>().hedge_NW;
	}
}

/**
 * Sets the type of fence (and whether there is one) for the given border.
 * @param t the tile to check for fences
 * @param side the border to check
 * @param h 0 if there is no fence, otherwise the fence type
 * @pre IsClearGround(t, CLEAR_FIELDS)
 */
inline void SetFence(Tile t, DiagDirection side, uint h)
{
	assert(IsClearGround(t, CLEAR_FIELDS));
	switch (side) {
		default: NOT_REACHED();
		case DIAGDIR_SE: t.GetTileBaseAs<TileType::Clear>().hedge_SE = h; break;
		case DIAGDIR_SW: t.GetTileBaseAs<TileType::Clear>().hedge_SW = h; break;
		case DIAGDIR_NE: t.GetTileBaseAs<TileType::Clear>().hedge_NE = h; break;
		case DIAGDIR_NW: t.GetTileExtendedAs<TileType::Clear>().hedge_NW = h; break;
	}
}


/**
 * Make a clear tile.
 * @param t       the tile to make a clear tile
 * @param g       the type of ground
 * @param density the density of the grass/snow/desert etc
 */
inline void MakeClear(Tile t, ClearGround g, uint density)
{
	SetTileType(t, TileType::Clear);
	t.ResetData();
	SetTileOwner(t, OWNER_NONE);
	SetClearGroundDensity(t, g, density);
}


/**
 * Make a (farm) field tile.
 * @param t          the tile to make a farm field
 * @param field_type the 'growth' level of the field
 * @param industry   the industry this tile belongs to
 */
inline void MakeField(Tile t, uint field_type, IndustryID industry)
{
	MakeClear(t, CLEAR_FIELDS, 3);
	SetIndustryIndexOfField(t, industry);
	SetFieldType(t, field_type);
}

/**
 * Make a snow tile.
 * @param t the tile to make snowy
 * @param density The density of snowiness.
 * @pre !IsSnowTile(t)
 */
inline void MakeSnow(Tile t, uint density = 0)
{
	assert(!IsSnowTile(t));
	t.GetTileBaseAs<TileType::Clear>().snow_presence = true;
	if (GetClearGround(t) == CLEAR_FIELDS) {
		SetClearGroundDensity(t, CLEAR_GRASS, density);
	} else {
		SetClearDensity(t, density);
	}
}

/**
 * Clear the snow from a tile and return it to its previous type.
 * @param t the tile to clear of snow
 * @pre IsSnowTile(t)
 */
inline void ClearSnow(Tile t)
{
	assert(IsSnowTile(t));
	t.GetTileBaseAs<TileType::Clear>().snow_presence = false;
	SetClearDensity(t, 3);
}

#endif /* CLEAR_MAP_H */
