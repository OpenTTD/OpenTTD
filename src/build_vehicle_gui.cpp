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
#include "network/network.h"
#include "articulated_vehicles.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "group.h"
#include "string_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "widgets/dropdown_func.h"
#include "engine_gui.h"
#include "cargotype.h"
#include "core/geometry_func.hpp"
#include "autoreplace_func.h"
#include "engine_cmd.h"
#include "train_cmd.h"
#include "vehicle_cmd.h"
#include "zoom_func.h"

#include "widgets/build_vehicle_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Get the height of a single 'entry' in the engine lists.
 * @param type the vehicle type to get the height of
 * @return the height for the entry
 */
uint GetEngineListHeight(VehicleType type)
{
	return std::max<uint>(FONT_HEIGHT_NORMAL + WidgetDimensions::scaled.matrix.Vertical(), GetVehicleImageCellSize(type, EIT_PURCHASE).height);
}

static const NWidgetPart _nested_build_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_BV_CAPTION), SetDataTip(STR_WHITE_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_SORT_ASCENDING_DESCENDING), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_BV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BV_SHOW_HIDDEN_ENGINES),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_BV_CARGO_FILTER_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_TOOLTIP_FILTER_CRITERIA),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	/* Vehicle list. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_BV_LIST), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_BV_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BV_SCROLLBAR),
	EndContainer(),
	/* Panel with details. */
	NWidget(WWT_PANEL, COLOUR_GREY, WID_BV_PANEL), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	/* Build/rename buttons, resize button. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BV_BUILD_SEL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_BUILD), SetResize(1, 0), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_SHOW_HIDE), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_RENAME), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** Special cargo filter criteria */
static const CargoID CF_ANY     = CT_NO_REFIT;   ///< Show all vehicles independent of carried cargo (i.e. no filtering)
static const CargoID CF_NONE    = CT_INVALID;    ///< Show only vehicles which do not carry cargo (e.g. train engines)
static const CargoID CF_ENGINES = CT_AUTO_REFIT; ///< Show only engines (for rail vehicles only)

bool _engine_sort_direction; ///< \c false = descending, \c true = ascending.
byte _engine_sort_last_criteria[]       = {0, 0, 0, 0};                 ///< Last set sort criteria, for each vehicle type.
bool _engine_sort_last_order[]          = {false, false, false, false}; ///< Last set direction of the sort order, for each vehicle type.
bool _engine_sort_show_hidden_engines[] = {false, false, false, false}; ///< Last set 'show hidden engines' setting for each vehicle type.
static CargoID _engine_sort_last_cargo_criteria[] = {CF_ANY, CF_ANY, CF_ANY, CF_ANY}; ///< Last set filter criteria, for each vehicle type.

/**
 * Determines order of engines by engineID
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineNumberSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	int r = Engine::Get(a.engine_id)->list_position - Engine::Get(b.engine_id)->list_position;

	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by introduction date
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineIntroDateSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	const int va = Engine::Get(a.engine_id)->intro_date;
	const int vb = Engine::Get(b.engine_id)->intro_date;
	const int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/* cached values for EngineNameSorter to spare many GetString() calls */
static EngineID _last_engine[2] = { INVALID_ENGINE, INVALID_ENGINE };

/**
 * Determines order of engines by name
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineNameSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	static char     last_name[2][64] = { "", "" };

	if (a.engine_id != _last_engine[0]) {
		_last_engine[0] = a.engine_id;
		SetDParam(0, a.engine_id);
		GetString(last_name[0], STR_ENGINE_NAME, lastof(last_name[0]));
	}

	if (b.engine_id != _last_engine[1]) {
		_last_engine[1] = b.engine_id;
		SetDParam(0, b.engine_id);
		GetString(last_name[1], STR_ENGINE_NAME, lastof(last_name[1]));
	}

	int r = strnatcmp(last_name[0], last_name[1]); // Sort by name (natural sorting).

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by reliability
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineReliabilitySorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	const int va = Engine::Get(a.engine_id)->reliability;
	const int vb = Engine::Get(b.engine_id)->reliability;
	const int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by purchase cost
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineCostSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	Money va = Engine::Get(a.engine_id)->GetCost();
	Money vb = Engine::Get(b.engine_id)->GetCost();
	int r = ClampToI32(va - vb);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by speed
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineSpeedSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	int va = Engine::Get(a.engine_id)->GetDisplayMaxSpeed();
	int vb = Engine::Get(b.engine_id)->GetDisplayMaxSpeed();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by power
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EnginePowerSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	int va = Engine::Get(a.engine_id)->GetPower();
	int vb = Engine::Get(b.engine_id)->GetPower();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by tractive effort
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineTractiveEffortSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	int va = Engine::Get(a.engine_id)->GetDisplayMaxTractiveEffort();
	int vb = Engine::Get(b.engine_id)->GetDisplayMaxTractiveEffort();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by running costs
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineRunningCostSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	Money va = Engine::Get(a.engine_id)->GetRunningCost();
	Money vb = Engine::Get(b.engine_id)->GetRunningCost();
	int r = ClampToI32(va - vb);

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of engines by running costs
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EnginePowerVsRunningCostSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	const Engine *e_a = Engine::Get(a.engine_id);
	const Engine *e_b = Engine::Get(b.engine_id);
	uint p_a = e_a->GetPower();
	uint p_b = e_b->GetPower();
	Money r_a = e_a->GetRunningCost();
	Money r_b = e_b->GetRunningCost();
	/* Check if running cost is zero in one or both engines.
	 * If only one of them is zero then that one has higher value,
	 * else if both have zero cost then compare powers. */
	if (r_a == 0) {
		if (r_b == 0) {
			/* If it is ambiguous which to return go with their ID */
			if (p_a == p_b) return EngineNumberSorter(a, b);
			return _engine_sort_direction != (p_a < p_b);
		}
		return !_engine_sort_direction;
	}
	if (r_b == 0) return _engine_sort_direction;
	/* Using double for more precision when comparing close values.
	 * This shouldn't have any major effects in performance nor in keeping
	 * the game in sync between players since it's used in GUI only in client side */
	double v_a = (double)p_a / (double)r_a;
	double v_b = (double)p_b / (double)r_b;
	/* Use EngineID to sort if both have same power/running cost,
	 * since we want consistent sorting.
	 * Also if both have no power then sort with reverse of running cost to simulate
	 * previous sorting behaviour for wagons. */
	if (v_a == 0 && v_b == 0) return !EngineRunningCostSorter(a, b);
	if (v_a == v_b)  return EngineNumberSorter(a, b);
	return _engine_sort_direction != (v_a < v_b);
}

/* Train sorting functions */

