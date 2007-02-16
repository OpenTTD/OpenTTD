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
#include "misc/autoptr.hpp"
#include "road.h"

/**
 * Called if a new block is added to the station-pool
 */
static void StationPoolNewBlock(uint start_item)
{
	Station *st;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 *  TODO - This is just a temporary stage, this will be removed. */
	for (st = GetStation(start_item); st != NULL; st = (st->index + 1U < GetStationPoolSize()) ? GetStation(st->index + 1U) : NULL) st->index = start_item++;
}

static void StationPoolCleanBlock(uint start_item, uint end_item)
{
	uint i;

	for (i = start_item; i <= end_item; i++) {
		Station *st = GetStation(i);
		if (st->IsValid()) st->~Station();
	}
}

/**
 * Called if a new block is added to the roadstop-pool
 */
static void RoadStopPoolNewBlock(uint start_item)
{
	RoadStop *rs;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (rs = GetRoadStop(start_item); rs != NULL; rs = (rs->index + 1U < GetRoadStopPoolSize()) ? GetRoadStop(rs->index + 1U) : NULL) {
		rs->xy    = INVALID_TILE;
		rs->index = start_item++;
	}
}

DEFINE_OLD_POOL(Station, Station, StationPoolNewBlock, StationPoolCleanBlock)
DEFINE_OLD_POOL(RoadStop, RoadStop, RoadStopPoolNewBlock, NULL)


extern void UpdateAirplanesOnNewStation(Station *st);


RoadStop* GetRoadStopByTile(TileIndex tile, RoadStop::Type type)
{
	const Station* st = GetStationByTile(tile);
	RoadStop* rs;

	for (rs = st->GetPrimaryRoadStop(type); rs->xy != tile; rs = rs->next) {
		assert(rs->next != NULL);
	}

	return rs;
}


static uint GetNumRoadStopsInStation(const Station* st, RoadStop::Type type)
{
	uint num = 0;
	const RoadStop *rs;

	assert(st != NULL);
	for (rs = st->GetPrimaryRoadStop(type); rs != NULL; rs = rs->next) num++;

	return num;
}


/* Calculate the radius of the station. Basicly it is the biggest
 *  radius that is available within the station */
static uint FindCatchmentRadius(const Station* st)
{
	CatchmentAera ret = CA_NONE;

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
			case AT_COMMUTER:      ret = max(ret, CA_AIR_COMMUTER); break;
			case AT_HELIDEPOT:     ret = max(ret, CA_AIR_HELIDEPOT); break;
			case AT_INTERCON:      ret = max(ret, CA_AIR_INTERCON); break;
			case AT_HELISTATION:   ret = max(ret, CA_AIR_HELISTATION); break;
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
			StationID t = GetStationIndex(tile_cur);
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

static Station *AllocateStation(void)
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
	if (AddBlockToPool(&_Station_pool)) return AllocateStation();

	_error_message = STR_3008_TOO_MANY_STATIONS_LOADING;
	return NULL;
}


/**
 * Counts the numbers of tiles matching a specific type in the area around
 * @param tile the center tile of the 'count area'
 * @param type the type of tile searched for
 * @param industry when type == MP_INDUSTRY, the type of the industry,
 *                 in all other cases this parameter is ignored
 * @result the noumber of matching tiles around
 */
static int CountMapSquareAround(TileIndex tile, TileType type, IndustryType industry)
{
	TileIndex cur_tile;
	int dx, dy;
	int num = 0;

	for (dx = -3; dx <= 3; dx++) {
		for (dy = -3; dy <= 3; dy++) {
			cur_tile = TILE_MASK(tile + TileDiffXY(dx, dy));

			if (IsTileType(cur_tile, type)) {
				switch (type) {
					case MP_INDUSTRY:
						if (GetIndustryType(cur_tile) == industry)
							num++;
						break;

					case MP_WATER:
						if (!IsWater(cur_tile))
							break;
						/* FALL THROUGH WHEN WATER TILE */
					case MP_TREES:
						num++;
						break;

					default:
						break;
				}
			}
		}
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
			if (s != st && s->town==t) {
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
		if (CountMapSquareAround(tile, MP_INDUSTRY, IT_COAL_MINE) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, IT_IRON_MINE) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, IT_COPPER_MINE) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, IT_GOLD_MINE) >= 2 ||
				CountMapSquareAround(tile, MP_INDUSTRY, IT_DIAMOND_MINE) >= 2) {
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
			CountMapSquareAround(tile, MP_WATER, 0) >= 5) {
		found = M(STR_SV_STNAME_LAKESIDE);
		goto done;
	}

	/* Check woods */
	if (HASBIT(free_names, M(STR_SV_STNAME_WOODS)) && (
				CountMapSquareAround(tile, MP_TREES, 0) >= 8 ||
				CountMapSquareAround(tile, MP_INDUSTRY, IT_FOREST) >= 2)
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
		if ((owner == PLAYER_SPECTATOR || st->owner == owner)) {
			uint cur_dist = DistanceManhattan(tile, st->xy);

			if (cur_dist < threshold) {
				threshold = cur_dist;
				best_station = st;
			}
		}
	}

	return best_station;
}

// Update the virtual coords needed to draw the station sign.
// st = Station to update for.
static void UpdateStationVirtCoord(Station *st)
{
	Point pt = RemapCoords2(TileX(st->xy) * TILE_SIZE, TileY(st->xy) * TILE_SIZE);

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
		UpdateStationVirtCoord(st);
	}
}

// Update the station virt coords while making the modified parts dirty.
static void UpdateStationVirtCoordDirty(Station *st)
{
	st->MarkDirty();
	UpdateStationVirtCoord(st);
	st->MarkDirty();
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
static void ShowRejectOrAcceptNews(const Station *st, uint num_items, CargoID *cargo, StringID msg)
{
	for (uint i = 0; i < num_items; i++) {
		SetDParam(i + 1, _cargoc.names_s[cargo[i]]);
	}

	SetDParam(0, st->index);
	AddNewsItem(msg, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_TILE, NT_ACCEPTANCE, 0), st->xy, 0);
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
					CargoID cargos[2] = { CT_INVALID, CT_INVALID };

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

typedef struct ottd_Rectangle {
	uint min_x;
	uint min_y;
	uint max_x;
	uint max_y;
} ottd_Rectangle;

static inline void MergePoint(ottd_Rectangle* rect, TileIndex tile)
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
	ottd_Rectangle rect;
	int rad;
	AcceptedCargo accepts;

	rect.min_x = MapSizeX();
	rect.min_y = MapSizeY();
	rect.max_x = rect.max_y = 0;
	// Don't update acceptance for a buoy
	if (st->IsBuoy()) return;

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
		const AirportFTAClass* afc = st->Airport();

		MergePoint(&rect, st->airport_tile);
		MergePoint(&rect,
			st->airport_tile + TileDiffXY(afc->size_x - 1, afc->size_y - 1)
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
		/* List of accept and reject strings for different number of
		 * cargo types */
		static const StringID accept_msg[] = {
			STR_3040_NOW_ACCEPTS,
			STR_3041_NOW_ACCEPTS_AND,
		};
		static const StringID reject_msg[] = {
			STR_303E_NO_LONGER_ACCEPTS,
			STR_303F_NO_LONGER_ACCEPTS_OR,
		};

		/* Array of accepted and rejected cargo types */
		CargoID accepts[2] = { CT_INVALID, CT_INVALID };
		CargoID rejects[2] = { CT_INVALID, CT_INVALID };
		uint num_acc = 0;
		uint num_rej = 0;

		/* Test each cargo type to see if its acceptange has changed */
		for (CargoID i = 0; i < NUM_CARGO; i++) {
			if (HASBIT(new_acc, i)) {
				if (!HASBIT(old_acc, i) && num_acc < lengthof(accepts)) {
					/* New cargo is accepted */
					accepts[num_acc++] = i;
				}
			} else {
				if (HASBIT(old_acc, i) && num_rej < lengthof(rejects)) {
					/* Old cargo is no longer accepted */
					rejects[num_rej++] = i;
				}
			}
		}

		/* Show news message if there are any changes */
		if (num_acc > 0) ShowRejectOrAcceptNews(st, num_acc, accepts, accept_msg[num_acc - 1]);
		if (num_rej > 0) ShowRejectOrAcceptNews(st, num_rej, rejects, reject_msg[num_rej - 1]);
	}

	// redraw the station view since acceptance changed
	InvalidateWindowWidget(WC_STATION_VIEW, st->index, 4);
}

static void UpdateStationSignCoord(Station *st)
{
	StationRect *r = &st->rect;

	if (r->IsEmpty()) return; // no tiles belong to this station

	// clamp sign coord to be inside the station rect
	st->xy = TileXY(clampu(TileX(st->xy), r->left, r->right), clampu(TileY(st->xy), r->top, r->bottom));
	UpdateStationVirtCoordDirty(st);
}

