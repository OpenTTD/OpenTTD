/* $Id$ */

/** @file terraform_gui.cpp GUI related to terraforming the map. */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "clear_map.h"
#include "player_func.h"
#include "player_base.h"
#include "gui.h"
#include "window_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "signs_func.h"
#include "variables.h"
#include "functions.h"
#include "sound_func.h"
#include "station_base.h"
#include "unmovable_map.h"
#include "textbuf_gui.h"
#include "genworld.h"
#include "settings_type.h"
#include "tree_map.h"

#include "table/sprites.h"
#include "table/strings.h"

void CcTerraform(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
	} else {
		extern TileIndex _terraform_err_tile;
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
			SetTropicZone(tile, (_ctrl_pressed) ? TROPICZONE_NORMAL : TROPICZONE_DESERT);
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
			case MP_TREES:
				if (GetTreeGround(tile) == TREE_GROUND_SHORE) continue;
			/* FALL THROUGH */
			case MP_CLEAR:
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

	switch (e->we.place.select_proc) {
		case DDSP_DEMOLISH_AREA:
			DoCommandP(end_tile, start_tile, 0, CcPlaySound10, CMD_CLEAR_AREA | CMD_MSG(STR_00B5_CAN_T_CLEAR_THIS_AREA));
			break;
		case DDSP_RAISE_AND_LEVEL_AREA:
			DoCommandP(end_tile, start_tile, 1, CcTerraform, CMD_LEVEL_LAND | CMD_MSG(STR_0808_CAN_T_RAISE_LAND_HERE));
			break;
		case DDSP_LOWER_AND_LEVEL_AREA:
			DoCommandP(end_tile, start_tile, (uint32)-1, CcTerraform, CMD_LEVEL_LAND | CMD_MSG(STR_0809_CAN_T_LOWER_LAND_HERE));
			break;
		case DDSP_LEVEL_AREA:
			DoCommandP(end_tile, start_tile, 0, CcPlaySound10, CMD_LEVEL_LAND);
			break;
		case DDSP_CREATE_ROCKS:
			GenerateRockyArea(end_tile, start_tile);
			break;
		case DDSP_CREATE_DESERT:
			GenerateDesertArea(end_tile, start_tile);
			break;
		case DDSP_CREATE_WATER:
			DoCommandP(end_tile, start_tile, _ctrl_pressed, CcBuildCanal, CMD_BUILD_CANAL | CMD_MSG(STR_CANT_BUILD_CANALS));
			break;
		case DDSP_CREATE_RIVER:
			DoCommandP(end_tile, start_tile, 2, CcBuildCanal, CMD_BUILD_CANAL | CMD_MSG(STR_CANT_BUILD_CANALS));
			break;
		default:
			return false;
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
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_DEMOLISH_AREA);
}

static void PlaceProc_RaiseLand(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_RAISE_AND_LEVEL_AREA);
}

static void PlaceProc_LowerLand(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_LOWER_AND_LEVEL_AREA);
}

void PlaceProc_LevelLand(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_LEVEL_AREA);
}

static void TerraformClick_Lower(Window *w)
{
	HandlePlacePushButton(w, 4, ANIMCURSOR_LOWERLAND, VHM_POINT, PlaceProc_LowerLand);
}

static void TerraformClick_Raise(Window *w)
{
	HandlePlacePushButton(w, 5, ANIMCURSOR_RAISELAND, VHM_POINT, PlaceProc_RaiseLand);
}

static void TerraformClick_Level(Window *w)
{
	HandlePlacePushButton(w, 6, SPR_CURSOR_LEVEL_LAND, VHM_POINT, PlaceProc_LevelLand);
}

static void TerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, 7, ANIMCURSOR_DEMOLISH , VHM_RECT, PlaceProc_DemolishArea);
}

static void TerraformClick_BuyLand(Window *w)
{
	HandlePlacePushButton(w, 8, SPR_CURSOR_BUY_LAND, VHM_RECT, PlaceProc_BuyLand);
}

