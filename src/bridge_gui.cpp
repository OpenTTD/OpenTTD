/* $Id$ */

/** @file bridge_gui.cpp Graphical user interface for bridge construction */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "gui.h"
#include "window_gui.h"
#include "viewport.h"
#include "command_func.h"
#include "economy_func.h"
#include "sound.h"
#include "variables.h"
#include "bridge.h"
#include "strings_func.h"
#include "window_func.h"
#include "map_func.h"

static struct BridgeData {
	uint8 last_size;
	uint count;
	TileIndex start_tile;
	TileIndex end_tile;
	uint8 type;
	uint8 indexes[MAX_BRIDGES];
	Money costs[MAX_BRIDGES];

	BridgeData()
		: last_size(4)
		, count(0)
		{};
} _bridgedata;

void CcBuildBridge(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_27_BLACKSMITH_ANVIL, tile);
}

static void BuildBridge(Window *w, int i)
{
	DeleteWindow(w);
	DoCommandP(_bridgedata.end_tile, _bridgedata.start_tile,
		_bridgedata.indexes[i] | (_bridgedata.type << 8), CcBuildBridge,
		CMD_BUILD_BRIDGE | CMD_MSG(STR_5015_CAN_T_BUILD_BRIDGE_HERE));
}

/* Names of the build bridge selection window */
enum BuildBridgeSelectionWidgets {
	BBSW_CLOSEBOX = 0,
	BBSW_CAPTION,
	BBSW_BRIDGE_LIST,
	BBSW_SCROLLBAR,
	BBSW_RESIZEBOX
};

static void BuildBridgeWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			w->resize.step_height = 22;
			w->vscroll.count = _bridgedata.count;

			if (_bridgedata.last_size <= 4) {
				w->vscroll.cap = 4;
			} else {
				/* Resize the bridge selection window if we used a bigger one the last time */
				w->vscroll.cap = (w->vscroll.count > _bridgedata.last_size) ? _bridgedata.last_size : w->vscroll.count;
				ResizeWindow(w, 0, (w->vscroll.cap - 4) * w->resize.step_height);
				w->widget[BBSW_BRIDGE_LIST].data = (w->vscroll.cap << 8) + 1;
			}
			break;

		case WE_PAINT: {
			DrawWindowWidgets(w);

			uint y = 15;
			for (uint i = 0; (i < w->vscroll.cap) && ((i + w->vscroll.pos) < _bridgedata.count); i++) {
				const Bridge *b = &_bridge[_bridgedata.indexes[i + w->vscroll.pos]];

				SetDParam(2, _bridgedata.costs[i + w->vscroll.pos]);
				SetDParam(1, b->speed * 10 / 16);
				SetDParam(0, b->material);

				DrawSprite(b->sprite, b->pal, 3, y);
				DrawString(44, y, STR_500D, TC_FROMSTRING);
				y += w->resize.step_height;
			}
			break;
		}

		case WE_KEYPRESS: {
			const uint8 i = e->we.keypress.keycode - '1';
			if (i < 9 && i < _bridgedata.count) {
				e->we.keypress.cont = false;
				BuildBridge(w, i);
			}

			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == BBSW_BRIDGE_LIST) {
				uint ind = ((int)e->we.click.pt.y - 14) / w->resize.step_height;
				if (ind < w->vscroll.cap) {
					ind += w->vscroll.pos;
					if (ind < _bridgedata.count) {
						BuildBridge(w, ind);
					}
				}
			}
			break;

		case WE_RESIZE:
			w->vscroll.cap += e->we.sizing.diff.y / (int)w->resize.step_height;
			w->widget[BBSW_BRIDGE_LIST].data = (w->vscroll.cap << 8) + 1;
			SetVScrollCount(w, _bridgedata.count);

			_bridgedata.last_size = w->vscroll.cap;
			break;
	}
}

