/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "rail_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "vehicle.h"
#include "viewport.h"
#include "station.h"
#include "command.h"
#include "player.h"
#include "engine.h"
#include "vehicle_gui.h"
#include "depot.h"
#include "train.h"
#include "newgrf_engine.h"
#include "date.h"
#include "strings.h"

typedef enum BuildTrainWidgets {
	BUILD_TRAIN_WIDGET_CLOSEBOX = 0,
	BUILD_TRAIN_WIDGET_CAPTION,
	BUILD_TRAIN_WIDGET_SORT_ASSENDING_DESCENDING,
	BUILD_TRAIN_WIDGET_SORT_TEXT,
	BUILD_TRAIN_WIDGET_SORT_DROPDOWN,
	BUILD_TRAIN_WIDGET_LIST,
	BUILD_TRAIN_WIDGET_SCROLLBAR,
	BUILD_TRAIN_WIDGET_PANEL,
	BUILD_TRAIN_WIDGET_ENGINES,
	BUILD_TRAIN_WIDGET_WAGONS,
	BUILD_TRAIN_WIDGET_BOTH,
	BUILD_TRAIN_WIDGET_BUILD,
	BUILD_TRAIN_WIDGET_RENAME,
	BUILD_TRAIN_WIDGET_RESIZE,
} BuildTrainWidget;

static const Widget _new_rail_vehicle_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
	{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   227,     0,    13, STR_JUST_STRING,        STR_018C_WINDOW_TITLE_DRAG_THIS},
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, STR_SORT_BY,            STR_SORT_ORDER_TIP},
	{      WWT_PANEL,   RESIZE_NONE,    14,    81,   215,    14,    25, 0x0,                    STR_SORT_CRITERIA_TIP},
	{    WWT_TEXTBTN,   RESIZE_NONE,    14,   216,   227,    14,    25, STR_0225,               STR_SORT_CRITERIA_TIP},
	{     WWT_MATRIX, RESIZE_BOTTOM,    14,     0,   215,    26,   137, 0x801,                  STR_8843_TRAIN_VEHICLE_SELECTION},
	{  WWT_SCROLLBAR, RESIZE_BOTTOM,    14,   216,   227,    26,   137, 0x0,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},
	{      WWT_PANEL,     RESIZE_TB,    14,     0,   227,   138,   209, 0x0,                    STR_NULL},
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,    76,   210,   221, STR_BLACK_ENGINES,      STR_BUILD_TRAIN_ENGINES_TIP},
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,    77,   151,   210,   221, STR_BLACK_WAGONS,       STR_BUILD_TRAIN_WAGONS_TIP},
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   152,   227,   210,   221, STR_BLACK_BOTH,         STR_BUILD_TRAIN_BOTH_TIP},
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   107,   222,   233, STR_881F_BUILD_VEHICLE, STR_8844_BUILD_THE_HIGHLIGHTED_TRAIN},
	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,   108,   215,   222,   233, STR_8820_RENAME,        STR_8845_RENAME_TRAIN_VEHICLE_TYPE},
	{  WWT_RESIZEBOX,     RESIZE_TB,    14,   216,   227,   222,   233, 0x0,                    STR_RESIZE_BUTTON},
	{   WIDGETS_END},
};

static bool _internal_sort_order; // descending/ascending
static byte _last_sort_criteria = 0;
static bool _last_sort_order = false;

typedef int CDECL VehicleSortListingTypeFunction(const void*, const void*);

static int CDECL TrainEngineNumberSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r = ListPositionOfEngine(va) - ListPositionOfEngine(vb);

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineCostSorter(const void *a, const void *b)
{
	const int va = RailVehInfo(*(const EngineID*)a)->base_cost;
	const int vb = RailVehInfo(*(const EngineID*)b)->base_cost;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineSpeedSorter(const void *a, const void *b)
{
	const int va = RailVehInfo(*(const EngineID*)a)->max_speed;
	const int vb = RailVehInfo(*(const EngineID*)b)->max_speed;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEnginePowerSorter(const void *a, const void *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*(const EngineID*)a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*(const EngineID*)b);

	const int va = rvi_a->power << (rvi_a->flags & RVI_MULTIHEAD ? 1 : 0);
	const int vb = rvi_b->power << (rvi_b->flags & RVI_MULTIHEAD ? 1 : 0);
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineIntroDateSorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->intro_date;
	const int vb = GetEngine(*(const EngineID*)b)->intro_date;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static EngineID _last_engine; // cached vehicle to hopefully speed up name-sorting

static char _bufcache[64]; // used together with _last_vehicle to hopefully speed up stringsorting
static int CDECL TrainEngineNameSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	char buf1[64];
	int r;

	GetString(buf1, GetCustomEngineName(va));

	if (vb != _last_engine) {
		_last_engine = vb;
		_bufcache[0] = '\0';

		GetString(_bufcache, GetCustomEngineName(vb));
	}

	r =  strcmp(buf1, _bufcache); // sort by name

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}

	return (_internal_sort_order & 1) ? -r : r;
}

static int CDECL TrainEngineRunningCostSorter(const void *a, const void *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*(const EngineID*)a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*(const EngineID*)b);

	const int va = rvi_a->running_cost_base * _price.running_rail[rvi_a->running_cost_class] * (rvi_a->flags & RVI_MULTIHEAD ? 2 : 1);
	const int vb = rvi_b->running_cost_base * _price.running_rail[rvi_b->running_cost_class] * (rvi_b->flags & RVI_MULTIHEAD ? 2 : 1);
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEnginePowerVsRunningCostSorter(const void *a, const void *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*(const EngineID*)a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*(const EngineID*)b);

	/* Here we are using a few tricks to get the right sort.
	 * We want power/running cost, but since we usually got higher running cost than power and we store the result in an int,
	 * we will actually calculate cunning cost/power (to make it more than 1).
	 * Because of this, the return value have to be reversed as well and we return b - a instead of a - b.
	 * Another thing is that both power and running costs should be doubled for multiheaded engines.
	 * Since it would be multipling with 2 in both numerator and denumerator, it will even themselves out and we skip checking for multiheaded. */
	const int va = (rvi_a->running_cost_base * _price.running_rail[rvi_a->running_cost_class]) / rvi_a->power;
	const int vb = (rvi_b->running_cost_base * _price.running_rail[rvi_b->running_cost_class]) / rvi_b->power;
	const int r = vb - va;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineReliabilitySorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->reliability;
	const int vb = GetEngine(*(const EngineID*)b)->reliability;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return TrainEngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static VehicleSortListingTypeFunction* const _engine_sorter[] = {
	&TrainEngineNumberSorter,
	&TrainEngineCostSorter,
	&TrainEngineSpeedSorter,
	&TrainEnginePowerSorter,
	&TrainEngineIntroDateSorter,
	&TrainEngineNameSorter,
	&TrainEngineRunningCostSorter,
	&TrainEnginePowerVsRunningCostSorter,
	&TrainEngineReliabilitySorter,
};

