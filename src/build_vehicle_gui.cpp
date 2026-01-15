/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
#include "newgrf_badge.h"
#include "newgrf_badge_config.h"
#include "newgrf_badge_gui.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "group.h"
#include "string_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "timer/timer_game_calendar.h"
#include "vehicle_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "engine_gui.h"
#include "cargotype.h"
#include "core/geometry_func.hpp"
#include "autoreplace_func.h"
#include "engine_cmd.h"
#include "train_cmd.h"
#include "vehicle_cmd.h"
#include "zoom_func.h"
#include "querystring_gui.h"
#include "stringfilter_type.h"
#include "hotkeys.h"

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
	return std::max<uint>(GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.matrix.Vertical(), GetVehicleImageCellSize(type, EIT_PURCHASE).height);
}

static constexpr std::initializer_list<NWidgetPart> _nested_build_vehicle_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_BV_CAPTION), SetTextStyle(TC_WHITE),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
			NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_BV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BV_SHOW_HIDDEN_ENGINES),
			NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_BV_CARGO_FILTER_DROPDOWN), SetResize(1, 0), SetFill(1, 0), SetToolTip(STR_TOOLTIP_FILTER_CRITERIA),
			NWidget(WWT_IMGBTN, COLOUR_GREY, WID_BV_CONFIGURE_BADGES), SetAspect(WidgetDimensions::ASPECT_UP_DOWN_BUTTON), SetResize(0, 0), SetFill(0, 1), SetSpriteTip(SPR_EXTRA_MENU, STR_BADGE_CONFIG_MENU_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_EDITBOX, COLOUR_GREY, WID_BV_FILTER), SetResize(1, 0), SetFill(1, 0), SetPadding(2), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
		NWidget(NWID_VERTICAL, NWidContainerFlag{}, WID_BV_BADGE_FILTER),
		EndContainer(),
	EndContainer(),
	/* Vehicle list. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_BV_LIST), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0), SetScrollbar(WID_BV_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BV_SCROLLBAR),
	EndContainer(),
	/* Panel with details. */
	NWidget(WWT_PANEL, COLOUR_GREY, WID_BV_PANEL), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	/* Build/rename buttons, resize button. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BV_BUILD_SEL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_BUILD), SetResize(1, 0), SetFill(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_SHOW_HIDE), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BV_RENAME), SetResize(1, 0), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};


bool _engine_sort_direction; ///< \c false = descending, \c true = ascending.
uint8_t _engine_sort_last_criteria[]       = {0, 0, 0, 0};                 ///< Last set sort criteria, for each vehicle type.
bool _engine_sort_last_order[]          = {false, false, false, false}; ///< Last set direction of the sort order, for each vehicle type.
bool _engine_sort_show_hidden_engines[] = {false, false, false, false}; ///< Last set 'show hidden engines' setting for each vehicle type.
static CargoType _engine_sort_last_cargo_criteria[] = {CargoFilterCriteria::CF_ANY, CargoFilterCriteria::CF_ANY, CargoFilterCriteria::CF_ANY, CargoFilterCriteria::CF_ANY}; ///< Last set filter criteria, for each vehicle type.

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
	const auto va = Engine::Get(a.engine_id)->intro_date;
	const auto vb = Engine::Get(b.engine_id)->intro_date;
	const auto r = va - vb;

	/* Use EngineID to sort instead since we want consistent sorting */
	if (r == 0) return EngineNumberSorter(a, b);
	return _engine_sort_direction ? r > 0 : r < 0;
}

/* cached values for EngineNameSorter to spare many GetString() calls */
static EngineID _last_engine[2] = { EngineID::Invalid(), EngineID::Invalid() };

/**
 * Determines order of engines by name
 * @param a first engine to compare
 * @param b second engine to compare
 * @return for descending order: returns true if a < b. Vice versa for ascending order
 */
static bool EngineNameSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	static std::string last_name[2] = { {}, {} };

	if (a.engine_id != _last_engine[0]) {
		_last_engine[0] = a.engine_id;
		last_name[0] = GetString(STR_ENGINE_NAME, PackEngineNameDParam(a.engine_id, EngineNameContext::PurchaseList));
	}

	if (b.engine_id != _last_engine[1]) {
		_last_engine[1] = b.engine_id;
		last_name[1] = GetString(STR_ENGINE_NAME, PackEngineNameDParam(b.engine_id, EngineNameContext::PurchaseList));
	}

	int r = StrNaturalCompare(last_name[0], last_name[1]); // Sort by name (natural sorting).

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
	int r = ClampTo<int32_t>(va - vb);

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
	int r = ClampTo<int32_t>(va - vb);

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
	if (v_a == 0 && v_b == 0) return EngineRunningCostSorter(b, a);
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

	uint16_t mail_a, mail_b;
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
	uint16_t r_a = Engine::Get(a.engine_id)->GetRange();
	uint16_t r_b = Engine::Get(b.engine_id)->GetRange();

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
const std::initializer_list<const StringID> _engine_sort_listing[] = {{
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
}};

/** Filters vehicles by cargo and engine (in case of rail vehicle). */
static bool CargoAndEngineFilter(const GUIEngineListItem *item, const CargoType cargo_type)
{
	if (cargo_type == CargoFilterCriteria::CF_ANY) {
		return true;
	} else if (cargo_type == CargoFilterCriteria::CF_ENGINES) {
		return Engine::Get(item->engine_id)->GetPower() != 0;
	} else {
		CargoTypes refit_mask = GetUnionOfArticulatedRefitMasks(item->engine_id, true) & _standard_cargo_mask;
		return (cargo_type == CargoFilterCriteria::CF_NONE ? refit_mask == 0 : HasBit(refit_mask, cargo_type));
	}
}

static GUIEngineList::FilterFunction * const _engine_filter_funcs[] = {
	&CargoAndEngineFilter,
};

static uint GetCargoWeight(const CargoArray &cap, VehicleType vtype)
{
	uint weight = 0;
	for (CargoType cargo = 0; cargo < NUM_CARGO; ++cargo) {
		if (cap[cargo] != 0) {
			if (vtype == VEH_TRAIN) {
				weight += CargoSpec::Get(cargo)->WeightOfNUnitsInTrain(cap[cargo]);
			} else {
				weight += CargoSpec::Get(cargo)->WeightOfNUnits(cap[cargo]);
			}
		}
	}
	return weight;
}

static int DrawCargoCapacityInfo(int left, int right, int y, TestedEngineDetails &te, bool refittable)
{
	for (const CargoSpec *cs : _sorted_cargo_specs) {
		CargoType cargo_type = cs->Index();
		if (te.all_capacities[cargo_type] == 0) continue;

		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_CAPACITY, cargo_type, te.all_capacities[cargo_type], refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY));
		y += GetCharacterHeight(FS_NORMAL);
	}

	return y;
}

