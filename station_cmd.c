/* $Id$ */

/** @file station_cmd.c
  */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
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
#include "pbs.h"
#include "train.h"
#include "ai/ai_event.h"

enum {
	/* Max stations: 64000 (64 * 1000) */
	STATION_POOL_BLOCK_SIZE_BITS = 6,       /* In bits, so (1 << 6) == 64 */
	STATION_POOL_MAX_BLOCKS      = 1000,

	/* Max roadstops: 64000 (32 * 2000) */
	ROADSTOP_POOL_BLOCK_SIZE_BITS = 5,       /* In bits, so (1 << 5) == 32 */
	ROADSTOP_POOL_MAX_BLOCKS      = 2000,
};

/**
 * Called if a new block is added to the station-pool
 */
static void StationPoolNewBlock(uint start_item)
{
	Station *st;

	FOR_ALL_STATIONS_FROM(st, start_item) st->index = start_item++;
}

/**
 * Called if a new block is added to the roadstop-pool
 */
static void RoadStopPoolNewBlock(uint start_item)
{
	RoadStop *rs;

	FOR_ALL_ROADSTOPS_FROM(rs, start_item) rs->index = start_item++;
}

/* Initialize the station-pool and roadstop-pool */
MemoryPool _station_pool = { "Stations", STATION_POOL_MAX_BLOCKS, STATION_POOL_BLOCK_SIZE_BITS, sizeof(Station), &StationPoolNewBlock, 0, 0, NULL };
MemoryPool _roadstop_pool = { "RoadStop", ROADSTOP_POOL_MAX_BLOCKS, ROADSTOP_POOL_BLOCK_SIZE_BITS, sizeof(RoadStop), &RoadStopPoolNewBlock, 0, 0, NULL };


// FIXME -- need to be embedded into Airport variable. Is dynamically
// deducteable from graphics-tile array, so will not be needed
const byte _airport_size_x[] = {4, 6, 1, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
const byte _airport_size_y[] = {3, 6, 1, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

void ShowAircraftDepotWindow(TileIndex tile);
extern void UpdateAirplanesOnNewStation(Station *st);

static void MarkStationDirty(const Station* st)
{
	if (st->sign.width_1 != 0) {
		InvalidateWindowWidget(WC_STATION_VIEW, st->index, 1);

		MarkAllViewportsDirty(
			st->sign.left - 6,
			st->sign.top,
			st->sign.left + (st->sign.width_1 << 2) + 12,
			st->sign.top + 48);
	}
}

static void InitializeRoadStop(RoadStop *road_stop, RoadStop *previous, TileIndex tile, StationID index)
{
	road_stop->xy = tile;
	road_stop->used = true;
	road_stop->status = 3; //stop is free
	road_stop->slot[0] = road_stop->slot[1] = INVALID_SLOT;
	road_stop->next = NULL;
	road_stop->prev = previous;
	road_stop->station = index;
}

RoadStop* GetPrimaryRoadStop(const Station* st, RoadStopType type)
{
	switch (type) {
		case RS_BUS:   return st->bus_stops;
		case RS_TRUCK: return st->truck_stops;
		default: NOT_REACHED();
	}

	return NULL;
}

RoadStop* GetRoadStopByTile(TileIndex tile, RoadStopType type)
{
	const Station* st = GetStation(_m[tile].m2);
	RoadStop* rs;

	for (rs = GetPrimaryRoadStop(st, type); rs->xy != tile; rs = rs->next) {
		assert(rs->next != NULL);
	}

	return rs;
}

uint GetNumRoadStops(const Station *st, RoadStopType type)
{
	uint num = 0;
	const RoadStop *rs;

	assert(st != NULL);
	for (rs = GetPrimaryRoadStop(st, type); rs != NULL; rs = rs->next) num++;

	return num;
}

RoadStop *AllocateRoadStop(void)
{
	RoadStop *rs;

	FOR_ALL_ROADSTOPS(rs) {
		if (!rs->used) {
			uint index = rs->index;

			memset(rs, 0, sizeof(*rs));
			rs->index = index;

			return rs;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_roadstop_pool)) return AllocateRoadStop();

	return NULL;
}

/* Calculate the radius of the station. Basicly it is the biggest
    radius that is available within the station */
static uint FindCatchmentRadius(const Station* st)
{
	uint ret = 0;

	if (st->bus_stops != NULL)   ret = max(ret, CA_BUS);
	if (st->truck_stops != NULL) ret = max(ret, CA_TRUCK);
	if (st->train_tile) ret = max(ret, CA_TRAIN);
	if (st->dock_tile)  ret = max(ret, CA_DOCK);

	if (st->airport_tile) {
		switch (st->airport_type) {
			case AT_OILRIG:        ret = max(ret, CA_AIR_OILPAD);   break;
			case AT_SMALL:         ret = max(ret, CA_AIR_SMALL);    break;
			case AT_HELIPORT:      ret = max(ret, CA_AIR_HELIPORT); break;
			case AT_LARGE:         ret = max(ret, CA_AIR_LARGE);    break;
			case AT_METROPOLITAN:  ret = max(ret, CA_AIR_METRO);    break;
			case AT_INTERNATIONAL: ret = max(ret, CA_AIR_INTER);    break;
		}
	}

	return ret;
}

#define CHECK_STATIONS_ERR ((Station*)-1)

static Station* GetStationAround(TileIndex tile, int w, int h, StationID closest_station)
{
	// check around to see if there's any stations there
	BEGIN_TILE_LOOP(tile_cur, w + 2, h + 2, tile - TileDiffXY(1, 1))
		if (IsTileType(tile_cur, MP_STATION)) {
			StationID t = _m[tile_cur].m2;
			{
				Station *st = GetStation(t);
				// you cannot take control of an oilrig!!
				if (st->airport_type == AT_OILRIG && st->facilities == (FACIL_AIRPORT|FACIL_DOCK))
					continue;
			}

			if (closest_station == INVALID_STATION) {
				closest_station = t;
			} else if (closest_station != t) {
				_error_message = STR_3006_ADJOINS_MORE_THAN_ONE_EXISTING;
				return CHECK_STATIONS_ERR;
			}
		}
	END_TILE_LOOP(tile_cur, w + 2, h + 2, tile - TileDiffXY(1, 1))
	return (closest_station == INVALID_STATION) ? NULL : GetStation(closest_station);
}

TileIndex GetStationTileForVehicle(const Vehicle *v, const Station *st)
{
	switch (v->type) {
		case VEH_Train: 		return st->train_tile;
		case VEH_Aircraft:	return st->airport_tile;
		case VEH_Ship:			return st->dock_tile;
		case VEH_Road:
			if (v->cargo_type == CT_PASSENGERS) {
				return (st->bus_stops != NULL) ? st->bus_stops->xy : 0;
			} else {
				return (st->truck_stops != NULL) ? st->truck_stops->xy : 0;
			}
		default:
			assert(false);
			return 0;
	}
}

static bool CheckStationSpreadOut(Station *st, TileIndex tile, int w, int h)
{
	StationID station_index = st->index;
	uint i;
	uint x1 = TileX(tile);
	uint y1 = TileY(tile);
	uint x2 = x1 + w - 1;
	uint y2 = y1 + h - 1;
	uint t;

	for (i = 0; i != MapSize(); i++) {
		if (IsTileType(i, MP_STATION) && _m[i].m2 == station_index) {
			t = TileX(i);
			if (t < x1) x1 = t;
			if (t > x2) x2 = t;

			t = TileY(i);
			if (t < y1) y1 = t;
			if (t > y2) y2 = t;
		}
	}

	if (y2 - y1 >= _patches.station_spread || x2 - x1 >= _patches.station_spread) {
		_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
		return false;
	}

	return true;
}

static Station *AllocateStation(void)
{
	Station *st = NULL;

	FOR_ALL_STATIONS(st) {
		if (st->xy == 0) {
			StationID index = st->index;

			memset(st, 0, sizeof(Station));
			st->index = index;

			return st;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_station_pool)) return AllocateStation();

	_error_message = STR_3008_TOO_MANY_STATIONS_LOADING;
	return NULL;
}


static int CountMapSquareAround(TileIndex tile, byte type, byte min, byte max)
{
	static const TileIndexDiffC _count_square_table[] = {
		{-3, -3}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
		{-6,  1}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
		{-6,  1}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
		{-6,  1}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
		{-6,  1}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
		{-6,  1}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
		{-6,  1}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}
	};
	const TileIndexDiffC *p;
	int num = 0;

	for (p = _count_square_table; p != endof(_count_square_table); ++p) {
		tile = TILE_MASK(tile + ToTileIndexDiff(*p));

		if (IsTileType(tile, type) && _m[tile].m5 >= min && _m[tile].m5 <= max)
			num++;
	}

	return num;
}

#define M(x) ((x) - STR_SV_STNAME)

static bool GenerateStationName(Station *st, TileIndex tile, int flag)
{
	static const uint32 _gen_station_name_bits[] = {
		0,                                      /* 0 */
		1 << M(STR_SV_STNAME_AIRPORT),          /* 1 */
		1 << M(STR_SV_STNAME_OILFIELD),         /* 2 */
		1 << M(STR_SV_STNAME_DOCKS),            /* 3 */
		0x1FF << M(STR_SV_STNAME_BUOY_1),       /* 4 */
		1 << M(STR_SV_STNAME_HELIPORT),         /* 5 */
	};

	Town *t = st->town;
	uint32 free_names = (uint32)-1;
	int found;
	uint z,z2;
	unsigned long tmp;

	{
		Station *s;

		FOR_ALL_STATIONS(s) {
			if (s != st && s->xy != 0 && s->town==t) {
				uint str = M(s->string_id);
				if (str <= 0x20) {
					if (str == M(STR_SV_STNAME_FOREST))
						str = M(STR_SV_STNAME_WOODS);
					CLRBIT(free_names, str);
				}
			}
		}
	}

	/* check default names */
	tmp = free_names & _gen_station_name_bits[flag];
	if (tmp != 0) {
		found = FindFirstBit(tmp);
		goto done;
	}

	/* check mine? */
	if (HASBIT(free_names, M(STR_SV_STNAME_MINES))) {
		if (CountMapSquareAround(tile, MP_INDUSTRY, 0, 6) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, 0x64, 0x73) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, 0x2F, 0x33) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, 0x48, 0x58) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, 0x5B, 0x63) >= 2) {
			found = M(STR_SV_STNAME_MINES);
			goto done;
		}
	}

	/* check close enough to town to get central as name? */
	if (DistanceMax(tile,t->xy) < 8) {
		found = M(STR_SV_STNAME);
		if (HASBIT(free_names, M(STR_SV_STNAME))) goto done;

		found = M(STR_SV_STNAME_CENTRAL);
		if (HASBIT(free_names, M(STR_SV_STNAME_CENTRAL))) goto done;
	}

	/* Check lakeside */
	if (HASBIT(free_names, M(STR_SV_STNAME_LAKESIDE)) &&
			DistanceFromEdge(tile) < 20 &&
			CountMapSquareAround(tile, MP_WATER, 0, 0) >= 5) {
		found = M(STR_SV_STNAME_LAKESIDE);
		goto done;
	}

	/* Check woods */
	if (HASBIT(free_names, M(STR_SV_STNAME_WOODS)) && (
				CountMapSquareAround(tile, MP_TREES, 0, 255) >= 8 ||
				CountMapSquareAround(tile, MP_INDUSTRY, 0x10, 0x11) >= 2)
			) {
		found = _opt.landscape == LT_DESERT ?
			M(STR_SV_STNAME_FOREST) : M(STR_SV_STNAME_WOODS);
		goto done;
	}

	/* check elevation compared to town */
	z = GetTileZ(tile);
	z2 = GetTileZ(t->xy);
	if (z < z2) {
		found = M(STR_SV_STNAME_VALLEY);
		if (HASBIT(free_names, M(STR_SV_STNAME_VALLEY))) goto done;
	} else if (z > z2) {
		found = M(STR_SV_STNAME_HEIGHTS);
		if (HASBIT(free_names, M(STR_SV_STNAME_HEIGHTS))) goto done;
	}

	/* check direction compared to town */
	{
		static const int8 _direction_and_table[] = {
			~( (1<<M(STR_SV_STNAME_WEST)) | (1<<M(STR_SV_STNAME_EAST)) | (1<<M(STR_SV_STNAME_NORTH)) ),
			~( (1<<M(STR_SV_STNAME_SOUTH)) | (1<<M(STR_SV_STNAME_WEST)) | (1<<M(STR_SV_STNAME_NORTH)) ),
			~( (1<<M(STR_SV_STNAME_SOUTH)) | (1<<M(STR_SV_STNAME_EAST)) | (1<<M(STR_SV_STNAME_NORTH)) ),
			~( (1<<M(STR_SV_STNAME_SOUTH)) | (1<<M(STR_SV_STNAME_WEST)) | (1<<M(STR_SV_STNAME_EAST)) ),
		};

		free_names &= _direction_and_table[
			(TileX(tile) < TileX(t->xy)) +
			(TileY(tile) < TileY(t->xy)) * 2];
	}

	tmp = free_names & ((1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7)|(1<<12)|(1<<26)|(1<<27)|(1<<28)|(1<<29)|(1<<30));
	if (tmp == 0) {
		_error_message = STR_3007_TOO_MANY_STATIONS_LOADING;
		return false;
	}
	found = FindFirstBit(tmp);

