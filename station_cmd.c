#include "stdafx.h"
#include "ttd.h"
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
#include "table/directions.h"

// FIXME -- need to be embedded into Airport variable. Is dynamically
// deducteable from graphics-tile array, so will not be needed
const byte _airport_size_x[5] = {4, 6, 1, 6, 7 };
const byte _airport_size_y[5] = {3, 6, 1, 6, 7 };

void ShowAircraftDepotWindow(uint tile);
extern void UpdateAirplanesOnNewStation(Station *st);

static void MarkStationDirty(Station *st)
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

#define CHECK_STATIONS_ERR ((Station*)-1)

static Station *GetStationAround(uint tile, int w, int h, int closest_station)
{
	// check around to see if there's any stations there
	BEGIN_TILE_LOOP(tile_cur, w + 2, h + 2, tile - TILE_XY(1,1))
		if (IS_TILETYPE(tile_cur, MP_STATION)) {
			int t;
			t = _map2[tile_cur];
			{
				Station *st = DEREF_STATION(t);
				// you cannot take control of an oilrig!!
				if (st->airport_type == AT_OILRIG && st->facilities == (FACIL_AIRPORT|FACIL_DOCK))
					continue;
			}

			if (closest_station == -1) {
				closest_station = t;
			} else if (closest_station != t) {
				_error_message = STR_3006_ADJOINS_MORE_THAN_ONE_EXISTING;
				return CHECK_STATIONS_ERR;
			}
		}
	END_TILE_LOOP(tile_cur, w + 2, h + 2, tile - TILE_XY(1,1))
	return (closest_station == -1) ? NULL : DEREF_STATION(closest_station);
}

TileIndex GetStationTileForVehicle(Vehicle *v, Station *st)
{
	switch (v->type) {
		case VEH_Train: 		return st->train_tile;
		case VEH_Aircraft:	return st->airport_tile;
		case VEH_Ship:			return st->dock_tile;
		case VEH_Road:			return (v->cargo_type == CT_PASSENGERS) ? st->bus_tile : st->lorry_tile;
		default:
			assert(false);
			return 0;
	}
}

static bool CheckStationSpreadOut(Station *st, uint tile, int w, int h)
{
	byte station_index = st->index;
	uint i;
	uint x1 = GET_TILE_X(tile);
	uint y1 = GET_TILE_Y(tile);
	uint x2 = x1 + w - 1;
	uint y2 = y1 + h - 1;
	uint t;

	for(i=0; i!=TILES_X*TILES_Y; i++) {
		if (IS_TILETYPE(i, MP_STATION) && _map2[i] == station_index) {
			t = GET_TILE_X(i);
			if (t < x1) x1 = t;
			if (t > x2) x2 = t;

			t = GET_TILE_Y(i);
			if (t < y1) y1 = t;
			if (t > y2) y2 = t;
		}
	}

	if (y2-y1 >= _patches.station_spread || x2-x1 >= _patches.station_spread) {
		_error_message = STR_306C_STATION_TOO_SPREAD_OUT;
		return false;
	}

	return true;
}

static Station *AllocateStation()
{
	Station *st, *a_free = NULL;
	int count;
	int num_free = 0;
	int i;

	for(st = _stations, count = lengthof(_stations); count != 0; count--, st++) {
		if (st->xy == 0) {
			num_free++;
			if (a_free==NULL)
				a_free = st;
		}
	}

	if (a_free == NULL ||
			(num_free < 30 && IS_HUMAN_PLAYER(_current_player))) {
		_error_message = STR_3008_TOO_MANY_STATIONS_LOADING;
		return NULL;
	}

	i = a_free->index;
	memset(a_free, 0, sizeof(Station));
	a_free->index = i;
	return a_free;
}


static int CountMapSquareAround(uint tile, byte type, byte min, byte max) {
	static const TileIndexDiff _count_square_table[7*7+1] = {
		TILE_XY(-3,-3), 1, 1, 1, 1, 1, 1,
		TILE_XY(-6,1),  1, 1, 1, 1, 1, 1,
		TILE_XY(-6,1),  1, 1, 1, 1, 1, 1,
		TILE_XY(-6,1),  1, 1, 1, 1, 1, 1,
		TILE_XY(-6,1),  1, 1, 1, 1, 1, 1,
		TILE_XY(-6,1),  1, 1, 1, 1, 1, 1,
		TILE_XY(-6,1),  1, 1, 1, 1, 1, 1,
		0,
	};
	int j;
	const TileIndexDiff *p = _count_square_table;
	int num = 0;

	while ( (j=*p++) != 0 ) {
		tile = TILE_MASK(tile + j);

		if (IS_TILETYPE(tile, type) && _map5[tile] >= min && _map5[tile] <= max)
			num++;
	}

	return num;
}

#define M(x) ((x) - STR_SV_STNAME)

