/* $Id$ */

/** @file station_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "string.h"
#include "aircraft.h"
#include "bridge_map.h"
#include "cmd_helper.h"
#include "debug.h"
#include "functions.h"
#include "landscape.h"
#include "station_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "station.h"
#include "gfx.h"
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
#include "roadveh.h"
#include "water_map.h"
#include "industry_map.h"
#include "newgrf_callbacks.h"
#include "newgrf_station.h"
#include "yapf/yapf.h"
#include "date.h"
#include "helpers.hpp"
#include "misc/autoptr.hpp"
#include "road_type.h"
#include "road_internal.h" /* For drawing catenary/checking road removal */
#include "cargotype.h"
#include "strings.h"
#include "autoslope.h"
#include "transparency.h"
#include "water.h"
#include "station_gui.h"

DEFINE_OLD_POOL_GENERIC(Station, Station)
DEFINE_OLD_POOL_GENERIC(RoadStop, RoadStop)


/**
 * Check whether the given tile is a hangar.
 * @param t the tile to of whether it is a hangar.
 * @pre IsTileType(t, MP_STATION)
 * @return true if and only if the tile is a hangar.
 */
bool IsHangar(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));

	const Station *st = GetStationByTile(t);
	const AirportFTAClass *apc = st->Airport();

	for (uint i = 0; i < apc->nof_depots; i++) {
		if (st->airport_tile + ToTileIndexDiff(apc->airport_depots[i]) == t) return true;
	}

	return false;
}

RoadStop* GetRoadStopByTile(TileIndex tile, RoadStop::Type type)
{
	const Station* st = GetStationByTile(tile);

	for (RoadStop *rs = st->GetPrimaryRoadStop(type);; rs = rs->next) {
		if (rs->xy == tile) return rs;
		assert(rs->next != NULL);
	}
}


static uint GetNumRoadStopsInStation(const Station* st, RoadStop::Type type)
{
	uint num = 0;

	assert(st != NULL);
	for (const RoadStop *rs = st->GetPrimaryRoadStop(type); rs != NULL; rs = rs->next) {
		num++;
	}

	return num;
}


/** Calculate the radius of the station. Basicly it is the biggest
 *  radius that is available within the station
 * @param st Station to query
 * @return the so calculated radius
 */
static uint FindCatchmentRadius(const Station* st)
{
	uint ret = CA_NONE;

	if (st->bus_stops   != NULL) ret = max<uint>(ret, CA_BUS);
	if (st->truck_stops != NULL) ret = max<uint>(ret, CA_TRUCK);
	if (st->train_tile  != 0)    ret = max<uint>(ret, CA_TRAIN);
	if (st->dock_tile   != 0)    ret = max<uint>(ret, CA_DOCK);
	if (st->airport_tile)        ret = max<uint>(ret, st->Airport()->catchment);

	return ret;
}

#define CHECK_STATIONS_ERR ((Station*)-1)