done:
	st->string_id = found + STR_SV_STNAME;
	return true;
}
#undef M

static Station* GetClosestStationFromTile(TileIndex tile, uint threshold, PlayerID owner)
{
	Station* best_station = NULL;
	Station* st;

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0 && (owner == OWNER_SPECTATOR || st->owner == owner)) {
			uint cur_dist = DistanceManhattan(tile, st->xy);

			if (cur_dist < threshold) {
				threshold = cur_dist;
				best_station = st;
			}
		}
	}

	return best_station;
}

static void StationInitialize(Station *st, TileIndex tile)
{
	GoodsEntry *ge;

	st->xy = tile;
	st->airport_tile = st->dock_tile = st->train_tile = 0;
	st->bus_stops = st->truck_stops = NULL;
	st->had_vehicle_of_type = 0;
	st->time_since_load = 255;
	st->time_since_unload = 255;
	st->delete_ctr = 0;
	st->facilities = 0;

	st->last_vehicle = INVALID_VEHICLE;

	for (ge = st->goods; ge != endof(st->goods); ge++) {
		ge->waiting_acceptance = 0;
		ge->days_since_pickup = 0;
		ge->enroute_from = INVALID_STATION;
		ge->rating = 175;
		ge->last_speed = 0;
		ge->last_age = 0xFF;
		ge->feeder_profit = 0;
	}

	_global_station_sort_dirty = true; // build a new station
}

// Update the virtual coords needed to draw the station sign.
// st = Station to update for.
static void UpdateStationVirtCoord(Station *st)
{
	Point pt = RemapCoords2(TileX(st->xy) * 16, TileY(st->xy) * 16);

	pt.y -= 32;
	if (st->facilities & FACIL_AIRPORT && st->airport_type == AT_OILRIG) pt.y -= 16;

	SetDParam(0, st->index);
	SetDParam(1, st->facilities);
	UpdateViewportSignPos(&st->sign, pt.x, pt.y, STR_305C_0);
}

// Update the virtual coords needed to draw the station sign for all stations.
void UpdateAllStationVirtCoord(void)
{
	Station* st;

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0) UpdateStationVirtCoord(st);
	}
}

// Update the station virt coords while making the modified parts dirty.
static void UpdateStationVirtCoordDirty(Station *st)
{
	MarkStationDirty(st);
	UpdateStationVirtCoord(st);
	MarkStationDirty(st);
}

// Get a mask of the cargo types that the station accepts.
static uint GetAcceptanceMask(const Station *st)
{
	uint mask = 0;
	uint i;

	for (i = 0; i != NUM_CARGO; i++) {
		if (st->goods[i].waiting_acceptance & 0x8000) mask |= 1 << i;
	}
	return mask;
}

// Items contains the two cargo names that are to be accepted or rejected.
// msg is the string id of the message to display.
static void ShowRejectOrAcceptNews(const Station *st, uint32 items, StringID msg)
{
	if (items) {
		SetDParam(2, GB(items, 16, 16));
		SetDParam(1, GB(items,  0, 16));
		SetDParam(0, st->index);
		AddNewsItem(msg + ((items >> 16)?1:0), NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_TILE, NT_ACCEPTANCE, 0), st->xy, 0);
	}
}

// Get a list of the cargo types being produced around the tile.
void GetProductionAroundTiles(AcceptedCargo produced, TileIndex tile,
	int w, int h, int rad)
{
	int x,y;
	int x1,y1,x2,y2;
	int xc,yc;

	memset(produced, 0, sizeof(AcceptedCargo));

	x = TileX(tile);
	y = TileY(tile);

	// expand the region by rad tiles on each side
	// while making sure that we remain inside the board.
	x2 = min(x + w + rad, MapSizeX());
	x1 = max(x - rad, 0);

	y2 = min(y + h + rad, MapSizeY());
	y1 = max(y - rad, 0);

	assert(x1 < x2);
	assert(y1 < y2);
	assert(w > 0);
	assert(h > 0);

	for (yc = y1; yc != y2; yc++) {
		for (xc = x1; xc != x2; xc++) {
			if (!(IS_INSIDE_1D(xc, x, w) && IS_INSIDE_1D(yc, y, h))) {
				GetProducedCargoProc *gpc;
				TileIndex tile = TileXY(xc, yc);

				gpc = _tile_type_procs[GetTileType(tile)]->get_produced_cargo_proc;
				if (gpc != NULL) {
					byte cargos[2] = { CT_INVALID, CT_INVALID };

					gpc(tile, cargos);
					if (cargos[0] != CT_INVALID) {
						produced[cargos[0]]++;
						if (cargos[1] != CT_INVALID) {
							produced[cargos[1]]++;
						}
					}
				}
			}
		}
	}
}

// Get a list of the cargo types that are accepted around the tile.
void GetAcceptanceAroundTiles(AcceptedCargo accepts, TileIndex tile,
	int w, int h, int rad)
{
	int x,y;
	int x1,y1,x2,y2;
	int xc,yc;

	memset(accepts, 0, sizeof(AcceptedCargo));

	x = TileX(tile);
	y = TileY(tile);

	// expand the region by rad tiles on each side
	// while making sure that we remain inside the board.
	x2 = min(x + w + rad, MapSizeX());
	y2 = min(y + h + rad, MapSizeY());
	x1 = max(x - rad, 0);
	y1 = max(y - rad, 0);

	assert(x1 < x2);
	assert(y1 < y2);
	assert(w > 0);
	assert(h > 0);

	for (yc = y1; yc != y2; yc++) {
		for (xc = x1; xc != x2; xc++) {
			TileIndex tile = TileXY(xc, yc);

			if (!IsTileType(tile, MP_STATION)) {
				AcceptedCargo ac;
				uint i;

				GetAcceptedCargo(tile, ac);
				for (i = 0; i < lengthof(ac); ++i) accepts[i] += ac[i];
			}
		}
	}
}

typedef struct Rectangle {
	uint min_x;
	uint min_y;
	uint max_x;
	uint max_y;
} Rectangle;

static void MergePoint(Rectangle* rect, TileIndex tile)
{
	uint x = TileX(tile);
	uint y = TileY(tile);

	if (rect->min_x > x) rect->min_x = x;
	if (rect->min_y > y) rect->min_y = y;
	if (rect->max_x < x) rect->max_x = x;
	if (rect->max_y < y) rect->max_y = y;
}

// Update the acceptance for a station.
// show_msg controls whether to display a message that acceptance was changed.
static void UpdateStationAcceptance(Station *st, bool show_msg)
{
	uint old_acc, new_acc;
	const RoadStop *cur_rs;
	int i;
	Rectangle rect;
	int rad;
	AcceptedCargo accepts;

	rect.min_x = MapSizeX();
	rect.min_y = MapSizeY();
	rect.max_x = rect.max_y = 0;
	// Don't update acceptance for a buoy
	if (IsBuoy(st)) return;

	/* old accepted goods types */
	old_acc = GetAcceptanceMask(st);

	// Put all the tiles that span an area in the table.
	if (st->train_tile != 0) {
		MergePoint(&rect, st->train_tile);
		MergePoint(&rect,
			st->train_tile + TileDiffXY(st->trainst_w - 1, st->trainst_h - 1)
		);
	}

	if (st->airport_tile != 0) {
		MergePoint(&rect, st->airport_tile);
		MergePoint(&rect,
			st->airport_tile + TileDiffXY(
				_airport_size_x[st->airport_type] - 1,
				_airport_size_y[st->airport_type] - 1
			)
		);
	}

	if (st->dock_tile != 0) MergePoint(&rect, st->dock_tile);

	for (cur_rs = st->bus_stops; cur_rs != NULL; cur_rs = cur_rs->next) {
		MergePoint(&rect, cur_rs->xy);
	}

	for (cur_rs = st->truck_stops; cur_rs != NULL; cur_rs = cur_rs->next) {
		MergePoint(&rect, cur_rs->xy);
	}

	rad = (_patches.modified_catchment) ? FindCatchmentRadius(st) : 4;

	// And retrieve the acceptance.
	if (rect.max_x >= rect.min_x) {
		GetAcceptanceAroundTiles(
			accepts,
			TileXY(rect.min_x, rect.min_y),
			rect.max_x - rect.min_x + 1,
			rect.max_y - rect.min_y + 1,
			rad
		);
	} else {
		memset(accepts, 0, sizeof(accepts));
	}

	// Adjust in case our station only accepts fewer kinds of goods
	for (i = 0; i != NUM_CARGO; i++) {
		uint amt = min(accepts[i], 15);

		// Make sure the station can accept the goods type.
		if ((i != CT_PASSENGERS && !(st->facilities & (byte)~FACIL_BUS_STOP)) ||
				(i == CT_PASSENGERS && !(st->facilities & (byte)~FACIL_TRUCK_STOP)))
			amt = 0;

		SB(st->goods[i].waiting_acceptance, 12, 4, amt);
	}

	// Only show a message in case the acceptance was actually changed.
	new_acc = GetAcceptanceMask(st);
	if (old_acc == new_acc)
		return;

	// show a message to report that the acceptance was changed?
	if (show_msg && st->owner == _local_player && st->facilities) {
		uint32 accept=0, reject=0; /* these contain two string ids each */
		const StringID *str = _cargoc.names_s;

		do {
			if (new_acc & 1) {
				if (!(old_acc & 1)) accept = (accept << 16) | *str;
			} else {
				if (old_acc & 1) reject = (reject << 16) | *str;
			}
		} while (str++,(new_acc>>=1) != (old_acc>>=1));

		ShowRejectOrAcceptNews(st, accept, STR_3040_NOW_ACCEPTS);
		ShowRejectOrAcceptNews(st, reject, STR_303E_NO_LONGER_ACCEPTS);
	}

	// redraw the station view since acceptance changed
	InvalidateWindowWidget(WC_STATION_VIEW, st->index, 4);
}

