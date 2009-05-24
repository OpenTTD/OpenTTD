/* $Id$ */

/** @file engine_gui.cpp GUI to show engine related information. */

#include "stdafx.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "engine_func.h"
#include "engine_base.h"
#include "command_func.h"
#include "news_type.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "engine_gui.h"
#include "articulated_vehicles.h"
#include "rail.h"

#include "table/strings.h"
#include "table/sprites.h"

StringID GetEngineCategoryName(EngineID engine)
{
	switch (Engine::Get(engine)->type) {
		default: NOT_REACHED();
		case VEH_ROAD:              return STR_ENGINE_PREVIEW_ROAD_VEHICLE;
		case VEH_AIRCRAFT:          return STR_ENGINE_PREVIEW_AIRCRAFT;
		case VEH_SHIP:              return STR_ENGINE_PREVIEW_SHIP;
		case VEH_TRAIN:
			return GetRailTypeInfo(RailVehInfo(engine)->railtype)->strings.new_loco;
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
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,   85,  144,  172,  183, STR_NO,                     STR_NULL},                           // EPW_NO
{ WWT_PUSHTXTBTN,  RESIZE_NONE,  COLOUR_LIGHT_BLUE,  155,  214,  172,  183, STR_YES,                    STR_NULL},                           // EPW_YES
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
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, EPW_NO), SetMinimalSize(60, 12), SetDataTip(STR_NO, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, EPW_YES), SetMinimalSize(60, 12), SetDataTip(STR_YES, STR_NULL),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 8),
	EndContainer(),
};

typedef void DrawEngineProc(int x, int y, EngineID engine, SpriteID pal);
typedef void DrawEngineInfoProc(EngineID, int left, int right, int top, int bottom);

struct DrawEngineInfo {
	DrawEngineProc *engine_proc;
	DrawEngineInfoProc *info_proc;
};

static void DrawTrainEngineInfo(EngineID engine, int left, int right, int top, int bottom);
static void DrawRoadVehEngineInfo(EngineID engine, int left, int right, int top, int bottom);
static void DrawShipEngineInfo(EngineID engine, int left, int right, int top, int bottom);
static void DrawAircraftEngineInfo(EngineID engine, int left, int right, int top, int bottom);