static bool GenerateStationName(Station *st, uint tile, int flag)
{
	static const uint32 _gen_station_name_bits[] = {
		0,																/* 0 */
		1 << M(STR_SV_STNAME_AIRPORT),		/* 1 */
		1 << M(STR_SV_STNAME_OILFIELD),		/* 2 */
		1 << M(STR_SV_STNAME_DOCKS),			/* 3 */
		0x1FF << M(STR_SV_STNAME_BUOY_1),	/* 4 */
		1 << M(STR_SV_STNAME_HELIPORT),		/* 5 */
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
	if (GetTileDist1D(tile,t->xy) < 8) {
		found = M(STR_SV_STNAME);
		if (HASBIT(free_names, M(STR_SV_STNAME))) goto done;

		found = M(STR_SV_STNAME_CENTRAL);
		if (HASBIT(free_names, M(STR_SV_STNAME_CENTRAL))) goto done;
	}

	/* Check lakeside */
	if (HASBIT(free_names, M(STR_SV_STNAME_LAKESIDE)) &&
			!CheckDistanceFromEdge(tile, 20) &&
			CountMapSquareAround(tile, MP_WATER, 0, 0) >= 5) {
				found = M(STR_SV_STNAME_LAKESIDE);
				goto done;
			}

	/* Check woods */
	if (HASBIT(free_names, M(STR_SV_STNAME_WOODS)) && (
			CountMapSquareAround(tile, MP_TREES, 0, 255) >= 8 ||
			CountMapSquareAround(tile, MP_INDUSTRY, 0x10, 0x11) >= 2 )) {
				found = (_opt.landscape==LT_DESERT) ? M(STR_SV_STNAME_FOREST) : M(STR_SV_STNAME_WOODS);
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
			(GET_TILE_X(tile) < GET_TILE_X(t->xy)) +
			(GET_TILE_Y(tile) < GET_TILE_Y(t->xy))*2];
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

static Station *GetClosestStationFromTile(uint tile, uint threshold, byte owner)
{
	Station *st, *best_station = NULL;
	uint cur_dist;

	FOR_ALL_STATIONS(st) {
		cur_dist = GetTileDist(tile, st->xy);
		if (cur_dist < threshold && (owner == 0xFF || st->owner == owner)) {
			threshold = cur_dist;
			best_station = st;
		}
	}

	return best_station;
}

static void StationInitialize(Station *st, TileIndex tile)
{
	int i;
	GoodsEntry *ge;

	st->xy = tile;
	st->bus_tile = st->lorry_tile = st->airport_tile = st->dock_tile = st->train_tile = 0;
	st->had_vehicle_of_type = 0;
	st->time_since_load = 255;
	st->time_since_unload = 255;
	st->delete_ctr = 0;
	st->facilities = 0;

	st->last_vehicle = INVALID_VEHICLE;

	for(i=0,ge=st->goods; i!=NUM_CARGO; i++, ge++) {
		ge->waiting_acceptance = 0;
		ge->days_since_pickup = 0;
		ge->enroute_from = 0xFF;
		ge->rating = 175;
		ge->last_speed = 0;
		ge->last_age = 0xFF;
	}

	_global_station_sort_dirty = true; // build a new station
}

// Update the virtual coords needed to draw the station sign.
// st = Station to update for.
static void UpdateStationVirtCoord(Station *st)
{
	Point pt = RemapCoords2(GET_TILE_X(st->xy) * 16, GET_TILE_Y(st->xy) * 16);
	pt.y -= 32;
	if (st->facilities&FACIL_AIRPORT && st->airport_type==AT_OILRIG) pt.y -= 16;

	SET_DPARAM16(0, st->index);
	SET_DPARAM8(1, st->facilities);
	UpdateViewportSignPos(&st->sign, pt.x, pt.y, STR_305C_0);
}

// Update the virtual coords needed to draw the station sign for all stations.
void UpdateAllStationVirtCoord()
{
	Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->xy != 0)
			UpdateStationVirtCoord(st);
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
static uint GetAcceptanceMask(Station *st)
{
	uint mask = 0;
	uint cur_mask = 1;
	int i;
	for(i=0; i!=NUM_CARGO; i++,cur_mask*=2) {
		if (st->goods[i].waiting_acceptance & 0x8000)
			mask |= cur_mask;
	}
	return mask;
}

// Items contains the two cargo names that are to be accepted or rejected.
// msg is the string id of the message to display.
static void ShowRejectOrAcceptNews(Station *st, uint32 items, StringID msg)
{
	if (items) {
		SET_DPARAM32(2, items >> 16);
		SET_DPARAM32(1, items & 0xFFFF);
		SET_DPARAM16(0, st->index);
		AddNewsItem(msg + ((items >> 16)?1:0), NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_TILE, NT_ACCEPTANCE, 0), st->xy, 0);
	}
}

// Get a list of the cargo types being produced around the tile.
void GetProductionAroundTiles(uint *produced, uint tile, int w, int h)
{
	int x,y;
	int x1,y1,x2,y2;
	int xc,yc;
	byte cargos[2];

	memset(produced, 0, NUM_CARGO * sizeof(uint));

	x = GET_TILE_X(tile);
	y = GET_TILE_Y(tile);

	// expand the region by 4 tiles on each side
	// while making sure that we remain inside the board.
	x2 = min(x + w+4, TILE_X_MAX+1);
	x1 = max(x-4, 0);

	y2 = min(y + h+4, TILE_Y_MAX+1);
	y1 = max(y-4, 0);

	assert(x1 < x2);
	assert(y1 < y2);
	assert(w > 0);
	assert(h > 0);

	yc = y1;
	do {
		xc = x1;
		do {
			if (!(IS_INSIDE_1D(xc, x, w) && IS_INSIDE_1D(yc, y, h))) {
				GetProducedCargoProc *gpc;
				uint tile = TILE_XY(xc, yc);
				gpc = _tile_type_procs[GET_TILETYPE(tile)]->get_produced_cargo_proc;
				if (gpc != NULL) {
					cargos[0] = cargos[1] = 0xFF;
					gpc(tile, cargos);
					if (cargos[0] != 0xFF) {
						produced[cargos[0]]++;
						if (cargos[1] != 0xFF) {
							produced[cargos[1]]++;
						}
					}
				}
			}
		} while (++xc != x2);
	} while (++yc != y2);
}

// Get a list of the cargo types that are accepted around the tile.
void GetAcceptanceAroundTiles(uint *accepts, uint tile, int w, int h)
{
	int x,y;
	int x1,y1,x2,y2;
	int xc,yc;
	AcceptedCargo ac;

	memset(accepts, 0, NUM_CARGO * sizeof(uint));

	x = GET_TILE_X(tile);
	y = GET_TILE_Y(tile);

	// expand the region by 4 tiles on each side
	// while making sure that we remain inside the board.
	x2 = min(x + w + 4, TILE_X_MAX+1);
	y2 = min(y + h + 4, TILE_Y_MAX+1);
	x1 = max(x-4, 0);
	y1 = max(y-4, 0);

	assert(x1 < x2);
	assert(y1 < y2);
	assert(w > 0);
	assert(h > 0);

	yc = y1;
	do {
		xc = x1;
		do {
			uint tile = TILE_XY(xc, yc);
			if (!IS_TILETYPE(tile, MP_STATION)) {
				GetAcceptedCargo(tile, &ac);
				accepts[ac.type_1] += ac.amount_1;
				accepts[ac.type_2] += ac.amount_2;
				accepts[ac.type_3] += ac.amount_3;
			}
		} while (++xc != x2);
	} while (++yc != y2);
}

// Update the acceptance for a station.
// show_msg controls whether to display a message that acceptance was changed.
static void UpdateStationAcceptance(Station *st, bool show_msg)
{
	uint old_acc, new_acc;
	TileIndex span[1+1+2+2+1];
	int i;
	int min_x, min_y, max_x, max_y;
	uint accepts[NUM_CARGO];

	// Don't update acceptance for a buoy
	if (st->had_vehicle_of_type & HVOT_BUOY)
		return;

	/* old accepted goods types */
	old_acc = GetAcceptanceMask(st);

	// Put all the tiles that span an area in the table.
	span[3] = span[5] = 0;
	span[0] = st->bus_tile;
	span[1] = st->lorry_tile;
	span[2] = st->train_tile;
	if (st->train_tile != 0) {
		span[3] = st->train_tile + TILE_XY(st->trainst_w-1, st->trainst_h-1);
	}
	span[4] = st->airport_tile;
	if (st->airport_tile != 0) {
		span[5] = st->airport_tile + TILE_XY(_airport_size_x[st->airport_type]-1, _airport_size_y[st->airport_type]-1);
	}
	span[6] = st->dock_tile;

	// Construct a rectangle from those points
	min_x = min_y = 0x7FFFFFFF;
	max_x = max_y = 0;

	for(i=0; i!=7; i++) {
		uint tile = span[i];
		if (tile) {
			min_x = min(min_x,GET_TILE_X(tile));
			max_x = max(max_x,GET_TILE_X(tile));
			min_y = min(min_y,GET_TILE_Y(tile));
			max_y = max(max_y,GET_TILE_Y(tile));
		}
	}

	// And retrieve the acceptance.
	if (max_x != 0) {
		GetAcceptanceAroundTiles(accepts, TILE_XY(min_x, min_y), max_x - min_x + 1, max_y-min_y+1);
	} else {
		memset(accepts, 0, sizeof(accepts));
	}

	// Adjust in case our station only accepts fewer kinds of goods
	for(i=0; i!=NUM_CARGO; i++) {
		uint amt = min(accepts[i], 15);

		// Make sure the station can accept the goods type.
		if ((i != CT_PASSENGERS && !(st->facilities & (byte)~FACIL_BUS_STOP)) ||
				(i == CT_PASSENGERS && !(st->facilities & (byte)~FACIL_TRUCK_STOP)))
			amt = 0;

		st->goods[i].waiting_acceptance = (st->goods[i].waiting_acceptance & ~0xF000) + (amt << 12);
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
static void DeleteStationIfEmpty(Station *st) {
	if (st->facilities == 0) {
		st->delete_ctr = 0;
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
}

// Tries to clear the given area. Returns the cost in case of success.
// Or an error code if it failed.
int32 CheckFlatLandBelow(uint tile, uint w, uint h, uint flags, uint invalid_dirs, int *station)
{
	int32 cost = 0, ret;

	uint tileh;
	int z, allowed_z = -1, flat_z;

	BEGIN_TILE_LOOP(tile_cur, w, h, tile)
		if (!EnsureNoVehicle(tile_cur))
			return CMD_ERROR;

		tileh = GetTileSlope(tile_cur, &z);

		// steep slopes are completely prohibited
		if (tileh & 0x10 || (((!_patches.ainew_active && _is_ai_player) || !_patches.build_on_slopes) && tileh != 0)) {
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
		// so station points to -1 if we can build on any station. or it points to a station if we're only allowed to build
		// on exactly that station.
		if (station && IS_TILETYPE(tile_cur, MP_STATION)) {
			if (_map5[tile] >= 8) {
				_error_message = STR_0007_FLAT_LAND_REQUIRED;
				return CMD_ERROR;
			} else {
				int st = _map2[tile_cur];
				if (*station == -1)
					*station = st;
				else if (*station != st) {
					_error_message = STR_3006_ADJOINS_MORE_THAN_ONE_EXISTING;
					return CMD_ERROR;
				}
			}
		} else {
			ret = DoCommandByTile(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (ret == CMD_ERROR) return CMD_ERROR;
			cost += ret;
		}
	END_TILE_LOOP(tile_cur, w, h, tile)

	return cost;
}

static bool CanExpandRailroadStation(Station *st, uint *fin, int direction)
{
	uint curw = st->trainst_w, curh = st->trainst_h;
	uint tile = fin[0];
	uint w = fin[1];
	uint h = fin[2];

	if (_patches.nonuniform_stations) {
		// determine new size of train station region..
		int x = min(GET_TILE_X(st->train_tile), GET_TILE_X(tile));
		int y = min(GET_TILE_Y(st->train_tile), GET_TILE_Y(tile));
		curw = max(GET_TILE_X(st->train_tile) + curw, GET_TILE_X(tile) + w) - x;
		curh = max(GET_TILE_Y(st->train_tile) + curh, GET_TILE_Y(tile) + h) - y;
		tile = TILE_XY(x,y);
	} else {
		// check so the direction is the same
		if ((_map5[st->train_tile] & 1) != direction) return false;

		// check if the new station adjoins the old station in either direction
		if (curw == w && st->train_tile == tile + TILE_XY(0, h)) {
			// above
			curh += h;
		} else if (curw == w && st->train_tile == tile - TILE_XY(0, curh)) {
			// below
			tile -= TILE_XY(0, curh);
			curh += h;
		} else if (curh == h && st->train_tile == tile + TILE_XY(w, 0)) {
			// to the left
			curw += w;
		} else if (curh == h && st->train_tile == tile - TILE_XY(curw, 0)) {
			// to the right
			tile -= TILE_XY(curw, 0);
			curw += w;
		} else
			return false;
	}
	// make sure the final size is not too big.
	if (curw > _patches.station_spread || curh > _patches.station_spread) return false;

	// now tile contains the new value for st->train_tile
	// curw, curh contain the new value for width and height
	fin[0] = tile;
	fin[1] = curw;
	fin[2] = curh;
	return true;
}

static byte FORCEINLINE *CreateSingle(byte *layout, int n)
{
	int i = n;
	do *layout++ = 0; while (--i);
	layout[((n-1) >> 1)-n] = 2;
	return layout;
}

static byte FORCEINLINE *CreateMulti(byte *layout, int n, byte b)
{
	int i = n;
	do *layout++ = b; while (--i);
	if (n > 4) {
		layout[0-n] = 0;
		layout[n-1-n] = 0;
	}
	return layout;
}

// stolen from TTDPatch
static void GetStationLayout(byte *layout, int numtracks, int plat_len)
{
	if (plat_len == 1) {
		CreateSingle(layout, numtracks);
	} else {
		if (numtracks & 1)
			layout = CreateSingle(layout, plat_len);
		numtracks>>=1;

		while (--numtracks >= 0) {
			layout = CreateMulti(layout, plat_len, 4);
			layout = CreateMulti(layout, plat_len, 6);
		}
	}
}

/* build railroad station
 * p1 & 1 - orientation
 * (p1 >> 8) & 0xFF - numtracks
 * (p1 >> 16) & 0xFF - platform length
 * p2  - railtype
 */

int32 CmdBuildRailroadStation(int x_org, int y_org, uint32 flags, uint32 p1, uint32 p2)
{
	/* unpack params */
	int w_org,h_org;
	uint tile_org;
	int32 cost, ret;
	Station *st;
	int est;
	int plat_len, numtracks;
	int direction;
	uint finalvalues[3];

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile_org = TILE_FROM_XY(x_org,y_org);

	/* Does the authority allow this? */
	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile_org))
		return CMD_ERROR;

	{
		/* unpack parameters */
		direction = p1 & 1;
		plat_len = (p1 >> 16) & 0xFF;
		numtracks = (p1 >> 8) & 0xFF;
		/* w = length, h = num_tracks */
		if (direction) {
			h_org = plat_len;
			w_org = numtracks;
		} else {
			w_org = plat_len;
			h_org = numtracks;
		}
	}

	// these values are those that will be stored in train_tile and station_platforms
	finalvalues[0] = tile_org;
	finalvalues[1] = w_org;
	finalvalues[2] = h_org;

	// Make sure the area below consists of clear tiles. (OR tiles belonging to a certain rail station)
	est = -1;
	// If DC_EXEC is in flag, do not want to pass it to CheckFlatLandBelow, because of a nice bug
	//  for detail info, see: https://sourceforge.net/tracker/index.php?func=detail&aid=1029064&group_id=103924&atid=636365
	if ((ret=CheckFlatLandBelow(tile_org, w_org, h_org, flags&~DC_EXEC, 5 << direction, _patches.nonuniform_stations ? &est : NULL)) == CMD_ERROR) return CMD_ERROR;
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
			if ((!_patches.ainew_active && _is_ai_player) || !_patches.join_stations || !CanExpandRailroadStation(st, finalvalues, direction))
				return_cmd_error(STR_3005_TOO_CLOSE_TO_ANOTHER_RAILROAD);
		}

		if (!CheckStationSpreadOut(st, tile_org, w_org, h_org))
			return CMD_ERROR;

	}	else {
		// Create a new station
		st = AllocateStation();
		if (st == NULL)
			return CMD_ERROR;

		st->town = ClosestTownFromTile(tile_org, (uint)-1);
		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(st->town->have_ratings, _current_player);

		if (!GenerateStationName(st, tile_org, 0))
			return CMD_ERROR;

		if (flags & DC_EXEC)
			StationInitialize(st, tile_org);
	}

	if (flags & DC_EXEC) {
		int tile_delta;
		byte *layout_ptr;
		uint station_index = st->index;

		// Now really clear the land below the station
		// It should never return CMD_ERROR.. but you never know ;)
		//  (a bit strange function name for it, but it really does clear the land, when DC_EXEC is in flags)
		if (CheckFlatLandBelow(tile_org, w_org, h_org, flags, 5 << direction, _patches.nonuniform_stations ? &est : NULL) == CMD_ERROR) return CMD_ERROR;

		st->train_tile = finalvalues[0];
		if (!st->facilities) st->xy = finalvalues[0];
		st->facilities |= FACIL_TRAIN;
		st->owner = _current_player;

		st->trainst_w = finalvalues[1];
		st->trainst_h = finalvalues[2];

		st->build_date = _date;

		tile_delta = direction ? TILE_XY(0,1) : TILE_XY(1,0);

		layout_ptr = alloca(numtracks * plat_len);
		GetStationLayout(layout_ptr, numtracks, plat_len);

		do {
			int tile = tile_org;
			int w = plat_len;
			do {

				ModifyTile(tile,
					MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
					MP_MAP2 | MP_MAP5 | MP_MAP3LO | MP_MAP3HI_CLEAR,
					station_index, /* map2 parameter */
					p2,				/* map3lo parameter */
					(*layout_ptr++) + direction   /* map5 parameter */
				);

				tile += tile_delta;
			} while (--w);
			tile_org += tile_delta ^ TILE_XY(1,1);
		} while (--numtracks);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}

	return cost;
}

static bool TileBelongsToRailStation(Station *st, uint tile)
{
	return IS_TILETYPE(tile, MP_STATION) && _map2[tile] == st->index && _map5[tile] < 8;
}

static void MakeRailwayStationAreaSmaller(Station *st)
{
	uint w = st->trainst_w;
	uint h = st->trainst_h;
	uint tile = st->train_tile;
	uint i;

restart:

	// too small?
	if (w != 0 && h != 0) {
		// check the left side, x = constant, y changes
		for(i=0; !TileBelongsToRailStation(st, tile + TILE_XY(0,i)) ;)
			// the left side is unused?
			if (++i==h) { tile += TILE_XY(1, 0); w--; goto restart; }

		// check the right side, x = constant, y changes
		for(i=0; !TileBelongsToRailStation(st, tile + TILE_XY(w-1,i)) ;)
			// the right side is unused?
			if (++i==h) { w--; goto restart; }

		// check the upper side, y = constant, x changes
		for(i=0; !TileBelongsToRailStation(st, tile + TILE_XY(i,0)) ;)
			// the left side is unused?
			if (++i==w) { tile += TILE_XY(0, 1); h--; goto restart; }

		// check the lower side, y = constant, x changes
		for(i=0; !TileBelongsToRailStation(st, tile + TILE_XY(i,h-1)) ;)
			// the left side is unused?
			if (++i==w) { h--; goto restart; }
	} else {
		tile = 0;
	}

	st->trainst_w = w;
	st->trainst_h = h;
	st->train_tile = tile;
}

// remove a single tile from a railroad station
int32 CmdRemoveFromRailroadStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x, y);
	Station *st;

	// make sure the specified tile belongs to the current player, and that it is a railroad station.
	if (!IS_TILETYPE(tile, MP_STATION) || _map5[tile] >= 8 || !_patches.nonuniform_stations) return CMD_ERROR;
	st = DEREF_STATION(_map2[tile]);
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
uint GetStationPlatforms(Station *st, uint tile)
{
	uint t;
	int dir,delta;
	int len;
	assert(TileBelongsToRailStation(st, tile));

	len = 0;
	dir = _map5[tile]&1;
	delta = dir ? TILE_XY(0,1) : TILE_XY(1,0);

	// find starting tile..
	t = tile;
	do { t -= delta; len++; } while (TileBelongsToRailStation(st, t) && (_map5[t]&1) == dir);

	// find ending tile
	t = tile;
	do { t += delta; len++; }while (TileBelongsToRailStation(st, t) && (_map5[t]&1) == dir);

	return len - 1;
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
			tile += TILE_XY(1, 0);
		} while (--w);
		w = w_bak;
		tile = tile + TILE_XY(-w, 1);
	} while (--h);

	if (flags & DC_EXEC) {
		st->train_tile = 0;
		st->facilities &= ~FACIL_TRAIN;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

int32 DoConvertStationRail(uint tile, uint totype, bool exec)
{
	Station *st;

	st = DEREF_STATION(_map2[tile]);
	if (!CheckOwnership(st->owner) || !EnsureNoVehicle(tile)) return CMD_ERROR;

	// tile is not a railroad station?
	if (_map5[tile] >= 8) return CMD_ERROR;

	// tile is already of requested type?
	if ( (uint)(_map3_lo[tile] & 0xF) == totype) return CMD_ERROR;

	if (exec) {
		// change type.
		_map3_lo[tile] = (_map3_lo[tile] & 0xF0) + totype;
		MarkTileDirtyByTile(tile);
	}

	return _price.build_rail >> 1;
}

/* Build a bus station
 * p1 - direction
 * p2 - unused
 */

int32 CmdBuildBusStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile;
	int32 cost;
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	if ((cost=CheckFlatLandBelow(tile, 1, 1, flags, 1 << p1, NULL)) == CMD_ERROR)
		return CMD_ERROR;

	st = GetStationAround(tile, 1, 1, -1);
	if (st == CHECK_STATIONS_ERR)
		return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st!=NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1))
			return CMD_ERROR;

		if (st->bus_tile != 0)
			return_cmd_error(STR_3044_TOO_CLOSE_TO_ANOTHER_BUS);
	} else {
		Town *t;

		st = AllocateStation();
		if (st == NULL)
			return CMD_ERROR;

		st->town = t = ClosestTownFromTile(tile, (uint)-1);

		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, 0))
			return CMD_ERROR;

		if (flags & DC_EXEC)
			StationInitialize(st, tile);
	}

	cost += _price.build_bus_station;

	if (flags & DC_EXEC) {
		st->bus_tile = tile;
		if (!st->facilities) st->xy = tile;
		st->facilities |= FACIL_BUS_STOP;
		st->bus_stop_status = 3;
		st->owner = _current_player;

		st->build_date = _date;

		ModifyTile(tile,
			MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
			MP_MAP2 | MP_MAP5 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
			st->index,			/* map2 parameter */
			p1 + 0x47       /* map5 parameter */
		);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
	return cost;
}

