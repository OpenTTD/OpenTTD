/* $Id$ */

#ifndef DEPOT_H
#define DEPOT_H

/** @file depot.h Header files for depots (not hangars)
  * @see depot.c */

#include "pool.h"
#include "tile.h"
#include "variables.h"

struct Depot {
	TileIndex xy;
	uint16 town_index;
	uint16 index;
};

extern MemoryPool _depot_pool;

/**
 * Get the pointer to the depot with index 'index'
 */
static inline Depot *GetDepot(uint index)
{
	return (Depot*)GetItemFromPool(&_depot_pool, index);
}

/**
 * Get the current size of the DepotPool
 */
static inline uint16 GetDepotPoolSize(void)
{
	return _depot_pool.total_items;
}

static inline bool IsDepotIndex(uint index)
{
	return index < GetDepotPoolSize();
}

#define FOR_ALL_DEPOTS_FROM(d, start) for (d = GetDepot(start); d != NULL; d = (d->index + 1 < GetDepotPoolSize()) ? GetDepot(d->index + 1) : NULL)
#define FOR_ALL_DEPOTS(d) FOR_ALL_DEPOTS_FROM(d, 0)

#define MIN_SERVINT_PERCENT  5
#define MAX_SERVINT_PERCENT 90
#define MIN_SERVINT_DAYS    30
#define MAX_SERVINT_DAYS   800

/** Get the service interval domain.
 * Get the new proposed service interval for the vehicle is indeed, clamped
 * within the given bounds. @see MIN_SERVINT_PERCENT ,etc.
 * @param index proposed service interval
 */
static inline uint16 GetServiceIntervalClamped(uint index)
{
	return (_patches.servint_ispercent) ? clamp(index, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT) : clamp(index, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS);
}

VARDEF TileIndex _last_built_train_depot_tile;
VARDEF TileIndex _last_built_road_depot_tile;
VARDEF TileIndex _last_built_aircraft_depot_tile;
VARDEF TileIndex _last_built_ship_depot_tile;

/**
 * Check if a depot really exists.
 */
static inline bool IsValidDepot(const Depot* depot)
{
	return depot->xy != 0; /* XXX: Replace by INVALID_TILE someday */
}

/**
 * Check if a tile is a depot of the given type.
 */
static inline bool IsTileDepotType(TileIndex tile, TransportType type)
{
	switch(type)
	{
		case TRANSPORT_RAIL:
			return IsTileType(tile, MP_RAILWAY) && (_m[tile].m5 & 0xFC) == 0xC0;

		case TRANSPORT_ROAD:
			return IsTileType(tile, MP_STREET) && (_m[tile].m5 & 0xF0) == 0x20;

		case TRANSPORT_WATER:
			return IsTileType(tile, MP_WATER) && (_m[tile].m5 & ~3) == 0x80;

		default:
			assert(0);
			return false;
	}
}

/**
 * Returns the direction the exit of the depot on the given tile is facing.
 */
static inline DiagDirection GetDepotDirection(TileIndex tile, TransportType type)
{
	assert(IsTileDepotType(tile, type));

	switch (type)
	{
		case TRANSPORT_RAIL:
		case TRANSPORT_ROAD:
			/* Rail and road store a diagonal direction in bits 0 and 1 */
			return (DiagDirection)GB(_m[tile].m5, 0, 2);
		case TRANSPORT_WATER:
			/* Water is stubborn, it stores the directions in a different order. */
			switch (GB(_m[tile].m5, 0, 2)) {
				case 0: return DIAGDIR_NE;
				case 1: return DIAGDIR_SW;
				case 2: return DIAGDIR_NW;
				case 3: return DIAGDIR_SE;
			}
		default:
			return INVALID_DIAGDIR; /* Not reached */
	}
}

/**
	Find out if the slope of the tile is suitable to build a depot of given direction
	@param direction The direction in which the depot's exit points. Starts with 0 as NE and goes Clockwise
	@param tileh The slope of the tile in question
	@return true if the construction is possible


    This is checked by the ugly 0x4C >> direction magic, which does the following:
      0x4C is 0100 1100 and tileh has only bits 0..3 set (steep tiles are ruled out)
      So: for direction (only the significant bits are shown)<p>
      00 (exit towards NE) we need either bit 2 or 3 set in tileh: 0x4C >> 0 = 1100<p>
      01 (exit towards SE) we need either bit 1 or 2 set in tileh: 0x4C >> 1 = 0110<p>
      02 (exit towards SW) we need either bit 0 or 1 set in tileh: 0x4C >> 2 = 0011<p>
      03 (exit towards NW) we need either bit 0 or 4 set in tileh: 0x4C >> 3 = 1001<p>
      So ((0x4C >> p2) & tileh) determines whether the depot can be built on the current tileh
*/
static inline bool CanBuildDepotByTileh(uint32 direction, uint tileh)
{
	return (0x4C >> direction) & tileh;
}


Depot *GetDepotByTile(TileIndex tile);
void InitializeDepot(void);
Depot *AllocateDepot(void);
void DoDeleteDepot(TileIndex tile);

#endif /* DEPOT_H */