/**
 * Determines order of train engines by capacity
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrainEngineCapacitySorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	const RailVehicleInfo *rvi_a = RailVehInfo(a.engine_id);
	const RailVehicleInfo *rvi_b = RailVehInfo(b.engine_id);

	int va = GetTotalCapacityOfArticulatedParts(a.engine_id) * (rvi_a->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int vb = GetTotalCapacityOfArticulatedParts(b.engine_id) * (rvi_b->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1);
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of train engines by engine / wagon
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool TrainEnginesThenWagonsSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	int val_a = (RailVehInfo(a.engine_id)->railveh_type == RAILVEH_WAGON ? 1 : 0);
	int val_b = (RailVehInfo(b.engine_id)->railveh_type == RAILVEH_WAGON ? 1 : 0);
	int r = val_a - val_b;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/* Road vehicle sorting functions */

/**
 * Determines order of road vehicles by capacity
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool RoadVehEngineCapacitySorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	int va = GetTotalCapacityOfArticulatedParts(a.engine_id);
	int vb = GetTotalCapacityOfArticulatedParts(b.engine_id);
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/* Ship vehicle sorting functions */

/**
 * Determines order of ships by capacity
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool ShipEngineCapacitySorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	const Engine *e_a = Engine::Get(a.engine_id);
	const Engine *e_b = Engine::Get(b.engine_id);

	int va = e_a->GetDisplayDefaultCapacity();
	int vb = e_b->GetDisplayDefaultCapacity();
	int r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/* Aircraft sorting functions */

/**
 * Determines order of aircraft by cargo
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool AircraftEngineCargoSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	const Engine *e_a = Engine::Get(a.engine_id);
	const Engine *e_b = Engine::Get(b.engine_id);

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
	return _engine_sort_direction ? r > 0 : r < 0;
}

/**
 * Determines order of aircraft by range.
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool AircraftRangeSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	uint16 r_a = Engine::Get(a.engine_id)->GetRange();
	uint16 r_b = Engine::Get(b.engine_id)->GetRange();

	int r = r_a - r_b;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/** Sort functions for the vehicle sort criteria, for each vehicle type. */
EngList_SortTypeFunction * const _engine_sort_functions[][11] = {{
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
	&AircraftRangeSorter,
}};

/** Dropdown menu strings for the vehicle sort criteria. */
const StringID _engine_sort_listing[][12] = {{
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
	STR_SORT_BY_RANGE,
	INVALID_STRING_ID
}};

/** Filters vehicles by cargo and engine (in case of rail vehicle). */
static bool CDECL CargoAndEngineFilter(const GUIEngineListItem *item, const CargoID cid)
{
	if (cid == CF_ANY) {
		return true;
	} else if (cid == CF_ENGINES) {
		return Engine::Get(item->engine_id)->GetPower() != 0;
	} else {
		CargoTypes refit_mask = GetUnionOfArticulatedRefitMasks(item->engine_id, true) & _standard_cargo_mask;
		return (cid == CF_NONE ? refit_mask == 0 : HasBit(refit_mask, cid));
	}
}

static GUIEngineList::FilterFunction * const _filter_funcs[] = {
	&CargoAndEngineFilter,
};

static uint GetCargoWeight(const CargoArray &cap, VehicleType vtype)
{
	uint weight = 0;
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (cap[c] != 0) {
			if (vtype == VEH_TRAIN) {
				weight += CargoSpec::Get(c)->WeightOfNUnitsInTrain(cap[c]);
			} else {
				weight += CargoSpec::Get(c)->WeightOfNUnits(cap[c]);
			}
		}
	}
	return weight;
}

static int DrawCargoCapacityInfo(int left, int right, int y, TestedEngineDetails &te, bool refittable)
{
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (te.all_capacities[c] == 0) continue;

		SetDParam(0, c);
		SetDParam(1, te.all_capacities[c]);
		SetDParam(2, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY);
		DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
		y += FONT_HEIGHT_NORMAL;
	}

	return y;
}

/* Draw rail wagon specific details */
static int DrawRailWagonPurchaseInfo(int left, int right, int y, EngineID engine_number, const RailVehicleInfo *rvi, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost */
	if (te.cost != 0) {
		SetDParam(0, e->GetCost() + te.cost);
		SetDParam(1, te.cost);
		DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT);
	} else {
		SetDParam(0, e->GetCost());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST);
	}
	y += FONT_HEIGHT_NORMAL;

	/* Wagon weight - (including cargo) */
	uint weight = e->GetDisplayWeight();
	SetDParam(0, weight);
	SetDParam(1, GetCargoWeight(te.all_capacities, VEH_TRAIN) + weight);
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
static int DrawRailEnginePurchaseInfo(int left, int right, int y, EngineID engine_number, const RailVehicleInfo *rvi, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase Cost - Engine weight */
	if (te.cost != 0) {
		SetDParam(0, e->GetCost() + te.cost);
		SetDParam(1, te.cost);
		SetDParam(2, e->GetDisplayWeight());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT_WEIGHT);
	} else {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayWeight());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST_WEIGHT);
	}
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
static int DrawRoadVehPurchaseInfo(int left, int right, int y, EngineID engine_number, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) {
		/* Purchase Cost */
		if (te.cost != 0) {
			SetDParam(0, e->GetCost() + te.cost);
			SetDParam(1, te.cost);
			DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT);
		} else {
			SetDParam(0, e->GetCost());
			DrawString(left, right, y, STR_PURCHASE_INFO_COST);
		}
		y += FONT_HEIGHT_NORMAL;

		/* Road vehicle weight - (including cargo) */
		int16 weight = e->GetDisplayWeight();
		SetDParam(0, weight);
		SetDParam(1, GetCargoWeight(te.all_capacities, VEH_ROAD) + weight);
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
		if (te.cost != 0) {
			SetDParam(0, e->GetCost() + te.cost);
			SetDParam(1, te.cost);
			SetDParam(2, e->GetDisplayMaxSpeed());
			DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT_SPEED);
		} else {
			SetDParam(0, e->GetCost());
			SetDParam(1, e->GetDisplayMaxSpeed());
			DrawString(left, right, y, STR_PURCHASE_INFO_COST_SPEED);
		}
		y += FONT_HEIGHT_NORMAL;
	}

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
	y += FONT_HEIGHT_NORMAL;

	return y;
}

/* Draw ship specific details */
static int DrawShipPurchaseInfo(int left, int right, int y, EngineID engine_number, bool refittable, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost - Max speed */
	uint raw_speed = e->GetDisplayMaxSpeed();
	uint ocean_speed = e->u.ship.ApplyWaterClassSpeedFrac(raw_speed, true);
	uint canal_speed = e->u.ship.ApplyWaterClassSpeedFrac(raw_speed, false);

	if (ocean_speed == canal_speed) {
		if (te.cost != 0) {
			SetDParam(0, e->GetCost() + te.cost);
			SetDParam(1, te.cost);
			SetDParam(2, ocean_speed);
			DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT_SPEED);
		} else {
			SetDParam(0, e->GetCost());
			SetDParam(1, ocean_speed);
			DrawString(left, right, y, STR_PURCHASE_INFO_COST_SPEED);
		}
		y += FONT_HEIGHT_NORMAL;
	} else {
		if (te.cost != 0) {
			SetDParam(0, e->GetCost() + te.cost);
			SetDParam(1, te.cost);
			DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT);
		} else {
			SetDParam(0, e->GetCost());
			DrawString(left, right, y, STR_PURCHASE_INFO_COST);
		}
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, ocean_speed);
		DrawString(left, right, y, STR_PURCHASE_INFO_SPEED_OCEAN);
		y += FONT_HEIGHT_NORMAL;

		SetDParam(0, canal_speed);
		DrawString(left, right, y, STR_PURCHASE_INFO_SPEED_CANAL);
		y += FONT_HEIGHT_NORMAL;
	}

	/* Cargo type + capacity */
	SetDParam(0, te.cargo);
	SetDParam(1, te.capacity);
	SetDParam(2, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY);
	DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
	y += FONT_HEIGHT_NORMAL;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
	y += FONT_HEIGHT_NORMAL;

	return y;
}

