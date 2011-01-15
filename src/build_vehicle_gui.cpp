/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file build_vehicle_gui.cpp GUI for building vehicles. */

#include "stdafx.h"
#include "engine_base.h"
#include "engine_func.h"
#include "station_base.h"
#include "articulated_vehicles.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "group.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "widgets/dropdown_func.h"
#include "engine_gui.h"
#include "cargotype.h"
#include "core/geometry_func.hpp"

#include "table/strings.h"

/**
 * Get the height of a single 'entry' in the engine lists.
 * @param type the vehicle type to get the height of
 * @return the height for the entry
 */
uint GetEngineListHeight(VehicleType type)
{
	return max<uint>(FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM, GetVehicleHeight(type));
}

enum BuildVehicleWidgets {
	BUILD_VEHICLE_WIDGET_CAPTION,
	BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING,
	BUILD_VEHICLE_WIDGET_SORT_DROPDOWN,
	BUILD_VEHICLE_WIDGET_CARGO_FILTER_DROPDOWN,
	BUILD_VEHICLE_WIDGET_LIST,
	BUILD_VEHICLE_WIDGET_SCROLLBAR,
	BUILD_VEHICLE_WIDGET_PANEL,
	BUILD_VEHICLE_WIDGET_BUILD,
	BUILD_VEHICLE_WIDGET_BUILD_SEL,
	BUILD_VEHICLE_WIDGET_RENAME,
	BUILD_VEHICLE_WIDGET_END
};

static const NWidgetPart _nested_build_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, BUILD_VEHICLE_WIDGET_CAPTION), SetDataTip(STR_WHITE_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0),
				NWidget(NWID_SPACER), SetFill(1, 1),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, BUILD_VEHICLE_WIDGET_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, BUILD_VEHICLE_WIDGET_CARGO_FILTER_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_FILTER_CRITERIA),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	/* Vehicle list. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, BUILD_VEHICLE_WIDGET_LIST), SetResize(1, 1), SetFill(1, 0), SetDataTip(0x101, STR_NULL), SetScrollbar(BUILD_VEHICLE_WIDGET_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, BUILD_VEHICLE_WIDGET_SCROLLBAR),
	EndContainer(),
	/* Panel with details. */
	NWidget(WWT_PANEL, COLOUR_GREY, BUILD_VEHICLE_WIDGET_PANEL), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	/* Build/rename buttons, resize button. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, BUILD_VEHICLE_WIDGET_BUILD_SEL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BUILD_VEHICLE_WIDGET_BUILD), SetResize(1, 0), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BUILD_VEHICLE_WIDGET_RENAME), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** Special cargo filter criteria */
static const CargoID CF_ANY  = CT_NO_REFIT; ///< Show all vehicles independent of carried cargo (i.e. no filtering)
static const CargoID CF_NONE = CT_INVALID;  ///< Show only vehicles which do not carry cargo (e.g. train engines)

static bool _internal_sort_order;           ///< false = descending, true = ascending
static byte _last_sort_criteria[]      = {0, 0, 0, 0};
static bool _last_sort_order[]         = {false, false, false, false};
static CargoID _last_filter_criteria[] = {CF_ANY, CF_ANY, CF_ANY, CF_ANY};

/**
 * Determines order of engines by engineID
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineNumberSorter(const EngineID *a, const EngineID *b)
{
	int r = ListPositionOfEngine(*a) - ListPositionOfEngine(*b);

	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by introduction date
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineIntroDateSorter(const EngineID *a, const EngineID *b)
{
	const int va = Engine::Get(*a)->intro_date;
	const int vb = Engine::Get(*b)->intro_date;
	const int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by name
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineNameSorter(const EngineID *a, const EngineID *b)
{
	static EngineID last_engine[2] = { INVALID_ENGINE, INVALID_ENGINE };
	static char     last_name[2][64] = { "\0", "\0" };

	const EngineID va = *a;
	const EngineID vb = *b;

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

	int r = strnatcmp(last_name[0], last_name[1]); // Sort by name (natural sorting).

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by reliability
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineReliabilitySorter(const EngineID *a, const EngineID *b)
{
	const int va = Engine::Get(*a)->reliability;
	const int vb = Engine::Get(*b)->reliability;
	const int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by purchase cost
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineCostSorter(const EngineID *a, const EngineID *b)
{
	Money va = Engine::Get(*a)->GetCost();
	Money vb = Engine::Get(*b)->GetCost();
	int r = ClampToI32(va - vb);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by speed
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineSpeedSorter(const EngineID *a, const EngineID *b)
{
	int va = Engine::Get(*a)->GetDisplayMaxSpeed();
	int vb = Engine::Get(*b)->GetDisplayMaxSpeed();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by power
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EnginePowerSorter(const EngineID *a, const EngineID *b)
{
	int va = Engine::Get(*a)->GetPower();
	int vb = Engine::Get(*b)->GetPower();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by tractive effort
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineTractiveEffortSorter(const EngineID *a, const EngineID *b)
{
	int va = Engine::Get(*a)->GetDisplayMaxTractiveEffort();
	int vb = Engine::Get(*b)->GetDisplayMaxTractiveEffort();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by running costs
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EngineRunningCostSorter(const EngineID *a, const EngineID *b)
{
	Money va = Engine::Get(*a)->GetRunningCost();
	Money vb = Engine::Get(*b)->GetRunningCost();
	int r = ClampToI32(va - vb);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of engines by running costs
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL EnginePowerVsRunningCostSorter(const EngineID *a, const EngineID *b)
{
	const Engine *e_a = Engine::Get(*a);
	const Engine *e_b = Engine::Get(*b);

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

/* Train sorting functions */