static const StringID _engine_sort_listing[] = {
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_ENGINE_SORT_POWER,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_ENGINE_SORT_RUNNING_COST,
	STR_ENGINE_SORT_POWER_VS_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	INVALID_STRING_ID
};

/**
 * Draw the purchase info details of train engine at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawTrainEnginePurchaseInfo(int x, int y, EngineID engine_number)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine_number);
	const Engine *e = GetEngine(engine_number);
	int multihead = (rvi->flags&RVI_MULTIHEAD?1:0);
	YearMonthDay ymd;
	ConvertDateToYMD(e->intro_date, &ymd);

	/* Purchase Cost - Engine weight */
	SetDParam(0, rvi->base_cost * (_price.build_railvehicle >> 3) >> 5);
	SetDParam(1, rvi->weight << multihead);
	DrawString(x,y, STR_PURCHASE_INFO_COST_WEIGHT, 0);
	y += 10;

	/* Max speed - Engine power */
	SetDParam(0, rvi->max_speed);
	SetDParam(1, rvi->power << multihead);
	DrawString(x,y, STR_PURCHASE_INFO_SPEED_POWER, 0);
	y += 10;

	/* Running cost */
	SetDParam(0, (rvi->running_cost_base * _price.running_rail[rvi->running_cost_class] >> 8) << multihead);
	DrawString(x,y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Powered wagons power - Powered wagons extra weight */
	if (rvi->pow_wag_power != 0) {
		SetDParam(0, rvi->pow_wag_power);
		SetDParam(1, rvi->pow_wag_weight);
		DrawString(x,y, STR_PURCHASE_INFO_PWAGPOWER_PWAGWEIGHT, 0);
		y += 10;
	};

	/* Cargo type + capacity, or N/A */
	SetDParam(0, STR_8838_N_A);
	SetDParam(2, STR_EMPTY);
	if (rvi->capacity != 0) {
		SetDParam(0, _cargoc.names_long[rvi->cargo_type]);
		SetDParam(1, (rvi->capacity * (CountArticulatedParts(engine_number) + 1)) << multihead);
		SetDParam(2, STR_9842_REFITTABLE);
	}
	DrawString(x,y, STR_PURCHASE_INFO_CAPACITY, 0);
	y += 10;

	/* Design date - Life length */
	SetDParam(0, ymd.year);
	SetDParam(1, e->lifelength);
	DrawString(x,y, STR_PURCHASE_INFO_DESIGNED_LIFE, 0);
	y += 10;

	/* Reliability */
	SetDParam(0, e->reliability * 100 >> 16);
	DrawString(x,y, STR_PURCHASE_INFO_RELIABILITY, 0);
	y += 10;

	/* Additional text from NewGRF */
	// XXX 227 will become a calculated width...
	y += ShowAdditionalText(x, y, 227, engine_number);
}

/**
 * Draw the purchase info details of a train wagon at a given location.
 * @param x,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 */
void DrawTrainWagonPurchaseInfo(int x, int y, EngineID engine_number)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine_number);
	bool refittable = (EngInfo(engine_number)->refit_mask != 0);

	/* Purchase cost */
	SetDParam(0, (rvi->base_cost * _price.build_railwagon) >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_COST, 0);
	y += 10;

	/* Wagon weight - (including cargo) */
	SetDParam(0, rvi->weight);
	SetDParam(1, (_cargoc.weights[rvi->cargo_type] * rvi->capacity >> 4) + rvi->weight);
	DrawString(x, y, STR_PURCHASE_INFO_WEIGHT_CWEIGHT, 0);
	y += 10;

	/* Cargo type + capacity, or N/A */
	SetDParam(0, STR_8838_N_A);
	SetDParam(2, STR_EMPTY);
	if (rvi->capacity != 0) {
		SetDParam(0, _cargoc.names_long[rvi->cargo_type]);
		SetDParam(1, rvi->capacity * (CountArticulatedParts(engine_number) + 1));
		SetDParam(2, refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	}
	DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
	y += 10;

	/* Wagon speed limit, displayed if above zero */
	if (rvi->max_speed > 0 && _patches.wagon_speed_limits) {
		SetDParam(0, rvi->max_speed);
		DrawString(x,y, STR_PURCHASE_INFO_SPEED, 0);
		y += 10;
	}

	/* Additional text from NewGRF */
	y += ShowAdditionalText(x, y, 227, engine_number);
}

void CcBuildWagon(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	Vehicle *v, *found;

	if (!success) return;

	// find a locomotive in the depot.
	found = NULL;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && IsFrontEngine(v) &&
				v->tile == tile &&
				v->u.rail.track == 0x80) {
			if (found != NULL) return; // must be exactly one.
			found = v;
		}
	}

	// if we found a loco,
	if (found != NULL) {
		found = GetLastVehicleInChain(found);
		// put the new wagon at the end of the loco.
		DoCommandP(0, _new_vehicle_id | (found->index << 16), 0, NULL, CMD_MOVE_RAIL_VEHICLE);
		RebuildVehicleLists();
	}
}

void CcBuildLoco(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	const Vehicle *v;

	if (!success) return;

	v = GetVehicle(_new_vehicle_id);
	if (tile == _backup_orders_tile) {
		_backup_orders_tile = 0;
		RestoreVehicleOrders(v, _backup_orders_data);
	}
	ShowTrainViewWindow(v);
}