/**
 * Draw aircraft specific details in the buy window.
 * @param left Left edge of the window to draw in.
 * @param right Right edge of the window to draw in.
 * @param y Top of the area to draw in.
 * @param engine_number Engine to display.
 * @param refittable If set, the aircraft can be refitted.
 * @return Bottom of the used area.
 */
static int DrawAircraftPurchaseInfo(int left, int right, int y, EngineID engine_number, bool refittable, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost - Max speed */
	if (te.cost != 0) {
		SetDParam(0, e->GetCost() + te.cost);
		SetDParam(1, te.cost);
		SetDParam(2, e->GetDisplayMaxSpeed());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST_REFIT_SPEED);
	} else {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		DrawString(left, right, y, STR_PURCHASE_INFO_COST_SPEED);
	}
	y += FONT_HEIGHT_NORMAL;

	/* Cargo capacity */
	if (te.mail_capacity > 0) {
		SetDParam(0, te.cargo);
		SetDParam(1, te.capacity);
		SetDParam(2, CT_MAIL);
		SetDParam(3, te.mail_capacity);
		DrawString(left, right, y, STR_PURCHASE_INFO_AIRCRAFT_CAPACITY);
	} else {
		/* Note, if the default capacity is selected by the refit capacity
		 * callback, then the capacity shown is likely to be incorrect. */
		SetDParam(0, te.cargo);
		SetDParam(1, te.capacity);
		SetDParam(2, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY);
		DrawString(left, right, y, STR_PURCHASE_INFO_CAPACITY);
	}
	y += FONT_HEIGHT_NORMAL;

	/* Running cost */
	SetDParam(0, e->GetRunningCost());
	DrawString(left, right, y, STR_PURCHASE_INFO_RUNNINGCOST);
	y += FONT_HEIGHT_NORMAL;

	/* Aircraft type */
	SetDParam(0, e->GetAircraftTypeText());
	DrawString(left, right, y, STR_PURCHASE_INFO_AIRCRAFT_TYPE);
	y += FONT_HEIGHT_NORMAL;

	/* Aircraft range, if available. */
	uint16 range = e->GetRange();
	if (range != 0) {
		SetDParam(0, range);
		DrawString(left, right, y, STR_PURCHASE_INFO_AIRCRAFT_RANGE);
		y += FONT_HEIGHT_NORMAL;
	}

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
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_ADDITIONAL_TEXT, 0, 0, engine, nullptr);
	if (callback == CALLBACK_FAILED || callback == 0x400) return y;
	const GRFFile *grffile = Engine::Get(engine)->GetGRF();
	if (callback > 0x400) {
		ErrorUnknownCallbackResult(grffile->grfid, CBID_VEHICLE_ADDITIONAL_TEXT, callback);
		return y;
	}

	StartTextRefStackUsage(grffile, 6);
	uint result = DrawStringMultiLine(left, right, y, INT32_MAX, GetGRFStringID(grffile->grfid, 0xD000 + callback), TC_BLACK);
	StopTextRefStackUsage();
	return result;
}

void TestedEngineDetails::FillDefaultCapacities(const Engine *e)
{
	this->cargo = e->GetDefaultCargoType();
	if (e->type == VEH_TRAIN || e->type == VEH_ROAD) {
		this->all_capacities = GetCapacityOfArticulatedParts(e->index);
		this->capacity = this->all_capacities[this->cargo];
		this->mail_capacity = 0;
	} else {
		this->capacity = e->GetDisplayDefaultCapacity(&this->mail_capacity);
		this->all_capacities[this->cargo] = this->capacity;
		this->all_capacities[CT_MAIL] = this->mail_capacity;
	}
	if (this->all_capacities.GetCount() == 0) this->cargo = CT_INVALID;
}

/**
 * Draw the purchase info details of a vehicle at a given location.
 * @param left,right,y location where to draw the info
 * @param engine_number the engine of which to draw the info of
 * @return y after drawing all the text
 */
int DrawVehiclePurchaseInfo(int left, int right, int y, EngineID engine_number, TestedEngineDetails &te)
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
				y = DrawRailWagonPurchaseInfo(left, right, y, engine_number, &e->u.rail, te);
			} else {
				y = DrawRailEnginePurchaseInfo(left, right, y, engine_number, &e->u.rail, te);
			}
			articulated_cargo = true;
			break;

		case VEH_ROAD:
			y = DrawRoadVehPurchaseInfo(left, right, y, engine_number, te);
			articulated_cargo = true;
			break;

		case VEH_SHIP:
			y = DrawShipPurchaseInfo(left, right, y, engine_number, refittable, te);
			break;

		case VEH_AIRCRAFT:
			y = DrawAircraftPurchaseInfo(left, right, y, engine_number, refittable, te);
			break;
	}

	if (articulated_cargo) {
		/* Cargo type + capacity, or N/A */
		int new_y = DrawCargoCapacityInfo(left, right, y, te, refittable);

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

	if (refittable) y = ShowRefitOptionsList(left, right, y, engine_number);

	/* Additional text from NewGRF */
	y = ShowAdditionalText(left, right, y, engine_number);

	/* The NewGRF's name which the vehicle comes from */
	const GRFConfig *config = GetGRFConfig(e->GetGRFID());
	if (_settings_client.gui.show_newgrf_name && config != nullptr)
	{
		DrawString(left, right, y, config->GetName(), TC_BLACK);
		y += FONT_HEIGHT_NORMAL;
	}

	return y;
}

/**
 * Engine drawing loop
 * @param type Type of vehicle (VEH_*)
 * @param r The Rect of the list
 * @param eng_list What engines to draw
 * @param min where to start in the list
 * @param max where in the list to end
 * @param selected_id what engine to highlight as selected, if any
 * @param show_count Whether to show the amount of engines or not
 * @param selected_group the group to list the engines of
 */
