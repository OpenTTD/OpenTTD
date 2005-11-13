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

Sorting _sorting;

static uint32 _internal_name_sorter_id; // internal StringID for default vehicle-names
static uint32 _last_vehicle_idx;        // cached index to hopefully speed up name-sorting
static bool   _internal_sort_order;     // descending/ascending

static uint16 _player_num_engines[TOTAL_NUM_ENGINES];
static RailType _railtype_selected_in_replace_gui;


typedef int CDECL VehicleSortListingTypeFunction(const void*, const void*);

static VehicleSortListingTypeFunction VehicleUnsortedSorter;
static VehicleSortListingTypeFunction VehicleNumberSorter;
static VehicleSortListingTypeFunction VehicleNameSorter;
static VehicleSortListingTypeFunction VehicleAgeSorter;
static VehicleSortListingTypeFunction VehicleProfitThisYearSorter;
static VehicleSortListingTypeFunction VehicleProfitLastYearSorter;
static VehicleSortListingTypeFunction VehicleCargoSorter;
static VehicleSortListingTypeFunction VehicleReliabilitySorter;
static VehicleSortListingTypeFunction VehicleMaxSpeedSorter;

static VehicleSortListingTypeFunction* const _vehicle_sorter[] = {
	&VehicleUnsortedSorter,
	&VehicleNumberSorter,
	&VehicleNameSorter,
	&VehicleAgeSorter,
	&VehicleProfitThisYearSorter,
	&VehicleProfitLastYearSorter,
	&VehicleCargoSorter,
	&VehicleReliabilitySorter,
	&VehicleMaxSpeedSorter
};

const StringID _vehicle_sort_listing[] = {
	STR_SORT_BY_UNSORTED,
	STR_SORT_BY_NUMBER,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_AGE,
	STR_SORT_BY_PROFIT_THIS_YEAR,
	STR_SORT_BY_PROFIT_LAST_YEAR,
	STR_SORT_BY_TOTAL_CAPACITY_PER_CARGOTYPE,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_MAX_SPEED,
	INVALID_STRING_ID
};

static const StringID _rail_types_list[] = {
	STR_RAIL_VEHICLES,
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

void BuildVehicleList(vehiclelist_d* vl, int type, PlayerID owner, StationID station)
{
	int subtype = (type != VEH_Aircraft) ? TS_Front_Engine : 2;
	int n = 0;
	int i;

	if (!(vl->flags & VL_REBUILD)) return;

	/* Create array for sorting */
	_vehicle_sort = realloc(_vehicle_sort, GetVehiclePoolSize() * sizeof(_vehicle_sort[0]));
	if (_vehicle_sort == NULL)
		error("Could not allocate memory for the vehicle-sorting-list");

	DEBUG(misc, 1) ("Building vehicle list for player %d station %d...",
		owner, station);

	if (station != INVALID_STATION) {
		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->subtype <= subtype) {
				const Order *order;

				FOR_VEHICLE_ORDERS(v, order) {
					if (order->type == OT_GOTO_STATION && order->station == station) {
						_vehicle_sort[n].index = v->index;
						_vehicle_sort[n].owner = v->owner;
						++n;
						break;
					}
				}
			}
		}
	} else {
		const Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == type && v->subtype <= subtype && v->owner == owner) {
				_vehicle_sort[n].index = v->index;
				_vehicle_sort[n].owner = v->owner;
				++n;
			}
		}
	}

	free(vl->sort_list);
	vl->sort_list = malloc(n * sizeof(vl->sort_list[0]));
	if (n != 0 && vl->sort_list == NULL)
		error("Could not allocate memory for the vehicle-sorting-list");
	vl->list_length = n;

	for (i = 0; i < n; ++i) vl->sort_list[i] = _vehicle_sort[i];

	vl->flags &= ~VL_REBUILD;
	vl->flags |= VL_RESORT;
}

