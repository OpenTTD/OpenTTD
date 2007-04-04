/* $Id$ */

/** @file terraform_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "clear_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "player.h"
#include "tile.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "sound.h"
#include "command.h"
#include "vehicle.h"
#include "signs.h"
#include "variables.h"

void CcTerraform(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
	} else {
		SetRedErrorSquare(_terraform_err_tile);
	}
}


/** Scenario editor command that generates desert areas */
static void GenerateDesertArea(TileIndex end, TileIndex start)
{
	int size_x, size_y;
	int sx = TileX(start);
	int sy = TileY(start);
	int ex = TileX(end);
	int ey = TileY(end);

	if (_game_mode != GM_EDITOR) return;

	if (ex < sx) Swap(ex, sx);
	if (ey < sy) Swap(ey, sy);
	size_x = (ex - sx) + 1;
	size_y = (ey - sy) + 1;

	_generating_world = true;
	BEGIN_TILE_LOOP(tile, size_x, size_y, TileXY(sx, sy)) {
		if (GetTileType(tile) != MP_WATER) {
			SetTropicZone(tile, (_ctrl_pressed) ? TROPICZONE_INVALID : TROPICZONE_DESERT);
			DoCommandP(tile, 0, 0, NULL, CMD_LANDSCAPE_CLEAR);
			MarkTileDirtyByTile(tile);
		}
	} END_TILE_LOOP(tile, size_x, size_y, 0);
	_generating_world = false;
}

/** Scenario editor command that generates rocky areas */
static void GenerateRockyArea(TileIndex end, TileIndex start)
{
	int size_x, size_y;
	bool success = false;
	int sx = TileX(start);
	int sy = TileY(start);
	int ex = TileX(end);
	int ey = TileY(end);

	if (_game_mode != GM_EDITOR) return;

	if (ex < sx) Swap(ex, sx);
	if (ey < sy) Swap(ey, sy);
	size_x = (ex - sx) + 1;
	size_y = (ey - sy) + 1;

	BEGIN_TILE_LOOP(tile, size_x, size_y, TileXY(sx, sy)) {
		switch (GetTileType(tile)) {
			case MP_CLEAR:
			case MP_TREES:
				MakeClear(tile, CLEAR_ROCKS, 3);
				break;

			default: continue;
		}
		MarkTileDirtyByTile(tile);
		success = true;
	} END_TILE_LOOP(tile, size_x, size_y, 0);

	if (success) SndPlayTileFx(SND_1F_SPLAT, end);
}

/**
 * A central place to handle all X_AND_Y dragged GUI functions.
 * @param e WindowEvent variable holding in its higher bits (excluding the lower
 * 4, since that defined the X_Y drag) the type of action to be performed
 * @return Returns true if the action was found and handled, and false otherwise. This
 * allows for additional implements that are more local. For example X_Y drag
 * of convertrail which belongs in rail_gui.cpp and not terraform_gui.cpp
 **/
bool GUIPlaceProcDragXY(const WindowEvent *e)
{
	TileIndex start_tile = e->we.place.starttile;
	TileIndex end_tile = e->we.place.tile;

	switch (e->we.place.userdata >> 4) {
	case GUI_PlaceProc_DemolishArea >> 4:
		DoCommandP(end_tile, start_tile, 0, CcPlaySound10, CMD_CLEAR_AREA | CMD_MSG(STR_00B5_CAN_T_CLEAR_THIS_AREA));
		break;
	case GUI_PlaceProc_LevelArea >> 4:
		DoCommandP(end_tile, start_tile, 0, CcPlaySound10, CMD_LEVEL_LAND | CMD_AUTO);
		break;
	case GUI_PlaceProc_RockyArea >> 4:
		GenerateRockyArea(end_tile, start_tile);
		break;
	case GUI_PlaceProc_DesertArea >> 4:
		GenerateDesertArea(end_tile, start_tile);
		break;
	case GUI_PlaceProc_WaterArea >> 4:
		DoCommandP(end_tile, start_tile, _ctrl_pressed, CcBuildCanal, CMD_BUILD_CANAL | CMD_AUTO | CMD_MSG(STR_CANT_BUILD_CANALS));
		break;
	default: return false;
	}

	return true;
}

typedef void OnButtonClick(Window *w);

static const uint16 _terraform_keycodes[] = {
	'Q',
	'W',
	'E',
	'D',
	'U',
	'I',
	'O',
};

void PlaceProc_DemolishArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y | GUI_PlaceProc_DemolishArea);
}

static void PlaceProc_RaiseLand(TileIndex tile)
{
	DoCommandP(
		tile, SLOPE_N, 1, CcTerraform,
		CMD_TERRAFORM_LAND | CMD_AUTO | CMD_MSG(STR_0808_CAN_T_RAISE_LAND_HERE)
	);
}

static void PlaceProc_LowerLand(TileIndex tile)
{
	DoCommandP(
		tile, SLOPE_N, 0, CcTerraform,
		CMD_TERRAFORM_LAND | CMD_AUTO | CMD_MSG(STR_0809_CAN_T_LOWER_LAND_HERE)
	);
}

void PlaceProc_LevelLand(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y | GUI_PlaceProc_LevelArea);
}