void DrawEngineList(VehicleType type, const Rect &r, const GUIEngineList &eng_list, uint16 min, uint16 max, EngineID selected_id, bool show_count, GroupID selected_group)
{
	static const int sprite_y_offsets[] = { -1, -1, -2, -2 };

	/* Obligatory sanity checks! */
	assert(max <= eng_list.size());

	bool rtl = _current_text_dir == TD_RTL;
	int step_size = GetEngineListHeight(type);
	int sprite_left  = GetVehicleImageCellSize(type, EIT_PURCHASE).extend_left;
	int sprite_right = GetVehicleImageCellSize(type, EIT_PURCHASE).extend_right;
	int sprite_width = sprite_left + sprite_right;
	int circle_width = std::max(GetScaledSpriteSize(SPR_CIRCLE_FOLDED).width, GetScaledSpriteSize(SPR_CIRCLE_UNFOLDED).width);
	int linecolour = _colour_gradient[COLOUR_ORANGE][4];

	Rect ir      = r.WithHeight(step_size).Shrink(WidgetDimensions::scaled.matrix);
	int sprite_y_offset = ScaleSpriteTrad(sprite_y_offsets[type]) + ir.Height() / 2;

	Dimension replace_icon = {0, 0};
	int count_width = 0;
	if (show_count) {
		replace_icon = GetSpriteSize(SPR_GROUP_REPLACE_ACTIVE);
		SetDParamMaxDigits(0, 3, FS_SMALL);
		count_width = GetStringBoundingBox(STR_TINY_BLACK_COMA).width;
	}

	Rect tr = ir.Indent(circle_width + WidgetDimensions::scaled.hsep_normal + sprite_width + WidgetDimensions::scaled.hsep_wide, rtl); // Name position
	Rect cr = tr.Indent(replace_icon.width + WidgetDimensions::scaled.hsep_wide, !rtl).WithWidth(count_width, !rtl);  // Count position
	Rect rr = tr.WithWidth(replace_icon.width, !rtl);                                                                 // Replace icon position
	if (show_count) tr = tr.Indent(count_width + WidgetDimensions::scaled.hsep_normal + replace_icon.width + WidgetDimensions::scaled.hsep_wide, !rtl);

	int normal_text_y_offset = (ir.Height() - FONT_HEIGHT_NORMAL) / 2;
	int small_text_y_offset  = ir.Height() - FONT_HEIGHT_SMALL;
	int replace_icon_y_offset = (ir.Height() - replace_icon.height) / 2;

	int y = ir.top;
	for (; min < max; min++, y += step_size) {
		const auto &item = eng_list[min];
		uint indent       = item.indent * WidgetDimensions::scaled.hsep_indent;
		bool has_variants = (item.flags & EngineDisplayFlags::HasVariants) != EngineDisplayFlags::None;
		bool is_folded    = (item.flags & EngineDisplayFlags::IsFolded)    != EngineDisplayFlags::None;
		bool shaded       = (item.flags & EngineDisplayFlags::Shaded)      != EngineDisplayFlags::None;
		/* Note: num_engines is only used in the autoreplace GUI, so it is correct to use _local_company here. */
		const uint num_engines = GetGroupNumEngines(_local_company, selected_group, item.engine_id);

		const Engine *e = Engine::Get(item.engine_id);
		bool hidden = HasBit(e->company_hidden, _local_company);
		StringID str = hidden ? STR_HIDDEN_ENGINE_NAME : STR_ENGINE_NAME;
		TextColour tc = (item.engine_id == selected_id) ? TC_WHITE : (TC_NO_SHADE | ((hidden | shaded) ? TC_GREY : TC_BLACK));

		SetDParam(0, item.engine_id);
		Rect itr = tr.Indent(indent, rtl);
		DrawString(itr.left, itr.right, y + normal_text_y_offset, str, tc);
		int sprite_x = ir.Indent(indent + circle_width + WidgetDimensions::scaled.hsep_normal, rtl).WithWidth(sprite_width, rtl).left + sprite_left;
		DrawVehicleEngine(r.left, r.right, sprite_x, y + sprite_y_offset, item.engine_id, (show_count && num_engines == 0) ? PALETTE_CRASH : GetEnginePalette(item.engine_id, _local_company), EIT_PURCHASE);
		if (show_count) {
			SetDParam(0, num_engines);
			DrawString(cr.left, cr.right, y + small_text_y_offset, STR_TINY_BLACK_COMA, TC_FROMSTRING, SA_RIGHT | SA_FORCE);
			if (EngineHasReplacementForCompany(Company::Get(_local_company), item.engine_id, selected_group)) DrawSprite(SPR_GROUP_REPLACE_ACTIVE, num_engines == 0 ? PALETTE_CRASH : PAL_NONE, rr.left, y + replace_icon_y_offset);
		}
		if (has_variants) {
			Rect fr = ir.Indent(indent, rtl).WithWidth(circle_width, rtl);
			DrawSpriteIgnorePadding(is_folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED, PAL_NONE, {fr.left, y, fr.right, y + ir.Height() - 1}, false, SA_CENTER);
		}
		if (indent > 0) {
			/* Draw tree lines */
			Rect fr = ir.Indent(indent - WidgetDimensions::scaled.hsep_indent, rtl).WithWidth(circle_width, rtl);
			int ycenter = y + normal_text_y_offset + FONT_HEIGHT_NORMAL / 2;
			bool continues = (min + 1U) < eng_list.size() && eng_list[min + 1].indent == item.indent;
			GfxDrawLine(fr.left + circle_width / 2, y - WidgetDimensions::scaled.matrix.top, fr.left + circle_width / 2, continues ? y - WidgetDimensions::scaled.matrix.top + step_size - 1 : ycenter, linecolour, WidgetDimensions::scaled.fullbevel.top);
			GfxDrawLine(fr.left + circle_width / 2, ycenter, fr.right, ycenter, linecolour, WidgetDimensions::scaled.fullbevel.top);
		}
	}
}

/**
 * Display the dropdown for the vehicle sort criteria.
 * @param w Parent window (holds the dropdown button).
 * @param vehicle_type %Vehicle type being sorted.
 * @param selected Currently selected sort criterium.
 * @param button Widget button.
 */
void DisplayVehicleSortDropDown(Window *w, VehicleType vehicle_type, int selected, int button)
{
	uint32 hidden_mask = 0;
	/* Disable sorting by power or tractive effort when the original acceleration model for road vehicles is being used. */
	if (vehicle_type == VEH_ROAD && _settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) {
		SetBit(hidden_mask, 3); // power
		SetBit(hidden_mask, 4); // tractive effort
		SetBit(hidden_mask, 8); // power by running costs
	}
	/* Disable sorting by tractive effort when the original acceleration model for trains is being used. */
	if (vehicle_type == VEH_TRAIN && _settings_game.vehicle.train_acceleration_model == AM_ORIGINAL) {
		SetBit(hidden_mask, 4); // tractive effort
	}
	ShowDropDownMenu(w, _engine_sort_listing[vehicle_type], selected, button, 0, hidden_mask);
}