void CcCloneTrain(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) ShowTrainViewWindow(GetVehicle(_new_vehicle_id));
}
static void engine_drawing_loop(const EngineID *engines, const uint16 engine_count,
								const int x, int *y, const EngineID sel, EngineID *position, const int16 show_max)
{
	for (; (*position) < min(engine_count, show_max); (*position)++) {
		EngineID i = engines[*position];

		DrawString(x + 59, *y + 2, GetCustomEngineName(i), sel == i ? 0xC : 0x10);
		DrawTrainEngine(x + 29, *y + 6, i, GetEnginePalette(i, _local_player));
		*y += 14;
	}
}

static inline void ExtendEngineListSize(const EngineID **engine_list, uint16 *engine_list_length, uint16 step_size)
{
	*engine_list_length = min(*engine_list_length + step_size, NUM_TRAIN_ENGINES);
	*engine_list = realloc((void*)*engine_list, (*engine_list_length) * sizeof((*engine_list)[0]));
}

static void GenerateBuildList(EngineID **engines, uint16 *num_engines, EngineID **wagons, uint16 *num_wagons, RailType railtype)
{
	uint16 engine_length = *num_engines;
	uint16 wagon_length  = *num_wagons;
	EngineID j;

	(*num_engines) = 0;
	(*num_wagons)  = 0;

	if (engines == NULL) ExtendEngineListSize((const EngineID**)engines, &engine_length, 25);
	if (wagons  == NULL) ExtendEngineListSize((const EngineID**)wagons,  &wagon_length,  25);

	for (j = 0; j < NUM_TRAIN_ENGINES; j++) {
		EngineID i = GetRailVehAtPosition(j); // XXX Can be removed when the wagon list is also sorted.
		const Engine *e = GetEngine(i);
		const RailVehicleInfo *rvi = RailVehInfo(i);

		if (!HasPowerOnRail(e->railtype, railtype)) continue;
		if (!IsEngineBuildable(i, VEH_Train)) continue;

		if (rvi->flags & RVI_WAGON) {
			if (*num_wagons == wagon_length) ExtendEngineListSize((const EngineID**)wagons, &wagon_length, 5);
			(*wagons)[(*num_wagons)++] = i;
		} else {
			if (*num_engines == engine_length) ExtendEngineListSize((const EngineID**)engines, &engine_length, 5);
			(*engines)[(*num_engines)++] = i;
		}
	}

	/* Reduce array sizes if they are too big */
	if (*num_engines != engine_length) *engines = realloc((void*)*engines, (*num_engines) * sizeof((*engines)[0]));
	if (*num_wagons  != wagon_length)  *wagons  = realloc((void*)*wagons,  (*num_wagons)  * sizeof((*wagons)[0]));
}

static void SortTrainBuildList(Window *w)
{
	_internal_sort_order = WP(w,buildvehicle_d).decenting_sort_order;
	qsort((void*)WP(w, buildvehicle_d).list_a, WP(w, buildvehicle_d).list_a_length, sizeof(WP(w, buildvehicle_d).list_a[0]),
		  _engine_sorter[WP(w,buildvehicle_d).sort_criteria]);
}

static void DrawTrainBuildWindow(Window *w)
{
	int x = 1;
	int y = 27;
	EngineID position = w->vscroll.pos;
	EngineID selected_id = WP(w,buildvehicle_d).sel_engine;
	int max = w->vscroll.pos + w->vscroll.cap;
	uint16 scrollcount = 0;

	SetWindowWidgetDisabledState(w, BUILD_TRAIN_WIDGET_BUILD, w->window_number == 0); // Disable unless we got a depot to build in

	/* Draw the clicked engine/wagon/both button pressed and unpress the other two */
	SetWindowWidgetLoweredState(w, BUILD_TRAIN_WIDGET_ENGINES, WP(w,buildvehicle_d).show_engine_button == 1);
	SetWindowWidgetLoweredState(w, BUILD_TRAIN_WIDGET_WAGONS,  WP(w,buildvehicle_d).show_engine_button == 2);
	SetWindowWidgetLoweredState(w, BUILD_TRAIN_WIDGET_BOTH,    WP(w,buildvehicle_d).show_engine_button == 3);

	if (WP(w,buildvehicle_d).data_invalidated) {
		GenerateBuildList(&WP(w, buildvehicle_d).list_a, &WP(w, buildvehicle_d).list_a_length, &WP(w, buildvehicle_d).list_b, &WP(w, buildvehicle_d).list_b_length, WP(w,buildvehicle_d).railtype);
		WP(w,buildvehicle_d).data_invalidated = false;
		SortTrainBuildList(w);

		/* Make sure that the selected engine is still in the list*/
		if (WP(w,buildvehicle_d).sel_engine != INVALID_ENGINE) {
			int i;
			bool found = false;
			if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 0)) {
				for (i = 0; i < WP(w, buildvehicle_d).list_a_length; i++) {
					if (WP(w,buildvehicle_d).sel_engine != WP(w, buildvehicle_d).list_a[i]) continue;
					found = true;
					break;
				}
			}
			if (!found && HASBIT(WP(w,buildvehicle_d).show_engine_button, 1)) {
				for (i = 0; i < WP(w, buildvehicle_d).list_b_length; i++) {
					if (WP(w,buildvehicle_d).sel_engine != WP(w, buildvehicle_d).list_b[i]) continue;
					found = true;
					break;
				}
			}
			if (!found) WP(w,buildvehicle_d).sel_engine = INVALID_ENGINE;
		}
	}

	if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 0)) scrollcount += WP(w, buildvehicle_d).list_a_length;
	if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 1)) scrollcount += WP(w, buildvehicle_d).list_b_length;

	SetVScrollCount(w, scrollcount);
	SetDParam(0, WP(w,buildvehicle_d).railtype + STR_881C_NEW_RAIL_VEHICLES);
	DrawWindowWidgets(w);

	if (selected_id == INVALID_ENGINE && scrollcount != 0) {
		if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 0) && WP(w, buildvehicle_d).list_a_length != 0) {
			selected_id = WP(w, buildvehicle_d).list_a[0];
		} else {
			selected_id = WP(w, buildvehicle_d).list_b[0];
		}
		WP(w,buildvehicle_d).sel_engine = selected_id;
	}

	/* Draw the engines */
	if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 0)) {
		engine_drawing_loop(WP(w, buildvehicle_d).list_a, WP(w, buildvehicle_d).list_a_length, x, &y, selected_id, &position, max);

		/* Magically set the number 0 line to the one right after the last engine
		* This way the line numbers fit the indexes in the wagon array */
		position -= WP(w, buildvehicle_d).list_a_length;
		max      -= WP(w, buildvehicle_d).list_a_length;
	}

	/* Draw the wagons */
	if (HASBIT(WP(w,buildvehicle_d).show_engine_button, 1)) {
		engine_drawing_loop(WP(w, buildvehicle_d).list_b,  WP(w, buildvehicle_d).list_b_length,  x, &y, selected_id, &position, max);
	}

	if (selected_id != INVALID_ENGINE) {
		const RailVehicleInfo *rvi = RailVehInfo(selected_id);

		if (rvi->flags & RVI_WAGON) {
			DrawTrainWagonPurchaseInfo(2, w->widget[BUILD_TRAIN_WIDGET_PANEL].top + 1, selected_id);
		} else {
			DrawTrainEnginePurchaseInfo(2, w->widget[BUILD_TRAIN_WIDGET_PANEL].top + 1, selected_id);
		}
	}
	DrawString(85, 15, _engine_sort_listing[WP(w,buildvehicle_d).sort_criteria], 0x10);
	DoDrawString(WP(w,buildvehicle_d).decenting_sort_order ? DOWNARROW : UPARROW, 69, 15, 0x10);
}