// This is called right after a station was deleted.
// It checks if the whole station is free of substations, and if so, the station will be
// deleted after a little while.
static void DeleteStationIfEmpty(Station* st)
{
	if (st->facilities == 0) {
		st->delete_ctr = 0;
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
}

static int32 ClearTile_Station(TileIndex tile, byte flags);

// Tries to clear the given area. Returns the cost in case of success.
// Or an error code if it failed.
int32 CheckFlatLandBelow(TileIndex tile, uint w, uint h, uint flags, uint invalid_dirs, StationID* station)
{
	int32 cost = 0, ret;

	uint tileh;
	uint z;
	int allowed_z = -1;
	int flat_z;

	BEGIN_TILE_LOOP(tile_cur, w, h, tile)
		if (!EnsureNoVehicle(tile_cur)) return CMD_ERROR;

		tileh = GetTileSlope(tile_cur, &z);

		/* Prohibit building if
			1) The tile is "steep" (i.e. stretches two height levels)
			-OR-
			2) The tile is non-flat if
				a) the player building is an "old-school" AI
				-OR-
				b) the build_on_slopes switch is disabled
		*/
		if (IsSteepTileh(tileh) ||
				((_is_old_ai_player || !_patches.build_on_slopes) && tileh != 0)) {
			_error_message = STR_0007_FLAT_LAND_REQUIRED;
			return CMD_ERROR;
		}

		flat_z = z;
		if (tileh) {
			// need to check so the entrance to the station is not pointing at a slope.
			if ((invalid_dirs&1 && !(tileh & 0xC) && (uint)w_cur == w) ||
					(invalid_dirs&2 && !(tileh & 6) &&	h_cur == 1) ||
					(invalid_dirs&4 && !(tileh & 3) && w_cur == 1) ||
					(invalid_dirs&8 && !(tileh & 9) && (uint)h_cur == h)) {
				_error_message = STR_0007_FLAT_LAND_REQUIRED;
				return CMD_ERROR;
			}
			cost += _price.terraform;
			flat_z += 8;
		}

		// get corresponding flat level and make sure that all parts of the station have the same level.
		if (allowed_z == -1) {
			// first tile
			allowed_z = flat_z;
		} else if (allowed_z != flat_z) {
			_error_message = STR_0007_FLAT_LAND_REQUIRED;
			return CMD_ERROR;
		}

		// if station is set, then we have special handling to allow building on top of already existing stations.
		// so station points to INVALID_STATION if we can build on any station. or it points to a station if we're only allowed to build
		// on exactly that station.
		if (station != NULL && IsTileType(tile_cur, MP_STATION)) {
			if (_m[tile_cur].m5 >= 8) {
				_error_message = ClearTile_Station(tile_cur, DC_AUTO); // get error message
				return CMD_ERROR;
			} else {
				StationID st = _m[tile_cur].m2;
				if (*station == INVALID_STATION) {
					*station = st;
				} else if (*station != st) {
					_error_message = STR_3006_ADJOINS_MORE_THAN_ONE_EXISTING;
					return CMD_ERROR;
				}
			}
		} else {
			ret = DoCommandByTile(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return CMD_ERROR;
			cost += ret;
		}
	END_TILE_LOOP(tile_cur, w, h, tile)

	return cost;
}

static bool CanExpandRailroadStation(Station *st, uint *fin, int direction)
{
	uint curw = st->trainst_w, curh = st->trainst_h;
	TileIndex tile = fin[0];
	uint w = fin[1];
	uint h = fin[2];

	if (_patches.nonuniform_stations) {
		// determine new size of train station region..
		int x = min(TileX(st->train_tile), TileX(tile));
		int y = min(TileY(st->train_tile), TileY(tile));
		curw = max(TileX(st->train_tile) + curw, TileX(tile) + w) - x;
		curh = max(TileY(st->train_tile) + curh, TileY(tile) + h) - y;
		tile = TileXY(x, y);
	} else {
		// check so the direction is the same
		if ((_m[st->train_tile].m5 & 1) != direction) {
			_error_message = STR_306D_NONUNIFORM_STATIONS_DISALLOWED;
			return false;
		}

		// check if the new station adjoins the old station in either direction
		if (curw == w && st->train_tile == tile + TileDiffXY(0, h)) {
			// above
			curh += h;
		} else if (curw == w && st->train_tile == tile - TileDiffXY(0, curh)) {
			// below
			tile -= TileDiffXY(0, curh);
			curh += h;
		} else if (curh == h && st->train_tile == tile + TileDiffXY(w, 0)) {
			// to the left
			curw += w;
		} else if (curh == h && st->train_tile == tile - TileDiffXY(curw, 0)) {
			// to the right
			tile -= TileDiffXY(curw, 0);
			curw += w;
		} else {
			_error_message = STR_306D_NONUNIFORM_STATIONS_DISALLOWED;
			return false;
		}
	}
	// make sure the final size is not too big.
	if (curw > _patches.station_spread || curh > _patches.station_spread) {
		_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
		return false;
	}

	// now tile contains the new value for st->train_tile
	// curw, curh contain the new value for width and height
	fin[0] = tile;
	fin[1] = curw;
	fin[2] = curh;
	return true;
}

static inline byte *CreateSingle(byte *layout, int n)
{
	int i = n;
	do *layout++ = 0; while (--i);
	layout[((n-1) >> 1)-n] = 2;
	return layout;
}

static inline byte *CreateMulti(byte *layout, int n, byte b)
{
	int i = n;
	do *layout++ = b; while (--i);
	if (n > 4) {
		layout[0-n] = 0;
		layout[n-1-n] = 0;
	}
	return layout;
}

static void GetStationLayout(byte *layout, int numtracks, int plat_len, const StationSpec *spec)
{
	if (spec != NULL && spec->lengths >= plat_len &&
			spec->platforms[plat_len - 1] >= numtracks &&
			spec->layouts[plat_len - 1][numtracks - 1]) {
		/* Custom layout defined, follow it. */
		memcpy(layout, spec->layouts[plat_len - 1][numtracks - 1],
			plat_len * numtracks);
		return;
	}

	if (plat_len == 1) {
		CreateSingle(layout, numtracks);
	} else {
		if (numtracks & 1) layout = CreateSingle(layout, plat_len);
		numtracks >>= 1;

		while (--numtracks >= 0) {
			layout = CreateMulti(layout, plat_len, 4);
			layout = CreateMulti(layout, plat_len, 6);
		}
	}
}

/** Build railroad station
 * @param x,y starting position of station dragging/placement
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0)    - orientation (p1 & 1)
 * - p1 = (bit  8-15) - number of tracks
 * - p1 = (bit 16-23) - platform length
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 3) - railtype (p2 & 0xF)
 * - p2 = (bit  4)    - set for custom station (p2 & 0x10)
 * - p2 = (bit  8-..) - custom station id (p2 >> 8)
 */
int32 CmdBuildRailroadStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Station *st;
	TileIndex tile_org;
	int w_org, h_org;
	int32 cost, ret;
	StationID est;
	int plat_len, numtracks;
	int direction;
	uint finalvalues[3];

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile_org = TileVirtXY(x, y);

	/* Does the authority allow this? */
	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile_org)) return CMD_ERROR;
	if (!ValParamRailtype(p2 & 0xF)) return CMD_ERROR;

	/* unpack parameters */
	direction = p1 & 1;
	numtracks = GB(p1,  8, 8);
	plat_len  = GB(p1, 16, 8);
	/* w = length, h = num_tracks */
	if (direction) {
		h_org = plat_len;
		w_org = numtracks;
	} else {
		w_org = plat_len;
		h_org = numtracks;
	}

	if (h_org > _patches.station_spread || w_org > _patches.station_spread) return CMD_ERROR;

	// these values are those that will be stored in train_tile and station_platforms
	finalvalues[0] = tile_org;
	finalvalues[1] = w_org;
	finalvalues[2] = h_org;

	// Make sure the area below consists of clear tiles. (OR tiles belonging to a certain rail station)
	est = INVALID_STATION;
	// If DC_EXEC is in flag, do not want to pass it to CheckFlatLandBelow, because of a nice bug
	//  for detail info, see: https://sourceforge.net/tracker/index.php?func=detail&aid=1029064&group_id=103924&atid=636365
	if (CmdFailed(ret = CheckFlatLandBelow(tile_org, w_org, h_org, flags&~DC_EXEC, 5 << direction, _patches.nonuniform_stations ? &est : NULL))) return CMD_ERROR;
	cost = ret + (numtracks * _price.train_station_track + _price.train_station_length) * plat_len;

	// Make sure there are no similar stations around us.
	st = GetStationAround(tile_org, w_org, h_org, est);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	// See if there is a deleted station close to us.
	if (st == NULL) {
		st = GetClosestStationFromTile(tile_org, 8, _current_player);
		if (st != NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		// Reuse an existing station.
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (st->train_tile != 0) {
			// check if we want to expanding an already existing station?
			if (_is_old_ai_player || !_patches.join_stations)
				return_cmd_error(STR_3005_TOO_CLOSE_TO_ANOTHER_RAILROAD);
			if (!CanExpandRailroadStation(st, finalvalues, direction))
				return CMD_ERROR;
		}

		//XXX can't we pack this in the "else" part of the if above?
		if (!CheckStationSpreadOut(st, tile_org, w_org, h_org)) return CMD_ERROR;
	}	else {
		// Create a new station
		st = AllocateStation();
		if (st == NULL) return CMD_ERROR;

		st->town = ClosestTownFromTile(tile_org, (uint)-1);
		if (_current_player < MAX_PLAYERS && flags & DC_EXEC)
			SETBIT(st->town->have_ratings, _current_player);

		if (!GenerateStationName(st, tile_org, 0)) return CMD_ERROR;

		if (flags & DC_EXEC) StationInitialize(st, tile_org);
	}

	if (flags & DC_EXEC) {
		TileIndexDiff tile_delta;
		byte *layout_ptr;
		StationID station_index = st->index;
		const StationSpec *statspec;

		// Now really clear the land below the station
		// It should never return CMD_ERROR.. but you never know ;)
		//  (a bit strange function name for it, but it really does clear the land, when DC_EXEC is in flags)
		if (CmdFailed(CheckFlatLandBelow(tile_org, w_org, h_org, flags, 5 << direction, _patches.nonuniform_stations ? &est : NULL))) return CMD_ERROR;

		st->train_tile = finalvalues[0];
		if (!st->facilities) st->xy = finalvalues[0];
		st->facilities |= FACIL_TRAIN;
		st->owner = _current_player;

		st->trainst_w = finalvalues[1];
		st->trainst_h = finalvalues[2];

		st->build_date = _date;

		tile_delta = direction ? TileDiffXY(0, 1) : TileDiffXY(1, 0);

		statspec = (p2 & 0x10) != 0 ? GetCustomStation(STAT_CLASS_DFLT, p2 >> 8) : NULL;
		layout_ptr = alloca(numtracks * plat_len);
		GetStationLayout(layout_ptr, numtracks, plat_len, statspec);

		do {
			TileIndex tile = tile_org;
			int w = plat_len;
			do {

				ModifyTile(tile,
					MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
					MP_MAP2 | MP_MAP5 | MP_MAP3LO | MP_MAP3HI,
					station_index, /* map2 parameter */
					p2 & 0xFF,     /* map3lo parameter */
					p2 >> 8,       /* map3hi parameter */
					(*layout_ptr++) + direction   /* map5 parameter */
				);

				tile += tile_delta;
			} while (--w);
			tile_org += tile_delta ^ TileDiffXY(1, 1); // perpendicular to tile_delta
		} while (--numtracks);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}

	return cost;
}

static void MakeRailwayStationAreaSmaller(Station *st)
{
	uint w = st->trainst_w;
	uint h = st->trainst_h;
	TileIndex tile = st->train_tile;
	uint i;

restart:

	// too small?
	if (w != 0 && h != 0) {
		// check the left side, x = constant, y changes
		for (i = 0; !TileBelongsToRailStation(st, tile + TileDiffXY(0, i));) {
			// the left side is unused?
			if (++i == h) {
				tile += TileDiffXY(1, 0);
				w--;
				goto restart;
			}
		}

		// check the right side, x = constant, y changes
		for (i = 0; !TileBelongsToRailStation(st, tile + TileDiffXY(w - 1, i));) {
			// the right side is unused?
			if (++i == h) {
				w--;
				goto restart;
			}
		}

		// check the upper side, y = constant, x changes
		for (i = 0; !TileBelongsToRailStation(st, tile + TileDiffXY(i, 0));) {
			// the left side is unused?
			if (++i == w) {
				tile += TileDiffXY(0, 1);
				h--;
				goto restart;
			}
		}

		// check the lower side, y = constant, x changes
		for (i = 0; !TileBelongsToRailStation(st, tile + TileDiffXY(i, h - 1));) {
			// the left side is unused?
			if (++i == w) {
				h--;
				goto restart;
			}
		}
	} else {
		tile = 0;
	}

	st->trainst_w = w;
	st->trainst_h = h;
	st->train_tile = tile;
}