/** GUI for building vehicles. */
struct BuildVehicleWindow : Window {
	VehicleType vehicle_type;                   ///< Type of vehicles shown in the window.
	union {
		RailType railtype;   ///< Rail type to show, or #INVALID_RAILTYPE.
		RoadType roadtype;   ///< Road type to show, or #INVALID_ROADTYPE.
	} filter;                                   ///< Filter to apply.
	bool descending_sort_order;                 ///< Sort direction, @see _engine_sort_direction
	byte sort_criteria;                         ///< Current sort criterium.
	bool show_hidden_engines;                   ///< State of the 'show hidden engines' button.
	bool listview_mode;                         ///< If set, only display the available vehicles and do not show a 'build' button.
	EngineID sel_engine;                        ///< Currently selected engine, or #INVALID_ENGINE
	EngineID rename_engine;                     ///< Engine being renamed.
	GUIEngineList eng_list;
	CargoID cargo_filter[NUM_CARGO + 3];        ///< Available cargo filters; CargoID or CF_ANY or CF_NONE or CF_ENGINES
	StringID cargo_filter_texts[NUM_CARGO + 4]; ///< Texts for filter_cargo, terminated by INVALID_STRING_ID
	byte cargo_filter_criteria;                 ///< Selected cargo filter
	int details_height;                         ///< Minimal needed height of the details panels, in text lines (found so far).
	Scrollbar *vscroll;
	TestedEngineDetails te;                     ///< Tested cost and capacity after refit.

	void SetBuyVehicleText()
	{
		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_BV_BUILD);

		bool refit = this->sel_engine != INVALID_ENGINE && this->cargo_filter[this->cargo_filter_criteria] != CF_ANY && this->cargo_filter[this->cargo_filter_criteria] != CF_NONE;
		if (refit) refit = Engine::Get(this->sel_engine)->GetDefaultCargoType() != this->cargo_filter[this->cargo_filter_criteria];