// This is called right after a station was deleted.
// It checks if the whole station is free of substations, and if so, the station will be
// deleted after a little while.
static void DeleteStationIfEmpty(Station* st)
{
	if (st->facilities == 0) {
		st->delete_ctr = 0;
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
	/* station remains but it probably lost some parts - station sign should stay in the station boundaries */
	UpdateStationSignCoord(st);
}

static int32 ClearTile_Station(TileIndex tile, byte flags);

// Tries to clear the given area. Returns the cost in case of success.
// Or an error code if it failed.
int32 CheckFlatLandBelow(TileIndex tile, uint w, uint h, uint flags, uint invalid_dirs, StationID* station)
{
	int32 cost = 0, ret;

	Slope tileh;
	uint z;
	int allowed_z = -1;
	int flat_z;

	BEGIN_TILE_LOOP(tile_cur, w, h, tile)
		if (MayHaveBridgeAbove(tile_cur) && IsBridgeAbove(tile_cur)) {
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		}

		if (!EnsureNoVehicle(tile_cur)) return CMD_ERROR;

		tileh = GetTileSlope(tile_cur, &z);

		/* Prohibit building if
		 *   1) The tile is "steep" (i.e. stretches two height levels)
		 * -OR-
		 *   2) The tile is non-flat if
		 *     a) the player building is an "old-school" AI
		 *   -OR-
		 *     b) the build_on_slopes switch is disabled
		 */
		if (IsSteepSlope(tileh) ||
				((_is_old_ai_player || !_patches.build_on_slopes) && tileh != SLOPE_FLAT)) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
		}

		flat_z = z;
		if (tileh != SLOPE_FLAT) {
			// need to check so the entrance to the station is not pointing at a slope.
			if ((invalid_dirs&1 && !(tileh & SLOPE_NE) && (uint)w_cur == w) ||
					(invalid_dirs&2 && !(tileh & SLOPE_SE) && h_cur == 1) ||
					(invalid_dirs&4 && !(tileh & SLOPE_SW) && w_cur == 1) ||
					(invalid_dirs&8 && !(tileh & SLOPE_NW) && (uint)h_cur == h)) {
				return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
			}
			cost += _price.terraform;
			flat_z += TILE_HEIGHT;
		}

		// get corresponding flat level and make sure that all parts of the station have the same level.
		if (allowed_z == -1) {
			// first tile
			allowed_z = flat_z;
		} else if (allowed_z != flat_z) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
		}

		// if station is set, then we have special handling to allow building on top of already existing stations.
		// so station points to INVALID_STATION if we can build on any station. or it points to a station if we're only allowed to build
		// on exactly that station.
		if (station != NULL && IsTileType(tile_cur, MP_STATION)) {
			if (!IsRailwayStation(tile_cur)) {
				return ClearTile_Station(tile_cur, DC_AUTO); // get error message
			} else {
				StationID st = GetStationIndex(tile_cur);
				if (*station == INVALID_STATION) {
					*station = st;
				} else if (*station != st) {
					return_cmd_error(STR_3006_ADJOINS_MORE_THAN_ONE_EXISTING);
				}
			}
		} else {
			ret = DoCommand(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost += ret;
		}
	END_TILE_LOOP(tile_cur, w, h, tile)

	return cost;
}