void SortVehicleList(vehiclelist_d *vl)
{
	if (!(vl->flags & VL_RESORT)) return;

	_internal_sort_order = vl->flags & VL_DESC;
	_internal_name_sorter_id = STR_SV_TRAIN_NAME;
	_last_vehicle_idx = 0; // used for "cache" in namesorting
	qsort(vl->sort_list, vl->list_length, sizeof(vl->sort_list[0]),
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
	if(v->age <= 365 * 2)
		ormod = PALETTE_TO_GREY;
	else if(v->profit_last_year < 0)
		ormod = PALETTE_TO_RED;
	else if(v->profit_last_year < 10000)
		ormod = PALETTE_TO_YELLOW;
	else
		ormod = PALETTE_TO_GREEN;
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
	uint32 cmask;
	CargoID cid, cargo = CT_INVALID;
	int y = 25;
	const Vehicle* u;
#define show_cargo(ctype) { \
		byte colour = 16; \
		if (sel == 0) { \
			cargo = ctype; \
			colour = 12; \
} \
		sel--; \
		DrawString(6, y, _cargoc.names_s[ctype], colour); \
		y += 10; \
}

		/* Check if vehicle has custom refit or normal ones, and get its bitmasked value.
		 * If its a train, 'or' this with the refit masks of the wagons. Now just 'and'
		 * it with the bitmask of available cargo on the current landscape, and
		 * where the bits are set: those are available */
		cmask = 0;
		u = v;
		do {
			cmask |= _engine_info[u->engine_type].refit_mask;
			u = u->next;
		} while (v->type == VEH_Train && u != NULL);

		/* Check which cargo has been selected from the refit window and draw list */
		for (cid = 0; cmask != 0; cmask >>= 1, cid++) {
			if (HASBIT(cmask, 0)) // vehicle is refittable to this cargo
				show_cargo(_local_cargo_id_ctype[cid]);
		}
		return cargo;
}

/************ Sorter functions *****************/
int CDECL GeneralOwnerSorter(const void *a, const void *b)
{
	return (*(const SortStruct*)a).owner - (*(const SortStruct*)b).owner;
}

/* Variables you need to set before calling this function!
* 1. (byte)_internal_sort_type:					sorting criteria to sort on
* 2. (bool)_internal_sort_order:				sorting order, descending/ascending
* 3. (uint32)_internal_name_sorter_id:	default StringID of the vehicle when no name is set. eg
*    STR_SV_TRAIN_NAME for trains or STR_SV_AIRCRAFT_NAME for aircraft
*/
static int CDECL VehicleUnsortedSorter(const void *a, const void *b)
{
	return ((const SortStruct*)a)->index - ((const SortStruct*)b)->index;
}

// if the sorting criteria had the same value, sort vehicle by unitnumber
#define VEHICLEUNITNUMBERSORTER(r, a, b) {if (r == 0) {r = a->unitnumber - b->unitnumber;}}

static int CDECL VehicleNumberSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->unitnumber - vb->unitnumber;

	return (_internal_sort_order & 1) ? -r : r;
}