static void NewRailVehicleWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			WP(w, buildvehicle_d).list_a_length = 0;
			WP(w, buildvehicle_d).list_b_length = 0;
			WP(w, buildvehicle_d).list_a        = NULL;
			WP(w, buildvehicle_d).list_b        = NULL;
			WP(w, buildvehicle_d).show_engine_button   = 3;
			WP(w, buildvehicle_d).data_invalidated     = true;
			WP(w, buildvehicle_d).sel_engine           = INVALID_ENGINE;
			WP(w, buildvehicle_d).sort_criteria        = _last_sort_criteria;
			WP(w, buildvehicle_d).decenting_sort_order = _last_sort_order;
			break;

		case WE_INVALIDATE_DATA:
			if (w->window_number != 0) WP(w,buildvehicle_d).railtype = GetRailType(w->window_number);
			WP(w,buildvehicle_d).data_invalidated = true;
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			free((void*)WP(w, buildvehicle_d).list_a);
			free((void*)WP(w, buildvehicle_d).list_b);
			break;

		case WE_PAINT:
			DrawTrainBuildWindow(w);
			break;


		case WE_CLICK: {
			switch (e->we.click.widget) {
				case BUILD_TRAIN_WIDGET_SORT_ASSENDING_DESCENDING:
					WP(w,buildvehicle_d).decenting_sort_order = !WP(w,buildvehicle_d).decenting_sort_order;
					_last_sort_order = WP(w,buildvehicle_d).decenting_sort_order;
					SortTrainBuildList(w);
					SetWindowDirty(w);
					break;

				case BUILD_TRAIN_WIDGET_SORT_TEXT: case BUILD_TRAIN_WIDGET_SORT_DROPDOWN:/* Select sorting criteria dropdown menu */
					ShowDropDownMenu(w, _engine_sort_listing, WP(w,buildvehicle_d).sort_criteria, BUILD_TRAIN_WIDGET_SORT_DROPDOWN, 0, 0);
					return;

				case BUILD_TRAIN_WIDGET_LIST: {
					uint i = ((e->we.click.pt.y - 26) / 14) + w->vscroll.pos;
					if (i < (uint)(WP(w, buildvehicle_d).list_a_length + WP(w, buildvehicle_d).list_b_length)) {
						if (i < WP(w, buildvehicle_d).list_a_length && HASBIT(WP(w,buildvehicle_d).show_engine_button, 0)) {
							WP(w,buildvehicle_d).sel_engine = WP(w, buildvehicle_d).list_a[i];
						} else {
							WP(w,buildvehicle_d).sel_engine = WP(w, buildvehicle_d).list_b[i - (HASBIT(WP(w,buildvehicle_d).show_engine_button, 0) ? WP(w, buildvehicle_d).list_a_length : 0)];
						}
						SetWindowDirty(w);
					}
				} break;

				case BUILD_TRAIN_WIDGET_ENGINES:
				case BUILD_TRAIN_WIDGET_WAGONS:
				case BUILD_TRAIN_WIDGET_BOTH: {
					/* First we need to figure out the new show_engine_wagon setting
					 * Because the button widgets are ordered as they are (in a row), we can calculate as following:
					 * engines = bit 0 (1 for set), wagons bit 1 (2 for set), both (2 | 1 = 3)
					 * Those numbers are the same as the clicked button - BUILD_TRAIN_WIDGET_ENGINES + 1 */

					byte click_state = e->we.click.widget - BUILD_TRAIN_WIDGET_ENGINES + 1;
					if (WP(w,buildvehicle_d).show_engine_button == click_state) break; // We clicked the pressed button
					WP(w,buildvehicle_d).sel_engine = INVALID_ENGINE;
					WP(w,buildvehicle_d).show_engine_button = click_state;
					w->vscroll.pos = 0;
					SetWindowDirty(w);
					}
					break;

				case BUILD_TRAIN_WIDGET_BUILD: {
					EngineID sel_eng = WP(w,buildvehicle_d).sel_engine;
					if (sel_eng != INVALID_ENGINE)
						DoCommandP(w->window_number, sel_eng, 0, (RailVehInfo(sel_eng)->flags & RVI_WAGON) ? CcBuildWagon : CcBuildLoco, CMD_BUILD_RAIL_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE));
				}	break;

				case BUILD_TRAIN_WIDGET_RENAME: {
					EngineID sel_eng = WP(w,buildvehicle_d).sel_engine;
					if (sel_eng != INVALID_ENGINE) {
						WP(w,buildvehicle_d).rename_engine = sel_eng;
						ShowQueryString(GetCustomEngineName(sel_eng),
										STR_886A_RENAME_TRAIN_VEHICLE_TYPE, 31, 160, w->window_class, w->window_number, CS_ALPHANUMERAL);
					}
				} break;
			}
		} break;

		case WE_ON_EDIT_TEXT: {
			if (e->we.edittext.str[0] != '\0') {
				_cmd_text = e->we.edittext.str;
				DoCommandP(0, WP(w,buildvehicle_d).rename_engine, 0, NULL,
					CMD_RENAME_ENGINE | CMD_MSG(STR_886B_CAN_T_RENAME_TRAIN_VEHICLE));
			}
		} break;

		case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
			if (WP(w,buildvehicle_d).sort_criteria != e->we.dropdown.index) {
				WP(w,buildvehicle_d).sort_criteria = e->we.dropdown.index;
				_last_sort_criteria = e->we.dropdown.index;
				SortTrainBuildList(w);
			}
			SetWindowDirty(w);
			break;

		case WE_RESIZE: {
			if (e->we.sizing.diff.y == 0) break;

			w->vscroll.cap += e->we.sizing.diff.y / 14;
			w->widget[BUILD_TRAIN_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;
		} break;
	}
}