static Station* GetStationAround(TileIndex tile, int w, int h, StationID closest_station)
{
	/* check around to see if there's any stations there */
	BEGIN_TILE_LOOP(tile_cur, w + 2, h + 2, tile - TileDiffXY(1, 1))
		if (IsTileType(tile_cur, MP_STATION)) {
			StationID t = GetStationIndex(tile_cur);

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

/**
 * Function to check whether the given tile matches some criterion.
 * @param tile the tile to check
 * @return true if it matches, false otherwise
 */
typedef bool (*CMSAMatcher)(TileIndex tile);

/**
 * Counts the numbers of tiles matching a specific type in the area around
 * @param tile the center tile of the 'count area'
 * @param type the type of tile searched for
 * @param industry when type == MP_INDUSTRY, the type of the industry,
 *                 in all other cases this parameter is ignored
 * @return the result the noumber of matching tiles around
 */
static int CountMapSquareAround(TileIndex tile, CMSAMatcher cmp)
{
	int num = 0;

	for (int dx = -3; dx <= 3; dx++) {
		for (int dy = -3; dy <= 3; dy++) {
			if (cmp(TILE_MASK(tile + TileDiffXY(dx, dy)))) num++;
		}
	}

	return num;
}

/**
 * Check whether the tile is a mine.
 * @param tile the tile to investigate.
 * @return true if and only if the tile is a mine
 */
static bool CMSAMine(TileIndex tile)
{
	/* No industry */
	if (!IsTileType(tile, MP_INDUSTRY)) return false;

	const Industry *ind = GetIndustryByTile(tile);

	/* No extractive industry */
	if ((GetIndustrySpec(ind->type)->life_type & INDUSTRYLIFE_EXTRACTIVE) == 0) return false;

	for (uint i = 0; i < lengthof(ind->produced_cargo); i++) {
		/* The industry extracts something non-liquid, i.e. no oil or plastic, so it is a mine */
		if (ind->produced_cargo[i] != CT_INVALID && (GetCargo(ind->produced_cargo[i])->classes & CC_LIQUID) == 0) return true;
	}

	return false;
}

/**
 * Check whether the tile is water.
 * @param tile the tile to investigate.
 * @return true if and only if the tile is a mine
 */
static bool CMSAWater(TileIndex tile)
{
	return IsTileType(tile, MP_WATER) && IsWater(tile);
}

/**
 * Check whether the tile is a tree.
 * @param tile the tile to investigate.
 * @return true if and only if the tile is a mine
 */
static bool CMSATree(TileIndex tile)
{
	return IsTileType(tile, MP_TREES);
}

/**
 * Check whether the tile is a forest.
 * @param tile the tile to investigate.
 * @return true if and only if the tile is a mine
 */
static bool CMSAForest(TileIndex tile)
{
	/* No industry */
	if (!IsTileType(tile, MP_INDUSTRY)) return false;

	const Industry *ind = GetIndustryByTile(tile);

	/* No extractive industry */
	if ((GetIndustrySpec(ind->type)->life_type & INDUSTRYLIFE_ORGANIC) == 0) return false;

	for (uint i = 0; i < lengthof(ind->produced_cargo); i++) {
		/* The industry produces wood. */
		if (ind->produced_cargo[i] != CT_INVALID && GetCargo(ind->produced_cargo[i])->label == 'WOOD') return true;
	}

	return false;
}

#define M(x) ((x) - STR_SV_STNAME)

enum StationNaming {
	STATIONNAMING_RAIL = 0,
	STATIONNAMING_ROAD = 0,
	STATIONNAMING_AIRPORT,
	STATIONNAMING_OILRIG,
	STATIONNAMING_DOCK,
	STATIONNAMING_BUOY,
	STATIONNAMING_HELIPORT,
};

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
	unsigned long tmp;

	{
		Station *s;

		FOR_ALL_STATIONS(s) {
			if (s != st && s->town == t) {
				uint str = M(s->string_id);
				if (str <= 0x20) {
					if (str == M(STR_SV_STNAME_FOREST))
						str = M(STR_SV_STNAME_WOODS);
					ClrBit(free_names, str);
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
	if (HasBit(free_names, M(STR_SV_STNAME_MINES))) {
		if (CountMapSquareAround(tile, CMSAMine) >= 2) {
			found = M(STR_SV_STNAME_MINES);
			goto done;
		}
	}

	/* check close enough to town to get central as name? */
	if (DistanceMax(tile, t->xy) < 8) {
		found = M(STR_SV_STNAME);
		if (HasBit(free_names, M(STR_SV_STNAME))) goto done;

		found = M(STR_SV_STNAME_CENTRAL);
		if (HasBit(free_names, M(STR_SV_STNAME_CENTRAL))) goto done;
	}

	/* Check lakeside */
	if (HasBit(free_names, M(STR_SV_STNAME_LAKESIDE)) &&
			DistanceFromEdge(tile) < 20 &&
			CountMapSquareAround(tile, CMSAWater) >= 5) {
		found = M(STR_SV_STNAME_LAKESIDE);
		goto done;
	}

	/* Check woods */
	if (HasBit(free_names, M(STR_SV_STNAME_WOODS)) && (
				CountMapSquareAround(tile, CMSATree) >= 8 ||
				CountMapSquareAround(tile, CMSAForest) >= 2)
			) {
		found = _opt.landscape == LT_TROPIC ?
			M(STR_SV_STNAME_FOREST) : M(STR_SV_STNAME_WOODS);
		goto done;
	}

	/* check elevation compared to town */
	{
		uint z = GetTileZ(tile);
		uint z2 = GetTileZ(t->xy);
		if (z < z2) {
			found = M(STR_SV_STNAME_VALLEY);
			if (HasBit(free_names, M(STR_SV_STNAME_VALLEY))) goto done;
		} else if (z > z2) {
			found = M(STR_SV_STNAME_HEIGHTS);
			if (HasBit(free_names, M(STR_SV_STNAME_HEIGHTS))) goto done;
		}
	}

	/* check direction compared to town */
	{
		static const int8 _direction_and_table[] = {
			~( (1 << M(STR_SV_STNAME_WEST)) | (1 << M(STR_SV_STNAME_EAST)) | (1 << M(STR_SV_STNAME_NORTH)) ),
			~( (1 << M(STR_SV_STNAME_SOUTH)) | (1 << M(STR_SV_STNAME_WEST)) | (1 << M(STR_SV_STNAME_NORTH)) ),
			~( (1 << M(STR_SV_STNAME_SOUTH)) | (1 << M(STR_SV_STNAME_EAST)) | (1 << M(STR_SV_STNAME_NORTH)) ),
			~( (1 << M(STR_SV_STNAME_SOUTH)) | (1 << M(STR_SV_STNAME_WEST)) | (1 << M(STR_SV_STNAME_EAST)) ),
		};

		free_names &= _direction_and_table[
			(TileX(tile) < TileX(t->xy)) +
			(TileY(tile) < TileY(t->xy)) * 2];
	}

	tmp = free_names & ((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 12) | (1 << 26) | (1 << 27) | (1 << 28) | (1 << 29) | (1 << 30));
	found = (tmp == 0) ? M(STR_SV_STNAME_FALLBACK) : FindFirstBit(tmp);

done:
	st->string_id = found + STR_SV_STNAME;
	return true;
}
#undef M

static Station* GetClosestStationFromTile(TileIndex tile)
{
	uint threshold = 8;
	Station* best_station = NULL;
	Station* st;

	FOR_ALL_STATIONS(st) {
		if (st->facilities == 0 && st->owner == _current_player) {
			uint cur_dist = DistanceManhattan(tile, st->xy);

			if (cur_dist < threshold) {
				threshold = cur_dist;
				best_station = st;
			}
		}
	}

	return best_station;
}

/** Update the virtual coords needed to draw the station sign.
 * @param st Station to update for.
 */
static void UpdateStationVirtCoord(Station *st)
{
	Point pt = RemapCoords2(TileX(st->xy) * TILE_SIZE, TileY(st->xy) * TILE_SIZE);

	pt.y -= 32;
	if (st->facilities & FACIL_AIRPORT && st->airport_type == AT_OILRIG) pt.y -= 16;

	SetDParam(0, st->index);
	SetDParam(1, st->facilities);
	UpdateViewportSignPos(&st->sign, pt.x, pt.y, STR_305C_0);
}

/** Update the virtual coords needed to draw the station sign for all stations. */
void UpdateAllStationVirtCoord()
{
	Station* st;

	FOR_ALL_STATIONS(st) {
		UpdateStationVirtCoord(st);
	}
}

/**
 * Update the station virt coords while making the modified parts dirty.
 *
 * This function updates the virt coords and mark the modified parts as dirty
 *
 * @param st The station to update the virt coords
 * @ingroup dirty
 */
static void UpdateStationVirtCoordDirty(Station *st)
{
	st->MarkDirty();
	UpdateStationVirtCoord(st);
	st->MarkDirty();
}

/** Get a mask of the cargo types that the station accepts.
 * @param st Station to query
 * @return the expected mask
 */
static uint GetAcceptanceMask(const Station *st)
{
	uint mask = 0;

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		if (HasBit(st->goods[i].acceptance_pickup, GoodsEntry::ACCEPTANCE)) mask |= 1 << i;
	}
	return mask;
}

/** Items contains the two cargo names that are to be accepted or rejected.
 * msg is the string id of the message to display.
 */
static void ShowRejectOrAcceptNews(const Station *st, uint num_items, CargoID *cargo, StringID msg)
{
	for (uint i = 0; i < num_items; i++) {
		SetDParam(i + 1, GetCargo(cargo[i])->name);
	}

	SetDParam(0, st->index);
	AddNewsItem(msg, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT | NF_TILE, NT_ACCEPTANCE, 0), st->xy, 0);
}

/**
* Get a list of the cargo types being produced around the tile (in a rectangle).
* @param produced: Destination array of produced cargo
* @param tile: Center of the search area
* @param w: Width of the center
* @param h: Height of the center
* @param rad: Radius of the search area
*/
void GetProductionAroundTiles(AcceptedCargo produced, TileIndex tile,
	int w, int h, int rad)
{
	memset(produced, 0, sizeof(AcceptedCargo));

	int x = TileX(tile);
	int y = TileY(tile);

	/* expand the region by rad tiles on each side
	 * while making sure that we remain inside the board. */
	int x2 = min(x + w + rad, MapSizeX());
	int x1 = max(x - rad, 0);

	int y2 = min(y + h + rad, MapSizeY());
	int y1 = max(y - rad, 0);

	assert(x1 < x2);
	assert(y1 < y2);
	assert(w > 0);
	assert(h > 0);

	for (int yc = y1; yc != y2; yc++) {
		for (int xc = x1; xc != x2; xc++) {
			if (!(IsInsideBS(xc, x, w) && IsInsideBS(yc, y, h))) {
				TileIndex tile = TileXY(xc, yc);

				GetProducedCargoProc *gpc = _tile_type_procs[GetTileType(tile)]->get_produced_cargo_proc;
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

/**
* Get a list of the cargo types that are accepted around the tile.
* @param accepts: Destination array of accepted cargo
* @param tile: Center of the search area
* @param w: Width of the center
* @param h: Height of the center
* @param rad: Radius of the rectangular search area
*/
void GetAcceptanceAroundTiles(AcceptedCargo accepts, TileIndex tile,
	int w, int h, int rad)
{
	memset(accepts, 0, sizeof(AcceptedCargo));

	int x = TileX(tile);
	int y = TileY(tile);

	/* expand the region by rad tiles on each side
	 * while making sure that we remain inside the board. */
	int x2 = min(x + w + rad, MapSizeX());
	int y2 = min(y + h + rad, MapSizeY());
	int x1 = max(x - rad, 0);
	int y1 = max(y - rad, 0);

	assert(x1 < x2);
	assert(y1 < y2);
	assert(w > 0);
	assert(h > 0);

	for (int yc = y1; yc != y2; yc++) {
		for (int xc = x1; xc != x2; xc++) {
			TileIndex tile = TileXY(xc, yc);

			if (!IsTileType(tile, MP_STATION)) {
				AcceptedCargo ac;

				GetAcceptedCargo(tile, ac);
				for (uint i = 0; i < lengthof(ac); ++i) accepts[i] += ac[i];
			}
		}
	}
}

struct ottd_Rectangle {
	uint min_x;
	uint min_y;
	uint max_x;
	uint max_y;
};

static inline void MergePoint(ottd_Rectangle* rect, TileIndex tile)
{
	uint x = TileX(tile);
	uint y = TileY(tile);

	if (rect->min_x > x) rect->min_x = x;
	if (rect->min_y > y) rect->min_y = y;
	if (rect->max_x < x) rect->max_x = x;
	if (rect->max_y < y) rect->max_y = y;
}

/** Update the acceptance for a station.
 * @param st Station to update
 * @param show_msg controls whether to display a message that acceptance was changed.
 */
static void UpdateStationAcceptance(Station *st, bool show_msg)
{
	/* Don't update acceptance for a buoy */
	if (st->IsBuoy()) return;

	ottd_Rectangle rect;
	rect.min_x = MapSizeX();
	rect.min_y = MapSizeY();
	rect.max_x = 0;
	rect.max_y = 0;

	/* old accepted goods types */
	uint old_acc = GetAcceptanceMask(st);

	/* Put all the tiles that span an area in the table. */
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

	for (const RoadStop *rs = st->bus_stops; rs != NULL; rs = rs->next) {
		MergePoint(&rect, rs->xy);
	}

	for (const RoadStop *rs = st->truck_stops; rs != NULL; rs = rs->next) {
		MergePoint(&rect, rs->xy);
	}

	/* And retrieve the acceptance. */
	AcceptedCargo accepts;
	if (rect.max_x >= rect.min_x) {
		GetAcceptanceAroundTiles(
			accepts,
			TileXY(rect.min_x, rect.min_y),
			rect.max_x - rect.min_x + 1,
			rect.max_y - rect.min_y + 1,
			_patches.modified_catchment ? FindCatchmentRadius(st) : 4
		);
	} else {
		memset(accepts, 0, sizeof(accepts));
	}

	/* Adjust in case our station only accepts fewer kinds of goods */
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		uint amt = min(accepts[i], 15);

		/* Make sure the station can accept the goods type. */
		bool is_passengers = IsCargoInClass(i, CC_PASSENGERS);
		if ((!is_passengers && !(st->facilities & (byte)~FACIL_BUS_STOP)) ||
				(is_passengers && !(st->facilities & (byte)~FACIL_TRUCK_STOP)))
			amt = 0;

		SB(st->goods[i].acceptance_pickup, GoodsEntry::ACCEPTANCE, 1, amt >= 8);
	}

	/* Only show a message in case the acceptance was actually changed. */
	uint new_acc = GetAcceptanceMask(st);
	if (old_acc == new_acc)
		return;

	/* show a message to report that the acceptance was changed? */
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
			if (HasBit(new_acc, i)) {
				if (!HasBit(old_acc, i) && num_acc < lengthof(accepts)) {
					/* New cargo is accepted */
					accepts[num_acc++] = i;
				}
			} else {
				if (HasBit(old_acc, i) && num_rej < lengthof(rejects)) {
					/* Old cargo is no longer accepted */
					rejects[num_rej++] = i;
				}
			}
		}

		/* Show news message if there are any changes */
		if (num_acc > 0) ShowRejectOrAcceptNews(st, num_acc, accepts, accept_msg[num_acc - 1]);
		if (num_rej > 0) ShowRejectOrAcceptNews(st, num_rej, rejects, reject_msg[num_rej - 1]);
	}

	/* redraw the station view since acceptance changed */
	InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_ACCEPTLIST);
}

static void UpdateStationSignCoord(Station *st)
{
	const StationRect *r = &st->rect;

	if (r->IsEmpty()) return; /* no tiles belong to this station */

	/* clamp sign coord to be inside the station rect */
	st->xy = TileXY(ClampU(TileX(st->xy), r->left, r->right), ClampU(TileY(st->xy), r->top, r->bottom));
	UpdateStationVirtCoordDirty(st);
}

/** This is called right after a station was deleted.
 * It checks if the whole station is free of substations, and if so, the station will be
 * deleted after a little while.
 * @param st Station
 */
static void DeleteStationIfEmpty(Station *st)
{
	if (st->facilities == 0) {
		st->delete_ctr = 0;
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
	/* station remains but it probably lost some parts - station sign should stay in the station boundaries */
	UpdateStationSignCoord(st);
}

static CommandCost ClearTile_Station(TileIndex tile, byte flags);

/** Tries to clear the given area.
 * @param tile TileIndex to start check
 * @param w width of search area
 * @param h height of search area
 * @param flags operation to perform
 * @param invalid_dirs prohibited directions
 * @param station StationID to be queried and returned if available
 * @param check_clear if clearing tile should be performed (in wich case, cost will be added)
 * @return the cost in case of success, or an error code if it failed.
 */
CommandCost CheckFlatLandBelow(TileIndex tile, uint w, uint h, uint flags, uint invalid_dirs, StationID *station, bool check_clear = true)
{
	CommandCost cost;
	int allowed_z = -1;

	BEGIN_TILE_LOOP(tile_cur, w, h, tile) {
		if (MayHaveBridgeAbove(tile_cur) && IsBridgeAbove(tile_cur)) {
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		}

		if (!EnsureNoVehicleOnGround(tile_cur)) return CMD_ERROR;

		uint z;
		Slope tileh = GetTileSlope(tile_cur, &z);

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

		int flat_z = z;
		if (tileh != SLOPE_FLAT) {
			/* need to check so the entrance to the station is not pointing at a slope. */
			if ((invalid_dirs & 1 && !(tileh & SLOPE_NE) && (uint)w_cur == w) ||
					(invalid_dirs & 2 && !(tileh & SLOPE_SE) && h_cur == 1) ||
					(invalid_dirs & 4 && !(tileh & SLOPE_SW) && w_cur == 1) ||
					(invalid_dirs & 8 && !(tileh & SLOPE_NW) && (uint)h_cur == h)) {
				return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
			}
			cost.AddCost(_price.terraform);
			flat_z += TILE_HEIGHT;
		}

		/* get corresponding flat level and make sure that all parts of the station have the same level. */
		if (allowed_z == -1) {
			/* first tile */
			allowed_z = flat_z;
		} else if (allowed_z != flat_z) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
		}

		/* if station is set, then we have special handling to allow building on top of already existing stations.
		 * so station points to INVALID_STATION if we can build on any station.
		 * Or it points to a station if we're only allowed to build on exactly that station. */
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
		} else if (check_clear) {
			CommandCost ret = DoCommand(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost.AddCost(ret);
		}
	} END_TILE_LOOP(tile_cur, w, h, tile)

	return cost;
}

static bool CanExpandRailroadStation(const Station* st, uint* fin, Axis axis)
{
	uint curw = st->trainst_w;
	uint curh = st->trainst_h;
	TileIndex tile = fin[0];
	uint w = fin[1];
	uint h = fin[2];

	if (_patches.nonuniform_stations) {
		/* determine new size of train station region.. */
		int x = min(TileX(st->train_tile), TileX(tile));
		int y = min(TileY(st->train_tile), TileY(tile));
		curw = max(TileX(st->train_tile) + curw, TileX(tile) + w) - x;
		curh = max(TileY(st->train_tile) + curh, TileY(tile) + h) - y;
		tile = TileXY(x, y);
	} else {
		/* do not allow modifying non-uniform stations,
		 * the uniform-stations code wouldn't handle it well */
		BEGIN_TILE_LOOP(t, st->trainst_w, st->trainst_h, st->train_tile)
			if (!st->TileBelongsToRailStation(t)) { // there may be adjoined station
				_error_message = STR_306D_NONUNIFORM_STATIONS_DISALLOWED;
				return false;
			}
		END_TILE_LOOP(t, st->trainst_w, st->trainst_h, st->train_tile)

		/* check so the orientation is the same */
		if (GetRailStationAxis(st->train_tile) != axis) {
			_error_message = STR_306D_NONUNIFORM_STATIONS_DISALLOWED;
			return false;
		}

		/* check if the new station adjoins the old station in either direction */
		if (curw == w && st->train_tile == tile + TileDiffXY(0, h)) {
			/* above */
			curh += h;
		} else if (curw == w && st->train_tile == tile - TileDiffXY(0, curh)) {
			/* below */
			tile -= TileDiffXY(0, curh);
			curh += h;
		} else if (curh == h && st->train_tile == tile + TileDiffXY(w, 0)) {
			/* to the left */
			curw += w;
		} else if (curh == h && st->train_tile == tile - TileDiffXY(curw, 0)) {
			/* to the right */
			tile -= TileDiffXY(curw, 0);
			curw += w;
		} else {
			_error_message = STR_306D_NONUNIFORM_STATIONS_DISALLOWED;
			return false;
		}
	}
	/* make sure the final size is not too big. */
	if (curw > _patches.station_spread || curh > _patches.station_spread) {
		_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
		return false;
	}

	/* now tile contains the new value for st->train_tile
	 * curw, curh contain the new value for width and height */
	fin[0] = tile;
	fin[1] = curw;
	fin[2] = curh;
	return true;
}

static inline byte *CreateSingle(byte *layout, int n)
{
	int i = n;
	do *layout++ = 0; while (--i);
	layout[((n - 1) >> 1) - n] = 2;
	return layout;
}

static inline byte *CreateMulti(byte *layout, int n, byte b)
{
	int i = n;
	do *layout++ = b; while (--i);
	if (n > 4) {
		layout[0 - n] = 0;
		layout[n - 1 - n] = 0;
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
 * @param flags operation to perform
 * @param p1 various bitstuffed elements
 * - p1 = (bit  0)    - orientation (Axis)
 * - p1 = (bit  8-15) - number of tracks
 * - p1 = (bit 16-23) - platform length
 * - p1 = (bit 24)    - allow stations directly adjacent to other stations.
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 3) - railtype (p2 & 0xF)
 * - p2 = (bit  8-15) - custom station class
 * - p2 = (bit 16-23) - custom station id
 */
CommandCost CmdBuildRailroadStation(TileIndex tile_org, uint32 flags, uint32 p1, uint32 p2)
{
	int w_org, h_org;
	CommandCost ret;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Does the authority allow this? */
	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile_org)) return CMD_ERROR;
	if (!ValParamRailtype(p2 & 0xF)) return CMD_ERROR;

	/* unpack parameters */
	Axis axis = Extract<Axis, 0>(p1);
	uint numtracks = GB(p1,  8, 8);
	uint plat_len  = GB(p1, 16, 8);
	if (axis == AXIS_X) {
		w_org = plat_len;
		h_org = numtracks;
	} else {
		h_org = plat_len;
		w_org = numtracks;
	}

	if (h_org > _patches.station_spread || w_org > _patches.station_spread) return CMD_ERROR;

	/* these values are those that will be stored in train_tile and station_platforms */
	uint finalvalues[3];
	finalvalues[0] = tile_org;
	finalvalues[1] = w_org;
	finalvalues[2] = h_org;

	/* Make sure the area below consists of clear tiles. (OR tiles belonging to a certain rail station) */
	StationID est = INVALID_STATION;
	/* If DC_EXEC is in flag, do not want to pass it to CheckFlatLandBelow, because of a nice bug
	 * for detail info, see:
	 * https://sourceforge.net/tracker/index.php?func=detail&aid=1029064&group_id=103924&atid=636365 */
	ret = CheckFlatLandBelow(tile_org, w_org, h_org, flags & ~DC_EXEC, 5 << axis, _patches.nonuniform_stations ? &est : NULL);
	if (CmdFailed(ret)) return ret;
	CommandCost cost(ret.GetCost() + (numtracks * _price.train_station_track + _price.train_station_length) * plat_len);

	Station *st = NULL;
	bool check_surrounding = true;

	if (_patches.adjacent_stations) {
		if (est != INVALID_STATION) {
			if (HasBit(p1, 24)) {
				/* You can't build an adjacent station over the top of one that
				 * already exists. */
				return_cmd_error(STR_MUST_REMOVE_RAILWAY_STATION_FIRST);
			} else {
				/* Extend the current station, and don't check whether it will
				 * be near any other stations. */
				st = GetStation(est);
				check_surrounding = false;
			}
		} else {
			/* There's no station here. Don't check the tiles surrounding this
			 * one if the player wanted to build an adjacent station. */
			if (HasBit(p1, 24)) check_surrounding = false;
		}
	}

	if (check_surrounding) {
		/* Make sure there are no similar stations around us. */
		st = GetStationAround(tile_org, w_org, h_org, est);
		if (st == CHECK_STATIONS_ERR) return CMD_ERROR;
	}

	/* See if there is a deleted station close to us. */
	if (st == NULL) st = GetClosestStationFromTile(tile_org);

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	 * to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		/* Reuse an existing station. */
		if (st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (st->train_tile != 0) {
			/* check if we want to expanding an already existing station? */
			if (_is_old_ai_player || !_patches.join_stations)
				return_cmd_error(STR_3005_TOO_CLOSE_TO_ANOTHER_RAILROAD);
			if (!CanExpandRailroadStation(st, finalvalues, axis))
				return CMD_ERROR;
		}

		/* XXX can't we pack this in the "else" part of the if above? */
		if (!st->rect.BeforeAddRect(tile_org, w_org, h_org, StationRect::ADD_TEST)) return CMD_ERROR;
	} else {
		/* allocate and initialize new station */
		st = new Station(tile_org);
		if (st == NULL) return_cmd_error(STR_3008_TOO_MANY_STATIONS_LOADING);

		/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
		st_auto_delete = st;

		st->town = ClosestTownFromTile(tile_org, (uint)-1);
		if (!GenerateStationName(st, tile_org, STATIONNAMING_RAIL)) return CMD_ERROR;

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SetBit(st->town->have_ratings, _current_player);
		}
	}

	/* Check if the given station class is valid */
	if (GB(p2, 8, 8) >= STAT_CLASS_MAX) return CMD_ERROR;

	/* Check if we can allocate a custom stationspec to this station */
	const StationSpec *statspec = GetCustomStationSpec((StationClassID)GB(p2, 8, 8), GB(p2, 16, 8));
	int specindex = AllocateSpecToStation(statspec, st, flags & DC_EXEC);
	if (specindex == -1) return CMD_ERROR;

	if (statspec != NULL) {
		/* Perform NewStation checks */

		/* Check if the station size is permitted */
		if (HasBit(statspec->disallowed_platforms, numtracks - 1) || HasBit(statspec->disallowed_lengths, plat_len - 1)) {
			return CMD_ERROR;
		}

		/* Check if the station is buildable */
		if (HasBit(statspec->callbackmask, CBM_STATION_AVAIL) && GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE) == 0) {
			return CMD_ERROR;
		}
	}

	if (flags & DC_EXEC) {
		TileIndexDiff tile_delta;
		byte *layout_ptr;
		byte numtracks_orig;
		Track track;

		/* Now really clear the land below the station
		 * It should never return CMD_ERROR.. but you never know ;)
		 * (a bit strange function name for it, but it really does clear the land, when DC_EXEC is in flags) */
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
				MakeRailStation(tile, st->owner, st->index, axis, layout & ~1, (RailType)GB(p2, 0, 4));
				SetCustomStationSpecIndex(tile, specindex);
				SetStationTileRandomBits(tile, GB(Random(), 0, 4));

				if (statspec != NULL) {
					/* Use a fixed axis for GetPlatformInfo as our platforms / numtracks are always the right way around */
					uint32 platinfo = GetPlatformInfo(AXIS_X, 0, plat_len, numtracks_orig, plat_len - w, numtracks_orig - numtracks, false);

					/* As the station is not yet completely finished, the station does not yet exist. */
					uint16 callback = GetStationCallback(CBID_STATION_TILE_LAYOUT, platinfo, 0, statspec, NULL, tile);
					if (callback != CALLBACK_FAILED && callback < 8) SetStationGfx(tile, (callback & ~1) + axis);
				}

				tile += tile_delta;
			} while (--w);
			SetSignalsOnBothDir(tile_org, track);
			YapfNotifyTrackLayoutChange(tile_org, track);
			tile_org += tile_delta ^ TileDiffXY(1, 1); // perpendicular to tile_delta
		} while (--numtracks);

		st->MarkTilesDirty(false);
		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_TRAINS);
		/* success, so don't delete the new station */
		st_auto_delete.Detach();
	}

	return cost;
}