static bool CanExpandRailroadStation(Station* st, uint* fin, Axis axis)
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
		// check so the orientation is the same
		if (GetRailStationAxis(st->train_tile) != axis) {
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

static void GetStationLayout(byte *layout, int numtracks, int plat_len, const StationSpec *statspec)
{
	if (statspec != NULL && statspec->lengths >= plat_len &&
			statspec->platforms[plat_len - 1] >= numtracks &&
			statspec->layouts[plat_len - 1][numtracks - 1]) {
		/* Custom layout defined, follow it. */
		memcpy(layout, statspec->layouts[plat_len - 1][numtracks - 1],
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
 * @param tile_org starting position of station dragging/placement
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0)    - orientation (p1 & 1)
 * - p1 = (bit  8-15) - number of tracks
 * - p1 = (bit 16-23) - platform length
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 3) - railtype (p2 & 0xF)
 * - p2 = (bit  8-15) - custom station class
 * - p2 = (bit 16-23) - custom station id
 */
int32 CmdBuildRailroadStation(TileIndex tile_org, uint32 flags, uint32 p1, uint32 p2)
{
	Station *st;
	int w_org, h_org;
	int32 cost, ret;
	StationID est;
	int plat_len, numtracks;
	Axis axis;
	uint finalvalues[3];
	const StationSpec *statspec;
	int specindex;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Does the authority allow this? */
	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile_org)) return CMD_ERROR;
	if (!ValParamRailtype(p2 & 0xF)) return CMD_ERROR;

	/* unpack parameters */
	axis = (Axis)(p1 & 1);
	numtracks = GB(p1,  8, 8);
	plat_len  = GB(p1, 16, 8);
	/* w = length, h = num_tracks */
	if (axis == AXIS_X) {
		w_org = plat_len;
		h_org = numtracks;
	} else {
		h_org = plat_len;
		w_org = numtracks;
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
	ret = CheckFlatLandBelow(tile_org, w_org, h_org, flags & ~DC_EXEC, 5 << axis, _patches.nonuniform_stations ? &est : NULL);
	if (CmdFailed(ret)) return ret;
	cost = ret + (numtracks * _price.train_station_track + _price.train_station_length) * plat_len;

	// Make sure there are no similar stations around us.
	st = GetStationAround(tile_org, w_org, h_org, est);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	// See if there is a deleted station close to us.
	if (st == NULL) {
		st = GetClosestStationFromTile(tile_org, 8, _current_player);
		if (st != NULL && st->facilities) st = NULL;
	}

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	 * to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		// Reuse an existing station.
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (st->train_tile != 0) {
			// check if we want to expanding an already existing station?
			if (_is_old_ai_player || !_patches.join_stations)
				return_cmd_error(STR_3005_TOO_CLOSE_TO_ANOTHER_RAILROAD);
			if (!CanExpandRailroadStation(st, finalvalues, axis))
				return CMD_ERROR;
		}

		//XXX can't we pack this in the "else" part of the if above?
		if (!st->rect.BeforeAddRect(tile_org, w_org, h_org, StationRect::ADD_TEST)) return CMD_ERROR;
	} else {
		/* allocate and initialize new station */
		st = new Station(tile_org);
		if (st == NULL) return CMD_ERROR;

		/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
		st_auto_delete = st;

		st->town = ClosestTownFromTile(tile_org, (uint)-1);
		if (!GenerateStationName(st, tile_org, 0)) return CMD_ERROR;

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SETBIT(st->town->have_ratings, _current_player);
		}
	}

	/* Check if the given station class is valid */
	if (GB(p2, 8, 8) >= STAT_CLASS_MAX) return CMD_ERROR;

	/* Check if we can allocate a custom stationspec to this station */
	statspec = GetCustomStationSpec((StationClassID)GB(p2, 8, 8), GB(p2, 16, 8));
	specindex = AllocateSpecToStation(statspec, st, flags & DC_EXEC);
	if (specindex == -1) return CMD_ERROR;

	if (statspec != NULL) {
		/* Perform NewStation checks */

		/* Check if the station size is permitted */
		if (HASBIT(statspec->disallowed_platforms, numtracks - 1) || HASBIT(statspec->disallowed_lengths, plat_len - 1)) {
			return CMD_ERROR;
		}

		/* Check if the station is buildable */
		if (HASBIT(statspec->callbackmask, CBM_STATION_AVAIL) && GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE) == 0) {
			return CMD_ERROR;
		}
	}

	if (flags & DC_EXEC) {
		TileIndexDiff tile_delta;
		byte *layout_ptr;
		byte numtracks_orig;
		Track track;

		// Now really clear the land below the station
		// It should never return CMD_ERROR.. but you never know ;)
		//  (a bit strange function name for it, but it really does clear the land, when DC_EXEC is in flags)
		ret = CheckFlatLandBelow(tile_org, w_org, h_org, flags, 5 << axis, _patches.nonuniform_stations ? &est : NULL);
		if (CmdFailed(ret)) return ret;

		st->train_tile = finalvalues[0];
		st->AddFacility(FACIL_TRAIN, finalvalues[0]);

		st->trainst_w = finalvalues[1];
		st->trainst_h = finalvalues[2];

		st->rect.BeforeAddRect(tile_org, w_org, h_org, StationRect::ADD_TRY);

		tile_delta = (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
		track = AxisToTrack(axis);

		layout_ptr = (byte*)alloca(numtracks * plat_len);
		GetStationLayout(layout_ptr, numtracks, plat_len, statspec);

		numtracks_orig = numtracks;

		do {
			TileIndex tile = tile_org;
			int w = plat_len;
			do {
				byte layout = *layout_ptr++;
				MakeRailStation(tile, st->owner, st->index, axis, layout, (RailType)GB(p2, 0, 4));
				SetCustomStationSpecIndex(tile, specindex);
				SetStationTileRandomBits(tile, GB(Random(), 0, 4));

				if (statspec != NULL) {
					/* Use a fixed axis for GetPlatformInfo as our platforms / numtracks are always the right way around */
					uint32 platinfo = GetPlatformInfo(AXIS_X, 0, plat_len, numtracks_orig, plat_len - w, numtracks_orig - numtracks, false);
					uint16 callback = GetStationCallback(CBID_STATION_TILE_LAYOUT, platinfo, 0, statspec, st, tile);
					if (callback != CALLBACK_FAILED && callback < 8) SetStationGfx(tile, callback + axis);
				}

				tile += tile_delta;
			} while (--w);
			SetSignalsOnBothDir(tile_org, track);
			YapfNotifyTrackLayoutChange(tile_org, track);
			tile_org += tile_delta ^ TileDiffXY(1, 1); // perpendicular to tile_delta
		} while (--numtracks);

		st->MarkTilesDirty();
		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		/* success, so don't delete the new station */
		st_auto_delete.Release();
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
		for (i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(0, i));) {
			// the left side is unused?
			if (++i == h) {
				tile += TileDiffXY(1, 0);
				w--;
				goto restart;
			}
		}

		// check the right side, x = constant, y changes
		for (i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(w - 1, i));) {
			// the right side is unused?
			if (++i == h) {
				w--;
				goto restart;
			}
		}

		// check the upper side, y = constant, x changes
		for (i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(i, 0));) {
			// the left side is unused?
			if (++i == w) {
				tile += TileDiffXY(0, 1);
				h--;
				goto restart;
			}
		}

		// check the lower side, y = constant, x changes
		for (i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(i, h - 1));) {
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
 * @param tile tile of station piece to remove
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdRemoveFromRailroadStation(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// make sure the specified tile belongs to the current player, and that it is a railroad station.
	if (!IsTileType(tile, MP_STATION) || !IsRailwayStation(tile) || !_patches.nonuniform_stations) return CMD_ERROR;
	st = GetStationByTile(tile);
	if (_current_player != OWNER_WATER && (!CheckOwnership(st->owner) || !EnsureNoVehicle(tile))) return CMD_ERROR;

	// if we reached here, it means we can actually delete it. do that.
	if (flags & DC_EXEC) {
		uint specindex = GetCustomStationSpecIndex(tile);
		Track track = GetRailStationTrack(tile);
		DoClearSquare(tile);
		st->rect.AfterRemoveTile(st, tile);
		SetSignalsOnBothDir(tile, track);
		YapfNotifyTrackLayoutChange(tile, track);

		DeallocateSpecFromStation(st, specindex);

		// now we need to make the "spanned" area of the railway station smaller if we deleted something at the edges.
		// we also need to adjust train_tile.
		MakeRailwayStationAreaSmaller(st);
		st->MarkTilesDirty();
		UpdateStationSignCoord(st);

		// if we deleted the whole station, delete the train facility.
		if (st->train_tile == 0) {
			st->facilities &= ~FACIL_TRAIN;
			UpdateStationVirtCoordDirty(st);
			DeleteStationIfEmpty(st);
		}
	}
	return _price.remove_rail_station;
}


static int32 RemoveRailroadStation(Station *st, TileIndex tile, uint32 flags)
{
	int w,h;
	int32 cost = 0;

	/* if there is flooding and non-uniform stations are enabled, remove platforms tile by tile */
	if (_current_player == OWNER_WATER && _patches.nonuniform_stations)
		return DoCommand(tile, 0, 0, DC_EXEC, CMD_REMOVE_FROM_RAILROAD_STATION);

	/* Current player owns the station? */
	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	/* determine width and height of platforms */
	tile = st->train_tile;
	w = st->trainst_w;
	h = st->trainst_h;

	assert(w != 0 && h != 0);

	/* clear all areas of the station */
	do {
		int w_bak = w;
		do {
			// for nonuniform stations, only remove tiles that are actually train station tiles
			if (st->TileBelongsToRailStation(tile)) {
				if (!EnsureNoVehicle(tile))
					return CMD_ERROR;
				cost += _price.remove_rail_station;
				if (flags & DC_EXEC) {
					Track track = GetRailStationTrack(tile);
					DoClearSquare(tile);
					SetSignalsOnBothDir(tile, track);
					YapfNotifyTrackLayoutChange(tile, track);
				}
			}
			tile += TileDiffXY(1, 0);
		} while (--w);
		w = w_bak;
		tile += TileDiffXY(-w, 1);
	} while (--h);

	if (flags & DC_EXEC) {
		st->rect.AfterRemoveRect(st, st->train_tile, st->trainst_w, st->trainst_h);

		st->train_tile = 0;
		st->trainst_w = st->trainst_h = 0;
		st->facilities &= ~FACIL_TRAIN;

		free(st->speclist);
		st->num_specs = 0;
		st->speclist  = NULL;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

int32 DoConvertStationRail(TileIndex tile, RailType totype, bool exec)
{
	const Station* st = GetStationByTile(tile);

	if (!CheckOwnership(st->owner) || !EnsureNoVehicle(tile)) return CMD_ERROR;

	// tile is not a railroad station?
	if (!IsRailwayStation(tile)) return CMD_ERROR;

	if (GetRailType(tile) == totype) return CMD_ERROR;

	// 'hidden' elrails can't be downgraded to normal rail when elrails are disabled
	if (_patches.disable_elrails && totype == RAILTYPE_RAIL && GetRailType(tile) == RAILTYPE_ELECTRIC) return CMD_ERROR;

	if (exec) {
		SetRailType(tile, totype);
		MarkTileDirtyByTile(tile);
		YapfNotifyTrackLayoutChange(tile, GetRailStationTrack(tile));
	}

	return _price.build_rail >> 1;
}

/**
 * @param[in] truck_station Determines whether a stop is RoadStop::BUS or RoadStop::TRUCK
 * @param[in] station The station to do the whole procedure for
 * @return a pointer to where to link a new RoadStop*
 */
static RoadStop **FindRoadStopSpot(bool truck_station, Station* st)
{
	RoadStop **primary_stop = (truck_station) ? &st->truck_stops : &st->bus_stops;

	if (*primary_stop == NULL) {
		//we have no roadstop of the type yet, so write a "primary stop"
		return primary_stop;
	} else {
		//there are stops already, so append to the end of the list
		RoadStop *stop = *primary_stop;
		while (stop->next != NULL) stop = stop->next;
		return &stop->next;
	}
}

/** Build a bus or truck stop
 * @param tile tile to build the stop at
 * @param p1 entrance direction (DiagDirection)
 * @param p2 bit 0: 0 for Bus stops, 1 for truck stops
 *           bit 1: 0 for normal, 1 for drive-through
 */
int32 CmdBuildRoadStop(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Station *st;
	RoadStop *road_stop;
	int32 cost;
	int32 ret;
	bool type = HASBIT(p2, 0);
	bool is_drive_through = HASBIT(p2, 1);
	bool build_over_road  = is_drive_through && IsTileType(tile, MP_STREET) && GetRoadTileType(tile) == ROAD_TILE_NORMAL;
	Owner cur_owner = _current_player;

	/* Saveguard the parameters */
	if (!IsValidDiagDirection((DiagDirection)p1)) return CMD_ERROR;
	/* If it is a drive-through stop check for valid axis */
	if (is_drive_through && !IsValidAxis((Axis)p1)) return CMD_ERROR;
	/* Road bits in the wrong direction */
	if (build_over_road && (GetRoadBits(tile) & ((Axis)p1 == AXIS_X ? ROAD_Y : ROAD_X)) != 0) return CMD_ERROR;
	/* Not allowed to build over this road */
	if (build_over_road && !IsTileOwner(tile, _current_player) && !(IsTileOwner(tile, OWNER_TOWN) && _patches.road_stop_on_town_road)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	if (build_over_road && IsTileOwner(tile, OWNER_TOWN)) _current_player = OWNER_TOWN;
	ret = CheckFlatLandBelow(tile, 1, 1, flags, is_drive_through ? 5 << p1 : 1 << p1, NULL);
	_current_player = cur_owner;
	if (CmdFailed(ret)) return ret;
	cost = build_over_road ? 0 : ret; // Don't add cost of clearing road when overbuilding

	st = GetStationAround(tile, 1, 1, INVALID_STATION);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st != NULL && st->facilities != 0) st = NULL;
	}

	//give us a road stop in the list, and check if something went wrong
	road_stop = new RoadStop(tile);
	if (road_stop == NULL) {
		return_cmd_error(type ? STR_3008B_TOO_MANY_TRUCK_STOPS : STR_3008A_TOO_MANY_BUS_STOPS);
	}

	/* ensure that in case of error (or no DC_EXEC) the new road stop gets deleted upon return */
	AutoPtrT<RoadStop> rs_auto_delete(road_stop);

	if (st != NULL &&
			GetNumRoadStopsInStation(st, RoadStop::BUS) + GetNumRoadStopsInStation(st, RoadStop::TRUCK) >= RoadStop::LIMIT) {
		return_cmd_error(type ? STR_3008B_TOO_MANY_TRUCK_STOPS : STR_3008A_TOO_MANY_BUS_STOPS);
	}

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	* to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player) {
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);
		}

		if (!st->rect.BeforeAddTile(tile, StationRect::ADD_TEST)) return CMD_ERROR;
	} else {
		/* allocate and initialize new station */
		st = new Station(tile);
		if (st == NULL) return CMD_ERROR;

		/* ensure that in case of error (or no DC_EXEC) the new station gets deleted upon return */
		st_auto_delete = st;


		Town *t = st->town = ClosestTownFromTile(tile, (uint)-1);
		if (!GenerateStationName(st, tile, 0)) return CMD_ERROR;

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SETBIT(t->have_ratings, _current_player);
		}

		st->sign.width_1 = 0;
	}

	cost += (type) ? _price.build_truck_station : _price.build_bus_station;

	if (flags & DC_EXEC) {
		// Insert into linked list of RoadStops
		RoadStop **currstop = FindRoadStopSpot(type, st);
		*currstop = road_stop;

		//initialize an empty station
		st->AddFacility((type) ? FACIL_TRUCK_STOP : FACIL_BUS_STOP, tile);

		st->rect.BeforeAddTile(tile, StationRect::ADD_TRY);

		MakeRoadStop(tile, st->owner, st->index, type ? RoadStop::TRUCK : RoadStop::BUS, is_drive_through, (DiagDirection)p1);
		if (is_drive_through & HASBIT(p2, 3)) SetStopBuiltOnTownRoad(tile);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		/* success, so don't delete the new station and the new road stop */
		st_auto_delete.Release();
		rs_auto_delete.Release();
	}
	return cost;
}