/** Remove a single tile from a railroad station.
 * This allows for custom-built station with holes and weird layouts
 * @param x,y tile coordinates to remove
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdRemoveFromRailroadStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile = TileVirtXY(x, y);
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// make sure the specified tile belongs to the current player, and that it is a railroad station.
	if (!IsTileType(tile, MP_STATION) || _m[tile].m5 >= 8 || !_patches.nonuniform_stations) return CMD_ERROR;
	st = GetStation(_m[tile].m2);
	if (_current_player != OWNER_WATER && (!CheckOwnership(st->owner) || !EnsureNoVehicle(tile))) return CMD_ERROR;

	// if we reached here, it means we can actually delete it. do that.
	if (flags & DC_EXEC) {
		DoClearSquare(tile);
		// now we need to make the "spanned" area of the railway station smaller if we deleted something at the edges.
		// we also need to adjust train_tile.
		MakeRailwayStationAreaSmaller(st);

		// if we deleted the whole station, delete the train facility.
		if (st->train_tile == 0) {
			st->facilities &= ~FACIL_TRAIN;
			UpdateStationVirtCoordDirty(st);
			DeleteStationIfEmpty(st);
		}
	}
	return _price.remove_rail_station;
}

// determine the number of platforms for the station
uint GetStationPlatforms(const Station *st, TileIndex tile)
{
	TileIndex t;
	TileIndexDiff delta;
	int dir;
	uint len;
	assert(TileBelongsToRailStation(st, tile));

	len = 0;
	dir = _m[tile].m5 & 1;
	delta = dir ? TileDiffXY(0, 1) : TileDiffXY(1, 0);

	// find starting tile..
	t = tile;
	do {
		t -= delta;
		len++;
	} while (TileBelongsToRailStation(st, t) && (_m[t].m5 & 1) == dir);

	// find ending tile
	t = tile;
	do {
		t += delta;
		len++;
	} while (TileBelongsToRailStation(st, t) && (_m[t].m5 & 1) == dir);

	return len - 1;
}

static const RealSpriteGroup *ResolveStationSpriteGroup(const SpriteGroup *spg, const Station *st)
{
	switch (spg->type) {
		case SGT_REAL:
			return &spg->g.real;

		case SGT_DETERMINISTIC: {
			const DeterministicSpriteGroup *dsg = &spg->g.determ;
			SpriteGroup *target;
			int value = -1;

			if ((dsg->variable >> 6) == 0) {
				/* General property */
				value = GetDeterministicSpriteValue(dsg->variable);

			} else {
				if (st == NULL) {
					/* We are in a build dialog of something,
					 * and we are checking for something undefined.
					 * That means we should get the first target
					 * (NOT the default one). */
					if (dsg->num_ranges > 0) {
						target = dsg->ranges[0].group;
					} else {
						target = dsg->default_group;
					}
					return ResolveStationSpriteGroup(target, NULL);
				}

				/* Station-specific property. */
				if (dsg->var_scope == VSG_SCOPE_PARENT) {
					/* TODO: Town structure. */

				} else /* VSG_SELF */ {
					if (dsg->variable == 0x40 || dsg->variable == 0x41) {
						/* FIXME: This is ad hoc only
						 * for waypoints. */
						value = 0x01010000;
					} else {
						/* TODO: Only small fraction done. */
						// TTDPatch runs on little-endian arch;
						// Variable is 0x70 + offset in the TTD's station structure
						switch (dsg->variable - 0x70) {
							case 0x80: value = st->facilities;             break;
							case 0x81: value = st->airport_type;           break;
							case 0x82: value = st->truck_stops->status;    break;
							case 0x83: value = st->bus_stops->status;      break;
							case 0x86: value = st->airport_flags & 0xFFFF; break;
							case 0x87: value = st->airport_flags & 0xFF;   break;
							case 0x8A: value = st->build_date;             break;
						}
					}
				}
			}

			target = value != -1 ? EvalDeterministicSpriteGroup(dsg, value) : dsg->default_group;
			return ResolveStationSpriteGroup(target, st);
		}

		default:
		case SGT_RANDOMIZED:
			error("I don't know how to handle random spritegroups yet!");
			return NULL;
	}
}

uint32 GetCustomStationRelocation(const StationSpec *spec, const Station *st, byte ctype)
{
	const RealSpriteGroup *rsg = ResolveStationSpriteGroup(spec->spritegroup[ctype], st);

	if (rsg->sprites_per_set != 0) {
		if (rsg->loading_count != 0) return rsg->loading[0]->g.result.result;

		if (rsg->loaded_count != 0) return rsg->loaded[0]->g.result.result;
	}

	error("Custom station 0x%08x::0x%02x has no sprites associated.",
		spec->grfid, spec->localidx);
	/* This is what gets subscribed of dtss->image in newgrf.c,
	 * so it's probably kinda "default offset". Try to use it as
	 * emergency measure. */
	return SPR_RAIL_PLATFORM_Y_FRONT;
}

static int32 RemoveRailroadStation(Station *st, TileIndex tile, uint32 flags)
{
	int w,h;
	int32 cost;

	/* if there is flooding and non-uniform stations are enabled, remove platforms tile by tile */
	if (_current_player == OWNER_WATER && _patches.nonuniform_stations)
		return DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_REMOVE_FROM_RAILROAD_STATION);

	/* Current player owns the station? */
	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	/* determine width and height of platforms */
	tile = st->train_tile;
	w = st->trainst_w;
	h = st->trainst_h;

	assert(w != 0 && h != 0);

	/* cost is area * constant */
	cost = w*h*_price.remove_rail_station;

	/* clear all areas of the station */
	do {
		int w_bak = w;
		do {
			// for nonuniform stations, only remove tiles that are actually train station tiles
			if (TileBelongsToRailStation(st, tile)) {
				if (!EnsureNoVehicle(tile))
					return CMD_ERROR;
				if (flags & DC_EXEC)
					DoClearSquare(tile);
			}
			tile += TileDiffXY(1, 0);
		} while (--w);
		w = w_bak;
		tile += TileDiffXY(-w, 1);
	} while (--h);

	if (flags & DC_EXEC) {
		st->train_tile = 0;
		st->facilities &= ~FACIL_TRAIN;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

int32 DoConvertStationRail(TileIndex tile, uint totype, bool exec)
{
	const Station *st = GetStation(_m[tile].m2);
	if (!CheckOwnership(st->owner) || !EnsureNoVehicle(tile)) return CMD_ERROR;

	// tile is not a railroad station?
	if (_m[tile].m5 >= 8) return CMD_ERROR;

	// tile is already of requested type?
	if (GB(_m[tile].m3, 0, 4) == totype) return CMD_ERROR;

	if (exec) {
		// change type.
		SB(_m[tile].m3, 0, 4, totype);
		MarkTileDirtyByTile(tile);
	}

	return _price.build_rail >> 1;
}

void FindRoadStationSpot(bool truck_station, Station *st, RoadStop ***currstop, RoadStop **prev)
{
	RoadStop **primary_stop;

	primary_stop = (truck_station) ? &st->truck_stops : &st->bus_stops;

	if (*primary_stop == NULL) {
		//we have no station of the type yet, so write a "primary station"
		//(the one at st->foo_stops)
		*currstop = primary_stop;
	} else {
		//there are stops already, so append to the end of the list
		*prev = *primary_stop;
		*currstop = &(*primary_stop)->next;
		while (**currstop != NULL) {
			*prev = (*prev)->next;
			*currstop = &(**currstop)->next;
		}
	}
}

/** Build a bus station
 * @param x,y coordinates to build bus station at
 * @param p1 busstop entrance direction (0 through 3), where 0 is NW, 1 is NE, etc.
 * @param p2 0 for Bus stops, 1 for truck stops
 */
int32 CmdBuildRoadStop(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Station *st;
	RoadStop *road_stop;
	RoadStop **currstop;
	RoadStop *prev = NULL;
	TileIndex tile;
	int32 cost;
	bool type = !!p2;

	/* Saveguard the parameters */
	if (p1 > 3) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TileVirtXY(x, y);

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	cost = CheckFlatLandBelow(tile, 1, 1, flags, 1 << p1, NULL);
	if (CmdFailed(cost)) return CMD_ERROR;

	st = GetStationAround(tile, 1, 1, -1);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st != NULL && st->facilities) st = NULL;
	}

	//give us a road stop in the list, and check if something went wrong
	road_stop = AllocateRoadStop();
	if (road_stop == NULL)
		return_cmd_error( (type) ? STR_3008B_TOO_MANY_TRUCK_STOPS : STR_3008A_TOO_MANY_BUS_STOPS);

	if ( st != NULL && (GetNumRoadStops(st, RS_BUS) + GetNumRoadStops(st, RS_TRUCK) >= ROAD_STOP_LIMIT))
		return_cmd_error( (type) ? STR_3008B_TOO_MANY_TRUCK_STOPS : STR_3008A_TOO_MANY_BUS_STOPS);

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1))
			return CMD_ERROR;

		FindRoadStationSpot(type, st, &currstop, &prev);
	} else {
		Town *t;

		st = AllocateStation();
		if (st == NULL) return CMD_ERROR;

		st->town = t = ClosestTownFromTile(tile, (uint)-1);

		FindRoadStationSpot(type, st, &currstop, &prev);

		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, 0)) return CMD_ERROR;

		if (flags & DC_EXEC) StationInitialize(st, tile);
	}

	cost += (type) ? _price.build_truck_station : _price.build_bus_station;

	if (flags & DC_EXEC) {
		//point to the correct item in the _busstops or _truckstops array
		*currstop = road_stop;

		//initialize an empty station
		InitializeRoadStop(road_stop, prev, tile, st->index);
		(*currstop)->type = type;
		if (!st->facilities) st->xy = tile;
		st->facilities |= (type) ? FACIL_TRUCK_STOP : FACIL_BUS_STOP;
		st->owner = _current_player;

		st->build_date = _date;

		ModifyTile(tile,
			MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
			MP_MAP2 | MP_MAP5 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
			st->index,                       /* map2 parameter */
			/* XXX - Truck stops have 0x43 _m[].m5 value + direction
			 * XXX - Bus stops have a _map5 value of 0x47 + direction */
			((type) ? 0x43 : 0x47) + p1 /* map5 parameter */
		);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);

		ai_event(_current_player, ottd_Event_BuildStation, st->index, tile);
		ai_event(_current_player, ottd_Event_BuildRoadStation, st->index, tile);
	}
	return cost;
}

// Remove a bus station
static int32 RemoveRoadStop(Station *st, uint32 flags, TileIndex tile)
{
	RoadStop **primary_stop;
	RoadStop *cur_stop;
	bool is_truck = _m[tile].m5 < 0x47;

	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	if (is_truck) { // truck stop
		primary_stop = &st->truck_stops;
		cur_stop = GetRoadStopByTile(tile, RS_TRUCK);
	} else {
		primary_stop = &st->bus_stops;
		cur_stop = GetRoadStopByTile(tile, RS_BUS);
	}

	assert(cur_stop != NULL);

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		uint i;
		DoClearSquare(tile);

		/* Clear all vehicles destined for this station */
		for (i = 0; i != NUM_SLOTS; i++) {
			if (cur_stop->slot[i] != INVALID_SLOT) {
				Vehicle *v = GetVehicle(cur_stop->slot[i]);
				ClearSlot(v, v->u.road.slot);
			}
		}

		cur_stop->used = false;
		if (cur_stop->prev != NULL) cur_stop->prev->next = cur_stop->next;
		if (cur_stop->next != NULL) cur_stop->next->prev = cur_stop->prev;

		//we only had one stop left
		if (cur_stop->next == NULL && cur_stop->prev == NULL) {
			//so we remove ALL stops
			*primary_stop = NULL;
			st->facilities &= (is_truck) ? ~FACIL_TRUCK_STOP : ~FACIL_BUS_STOP;
		} else if (cur_stop == *primary_stop) {
			//removed the first stop in the list
			//need to set the primary element to the next stop
			*primary_stop = (*primary_stop)->next;
		}

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return (is_truck) ? _price.remove_truck_station : _price.remove_bus_station;
}



// FIXME -- need to move to its corresponding Airport variable
// Country Airfield (small)
static const byte _airport_map5_tiles_country[] = {
	54, 53, 52, 65,
	58, 57, 56, 55,
	64, 63, 63, 62
};