/**
 * Determines order of train engines by capacity
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL TrainEngineCapacitySorter(const EngineID *a, const EngineID *b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(*a);
	const RailVehicleInfo *rvi_b = RailVehInfo(*b);

	int va = GetTotalCapacityOfArticulatedParts(*a) * (rvi_a->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int vb = GetTotalCapacityOfArticulatedParts(*b) * (rvi_b->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/**
 * Determines order of train engines by engine / wagon
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL TrainEnginesThenWagonsSorter(const EngineID *a, const EngineID *b)
{
	int val_a = (RailVehInfo(*a)->railveh_type == RAILVEH_WAGON ? 1 : 0);
	int val_b = (RailVehInfo(*b)->railveh_type == RAILVEH_WAGON ? 1 : 0);
	int r = val_a - val_b;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/* Road vehicle sorting functions */

/**
 * Determines order of road vehicles by capacity
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL RoadVehEngineCapacitySorter(const EngineID *a, const EngineID *b)
{
	int va = GetTotalCapacityOfArticulatedParts(*a);
	int vb = GetTotalCapacityOfArticulatedParts(*b);
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/* Ship vehicle sorting functions */

/**
 * Determines order of ships by capacity
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL ShipEngineCapacitySorter(const EngineID *a, const EngineID *b)
{
	const Engine *e_a = Engine::Get(*a);
	const Engine *e_b = Engine::Get(*b);

	int va = e_a->GetDisplayDefaultCapacity();
	int vb = e_b->GetDisplayDefaultCapacity();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _internal_sort_order ? -r : r;
}

/* Aircraft sorting functions */

/**
 * Determines order of aircraft by cargo
 * @param *a first engine to compare
 * @param *b second engine to compare
 * @return for descending order: returns < 0 if a < b and > 0 for a > b. Vice versa for ascending order and 0 for equal
 */
static int CDECL AircraftEngineCargoSorter(const EngineID *a, const EngineID *b)
{
	const Engine *e_a = Engine::Get(*a);
	const Engine *e_b = Engine::Get(*b);

	uint16 mail_a, mail_b;
	int va = e_a->GetDisplayDefaultCapacity(&mail_a);
	int vb = e_b->GetDisplayDefaultCapacity(&mail_b);
	int r = va - vb;

	if (r == 0) {
		/* The planes have the same passenger capacity. Check mail capacity instead */
		r = mail_a - mail_b;

		if (r == 0) {
			/* Use EngineID to sort instead since we want consistent sorting */
			return EngineNumberSorter(a, b);
		}
	}
	return _internal_sort_order ? -r : r;
}