static const DrawEngineInfo _draw_engine_list[4] = {
	{ DrawTrainEngine,    DrawTrainEngineInfo    },
	{ DrawRoadVehEngine,  DrawRoadVehEngineInfo  },
	{ DrawShipEngine,     DrawShipEngineInfo     },
	{ DrawAircraftEngine, DrawAircraftEngineInfo },
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

		const DrawEngineInfo *dei = &_draw_engine_list[Engine::Get(engine)->type];

		int width = this->width;
		dei->engine_proc(width >> 1, 100, engine, 0);
		dei->info_proc(engine, this->widget[EPW_BACKGROUND].left + 26, this->widget[EPW_BACKGROUND].right - 26, 100, 170);
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

uint GetTotalCapacityOfArticulatedParts(EngineID engine, VehicleType type)
{
	uint total = 0;

	uint16 *cap = GetCapacityOfArticulatedParts(engine, type);
	for (uint c = 0; c < NUM_CARGO; c++) {
		total += cap[c];
	}

	return total;
}

static void DrawTrainEngineInfo(EngineID engine, int left, int right, int top, int bottom)
{
	const Engine *e = Engine::Get(engine);

	SetDParam(0, e->GetCost());
	SetDParam(2, e->GetDisplayMaxSpeed());
	SetDParam(3, e->GetPower());
	SetDParam(1, e->GetDisplayWeight());

	SetDParam(4, e->GetRunningCost());

	uint capacity = GetTotalCapacityOfArticulatedParts(engine, VEH_TRAIN);
	if (capacity != 0) {
		SetDParam(5, e->GetDefaultCargoType());
		SetDParam(6, capacity);
	} else {
		SetDParam(5, CT_INVALID);
	}
	DrawStringMultiLine(left, right, top, bottom, STR_VEHICLE_INFO_COST_WEIGHT_SPEED_POWER, TC_FROMSTRING, SA_CENTER);
}

static void DrawAircraftEngineInfo(EngineID engine, int left, int right, int top, int bottom)
{
	const Engine *e = Engine::Get(engine);
	CargoID cargo = e->GetDefaultCargoType();

	if (cargo == CT_INVALID || cargo == CT_PASSENGERS) {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		SetDParam(2, CT_PASSENGERS),
		SetDParam(3, e->GetDisplayDefaultCapacity());
		SetDParam(4, CT_MAIL),
		SetDParam(5, e->u.air.mail_capacity);
		SetDParam(6, e->GetRunningCost());

		DrawStringMultiLine(left, right, top, bottom, STR_VEHICLE_INFO_COST_MAX_SPEED_CAPACITY_CAPACITY_RUNCOST, TC_FROMSTRING, SA_CENTER);
	} else {
		SetDParam(0, e->GetCost());
		SetDParam(1, e->GetDisplayMaxSpeed());
		SetDParam(2, cargo);
		SetDParam(3, e->GetDisplayDefaultCapacity());
		SetDParam(4, e->GetRunningCost());

		DrawStringMultiLine(left, right, top, bottom, STR_VEHICLE_INFO_COST_MAX_SPEED_CAPACITY_RUNCOST, TC_FROMSTRING, SA_CENTER);
	}
}

static void DrawRoadVehEngineInfo(EngineID engine, int left, int right, int top, int bottom)
{
	const Engine *e = Engine::Get(engine);

	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	uint capacity = GetTotalCapacityOfArticulatedParts(engine, VEH_ROAD);
	if (capacity != 0) {
		SetDParam(2, e->GetDefaultCargoType());
		SetDParam(3, capacity);
	} else {
		SetDParam(2, CT_INVALID);
	}
	SetDParam(4, e->GetRunningCost());

	DrawStringMultiLine(left, right, top, bottom, STR_VEHICLE_INFO_COST_MAX_SPEED_CAPACITY_RUNCOST, TC_FROMSTRING, SA_CENTER);
}

static void DrawShipEngineInfo(EngineID engine, int left, int right, int top, int bottom)
{
	const Engine *e = Engine::Get(engine);

	SetDParam(0, e->GetCost());
	SetDParam(1, e->GetDisplayMaxSpeed());
	SetDParam(2, e->GetDefaultCargoType());
	SetDParam(3, e->GetDisplayDefaultCapacity());
	SetDParam(4, e->GetRunningCost());
	DrawStringMultiLine(left, right, top, bottom, STR_VEHICLE_INFO_COST_MAX_SPEED_CAPACITY_RUNCOST, TC_FROMSTRING, SA_CENTER);
}

void DrawNewsNewVehicleAvail(Window *w, const NewsItem *ni)
{
	assert(ni->reftype1 == NR_ENGINE);
	EngineID engine = ni->ref1;
	const DrawEngineInfo *dei = &_draw_engine_list[Engine::Get(engine)->type];

	SetDParam(0, GetEngineCategoryName(engine));
	DrawStringMultiLine(1, w->width - 2, 0, 56, STR_NEW_VEHICLE_NOW_AVAILABLE, TC_FROMSTRING, SA_CENTER);

	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SetDParam(0, engine);
	DrawStringMultiLine(1, w->width - 2, 56, 88, STR_NEW_VEHICLE_TYPE, TC_FROMSTRING, SA_CENTER);

	dei->engine_proc(w->width >> 1, 88, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 112, PALETTE_TO_STRUCT_GREY, FILLRECT_RECOLOUR);
	dei->info_proc(engine, 26, w->width - 26, 100, 170);
}


/** Sort all items using qsort() and given 'CompareItems' function
 * @param el list to be sorted
 * @param compare function for evaluation of the quicksort
 */
void EngList_Sort(GUIEngineList *el, EngList_SortTypeFunction compare)
{
	uint size = el->Length();
	/* out-of-bounds access at the next line for size == 0 (even with operator[] at some systems)
	 * generally, do not sort if there are less than 2 items */
	if (size < 2) return;
	qsort(el->Begin(), size, sizeof(*el->Begin()), compare); // MorphOS doesn't know vector::at(int) ...
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
	qsort(el->Get(begin), num_items, sizeof(*el->Begin()), compare);
}