// City Airport (large)
static const byte _airport_map5_tiles_town[] = {
	31,  9, 33,  9,  9, 32,
	27, 36, 29, 34,  8, 10,
	30, 11, 35, 13, 20, 21,
	51, 12, 14, 17, 19, 28,
	38, 13, 15, 16, 18, 39,
	26, 22, 23, 24, 25, 26
};

// Metropolitain Airport (large) - 2 runways
static const byte _airport_map5_tiles_metropolitan[] = {
	 31,  9, 33,  9,  9, 32,
	 27, 36, 29, 34,  8, 10,
	 30, 11, 35, 13, 20, 21,
	102,  8,  8,  8,  8, 28,
	 83, 84, 84, 84, 84, 83,
	 26, 23, 23, 23, 23, 26
};

// International Airport (large) - 2 runways
static const byte _airport_map5_tiles_international[] = {
  88, 89, 89, 89, 89, 89,  88,
 	51,  8,  8,  8,  8,  8,  32,
	30,  8, 11, 27, 11,  8,  10,
	32,  8, 11, 27, 11,  8, 114,
	87,  8, 11, 85, 11,  8, 114,
	87,  8,  8,  8,  8,  8,  90,
	26, 23, 23, 23, 23, 23,  26
};

// Heliport
static const byte _airport_map5_tiles_heliport[] = {
	66,
};

static const byte * const _airport_map5_tiles[] = {
	_airport_map5_tiles_country,				// Country Airfield (small)
	_airport_map5_tiles_town,						// City Airport (large)
	_airport_map5_tiles_heliport,				// Heliport
	_airport_map5_tiles_metropolitan,   // Metropolitain Airport (large)
	_airport_map5_tiles_international,	// International Airport (xlarge)
};

/** Place an Airport.
 * @param x,y tile coordinates where airport will be built
 * @param p1 airport type, @see airport.h
 * @param p2 unused
 */
int32 CmdBuildAirport(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile;
	Town *t;
	Station *st;
	int32 cost;
	int w, h;
	bool airport_upgrade = true;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Check if a valid, buildable airport was chosen for construction */
	if (p1 > lengthof(_airport_map5_tiles) || !HASBIT(GetValidAirports(), p1)) return CMD_ERROR;

	tile = TileVirtXY(x, y);

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	t = ClosestTownFromTile(tile, (uint)-1);

	/* Check if local auth refuses a new airport */
	{
		uint num = 0;
		FOR_ALL_STATIONS(st) {
			if (st->xy != 0 && st->town == t && st->facilities&FACIL_AIRPORT && st->airport_type != AT_OILRIG)
				num++;
		}
		if (num >= 2) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2035_LOCAL_AUTHORITY_REFUSES);
		}
	}

	w = _airport_size_x[p1];
	h = _airport_size_y[p1];

	cost = CheckFlatLandBelow(tile, w, h, flags, 0, NULL);
	if (CmdFailed(cost)) return CMD_ERROR;

	st = GetStationAround(tile, w, h, -1);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st != NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1))
			return CMD_ERROR;

		if (st->airport_tile != 0)
			return_cmd_error(STR_300D_TOO_CLOSE_TO_ANOTHER_AIRPORT);
	} else {
		airport_upgrade = false;

		st = AllocateStation();
		if (st == NULL) return CMD_ERROR;

		st->town = t;

		if (_current_player < MAX_PLAYERS && flags & DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		// if airport type equals Heliport then generate
		// type 5 name, which is heliport, otherwise airport names (1)
		if (!GenerateStationName(st, tile, (p1 == AT_HELIPORT) ? 5 : 1))
			return CMD_ERROR;

		if (flags & DC_EXEC)
			StationInitialize(st, tile);
	}

	cost += _price.build_airport * w * h;

	if (flags & DC_EXEC) {
		const AirportFTAClass *afc = GetAirport(p1);

		st->owner = _current_player;
		if (IsLocalPlayer() && afc->nof_depots != 0)
			_last_built_aircraft_depot_tile = tile + ToTileIndexDiff(afc->airport_depots[0]);

		st->airport_tile = tile;
		if (!st->facilities) st->xy = tile;
		st->facilities |= FACIL_AIRPORT;
		st->airport_type = (byte)p1;
		st->airport_flags = 0;

		st->build_date = _date;

		/* if airport was demolished while planes were en-route to it, the
		 * positions can no longer be the same (v->u.air.pos), since different
		 * airports have different indexes. So update all planes en-route to this
		 * airport. Only update if
		 * 1. airport is upgraded
		 * 2. airport is added to existing station (unfortunately unavoideable)
		 */
		if (airport_upgrade) UpdateAirplanesOnNewStation(st);

		{
			const byte *b = _airport_map5_tiles[p1];

			BEGIN_TILE_LOOP(tile_cur,w,h,tile) {
				ModifyTile(tile_cur,
					MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
					MP_MAP2 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR | MP_MAP5,
					st->index, *b++);
			} END_TILE_LOOP(tile_cur,w,h,tile)
		}

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}

	return cost;
}

static int32 RemoveAirport(Station *st, uint32 flags)
{
	TileIndex tile;
	int w,h;
	int32 cost;

	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	tile = st->airport_tile;

	w = _airport_size_x[st->airport_type];
	h = _airport_size_y[st->airport_type];

	cost = w * h * _price.remove_airport;

	{
BEGIN_TILE_LOOP(tile_cur,w,h,tile)
		if (!EnsureNoVehicle(tile_cur))
			return CMD_ERROR;

		if (flags & DC_EXEC) {
			DeleteAnimatedTile(tile_cur);
			DoClearSquare(tile_cur);
		}
END_TILE_LOOP(tile_cur, w,h,tile)
	}

	if (flags & DC_EXEC) {
		const AirportFTAClass *afc = GetAirport(st->airport_type);
		uint i;

		for (i = 0; i < afc->nof_depots; ++i)
			DeleteWindowById(WC_VEHICLE_DEPOT, tile + ToTileIndexDiff(afc->airport_depots[i]));

		st->airport_tile = 0;
		st->facilities &= ~FACIL_AIRPORT;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

/** Build a buoy.
 * @param x,y tile coordinates of bouy construction
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdBuildBuoy(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti;
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	FindLandscapeHeight(&ti, x, y);

	if (ti.type != MP_WATER || ti.tileh != 0 || ti.map5 != 0 || ti.tile == 0)
		return_cmd_error(STR_304B_SITE_UNSUITABLE);

	st = AllocateStation();
	if (st == NULL) return CMD_ERROR;

	st->town = ClosestTownFromTile(ti.tile, (uint)-1);
	st->sign.width_1 = 0;

	if (!GenerateStationName(st, ti.tile, 4)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StationInitialize(st, ti.tile);
		st->dock_tile = ti.tile;
		st->facilities |= FACIL_DOCK;
		/* Buoys are marked in the Station struct by this flag. Yes, it is this
		 * braindead.. */
		st->had_vehicle_of_type |= HVOT_BUOY;
		st->owner = OWNER_NONE;

		st->build_date = _date;

		ModifyTile(ti.tile,
			MP_SETTYPE(MP_STATION) |
			MP_MAP2 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR | MP_MAPOWNER | MP_MAP5,
			st->index,		/* map2 */
			OWNER_NONE,		/* map_owner */
			0x52					/* map5 */
		);

		UpdateStationVirtCoordDirty(st);

		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}

	return _price.build_dock;
}

/* Checks if any ship is servicing the buoy specified. Returns yes or no */
static bool CheckShipsOnBuoy(Station *st)
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship) {
			const Order *order;
			FOR_VEHICLE_ORDERS(v, order) {
				if (order->type == OT_GOTO_STATION && order->station == st->index) {
					return true;
				}
			}
		}
	}
	return false;
}

static int32 RemoveBuoy(Station *st, uint32 flags)
{
	TileIndex tile;

	if (_current_player >= MAX_PLAYERS) {
		/* XXX: strange stuff */
		return_cmd_error(INVALID_STRING_ID);
	}

	tile = st->dock_tile;

	if (CheckShipsOnBuoy(st))   return_cmd_error(STR_BUOY_IS_IN_USE);
	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		st->dock_tile = 0;
		/* Buoys are marked in the Station struct by this flag. Yes, it is this
		 * braindead.. */
		st->facilities &= ~FACIL_DOCK;
		st->had_vehicle_of_type &= ~HVOT_BUOY;

		ModifyTile(tile,
			MP_SETTYPE(MP_WATER) |
			MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR,
			OWNER_WATER, /* map_owner */
			0			/* map5 */
		);

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return _price.remove_truck_station;
}

static const TileIndexDiffC _dock_tileoffs_chkaround[] = {
	{-1,  0},
	{ 0,  0},
	{ 0,  0},
	{ 0, -1}
};
static const byte _dock_w_chk[4] = { 2,1,2,1 };
static const byte _dock_h_chk[4] = { 1,2,1,2 };

/** Build a dock/haven.
 * @param x,y tile coordinates where dock will be built
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdBuildDock(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti;
	int direction;
	int32 cost;
	TileIndex tile, tile_cur;
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	FindLandscapeHeight(&ti, x, y);

	if ((direction=0,ti.tileh) != 3 &&
			(direction++,ti.tileh) != 9 &&
			(direction++,ti.tileh) != 12 &&
			(direction++,ti.tileh) != 6)
		return_cmd_error(STR_304B_SITE_UNSUITABLE);

	if (!EnsureNoVehicle(ti.tile)) return CMD_ERROR;

	cost = DoCommandByTile(ti.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	tile_cur = (tile=ti.tile) + TileOffsByDir(direction);

	if (!EnsureNoVehicle(tile_cur)) return CMD_ERROR;

	FindLandscapeHeightByTile(&ti, tile_cur);
	if (ti.tileh != 0 || ti.type != MP_WATER) return_cmd_error(STR_304B_SITE_UNSUITABLE);

	cost = DoCommandByTile(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	tile_cur = tile_cur + TileOffsByDir(direction);
	FindLandscapeHeightByTile(&ti, tile_cur);
	if (ti.tileh != 0 || ti.type != MP_WATER)
		return_cmd_error(STR_304B_SITE_UNSUITABLE);

	/* middle */
	st = GetStationAround(
		tile + ToTileIndexDiff(_dock_tileoffs_chkaround[direction]),
		_dock_w_chk[direction], _dock_h_chk[direction], -1);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st!=NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1)) return CMD_ERROR;

		if (st->dock_tile != 0) return_cmd_error(STR_304C_TOO_CLOSE_TO_ANOTHER_DOCK);
	} else {
		Town *t;

		st = AllocateStation();
		if (st == NULL) return CMD_ERROR;

		st->town = t = ClosestTownFromTile(tile, (uint)-1);

		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, 3)) return CMD_ERROR;

		if (flags & DC_EXEC) StationInitialize(st, tile);
	}

	if (flags & DC_EXEC) {
		st->dock_tile = tile;
		if (!st->facilities) st->xy = tile;
		st->facilities |= FACIL_DOCK;
		st->owner = _current_player;

		st->build_date = _date;

		ModifyTile(tile,
			MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
			MP_MAP2 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR |
			MP_MAP5,
			st->index,
			direction + 0x4C);

		ModifyTile(tile + TileOffsByDir(direction),
			MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
			MP_MAP2 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR |
			MP_MAP5,
			st->index,
			(direction&1) + 0x50);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
	return _price.build_dock;
}