static void TerraformClick_Trees(Window *w)
{
	/* This button is NOT a place-push-button, so don't treat it as such */
	ShowBuildTreesToolbar();
}

static void TerraformClick_PlaceSign(Window *w)
{
	HandlePlacePushButton(w, 10, SPR_CURSOR_SIGN, VHM_RECT, PlaceProc_Sign);
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
		VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
		break;

	case WE_PLACE_MOUSEUP:
		if (e->we.place.pt.x != -1) {
			switch (e->we.place.select_proc) {
				case DDSP_DEMOLISH_AREA:
				case DDSP_RAISE_AND_LEVEL_AREA:
				case DDSP_LOWER_AND_LEVEL_AREA:
				case DDSP_LEVEL_AREA:
					GUIPlaceProcDragXY(e);
					break;
			}
		}
		break;

	case WE_ABORT_PLACE_OBJ:
		w->RaiseButtons();
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
{   WWT_IMGBTN,   RESIZE_NONE,     7, 136, 157,  14,  35, SPR_IMG_SIGN,            STR_0289_PLACE_SIGN},

{   WIDGETS_END},
};

static const WindowDesc _terraform_desc = {
	WDP_ALIGN_TBR, 22 + 36, 158, 36, 158, 36,
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

		link->SetDirty();
	}
}

static byte _terraform_size = 1;

/**
 * Raise/Lower a bigger chunk of land at the same time in the editor. When
 * raising get the lowest point, when lowering the highest point, and set all
 * tiles in the selection to that height.
 * @todo : Incorporate into game itself to allow for ingame raising/lowering of
 *         larger chunks at the same time OR remove altogether, as we have 'level land' ?
 * @param tile The top-left tile where the terraforming will start
 * @param mode 1 for raising, 0 for lowering land
 */
static void CommonRaiseLowerBigLand(TileIndex tile, int mode)
{
	int sizex, sizey;
	uint h;

	_generating_world = true; // used to create green terraformed land

	if (_terraform_size == 1) {
		StringID msg =
			mode ? STR_0808_CAN_T_RAISE_LAND_HERE : STR_0809_CAN_T_LOWER_LAND_HERE;

		DoCommandP(tile, SLOPE_N, (uint32)mode, CcTerraform, CMD_TERRAFORM_LAND | CMD_MSG(msg));
	} else {
		SndPlayTileFx(SND_1F_SPLAT, tile);

		assert(_terraform_size != 0);
		/* check out for map overflows */
		sizex = min(MapSizeX() - TileX(tile) - 1, _terraform_size);
		sizey = min(MapSizeY() - TileY(tile) - 1, _terraform_size);

		if (sizex == 0 || sizey == 0) return;

		if (mode != 0) {
			/* Raise land */
			h = 15; // XXX - max height
			BEGIN_TILE_LOOP(tile2, sizex, sizey, tile) {
				h = min(h, TileHeight(tile2));
			} END_TILE_LOOP(tile2, sizex, sizey, tile)
		} else {
			/* Lower land */
			h = 0;
			BEGIN_TILE_LOOP(tile2, sizex, sizey, tile) {
				h = max(h, TileHeight(tile2));
			} END_TILE_LOOP(tile2, sizex, sizey, tile)
		}

		BEGIN_TILE_LOOP(tile2, sizex, sizey, tile) {
			if (TileHeight(tile2) == h) {
				DoCommandP(tile2, SLOPE_N, (uint32)mode, NULL, CMD_TERRAFORM_LAND);
			}
		} END_TILE_LOOP(tile2, sizex, sizey, tile)
	}

	_generating_world = false;
}

static void PlaceProc_RaiseBigLand(TileIndex tile)
{
	CommonRaiseLowerBigLand(tile, 1);
}

static void PlaceProc_LowerBigLand(TileIndex tile)
{
	CommonRaiseLowerBigLand(tile, 0);
}

static void PlaceProc_RockyArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_ROCKS);
}

