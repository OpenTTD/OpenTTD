/* $Id$ */

/** @file town_map.h Accessors for towns */

#ifndef TOWN_MAP_H
#define TOWN_MAP_H

#include "town.h"
#include "date_func.h"
#include "tile_map.h"
#include "functions.h"

/**
 * Get the index of which town this house/street is attached to.
 * @param t the tile
 * @pre IsTileType(t, MP_HOUSE) or IsTileType(t, MP_ROAD)
 * @return TownID
 */
static inline TownID GetTownIndex(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_ROAD)); // XXX incomplete
	return _m[t].m2;
}

/**
 * Set the town index for a road or house tile.
 * @param t the tile
 * @pre IsTileType(t, MP_HOUSE) or IsTileType(t, MP_ROAD)
 * @param index the index of the town
 * @pre IsTileType(t, MP_ROAD) || IsTileType(t, MP_HOUSE)
 */
static inline void SetTownIndex(TileIndex t, TownID index)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_ROAD));
	_m[t].m2 = index;
}

/**
 * Gets the town associated with the house or road tile
 * @param t the tile to get the town of
 * @return the town
 */
static inline Town *GetTownByTile(TileIndex t)
{
	return GetTown(GetTownIndex(t));
}

/**
 * Get the type of this house, which is an index into the house spec array
 * Since m4 is only a byte and we want to support 512 houses, we use the bit 6
 * of m3 as an additional bit to house type.
 * @param t the tile
 * @pre IsTileType(t, MP_HOUSE)
 * @return house type
 */
static inline HouseID GetHouseType(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return _m[t].m4 | (GB(_m[t].m3, 6, 1) << 8);
}

/**
 * Set the house type.
 * @param t the tile
 * @param house_id the new house type
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseType(TileIndex t, HouseID house_id)
{
	assert(IsTileType(t, MP_HOUSE));
	_m[t].m4 = GB(house_id, 0, 8);
	SB(_m[t].m3, 6, 1, GB(house_id, 8, 1));
}

/**
 * Check if the lift of this animated house has a destination
 * @param t the tile
 * @return has destination
 */
static inline bool LiftHasDestination(TileIndex t)
{
	return HasBit(_me[t].m7, 0);
}

/**
 * Set the new destination of the lift for this animated house, and activate
 * the LiftHasDestination bit.
 * @param t the tile
 * @param dest new destination
 */
static inline void SetLiftDestination(TileIndex t, byte dest)
{
	SetBit(_me[t].m7, 0);
	SB(_me[t].m7, 1, 3, dest);
}

/**
 * Get the current destination for this lift
 * @param t the tile
 * @return destination
 */
static inline byte GetLiftDestination(TileIndex t)
{
	return GB(_me[t].m7, 1, 3);
}

/**
 * Stop the lift of this animated house from moving.
 * Clears the first 4 bits of m7 at once, clearing the LiftHasDestination bit
 * and the destination.
 * @param t the tile
 */
static inline void HaltLift(TileIndex t)
{
	SB(_me[t].m7, 0, 4, 0);
	DeleteAnimatedTile(t);
}

/**
 * Get the position of the lift on this animated house
 * @param t the tile
 * @return position, from 0 to 36
 */
static inline byte GetLiftPosition(TileIndex t)
{
	return GB(_m[t].m6, 2, 6);
}

/**
 * Set the position of the lift on this animated house
 * @param t the tile
 * @param pos position, from 0 to 36
 */
static inline void SetLiftPosition(TileIndex t, byte pos)
{
	SB(_m[t].m6, 2, 6, pos);
}

/**
 * Get the current animation frame for this house
 * @param t the tile
 * @pre IsTileType(t, MP_HOUSE)
 * @return frame number
 */
static inline byte GetHouseAnimationFrame(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return GB(_m[t].m6, 3, 5);
}

/**
 * Set a new animation frame for this house
 * @param t the tile
 * @param frame the new frame number
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseAnimationFrame(TileIndex t, byte frame)
{
	assert(IsTileType(t, MP_HOUSE));
	SB(_m[t].m6, 3, 5, frame);
}

/**
 * Get the completion of this house
 * @param t the tile
 * @return true if it is, false if it is not
 */
static inline bool IsHouseCompleted(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return HasBit(_m[t].m3, 7);
}

/**
 * Mark this house as been completed
 * @param t the tile
 * @param status
 */
static inline void SetHouseCompleted(TileIndex t, bool status)
{
	assert(IsTileType(t, MP_HOUSE));
	SB(_m[t].m3, 7, 1, !!status);
}

/**
 * Make the tile a house.
 * @param t tile index
 * @param tid Town index
 * @param counter of construction step
 * @param stage of construction (used for drawing)
 * @param type of house.  Index into house specs array
 * @param random_bits required for newgrf houses
 * @pre IsTileType(t, MP_CLEAR)
 */
