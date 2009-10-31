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
#include "engine_func.h"
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

/** Return the category of an engine.
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
	EPW_CLOSE,      ///< Close button
	EPW_CAPTION,    ///< Title bar/caption
	EPW_BACKGROUND, ///< Background
	EPW_NO,         ///< No button
	EPW_YES,        ///< Yes button
};

static const Widget _engine_preview_widgets[] = {
{   WWT_CLOSEBOX,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,    0,   10,    0,   13, STR_BLACK_CROSS,            STR_TOOLTIP_CLOSE_WINDOW},           // EPW_CLOSE
{    WWT_CAPTION,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,   11,  299,    0,   13, STR_ENGINE_PREVIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS}, // EPW_CAPTION
{      WWT_PANEL,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,    0,  299,   14,  191, 0x0,                        STR_NULL},                           // EPW_BACKGROUND
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,   85,  144,  172,  183, STR_QUIT_NO,                     STR_NULL},                           // EPW_NO
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,  155,  214,  172,  183, STR_QUIT_YES,                    STR_NULL},                           // EPW_YES
{   WIDGETS_END},
};

static const NWidgetPart _nested_engine_preview_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, EPW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, EPW_CAPTION), SetDataTip(STR_ENGINE_PREVIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, EPW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 158),
		NWidget(NWID_HORIZONTAL), SetPIP(85, 10, 85),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, EPW_NO), SetMinimalSize(60, 12), SetDataTip(STR_QUIT_NO, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, EPW_YES), SetMinimalSize(60, 12), SetDataTip(STR_QUIT_YES, STR_NULL),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 8),
	EndContainer(),
};

struct EnginePreviewWindow : Window {
	EnginePreviewWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		EngineID engine = this->window_number;
		SetDParam(0, GetEngineCategoryName(engine));
		DrawStringMultiLine(this->widget[EPW_BACKGROUND].left + 2, this->widget[EPW_BACKGROUND].right - 2, 18, 80, STR_ENGINE_PREVIEW_MESSAGE, TC_FROMSTRING, SA_CENTER);

		SetDParam(0, engine);
		DrawString(this->widget[EPW_BACKGROUND].left + 2, this->widget[EPW_BACKGROUND].right - 2, 80, STR_ENGINE_NAME, TC_BLACK, SA_CENTER);

		int width = this->width;
		DrawVehicleEngine(width >> 1, 100, engine, GetEnginePalette(engine, _local_company));
		DrawStringMultiLine(this->widget[EPW_BACKGROUND].left + 26, this->widget[EPW_BACKGROUND].right - 26, 100, 170, GetEngineInfoString(engine), TC_FROMSTRING, SA_CENTER);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case EPW_YES:
				DoCommandP(0, this->window_number, 0, CMD_WANT_ENGINE_PREVIEW);
				/* Fallthrough */
			case EPW_NO:
				delete this;
				break;
		}
	}
};

static const WindowDesc _engine_preview_desc(
	WDP_CENTER, WDP_CENTER, 300, 192, 300, 192,
	WC_ENGINE_PREVIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	_engine_preview_widgets, _nested_engine_preview_widgets, lengthof(_nested_engine_preview_widgets)
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
	return (_settings_game.vehicle.train_acceleration_model != TAM_ORIGINAL && e->u.rail.railtype != RAILTYPE_MAGLEV) ? STR_ENGINE_PREVIEW_COST_WEIGHT_SPEED_POWER_MAX_TE : STR_ENGINE_PREVIEW_COST_WEIGHT_SPEED_POWER;
}

static StringID GetAircraftEngineInfoString(const Engine *e)
{
	CargoID cargo = e->GetDefaultCargoType();
	uint16 mail_capacity;
	uint capacity = e->GetDisplayDefaultCapacity(&mail_capacity);

	if (mail_capacity > 0) {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		SetDParam(2, cargo);
		SetDParam(3, capacity);
		SetDParam(4, CT_MAIL);
		SetDParam(5, mail_capacity);
		SetDParam(6, e->GetRunningCost());
		return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAPACITY_CAPACITY_RUNCOST;
	} else {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		SetDParam(2, cargo);
		SetDParam(3, capacity);
		SetDParam(4, e->GetRunningCost());
		return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAPACITY_RUNCOST;
	}
}

static StringID GetRoadVehEngineInfoString(const Engine *e)
{
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
 * @param x      Horizontal position to use for drawing the engine.
 * @param y      Vertical position to use for drawing the engine.
 * @param engine Engine to draw.
 * @param pal    Palette to use for drawing.
 */
void DrawVehicleEngine(int x, int y, EngineID engine, SpriteID pal)
{
	const Engine *e = Engine::Get(engine);

	switch (e->type) {
		case VEH_TRAIN:
			DrawTrainEngine(x, y, engine, pal);
			break;

		case VEH_ROAD:
			DrawRoadVehEngine(x, y, engine, pal);
			break;

		case VEH_SHIP:
			DrawShipEngine(x, y, engine, pal);
			break;

		case VEH_AIRCRAFT:
			DrawAircraftEngine(x, y, engine, pal);
			break;

		default: NOT_REACHED();
	}
}

/** Sort all items using quick sort and given 'CompareItems' function
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

/** Sort selected range of items (on indices @ <begin, begin+num_items-1>)
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