// Remove a bus station
static int32 RemoveRoadStop(Station *st, uint32 flags, TileIndex tile)
{
	RoadStop **primary_stop;
	RoadStop *cur_stop;
	bool is_truck = IsTruckStop(tile);

	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner)) {
		return CMD_ERROR;
	}

	if (is_truck) { // truck stop
		primary_stop = &st->truck_stops;
		cur_stop = GetRoadStopByTile(tile, RoadStop::TRUCK);
	} else {
		primary_stop = &st->bus_stops;
		cur_stop = GetRoadStopByTile(tile, RoadStop::BUS);
	}

	assert(cur_stop != NULL);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (*primary_stop == cur_stop) {
			// removed the first stop in the list
			*primary_stop = cur_stop->next;
			// removed the only stop?
			if (*primary_stop == NULL) {
				st->facilities &= (is_truck ? ~FACIL_TRUCK_STOP : ~FACIL_BUS_STOP);
			}
		} else {
			// tell the predecessor in the list to skip this stop
			RoadStop *pred = *primary_stop;
			while (pred->next != cur_stop) pred = pred->next;
			pred->next = cur_stop->next;
		}

		delete cur_stop;
		DoClearSquare(tile);
		st->rect.AfterRemoveTile(st, tile);

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return (is_truck) ? _price.remove_truck_station : _price.remove_bus_station;
}

/** Remove a bus or truck stop
 * @param tile tile to remove the stop from
 * @param p1 not used
 * @param p2 bit 0: 0 for Bus stops, 1 for truck stops
 */
int32 CmdRemoveRoadStop(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Station* st;
	bool is_drive_through;
	bool is_towns_road = false;
	RoadBits road_bits;
	int32 ret;

	/* Make sure the specified tile is a road stop of the correct type */
	if (!IsTileType(tile, MP_STATION) || !IsRoadStop(tile) || (uint32)GetRoadStopType(tile) != p2) return CMD_ERROR;
	st = GetStationByTile(tile);
	/* Save the stop info before it is removed */
	is_drive_through = IsDriveThroughStopTile(tile);
	road_bits = GetAnyRoadBits(tile);
	if (is_drive_through) is_towns_road = GetStopBuiltOnTownRoad(tile);

	ret = RemoveRoadStop(st, flags, tile);

	/* If the stop was a drive-through stop replace the road */
	if ((flags & DC_EXEC) && !CmdFailed(ret) && is_drive_through) {
		uint index = 0;
		Owner cur_owner = _current_player;

		if (is_towns_road) {
			index = ClosestTownFromTile(tile, _patches.dist_local_authority)->index;
			_current_player = OWNER_TOWN;
		}
		DoCommand(tile, road_bits, index, DC_EXEC, CMD_BUILD_ROAD);
		_current_player = cur_owner;
	}

	return ret;
}

// FIXME -- need to move to its corresponding Airport variable
// Country Airfield (small)
static const byte _airport_sections_country[] = {
	54, 53, 52, 65,
	58, 57, 56, 55,
	64, 63, 63, 62
};

// City Airport (large)
static const byte _airport_sections_town[] = {
	31,  9, 33,  9,  9, 32,
	27, 36, 29, 34,  8, 10,
	30, 11, 35, 13, 20, 21,
	51, 12, 14, 17, 19, 28,
	38, 13, 15, 16, 18, 39,
	26, 22, 23, 24, 25, 26
};

// Metropolitain Airport (large) - 2 runways
static const byte _airport_sections_metropolitan[] = {
	 31,  9, 33,  9,  9, 32,
	 27, 36, 29, 34,  8, 10,
	 30, 11, 35, 13, 20, 21,
	102,  8,  8,  8,  8, 28,
	 83, 84, 84, 84, 84, 83,
	 26, 23, 23, 23, 23, 26
};

// International Airport (large) - 2 runways
static const byte _airport_sections_international[] = {
	88, 89, 89, 89, 89, 89,  88,
	51,  8,  8,  8,  8,  8,  32,
	30,  8, 11, 27, 11,  8,  10,
	32,  8, 11, 27, 11,  8, 114,
	87,  8, 11, 85, 11,  8, 114,
	87,  8,  8,  8,  8,  8,  90,
	26, 23, 23, 23, 23, 23,  26
};

// Intercontinental Airport (vlarge) - 4 runways
static const byte _airport_sections_intercontinental[] = {
	102, 120,  89,  89,  89,  89,  89,  89, 118,
	120,  22,  22,  22,  22,  22,  22, 119, 117,
	 87,  54,  87,   8,   8,   8,   8,  51, 117,
	 87, 162,  87,  85, 116, 116,   8,   9,  10,
	 87,   8,   8,  11,  31,  11,   8, 160,  32,
	 32, 160,   8,  11,  27,  11,   8,   8,  10,
	 87,   8,   8,  11,  30,  11,   8,   8,  10,
	 87, 142,   8,  11,  29,  11,  10, 163,  10,
	 87, 164,  87,   8,   8,   8,  10,  37, 117,
	 87, 120,  89,  89,  89,  89,  89,  89, 119,
	121,  22,  22,  22,  22,  22,  22, 119,  37
};


// Commuter Airfield (small)
static const byte _airport_sections_commuter[] = {
	85, 30, 115, 115, 32,
	87, 8,    8,   8, 10,
	87, 11,  11,  11, 10,
	26, 23,  23,  23, 26
};

// Heliport
static const byte _airport_sections_heliport[] = {
	66,
};

// Helidepot
static const byte _airport_sections_helidepot[] = {
	124, 32,
	122, 123
};

// Helistation
static const byte _airport_sections_helistation[] = {
	 32, 134, 159, 158,
	161, 142, 142, 157
};

static const byte * const _airport_sections[] = {
	_airport_sections_country,           // Country Airfield (small)
	_airport_sections_town,              // City Airport (large)
	_airport_sections_heliport,          // Heliport
	_airport_sections_metropolitan,      // Metropolitain Airport (large)
	_airport_sections_international,     // International Airport (xlarge)
	_airport_sections_commuter,          // Commuter Airport (small)
	_airport_sections_helidepot,         // Helidepot
	_airport_sections_intercontinental,  // Intercontinental Airport (xxlarge)
	_airport_sections_helistation        // Helistation
};

/** Place an Airport.
 * @param tile tile where airport will be built
 * @param p1 airport type, @see airport.h
 * @param p2 unused
 */