static EngList_SortTypeFunction * const _sorter[][11] = {{
	/* Trains */
	&EngineNumberSorter,
	&EngineCostSorter,
	&EngineSpeedSorter,
	&EnginePowerSorter,
	&EngineTractiveEffortSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineRunningCostSorter,
	&EnginePowerVsRunningCostSorter,
	&EngineReliabilitySorter,
	&TrainEngineCapacitySorter,
}, {
	/* Road vehicles */
	&EngineNumberSorter,
	&EngineCostSorter,
	&EngineSpeedSorter,
	&EnginePowerSorter,
	&EngineTractiveEffortSorter,
	&EngineIntroDateSorter,
	&EngineNameSorter,
	&EngineRunningCostSorter,
	&EnginePowerVsRunningCostSorter,
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

static const StringID _sort_listing[][12] = {{
	/* Trains */
	STR_SORT_BY_ENGINE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_POWER,
	STR_SORT_BY_TRACTIVE_EFFORT,
	STR_SORT_BY_INTRO_DATE,
	STR_SORT_BY_NAME,
	STR_SORT_BY_RUNNING_COST,
	STR_SORT_BY_POWER_VS_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Road vehicles */
	STR_SORT_BY_ENGINE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_POWER,
	STR_SORT_BY_TRACTIVE_EFFORT,
	STR_SORT_BY_INTRO_DATE,
	STR_SORT_BY_NAME,
	STR_SORT_BY_RUNNING_COST,
	STR_SORT_BY_POWER_VS_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Ships */
	STR_SORT_BY_ENGINE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_INTRO_DATE,
	STR_SORT_BY_NAME,
	STR_SORT_BY_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_CARGO_CAPACITY,
	INVALID_STRING_ID
}, {
	/* Aircraft */
	STR_SORT_BY_ENGINE_ID,
	STR_SORT_BY_COST,
	STR_SORT_BY_MAX_SPEED,
	STR_SORT_BY_INTRO_DATE,
	STR_SORT_BY_NAME,
	STR_SORT_BY_RUNNING_COST,
	STR_SORT_BY_RELIABILITY,
	STR_SORT_BY_CARGO_CAPACITY,
	INVALID_STRING_ID
}};

/** Cargo filter functions */
static bool CDECL CargoFilter(const EngineID *eid, const CargoID cid)
{
	if (cid == CF_ANY) return true;
	uint32 refit_mask = GetUnionOfArticulatedRefitMasks(*eid, true);
	return (cid == CF_NONE ? refit_mask == 0 : HasBit(refit_mask, cid));
}

static GUIEngineList::FilterFunction * const _filter_funcs[] = {
	&CargoFilter,
};

static int DrawCargoCapacityInfo(int left, int right, int y, EngineID engine, bool refittable)
{
	CargoArray cap = GetCapacityOfArticulatedParts(engine);

	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (cap[c] == 0) continue;

		SetDParam(0, c);
		SetDParam(1, cap[c]);
		SetDParam(2, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY);
		DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
		y += FONT_HEIGHT_NORMAL;

		/* Only show as refittable once */
		refittable = false;
	}

	return y;
}

/* Draw rail wagon specific details */
static int DrawRailWagonPurchaseInfo(int left, int right, int y, EngineID engine_number, const RailVehicleInfo *rvi)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost */
	SetDParam(0, e->GetCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_COST);
	y += FONT_HEIGHT_NORMAL;

	/* Wagon weight - (including cargo) */
	uint weight = e->GetDisplayWeight();
	SetDParam(0, weight);
	uint cargo_weight = (e->CanCarryCargo() ? CargoSpec::Get(e->GetDefaultCargoType())->weight * e->GetDisplayDefaultCapacity() >> 4 : 0);
	SetDParam(1, cargo_weight + weight);
	DrawString(left, right, y, STR_PURCHASE_INFO_WEIGHT_CWEIGHT);
	y += FONT_HEIGHT_NORMAL;

	/* Wagon speed limit, displayed if above zero */
	if (_settings_game.vehicle.wagon_speed_limits) {
		uint max_speed = e->GetDisplayMaxSpeed();
		if (max_speed > 0) {
			SetDParam(0, max_speed);
			DrawString(left, right, y, STR_PURCHASE_INFO_SPEED);
			y += FONT_HEIGHT_NORMAL;
		}
	}

	/* Running cost */
	if (rvi->running_cost_class != INVALID_PRICE) {
		SetDParam(0, e->GetRunningCost());
		DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
		y += FONT_HEIGHT_NORMAL;
	}

	return y;
}

/* Draw locomotive specific details */
static int DrawRailEnginePurchaseInfo(int left, int right, int y, EngineID engine_number, const RailVehicleInfo *rvi)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase Cost - Engine weight */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayWeight());
	DrawString(left, right, y, STR_PURCHASE_INFO_COST_WEIGHT);
	y += FONT_HEIGHT_NORMAL;

	/* Max speed - Engine power */
	SetDParam(0, e->GetDisplayMaxSpeed());
	SetDParam(1, e->GetPower());
	DrawString(left, right, y, STR_PURCHASE_INFO_SPEED_POWER);
	y += FONT_HEIGHT_NORMAL;

	/* Max tractive effort - not applicable if old acceleration or maglev */
	if (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL && GetRailTypeInfo(rvi->railtype)->acceleration_type != 2) {
		SetDParam(0, e->GetDisplayMaxTractiveEffort());
		DrawString(left, right, y, STR_PURCHASE_INFO_MAX_TE);
		y += FONT_HEIGHT_NORMAL;
	}

	/* Running cost */
	if (rvi->running_cost_class != INVALID_PRICE) {
		SetDParam(0, e->GetRunningCost());
		DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
		y += FONT_HEIGHT_NORMAL;
	}

	/* Powered wagons power - Powered wagons extra weight */
	if (rvi->pow_wag_power != 0) {
		SetDParam(0, rvi->pow_wag_power);
		SetDParam(1, rvi->pow_wag_weight);
		DrawString(left, right, y, STR_PURCHASE_INFO_PWAGPOWER_PWAGWEIGHT);
		y += FONT_HEIGHT_NORMAL;
	}

	return y;
}