static void PlaceProc_LightHouse(TileIndex tile)
{
	/* not flat || not(trees || clear without bridge above) */
	if (GetTileSlope(tile, NULL) != SLOPE_FLAT || !(IsTileType(tile, MP_TREES) || (IsTileType(tile, MP_CLEAR) && !IsBridgeAbove(tile)))) {
		return;
	}

	MakeLighthouse(tile);
	MarkTileDirtyByTile(tile);
	SndPlayTileFx(SND_1F_SPLAT, tile);
}

static void PlaceProc_Transmitter(TileIndex tile)
{
	/* not flat || not(trees || clear without bridge above) */
	if (GetTileSlope(tile, NULL) != SLOPE_FLAT || !(IsTileType(tile, MP_TREES) || (IsTileType(tile, MP_CLEAR) && !IsBridgeAbove(tile)))) {
		return;
	}

	MakeTransmitter(tile);
	MarkTileDirtyByTile(tile);
	SndPlayTileFx(SND_1F_SPLAT, tile);
}

static void PlaceProc_DesertArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_DESERT);
}

static void PlaceProc_WaterArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_WATER);
}

static void PlaceProc_RiverArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_RIVER);
}

static const Widget _scen_edit_land_gen_widgets[] = {
{  WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                  STR_018B_CLOSE_WINDOW},
{   WWT_CAPTION,   RESIZE_NONE,     7,    11,   191,     0,    13, STR_0223_LAND_GENERATION,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_STICKYBOX,   RESIZE_NONE,     7,   192,   203,     0,    13, STR_NULL,                  STR_STICKY_BUTTON},
{     WWT_PANEL,   RESIZE_NONE,     7,     0,   203,    14,   102, 0x0,                       STR_NULL},
{    WWT_IMGBTN,   RESIZE_NONE,    14,     2,    23,    16,    37, SPR_IMG_DYNAMITE,          STR_018D_DEMOLISH_BUILDINGS_ETC},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    24,    45,    16,    37, SPR_IMG_TERRAFORM_DOWN,    STR_018E_LOWER_A_CORNER_OF_LAND},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    46,    67,    16,    37, SPR_IMG_TERRAFORM_UP,      STR_018F_RAISE_A_CORNER_OF_LAND},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    68,    89,    16,    37, SPR_IMG_LEVEL_LAND,        STR_LEVEL_LAND_TOOLTIP},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    90,   111,    16,    37, SPR_IMG_BUILD_CANAL,       STR_CREATE_LAKE},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   112,   133,    16,    37, SPR_IMG_BUILD_RIVER,       STR_CREATE_RIVER},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   134,   156,    16,    37, SPR_IMG_ROCKS,             STR_028C_PLACE_ROCKY_AREAS_ON_LANDSCAPE},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   157,   179,    16,    37, SPR_IMG_LIGHTHOUSE_DESERT, STR_NULL}, // XXX - dynamic
{    WWT_IMGBTN,   RESIZE_NONE,    14,   180,   201,    16,    37, SPR_IMG_TRANSMITTER,       STR_028E_PLACE_TRANSMITTER},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   150,   161,    45,    56, SPR_ARROW_UP,              STR_0228_INCREASE_SIZE_OF_LAND_AREA},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   150,   161,    58,    69, SPR_ARROW_DOWN,            STR_0229_DECREASE_SIZE_OF_LAND_AREA},
{   WWT_TEXTBTN,   RESIZE_NONE,    14,    24,   179,    76,    87, STR_SE_NEW_WORLD,          STR_022A_GENERATE_RANDOM_LAND},
{   WWT_TEXTBTN,   RESIZE_NONE,    14,    24,   179,    89,   100, STR_022B_RESET_LANDSCAPE,  STR_RESET_LANDSCAPE_TOOLTIP},
{   WIDGETS_END},
};