// Remove a bus station
static int32 RemoveBusStation(Station *st, uint32 flags)
{
	uint tile;

	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	tile = st->bus_tile;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);

		st->bus_tile = 0;
		st->facilities &= ~FACIL_BUS_STOP;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return _price.remove_bus_station;
}


/* Build a truck station
 * p1 - direction
 * p2 - unused
 */
int32 CmdBuildTruckStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile;
	int32 cost = 0;
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);

	if (!(flags & DC_NO_TOWN_RATING) && !CheckIfAuthorityAllows(tile))
		return CMD_ERROR;

	if ((cost=CheckFlatLandBelow(tile, 1, 1, flags, 1 << p1, NULL)) == CMD_ERROR)
		return CMD_ERROR;

	st = GetStationAround(tile, 1, 1, -1);
	if (st == CHECK_STATIONS_ERR)
		return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st!=NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1))
			return CMD_ERROR;

		if (st->lorry_tile != 0)
			return_cmd_error(STR_3045_TOO_CLOSE_TO_ANOTHER_TRUCK);
	} else {
		Town *t;

		st = AllocateStation();
		if (st == NULL)
			return CMD_ERROR;

		st->town = t = ClosestTownFromTile(tile, (uint)-1);

		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, 0))
			return CMD_ERROR;

		if (flags & DC_EXEC)
			StationInitialize(st, tile);
	}

	cost += _price.build_truck_station;

	if (flags & DC_EXEC) {
		st->lorry_tile = tile;
		if (!st->facilities) st->xy = tile;
		st->facilities |= FACIL_TRUCK_STOP;
		st->truck_stop_status = 3;
		st->owner = _current_player;

		st->build_date = _date;

		ModifyTile(tile,
			MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
			MP_MAP2 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR | MP_MAP5,
			st->index,			/* map2 parameter */
			p1 + 0x43       /* map5 parameter */
		);

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}
	return cost;
}

