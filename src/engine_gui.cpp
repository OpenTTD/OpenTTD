/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "engine.h"
#include "command.h"
#include "news.h"
#include "variables.h"
#include "newgrf_engine.h"


static StringID GetEngineCategoryName(EngineID engine)
{
	if (engine < NUM_TRAIN_ENGINES) {
		switch (RailVehInfo(engine)->railtype) {
			case RAILTYPE_RAIL:     return STR_8102_RAILROAD_LOCOMOTIVE;
			case RAILTYPE_ELECTRIC: return STR_8102_RAILROAD_LOCOMOTIVE;
			case RAILTYPE_MONO:     return STR_8106_MONORAIL_LOCOMOTIVE;
			case RAILTYPE_MAGLEV:   return STR_8107_MAGLEV_LOCOMOTIVE;
			default: NOT_REACHED();
		}
	}

	if (engine < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES)
		return STR_8103_ROAD_VEHICLE;

	if (engine < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES)
		return STR_8105_SHIP;

	return STR_8104_AIRCRAFT;
}

static const Widget _engine_preview_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     5,     0,    10,     0,    13, STR_00C5,                                  STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     5,    11,   299,     0,    13, STR_8100_MESSAGE_FROM_VEHICLE_MANUFACTURE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     5,     0,   299,    14,   191, 0x0,                                       STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     5,    85,   144,   172,   183, STR_00C9_NO,                               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,     5,   155,   214,   172,   183, STR_00C8_YES,                              STR_NULL},
{   WIDGETS_END},
};

typedef void DrawEngineProc(int x, int y, EngineID engine, SpriteID pal);
typedef void DrawEngineInfoProc(EngineID, int x, int y, int maxw);

typedef struct DrawEngineInfo {
	DrawEngineProc *engine_proc;
	DrawEngineInfoProc *info_proc;
} DrawEngineInfo;

static void DrawTrainEngineInfo(EngineID engine, int x, int y, int maxw);
static void DrawRoadVehEngineInfo(EngineID engine, int x, int y, int maxw);
static void DrawShipEngineInfo(EngineID engine, int x, int y, int maxw);
static void DrawAircraftEngineInfo(EngineID engine, int x, int y, int maxw);

static const DrawEngineInfo _draw_engine_list[4] = {
	{DrawTrainEngine,DrawTrainEngineInfo},
	{DrawRoadVehEngine,DrawRoadVehEngineInfo},
	{DrawShipEngine,DrawShipEngineInfo},
	{DrawAircraftEngine,DrawAircraftEngineInfo},
};

static void EnginePreviewWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		EngineID engine = w->window_number;
		const DrawEngineInfo* dei;
		int width;

		DrawWindowWidgets(w);

		SetDParam(0, GetEngineCategoryName(engine));
		DrawStringMultiCenter(150, 44, STR_8101_WE_HAVE_JUST_DESIGNED_A, 296);

		DrawStringCentered(w->width >> 1, 80, GetCustomEngineName(engine), 0x10);

		(dei = _draw_engine_list,engine < NUM_TRAIN_ENGINES) ||
		(dei++,engine < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES) ||
		(dei++,engine < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES) ||
		(dei++, true);

		width = w->width;
		dei->engine_proc(width >> 1, 100, engine, 0);
		dei->info_proc(engine, width >> 1, 130, width - 52);
		break;
	}

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 4:
				DoCommandP(0, w->window_number, 0, NULL, CMD_WANT_ENGINE_PREVIEW);
				/* Fallthrough */
			case 3:
				DeleteWindow(w);
				break;
		}
		break;
	}
}

static const WindowDesc _engine_preview_desc = {
	WDP_CENTER, WDP_CENTER, 300, 192,
	WC_ENGINE_PREVIEW, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_engine_preview_widgets,
	EnginePreviewWndProc
};


void ShowEnginePreviewWindow(EngineID engine)
{
	AllocateWindowDescFront(&_engine_preview_desc, engine);
}

static void DrawTrainEngineInfo(EngineID engine, int x, int y, int maxw)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine);
	uint multihead = (rvi->railveh_type == RAILVEH_MULTIHEAD) ? 1 : 0;

	SetDParam(0, (_price.build_railvehicle >> 3) * rvi->base_cost >> 5);
	SetDParam(2, rvi->max_speed * 10 / 16);
	SetDParam(3, rvi->power << multihead);
	SetDParam(1, rvi->weight << multihead);

	SetDParam(4, rvi->running_cost_base * _price.running_rail[rvi->running_cost_class] >> 8 << multihead);

	if (rvi->capacity != 0) {
		SetDParam(5, rvi->cargo_type);
		SetDParam(6, rvi->capacity << multihead);
	} else {
		SetDParam(5, CT_INVALID);
	}
	DrawStringMultiCenter(x, y, STR_VEHICLE_INFO_COST_WEIGHT_SPEED_POWER, maxw);
}