		if (refit) {
			widget->widget_data = STR_BUY_VEHICLE_TRAIN_BUY_REFIT_VEHICLE_BUTTON + this->vehicle_type;
			widget->tool_tip    = STR_BUY_VEHICLE_TRAIN_BUY_REFIT_VEHICLE_TOOLTIP + this->vehicle_type;
		} else {
			widget->widget_data = STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_BUTTON + this->vehicle_type;
			widget->tool_tip    = STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_TOOLTIP + this->vehicle_type;
		}
	}

	void AddChildren(const GUIEngineList &source, EngineID parent, int indent)
	{
		for (const auto &item : source) {
			if (item.variant_id != parent || item.engine_id == parent) continue;

			const Engine *e = Engine::Get(item.engine_id);
			EngineDisplayFlags flags = item.flags;
			if (e->display_last_variant != INVALID_ENGINE) flags &= ~EngineDisplayFlags::Shaded;
			this->eng_list.emplace_back(e->display_last_variant == INVALID_ENGINE ? item.engine_id : e->display_last_variant, item.engine_id, flags, indent);

			/* Add variants if not folded */
			if ((item.flags & (EngineDisplayFlags::HasVariants | EngineDisplayFlags::IsFolded)) == EngineDisplayFlags::HasVariants) {
				/* Add this engine again as a child */
				if ((item.flags & EngineDisplayFlags::Shaded) == EngineDisplayFlags::None) {
					this->eng_list.emplace_back(item.engine_id, item.engine_id, EngineDisplayFlags::None, indent + 1);
				}
				AddChildren(source, item.engine_id, indent + 1);
			}
		}
	}

	BuildVehicleWindow(WindowDesc *desc, TileIndex tile, VehicleType type) : Window(desc)
	{
		this->vehicle_type = type;
		this->listview_mode = tile == INVALID_TILE;
		this->window_number = this->listview_mode ? (int)type : (int)tile;

		this->sel_engine = INVALID_ENGINE;

		this->sort_criteria         = _engine_sort_last_criteria[type];
		this->descending_sort_order = _engine_sort_last_order[type];
		this->show_hidden_engines   = _engine_sort_show_hidden_engines[type];

		this->UpdateFilterByTile();

		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_BV_SCROLLBAR);

		/* If we are just viewing the list of vehicles, we do not need the Build button.
		 * So we just hide it, and enlarge the Rename button by the now vacant place. */
		if (this->listview_mode) this->GetWidget<NWidgetStacked>(WID_BV_BUILD_SEL)->SetDisplayedPlane(SZSP_NONE);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_BV_LIST);
		widget->tool_tip = STR_BUY_VEHICLE_TRAIN_LIST_TOOLTIP + type;

		widget = this->GetWidget<NWidgetCore>(WID_BV_SHOW_HIDE);
		widget->tool_tip = STR_BUY_VEHICLE_TRAIN_HIDE_SHOW_TOGGLE_TOOLTIP + type;

		widget = this->GetWidget<NWidgetCore>(WID_BV_RENAME);
		widget->widget_data = STR_BUY_VEHICLE_TRAIN_RENAME_BUTTON + type;
		widget->tool_tip    = STR_BUY_VEHICLE_TRAIN_RENAME_TOOLTIP + type;

		widget = this->GetWidget<NWidgetCore>(WID_BV_SHOW_HIDDEN_ENGINES);
		widget->widget_data = STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN + type;
		widget->tool_tip    = STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP + type;
		widget->SetLowered(this->show_hidden_engines);

		this->details_height = ((this->vehicle_type == VEH_TRAIN) ? 10 : 9);

		this->FinishInitNested(tile == INVALID_TILE ? (int)type : (int)tile);

		this->owner = (tile != INVALID_TILE) ? GetTileOwner(tile) : _local_company;

		this->eng_list.ForceRebuild();
		this->GenerateBuildList(); // generate the list, since we need it in the next line

		/* Select the first unshaded engine in the list as default when opening the window */
		EngineID engine = INVALID_ENGINE;
		auto it = std::find_if(this->eng_list.begin(), this->eng_list.end(), [&](GUIEngineListItem &item){ return (item.flags & EngineDisplayFlags::Shaded) == EngineDisplayFlags::None; });
		if (it != this->eng_list.end()) engine = it->engine_id;
		this->SelectEngine(engine);
	}

	/** Set the filter type according to the depot type */
	void UpdateFilterByTile()
	{
		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				if (this->listview_mode) {
					this->filter.railtype = INVALID_RAILTYPE;
				} else {
					this->filter.railtype = GetRailType(this->window_number);
				}
				break;

			case VEH_ROAD:
				if (this->listview_mode) {
					this->filter.roadtype = INVALID_ROADTYPE;
				} else {
					this->filter.roadtype = GetRoadTypeRoad(this->window_number);
					if (this->filter.roadtype == INVALID_ROADTYPE) {
						this->filter.roadtype = GetRoadTypeTram(this->window_number);
					}
				}
				break;

			case VEH_SHIP:
			case VEH_AIRCRAFT:
				break;
		}
	}

	/** Populate the filter list and set the cargo filter criteria. */
	void SetCargoFilterArray()
	{
		uint filter_items = 0;

		/* Add item for disabling filtering. */
		this->cargo_filter[filter_items] = CF_ANY;
		this->cargo_filter_texts[filter_items] = STR_PURCHASE_INFO_ALL_TYPES;
		filter_items++;

		/* Specific filters for trains. */
		if (this->vehicle_type == VEH_TRAIN) {
			/* Add item for locomotives only in case of trains. */
			this->cargo_filter[filter_items] = CF_ENGINES;
			this->cargo_filter_texts[filter_items] = STR_PURCHASE_INFO_ENGINES_ONLY;
			filter_items++;

			/* Add item for vehicles not carrying anything, e.g. train engines.
			 * This could also be useful for eyecandy vehicles of other types, but is likely too confusing for joe, */
			this->cargo_filter[filter_items] = CF_NONE;
			this->cargo_filter_texts[filter_items] = STR_PURCHASE_INFO_NONE;
			filter_items++;
		}

		/* Collect available cargo types for filtering. */
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			this->cargo_filter[filter_items] = cs->Index();
			this->cargo_filter_texts[filter_items] = cs->name;
			filter_items++;
		}

		/* Terminate the filter list. */
		this->cargo_filter_texts[filter_items] = INVALID_STRING_ID;

		/* If not found, the cargo criteria will be set to all cargoes. */
		this->cargo_filter_criteria = 0;

		/* Find the last cargo filter criteria. */
		for (uint i = 0; i < filter_items; i++) {
			if (this->cargo_filter[i] == _engine_sort_last_cargo_criteria[this->vehicle_type]) {
				this->cargo_filter_criteria = i;
				break;
			}
		}

		this->eng_list.SetFilterFuncs(_filter_funcs);
		this->eng_list.SetFilterState(this->cargo_filter[this->cargo_filter_criteria] != CF_ANY);
	}

	void SelectEngine(EngineID engine)
	{
		CargoID cargo = this->cargo_filter[this->cargo_filter_criteria];
		if (cargo == CF_ANY) cargo = CF_NONE;

		this->sel_engine = engine;
		this->SetBuyVehicleText();

		if (this->sel_engine == INVALID_ENGINE) return;

		const Engine *e = Engine::Get(this->sel_engine);

		if (!this->listview_mode) {
			/* Query for cost and refitted capacity */
			auto [ret, veh_id, refit_capacity, refit_mail, cargo_capacities] = Command<CMD_BUILD_VEHICLE>::Do(DC_QUERY_COST, this->window_number, this->sel_engine, true, cargo, INVALID_CLIENT_ID);
			if (ret.Succeeded()) {
				this->te.cost          = ret.GetCost() - e->GetCost();
				this->te.capacity      = refit_capacity;
				this->te.mail_capacity = refit_mail;
				this->te.cargo         = (cargo == CT_INVALID) ? e->GetDefaultCargoType() : cargo;
				this->te.all_capacities = cargo_capacities;
				return;
			}
		}

		/* Purchase test was not possible or failed, fill in the defaults instead. */
		this->te.cost     = 0;
		this->te.FillDefaultCapacities(e);
	}

	void OnInit() override
	{
		this->SetCargoFilterArray();
	}

	/** Filter the engine list against the currently selected cargo filter */
	void FilterEngineList()
	{
		this->eng_list.Filter(this->cargo_filter[this->cargo_filter_criteria]);
		if (0 == this->eng_list.size()) { // no engine passed through the filter, invalidate the previously selected engine
			this->SelectEngine(INVALID_ENGINE);
		} else if (std::find(this->eng_list.begin(), this->eng_list.end(), this->sel_engine) == this->eng_list.end()) { // previously selected engine didn't pass the filter, select the first engine of the list
			this->SelectEngine(this->eng_list[0].engine_id);
		}
	}

	/** Filter a single engine */
	bool FilterSingleEngine(EngineID eid)
	{
		CargoID filter_type = this->cargo_filter[this->cargo_filter_criteria];
		GUIEngineListItem item = {eid, eid, EngineDisplayFlags::None, 0};
		return CargoAndEngineFilter(&item, filter_type);
	}

	/* Figure out what train EngineIDs to put in the list */
	void GenerateBuildTrainList(GUIEngineList &list)
	{
		std::vector<EngineID> variants;
		EngineID sel_id = INVALID_ENGINE;
		size_t num_engines = 0;

		list.clear();

		/* Make list of all available train engines and wagons.
		 * Also check to see if the previously selected engine is still available,
		 * and if not, reset selection to INVALID_ENGINE. This could be the case
		 * when engines become obsolete and are removed */
		for (const Engine *e : Engine::IterateType(VEH_TRAIN)) {
			if (!this->show_hidden_engines && e->IsHidden(_local_company)) continue;
			EngineID eid = e->index;
			const RailVehicleInfo *rvi = &e->u.rail;

			if (this->filter.railtype != INVALID_RAILTYPE && !HasPowerOnRail(rvi->railtype, this->filter.railtype)) continue;
			if (!IsEngineBuildable(eid, VEH_TRAIN, _local_company)) continue;

			/* Filter now! So num_engines and num_wagons is valid */
			if (!FilterSingleEngine(eid)) continue;

			list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);

			if (rvi->railveh_type != RAILVEH_WAGON) num_engines++;
			if (e->info.variant_id != eid && e->info.variant_id != INVALID_ENGINE) variants.push_back(e->info.variant_id);
			if (eid == this->sel_engine) sel_id = eid;
		}

		/* ensure primary engine of variant group is in list */
		for (const auto &variant : variants) {
			if (std::find(list.begin(), list.end(), variant) == list.end()) {
				const Engine *e = Engine::Get(variant);
				list.emplace_back(variant, e->info.variant_id, e->display_flags | EngineDisplayFlags::Shaded, 0);
				if (e->u.rail.railveh_type != RAILVEH_WAGON) num_engines++;
			}
		}

		this->SelectEngine(sel_id);

		/* invalidate cached values for name sorter - engine names could change */
		_last_engine[0] = _last_engine[1] = INVALID_ENGINE;

		/* make engines first, and then wagons, sorted by selected sort_criteria */
		_engine_sort_direction = false;
		EngList_Sort(&list, TrainEnginesThenWagonsSorter);

		/* and then sort engines */
		_engine_sort_direction = this->descending_sort_order;
		EngList_SortPartial(&list, _engine_sort_functions[0][this->sort_criteria], 0, num_engines);

		/* and finally sort wagons */
		EngList_SortPartial(&list, _engine_sort_functions[0][this->sort_criteria], num_engines, list.size() - num_engines);
	}

	/* Figure out what road vehicle EngineIDs to put in the list */
	void GenerateBuildRoadVehList()
	{
		EngineID sel_id = INVALID_ENGINE;

		this->eng_list.clear();

		for (const Engine *e : Engine::IterateType(VEH_ROAD)) {
			if (!this->show_hidden_engines && e->IsHidden(_local_company)) continue;
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_ROAD, _local_company)) continue;
			if (this->filter.roadtype != INVALID_ROADTYPE && !HasPowerOnRoad(e->u.road.roadtype, this->filter.roadtype)) continue;

			this->eng_list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);

			if (eid == this->sel_engine) sel_id = eid;
		}
		this->SelectEngine(sel_id);
	}

	/* Figure out what ship EngineIDs to put in the list */
	void GenerateBuildShipList()
	{
		EngineID sel_id = INVALID_ENGINE;
		this->eng_list.clear();

		for (const Engine *e : Engine::IterateType(VEH_SHIP)) {
			if (!this->show_hidden_engines && e->IsHidden(_local_company)) continue;
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_SHIP, _local_company)) continue;
			this->eng_list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);

			if (eid == this->sel_engine) sel_id = eid;
		}
		this->SelectEngine(sel_id);
	}

	/* Figure out what aircraft EngineIDs to put in the list */
	void GenerateBuildAircraftList()
	{
		EngineID sel_id = INVALID_ENGINE;

		this->eng_list.clear();

		const Station *st = this->listview_mode ? nullptr : Station::GetByTile(this->window_number);

		/* Make list of all available planes.
		 * Also check to see if the previously selected plane is still available,
		 * and if not, reset selection to INVALID_ENGINE. This could be the case
		 * when planes become obsolete and are removed */
		for (const Engine *e : Engine::IterateType(VEH_AIRCRAFT)) {
			if (!this->show_hidden_engines && e->IsHidden(_local_company)) continue;
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_AIRCRAFT, _local_company)) continue;
			/* First VEH_END window_numbers are fake to allow a window open for all different types at once */
			if (!this->listview_mode && !CanVehicleUseStation(eid, st)) continue;

			this->eng_list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);
			if (eid == this->sel_engine) sel_id = eid;
		}

		this->SelectEngine(sel_id);
	}

	/* Generate the list of vehicles */
	void GenerateBuildList()
	{
		if (!this->eng_list.NeedRebuild()) return;

		/* Update filter type in case the road/railtype of the depot got converted */
		this->UpdateFilterByTile();

		this->eng_list.clear();

		GUIEngineList list;

		switch (this->vehicle_type) {
			default: NOT_REACHED();
			case VEH_TRAIN:
				this->GenerateBuildTrainList(list);
				AddChildren(list, INVALID_ENGINE, 0);
				this->eng_list.shrink_to_fit();
				this->eng_list.RebuildDone();
				return;
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

		/* ensure primary engine of variant group is in list after filtering */
		std::vector<EngineID> variants;
		for (const auto &item : this->eng_list) {
			if (item.engine_id != item.variant_id && item.variant_id != INVALID_ENGINE) variants.push_back(item.variant_id);
		}
		for (const auto &variant : variants) {
			if (std::find(this->eng_list.begin(), this->eng_list.end(), variant) == this->eng_list.end()) {
				const Engine *e = Engine::Get(variant);
				this->eng_list.emplace_back(variant, e->info.variant_id, e->display_flags | EngineDisplayFlags::Shaded, 0);
			}
		}

		_engine_sort_direction = this->descending_sort_order;
		EngList_Sort(&this->eng_list, _engine_sort_functions[this->vehicle_type][this->sort_criteria]);

		this->eng_list.swap(list);
		AddChildren(list, INVALID_ENGINE, 0);
		this->eng_list.shrink_to_fit();
		this->eng_list.RebuildDone();
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_BV_SORT_ASCENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_engine_sort_last_order[this->vehicle_type] = this->descending_sort_order;
				this->eng_list.ForceRebuild();
				this->SetDirty();
				break;

			case WID_BV_SHOW_HIDDEN_ENGINES:
				this->show_hidden_engines ^= true;
				_engine_sort_show_hidden_engines[this->vehicle_type] = this->show_hidden_engines;
				this->eng_list.ForceRebuild();
				this->SetWidgetLoweredState(widget, this->show_hidden_engines);
				this->SetDirty();
				break;

			case WID_BV_LIST: {
				uint i = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_BV_LIST);
				size_t num_items = this->eng_list.size();
				EngineID e = INVALID_ENGINE;
				if (i < num_items) {
					const auto &item = this->eng_list[i];
					const Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.matrix).WithWidth(WidgetDimensions::scaled.hsep_indent * (item.indent + 1), _current_text_dir == TD_RTL);
					if ((item.flags & EngineDisplayFlags::HasVariants) != EngineDisplayFlags::None && IsInsideMM(r.left, r.right, pt.x)) {
						/* toggle folded flag on engine */
						assert(item.variant_id != INVALID_ENGINE);
						Engine *engine = Engine::Get(item.variant_id);
						engine->display_flags ^= EngineDisplayFlags::IsFolded;

						InvalidateWindowData(WC_REPLACE_VEHICLE, this->vehicle_type, 0); // Update the autoreplace window
						InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
						return;
					}
					if ((item.flags & EngineDisplayFlags::Shaded) == EngineDisplayFlags::None) e = item.engine_id;
				}
				this->SelectEngine(e);
				this->SetDirty();
				if (_ctrl_pressed) {
					this->OnClick(pt, WID_BV_SHOW_HIDE, 1);
				} else if (click_count > 1 && !this->listview_mode) {
					this->OnClick(pt, WID_BV_BUILD, 1);
				}
				break;
			}

			case WID_BV_SORT_DROPDOWN: // Select sorting criteria dropdown menu
				DisplayVehicleSortDropDown(this, this->vehicle_type, this->sort_criteria, WID_BV_SORT_DROPDOWN);
				break;

			case WID_BV_CARGO_FILTER_DROPDOWN: // Select cargo filtering criteria dropdown menu
				ShowDropDownMenu(this, this->cargo_filter_texts, this->cargo_filter_criteria, WID_BV_CARGO_FILTER_DROPDOWN, 0, 0);
				break;

			case WID_BV_SHOW_HIDE: {
				const Engine *e = (this->sel_engine == INVALID_ENGINE) ? nullptr : Engine::Get(this->sel_engine);
				if (e != nullptr) {
					Command<CMD_SET_VEHICLE_VISIBILITY>::Post(this->sel_engine, !e->IsHidden(_current_company));
				}
				break;
			}

			case WID_BV_BUILD: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != INVALID_ENGINE) {
					CargoID cargo = this->cargo_filter[this->cargo_filter_criteria];
					if (cargo == CF_ANY || cargo == CF_ENGINES) cargo = CF_NONE;
					if (this->vehicle_type == VEH_TRAIN && RailVehInfo(sel_eng)->railveh_type == RAILVEH_WAGON) {
						Command<CMD_BUILD_VEHICLE>::Post(GetCmdBuildVehMsg(this->vehicle_type), CcBuildWagon, this->window_number, sel_eng, true, cargo, INVALID_CLIENT_ID);
					} else {
						Command<CMD_BUILD_VEHICLE>::Post(GetCmdBuildVehMsg(this->vehicle_type), CcBuildPrimaryVehicle, this->window_number, sel_eng, true, cargo, INVALID_CLIENT_ID);
					}

					/* Update last used variant and refresh if necessary. */
					bool refresh = false;
					int recursion = 10; /* In case of infinite loop */
					for (Engine *e = Engine::Get(sel_eng); recursion > 0; e = Engine::Get(e->info.variant_id), --recursion) {
						refresh |= (e->display_last_variant != sel_eng);
						e->display_last_variant = sel_eng;
						if (e->info.variant_id == INVALID_ENGINE) break;
					}
					if (refresh) {
						InvalidateWindowData(WC_REPLACE_VEHICLE, this->vehicle_type, 0); // Update the autoreplace window
						InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
						return;
					}
				}
				break;
			}

			case WID_BV_RENAME: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != INVALID_ENGINE) {
					this->rename_engine = sel_eng;
					SetDParam(0, sel_eng);
					ShowQueryString(STR_ENGINE_NAME, STR_QUERY_RENAME_TRAIN_TYPE_CAPTION + this->vehicle_type, MAX_LENGTH_ENGINE_NAME_CHARS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				}
				break;
			}
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* When switching to original acceleration model for road vehicles, clear the selected sort criteria if it is not available now. */
		if (this->vehicle_type == VEH_ROAD &&
				_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL &&
				this->sort_criteria > 7) {
			this->sort_criteria = 0;
			_engine_sort_last_criteria[VEH_ROAD] = 0;
		}
		this->eng_list.ForceRebuild();
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_BV_CAPTION:
				if (this->vehicle_type == VEH_TRAIN && !this->listview_mode) {
					const RailtypeInfo *rti = GetRailTypeInfo(this->filter.railtype);
					SetDParam(0, rti->strings.build_caption);
				} else if (this->vehicle_type == VEH_ROAD && !this->listview_mode) {
					const RoadTypeInfo *rti = GetRoadTypeInfo(this->filter.roadtype);
					SetDParam(0, rti->strings.build_caption);
				} else {
					SetDParam(0, (this->listview_mode ? STR_VEHICLE_LIST_AVAILABLE_TRAINS : STR_BUY_VEHICLE_TRAIN_ALL_CAPTION) + this->vehicle_type);
				}
				break;

			case WID_BV_SORT_DROPDOWN:
				SetDParam(0, _engine_sort_listing[this->vehicle_type][this->sort_criteria]);
				break;

			case WID_BV_CARGO_FILTER_DROPDOWN:
				SetDParam(0, this->cargo_filter_texts[this->cargo_filter_criteria]);
				break;

			case WID_BV_SHOW_HIDE: {
				const Engine *e = (this->sel_engine == INVALID_ENGINE) ? nullptr : Engine::Get(this->sel_engine);
				if (e != nullptr && e->IsHidden(_local_company)) {
					SetDParam(0, STR_BUY_VEHICLE_TRAIN_SHOW_TOGGLE_BUTTON + this->vehicle_type);
				} else {
					SetDParam(0, STR_BUY_VEHICLE_TRAIN_HIDE_TOGGLE_BUTTON + this->vehicle_type);
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_BV_LIST:
				resize->height = GetEngineListHeight(this->vehicle_type);
				size->height = 3 * resize->height;
				size->width = std::max(size->width, GetVehicleImageCellSize(this->vehicle_type, EIT_PURCHASE).extend_left + GetVehicleImageCellSize(this->vehicle_type, EIT_PURCHASE).extend_right + 165) + padding.width;
				break;

			case WID_BV_PANEL:
				size->height = FONT_HEIGHT_NORMAL * this->details_height + padding.height;
				break;

			case WID_BV_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case WID_BV_BUILD:
				*size = GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_BUTTON + this->vehicle_type);
				*size = maxdim(*size, GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_BUY_REFIT_VEHICLE_BUTTON + this->vehicle_type));
				size->width += padding.width;
				size->height += padding.height;
				break;

			case WID_BV_SHOW_HIDE:
				*size = GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_HIDE_TOGGLE_BUTTON + this->vehicle_type);
				*size = maxdim(*size, GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_SHOW_TOGGLE_BUTTON + this->vehicle_type));
				size->width += padding.width;
				size->height += padding.height;
				break;
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (widget) {
			case WID_BV_LIST:
				DrawEngineList(
					this->vehicle_type,
					r,
					this->eng_list,
					this->vscroll->GetPosition(),
					static_cast<uint16>(std::min<size_t>(this->vscroll->GetPosition() + this->vscroll->GetCapacity(), this->eng_list.size())),
					this->sel_engine,
					false,
					DEFAULT_GROUP
				);
				break;

			case WID_BV_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_BV_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;
		}
	}

	void OnPaint() override
	{
		this->GenerateBuildList();
		this->vscroll->SetCount((uint)this->eng_list.size());

		this->SetWidgetsDisabledState(this->sel_engine == INVALID_ENGINE, WID_BV_SHOW_HIDE, WID_BV_BUILD, WIDGET_LIST_END);

		/* Disable renaming engines in network games if you are not the server. */
		this->SetWidgetDisabledState(WID_BV_RENAME, this->sel_engine == INVALID_ENGINE || (_networking && !_network_server));

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			if (this->sel_engine != INVALID_ENGINE) {
				const Rect r = this->GetWidget<NWidgetBase>(WID_BV_PANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect);
				int text_end = DrawVehiclePurchaseInfo(r.left, r.right, r.top, this->sel_engine, this->te);
				needed_height = std::max(needed_height, (text_end - r.top) / FONT_HEIGHT_NORMAL);
			}
			if (needed_height != this->details_height) { // Details window are not high enough, enlarge them.
				int resize = needed_height - this->details_height;
				this->details_height = needed_height;
				this->ReInit(0, resize * FONT_HEIGHT_NORMAL);
				return;
			}
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (str == nullptr) return;

		Command<CMD_RENAME_ENGINE>::Post(STR_ERROR_CAN_T_RENAME_TRAIN_TYPE + this->vehicle_type, this->rename_engine, str);
	}

	void OnDropdownSelect(int widget, int index) override
	{
		switch (widget) {
			case WID_BV_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_engine_sort_last_criteria[this->vehicle_type] = this->sort_criteria;
					this->eng_list.ForceRebuild();
				}
				break;

			case WID_BV_CARGO_FILTER_DROPDOWN: // Select a cargo filter criteria
				if (this->cargo_filter_criteria != index) {
					this->cargo_filter_criteria = index;
					_engine_sort_last_cargo_criteria[this->vehicle_type] = this->cargo_filter[this->cargo_filter_criteria];
					/* deactivate filter if criteria is 'Show All', activate it otherwise */
					this->eng_list.SetFilterState(this->cargo_filter[this->cargo_filter_criteria] != CF_ANY);
					this->eng_list.ForceRebuild();
					this->SelectEngine(this->sel_engine);
				}
				break;
		}
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BV_LIST);
	}
};

static WindowDesc _build_vehicle_desc(
	WDP_AUTO, "build_vehicle", 240, 268,
	WC_BUILD_VEHICLE, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_vehicle_widgets, lengthof(_nested_build_vehicle_widgets)
);

void ShowBuildVehicleWindow(TileIndex tile, VehicleType type)
{
	/* We want to be able to open both Available Train as Available Ships,
	 *  so if tile == INVALID_TILE (Available XXX Window), use 'type' as unique number.
	 *  As it always is a low value, it won't collide with any real tile
	 *  number. */
	uint num = (tile == INVALID_TILE) ? (int)type : (int)tile;

	assert(IsCompanyBuildableVehicleType(type));

	CloseWindowById(WC_BUILD_VEHICLE, num);

	new BuildVehicleWindow(&_build_vehicle_desc, tile, type);
}
