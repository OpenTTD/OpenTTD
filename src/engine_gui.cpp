/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_gui.cpp GUI to show engine related information. */

#include "stdafx.h"
#include "window_gui.h"
#include "engine_base.h"
#include "command_func.h"
#include "strings_func.h"
#include "engine_gui.h"
#include "articulated_vehicles.h"
#include "vehicle_func.h"
#include "company_func.h"
#include "rail.h"
#include "road.h"
#include "settings_type.h"
#include "train.h"
#include "roadveh.h"
#include "ship.h"
#include "aircraft.h"
#include "engine_cmd.h"
#include "zoom_func.h"

#include "widgets/engine_widget.h"

#include "table/strings.h"

#include "safeguards.h"

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
		case VEH_ROAD:
			return GetRoadTypeInfo(e->u.road.roadtype)->strings.new_engine;
		case VEH_AIRCRAFT:          return STR_ENGINE_PREVIEW_AIRCRAFT;
		case VEH_SHIP:              return STR_ENGINE_PREVIEW_SHIP;
		case VEH_TRAIN:
			return GetRailTypeInfo(e->u.rail.railtype)->strings.new_loco;
	}
}

static const NWidgetPart _nested_engine_preview_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE), SetDataTip(STR_ENGINE_PREVIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_EP_QUESTION), SetMinimalSize(300, 0), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(85, WidgetDimensions::unscaled.hsep_wide, 85),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_NO), SetDataTip(STR_QUIT_NO, STR_NULL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, WID_EP_YES), SetDataTip(STR_QUIT_YES, STR_NULL), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct EnginePreviewWindow : Window {
	int vehicle_space; // The space to show the vehicle image

	EnginePreviewWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);

		/* There is no way to recover the window; so disallow closure via DEL; unless SHIFT+DEL */
		this->flags |= WF_STICKY;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_EP_QUESTION) return;

		/* Get size of engine sprite, on loan from depot_gui.cpp */
		EngineID engine = this->window_number;
		EngineImageType image_type = EIT_PURCHASE;
		uint x, y;
		int x_offs, y_offs;

		const Engine *e = Engine::Get(engine);
		switch (e->type) {
			default: NOT_REACHED();
			case VEH_TRAIN:    GetTrainSpriteSize(   engine, x, y, x_offs, y_offs, image_type); break;
			case VEH_ROAD:     GetRoadVehSpriteSize( engine, x, y, x_offs, y_offs, image_type); break;
			case VEH_SHIP:     GetShipSpriteSize(    engine, x, y, x_offs, y_offs, image_type); break;
			case VEH_AIRCRAFT: GetAircraftSpriteSize(engine, x, y, x_offs, y_offs, image_type); break;
		}
		this->vehicle_space = std::max<int>(ScaleSpriteTrad(40), y - y_offs);

		size->width = std::max(size->width, x - x_offs);
		SetDParam(0, GetEngineCategoryName(engine));
		size->height = GetStringHeight(STR_ENGINE_PREVIEW_MESSAGE, size->width) + WidgetDimensions::scaled.vsep_wide + GetCharacterHeight(FS_NORMAL) + this->vehicle_space;
		SetDParam(0, engine);
		size->height += GetStringHeight(GetEngineInfoString(engine), size->width);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_EP_QUESTION) return;

		EngineID engine = this->window_number;
		SetDParam(0, GetEngineCategoryName(engine));
		int y = DrawStringMultiLine(r, STR_ENGINE_PREVIEW_MESSAGE, TC_FROMSTRING, SA_HOR_CENTER | SA_TOP) + WidgetDimensions::scaled.vsep_wide;

		SetDParam(0, PackEngineNameDParam(engine, EngineNameContext::PreviewNews));
		DrawString(r.left, r.right, y, STR_ENGINE_NAME, TC_BLACK, SA_HOR_CENTER);
		y += GetCharacterHeight(FS_NORMAL);

		DrawVehicleEngine(r.left, r.right, this->width >> 1, y + this->vehicle_space / 2, engine, GetEnginePalette(engine, _local_company), EIT_PREVIEW);

		y += this->vehicle_space;
		DrawStringMultiLine(r.left, r.right, y, r.bottom, GetEngineInfoString(engine), TC_FROMSTRING, SA_CENTER);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_EP_YES:
				Command<CMD_WANT_ENGINE_PREVIEW>::Post(this->window_number);
				FALLTHROUGH;
			case WID_EP_NO:
				if (!_shift_pressed) this->Close();
				break;
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		EngineID engine = this->window_number;
		if (Engine::Get(engine)->preview_company != _local_company) this->Close();
	}
};