static int32 RemoveDock(Station *st, uint32 flags)
{
	TileIndex tile1;
	TileIndex tile2;

	if (!CheckOwnership(st->owner)) return CMD_ERROR;

	tile1 = st->dock_tile;
	tile2 = tile1 + TileOffsByDir(_m[tile1].m5 - 0x4C);

	if (!EnsureNoVehicle(tile1)) return CMD_ERROR;
	if (!EnsureNoVehicle(tile2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile1);

		// convert the water tile to water.
		ModifyTile(tile2, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);

		st->dock_tile = 0;
		st->facilities &= ~FACIL_DOCK;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return _price.remove_dock;
}

#include "table/station_land.h"


extern uint16 _custom_sprites_base;

static void DrawTile_Station(TileInfo *ti)
{
	uint32 image_or_modificator;
	uint32 image;
	const DrawTileSeqStruct *dtss;
	const DrawTileSprites *t = NULL;
	RailType railtype = GB(_m[ti->tile].m3, 0, 4);
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	SpriteID offset;
	uint32 relocation = 0;

	{
		PlayerID owner = GetTileOwner(ti->tile);
		image_or_modificator = PALETTE_TO_GREY; /* NOTE: possible bug in ttd here? */
		if (owner < MAX_PLAYERS) image_or_modificator = PLAYER_SPRITE_COLOR(owner);
	}

	// don't show foundation for docks (docks are between 76 (0x4C) and 81 (0x51))
	if (ti->tileh != 0 && (ti->map5 < 0x4C || ti->map5 > 0x51))
		DrawFoundation(ti, ti->tileh);

	if (_m[ti->tile].m3 & 0x10) {
		// look for customization
		const StationSpec *statspec = GetCustomStation(STAT_CLASS_DFLT, _m[ti->tile].m4);

		//debug("Cust-o-mized %p", statspec);

		if (statspec != NULL) {
			const Station* st = GetStation(_m[ti->tile].m2);

			relocation = GetCustomStationRelocation(statspec, st, 0);
			//debug("Relocation %d", relocation);
			t = &statspec->renderdata[ti->map5];
		}
	}

	if (t == NULL) t = &_station_display_datas[ti->map5];

	image = t->ground_sprite;
	if (image & PALETTE_MODIFIER_COLOR) image |= image_or_modificator;

	// For custom sprites, there's no railtype-based pitching.
	offset = (image & SPRITE_MASK) < _custom_sprites_base ? rti->total_offset : railtype;
	image += offset;

	// station_land array has been increased from 82 elements to 114
	// but this is something else. If AI builds station with 114 it looks all weird
	DrawGroundSprite(image);

	if (_debug_pbs_level >= 1) {
		byte pbs = PBSTileReserved(ti->tile);
		if (pbs & TRACK_BIT_DIAG1) DrawGroundSprite(rti->base_sprites.single_y | PALETTE_CRASH);
		if (pbs & TRACK_BIT_DIAG2) DrawGroundSprite(rti->base_sprites.single_x | PALETTE_CRASH);
		if (pbs & TRACK_BIT_UPPER) DrawGroundSprite(rti->base_sprites.single_n | PALETTE_CRASH);
		if (pbs & TRACK_BIT_LOWER) DrawGroundSprite(rti->base_sprites.single_s | PALETTE_CRASH);
		if (pbs & TRACK_BIT_LEFT)  DrawGroundSprite(rti->base_sprites.single_w | PALETTE_CRASH);
		if (pbs & TRACK_BIT_RIGHT) DrawGroundSprite(rti->base_sprites.single_e | PALETTE_CRASH);
	}

	foreach_draw_tile_seq(dtss, t->seq) {
		image = dtss->image + relocation;
		image += offset;
		if (_display_opt & DO_TRANS_BUILDINGS) {
			MAKE_TRANSPARENT(image);
		} else {
			if (image & PALETTE_MODIFIER_COLOR) image |= image_or_modificator;
		}

		if ((byte)dtss->delta_z != 0x80) {
			AddSortableSpriteToDraw(image, ti->x + dtss->delta_x, ti->y + dtss->delta_y, dtss->width, dtss->height, dtss->unk, ti->z + dtss->delta_z);
		} else {
			AddChildSpriteScreen(image, dtss->delta_x, dtss->delta_y);
		}
	}
}

void StationPickerDrawSprite(int x, int y, RailType railtype, int image)
{
	uint32 ormod, img;
	const DrawTileSeqStruct *dtss;
	const DrawTileSprites *t;
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);

	ormod = PLAYER_SPRITE_COLOR(_local_player);

	t = &_station_display_datas[image];

	img = t->ground_sprite;
	if (img & PALETTE_MODIFIER_COLOR) img |= ormod;
	DrawSprite(img + rti->total_offset, x, y);

	foreach_draw_tile_seq(dtss, t->seq) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		DrawSprite((dtss->image | ormod) + rti->total_offset, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Station(const TileInfo* ti)
{
	return (ti->tileh != 0) ? ti->z + 8 : ti->z;
}

static uint GetSlopeTileh_Station(const TileInfo *ti)
{
	return 0;
}

static void GetAcceptedCargo_Station(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Station(TileIndex tile, TileDesc *td)
{
	byte m5;
	StringID str;

	td->owner = GetTileOwner(tile);
	td->build_date = GetStation(_m[tile].m2)->build_date;

	m5 = _m[tile].m5;
	(str=STR_305E_RAILROAD_STATION, m5 < 8) ||
	(str=STR_305F_AIRCRAFT_HANGAR, m5==32 || m5==45) || // hangars
	(str=STR_3060_AIRPORT, m5 < 0x43 || (m5 >= 83 && m5 <= 114)) ||
	(str=STR_3061_TRUCK_LOADING_AREA, m5 < 0x47) ||
	(str=STR_3062_BUS_STATION, m5 < 0x4B) ||
	(str=STR_4807_OIL_RIG, m5 == 0x4B) ||
	(str=STR_3063_SHIP_DOCK, m5 != 0x52) ||
	(str=STR_3069_BUOY, true);
	td->str = str;
}


static uint32 GetTileTrackStatus_Station(TileIndex tile, TransportType mode)
{
	uint i = _m[tile].m5;
	uint j = 0;

	switch (mode) {
		case TRANSPORT_RAIL:
			if (i < 8) {
				static const byte tile_track_status_rail[] = { 1, 2, 1, 2, 1, 2, 1, 2 };
				j = tile_track_status_rail[i];
			}
			j += (j << 8);
			break;

		case TRANSPORT_WATER:
			// buoy is coded as a station, it is always on open water
			// (0x3F, all tracks available)
			if (i == 0x52) j = 0x3F;
			j += (j << 8);
			break;

		default:
			break;
	}

	return j;
}


static void TileLoop_Station(TileIndex tile)
{
	// FIXME -- GetTileTrackStatus_Station -> animated stationtiles
	// hardcoded.....not good
	switch (_m[tile].m5) {
		case 0x27: // large big airport
		case 0x3A: // flag small airport
		case 0x5A: // radar international airport
		case 0x66: // radar metropolitan airport
			AddAnimatedTile(tile);
			break;

		case 0x4B: // oilrig (station part)
		case 0x52: // bouy
			TileLoop_Water(tile);
			break;

		default: break;
	}
}


static void AnimateTile_Station(TileIndex tile)
{
	byte m5 = _m[tile].m5;
	//FIXME -- AnimateTile_Station -> not nice code, lots of things double
  // again hardcoded...was a quick hack

  // turning radar / windsack on airport
	if (m5 >= 39 && m5 <= 50) { // turning radar (39 - 50)
		if (_tick_counter & 3)
			return;

		if (++m5 == 50+1)
			m5 = 39;

		_m[tile].m5 = m5;
		MarkTileDirtyByTile(tile);
  //added - begin
	} else if (m5 >= 90 && m5 <= 113) { // turning radar with ground under it (different fences) (90 - 101 | 102 - 113)
		if (_tick_counter & 3)
			return;

		m5++;

		if (m5 == 101+1) {m5 = 90;}  // radar with fences in south
		else if (m5 == 113+1) {m5 = 102;} // radar with fences in north

		_m[tile].m5 = m5;
		MarkTileDirtyByTile(tile);
	//added - end
	} else if (m5 >= 0x3A && m5 <= 0x3D) {  // windsack (58 - 61)
		if (_tick_counter & 1)
			return;

		if (++m5 == 0x3D+1)
			m5 = 0x3A;

		_m[tile].m5 = m5;
		MarkTileDirtyByTile(tile);
	}
}

static void ClickTile_Station(TileIndex tile)
{
  // 0x20 - hangar large airport (32)
  // 0x41 - hangar small airport (65)
	if (_m[tile].m5 == 32 || _m[tile].m5 == 65) {
		ShowAircraftDepotWindow(tile);
	} else {
		ShowStationViewWindow(_m[tile].m2);
	}
}

static const byte _enter_station_speedtable[12] = {
	215, 195, 175, 155, 135, 115, 95, 75, 55, 35, 15, 0
};

static uint32 VehicleEnter_Station(Vehicle *v, TileIndex tile, int x, int y)
{
	StationID station_id;
	byte dir;

	if (v->type == VEH_Train) {
		if (IS_BYTE_INSIDE(_m[tile].m5, 0, 8) && IsFrontEngine(v) &&
				!IsCompatibleTrainStationTile(tile + TileOffsByDir(v->direction >> 1), tile)) {

			station_id = _m[tile].m2;
			if ((!(v->current_order.flags & OF_NON_STOP) && !_patches.new_nonstop) ||
					(v->current_order.type == OT_GOTO_STATION && v->current_order.station == station_id)) {
				if (!(_patches.new_nonstop && v->current_order.flags & OF_NON_STOP) &&
						v->current_order.type != OT_LEAVESTATION &&
						v->last_station_visited != station_id) {
					x &= 0xF;
					y &= 0xF;

					dir = v->direction & 6;
					if (dir & 2) intswap(x,y);
					if (y == 8) {
						if (dir != 2 && dir != 4) x = ~x & 0xF;
						if (x == 12) return 2 | (station_id << 8); /* enter station */
						if (x < 12) {
							uint16 spd;

							v->vehstatus |= VS_TRAIN_SLOWING;
							spd = _enter_station_speedtable[x];
							if (spd < v->cur_speed) v->cur_speed = spd;
						}
					}
				}
			}
		}
	} else if (v->type == VEH_Road) {
		if (v->u.road.state < 16 && !HASBIT(v->u.road.state, 2) && v->u.road.frame == 0) {
			if (IS_BYTE_INSIDE(_m[tile].m5, 0x43, 0x4B)) {
				/* Attempt to allocate a parking bay in a road stop */
				RoadStop *rs = GetRoadStopByTile(tile, GetRoadStopType(tile));

				/* rs->status bits 0 and 1 describe current the two parking spots.
				 * 0 means occupied, 1 means free. */

				// Check if station is busy or if there are no free bays.
				if (HASBIT(rs->status, 7) || GB(rs->status, 0, 2) == 0)
					return 8;

				v->u.road.state += 32;

				// if the first bay is free, allocate that, else the second bay must be free.
				if (HASBIT(rs->status, 0)) {
					CLRBIT(rs->status, 0);
				} else {
					CLRBIT(rs->status, 1);
					v->u.road.state += 2;
				}

				// mark the station as busy
				SETBIT(rs->status, 7);
			}
		}
	}

	return 0;
}

/** Removes a station from the list.
  * This is done by setting the .xy property to 0,
  * and doing some maintenance, especially clearing vehicle orders.
  * Aircraft-Hangar orders need special treatment here, as the hangars are
  * actually part of a station (tiletype is STATION), but the order type
  * is OT_GOTO_DEPOT.
  * @param st Station to be deleted
  */
static void DeleteStation(Station *st)
{
	Order order;
	StationID index;
	Vehicle *v;
	st->xy = 0;

	DeleteName(st->string_id);
	MarkStationDirty(st);
	_global_station_sort_dirty = true; // delete station, remove sign
	InvalidateWindowClasses(WC_STATION_LIST);

	index = st->index;
	DeleteWindowById(WC_STATION_VIEW, index);

	//Now delete all orders that go to the station
	order.type = OT_GOTO_STATION;
	order.station = index;
	DeleteDestinationFromVehicleOrder(order);

	//And do the same with aircraft that have the station as a hangar-stop
	FOR_ALL_VEHICLES(v) {
		bool invalidate = false;
		if (v->type == VEH_Aircraft) {
			Order *order;
			FOR_VEHICLE_ORDERS(v, order) {
				if (order->type == OT_GOTO_DEPOT && order->station == index) {
					order->type = OT_DUMMY;
					order->flags = 0;
					invalidate = true;
				}
			}
		}
		//Orders for the vehicle have been changed, invalidate the window
		if (invalidate) InvalidateWindow(WC_VEHICLE_ORDERS, v->index);
	}

	//Subsidies need removal as well
	DeleteSubsidyWithStation(index);
}

void DeleteAllPlayerStations(void)
{
	Station *st;

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0 && st->owner < MAX_PLAYERS) DeleteStation(st);
	}
}

static void CheckOrphanedSlots(const Station *st, RoadStopType rst)
{
	RoadStop *rs;
	int k;

	for (rs = GetPrimaryRoadStop(st, rst); rs != NULL; rs = rs->next) {
		for (k = 0; k < NUM_SLOTS; k++) {
			if (rs->slot[k] != INVALID_SLOT) {
				const Vehicle *v = GetVehicle(rs->slot[k]);

				if (v->type != VEH_Road || v->u.road.slot != rs) {
					DEBUG(ms, 0) (
						"Multistop: Orphaned %s slot at 0x%X of station %d (don't panic)",
						(rst == RS_BUS) ? "bus" : "truck", rs->xy, st->index);
					rs->slot[k] = INVALID_SLOT;
				}
			}
		}
	}
}

/* this function is called for one station each tick */
static void StationHandleBigTick(Station *st)
{
	UpdateStationAcceptance(st, true);

	if (st->facilities == 0 && ++st->delete_ctr >= 8) DeleteStation(st);

	// Here we saveguard against orphaned slots
	CheckOrphanedSlots(st, RS_BUS);
	CheckOrphanedSlots(st, RS_TRUCK);
}

static inline void byte_inc_sat(byte *p) { byte b = *p + 1; if (b != 0) *p = b; }

static void UpdateStationRating(Station *st)
{
	GoodsEntry *ge;
	int rating;
	StationID index;
	int waiting;
	bool waiting_changed = false;

	byte_inc_sat(&st->time_since_load);
	byte_inc_sat(&st->time_since_unload);

	ge = st->goods;
	do {
		if (ge->enroute_from != INVALID_STATION) {
			byte_inc_sat(&ge->enroute_time);
			byte_inc_sat(&ge->days_since_pickup);

			rating = 0;

			{
				int b = ge->last_speed;
				if ((b-=85) >= 0)
					rating += b >> 2;
			}

			{
				byte age = ge->last_age;
				(age >= 3) ||
				(rating += 10, age >= 2) ||
				(rating += 10, age >= 1) ||
				(rating += 13, true);
			}

			if (st->owner < MAX_PLAYERS && HASBIT(st->town->statues, st->owner))
				rating += 26;

			{
				byte days = ge->days_since_pickup;
				if (st->last_vehicle != INVALID_VEHICLE &&
						GetVehicle(st->last_vehicle)->type == VEH_Ship)
							days >>= 2;
				(days > 21) ||
				(rating += 25, days > 12) ||
				(rating += 25, days > 6) ||
				(rating += 45, days > 3) ||
				(rating += 35, true);
			}

			{
				waiting = GB(ge->waiting_acceptance, 0, 12);
				(rating -= 90, waiting > 1500) ||
				(rating += 55, waiting > 1000) ||
				(rating += 35, waiting > 600) ||
				(rating += 10, waiting > 300) ||
				(rating += 20, waiting > 100) ||
				(rating += 10, true);
			}

			{
				int or = ge->rating; // old rating

				// only modify rating in steps of -2, -1, 0, 1 or 2
				ge->rating = rating = or + clamp(clamp(rating, 0, 255) - or, -2, 2);

				// if rating is <= 64 and more than 200 items waiting, remove some random amount of goods from the station
				if (rating <= 64 && waiting >= 200) {
					int dec = Random() & 0x1F;
					if (waiting < 400) dec &= 7;
					waiting -= dec + 1;
					waiting_changed = true;
				}

				// if rating is <= 127 and there are any items waiting, maybe remove some goods.
				if (rating <= 127 && waiting != 0) {
					uint32 r = Random();
					if ( (uint)rating <= (r & 0x7F) ) {
						waiting = max(waiting - ((r >> 8)&3) - 1, 0);
						waiting_changed = true;
					}
				}

				if (waiting_changed) SB(ge->waiting_acceptance, 0, 12, waiting);
			}
		}
	} while (++ge != endof(st->goods));

	index = st->index;

	if (waiting_changed)
		InvalidateWindow(WC_STATION_VIEW, index);
	else
		InvalidateWindowWidget(WC_STATION_VIEW, index, 5);
}

/* called for every station each tick */
static void StationHandleSmallTick(Station *st)
{
	byte b;

	if (st->facilities == 0) return;

	b = st->delete_ctr + 1;
	if (b >= 185) b = 0;
	st->delete_ctr = b;

	if (b == 0) UpdateStationRating(st);
}

void OnTick_Station(void)
{
	uint i;
	Station *st;

	if (_game_mode == GM_EDITOR) return;

	i = _station_tick_ctr;
	if (++_station_tick_ctr == GetStationPoolSize()) _station_tick_ctr = 0;

	st = GetStation(i);
	if (st->xy != 0) StationHandleBigTick(st);

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0) StationHandleSmallTick(st);
	}
}

