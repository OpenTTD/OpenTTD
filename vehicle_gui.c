/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "player.h"
#include "station.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "vehicle.h"
#include "window.h"
#include "engine.h"
#include "gui.h"
#include "command.h"
#include "gfx.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "viewport.h"
#include "train.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "date.h"
#include "ship.h"
#include "aircraft.h"
#include "roadveh.h"
#include "depot.h"

Sorting _sorting;

static uint32 _internal_name_sorter_id; // internal StringID for default vehicle-names
static const Vehicle* _last_vehicle; // cached vehicle to hopefully speed up name-sorting
static bool   _internal_sort_order;     // descending/ascending

static uint16 _player_num_engines[TOTAL_NUM_ENGINES];
static RailType _railtype_selected_in_replace_gui;


typedef int CDECL VehicleSortListingTypeFunction(const void*, const void*);

static VehicleSortListingTypeFunction VehicleNumberSorter;
static VehicleSortListingTypeFunction VehicleNameSorter;
static VehicleSortListingTypeFunction VehicleAgeSorter;
static VehicleSortListingTypeFunction VehicleProfitThisYearSorter;
static VehicleSortListingTypeFunction VehicleProfitLastYearSorter;
static VehicleSortListingTypeFunction VehicleCargoSorter;
static VehicleSortListingTypeFunction VehicleReliabilitySorter;
static VehicleSortListingTypeFunction VehicleMaxSpeedSorter;
static VehicleSortListingTypeFunction VehicleModelSorter;
static VehicleSortListingTypeFunction VehicleValueSorter;

static VehicleSortListingTypeFunction* const _vehicle_sorter[] = {
	&VehicleNumberSorter,
	&VehicleNameSorter,
	&VehicleAgeSorter,
	&VehicleProfitThisYearSorter,
	&VehicleProfitLastYearSorter,
	&VehicleCargoSorter,
	&VehicleReliabilitySorter,
	&VehicleMaxSpeedSorter,
	&VehicleModelSorter,
	&VehicleValueSorter,
};

static const StringID _vehicle_sort_listing[] = {
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_AGE,
	STR_SORT_BY_PROFIT_THIS_YEAR,
	STR_SORT_BY_PROFIT_LAST_YEAR,
	STR_SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_MODEL,
	STR_SORT_BY_VALUE,
	INVALID_STRING_ID
};

static const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
	STR_ELRAIL_VEHICLES,
	STR_MONORAIL_VEHICLES,
	STR_MAGLEV_VEHICLES,
	INVALID_STRING_ID
};

void RebuildVehicleLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w)
		switch (w->window_class) {
		case WC_TRAINS_LIST: case WC_ROADVEH_LIST:
		case WC_SHIPS_LIST:  case WC_AIRCRAFT_LIST:
			WP(w, vehiclelist_d).flags |= VL_REBUILD;
			SetWindowDirty(w);
			break;
		default: break;
		}
}

void ResortVehicleLists(void)
{
	Window *w;

	for (w = _windows; w != _last_window; ++w)
		switch (w->window_class) {
		case WC_TRAINS_LIST: case WC_ROADVEH_LIST:
		case WC_SHIPS_LIST:  case WC_AIRCRAFT_LIST:
			WP(w, vehiclelist_d).flags |= VL_RESORT;
			SetWindowDirty(w);
			break;
		default: break;
		}
}

static void BuildVehicleList(vehiclelist_d* vl, int type, PlayerID owner, StationID station, OrderID order, uint16 window_type)
{
	const Vehicle** sort_list;
	uint n = 0;
	uint i;

	if (!(vl->flags & VL_REBUILD)) return;

	sort_list = malloc(GetVehicleArraySize() * sizeof(sort_list[0]));
	if (sort_list == NULL) {
		error("Could not allocate memory for the vehicle-sorting-list");
	}

	DEBUG(misc, 1) ("Building vehicle list for player %d station %d...",
		owner, station);

	n = GenerateVehicleSortList(sort_list, type, owner, station, order, window_type);

	free((void*)vl->sort_list);
	vl->sort_list = malloc(n * sizeof(vl->sort_list[0]));
	if (n != 0 && vl->sort_list == NULL) {
		error("Could not allocate memory for the vehicle-sorting-list");
	}
	vl->list_length = n;

	for (i = 0; i < n; ++i) vl->sort_list[i] = sort_list[i];
	free((void*)sort_list);

	vl->flags &= ~VL_REBUILD;
	vl->flags |= VL_RESORT;
}

static void SortVehicleList(vehiclelist_d *vl)
{
	if (!(vl->flags & VL_RESORT)) return;

	_internal_sort_order = vl->flags & VL_DESC;
	_internal_name_sorter_id = STR_SV_TRAIN_NAME;
	_last_vehicle = NULL; // used for "cache" in namesorting
	qsort((void*)vl->sort_list, vl->list_length, sizeof(vl->sort_list[0]),
		_vehicle_sorter[vl->sort_type]);

	vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
	vl->flags &= ~VL_RESORT;
}


/* General Vehicle GUI based procedures that are independent of vehicle types */
void InitializeVehiclesGuiList(void)
{
	_railtype_selected_in_replace_gui = RAILTYPE_RAIL;
}

// draw the vehicle profit button in the vehicle list window.
void DrawVehicleProfitButton(const Vehicle *v, int x, int y)
{
	uint32 ormod;

	// draw profit-based colored icons
	if (v->age <= 365 * 2) {
		ormod = PALETTE_TO_GREY;
	} else if (v->profit_last_year < 0) {
		ormod = PALETTE_TO_RED;
	} else if (v->profit_last_year < 10000) {
		ormod = PALETTE_TO_YELLOW;
	} else {
		ormod = PALETTE_TO_GREEN;
	}
	DrawSprite(SPR_BLOT | ormod, x, y);
}