// Remove a truck station
static int32 RemoveTruckStation(Station *st, uint32 flags)
{
	uint tile;

	if (_current_player != OWNER_WATER && !CheckOwnership(st->owner))
		return CMD_ERROR;

	tile = st->lorry_tile;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);

		st->lorry_tile = 0;
		st->facilities &= ~FACIL_TRUCK_STOP;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return _price.remove_truck_station;
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
	31, 9, 33, 9, 9, 32,
	27, 36, 29, 34, 8, 10,
	30, 11, 35, 13, 20, 21,
	51, 12, 14, 17, 19, 28,
	38, 13, 15, 16, 18, 39,
	26, 22, 23, 24, 25, 26
};

// Metropolitain Airport (large) - 2 runways
static const byte _airport_map5_tiles_metropolitan[] = {
	31, 9, 33, 9, 9, 32,
	27, 36, 29, 34, 8, 10,
	30, 11, 35, 13, 20, 21,
 102,  8,  8,  8,  8, 28,
	83, 84, 84, 84, 84, 83,
	26, 23, 23, 23, 23, 26
};

// International Airport (large) - 2 runways
static const byte _airport_map5_tiles_international[] = {
  88, 89, 89, 89, 89, 89, 88,
 	51,  8,  8,  8,  8,  8, 32,
	30,  8, 11, 27, 11,  8, 10,
	32,  8, 11, 27, 11,  8, 114,
	87,  8, 11, 85, 11,  8, 114,
	87,  8,  8,  8,  8,  8, 90,
	26, 23, 23, 23, 23, 23, 26
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

/* Place an Airport
 * p1 - airport type
 * p2 - unused
 */
int32 CmdBuildAirport(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile;
	Town *t;
	Station *st;
	int32 cost;
	int w,h;
	bool airport_upgrade = true;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);

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
			SET_DPARAM16(0, t->index);
			return_cmd_error(STR_2035_LOCAL_AUTHORITY_REFUSES);
		}
	}

	w = _airport_size_x[p1];
	h = _airport_size_y[p1];

	cost = CheckFlatLandBelow(tile, w, h, flags, 0, NULL);
	if (cost == CMD_ERROR)
		return CMD_ERROR;

	st = GetStationAround(tile, w, h, -1);
	if (st == CHECK_STATIONS_ERR)
		return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st!=NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1))
			return CMD_ERROR;

		if (st->airport_tile != 0)
			return_cmd_error(STR_300D_TOO_CLOSE_TO_ANOTHER_AIRPORT);
	} else {
		Town *t;

		airport_upgrade = false;

		st = AllocateStation();
		if (st == NULL)
			return CMD_ERROR;

		st->town = t = ClosestTownFromTile(tile, (uint)-1);

		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		// if airport type equals Heliport then generate
		// type 5 name, which is heliport, otherwise airport names (1)
		if (!GenerateStationName(st, tile, p1 == AT_HELIPORT ? 5 : 1))
			return CMD_ERROR;

		if (flags & DC_EXEC)
			StationInitialize(st, tile);
	}

	cost += _price.build_airport * w * h;

	if (flags & DC_EXEC) {
		st->owner = _current_player;
		if (_current_player == _local_player && p1 <= AT_INTERNATIONAL) {
      _last_built_aircraft_depot_tile = tile + GetAirport(p1)->airport_depots[0];
    }

		st->airport_tile = tile;
		if (!st->facilities) st->xy = tile;
		st->facilities |= FACIL_AIRPORT;
		st->airport_type = (byte)p1;
		st->airport_flags = 0;

		st->build_date = _date;

		/*	if airport was demolished while planes were en-route to it, the positions can no longer
				be the same (v->u.air.pos), since different airports have different indexes. So update
				all planes en-route to this airport. Only update if
				1. airport is upgraded
				2. airport is added to existing station (unfortunately unavoideable)
		*/
		if (airport_upgrade) {UpdateAirplanesOnNewStation(st);}

		{
			const byte *b = _airport_map5_tiles[p1];
BEGIN_TILE_LOOP(tile_cur,w,h,tile)
				ModifyTile(tile_cur,
					MP_SETTYPE(MP_STATION) | MP_MAPOWNER_CURRENT |
					MP_MAP2 | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR | MP_MAP5,
					st->index, *b++);
END_TILE_LOOP(tile_cur,w,h,tile)
		}

		UpdateStationVirtCoordDirty(st);
		UpdateStationAcceptance(st, false);
		InvalidateWindow(WC_STATION_LIST, st->owner);
	}

	return cost;
}