/* Widget definition for the rail bridge selection window */
static const Widget _build_bridge_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  7,   0,  10,   0,  13, STR_00C5,                    STR_018B_CLOSE_WINDOW},            // BBSW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  7,  11, 199,   0,  13, STR_100D_SELECT_RAIL_BRIDGE, STR_018C_WINDOW_TITLE_DRAG_THIS},  // BBSW_CAPTION
{     WWT_MATRIX, RESIZE_BOTTOM,  7,   0, 187,  14, 101, 0x401,                       STR_101F_BRIDGE_SELECTION_CLICK},  // BBSW_BRIDGE_LIST
{  WWT_SCROLLBAR, RESIZE_BOTTOM,  7, 188, 199,  14,  89, 0x0,                         STR_0190_SCROLL_BAR_SCROLLS_LIST}, // BBSW_SCROLLBAR
{  WWT_RESIZEBOX,     RESIZE_TB,  7, 188, 199,  90, 101, 0x0,                         STR_RESIZE_BUTTON},                // BBSW_RESIZEBOX
{   WIDGETS_END},
};

/* Window definition for the rail bridge selection window */
static const WindowDesc _build_bridge_desc = {
	WDP_AUTO, WDP_AUTO, 200, 102, 200, 102,
	WC_BUILD_BRIDGE, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_build_bridge_widgets,
	BuildBridgeWndProc
};

/* Widget definition for the road bridge selection window */
static const Widget _build_road_bridge_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  7,   0,  10,   0,  13, STR_00C5,                    STR_018B_CLOSE_WINDOW},            // BBSW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  7,  11, 199,   0,  13, STR_1803_SELECT_ROAD_BRIDGE, STR_018C_WINDOW_TITLE_DRAG_THIS},  // BBSW_CAPTION
{     WWT_MATRIX, RESIZE_BOTTOM,  7,   0, 187,  14, 101, 0x401,                       STR_101F_BRIDGE_SELECTION_CLICK},  // BBSW_BRIDGE_LIST
{  WWT_SCROLLBAR, RESIZE_BOTTOM,  7, 188, 199,  14,  89, 0x0,                         STR_0190_SCROLL_BAR_SCROLLS_LIST}, // BBSW_SCROLLBAR
{  WWT_RESIZEBOX,     RESIZE_TB,  7, 188, 199,  90, 101, 0x0,                         STR_RESIZE_BUTTON},                // BBSW_RESIZEBOX
{   WIDGETS_END},
};

/* Window definition for the road bridge selection window */
static const WindowDesc _build_road_bridge_desc = {
	WDP_AUTO, WDP_AUTO, 200, 102, 200, 102,
	WC_BUILD_BRIDGE, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_build_road_bridge_widgets,
	BuildBridgeWndProc
};


void ShowBuildBridgeWindow(TileIndex start, TileIndex end, byte bridge_type)
{
	DeleteWindowById(WC_BUILD_BRIDGE, 0);

	_bridgedata.type = bridge_type;
	_bridgedata.start_tile = start;
	_bridgedata.end_tile = end;

	/* only query bridge building possibility once, result is the same for all bridges!
	 * returns CMD_ERROR on failure, and price on success */
	StringID errmsg = INVALID_STRING_ID;
	CommandCost ret = DoCommand(end, start, (bridge_type << 8), DC_AUTO | DC_QUERY_COST, CMD_BUILD_BRIDGE);

	uint8 j = 0;
	if (CmdFailed(ret)) {
		errmsg = _error_message;
	} else {
		/* check which bridges can be built
		 * get absolute bridge length
		 * length of the middle parts of the bridge */
		const uint bridge_len = GetBridgeLength(start, end);
		/* total length of bridge */
		const uint tot_bridgedata_len = CalcBridgeLenCostFactor(bridge_len + 2);

		/* loop for all bridgetypes */
		for (bridge_type = 0; bridge_type != MAX_BRIDGES; bridge_type++) {
			if (CheckBridge_Stuff(bridge_type, bridge_len)) {
				/* bridge is accepted, add to list */
				const Bridge *b = &_bridge[bridge_type];
				/* Add to terraforming & bulldozing costs the cost of the
				 * bridge itself (not computed with DC_QUERY_COST) */
				_bridgedata.costs[j] = ret.GetCost() + (((int64)tot_bridgedata_len * _price.build_bridge * b->price) >> 8);
				_bridgedata.indexes[j] = bridge_type;
				j++;
			}
		}

		_bridgedata.count = j;
	}

	if (j != 0) {
		AllocateWindowDesc((_bridgedata.type & 0x80) ? &_build_road_bridge_desc : &_build_bridge_desc);
	} else {
		ShowErrorMessage(errmsg, STR_5015_CAN_T_BUILD_BRIDGE_HERE, TileX(end) * TILE_SIZE, TileY(end) * TILE_SIZE);
	}
}