static const int8 _multi_terraform_coords[][2] = {
	{  0, -2},
	{  4,  0}, { -4,  0}, {  0,  2},
	{ -8,  2}, { -4,  4}, {  0,  6}, {  4,  4}, {  8,  2},
	{-12,  0}, { -8, -2}, { -4, -4}, {  0, -6}, {  4, -4}, {  8, -2}, { 12,  0},
	{-16,  2}, {-12,  4}, { -8,  6}, { -4,  8}, {  0, 10}, {  4,  8}, {  8,  6}, { 12,  4}, { 16,  2},
	{-20,  0}, {-16, -2}, {-12, -4}, { -8, -6}, { -4, -8}, {  0,-10}, {  4, -8}, {  8, -6}, { 12, -4}, { 16, -2}, { 20,  0},
	{-24,  2}, {-20,  4}, {-16,  6}, {-12,  8}, { -8, 10}, { -4, 12}, {  0, 14}, {  4, 12}, {  8, 10}, { 12,  8}, { 16,  6}, { 20,  4}, { 24,  2},
	{-28,  0}, {-24, -2}, {-20, -4}, {-16, -6}, {-12, -8}, { -8,-10}, { -4,-12}, {  0,-14}, {  4,-12}, {  8,-10}, { 12, -8}, { 16, -6}, { 20, -4}, { 24, -2}, { 28,  0},
};

/**
 * @todo Merge with terraform_gui.cpp (move there) after I have cooled down at its braindeadness
 * and changed OnButtonClick to include the widget as well in the function declaration. Post 0.4.0 - Darkvater
 */
static void EditorTerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, 4, ANIMCURSOR_DEMOLISH, VHM_RECT, PlaceProc_DemolishArea);
}

static void EditorTerraformClick_LowerBigLand(Window *w)
{
	HandlePlacePushButton(w, 5, ANIMCURSOR_LOWERLAND, VHM_POINT, PlaceProc_LowerBigLand);
}

static void EditorTerraformClick_RaiseBigLand(Window *w)
{
	HandlePlacePushButton(w, 6, ANIMCURSOR_RAISELAND, VHM_POINT, PlaceProc_RaiseBigLand);
}

static void EditorTerraformClick_LevelLand(Window *w)
{
	HandlePlacePushButton(w, 7, SPR_CURSOR_LEVEL_LAND, VHM_POINT, PlaceProc_LevelLand);
}

static void EditorTerraformClick_WaterArea(Window *w)
{
	HandlePlacePushButton(w, 8, SPR_CURSOR_CANAL, VHM_RECT, PlaceProc_WaterArea);
}

static void EditorTerraformClick_RiverArea(Window *w)
{
	HandlePlacePushButton(w, 9, SPR_CURSOR_RIVER, VHM_RECT, PlaceProc_RiverArea);
}

static void EditorTerraformClick_RockyArea(Window *w)
{
	HandlePlacePushButton(w, 10, SPR_CURSOR_ROCKY_AREA, VHM_RECT, PlaceProc_RockyArea);
}

static void EditorTerraformClick_DesertLightHouse(Window *w)
{
	HandlePlacePushButton(w, 11, SPR_CURSOR_LIGHTHOUSE, VHM_RECT, (_opt.landscape == LT_TROPIC) ? PlaceProc_DesertArea : PlaceProc_LightHouse);
}

static void EditorTerraformClick_Transmitter(Window *w)
{
	HandlePlacePushButton(w, 12, SPR_CURSOR_TRANSMITTER, VHM_RECT, PlaceProc_Transmitter);
}

static const uint16 _editor_terraform_keycodes[] = {
	'D',
	'Q',
	'W',
	'E',
	'R',
	'T',
	'Y',
	'U',
	'I'
};

typedef void OnButtonClick(Window *w);
static OnButtonClick * const _editor_terraform_button_proc[] = {
	EditorTerraformClick_Dynamite,
	EditorTerraformClick_LowerBigLand,
	EditorTerraformClick_RaiseBigLand,
	EditorTerraformClick_LevelLand,
	EditorTerraformClick_WaterArea,
	EditorTerraformClick_RiverArea,
	EditorTerraformClick_RockyArea,
	EditorTerraformClick_DesertLightHouse,
	EditorTerraformClick_Transmitter
};


/** Callback function for the scenario editor 'reset landscape' confirmation window
 * @param w Window unused
 * @param confirmed boolean value, true when yes was clicked, false otherwise */