int32 CmdBuildAirport(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Town *t;
	Station *st;
	int32 cost;
	int32 ret;
	int w, h;
	bool airport_upgrade = true;
	const AirportFTAClass* afc;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Check if a valid, buildable airport was chosen for construction */
	if (p1 > lengthof(_airport_sections) || !HASBIT(GetValidAirports(), p1)) return CMD_ERROR;

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	t = ClosestTownFromTile(tile, (uint)-1);

	/* Check if local auth refuses a new airport */
	{
		uint num = 0;
		FOR_ALL_STATIONS(st) {
			if (st->town == t && st->facilities&FACIL_AIRPORT && st->airport_type != AT_OILRIG)
				num++;
		}
		if (num >= 2) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2035_LOCAL_AUTHORITY_REFUSES);
		}
	}

	afc = GetAirport(p1);
	w = afc->size_x;
	h = afc->size_y;

	ret = CheckFlatLandBelow(tile, w, h, flags, 0, NULL);
	if (CmdFailed(ret)) return ret;
	cost = ret;

	st = GetStationAround(tile, w, h, INVALID_STATION);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st != NULL && st->facilities) st = NULL;
	}

	if (w > _patches.station_spread || h > _patches.station_spread) {
		_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
		return CMD_ERROR;
	}

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	 * to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!st->rect.BeforeAddRect(tile, w, h, StationRect::ADD_TEST)) return CMD_ERROR;

		if (st->airport_tile != 0)
			return_cmd_error(STR_300D_TOO_CLOSE_TO_ANOTHER_AIRPORT);
	} else {
		airport_upgrade = false;

		/* allocate and initialize new station */
		st = new Station(tile);
		if (st == NULL) return CMD_ERROR;

		/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
		st_auto_delete = st;

		st->town = t;

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SETBIT(t->have_ratings, _current_player);
		}

		st->sign.width_1 = 0;

		// if airport type equals Heliport then generate
		// type 5 name, which is heliport, otherwise airport names (1)
		if (!GenerateStationName(st, tile, (p1 == AT_HELIPORT)||(p1 == AT_HELIDEPOT)||(p1 == AT_HELISTATION) ? 5 : 1))
			return CMD_ERROR;
	}

	cost += _price.build_airport * w * h;

	if (flags & DC_EXEC) {
		st->airport_tile = tile;
		st->AddFacility(FACIL_AIRPORT, tile);
		st->airport_type = (byte)p1;
		st->airport_flags = 0;

		st->rect.BeforeAddRect(tile, w, h, StationRect::ADD_TRY);

		/* if airport was demolished while planes were en-route to it, the
		 * positions can no longer be the same (v->u.air.pos), since different
		 * airports have different indexes. So update all planes en-route to this
		 * airport. Only update if
		 * 1. airport is upgraded
		 * 2. airport is added to existing station (unfortunately unavoideable)
		 */
		if (airport_upgrade) UpdateAirplanesOnNewStation(st);

		{
			const byte *b = _airport_sections[p1];

			BEGIN_TILE_LOOP(tile_cur, w, h, tile) {
				MakeAirport(tile_cur, st->owner, st->index, *b++);
			} END_TILE_LOOP(tile_cur, w, h, tile)
		}

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		/* success, so don't delete the new station */
		st_auto_delete.Release();
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

	const AirportFTAClass *afc = st->Airport();
	w = afc->size_x;
	h = afc->size_y;

	cost = w * h * _price.remove_airport;

	BEGIN_TILE_LOOP(tile_cur, w, h, tile) {
		if (!EnsureNoVehicle(tile_cur)) return CMD_ERROR;

		if (flags & DC_EXEC) {
			DeleteAnimatedTile(tile_cur);
			DoClearSquare(tile_cur);
		}
	} END_TILE_LOOP(tile_cur, w,h,tile)

	if (flags & DC_EXEC) {
		uint i;

		for (i = 0; i < afc->nof_depots; ++i) {
			DeleteWindowById(
				WC_VEHICLE_DEPOT, tile + ToTileIndexDiff(afc->airport_depots[i])
			);
		}

		st->rect.AfterRemoveRect(st, tile, w, h);

		st->airport_tile = 0;
		st->facilities &= ~FACIL_AIRPORT;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

/** Build a buoy.
 * @param tile tile where to place the bouy
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdBuildBuoy(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!IsClearWaterTile(tile) || tile == 0) return_cmd_error(STR_304B_SITE_UNSUITABLE);

	/* allocate and initialize new station */
	st = new Station(tile);
	if (st == NULL) return CMD_ERROR;

	/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
	AutoPtrT<Station> st_auto_delete(st);

	st->town = ClosestTownFromTile(tile, (uint)-1);
	st->sign.width_1 = 0;

	if (!GenerateStationName(st, tile, 4)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		st->dock_tile = tile;
		st->facilities |= FACIL_DOCK;
		/* Buoys are marked in the Station struct by this flag. Yes, it is this
		 * braindead.. */
		st->had_vehicle_of_type |= HVOT_BUOY;
		st->owner = OWNER_NONE;

		st->build_date = _date;

		MakeBuoy(tile, st->index);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		/* success, so don't delete the new station */
		st_auto_delete.Release();
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
				if (order->type == OT_GOTO_STATION && order->dest == st->index) {
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

	/* XXX: strange stuff */
	if (!IsValidPlayer(_current_player))  return_cmd_error(INVALID_STRING_ID);

	tile = st->dock_tile;

	if (CheckShipsOnBuoy(st))   return_cmd_error(STR_BUOY_IS_IN_USE);
	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		st->dock_tile = 0;
		/* Buoys are marked in the Station struct by this flag. Yes, it is this
		 * braindead.. */
		st->facilities &= ~FACIL_DOCK;
		st->had_vehicle_of_type &= ~HVOT_BUOY;

		/* We have to set the water tile's state to the same state as before the
		 * buoy was placed. Otherwise one could plant a buoy on a canal edge,
		 * remove it and flood the land (if the canal edge is at level 0) */
		Owner o = GetTileOwner(tile);
		if (o == OWNER_WATER) {
			MakeWater(tile);
		} else {
			MakeCanal(tile, o);
		}
		MarkTileDirtyByTile(tile);

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
static const byte _dock_w_chk[4] = { 2, 1, 2, 1 };
static const byte _dock_h_chk[4] = { 1, 2, 1, 2 };

/** Build a dock/haven.
 * @param tile tile where dock will be built
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdBuildDock(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile_cur;
	DiagDirection direction;
	int32 cost;
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileSlope(tile, NULL)) {
		case SLOPE_SW: direction = DIAGDIR_NE; break;
		case SLOPE_SE: direction = DIAGDIR_NW; break;
		case SLOPE_NW: direction = DIAGDIR_SE; break;
		case SLOPE_NE: direction = DIAGDIR_SW; break;
		default: return_cmd_error(STR_304B_SITE_UNSUITABLE);
	}

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile)) return CMD_ERROR;

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	tile_cur = tile + TileOffsByDiagDir(direction);

	if (!EnsureNoVehicle(tile_cur)) return CMD_ERROR;

	if (!IsTileType(tile_cur, MP_WATER) || GetTileSlope(tile_cur, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_304B_SITE_UNSUITABLE);
	}

	cost = DoCommand(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	tile_cur += TileOffsByDiagDir(direction);
	if (!IsTileType(tile_cur, MP_WATER) || GetTileSlope(tile_cur, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_304B_SITE_UNSUITABLE);
	}

	/* middle */
	st = GetStationAround(
		tile + ToTileIndexDiff(_dock_tileoffs_chkaround[direction]),
		_dock_w_chk[direction], _dock_h_chk[direction], INVALID_STATION);
	if (st == CHECK_STATIONS_ERR) return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st!=NULL && st->facilities) st = NULL;
	}

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	* to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!st->rect.BeforeAddRect(tile, _dock_w_chk[direction], _dock_h_chk[direction], StationRect::ADD_TEST)) return CMD_ERROR;

		if (st->dock_tile != 0) return_cmd_error(STR_304C_TOO_CLOSE_TO_ANOTHER_DOCK);
	} else {
		/* allocate and initialize new station */
		st = new Station(tile);
		if (st == NULL) return CMD_ERROR;

		/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
		st_auto_delete = st;

		Town *t = st->town = ClosestTownFromTile(tile, (uint)-1);

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SETBIT(t->have_ratings, _current_player);
		}

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, 3)) return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		st->dock_tile = tile;
		st->AddFacility(FACIL_DOCK, tile);

		st->rect.BeforeAddRect(tile, _dock_w_chk[direction], _dock_h_chk[direction], StationRect::ADD_TRY);

		MakeDock(tile, st->owner, st->index, direction);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		/* success, so don't delete the new station */
		st_auto_delete.Release();
	}
	return _price.build_dock;
}

static int32 RemoveDock(Station *st, uint32 flags)
{
	TileIndex tile1;
	TileIndex tile2;

	if (!CheckOwnership(st->owner)) return CMD_ERROR;

	tile1 = st->dock_tile;
	tile2 = tile1 + TileOffsByDiagDir(GetDockDirection(tile1));

	if (!EnsureNoVehicle(tile1)) return CMD_ERROR;
	if (!EnsureNoVehicle(tile2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile1);
		MakeWater(tile2);

		st->rect.AfterRemoveTile(st, tile1);
		st->rect.AfterRemoveTile(st, tile2);

		MarkTileDirtyByTile(tile2);

		st->dock_tile = 0;
		st->facilities &= ~FACIL_DOCK;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return _price.remove_dock;
}

#include "table/station_land.h"

const DrawTileSprites *GetStationTileLayout(byte gfx)
{
	return &_station_display_datas[gfx];
}

/* For drawing canal edges on buoys */
extern void DrawCanalWater(TileIndex tile);

static void DrawTile_Station(TileInfo *ti)
{
	const DrawTileSeqStruct *dtss;
	const DrawTileSprites *t = NULL;
	RailType railtype = GetRailType(ti->tile);
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	uint32 relocation = 0;
	const Station *st = NULL;
	const StationSpec *statspec = NULL;
	PlayerID owner = GetTileOwner(ti->tile);
	SpriteID image;
	SpriteID palette;

	if (IsValidPlayer(owner)) {
		palette = PLAYER_SPRITE_COLOR(owner);
	} else {
		// Some stations are not owner by a player, namely oil rigs
		palette = PALETTE_TO_GREY;
	}

	// don't show foundation for docks
	if (ti->tileh != SLOPE_FLAT && !IsDock(ti->tile))
		DrawFoundation(ti, ti->tileh);

	if (IsCustomStationSpecIndex(ti->tile)) {
		// look for customization
		st = GetStationByTile(ti->tile);
		statspec = st->speclist[GetCustomStationSpecIndex(ti->tile)].spec;

		//debug("Cust-o-mized %p", statspec);

		if (statspec != NULL) {
			uint tile = GetStationGfx(ti->tile);

			relocation = GetCustomStationRelocation(statspec, st, ti->tile);

			if (HASBIT(statspec->callbackmask, CBM_CUSTOM_LAYOUT)) {
				uint16 callback = GetStationCallback(CBID_STATION_SPRITE_LAYOUT, 0, 0, statspec, st, ti->tile);
				if (callback != CALLBACK_FAILED) tile = (callback & ~1) + GetRailStationAxis(ti->tile);
			}

			/* Ensure the chosen tile layout is valid for this custom station */
			if (statspec->renderdata != NULL) {
				t = &statspec->renderdata[tile < statspec->tiles ? tile : (uint)GetRailStationAxis(ti->tile)];
			}
		}
	}

	if (t == NULL || t->seq == NULL) t = &_station_display_datas[GetStationGfx(ti->tile)];

	image = t->ground_sprite;
	if (HASBIT(image, SPRITE_MODIFIER_USE_OFFSET)) {
		image += GetCustomStationGroundRelocation(statspec, st, ti->tile);
		image += rti->custom_ground_offset;
	} else {
		image += rti->total_offset;
	}

	// station_land array has been increased from 82 elements to 114
	// but this is something else. If AI builds station with 114 it looks all weird
	DrawGroundSprite(image, HASBIT(image, PALETTE_MODIFIER_COLOR) ? palette : PAL_NONE);

	if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC && IsStationTileElectrifiable(ti->tile)) DrawCatenary(ti);

	if (IsBuoyTile(ti->tile) && (ti->z != 0 || !IsTileOwner(ti->tile, OWNER_WATER))) DrawCanalWater(ti->tile);

	foreach_draw_tile_seq(dtss, t->seq) {
		SpriteID pal;

		image = dtss->image;
		if (relocation == 0 || HASBIT(image, SPRITE_MODIFIER_USE_OFFSET)) {
			image += rti->total_offset;
		} else {
			image += relocation;
		}

		if (_display_opt & DO_TRANS_BUILDINGS) {
			SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
			pal = PALETTE_TO_TRANSPARENT;
		} else if (HASBIT(image, PALETTE_MODIFIER_COLOR)) {
			pal = palette;
		} else {
			pal = dtss->pal;
		}

		if ((byte)dtss->delta_z != 0x80) {
			AddSortableSpriteToDraw(
				image, pal,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z
			);
		} else {
			AddChildSpriteScreen(image, pal, dtss->delta_x, dtss->delta_y);
		}
	}
}

