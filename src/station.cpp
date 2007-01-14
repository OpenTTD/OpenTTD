/* $Id$ */

/** @file station_cmd.c */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "debug.h"
#include "functions.h"
#include "station_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "station.h"
#include "gfx.h"
#include "window.h"
#include "viewport.h"
#include "command.h"
#include "town.h"
#include "vehicle.h"
#include "news.h"
#include "saveload.h"
#include "economy.h"
#include "player.h"
#include "airport.h"
#include "sprite.h"
#include "depot.h"
#include "train.h"
#include "water_map.h"
#include "industry_map.h"
#include "newgrf_callbacks.h"
#include "newgrf_station.h"
#include "yapf/yapf.h"
#include "date.h"
#include "helpers.hpp"

void StationRect_Init(Station *st); // don't worry, will be removed soon

Station::Station(TileIndex tile)
{
	DEBUG(station, cDebugCtorLevel, "I+%3d", index);

	xy = tile;
	airport_tile = dock_tile = train_tile = 0;
	bus_stops = truck_stops = NULL;
	had_vehicle_of_type = 0;
	time_since_load = 255;
	time_since_unload = 255;
	delete_ctr = 0;
	facilities = 0;

	last_vehicle_type = VEH_Invalid;

	random_bits = Random();
	waiting_triggers = 0;

	StationRect_Init(this);
}

/**
* Clean up a station by clearing vehicle orders and invalidating windows.
* Aircraft-Hangar orders need special treatment here, as the hangars are
* actually part of a station (tiletype is STATION), but the order type
* is OT_GOTO_DEPOT.
* @param st Station to be deleted
*/
Station::~Station()
{
	DEBUG(station, cDebugCtorLevel, "I-%3d", index);

	DeleteName(string_id);
	MarkDirty();
	RebuildStationLists();
	InvalidateWindowClasses(WC_STATION_LIST);

	DeleteWindowById(WC_STATION_VIEW, index);

	/* Now delete all orders that go to the station */
	RemoveOrderFromAllVehicles(OT_GOTO_STATION, index);

	//Subsidies need removal as well
	DeleteSubsidyWithStation(index);

	free(speclist);
	xy = 0;
}

void* Station::operator new(size_t size)
{
	Station *st = AllocateRaw();
	return st;
}

void* Station::operator new(size_t size, int st_idx)
{
	if (!AddBlockIfNeeded(&_Station_pool, st_idx))
		error("Stations: failed loading savegame: too many stations");

	Station *st = GetStation(st_idx);
	return st;
}

void Station::operator delete(void *p)
{
}

void Station::operator delete(void *p, int st_idx)
{
}

void Station::MarkDirty() const
{
	if (sign.width_1 != 0) {
		InvalidateWindowWidget(WC_STATION_VIEW, index, 1);

		MarkAllViewportsDirty(
			sign.left - 6,
			sign.top,
			sign.left + (sign.width_1 << 2) + 12,
			sign.top + 48);
	}
}

void Station::MarkTilesDirty() const
{
	TileIndex tile = train_tile;
	int w, h;

	// XXX No station is recorded as 0, not INVALID_TILE...
	if (tile == 0) return;

	for (h = 0; h < trainst_h; h++) {
		for (w = 0; w < trainst_w; w++) {
			if (TileBelongsToRailStation(tile)) {
				MarkTileDirtyByTile(tile);
			}
			tile += TileDiffXY(1, 0);
		}
		tile += TileDiffXY(-w, 1);
	}
}

bool Station::TileBelongsToRailStation(TileIndex tile) const
{
	return IsTileType(tile, MP_STATION) && GetStationIndex(tile) == index && IsRailwayStation(tile);
}

/*static*/ Station *Station::AllocateRaw(void)
{
	Station *st = NULL;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (st = GetStation(0); st != NULL; st = (st->index + 1U < GetStationPoolSize()) ? GetStation(st->index + 1U) : NULL) {
		if (!IsValidStation(st)) {
			StationID index = st->index;

			memset(st, 0, sizeof(Station));
			st->index = index;
			return st;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_Station_pool)) return AllocateRaw();

	_error_message = STR_3008_TOO_MANY_STATIONS_LOADING;
	return NULL;
}
