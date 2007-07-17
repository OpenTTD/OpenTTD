/* $Id$ */

/** @file build_vehicle_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "debug.h"
#include "functions.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "vehicle.h"
#include "articulated_vehicles.h"
#include "gfx.h"
#include "station.h"
#include "command.h"
#include "engine.h"
#include "player.h"
#include "depot.h"
#include "airport.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "date.h"
#include "strings.h"
#include "cargotype.h"
#include "group.h"
#include "road_map.h"


enum BuildVehicleWidgets {
	BUILD_VEHICLE_WIDGET_CLOSEBOX = 0,
	BUILD_VEHICLE_WIDGET_CAPTION,
	BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING,
	BUILD_VEHICLE_WIDGET_SORT_TEXT,
	BUILD_VEHICLE_WIDGET_SORT_DROPDOWN,
	BUILD_VEHICLE_WIDGET_LIST,
	BUILD_VEHICLE_WIDGET_SCROLLBAR,
	BUILD_VEHICLE_WIDGET_PANEL,
	BUILD_VEHICLE_WIDGET_BUILD,
	BUILD_VEHICLE_WIDGET_RENAME,
	BUILD_VEHICLE_WIDGET_RESIZE,
	BUILD_VEHICLE_WIDGET_END
};

static const Widget _build_vehicle_widgets[] = {
	{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW },
	{    WWT_CAPTION,  RESIZE_RIGHT,    14,    11,   239,     0,    13, 0x0,                     STR_018C_WINDOW_TITLE_DRAG_THIS },
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,     0,    80,    14,    25, STR_SORT_BY,             STR_SORT_ORDER_TIP},
	{      WWT_PANEL,  RESIZE_RIGHT,    14,    81,   227,    14,    25, 0x0,                     STR_SORT_CRITERIA_TIP},
	{    WWT_TEXTBTN,     RESIZE_LR,    14,   228,   239,    14,    25, STR_0225,                STR_SORT_CRITERIA_TIP},
	{     WWT_MATRIX,     RESIZE_RB,    14,     0,   227,    26,   121, 0x0,                     STR_NULL },
	{  WWT_SCROLLBAR,    RESIZE_LRB,    14,   228,   239,    26,   121, 0x0,                     STR_0190_SCROLL_BAR_SCROLLS_LIST },
	{      WWT_PANEL,    RESIZE_RTB,    14,     0,   239,   122,   243, 0x0,                     STR_NULL },

	{ WWT_PUSHTXTBTN,     RESIZE_TB,    14,     0,   114,   244,   255, 0x0,                     STR_NULL },
	{ WWT_PUSHTXTBTN,    RESIZE_RTB,    14,   115,   227,   244,   255, 0x0,                     STR_NULL },
	{  WWT_RESIZEBOX,   RESIZE_LRTB,    14,   228,   239,   244,   255, 0x0,                     STR_RESIZE_BUTTON },
	{   WIDGETS_END},
};

/* Setup widget strings to fit the different types of vehicles */
static void SetupWindowStrings(Window *w, byte type)
{
	switch (type) {
		case VEH_TRAIN:
			w->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = STR_JUST_STRING;
			w->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_8843_TRAIN_VEHICLE_SELECTION;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_881F_BUILD_VEHICLE;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_8844_BUILD_THE_HIGHLIGHTED_TRAIN;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_8820_RENAME;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_8845_RENAME_TRAIN_VEHICLE_TYPE;
			break;
		case VEH_ROAD:
			w->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = STR_9006_NEW_ROAD_VEHICLES;
			w->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_9026_ROAD_VEHICLE_SELECTION;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_9007_BUILD_VEHICLE;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_9027_BUILD_THE_HIGHLIGHTED_ROAD;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_9034_RENAME;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_9035_RENAME_ROAD_VEHICLE_TYPE;
			break;
		case VEH_SHIP:
			w->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = STR_9808_NEW_SHIPS;
			w->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_9825_SHIP_SELECTION_LIST_CLICK;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_9809_BUILD_SHIP;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_9826_BUILD_THE_HIGHLIGHTED_SHIP;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_9836_RENAME;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_9837_RENAME_SHIP_TYPE;
			break;
		case VEH_AIRCRAFT:
			w->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = STR_A005_NEW_AIRCRAFT;
			w->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_A025_AIRCRAFT_SELECTION_LIST;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_A006_BUILD_AIRCRAFT;
			w->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_A037_RENAME;
			w->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_A038_RENAME_AIRCRAFT_TYPE;
			break;
	}
}

static bool _internal_sort_order; // descending/ascending

static byte _last_sort_criteria[]    = {0, 0, 0, 0};
static bool _last_sort_order[]       = {false, false, false, false};