static int32 RemoveAirport(Station *st, uint32 flags)
{
	uint tile;
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
		if (st->airport_type <= AT_INTERNATIONAL)
      DeleteWindowById(WC_VEHICLE_DEPOT, tile + GetAirport(st->airport_type)->airport_depots[0]);
		st->airport_tile = 0;
		st->facilities &= ~FACIL_AIRPORT;

		UpdateStationVirtCoordDirty(st);
		DeleteStationIfEmpty(st);
	}

	return cost;
}

/* Build a buoy
 * p1,p2 unused
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
	if (st == NULL)
		return CMD_ERROR;

	st->town = ClosestTownFromTile(ti.tile, (uint)-1);
	st->sign.width_1 = 0;

	if (!GenerateStationName(st, ti.tile, 4))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		StationInitialize(st, ti.tile);
		st->dock_tile = ti.tile;
		st->facilities |= FACIL_DOCK;
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

static int32 RemoveBuoy(Station *st, uint32 flags)
{
	uint tile;

	if (_current_player >= MAX_PLAYERS) {
		/* XXX: strange stuff */
		return_cmd_error(INVALID_STRING_ID);
	}

	tile = st->dock_tile;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		st->dock_tile = 0;
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

static const TileIndexDiff _dock_tileoffs_chkaround[4] = {
	TILE_XY(-1,0),
	0,0,
	TILE_XY(0,-1),
};
static const byte _dock_w_chk[4] = { 2,1,2,1 };
static const byte _dock_h_chk[4] = { 1,2,1,2 };