static char _bufcache[64];	// used together with _last_vehicle_idx to hopefully speed up stringsorting
static int CDECL VehicleNameSorter(const void *a, const void *b)
{
	const SortStruct *cmp1 = (const SortStruct*)a;
	const SortStruct *cmp2 = (const SortStruct*)b;
	const Vehicle *va = GetVehicle(cmp1->index);
	const Vehicle *vb = GetVehicle(cmp2->index);
	char buf1[64] = "\0";
	int r;

	if (va->string_id != _internal_name_sorter_id) {
		SetDParam(0, va->string_id);
		GetString(buf1, STR_JUST_STRING);
	}

	if ( cmp2->index != _last_vehicle_idx) {
		_last_vehicle_idx = cmp2->index;
		_bufcache[0] = '\0';
		if (vb->string_id != _internal_name_sorter_id) {
			SetDParam(0, vb->string_id);
			GetString(_bufcache, STR_JUST_STRING);
		}
	}

	r =  strcmp(buf1, _bufcache);	// sort by name

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleAgeSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->age - vb->age;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitThisYearSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->profit_this_year - vb->profit_this_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleProfitLastYearSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->profit_last_year - vb->profit_last_year;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleCargoSorter(const void *a, const void *b)
{
	const Vehicle* va = GetVehicle(((const SortStruct*)a)->index);
	const Vehicle* vb = GetVehicle(((const SortStruct*)b)->index);
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
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
	int r = va->reliability - vb->reliability;

	VEHICLEUNITNUMBERSORTER(r, va, vb);

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL VehicleMaxSpeedSorter(const void *a, const void *b)
{
	const Vehicle *va = GetVehicle((*(const SortStruct*)a).index);
	const Vehicle *vb = GetVehicle((*(const SortStruct*)b).index);
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

// this define is to match engine.c, but engine.c keeps it to itself
// ENGINE_AVAILABLE is used in ReplaceVehicleWndProc
#define ENGINE_AVAILABLE ((e->flags & 1 && HASBIT(info->climates, _opt.landscape)) || HASBIT(e->player_avail, _local_player))

/*  if show_outdated is selected, it do not sort psudo engines properly but it draws all engines
 *	if used compined with show_cars set to false, it will work as intended. Replace window do it like that
 *  this was a big hack even before show_outdated was added. Stupid newgrf :p										*/
static void train_engine_drawing_loop(int *x, int *y, int *pos, int *sel, int *selected_id, RailType railtype,
	uint8 lines_drawn, bool is_engine, bool show_cars, bool show_outdated)
{
	EngineID i;
	byte colour;
	const Player *p = GetPlayer(_local_player);

	for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
		const Engine *e = GetEngine(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const EngineInfo *info = &_engine_info[i];

		if (p->engine_replacement[i] == INVALID_ENGINE && _player_num_engines[i] == 0 && show_outdated ) continue;

		if ( rvi->power == 0 && !(show_cars) )   // disables display of cars (works since they do not have power)
			continue;

		if (*sel == 0) *selected_id = i;


		colour = *sel == 0 ? 0xC : 0x10;
		if (!(ENGINE_AVAILABLE && show_outdated && RailVehInfo(i)->power && e->railtype == railtype)) {
			if (e->railtype != railtype || !(rvi->flags & RVI_WAGON) != is_engine ||
				!HASBIT(e->player_avail, _local_player))
				continue;
		} /*else {
		// TODO find a nice red colour for vehicles being replaced
			if ( _autoreplace_array[i] != i )
				colour = *sel == 0 ? 0x44 : 0x45;
		} */

		if (IS_INT_INSIDE(--*pos, -lines_drawn, 0)) {
			DrawString(*x + 59, *y + 2, GetCustomEngineName(i),
				colour);
			// show_outdated is true only for left side, which is where we show old replacements
			DrawTrainEngine(*x + 29, *y + 6, i, (_player_num_engines[i] == 0 && show_outdated) ?
				PALETTE_CRASH : SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
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
	RailType railtype;
	int selected_id[2] = {-1,-1};
	int sel[2];
	int count = 0;
	int count2 = 0;
	EngineID engine_id;
	const Player *p = GetPlayer(_local_player);

	sel[0] = WP(w,replaceveh_d).sel_index[0];
	sel[1] = WP(w,replaceveh_d).sel_index[1];

	switch (WP(w,replaceveh_d).vehicletype) {
		case VEH_Train: {
			railtype = _railtype_selected_in_replace_gui;
			w->widget[13].color = _player_colors[_local_player];	// sets the colour of that art thing
			w->widget[16].color = _player_colors[_local_player];	// sets the colour of that art thing
			for (engine_id = 0; engine_id < NUM_TRAIN_ENGINES; engine_id++) {
				const Engine *e = GetEngine(engine_id);
				const EngineInfo *info = &_engine_info[engine_id];

				if (ENGINE_AVAILABLE && RailVehInfo(engine_id)->power && e->railtype == railtype ) {
					if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
						if (sel[0]==0)  selected_id[0] = engine_id;
						count++;
						sel[0]--;
					}
					if (HASBIT(e->player_avail, _local_player)) {
						if (sel[1]==0)  selected_id[1] = engine_id;
						count2++;
						sel[1]--;
					}
				}
			}
			break;
		}
		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			const Engine* e = GetEngine(ROAD_ENGINES_INDEX);
			byte cargo;
			const EngineInfo* info;
			engine_id = ROAD_ENGINES_INDEX;

			do {
				info = &_engine_info[engine_id];
				if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
					if (sel[0]==0)  selected_id[0] = engine_id;
					count++;
					sel[0]--;
				}
			} while (++engine_id,++e,--num);

			if ( selected_id[0] != -1 ) {   // only draw right array if we have anything in the left one
				num = NUM_ROAD_ENGINES;
				engine_id = ROAD_ENGINES_INDEX;
				e = GetEngine(ROAD_ENGINES_INDEX);
				cargo = RoadVehInfo(selected_id[0])->cargo_type;

				do {
					if ( cargo == RoadVehInfo(engine_id)->cargo_type && HASBIT(e->player_avail, _local_player)) {
						count2++;
						if (sel[1]==0)  selected_id[1] = engine_id;
						sel[1]--;
					}
				} while (++engine_id,++e,--num);
			}
			break;
		}

		case VEH_Ship: {
			int num = NUM_SHIP_ENGINES;
			const Engine* e = GetEngine(SHIP_ENGINES_INDEX);
			byte cargo, refittable;
			const EngineInfo* info;
			engine_id = SHIP_ENGINES_INDEX;

			do {
				info = &_engine_info[engine_id];
				if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
					if ( sel[0] == 0 )  selected_id[0] = engine_id;
					count++;
					sel[0]--;
				}
			} while (++engine_id,++e,--num);

			if ( selected_id[0] != -1 ) {
				num = NUM_SHIP_ENGINES;
				e = GetEngine(SHIP_ENGINES_INDEX);
				engine_id = SHIP_ENGINES_INDEX;
				cargo = ShipVehInfo(selected_id[0])->cargo_type;
				refittable = ShipVehInfo(selected_id[0])->refittable;

				do {
					if (HASBIT(e->player_avail, _local_player)
					&& ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {

						if ( sel[1]==0)  selected_id[1] = engine_id;
						sel[1]--;
						count2++;
					}
				} while (++engine_id,++e,--num);
			}
			break;
		}   //end of ship

		case VEH_Aircraft:{
			int num = NUM_AIRCRAFT_ENGINES;
			byte subtype;
			const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);
			const EngineInfo* info;
			engine_id = AIRCRAFT_ENGINES_INDEX;

			do {
				info = &_engine_info[engine_id];
				if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
					count++;
					if (sel[0]==0)  selected_id[0] = engine_id;
					sel[0]--;
				}
			} while (++engine_id,++e,--num);

			if ( selected_id[0] != -1 ) {
				num = NUM_AIRCRAFT_ENGINES;
				e = GetEngine(AIRCRAFT_ENGINES_INDEX);
				subtype = AircraftVehInfo(selected_id[0])->subtype;
				engine_id = AIRCRAFT_ENGINES_INDEX;
				do {
					if (HASBIT(e->player_avail, _local_player)) {
						if (HASBIT(subtype, 0) == HASBIT(AircraftVehInfo(engine_id)->subtype, 0)) {
							count2++;
							if (sel[1]==0)  selected_id[1] = engine_id;
							sel[1]--;
						}
					}
				} while (++engine_id,++e,--num);
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
	int sel1, int sel2, int selected_id1, int selected_id2)
{
	int sel[2];
	int selected_id[2];
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
			train_engine_drawing_loop(&x, &y, &pos, &sel[0], &selected_id[0], railtype, w->vscroll.cap, true, false, true); // True engines
			train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, true, false, false); // True engines
			train_engine_drawing_loop(&x2, &y2, &pos2, &sel[1], &selected_id[1], railtype, w->vscroll.cap, false, false, false); // Feeble wagons
			break;
		}

		case VEH_Road: {
			int num = NUM_ROAD_ENGINES;
			const Engine* e = GetEngine(ROAD_ENGINES_INDEX);
			EngineID engine_id = ROAD_ENGINES_INDEX;
			byte cargo;
			const EngineInfo* info;

			if ( selected_id[0] >= ROAD_ENGINES_INDEX && selected_id[0] <= SHIP_ENGINES_INDEX ) {
				cargo = RoadVehInfo(selected_id[0])->cargo_type;

				do {
					info = &_engine_info[engine_id];
					if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+59, y+2, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x+29, y+6, engine_id, _player_num_engines[engine_id] > 0 ? SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)) : PALETTE_CRASH);
							SetDParam(0, _player_num_engines[engine_id]);
							DrawStringRightAligned(213, y+5, STR_TINY_BLACK, 0);
							y += 14;
						}
					sel[0]--;
					}

					if ( RoadVehInfo(engine_id)->cargo_type == cargo && HASBIT(e->player_avail, _local_player) ) {
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0) && RoadVehInfo(engine_id)->cargo_type == cargo) {
							DrawString(x2+59, y2+2, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawRoadVehEngine(x2+29, y2+6, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
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
			const EngineInfo* info;

			if ( selected_id[0] != -1 ) {
				cargo = ShipVehInfo(selected_id[0])->cargo_type;
				refittable = ShipVehInfo(selected_id[0])->refittable;

				do {
					info = &_engine_info[engine_id];
					if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+75, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawShipEngine(x+35, y+10, engine_id, _player_num_engines[engine_id] > 0 ? SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)) : PALETTE_CRASH);
							SetDParam(0, _player_num_engines[engine_id]);
							DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
							y += 24;
						}
						sel[0]--;
					}
					if ( selected_id[0] != -1 ) {
						if (HASBIT(e->player_avail, _local_player) && ( cargo == ShipVehInfo(engine_id)->cargo_type || refittable & ShipVehInfo(engine_id)->refittable)) {
							if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
								DrawString(x2+75, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
								DrawShipEngine(x2+35, y2+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
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
			if ( selected_id[0] != -1 ) {
				int num = NUM_AIRCRAFT_ENGINES;
				const Engine* e = GetEngine(AIRCRAFT_ENGINES_INDEX);
				EngineID engine_id = AIRCRAFT_ENGINES_INDEX;
				byte subtype = AircraftVehInfo(selected_id[0])->subtype;
				const EngineInfo* info;

				do {
					info = &_engine_info[engine_id];
					if (_player_num_engines[engine_id] > 0 || p->engine_replacement[engine_id] != INVALID_ENGINE) {
						if (sel[0]==0) selected_id[0] = engine_id;
						if (IS_INT_INSIDE(--pos, -w->vscroll.cap, 0)) {
							DrawString(x+62, y+7, GetCustomEngineName(engine_id), sel[0]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x+29, y+10, engine_id, _player_num_engines[engine_id] > 0 ? SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)) : PALETTE_CRASH);
							SetDParam(0, _player_num_engines[engine_id]);
							DrawStringRightAligned(213, y+15, STR_TINY_BLACK, 0);
							y += 24;
						}
						sel[0]--;
					}
					if ( (HASBIT(subtype, 0) == HASBIT(AircraftVehInfo(engine_id)->subtype, 0))
						&& HASBIT(e->player_avail, _local_player) ) {
						if (sel[1]==0) selected_id[1] = engine_id;
						if (IS_INT_INSIDE(--pos2, -w->vscroll.cap, 0)) {
							DrawString(x2+62, y2+7, GetCustomEngineName(engine_id), sel[1]==0 ? 0xC : 0x10);
							DrawAircraftEngine(x2+29, y2+10, engine_id, SPRITE_PALETTE(PLAYER_SPRITE_COLOR(_local_player)));
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
	static const StringID _vehicle_type_names[4] = {STR_019F_TRAIN, STR_019C_ROAD_VEHICLE, STR_019E_SHIP,STR_019D_AIRCRAFT};
	const Player *p = GetPlayer(_local_player);

	switch (e->event) {
		case WE_PAINT: {
				int pos = w->vscroll.pos;
				int selected_id[2] = {-1,-1};
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
						if ( vehicle->owner == _local_player ) {
							if (vehicle->type == VEH_Aircraft && vehicle->subtype > 2) continue;

							// do not count the vehicles, that contains only 0 in all var
							if (vehicle->engine_type == 0 && vehicle->spritenum == 0 ) continue;

							if (vehicle->type != GetEngine(vehicle->engine_type)->type) continue;

							_player_num_engines[vehicle->engine_type]++;
						}
					}
				}

				SetupScrollStuffForReplaceWindow(w);

				selected_id[0] = WP(w,replaceveh_d).sel_engine[0];
				selected_id[1] = WP(w,replaceveh_d).sel_engine[1];

			// sets the selected left item to the top one if it's greater than the number of vehicles in the left side

				if ( WP(w,replaceveh_d).count[0] <= sel[0] ) {
					if (WP(w,replaceveh_d).count[0]) {
						sel[0] = 0;
						WP(w,replaceveh_d).sel_index[0] = 0;
						w->vscroll.pos = 0;
						// now we go back to set selected_id[1] properly
						SetWindowDirty(w);
						return;
					} else { //there are no vehicles in the left window
						selected_id[1] = -1;
					}
				}

				if ( WP(w,replaceveh_d).count[1] <= sel[1] ) {
					if (WP(w,replaceveh_d).count[1]) {
						sel[1] = 0;
						WP(w,replaceveh_d).sel_index[1] = 0;
						w->vscroll2.pos = 0;
						// now we go back to set selected_id[1] properly
						SetWindowDirty(w);
						return;
					} else { //there are no vehicles in the right window
						selected_id[1] = -1;
					}
				}

				// Disable the "Start Replacing" button if:
				//    Either list is empty
				// or Both lists have the same vehicle selected
				// or The right list (new replacement) has the existing replacement vehicle selected
				if (selected_id[0] == -1 || selected_id[1] == -1 ||
						selected_id[0] == selected_id[1] ||
						p->engine_replacement[selected_id[0]] == selected_id[1])
					SETBIT(w->disabled_state, 4);
				else
					CLRBIT(w->disabled_state, 4);

				// Disable the "Stop Replacing" button if:
				//    The left list (existing vehicle) is empty
				// or The selected vehicle has no replacement set up
				if (selected_id[0] == -1 ||
						p->engine_replacement[selected_id[0]] == INVALID_ENGINE)
					SETBIT(w->disabled_state, 6);
				else
					CLRBIT(w->disabled_state, 6);

				// now the actual drawing of the window itself takes place
				SetDParam(0, _vehicle_type_names[WP(w, replaceveh_d).vehicletype - VEH_Train]);

				if (WP(w, replaceveh_d).vehicletype == VEH_Train) {
					// set on/off for renew_keep_length
					SetDParam(1, p->renew_keep_length ? STR_CONFIG_PATCHES_ON : STR_CONFIG_PATCHES_OFF);
				}

				DrawWindowWidgets(w);

				// sets up the string for the vehicle that is being replaced to
				if ( selected_id[0] != -1 ) {
					if (p->engine_replacement[selected_id[0]] == INVALID_ENGINE)
						SetDParam(0, STR_NOT_REPLACING);
					else
						SetDParam(0, GetCustomEngineName(p->engine_replacement[selected_id[0]]));
				} else {
					SetDParam(0, STR_NOT_REPLACING_VEHICLE_SELECTED);
				}


				DrawString(145, (w->resize.step_height == 24 ? 67 : 87 ) + ( w->resize.step_height * w->vscroll.cap), STR_02BD, 0x10);


				/*	now we draw the two arrays according to what we just counted */
				DrawEngineArrayInReplaceWindow(w, x, y, x2, y2, pos, pos2, sel[0], sel[1], selected_id[0], selected_id[1]);

				WP(w,replaceveh_d).sel_engine[0] = selected_id[0];
				WP(w,replaceveh_d).sel_engine[1] = selected_id[1];
				/* now we draw the info about the vehicles we selected */
				switch (WP(w,replaceveh_d).vehicletype) {
					case VEH_Train: {
						byte i = 0;
						int offset = 0;

						for ( i = 0 ; i < 2 ; i++) {
							if ( i )
							offset = 228;
							if (selected_id[i] != -1) {
								if (!(RailVehInfo(selected_id[i])->flags & RVI_WAGON)) {
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
						if (selected_id[0] != -1) {
							DrawRoadVehPurchaseInfo(2, 15 + (14 * w->vscroll.cap), selected_id[0]);
							if (selected_id[1] != -1) {
								DrawRoadVehPurchaseInfo(2 + 228, 15 + (14 * w->vscroll.cap), selected_id[1]);
							}
						}
						break;
					}   // end of VEH_Road

					case VEH_Ship: {
						if (selected_id[0] != -1) {
							DrawShipPurchaseInfo(2, 15 + (24 * w->vscroll.cap), selected_id[0]);
							if (selected_id[1] != -1) {
								DrawShipPurchaseInfo(2 + 228, 15 + (24 * w->vscroll.cap), selected_id[1]);
							}
						}
						break;
					}   // end of VEH_Ship

					case VEH_Aircraft: {
						if (selected_id[0] != -1) {
							DrawAircraftPurchaseInfo(2, 15 + (24 * w->vscroll.cap), selected_id[0]);
							if (selected_id[1] != -1) {
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
				case 14: case 15: { /* Select sorting criteria dropdown menu */
					ShowDropDownMenu(w, _rail_types_list, _railtype_selected_in_replace_gui, 15, 0, ~GetPlayer(_local_player)->avail_railtypes);
					break;
				}
				case 17: { /* toggle renew_keep_length */
					DoCommandP(0, 5, p->renew_keep_length ? 0 : 1, NULL, CMD_REPLACE_VEHICLE);
				} break;
				case 4: { /* Start replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					EngineID veh_to = WP(w, replaceveh_d).sel_engine[1];
					DoCommandP(0, 3, veh_from + (veh_to << 16), NULL, CMD_REPLACE_VEHICLE);
					SetWindowDirty(w);
					break;
				}

				case 6: { /* Stop replacing */
					EngineID veh_from = WP(w, replaceveh_d).sel_engine[0];
					DoCommandP(0, 3, veh_from + (INVALID_ENGINE << 16), NULL, CMD_REPLACE_VEHICLE);
					SetWindowDirty(w);
					break;
				}

				case 7:
					// sets up that the left one was clicked. The default values are for the right one (9)
					// this way, the code for 9 handles both sides
					click_scroll_pos = w->vscroll.pos;
					click_scroll_cap = w->vscroll.cap;
					click_side = 0;
				case 9: {
					uint i = (e->click.pt.y - 14) / w->resize.step_height;
					if (i < click_scroll_cap) {
						WP(w,replaceveh_d).sel_index[click_side] = i + click_scroll_pos;
						SetWindowDirty(w);
					}
				} break;
			}

		} break;

		case WE_DROPDOWN_SELECT: { /* we have selected a dropdown item in the list */
			_railtype_selected_in_replace_gui = e->dropdown.index;
			SetWindowDirty(w);
		} break;

		case WE_RESIZE: {
			w->vscroll.cap  += e->sizing.diff.y / (int)w->resize.step_height;
			w->vscroll2.cap += e->sizing.diff.y / (int)w->resize.step_height;

			w->widget[7].unkA = (w->vscroll.cap  << 8) + 1;
			w->widget[9].unkA = (w->vscroll2.cap << 8) + 1;
		} break;
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
{      WWT_PANEL,     RESIZE_TB,    14,     0,   138,   198,   209, STR_NULL,       STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   153,   210,   221, STR_NULL,       STR_NULL},
{      WWT_PANEL,     RESIZE_TB,    14,   154,   277,   210,   221, STR_NULL,       STR_REPLACE_HELP_RAILTYPE},
{   WWT_CLOSEBOX,     RESIZE_TB,    14,   278,   289,   210,   221, STR_0225,       STR_REPLACE_HELP_RAILTYPE},
{      WWT_PANEL,     RESIZE_TB,    14,   290,   305,   210,   221, STR_NULL,       STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   317,   455,   198,   209, STR_REPLACE_REMOVE_WAGON,       STR_REPLACE_REMOVE_WAGON_HELP},
// end of train specific stuff
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   210,   221, STR_NULL,       STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_road_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,       STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   126,   197, STR_NULL,       STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   198,   209, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   198,   209, STR_NULL,       STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   198,   209, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   125, 0x801,          STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   125, 0x801,          STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   125, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   126,   197, STR_NULL,       STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   198,   209, STR_NULL,       STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const Widget _replace_ship_aircraft_vehicle_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   443,     0,    13, STR_REPLACE_VEHICLES_WHITE,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   444,   455,     0,    13, STR_NULL,       STR_STICKY_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   110,   161, STR_NULL,       STR_NULL},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   138,   162,   173, STR_REPLACE_VEHICLES_START,  STR_REPLACE_HELP_START_BUTTON},
{      WWT_PANEL,     RESIZE_TB,    14,   139,   305,   162,   173, STR_NULL,       STR_REPLACE_HELP_REPLACE_INFO_TAB},
{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   306,   443,   162,   173, STR_REPLACE_VEHICLES_STOP,   STR_REPLACE_HELP_STOP_BUTTON},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    14,   109, 0x401,          STR_REPLACE_HELP_LEFT_ARRAY},
{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    14,   109, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{     WWT_MATRIX, RESIZE_BOTTOM,    14,   228,   443,    14,   109, 0x401,          STR_REPLACE_HELP_RIGHT_ARRAY},
{ WWT_SCROLL2BAR, RESIZE_BOTTOM,    14,   444,   455,    14,   109, STR_NULL,       STR_0190_SCROLL_BAR_SCROLLS_LIST},
{      WWT_PANEL,     RESIZE_TB,    14,   228,   455,   110,   161, STR_NULL,       STR_NULL},
{  WWT_RESIZEBOX,     RESIZE_TB,    14,   444,   455,   162,   173, STR_NULL,       STR_RESIZE_BUTTON},
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
	-1, -1, 456, 174,
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
* Assigns an already open vehicle window to a new vehicle. If the vehicle got any sub window open (orders and so on) it will change owner too
* @param *from_v the current owner of the window
* @param *to_v the new owner of the window
*/
void ChangeVehicleViewWindow(const Vehicle *from_v, const Vehicle *to_v)
{
	Window *w;

	w = FindWindowById(WC_VEHICLE_VIEW, from_v->index);

	if (w != NULL) {
		w->window_number = to_v->index;
		WP(w, vp_d).follow_vehicle = to_v->index;	// tell the viewport to follow the new vehicle
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