static void MakeRailwayStationAreaSmaller(Station *st)
{
	uint w = st->trainst_w;
	uint h = st->trainst_h;
	TileIndex tile = st->train_tile;

restart:

	/* too small? */
	if (w != 0 && h != 0) {
		/* check the left side, x = constant, y changes */
		for (uint i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(0, i));) {
			/* the left side is unused? */
			if (++i == h) {
				tile += TileDiffXY(1, 0);
				w--;
				goto restart;
			}
		}

		/* check the right side, x = constant, y changes */
		for (uint i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(w - 1, i));) {
			/* the right side is unused? */
			if (++i == h) {
				w--;
				goto restart;
			}
		}

		/* check the upper side, y = constant, x changes */
		for (uint i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(i, 0));) {
			/* the left side is unused? */
			if (++i == w) {
				tile += TileDiffXY(0, 1);
				h--;
				goto restart;
			}
		}

		/* check the lower side, y = constant, x changes */
		for (uint i = 0; !st->TileBelongsToRailStation(tile + TileDiffXY(i, h - 1));) {
			/* the left side is unused? */
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
 * @param flags operation to perform
 * @param p1 start_tile
 * @param p2 unused
 */
CommandCost CmdRemoveFromRailroadStation(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start = p1 == 0 ? tile : p1;

	/* Count of the number of tiles removed */
	int quantity = 0;

	if (tile >= MapSize() || start >= MapSize()) return CMD_ERROR;

	/* make sure sx,sy are smaller than ex,ey */
	int ex = TileX(tile);
	int ey = TileY(tile);
	int sx = TileX(start);
	int sy = TileY(start);
	if (ex < sx) Swap(ex, sx);
	if (ey < sy) Swap(ey, sy);
	tile = TileXY(sx, sy);

	int size_x = ex - sx + 1;
	int size_y = ey - sy + 1;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Do the action for every tile into the area */
	BEGIN_TILE_LOOP(tile2, size_x, size_y, tile) {
		/* Make sure the specified tile is a railroad station */
		if (!IsTileType(tile2, MP_STATION) || !IsRailwayStation(tile2)) {
			continue;
		}

		/* If there is a vehicle on ground, do not allow to remove (flood) the tile */
		if (!EnsureNoVehicleOnGround(tile2)) {
			continue;
		}

		/* Check ownership of station */
		Station *st = GetStationByTile(tile2);
		if (_current_player != OWNER_WATER && !CheckOwnership(st->owner)) {
			continue;
		}

		/* Do not allow removing from stations if non-uniform stations are not enabled
		 * The check must be here to give correct error message
 		 */
		if (!_patches.nonuniform_stations) return_cmd_error(STR_306D_NONUNIFORM_STATIONS_DISALLOWED);

		/* If we reached here, the tile is valid so increase the quantity of tiles we will remove */
		quantity++;

		if (flags & DC_EXEC) {
			uint specindex = GetCustomStationSpecIndex(tile2);
			Track track = GetRailStationTrack(tile2);
			DoClearSquare(tile2);
			st->rect.AfterRemoveTile(st, tile2);
			SetSignalsOnBothDir(tile2, track);
			YapfNotifyTrackLayoutChange(tile2, track);

			DeallocateSpecFromStation(st, specindex);

			/* now we need to make the "spanned" area of the railway station smaller
			 * if we deleted something at the edges.
			 * we also need to adjust train_tile. */
			MakeRailwayStationAreaSmaller(st);
			st->MarkTilesDirty(false);
			UpdateStationSignCoord(st);

			/* if we deleted the whole station, delete the train facility. */
			if (st->train_tile == 0) {
				st->facilities &= ~FACIL_TRAIN;
				InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_TRAINS);
				UpdateStationVirtCoordDirty(st);
				DeleteStationIfEmpty(st);
			}
		}
	} END_TILE_LOOP(tile2, size_x, size_y, tile)

	/* If we've not removed any tiles, give an error */
	if (quantity == 0) return CMD_ERROR;

	return CommandCost(_price.remove_rail_station * quantity);
}