static const WindowDesc _new_rail_vehicle_desc = {
	-1, -1, 228, 234,
	WC_BUILD_VEHICLE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_new_rail_vehicle_widgets,
	NewRailVehicleWndProc
};

void ShowBuildTrainWindow(TileIndex tile)
{
	Window *w;

	DeleteWindowById(WC_BUILD_VEHICLE, tile);

	w = AllocateWindowDesc(&_new_rail_vehicle_desc);
	w->window_number = tile;
	w->vscroll.cap = 8;
	w->widget[BUILD_TRAIN_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 14;
	w->resize.height = w->height - 14 * 4; // Minimum of 4 vehicles in the display

	if (tile != 0) {
		w->caption_color = GetTileOwner(tile);
		WP(w,buildvehicle_d).railtype = GetRailType(tile);
	} else {
		w->caption_color = _local_player;
		WP(w,buildvehicle_d).railtype = GetBestRailtype(GetPlayer(_local_player));
	}
}

/**
 * Get the number of pixels for the given wagon length.
 * @param len Length measured in 1/8ths of a standard wagon.
 * @return Number of pixels across.
 */
int WagonLengthToPixels(int len) {
	return (len * _traininfo_vehicle_width) / 8;
}

void DrawTrainImage(const Vehicle *v, int x, int y, int count, int skip, VehicleID selection)
{
	DrawPixelInfo tmp_dpi, *old_dpi;
	int dx = -(skip * 8) / _traininfo_vehicle_width;
	/* Position of highlight box */
	int highlight_l = 0;
	int highlight_r = 0;

	if (!FillDrawPixelInfo(&tmp_dpi, x - 2, y - 1, count + 1, 14)) return;

	count = (count * 8) / _traininfo_vehicle_width;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	do {
		int width = v->u.rail.cached_veh_length;

		if (dx + width > 0) {
			if (dx <= count) {
				PalSpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
				DrawSprite(GetTrainImage(v, DIR_W) | pal, 16 + WagonLengthToPixels(dx), 7 + (is_custom_sprite(RailVehInfo(v->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
				if (v->index == selection) {
					/* Set the highlight position */
					highlight_l = WagonLengthToPixels(dx) + 1;
					highlight_r = WagonLengthToPixels(dx + width) + 1;
				}
			}
		}
		dx += width;

		v = v->next;
	} while (dx < count && v != NULL);

	if (highlight_l != highlight_r) {
		/* Draw the highlight. Now done after drawing all the engines, as
		 * the next engine after the highlight could overlap it. */
		DrawFrameRect(highlight_l, 0, highlight_r, 13, 15, FR_BORDERONLY);
	}

	_cur_dpi = old_dpi;
}

static const Widget _train_view_widgets[] = {
{   WWT_CLOSEBOX,  RESIZE_NONE, 14,   0,  10,   0,  13, STR_00C5,        STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION, RESIZE_RIGHT, 14,  11, 237,   0,  13, STR_882E,        STR_018C_WINDOW_TITLE_DRAG_THIS },
{  WWT_STICKYBOX,    RESIZE_LR, 14, 238, 249,   0,  13, 0x0,             STR_STICKY_BUTTON },
{      WWT_PANEL,    RESIZE_RB, 14,   0, 231,  14, 121, 0x0,             STR_NULL },
{          WWT_6,    RESIZE_RB, 14,   2, 229,  16, 119, 0x0,             STR_NULL },
{ WWT_PUSHIMGBTN,   RESIZE_RTB, 14,   0, 237, 122, 133, 0x0,             STR_8846_CURRENT_TRAIN_ACTION_CLICK },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  14,  31, 0x2AB,           STR_8848_CENTER_MAIN_VIEW_ON_TRAIN },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  32,  49, 0x2AD,           STR_8849_SEND_TRAIN_TO_DEPOT },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  50,  67, 0x2B1,           STR_884A_FORCE_TRAIN_TO_PROCEED },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  68,  85, 0x2CB,           STR_884B_REVERSE_DIRECTION_OF_TRAIN },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  86, 103, 0x2B2,           STR_8847_SHOW_TRAIN_S_ORDERS },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249, 104, 121, 0x2B3,           STR_884C_SHOW_TRAIN_DETAILS },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  68,  85, 0x2B4,           STR_RAIL_REFIT_VEHICLE_TO_CARRY },
{ WWT_PUSHIMGBTN,    RESIZE_LR, 14, 232, 249,  32,  49, SPR_CLONE_TRAIN, STR_CLONE_TRAIN_INFO },
{      WWT_PANEL,   RESIZE_LRB, 14, 232, 249, 122, 121, 0x0,             STR_NULL },
{  WWT_RESIZEBOX,  RESIZE_LRTB, 14, 238, 249, 122, 133, 0x0,             STR_NULL },
{ WIDGETS_END }
};

static void ShowTrainDetailsWindow(const Vehicle *v);