/* Draw rail wagon specific details */
static int DrawRailWagonPurchaseInfo(int left, int right, int y, EngineID engine_number, const RailVehicleInfo *rvi, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost */
	if (te.cost != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT, e->GetCost() + te.cost, te.cost));
	} else {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST, e->GetCost()));
	}
	y += GetCharacterHeight(FS_NORMAL);

	/* Wagon weight - (including cargo) */
	uint weight = e->GetDisplayWeight();
	DrawString(left, right, y,
			GetString(STR_PURCHASE_INFO_WEIGHT_CWEIGHT, weight, GetCargoWeight(te.all_capacities, VEH_TRAIN) + weight));
	y += GetCharacterHeight(FS_NORMAL);

	/* Wagon speed limit, displayed if above zero */
	if (_settings_game.vehicle.wagon_speed_limits) {
		uint max_speed = e->GetDisplayMaxSpeed();
		if (max_speed > 0) {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED, PackVelocity(max_speed, e->type)));
			y += GetCharacterHeight(FS_NORMAL);
		}
	}

	/* Running cost */
	if (rvi->running_cost_class != Price::Invalid) {
		DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_RUNNINGCOST_PERIOD : STR_PURCHASE_INFO_RUNNINGCOST_YEAR, e->GetRunningCost()));
		y += GetCharacterHeight(FS_NORMAL);
	}

	return y;
}

/* Draw locomotive specific details */
static int DrawRailEnginePurchaseInfo(int left, int right, int y, EngineID engine_number, const RailVehicleInfo *rvi, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase Cost - Engine weight */
	if (te.cost != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT_WEIGHT, e->GetCost() + te.cost, te.cost, e->GetDisplayWeight()));
	} else {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_WEIGHT, e->GetCost(), e->GetDisplayWeight()));
	}
	y += GetCharacterHeight(FS_NORMAL);

	/* Supported rail types */
	std::string railtypes{};
	std::string_view list_separator = GetListSeparator();

	for (const auto &rt : _sorted_railtypes) {
		if (!rvi->railtypes.Test(rt)) continue;

		if (!railtypes.empty()) railtypes += list_separator;
		AppendStringInPlace(railtypes, GetRailTypeInfo(rt)->strings.name);
	}
	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_RAILTYPES, railtypes));
	y += GetCharacterHeight(FS_NORMAL);

	/* Max speed - Engine power */
	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED_POWER, PackVelocity(e->GetDisplayMaxSpeed(), e->type), e->GetPower()));
	y += GetCharacterHeight(FS_NORMAL);

	/* Max tractive effort - not applicable if old acceleration or maglev */
	if (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL) {
		bool is_maglev = true;
		for (RailType rt : rvi->railtypes) {
			is_maglev &= GetRailTypeInfo(rt)->acceleration_type == VehicleAccelerationModel::Maglev;
		}
		if (!is_maglev) {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_MAX_TE, e->GetDisplayMaxTractiveEffort()));
			y += GetCharacterHeight(FS_NORMAL);
		}
	}

	/* Running cost */
	if (rvi->running_cost_class != Price::Invalid) {
		DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_RUNNINGCOST_PERIOD : STR_PURCHASE_INFO_RUNNINGCOST_YEAR, e->GetRunningCost()));
		y += GetCharacterHeight(FS_NORMAL);
	}

	/* Powered wagons power - Powered wagons extra weight */
	if (rvi->pow_wag_power != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_PWAGPOWER_PWAGWEIGHT, rvi->pow_wag_power, rvi->pow_wag_weight));
		y += GetCharacterHeight(FS_NORMAL);
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
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT, e->GetCost() + te.cost, te.cost));
		} else {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST, e->GetCost()));
		}
		y += GetCharacterHeight(FS_NORMAL);

		/* Road vehicle weight - (including cargo) */
		int16_t weight = e->GetDisplayWeight();
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_WEIGHT_CWEIGHT, weight, GetCargoWeight(te.all_capacities, VEH_ROAD) + weight));
		y += GetCharacterHeight(FS_NORMAL);

		/* Max speed - Engine power */
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED_POWER, PackVelocity(e->GetDisplayMaxSpeed(), e->type), e->GetPower()));
		y += GetCharacterHeight(FS_NORMAL);

		/* Max tractive effort */
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_MAX_TE, e->GetDisplayMaxTractiveEffort()));
		y += GetCharacterHeight(FS_NORMAL);
	} else {
		/* Purchase cost - Max speed */
		if (te.cost != 0) {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT_SPEED, e->GetCost() + te.cost, te.cost, PackVelocity(e->GetDisplayMaxSpeed(), e->type)));
		} else {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_SPEED, e->GetCost(), PackVelocity(e->GetDisplayMaxSpeed(), e->type)));
		}
		y += GetCharacterHeight(FS_NORMAL);
	}

	/* Running cost */
	DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_RUNNINGCOST_PERIOD : STR_PURCHASE_INFO_RUNNINGCOST_YEAR, e->GetRunningCost()));
	y += GetCharacterHeight(FS_NORMAL);

	return y;
}

/* Draw ship specific details */
static int DrawShipPurchaseInfo(int left, int right, int y, EngineID engine_number, bool refittable, TestedEngineDetails &te)
{
	const Engine *e = Engine::Get(engine_number);

	/* Purchase cost - Max speed */
	uint raw_speed = e->GetDisplayMaxSpeed();
	uint ocean_speed = e->VehInfo<ShipVehicleInfo>().ApplyWaterClassSpeedFrac(raw_speed, true);
	uint canal_speed = e->VehInfo<ShipVehicleInfo>().ApplyWaterClassSpeedFrac(raw_speed, false);

	if (ocean_speed == canal_speed) {
		if (te.cost != 0) {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT_SPEED, e->GetCost() + te.cost, te.cost, PackVelocity(ocean_speed, e->type)));
		} else {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_SPEED, e->GetCost(), PackVelocity(ocean_speed, e->type)));
		}
		y += GetCharacterHeight(FS_NORMAL);
	} else {
		if (te.cost != 0) {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT, e->GetCost() + te.cost, te.cost));
		} else {
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST, e->GetCost()));
		}
		y += GetCharacterHeight(FS_NORMAL);

		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED_OCEAN, PackVelocity(ocean_speed, e->type)));
		y += GetCharacterHeight(FS_NORMAL);

		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_SPEED_CANAL, PackVelocity(canal_speed, e->type)));
		y += GetCharacterHeight(FS_NORMAL);
	}

	/* Cargo type + capacity */
	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_CAPACITY, te.cargo, te.capacity, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY));
	y += GetCharacterHeight(FS_NORMAL);

	/* Running cost */
	DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_RUNNINGCOST_PERIOD : STR_PURCHASE_INFO_RUNNINGCOST_YEAR, e->GetRunningCost()));
	y += GetCharacterHeight(FS_NORMAL);

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
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_REFIT_SPEED, e->GetCost() + te.cost, te.cost, PackVelocity(e->GetDisplayMaxSpeed(), e->type)));
	} else {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_COST_SPEED, e->GetCost(), PackVelocity(e->GetDisplayMaxSpeed(), e->type)));
	}
	y += GetCharacterHeight(FS_NORMAL);

	/* Cargo capacity */
	if (te.mail_capacity > 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_AIRCRAFT_CAPACITY, te.cargo, te.capacity, GetCargoTypeByLabel(CT_MAIL), te.mail_capacity));
	} else {
		/* Note, if the default capacity is selected by the refit capacity
		 * callback, then the capacity shown is likely to be incorrect. */
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_CAPACITY, te.cargo, te.capacity, refittable ? STR_PURCHASE_INFO_REFITTABLE : STR_EMPTY));
	}
	y += GetCharacterHeight(FS_NORMAL);

	/* Running cost */
	DrawString(left, right, y, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_PURCHASE_INFO_RUNNINGCOST_PERIOD : STR_PURCHASE_INFO_RUNNINGCOST_YEAR, e->GetRunningCost()));
	y += GetCharacterHeight(FS_NORMAL);

	/* Aircraft type */
	DrawString(left, right, y, GetString(STR_PURCHASE_INFO_AIRCRAFT_TYPE, e->GetAircraftTypeText()));
	y += GetCharacterHeight(FS_NORMAL);

	/* Aircraft range, if available. */
	uint16_t range = e->GetRange();
	if (range != 0) {
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_AIRCRAFT_RANGE, range));
		y += GetCharacterHeight(FS_NORMAL);
	}

	return y;
}