/** Draw the list of available refit options for a consist.
 * Draw the list and highlight the selected refit option (if any)
 * @param *v first vehicle in consist to get the refit-options of
 * @param sel selected refit cargo-type in the window
 * @return the cargo type that is hightlighted, CT_INVALID if none
 */
CargoID DrawVehicleRefitWindow(const Vehicle *v, int sel)
{
	uint32 cmask = 0;
	CargoID cid, cargo = CT_INVALID;
	int y = 25;
	const Vehicle* u = v;

	/* Check if vehicle has custom refit or normal ones, and get its bitmasked value.
	 * If its a train, 'or' this with the refit masks of the wagons. Now just 'and'
	 * it with the bitmask of available cargo on the current landscape, and
	 * where the bits are set: those are available */
	do {
		cmask |= EngInfo(u->engine_type)->refit_mask;
		u = u->next;
	} while (v->type == VEH_Train && u != NULL);

	/* Check which cargo has been selected from the refit window and draw list */
	for (cid = 0; cmask != 0; cmask >>= 1, cid++) {
		if (HASBIT(cmask, 0)) {
			// vehicle is refittable to this cargo
			byte colour = 16;
			if (sel == 0) {
				cargo = _local_cargo_id_ctype[cid];
				colour = 12;
			}

			sel--;
			DrawString(6, y, _cargoc.names_s[_local_cargo_id_ctype[cid]], colour);
			y += 10;
		}
	}

	return cargo;
}

/* Display additional text from NewGRF in the purchase information window */
int ShowAdditionalText(int x, int y, int w, EngineID engine)
{
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_ADDITIONAL_TEXT, 0, 0, engine, NULL);
	if (callback == CALLBACK_FAILED) return 0;

	DrawStringTruncated(x, y, GetGRFStringID(GetEngineGRFID(engine), 0xD000 + callback), 16, w);
	return 10;
}


// if the sorting criteria had the same value, sort vehicle by unitnumber
#define VEHICLEUNITNUMBERSORTER(r, a, b) {if (r == 0) {r = a->unitnumber - b->unitnumber;}}

static int CDECL VehicleNumberSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