void StationMonthlyLoop(void)
{
}


void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius)
{
	Station *st;

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0 && st->owner == owner &&
				DistanceManhattan(tile, st->xy) <= radius) {
			uint i;

			for (i = 0; i != NUM_CARGO; i++) {
				GoodsEntry* ge = &st->goods[i];

				if (ge->enroute_from != INVALID_STATION) {
					ge->rating = clamp(ge->rating + amount, 0, 255);
				}
			}
		}
	}
}

static void UpdateStationWaiting(Station *st, int type, uint amount)
{
	SB(st->goods[type].waiting_acceptance, 0, 12,
		min(0xFFF, GB(st->goods[type].waiting_acceptance, 0, 12) + amount)
	);

	st->goods[type].enroute_time = 0;
	st->goods[type].enroute_from = st->index;
	InvalidateWindow(WC_STATION_VIEW, st->index);
}

/** Rename a station
 * @param x,y unused
 * @param p1 station ID that is to be renamed
 * @param p2 unused
 */
int32 CmdRenameStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Station *st;

	if (!IsStationIndex(p1) || _cmd_text[0] == '\0') return CMD_ERROR;
	st = GetStation(p1);

	if (!IsValidStation(st) || !CheckOwnership(st->owner)) return CMD_ERROR;

	str = AllocateNameUnique(_cmd_text, 6);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StringID old_str = st->string_id;

		st->string_id = str;
		UpdateStationVirtCoord(st);
		DeleteName(old_str);
		_station_sort_dirty[st->owner] = true; // rename a station
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}


uint MoveGoodsToStation(TileIndex tile, int w, int h, int type, uint amount)
{
	Station *around_ptr[8];
	StationID around[8];
	StationID st_index;
	int i;
	Station *st;
	uint moved;
	uint best_rating, best_rating2;
	Station *st1, *st2;
	int t;
	int rad=0;
	int w_prod=0, h_prod=0; //width and height of the "producer" of the cargo
	int x_min_prod, x_max_prod;     //min and max coordinates of the producer
	int y_min_prod, y_max_prod;     //relative
	int x_dist, y_dist;
	int max_rad;


	memset(around, 0xff, sizeof(around));

	if (_patches.modified_catchment) {
		w_prod = w;
		h_prod = h;
		w += 16;
		h += 16;
		max_rad = 8;
	} else {
 		w += 8;
 		h += 8;
		max_rad = 4;
	}

	BEGIN_TILE_LOOP(cur_tile, w, h, tile - TileDiffXY(max_rad, max_rad))
		cur_tile = TILE_MASK(cur_tile);
		if (IsTileType(cur_tile, MP_STATION)) {
			st_index = _m[cur_tile].m2;
			for (i = 0; i != 8; i++) {
				if (around[i] == INVALID_STATION) {
					st = GetStation(st_index);
					if (!IsBuoy(st) &&
							( !st->town->exclusive_counter || (st->town->exclusivity == st->owner) ) && // check exclusive transport rights
							st->goods[type].rating != 0 &&
							(!_patches.selectgoods || st->goods[type].last_speed) && // if last_speed is 0, no vehicle has been there.
							((st->facilities & (byte)~FACIL_BUS_STOP)!=0 || type==CT_PASSENGERS) && // if we have other fac. than a bus stop, or the cargo is passengers
							((st->facilities & (byte)~FACIL_TRUCK_STOP)!=0 || type!=CT_PASSENGERS)) { // if we have other fac. than a cargo bay or the cargo is not passengers
								if (_patches.modified_catchment) {
									rad = FindCatchmentRadius(st);
									x_min_prod = y_min_prod = 9;
									x_max_prod = 8 + w_prod;
									y_max_prod = 8 + h_prod;

									x_dist = min(w_cur - x_min_prod, x_max_prod - w_cur);

									if (w_cur < x_min_prod) {
										x_dist = x_min_prod - w_cur;
									} else {        //save cycles
										if (w_cur > x_max_prod) x_dist = w_cur - x_max_prod;
									}

									y_dist = min(h_cur - y_min_prod, y_max_prod - h_cur);
									if (h_cur < y_min_prod) {
										y_dist = y_min_prod - h_cur;
									} else {
										if (h_cur > y_max_prod) y_dist = h_cur - y_max_prod;
									}

								} else {
									x_dist = y_dist = 0;
								}

								if ( !(x_dist > rad) && !(y_dist > rad) ) {

									around[i] = st_index;
									around_ptr[i] = st;
								}
							}
					break;
				} else if (around[i] == st_index)
					break;
			}
		}
	END_TILE_LOOP(cur_tile, w, h, tile - TileDiffXY(max_rad, max_rad))

	/* no stations around at all? */
	if (around[0] == INVALID_STATION)
		return 0;

	if (around[1] == INVALID_STATION) {
		/* only one station around */
		moved = (amount * around_ptr[0]->goods[type].rating >> 8) + 1;
		UpdateStationWaiting(around_ptr[0], type, moved);
		return moved;
	}

	/* several stations around, find the two with the highest rating */
	st2 = st1 = NULL;
	best_rating = best_rating2 = 0;

	for( i = 0; i != 8 && around[i] != INVALID_STATION; i++) {
		if (around_ptr[i]->goods[type].rating >= best_rating) {
			best_rating2 = best_rating;
			st2 = st1;

			best_rating = around_ptr[i]->goods[type].rating;
			st1 = around_ptr[i];
		} else if (around_ptr[i]->goods[type].rating >= best_rating2) {
			best_rating2 = around_ptr[i]->goods[type].rating;
			st2 = around_ptr[i];
		}
	}

	assert(st1 != NULL);
	assert(st2 != NULL);
	assert(best_rating != 0 || best_rating2 != 0);

	/* the 2nd highest one gets a penalty */
	best_rating2 >>= 1;

	/* amount given to station 1 */
	t = (best_rating * (amount + 1)) / (best_rating + best_rating2);

	moved = 0;
	if (t != 0) {
		moved = (t * best_rating >> 8) + 1;
		amount -= t;
		UpdateStationWaiting(st1, type, moved);
	}

	if (amount != 0) {
		moved += (amount = (amount * best_rating2 >> 8) + 1);
		UpdateStationWaiting(st2, type, amount);
	}

	return moved;
}

void BuildOilRig(TileIndex tile)
{
	uint j;
	Station *st = AllocateStation();

	if (st == NULL) {
		DEBUG(misc, 0) ("Couldn't allocate station for oilrig at %#X, reverting to oilrig only...", tile);
		return;
	}
	if (!GenerateStationName(st, tile, 2)) {
		DEBUG(misc, 0) ("Couldn't allocate station-name for oilrig at %#X, reverting to oilrig only...", tile);
		return;
	}

	st->town = ClosestTownFromTile(tile, (uint)-1);
	st->sign.width_1 = 0;

	SetTileType(tile, MP_STATION);
	SetTileOwner(tile, OWNER_NONE);
	_m[tile].m2 = st->index;
	_m[tile].m3 = 0;
	_m[tile].m4 = 0;
	_m[tile].m5 = 0x4B;

	st->owner = OWNER_NONE;
	st->airport_flags = 0;
	st->airport_type = AT_OILRIG;
	st->xy = tile;
	st->bus_stops = NULL;
	st->truck_stops = NULL;
	st->airport_tile = tile;
	st->dock_tile = tile;
	st->train_tile = 0;
	st->had_vehicle_of_type = 0;
	st->time_since_load = 255;
	st->time_since_unload = 255;
	st->delete_ctr = 0;
	st->last_vehicle = INVALID_VEHICLE;
	st->facilities = FACIL_AIRPORT | FACIL_DOCK;
	st->build_date = _date;

	for (j = 0; j != NUM_CARGO; j++) {
		st->goods[j].waiting_acceptance = 0;
		st->goods[j].days_since_pickup = 0;
		st->goods[j].enroute_from = INVALID_STATION;
		st->goods[j].rating = 175;
		st->goods[j].last_speed = 0;
		st->goods[j].last_age = 255;
	}

	UpdateStationVirtCoordDirty(st);
	UpdateStationAcceptance(st, false);
}

