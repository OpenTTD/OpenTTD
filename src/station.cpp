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

	/* Subsidies need removal as well */
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

	/* XXX No station is recorded as 0, not INVALID_TILE... */
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



/************************************************************************/
/*                     StationRect implementation                       */
/************************************************************************/

StationRect::StationRect()
{
	MakeEmpty();
}

void StationRect::MakeEmpty()
{
	left = top = right = bottom = 0;
}

bool StationRect::PtInRectXY(int x, int y) const
{
	return (left <= x && x <= right && top <= y && y <= bottom);
}

bool StationRect::IsEmpty() const
{
	return (left == 0 || left > right || top > bottom);
}

bool StationRect::BeforeAddTile(TileIndex tile, StationRectMode mode)
{
	int x = TileX(tile);
	int y = TileY(tile);
	if (IsEmpty()) {
		/* we are adding the first station tile */
		left = right = x;
		top = bottom = y;

	} else if (!PtInRectXY(x, y)) {
		/* current rect is not empty and new point is outside this rect */
		/* make new spread-out rectangle */
		Rect new_rect = {min(x, left), min(y, top), max(x, right), max(y, bottom)};

		/* check new rect dimensions against preset max */
		int w = new_rect.right - new_rect.left + 1;
		int h = new_rect.bottom - new_rect.top + 1;
		if (mode != ADD_FORCE && (w > _patches.station_spread || h > _patches.station_spread)) {
			assert(mode != ADD_TRY);
			_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
			return false;
		}

		/* spread-out ok, return true */
		if (mode != ADD_TEST) {
			/* we should update the station rect */
			*this = new_rect;
		}
	} else {
		; // new point is inside the rect, we don't need to do anything
	}
	return true;
}

bool StationRect::BeforeAddRect(TileIndex tile, int w, int h, StationRectMode mode)
{
	return BeforeAddTile(tile, mode) && BeforeAddTile(TILE_ADDXY(tile, w - 1, h - 1), mode);
}

/*static*/ bool StationRect::ScanForStationTiles(StationID st_id, int left_a, int top_a, int right_a, int bottom_a)
{
	TileIndex top_left = TileXY(left_a, top_a);
	int width = right_a - left_a + 1;
	int height = bottom_a - top_a + 1;

	BEGIN_TILE_LOOP(tile, width, height, top_left)
		if (IsTileType(tile, MP_STATION) && GetStationIndex(tile) == st_id) return true;
	END_TILE_LOOP(tile, width, height, top_left);

	return false;
}

bool StationRect::AfterRemoveTile(Station *st, TileIndex tile)
{
	int x = TileX(tile);
	int y = TileY(tile);

	/* look if removed tile was on the bounding rect edge
	 * and try to reduce the rect by this edge
	 * do it until we have empty rect or nothing to do */
	for (;;) {
		/* check if removed tile is on rect edge */
		bool left_edge = (x == left);
		bool right_edge = (x == right);
		bool top_edge = (y == top);
		bool bottom_edge = (y == bottom);

		/* can we reduce the rect in either direction? */
		bool reduce_x = ((left_edge || right_edge) && !ScanForStationTiles(st->index, x, top, x, bottom));
		bool reduce_y = ((top_edge || bottom_edge) && !ScanForStationTiles(st->index, left, y, right, y));
		if (!(reduce_x || reduce_y)) break; // nothing to do (can't reduce)

		if (reduce_x) {
			/* reduce horizontally */
			if (left_edge) {
				/* move left edge right */
				left = x = x + 1;
			} else {
				/* move right edge left */
				right = x = x - 1;
			}
		}
		if (reduce_y) {
			/* reduce vertically */
			if (top_edge) {
				/* move top edge down */
				top = y = y + 1;
			} else {
				/* move bottom edge up */
				bottom = y = y - 1;
			}
		}

		if (left > right || top > bottom) {
			/* can't continue, if the remaining rectangle is empty */
			MakeEmpty();
			return true; // empty remaining rect
		}
	}
	return false; // non-empty remaining rect
}

bool StationRect::AfterRemoveRect(Station *st, TileIndex tile, int w, int h)
{
	assert(PtInRectXY(TileX(tile), TileY(tile)));
	assert(PtInRectXY(TileX(tile) + w - 1, TileY(tile) + h - 1));

	bool empty = AfterRemoveTile(st, tile);
	if (w != 1 || h != 1) empty = empty || AfterRemoveTile(st, TILE_ADDXY(tile, w - 1, h - 1));
	return empty;
}

StationRect& StationRect::operator = (Rect src)
{
	left = src.left;
	top = src.top;
	right = src.right;
	bottom = src.bottom;
	return *this;
}


/************************************************************************/
/*                     RoadStop implementation                          */
/************************************************************************/

/** Allocates a new RoadStop onto the pool, or recycles an unsed one
 *  @return a pointer to the new roadstop
 */
void *RoadStop::operator new(size_t size)
{
	RoadStop *rs = AllocateRaw();
	return rs;
}

/** Gets a RoadStop with a given index and allocates it when needed
  * @return a pointer to the roadstop
  */
void *RoadStop::operator new(size_t size, int index)
{
	if (!AddBlockIfNeeded(&_RoadStop_pool, index)) {
		error("RoadStops: failed loading savegame: too many RoadStops");
	}

	RoadStop *rs = GetRoadStop(index);
	return rs;
}

void RoadStop::operator delete(void *p)
{
}

void RoadStop::operator delete(void *p, int index)
{
}

/** Initializes a RoadStop */
RoadStop::RoadStop(TileIndex tile, StationID index) :
	xy(tile),
	used(true),
	status(3), // stop is free
	num_vehicles(0),
	station(index),
	next(NULL),
	prev(NULL)
{
	DEBUG(ms, cDebugCtorLevel,  "I+%3d at %d[0x%x]", index, tile, tile);
}

/** De-Initializes a RoadStops. This includes clearing all slots that vehicles might
  * have and unlinks it from the linked list of road stops at the given station
  */
RoadStop::~RoadStop()
{
	/* Clear the slot assignment of all vehicles heading for this road stop */
	if (num_vehicles != 0) {
		Vehicle *v;

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Road && v->u.road.slot == this) ClearSlot(v);
		}
	}
	assert(num_vehicles == 0);

	if (prev != NULL) prev->next = next;
	if (next != NULL) next->prev = prev;

	used = false;
	DEBUG(ms, cDebugCtorLevel , "I-%3d at %d[0x%x]", station, xy, xy);

	xy = INVALID_TILE;
	station = INVALID_STATION;
}


/** Low-level function for allocating a RoadStop on the pool */
RoadStop *RoadStop::AllocateRaw( void )
{
	RoadStop *rs;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (rs = GetRoadStop(0); rs != NULL; rs = (rs->index + 1U < GetRoadStopPoolSize()) ? GetRoadStop(rs->index + 1U) : NULL) {
		if (!IsValidRoadStop(rs)) {
			RoadStopID index = rs->index;

			memset(rs, 0, sizeof(*rs));
			rs->index = index;

			return rs;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_RoadStop_pool)) return AllocateRaw();

	return NULL;
}
