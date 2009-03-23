/* $Id$ */

/** @file build_vehicle_gui.cpp GUI for building vehicles. */

#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "articulated_vehicles.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "group.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "gfx_func.h"
#include "widgets/dropdown_func.h"
#include "window_gui.h"
#include "engine_gui.h"
#include "settings_type.h"

#include "table/sprites.h"
#include "table/strings.h"

enum BuildVehicleWidgets {
	BUILD_VEHICLE_WIDGET_CLOSEBOX = 0,
	BUILD_VEHICLE_WIDGET_CAPTION,
	BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING,
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
	{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW },
	{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   239,     0,    13, 0x0,                     STR_018C_WINDOW_TITLE_DRAG_THIS },
	{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    80,    14,    25, STR_SORT_BY,             STR_SORT_ORDER_TIP},
	{   WWT_DROPDOWN,  RESIZE_RIGHT,  COLOUR_GREY,    81,   239,    14,    25, 0x0,                     STR_SORT_CRITERIA_TIP},
	{     WWT_MATRIX,     RESIZE_RB,  COLOUR_GREY,     0,   227,    26,    39, 0x101,                   STR_NULL },
	{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   228,   239,    26,    39, 0x0,                     STR_0190_SCROLL_BAR_SCROLLS_LIST },
	{      WWT_PANEL,    RESIZE_RTB,  COLOUR_GREY,     0,   239,    40,   161, 0x0,                     STR_NULL },

	{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_GREY,     0,   114,   162,   173, 0x0,                     STR_NULL },
	{ WWT_PUSHTXTBTN,    RESIZE_RTB,  COLOUR_GREY,   115,   227,   162,   173, 0x0,                     STR_NULL },
	{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   228,   239,   162,   173, 0x0,                     STR_RESIZE_BUTTON },
	{   WIDGETS_END},
};


static bool _internal_sort_order; // descending/ascending
static byte _last_sort_criteria[]    = {0, 0, 0, 0};
static bool _last_sort_order[]       = {false, false, false, false};

static int CDECL EngineNumberSorter(const void *a, const void *b)
{
	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;
	int r = ListPositionOfEngine(va) - ListPositionOfEngine(vb);

	return _internal_sort_order ? -r : r;
}