static void TrainViewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Vehicle *v, *u;
		StringID str;
		bool is_localplayer;

		v = GetVehicle(w->window_number);

		is_localplayer = v->owner == _local_player;
		SetWindowWidgetDisabledState(w,  7, !is_localplayer);
		SetWindowWidgetDisabledState(w,  8, !is_localplayer);
		SetWindowWidgetDisabledState(w,  9, !is_localplayer);
		SetWindowWidgetDisabledState(w, 12, !is_localplayer);
		SetWindowWidgetDisabledState(w, 13, !is_localplayer);

		if (is_localplayer) {
			/* See if any vehicle can be refitted */
			for (u = v; u != NULL; u = u->next) {
				if (EngInfo(u->engine_type)->refit_mask != 0 ||
						(!(RailVehInfo(v->engine_type)->flags & RVI_WAGON) && v->cargo_cap != 0)) {
					EnableWindowWidget(w, 12);
					/* We have a refittable carriage, bail out */
					break;
				}
			}
		}

		/* draw widgets & caption */
		SetDParam(0, v->string_id);
		SetDParam(1, v->unitnumber);
		DrawWindowWidgets(w);

		if (v->u.rail.crash_anim_pos != 0) {
			str = STR_8863_CRASHED;
		} else if (v->breakdown_ctr == 1) {
			str = STR_885C_BROKEN_DOWN;
		} else if (v->vehstatus & VS_STOPPED) {
			if (v->u.rail.last_speed == 0) {
				if (v->u.rail.cached_power == 0) {
					str = STR_TRAIN_NO_POWER;
				} else {
					str = STR_8861_STOPPED;
				}
			} else {
				SetDParam(0, v->u.rail.last_speed);
				str = STR_TRAIN_STOPPING + _patches.vehicle_speed;
			}
		} else {
			switch (v->current_order.type) {
			case OT_GOTO_STATION: {
				str = STR_HEADING_FOR_STATION + _patches.vehicle_speed;
				SetDParam(0, v->current_order.dest);
				SetDParam(1, v->u.rail.last_speed);
			} break;

			case OT_GOTO_DEPOT: {
				Depot *dep = GetDepot(v->current_order.dest);
				SetDParam(0, dep->town_index);
				if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
					str = STR_HEADING_FOR_TRAIN_DEPOT + _patches.vehicle_speed;
				} else {
					str = STR_HEADING_FOR_TRAIN_DEPOT_SERVICE + _patches.vehicle_speed;
				}
				SetDParam(1, v->u.rail.last_speed);
			} break;

			case OT_LOADING:
			case OT_LEAVESTATION:
				str = STR_882F_LOADING_UNLOADING;
				break;

			case OT_GOTO_WAYPOINT: {
				SetDParam(0, v->current_order.dest);
				str = STR_HEADING_FOR_WAYPOINT + _patches.vehicle_speed;
				SetDParam(1, v->u.rail.last_speed);
				break;
			}

			default:
				if (v->num_orders == 0) {
					str = STR_NO_ORDERS + _patches.vehicle_speed;
					SetDParam(0, v->u.rail.last_speed);
				} else {
					str = STR_EMPTY;
				}
				break;
			}
		}

		/* draw the flag plus orders */
		DrawSprite(v->vehstatus & VS_STOPPED ? SPR_FLAG_VEH_STOPPED : SPR_FLAG_VEH_RUNNING, 2, w->widget[5].top + 1);
		DrawStringCenteredTruncated(w->widget[5].left + 8, w->widget[5].right, w->widget[5].top + 1, str, 0);
		DrawWindowViewport(w);
	}	break;

	case WE_CLICK: {
		int wid = e->we.click.widget;
		Vehicle *v = GetVehicle(w->window_number);

		switch (wid) {
		case 5: /* start/stop train */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_START_STOP_TRAIN | CMD_MSG(STR_883B_CAN_T_STOP_START_TRAIN));
			break;
		case 6: /* center main view */
			ScrollMainWindowTo(v->x_pos, v->y_pos);
			break;
		case 7: /* goto depot */
			/* TrainGotoDepot has a nice randomizer in the pathfinder, which causes desyncs... */
			DoCommandP(v->tile, v->index, _ctrl_pressed ? DEPOT_SERVICE : 0, NULL, CMD_SEND_TRAIN_TO_DEPOT | CMD_NO_TEST_IF_IN_NETWORK | CMD_MSG(STR_8830_CAN_T_SEND_TRAIN_TO_DEPOT));
			break;
		case 8: /* force proceed */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_FORCE_TRAIN_PROCEED | CMD_MSG(STR_8862_CAN_T_MAKE_TRAIN_PASS_SIGNAL));
			break;
		case 9: /* reverse direction */
			DoCommandP(v->tile, v->index, 0, NULL, CMD_REVERSE_TRAIN_DIRECTION | CMD_MSG(STR_8869_CAN_T_REVERSE_DIRECTION));
			break;
		case 10: /* show train orders */
			ShowOrdersWindow(v);
			break;
		case 11: /* show train details */
			ShowTrainDetailsWindow(v);
			break;
		case 12:
			ShowVehicleRefitWindow(v, INVALID_VEH_ORDER_ID);
			break;
		case 13:
			DoCommandP(v->tile, v->index, _ctrl_pressed ? 1 : 0, NULL, CMD_CLONE_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE));
			break;
		}
	} break;

	case WE_RESIZE:
		w->viewport->width          += e->we.sizing.diff.x;
		w->viewport->height         += e->we.sizing.diff.y;
		w->viewport->virtual_width  += e->we.sizing.diff.x;
		w->viewport->virtual_height += e->we.sizing.diff.y;
		break;

	case WE_DESTROY:
		DeleteWindowById(WC_VEHICLE_REFIT, w->window_number);
		DeleteWindowById(WC_VEHICLE_ORDERS, w->window_number);
		DeleteWindowById(WC_VEHICLE_DETAILS, w->window_number);
		break;

	case WE_MOUSELOOP: {
		const Vehicle *v = GetVehicle(w->window_number);
		bool train_stopped = CheckTrainStoppedInDepot(v)  >= 0;

		/* Widget 7 (send to depot) must be hidden if the train is already stopped in hangar.
		 * Widget 13 (clone) should then be shown, since cloning is allowed only while in depot and stopped.
		 * This sytem allows to have two buttons, on top of each other.
		 * The same system applies to widget 9 and 12, reverse direction and refit*/
		if (train_stopped != IsWindowWidgetHidden(w, 7) || train_stopped == IsWindowWidgetHidden(w, 13)) {
			SetWindowWidgetHiddenState(w,  7, train_stopped);  // send to depot
			SetWindowWidgetHiddenState(w,  9, train_stopped);  // reverse direction
			SetWindowWidgetHiddenState(w, 12, !train_stopped); // refit
			SetWindowWidgetHiddenState(w, 13, !train_stopped); // clone
			SetWindowDirty(w);
		}
		break;
	}

	}
}