/* Draw road vehicle specific details */
static int DrawRoadVehPurchaseInfo(int left, int right, int y, EngineID engine_number)
{
	const Engine *e = Engine::Get(engine_number);

	if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) {
		/* Purchase Cost */
		SetDParam(0, e->GetCost());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST);
		y += FONT_HEIGHT_NORMAL;

		/* Road vehicle weight - (including cargo) */
		int16 weight = e->GetDisplayWeight();
		SetDParam(0, weight);
		uint cargo_weight = CargoSpec::Get(e->GetDefaultCargoType())->weight * GetTotalCapacityOfArticulatedParts(engine_number) / 16;
		SetDParam(1, cargo_weight + weight);
		DrawString(left, right, y, STR_PURCHASE_INFO_WEIGHT_CWEIGHT);
		y += FONT_HEIGHT_NORMAL;

		/* Max speed - Engine power */
		SetDParam(0, e->GetDisplayMaxSpeed());
		SetDParam(1, e->GetPower());
		DrawString(left, right, y, STR_PURCHASE_INFO_SPEED_POWER);
		y += FONT_HEIGHT_NORMAL;

		/* Max tractive effort */
		SetDParam(0, e->GetDisplayMaxTractiveEffort());
		DrawString(left, right, y, STR_PURCHASE_INFO_MAX_TE);
		y += FONT_HEIGHT_NORMAL;
	} else {
		/* Purchase cost - Max speed */
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST_SPEED);
		y += FONT_HEIGHT_NORMAL;
	}

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
	y += FONT_HEIGHT_NORMAL;

	return y;
}

/* Draw ship specific details */
static int DrawShipPurchaseInfo(int left, int right, int y, EngineID engine_number, bool refittable)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost - Max speed */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	DrawString(left, right, y, STR_PURCHASE_INFO_COST_SPEED);
	y += FONT_HEIGHT_NORMAL;

	/* Cargo type + capacity */
	SetDParam(0, e->GetDefaultCargoType());
	SetDParam(1, e->GetDisplayDefaultCapacity());
	SetDParam(2, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY);
	DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
	y += FONT_HEIGHT_NORMAL;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
	y += FONT_HEIGHT_NORMAL;

	return y;
}

/* Draw aircraft specific details */
static int DrawAircraftPurchaseInfo(int left, int right, int y, EngineID engine_number, bool refittable)
{
	const Engine *e = Engine::Get(engine_number);
	CargoID cargo = e->GetDefaultCargoType();

	/* Purchase cost - Max speed */
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	DrawString(left, right, y, STR_PURCHASE_INFO_COST_SPEED);
	y += FONT_HEIGHT_NORMAL;

	/* Cargo capacity */
	uint16 mail_capacity;
	uint capacity = e->GetDisplayDefaultCapacity(&mail_capacity);
	if (mail_capacity > 0) {
		SetDParam(0, cargo);
		SetDParam(1, capacity);
		SetDParam(2, CT_MAIL);
		SetDParam(3, mail_capacity);
		DrawString(left, right, y, STR_PURCHASE_INFO_AIRCRAFT_CAPACITY);
	} else {
		/* Note, if the default capacity is selected by the refit capacity
		 * callback, then the capacity shown is likely to be incorrect. */
		SetDParam(0, cargo);
		SetDParam(1, capacity);
		SetDParam(2, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY);
		DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
	}
	y += FONT_HEIGHT_NORMAL;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
	y += FONT_HEIGHT_NORMAL;

	return y;
}

/**
 * Display additional text from NewGRF in the purchase information window
 * @param left   Left border of text bounding box
 * @param right  Right border of text bounding box
 * @param y      Top border of text bounding box
 * @param engine Engine to query the additional purchase information for
 * @return       Bottom border of text bounding box
 */
static uint ShowAdditionalText(int left, int right, int y, EngineID engine)
{
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_ADDITIONAL_TEXT, 0, 0, engine, NULL);
	if (callback == CALLBACK_FAILED) return y;

	/* STR_BLACK_STRING is used to start the string with {BLACK} */
	SetDParam(0, GetGRFStringID(GetEngineGRFID(engine), 0xD000 + callback));
	PrepareTextRefStackUsage(0);
	uint result = DrawStringMultiLine(left, right, y, INT32_MAX, STR_BLACK_STRING);
	StopTextRefStackUsage();
	return result;
}