/**
 * Try to get the NewGRF engine additional text callback as an optional std::string.
 * @param engine The engine whose additional text to get.
 * @return The std::string if present, otherwise std::nullopt.
 */
static std::optional<std::string> GetNewGRFAdditionalText(EngineID engine)
{
	std::array<int32_t, 16> regs100;
	uint16_t callback = GetVehicleCallback(CBID_VEHICLE_ADDITIONAL_TEXT, 0, 0, engine, nullptr, regs100);
	if (callback == CALLBACK_FAILED || callback == 0x400) return std::nullopt;
	const GRFFile *grffile = Engine::Get(engine)->GetGRF();
	assert(grffile != nullptr);
	if (callback == 0x40F) {
		return GetGRFStringWithTextStack(grffile, static_cast<GRFStringID>(regs100[0]), std::span{regs100}.subspan(1));
	}
	if (callback > 0x400) {
		ErrorUnknownCallbackResult(grffile->grfid, CBID_VEHICLE_ADDITIONAL_TEXT, callback);
		return std::nullopt;
	}

	return GetGRFStringWithTextStack(grffile, GRFSTR_MISC_GRF_TEXT + callback, regs100);
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
	auto text = GetNewGRFAdditionalText(engine);
	if (!text) return y;
	return DrawStringMultiLine(left, right, y, INT32_MAX, *text, TC_BLACK);
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
		if (IsValidCargoType(GetCargoTypeByLabel(CT_MAIL))) {
			this->all_capacities[GetCargoTypeByLabel(CT_MAIL)] = this->mail_capacity;
		} else {
			this->mail_capacity = 0;
		}
	}
	if (this->all_capacities.GetCount() == 0) this->cargo = INVALID_CARGO;
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
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(e->intro_date);
	bool refittable = IsArticulatedVehicleRefittable(engine_number);
	bool articulated_cargo = false;

	switch (e->type) {
		default: NOT_REACHED();
		case VEH_TRAIN:
			if (e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON) {
				y = DrawRailWagonPurchaseInfo(left, right, y, engine_number, &e->VehInfo<RailVehicleInfo>(), te);
			} else {
				y = DrawRailEnginePurchaseInfo(left, right, y, engine_number, &e->VehInfo<RailVehicleInfo>(), te);
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
			DrawString(left, right, y, GetString(STR_PURCHASE_INFO_CAPACITY, INVALID_CARGO, 0, STR_EMPTY));
			y += GetCharacterHeight(FS_NORMAL);
		} else {
			y = new_y;
		}
	}

	/* Draw details that apply to all types except rail wagons. */
	if (e->type != VEH_TRAIN || e->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON) {
		/* Design date - Life length */
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_DESIGNED_LIFE, ymd.year, TimerGameCalendar::DateToYear(e->GetLifeLengthInDays())));
		y += GetCharacterHeight(FS_NORMAL);

		/* Reliability */
		DrawString(left, right, y, GetString(STR_PURCHASE_INFO_RELIABILITY, ToPercent16(e->reliability)));
		y += GetCharacterHeight(FS_NORMAL);
	}

	if (refittable) y = ShowRefitOptionsList(left, right, y, engine_number);

	y = DrawBadgeNameList({left, y, right, INT16_MAX}, e->badges, static_cast<GrfSpecFeature>(GSF_TRAINS + e->type));

	/* Additional text from NewGRF */
	y = ShowAdditionalText(left, right, y, engine_number);

	/* The NewGRF's name which the vehicle comes from */
	const GRFConfig *config = GetGRFConfig(e->GetGRFID());
	if (_settings_client.gui.show_newgrf_name && config != nullptr)
	{
		DrawString(left, right, y, config->GetName(), TC_BLACK);
		y += GetCharacterHeight(FS_NORMAL);
	}

	return y;
}

static void DrawEngineBadgeColumn(const Rect &r, int column_group, const GUIBadgeClasses &badge_classes, const Engine *e, PaletteID remap)
{
	DrawBadgeColumn(r, column_group, badge_classes, e->badges, static_cast<GrfSpecFeature>(GSF_TRAINS + e->type), e->info.base_intro, remap);
}

/**
 * Engine drawing loop
 * @param type Type of vehicle (VEH_*)
 * @param r The Rect of the list
 * @param eng_list What engines to draw
 * @param sb Scrollbar of list.
 * @param selected_id what engine to highlight as selected, if any
 * @param show_count Whether to show the amount of engines or not
 * @param selected_group the group to list the engines of
 */