static char _bufcache[64]; // used together with _last_vehicle to hopefully speed up stringsorting
static int CDECL VehicleNameSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	char buf1[64] = "\0";
	int r;

	if (va->string_id != _internal_name_sorter_id) {
		SetDParam(0, va->string_id);
		GetString(buf1, STR_JUST_STRING);
	}

	if (vb != _last_vehicle) {
		_last_vehicle = vb;
		_bufcache[0] = '\0';
		if (vb->string_id != _internal_name_sorter_id) {
			SetDParam(0, vb->string_id);
			GetString(_bufcache, STR_JUST_STRING);
		}
	}

	r =  strcmp(buf1, _bufcache); // sort by name

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleAgeSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->age - vb->age;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitThisYearSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->profit_this_year - vb->profit_this_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->profit_last_year - vb->profit_last_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleCargoSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	const Vehicle* v;
	AcceptedCargo cargoa;
	AcceptedCargo cargob;
	int r = 0;
	int i;

	memset(cargoa, 0, sizeof(cargoa));
	memset(cargob, 0, sizeof(cargob));
	for (v = va; v != NULL; v = v->next) cargoa[v->cargo_type] += v->cargo_cap;
	for (v = vb; v != NULL; v = v->next) cargob[v->cargo_type] += v->cargo_cap;

	for (i = 0; i < NUM_CARGO; i++) {
		r = cargoa[i] - cargob[i];
		if (r != 0) break;
	}

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleReliabilitySorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->reliability - vb->reliability;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleMaxSpeedSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int max_speed_a = 0xFFFF, max_speed_b = 0xFFFF;
	int r;
	const Vehicle *ua = va, *ub = vb;

	if (va->type == VEH_Train && vb->type == VEH_Train) {
		do {
			if (RailVehInfo(ua->engine_type)->max_speed != 0)
				max_speed_a = min(max_speed_a, RailVehInfo(ua->engine_type)->max_speed);
		} while ((ua = ua->next) != NULL);

		do {
			if (RailVehInfo(ub->engine_type)->max_speed != 0)
				max_speed_b = min(max_speed_b, RailVehInfo(ub->engine_type)->max_speed);
		} while ((ub = ub->next) != NULL);

		r = max_speed_a - max_speed_b;
	} else {
		r = va->max_speed - vb->max_speed;
	}

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleModelSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	int r = va->engine_type - vb->engine_type;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleValueSorter(const void *a, const void *b)
{
	const Vehicle* va = *(const Vehicle**)a;
	const Vehicle* vb = *(const Vehicle**)b;
	const Vehicle *u;
	int valuea = 0, valueb = 0;
	int r;

	for (u = va; u != NULL; u = u->next) valuea += u->value;
	for (u = vb; u != NULL; u = u->next) valueb += u->value;

	r = valuea - valueb;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

// this define is to match engine.c, but engine.c keeps it to itself
// ENGINE_AVAILABLE is used in ReplaceVehicleWndProc
#define ENGINE_AVAILABLE ((e->flags & 1 && HASBIT(info->climates, _opt.landscape)) || HASBIT(e->player_avail, _local_player))

/*  if show_outdated is selected, it do not sort psudo engines properly but it draws all engines
 * if used compined with show_cars set to false, it will work as intended. Replace window do it like that
 *  this was a big hack even before show_outdated was added. Stupid newgrf :p
 */
static void train_engine_drawing_loop(int *x, int *y, int *pos, int *sel, EngineID *selected_id, RailType railtype,
	uint8 lines_drawn, bool is_engine, bool show_cars, bool show_outdated, bool show_compatible)
{
	EngineID j;
	byte colour;
	const Player *p = GetPlayer(_local_player);

	for (j = 0; j < NUM_TRAIN_ENGINES; j++) {
		EngineID i = GetRailVehAtPosition(j);
		const Engine *e = GetEngine(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const EngineInfo *info = EngInfo(i);

		if (!EngineHasReplacementForPlayer(p, i) && _player_num_engines[i] == 0 && show_outdated) continue;

		if ((rvi->power == 0 && !show_cars) || (rvi->power != 0 && show_cars))  // show wagons or engines (works since wagons do not have power)
			continue;

		if (*sel == 0) *selected_id = j;


		colour = *sel == 0 ? 0xC : 0x10;
		if (!(ENGINE_AVAILABLE && show_outdated && RailVehInfo(i)->power && IsCompatibleRail(e->railtype, railtype))) {
			if ((!IsCompatibleRail(e->railtype, railtype) && show_compatible)
				|| (e->railtype != railtype && !show_compatible)
				|| !(rvi->flags & RVI_WAGON) != is_engine ||
				!HASBIT(e->player_avail, _local_player))
				continue;
#if 0
		} else {
			// TODO find a nice red colour for vehicles being replaced
			if ( _autoreplace_array[i] != i )
				colour = *sel == 0 ? 0x44 : 0x45;
#endif
		}

		if (IS_INT_INSIDE(--*pos, -lines_drawn, 0)) {
			DrawString(*x + 59, *y + 2, GetCustomEngineName(i),
				colour);
			// show_outdated is true only for left side, which is where we show old replacements
			DrawTrainEngine(*x + 29, *y + 6, i, (_player_num_engines[i] == 0 && show_outdated) ?
				PALETTE_CRASH : GetEnginePalette(i, _local_player));
			if ( show_outdated ) {
				SetDParam(0, _player_num_engines[i]);
				DrawStringRightAligned(213, *y+5, STR_TINY_BLACK, 0);
			}
			*y += 14;
		}
		--*sel;
	}
}


static void SetupScrollStuffForReplaceWindow(Window *w)
{
	EngineID selected_id[2] = { INVALID_ENGINE, INVALID_ENGINE };
	const Player* p = GetPlayer(_local_player);
	uint sel[2];
	uint count = 0;
	uint count2 = 0;
	EngineID i;

	sel[0] = WP(w,replaceveh_d).sel_index[0];
	sel[1] = WP(w,replaceveh_d).sel_index[1];

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			RailType railtype = _railtype_selected_in_replace_gui;

			w->widget[13].color = _player_colors[_local_player]; // sets the colour of that art thing
			w->widget[16].color = _player_colors[_local_player]; // sets the colour of that art thing

			for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
				EngineID eid = GetRailVehAtPosition(i);
				const Engine* e = GetEngine(eid);
				const EngineInfo* info = EngInfo(eid);

				// left window contains compatible engines while right window only contains engines of the selected type
				if (ENGINE_AVAILABLE &&
						(RailVehInfo(eid)->power != 0) == (WP(w, replaceveh_d).wagon_btnstate != 0)) {
					if (IsCompatibleRail(e->railtype, railtype) && (_player_num_engines[eid] > 0 || EngineHasReplacementForPlayer(p, eid))) {
						if (sel[0] == count) selected_id[0] = eid;
						count++;
					}
					if (e->railtype == railtype && HASBIT(e->player_avail, _local_player)) {
						if (sel[1] == count2) selected_id[1] = eid;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Road: {
			for (i = ROAD_ENGINES_INDEX; i < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; i++) {
				if (_player_num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) { // only draw right array if we have anything in the left one
				CargoID cargo = RoadVehInfo(selected_id[0])->cargo_type;

				for (i = ROAD_ENGINES_INDEX; i < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; i++) {
					if (cargo == RoadVehInfo(i)->cargo_type &&
							HASBIT(GetEngine(i)->player_avail, _local_player)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Ship: {
			for (i = SHIP_ENGINES_INDEX; i < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; i++) {
				if (_player_num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) {
				const ShipVehicleInfo* svi = ShipVehInfo(selected_id[0]);
				CargoID cargo = svi->cargo_type;
				byte refittable = svi->refittable;

				for (i = SHIP_ENGINES_INDEX; i < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; i++) {
					if (HASBIT(GetEngine(i)->player_avail, _local_player) && (
								ShipVehInfo(i)->cargo_type == cargo ||
								ShipVehInfo(i)->refittable & refittable
							)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}

		case VEH_Aircraft: {
			for (i = AIRCRAFT_ENGINES_INDEX; i < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; i++) {
				if (_player_num_engines[i] > 0 || EngineHasReplacementForPlayer(p, i)) {
					if (sel[0] == count) selected_id[0] = i;
					count++;
				}
			}

			if (selected_id[0] != INVALID_ENGINE) {
				byte subtype = AircraftVehInfo(selected_id[0])->subtype;

				for (i = AIRCRAFT_ENGINES_INDEX; i < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; i++) {
					if (HASBIT(GetEngine(i)->player_avail, _local_player) &&
							(subtype & AIR_CTOL) == (AircraftVehInfo(i)->subtype & AIR_CTOL)) {
						if (sel[1] == count2) selected_id[1] = i;
						count2++;
					}
				}
			}
			break;
		}
	}
	// sets up the number of items in each list
	SetVScrollCount(w, count);
	SetVScroll2Count(w, count2);
	WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
	WP(w,replaceveh_d).sel_engine[1] = selected_id[1];

	WP(w,replaceveh_d).count[0] = count;
	WP(w,replaceveh_d).count[1] = count2;
	return;
}


static void DrawEngineArrayInReplaceWindow(Window *w, int x, int y, int x2, int y2, int pos, int pos2,
	int sel1, int sel2, EngineID selected_id1, EngineID selected_id2)
{
	int sel[2];
	EngineID selected_id[2];
	const Player *p = GetPlayer(_local_player);

	sel[0] = sel1;
	sel[1] = sel2;

	selected_id[0] = selected_id1;
	selected_id[1] = selected_id2;

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			RailType railtype = _railtype_selected_in_replace_gui;
			DrawString(157, 99 + (14 * w->vscroll.cap), _rail_types_list[railtype], 0x10);
			/* draw sorting criteria string */

			/* Ensure that custom engines which substituted wagons
			* are sorted correctly.
			* XXX - DO NOT EVER DO THIS EVER AGAIN! GRRR hacking in wagons as
			* engines to get more types.. Stays here until we have our own format
			* then it is exit!!! */
			if (WP(w,replaceveh_d).wagon_btnstate) {
				train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, true, false, true, true); // True engines
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, true, false, false, false); // True engines
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, false, false, false); // Feeble wagons
			} else {
				train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, false, true, true, true);
				train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, true, false, true);
			}
			break;
		}

		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			const Engine* e = GetEngine(ROAD_ENGINES_INDEX);
			EngineID engine_id = ROAD_ENGINES_INDEX;
			byte cargo;

			if (selected_id[0] >= ROAD_ENGINES_INDEX && selected_id[0] < SHIP_ENGINES_INDEX) {
				cargo = RoadVehInfo(selected_id[0])->cargo_type;

				do {
					if (_player_num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+59, y+2, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x+29, y+6, engine_id, _player_num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
							SetDParam(0, _player_num_engines[engine_id]);
							DrawStringRightAligned(213, y+5, STR_TINY_BLACK, 0);
							y += 14;
						}
					sel[0]--;
					}

					if (RoadVehInfo(engine_id)->cargo_type == cargo && HASBIT(e->player_avail, _local_player)) {
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0) && RoadVehInfo(engine_id)->cargo_type == cargo) {
							DrawString(x2+59, y2+2, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x2+29, y2+6, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 14;
						}
						sel[1]--;
					}
				} while (++engine_id, ++e,--num);
			}
			break;
		}

		case VEH_Ship: {
			int num = NUM_SHIP_ENGINES;
			const Engine* e = GetEngine(SHIP_ENGINES_INDEX);
			EngineID engine_id = SHIP_ENGINES_INDEX;
			byte cargo, refittable;

			if (selected_id[0] != INVALID_ENGINE) {
				cargo = ShipVehInfo(selected_id[0])->cargo_type;
				refittable = ShipVehInfo(selected_id[0])->refittable;

				do {
					if (_player_num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawShipEngine(x+35, y+10, engine_id, _player_num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
							SetDParam(0, _player_num_engines[engine_id]);
							DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
							y += 24;
						}
						sel[0]--;
					}
					if (selected_id[0] != INVALID_ENGINE) {
						if (HASBIT(e->player_avail, _local_player) && ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {
							if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
								DrawString(x2+75, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
								DrawShipEngine(x2+35, y2+10, engine_id, GetEnginePalette(engine_id, _local_player));
								y2 += 24;
							}
							sel[1]--;
						}
					}
				} while (++engine_id, ++e,--num);
			}
			break;
		}   //end of ship

		case VEH_Aircraft: {
			if (selected_id[0] != INVALID_ENGINE) {
				int num = NUM_AIRCRAFT_ENGINES;
				const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);
				EngineID engine_id = AIRCRAFT_ENGINES_INDEX;
				byte subtype = AircraftVehInfo(selected_id[0])->subtype;

				do {
					if (_player_num_engines[engine_id] > 0 || EngineHasReplacementForPlayer(p, engine_id)) {
						if (sel[0] == 0) selected_id[0] = engine_id;
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x+29, y+10, engine_id, _player_num_engines[engine_id] > 0 ? GetEnginePalette(engine_id, _local_player) : PALETTE_CRASH);
							SetDParam(0, _player_num_engines[engine_id]);
							DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
							y += 24;
						}
						sel[0]--;
					}
					if ((subtype & AIR_CTOL) == (AircraftVehInfo(engine_id)->subtype & AIR_CTOL) &&
							HASBIT(e->player_avail, _local_player)) {
						if (sel[1] == 0) selected_id[1] = engine_id;
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
							DrawString(x2+62, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x2+29, y2+10, engine_id, GetEnginePalette(engine_id, _local_player));
							y2 += 24;
						}
						sel[1]--;
					}
				} while (++engine_id, ++e,--num);
			}
			break;
		}   // end of aircraft
	}
}