/**
 * Draw the purchase info details of a vehicle at a given location.
 * @param left,right,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 * @return y after drawing all the text
 */
int DrawVehiclePurchaseInfo(int left, int right, int y, EngineID engine_number)
{
	const Engine *e = Engine::Get(engine_number);
	YearMonthDay ymd;
	ConvertDateToYMD(e->intro_date, &ymd);
	bool refittable = IsArticulatedVehicleRefittable(engine_number);
	bool articulated_cargo = false;

	switch (e->type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			if (e->u.rail.railveh_type == RAILVEH_WAGON) {
				y = DrawRailWagonPurchaseInfo(left, right, y, engine_number, &e->u.rail);
			} else {
				y = DrawRailEnginePurchaseInfo(left, right, y, engine_number, &e->u.rail);
			}
			articulated_cargo = true;
			break;

		case VEH_ROAD:
			y = DrawRoadVehPurchaseInfo(left, right, y, engine_number);
			articulated_cargo = true;
			break;

		case VEH_SHIP:
			y = DrawShipPurchaseInfo(left, right, y, engine_number, refittable);
			break;

		case VEH_AIRCRAFT:
			y = DrawAircraftPurchaseInfo(left, right, y, engine_number, refittable);
			break;
	}

	if (articulated_cargo) {
		/* Cargo type + capacity, or N/A */
		int new_y = DrawCargoCapacityInfo(left, right, y, engine_number, refittable);

		if (new_y == y) {
			SetDParam(0, CT_INVALID);
			SetDParam(2, STR_EMPTY);
			DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
			y += FONT_HEIGHT_NORMAL;
		} else {
			y = new_y;
		}
	}

	/* Draw details that apply to all types except rail wagons. */
	if (e->type != VEH_TRAIN || e->u.rail.railveh_type != RAILVEH_WAGON) {
		/* Design date - Life length */
		SetDParam(0, ymd.year);
		SetDParam(1, e->GetLifeLengthInDays() / DAYS_IN_LEAP_YEAR);
		DrawString(left, right, y, STR_PURCHASE_INFO_DESIGNED_LIFE);
		y += FONT_HEIGHT_NORMAL;

		/* Reliability */
		SetDParam(0, ToPercent16(e->reliability));
		DrawString(left, right, y, STR_PURCHASE_INFO_RELIABILITY);
		y += FONT_HEIGHT_NORMAL;
	}

	/* Additional text from NewGRF */
	y = ShowAdditionalText(left, right, y, engine_number);
	if (refittable) y = ShowRefitOptionsList(left, right, y, engine_number);

	return y;
}

/**
 * Engine drawing loop
 * @param type Type of vehicle (VEH_*)
 * @param l The left most location of the list
 * @param r The right most location of the list
 * @param y The top most location of the list
 * @param eng_list What engines to draw
 * @param min where to start in the list
 * @param max where in the list to end
 * @param selected_id what engine to highlight as selected, if any
 * @param show_count Whether to show the amount of engines or not
 * @param selected_group the group to list the engines of
 */
void DrawEngineList(VehicleType type, int l, int r, int y, const GUIEngineList *eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group)
{
	static const int sprite_widths[]  = { 60, 60, 76, 67 };
	static const int sprite_y_offsets[] = { -1, -1, -2, -2 };

	/* Obligatory sanity checks! */
	assert((uint)type < lengthof(sprite_widths));
	assert_compile(lengthof(sprite_y_offsets) == lengthof(sprite_widths));
	assert(max <= eng_list->Length());

	bool rtl = _current_text_dir == TD_RTL;
	int step_size = GetEngineListHeight(type);
	int sprite_width = sprite_widths[type];

	int sprite_x        = (rtl ? r - sprite_width / 2 : l + sprite_width / 2) - 1;
	int sprite_y_offset = sprite_y_offsets[type] + step_size / 2;

	int text_left  = l + (rtl ? WD_FRAMERECT_LEFT : sprite_width);
	int text_right = r - (rtl ? sprite_width : WD_FRAMERECT_RIGHT);

	int normal_text_y_offset = (step_size - FONT_HEIGHT_NORMAL) / 2;
	int small_text_y_offset  = step_size - FONT_HEIGHT_SMALL - WD_FRAMERECT_BOTTOM - 1;

	for (; min < max; min++, y += step_size) {
		const EngineID engine = (*eng_list)[min];
		/* Note: num_engines is only used in the autoreplace GUI, so it is correct to use _local_company here. */
		const uint num_engines = GetGroupNumEngines(_local_company, selected_group, engine);

		SetDParam(0, engine);
		DrawString(text_left, text_right, y + normal_text_y_offset, STR_ENGINE_NAME, engine == selected_id ? TC_WHITE : TC_BLACK);
		DrawVehicleEngine(l, r, sprite_x, y + sprite_y_offset, engine, (show_count && num_engines == 0) ? PALETTE_CRASH : GetEnginePalette(engine, _local_company));
		if (show_count) {
			SetDParam(0, num_engines);
			DrawString(text_left, text_right, y + small_text_y_offset, STR_TINY_BLACK_COMA, TC_FROMSTRING, SA_RIGHT);
		}
	}
}