static const WindowDesc _train_view_desc = {
	-1,-1, 250, 134,
	WC_VEHICLE_VIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	_train_view_widgets,
	TrainViewWndProc
};

void ShowTrainViewWindow(const Vehicle *v)
{
	Window *w = AllocateWindowDescFront(&_train_view_desc,v->index);

	if (w != NULL) {
		w->caption_color = v->owner;
		AssignWindowViewport(w, 3, 17, 0xE2, 0x66, w->window_number | (1 << 31), 0);
	}
}

static void TrainDetailsCargoTab(const Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		uint num = v->cargo_count;
		StringID str = STR_8812_EMPTY;

		if (num != 0) {
			SetDParam(0, v->cargo_type);
			SetDParam(1, num);
			SetDParam(2, v->cargo_source);
			str = STR_8813_FROM;
		}
		DrawString(x, y, str, 0);
	}
}

static void TrainDetailsInfoTab(const Vehicle *v, int x, int y)
{
	if (RailVehInfo(v->engine_type)->flags & RVI_WAGON) {
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(1, v->value);
		DrawString(x, y, STR_882D_VALUE, 0x10);
	} else {
		SetDParam(0, GetCustomEngineName(v->engine_type));
		SetDParam(1, v->build_year);
		SetDParam(2, v->value);
		DrawString(x, y, STR_882C_BUILT_VALUE, 0x10);
	}
}

static void TrainDetailsCapacityTab(const Vehicle *v, int x, int y)
{
	if (v->cargo_cap != 0) {
		SetDParam(0, _cargoc.names_long[v->cargo_type]);
		SetDParam(1, v->cargo_cap);
		DrawString(x, y, STR_013F_CAPACITY, 0);
	}
}


static void DrawTrainDetailsWindow(Window *w)
{
	byte det_tab = WP(w, traindetails_d).tab;
	const Vehicle* v;
	const Vehicle* u;
	AcceptedCargo act_cargo;
	AcceptedCargo max_cargo;
	uint i;
	int num;
	int x;
	int y;
	int sel;

	num = 0;
	u = v = GetVehicle(w->window_number);
	if (det_tab == 3) { // Total cargo tab
		for (i = 0; i < lengthof(act_cargo); i++) {
			act_cargo[i] = 0;
			max_cargo[i] = 0;
		}

		do {
			act_cargo[u->cargo_type] += u->cargo_count;
			max_cargo[u->cargo_type] += u->cargo_cap;
		} while ((u = GetNextVehicle(u)) != NULL);

		/* Set scroll-amount seperately from counting, as to not compute num double
		 * for more carriages of the same type
		 */
		for (i = 0; i != NUM_CARGO; i++) {
			if (max_cargo[i] > 0) num++; // only count carriages that the train has
		}
		num++; // needs one more because first line is description string
	} else {
		do {
			num++;
		} while ((u = GetNextVehicle(u)) != NULL);
	}

	SetVScrollCount(w, num);

	DisableWindowWidget(w, det_tab + 9);
	SetWindowWidgetDisabledState(w, 2, v->owner != _local_player);

	/* disable service-scroller when interval is set to disabled */
	SetWindowWidgetDisabledState(w, 6, !_patches.servint_trains);
	SetWindowWidgetDisabledState(w, 7, !_patches.servint_trains);

	SetDParam(0, v->string_id);
	SetDParam(1, v->unitnumber);
	DrawWindowWidgets(w);

	SetDParam(1, v->age / 366);

	x = 2;

	SetDParam(0, (v->age + 365 < v->max_age) ? STR_AGE : STR_AGE_RED);
	SetDParam(2, v->max_age / 366);
	SetDParam(3, GetTrainRunningCost(v) >> 8);
	DrawString(x, 15, STR_885D_AGE_RUNNING_COST_YR, 0);

	SetDParam(2, v->u.rail.cached_max_speed);
	SetDParam(1, v->u.rail.cached_power);
	SetDParam(0, v->u.rail.cached_weight);
	DrawString(x, 25, STR_VEHICLE_INFO_WEIGHT_POWER_MAX_SPEED, 0);

	SetDParam(0, v->profit_this_year);
	SetDParam(1, v->profit_last_year);
	DrawString(x, 35, STR_885F_PROFIT_THIS_YEAR_LAST_YEAR, 0);

	SetDParam(0, 100 * (v->reliability>>8) >> 8);
	SetDParam(1, v->breakdowns_since_last_service);
	DrawString(x, 45, STR_8860_RELIABILITY_BREAKDOWNS, 0);

	SetDParam(0, v->service_interval);
	SetDParam(1, v->date_of_last_service);
	DrawString(x + 11, 57 + (w->vscroll.cap * 14), _patches.servint_ispercent ? STR_SERVICING_INTERVAL_PERCENT : STR_883C_SERVICING_INTERVAL_DAYS, 0);

	y = 57;
	sel = w->vscroll.pos;

	// draw the first 3 details tabs
	if (det_tab != 3) {
		x = 1;
		for (;;) {
			if (--sel < 0 && sel >= -w->vscroll.cap) {
				int dx = 0;
				int px;
				int py;

				u = v;
				do {
					PalSpriteID pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
					DrawSprite(GetTrainImage(u, DIR_W) | pal, x + 14 + WagonLengthToPixels(dx), y + 6 + (is_custom_sprite(RailVehInfo(u->engine_type)->image_index) ? _traininfo_vehicle_pitch : 0));
					dx += u->u.rail.cached_veh_length;
					u = u->next;
				} while (u != NULL && IsArticulatedPart(u));

				px = x + WagonLengthToPixels(dx) + 2;
				py = y + 2;
				switch (det_tab) {
					default: NOT_REACHED();
					case 0: TrainDetailsCargoTab(   v, px, py); break;
					case 1: TrainDetailsInfoTab(    v, px, py); break;
					case 2: TrainDetailsCapacityTab(v, px, py); break;
				}
				y += 14;
			}
			v = GetNextVehicle(v);
			if (v == NULL) return;
		}
	} else {
		// draw total cargo tab
		DrawString(x, y + 2, STR_013F_TOTAL_CAPACITY_TEXT, 0);
		for (i = 0; i != NUM_CARGO; i++) {
			if (max_cargo[i] > 0 && --sel < 0 && sel > -w->vscroll.cap) {
				y += 14;
				SetDParam(0, i);            // {CARGO} #1
				SetDParam(1, act_cargo[i]); // {CARGO} #2
				SetDParam(2, i);            // {SHORTCARGO} #1
				SetDParam(3, max_cargo[i]); // {SHORTCARGO} #2
				DrawString(x, y + 2, STR_013F_TOTAL_CAPACITY, 0);
			}
		}
	}
}

