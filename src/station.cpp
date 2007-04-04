/* $Id$ */

/** @file station.cpp */

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

	last_vehicle_type = VEH_INVALID;

	random_bits = 0; // Random() must be called when station is really built (DC_EXEC)
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

/** Called when new facility is built on the station. If it is the first facility
 * it initializes also 'xy' and 'random_bits' members */
void Station::AddFacility(byte new_facility_bit, TileIndex facil_xy)
{
	if (facilities == 0) {
		xy = facil_xy;
		random_bits = Random();
	}
	facilities |= new_facility_bit;
	owner = _current_player;
	build_date = _date;
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

/*static*/ Station *Station::AllocateRaw()
{
	Station *st = NULL;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (st = GetStation(0); st != NULL; st = (st->index + 1U < GetStationPoolSize()) ? GetStation(st->index + 1U) : NULL) {
		if (!st->IsValid()) {
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


/** Obtain the length of a platform
 * @pre tile must be a railway station tile
 * @param tile A tile that contains the platform in question
 * @return The length of the platform
 */
uint Station::GetPlatformLength(TileIndex tile) const
{
	TileIndex t;
	TileIndexDiff delta;
	uint len = 0;
	assert(TileBelongsToRailStation(tile));

	delta = (GetRailStationAxis(tile) == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));

	t = tile;
	do {
		t -= delta;
		len++;
	} while (IsCompatibleTrainStationTile(t, tile));

	t = tile;
	do {
		t += delta;
		len++;
	} while (IsCompatibleTrainStationTile(t, tile));

	return len - 1;
}

/** Determines the REMAINING length of a platform, starting at (and including)
 * the given tile.
 * @param tile the tile from which to start searching. Must be a railway station tile
 * @param dir The direction in which to search.
 * @return The platform length
 */
uint Station::GetPlatformLength(TileIndex tile, DiagDirection dir) const
{
	TileIndex start_tile = tile;
	uint length = 0;
	assert(IsRailwayStationTile(tile));
	assert(dir < DIAGDIR_END);

	do {
		length ++;
		tile += TileOffsByDiagDir(dir);
	} while (IsCompatibleTrainStationTile(tile, start_tile));

	return length;
}

/** Determines whether a station is a buoy only.
 * @todo Ditch this encoding of buoys
 */
bool Station::IsBuoy() const
{
	return (had_vehicle_of_type & HVOT_BUOY) != 0;
}

/** Determines whether a station exists
 * @todo replace 0 by INVALID_TILE
 */
bool Station::IsValid() const
{
	return xy != 0;
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

/**
 * Determines whether a given point (x, y) is within a certain distance of
 * the station rectangle.
 * @note x and y are in Tile coordinates
 * @param x X coordinate
 * @param y Y coordinate
 * @param distance The maxmium distance a point may have (L1 norm)
 * @return true if the point is within distance tiles of the station rectangle
 */
bool StationRect::PtInExtendedRect(int x, int y, int distance) const
{
	return (left - distance <= x && x <= right + distance && top - distance <= y && y <= bottom + distance);
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

	} else if (!PtInExtendedRect(x, y)) {
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
	assert(PtInExtendedRect(TileX(tile), TileY(tile)));
	assert(PtInExtendedRect(TileX(tile) + w - 1, TileY(tile) + h - 1));

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
RoadStop::RoadStop(TileIndex tile) :
	xy(tile),
	status(3), // stop is free
	num_vehicles(0),
	next(NULL)
{
	DEBUG(ms, cDebugCtorLevel,  "I+ at %d[0x%x]", tile, tile);
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
			if (v->type == VEH_ROAD && v->u.road.slot == this) ClearSlot(v);
		}
	}
	assert(num_vehicles == 0);

	DEBUG(ms, cDebugCtorLevel , "I- at %d[0x%x]", xy, xy);

	xy = INVALID_TILE;
}


/** Low-level function for allocating a RoadStop on the pool */
RoadStop *RoadStop::AllocateRaw()
{
	RoadStop *rs;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (rs = GetRoadStop(0); rs != NULL; rs = (rs->index + 1U < GetRoadStopPoolSize()) ? GetRoadStop(rs->index + 1U) : NULL) {
		if (!rs->IsValid()) {
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

/** Determines whether a RoadStop is a valid (i.e. existing) one */
bool RoadStop::IsValid() const
{
	return xy != INVALID_TILE;
}

/** Checks whether there is a free bay in this road stop */
bool RoadStop::HasFreeBay() const
{
	return GB(status, 0, MAX_BAY_COUNT) != 0;
}

/** Checks whether the given bay is free in this road stop */
bool RoadStop::IsFreeBay(uint nr) const
{
	assert(nr < MAX_BAY_COUNT);
	return HASBIT(status, nr);
}

/**
 * Allocates a bay
 * @return the allocated bay number
 * @pre this->HasFreeBay()
 */
uint RoadStop::AllocateBay()
{
	assert(HasFreeBay());

	/* Find the first free bay. If the bit is set, the bay is free. */
	uint bay_nr = 0;
	while (!HASBIT(status, bay_nr)) bay_nr++;

	CLRBIT(status, bay_nr);
	return bay_nr;
}

/**
 * Allocates a bay in a drive-through road stop
 * @param nr the number of the bay to allocate
 */
void RoadStop::AllocateDriveThroughBay(uint nr)
{
	assert(nr < MAX_BAY_COUNT);
	CLRBIT(status, nr);
}

/**
 * Frees the given bay
 * @param nr the number of the bay to free
 */
void RoadStop::FreeBay(uint nr)
{
	assert(nr < MAX_BAY_COUNT);
	SETBIT(status, nr);
}


/** Checks whether the entrance of the road stop is occupied by a vehicle */
bool RoadStop::IsEntranceBusy() const
{
	return HASBIT(status, 7);
}

/** Makes an entrance occupied or free */
void RoadStop::SetEntranceBusy(bool busy)
{
	SB(status, 7, 1, busy);
}