static CommandCost RemoveRailroadStation(Station *st, TileIndex tile, uint32 flags)
{
	/* if there is flooding and non-uniform stations are enabled, remove platforms tile by tile */
	if (_current_player == OWNER_WATER && _patches.nonuniform_stations)
		return DoCommand(tile, 0, 0, DC_EXEC, CMD_REMOVE_FROM_RAILROAD_STATION);

	/* Current player owns the station? */
	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	/* determine width and height of platforms */
	tile = st->train_tile;
	int w = st->trainst_w;
	int h = st->trainst_h;

	assert(w != 0 && h != 0);

	CommandCost cost;
	/* clear all areas of the station */
	do {
		int w_bak = w;
		do {
			// for nonuniform stations, only remove tiles that are actually train station tiles
			if (st->TileBelongsToRailStation(tile)) {
				if (!EnsureNoVehicleOnGround(tile))
					return CMD_ERROR;
				cost.AddCost(_price.remove_rail_station);
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

		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_TRAINS);
		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

/**
 * @param truck_station Determines whether a stop is RoadStop::BUS or RoadStop::TRUCK
 * @param st The Station to do the whole procedure for
 * @return a pointer to where to link a new RoadStop*
 */
static RoadStop **FindRoadStopSpot(bool truck_station, Station* st)
{
	RoadStop **primary_stop = (truck_station) ? &st->truck_stops : &st->bus_stops;

	if (*primary_stop == NULL) {
		/* we have no roadstop of the type yet, so write a "primary stop" */
		return primary_stop;
	} else {
		/* there are stops already, so append to the end of the list */
		RoadStop *stop = *primary_stop;
		while (stop->next != NULL) stop = stop->next;
		return &stop->next;
	}
}

/** Build a bus or truck stop
 * @param tile tile to build the stop at
 * @param flags operation to perform
 * @param p1 entrance direction (DiagDirection)
 * @param p2 bit 0: 0 for Bus stops, 1 for truck stops
 *           bit 1: 0 for normal, 1 for drive-through
 *           bit 2..4: the roadtypes
 *           bit 5: allow stations directly adjacent to other stations.
 */
CommandCost CmdBuildRoadStop(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	bool type = HasBit(p2, 0);
	bool is_drive_through = HasBit(p2, 1);
	bool build_over_road  = is_drive_through && IsTileType(tile, MP_ROAD) && GetRoadTileType(tile) == ROAD_TILE_NORMAL;
	bool town_owned_road  = build_over_road && IsTileOwner(tile, OWNER_TOWN);
	RoadTypes rts = (RoadTypes)GB(p2, 2, 3);

	if (!AreValidRoadTypes(rts) || !HasRoadTypesAvail(_current_player, rts)) return CMD_ERROR;

	/* Trams only have drive through stops */
	if (!is_drive_through && HasBit(rts, ROADTYPE_TRAM)) return CMD_ERROR;

	/* Saveguard the parameters */
	if (!IsValidDiagDirection((DiagDirection)p1)) return CMD_ERROR;
	/* If it is a drive-through stop check for valid axis */
	if (is_drive_through && !IsValidAxis((Axis)p1)) return CMD_ERROR;
	/* Road bits in the wrong direction */
	if (build_over_road && (GetAllRoadBits(tile) & ((Axis)p1 == AXIS_X ? ROAD_Y : ROAD_X)) != 0) return_cmd_error(STR_DRIVE_THROUGH_ERROR_DIRECTION);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile)) return CMD_ERROR;

	CommandCost cost;

	/* Not allowed to build over this road */
	if (build_over_road) {
		if (IsTileOwner(tile, OWNER_TOWN) && !_patches.road_stop_on_town_road) return_cmd_error(STR_DRIVE_THROUGH_ERROR_ON_TOWN_ROAD);
		if (GetRoadTileType(tile) != ROAD_TILE_NORMAL) return CMD_ERROR;

		/* Don't allow building the roadstop when vehicles are already driving on it */
		if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

		RoadTypes cur_rts = GetRoadTypes(tile);
		if (GetRoadOwner(tile, ROADTYPE_ROAD) != OWNER_TOWN && HasBit(cur_rts, ROADTYPE_ROAD) && !CheckOwnership(GetRoadOwner(tile, ROADTYPE_ROAD))) return CMD_ERROR;
		if (HasBit(cur_rts, ROADTYPE_TRAM) && !CheckOwnership(GetRoadOwner(tile, ROADTYPE_TRAM))) return CMD_ERROR;

		/* Do not remove roadtypes! */
		rts |= cur_rts;
	}
	cost = CheckFlatLandBelow(tile, 1, 1, flags, is_drive_through ? 5 << p1 : 1 << p1, NULL, !build_over_road);
	if (CmdFailed(cost)) return cost;

	Station *st = NULL;

	if (!_patches.adjacent_stations || !HasBit(p2, 5)) {
		st = GetStationAround(tile, 1, 1, INVALID_STATION);
		if (st == CHECK_STATIONS_ERR) return CMD_ERROR;
	}

	/* Find a station close to us */
	if (st == NULL) st = GetClosestStationFromTile(tile);

	/* give us a road stop in the list, and check if something went wrong */
	RoadStop *road_stop = new RoadStop(tile);
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
		if (st->owner != _current_player) {
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);
		}

		if (!st->rect.BeforeAddTile(tile, StationRect::ADD_TEST)) return CMD_ERROR;
	} else {
		/* allocate and initialize new station */
		st = new Station(tile);
		if (st == NULL) return_cmd_error(STR_3008_TOO_MANY_STATIONS_LOADING);

		/* ensure that in case of error (or no DC_EXEC) the new station gets deleted upon return */
		st_auto_delete = st;


		Town *t = st->town = ClosestTownFromTile(tile, (uint)-1);
		if (!GenerateStationName(st, tile, STATIONNAMING_ROAD)) return CMD_ERROR;

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SetBit(t->have_ratings, _current_player);
		}

		st->sign.width_1 = 0;
	}

	cost.AddCost((type) ? _price.build_truck_station : _price.build_bus_station);

	if (flags & DC_EXEC) {
		/* Insert into linked list of RoadStops */
		RoadStop **currstop = FindRoadStopSpot(type, st);
		*currstop = road_stop;

		/*initialize an empty station */
		st->AddFacility((type) ? FACIL_TRUCK_STOP : FACIL_BUS_STOP, tile);

		st->rect.BeforeAddTile(tile, StationRect::ADD_TRY);

		RoadStop::Type rs_type = type ? RoadStop::TRUCK : RoadStop::BUS;
		if (is_drive_through) {
			MakeDriveThroughRoadStop(tile, st->owner, st->index, rs_type, rts, (Axis)p1, town_owned_road);
		} else {
			MakeRoadStop(tile, st->owner, st->index, rs_type, rts, (DiagDirection)p1);
		}

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_ROADVEHS);
		/* success, so don't delete the new station and the new road stop */
		st_auto_delete.Detach();
		rs_auto_delete.Detach();
	}
	return cost;
}

/** Remove a bus station
 * @param st Station to remove
 * @param flags operation to perform
 * @param tile TileIndex been queried
 * @return cost or failure of operation
 */
static CommandCost RemoveRoadStop(Station *st, uint32 flags, TileIndex tile)
{
	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner)) {
		return CMD_ERROR;
	}

	bool is_truck = IsTruckStop(tile);

	RoadStop **primary_stop;
	RoadStop *cur_stop;
	if (is_truck) { // truck stop
		primary_stop = &st->truck_stops;
		cur_stop = GetRoadStopByTile(tile, RoadStop::TRUCK);
	} else {
		primary_stop = &st->bus_stops;
		cur_stop = GetRoadStopByTile(tile, RoadStop::BUS);
	}

	assert(cur_stop != NULL);

	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (*primary_stop == cur_stop) {
			/* removed the first stop in the list */
			*primary_stop = cur_stop->next;
			/* removed the only stop? */
			if (*primary_stop == NULL) {
				st->facilities &= (is_truck ? ~FACIL_TRUCK_STOP : ~FACIL_BUS_STOP);
			}
		} else {
			/* tell the predecessor in the list to skip this stop */
			RoadStop *pred = *primary_stop;
			while (pred->next != cur_stop) pred = pred->next;
			pred->next = cur_stop->next;
		}

		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_ROADVEHS);
		delete cur_stop;
		DoClearSquare(tile);
		st->rect.AfterRemoveTile(st, tile);

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return CommandCost((is_truck) ? _price.remove_truck_station : _price.remove_bus_station);
}

/** Remove a bus or truck stop
 * @param tile tile to remove the stop from
 * @param flags operation to perform
 * @param p1 not used
 * @param p2 bit 0: 0 for Bus stops, 1 for truck stops
 */
CommandCost CmdRemoveRoadStop(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* Make sure the specified tile is a road stop of the correct type */
	if (!IsTileType(tile, MP_STATION) || !IsRoadStop(tile) || (uint32)GetRoadStopType(tile) != p2) return CMD_ERROR;
	Station *st = GetStationByTile(tile);
	/* Save the stop info before it is removed */
	bool is_drive_through = IsDriveThroughStopTile(tile);
	RoadTypes rts = GetRoadTypes(tile);
	RoadBits road_bits = IsDriveThroughStopTile(tile) ?
			((GetRoadStopDir(tile) == DIAGDIR_NE) ? ROAD_X : ROAD_Y) :
			DiagDirToRoadBits(GetRoadStopDir(tile));
	bool is_towns_road = is_drive_through && GetStopBuiltOnTownRoad(tile);

	CommandCost ret = RemoveRoadStop(st, flags, tile);

	/* If the stop was a drive-through stop replace the road */
	if ((flags & DC_EXEC) && CmdSucceeded(ret) && is_drive_through) {
		/* Rebuild the drive throuhg road stop. As a road stop can only be
		 * removed by the owner of the roadstop, _current_player is the
		 * owner of the road stop. */
		MakeRoadNormal(tile, road_bits, rts, is_towns_road ? ClosestTownFromTile(tile, (uint)-1)->index : 0,
				is_towns_road ? OWNER_TOWN : _current_player, _current_player, _current_player);
	}

	return ret;
}

/* FIXME -- need to move to its corresponding Airport variable*/

/* Country Airfield (small) */
static const byte _airport_sections_country[] = {
	54, 53, 52, 65,
	58, 57, 56, 55,
	64, 63, 63, 62
};

/* City Airport (large) */
static const byte _airport_sections_town[] = {
	31,  9, 33,  9,  9, 32,
	27, 36, 29, 34,  8, 10,
	30, 11, 35, 13, 20, 21,
	51, 12, 14, 17, 19, 28,
	38, 13, 15, 16, 18, 39,
	26, 22, 23, 24, 25, 26
};

/* Metropolitain Airport (large) - 2 runways */
static const byte _airport_sections_metropolitan[] = {
	 31,  9, 33,  9,  9, 32,
	 27, 36, 29, 34,  8, 10,
	 30, 11, 35, 13, 20, 21,
	102,  8,  8,  8,  8, 28,
	 83, 84, 84, 84, 84, 83,
	 26, 23, 23, 23, 23, 26
};