struct BuildVehicleWindow : Window {
	VehicleType vehicle_type;
	union {
		RailTypeByte railtype;
		RoadTypes roadtypes;
	} filter;
	bool descending_sort_order;
	byte sort_criteria;
	bool listview_mode;
	EngineID sel_engine;
	EngineID rename_engine;
	GUIEngineList eng_list;
	CargoID cargo_filter[NUM_CARGO + 2];        ///< Available cargo filters; CargoID or CF_ANY or CF_NONE
	StringID cargo_filter_texts[NUM_CARGO + 3]; ///< Texts for filter_cargo, terminated by INVALID_STRING_ID
	byte cargo_filter_criteria;                 ///< Selected cargo filter
	int details_height;                         ///< Minimal needed height of the details panels (found so far).
	Scrollbar *vscroll;

	BuildVehicleWindow(const WindowDesc *desc, TileIndex tile, VehicleType type) : Window()
	{
		this->vehicle_type = type;
		this->window_number = tile == INVALID_TILE ? (int)type : tile;

		this->sel_engine      = INVALID_ENGINE;

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
			case VEH_AIRCRAFT:
				break;
		}

		this->listview_mode = (this->window_number <= VEH_END);

		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(BUILD_VEHICLE_WIDGET_SCROLLBAR);

		/* If we are just viewing the list of vehicles, we do not need the Build button.
		 * So we just hide it, and enlarge the Rename buton by the now vacant place. */
		if (this->listview_mode) this->GetWidget<NWidgetStacked>(BUILD_VEHICLE_WIDGET_BUILD_SEL)->SetDisplayedPlane(SZSP_NONE);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(BUILD_VEHICLE_WIDGET_LIST);
		widget->tool_tip = STR_BUY_VEHICLE_TRAIN_LIST_TOOLTIP + type;

		widget = this->GetWidget<NWidgetCore>(BUILD_VEHICLE_WIDGET_BUILD);
		widget->widget_data = STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_BUTTON + type;
		widget->tool_tip    = STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_TOOLTIP + type;

		widget = this->GetWidget<NWidgetCore>(BUILD_VEHICLE_WIDGET_RENAME);
		widget->widget_data = STR_BUY_VEHICLE_TRAIN_RENAME_BUTTON + type;
		widget->tool_tip    = STR_BUY_VEHICLE_TRAIN_RENAME_TOOLTIP + type;

		this->details_height = ((this->vehicle_type == VEH_TRAIN) ? 10 : 9) * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;

		this->FinishInitNested(desc, tile == INVALID_TILE ? (int)type : tile);

		this->owner = (tile != INVALID_TILE) ? GetTileOwner(tile) : _local_company;

