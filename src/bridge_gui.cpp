/* $Id$ */

/** @file bridge_gui.cpp Graphical user interface for bridge construction */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "strings.h"
#include "functions.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "command.h"
#include "sound.h"
#include "variables.h"
#include "bridge.h"

static struct BridgeData {
	uint count;
	TileIndex start_tile;
	TileIndex end_tile;
	byte type;
	byte indexes[MAX_BRIDGES];
	Money costs[MAX_BRIDGES];
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

static void BuildBridgeWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			DrawWindowWidgets(w);

			for (uint i = 0; i < 4 && i + w->vscroll.pos < _bridgedata.count; i++) {
				const Bridge *b = &_bridge[_bridgedata.indexes[i + w->vscroll.pos]];

				SetDParam(2, _bridgedata.costs[i + w->vscroll.pos]);
				SetDParam(1, b->speed * 10 / 16);
				SetDParam(0, b->material);
				DrawSprite(b->sprite, b->pal, 3, 15 + i * 22);

				DrawString(44, 15 + i * 22 , STR_500D, 0);
			}
			break;

		case WE_KEYPRESS: {
			uint i = e->we.keypress.keycode - '1';
			if (i < 9 && i < _bridgedata.count) {
				e->we.keypress.cont = false;
				BuildBridge(w, i);
			}

			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == 2) {
				uint ind = ((int)e->we.click.pt.y - 14) / 22;
				if (ind < 4 && (ind += w->vscroll.pos) < _bridgedata.count)
					BuildBridge(w, ind);
			}
			break;
	}
}

static const Widget _build_bridge_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   199,     0,    13, STR_100D_SELECT_RAIL_BRIDGE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,   RESIZE_NONE,     7,     0,   187,    14,   101, 0x401,                       STR_101F_BRIDGE_SELECTION_CLICK},
{  WWT_SCROLLBAR,   RESIZE_NONE,     7,   188,   199,    14,   101, 0x0,                         STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _build_bridge_desc = {
	WDP_AUTO, WDP_AUTO, 200, 102, 200, 102,
	WC_BUILD_BRIDGE, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_bridge_widgets,
	BuildBridgeWndProc
};


static const Widget _build_road_bridge_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   199,     0,    13, STR_1803_SELECT_ROAD_BRIDGE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,   RESIZE_NONE,     7,     0,   187,    14,   101, 0x401,                       STR_101F_BRIDGE_SELECTION_CLICK},
{  WWT_SCROLLBAR,   RESIZE_NONE,     7,   188,   199,    14,   101, 0x0,                         STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _build_road_bridge_desc = {
	WDP_AUTO, WDP_AUTO, 200, 102, 200, 102,
	WC_BUILD_BRIDGE, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_bridge_widgets,
	BuildBridgeWndProc
};


void ShowBuildBridgeWindow(TileIndex start, TileIndex end, byte bridge_type)
{
	uint j = 0;
	CommandCost ret;
	StringID errmsg;

	DeleteWindowById(WC_BUILD_BRIDGE, 0);

	_bridgedata.type = bridge_type;
	_bridgedata.start_tile = start;
	_bridgedata.end_tile = end;

	errmsg = INVALID_STRING_ID;

	// only query bridge building possibility once, result is the same for all bridges!
	// returns CMD_ERROR on failure, and price on success
	ret = DoCommand(end, start, (bridge_type << 8), DC_AUTO | DC_QUERY_COST, CMD_BUILD_BRIDGE);

	if (CmdFailed(ret)) {
		errmsg = _error_message;
	} else {
		// check which bridges can be built
		// get absolute bridge length
		// length of the middle parts of the bridge
		int bridge_len = GetBridgeLength(start, end);
		// total length of bridge
		int tot_bridgedata_len = bridge_len + 2;

		tot_bridgedata_len = CalcBridgeLenCostFactor(tot_bridgedata_len);

		for (bridge_type = 0; bridge_type != MAX_BRIDGES; bridge_type++) { // loop for all bridgetypes
			if (CheckBridge_Stuff(bridge_type, bridge_len)) {
				const Bridge *b = &_bridge[bridge_type];
				// bridge is accepted, add to list
				// add to terraforming & bulldozing costs the cost of the bridge itself (not computed with DC_QUERY_COST)
				_bridgedata.costs[j] = ret.GetCost() + (((int64)tot_bridgedata_len * _price.build_bridge * b->price) >> 8);
				_bridgedata.indexes[j] = bridge_type;
				j++;
			}
		}
	}

	_bridgedata.count = j;

	if (j != 0) {
		Window *w = AllocateWindowDesc((_bridgedata.type & 0x80) ? &_build_road_bridge_desc : &_build_bridge_desc);
		w->vscroll.cap = 4;
		w->vscroll.count = (byte)j;
	} else {
		ShowErrorMessage(errmsg, STR_5015_CAN_T_BUILD_BRIDGE_HERE, TileX(end) * TILE_SIZE, TileY(end) * TILE_SIZE);
	}
}