/* International Airport (large) - 2 runways */
static const byte _airport_sections_international[] = {
	88, 89, 89, 89, 89, 89,  88,
	51,  8,  8,  8,  8,  8,  32,
	30,  8, 11, 27, 11,  8,  10,
	32,  8, 11, 27, 11,  8, 114,
	87,  8, 11, 85, 11,  8, 114,
	87,  8,  8,  8,  8,  8,  90,
	26, 23, 23, 23, 23, 23,  26
};

/* Intercontinental Airport (vlarge) - 4 runways */
static const byte _airport_sections_intercontinental[] = {
	102, 120,  89,  89,  89,  89,  89,  89, 118,
	120,  23,  23,  23,  23,  23,  23, 119, 117,
	 87,  54,  87,   8,   8,   8,   8,  51, 117,
	 87, 162,  87,  85, 116, 116,   8,   9,  10,
	 87,   8,   8,  11,  31,  11,   8, 160,  32,
	 32, 160,   8,  11,  27,  11,   8,   8,  10,
	 87,   8,   8,  11,  30,  11,   8,   8,  10,
	 87, 142,   8,  11,  29,  11,  10, 163,  10,
	 87, 164,  87,   8,   8,   8,  10,  37, 117,
	 87, 120,  89,  89,  89,  89,  89,  89, 119,
	121,  23,  23,  23,  23,  23,  23, 119,  37
};


/* Commuter Airfield (small) */
static const byte _airport_sections_commuter[] = {
	85, 30, 115, 115, 32,
	87, 8,    8,   8, 10,
	87, 11,  11,  11, 10,
	26, 23,  23,  23, 26
};

/* Heliport */
static const byte _airport_sections_heliport[] = {
	66,
};

/* Helidepot */
static const byte _airport_sections_helidepot[] = {
	124, 32,
	122, 123
};

/* Helistation */
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
 * @param flags operation to perform
 * @param p1 airport type, @see airport.h
 * @param p2 (bit 0) - allow airports directly adjacent to other airports.
 */
CommandCost CmdBuildAirport(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	bool airport_upgrade = true;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Check if a valid, buildable airport was chosen for construction */
	if (p1 > lengthof(_airport_sections) || !HasBit(GetValidAirports(), p1)) return CMD_ERROR;

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	Town *t = ClosestTownFromTile(tile, (uint)-1);

	/* Check if local auth refuses a new airport */
	{
		uint num = 0;
		const Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->town == t && st->facilities&FACIL_AIRPORT && st->airport_type != AT_OILRIG)
				num++;
		}
		if (num >= 2) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2035_LOCAL_AUTHORITY_REFUSES);
		}
	}

	const AirportFTAClass *afc = GetAirport(p1);
	int w = afc->size_x;
	int h = afc->size_y;

	CommandCost ret = CheckFlatLandBelow(tile, w, h, flags, 0, NULL);
	if (CmdFailed(ret)) return ret;
	CommandCost cost(ret.GetCost());

	Station *st = NULL;

	if (!_patches.adjacent_stations || !HasBit(p2, 0)) {
		st = GetStationAround(tile, w, h, INVALID_STATION);
		if (st == CHECK_STATIONS_ERR) return CMD_ERROR;
	}

	/* Find a station close to us */
	if (st == NULL) st = GetClosestStationFromTile(tile);

	if (w > _patches.station_spread || h > _patches.station_spread) {
		_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
		return CMD_ERROR;
	}

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	 * to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		if (st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!st->rect.BeforeAddRect(tile, w, h, StationRect::ADD_TEST)) return CMD_ERROR;

		if (st->airport_tile != 0)
			return_cmd_error(STR_300D_TOO_CLOSE_TO_ANOTHER_AIRPORT);
	} else {
		airport_upgrade = false;

		/* allocate and initialize new station */
		st = new Station(tile);
		if (st == NULL) return_cmd_error(STR_3008_TOO_MANY_STATIONS_LOADING);

		/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
		st_auto_delete = st;

		st->town = t;

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SetBit(t->have_ratings, _current_player);
		}

		st->sign.width_1 = 0;

		/* If only helicopters may use the airport generate a helicopter related (5)
		 * station name, otherwise generate a normal airport name (1) */
		if (!GenerateStationName(st, tile, !(afc->flags & AirportFTAClass::AIRPLANES) ? STATIONNAMING_HELIPORT : STATIONNAMING_AIRPORT)) {
			return CMD_ERROR;
		}
	}

	cost.AddCost(_price.build_airport * w * h);

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
				MakeAirport(tile_cur, st->owner, st->index, *b - ((*b < 67) ? 8 : 24));
				b++;
			} END_TILE_LOOP(tile_cur, w, h, tile)
		}

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		RebuildStationLists();
		InvalidateWindow(WC_STATION_LIST, st->owner);
		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_PLANES);
		/* success, so don't delete the new station */
		st_auto_delete.Detach();
	}

	return cost;
}

static CommandCost RemoveAirport(Station *st, uint32 flags)
{
	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	TileIndex tile = st->airport_tile;

	const AirportFTAClass *afc = st->Airport();
	int w = afc->size_x;
	int h = afc->size_y;

	CommandCost cost(w * h * _price.remove_airport);

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (!(v->type == VEH_AIRCRAFT && IsNormalAircraft(v))) continue;

		if (v->u.air.targetairport == st->index && v->u.air.state != FLYING) return CMD_ERROR;
	}

	BEGIN_TILE_LOOP(tile_cur, w, h, tile) {
		if (!EnsureNoVehicleOnGround(tile_cur)) return CMD_ERROR;

		if (flags & DC_EXEC) {
			DeleteAnimatedTile(tile_cur);
			DoClearSquare(tile_cur);
		}
	} END_TILE_LOOP(tile_cur, w, h, tile)

	if (flags & DC_EXEC) {
		for (uint i = 0; i < afc->nof_depots; ++i) {
			DeleteWindowById(
				WC_VEHICLE_DEPOT, tile + ToTileIndexDiff(afc->airport_depots[i])
			);
		}

		st->rect.AfterRemoveRect(st, tile, w, h);

		st->airport_tile = 0;
		st->facilities &= ~FACIL_AIRPORT;

		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_PLANES);
		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

/** Build a buoy.
 * @param tile tile where to place the bouy
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdBuildBuoy(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!IsWaterTile(tile) || tile == 0) return_cmd_error(STR_304B_SITE_UNSUITABLE);
	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	/* allocate and initialize new station */
	Station *st = new Station(tile);
	if (st == NULL) return_cmd_error(STR_3008_TOO_MANY_STATIONS_LOADING);

	/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
	AutoPtrT<Station> st_auto_delete(st);

	st->town = ClosestTownFromTile(tile, (uint)-1);
	st->sign.width_1 = 0;

	if (!GenerateStationName(st, tile, STATIONNAMING_BUOY)) return CMD_ERROR;

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
		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_SHIPS);
		/* success, so don't delete the new station */
		st_auto_delete.Detach();
	}

	return CommandCost(_price.build_dock);
}

/* Checks if any ship is servicing the buoy specified. Returns yes or no */
static bool CheckShipsOnBuoy(Station *st)
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_SHIP) {
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

static CommandCost RemoveBuoy(Station *st, uint32 flags)
{
	/* XXX: strange stuff */
	if (!IsValidPlayer(_current_player))  return_cmd_error(INVALID_STRING_ID);

	TileIndex tile = st->dock_tile;

	if (CheckShipsOnBuoy(st))   return_cmd_error(STR_BUOY_IS_IN_USE);
	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		st->dock_tile = 0;
		/* Buoys are marked in the Station struct by this flag. Yes, it is this
		 * braindead.. */
		st->facilities &= ~FACIL_DOCK;
		st->had_vehicle_of_type &= ~HVOT_BUOY;

		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_SHIPS);

		/* We have to set the water tile's state to the same state as before the
		 * buoy was placed. Otherwise one could plant a buoy on a canal edge,
		 * remove it and flood the land (if the canal edge is at level 0) */
		MakeWaterOrCanalDependingOnOwner(tile, GetTileOwner(tile));
		MarkTileDirtyByTile(tile);

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return CommandCost(_price.remove_truck_station);
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
 * @param flags operation to perform
 * @param p1 (bit 0) - allow docks directly adjacent to other docks.
 * @param p2 unused
 */
CommandCost CmdBuildDock(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	DiagDirection direction;
	switch (GetTileSlope(tile, NULL)) {
		case SLOPE_SW: direction = DIAGDIR_NE; break;
		case SLOPE_SE: direction = DIAGDIR_NW; break;
		case SLOPE_NW: direction = DIAGDIR_SE; break;
		case SLOPE_NE: direction = DIAGDIR_SW; break;
		default: return_cmd_error(STR_304B_SITE_UNSUITABLE);
	}

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile)) return CMD_ERROR;

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	TileIndex tile_cur = tile + TileOffsByDiagDir(direction);

	if (!IsTileType(tile_cur, MP_WATER) || GetTileSlope(tile_cur, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_304B_SITE_UNSUITABLE);
	}

	if (MayHaveBridgeAbove(tile_cur) && IsBridgeAbove(tile_cur)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	cost = DoCommand(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	tile_cur += TileOffsByDiagDir(direction);
	if (!IsTileType(tile_cur, MP_WATER) || GetTileSlope(tile_cur, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_304B_SITE_UNSUITABLE);
	}

	/* middle */
	Station *st = NULL;

	if (!_patches.adjacent_stations || !HasBit(p1, 0)) {
		st = GetStationAround(
				tile + ToTileIndexDiff(_dock_tileoffs_chkaround[direction]),
				_dock_w_chk[direction], _dock_h_chk[direction], INVALID_STATION);
		if (st == CHECK_STATIONS_ERR) return CMD_ERROR;
	}

	/* Find a station close to us */
	if (st == NULL) st = GetClosestStationFromTile(tile);

	/* In case of new station if DC_EXEC is NOT set we still need to create the station
	 * to test if everything is OK. In this case we need to delete it before return. */
	AutoPtrT<Station> st_auto_delete;

	if (st != NULL) {
		if (st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!st->rect.BeforeAddRect(tile, _dock_w_chk[direction], _dock_h_chk[direction], StationRect::ADD_TEST)) return CMD_ERROR;

		if (st->dock_tile != 0) return_cmd_error(STR_304C_TOO_CLOSE_TO_ANOTHER_DOCK);
	} else {
		/* allocate and initialize new station */
		st = new Station(tile);
		if (st == NULL) return_cmd_error(STR_3008_TOO_MANY_STATIONS_LOADING);

		/* ensure that in case of error (or no DC_EXEC) the station gets deleted upon return */
		st_auto_delete = st;

		Town *t = st->town = ClosestTownFromTile(tile, (uint)-1);

		if (IsValidPlayer(_current_player) && (flags & DC_EXEC) != 0) {
			SetBit(t->have_ratings, _current_player);
		}

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, STATIONNAMING_DOCK)) return CMD_ERROR;
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
		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_SHIPS);
		/* success, so don't delete the new station */
		st_auto_delete.Detach();
	}
	return CommandCost(_price.build_dock);
}