static void TerraformClick_Lower(Window *w)
{
	HandlePlacePushButton(w, 4, ANIMCURSOR_LOWERLAND, 2, PlaceProc_LowerLand);
}

static void TerraformClick_Raise(Window *w)
{
	HandlePlacePushButton(w, 5, ANIMCURSOR_RAISELAND, 2, PlaceProc_RaiseLand);
}

static void TerraformClick_Level(Window *w)
{
	HandlePlacePushButton(w, 6, SPR_CURSOR_LEVEL_LAND, 2, PlaceProc_LevelLand);
}

static void TerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, 7, ANIMCURSOR_DEMOLISH , 1, PlaceProc_DemolishArea);
}

static void TerraformClick_BuyLand(Window *w)
{
	HandlePlacePushButton(w, 8, SPR_CURSOR_BUY_LAND, 1, PlaceProc_BuyLand);
}

static void TerraformClick_Trees(Window *w)
{
	/* This button is NOT a place-push-button, so don't treat it as such */
	ShowBuildTreesToolbar();
}

static void TerraformClick_PlaceSign(Window *w)
{
	HandlePlacePushButton(w, 10, SPR_CURSOR_SIGN, 1, PlaceProc_Sign);
}

static OnButtonClick * const _terraform_button_proc[] = {
	TerraformClick_Lower,
	TerraformClick_Raise,
	TerraformClick_Level,
	TerraformClick_Dynamite,
	TerraformClick_BuyLand,
	TerraformClick_Trees,
	TerraformClick_PlaceSign,
};

static void TerraformToolbWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		DrawWindowWidgets(w);
		break;

	case WE_CLICK:
		if (e->we.click.widget >= 4) _terraform_button_proc[e->we.click.widget - 4](w);
		break;

	case WE_KEYPRESS: {
		uint i;

		for (i = 0; i != lengthof(_terraform_keycodes); i++) {
			if (e->we.keypress.keycode == _terraform_keycodes[i]) {
				e->we.keypress.cont = false;
				_terraform_button_proc[i](w);
				break;
			}
		}
		break;
	}

	case WE_PLACE_OBJ:
		_place_proc(e->we.place.tile);
		return;

	case WE_PLACE_DRAG:
		VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.userdata & 0xF);
		break;

	case WE_PLACE_MOUSEUP:
		if (e->we.place.pt.x != -1 &&
				(e->we.place.userdata & 0xF) == VPM_X_AND_Y) { // dragged actions
			GUIPlaceProcDragXY(e);
		}
		break;

	case WE_ABORT_PLACE_OBJ:
		RaiseWindowButtons(w);
		break;
	}
}

static const Widget _terraform_widgets[] = {
{ WWT_CLOSEBOX,   RESIZE_NONE,     7,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION,   RESIZE_NONE,     7,  11, 145,   0,  13, STR_LANDSCAPING_TOOLBAR, STR_018C_WINDOW_TITLE_DRAG_THIS},
{WWT_STICKYBOX,   RESIZE_NONE,     7, 146, 157,   0,  13, STR_NULL,                STR_STICKY_BUTTON},

{    WWT_PANEL,   RESIZE_NONE,     7,  66,  69,  14,  35, 0x0,                    STR_NULL},
{   WWT_IMGBTN,   RESIZE_NONE,     7,   0,  21,  14,  35, SPR_IMG_TERRAFORM_DOWN,  STR_018E_LOWER_A_CORNER_OF_LAND},
{   WWT_IMGBTN,   RESIZE_NONE,     7,  22,  43,  14,  35, SPR_IMG_TERRAFORM_UP,    STR_018F_RAISE_A_CORNER_OF_LAND},
{   WWT_IMGBTN,   RESIZE_NONE,     7,  44,  65,  14,  35, SPR_IMG_LEVEL_LAND,      STR_LEVEL_LAND_TOOLTIP},
{   WWT_IMGBTN,   RESIZE_NONE,     7,  70,  91,  14,  35, SPR_IMG_DYNAMITE,        STR_018D_DEMOLISH_BUILDINGS_ETC},
{   WWT_IMGBTN,   RESIZE_NONE,     7,  92, 113,  14,  35, SPR_IMG_BUY_LAND,        STR_0329_PURCHASE_LAND_FOR_FUTURE},
{   WWT_IMGBTN,   RESIZE_NONE,     7, 114, 135,  14,  35, SPR_IMG_PLANTTREES,      STR_0185_PLANT_TREES_PLACE_SIGNS},
{   WWT_IMGBTN,   RESIZE_NONE,     7, 136, 157,  14,  35, SPR_IMG_PLACE_SIGN,      STR_0289_PLACE_SIGN},

{   WIDGETS_END},
};

static const WindowDesc _terraform_desc = {
	WDP_ALIGN_TBR, 22+36, 158, 36,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_terraform_widgets,
	TerraformToolbWndProc
};

void ShowTerraformToolbar(Window *link)
{
	if (!IsValidPlayer(_current_player)) return;
	Window *w = AllocateWindowDescFront(&_terraform_desc, 0);
	if (w != NULL && link != NULL) {
		/* Align the terraform toolbar under the main toolbar and put the linked
		 * toolbar to left of it
		 */
		w->top = 22;
		link->left = w->left - link->width;
	}
}