void DrawEngineList(VehicleType type, const Rect &r, const GUIEngineList &eng_list, const Scrollbar &sb, EngineID selected_id, bool show_count, GroupID selected_group, const GUIBadgeClasses &badge_classes)
{
	static const std::array<int8_t, VehicleType::VEH_COMPANY_END> sprite_y_offsets = { 0, 0, -1, -1 };

	auto [first, last] = sb.GetVisibleRangeIterators(eng_list);

	bool rtl = _current_text_dir == TD_RTL;
	int step_size = GetEngineListHeight(type);
	int sprite_left  = GetVehicleImageCellSize(type, EIT_PURCHASE).extend_left;
	int sprite_right = GetVehicleImageCellSize(type, EIT_PURCHASE).extend_right;
	int sprite_width = sprite_left + sprite_right;
	int circle_width = std::max(GetScaledSpriteSize(SPR_CIRCLE_FOLDED).width, GetScaledSpriteSize(SPR_CIRCLE_UNFOLDED).width);
	PixelColour linecolour = GetColourGradient(COLOUR_ORANGE, SHADE_NORMAL);

	auto badge_column_widths = badge_classes.GetColumnWidths();

	Rect ir = r.WithHeight(step_size).Shrink(WidgetDimensions::scaled.matrix, RectPadding::zero);
	int sprite_y_offset = ScaleSpriteTrad(sprite_y_offsets[type]) + ir.Height() / 2;

	Dimension replace_icon = {0, 0};
	int count_width = 0;
	if (show_count) {
		replace_icon = GetSpriteSize(SPR_GROUP_REPLACE_ACTIVE);

		uint biggest_num_engines = 0;
		for (auto it = first; it != last; ++it) {
			const uint num_engines = GetGroupNumEngines(_local_company, selected_group, it->engine_id);
			biggest_num_engines = std::max(biggest_num_engines, num_engines);
		}

		count_width = GetStringBoundingBox(GetString(STR_JUST_COMMA, biggest_num_engines), FS_SMALL).width;
	}

	const int text_row_height = ir.Shrink(WidgetDimensions::scaled.matrix).Height();
	const int normal_text_y_offset = (text_row_height - GetCharacterHeight(FS_NORMAL)) / 2;
	const int small_text_y_offset  = text_row_height - GetCharacterHeight(FS_SMALL);

	const int offset = (rtl ? -circle_width : circle_width) / 2;
	const int level_width = rtl ? -WidgetDimensions::scaled.hsep_indent : WidgetDimensions::scaled.hsep_indent;

	for (auto it = first; it != last; ++it) {
		const auto &item = *it;
		const Engine *e = Engine::Get(item.engine_id);

		uint indent       = item.indent * WidgetDimensions::scaled.hsep_indent;
		bool has_variants = item.flags.Test(EngineDisplayFlag::HasVariants);
		bool is_folded    = item.flags.Test(EngineDisplayFlag::IsFolded);
		bool shaded       = item.flags.Test(EngineDisplayFlag::Shaded);

		Rect textr = ir.Shrink(WidgetDimensions::scaled.matrix);
		Rect tr = ir.Indent(indent, rtl);

		if (item.indent > 0) {
			/* Draw tree continuation lines. */
			int tx = (rtl ? ir.right : ir.left) + offset;
			for (uint lvl = 1; lvl <= item.indent; ++lvl) {
				if (HasBit(item.level_mask, lvl)) GfxDrawLine(tx, ir.top, tx, ir.bottom, linecolour, WidgetDimensions::scaled.fullbevel.top);
				if (lvl < item.indent) tx += level_width;
			}
			/* Draw our node in the tree. */
			int ycentre = CentreBounds(textr.top, textr.bottom, WidgetDimensions::scaled.fullbevel.top);
			if (!HasBit(item.level_mask, item.indent)) GfxDrawLine(tx, ir.top, tx, ycentre, linecolour, WidgetDimensions::scaled.fullbevel.top);
			GfxDrawLine(tx, ycentre, tx + offset - (rtl ? -1 : 1), ycentre, linecolour, WidgetDimensions::scaled.fullbevel.top);
		}

		if (has_variants) {
			Rect fr = tr.WithWidth(circle_width, rtl);
			DrawSpriteIgnorePadding(is_folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED, PAL_NONE, fr.WithY(textr), SA_CENTER);
		}

		tr = tr.Indent(circle_width + WidgetDimensions::scaled.hsep_normal, rtl);

		/* Note: num_engines is only used in the autoreplace GUI, so it is correct to use _local_company here. */
		const uint num_engines = GetGroupNumEngines(_local_company, selected_group, item.engine_id);
		const PaletteID pal = (show_count && num_engines == 0) ? PALETTE_CRASH : GetEnginePalette(item.engine_id, _local_company);

		if (badge_column_widths.size() >= 1 && badge_column_widths[0] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[0], rtl);
			DrawEngineBadgeColumn(br, 0, badge_classes, e, pal);
			tr = tr.Indent(badge_column_widths[0], rtl);
		}

		int sprite_x = tr.WithWidth(sprite_width, rtl).left + sprite_left;
		DrawVehicleEngine(r.left, r.right, sprite_x, tr.top + sprite_y_offset, item.engine_id, pal, EIT_PURCHASE);

		tr = tr.Indent(sprite_width + WidgetDimensions::scaled.hsep_wide, rtl);

		if (badge_column_widths.size() >= 2 && badge_column_widths[1] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[1], rtl);
			DrawEngineBadgeColumn(br, 1, badge_classes, e, pal);
			tr = tr.Indent(badge_column_widths[1], rtl);
		}

		if (show_count) {
			/* Rect for replace-protection icon. */
			Rect rr = tr.WithWidth(replace_icon.width, !rtl);
			tr = tr.Indent(replace_icon.width + WidgetDimensions::scaled.hsep_normal, !rtl);
			/* Rect for engine type count text. */
			Rect cr = tr.WithWidth(count_width, !rtl);
			tr = tr.Indent(count_width + WidgetDimensions::scaled.hsep_normal, !rtl);

			DrawString(cr.left, cr.right, textr.top + small_text_y_offset, GetString(STR_JUST_COMMA, num_engines), TC_BLACK, SA_RIGHT | SA_FORCE, false, FS_SMALL);

			if (EngineHasReplacementForCompany(Company::Get(_local_company), item.engine_id, selected_group)) {
				DrawSpriteIgnorePadding(SPR_GROUP_REPLACE_ACTIVE, num_engines == 0 ? PALETTE_CRASH : PAL_NONE, rr, SA_CENTER);
			}
		}

		if (badge_column_widths.size() >= 3 && badge_column_widths[2] > 0) {
			Rect br = tr.WithWidth(badge_column_widths[2], !rtl).Indent(WidgetDimensions::scaled.hsep_wide, rtl);
			DrawEngineBadgeColumn(br, 2, badge_classes, e, pal);
			tr = tr.Indent(badge_column_widths[2], !rtl);
		}

		bool hidden = e->company_hidden.Test(_local_company);
		StringID str = hidden ? STR_HIDDEN_ENGINE_NAME : STR_ENGINE_NAME;
		TextColour tc = (item.engine_id == selected_id) ? TC_WHITE : ((hidden | shaded) ? (TC_GREY | TC_FORCED | TC_NO_SHADE) : TC_BLACK);

		/* If the count is visible then this is part of in-use autoreplace list. */
		auto engine_name = PackEngineNameDParam(item.engine_id, show_count ? EngineNameContext::AutoreplaceVehicleInUse : EngineNameContext::PurchaseList, item.indent);
		DrawString(tr.left, tr.right, textr.top + normal_text_y_offset,GetString(str, engine_name), tc);

		ir = ir.Translate(0, step_size);
	}
}

/**
 * Display the dropdown for the vehicle sort criteria.
 * @param w Parent window (holds the dropdown button).
 * @param vehicle_type %Vehicle type being sorted.
 * @param selected Currently selected sort criterium.
 * @param button Widget button.
 */