static CommandCost RemoveDock(Station *st, uint32 flags)
{
	if (!CheckOwnership(st->owner)) return CMD_ERROR;

	TileIndex tile1 = st->dock_tile;
	TileIndex tile2 = tile1 + TileOffsByDiagDir(GetDockDirection(tile1));

	if (!EnsureNoVehicleOnGround(tile1)) return CMD_ERROR;
	if (!EnsureNoVehicleOnGround(tile2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile1);
		MakeWaterOrCanalDependingOnSurroundings(tile2, st->owner);

		st->rect.AfterRemoveTile(st, tile1);
		st->rect.AfterRemoveTile(st, tile2);

		MarkTileDirtyByTile(tile2);

		st->dock_tile = 0;
		st->facilities &= ~FACIL_DOCK;

		InvalidateWindowWidget(WC_STATION_VIEW, st->index, SVW_SHIPS);
		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return CommandCost(_price.remove_dock);
}

#include "table/station_land.h"

const DrawTileSprites *GetStationTileLayout(StationType st, byte gfx)
{
	return &_station_display_datas[st][gfx];
}

static void DrawTile_Station(TileInfo *ti)
{
	const DrawTileSprites *t = NULL;
	RailType railtype;
	RoadTypes roadtypes;
	if (IsRailwayStation(ti->tile)) {
		railtype = GetRailType(ti->tile);
		roadtypes = ROADTYPES_NONE;
	} else {
		roadtypes = GetRoadTypes(ti->tile);
		railtype = RAILTYPE_BEGIN;
	}
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	uint32 relocation = 0;
	const Station *st = NULL;
	const StationSpec *statspec = NULL;
	PlayerID owner = GetTileOwner(ti->tile);

	SpriteID palette;
	if (IsValidPlayer(owner)) {
		palette = PLAYER_SPRITE_COLOR(owner);
	} else {
		/* Some stations are not owner by a player, namely oil rigs */
		palette = PALETTE_TO_GREY;
	}

	/* don't show foundation for docks */
	if (ti->tileh != SLOPE_FLAT && !IsDock(ti->tile))
		DrawFoundation(ti, FOUNDATION_LEVELED);

	if (IsCustomStationSpecIndex(ti->tile)) {
		/* look for customization */
		st = GetStationByTile(ti->tile);
		statspec = st->speclist[GetCustomStationSpecIndex(ti->tile)].spec;

		//debug("Cust-o-mized %p", statspec);

		if (statspec != NULL) {
			uint tile = GetStationGfx(ti->tile);

			relocation = GetCustomStationRelocation(statspec, st, ti->tile);

			if (HasBit(statspec->callbackmask, CBM_STATION_SPRITE_LAYOUT)) {
				uint16 callback = GetStationCallback(CBID_STATION_SPRITE_LAYOUT, 0, 0, statspec, st, ti->tile);
				if (callback != CALLBACK_FAILED) tile = (callback & ~1) + GetRailStationAxis(ti->tile);
			}

			/* Ensure the chosen tile layout is valid for this custom station */
			if (statspec->renderdata != NULL) {
				t = &statspec->renderdata[tile < statspec->tiles ? tile : (uint)GetRailStationAxis(ti->tile)];
			}
		}
	}

	if (t == NULL || t->seq == NULL) t = &_station_display_datas[GetStationType(ti->tile)][GetStationGfx(ti->tile)];

	SpriteID image = t->ground_sprite;
	if (HasBit(image, SPRITE_MODIFIER_USE_OFFSET)) {
		image += GetCustomStationGroundRelocation(statspec, st, ti->tile);
		image += rti->custom_ground_offset;
	} else {
		image += rti->total_offset;
	}

	/* station_land array has been increased from 82 elements to 114
	 * but this is something else. If AI builds station with 114 it looks all weird */
	DrawGroundSprite(image, HasBit(image, PALETTE_MODIFIER_COLOR) ? palette : PAL_NONE);

	if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC && IsStationTileElectrifiable(ti->tile)) DrawCatenary(ti);

	if (HasBit(roadtypes, ROADTYPE_TRAM)) {
		Axis axis = GetRoadStopDir(ti->tile) == DIAGDIR_NE ? AXIS_X : AXIS_Y;
		DrawGroundSprite((HasBit(roadtypes, ROADTYPE_ROAD) ? SPR_TRAMWAY_OVERLAY : SPR_TRAMWAY_TRAM) + (axis ^ 1), PAL_NONE);
		DrawTramCatenary(ti, axis == AXIS_X ? ROAD_X : ROAD_Y);
	}

	if (IsCanalBuoyTile(ti->tile)) DrawCanalWater(ti->tile);

	const DrawTileSeqStruct *dtss;
	foreach_draw_tile_seq(dtss, t->seq) {
		image = dtss->image;
		if (relocation == 0 || HasBit(image, SPRITE_MODIFIER_USE_OFFSET)) {
			image += rti->total_offset;
		} else {
			image += relocation;
		}

		SpriteID pal;
		if (!IsTransparencySet(TO_BUILDINGS) && HasBit(image, PALETTE_MODIFIER_COLOR)) {
			pal = palette;
		} else {
			pal = dtss->pal;
		}

		if ((byte)dtss->delta_z != 0x80) {
			AddSortableSpriteToDraw(
				image, pal,
				ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->size_x, dtss->size_y,
				dtss->size_z, ti->z + dtss->delta_z,
				IsTransparencySet(TO_BUILDINGS)
			);
		} else {
			AddChildSpriteScreen(image, pal, dtss->delta_x, dtss->delta_y, IsTransparencySet(TO_BUILDINGS));
		}
	}
}