void DrawNewsNewTrainAvail(Window *w)
{
	EngineID engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;
	SetDParam(0, GetEngineCategoryName(engine));
	DrawStringMultiCenter(w->width >> 1, 20, STR_8859_NEW_NOW_AVAILABLE, w->width - 2);

	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SetDParam(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_885A, w->width - 2);

	DrawTrainEngine(w->width >> 1, 88, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 112, PALETTE_TO_STRUCT_GREY | (1 << USE_COLORTABLE));
	DrawTrainEngineInfo(engine, w->width >> 1, 129, w->width - 52);
}

StringID GetNewsStringNewTrainAvail(const NewsItem *ni)
{
	EngineID engine = ni->string_id;
	SetDParam(0, STR_8859_NEW_NOW_AVAILABLE);
	SetDParam(1, GetEngineCategoryName(engine));
	SetDParam(2, GetCustomEngineName(engine));
	return STR_02B6;
}

static void DrawAircraftEngineInfo(EngineID engine, int x, int y, int maxw)
{
	const AircraftVehicleInfo *avi = AircraftVehInfo(engine);
	SetDParam(0, (_price.aircraft_base >> 3) * avi->base_cost >> 5);
	SetDParam(1, avi->max_speed * 8);
	SetDParam(2, avi->passenger_capacity);
	SetDParam(3, avi->mail_capacity);
	SetDParam(4, avi->running_cost * _price.aircraft_running >> 8);

	DrawStringMultiCenter(x, y, STR_A02E_COST_MAX_SPEED_CAPACITY, maxw);
}

void DrawNewsNewAircraftAvail(Window *w)
{
	EngineID engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;

	DrawStringMultiCenter(w->width >> 1, 20, STR_A02C_NEW_AIRCRAFT_NOW_AVAILABLE, w->width - 2);
	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SetDParam(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_A02D, w->width - 2);

	DrawAircraftEngine(w->width >> 1, 93, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 110, PALETTE_TO_STRUCT_GREY | (1 << USE_COLORTABLE));
	DrawAircraftEngineInfo(engine, w->width >> 1, 131, w->width - 52);
}

StringID GetNewsStringNewAircraftAvail(const NewsItem *ni)
{
	EngineID engine = ni->string_id;
	SetDParam(0, STR_A02C_NEW_AIRCRAFT_NOW_AVAILABLE);
	SetDParam(1, GetCustomEngineName(engine));
	return STR_02B6;
}

static void DrawRoadVehEngineInfo(EngineID engine, int x, int y, int maxw)
{
	const RoadVehicleInfo *rvi = RoadVehInfo(engine);

	SetDParam(0, (_price.roadveh_base >> 3) * rvi->base_cost >> 5);
	SetDParam(1, rvi->max_speed * 10 / 32);
	SetDParam(2, rvi->running_cost * _price.roadveh_running >> 8);
	SetDParam(3, rvi->cargo_type);
	SetDParam(4, rvi->capacity);

	DrawStringMultiCenter(x, y, STR_902A_COST_SPEED_RUNNING_COST, maxw);
}

void DrawNewsNewRoadVehAvail(Window *w)
{
	EngineID engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;
	DrawStringMultiCenter(w->width >> 1, 20, STR_9028_NEW_ROAD_VEHICLE_NOW_AVAILABLE, w->width - 2);
	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SetDParam(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_9029, w->width - 2);

	DrawRoadVehEngine(w->width >> 1, 88, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 112, PALETTE_TO_STRUCT_GREY | (1 << USE_COLORTABLE));
	DrawRoadVehEngineInfo(engine, w->width >> 1, 129, w->width - 52);
}

StringID GetNewsStringNewRoadVehAvail(const NewsItem *ni)
{
	EngineID engine = ni->string_id;
	SetDParam(0, STR_9028_NEW_ROAD_VEHICLE_NOW_AVAILABLE);
	SetDParam(1, GetCustomEngineName(engine));
	return STR_02B6;
}

static void DrawShipEngineInfo(EngineID engine, int x, int y, int maxw)
{
	const ShipVehicleInfo *svi = ShipVehInfo(engine);
	SetDParam(0, svi->base_cost * (_price.ship_base >> 3) >> 5);
	SetDParam(1, svi->max_speed * 10 / 32);
	SetDParam(2, svi->cargo_type);
	SetDParam(3, svi->capacity);
	SetDParam(4, svi->running_cost * _price.ship_running >> 8);
	DrawStringMultiCenter(x, y, STR_982E_COST_MAX_SPEED_CAPACITY, maxw);
}

void DrawNewsNewShipAvail(Window *w)
{
	EngineID engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;

	DrawStringMultiCenter(w->width >> 1, 20, STR_982C_NEW_SHIP_NOW_AVAILABLE, w->width - 2);
	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SetDParam(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_982D, w->width - 2);

	DrawShipEngine(w->width >> 1, 93, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 110, PALETTE_TO_STRUCT_GREY | (1 << USE_COLORTABLE));
	DrawShipEngineInfo(engine, w->width >> 1, 131, w->width - 52);
}

StringID GetNewsStringNewShipAvail(const NewsItem *ni)
{
	EngineID engine = ni->string_id;
	SetDParam(0, STR_982C_NEW_SHIP_NOW_AVAILABLE);
	SetDParam(1, GetCustomEngineName(engine));
	return STR_02B6;
}