static WindowDesc _engine_preview_desc(__FILE__, __LINE__,
	WDP_CENTER, nullptr, 0, 0,
	WC_ENGINE_PREVIEW, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_engine_preview_widgets), std::end(_nested_engine_preview_widgets)
);


void ShowEnginePreviewWindow(EngineID engine)
{
	AllocateWindowDescFront<EnginePreviewWindow>(&_engine_preview_desc, engine);
}

/**
 * Get the capacity of an engine with articulated parts.
 * @param engine The engine to get the capacity of.
 * @return The capacity.
 */
uint GetTotalCapacityOfArticulatedParts(EngineID engine)
{
	CargoArray cap = GetCapacityOfArticulatedParts(engine);
	return cap.GetSum<uint>();
}

static StringID GetTrainEngineInfoString(const Engine *e)
{
	SetDParam(0, e->GetCost());
	SetDParam(2, PackVelocity(e->GetDisplayMaxSpeed(), e->type));
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
	uint16_t mail_capacity;
	uint capacity = e->GetDisplayDefaultCapacity(&mail_capacity);
	uint16_t range = e->GetRange();

	uint i = 0;
	SetDParam(i++, e->GetCost());
	SetDParam(i++, PackVelocity(e->GetDisplayMaxSpeed(), e->type));
	SetDParam(i++, e->GetAircraftTypeText());
	if (range > 0) SetDParam(i++, range);
	SetDParam(i++, cargo);
	SetDParam(i++, capacity);

	if (mail_capacity > 0) {
		SetDParam(i++, CT_MAIL);
		SetDParam(i++, mail_capacity);
		SetDParam(i++, e->GetRunningCost());
		return range > 0 ? STR_ENGINE_PREVIEW_COST_MAX_SPEED_TYPE_RANGE_CAP_CAP_RUNCOST : STR_ENGINE_PREVIEW_COST_MAX_SPEED_TYPE_CAP_CAP_RUNCOST;
	} else {
		SetDParam(i++, e->GetRunningCost());
		return range > 0 ? STR_ENGINE_PREVIEW_COST_MAX_SPEED_TYPE_RANGE_CAP_RUNCOST : STR_ENGINE_PREVIEW_COST_MAX_SPEED_TYPE_CAP_RUNCOST;
	}
}

static StringID GetRoadVehEngineInfoString(const Engine *e)
{
	if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) {
		SetDParam(0, e->GetCost());
		SetDParam(1, PackVelocity(e->GetDisplayMaxSpeed(), e->type));
		uint capacity = GetTotalCapacityOfArticulatedParts(e->index);
		if (capacity != 0) {
			SetDParam(2, e->GetDefaultCargoType());
			SetDParam(3, capacity);
		} else {
			SetDParam(2, CT_INVALID);
		}
		SetDParam(4, e->GetRunningCost());
		return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAP_RUNCOST;
	} else {
		SetDParam(0, e->GetCost());
		SetDParam(2, PackVelocity(e->GetDisplayMaxSpeed(), e->type));
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
	SetDParam(1, PackVelocity(e->GetDisplayMaxSpeed(), e->type));
	SetDParam(2, e->GetDefaultCargoType());
	SetDParam(3, e->GetDisplayDefaultCapacity());
	SetDParam(4, e->GetRunningCost());
	return STR_ENGINE_PREVIEW_COST_MAX_SPEED_CAP_RUNCOST;
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
void DrawVehicleEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type)
{
	const Engine *e = Engine::Get(engine);

	switch (e->type) {
		case VEH_TRAIN:
			DrawTrainEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		case VEH_ROAD:
			DrawRoadVehEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		case VEH_SHIP:
			DrawShipEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		case VEH_AIRCRAFT:
			DrawAircraftEngine(left, right, preferred_x, y, engine, pal, image_type);
			break;

		default: NOT_REACHED();
	}
}

/**
 * Sort all items using quick sort and given 'CompareItems' function
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 */
void EngList_Sort(GUIEngineList &el, EngList_SortTypeFunction compare)
{
	if (el.size() < 2) return;
	std::sort(el.begin(), el.end(), compare);
}

/**
 * Sort selected range of items (on indices @ <begin, begin+num_items-1>)
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 * @param begin start of sorting
 * @param num_items count of items to be sorted
 */
void EngList_SortPartial(GUIEngineList &el, EngList_SortTypeFunction compare, size_t begin, size_t num_items)
{
	if (num_items < 2) return;
	assert(begin < el.size());
	assert(begin + num_items <= el.size());
	std::sort(el.begin() + begin, el.begin() + begin + num_items, compare);
}