static void ReplaceVehicleWndProc(Window *w, WindowEvent *e)
{
	static const StringID _vehicle_type_names[] = {
		STR_019F_TRAIN,
		STR_019C_ROAD_VEHICLE,
		STR_019E_SHIP,
		STR_019D_AIRCRAFT
	};

	switch (e->event) {
		case WE_PAINT: {
				const Player *p = GetPlayer(_local_player);
				int pos = w->vscroll.pos;
				EngineID selected_id[2] = { INVALID_ENGINE, INVALID_ENGINE };
				int x = 1;
				int y = 15;
				int pos2 = w->vscroll2.pos;
				int x2 = 1 + 228;
				int y2 = 15;
				int sel[2];
				sel[0] = WP(w,replaceveh_d).sel_index[0];
				sel[1] = WP(w,replaceveh_d).sel_index[1];

				{
					uint i;
					const Vehicle *vehicle;

					for (i = 0; i < lengthof(_player_num_engines); i++) {
						_player_num_engines[i] = 0;
					}
					FOR_ALL_VEHICLES(vehicle) {
						if (vehicle->owner == _local_player) {
							if (vehicle->type == VEH_Aircraft && vehicle->subtype > 2) continue;

							// do not count the vehicles, that contains only 0 in all var
							if (vehicle->engine_type == 0 && vehicle->spritenum == 0) continue;

							if (vehicle->type != GetEngine(vehicle->engine_type)->type) continue;

							_player_num_engines[vehicle->engine_type]++;
						}
					}
				}

				SetupScrollStuffForReplaceWindow(w);

				selected_id[0] = WP(w,replaceveh_d).sel_engine[0];
				selected_id[1] = WP(w,replaceveh_d).sel_engine[1];

			// sets the selected left item to the top one if it's greater than the number of vehicles in the left side

				if (WP(w,replaceveh_d).count[0] <= sel[0]) {
					if (WP(w,replaceveh_d).count[0]) {
						sel[0] = 0;
						WP(w,replaceveh_d).sel_index[0] = 0;
						w->vscroll.pos = 0;
						// now we go back to set selected_id[1] properly
						SetWindowDirty(w);
						return;
					} else { //there are no vehicles in the left window
						selected_id[1] = INVALID_ENGINE;
					}
				}

				if (WP(w,replaceveh_d).count[1] <= sel[1]) {
					if (WP(w,replaceveh_d).count[1]) {
						sel[1] = 0;
						WP(w,replaceveh_d).sel_index[1] = 0;
						w->vscroll2.pos = 0;
						// now we go back to set selected_id[1] properly
						SetWindowDirty(w);
						return;
					} else { //there are no vehicles in the right window
						selected_id[1] = INVALID_ENGINE;
					}
				}

				// Disable the "Start Replacing" button if:
				//    Either list is empty
				// or Both lists have the same vehicle selected
				// or The selected replacement engine has a replacement (to prevent loops)
				// or The right list (new replacement) has the existing replacement vehicle selected
				if (selected_id[0] == INVALID_ENGINE ||
						selected_id[1] == INVALID_ENGINE ||
						selected_id[0] == selected_id[1] ||
						EngineReplacementForPlayer(p, selected_id[1]) != INVALID_ENGINE ||
						EngineReplacementForPlayer(p, selected_id[0]) == selected_id[1]) {
					SETBIT(w->disabled_state, 4);
				} else {
					CLRBIT(w->disabled_state, 4);
				}

				// Disable the "Stop Replacing" button if:
				//    The left list (existing vehicle) is empty
				// or The selected vehicle has no replacement set up
				if (selected_id[0] == INVALID_ENGINE ||
						!EngineHasReplacementForPlayer(p, selected_id[0])) {
					SETBIT(w->disabled_state, 6);
				} else {
					CLRBIT(w->disabled_state, 6);
				}

				// now the actual drawing of the window itself takes place
				SetDParam(0, _vehicle_type_names[WP(w, replaceveh_d).vehicletype - VEH_Train]);

				if (WP(w, replaceveh_d).vehicletype == VEH_Train) {
					// set on/off for renew_keep_length
					SetDParam(1, p->renew_keep_length ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);

					// set wagon/engine button
					SetDParam(2, WP(w, replaceveh_d).wagon_btnstate ? STR_ENGINES : STR_WAGONS);
				}

				DrawWindowWidgets(w);

				// sets up the string for the vehicle that is being replaced to
				if (selected_id[0] != INVALID_ENGINE) {
					if (!EngineHasReplacementForPlayer(p, selected_id[0])) {
						SetDParam(0, STR_NOT_REPLACING);
					} else {
						SetDParam(0, GetCustomEngineName(EngineReplacementForPlayer(p, selected_id[0])));
					}
				} else {
					SetDParam(0, STR_NOT_REPLACING_VEHICLE_SELECTED);
				}

				DrawString(145, 87 + w->resize.step_height * w->vscroll.cap, STR_02BD, 0x10);

				/* now we draw the two arrays according to what we just counted */
				DrawEngineArrayInReplaceWindow(w, x, y, x2, y2, pos, pos2, sel[0], sel[1], selected_id[0], selected_id[1]);

				WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
				WP(w,replaceveh_d).sel_engine[1] = selected_id[1];
				/* now we draw the info about the vehicles we selected */
				switch (WP(w,replaceveh_d).vehicletype) {
					case VEH_Train: {
						byte i = 0;
						int offset = 0;

						for (i = 0 ; i < 2 ; i++) {
							if (i > 0) offset = 228;
							if (selected_id[i] != INVALID_ENGINE) {
								if (WP(w, replaceveh_d).wagon_btnstate) {
									/* it's an engine */
									DrawTrainEnginePurchaseInfo(2 + offset, 15 + (14 * w->vscroll.cap), selected_id[i]);
								} else {
									/* it's a wagon. Train cars are not replaced with the current GUI, but this code is ready for newgrf if anybody adds that*/
									DrawTrainWagonPurchaseInfo(2 + offset, 15 + (14 * w->vscroll.cap), selected_id[i]);
								}
							}
						}
						break;
					}   //end if case  VEH_Train

					case VEH_Road: {
						if (selected_id[0] != INVALID_ENGINE) {
							DrawRoadVehPurchaseInfo(2, 15 + (14 * w->vscroll.cap), selected_id[0]);
							if (selected_id[1] != INVALID_ENGINE) {
								DrawRoadVehPurchaseInfo(2 + 228, 15 + (14 * w->vscroll.cap), selected_id[1]);
							}
						}
						break;
					}   // end of VEH_Road

					case VEH_Ship: {
						if (selected_id[0] != INVALID_ENGINE) {
							DrawShipPurchaseInfo(2, 15 + (24 * w->vscroll.cap), selected_id[0]);
							if (selected_id[1] != INVALID_ENGINE) {
								DrawShipPurchaseInfo(2 + 228, 15 + (24 * w->vscroll.cap), selected_id[1]);
							}
						}
						break;
					}   // end of VEH_Ship

					case VEH_Aircraft: {
						if (selected_id[0] != INVALID_ENGINE) {
							DrawAircraftPurchaseInfo(2, 15 + (24 * w->vscroll.cap), selected_id[0]);
							if (selected_id[1] != INVALID_ENGINE) {
								DrawAircraftPurchaseInfo(2 + 228, 15 + (24 * w->vscroll.cap), selected_id[1]);
							}
						}
						break;
					}   // end of VEH_Aircraft
				}
			} break;   // end of paint

		case WE_CLICK: {
			// these 3 variables is used if any of the lists is clicked
			uint16 click_scroll_pos = w->vscroll2.pos;
			uint16 click_scroll_cap = w->vscroll2.cap;
			byte click_side = 1;

			switch (e->click.widget) {
				case 12:
					WP(w, replaceveh_d).wagon_btnstate = !(WP(w, replaceveh_d).wagon_btnstate);
					SetWindowDirty(w);
					break;

				case 14:
				case 15: /* Railtype selection dropdown menu */
					ShowDropDownMenu(w, _rail_types_list, _railtype_selected_in_replace_gui, 15, 0, ~GetPlayer(_local_player)->avail_railtypes);
					break;

				case 17: /* toggle renew_keep_length */
					DoCommandP(0, 5, GetPlayer(_local_player)->renew_keep_length ? 0 : 1, NULL, CMD_REPLACE_VEHICLE);
					break;

				case 4: { /* Start replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					EngineID veh_to = WP(w, replaceveh_d).sel_engine[1];
					DoCommandP(0, 3, veh_from + (veh_to << 16), NULL, CMD_REPLACE_VEHICLE);
					break;
				}

				case 6: { /* Stop replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					DoCommandP(0, 3, veh_from + (INVALID_ENGINE << 16), NULL, CMD_REPLACE_VEHICLE);
					break;
				}

				case 7:
					// sets up that the left one was clicked. The default values are for the right one (9)
					// this way, the code for 9 handles both sides
					click_scroll_pos = w->vscroll.pos;
					click_scroll_cap = w->vscroll.cap;
					click_side = 0;
					/* FALL THROUGH */

				case 9: {
					uint i = (e->click.pt.y - 14) / w->resize.step_height;
					if (i < click_scroll_cap) {
						WP(w,replaceveh_d).sel_index[click_side] = i + click_scroll_pos;
						SetWindowDirty(w);
					}
					break;
				}
			}
			break;
		}

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			_railtype_selected_in_replace_gui = e->dropdown.index;
			/* Reset scrollbar positions */
			w->vscroll.pos  = 0;
			w->vscroll2.pos = 0;
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			w->vscroll.cap  += e->sizing.diff.y / (int)w->resize.step_height;
			w->vscroll2.cap += e->sizing.diff.y / (int)w->resize.step_height;

			w->widget[7].unkA = (w->vscroll.cap  << 8) + 1;
			w->widget[9].unkA = (w->vscroll2.cap << 8) + 1;
			break;
	}
}

static const Widget _replace_rail_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,       STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   197, STR_NULL,       STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   210,   221, STR_REPLACE_VEHICLES_START, STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   316,   198,   209, STR_NULL,       STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   210,   221, STR_REPLACE_VEHICLES_STOP,  STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,          STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,          STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   197, STR_NULL,       STR_NULL},
// train specific stuff
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   198,   209, STR_REPLACE_ENGINE_WAGON_SELECT,       STR_REPLACE_ENGINE_WAGON_SELECT_HELP},  // widget 12
{      WWT_PANEL,     RESIZE_TB,    14,   139,   153,   210,   221, STR_NULL,       STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,   154,   277,   210,   221, STR_NULL,       STR_REPLACE_HELP_RAILTYPE},
{    WWT_TEXTBTN,     RESIZE_TB,    14,   278,   289,   210,   221, STR_0225,       STR_REPLACE_HELP_RAILTYPE},
{      WWT_PANEL,     RESIZE_TB,    14,   290,   305,   210,   221, STR_NULL,       STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   317,   455,   198,   209, STR_REPLACE_REMOVE_WAGON,       STR_REPLACE_REMOVE_WAGON_HELP},
// end of train specific stuff
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   210,   221, STR_NULL,       STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_road_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,                    STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   197, STR_NULL,                    STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   198,   209, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   198,   209, STR_NULL,                    STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   198,   209, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,                       STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,                       STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   197, STR_NULL,                    STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   198,   209, STR_NULL,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_ship_aircraft_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,                    STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   110,   181, STR_NULL,                    STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   182,   193, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   182,   193, STR_NULL,                    STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   182,   193, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   109, 0x401,                       STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   109, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   109, 0x401,                       STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   109, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   110,   181, STR_NULL,                    STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   182,   193, STR_NULL,                    STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _replace_rail_vehicle_desc = {
	-1, -1, 456, 222,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_rail_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_road_vehicle_desc = {
	-1, -1, 456, 210,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_road_vehicle_widgets,
	ReplaceVehicleWndProc
};

static const WindowDesc _replace_ship_aircraft_vehicle_desc = {
	-1, -1, 456, 194,
	WC_REPLACE_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_replace_ship_aircraft_vehicle_widgets,
	ReplaceVehicleWndProc
};


void ShowReplaceVehicleWindow(byte vehicletype)
{
	Window *w;

	DeleteWindowById(WC_REPLACE_VEHICLE, vehicletype);

	switch (vehicletype) {
		case VEH_Train:
			w = AllocateWindowDescFront(&_replace_rail_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			WP(w, replaceveh_d).wagon_btnstate = true;
			break;
		case VEH_Road:
			w = AllocateWindowDescFront(&_replace_road_vehicle_desc, vehicletype);
			w->vscroll.cap  = 8;
			w->resize.step_height = 14;
			break;
		case VEH_Ship:
		case VEH_Aircraft:
			w = AllocateWindowDescFront(&_replace_ship_aircraft_vehicle_desc, vehicletype);
			w->vscroll.cap  = 4;
			w->resize.step_height = 24;
			break;
		default: return;
	}
	w->caption_color = _local_player;
	WP(w,replaceveh_d).vehicletype = vehicletype;
	w->vscroll2.cap = w->vscroll.cap;   // these two are always the same
}

void InitializeGUI(void)
{
	memset(&_sorting, 0, sizeof(_sorting));
}

/** Assigns an already open vehicle window to a new vehicle.
 * Assigns an already open vehicle window to a new vehicle. If the vehicle got
 * any sub window open (orders and so on) it will change owner too.
 * @param *from_v the current owner of the window
 * @param *to_v the new owner of the window
 */
void ChangeVehicleViewWindow(const Vehicle *from_v, const Vehicle *to_v)
{
	Window *w;

	w = FindWindowById(WC_VEHICLE_VIEW, from_v->index);
	if (w != NULL) {
		w->window_number = to_v->index;
		WP(w, vp_d).follow_vehicle = to_v->index;
		SetWindowDirty(w);

		w = FindWindowById(WC_VEHICLE_ORDERS, from_v->index);
		if (w != NULL) {
			w->window_number = to_v->index;
			SetWindowDirty(w);
		}

		w = FindWindowById(WC_VEHICLE_REFIT, from_v->index);
		if (w != NULL) {
			w->window_number = to_v->index;
			SetWindowDirty(w);
		}

		w = FindWindowById(WC_VEHICLE_DETAILS, from_v->index);
		if (w != NULL) {
			w->window_number = to_v->index;
			SetWindowDirty(w);
		}
	}
}

/*
 * bitmask for w->window_number
 * 0-7 PlayerID (owner)
 * 8-10 window type (use flags in vehicle_gui.h)
 * 11-15 vehicle type (using VEH_, but can be compressed to fewer bytes if needed)
 * 16-31 StationID or OrderID depending on window type (bit 8-10)
 **/

void PlayerVehWndProc(Window *w, WindowEvent *e)
{
	vehiclelist_d *vl = &WP(w, vehiclelist_d);
	const byte vehicle_type = GB(w->window_number, 11, 5);

	switch (e->event) {
		case WE_PAINT: {
			int x = 2;
			int y = PLY_WND_PRC__OFFSET_TOP_WIDGET;
			int max;
			int i;
			const PlayerID owner = GB(w->window_number, 0, 8);
			const Player *p = GetPlayer(owner);
			const uint16 window_type = w->window_number & VLW_FLAGS;
			const StationID station = (window_type == VLW_STATION_LIST)  ? GB(w->window_number, 16, 16) : INVALID_STATION;
			const OrderID order     = (window_type == VLW_SHARED_ORDERS) ? GB(w->window_number, 16, 16) : INVALID_ORDER;

			BuildVehicleList(vl, vehicle_type, owner, station, order, window_type);
			SortVehicleList(vl);
			SetVScrollCount(w, vl->list_length);

			/* draw the widgets */
			switch (window_type) {
				case VLW_SHARED_ORDERS:
					/* Shared Orders */
					if (vl->list_length == 0) {
						/* The list is empty, so the last vehicle is sold or crashed */
						/* Delete the window because the order is now not in use anymore */
						DeleteWindow(w);
						return;
					}
					SetDParam(0, w->vscroll.count);
					w->widget[1].unkA  = STR_VEH_WITH_SHARED_ORDERS_LIST;

					if (owner != _local_player) break; // only set up buttons for local player
					w->widget[10].unkA = STR_EMPTY;
					SETBIT(w->disabled_state, 10);
					break;

				case VLW_STANDARD:
					/* Company Name */
					SetDParam(0, p->name_1);
					SetDParam(1, p->name_2);
					SetDParam(2, w->vscroll.count);

					switch (vehicle_type) {
						case VEH_Train:    w->widget[1].unkA = STR_881B_TRAINS;        break;
						case VEH_Road:     w->widget[1].unkA = STR_9001_ROAD_VEHICLES; break;
						case VEH_Ship:     w->widget[1].unkA = STR_9805_SHIPS;         break;
						case VEH_Aircraft: w->widget[1].unkA = STR_A009_AIRCRAFT;      break;
						default: NOT_REACHED(); break;
					}

					if (owner != _local_player) break; // only set up buttons for local player
					if (vl->list_length == 0) SETBIT(w->disabled_state, 9);
					break;

				case VLW_STATION_LIST:
					/* Station Name */
					SetDParam(0, station);
					SetDParam(1, w->vscroll.count);
					switch (vehicle_type) {
						case VEH_Train:    w->widget[1].unkA = STR_SCHEDULED_TRAINS;        break;
						case VEH_Road:     w->widget[1].unkA = STR_SCHEDULED_ROAD_VEHICLES; break;
						case VEH_Ship:     w->widget[1].unkA = STR_SCHEDULED_SHIPS;         break;
						case VEH_Aircraft: w->widget[1].unkA = STR_SCHEDULED_AIRCRAFT;      break;
						default: NOT_REACHED(); break;
					}
					if (owner != _local_player) break; // only set up buttons for local player
					if (vl->list_length == 0) SETBIT(w->disabled_state, 9);
					break;

				default: NOT_REACHED(); break;
			}
			DrawWindowWidgets(w);

			/* draw sorting criteria string */
			DrawString(85, 15, _vehicle_sort_listing[vl->sort_type], 0x10);
			/* draw arrow pointing up/down for ascending/descending sorting */
			DoDrawString(vl->flags & VL_DESC ? DOWNARROW : UPARROW, 69, 15, 0x10);

			max = min(w->vscroll.pos + w->vscroll.cap, vl->list_length);
			for (i = w->vscroll.pos; i < max; ++i) {
				const Vehicle* v = vl->sort_list[i];
				StringID str;

				str = v->age > v->max_age - 366 ? STR_00E3 : STR_00E2;
				SetDParam(0, v->profit_this_year);
				SetDParam(1, v->profit_last_year);
				switch (vehicle_type) {
					case VEH_Train:
						DrawTrainImage(v, x + 21, y + 6, w->hscroll.cap, 0, INVALID_VEHICLE);
						DrawString(x + 21, y + 18, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);
						if (IsTileDepotType(v->tile, TRANSPORT_RAIL) && (v->vehstatus & VS_HIDDEN)) str = STR_021F;
						if (v->string_id != STR_SV_TRAIN_NAME) {
							SetDParam(0, v->string_id);
							DrawString(x + 21, y, STR_01AB, 0);
						}
						break;
					case VEH_Road:
						DrawRoadVehImage(v, x + 22, y + 6, INVALID_VEHICLE);
						DrawString(x + 24, y + 18, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);
						if (IsRoadVehInDepot(v)) str = STR_021F;
						if (v->string_id != STR_SV_ROADVEH_NAME) {
							SetDParam(0, v->string_id);
							DrawString(x + 24, y, STR_01AB, 0);
						}
						break;
					case VEH_Ship:
						DrawShipImage(v, x + 19, y + 6, INVALID_VEHICLE);
						DrawString(x + 12, y + 28, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);
						if (IsShipInDepot(v)) str = STR_021F;
						if (v->string_id != STR_SV_SHIP_NAME) {
							SetDParam(0, v->string_id);
							DrawString(x + 12, y, STR_01AB, 0);
						}
						DrawSmallOrderListShip(v, x + 138, y);
						break;
					case VEH_Aircraft:
						DrawAircraftImage(v, x + 19, y + 6, INVALID_VEHICLE);
						DrawString(x + 19, y + 28, STR_0198_PROFIT_THIS_YEAR_LAST_YEAR, 0);
						if (IsAircraftInHangar(v)) str = STR_021F;
						if (v->string_id != STR_SV_AIRCRAFT_NAME) {
							SetDParam(0, v->string_id);
							DrawString(x + 19, y, STR_01AB, 0);
						}
						DrawSmallOrderListAircraft(v, x + 136, y);
						break;
					default: NOT_REACHED(); break;
				}
				SetDParam(0, v->unitnumber);
				DrawString(x, y + 2, str, 0);

				DrawVehicleProfitButton(v, x, y + 13);

				y += w->resize.step_height;
			}
		}	break;

		case WE_CLICK: {
			switch (e->click.widget) {
				case 3: /* Flip sorting method ascending/descending */
					vl->flags ^= VL_DESC;
					vl->flags |= VL_RESORT;

					switch (vehicle_type) {
						case VEH_Train:    _sorting.train.order    = !!(vl->flags & VL_DESC); break;
						case VEH_Road:     _sorting.roadveh.order  = !!(vl->flags & VL_DESC); break;
						case VEH_Ship:     _sorting.ship.order     = !!(vl->flags & VL_DESC); break;
						case VEH_Aircraft: _sorting.aircraft.order = !!(vl->flags & VL_DESC); break;
						default: NOT_REACHED(); break;
					}
					SetWindowDirty(w);
					break;
				case 4: case 5:/* Select sorting criteria dropdown menu */
					ShowDropDownMenu(w, _vehicle_sort_listing, vl->sort_type, 5, 0, 0);
					return;
				case 7: { /* Matrix to show vehicles */
					uint32 id_v = (e->click.pt.y - PLY_WND_PRC__OFFSET_TOP_WIDGET) / w->resize.step_height;
					const Vehicle *v;

					if (id_v >= w->vscroll.cap) return; // click out of bounds

					id_v += w->vscroll.pos;

					if (id_v >= vl->list_length) return; // click out of list bound

					v = vl->sort_list[id_v];

					switch (vehicle_type) {
						case VEH_Train: ShowTrainViewWindow(v); break;
						case VEH_Road: ShowRoadVehViewWindow(v); break;
						case VEH_Ship: ShowShipViewWindow(v); break;
						case VEH_Aircraft: ShowAircraftViewWindow(v); break;
						default: NOT_REACHED(); break;
					}
				} break;

				case 9: // Left button
					if (GB(w->window_number, 0, 8) /* OwnerID */ != _local_player) break;
					assert(vl->list_length != 0);
					DoCommandP(0, GB(w->window_number, 16, 16) /* StationID or OrderID (depending on VLW). Nomatter which one it is, it's needed here */,
						(w->window_number & VLW_FLAGS) | DEPOT_MASS_SEND | (_ctrl_pressed ? DEPOT_SERVICE : 0), NULL, CMD_SEND_TO_DEPOT(vehicle_type));
					break;

				case 10: // Right button
					ShowReplaceVehicleWindow(vehicle_type);
					break;
			}
		}	break;

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			if (vl->sort_type != e->dropdown.index) {
				// value has changed -> resort
				vl->flags |= VL_RESORT;
				vl->sort_type = e->dropdown.index;
				switch (vehicle_type) {
					case VEH_Train:    _sorting.train.criteria    = vl->sort_type; break;
					case VEH_Road:     _sorting.roadveh.criteria  = vl->sort_type; break;
					case VEH_Ship:     _sorting.ship.criteria     = vl->sort_type; break;
					case VEH_Aircraft: _sorting.aircraft.criteria = vl->sort_type; break;
					default: NOT_REACHED(); break;
				}
			}
			SetWindowDirty(w);
			break;

		case WE_CREATE:
			/* IMPORTANT: order and criteria needs to be set after the window is created, but before it's used */
			/* w->window_number is not yet set at this point, so we lack info on what kind of vehicle we are dealing with */
			vl->sort_list = NULL;
			vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;	// Set up resort timer
			break;

		case WE_DESTROY:
			free((void*)vl->sort_list);
			break;

		case WE_TICK: /* resort the list every 20 seconds orso (10 days) */
			if (--vl->resort_timer == 0) {
				StationID station = ((w->window_number & VLW_FLAGS) == VLW_STATION_LIST) ? GB(w->window_number, 16, 16) : INVALID_STATION;
				PlayerID owner = GB(w->window_number, 0, 8);
				DEBUG(misc, 1) ("Periodic resort %d list player %d station %d", vehicle_type, owner, station);
				vl->resort_timer = DAY_TICKS * PERIODIC_RESORT_DAYS;
				vl->flags |= VL_RESORT;
				SetWindowDirty(w);
			}
			break;

		case WE_RESIZE:
			/* Update the scroll + matrix */
			if (vehicle_type == VEH_Train) w->hscroll.cap += e->sizing.diff.x;
			w->vscroll.cap += e->sizing.diff.y / (int)w->resize.step_height;
			w->widget[7].unkA = (w->vscroll.cap << 8) + 1;
			break;
	}
}
