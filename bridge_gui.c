/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "command.h"
#include "sound.h"
#include "variables.h"

static struct BridgeData {
	uint count;
	TileIndex start_tile;
	TileIndex end_tile;
	byte type;
	byte indexes[MAX_BRIDGES];
	int32 costs[MAX_BRIDGES];
} _bridge;

extern const uint16 _bridge_type_price_mod[MAX_BRIDGES];

extern const PalSpriteID _bridge_sprites[MAX_BRIDGES];
extern const uint16 _bridge_speeds[MAX_BRIDGES];
extern const StringID _bridge_material[MAX_BRIDGES];

void CcBuildBridge(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_27_BLACKSMITH_ANVIL, tile);
}

static void BuildBridge(Window *w, int i)
{
	DeleteWindow(w);
	DoCommandP(_bridge.end_tile, _bridge.start_tile, _bridge.indexes[i] | (_bridge.type << 8), CcBuildBridge,
		CMD_BUILD_BRIDGE | CMD_AUTO | CMD_MSG(STR_5015_CAN_T_BUILD_BRIDGE_HERE));
}

static void BuildBridgeWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		uint i;

		DrawWindowWidgets(w);

		for (i = 0; i < 4 && i + w->vscroll.pos < _bridge.count; i++) {
			int ind = _bridge.indexes[i + w->vscroll.pos];

			SetDParam(2, _bridge.costs[i + w->vscroll.pos]);
			SetDParam(1, (_bridge_speeds[ind] >> 4) * 10);
			SetDParam(0, _bridge_material[ind]);
			DrawSprite(_bridge_sprites[ind], 3, 15 + i * 22);

			DrawString(44, 15 + i*22 , STR_500D, 0);
		}
	} break;

	case WE_KEYPRESS: {
		uint i = e->keypress.keycode - '1';
		if (i < 9 && i < _bridge.count) {
			e->keypress.cont = false;
			BuildBridge(w, i);
		}

		break;
	}

	case WE_CLICK:
	 if (e->click.widget == 2) {
			uint ind = ((int)e->click.pt.y - 14) / 22;
			if (ind < 4 && (ind += w->vscroll.pos) < _bridge.count)
				BuildBridge(w, ind);
		}
		break;
	}
}

static const Widget _build_bridge_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   199,     0,    13, STR_100D_SELECT_RAIL_BRIDGE,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,   RESIZE_NONE,     7,     0,   187,    14,   101, 0x401,												STR_101F_BRIDGE_SELECTION_CLICK},
{  WWT_SCROLLBAR,   RESIZE_NONE,     7,   188,   199,    14,   101, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _build_bridge_desc = {
	-1, -1, 200, 102,
	WC_BUILD_BRIDGE,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_bridge_widgets,
	BuildBridgeWndProc
};


static const Widget _build_road_bridge_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   199,     0,    13, STR_1803_SELECT_ROAD_BRIDGE,	STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_MATRIX,   RESIZE_NONE,     7,     0,   187,    14,   101, 0x401,												STR_101F_BRIDGE_SELECTION_CLICK},
{  WWT_SCROLLBAR,   RESIZE_NONE,     7,   188,   199,    14,   101, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _build_road_bridge_desc = {
	-1, -1, 200, 102,
	WC_BUILD_BRIDGE,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_bridge_widgets,
	BuildBridgeWndProc
};


void ShowBuildBridgeWindow(TileIndex start, TileIndex end, byte bridge_type)
{
	uint j = 0;
	int32 ret;
	uint16 errmsg;

	DeleteWindowById(WC_BUILD_BRIDGE, 0);

	_bridge.type = bridge_type;
	_bridge.start_tile = start;
	_bridge.end_tile = end;

	errmsg = 0xFFFF;

	// only query bridge building possibility once, result is the same for all bridges!
	// returns CMD_ERROR on failure, and priCe on success
	ret = DoCommandByTile(end, start, (bridge_type << 8), DC_AUTO | DC_QUERY_COST, CMD_BUILD_BRIDGE);

	if (ret == CMD_ERROR) {
		errmsg = _error_message;
	}
	// check which bridges can be built
	else {
		int bridge_len;			// length of the middle parts of the bridge
		int tot_bridge_len;	// total length of bridge

		// get absolute bridge length
		bridge_len = GetBridgeLength(start, end);
		tot_bridge_len = bridge_len + 2;

		tot_bridge_len = CalcBridgeLenCostFactor(tot_bridge_len);

		for (bridge_type = 0; bridge_type != MAX_BRIDGES; bridge_type++) {	// loop for all bridgetypes

			if (CheckBridge_Stuff(bridge_type, bridge_len)) {
				// bridge is accepted, add to list
				// add to terraforming & bulldozing costs the cost of the bridge itself (not computed with DC_QUERY_COST)
				_bridge.costs[j] = ret + (((int64)tot_bridge_len * _price.build_bridge * _bridge_type_price_mod[bridge_type]) >> 8);
				_bridge.indexes[j] = bridge_type;
				j++;
			}
		}
	}

	_bridge.count = j;

	if (j != 0) {
		Window *w = AllocateWindowDesc((_bridge.type & 0x80) ? &_build_road_bridge_desc : &_build_bridge_desc);
		w->vscroll.cap = 4;
		w->vscroll.count = (byte)j;
	} else {
		ShowErrorMessage(errmsg, STR_5015_CAN_T_BUILD_BRIDGE_HERE, TileX(end) * 16, TileY(end) * 16);
	}
}