static int CDECL EngineIntroDateSorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->intro_date;
	const int vb = GetEngine(*(const EngineID*)b)->intro_date;
	const int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineNameSorter(const void *a, const void *b)
{
	static EngineID last_engine[2] = { INVALID_ENGINE, INVALID_ENGINE };
	static char     last_name[2][64] = { "\0", "\0" };

	const EngineID va = *(const EngineID*)a;
	const EngineID vb = *(const EngineID*)b;

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

	int r = strcmp(last_name[0], last_name[1]); // sort by name

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineReliabilitySorter(const void *a, const void *b)
{
	const int va = GetEngine(*(const EngineID*)a)->reliability;
	const int vb = GetEngine(*(const EngineID*)b)->reliability;
	const int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineCostSorter(const void *a, const void *b)
{
	Money va = GetEngine(*(const EngineID*)a)->GetCost();
	Money vb = GetEngine(*(const EngineID*)b)->GetCost();
	int r = ClampToI32(va - vb);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineSpeedSorter(const void *a, const void *b)
{
	int va = GetEngine(*(const EngineID*)a)->GetDisplayMaxSpeed();
	int vb = GetEngine(*(const EngineID*)b)->GetDisplayMaxSpeed();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL EnginePowerSorter(const void *a, const void *b)
{
	int va = GetEngine(*(const EngineID*)a)->GetPower();
	int vb = GetEngine(*(const EngineID*)b)->GetPower();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL EngineRunningCostSorter(const void *a, const void *b)
{
	Money va = GetEngine(*(const EngineID*)a)->GetRunningCost();
	Money vb = GetEngine(*(const EngineID*)b)->GetRunningCost();
	int r = ClampToI32(va - vb);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/* Train sorting functions */
static int CDECL TrainEnginePowerVsRunningCostSorter(const void *a, const void *b)
{
	const Engine *e_a = GetEngine(*(const EngineID*)a);
	const Engine *e_b = GetEngine(*(const EngineID*)b);

	/* Here we are using a few tricks to get the right sort.
	 * We want power/running cost, but since we usually got higher running cost than power and we store the result in an int,
	 * we will actually calculate cunning cost/power (to make it more than 1).
	 * Because of this, the return value have to be reversed as well and we return b - a instead of a - b.
	 * Another thing is that both power and running costs should be doubled for multiheaded engines.
	 * Since it would be multipling with 2 in both numerator and denumerator, it will even themselves out and we skip checking for multiheaded. */
	Money va = (e_a->GetRunningCost()) / max(1U, (uint)e_a->GetPower());
	Money vb = (e_b->GetRunningCost()) / max(1U, (uint)e_b->GetPower());
	int r = ClampToI32(vb - va);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

static int CDECL TrainEngineCapacitySorter(const void *a, const void *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*(const EngineID*)a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*(const EngineID*)b);

	int va = GetTotalCapacityOfArticulatedParts(*(const EngineID*)a, VEH_TRAIN) * (rvi_a->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int vb = GetTotalCapacityOfArticulatedParts(*(const EngineID*)b, VEH_TRAIN) * (rvi_b->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
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

/* Road vehicle sorting functions */
static int CDECL RoadVehEngineCapacitySorter(const void *a, const void *b)
{
	int va = GetTotalCapacityOfArticulatedParts(*(const EngineID*)a, VEH_ROAD);
	int vb = GetTotalCapacityOfArticulatedParts(*(const EngineID*)b, VEH_ROAD);
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/* Ship vehicle sorting functions */
static int CDECL ShipEngineCapacitySorter(const void *a, const void *b)
{
	const Engine *e_a = GetEngine(*(const EngineID*)a);
	const Engine *e_b = GetEngine(*(const EngineID*)b);

	int va = e_a->GetDisplayDefaultCapacity();
	int vb = e_b->GetDisplayDefaultCapacity();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/* Aircraft sorting functions */
static int CDECL AircraftEngineCargoSorter(const void *a, const void *b)
{
	const Engine *e_a = GetEngine(*(const EngineID*)a);
	const Engine *e_b = GetEngine(*(const EngineID*)b);

	int va = e_a->GetDisplayDefaultCapacity();
	int vb = e_b->GetDisplayDefaultCapacity();
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
	&EngineNumberSorter,
	&EngineCostSorter,
	&EngineSpeedSorter,
	&EnginePowerSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineRunningCostSorter,
	&TrainEnginePowerVsRunningCostSorter,
	&EngineReliabilitySorter,
	&TrainEngineCapacitySorter,
}, {
	/* Road vehicles */
	&EngineNumberSorter,
	&EngineCostSorter,
	&EngineSpeedSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineRunningCostSorter,
	&EngineReliabilitySorter,
	&RoadVehEngineCapacitySorter,
}, {
	/* Ships */
	&EngineNumberSorter,
	&EngineCostSorter,
	&EngineSpeedSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineRunningCostSorter,
	&EngineReliabilitySorter,
	&ShipEngineCapacitySorter,
}, {
	/* Aircraft */
	&EngineNumberSorter,
	&EngineCostSorter,
	&EngineSpeedSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineRunningCostSorter,
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
	STR_ENGINE_SORT_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_ENGINE_SORT_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_ENGINE_SORT_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Ships */
	STR_ENGINE_SORT_ENGINE_ID,
	STR_ENGINE_SORT_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_ENGINE_SORT_INTRO_DATE,
	STR_SORT_BY_DROPDOWN_NAME,
	STR_ENGINE_SORT_RUNNING_COST,
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

static int DrawCargoCapacityInfo(int x, int y, EngineID engine, VehicleType type, bool refittable)
{
	uint16 *cap = GetCapacityOfArticulatedParts(engine, type);

	for (uint c = 0; c < NUM_CARGO; c++) {
		if (cap[c] == 0) continue;

		SetDParam(0, c);
		SetDParam(1, cap[c]);
		SetDParam(2, refittable ? STR_9842_REFITTABLE : STR_EMPTY);
		DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, TC_FROMSTRING);
		y += 10;

		/* Only show as refittable once */
		refittable = false;
	}

	return y;
}

/* Draw rail wagon specific details */
static int DrawRailWagonPurchaseInfo(int x, int y, EngineID engine_number, const RailVehicleInfo *rvi)
{
	const Engine *e = GetEngine(engine_number);

	/* Purchase cost */
	SetDParam(0, e->GetCost());
	DrawString(x, y, STR_PURCHASE_INFO_COST, TC_FROMSTRING);
	y += 10;

	/* Wagon weight - (including cargo) */
	uint weight = e->GetDisplayWeight();
	SetDParam(0, weight);
	uint cargo_weight = (e->CanCarryCargo() ? GetCargo(e->GetDefaultCargoType())->weight * e->GetDisplayDefaultCapacity() >> 4 : 0);
	SetDParam(1, cargo_weight + weight);
	DrawString(x, y, STR_PURCHASE_INFO_WEIGHT_CWEIGHT, TC_FROMSTRING);
	y += 10;

	/* Wagon speed limit, displayed if above zero */
	if (_settings_game.vehicle.wagon_speed_limits) {
		uint max_speed = e->GetDisplayMaxSpeed();
		if (max_speed > 0) {
			SetDParam(0, max_speed);
			DrawString(x, y, STR_PURCHASE_INFO_SPEED, TC_FROMSTRING);
			y += 10;
		}
	}

	/* Running cost */
	if (rvi->running_cost_class != 0xFF) {
		SetDParam(0, e->GetRunningCost());
		DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, TC_FROMSTRING);
		y += 10;
	}

	return y;
}

/* Draw locomotive specific details */
static int DrawRailEnginePurchaseInfo(int x, int y, EngineID engine_number, const RailVehicleInfo *rvi)
{
	const Engine *e = GetEngine(engine_number);

	/* Purchase Cost - Engine weight */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayWeight());
	DrawString(x, y, STR_PURCHASE_INFO_COST_WEIGHT, TC_FROMSTRING);
	y += 10;

	/* Max speed - Engine power */
	SetDParam(0, e->GetDisplayMaxSpeed());
	SetDParam(1, e->GetPower());
	DrawString(x, y, STR_PURCHASE_INFO_SPEED_POWER, TC_FROMSTRING);
	y += 10;

	/* Max tractive effort - not applicable if old acceleration or maglev */
	if (_settings_game.vehicle.train_acceleration_model != TAM_ORIGINAL && rvi->railtype != RAILTYPE_MAGLEV) {
		SetDParam(0, e->GetDisplayMaxTractiveEffort());
		DrawString(x, y, STR_PURCHASE_INFO_MAX_TE, TC_FROMSTRING);
		y += 10;
	}

	/* Running cost */
	if (rvi->running_cost_class != 0xFF) {
		SetDParam(0, e->GetRunningCost());
		DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, TC_FROMSTRING);
		y += 10;
	}

	/* Powered wagons power - Powered wagons extra weight */
	if (rvi->pow_wag_power != 0) {
		SetDParam(0, rvi->pow_wag_power);
		SetDParam(1, rvi->pow_wag_weight);
		DrawString(x, y, STR_PURCHASE_INFO_PWAGPOWER_PWAGWEIGHT, TC_FROMSTRING);
		y += 10;
	};

	return y;
}

/* Draw road vehicle specific details */
static int DrawRoadVehPurchaseInfo(int x, int y, EngineID engine_number)
{
	const Engine *e = GetEngine(engine_number);

	/* Purchase cost - Max speed */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, TC_FROMSTRING);
	y += 10;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, TC_FROMSTRING);
	y += 10;

	return y;
}

/* Draw ship specific details */
static int DrawShipPurchaseInfo(int x, int y, EngineID engine_number, const ShipVehicleInfo *svi, bool refittable)
{
	const Engine *e = GetEngine(engine_number);

	/* Purchase cost - Max speed */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, TC_FROMSTRING);
	y += 10;

	/* Cargo type + capacity */
	SetDParam(0, e->GetDefaultCargoType());
	SetDParam(1, e->GetDisplayDefaultCapacity());
	SetDParam(2, refittable ? STR_9842_REFITTABLE : STR_EMPTY);
	DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, TC_FROMSTRING);
	y += 10;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, TC_FROMSTRING);
	y += 10;

	return y;
}

/* Draw aircraft specific details */
static int DrawAircraftPurchaseInfo(int x, int y, EngineID engine_number, const AircraftVehicleInfo *avi, bool refittable)
{
	const Engine *e = GetEngine(engine_number);
	CargoID cargo = e->GetDefaultCargoType();

	/* Purchase cost - Max speed */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	DrawString(x, y, STR_PURCHASE_INFO_COST_SPEED, TC_FROMSTRING);
	y += 10;

	/* Cargo capacity */
	if (cargo == CT_INVALID || cargo == CT_PASSENGERS) {
		SetDParam(0, e->GetDisplayDefaultCapacity());
		SetDParam(1, avi->mail_capacity);
		DrawString(x, y, STR_PURCHASE_INFO_AIRCRAFT_CAPACITY, TC_FROMSTRING);
	} else {
		/* Note, if the default capacity is selected by the refit capacity
		 * callback, then the capacity shown is likely to be incorrect. */
		SetDParam(0, cargo);
		SetDParam(1, e->GetDisplayDefaultCapacity());
		SetDParam(2, refittable ? STR_9842_REFITTABLE : STR_EMPTY);
		DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, TC_FROMSTRING);
	}
	y += 10;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(x, y, STR_PURCHASE_INFO_RUNNINGCOST, TC_FROMSTRING);
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
	bool refittable = IsArticulatedVehicleRefittable(engine_number);

	switch (e->type) {
		default: NOT_REACHED();
		case VEH_TRAIN: {
			const RailVehicleInfo *rvi = RailVehInfo(engine_number);
			if (rvi->railveh_type == RAILVEH_WAGON) {
				y = DrawRailWagonPurchaseInfo(x, y, engine_number, rvi);
			} else {
				y = DrawRailEnginePurchaseInfo(x, y, engine_number, rvi);
			}

			/* Cargo type + capacity, or N/A */
			int new_y = DrawCargoCapacityInfo(x, y, engine_number, VEH_TRAIN, refittable);

			if (new_y == y) {
				SetDParam(0, CT_INVALID);
				SetDParam(2, STR_EMPTY);
				DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, TC_FROMSTRING);
				y += 10;
			} else {
				y = new_y;
			}
			break;
		}
		case VEH_ROAD: {
			y = DrawRoadVehPurchaseInfo(x, y, engine_number);

			/* Cargo type + capacity, or N/A */
			int new_y = DrawCargoCapacityInfo(x, y, engine_number, VEH_ROAD, refittable);

			if (new_y == y) {
				SetDParam(0, CT_INVALID);
				SetDParam(2, STR_EMPTY);
				DrawString(x, y, STR_PURCHASE_INFO_CAPACITY, TC_FROMSTRING);
				y += 10;
			} else {
				y = new_y;
			}
			break;
		}
		case VEH_SHIP:
			y = DrawShipPurchaseInfo(x, y, engine_number, ShipVehInfo(engine_number), refittable);
			break;
		case VEH_AIRCRAFT:
			y = DrawAircraftPurchaseInfo(x, y, engine_number, AircraftVehInfo(engine_number), refittable);
			break;
	}

	/* Draw details, that applies to all types except rail wagons */
	if (e->type != VEH_TRAIN || RailVehInfo(engine_number)->railveh_type != RAILVEH_WAGON) {
		/* Design date - Life length */
		SetDParam(0, ymd.year);
		SetDParam(1, e->lifelength);
		DrawString(x, y, STR_PURCHASE_INFO_DESIGNED_LIFE, TC_FROMSTRING);
		y += 10;

		/* Reliability */
		SetDParam(0, e->reliability * 100 >> 16);
		DrawString(x, y, STR_PURCHASE_INFO_RELIABILITY, TC_FROMSTRING);
		y += 10;
	}

	/* Additional text from NewGRF */
	y += ShowAdditionalText(x, y, w, engine_number);
	if (refittable) y += ShowRefitOptionsList(x, y, w, engine_number);

	return y;
}

static void DrawVehicleEngine(VehicleType type, int x, int y, EngineID engine, SpriteID pal)
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
 * @param count_location Offset to print the engine count (used by autoreplace). 0 means it's off
 */
void DrawEngineList(VehicleType type, int x, int r, int y, const GUIEngineList *eng_list, uint16 min, uint16 max, EngineID selected_id, int count_location, GroupID selected_group)
{
	byte step_size = GetVehicleListHeight(type);
	byte x_offset = 0;
	byte y_offset = 0;

	assert(max <= eng_list->Length());

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

	uint maxw = r - x - x_offset;

	for (; min < max; min++, y += step_size) {
		const EngineID engine = (*eng_list)[min];
		/* Note: num_engines is only used in the autoreplace GUI, so it is correct to use _local_company here. */
		const uint num_engines = GetGroupNumEngines(_local_company, selected_group, engine);

		SetDParam(0, engine);
		DrawStringTruncated(x + x_offset, y, STR_ENGINE_NAME, engine == selected_id ? TC_WHITE : TC_BLACK, maxw);
		DrawVehicleEngine(type, x, y + y_offset, engine, (count_location != 0 && num_engines == 0) ? PALETTE_CRASH : GetEnginePalette(engine, _local_company));
		if (count_location != 0) {
			SetDParam(0, num_engines);
			DrawStringRightAligned(count_location, y + (GetVehicleListHeight(type) == 14 ? 3 : 8), STR_TINY_BLACK, TC_FROMSTRING);
		}
	}
}


struct BuildVehicleWindow : Window {
	VehicleType vehicle_type;
	union {
		RailTypeByte railtype;
		AirportFTAClass::Flags flags;
		RoadTypes roadtypes;
	} filter;
	bool descending_sort_order;
	byte sort_criteria;
	bool regenerate_list;
	bool listview_mode;
	EngineID sel_engine;
	EngineID rename_engine;
	GUIEngineList eng_list;

	BuildVehicleWindow(const WindowDesc *desc, TileIndex tile, VehicleType type) : Window(desc, tile == INVALID_TILE ? (int)type : tile)
	{
		this->vehicle_type = type;
		int vlh = GetVehicleListHeight(this->vehicle_type);

		ResizeWindow(this, 0, vlh - 14);
		this->resize.step_height = vlh;
		this->vscroll.cap = 1;
		this->widget[BUILD_VEHICLE_WIDGET_LIST].data = 0x101;

		this->resize.width  = this->width;
		this->resize.height = this->height;

		this->owner = (tile != INVALID_TILE) ? GetTileOwner(tile) : _local_company;

		this->sel_engine      = INVALID_ENGINE;
		this->regenerate_list = false;

		this->sort_criteria         = _last_sort_criteria[type];
		this->descending_sort_order = _last_sort_order[type];

		switch (type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				this->filter.railtype = (tile == INVALID_TILE) ? RAILTYPE_END : GetRailType(tile);
				break;
			case VEH_ROAD:
				this->filter.roadtypes = (tile == INVALID_TILE) ? ROADTYPES_ALL : GetRoadTypes(tile);
			case VEH_SHIP:
				break;
			case VEH_AIRCRAFT:
				this->filter.flags =
					tile == INVALID_TILE ? AirportFTAClass::ALL : GetStationByTile(tile)->Airport()->flags;
				break;
		}
		this->SetupWindowStrings(type);

		this->listview_mode = (this->window_number <= VEH_END);
		/* If we are just viewing the list of vehicles, we do not need the Build button.
		 * So we just hide it, and enlarge the Rename buton by the now vacant place. */
		if (this->listview_mode) {
			this->HideWidget(BUILD_VEHICLE_WIDGET_BUILD);
			this->widget[BUILD_VEHICLE_WIDGET_RENAME].left = this->widget[BUILD_VEHICLE_WIDGET_BUILD].left;
		} else {
			/* Both are visible, adjust the size of each */
			ResizeButtons(this, BUILD_VEHICLE_WIDGET_BUILD, BUILD_VEHICLE_WIDGET_RENAME);
		}

		this->GenerateBuildList(); // generate the list, since we need it in the next line
		/* Select the first engine in the list as default when opening the window */
		if (this->eng_list.Length() > 0) this->sel_engine = this->eng_list[0];

		this->FindWindowPlacementAndResize(desc);
	}

	/* Setup widget strings to fit the different types of vehicles */
	void SetupWindowStrings(VehicleType type)
	{
		switch (type) {
			default: NOT_REACHED();

			case VEH_TRAIN:
				this->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = this->listview_mode ? STR_AVAILABLE_TRAINS : STR_JUST_STRING;
				this->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_8843_TRAIN_VEHICLE_SELECTION;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_881F_BUILD_VEHICLE;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_8844_BUILD_THE_HIGHLIGHTED_TRAIN;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_8820_RENAME;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_8845_RENAME_TRAIN_VEHICLE_TYPE;
				break;

			case VEH_ROAD:
				this->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = this->listview_mode ? STR_AVAILABLE_ROAD_VEHICLES : STR_9006_NEW_ROAD_VEHICLES;
				this->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_9026_ROAD_VEHICLE_SELECTION;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_9007_BUILD_VEHICLE;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_9027_BUILD_THE_HIGHLIGHTED_ROAD;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_9034_RENAME;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_9035_RENAME_ROAD_VEHICLE_TYPE;
				break;

			case VEH_SHIP:
				this->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = this->listview_mode ? STR_AVAILABLE_SHIPS : STR_9808_NEW_SHIPS;
				this->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_9825_SHIP_SELECTION_LIST_CLICK;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_9809_BUILD_SHIP;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_9826_BUILD_THE_HIGHLIGHTED_SHIP;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_9836_RENAME;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_9837_RENAME_SHIP_TYPE;
				break;

			case VEH_AIRCRAFT:
				this->widget[BUILD_VEHICLE_WIDGET_CAPTION].data    = this->listview_mode ? STR_AVAILABLE_AIRCRAFT : STR_A005_NEW_AIRCRAFT;
				this->widget[BUILD_VEHICLE_WIDGET_LIST].tooltips   = STR_A025_AIRCRAFT_SELECTION_LIST;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].data      = STR_A006_BUILD_AIRCRAFT;
				this->widget[BUILD_VEHICLE_WIDGET_BUILD].tooltips  = STR_A026_BUILD_THE_HIGHLIGHTED_AIRCRAFT;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].data     = STR_A037_RENAME;
				this->widget[BUILD_VEHICLE_WIDGET_RENAME].tooltips = STR_A038_RENAME_AIRCRAFT_TYPE;
				break;
		}
	}

	/* Figure out what train EngineIDs to put in the list */
	void GenerateBuildTrainList()
	{
		EngineID sel_id = INVALID_ENGINE;
		int num_engines = 0;
		int num_wagons  = 0;

		this->filter.railtype = (this->listview_mode) ? RAILTYPE_END : GetRailType(this->window_number);

		this->eng_list.Clear();

		/* Make list of all available train engines and wagons.
		 * Also check to see if the previously selected engine is still available,
		 * and if not, reset selection to INVALID_ENGINE. This could be the case
		 * when engines become obsolete and are removed */
		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, VEH_TRAIN) {
			EngineID eid = e->index;
			const RailVehicleInfo *rvi = &e->u.rail;

			if (this->filter.railtype != RAILTYPE_END && !HasPowerOnRail(rvi->railtype, this->filter.railtype)) continue;
			if (!IsEngineBuildable(eid, VEH_TRAIN, _local_company)) continue;

			*this->eng_list.Append() = eid;

			if (rvi->railveh_type != RAILVEH_WAGON) {
				num_engines++;
			} else {
				num_wagons++;
			}

			if (eid == this->sel_engine) sel_id = eid;
		}

		this->sel_engine = sel_id;

		/* make engines first, and then wagons, sorted by ListPositionOfEngine() */
		_internal_sort_order = false;
		EngList_Sort(&this->eng_list, TrainEnginesThenWagonsSorter);

		/* and then sort engines */
		_internal_sort_order = this->descending_sort_order;
		EngList_SortPartial(&this->eng_list, _sorter[0][this->sort_criteria], 0, num_engines);

		/* and finally sort wagons */
		EngList_SortPartial(&this->eng_list, _sorter[0][this->sort_criteria], num_engines, num_wagons);
	}

	/* Figure out what road vehicle EngineIDs to put in the list */
	void GenerateBuildRoadVehList()
	{
		EngineID sel_id = INVALID_ENGINE;

		this->eng_list.Clear();

		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_ROAD, _local_company)) continue;
			if (!HasBit(this->filter.roadtypes, HasBit(EngInfo(eid)->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD)) continue;
			*this->eng_list.Append() = eid;

			if (eid == this->sel_engine) sel_id = eid;
		}
		this->sel_engine = sel_id;
	}

	/* Figure out what ship EngineIDs to put in the list */
	void GenerateBuildShipList()
	{
		EngineID sel_id = INVALID_ENGINE;
		this->eng_list.Clear();

		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, VEH_SHIP) {
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_SHIP, _local_company)) continue;
			*this->eng_list.Append() = eid;

			if (eid == this->sel_engine) sel_id = eid;
		}
		this->sel_engine = sel_id;
	}

	/* Figure out what aircraft EngineIDs to put in the list */
	void GenerateBuildAircraftList()
	{
		EngineID sel_id = INVALID_ENGINE;

		this->eng_list.Clear();

		const Station *st = this->listview_mode ? NULL : GetStationByTile(this->window_number);

		/* Make list of all available planes.
		 * Also check to see if the previously selected plane is still available,
		 * and if not, reset selection to INVALID_ENGINE. This could be the case
		 * when planes become obsolete and are removed */
		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, VEH_AIRCRAFT) {
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_AIRCRAFT, _local_company)) continue;
			/* First VEH_END window_numbers are fake to allow a window open for all different types at once */
			if (!this->listview_mode && !CanVehicleUseStation(eid, st)) continue;

			*this->eng_list.Append() = eid;
			if (eid == this->sel_engine) sel_id = eid;
		}

		this->sel_engine = sel_id;
	}

	/* Generate the list of vehicles */
	void GenerateBuildList()
	{
		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				this->GenerateBuildTrainList();
				return; // trains should not reach the last sorting
			case VEH_ROAD:
				this->GenerateBuildRoadVehList();
				break;
			case VEH_SHIP:
				this->GenerateBuildShipList();
				break;
			case VEH_AIRCRAFT:
				this->GenerateBuildAircraftList();
				break;
		}
		_internal_sort_order = this->descending_sort_order;
		EngList_Sort(&this->eng_list, _sorter[this->vehicle_type][this->sort_criteria]);
	}

	void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_last_sort_order[this->vehicle_type] = this->descending_sort_order;
				this->regenerate_list = true;
				this->SetDirty();
				break;

			case BUILD_VEHICLE_WIDGET_LIST: {
				uint i = (pt.y - this->widget[BUILD_VEHICLE_WIDGET_LIST].top) / GetVehicleListHeight(this->vehicle_type) + this->vscroll.pos;
				size_t num_items = this->eng_list.Length();
				this->sel_engine = (i < num_items) ? this->eng_list[i] : INVALID_ENGINE;
				this->SetDirty();
				break;
			}

			case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN: // Select sorting criteria dropdown menu
				ShowDropDownMenu(this, _sort_listing[this->vehicle_type], this->sort_criteria, BUILD_VEHICLE_WIDGET_SORT_DROPDOWN, 0, 0);
				break;

			case BUILD_VEHICLE_WIDGET_BUILD: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != INVALID_ENGINE) {
					switch (this->vehicle_type) {
						default: NOT_REACHED();
						case VEH_TRAIN:
							DoCommandP(this->window_number, sel_eng, 0,
									CMD_BUILD_RAIL_VEHICLE | CMD_MSG(STR_882B_CAN_T_BUILD_RAILROAD_VEHICLE),
									(RailVehInfo(sel_eng)->railveh_type == RAILVEH_WAGON) ? CcBuildWagon : CcBuildLoco);
							break;
						case VEH_ROAD:
							DoCommandP(this->window_number, sel_eng, 0, CMD_BUILD_ROAD_VEH | CMD_MSG(STR_9009_CAN_T_BUILD_ROAD_VEHICLE), CcBuildRoadVeh);
							break;
						case VEH_SHIP:
							DoCommandP(this->window_number, sel_eng, 0, CMD_BUILD_SHIP | CMD_MSG(STR_980D_CAN_T_BUILD_SHIP), CcBuildShip);
							break;
						case VEH_AIRCRAFT:
							DoCommandP(this->window_number, sel_eng, 0, CMD_BUILD_AIRCRAFT | CMD_MSG(STR_A008_CAN_T_BUILD_AIRCRAFT), CcBuildAircraft);
							break;
					}
				}
				break;
			}

			case BUILD_VEHICLE_WIDGET_RENAME: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != INVALID_ENGINE) {
					StringID str = STR_NULL;

					this->rename_engine = sel_eng;
					switch (this->vehicle_type) {
						default: NOT_REACHED();
						case VEH_TRAIN:    str = STR_886A_RENAME_TRAIN_VEHICLE_TYPE; break;
						case VEH_ROAD:     str = STR_9036_RENAME_ROAD_VEHICLE_TYPE;  break;
						case VEH_SHIP:     str = STR_9838_RENAME_SHIP_TYPE;          break;
						case VEH_AIRCRAFT: str = STR_A039_RENAME_AIRCRAFT_TYPE;      break;
					}
					SetDParam(0, sel_eng);
					ShowQueryString(STR_ENGINE_NAME, str, MAX_LENGTH_ENGINE_NAME_BYTES, MAX_LENGTH_ENGINE_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				}
				break;
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		this->regenerate_list = true;
	}

	virtual void OnPaint()
	{
		if (this->regenerate_list) {
			this->regenerate_list = false;
			this->GenerateBuildList();
		}

		uint max = min(this->vscroll.pos + this->vscroll.cap, this->eng_list.Length());

		SetVScrollCount(this, this->eng_list.Length());
		if (this->vehicle_type == VEH_TRAIN) {
			if (this->filter.railtype == RAILTYPE_END) {
				SetDParam(0, STR_ALL_AVAIL_RAIL_VEHICLES);
			} else {
				const RailtypeInfo *rti = GetRailTypeInfo(this->filter.railtype);
				SetDParam(0, rti->strings.build_caption);
			}
		}

		/* Set text of sort by dropdown */
		this->widget[BUILD_VEHICLE_WIDGET_SORT_DROPDOWN].data = _sort_listing[this->vehicle_type][this->sort_criteria];

		this->DrawWidgets();

		DrawEngineList(this->vehicle_type, this->widget[BUILD_VEHICLE_WIDGET_LIST].left + 2, this->widget[BUILD_VEHICLE_WIDGET_LIST].right, this->widget[BUILD_VEHICLE_WIDGET_LIST].top + 1, &this->eng_list, this->vscroll.pos, max, this->sel_engine, 0, DEFAULT_GROUP);

		if (this->sel_engine != INVALID_ENGINE) {
			const Widget *wi = &this->widget[BUILD_VEHICLE_WIDGET_PANEL];
			int text_end = DrawVehiclePurchaseInfo(2, wi->top + 1, wi->right - wi->left - 2, this->sel_engine);

			if (text_end > wi->bottom) {
				this->SetDirty();
				ResizeWindowForWidget(this, BUILD_VEHICLE_WIDGET_PANEL, 0, text_end - wi->bottom);
				this->SetDirty();
			}
		}

		this->DrawSortButtonState(BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		if (widget == BUILD_VEHICLE_WIDGET_LIST) {
			/* When double clicking, we want to buy a vehicle */
			this->OnClick(pt, BUILD_VEHICLE_WIDGET_BUILD);
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		StringID err_str = STR_NULL;
		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:    err_str = STR_886B_CAN_T_RENAME_TRAIN_VEHICLE; break;
			case VEH_ROAD:     err_str = STR_9037_CAN_T_RENAME_ROAD_VEHICLE;  break;
			case VEH_SHIP:     err_str = STR_9839_CAN_T_RENAME_SHIP_TYPE;     break;
			case VEH_AIRCRAFT: err_str = STR_A03A_CAN_T_RENAME_AIRCRAFT_TYPE; break;
		}
		DoCommandP(0, this->rename_engine, 0, CMD_RENAME_ENGINE | CMD_MSG(err_str), NULL, str);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (this->sort_criteria != index) {
			this->sort_criteria = index;
			_last_sort_criteria[this->vehicle_type] = this->sort_criteria;
			this->regenerate_list = true;
		}
		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (delta.x != 0 && !this->listview_mode) {
			ResizeButtons(this, BUILD_VEHICLE_WIDGET_BUILD, BUILD_VEHICLE_WIDGET_RENAME);
		}
		if (delta.y == 0) return;

		this->vscroll.cap += delta.y / (int)GetVehicleListHeight(this->vehicle_type);
		this->widget[BUILD_VEHICLE_WIDGET_LIST].data = (this->vscroll.cap << 8) + 1;
	}
};

static const WindowDesc _build_vehicle_desc(
	WDP_AUTO, WDP_AUTO, 240, 174, 240, 256,
	WC_BUILD_VEHICLE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE | WDF_CONSTRUCTION,
	_build_vehicle_widgets
);

void ShowBuildVehicleWindow(TileIndex tile, VehicleType type)
{
	/* We want to be able to open both Available Train as Available Ships,
	 *  so if tile == INVALID_TILE (Available XXX Window), use 'type' as unique number.
	 *  As it always is a low value, it won't collide with any real tile
	 *  number. */
	uint num = (tile == INVALID_TILE) ? (int)type : tile;

	assert(IsCompanyBuildableVehicleType(type));

	DeleteWindowById(WC_BUILD_VEHICLE, num);

	new BuildVehicleWindow(&_build_vehicle_desc, tile, type);
}
