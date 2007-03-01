/* $Id$ */

/** @file depot.h Header files for depots (not hangars) */

#ifndef DEPOT_H
#define DEPOT_H

#include "direction.h"
#include "oldpool.h"
#include "tile.h"
#include "variables.h"
#include "road_map.h"
#include "rail_map.h"
#include "water_map.h"

struct Depot {
	TileIndex xy;
	TownID town_index;
	DepotID index;
};

DECLARE_OLD_POOL(Depot, Depot, 3, 8000)

/**
 * Check if a depot really exists.
 */
static inline bool IsValidDepot(const Depot *depot)
{
	return depot != NULL && depot->xy != 0;
}

static inline bool IsValidDepotID(uint index)
{
	return index < GetDepotPoolSize() && IsValidDepot(GetDepot(index));
}

void DestroyDepot(Depot *depot);

static inline void DeleteDepot(Depot *depot)
{
	DestroyDepot(depot);
	depot->xy = 0;
}

void ShowDepotWindow(TileIndex tile, byte type);

#define FOR_ALL_DEPOTS_FROM(d, start) for (d = GetDepot(start); d != NULL; d = (d->index + 1U < GetDepotPoolSize()) ? GetDepot(d->index + 1U) : NULL) if (IsValidDepot(d))
#define FOR_ALL_DEPOTS(d) FOR_ALL_DEPOTS_FROM(d, 0)

#define MIN_SERVINT_PERCENT  5
#define MAX_SERVINT_PERCENT 90
#define MIN_SERVINT_DAYS    30
#define MAX_SERVINT_DAYS   800

/**
 * Get the service interval domain.
 * Get the new proposed service interval for the vehicle is indeed, clamped
 * within the given bounds. @see MIN_SERVINT_PERCENT ,etc.
 * @param index proposed service interval
 * @return service interval
 */
static inline Date GetServiceIntervalClamped(uint index)
{
	return (_patches.servint_ispercent) ? clamp(index, MIN_SERVINT_PERCENT, MAX_SERVINT_PERCENT) : clamp(index, MIN_SERVINT_DAYS, MAX_SERVINT_DAYS);
}

/**
 * Check if a tile is a depot of the given type.
 */
static inline bool IsTileDepotType(TileIndex tile, TransportType type)
{
	switch (type) {
		case TRANSPORT_RAIL:
			return IsTileType(tile, MP_RAILWAY) && GetRailTileType(tile)  == RAIL_TILE_DEPOT;

		case TRANSPORT_ROAD:
			return IsTileType(tile, MP_STREET)  && GetRoadTileType(tile)  == ROAD_TILE_DEPOT;

		case TRANSPORT_WATER:
			return IsTileType(tile, MP_WATER)   && GetWaterTileType(tile) == WATER_TILE_DEPOT;

		default:
			NOT_REACHED();
			return false;
	}
}


/**
 * Find out if the slope of the tile is suitable to build a depot of given direction
 * @param direction The direction in which the depot's exit points
 * @param tileh The slope of the tile in question
 * @return true if the construction is possible

 * This is checked by the ugly 0x4C >> direction magic, which does the following:
 * 0x4C is 0100 1100 and tileh has only bits 0..3 set (steep tiles are ruled out)
 * So: for direction (only the significant bits are shown)<p>
 * 00 (exit towards NE) we need either bit 2 or 3 set in tileh: 0x4C >> 0 = 1100<p>
 * 01 (exit towards SE) we need either bit 1 or 2 set in tileh: 0x4C >> 1 = 0110<p>
 * 02 (exit towards SW) we need either bit 0 or 1 set in tileh: 0x4C >> 2 = 0011<p>
 * 03 (exit towards NW) we need either bit 0 or 4 set in tileh: 0x4C >> 3 = 1001<p>
 * So ((0x4C >> direction) & tileh) determines whether the depot can be built on the current tileh
 */
static inline bool CanBuildDepotByTileh(DiagDirection direction, Slope tileh)
{
	return ((0x4C >> direction) & tileh) != 0;
}

Depot *GetDepotByTile(TileIndex tile);
void InitializeDepots(void);
Depot *AllocateDepot(void);

void DeleteDepotHighlightOfVehicle(const Vehicle *v);

#endif /* DEPOT_H */