static void ResetLandscapeConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		Player *p;

		/* Set generating_world to true to get instant-green grass after removing
		 * player property. */
		_generating_world = true;
		/* Delete all players */
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				ChangeOwnershipOfPlayerItems(p->index, PLAYER_SPECTATOR);
				p->is_active = false;
			}
		}
		_generating_world = false;

		/* Delete all stations owned by a player */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (IsValidPlayer(st->owner)) delete st;
		}
	}
}

static void ScenEditLandGenWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			/* XXX - lighthouse button is widget 11!! Don't forget when changing */
			w->widget[11].tooltips = (_opt.landscape == LT_TROPIC) ? STR_028F_DEFINE_DESERT_AREA : STR_028D_PLACE_LIGHTHOUSE;
			break;

		case WE_PAINT: {
			DrawWindowWidgets(w);

			int n = _terraform_size * _terraform_size;
			const int8 *coords = &_multi_terraform_coords[0][0];

			assert(n != 0);
			do {
				DrawSprite(SPR_WHITE_POINT, PAL_NONE, 88 + coords[0], 55 + coords[1]);
				coords += 2;
			} while (--n);

			if (w->IsWidgetLowered(5) || w->IsWidgetLowered(6)) // change area-size if raise/lower corner is selected
				SetTileSelectSize(_terraform_size, _terraform_size);

		} break;

		case WE_KEYPRESS:
			for (uint i = 0; i != lengthof(_editor_terraform_keycodes); i++) {
				if (e->we.keypress.keycode == _editor_terraform_keycodes[i]) {
					e->we.keypress.cont = false;
					_editor_terraform_button_proc[i](w);
					break;
				}
			}
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12:
					_editor_terraform_button_proc[e->we.click.widget - 4](w);
					break;
				case 13: case 14: { // Increase/Decrease terraform size
					int size = (e->we.click.widget == 13) ? 1 : -1;
					w->HandleButtonClick(e->we.click.widget);
					size += _terraform_size;

					if (!IsInsideMM(size, 1, 8 + 1)) return;
					_terraform_size = size;

					SndPlayFx(SND_15_BEEP);
					w->SetDirty();
				} break;
				case 15: // gen random land
					w->HandleButtonClick(15);
					ShowCreateScenario();
					break;
				case 16: // Reset landscape
					ShowQuery(
						STR_022C_RESET_LANDSCAPE,
						STR_RESET_LANDSCAPE_CONFIRMATION_TEXT,
						NULL,
						ResetLandscapeConfirmationCallback);
					break;
			}
			break;

		case WE_TIMEOUT:
			for (uint i = 0; i < w->widget_count; i++) {
				if (w->IsWidgetLowered(i)) {
					w->RaiseWidget(i);
					w->InvalidateWidget(i);
				}
				if (i == 3) i = 12;
			}
			break;

		case WE_PLACE_OBJ:
			_place_proc(e->we.place.tile);
			break;

		case WE_PLACE_DRAG:
			VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
			break;

		case WE_PLACE_MOUSEUP:
			if (e->we.place.pt.x != -1) {
				switch (e->we.place.select_proc) {
					case DDSP_CREATE_ROCKS:
					case DDSP_CREATE_DESERT:
					case DDSP_CREATE_WATER:
					case DDSP_CREATE_RIVER:
					case DDSP_RAISE_AND_LEVEL_AREA:
					case DDSP_LOWER_AND_LEVEL_AREA:
					case DDSP_LEVEL_AREA:
					case DDSP_DEMOLISH_AREA:
						GUIPlaceProcDragXY(e);
						break;
				}
			}
			break;

		case WE_ABORT_PLACE_OBJ:
			w->RaiseButtons();
			w->SetDirty();
			break;
	}
}

static const WindowDesc _scen_edit_land_gen_desc = {
	WDP_AUTO, WDP_AUTO, 204, 103, 204, 103,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_scen_edit_land_gen_widgets,
	ScenEditLandGenWndProc,
};

void ShowEditorTerraformToolbar()
{
	AllocateWindowDescFront(&_scen_edit_land_gen_desc, 0);
}