void StationPickerDrawSprite(int x, int y, RailType railtype, int image)
{
	SpriteID pal, img;
	const DrawTileSeqStruct *dtss;
	const DrawTileSprites *t;
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);

	pal = PLAYER_SPRITE_COLOR(_local_player);

	t = &_station_display_datas[image];

	img = t->ground_sprite;
	DrawSprite(img + rti->total_offset, HASBIT(img, PALETTE_MODIFIER_COLOR) ? pal : PAL_NONE, x, y);

	foreach_draw_tile_seq(dtss, t->seq) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		DrawSprite(dtss->image + rti->total_offset, pal, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Station(TileIndex tile, uint x, uint y)
{
	return GetTileMaxZ(tile);
}

static Slope GetSlopeTileh_Station(TileIndex tile, Slope tileh)
{
	return SLOPE_FLAT;
}

static void GetAcceptedCargo_Station(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Station(TileIndex tile, TileDesc *td)
{
	StringID str;

	td->owner = GetTileOwner(tile);
	td->build_date = GetStationByTile(tile)->build_date;

	switch (GetStationType(tile)) {
		default: NOT_REACHED();
		case STATION_RAIL:    str = STR_305E_RAILROAD_STATION; break;
		case STATION_AIRPORT:
			str = (IsHangar(tile) ? STR_305F_AIRCRAFT_HANGAR : STR_3060_AIRPORT);
			break;
		case STATION_TRUCK:   str = STR_3061_TRUCK_LOADING_AREA; break;
		case STATION_BUS:     str = STR_3062_BUS_STATION; break;
		case STATION_OILRIG:  str = STR_4807_OIL_RIG; break;
		case STATION_DOCK:    str = STR_3063_SHIP_DOCK; break;
		case STATION_BUOY:    str = STR_3069_BUOY; break;
	}
	td->str = str;
}


static uint32 GetTileTrackStatus_Station(TileIndex tile, TransportType mode)
{
	switch (mode) {
		case TRANSPORT_RAIL:
			if (IsRailwayStation(tile) && !IsStationTileBlocked(tile)) {
				return TrackToTrackBits(GetRailStationTrack(tile)) * 0x101;
			}
			break;

		case TRANSPORT_WATER:
			// buoy is coded as a station, it is always on open water
			if (IsBuoy(tile)) {
				TrackBits ts = TRACK_BIT_ALL;
				// remove tracks that connect NE map edge
				if (TileX(tile) == 0) ts &= ~(TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT);
				// remove tracks that connect NW map edge
				if (TileY(tile) == 0) ts &= ~(TRACK_BIT_Y | TRACK_BIT_LEFT | TRACK_BIT_UPPER);
				return uint32(ts) * 0x101;
			}
			break;

		case TRANSPORT_ROAD:
			if (IsRoadStopTile(tile)) {
				return AxisToTrackBits(DiagDirToAxis(GetRoadStopDir(tile))) * 0x101;
			}
			break;

		default:
			break;
	}

	return 0;
}


static void TileLoop_Station(TileIndex tile)
{
	// FIXME -- GetTileTrackStatus_Station -> animated stationtiles
	// hardcoded.....not good
	switch (GetStationGfx(tile)) {
		case GFX_RADAR_LARGE_FIRST:
		case GFX_WINDSACK_FIRST : // for small airport
		case GFX_RADAR_INTERNATIONAL_FIRST:
		case GFX_RADAR_METROPOLITAN_FIRST:
		case GFX_RADAR_DISTRICTWE_FIRST: // radar district W-E airport
		case GFX_WINDSACK_INTERCON_FIRST : // for intercontinental airport
			AddAnimatedTile(tile);
			break;

		case GFX_OILRIG_BASE: //(station part)
		case GFX_BUOY_BASE:
			TileLoop_Water(tile);
			break;

		default: break;
	}
}


static void AnimateTile_Station(TileIndex tile)
{
	typedef struct AnimData {
		StationGfx from; // first sprite
		StationGfx to;   // last sprite
		byte delay;
	} AnimData;

	static const AnimData data[] = {
		{ GFX_RADAR_LARGE_FIRST,         GFX_RADAR_LARGE_LAST,         3 },
		{ GFX_WINDSACK_FIRST,            GFX_WINDSACK_LAST,            1 },
		{ GFX_RADAR_INTERNATIONAL_FIRST, GFX_RADAR_INTERNATIONAL_LAST, 3 },
		{ GFX_RADAR_METROPOLITAN_FIRST,  GFX_RADAR_METROPOLITAN_LAST,  3 },
		{ GFX_RADAR_DISTRICTWE_FIRST,    GFX_RADAR_DISTRICTWE_LAST,    3 },
		{ GFX_WINDSACK_INTERCON_FIRST,   GFX_WINDSACK_INTERCON_LAST,   1 }
	};

	StationGfx gfx = GetStationGfx(tile);
	const AnimData* i;

	for (i = data; i != endof(data); i++) {
		if (i->from <= gfx && gfx <= i->to) {
			if ((_tick_counter & i->delay) == 0) {
				SetStationGfx(tile, gfx < i->to ? gfx + 1 : i->from);
				MarkTileDirtyByTile(tile);
			}
			break;
		}
	}
}


static void ClickTile_Station(TileIndex tile)
{
	if (IsHangar(tile)) {
		ShowDepotWindow(tile, VEH_Aircraft);
	} else {
		ShowStationViewWindow(GetStationIndex(tile));
	}
}

static const byte _enter_station_speedtable[12] = {
	215, 195, 175, 155, 135, 115, 95, 75, 55, 35, 15, 0
};

static uint32 VehicleEnter_Station(Vehicle *v, TileIndex tile, int x, int y)
{
	if (v->type == VEH_Train) {
		if (IsRailwayStation(tile) && IsFrontEngine(v) &&
				!IsCompatibleTrainStationTile(tile + TileOffsByDiagDir(DirToDiagDir(v->direction)), tile)) {
			StationID station_id = GetStationIndex(tile);

			if ((!(v->current_order.flags & OF_NON_STOP) && !_patches.new_nonstop) ||
					(v->current_order.type == OT_GOTO_STATION && v->current_order.dest == station_id)) {
				if (!(_patches.new_nonstop && v->current_order.flags & OF_NON_STOP) &&
						v->current_order.type != OT_LEAVESTATION &&
						v->last_station_visited != station_id) {
					DiagDirection dir = DirToDiagDir(v->direction);

					x &= 0xF;
					y &= 0xF;

					if (DiagDirToAxis(dir) != AXIS_X) intswap(x, y);
					if (y == TILE_SIZE / 2) {
						if (dir != DIAGDIR_SE && dir != DIAGDIR_SW) x = TILE_SIZE - 1 - x;
						if (x == 12) return VETSB_ENTERED_STATION | (station_id << VETS_STATION_ID_OFFSET); /* enter station */
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
		if (v->u.road.state < RVSB_IN_ROAD_STOP && v->u.road.frame == 0) {
			if (IsRoadStop(tile)) {
				/* Attempt to allocate a parking bay in a road stop */
				RoadStop *rs = GetRoadStopByTile(tile, GetRoadStopType(tile));

				if (IsDriveThroughStopTile(tile)) {
					/* Vehicles entering a drive-through stop from the 'normal' side use first bay (bay 0). */
					byte side = ((DirToDiagDir(v->direction) == ReverseDiagDir(GetRoadStopDir(tile))) == (v->u.road.overtaking == 0)) ? 0 : 1;

					if (!rs->IsFreeBay(side)) return VETSB_CANNOT_ENTER;

					/* Check if the vehicle is stopping at this road stop */
					if (GetRoadStopType(tile) == ((v->cargo_type == CT_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK) &&
							v->current_order.dest == GetStationIndex(tile)) {
						SETBIT(v->u.road.state, RVS_IS_STOPPING);
						rs->AllocateDriveThroughBay(side);
					}

					/* Indicate if vehicle is using second bay. */
					if (side == 1) SETBIT(v->u.road.state, RVS_USING_SECOND_BAY);
					/* Indicate a drive-through stop */
					SETBIT(v->u.road.state, RVS_IN_DT_ROAD_STOP);
					return VETSB_CONTINUE;
				}

				/* For normal (non drive-through) road stops */
				/* Check if station is busy or if there are no free bays. */
				if (rs->IsEntranceBusy() || !rs->HasFreeBay()) return VETSB_CANNOT_ENTER;

				SETBIT(v->u.road.state, RVS_IN_ROAD_STOP);

				/* Allocate a bay and update the road state */
				uint bay_nr = rs->AllocateBay();
				SB(v->u.road.state, RVS_USING_SECOND_BAY, 1, bay_nr);

				/* Mark the station entrace as busy */
				rs->SetEntranceBusy(true);
			}
		}
	}

	return VETSB_CONTINUE;
}

/* this function is called for one station each tick */
static void StationHandleBigTick(Station *st)
{
	UpdateStationAcceptance(st, true);

	if (st->facilities == 0 && ++st->delete_ctr >= 8) delete st;

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

			if (IsValidPlayer(st->owner) && HASBIT(st->town->statues, st->owner)) rating += 26;

			{
				byte days = ge->days_since_pickup;
				if (st->last_vehicle_type == VEH_Ship)
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
				int or_ = ge->rating; // old rating

				// only modify rating in steps of -2, -1, 0, 1 or 2
				ge->rating = rating = or_ + clamp(clamp(rating, 0, 255) - or_, -2, 2);

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
					if (rating <= (int)GB(r, 0, 7)) {
						waiting = max(waiting - (int)GB(r, 8, 2) - 1, 0);
						waiting_changed = true;
					}
				}

				if (waiting_changed) SB(ge->waiting_acceptance, 0, 12, waiting);
			}
		}
	} while (++ge != endof(st->goods));

	index = st->index;

	if (waiting_changed) {
		InvalidateWindow(WC_STATION_VIEW, index);
	} else {
		InvalidateWindowWidget(WC_STATION_VIEW, index, 5);
	}
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
	if (++_station_tick_ctr > GetMaxStationIndex()) _station_tick_ctr = 0;

	if (IsValidStationID(i)) StationHandleBigTick(GetStation(i));

	FOR_ALL_STATIONS(st) {
		StationHandleSmallTick(st);
	}
}

void StationMonthlyLoop(void)
{
}


void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius)
{
	Station *st;

	FOR_ALL_STATIONS(st) {
		if (st->owner == owner &&
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
	st->goods[type].enroute_from_xy = st->xy;
	InvalidateWindow(WC_STATION_VIEW, st->index);
	st->MarkTilesDirty();
}

/** Rename a station
 * @param tile unused
 * @param p1 station ID that is to be renamed
 * @param p2 unused
 */
int32 CmdRenameStation(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Station *st;

	if (!IsValidStationID(p1) || _cmd_text[0] == '\0') return CMD_ERROR;
	st = GetStation(p1);

	if (!CheckOwnership(st->owner)) return CMD_ERROR;

	str = AllocateNameUnique(_cmd_text, 6);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StringID old_str = st->string_id;

		st->string_id = str;
		UpdateStationVirtCoord(st);
		DeleteName(old_str);
		ResortStationLists();
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}


uint MoveGoodsToStation(TileIndex tile, int w, int h, int type, uint amount)
{
	Station* around[8];
	uint i;
	uint moved;
	uint best_rating, best_rating2;
	Station *st1, *st2;
	uint t;
	int rad = 0;
	int w_prod; //width and height of the "producer" of the cargo
	int h_prod;
	int max_rad;

	for (i = 0; i < lengthof(around); i++) around[i] = NULL;

	if (_patches.modified_catchment) {
		w_prod = w;
		h_prod = h;
		w += 16;
		h += 16;
		max_rad = 8;
	} else {
		w_prod = 0;
		h_prod = 0;
		w += 8;
		h += 8;
		max_rad = 4;
	}

	BEGIN_TILE_LOOP(cur_tile, w, h, tile - TileDiffXY(max_rad, max_rad))
		Station* st;

		cur_tile = TILE_MASK(cur_tile);
		if (!IsTileType(cur_tile, MP_STATION)) continue;

		st = GetStationByTile(cur_tile);

		for (i = 0; i != lengthof(around); i++) {
			if (around[i] == NULL) {
				if (!st->IsBuoy() &&
						(st->town->exclusive_counter == 0 || st->town->exclusivity == st->owner) && // check exclusive transport rights
						st->goods[type].rating != 0 &&
						(!_patches.selectgoods || st->goods[type].last_speed > 0) && // if last_speed is 0, no vehicle has been there.
						((st->facilities & ~FACIL_BUS_STOP)   != 0 || type == CT_PASSENGERS) && // if we have other fac. than a bus stop, or the cargo is passengers
						((st->facilities & ~FACIL_TRUCK_STOP) != 0 || type != CT_PASSENGERS)) { // if we have other fac. than a cargo bay or the cargo is not passengers
					int x_dist;
					int y_dist;

					if (_patches.modified_catchment) {
						// min and max coordinates of the producer relative
						const int x_min_prod = 9;
						const int x_max_prod = 8 + w_prod;
						const int y_min_prod = 9;
						const int y_max_prod = 8 + h_prod;

						rad = FindCatchmentRadius(st);

						x_dist = min(w_cur - x_min_prod, x_max_prod - w_cur);
						if (w_cur < x_min_prod) {
							x_dist = x_min_prod - w_cur;
						} else if (w_cur > x_max_prod) {
							x_dist = w_cur - x_max_prod;
						}

						y_dist = min(h_cur - y_min_prod, y_max_prod - h_cur);
						if (h_cur < y_min_prod) {
							y_dist = y_min_prod - h_cur;
						} else if (h_cur > y_max_prod) {
							y_dist = h_cur - y_max_prod;
						}
					} else {
						x_dist = 0;
						y_dist = 0;
					}

					if (x_dist <= rad && y_dist <= rad) around[i] = st;
				}
				break;
			} else if (around[i] == st) {
				break;
			}
		}
	END_TILE_LOOP(cur_tile, w, h, tile - TileDiffXY(max_rad, max_rad))

	/* no stations around at all? */
	if (around[0] == NULL) return 0;

	if (around[1] == NULL) {
		/* only one station around */
		moved = (amount * around[0]->goods[type].rating >> 8) + 1;
		UpdateStationWaiting(around[0], type, moved);
		return moved;
	}

	/* several stations around, find the two with the highest rating */
	st2 = st1 = NULL;
	best_rating = best_rating2 = 0;

	for (i = 0; i != lengthof(around) && around[i] != NULL; i++) {
		if (around[i]->goods[type].rating >= best_rating) {
			best_rating2 = best_rating;
			st2 = st1;

			best_rating = around[i]->goods[type].rating;
			st1 = around[i];
		} else if (around[i]->goods[type].rating >= best_rating2) {
			best_rating2 = around[i]->goods[type].rating;
			st2 = around[i];
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
		moved = t * best_rating / 256 + 1;
		amount -= t;
		UpdateStationWaiting(st1, type, moved);
	}

	if (amount != 0) {
		amount = amount * best_rating2 / 256 + 1;
		moved += amount;
		UpdateStationWaiting(st2, type, amount);
	}

	return moved;
}

void BuildOilRig(TileIndex tile)
{
	uint j;
	Station *st = new Station();

	if (st == NULL) {
		DEBUG(misc, 0, "Can't allocate station for oilrig at 0x%X, reverting to oilrig only", tile);
		return;
	}
	if (!GenerateStationName(st, tile, 2)) {
		DEBUG(misc, 0, "Can't allocate station-name for oilrig at 0x%X, reverting to oilrig only", tile);
		return;
	}

	st->town = ClosestTownFromTile(tile, (uint)-1);
	st->sign.width_1 = 0;

	MakeOilrig(tile, st->index);

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
	st->last_vehicle_type = VEH_Invalid;
	st->facilities = FACIL_AIRPORT | FACIL_DOCK;
	st->build_date = _date;

	for (j = 0; j != NUM_CARGO; j++) {
		st->goods[j].waiting_acceptance = 0;
		st->goods[j].days_since_pickup = 0;
		st->goods[j].enroute_from = INVALID_STATION;
		st->goods[j].enroute_from_xy = INVALID_TILE;
		st->goods[j].rating = 175;
		st->goods[j].last_speed = 0;
		st->goods[j].last_age = 255;
	}

	UpdateStationVirtCoordDirty(st);
	UpdateStationAcceptance(st, false);
}

void DeleteOilRig(TileIndex tile)
{
	Station* st = GetStationByTile(tile);

	MakeWater(tile);

	st->dock_tile = 0;
	st->airport_tile = 0;
	st->facilities &= ~(FACIL_AIRPORT | FACIL_DOCK);
	st->airport_flags = 0;
	UpdateStationVirtCoordDirty(st);
	if (st->facilities == 0) delete st;
}

static void ChangeTileOwner_Station(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		Station* st = GetStationByTile(tile);

		SetTileOwner(tile, new_player);
		st->owner = new_player;
		RebuildStationLists();
		InvalidateWindowClasses(WC_STATION_LIST);
	} else {
		DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static int32 ClearTile_Station(TileIndex tile, byte flags)
{
	Station *st;

	if (flags & DC_AUTO) {
		switch (GetStationType(tile)) {
			case STATION_RAIL:    return_cmd_error(STR_300B_MUST_DEMOLISH_RAILROAD);
			case STATION_AIRPORT: return_cmd_error(STR_300E_MUST_DEMOLISH_AIRPORT_FIRST);
			case STATION_TRUCK:   return_cmd_error(STR_3047_MUST_DEMOLISH_TRUCK_STATION);
			case STATION_BUS:     return_cmd_error(STR_3046_MUST_DEMOLISH_BUS_STATION);
			case STATION_BUOY:    return_cmd_error(STR_306A_BUOY_IN_THE_WAY);
			case STATION_DOCK:    return_cmd_error(STR_304D_MUST_DEMOLISH_DOCK_FIRST);
			case STATION_OILRIG:
				SetDParam(0, STR_4807_OIL_RIG);
				return_cmd_error(STR_4800_IN_THE_WAY);
		}
	}

	st = GetStationByTile(tile);

	switch (GetStationType(tile)) {
		case STATION_RAIL:    return RemoveRailroadStation(st, tile, flags);
		case STATION_AIRPORT: return RemoveAirport(st, flags);
		case STATION_TRUCK:
			if (IsDriveThroughStopTile(tile) && GetStopBuiltOnTownRoad(tile))
				return_cmd_error(STR_3047_MUST_DEMOLISH_TRUCK_STATION);
			return RemoveRoadStop(st, flags, tile);
		case STATION_BUS:
			if (IsDriveThroughStopTile(tile) && GetStopBuiltOnTownRoad(tile))
				return_cmd_error(STR_3046_MUST_DEMOLISH_BUS_STATION);
			return RemoveRoadStop(st, flags, tile);
		case STATION_BUOY:    return RemoveBuoy(st, flags);
		case STATION_DOCK:    return RemoveDock(st, flags);
		default: break;
	}

	return CMD_ERROR;
}

void InitializeStations(void)
{
	/* Clean the station pool and create 1 block in it */
	CleanPool(&_Station_pool);
	AddBlockToPool(&_Station_pool);

	/* Clean the roadstop pool and create 1 block in it */
	CleanPool(&_RoadStop_pool);
	AddBlockToPool(&_RoadStop_pool);

	_station_tick_ctr = 0;

}


void AfterLoadStations(void)
{
	Station *st;
	uint i;
	TileIndex tile;

	/* Update the speclists of all stations to point to the currently loaded custom stations. */
	FOR_ALL_STATIONS(st) {
		for (i = 0; i < st->num_specs; i++) {
			if (st->speclist[i].grfid == 0) continue;

			st->speclist[i].spec = GetCustomStationSpecByGrf(st->speclist[i].grfid, st->speclist[i].localidx);
		}
	}

	for (tile = 0; tile < MapSize(); tile++) {
		if (GetTileType(tile) != MP_STATION) continue;
		st = GetStationByTile(tile);
		st->rect.BeforeAddTile(tile, StationRect::ADD_FORCE);
	}
}


extern const TileTypeProcs _tile_type_station_procs = {
	DrawTile_Station,           /* draw_tile_proc */
	GetSlopeZ_Station,          /* get_slope_z_proc */
	ClearTile_Station,          /* clear_tile_proc */
	GetAcceptedCargo_Station,   /* get_accepted_cargo_proc */
	GetTileDesc_Station,        /* get_tile_desc_proc */
	GetTileTrackStatus_Station, /* get_tile_track_status_proc */
	ClickTile_Station,          /* click_tile_proc */
	AnimateTile_Station,        /* animate_tile_proc */
	TileLoop_Station,           /* tile_loop_clear */
	ChangeTileOwner_Station,    /* change_tile_owner_clear */
	NULL,                       /* get_produced_cargo_proc */
	VehicleEnter_Station,       /* vehicle_enter_tile_proc */
	GetSlopeTileh_Station,      /* get_slope_tileh_proc */
};

static const SaveLoad _roadstop_desc[] = {
	SLE_VAR(RoadStop,xy,           SLE_UINT32),
	SLE_CONDNULL(1, 0, 44),
	SLE_VAR(RoadStop,status,       SLE_UINT8),
	/* Index was saved in some versions, but this is not needed */
	SLE_CONDNULL(4, 0, 8),
	SLE_CONDNULL(2, 0, 44),
	SLE_CONDNULL(1, 0, 25),

	SLE_REF(RoadStop,next,         REF_ROADSTOPS),
	SLE_CONDNULL(2, 0, 44),

	SLE_CONDNULL(4, 0, 24),
	SLE_CONDNULL(1, 25, 25),

	SLE_END()
};

static const SaveLoad _station_desc[] = {
	SLE_CONDVAR(Station, xy,                         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, xy,                         SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDNULL(4, 0, 5), // bus/lorry tile
	SLE_CONDVAR(Station, train_tile,                 SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, train_tile,                 SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Station, airport_tile,               SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, airport_tile,               SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Station, dock_tile,                  SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, dock_tile,                  SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_REF(Station, town,                       REF_TOWN),
	    SLE_VAR(Station, trainst_w,                  SLE_UINT8),
	SLE_CONDVAR(Station, trainst_h,                  SLE_UINT8,                   2, SL_MAX_VERSION),

	// alpha_order was stored here in savegame format 0 - 3
	SLE_CONDNULL(1, 0, 3),

	    SLE_VAR(Station, string_id,                  SLE_STRINGID),
	    SLE_VAR(Station, had_vehicle_of_type,        SLE_UINT16),

	    SLE_VAR(Station, time_since_load,            SLE_UINT8),
	    SLE_VAR(Station, time_since_unload,          SLE_UINT8),
	    SLE_VAR(Station, delete_ctr,                 SLE_UINT8),
	    SLE_VAR(Station, owner,                      SLE_UINT8),
	    SLE_VAR(Station, facilities,                 SLE_UINT8),
	    SLE_VAR(Station, airport_type,               SLE_UINT8),

	SLE_CONDNULL(2, 0, 5), // Truck/bus stop status
	SLE_CONDNULL(1, 0, 4), // Blocked months

	SLE_CONDVAR(Station, airport_flags,              SLE_VAR_U64 | SLE_FILE_U16,  0,  2),
	SLE_CONDVAR(Station, airport_flags,              SLE_VAR_U64 | SLE_FILE_U32,  3, 45),
	SLE_CONDVAR(Station, airport_flags,              SLE_UINT64,                 46, SL_MAX_VERSION),

	SLE_CONDNULL(2, 0, 25), /* Ex last-vehicle */
	SLE_CONDVAR(Station, last_vehicle_type,          SLE_UINT8,                  26, SL_MAX_VERSION),

	// Was custom station class and id
	SLE_CONDNULL(2, 3, 25),
	SLE_CONDVAR(Station, build_date,                 SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(Station, build_date,                 SLE_INT32,                  31, SL_MAX_VERSION),

	SLE_CONDREF(Station, bus_stops,                  REF_ROADSTOPS,               6, SL_MAX_VERSION),
	SLE_CONDREF(Station, truck_stops,                REF_ROADSTOPS,               6, SL_MAX_VERSION),

	/* Used by newstations for graphic variations */
	SLE_CONDVAR(Station, random_bits,                SLE_UINT16,                 27, SL_MAX_VERSION),
	SLE_CONDVAR(Station, waiting_triggers,           SLE_UINT8,                  27, SL_MAX_VERSION),
	SLE_CONDVAR(Station, num_specs,                  SLE_UINT8,                  27, SL_MAX_VERSION),

	// reserve extra space in savegame here. (currently 32 bytes)
	SLE_CONDNULL(32, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _goods_desc[] = {
	    SLE_VAR(GoodsEntry, waiting_acceptance, SLE_UINT16),
	    SLE_VAR(GoodsEntry, days_since_pickup,  SLE_UINT8),
	    SLE_VAR(GoodsEntry, rating,             SLE_UINT8),
	SLE_CONDVAR(GoodsEntry, enroute_from,       SLE_FILE_U8 | SLE_VAR_U16,  0, 6),
	SLE_CONDVAR(GoodsEntry, enroute_from,       SLE_UINT16,                 7, SL_MAX_VERSION),
	SLE_CONDVAR(GoodsEntry, enroute_from_xy,    SLE_UINT32,                44, SL_MAX_VERSION),
	    SLE_VAR(GoodsEntry, enroute_time,       SLE_UINT8),
	    SLE_VAR(GoodsEntry, last_speed,         SLE_UINT8),
	    SLE_VAR(GoodsEntry, last_age,           SLE_UINT8),
	SLE_CONDVAR(GoodsEntry, feeder_profit,      SLE_INT32,                 14, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _station_speclist_desc[] = {
	SLE_CONDVAR(StationSpecList, grfid,    SLE_UINT32, 27, SL_MAX_VERSION),
	SLE_CONDVAR(StationSpecList, localidx, SLE_UINT8,  27, SL_MAX_VERSION),

	SLE_END()
};


static void SaveLoad_STNS(Station *st)
{
	uint i;

	SlObject(st, _station_desc);
	for (i = 0; i != NUM_CARGO; i++) {
		SlObject(&st->goods[i], _goods_desc);

		/* In older versions, enroute_from had 0xFF as INVALID_STATION, is now 0xFFFF */
		if (CheckSavegameVersion(7) && st->goods[i].enroute_from == 0xFF) {
			st->goods[i].enroute_from = INVALID_STATION;
		}
		if (CheckSavegameVersion(44)) {
			/* Store position of the station where the goods come from, so there are no
			 * very high payments when stations get removed. However, if the station
			 * where the goods came from is already removed, the source information is
			 * lost. In that case we set it to the position of this station */
			st->goods[i].enroute_from_xy = IsValidStationID(st->goods[i].enroute_from) ? GetStation(st->goods[i].enroute_from)->xy : st->xy;
		}
	}

	if (st->num_specs != 0) {
		/* Allocate speclist memory when loading a game */
		if (st->speclist == NULL) st->speclist = CallocT<StationSpecList>(st->num_specs);
		for (i = 0; i < st->num_specs; i++) SlObject(&st->speclist[i], _station_speclist_desc);
	}
}

static void Save_STNS(void)
{
	Station *st;
	// Write the stations
	FOR_ALL_STATIONS(st) {
		SlSetArrayIndex(st->index);
		SlAutolength((AutolengthProc*)SaveLoad_STNS, st);
	}
}

static void Load_STNS(void)
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Station *st = new (index) Station();

		SaveLoad_STNS(st);

		// this means it's an oldstyle savegame without support for nonuniform stations
		if (st->train_tile != 0 && st->trainst_h == 0) {
			uint w = GB(st->trainst_w, 4, 4);
			uint h = GB(st->trainst_w, 0, 4);

			if (GetRailStationAxis(st->train_tile) == AXIS_Y) uintswap(w, h);
			st->trainst_w = w;
			st->trainst_h = h;
		}
	}

	/* This is to ensure all pointers are within the limits of _stations_size */
	if (_station_tick_ctr > GetMaxStationIndex()) _station_tick_ctr = 0;
}

static void Save_ROADSTOP(void)
{
	RoadStop *rs;

	FOR_ALL_ROADSTOPS(rs) {
		SlSetArrayIndex(rs->index);
		SlObject(rs, _roadstop_desc);
	}
}

static void Load_ROADSTOP(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		RoadStop *rs = new (index) RoadStop(INVALID_TILE);

		SlObject(rs, _roadstop_desc);
	}
}

extern const ChunkHandler _station_chunk_handlers[] = {
	{ 'STNS', Save_STNS,      Load_STNS,      CH_ARRAY },
	{ 'ROAD', Save_ROADSTOP,  Load_ROADSTOP,  CH_ARRAY | CH_LAST},
};