void DeleteOilRig(TileIndex tile)
{
	Station *st = GetStation(_m[tile].m2);

	DoClearSquare(tile);

	st->dock_tile = 0;
	st->airport_tile = 0;
	st->facilities &= ~(FACIL_AIRPORT | FACIL_DOCK);
	st->airport_flags = 0;
	UpdateStationVirtCoordDirty(st);
	DeleteStation(st);
}

static void ChangeTileOwner_Station(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != OWNER_SPECTATOR) {
		Station *st = GetStation(_m[tile].m2);
		SetTileOwner(tile, new_player);
		st->owner = new_player;
		_global_station_sort_dirty = true; // transfer ownership of station to another player
		InvalidateWindowClasses(WC_STATION_LIST);
	} else {
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static int32 ClearTile_Station(TileIndex tile, byte flags)
{
	byte m5 = _m[tile].m5;
	Station *st;

	if (flags & DC_AUTO) {
		if (m5 < 8) return_cmd_error(STR_300B_MUST_DEMOLISH_RAILROAD);
		if (m5 < 0x43 || (m5 >= 83 && m5 <= 114)) return_cmd_error(STR_300E_MUST_DEMOLISH_AIRPORT_FIRST);
		if (m5 < 0x47) return_cmd_error(STR_3047_MUST_DEMOLISH_TRUCK_STATION);
		if (m5 < 0x4B) return_cmd_error(STR_3046_MUST_DEMOLISH_BUS_STATION);
		if (m5 == 0x52) return_cmd_error(STR_306A_BUOY_IN_THE_WAY);
		if (m5 != 0x4B && m5 < 0x53) return_cmd_error(STR_304D_MUST_DEMOLISH_DOCK_FIRST);
		SetDParam(0, STR_4807_OIL_RIG);
		return_cmd_error(STR_4800_IN_THE_WAY);
	}

	st = GetStation(_m[tile].m2);

	if (m5 < 8) return RemoveRailroadStation(st, tile, flags);
	// original airports < 67, new airports between 83 - 114
	if (m5 < 0x43 || (m5 >= 83 && m5 <= 114)) return RemoveAirport(st, flags);
	if (m5 < 0x4B) return RemoveRoadStop(st, flags, tile);
	if (m5 == 0x52) return RemoveBuoy(st, flags);
	if (m5 != 0x4B && m5 < 0x53) return RemoveDock(st, flags);

	return CMD_ERROR;
}

void InitializeStations(void)
{
	/* Clean the station pool and create 1 block in it */
	CleanPool(&_station_pool);
	AddBlockToPool(&_station_pool);

	/* Clean the roadstop pool and create 1 block in it */
	CleanPool(&_roadstop_pool);
	AddBlockToPool(&_roadstop_pool);

	_station_tick_ctr = 0;

	// set stations to be sorted on load of savegame
	memset(_station_sort_dirty, true, sizeof(_station_sort_dirty));
	_global_station_sort_dirty = true; // load of savegame
}


const TileTypeProcs _tile_type_station_procs = {
	DrawTile_Station,						/* draw_tile_proc */
	GetSlopeZ_Station,					/* get_slope_z_proc */
	ClearTile_Station,					/* clear_tile_proc */
	GetAcceptedCargo_Station,		/* get_accepted_cargo_proc */
	GetTileDesc_Station,				/* get_tile_desc_proc */
	GetTileTrackStatus_Station,	/* get_tile_track_status_proc */
	ClickTile_Station,					/* click_tile_proc */
	AnimateTile_Station,				/* animate_tile_proc */
	TileLoop_Station,						/* tile_loop_clear */
	ChangeTileOwner_Station,		/* change_tile_owner_clear */
	NULL,												/* get_produced_cargo_proc */
	VehicleEnter_Station,				/* vehicle_enter_tile_proc */
	NULL,												/* vehicle_leave_tile_proc */
	GetSlopeTileh_Station,			/* get_slope_tileh_proc */
};

static const SaveLoad _roadstop_desc[] = {
	SLE_VAR(RoadStop,xy,           SLE_UINT32),
	SLE_VAR(RoadStop,used,         SLE_UINT8),
	SLE_VAR(RoadStop,status,       SLE_UINT8),
	/* Index was saved in some versions, but this is not needed */
	SLE_CONDARR(NullStruct,null,SLE_FILE_U32 | SLE_VAR_NULL, 1, 0, 8),
	SLE_VAR(RoadStop,station,      SLE_UINT16),
	SLE_VAR(RoadStop,type,         SLE_UINT8),

	SLE_REF(RoadStop,next,         REF_ROADSTOPS),
	SLE_REF(RoadStop,prev,         REF_ROADSTOPS),

	SLE_ARR(RoadStop,slot,         SLE_UINT16, NUM_SLOTS),

	SLE_END()
};

static const SaveLoad _station_desc[] = {
	SLE_CONDVAR(Station, xy,           SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Station, xy,           SLE_UINT32, 6, 255),
	SLE_CONDVAR(Station, bus_tile_obsolete,    SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Station, lorry_tile_obsolete,  SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Station, train_tile,   SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Station, train_tile,   SLE_UINT32, 6, 255),
	SLE_CONDVAR(Station, airport_tile, SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Station, airport_tile, SLE_UINT32, 6, 255),
	SLE_CONDVAR(Station, dock_tile,    SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Station, dock_tile,    SLE_UINT32, 6, 255),
	SLE_REF(Station,town,						REF_TOWN),
	SLE_VAR(Station,trainst_w,			SLE_UINT8),
	SLE_CONDVAR(Station,trainst_h,	SLE_UINT8, 2, 255),

	// alpha_order was stored here in savegame format 0 - 3
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 1, 0, 3),

	SLE_VAR(Station,string_id,			SLE_STRINGID),
	SLE_VAR(Station,had_vehicle_of_type,SLE_UINT16),

	SLE_VAR(Station,time_since_load,		SLE_UINT8),
	SLE_VAR(Station,time_since_unload,	SLE_UINT8),
	SLE_VAR(Station,delete_ctr,					SLE_UINT8),
	SLE_VAR(Station,owner,							SLE_UINT8),
	SLE_VAR(Station,facilities,					SLE_UINT8),
	SLE_VAR(Station,airport_type,				SLE_UINT8),

	// truck/bus_stop_status was stored here in savegame format 0 - 6
	SLE_CONDVAR(Station,truck_stop_status_obsolete,	SLE_UINT8, 0, 5),
	SLE_CONDVAR(Station,bus_stop_status_obsolete,		SLE_UINT8, 0, 5),

	// blocked_months was stored here in savegame format 0 - 4.0
	SLE_CONDVAR(Station,blocked_months_obsolete,	SLE_UINT8, 0, 4),

	SLE_CONDVAR(Station,airport_flags,			SLE_VAR_U32 | SLE_FILE_U16, 0, 2),
	SLE_CONDVAR(Station,airport_flags,			SLE_UINT32, 3, 255),

	SLE_VAR(Station,last_vehicle,				SLE_UINT16),

	SLE_CONDVAR(Station,class_id,				SLE_UINT8, 3, 255),
	SLE_CONDVAR(Station,stat_id,				SLE_UINT8, 3, 255),
	SLE_CONDVAR(Station,build_date,			SLE_UINT16, 3, 255),

	SLE_CONDREF(Station,bus_stops,					REF_ROADSTOPS, 6, 255),
	SLE_CONDREF(Station,truck_stops,				REF_ROADSTOPS, 6, 255),

	// reserve extra space in savegame here. (currently 28 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 32, 2, 255),

	SLE_END()
};

static const SaveLoad _goods_desc[] = {
	SLE_VAR(GoodsEntry,waiting_acceptance,SLE_UINT16),
	SLE_VAR(GoodsEntry,days_since_pickup,	SLE_UINT8),
	SLE_VAR(GoodsEntry,rating,						SLE_UINT8),
	SLE_CONDVAR(GoodsEntry,enroute_from,			SLE_FILE_U8 | SLE_VAR_U16, 0, 6),
	SLE_CONDVAR(GoodsEntry,enroute_from,			SLE_UINT16, 7, 255),
	SLE_VAR(GoodsEntry,enroute_time,			SLE_UINT8),
	SLE_VAR(GoodsEntry,last_speed,				SLE_UINT8),
	SLE_VAR(GoodsEntry,last_age,					SLE_UINT8),
	SLE_CONDVAR(GoodsEntry,feeder_profit,			SLE_INT32, 14, 255),

	SLE_END()
};


static void SaveLoad_STNS(Station *st)
{
	int i;

	SlObject(st, _station_desc);
	for (i = 0; i != NUM_CARGO; i++) {
		SlObject(&st->goods[i], _goods_desc);

		/* In older versions, enroute_from had 0xFF as INVALID_STATION, is now 0xFFFF */
		if (CheckSavegameVersion(7) && st->goods[i].enroute_from == 0xFF) {
			st->goods[i].enroute_from = INVALID_STATION;
		}
	}
}

static void Save_STNS(void)
{
	Station *st;
	// Write the stations
	FOR_ALL_STATIONS(st) {
		if (st->xy != 0) {
			SlSetArrayIndex(st->index);
			SlAutolength((AutolengthProc*)SaveLoad_STNS, st);
		}
	}
}

static void Load_STNS(void)
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Station *st;

		if (!AddBlockIfNeeded(&_station_pool, index))
			error("Stations: failed loading savegame: too many stations");

		st = GetStation(index);
		SaveLoad_STNS(st);

		// this means it's an oldstyle savegame without support for nonuniform stations
		if (st->train_tile != 0 && st->trainst_h == 0) {
			int w = GB(st->trainst_w, 4, 4);
			int h = GB(st->trainst_w, 0, 4);

			if (_m[st->train_tile].m5 & 1) intswap(w, h);
			st->trainst_w = w;
			st->trainst_h = h;
		}

		/* In older versions, we had just 1 tile for a bus/lorry, now we have more..
		 *  convert, if needed */
		if (CheckSavegameVersion(6)) {
			if (st->bus_tile_obsolete != 0) {
				st->bus_stops = AllocateRoadStop();
				if (st->bus_stops == NULL)
					error("Station: too many busstations in savegame");

				InitializeRoadStop(st->bus_stops, NULL, st->bus_tile_obsolete, st->index);
			}
			if (st->lorry_tile_obsolete != 0) {
				st->truck_stops = AllocateRoadStop();
				if (st->truck_stops == NULL)
					error("Station: too many truckstations in savegame");

				InitializeRoadStop(st->truck_stops, NULL, st->lorry_tile_obsolete, st->index);
			}
		}
	}

	/* This is to ensure all pointers are within the limits of _stations_size */
	if (_station_tick_ctr > GetStationPoolSize()) _station_tick_ctr = 0;
}

static void Save_ROADSTOP(void)
{
	RoadStop *rs;

	FOR_ALL_ROADSTOPS(rs) {
		if (rs->used) {
			SlSetArrayIndex(rs->index);
			SlObject(rs, _roadstop_desc);
		}
	}
}

static void Load_ROADSTOP(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		RoadStop *rs;

		if (!AddBlockIfNeeded(&_roadstop_pool, index))
			error("RoadStops: failed loading savegame: too many RoadStops");

		rs = GetRoadStop(index);
		SlObject(rs, _roadstop_desc);
	}
}

const ChunkHandler _station_chunk_handlers[] = {
	{ 'STNS', Save_STNS,      Load_STNS,      CH_ARRAY },
	{ 'ROAD', Save_ROADSTOP,  Load_ROADSTOP,  CH_ARRAY | CH_LAST},
};