static inline void MakeHouseTile(TileIndex t, TownID tid, byte counter, byte stage, HouseID type, byte random_bits)
{
	assert(IsTileType(t, MP_CLEAR));

	SetTileType(t, MP_HOUSE);
	_m[t].m1 = random_bits;
	_m[t].m2 = tid;
	_m[t].m3 = 0;
	SetHouseType(t, type);
	SetHouseCompleted(t, stage == TOWN_HOUSE_COMPLETED);
	_m[t].m5 = IsHouseCompleted(t) ? 0 : (stage << 3 | counter);
	SetHouseAnimationFrame(t, 0);
	_me[t].m7 = GetHouseSpecs(type)->processing_time;

	if (GetHouseSpecs(type)->building_flags & BUILDING_IS_ANIMATED) AddAnimatedTile(t);
	MarkTileDirtyByTile(t);
}

/**
 * House Construction Scheme.
 *  Construction counter, for buildings under construction. Incremented on every
 *  periodic tile processing.
 *  On wraparound, the stage of building in is increased.
 *  GetHouseBuildingStage is taking care of the real stages,
 *  (as the sprite for the next phase of house building)
 *  (Get|Inc)HouseConstructionTick is simply a tick counter between the
 *  different stages
 */

/**
 * Gets the building stage of a house
 * Since the stage is used for determining what sprite to use,
 * if the house is complete (and that stage no longuer is available),
 * fool the system by returning the TOWN_HOUSE_COMPLETE (3),
 * thus showing a beautiful complete house.
 * @param t the tile of the house to get the building stage of
 * @pre IsTileType(t, MP_HOUSE)
 * @return the building stage of the house
 */
static inline byte GetHouseBuildingStage(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return IsHouseCompleted(t) ? (byte)TOWN_HOUSE_COMPLETED : GB(_m[t].m5, 3, 2);
}

/**
 * Gets the construction stage of a house
 * @param t the tile of the house to get the construction stage of
 * @pre IsTileType(t, MP_HOUSE)
 * @return the construction stage of the house
 */
static inline byte GetHouseConstructionTick(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return IsHouseCompleted(t) ? 0 : GB(_m[t].m5, 0, 3);
}

/**
 * Sets the increment stage of a house
 * It is working with the whole counter + stage 5 bits, making it
 * easier to work:  the wraparound is automatic.
 * @param t the tile of the house to increment the construction stage of
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void IncHouseConstructionTick(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	AB(_m[t].m5, 0, 5, 1);

	if (GB(_m[t].m5, 3, 2) == TOWN_HOUSE_COMPLETED) {
		/* House is now completed.
		 * Store the year of construction as well, for newgrf house purpose */
		SetHouseCompleted(t, true);
		_m[t].m5 = Clamp(_cur_year - ORIGINAL_BASE_YEAR, 0, 0xFF);
	}
}

/**
 * Get the year that this house was constructed (between 1920 and 2175).
 * @param t the tile of this house
 * @pre IsTileType(t, MP_HOUSE)
 * @return year
 */
static inline Year GetHouseConstructionYear(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return IsHouseCompleted(t) ? _m[t].m5 + ORIGINAL_BASE_YEAR : 0;
}

/**
 * Set the random bits for this house.
 * This is required for newgrf house
 * @param t      the tile of this house
 * @param random the new random bits
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseRandomBits(TileIndex t, byte random)
{
	assert(IsTileType(t, MP_HOUSE));
	_m[t].m1 = random;
}

/**
 * Get the random bits for this house.
 * This is required for newgrf house
 * @param t the tile of this house
 * @pre IsTileType(t, MP_HOUSE)
 * @return random bits
 */
static inline byte GetHouseRandomBits(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return _m[t].m1;
}

/**
 * Set the activated triggers bits for this house.
 * This is required for newgrf house
 * @param t        the tile of this house
 * @param triggers the activated triggers
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseTriggers(TileIndex t, byte triggers)
{
	assert(IsTileType(t, MP_HOUSE));
	SB(_m[t].m3, 0, 5, triggers);
}

/**
 * Get the already activated triggers bits for this house.
 * This is required for newgrf house
 * @param t the tile of this house
 * @pre IsTileType(t, MP_HOUSE)
 * @return triggers
 */
static inline byte GetHouseTriggers(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return GB(_m[t].m3, 0, 5);
}

/**
 * Get the amount of time remaining before the tile loop processes this tile.
 * @param t the house tile
 * @pre IsTileType(t, MP_HOUSE)
 * @return time remaining
 */
static inline byte GetHouseProcessingTime(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return _me[t].m7;
}

/**
 * Set the amount of time remaining before the tile loop processes this tile.
 * @param t the house tile
 * @param time the time to be set
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseProcessingTime(TileIndex t, byte time)
{
	assert(IsTileType(t, MP_HOUSE));
	_me[t].m7 = time;
}

/**
 * Decrease the amount of time remaining before the tile loop processes this tile.
 * @param t the house tile
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void DecHouseProcessingTime(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	_me[t].m7--;
}

#endif /* TOWN_MAP_H */