int32 CmdBuildDock(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti;
	int direction;
	int32 cost;
	uint tile, tile_cur;
	Station *st;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	FindLandscapeHeight(&ti, x, y);

	if ((direction=0,ti.tileh) != 3 &&
			(direction++,ti.tileh) != 9 &&
			(direction++,ti.tileh) != 12 &&
			(direction++,ti.tileh) != 6)
		return_cmd_error(STR_304B_SITE_UNSUITABLE);

	if (!EnsureNoVehicle(ti.tile))
		return CMD_ERROR;

	cost = DoCommandByTile(ti.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (cost == CMD_ERROR)
		return CMD_ERROR;

	tile_cur = (tile=ti.tile) + _tileoffs_by_dir[direction];

	if (!EnsureNoVehicle(tile_cur))
		return CMD_ERROR;

	FindLandscapeHeightByTile(&ti, tile_cur);
	if (ti.tileh != 0 || ti.type != MP_WATER)
		return_cmd_error(STR_304B_SITE_UNSUITABLE);

	cost = DoCommandByTile(tile_cur, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (cost == CMD_ERROR)
		return CMD_ERROR;

	tile_cur = tile_cur + _tileoffs_by_dir[direction];
	FindLandscapeHeightByTile(&ti, tile_cur);
	if (ti.tileh != 0 || ti.type != MP_WATER)
		return_cmd_error(STR_304B_SITE_UNSUITABLE);

	/* middle */
	st = GetStationAround(tile + _dock_tileoffs_chkaround[direction],
		_dock_w_chk[direction], _dock_h_chk[direction], -1);
	if (st == CHECK_STATIONS_ERR)
		return CMD_ERROR;

	/* Find a station close to us */
	if (st == NULL) {
		st = GetClosestStationFromTile(tile, 8, _current_player);
		if (st!=NULL && st->facilities) st = NULL;
	}

	if (st != NULL) {
		if (st->owner != OWNER_NONE && st->owner != _current_player)
			return_cmd_error(STR_3009_TOO_CLOSE_TO_ANOTHER_STATION);

		if (!CheckStationSpreadOut(st, tile, 1, 1))
			return CMD_ERROR;

		if (st->dock_tile != 0)
			return_cmd_error(STR_304C_TOO_CLOSE_TO_ANOTHER_DOCK);
	} else {
		Town *t;

		st = AllocateStation();
		if (st == NULL)
			return CMD_ERROR;

		st->town = t = ClosestTownFromTile(tile, (uint)-1);

		if (_current_player < MAX_PLAYERS && flags&DC_EXEC)
			SETBIT(t->have_ratings, _current_player);

		st->sign.width_1 = 0;

		if (!GenerateStationName(st, tile, 3))
			return CMD_ERROR;

		if (flags & DC_EXEC)
			StationInitialize(st, tile);
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

		ModifyTile(tile + _tileoffs_by_dir[direction],
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
	uint tile1, tile2;

	if (!CheckOwnership(st->owner))
		return CMD_ERROR;

	tile1 = st->dock_tile;
	tile2 = tile1 + _tileoffs_by_dir[_map5[tile1] - 0x4C];

	if (!EnsureNoVehicle(tile1))
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile2))
		return CMD_ERROR;

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


typedef struct DrawTileSeqStruct {
	int8 delta_x;
	int8 delta_y;
	int8 delta_z;
	byte width,height;
	byte unk;
	SpriteID image;
} DrawTileSeqStruct;


static void DrawTile_Station(TileInfo *ti)
{
	uint32 image_or_modificator;
	uint32 base_img, image;
	const DrawTileSeqStruct *dtss;
	const byte *t;

	// station_land array has been increased from 82 elements to 114
	// but this is something else. If AI builds station with 114 it looks all weird
	base_img = (_map3_lo[ti->tile] & 0xF) * 82;

	{
		uint owner = _map_owner[ti->tile];
		image_or_modificator = 0x315 << 16; /* NOTE: possible bug in ttd here? */
		if (owner < MAX_PLAYERS)
			image_or_modificator = PLAYER_SPRITE_COLOR(owner);
	}

	// don't show foundation for docks (docks are between 76 (0x4C) and 81 (0x51))
	if (ti->tileh != 0 && (ti->map5 < 0x4C || ti->map5 > 0x51))
		DrawFoundation(ti, ti->tileh);

	t = _station_display_datas[ti->map5];

	image = *(const uint32*)t;
	t += sizeof(uint32);
	if (image & 0x8000)
		image |= image_or_modificator;
	DrawGroundSprite(image + base_img);

	for(dtss = (const DrawTileSeqStruct *)t; (byte)dtss->delta_x != 0x80; dtss++) {
		if ((byte)dtss->delta_z != 0x80) {
			image =	dtss->image + base_img;
			if (_display_opt & DO_TRANS_BUILDINGS) {
				if (image&0x8000) image |= image_or_modificator;
			} else {
				image = (image & 0x3FFF) | 0x03224000;
			}

			AddSortableSpriteToDraw(image, ti->x + dtss->delta_x, ti->y + dtss->delta_y, dtss->width, dtss->height, dtss->unk, ti->z + dtss->delta_z);
		} else {
			image = *(const uint32*)&dtss->height + base_img; /* endian ok */

			if (_display_opt & DO_TRANS_BUILDINGS) {
				if (image&0x8000) image |= image_or_modificator;
			} else {
				image = (image & 0x3FFF) | 0x03224000;
			}
			AddChildSpriteScreen(image, dtss->delta_x, dtss->delta_y);
		}
	}
}

void StationPickerDrawSprite(int x, int y, int railtype, int image)
{
	uint32 ormod, img;
	const DrawTileSeqStruct *dtss;
	const byte *t;

	/* baseimage */
	railtype *= TRACKTYPE_SPRITE_PITCH;

	ormod = PLAYER_SPRITE_COLOR(_local_player);

	t = _station_display_datas[image];

	img = *(const uint32*)t;
	t += sizeof(uint32);
	if (img & 0x8000)
		img |= ormod;
	DrawSprite(img, x, y);

	for(dtss = (const DrawTileSeqStruct *)t; (byte)dtss->delta_x != 0x80; dtss++) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		DrawSprite((dtss->image | ormod) + railtype, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Station(TileInfo *ti)
{
	uint z = ti->z;
	if (ti->tileh != 0)
		z += 8;
	return z;
}

static uint GetSlopeTileh_Station(TileInfo *ti)
{
	return 0;
}

static void GetAcceptedCargo_Station(uint tile, AcceptedCargo *ac)
{
	/* not used */
}

static void GetTileDesc_Station(uint tile, TileDesc *td)
{
	byte m5;
	StringID str;

	td->owner = _map_owner[tile];
	td->build_date = DEREF_STATION(_map2[tile])->build_date;

	m5 = _map5[tile];
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


static const byte _tile_track_status_rail[8] = { 1,2,1,2,1,2,1,2 };

static uint32 GetTileTrackStatus_Station(uint tile, TransportType mode) {
	uint i = _map5[tile];
	uint j = 0;

	if (mode == TRANSPORT_RAIL) {
		if (i < 8)
			j = _tile_track_status_rail[i];
		j += (j << 8);
	} else if (mode == TRANSPORT_ROAD) {
		Station *st = DEREF_STATION(_map2[tile]);
		if ( (IS_BYTE_INSIDE(i, 0x43, 0x47) && (_patches.roadveh_queue || st->truck_stop_status&3)) ||
		(IS_BYTE_INSIDE(i, 0x47, 0x4B) && (_patches.roadveh_queue || st->bus_stop_status&3)) ) {
			/* This is a bus/truck stop, and there is free space
			 * (or we allow queueing) */

			/* We reverse the dir because it points out of the
			 * exit, and we want to get in. Maybe we should return
			 * both dirs here? */
			byte dir = _reverse_dir[(i-0x43)&3];
			j = 1 << _dir_to_straight_trackdir[dir];
		}
	} else if (mode == TRANSPORT_WATER) {
		// buoy is coded as a station, it is always on open water
		// (0x3F, all tracks available)
		if (i == 0x52) j = 0x3F;
		j += (j << 8);
	}
	return j;
}

static void TileLoop_Station(uint tile)
{
  //FIXME -- GetTileTrackStatus_Station -> animated stationtiles
  // hardcoded.....not good
  // 0x27 - large big airport (39)
  // 0x66 - radar metropolitan airport (102)
  // 0x5A - radar international airport (90)
  // 0x3A - flag small airport (58)
	if (_map5[tile] == 39 || _map5[tile] == 58 || _map5[tile] == 90 || _map5[tile] == 102)
		AddAnimatedTile(tile);

	// treat a bouy tile as water.
	else if (_map5[tile] == 0x52)
		TileLoop_Water(tile);

	// treat a oilrig (the station part) as water
	else if (_map5[tile] == 0x4B)
		TileLoop_Water(tile);

}


static void AnimateTile_Station(uint tile)
{
	byte m5 = _map5[tile];
	//FIXME -- AnimateTile_Station -> not nice code, lots of things double
  // again hardcoded...was a quick hack

  // turning radar / windsack on airport
	if (m5 >= 39 && m5 <= 50) { // turning radar (39 - 50)
		if (_tick_counter & 3)
			return;

		if (++m5 == 50+1)
			m5 = 39;

		_map5[tile] = m5;
		MarkTileDirtyByTile(tile);
  //added - begin
	} else if (m5 >= 90 && m5 <= 113) { // turning radar with ground under it (different fences) (90 - 101 | 102 - 113)
		if (_tick_counter & 3)
			return;

		m5++;

		if (m5 == 101+1) {m5 = 90;}  // radar with fences in south
		else if (m5 == 113+1) {m5 = 102;} // radar with fences in north

		_map5[tile] = m5;
		MarkTileDirtyByTile(tile);
	//added - end
	} else if (m5 >= 0x3A && m5 <= 0x3D) {  // windsack (58 - 61)
		if (_tick_counter & 1)
			return;

		if (++m5 == 0x3D+1)
			m5 = 0x3A;

		_map5[tile] = m5;
		MarkTileDirtyByTile(tile);
	}
}

static void ClickTile_Station(uint tile)
{
  // 0x20 - hangar large airport (32)
  // 0x41 - hangar small airport (65)
	if (_map5[tile] == 32 || _map5[tile] == 65) {
		ShowAircraftDepotWindow(tile);
	} else {
		ShowStationViewWindow(_map2[tile]);
	}
}

static INLINE bool IsTrainStationTile(uint tile) {
	return IS_TILETYPE(tile, MP_STATION) && IS_BYTE_INSIDE(_map5[tile], 0, 8);
}

static const byte _enter_station_speedtable[12] = {
	215, 195, 175, 155, 135, 115, 95, 75, 55, 35, 15, 0
};

static uint32 VehicleEnter_Station(Vehicle *v, uint tile, int x, int y)
{
	byte station_id;
	byte dir;
	uint16 spd;

	if (v->type == VEH_Train) {
		if (IS_BYTE_INSIDE(_map5[tile], 0, 8) && v->subtype == 0 &&
			!IsTrainStationTile(tile + _tileoffs_by_dir[v->direction >> 1])) {

			station_id = _map2[tile];
			if ((!(v->next_order & OF_NON_STOP) && !_patches.new_nonstop) ||
					 (((v->next_order & OT_MASK) == OT_GOTO_STATION && v->next_order_param == station_id))) {

				if (!(_patches.new_nonstop && (v->next_order & OF_NON_STOP)) && v->next_order != OT_LEAVESTATION && v->last_station_visited != station_id) {
					x &= 0xF;
					y &= 0xF;

					dir = v->direction & 6;
					if (dir & 2) intswap(x,y);
					if (y == 8) {
						if (dir != 2 && dir != 4) {
							x = (~x)&0xF;
						}
						if (x == 12) return 2 | (station_id << 8); /* enter station */
						if (x < 12) {
							v->vehstatus |= VS_TRAIN_SLOWING;
							spd = _enter_station_speedtable[x];
							if (spd < v->cur_speed)
								v->cur_speed = spd;
						}
					}
				}
			}
		}
	} else if (v->type == VEH_Road) {
		if (v->u.road.state < 16 && (v->u.road.state&4)==0 && v->u.road.frame==0) {
			Station *st = DEREF_STATION(_map2[tile]);
			byte m5 = _map5[tile];
			byte *b, bb,state;

			if (IS_BYTE_INSIDE(m5, 0x43, 0x4B)) {
				b = (m5 >= 0x47) ? &st->bus_stop_status : &st->truck_stop_status;

				bb = *b;

				/* bb bits 1..0 describe current the two parking spots.
				0 means occupied, 1 means free. */

				// Station busy?
				if (bb & 0x80 || (bb&3) == 0)
					return 8;

				state = v->u.road.state + 32;
				if (bb & 1) {
					bb &= ~1;
				} else {
					bb &= ~2;
					state += 2;
				}
				*b = bb;
				v->u.road.state = state;
			}
		}
	}

	return 0;
}

static void DeleteStation(Station *st)
{
	int index;
	st->xy = 0;

	DeleteName(st->string_id);
	MarkStationDirty(st);
	_global_station_sort_dirty = true; // delete station, remove sign
	InvalidateWindowClasses(WC_STATION_LIST);

	index = st->index;
	DeleteWindowById(WC_STATION_VIEW, index);
	DeleteCommandFromVehicleSchedule((index << 8) + OT_GOTO_STATION);
	DeleteSubsidyWithStation(index);
}

void DeleteAllPlayerStations()
{
	Station *st;

	FOR_ALL_STATIONS(st) {
		if (st->xy && st->owner < MAX_PLAYERS)
			DeleteStation(st);
	}
}

/* this function is called for one station each tick */
static void StationHandleBigTick(Station *st)
{
	UpdateStationAcceptance(st, true);

	if (st->facilities == 0) {
		if (++st->delete_ctr >= 8)
			DeleteStation(st);
	}
}

static INLINE void byte_inc_sat(byte *p) { byte b = *p + 1; if (b != 0) *p = b; }

static byte _rating_boost[3] = { 0, 31, 63};

static void UpdateStationRating(Station *st)
{
	GoodsEntry *ge;
	int rating, index;
	int waiting;
	bool waiting_changed = false;

	byte_inc_sat(&st->time_since_load);
	byte_inc_sat(&st->time_since_unload);

	ge = st->goods;
	do {
		if (ge->enroute_from != 0xFF) {
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

			{
				if (!IS_HUMAN_PLAYER(st->owner) && st->owner != OWNER_NONE)
							rating += _rating_boost[_opt.diff.competitor_intelligence];
			}

			if (st->owner < 8 && HASBIT(st->town->statues, st->owner))
				rating += 26;

			{
				byte days = ge->days_since_pickup;
				if (st->last_vehicle != INVALID_VEHICLE &&
						(&_vehicles[st->last_vehicle])->type == VEH_Ship)
							days >>= 2;
				(days > 21) ||
				(rating += 25, days > 12) ||
				(rating += 25, days > 6) ||
				(rating += 45, days > 3) ||
				(rating += 35, true);
			}

			{
				waiting = ge->waiting_acceptance & 0xFFF;
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

				if (waiting_changed)
					ge->waiting_acceptance = (ge->waiting_acceptance & ~0xFFF) + waiting;
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

	if (st->facilities == 0)
		return;

	b = st->delete_ctr + 1;
	if (b >= 185) b = 0;
	st->delete_ctr = b;

	if (b == 0)
		UpdateStationRating(st);
}

void OnTick_Station()
{
	int i;
	Station *st;

	if (_game_mode == GM_EDITOR)
		return;

	i = _station_tick_ctr;
	if (++_station_tick_ctr == lengthof(_stations))
		_station_tick_ctr = 0;

	st = DEREF_STATION(i);
	if (st->xy != 0)
		StationHandleBigTick(st);

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0)
			StationHandleSmallTick(st);
	}

}

void StationMonthlyLoop()
{
}


void ModifyStationRatingAround(TileIndex tile, byte owner, int amount, uint radius)
{
	Station *st;
	GoodsEntry *ge;
	int i;

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0 && st->owner == owner && GetTileDist(tile, st->xy) <= radius) {
			ge = st->goods;
			for(i=0; i!=NUM_CARGO; i++,ge++) {
				if (ge->enroute_from != 0xFF) {
					ge->rating = clamp(ge->rating + amount, 0, 255);
				}
			}
		}
	}
}

static void UpdateStationWaiting(Station *st, int type, uint amount)
{
	st->goods[type].waiting_acceptance =
		(st->goods[type].waiting_acceptance & ~0xFFF) +
			min(0xFFF, (st->goods[type].waiting_acceptance & 0xFFF) + amount);

	st->goods[type].enroute_time = 0;
	st->goods[type].enroute_from = st->index;
	InvalidateWindow(WC_STATION_VIEW, st->index);
}

int32 CmdRenameStation(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str,old_str;
	Station *st;

	str = AllocateName((byte*)_decode_parameters, 6);
	if (str == 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		st = DEREF_STATION(p1);
		old_str = st->string_id;
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


uint MoveGoodsToStation(uint tile, int w, int h, int type, uint amount)
{
	Station *around_ptr[8];
	byte around[8], st_index;
	int i;
	Station *st;
	uint moved;
	uint best_rating, best_rating2;
	Station *st1, *st2;
	int t;

	memset(around, 0xff, sizeof(around));

	w += 8;
	h += 8;

	BEGIN_TILE_LOOP(cur_tile, w, h, tile - TILE_XY(4,4))
		cur_tile = TILE_MASK(cur_tile);
		if (IS_TILETYPE(cur_tile, MP_STATION)) {
			st_index = _map2[cur_tile];
			for(i=0; i!=8; i++)	{
				if (around[i] == 0xFF) {
					st = DEREF_STATION(st_index);
					if ((st->had_vehicle_of_type & HVOT_BUOY) == 0 &&
							( !st->town->exclusive_counter || (st->town->exclusivity == st->owner) ) && // check exclusive transport rights
							st->goods[type].rating != 0 &&
							(!_patches.selectgoods || st->goods[type].last_speed) && // if last_speed is 0, no vehicle has been there.
							((st->facilities & (byte)~FACIL_BUS_STOP)!=0 || type==CT_PASSENGERS) && // if we have other fac. than a bus stop, or the cargo is passengers
							((st->facilities & (byte)~FACIL_TRUCK_STOP)!=0 || type!=CT_PASSENGERS)) { // if we have other fac. than a cargo bay or the cargo is not passengers

						around[i] = st_index;
						around_ptr[i] = st;
					}
					break;
				} else if (around[i] == st_index)
					break;
			}
		}
	END_TILE_LOOP(cur_tile, w, h, tile - TILE_XY(4,4))

	/* no stations around at all? */
	if (around[0] == 0xFF)
		return 0;

	if (around[1] == 0xFF) {
		/* only one station around */
		moved = (amount * around_ptr[0]->goods[type].rating >> 8) + 1;
		UpdateStationWaiting(around_ptr[0], type, moved);
		return moved;
	}

	/* several stations around, find the two with the highest rating */
	st2 = st1 = NULL;
	best_rating = best_rating2 = 0;
	for(i=0; i!=8 && around[i] != 0xFF; i++) {
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

	assert(amount >= 0);

	if (amount != 0) {
		moved += (amount = (amount * best_rating2 >> 8) + 1);
		UpdateStationWaiting(st2, type, amount);
	}

	return moved;
}

void BuildOilRig(uint tile)
{
	Station *st;
	int j;

	FOR_ALL_STATIONS(st) {
		if (st->xy == 0) {
			st->town = ClosestTownFromTile(tile, (uint)-1);
			st->sign.width_1 = 0;
			if (!GenerateStationName(st, tile, 2))
				return;

			_map_type_and_height[tile] &= 0xF;
			_map_type_and_height[tile] |= MP_STATION << 4;
			_map5[tile] = 0x4B;
			_map_owner[tile] = OWNER_NONE;
			_map3_lo[tile] = 0;
			_map3_hi[tile] = 0;
			_map2[tile] = st->index;

			st->owner = OWNER_NONE;
      st->airport_flags = 0;
			st->airport_type = AT_OILRIG;
			st->xy = tile;
			st->bus_tile = 0;
			st->lorry_tile = 0;
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
			for(j=0; j!=NUM_CARGO; j++) {
				st->goods[j].waiting_acceptance = 0;
				st->goods[j].days_since_pickup = 0;
				st->goods[j].enroute_from = 0xFF;
				st->goods[j].rating = 175;
				st->goods[j].last_speed = 0;
				st->goods[j].last_age = 255;
			}

			UpdateStationVirtCoordDirty(st);
			UpdateStationAcceptance(st, false);
			return;
		}
	}
}

void DeleteOilRig(uint tile)
{
	Station *st = DEREF_STATION(_map2[tile]);

	DoClearSquare(tile);

	st->dock_tile = 0;
	st->airport_tile = 0;
	st->facilities &= ~(FACIL_AIRPORT | FACIL_DOCK);
	st->airport_flags = 0;
	UpdateStationVirtCoordDirty(st);
	DeleteStation(st);
}

static void ChangeTileOwner_Station(uint tile, byte old_player, byte new_player)
{
	if (_map_owner[tile] != old_player)
		return;

	if (new_player != 255) {
		Station *st = DEREF_STATION(_map2[tile]);
		_map_owner[tile] = new_player;
		st->owner = new_player;
		_global_station_sort_dirty = true; // transfer ownership of station to another player
		InvalidateWindowClasses(WC_STATION_LIST);
	} else {
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static int32 ClearTile_Station(uint tile, byte flags) {
	byte m5 = _map5[tile];
	Station *st;

	if (flags & DC_AUTO) {
		if (m5 < 8) return_cmd_error(STR_300B_MUST_DEMOLISH_RAILROAD);
		if (m5 < 0x43 || (m5 >= 83 && m5 <= 114)) return_cmd_error(STR_300E_MUST_DEMOLISH_AIRPORT_FIRST);
		if (m5 < 0x47) return_cmd_error(STR_3047_MUST_DEMOLISH_TRUCK_STATION);
		if (m5 < 0x4B) return_cmd_error(STR_3046_MUST_DEMOLISH_BUS_STATION);
		if (m5 == 0x52) return_cmd_error(STR_306A_BUOY_IN_THE_WAY);
		if (m5 != 0x4B && m5 < 0x53) return_cmd_error(STR_304D_MUST_DEMOLISH_DOCK_FIRST);
		SET_DPARAM16(0, STR_4807_OIL_RIG);
		return_cmd_error(STR_4800_IN_THE_WAY);
	}

	st = DEREF_STATION(_map2[tile]);

	if (m5 < 8)
		return RemoveRailroadStation(st, tile, flags);

	// original airports < 67, new airports between 83 - 114
	if (m5 < 0x43 || ( m5 >= 83 && m5 <= 114) )
		return RemoveAirport(st, flags);

	if (m5 < 0x47)
		return RemoveTruckStation(st, flags);

	if (m5 < 0x4B)
		return RemoveBusStation(st, flags);

	if (m5 == 0x52)
		return RemoveBuoy(st, flags);

	if (m5 != 0x4B && m5 < 0x53)
		return RemoveDock(st, flags);

	return CMD_ERROR;

}

void InitializeStations()
{
	int i;
	
	memset(_stations, 0, sizeof(_stations));
	for(i = 0; i != lengthof(_stations); i++)
		_stations[i].index=i;

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


static const byte _station_desc[] = {
	SLE_VAR(Station,xy,							SLE_UINT16),
	SLE_VAR(Station,bus_tile,				SLE_UINT16),
	SLE_VAR(Station,lorry_tile,			SLE_UINT16),
	SLE_VAR(Station,train_tile,			SLE_UINT16),
	SLE_VAR(Station,airport_tile,		SLE_UINT16),
	SLE_VAR(Station,dock_tile,			SLE_UINT16),
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
	SLE_VAR(Station,truck_stop_status,	SLE_UINT8),
	SLE_VAR(Station,bus_stop_status,		SLE_UINT8),

	// blocked_months was stored here in savegame format 0 - 4.0
	SLE_CONDVAR(Station,blocked_months_obsolete,	SLE_UINT8, 0, 4),

	SLE_CONDVAR(Station,airport_flags,			SLE_VAR_U32 | SLE_FILE_U16, 0, 2),
	SLE_CONDVAR(Station,airport_flags,			SLE_UINT32, 3, 255),

	SLE_VAR(Station,last_vehicle,				SLE_UINT16),

	SLE_CONDVAR(Station,class_id,				SLE_UINT8, 3, 255),
	SLE_CONDVAR(Station,stat_id,				SLE_UINT8, 3, 255),
	SLE_CONDVAR(Station,build_date,			SLE_UINT16, 3, 255),

	// reserve extra space in savegame here. (currently 32 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 32, 2, 255),

	SLE_END()
};

static const byte _goods_desc[] = {
	SLE_VAR(GoodsEntry,waiting_acceptance,SLE_UINT16),
	SLE_VAR(GoodsEntry,days_since_pickup,	SLE_UINT8),
	SLE_VAR(GoodsEntry,rating,						SLE_UINT8),
	SLE_VAR(GoodsEntry,enroute_from,			SLE_UINT8),
	SLE_VAR(GoodsEntry,enroute_time,			SLE_UINT8),
	SLE_VAR(GoodsEntry,last_speed,				SLE_UINT8),
	SLE_VAR(GoodsEntry,last_age,					SLE_UINT8),

	SLE_END()
};


static void SaveLoad_STNS(Station *st)
{
	int i;
	SlObject(st, _station_desc);
	for(i=0; i!=NUM_CARGO; i++)
		SlObject(&st->goods[i], _goods_desc);
}

static void Save_STNS()
{
	Station *st;
	// Write the vehicles
	FOR_ALL_STATIONS(st) {
		if (st->xy != 0) {
			SlSetArrayIndex(st->index);
			SlAutolength((AutolengthProc*)SaveLoad_STNS, st);
		}
	}
}

static void Load_STNS()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Station *st = &_stations[index];
		SaveLoad_STNS(st);

		// this means it's an oldstyle savegame without support for nonuniform stations
		if (st->train_tile && st->trainst_h == 0) {
			int w = st->trainst_w >> 4;
			int h = st->trainst_w & 0xF;
			if (_map5[st->train_tile]&1) intswap(w,h);
			st->trainst_w = w;
			st->trainst_h = h;
		}
	}
}

const ChunkHandler _station_chunk_handlers[] = {
	{ 'STNS', Save_STNS, Load_STNS, CH_ARRAY | CH_LAST},
};