static void TrainDetailsWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		DrawTrainDetailsWindow(w);
		break;
	case WE_CLICK: {
		int mod;
		const Vehicle *v;
		switch (e->we.click.widget) {
		case 2: /* name train */
			v = GetVehicle(w->window_number);
			SetDParam(0, v->unitnumber);
			ShowQueryString(v->string_id, STR_8865_NAME_TRAIN, 31, 150, w->window_class, w->window_number, CS_ALPHANUMERAL);
			break;
		case 6: /* inc serv interval */
			mod = _ctrl_pressed? 5 : 10;
			goto do_change_service_int;

		case 7: /* dec serv interval */
			mod = _ctrl_pressed? -5 : -10;
do_change_service_int:
			v = GetVehicle(w->window_number);

			mod = GetServiceIntervalClamped(mod + v->service_interval);
			if (mod == v->service_interval) return;

			DoCommandP(v->tile, v->index, mod, NULL, CMD_CHANGE_SERVICE_INT | CMD_MSG(STR_018A_CAN_T_CHANGE_SERVICING));
			break;
		/* details buttons*/
		case 9:  // Cargo
		case 10: // Information
		case 11: // Capacities
		case 12: // Total cargo
			EnableWindowWidget(w,  9);
			EnableWindowWidget(w, 10);
			EnableWindowWidget(w, 11);
			EnableWindowWidget(w, 12);
			EnableWindowWidget(w, e->we.click.widget);
			WP(w,traindetails_d).tab = e->we.click.widget - 9;
			SetWindowDirty(w);
			break;
		}
	} break;

	case WE_ON_EDIT_TEXT:
		if (e->we.edittext.str[0] != '\0') {
			_cmd_text = e->we.edittext.str;
			DoCommandP(0, w->window_number, 0, NULL,
				CMD_NAME_VEHICLE | CMD_MSG(STR_8866_CAN_T_NAME_TRAIN));
		}
		break;

	case WE_RESIZE:
		if (e->we.sizing.diff.y == 0) break;

		w->vscroll.cap += e->we.sizing.diff.y / 14;
		w->widget[4].data = (w->vscroll.cap << 8) + 1;
		break;
	}
}

static const Widget _train_details_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,   14,   0,  10,   0,  13, STR_00C5,             STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE,   14,  11, 329,   0,  13, STR_8802_DETAILS,     STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN, RESIZE_NONE,   14, 330, 369,   0,  13, STR_01AA_NAME,        STR_8867_NAME_TRAIN},
{      WWT_PANEL, RESIZE_NONE,   14,   0, 369,  14,  55, 0x0,                  STR_NULL},
{     WWT_MATRIX, RESIZE_BOTTOM, 14,   0, 357,  56, 139, 0x601,                STR_NULL},
{  WWT_SCROLLBAR, RESIZE_BOTTOM, 14, 358, 369,  56, 139, 0x0,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,   0,  10, 140, 145, STR_0188,             STR_884D_INCREASE_SERVICING_INTERVAL},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,   0,  10, 146, 151, STR_0189,             STR_884E_DECREASE_SERVICING_INTERVAL},
{      WWT_PANEL, RESIZE_TB,     14,  11, 369, 140, 151, 0x0,                  STR_NULL},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,   0,  89, 152, 163, STR_013C_CARGO,       STR_884F_SHOW_DETAILS_OF_CARGO_CARRIED},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14,  90, 178, 152, 163, STR_013D_INFORMATION, STR_8850_SHOW_DETAILS_OF_TRAIN_VEHICLES},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14, 179, 268, 152, 163, STR_013E_CAPACITIES,  STR_8851_SHOW_CAPACITIES_OF_EACH},
{ WWT_PUSHTXTBTN, RESIZE_TB,     14, 269, 357, 152, 163, STR_013E_TOTAL_CARGO, STR_8852_SHOW_TOTAL_CARGO},
{  WWT_RESIZEBOX, RESIZE_TB,     14, 358, 369, 152, 163, 0x0,                  STR_RESIZE_BUTTON},
{   WIDGETS_END},
};


static const WindowDesc _train_details_desc = {
	-1,-1, 370, 164,
	WC_VEHICLE_DETAILS,WC_VEHICLE_VIEW,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_train_details_widgets,
	TrainDetailsWndProc
};


static void ShowTrainDetailsWindow(const Vehicle *v)
{
	Window *w;
	VehicleID veh = v->index;

	DeleteWindowById(WC_VEHICLE_ORDERS, veh);
	DeleteWindowById(WC_VEHICLE_DETAILS, veh);

	_alloc_wnd_parent_num = veh;
	w = AllocateWindowDesc(&_train_details_desc);

	w->window_number = veh;
	w->caption_color = v->owner;
	w->vscroll.cap = 6;
	w->widget[4].data = (w->vscroll.cap << 8) + 1;

	w->resize.step_height = 14;
	w->resize.height = w->height - 14 * 2; /* Minimum of 4 wagons in the display */

	WP(w,traindetails_d).tab = 0;
}