void StationPickerDrawSprite(int x, int y, StationType st, RailType railtype, RoadType roadtype, int image)
{
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	SpriteID pal = PLAYER_SPRITE_COLOR(_local_player);
	const DrawTileSprites *t = &_station_display_datas[st][image];

	SpriteID img = t->ground_sprite;
	DrawSprite(img + rti->total_offset, HasBit(img, PALETTE_MODIFIER_COLOR) ? pal : PAL_NONE, x, y);

	if (roadtype == ROADTYPE_TRAM) {
		DrawSprite(SPR_TRAMWAY_TRAM + (t->ground_sprite == SPR_ROAD_PAVED_STRAIGHT_X ? 1 : 0), PAL_NONE, x, y);
	}

	const DrawTileSeqStruct *dtss;
	foreach_draw_tile_seq(dtss, t->seq) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		DrawSprite(dtss->image + rti->total_offset, pal, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Station(TileIndex tile, uint x, uint y)
{
	return GetTileMaxZ(tile);
}

static Foundation GetFoundation_Station(TileIndex tile, Slope tileh)
{
	return FlatteningFoundation(tileh);
}

static void GetAcceptedCargo_Station(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Station(TileIndex tile, TileDesc *td)
{
	td->owner = GetTileOwner(tile);
	td->build_date = GetStationByTile(tile)->build_date;

	StringID str;
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


static uint32 GetTileTrackStatus_Station(TileIndex tile, TransportType mode, uint sub_mode)
{
	switch (mode) {
		case TRANSPORT_RAIL:
			if (IsRailwayStation(tile) && !IsStationTileBlocked(tile)) {
				return TrackToTrackBits(GetRailStationTrack(tile)) * 0x101;
			}
			break;

		case TRANSPORT_WATER:
			/* buoy is coded as a station, it is always on open water */
			if (IsBuoy(tile)) {
				TrackBits ts = TRACK_BIT_ALL;
				/* remove tracks that connect NE map edge */
				if (TileX(tile) == 0) ts &= ~(TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT);
				/* remove tracks that connect NW map edge */
				if (TileY(tile) == 0) ts &= ~(TRACK_BIT_Y | TRACK_BIT_LEFT | TRACK_BIT_UPPER);
				return uint32(ts) * 0x101;
			}
			break;

		case TRANSPORT_ROAD:
			if ((GetRoadTypes(tile) & sub_mode) != 0 && IsRoadStopTile(tile)) {
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
	/* FIXME -- GetTileTrackStatus_Station -> animated stationtiles
	 * hardcoded.....not good */
	switch (GetStationType(tile)) {
		case STATION_AIRPORT:
			switch (GetStationGfx(tile)) {
				case GFX_RADAR_LARGE_FIRST:
				case GFX_WINDSACK_FIRST : // for small airport
				case GFX_RADAR_INTERNATIONAL_FIRST:
				case GFX_RADAR_METROPOLITAN_FIRST:
				case GFX_RADAR_DISTRICTWE_FIRST: // radar district W-E airport
				case GFX_WINDSACK_INTERCON_FIRST : // for intercontinental airport
					AddAnimatedTile(tile);
					break;
			}
			break;

		case STATION_OILRIG: //(station part)
		case STATION_BUOY:
			TileLoop_Water(tile);
			break;

		default: break;
	}
}


static void AnimateTile_Station(TileIndex tile)
{
	struct AnimData {
		StationGfx from; // first sprite
		StationGfx to;   // last sprite
		byte delay;
	};

	static const AnimData data[] = {
		{ GFX_RADAR_LARGE_FIRST,         GFX_RADAR_LARGE_LAST,         3 },
		{ GFX_WINDSACK_FIRST,            GFX_WINDSACK_LAST,            1 },
		{ GFX_RADAR_INTERNATIONAL_FIRST, GFX_RADAR_INTERNATIONAL_LAST, 3 },
		{ GFX_RADAR_METROPOLITAN_FIRST,  GFX_RADAR_METROPOLITAN_LAST,  3 },
		{ GFX_RADAR_DISTRICTWE_FIRST,    GFX_RADAR_DISTRICTWE_LAST,    3 },
		{ GFX_WINDSACK_INTERCON_FIRST,   GFX_WINDSACK_INTERCON_LAST,   1 }
	};

	StationGfx gfx = GetStationGfx(tile);

	for (const AnimData *i = data; i != endof(data); i++) {
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
		ShowDepotWindow(tile, VEH_AIRCRAFT);
	} else {
		ShowStationViewWindow(GetStationIndex(tile));
	}
}

static const byte _enter_station_speedtable[12] = {
	215, 195, 175, 155, 135, 115, 95, 75, 55, 35, 15, 0
};

static uint32 VehicleEnter_Station(Vehicle *v, TileIndex tile, int x, int y)
{
	if (v->type == VEH_TRAIN) {
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

					if (DiagDirToAxis(dir) != AXIS_X) Swap(x, y);
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
	} else if (v->type == VEH_ROAD) {
		if (v->u.road.state < RVSB_IN_ROAD_STOP && !IsReversingRoadTrackdir((Trackdir)v->u.road.state) && v->u.road.frame == 0) {
			if (IsRoadStop(tile) && IsRoadVehFront(v)) {
				/* Attempt to allocate a parking bay in a road stop */
				RoadStop *rs = GetRoadStopByTile(tile, GetRoadStopType(tile));

				if (IsDriveThroughStopTile(tile)) {
					/* Vehicles entering a drive-through stop from the 'normal' side use first bay (bay 0). */
					byte side = ((DirToDiagDir(v->direction) == ReverseDiagDir(GetRoadStopDir(tile))) == (v->u.road.overtaking == 0)) ? 0 : 1;

					if (!rs->IsFreeBay(side)) return VETSB_CANNOT_ENTER;

					/* Check if the vehicle is stopping at this road stop */
					if (GetRoadStopType(tile) == (IsCargoInClass(v->cargo_type, CC_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK) &&
							v->current_order.dest == GetStationIndex(tile)) {
						SetBit(v->u.road.state, RVS_IS_STOPPING);
						rs->AllocateDriveThroughBay(side);
					}

					/* Indicate if vehicle is using second bay. */
					if (side == 1) SetBit(v->u.road.state, RVS_USING_SECOND_BAY);
					/* Indicate a drive-through stop */
					SetBit(v->u.road.state, RVS_IN_DT_ROAD_STOP);
					return VETSB_CONTINUE;
				}

				/* For normal (non drive-through) road stops */
				/* Check if station is busy or if there are no free bays or whether it is a articulated vehicle. */
				if (rs->IsEntranceBusy() || !rs->HasFreeBay() || RoadVehHasArticPart(v)) return VETSB_CANNOT_ENTER;

				SetBit(v->u.road.state, RVS_IN_ROAD_STOP);

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
	bool waiting_changed = false;

	byte_inc_sat(&st->time_since_load);
	byte_inc_sat(&st->time_since_unload);

	GoodsEntry *ge = st->goods;
	do {
		/* Slowly increase the rating back to his original level in the case we
		 *  didn't deliver cargo yet to this station. This happens when a bribe
		 *  failed while you didn't moved that cargo yet to a station. */
		if (!HasBit(ge->acceptance_pickup, GoodsEntry::PICKUP) && ge->rating < INITIAL_STATION_RATING) {
			ge->rating++;
		}

		/* Only change the rating if we are moving this cargo */
		if (HasBit(ge->acceptance_pickup, GoodsEntry::PICKUP)) {
			byte_inc_sat(&ge->days_since_pickup);

			int rating = 0;

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

			if (IsValidPlayer(st->owner) && HasBit(st->town->statues, st->owner)) rating += 26;

			{
				byte days = ge->days_since_pickup;
				if (st->last_vehicle_type == VEH_SHIP) days >>= 2;
				(days > 21) ||
				(rating += 25, days > 12) ||
				(rating += 25, days > 6) ||
				(rating += 45, days > 3) ||
				(rating += 35, true);
			}

			uint waiting = ge->cargo.Count();
			(rating -= 90, waiting > 1500) ||
			(rating += 55, waiting > 1000) ||
			(rating += 35, waiting > 600) ||
			(rating += 10, waiting > 300) ||
			(rating += 20, waiting > 100) ||
			(rating += 10, true);

			{
				int or_ = ge->rating; // old rating

				/* only modify rating in steps of -2, -1, 0, 1 or 2 */
				ge->rating = rating = or_ + Clamp(Clamp(rating, 0, 255) - or_, -2, 2);

				/* if rating is <= 64 and more than 200 items waiting,
				 * remove some random amount of goods from the station */
				if (rating <= 64 && waiting >= 200) {
					int dec = Random() & 0x1F;
					if (waiting < 400) dec &= 7;
					waiting -= dec + 1;
					waiting_changed = true;
				}

				/* if rating is <= 127 and there are any items waiting, maybe remove some goods. */
				if (rating <= 127 && waiting != 0) {
					uint32 r = Random();
					if (rating <= (int)GB(r, 0, 7)) {
						/* Need to have int, otherwise it will just overflow etc. */
						waiting = max((int)waiting - (int)GB(r, 8, 2) - 1, 0);
						waiting_changed = true;
					}
				}

				/* At some point we really must cap the cargo. Previously this
				 * was a strict 4095, but now we'll have a less strict, but
				 * increasingly agressive truncation of the amount of cargo. */
				static const uint WAITING_CARGO_THRESHOLD  = 1 << 12;
				static const uint WAITING_CARGO_CUT_FACTOR = 1 <<  6;
				static const uint MAX_WAITING_CARGO        = 1 << 15;

				if (waiting > WAITING_CARGO_THRESHOLD) {
					uint difference = waiting - WAITING_CARGO_THRESHOLD;
					waiting -= (difference / WAITING_CARGO_CUT_FACTOR);

					waiting = min(waiting, MAX_WAITING_CARGO);
					waiting_changed = true;
				}

				if (waiting_changed) ge->cargo.Truncate(waiting);
			}
		}
	} while (++ge != endof(st->goods));

	StationID index = st->index;
	if (waiting_changed) {
		InvalidateWindow(WC_STATION_VIEW, index); // update whole window
	} else {
		InvalidateWindowWidget(WC_STATION_VIEW, index, SVW_RATINGLIST); // update only ratings list
	}
}

/* called for every station each tick */
static void StationHandleSmallTick(Station *st)
{
	if (st->facilities == 0) return;

	byte b = st->delete_ctr + 1;
	if (b >= 185) b = 0;
	st->delete_ctr = b;

	if (b == 0) UpdateStationRating(st);
}

void OnTick_Station()
{
	if (_game_mode == GM_EDITOR) return;

	uint i = _station_tick_ctr;
	if (++_station_tick_ctr > GetMaxStationIndex()) _station_tick_ctr = 0;

	if (IsValidStationID(i)) StationHandleBigTick(GetStation(i));

	Station *st;
	FOR_ALL_STATIONS(st) StationHandleSmallTick(st);
}

void StationMonthlyLoop()
{
}


void ModifyStationRatingAround(TileIndex tile, PlayerID owner, int amount, uint radius)
{
	Station *st;

	FOR_ALL_STATIONS(st) {
		if (st->owner == owner &&
				DistanceManhattan(tile, st->xy) <= radius) {
			for (CargoID i = 0; i < NUM_CARGO; i++) {
				GoodsEntry* ge = &st->goods[i];

				if (ge->acceptance_pickup != 0) {
					ge->rating = Clamp(ge->rating + amount, 0, 255);
				}
			}
		}
	}
}

static void UpdateStationWaiting(Station *st, CargoID type, uint amount)
{
	st->goods[type].cargo.Append(new CargoPacket(st->index, amount));
	SetBit(st->goods[type].acceptance_pickup, GoodsEntry::PICKUP);

	InvalidateWindow(WC_STATION_VIEW, st->index);
	st->MarkTilesDirty(true);
}

static bool IsUniqueStationName(const char *name)
{
	const Station *st;
	char buf[512];

	FOR_ALL_STATIONS(st) {
		SetDParam(0, st->index);
		GetString(buf, STR_STATION, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
	}

	return true;
}

/** Rename a station
 * @param tile unused
 * @param flags operation to perform
 * @param p1 station ID that is to be renamed
 * @param p2 unused
 */
CommandCost CmdRenameStation(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidStationID(p1) || StrEmpty(_cmd_text)) return CMD_ERROR;
	Station *st = GetStation(p1);

	if (!CheckOwnership(st->owner)) return CMD_ERROR;

	if (!IsUniqueStationName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

	StringID str = AllocateName(_cmd_text, 6);
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

	return CommandCost();
}

/**
* Find all (non-buoy) stations around an industry tile
*
* @param tile: Center tile to search from
* @param w: Width of the center
* @param h: Height of the center
*
* @return: Set of found stations
*/
StationSet FindStationsAroundIndustryTile(TileIndex tile, int w, int h)
{
	StationSet station_set;

	int w_prod; // width and height of the "producer" of the cargo
	int h_prod;
	int max_rad;
	if (_patches.modified_catchment) {
		w_prod = w;
		h_prod = h;
		w += 2 * MAX_CATCHMENT;
		h += 2 * MAX_CATCHMENT;
		max_rad = MAX_CATCHMENT;
	} else {
		w_prod = 0;
		h_prod = 0;
		w += 8;
		h += 8;
		max_rad = 4;
	}

	BEGIN_TILE_LOOP(cur_tile, w, h, tile - TileDiffXY(max_rad, max_rad))
		cur_tile = TILE_MASK(cur_tile);
		if (!IsTileType(cur_tile, MP_STATION)) continue;

		Station *st = GetStationByTile(cur_tile);

		if (st->IsBuoy()) continue; // bouys don't accept cargo


		if (_patches.modified_catchment) {
			/* min and max coordinates of the producer relative */
			const int x_min_prod = max_rad + 1;
			const int x_max_prod = max_rad + w_prod;
			const int y_min_prod = max_rad + 1;
			const int y_max_prod = max_rad + h_prod;

			int rad = FindCatchmentRadius(st);

			int x_dist = min(w_cur - x_min_prod, x_max_prod - w_cur);
			if (w_cur < x_min_prod) {
				x_dist = x_min_prod - w_cur;
			} else if (w_cur > x_max_prod) {
				x_dist = w_cur - x_max_prod;
			}

			if (x_dist > rad) continue;

			int y_dist = min(h_cur - y_min_prod, y_max_prod - h_cur);
			if (h_cur < y_min_prod) {
				y_dist = y_min_prod - h_cur;
			} else if (h_cur > y_max_prod) {
				y_dist = h_cur - y_max_prod;
			}

			if (y_dist > rad) continue;
		}

		/* Insert the station in the set. This will fail if it has
		 * already been added.
		 */
		station_set.insert(st);

	END_TILE_LOOP(cur_tile, w, h, tile - TileDiffXY(max_rad, max_rad))

	return station_set;
}

uint MoveGoodsToStation(TileIndex tile, int w, int h, CargoID type, uint amount)
{
	Station *st1 = NULL;	// Station with best rating
	Station *st2 = NULL;	// Second best station
	uint best_rating1 = 0;	// rating of st1
	uint best_rating2 = 0;	// rating of st2

	StationSet all_stations = FindStationsAroundIndustryTile(tile, w, h);
	for (StationSet::iterator st_iter = all_stations.begin(); st_iter != all_stations.end(); ++st_iter) {
		Station *st = *st_iter;

		/* Is the station reserved exclusively for somebody else? */
		if (st->town->exclusive_counter > 0 && st->town->exclusivity != st->owner) continue;

		if (st->goods[type].rating == 0) continue; // Lowest possible rating, better not to give cargo anymore

		if (_patches.selectgoods && st->goods[type].last_speed == 0) continue; // Selectively servicing stations, and not this one

		if (IsCargoInClass(type, CC_PASSENGERS)) {
			if (st->facilities == FACIL_TRUCK_STOP) continue; // passengers are never served by just a truck stop
		} else {
			if (st->facilities == FACIL_BUS_STOP) continue; // non-passengers are never served by just a bus stop
		}

		/* This station can be used, add it to st1/st2 */
		if (st1 == NULL || st->goods[type].rating >= best_rating1) {
			st2 = st1; best_rating2 = best_rating1; st1 = st; best_rating1 = st->goods[type].rating;
		} else if (st2 == NULL || st->goods[type].rating >= best_rating2) {
			st2 = st; best_rating2 = st->goods[type].rating;
		}
	}

	/* no stations around at all? */
	if (st1 == NULL) return 0;

	if (st2 == NULL) {
		/* only one station around */
		uint moved = amount * best_rating1 / 256 + 1;
		UpdateStationWaiting(st1, type, moved);
		return moved;
	}

	/* several stations around, the best two (highest rating) are in st1 and st2 */
	assert(st1 != NULL);
	assert(st2 != NULL);
	assert(best_rating1 != 0 || best_rating2 != 0);

	/* the 2nd highest one gets a penalty */
	best_rating2 >>= 1;

	/* amount given to station 1 */
	uint t = (best_rating1 * (amount + 1)) / (best_rating1 + best_rating2);

	uint moved = 0;
	if (t != 0) {
		moved = t * best_rating1 / 256 + 1;
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
	Station *st = new Station();

	if (st == NULL) {
		DEBUG(misc, 0, "Can't allocate station for oilrig at 0x%X, reverting to oilrig only", tile);
		return;
	}

	st->town = ClosestTownFromTile(tile, (uint)-1);
	st->sign.width_1 = 0;

	if (!GenerateStationName(st, tile, STATIONNAMING_OILRIG)) {
		DEBUG(misc, 0, "Can't allocate station-name for oilrig at 0x%X, reverting to oilrig only", tile);
		delete st;
		return;
	}

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
	st->last_vehicle_type = VEH_INVALID;
	st->facilities = FACIL_AIRPORT | FACIL_DOCK;
	st->build_date = _date;

	for (CargoID j = 0; j < NUM_CARGO; j++) {
		st->goods[j].acceptance_pickup = 0;
		st->goods[j].days_since_pickup = 255;
		st->goods[j].rating = INITIAL_STATION_RATING;
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
		if (IsDriveThroughStopTile(tile) && GetStopBuiltOnTownRoad(tile)) {
			/* For a drive-through stop on a town-owned road remove the stop and replace the road */
			DoCommand(tile, 0, (GetStationType(tile) == STATION_TRUCK) ? RoadStop::TRUCK : RoadStop::BUS, DC_EXEC, CMD_REMOVE_ROAD_STOP);
		} else {
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}
}

/**
 * Check if a drive-through road stop tile can be cleared.
 * Road stops built on town-owned roads check the conditions
 * that would allow clearing of the original road.
 * @param tile road stop tile to check
 * @return true if the road can be cleared
 */
static bool CanRemoveRoadWithStop(TileIndex tile)
{
	/* The road can always be cleared if it was not a town-owned road */
	if (!GetStopBuiltOnTownRoad(tile)) return true;

	bool edge_road;
	return CheckAllowRemoveRoad(tile, GetAnyRoadBits(tile, ROADTYPE_ROAD), OWNER_TOWN, &edge_road, ROADTYPE_ROAD) &&
				CheckAllowRemoveRoad(tile, GetAnyRoadBits(tile, ROADTYPE_TRAM), OWNER_TOWN, &edge_road, ROADTYPE_TRAM);
}

static CommandCost ClearTile_Station(TileIndex tile, byte flags)
{
	if (flags & DC_AUTO) {
		switch (GetStationType(tile)) {
			case STATION_RAIL:    return_cmd_error(STR_300B_MUST_DEMOLISH_RAILROAD);
			case STATION_AIRPORT: return_cmd_error(STR_300E_MUST_DEMOLISH_AIRPORT_FIRST);
			case STATION_TRUCK:   return_cmd_error(HasBit(GetRoadTypes(tile), ROADTYPE_TRAM) ? STR_3047_MUST_DEMOLISH_CARGO_TRAM_STATION : STR_3047_MUST_DEMOLISH_TRUCK_STATION);
			case STATION_BUS:     return_cmd_error(HasBit(GetRoadTypes(tile), ROADTYPE_TRAM) ? STR_3046_MUST_DEMOLISH_PASSENGER_TRAM_STATION : STR_3046_MUST_DEMOLISH_BUS_STATION);
			case STATION_BUOY:    return_cmd_error(STR_306A_BUOY_IN_THE_WAY);
			case STATION_DOCK:    return_cmd_error(STR_304D_MUST_DEMOLISH_DOCK_FIRST);
			case STATION_OILRIG:
				SetDParam(0, STR_4807_OIL_RIG);
				return_cmd_error(STR_4800_IN_THE_WAY);
		}
	}

	Station *st = GetStationByTile(tile);

	switch (GetStationType(tile)) {
		case STATION_RAIL:    return RemoveRailroadStation(st, tile, flags);
		case STATION_AIRPORT: return RemoveAirport(st, flags);
		case STATION_TRUCK:
			if (IsDriveThroughStopTile(tile) && !CanRemoveRoadWithStop(tile))
				return_cmd_error(STR_3047_MUST_DEMOLISH_TRUCK_STATION);
			return RemoveRoadStop(st, flags, tile);
		case STATION_BUS:
			if (IsDriveThroughStopTile(tile) && !CanRemoveRoadWithStop(tile))
				return_cmd_error(STR_3046_MUST_DEMOLISH_BUS_STATION);
			return RemoveRoadStop(st, flags, tile);
		case STATION_BUOY:    return RemoveBuoy(st, flags);
		case STATION_DOCK:    return RemoveDock(st, flags);
		default: break;
	}

	return CMD_ERROR;
}

void InitializeStations()
{
	/* Clean the station pool and create 1 block in it */
	_Station_pool.CleanPool();
	_Station_pool.AddBlockToPool();

	/* Clean the roadstop pool and create 1 block in it */
	_RoadStop_pool.CleanPool();
	_RoadStop_pool.AddBlockToPool();

	_station_tick_ctr = 0;

}


void AfterLoadStations()
{
	/* Update the speclists of all stations to point to the currently loaded custom stations. */
	Station *st;
	FOR_ALL_STATIONS(st) {
		for (uint i = 0; i < st->num_specs; i++) {
			if (st->speclist[i].grfid == 0) continue;

			st->speclist[i].spec = GetCustomStationSpecByGrf(st->speclist[i].grfid, st->speclist[i].localidx);
		}

		for (CargoID c = 0; c < NUM_CARGO; c++) st->goods[c].cargo.InvalidateCache();
	}
}

static CommandCost TerraformTile_Station(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	if (_patches.build_on_slopes && AutoslopeEnabled()) {
		/* TODO: If you implement newgrf callback 149 'land slope check', you have to decide what to do with it here.
		 *       TTDP does not call it.
		 */
		if (!IsSteepSlope(tileh_new) && (GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new))) {
			switch (GetStationType(tile)) {
				case STATION_RAIL: {
					DiagDirection direction = AxisToDiagDir(GetRailStationAxis(tile));
					if (!AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, direction)) break;
					if (!AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, ReverseDiagDir(direction))) break;
					return _price.terraform;
				}

				case STATION_AIRPORT:
					return _price.terraform;

				case STATION_TRUCK:
				case STATION_BUS: {
					DiagDirection direction = GetRoadStopDir(tile);
					if (!AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, direction)) break;
					if (IsDriveThroughStopTile(tile)) {
						if (!AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, ReverseDiagDir(direction))) break;
					}
					return _price.terraform;
				}

				default: break;
			}
		}
	}
	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
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
	GetFoundation_Station,      /* get_foundation_proc */
	TerraformTile_Station,      /* terraform_tile_proc */
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

	/* alpha_order was stored here in savegame format 0 - 3 */
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

	/* Was custom station class and id */
	SLE_CONDNULL(2, 3, 25),
	SLE_CONDVAR(Station, build_date,                 SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(Station, build_date,                 SLE_INT32,                  31, SL_MAX_VERSION),

	SLE_CONDREF(Station, bus_stops,                  REF_ROADSTOPS,               6, SL_MAX_VERSION),
	SLE_CONDREF(Station, truck_stops,                REF_ROADSTOPS,               6, SL_MAX_VERSION),

	/* Used by newstations for graphic variations */
	SLE_CONDVAR(Station, random_bits,                SLE_UINT16,                 27, SL_MAX_VERSION),
	SLE_CONDVAR(Station, waiting_triggers,           SLE_UINT8,                  27, SL_MAX_VERSION),
	SLE_CONDVAR(Station, num_specs,                  SLE_UINT8,                  27, SL_MAX_VERSION),

	SLE_CONDLST(Station, loading_vehicles,           REF_VEHICLE,                57, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 32 bytes) */
	SLE_CONDNULL(32, 2, SL_MAX_VERSION),

	SLE_END()
};

static uint16 _waiting_acceptance;
static uint16 _cargo_source;
static uint32 _cargo_source_xy;
static uint16 _cargo_days;
static Money  _cargo_feeder_share;

static const SaveLoad _station_speclist_desc[] = {
	SLE_CONDVAR(StationSpecList, grfid,    SLE_UINT32, 27, SL_MAX_VERSION),
	SLE_CONDVAR(StationSpecList, localidx, SLE_UINT8,  27, SL_MAX_VERSION),

	SLE_END()
};


void SaveLoad_STNS(Station *st)
{
	static const SaveLoad _goods_desc[] = {
		SLEG_CONDVAR(            _waiting_acceptance, SLE_UINT16,                  0, 67),
		 SLE_CONDVAR(GoodsEntry, acceptance_pickup,   SLE_UINT8,                  68, SL_MAX_VERSION),
		SLE_CONDNULL(2,                                                           51, 67),
		     SLE_VAR(GoodsEntry, days_since_pickup,   SLE_UINT8),
		     SLE_VAR(GoodsEntry, rating,              SLE_UINT8),
		SLEG_CONDVAR(            _cargo_source,       SLE_FILE_U8 | SLE_VAR_U16,   0, 6),
		SLEG_CONDVAR(            _cargo_source,       SLE_UINT16,                  7, 67),
		SLEG_CONDVAR(            _cargo_source_xy,    SLE_UINT32,                 44, 67),
		SLEG_CONDVAR(            _cargo_days,         SLE_UINT8,                   0, 67),
		     SLE_VAR(GoodsEntry, last_speed,          SLE_UINT8),
		     SLE_VAR(GoodsEntry, last_age,            SLE_UINT8),
		SLEG_CONDVAR(            _cargo_feeder_share, SLE_FILE_U32 | SLE_VAR_I64, 14, 64),
		SLEG_CONDVAR(            _cargo_feeder_share, SLE_INT64,                  65, 67),
		 SLE_CONDLST(GoodsEntry, cargo.packets,       REF_CARGO_PACKET,           68, SL_MAX_VERSION),

		SLE_END()
};


	SlObject(st, _station_desc);

	_waiting_acceptance = 0;

	uint num_cargo = CheckSavegameVersion(55) ? 12 : NUM_CARGO;
	for (CargoID i = 0; i < num_cargo; i++) {
		GoodsEntry *ge = &st->goods[i];
		SlObject(ge, _goods_desc);
		if (CheckSavegameVersion(68)) {
			SB(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE, 1, HasBit(_waiting_acceptance, 15));
			if (GB(_waiting_acceptance, 0, 12) != 0) {
				/* Don't construct the packet with station here, because that'll fail with old savegames */
				CargoPacket *cp = new CargoPacket();
				/* In old versions, enroute_from used 0xFF as INVALID_STATION */
				cp->source          = (CheckSavegameVersion(7) && _cargo_source == 0xFF) ? INVALID_STATION : _cargo_source;
				cp->count           = GB(_waiting_acceptance, 0, 12);
				cp->days_in_transit = _cargo_days;
				cp->feeder_share    = _cargo_feeder_share;
				cp->source_xy       = _cargo_source_xy;
				cp->days_in_transit = _cargo_days;
				cp->feeder_share    = _cargo_feeder_share;
				SB(ge->acceptance_pickup, GoodsEntry::PICKUP, 1, 1);
				ge->cargo.Append(cp);
			}
		}
	}

	if (st->num_specs != 0) {
		/* Allocate speclist memory when loading a game */
		if (st->speclist == NULL) st->speclist = CallocT<StationSpecList>(st->num_specs);
		for (uint i = 0; i < st->num_specs; i++) {
			SlObject(&st->speclist[i], _station_speclist_desc);
		}
	}
}

static void Save_STNS()
{
	Station *st;
	/* Write the stations */
	FOR_ALL_STATIONS(st) {
		SlSetArrayIndex(st->index);
		SlAutolength((AutolengthProc*)SaveLoad_STNS, st);
	}
}

static void Load_STNS()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Station *st = new (index) Station();

		SaveLoad_STNS(st);

		/* this means it's an oldstyle savegame without support for nonuniform stations */
		if (st->train_tile != 0 && st->trainst_h == 0) {
			uint w = GB(st->trainst_w, 4, 4);
			uint h = GB(st->trainst_w, 0, 4);

			if (GetRailStationAxis(st->train_tile) != AXIS_X) Swap(w, h);
			st->trainst_w = w;
			st->trainst_h = h;
		}
	}

	/* This is to ensure all pointers are within the limits of _stations_size */
	if (_station_tick_ctr > GetMaxStationIndex()) _station_tick_ctr = 0;
}

static void Save_ROADSTOP()
{
	RoadStop *rs;

	FOR_ALL_ROADSTOPS(rs) {
		SlSetArrayIndex(rs->index);
		SlObject(rs, _roadstop_desc);
	}
}

static void Load_ROADSTOP()
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