		this->eng_list.ForceRebuild();
		this->GenerateBuildList(); // generate the list, since we need it in the next line
		/* Select the first engine in the list as default when opening the window */
		if (this->eng_list.Length() > 0) this->sel_engine = this->eng_list[0];
	}

	/** Populate the filter list and set the cargo filter criteria. */
	void SetCargoFilterArray()
	{
		uint filter_items = 0;

		/* Add item for disabling filtering. */
		this->cargo_filter[filter_items] = CF_ANY;
		this->cargo_filter_texts[filter_items] = STR_PURCHASE_INFO_ALL_TYPES;
		filter_items++;

		/* Add item for vehicles not carrying anything, e.g. train engines.
		 * This could also be useful for eyecandy vehicles of other types, but is likely too confusing for joe, */
		if (this->vehicle_type == VEH_TRAIN) {
			this->cargo_filter[filter_items] = CF_NONE;
			this->cargo_filter_texts[filter_items] = STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE;
			filter_items++;
		}

		/* Collect available cargo types for filtering. */
		const CargoSpec *cs;
		FOR_ALL_SORTED_STANDARD_CARGOSPECS(cs) {
			this->cargo_filter[filter_items] = cs->Index();
			this->cargo_filter_texts[filter_items] = cs->name;
			filter_items++;
		}

		/* Terminate the filter list. */
		this->cargo_filter_texts[filter_items] = INVALID_STRING_ID;

		/* If not found, the cargo criteria will be set to all cargos. */
		this->cargo_filter_criteria = 0;

		/* Find the last cargo filter criteria. */
		for (uint i = 0; i < filter_items; i++) {
			if (this->cargo_filter[i] == _last_filter_criteria[this->vehicle_type]) {
				this->cargo_filter_criteria = i;
				break;
			}
		}

		this->eng_list.SetFilterFuncs(_filter_funcs);
		this->eng_list.SetFilterState(this->cargo_filter[this->cargo_filter_criteria] != CF_ANY);
	}

	void OnInit()
	{
		this->SetCargoFilterArray();
	}

	/** Filter the engine list against the currently selected cargo filter */
	void FilterEngineList()
	{
		this->eng_list.Filter(this->cargo_filter[this->cargo_filter_criteria]);
		if (0 == this->eng_list.Length()) { // no engine passed through the filter, invalidate the previously selected engine
			this->sel_engine = INVALID_ENGINE;
		} else if (!this->eng_list.Contains(this->sel_engine)) { // previously selected engine didn't pass the filter, select the first engine of the list
			this->sel_engine = this->eng_list[0];
		}
	}

	/** Filter a single engine */
	bool FilterSingleEngine(EngineID eid)
	{
		CargoID filter_type = this->cargo_filter[this->cargo_filter_criteria];
		return (filter_type == CF_ANY || CargoFilter(&eid, filter_type));
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

			/* Filter now! So num_engines and num_wagons is valid */
			if (!FilterSingleEngine(eid)) continue;

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

		const Station *st = this->listview_mode ? NULL : Station::GetByTile(this->window_number);

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
		if (!this->eng_list.NeedRebuild()) return;
		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				this->GenerateBuildTrainList();
				this->eng_list.Compact();
				this->eng_list.RebuildDone();
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

		this->FilterEngineList();

		_internal_sort_order = this->descending_sort_order;
		EngList_Sort(&this->eng_list, _sorter[this->vehicle_type][this->sort_criteria]);

		this->eng_list.Compact();
		this->eng_list.RebuildDone();
	}

	void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_last_sort_order[this->vehicle_type] = this->descending_sort_order;
				this->eng_list.ForceRebuild();
				this->SetDirty();
				break;

			case BUILD_VEHICLE_WIDGET_LIST: {
				uint i = this->vscroll->GetScrolledRowFromWidget(pt.y, this, BUILD_VEHICLE_WIDGET_LIST);
				size_t num_items = this->eng_list.Length();
				this->sel_engine = (i < num_items) ? this->eng_list[i] : INVALID_ENGINE;
				this->SetDirty();
				if (click_count > 1 && !this->listview_mode) this->OnClick(pt, BUILD_VEHICLE_WIDGET_BUILD, 1);
				break;
			}

			case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN: { // Select sorting criteria dropdown menu
				uint32 hidden_mask = 0;
				/* Disable sorting by power or tractive effort when the original acceleration model for road vehicles is being used. */
				if (this->vehicle_type == VEH_ROAD &&
						_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) {
					SetBit(hidden_mask, 3); // power
					SetBit(hidden_mask, 4); // tractive effort
					SetBit(hidden_mask, 8); // power by running costs
				}
				/* Disable sorting by tractive effort when the original acceleration model for trains is being used. */
				if (this->vehicle_type == VEH_TRAIN &&
						_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL) {
					SetBit(hidden_mask, 4); // tractive effort
				}
				ShowDropDownMenu(this, _sort_listing[this->vehicle_type], this->sort_criteria, BUILD_VEHICLE_WIDGET_SORT_DROPDOWN, 0, hidden_mask);
				break;
			}

			case BUILD_VEHICLE_WIDGET_CARGO_FILTER_DROPDOWN: // Select cargo filtering criteria dropdown menu
				ShowDropDownMenu(this, this->cargo_filter_texts, this->cargo_filter_criteria, BUILD_VEHICLE_WIDGET_CARGO_FILTER_DROPDOWN, 0, 0);
				break;

			case BUILD_VEHICLE_WIDGET_BUILD: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != INVALID_ENGINE) {
					CommandCallback *callback = (this->vehicle_type == VEH_TRAIN && RailVehInfo(sel_eng)->railveh_type == RAILVEH_WAGON) ? CcBuildWagon : CcBuildPrimaryVehicle;
					DoCommandP(this->window_number, sel_eng, 0, GetCmdBuildVeh(this->vehicle_type), callback);
				}
				break;
			}

			case BUILD_VEHICLE_WIDGET_RENAME: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != INVALID_ENGINE) {
					this->rename_engine = sel_eng;
					SetDParam(0, sel_eng);
					ShowQueryString(STR_ENGINE_NAME, STR_QUERY_RENAME_TRAIN_TYPE_CAPTION + this->vehicle_type, MAX_LENGTH_ENGINE_NAME_CHARS, MAX_LENGTH_ENGINE_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				}
				break;
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		/* When switching to original acceleration model for road vehicles, clear the selected sort criteria if it is not available now. */
		if (this->vehicle_type == VEH_ROAD &&
				_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL &&
				this->sort_criteria > 7) {
			this->sort_criteria = 0;
			_last_sort_criteria[VEH_ROAD] = 0;
		}
		this->eng_list.ForceRebuild();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case BUILD_VEHICLE_WIDGET_CAPTION:
				if (this->vehicle_type == VEH_TRAIN && !this->listview_mode) {
					const RailtypeInfo *rti = GetRailTypeInfo(this->filter.railtype);
					SetDParam(0, rti->strings.build_caption);
				} else {
					SetDParam(0, (this->listview_mode ? STR_VEHICLE_LIST_AVAILABLE_TRAINS : STR_BUY_VEHICLE_TRAIN_ALL_CAPTION) + this->vehicle_type);
				}
				break;

			case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN:
				SetDParam(0, _sort_listing[this->vehicle_type][this->sort_criteria]);
				break;

			case BUILD_VEHICLE_WIDGET_CARGO_FILTER_DROPDOWN:
				SetDParam(0, this->cargo_filter_texts[this->cargo_filter_criteria]);
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case BUILD_VEHICLE_WIDGET_LIST:
				resize->height = GetEngineListHeight(this->vehicle_type);
				size->height = 3 * resize->height;
				break;

			case BUILD_VEHICLE_WIDGET_PANEL:
				size->height = this->details_height;
				break;

			case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + WD_SORTBUTTON_ARROW_WIDTH * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case BUILD_VEHICLE_WIDGET_LIST:
				DrawEngineList(this->vehicle_type, r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, &this->eng_list, this->vscroll->GetPosition(), min(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), this->eng_list.Length()), this->sel_engine, false, DEFAULT_GROUP);
				break;

			case BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING:
				this->DrawSortButtonState(BUILD_VEHICLE_WIDGET_SORT_ASSENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;
		}
	}

	virtual void OnPaint()
	{
		this->GenerateBuildList();
		this->vscroll->SetCount(this->eng_list.Length());

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			if (this->sel_engine != INVALID_ENGINE) {
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(BUILD_VEHICLE_WIDGET_PANEL);
				int text_end = DrawVehiclePurchaseInfo(nwi->pos_x + WD_FRAMETEXT_LEFT, nwi->pos_x + nwi->current_x - WD_FRAMETEXT_RIGHT,
						nwi->pos_y + WD_FRAMERECT_TOP, this->sel_engine);
				needed_height = max(needed_height, text_end - (int)nwi->pos_y + WD_FRAMERECT_BOTTOM);
			}
			if (needed_height != this->details_height) { // Details window are not high enough, enlarge them.
				int resize = needed_height - this->details_height;
				this->details_height = needed_height;
				this->ReInit(0, resize);
				return;
			}
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		DoCommandP(0, this->rename_engine, 0, CMD_RENAME_ENGINE | CMD_MSG(STR_ERROR_CAN_T_RENAME_TRAIN_TYPE + this->vehicle_type), NULL, str);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case BUILD_VEHICLE_WIDGET_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_last_sort_criteria[this->vehicle_type] = this->sort_criteria;
					this->eng_list.ForceRebuild();
				}
				break;

			case BUILD_VEHICLE_WIDGET_CARGO_FILTER_DROPDOWN: // Select a cargo filter criteria
				if (this->cargo_filter_criteria != index) {
					this->cargo_filter_criteria = index;
					_last_filter_criteria[this->vehicle_type] = this->cargo_filter[this->cargo_filter_criteria];
					/* deactivate filter if criteria is 'Show All', activate it otherwise */
					this->eng_list.SetFilterState(this->cargo_filter[this->cargo_filter_criteria] != CF_ANY);
					this->eng_list.ForceRebuild();
				}
				break;
		}
		this->SetDirty();
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, BUILD_VEHICLE_WIDGET_LIST);
		this->GetWidget<NWidgetCore>(BUILD_VEHICLE_WIDGET_LIST)->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}
};

static const WindowDesc _build_vehicle_desc(
	WDP_AUTO, 240, 268,
	WC_BUILD_VEHICLE, WC_NONE,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_build_vehicle_widgets, lengthof(_nested_build_vehicle_widgets)
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