static int CDECL EngineNumberSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL EngineIntroDateSorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->intro_date;
	const int vb = GetEngine(*(const EngineID*)b)->intro_date;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineNameSorter(const void *a, const void *b)
{
	static EngineID last_engine[2] = { INVALID_ENGINE, INVALID_ENGINE };
	static char     last_name[2][64] = { "\0", "\0" };

	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r;

	if (va != last_engine[0]) {
		last_engine[0] = va;
		SetDParam(0, va);
		GetString(last_name[0], STR_ENGINE_NAME, lastof(last_name[0]));
	}

	if (vb != last_engine[1]) {
		last_engine[1] = vb;
		SetDParam(0, vb);
		GetString(last_name[1], STR_ENGINE_NAME, lastof(last_name[1]));
	}

	r = strcmp(last_name[0], last_name[1]); // sort by name

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineReliabilitySorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->reliability;
	const int vb = GetEngine(*(const EngineID*)b)->reliability;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

/* Train sorting functions */
static int CDECL TrainEngineCostSorter(const void *a, const void *b)
{
	int va = RailVehInfo(*(const EngineID*)a)->base_cost;
	int vb = RailVehInfo(*(const EngineID*)b)->base_cost;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineSpeedSorter(const void *a, const void *b)
{
	int va = RailVehInfo(*(const EngineID*)a)->max_speed;
	int vb = RailVehInfo(*(const EngineID*)b)->max_speed;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEnginePowerSorter(const void *a, const void *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*(const EngineID*)a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*(const EngineID*)b);

	int va = rvi_a->power << (rvi_a->railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);
	int vb = rvi_b->power << (rvi_b->railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineRunningCostSorter(const void *a, const void *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*(const EngineID*)a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*(const EngineID*)b);

	Money va = rvi_a->running_cost_base * _price.running_rail[rvi_a->running_cost_class] * (rvi_a->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	Money vb = rvi_b->running_cost_base * _price.running_rail[rvi_b->running_cost_class] * (rvi_b->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int r = ClampToI32(va - vb);

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
	Money va = (rvi_a->running_cost_base * _price.running_rail[rvi_a->running_cost_class]) / max((uint16)1, rvi_a->power);
	Money vb = (rvi_b->running_cost_base * _price.running_rail[rvi_b->running_cost_class]) / max((uint16)1, rvi_b->power);
	int r = ClampToI32(vb - va);

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineNumberSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r = ListPositionOfEngine(va) - ListPositionOfEngine(vb);

	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineCapacitySorter(const void *a, const void *b)
{
	int va = RailVehInfo(*(const EngineID*)a)->capacity;
	int vb = RailVehInfo(*(const EngineID*)b)->capacity;
	int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEnginesThenWagonsSorter(const void *a, const void *b)
{
	EngineID va = *(const EngineID*)a;
	EngineID vb = *(const EngineID*)b;
	int val_a = (RailVehInfo(va)->railveh_type == RAILVEH_WAGON ? 1 : 0);
	int val_b = (RailVehInfo(vb)->railveh_type == RAILVEH_WAGON ? 1 : 0);
	int r = val_a - val_b;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);

	return _internal_sort_order ? -r : r;
}

static int CDECL RoadVehEngineCapacitySorter(const void *a, const void *b)
{
	int va = RoadVehInfo(*(const EngineID*)a)->capacity;
	int vb = RoadVehInfo(*(const EngineID*)b)->capacity;
	int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL ShipEngineCapacitySorter(const void *a, const void *b)
{
	int va = ShipVehInfo(*(const EngineID*)a)->capacity;
	int vb = ShipVehInfo(*(const EngineID*)b)->capacity;
	int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

/* Aircraft sorting functions */

static int CDECL AircraftEngineCostSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->base_cost;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->base_cost;
	int r = va - vb;

	return _internal_sort_order ? -r : r;
}

static int CDECL AircraftEngineSpeedSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->max_speed;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->max_speed;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL AircraftEngineRunningCostSorter(const void *a, const void *b)
{
	const int va = AircraftVehInfo(*(const EngineID*)a)->running_cost;
	const int vb = AircraftVehInfo(*(const EngineID*)b)->running_cost;
	const int r = va - vb;

	if (r == 0) {
		/* Use EngineID to sort instead since we want consistent sorting */
		return EngineNumberSorter(a, b);
	}
	return _internal_sort_order ? -r : r;
}

static int CDECL AircraftEngineCargoSorter(const void *a, const void *b)
{
	int va = AircraftVehInfo(*(const EngineID*)a)->passenger_capacity;
	int vb = AircraftVehInfo(*(const EngineID*)b)->passenger_capacity;
	int r = va - vb;

	if (r == 0) {
		/* The planes has the same passenger capacity. Check mail capacity instead */
		va = AircraftVehInfo(*(const EngineID*)a)->mail_capacity;
		vb = AircraftVehInfo(*(const EngineID*)b)->mail_capacity;
		r = va - vb;

		if (r == 0) {
			/* Use EngineID to sort instead since we want consistent sorting */
			return EngineNumberSorter(a, b);
		}
	}
	return _internal_sort_order ? -r : r;
}

static EngList_SortTypeFunction * const _sorter[][10] = {{
	/* Trains */
	&TrainEngineNumberSorter,
	&TrainEngineCostSorter,
	&TrainEngineSpeedSorter,
	&TrainEnginePowerSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&TrainEngineRunningCostSorter,
	&TrainEnginePowerVsRunningCostSorter,
	&EngineReliabilitySorter,
	&TrainEngineCapacitySorter,
}, {
	/* Road vehicles */
	&EngineNumberSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineReliabilitySorter,
	&RoadVehEngineCapacitySorter,
}, {
	/* Ships */
	&EngineNumberSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineReliabilitySorter,
	&ShipEngineCapacitySorter,
}, {
	/* Aircraft */
	&EngineNumberSorter,
	&AircraftEngineCostSorter,
	&AircraftEngineSpeedSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&AircraftEngineRunningCostSorter,
	&EngineReliabilitySorter,
	&AircraftEngineCargoSorter,
}};

static const StringID _sort_listing[][11] = {{
	/* Trains */
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_ENGINE_SORT_POWER,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_ENGINE_SORT_RUNNING_COST,
	STR_ENGINE_SORT_POWER_VS_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_ENGINE_SORT_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Road vehicles */
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_RELIABILITY,
	STR_ENGINE_SORT_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Ships */
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_SORT_BY_RELIABILITY,
	STR_ENGINE_SORT_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Aircraft */
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_ENGINE_SORT_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_ENGINE_SORT_CARGO_CAPACITY,
	INVALID_STRING_ID
}};

/* Draw rail wagon specific details */
static int DrawRailWagonPurchaseInfo(int x, int y, EngineID engine_number, const RailVehicleInfo *rvi)
{
	/* Purchase cost */
	SetDParam(0, (GetEngineProperty(engine_number, 0x17, rvi->base_cost) * _price.build_railwagon) >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_COST, 0);
	y += 10;

	/* Wagon weight - (including cargo) */
	uint weight = GetEngineProperty(engine_number, 0x16, rvi->weight);
	SetDParam(0, weight);
	SetDParam(1, (GetCargo(rvi->cargo_type)->weight * GetEngineProperty(engine_number, 0x14, rvi->capacity) >> 4) + weight);
	DrawString(x, y, STR_PURCHASE_INFO_WEIGHT_CWEIGHT, 0);
	y += 10;

	/* Wagon speed limit, displayed if above zero */
	if (_patches.wagon_speed_limits) {
		uint max_speed = GetEngineProperty(engine_number, 0x09, rvi->max_speed);
		if (max_speed > 0) {
			SetDParam(0, max_speed * 10 / 16);
			DrawString(x, y, STR_PURCHASE_INFO_SPEED, 0);
			y += 10;
		}
	}
	return y;
}

/* Draw locomotive specific details */
static int DrawRailEnginePurchaseInfo(int x, int y, EngineID engine_number, const RailVehicleInfo *rvi)
{
	int multihead = (rvi->railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);
	uint weight = GetEngineProperty(engine_number, 0x16, rvi->weight);

	/* Purchase Cost - Engine weight */
	SetDParam(0, GetEngineProperty(engine_number, 0x17, rvi->base_cost) * (_price.build_railvehicle >> 3) >> 5);
	SetDParam(1, weight << multihead);
	DrawString(x, y, STR_PURCHASE_INFO_COST_WEIGHT, 0);
	y += 10;

	/* Max speed - Engine power */
	SetDParam(0, GetEngineProperty(engine_number, 0x09, rvi->max_speed) * 10 / 16);
	SetDParam(1, GetEngineProperty(engine_number, 0x0B, rvi->power) << multihead);
	DrawString(x, y, STR_PURCHASE_INFO_SPEED_POWER, 0);
	y += 10;

	/* Max tractive effort - not applicable if old acceleration or maglev */
	if (_patches.realistic_acceleration && rvi->railtype != RAILTYPE_MAGLEV) {
		SetDParam(0, ((weight << multihead) * 10 * GetEngineProperty(engine_number, 0x1F, rvi->tractive_effort)) / 256);
		DrawString(x, y, STR_PURCHASE_INFO_MAX_TE, 0);
		y += 10;
	}

	/* Running cost */
	SetDParam(0, (GetEngineProperty(engine_number, 0x0D, rvi->running_cost_base) * _price.running_rail[rvi->running_cost_class] >> 8) << multihead);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Powered wagons power - Powered wagons extra weight */
	if (rvi->pow_wag_power != 0) {
		SetDParam(0, rvi->pow_wag_power);
		SetDParam(1, rvi->pow_wag_weight);
		DrawString(x, y, STR_PURCHASE_INFO_PWAGPOWER_PWAGWEIGHT, 0);
		y += 10;
	};

	return y;
}

/* Draw road vehicle specific details */
static int DrawRoadVehPurchaseInfo(int x, int y, EngineID engine_number, const RoadVehicleInfo *rvi)
{
	bool refittable = (EngInfo(engine_number)->refit_mask != 0);

	/* Purchase cost - Max speed */
	SetDParam(0, GetEngineProperty(engine_number, 0x11, rvi->base_cost) * (_price.roadveh_base >> 3) >> 5);
	SetDParam(1, rvi->max_speed * 10 / 32);
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Running cost */
	SetDParam(0, rvi->running_cost * _price.roadveh_running >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	/* Cargo type + capacity */
	SetDParam(0, rvi->cargo_type);
	SetDParam(1, GetEngineProperty(engine_number, 0x0F, rvi->capacity));
	SetDParam(2, refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
	y += 10;

	return y;
}

/* Draw ship specific details */
static int DrawShipPurchaseInfo(int x, int y, EngineID engine_number, const ShipVehicleInfo *svi)
{
	/* Purchase cost - Max speed */
	SetDParam(0, GetEngineProperty(engine_number, 0x0A, svi->base_cost) * (_price.ship_base >> 3) >> 5);
	SetDParam(1, GetEngineProperty(engine_number, 0x0B, svi->max_speed) * 10 / 32);
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Cargo type + capacity */
	SetDParam(0, svi->cargo_type);
	SetDParam(1, GetEngineProperty(engine_number, 0x0D, svi->capacity));
	SetDParam(2, svi->refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
	y += 10;

	/* Running cost */
	SetDParam(0, GetEngineProperty(engine_number, 0x0F, svi->running_cost) * _price.ship_running >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	return y;
}

/* Draw aircraft specific details */
static int DrawAircraftPurchaseInfo(int x, int y, EngineID engine_number, const AircraftVehicleInfo *avi)
{
	CargoID cargo;

	/* Purchase cost - Max speed */
	SetDParam(0, GetEngineProperty(engine_number, 0x0B, avi->base_cost) * (_price.aircraft_base >> 3) >> 5);
	SetDParam(1, avi->max_speed * 10 / 16);
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, 0);
	y += 10;

	/* Cargo capacity */
	cargo = FindFirstRefittableCargo(engine_number);
	if (cargo == CT_INVALID || cargo == CT_PASSENGERS) {
		SetDParam(0, avi->passenger_capacity);
		SetDParam(1, avi->mail_capacity);
		DrawString(x, y, STR_PURCHASE_INFO_AIRCRAFT_CAPACITY, 0);
	} else {
		/* Note, if the default capacity is selected by the refit capacity
		* callback, then the capacity shown is likely to be incorrect. */
		SetDParam(0, cargo);
		SetDParam(1, AircraftDefaultCargoCapacity(cargo, avi));
		SetDParam(2, STR_9842_REFITTABLE);
		DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
	}
	y += 10;

	/* Running cost */
	SetDParam(0, GetEngineProperty(engine_number, 0x0E, avi->running_cost) * _price.aircraft_running >> 8);
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, 0);
	y += 10;

	return y;
}

/**
 * Draw the purchase info details of a vehicle at a given location.
 * @param x,y location where to draw the info
 * @param w how wide are the text allowed to be (size of widget/window to Draw in)
 * @param engine_number the engine of which to draw the info of
 * @return y after drawing all the text
 */
int DrawVehiclePurchaseInfo(int x, int y, uint w, EngineID engine_number)
{
	const Engine *e = GetEngine(engine_number);
	YearMonthDay ymd;
	ConvertDateToYMD(e->intro_date, &ymd);
	bool refitable = false;

	switch (e->type) {
		default: NOT_REACHED();
		case VEH_TRAIN: {
			const RailVehicleInfo *rvi = RailVehInfo(engine_number);
			uint capacity = GetEngineProperty(engine_number, 0x14, rvi->capacity);

			refitable = (EngInfo(engine_number)->refit_mask != 0) && (capacity > 0);

			if (rvi->railveh_type == RAILVEH_WAGON) {
				y = DrawRailWagonPurchaseInfo(x, y, engine_number, rvi);
			} else {
				y = DrawRailEnginePurchaseInfo(x, y, engine_number, rvi);
			}

			/* Cargo type + capacity, or N/A */
			if (rvi->capacity == 0) {
				SetDParam(0, CT_INVALID);
				SetDParam(2, STR_EMPTY);
			} else {
				int multihead = (rvi->railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);

				SetDParam(0, rvi->cargo_type);
				SetDParam(1, (capacity * (CountArticulatedParts(engine_number) + 1)) << multihead);
				SetDParam(2, refitable ? STR_9842_REFITTABLE : STR_EMPTY);
			}
			DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, 0);
			y += 10;
		}
			break;
		case VEH_ROAD:
			y = DrawRoadVehPurchaseInfo(x, y, engine_number, RoadVehInfo(engine_number));
			refitable = true;
			break;
		case VEH_SHIP: {
			const ShipVehicleInfo *svi = ShipVehInfo(engine_number);
			y = DrawShipPurchaseInfo(x, y, engine_number, svi);
			refitable = svi->refittable;
		} break;
		case VEH_AIRCRAFT:
			y = DrawAircraftPurchaseInfo(x, y, engine_number, AircraftVehInfo(engine_number));
			refitable = true;
			break;
	}

	/* Draw details, that applies to all types except rail wagons */
	if (e->type != VEH_TRAIN || RailVehInfo(engine_number)->railveh_type != RAILVEH_WAGON) {
		/* Design date - Life length */
		SetDParam(0, ymd.year);
		SetDParam(1, e->lifelength);
		DrawString(x, y, STR_PURCHASE_INFO_DESIGNED_LIFE, 0);
		y += 10;

		/* Reliability */
		SetDParam(0, e->reliability * 100 >> 16);
		DrawString(x, y, STR_PURCHASE_INFO_RELIABILITY, 0);
		y += 10;
	}

	/* Additional text from NewGRF */
	y += ShowAdditionalText(x, y, w, engine_number);
	if (refitable) y += ShowRefitOptionsList(x, y, w, engine_number);

	return y;
}

/* Figure out what train EngineIDs to put in the list */
static void GenerateBuildTrainList(Window *w)
{
	EngineID eid, sel_id;
	int num_engines = 0;
	int num_wagons  = 0;
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	bv->filter.railtype = (w->window_number <= VEH_END) ? RAILTYPE_END : GetRailType(w->window_number);

	EngList_RemoveAll(&bv->eng_list);

	/* Make list of all available train engines and wagons.
	 * Also check to see if the previously selected engine is still available,
	 * and if not, reset selection to INVALID_ENGINE. This could be the case
	 * when engines become obsolete and are removed */
	for (sel_id = INVALID_ENGINE, eid = 0; eid < NUM_TRAIN_ENGINES; eid++) {
		const RailVehicleInfo *rvi = RailVehInfo(eid);

		if (bv->filter.railtype != RAILTYPE_END && !HasPowerOnRail(rvi->railtype, bv->filter.railtype)) continue;
		if (!IsEngineBuildable(eid, VEH_TRAIN, _local_player)) continue;

		EngList_Add(&bv->eng_list, eid);
		if (rvi->railveh_type != RAILVEH_WAGON) {
			num_engines++;
		} else {
			num_wagons++;
		}

		if (eid == bv->sel_engine) sel_id = eid;
	}

	bv->sel_engine = sel_id;

	/* make engines first, and then wagons, sorted by ListPositionOfEngine() */
	_internal_sort_order = false;
	EngList_Sort(&bv->eng_list, TrainEnginesThenWagonsSorter);

	/* and then sort engines */
	_internal_sort_order = bv->descending_sort_order;
	EngList_SortPartial(&bv->eng_list, _sorter[0][bv->sort_criteria], 0, num_engines);

	/* and finally sort wagons */
	EngList_SortPartial(&bv->eng_list, _sorter[0][bv->sort_criteria], num_engines, num_wagons);
}

/* Figure out what road vehicle EngineIDs to put in the list */
static void GenerateBuildRoadVehList(Window *w)
{
	EngineID eid, sel_id;
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	EngList_RemoveAll(&bv->eng_list);

	sel_id = INVALID_ENGINE;

	for (eid = ROAD_ENGINES_INDEX; eid < ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES; eid++) {
		if (!IsEngineBuildable(eid, VEH_ROAD, _local_player)) continue;
		if (!HASBIT(bv->filter.roadtypes, HASBIT(EngInfo(eid)->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD)) continue;
		EngList_Add(&bv->eng_list, eid);

		if (eid == bv->sel_engine) sel_id = eid;
	}
	bv->sel_engine = sel_id;
}

/* Figure out what ship EngineIDs to put in the list */
static void GenerateBuildShipList(Window *w)
{
	EngineID eid, sel_id;
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	EngList_RemoveAll(&bv->eng_list);

	sel_id = INVALID_ENGINE;

	for (eid = SHIP_ENGINES_INDEX; eid < SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES; eid++) {
		if (!IsEngineBuildable(eid, VEH_SHIP, _local_player)) continue;
		EngList_Add(&bv->eng_list, eid);

		if (eid == bv->sel_engine) sel_id = eid;
	}
	bv->sel_engine = sel_id;
}

/* Figure out what aircraft EngineIDs to put in the list */
static void GenerateBuildAircraftList(Window *w)
{
	EngineID eid, sel_id;
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	EngList_RemoveAll(&bv->eng_list);

	/* Make list of all available planes.
	 * Also check to see if the previously selected plane is still available,
	 * and if not, reset selection to INVALID_ENGINE. This could be the case
	 * when planes become obsolete and are removed */
	sel_id = INVALID_ENGINE;
	for (eid = AIRCRAFT_ENGINES_INDEX; eid < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; eid++) {
		if (!IsEngineBuildable(eid, VEH_AIRCRAFT, _local_player)) continue;
		/* First VEH_END window_numbers are fake to allow a window open for all different types at once */
		if (w->window_number > VEH_END && !IsAircraftBuildableAtStation(eid, w->window_number)) continue;

		EngList_Add(&bv->eng_list, eid);
		if (eid == bv->sel_engine) sel_id = eid;
	}

	bv->sel_engine = sel_id;
}

/* Generate the list of vehicles */
static void GenerateBuildList(Window *w)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	switch (bv->vehicle_type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			GenerateBuildTrainList(w);
			return; // trains should not reach the last sorting
		case VEH_ROAD:
			GenerateBuildRoadVehList(w);
			break;
		case VEH_SHIP:
			GenerateBuildShipList(w);
			break;
		case VEH_AIRCRAFT:
			GenerateBuildAircraftList(w);
			break;
	}
	_internal_sort_order = bv->descending_sort_order;
	EngList_Sort(&bv->eng_list, _sorter[bv->vehicle_type][bv->sort_criteria]);
}

static void DrawVehicleEngine(byte type, int x, int y, EngineID engine, SpriteID pal)
{
	switch (type) {
		case VEH_TRAIN:    DrawTrainEngine(   x, y, engine, pal); break;
		case VEH_ROAD:     DrawRoadVehEngine( x, y, engine, pal); break;
		case VEH_SHIP:     DrawShipEngine(    x, y, engine, pal); break;
		case VEH_AIRCRAFT: DrawAircraftEngine(x, y, engine, pal); break;
		default: NOT_REACHED();
	}
}

/** Engine drawing loop
 * @param type Type of vehicle (VEH_*)
 * @param x,y Where should the list start
 * @param eng_list What engines to draw
 * @param min where to start in the list
 * @param max where in the list to end
 * @param selected_id what engine to highlight as selected, if any
 * @param show_count Display the number of vehicles (used by autoreplace)
 */
void DrawEngineList(VehicleType type, int x, int y, const EngineList eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group)
{
	byte step_size = GetVehicleListHeight(type);
	byte x_offset = 0;
	byte y_offset = 0;

	assert(max <= EngList_Count(&eng_list));

	switch (type) {
		case VEH_TRAIN:
			x++; // train and road vehicles use the same offset, except trains are one more pixel to the right
			/* Fallthough */
		case VEH_ROAD:
			x += 26;
			x_offset = 30;
			y += 2;
			y_offset = 4;
			break;
		case VEH_SHIP:
			x += 35;
			x_offset = 40;
			y += 7;
			y_offset = 3;
			break;
		case VEH_AIRCRAFT:
			x += 27;
			x_offset = 33;
			y += 7;
			y_offset = 3;
			break;
		default: NOT_REACHED();
	}

	for (; min < max; min++, y += step_size) {
		const EngineID engine = eng_list[min];
		const uint num_engines = GetGroupNumEngines(selected_group, engine);

		SetDParam(0, engine);
		DrawString(x + x_offset, y, STR_ENGINE_NAME, engine == selected_id ? 0xC : 0x10);
		DrawVehicleEngine(type, x, y + y_offset, engine, (show_count && num_engines == 0) ? PALETTE_CRASH : GetEnginePalette(engine, _local_player));
		if (show_count) {
			SetDParam(0, num_engines);
			DrawStringRightAligned(213, y + (GetVehicleListHeight(type) == 14 ? 3 : 8), STR_TINY_BLACK, 0);
		}
	}
}

static void ExpandPurchaseInfoWidget(Window *w, int expand_by)
{
	Widget *wi = &w->widget[BUILD_VEHICLE_WIDGET_PANEL];

	SetWindowDirty(w);
	wi->bottom += expand_by;

	for (uint i = BUILD_VEHICLE_WIDGET_BUILD; i < BUILD_VEHICLE_WIDGET_END; i++) {
		wi = &w->widget[i];
		wi->top += expand_by;
		wi->bottom += expand_by;
	}

	w->height += expand_by;
	SetWindowDirty(w);
}

static void DrawBuildVehicleWindow(Window *w)
{
	const buildvehicle_d *bv = &WP(w, buildvehicle_d);
	uint max = min(w->vscroll.pos + w->vscroll.cap, EngList_Count(&bv->eng_list));

	SetWindowWidgetDisabledState(w, BUILD_VEHICLE_WIDGET_BUILD, w->window_number <= VEH_END);

	SetVScrollCount(w, EngList_Count(&bv->eng_list));
	SetDParam(0, bv->filter.railtype + STR_881C_NEW_RAIL_VEHICLES); // This should only affect rail vehicles
	DrawWindowWidgets(w);

	DrawEngineList(bv->vehicle_type, 2, 27, bv->eng_list, w->vscroll.pos, max, bv->sel_engine, false, DEFAULT_GROUP);

	if (bv->sel_engine != INVALID_ENGINE) {
		const Widget *wi = &w->widget[BUILD_VEHICLE_WIDGET_PANEL];
		int text_end = DrawVehiclePurchaseInfo(2, wi->top + 1, wi->right - wi->left - 2, bv->sel_engine);

		if (text_end > wi->bottom) ExpandPurchaseInfoWidget(w, text_end - wi->bottom);
	}

	DrawString(85, 15, _sort_listing[bv->vehicle_type][bv->sort_criteria], 0x10);
	DoDrawString(bv->descending_sort_order ? DOWNARROW : UPARROW, 69, 15, 0x10);
}

static void BuildVehicleClickEvent(Window *w, WindowEvent *e)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	switch (e->we.click.widget) {
		case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING:
			bv->descending_sort_order ^= true;
			_last_sort_order[bv->vehicle_type] = bv->descending_sort_order;
			bv->regenerate_list = true;
			SetWindowDirty(w);
			break;

		case BUILD_VEHICLE_WIDGET_LIST: {
			uint i = (e->we.click.pt.y - 26) / GetVehicleListHeight(bv->vehicle_type) + w->vscroll.pos;
			uint num_items = EngList_Count(&bv->eng_list);
			bv->sel_engine = (i < num_items) ? bv->eng_list[i] : INVALID_ENGINE;
			SetWindowDirty(w);
			break;
		}

		case BUILD_VEHICLE_WIDGET_SORT_TEXT: case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN: // Select sorting criteria dropdown menu
			ShowDropDownMenu(w, _sort_listing[bv->vehicle_type], bv->sort_criteria, BUILD_VEHICLE_WIDGET_SORT_DROPDOWN, 0, 0);
			break;

		case BUILD_VEHICLE_WIDGET_BUILD: {
			EngineID sel_eng = bv->sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				switch (bv->vehicle_type) {
					default: NOT_REACHED();
					case VEH_TRAIN:
						DoCommandP(w->window_number, sel_eng, 0, (RailVehInfo(sel_eng)->railveh_type == RAILVEH_WAGON) ? CcBuildWagon : CcBuildLoco,
								   CMD_BUILD_RAIL_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE));
						break;
					case VEH_ROAD:
						DoCommandP(w->window_number, sel_eng, 0, CcBuildRoadVeh, CMD_BUILD_ROAD_VEH | CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE));
						break;
					case VEH_SHIP:
						DoCommandP(w->window_number, sel_eng, 0, CcBuildShip, CMD_BUILD_SHIP | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP));
						break;
					case VEH_AIRCRAFT:
						DoCommandP(w->window_number, sel_eng, 0, CcBuildAircraft, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT));
						break;
				}
			}
			break;
		}

		case BUILD_VEHICLE_WIDGET_RENAME: {
			EngineID sel_eng = bv->sel_engine;
			if (sel_eng != INVALID_ENGINE) {
				StringID str = STR_NULL;

				bv->rename_engine = sel_eng;
				switch (bv->vehicle_type) {
					default: NOT_REACHED();
					case VEH_TRAIN:    str = STR_886A_RENAME_TRAIN_VEHICLE_TYPE; break;
					case VEH_ROAD:     str = STR_9036_RENAME_ROAD_VEHICLE_TYPE;  break;
					case VEH_SHIP:     str = STR_9838_RENAME_SHIP_TYPE;          break;
					case VEH_AIRCRAFT: str = STR_A039_RENAME_AIRCRAFT_TYPE;      break;
				}
				SetDParam(0, sel_eng);
				ShowQueryString(STR_ENGINE_NAME, str, 31, 160, w, CS_ALPHANUMERAL);
			}
			break;
		}
	}
}

static void NewVehicleWndProc(Window *w, WindowEvent *e)
{
	buildvehicle_d *bv = &WP(w, buildvehicle_d);

	switch (e->event) {
		case WE_INVALIDATE_DATA:
			bv->regenerate_list = true;
			SetWindowDirty(w);
			break;

		case WE_DESTROY:
			EngList_Destroy(&bv->eng_list);
			break;

		case WE_PAINT:
			if (bv->regenerate_list) {
				bv->regenerate_list = false;
				GenerateBuildList(w);
			}
			DrawBuildVehicleWindow(w);
			break;

		case WE_CLICK:
			BuildVehicleClickEvent(w, e);
			break;

		case WE_DOUBLE_CLICK:
			if (e->we.click.widget == BUILD_VEHICLE_WIDGET_LIST) {
				/* When double clicking, we want to buy a vehicle */
				e->we.click.widget = BUILD_VEHICLE_WIDGET_BUILD;
				BuildVehicleClickEvent(w, e);
			}
			break;

		case WE_ON_EDIT_TEXT: {
			if (e->we.edittext.str[0] != '\0') {
				StringID str = STR_NULL;
				_cmd_text = e->we.edittext.str;
				switch (bv->vehicle_type) {
					default: NOT_REACHED();
					case VEH_TRAIN:    str = STR_886B_CAN_T_RENAME_TRAIN_VEHICLE; break;
					case VEH_ROAD:     str = STR_9037_CAN_T_RENAME_ROAD_VEHICLE;  break;
					case VEH_SHIP:     str = STR_9839_CAN_T_RENAME_SHIP_TYPE;     break;
					case VEH_AIRCRAFT: str = STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE; break;
				}
				DoCommandP(0, bv->rename_engine, 0, NULL, CMD_RENAME_ENGINE | CMD_MSG(str));
			}
			break;
		}

		case WE_DROPDOWN_SELECT: // we have selected a dropdown item in the list
			if (bv->sort_criteria != e->we.dropdown.index) {
				bv->sort_criteria = e->we.dropdown.index;
				_last_sort_criteria[bv->vehicle_type] = bv->sort_criteria;
				bv->regenerate_list = true;
			}
			SetWindowDirty(w);
			break;

		case WE_RESIZE:
			if (e->we.sizing.diff.x != 0) ResizeButtons(w, BUILD_VEHICLE_WIDGET_BUILD, BUILD_VEHICLE_WIDGET_RENAME);
			if (e->we.sizing.diff.y == 0) break;

			w->vscroll.cap += e->we.sizing.diff.y / (int)GetVehicleListHeight(bv->vehicle_type);
			w->widget[BUILD_VEHICLE_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;
			break;
	}
}

static const WindowDesc _build_vehicle_desc = {
	WDP_AUTO, WDP_AUTO, 240, 256,
	WC_BUILD_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_build_vehicle_widgets,
	NewVehicleWndProc
};

void ShowBuildVehicleWindow(TileIndex tile, VehicleType type)
{
	buildvehicle_d *bv;
	Window *w;
	/* We want to be able to open both Available Train as Available Ships,
	 *  so if tile == 0 (Available XXX Window), use 'type' as unique number.
	 *  As it always is a low value, it won't collide with any real tile
	 *  number. */
	uint num = (tile == 0) ? (int)type : tile;

	assert(IsPlayerBuildableVehicleType(type));

	DeleteWindowById(WC_BUILD_VEHICLE, num);

	w = AllocateWindowDescFront(&_build_vehicle_desc, num);

	if (w == NULL) return;

	w->caption_color = (tile != 0) ? GetTileOwner(tile) : _local_player;
	w->resize.step_height = GetVehicleListHeight(type);
	w->vscroll.cap = w->resize.step_height == 24 ? 4 : 8;
	w->widget[BUILD_VEHICLE_WIDGET_LIST].data = (w->vscroll.cap << 8) + 1;

	bv = &WP(w, buildvehicle_d);
	EngList_Create(&bv->eng_list);
	bv->sel_engine      = INVALID_ENGINE;

	bv->vehicle_type    = type;
	bv->regenerate_list = false;

	bv->sort_criteria         = _last_sort_criteria[type];
	bv->descending_sort_order = _last_sort_order[type];

	switch (type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			WP(w, buildvehicle_d).filter.railtype = (tile == 0) ? RAILTYPE_END : GetRailType(tile);
			ResizeWindow(w, 0, 16);
			break;
		case VEH_ROAD:
			WP(w, buildvehicle_d).filter.roadtypes = (tile == 0) ? ROADTYPES_ALL : GetRoadTypes(tile);
			ResizeWindow(w, 0, 16);
		case VEH_SHIP:
			break;
		case VEH_AIRCRAFT:
			bv->filter.flags =
				tile == 0 ? AirportFTAClass::ALL : GetStationByTile(tile)->Airport()->flags;
			break;
	}
	SetupWindowStrings(w, type);
	ResizeButtons(w, BUILD_VEHICLE_WIDGET_BUILD, BUILD_VEHICLE_WIDGET_RENAME);

	w->resize.width  = w->width;
	w->resize.height = w->height;

	GenerateBuildList(w); // generate the list, since we need it in the next line
	/* Select the first engine in the list as default when opening the window */
	if (EngList_Count(&bv->eng_list) > 0) bv->sel_engine = bv->eng_list[0];
}
