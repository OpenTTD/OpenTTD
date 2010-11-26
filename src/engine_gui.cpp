/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_gui.cpp GUI to show engine related information. */

#include "stdafx.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "engine_base.h"
#include "command_func.h"
#include "strings_func.h"
#include "engine_gui.h"
#include "articulated_vehicles.h"
#include "vehicle_func.h"
#include "company_func.h"
#include "rail.h"
#include "settings_type.h"

#include "table/strings.h"

/**
 * Return the category of an engine.
 * @param engine Engine to examine.
 * @return String describing the category ("road veh", "train". "airplane", or "ship") of the engine.
 */
StringID GetEngineCategoryName(EngineID engine)
{
	const Engine *e = Engine::Get(engine);
	switch (e->type) {
		default: NOT_REACHED();
		case VEH_ROAD:              return STR_ENGINE_PREVIEW_ROAD_VEHICLE;
		case VEH_AIRCRAFT:          return STR_ENGINE_PREVIEW_AIRCRAFT;
		case VEH_SHIP:              return STR_ENGINE_PREVIEW_SHIP;
		case VEH_TRAIN:
			return GetRailTypeInfo(e->u.rail.railtype)->strings.new_loco;
	}
}

/** Widgets used for the engine preview window */
enum EnginePreviewWidgets {
	EPW_QUESTION,   ///< The container for the question
	EPW_NO,         ///< No button
	EPW_YES,        ///< Yes button
};

static const NWidgetPart _nested_engine_preview_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_ENGINE_PREVIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(WWT_EMPTY, INVALID_COLOUR, EPW_QUESTION), SetMinimalSize(300, 0), SetPadding(8, 8, 8, 8), SetFill(1, 0),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, 10, 85),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, EPW_NO), SetDataTip(STR_QUIT_NO, STR_NULL), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, EPW_YES), SetDataTip(STR_QUIT_YES, STR_NULL), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 8),
	EndContainer(),
};

struct EnginePreviewWindow : Window {
	static const int VEHICLE_SPACE = 40; // The space to show the vehicle image

	EnginePreviewWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != EPW_QUESTION) return;

		EngineID engine = this->window_number;
		SetDParam(0, GetEngineCategoryName(engine));
		size->height = GetStringHeight(STR_ENGINE_PREVIEW_MESSAGE, size->width) + WD_PAR_VSEP_WIDE + FONT_HEIGHT_NORMAL + VEHICLE_SPACE;
		SetDParam(0, engine);
		size->height += GetStringHeight(GetEngineInfoString(engine), size->width);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != EPW_QUESTION) return;

		EngineID engine = this->window_number;
		SetDParam(0, GetEngineCategoryName(engine));
		int y = r.top + GetStringHeight(STR_ENGINE_PREVIEW_MESSAGE, r.right - r.top + 1);
		y = DrawStringMultiLine(r.left, r.right, r.top, y, STR_ENGINE_PREVIEW_MESSAGE, TC_FROMSTRING, SA_CENTER) + WD_PAR_VSEP_WIDE;

		SetDParam(0, engine);
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_ENGINE_NAME, TC_BLACK, SA_HOR_CENTER);
		y += FONT_HEIGHT_NORMAL;

		DrawVehicleEngine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, this->width >> 1, y + VEHICLE_SPACE / 2, engine, GetEnginePalette(engine, _local_company));

		y += VEHICLE_SPACE;
		DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, r.bottom, GetEngineInfoString(engine), TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case EPW_YES:
				DoCommandP(0, this->window_number, 0, CMD_WANT_ENGINE_PREVIEW);
				/* FALL THROUGH */
			case EPW_NO:
				delete this;
				break;
		}
	}
};

static const WindowDesc _engine_preview_desc(
	WDP_CENTER, 0, 0,
	WC_ENGINE_PREVIEW, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_engine_preview_widgets, lengthof(_nested_engine_preview_widgets)
);


void ShowEnginePreviewWindow(EngineID engine)
{
	AllocateWindowDescFront<EnginePreviewWindow>(&_engine_preview_desc, engine);
}

uint GetTotalCapacityOfArticulatedParts(EngineID engine)
{
	uint total = 0;

	CargoArray cap = GetCapacityOfArticulatedParts(engine);
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		total += cap[c];
	}

	return total;
}

static StringID GetTrainEngineInfoString(const Engine *e)
{
	SetDParam(0, e->GetCost());
	SetDParam(2, e->GetDisplayMaxSpeed());
	SetDParam(3, e->GetPower());
	SetDParam(1, e->GetDisplayWeight());
	SetDParam(7, e->GetDisplayMaxTractiveEffort());

	SetDParam(4, e->GetRunningCost());

	uint capacity = GetTotalCapacityOfArticulatedParts(e->index);
	if (capacity != 0) {
		SetDParam(5, e->GetDefaultCargoType());
		SetDParam(6, capacity);
	} else {
		SetDParam(5, CT_INVALID);
	}
	return (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL && GetRailTypeInfo(e->u.rail.railtype)->acceleration_type != 2) ? STR_ENGINE_PREVIEW_COST_WEIGHT_SPEED_POWER_MAX_TE : STR_ENGINE_PREVIEW_COST_WEIGHT_SPEED_POWER;
}