void DisplayVehicleSortDropDown(Window *w, VehicleType vehicle_type, int selected, WidgetID button)
{
	uint32_t hidden_mask = 0;
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

/**
 * Add children to GUI engine list to build a hierarchical tree.
 * @param dst Destination list.
 * @param src Source list.
 * @param parent Current tree parent (set by self with recursion).
 * @param indent Current tree indentation level (set by self with recursion).
 */
void GUIEngineListAddChildren(GUIEngineList &dst, const GUIEngineList &src, EngineID parent, uint8_t indent)
{
	for (const auto &item : src) {
		if (item.variant_id != parent || item.engine_id == parent) continue;

		const Engine *e = Engine::Get(item.engine_id);
		EngineDisplayFlags flags = item.flags;
		if (e->display_last_variant != EngineID::Invalid()) flags.Reset(EngineDisplayFlag::Shaded);
		dst.emplace_back(e->display_last_variant == EngineID::Invalid() ? item.engine_id : e->display_last_variant, item.engine_id, flags, indent);

		/* Add variants if not folded */
		if (item.flags.Test(EngineDisplayFlag::HasVariants) && !item.flags.Test(EngineDisplayFlag::IsFolded)) {
			/* Add this engine again as a child */
			if (!item.flags.Test(EngineDisplayFlag::Shaded)) {
				dst.emplace_back(item.engine_id, item.engine_id, EngineDisplayFlags{}, indent + 1);
			}
			GUIEngineListAddChildren(dst, src, item.engine_id, indent + 1);
		}
	}

	if (indent > 0 || dst.empty()) return;

	/* Hierarchy is complete, traverse in reverse to find where indentation levels continue. */
	uint16_t level_mask = 0;
	for (auto it = std::rbegin(dst); std::next(it) != std::rend(dst); ++it) {
		auto next_it = std::next(it);
		SB(level_mask, it->indent, 1, it->indent <= next_it->indent);
		next_it->level_mask = level_mask;
	}
}

/** GUI for building vehicles. */
struct BuildVehicleWindow : Window {
	VehicleType vehicle_type = VEH_INVALID; ///< Type of vehicles shown in the window.
	union {
		RailType railtype;   ///< Rail type to show, or #INVALID_RAILTYPE.
		RoadType roadtype;   ///< Road type to show, or #INVALID_ROADTYPE.
	} filter{}; ///< Filter to apply.
	bool descending_sort_order = false; ///< Sort direction, @see _engine_sort_direction
	uint8_t sort_criteria = 0; ///< Current sort criterium.
	bool show_hidden_engines = false; ///< State of the 'show hidden engines' button.
	bool listview_mode = false; ///< If set, only display the available vehicles and do not show a 'build' button.
	EngineID sel_engine = EngineID::Invalid(); ///< Currently selected engine, or #EngineID::Invalid()
	EngineID rename_engine = EngineID::Invalid(); ///< Engine being renamed.
	GUIEngineList eng_list{};
	CargoType cargo_filter_criteria{}; ///< Selected cargo filter
	int details_height = 0; ///< Minimal needed height of the details panels, in text lines (found so far).
	Scrollbar *vscroll = nullptr;
	TestedEngineDetails te{}; ///< Tested cost and capacity after refit.
	GUIBadgeClasses badge_classes{};

	static constexpr int BADGE_COLUMNS = 3; ///< Number of columns available for badges (0 = left of image, 1 = between image and name, 2 = after name)

	StringFilter string_filter{}; ///< Filter for vehicle name
	QueryString vehicle_editbox; ///< Filter editbox

	std::pair<WidgetID, WidgetID> badge_filters{}; ///< First and last widgets IDs of badge filters.
	BadgeFilterChoices badge_filter_choices{};

	void SetBuyVehicleText()
	{
		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_BV_BUILD);

		bool refit = this->sel_engine != EngineID::Invalid() && this->cargo_filter_criteria != CargoFilterCriteria::CF_ANY && this->cargo_filter_criteria != CargoFilterCriteria::CF_NONE && this->cargo_filter_criteria != CargoFilterCriteria::CF_ENGINES;
		if (refit) refit = Engine::Get(this->sel_engine)->GetDefaultCargoType() != this->cargo_filter_criteria;

		if (refit) {
			widget->SetStringTip(STR_BUY_VEHICLE_TRAIN_BUY_REFIT_VEHICLE_BUTTON + this->vehicle_type, STR_BUY_VEHICLE_TRAIN_BUY_REFIT_VEHICLE_TOOLTIP + this->vehicle_type);
		} else {
			widget->SetStringTip(STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_BUTTON + this->vehicle_type, STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_TOOLTIP + this->vehicle_type);
		}
	}

	BuildVehicleWindow(WindowDesc &desc, TileIndex tile, VehicleType type) : Window(desc), vehicle_editbox(MAX_LENGTH_VEHICLE_NAME_CHARS * MAX_CHAR_LENGTH, MAX_LENGTH_VEHICLE_NAME_CHARS)
	{
		this->vehicle_type = type;
		this->listview_mode = tile == INVALID_TILE;
		this->window_number = this->listview_mode ? (int)type : tile.base();

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
		widget->SetToolTip(STR_BUY_VEHICLE_TRAIN_LIST_TOOLTIP + type);

		widget = this->GetWidget<NWidgetCore>(WID_BV_SHOW_HIDE);
		widget->SetToolTip(STR_BUY_VEHICLE_TRAIN_HIDE_SHOW_TOGGLE_TOOLTIP + type);

		widget = this->GetWidget<NWidgetCore>(WID_BV_RENAME);
		widget->SetStringTip(STR_BUY_VEHICLE_TRAIN_RENAME_BUTTON + type, STR_BUY_VEHICLE_TRAIN_RENAME_TOOLTIP + type);

		widget = this->GetWidget<NWidgetCore>(WID_BV_SHOW_HIDDEN_ENGINES);
		widget->SetStringTip(STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN + type, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP + type);
		widget->SetLowered(this->show_hidden_engines);

		this->details_height = ((this->vehicle_type == VEH_TRAIN) ? 10 : 9);

		if (tile == INVALID_TILE) {
			this->FinishInitNested(type);
		} else {
			this->FinishInitNested(tile);
		}

		this->querystrings[WID_BV_FILTER] = &this->vehicle_editbox;
		this->vehicle_editbox.cancel_button = QueryString::ACTION_CLEAR;

		this->owner = (tile != INVALID_TILE) ? GetTileOwner(tile) : _local_company;

		this->eng_list.ForceRebuild();
		this->GenerateBuildList(); // generate the list, since we need it in the next line

		/* Select the first unshaded engine in the list as default when opening the window */
		EngineID engine = EngineID::Invalid();
		auto it = std::ranges::find_if(this->eng_list, [](const GUIEngineListItem &item) { return !item.flags.Test(EngineDisplayFlag::Shaded); });
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

	StringID GetCargoFilterLabel(CargoType cargo_type) const
	{
		switch (cargo_type) {
			case CargoFilterCriteria::CF_ANY: return STR_PURCHASE_INFO_ALL_TYPES;
			case CargoFilterCriteria::CF_ENGINES: return STR_PURCHASE_INFO_ENGINES_ONLY;
			case CargoFilterCriteria::CF_NONE: return STR_PURCHASE_INFO_NONE;
			default: return CargoSpec::Get(cargo_type)->name;
		}
	}

	/** Populate the filter list and set the cargo filter criteria. */
	void SetCargoFilterArray()
	{
		/* Set the last cargo filter criteria. */
		this->cargo_filter_criteria = _engine_sort_last_cargo_criteria[this->vehicle_type];
		if (this->cargo_filter_criteria < NUM_CARGO && !HasBit(_standard_cargo_mask, this->cargo_filter_criteria)) this->cargo_filter_criteria = CargoFilterCriteria::CF_ANY;

		this->eng_list.SetFilterFuncs(_engine_filter_funcs);
		this->eng_list.SetFilterState(this->cargo_filter_criteria != CargoFilterCriteria::CF_ANY);
	}

	void SelectEngine(EngineID engine)
	{
		CargoType cargo = this->cargo_filter_criteria;
		if (cargo == CargoFilterCriteria::CF_ANY || cargo == CargoFilterCriteria::CF_ENGINES || cargo == CargoFilterCriteria::CF_NONE) cargo = INVALID_CARGO;

		this->sel_engine = engine;
		this->SetBuyVehicleText();

		if (this->sel_engine == EngineID::Invalid()) return;

		const Engine *e = Engine::Get(this->sel_engine);

		if (!this->listview_mode) {
			/* Query for cost and refitted capacity */
			auto [ret, veh_id, refit_capacity, refit_mail, cargo_capacities] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, TileIndex(this->window_number), this->sel_engine, true, cargo, INVALID_CLIENT_ID);
			if (ret.Succeeded()) {
				this->te.cost          = ret.GetCost() - e->GetCost();
				this->te.capacity      = refit_capacity;
				this->te.mail_capacity = refit_mail;
				this->te.cargo         = !IsValidCargoType(cargo) ? e->GetDefaultCargoType() : cargo;
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
		this->badge_classes = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_TRAINS + this->vehicle_type));
		this->SetCargoFilterArray();

		this->badge_filters = AddBadgeDropdownFilters(this, WID_BV_BADGE_FILTER, WID_BV_BADGE_FILTER, COLOUR_GREY, static_cast<GrfSpecFeature>(GSF_TRAINS + this->vehicle_type));

		this->widget_lookup.clear();
		this->nested_root->FillWidgetLookup(this->widget_lookup);
	}

	/** Filter the engine list against the currently selected cargo filter */
	void FilterEngineList()
	{
		this->eng_list.Filter(this->cargo_filter_criteria);
		if (0 == this->eng_list.size()) { // no engine passed through the filter, invalidate the previously selected engine
			this->SelectEngine(EngineID::Invalid());
		} else if (std::ranges::find(this->eng_list, this->sel_engine, &GUIEngineListItem::engine_id) == this->eng_list.end()) { // previously selected engine didn't pass the filter, select the first engine of the list
			this->SelectEngine(this->eng_list[0].engine_id);
		}
	}

	/** Filter a single engine */
	bool FilterSingleEngine(EngineID eid)
	{
		GUIEngineListItem item = {eid, eid, EngineDisplayFlags{}, 0};
		return CargoAndEngineFilter(&item, this->cargo_filter_criteria);
	}

	/** Filter by name and NewGRF extra text */
	bool FilterByText(const Engine *e)
	{
		/* Do not filter if the filter text box is empty */
		if (this->string_filter.IsEmpty()) return true;

		/* Filter engine name */
		this->string_filter.ResetState();
		this->string_filter.AddLine(GetString(STR_ENGINE_NAME, PackEngineNameDParam(e->index, EngineNameContext::PurchaseList)));

		/* Filter NewGRF extra text */
		auto text = GetNewGRFAdditionalText(e->index);
		if (text) this->string_filter.AddLine(*text);

		return this->string_filter.GetState();
	}

	/* Figure out what train EngineIDs to put in the list */
	void GenerateBuildTrainList(GUIEngineList &list)
	{
		FlatSet<EngineID> variants;
		EngineID sel_id = EngineID::Invalid();
		size_t num_engines = 0;

		list.clear();

		BadgeTextFilter btf(this->string_filter, GSF_TRAINS);
		BadgeDropdownFilter bdf(this->badge_filter_choices);

		/* Make list of all available train engines and wagons.
		 * Also check to see if the previously selected engine is still available,
		 * and if not, reset selection to EngineID::Invalid(). This could be the case
		 * when engines become obsolete and are removed */
		for (const Engine *e : Engine::IterateType(VEH_TRAIN)) {
			if (!this->show_hidden_engines && e->IsVariantHidden(_local_company)) continue;
			EngineID eid = e->index;
			const RailVehicleInfo *rvi = &e->VehInfo<RailVehicleInfo>();

			if (this->filter.railtype != INVALID_RAILTYPE && !HasPowerOnRail(rvi->railtypes, this->filter.railtype)) continue;
			if (!IsEngineBuildable(eid, VEH_TRAIN, _local_company)) continue;

			/* Filter now! So num_engines and num_wagons is valid */
			if (!FilterSingleEngine(eid)) continue;

			if (!bdf.Filter(e->badges)) continue;

			/* Filter by name or NewGRF extra text */
			if (!FilterByText(e) && !btf.Filter(e->badges)) continue;

			list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);

			if (rvi->railveh_type != RAILVEH_WAGON) num_engines++;

			/* Add all parent variants of this engine to the variant list */
			EngineID parent = e->info.variant_id;
			while (parent != EngineID::Invalid() && variants.insert(parent).second) {
				parent = Engine::Get(parent)->info.variant_id;
			}

			if (eid == this->sel_engine) sel_id = eid;
		}

		/* ensure primary engine of variant group is in list */
		for (const auto &variant : variants) {
			if (std::ranges::find(list, variant, &GUIEngineListItem::engine_id) == list.end()) {
				const Engine *e = Engine::Get(variant);
				list.emplace_back(variant, e->info.variant_id, e->display_flags | EngineDisplayFlag::Shaded, 0);
				if (e->VehInfo<RailVehicleInfo>().railveh_type != RAILVEH_WAGON) num_engines++;
			}
		}

		this->SelectEngine(sel_id);

		/* invalidate cached values for name sorter - engine names could change */
		_last_engine[0] = _last_engine[1] = EngineID::Invalid();

		/* make engines first, and then wagons, sorted by selected sort_criteria */
		_engine_sort_direction = false;
		EngList_Sort(list, TrainEnginesThenWagonsSorter);

		/* and then sort engines */
		_engine_sort_direction = this->descending_sort_order;
		EngList_SortPartial(list, _engine_sort_functions[0][this->sort_criteria], 0, num_engines);

		/* and finally sort wagons */
		EngList_SortPartial(list, _engine_sort_functions[0][this->sort_criteria], num_engines, list.size() - num_engines);
	}

	/* Figure out what road vehicle EngineIDs to put in the list */
	void GenerateBuildRoadVehList()
	{
		EngineID sel_id = EngineID::Invalid();

		this->eng_list.clear();

		BadgeTextFilter btf(this->string_filter, GSF_ROADVEHICLES);
		BadgeDropdownFilter bdf(this->badge_filter_choices);

		for (const Engine *e : Engine::IterateType(VEH_ROAD)) {
			if (!this->show_hidden_engines && e->IsVariantHidden(_local_company)) continue;
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_ROAD, _local_company)) continue;
			if (this->filter.roadtype != INVALID_ROADTYPE && !HasPowerOnRoad(e->VehInfo<RoadVehicleInfo>().roadtype, this->filter.roadtype)) continue;
			if (!bdf.Filter(e->badges)) continue;

			/* Filter by name or NewGRF extra text */
			if (!FilterByText(e) && !btf.Filter(e->badges)) continue;

			this->eng_list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);

			if (eid == this->sel_engine) sel_id = eid;
		}
		this->SelectEngine(sel_id);
	}

	/* Figure out what ship EngineIDs to put in the list */
	void GenerateBuildShipList()
	{
		EngineID sel_id = EngineID::Invalid();
		this->eng_list.clear();

		BadgeTextFilter btf(this->string_filter, GSF_SHIPS);
		BadgeDropdownFilter bdf(this->badge_filter_choices);

		for (const Engine *e : Engine::IterateType(VEH_SHIP)) {
			if (!this->show_hidden_engines && e->IsVariantHidden(_local_company)) continue;
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_SHIP, _local_company)) continue;
			if (!bdf.Filter(e->badges)) continue;

			/* Filter by name or NewGRF extra text */
			if (!FilterByText(e) && !btf.Filter(e->badges)) continue;

			this->eng_list.emplace_back(eid, e->info.variant_id, e->display_flags, 0);

			if (eid == this->sel_engine) sel_id = eid;
		}
		this->SelectEngine(sel_id);
	}

	/* Figure out what aircraft EngineIDs to put in the list */
	void GenerateBuildAircraftList()
	{
		EngineID sel_id = EngineID::Invalid();

		this->eng_list.clear();

		const Station *st = this->listview_mode ? nullptr : Station::GetByTile(TileIndex(this->window_number));

		BadgeTextFilter btf(this->string_filter, GSF_AIRCRAFT);
		BadgeDropdownFilter bdf(this->badge_filter_choices);

		/* Make list of all available planes.
		 * Also check to see if the previously selected plane is still available,
		 * and if not, reset selection to EngineID::Invalid(). This could be the case
		 * when planes become obsolete and are removed */
		for (const Engine *e : Engine::IterateType(VEH_AIRCRAFT)) {
			if (!this->show_hidden_engines && e->IsVariantHidden(_local_company)) continue;
			EngineID eid = e->index;
			if (!IsEngineBuildable(eid, VEH_AIRCRAFT, _local_company)) continue;
			/* First VEH_END window_numbers are fake to allow a window open for all different types at once */
			if (!this->listview_mode && !CanVehicleUseStation(eid, st)) continue;
			if (!bdf.Filter(e->badges)) continue;

			/* Filter by name or NewGRF extra text */
			if (!FilterByText(e) && !btf.Filter(e->badges)) continue;

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
				GUIEngineListAddChildren(this->eng_list, list);
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
		FlatSet<EngineID> variants;
		for (const auto &item : this->eng_list) {
			EngineID parent = item.variant_id;
			while (parent != EngineID::Invalid() && variants.insert(parent).second) {
				parent = Engine::Get(parent)->info.variant_id;
			}
		}

		for (const auto &variant : variants) {
			if (std::ranges::find(this->eng_list, variant, &GUIEngineListItem::engine_id) == this->eng_list.end()) {
				const Engine *e = Engine::Get(variant);
				this->eng_list.emplace_back(variant, e->info.variant_id, e->display_flags | EngineDisplayFlag::Shaded, 0);
			}
		}

		_engine_sort_direction = this->descending_sort_order;
		EngList_Sort(this->eng_list, _engine_sort_functions[this->vehicle_type][this->sort_criteria]);

		this->eng_list.swap(list);
		GUIEngineListAddChildren(this->eng_list, list, EngineID::Invalid(), 0);
		this->eng_list.RebuildDone();
	}

	DropDownList BuildCargoDropDownList() const
	{
		DropDownList list;

		/* Add item for disabling filtering. */
		list.push_back(MakeDropDownListStringItem(this->GetCargoFilterLabel(CargoFilterCriteria::CF_ANY), CargoFilterCriteria::CF_ANY));
		/* Specific filters for trains. */
		if (this->vehicle_type == VEH_TRAIN) {
			/* Add item for locomotives only in case of trains. */
			list.push_back(MakeDropDownListStringItem(this->GetCargoFilterLabel(CargoFilterCriteria::CF_ENGINES), CargoFilterCriteria::CF_ENGINES));
			/* Add item for vehicles not carrying anything, e.g. train engines.
			 * This could also be useful for eyecandy vehicles of other types, but is likely too confusing for joe, */
			list.push_back(MakeDropDownListStringItem(this->GetCargoFilterLabel(CargoFilterCriteria::CF_NONE), CargoFilterCriteria::CF_NONE));
		}

		/* Add cargos */
		Dimension d = GetLargestCargoIconSize();
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			list.push_back(MakeDropDownListIconItem(d, cs->GetCargoIcon(), PAL_NONE, cs->name, cs->Index()));
		}

		return list;
	}

	DropDownList BuildBadgeConfigurationList() const
	{
		static const auto separators = {STR_BADGE_CONFIG_PREVIEW, STR_BADGE_CONFIG_NAME};
		return BuildBadgeClassConfigurationList(this->badge_classes, BADGE_COLUMNS, separators, COLOUR_GREY);
	}

	void BuildVehicle()
	{
		EngineID sel_eng = this->sel_engine;
		if (sel_eng == EngineID::Invalid()) return;

		CargoType cargo = this->cargo_filter_criteria;
		if (cargo == CargoFilterCriteria::CF_ANY || cargo == CargoFilterCriteria::CF_ENGINES || cargo == CargoFilterCriteria::CF_NONE) cargo = INVALID_CARGO;
		if (this->vehicle_type == VEH_TRAIN && RailVehInfo(sel_eng)->railveh_type == RAILVEH_WAGON) {
			Command<Commands::BuildVehicle>::Post(GetCmdBuildVehMsg(this->vehicle_type), CcBuildWagon, TileIndex(this->window_number), sel_eng, true, cargo, INVALID_CLIENT_ID);
		} else {
			Command<Commands::BuildVehicle>::Post(GetCmdBuildVehMsg(this->vehicle_type), CcBuildPrimaryVehicle, TileIndex(this->window_number), sel_eng, true, cargo, INVALID_CLIENT_ID);
		}

		/* Update last used variant in hierarchy and refresh if necessary. */
		bool refresh = false;
		EngineID parent = sel_eng;
		while (parent != EngineID::Invalid()) {
			Engine *e = Engine::Get(parent);
			refresh |= (e->display_last_variant != sel_eng);
			e->display_last_variant = sel_eng;
			parent = e->info.variant_id;
		}

		if (refresh) {
			InvalidateWindowData(WC_REPLACE_VEHICLE, this->vehicle_type, 0); // Update the autoreplace window
			InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
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
				EngineID e = EngineID::Invalid();
				const auto it = this->vscroll->GetScrolledItemFromWidget(this->eng_list, pt.y, this, WID_BV_LIST);
				if (it != this->eng_list.end()) {
					const auto &item = *it;
					const Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.matrix).WithWidth(WidgetDimensions::scaled.hsep_indent * (item.indent + 1), _current_text_dir == TD_RTL);
					if (item.flags.Test(EngineDisplayFlag::HasVariants) && IsInsideMM(r.left, r.right, pt.x)) {
						/* toggle folded flag on engine */
						assert(item.variant_id != EngineID::Invalid());
						Engine *engine = Engine::Get(item.variant_id);
						engine->display_flags.Flip(EngineDisplayFlag::IsFolded);

						InvalidateWindowData(WC_REPLACE_VEHICLE, this->vehicle_type, 0); // Update the autoreplace window
						InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
						return;
					}
					if (!item.flags.Test(EngineDisplayFlag::Shaded)) e = item.engine_id;
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
				ShowDropDownList(this, this->BuildCargoDropDownList(), this->cargo_filter_criteria, widget);
				break;

			case WID_BV_CONFIGURE_BADGES:
				if (this->badge_classes.GetClasses().empty()) break;
				ShowDropDownList(this, this->BuildBadgeConfigurationList(), -1, widget, 0, DropDownOption::Persist);
				break;

			case WID_BV_SHOW_HIDE: {
				const Engine *e = (this->sel_engine == EngineID::Invalid()) ? nullptr : Engine::Get(this->sel_engine);
				if (e != nullptr) {
					Command<Commands::SetVehicleVisibility>::Post(this->sel_engine, !e->IsHidden(_current_company));
				}
				break;
			}

			case WID_BV_BUILD:
				this->BuildVehicle();
				break;

			case WID_BV_RENAME: {
				EngineID sel_eng = this->sel_engine;
				if (sel_eng != EngineID::Invalid()) {
					this->rename_engine = sel_eng;
					ShowQueryString(GetString(STR_ENGINE_NAME, PackEngineNameDParam(sel_eng, EngineNameContext::Generic)), STR_QUERY_RENAME_TRAIN_TYPE_CAPTION + this->vehicle_type, MAX_LENGTH_ENGINE_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				}
				break;
			}

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					PaletteID palette = SPR_2CCMAP_BASE + Company::Get(_local_company)->GetCompanyRecolourOffset(LS_DEFAULT);
					ShowDropDownList(this, this->GetWidget<NWidgetBadgeFilter>(widget)->GetDropDownList(palette), -1, widget, 0);
				}
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
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

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_BV_CAPTION:
				if (this->vehicle_type == VEH_TRAIN && !this->listview_mode) {
					const RailTypeInfo *rti = GetRailTypeInfo(this->filter.railtype);
					return GetString(rti->strings.build_caption);
				}
				if (this->vehicle_type == VEH_ROAD && !this->listview_mode) {
					const RoadTypeInfo *rti = GetRoadTypeInfo(this->filter.roadtype);
					return GetString(rti->strings.build_caption);
				}
				return GetString((this->listview_mode ? STR_VEHICLE_LIST_AVAILABLE_TRAINS : STR_BUY_VEHICLE_TRAIN_ALL_CAPTION) + this->vehicle_type);

			case WID_BV_SORT_DROPDOWN:
				return GetString(std::data(_engine_sort_listing[this->vehicle_type])[this->sort_criteria]);

			case WID_BV_CARGO_FILTER_DROPDOWN:
				return GetString(this->GetCargoFilterLabel(this->cargo_filter_criteria));

			case WID_BV_SHOW_HIDE: {
				const Engine *e = (this->sel_engine == EngineID::Invalid()) ? nullptr : Engine::Get(this->sel_engine);
				if (e != nullptr && e->IsHidden(_local_company)) {
					return GetString(STR_BUY_VEHICLE_TRAIN_SHOW_TOGGLE_BUTTON + this->vehicle_type);
				}
				return GetString(STR_BUY_VEHICLE_TRAIN_HIDE_TOGGLE_BUTTON + this->vehicle_type);
			}

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					return this->GetWidget<NWidgetBadgeFilter>(widget)->GetStringParameter(this->badge_filter_choices);
				}

				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_BV_LIST:
				fill.height = resize.height = GetEngineListHeight(this->vehicle_type);
				size.height = 3 * resize.height;
				size.width = std::max(size.width, this->badge_classes.GetTotalColumnsWidth() + GetVehicleImageCellSize(this->vehicle_type, EIT_PURCHASE).extend_left + GetVehicleImageCellSize(this->vehicle_type, EIT_PURCHASE).extend_right + 165) + padding.width;
				break;

			case WID_BV_PANEL:
				size.height = GetCharacterHeight(FS_NORMAL) * this->details_height + padding.height;
				break;

			case WID_BV_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_BV_CARGO_FILTER_DROPDOWN:
				size.width = std::max(size.width, GetDropDownListDimension(this->BuildCargoDropDownList()).width + padding.width);
				break;

			case WID_BV_CONFIGURE_BADGES:
				/* Hide the configuration button if no configurable badges are present. */
				if (this->badge_classes.GetClasses().empty()) size = {0, 0};
				break;

			case WID_BV_BUILD:
				size = GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_BUY_VEHICLE_BUTTON + this->vehicle_type);
				size = maxdim(size, GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_BUY_REFIT_VEHICLE_BUTTON + this->vehicle_type));
				size.width += padding.width;
				size.height += padding.height;
				break;

			case WID_BV_SHOW_HIDE:
				size = GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_HIDE_TOGGLE_BUTTON + this->vehicle_type);
				size = maxdim(size, GetStringBoundingBox(STR_BUY_VEHICLE_TRAIN_SHOW_TOGGLE_BUTTON + this->vehicle_type));
				size.width += padding.width;
				size.height += padding.height;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BV_LIST:
				DrawEngineList(
					this->vehicle_type,
					r,
					this->eng_list,
					*this->vscroll,
					this->sel_engine,
					false,
					DEFAULT_GROUP,
					this->badge_classes
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
		this->vscroll->SetCount(this->eng_list.size());

		this->SetWidgetsDisabledState(this->sel_engine == EngineID::Invalid(), WID_BV_SHOW_HIDE, WID_BV_BUILD);

		/* Disable renaming engines in network games if you are not the server. */
		this->SetWidgetDisabledState(WID_BV_RENAME, this->sel_engine == EngineID::Invalid() || (_networking && !_network_server));

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			if (this->sel_engine != EngineID::Invalid()) {
				const Rect r = this->GetWidget<NWidgetBase>(WID_BV_PANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				int text_end = DrawVehiclePurchaseInfo(r.left, r.right, r.top, this->sel_engine, this->te);
				needed_height = std::max(needed_height, (text_end - r.top) / GetCharacterHeight(FS_NORMAL));
			}
			if (needed_height != this->details_height) { // Details window are not high enough, enlarge them.
				int resize = needed_height - this->details_height;
				this->details_height = needed_height;
				this->ReInit(0, resize * GetCharacterHeight(FS_NORMAL));
				return;
			}
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		Command<Commands::RenameEngine>::Post(STR_ERROR_CAN_T_RENAME_TRAIN_TYPE + this->vehicle_type, this->rename_engine, *str);
	}

	void OnDropdownSelect(WidgetID widget, int index, int click_result) override
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
					_engine_sort_last_cargo_criteria[this->vehicle_type] = this->cargo_filter_criteria;
					/* deactivate filter if criteria is 'Show All', activate it otherwise */
					this->eng_list.SetFilterState(this->cargo_filter_criteria != CargoFilterCriteria::CF_ANY);
					this->eng_list.ForceRebuild();
					this->SelectEngine(this->sel_engine);
				}
				break;

			case WID_BV_CONFIGURE_BADGES: {
				bool reopen = HandleBadgeConfigurationDropDownClick(static_cast<GrfSpecFeature>(GSF_TRAINS + this->vehicle_type), BADGE_COLUMNS, index, click_result, this->badge_filter_choices);

				this->ReInit();

				if (reopen) {
					ReplaceDropDownList(this, this->BuildBadgeConfigurationList(), -1);
				} else {
					this->CloseChildWindows(WC_DROPDOWN_MENU);
				}

				/* We need to refresh if a filter is removed. */
				this->eng_list.ForceRebuild();
				break;
			}

			default:
				if (IsInsideMM(widget, this->badge_filters.first, this->badge_filters.second)) {
					if (index < 0) {
						ResetBadgeFilter(this->badge_filter_choices, this->GetWidget<NWidgetBadgeFilter>(widget)->GetBadgeClassID());
					} else {
						SetBadgeFilter(this->badge_filter_choices, BadgeID(index));
					}
					this->eng_list.ForceRebuild();
				}
				break;
		}
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BV_LIST);
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_BV_FILTER) {
			this->string_filter.SetFilterTerm(this->vehicle_editbox.text.GetText());
			this->InvalidateData();
		}
	}

	static inline HotkeyList hotkeys{"buildvehicle", {
		Hotkey('F', "focus_filter_box", WID_BV_FILTER),
	}};
};

static WindowDesc _build_vehicle_desc(
	WDP_AUTO, "build_vehicle", 240, 268,
	WC_BUILD_VEHICLE, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_build_vehicle_widgets,
	&BuildVehicleWindow::hotkeys
);

void ShowBuildVehicleWindow(TileIndex tile, VehicleType type)
{
	/* We want to be able to open both Available Train as Available Ships,
	 *  so if tile == INVALID_TILE (Available XXX Window), use 'type' as unique number.
	 *  As it always is a low value, it won't collide with any real tile
	 *  number. */
	uint num = (tile == INVALID_TILE) ? (int)type : tile.base();

	assert(IsCompanyBuildableVehicleType(type));

	CloseWindowById(WC_BUILD_VEHICLE, num);

	new BuildVehicleWindow(_build_vehicle_desc, tile, type);
}
