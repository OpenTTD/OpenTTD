#include "stdafx.h"
#include "ttd.h"

#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "engine.h"
#include "command.h"
#include "news.h"

void DrawShipEngine(int x, int y, int engine, uint32 image_ormod);
void DrawShipEngineInfo(int engine, int x, int y, int maxw);


StringID GetEngineCategoryName(byte engine)
{
	if (engine < NUM_NORMAL_RAIL_ENGINES)
		return STR_8102_RAILROAD_LOCOMOTIVE;

	if (engine < NUM_NORMAL_RAIL_ENGINES + NUM_MONORAIL_ENGINES)
		return STR_8106_MONORAIL_LOCOMOTIVE;

	if (engine < NUM_TRAIN_ENGINES)
		return STR_8107_MAGLEV_LOCOMOTIVE;

	if (engine < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES)
		return STR_8103_ROAD_VEHICLE;

	if (engine < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES)
		return STR_8105_SHIP;

	return STR_8104_AIRCRAFT;
}

static const Widget _engine_preview_widgets[] = {
{    WWT_TEXTBTN,     5,     0,    10,     0,    13, STR_00C5,			STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,     5,    11,   299,     0,    13, STR_8100_MESSAGE_FROM_VEHICLE_MANUFACTURE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,     5,     0,   299,    14,   191, 0x0,						STR_NULL},
{ WWT_PUSHTXTBTN,     5,    85,   144,   172,   183, STR_00C9_NO,		STR_NULL},
{ WWT_PUSHTXTBTN,     5,   155,   214,   172,   183, STR_00C8_YES,	STR_NULL},
{   WIDGETS_END},
};

typedef void DrawEngineProc(int x, int y, int engine, uint32 image_ormod);
typedef void DrawEngineInfoProc(int x, int y, int engine, int maxw);

typedef struct DrawEngineInfo {
	DrawEngineProc *engine_proc;
	DrawEngineInfoProc *info_proc;
} DrawEngineInfo;

static const DrawEngineInfo _draw_engine_list[4] = {
	{DrawTrainEngine,DrawTrainEngineInfo},
	{DrawRoadVehEngine,DrawRoadVehEngineInfo},
	{DrawShipEngine,DrawShipEngineInfo},
	{DrawAircraftEngine,DrawAircraftEngineInfo},
};

static void EnginePreviewWndProc(Window *w, WindowEvent *e)
{
	byte eng;
	int engine;
	const DrawEngineInfo *dei;
	int width;

	switch(e->event) {
	case WE_PAINT:
		DrawWindowWidgets(w);
		engine = w->window_number;

		SET_DPARAM16(0, GetEngineCategoryName(engine));
		DrawStringMultiCenter(150, 44, STR_8101_WE_HAVE_JUST_DESIGNED_A, 296);

		DrawStringCentered(w->width >> 1, 80, GetCustomEngineName(engine), 0x10);

		eng = (byte)engine;
		(dei = _draw_engine_list,eng < NUM_TRAIN_ENGINES) ||
		(dei++,eng < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES) ||
		(dei++,eng < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES) ||
		(dei++, true);

		width = w->width;
		dei->engine_proc(width >> 1, 100, engine, 0);
		dei->info_proc(engine, width >> 1, 130, width - 52);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: DeleteWindow(w); break;
		case 4:
			DoCommandP(0, w->window_number, 0, NULL, CMD_WANT_ENGINE_PREVIEW);
			DeleteWindow(w);
			break;
		}
		break;
	}
}

static const WindowDesc _engine_preview_desc = {
	WDP_CENTER, WDP_CENTER, 300, 192,
	WC_ENGINE_PREVIEW,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_engine_preview_widgets,
	EnginePreviewWndProc
};


void ShowEnginePreviewWindow(int engine)
{
	Window *w;

	w = AllocateWindowDesc(&_engine_preview_desc);
	w->window_number = engine;
}

void DrawNewsNewTrainAvail(Window *w)
{
	int engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;
	SET_DPARAM16(0, GetEngineCategoryName(engine));
	DrawStringMultiCenter(w->width >> 1, 20, STR_8859_NEW_NOW_AVAILABLE, w->width - 2);

	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SET_DPARAM16(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_885A, w->width - 2);

	DrawTrainEngine(w->width >> 1, 88, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 112, 0x4323);
	DrawTrainEngineInfo(engine, w->width >> 1, 129, w->width - 52);
}

StringID GetNewsStringNewTrainAvail(NewsItem *ni)
{
	int engine = ni->string_id;
	SET_DPARAM16(0, STR_8859_NEW_NOW_AVAILABLE);
	SET_DPARAM16(1, GetEngineCategoryName(engine));
	SET_DPARAM16(2, GetCustomEngineName(engine));
	return STR_02B6;
}

void DrawNewsNewAircraftAvail(Window *w)
{
	int engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;

	DrawStringMultiCenter(w->width >> 1, 20, STR_A02C_NEW_AIRCRAFT_NOW_AVAILABLE, w->width - 2);
	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SET_DPARAM16(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_A02D, w->width - 2);

	DrawAircraftEngine(w->width >> 1, 93, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 110, 0x4323);
	DrawAircraftEngineInfo(engine, w->width >> 1, 131, w->width - 52);
}

StringID GetNewsStringNewAircraftAvail(NewsItem *ni)
{
	int engine = ni->string_id;
	SET_DPARAM16(0, STR_A02C_NEW_AIRCRAFT_NOW_AVAILABLE);
	SET_DPARAM16(1, GetCustomEngineName(engine));
	return STR_02B6;
}

void DrawNewsNewRoadVehAvail(Window *w)
{
	int engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;
	DrawStringMultiCenter(w->width >> 1, 20, STR_9028_NEW_ROAD_VEHICLE_NOW_AVAILABLE, w->width - 2);
	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SET_DPARAM16(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_9029, w->width - 2);

	DrawRoadVehEngine(w->width >> 1, 88, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 112, 0x4323);
	DrawRoadVehEngineInfo(engine, w->width >> 1, 129, w->width - 52);
}

StringID GetNewsStringNewRoadVehAvail(NewsItem *ni)
{
	int engine = ni->string_id;
	SET_DPARAM16(0, STR_9028_NEW_ROAD_VEHICLE_NOW_AVAILABLE);
	SET_DPARAM16(1, GetCustomEngineName(engine));
	return STR_02B6;
}

void DrawNewsNewShipAvail(Window *w)
{
	int engine;

	DrawNewsBorder(w);

	engine = WP(w,news_d).ni->string_id;

	DrawStringMultiCenter(w->width >> 1, 20, STR_982C_NEW_SHIP_NOW_AVAILABLE, w->width - 2);
	GfxFillRect(25, 56, w->width - 25, w->height - 2, 10);

	SET_DPARAM16(0, GetCustomEngineName(engine));
	DrawStringMultiCenter(w->width >> 1, 57, STR_982D, w->width - 2);

	DrawShipEngine(w->width >> 1, 93, engine, 0);
	GfxFillRect(25, 56, w->width - 56, 110, 0x4323);
	DrawShipEngineInfo(engine, w->width >> 1, 131, w->width - 52);
}

StringID GetNewsStringNewShipAvail(NewsItem *ni)
{
	int engine = ni->string_id;
	SET_DPARAM16(0, STR_982C_NEW_SHIP_NOW_AVAILABLE);
	SET_DPARAM16(1, GetCustomEngineName(engine));
	return STR_02B6;
}