static StringID GetAircraftEngineInfoString(const Engine *e)
{
	CargoID cargo = e->GetDefaultCargoType();
	uint16 mail_capacity;
	uint capacity = e->GetDisplayDefaultCapacity(&mail_capacity);

	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	SetDParam(2, cargo);
	SetDParam(3, capacity);

	if (mail_capacity > 0) {
		SetDParam(4, CT_MAIL);
		SetDParam(5, mail_capacity);
		SetDParam(6, e->GetRunningCost());
		return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAPACITY_CAPACITY_RUNCOST;
	} else {
		SetDParam(4, e->GetRunningCost());
		return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAPACITY_RUNCOST;
	}
}

static StringID GetRoadVehEngineInfoString(const Engine *e)
{
	if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		uint capacity = GetTotalCapacityOfArticulatedParts(e->index);
		if (capacity != 0) {
			SetDParam(2, e->GetDefaultCargoType());
			SetDParam(3, capacity);
		} else {
			SetDParam(2, CT_INVALID);
		}
		SetDParam(4, e->GetRunningCost());
		return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAPACITY_RUNCOST;
	} else {
		SetDParam(0, e->GetCost());
		SetDParam(2, e->GetDisplayMaxSpeed());
		SetDParam(3, e->GetPower());
		SetDParam(1, e->GetDisplayWeight());
		SetDParam(7, e->GetDisplayMaxTractiveEffort());

		SetDParam(4, e->GetRunningCost());

		uint capacity = GetTotalCapacityOfArticulatedParts(e->index);
		if (capacity != 0) {
			SetDParam(5, e->GetDefaultCargoType());
			SetDParam(6, capacity);
		} else {
			SetDParam(5, CT_INVALID);
		}
		return STR_ENGINE_PREVIEW_COST_WEIGHT_SPEED_POWER_MAX_TE;
	}
}

static StringID GetShipEngineInfoString(const Engine *e)
{
	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	SetDParam(2, e->GetDefaultCargoType());
	SetDParam(3, e->GetDisplayDefaultCapacity());
	SetDParam(4, e->GetRunningCost());
	return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAPACITY_RUNCOST;
}


/**
 * Get a multi-line string with some technical data, describing the engine.
 * @param engine Engine to describe.
 * @return String describing the engine.
 * @post \c DParam array is set up for printing the string.
 */
StringID GetEngineInfoString(EngineID engine)
{
	const Engine *e = Engine::Get(engine);

	switch (e->type) {
		case VEH_TRAIN:
			return GetTrainEngineInfoString(e);

		case VEH_ROAD:
			return GetRoadVehEngineInfoString(e);

		case VEH_SHIP:
			return GetShipEngineInfoString(e);

		case VEH_AIRCRAFT:
			return GetAircraftEngineInfoString(e);

		default: NOT_REACHED();
	}
}

/**
 * Draw an engine.
 * @param left   Minimum horizontal position to use for drawing the engine
 * @param right  Maximum horizontal position to use for drawing the engine
 * @param preferred_x Horizontal position to use for drawing the engine.
 * @param y      Vertical position to use for drawing the engine.
 * @param engine Engine to draw.
 * @param pal    Palette to use for drawing.
 */
void DrawVehicleEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal)
{
	const Engine *e = Engine::Get(engine);

	switch (e->type) {
		case VEH_TRAIN:
			DrawTrainEngine(left, right, preferred_x, y, engine, pal);
			break;

		case VEH_ROAD:
			DrawRoadVehEngine(left, right, preferred_x, y, engine, pal);
			break;

		case VEH_SHIP:
			DrawShipEngine(left, right, preferred_x, y, engine, pal);
			break;

		case VEH_AIRCRAFT:
			DrawAircraftEngine(left, right, preferred_x, y, engine, pal);
			break;

		default: NOT_REACHED();
	}
}

/**
 * Sort all items using quick sort and given 'CompareItems' function
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 */
void EngList_Sort(GUIEngineList *el, EngList_SortTypeFunction compare)
{
	uint size = el->Length();
	/* out-of-bounds access at the next line for size == 0 (even with operator[] at some systems)
	 * generally, do not sort if there are less than 2 items */
	if (size < 2) return;
	QSortT(el->Begin(), size, compare);
}

/**
 * Sort selected range of items (on indices @ <begin, begin+num_items-1>)
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 * @param begin start of sorting
 * @param num_items count of items to be sorted
 */
void EngList_SortPartial(GUIEngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items)
{
	if (num_items < 2) return;
	assert(begin < el->Length());
	assert(begin + num_items <= el->Length());
	QSortT(el->Get(begin), num_items, compare);
}

