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
#include "landscape_type.h"
#include "tilehighlight_func.h"

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
bool GUIPlaceProcDragXY(ViewportDragDropSelectionProcess proc, TileIndex start_tile, TileIndex end_tile)
{
	switch (proc) {
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

void CcPlaySound1E(bool success, TileIndex tile, uint32 p1, uint32 p2);

static void PlaceProc_BuyLand(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_PURCHASE_LAND_AREA | CMD_NO_WATER | CMD_MSG(STR_5806_CAN_T_PURCHASE_THIS_LAND));
}

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

static void PlaceProc_LevelLand(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_LEVEL_AREA);
}

/** Enum referring to the widgets of the terraform toolbar */
enum TerraformToolbarWidgets {
	TTW_CLOSEBOX = 0,                     ///< Close window button
	TTW_CAPTION,                          ///< Window caption
	TTW_STICKY,                           ///< Sticky window button
	TTW_SEPERATOR,                        ///< Thin seperator line between level land button and demolish button
	TTW_BUTTONS_START,                    ///< Start of pushable buttons
	TTW_LOWER_LAND = TTW_BUTTONS_START,   ///< Lower land button
	TTW_RAISE_LAND,                       ///< Raise land button
	TTW_LEVEL_LAND,                       ///< Level land button
	TTW_DEMOLISH,                         ///< Demolish aka dynamite button
	TTW_BUY_LAND,                         ///< Buy land button
	TTW_PLANT_TREES,                      ///< Plant trees button (note: opens seperate window, no place-push-button)
	TTW_PLACE_SIGN,                       ///< Place sign button
};

static void TerraformClick_Lower(Window *w)
{
	HandlePlacePushButton(w, TTW_LOWER_LAND, ANIMCURSOR_LOWERLAND, VHM_POINT, PlaceProc_LowerLand);
}

static void TerraformClick_Raise(Window *w)
{
	HandlePlacePushButton(w, TTW_RAISE_LAND, ANIMCURSOR_RAISELAND, VHM_POINT, PlaceProc_RaiseLand);
}

static void TerraformClick_Level(Window *w)
{
	HandlePlacePushButton(w, TTW_LEVEL_LAND, SPR_CURSOR_LEVEL_LAND, VHM_POINT, PlaceProc_LevelLand);
}

static void TerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, TTW_DEMOLISH, ANIMCURSOR_DEMOLISH , VHM_RECT, PlaceProc_DemolishArea);
}

static void TerraformClick_BuyLand(Window *w)
{
	HandlePlacePushButton(w, TTW_BUY_LAND, SPR_CURSOR_BUY_LAND, VHM_RECT, PlaceProc_BuyLand);
}

static void TerraformClick_Trees(Window *w)
{
	/* This button is NOT a place-push-button, so don't treat it as such */
	ShowBuildTreesToolbar();
}

static void TerraformClick_PlaceSign(Window *w)
{
	HandlePlacePushButton(w, TTW_PLACE_SIGN, SPR_CURSOR_SIGN, VHM_RECT, PlaceProc_Sign);
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

struct TerraformToolbarWindow : Window {
	TerraformToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	~TerraformToolbarWindow()
	{
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= TTW_BUTTONS_START) _terraform_button_proc[widget - TTW_BUTTONS_START](this);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		for (uint i = 0; i != lengthof(_terraform_keycodes); i++) {
			if (keycode == _terraform_keycodes[i]) {
				_terraform_button_proc[i](this);
				return ES_HANDLED;
			}
		}
		return ES_NOT_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1) {
			switch (select_proc) {
				default: NOT_REACHED();
				case DDSP_DEMOLISH_AREA:
				case DDSP_RAISE_AND_LEVEL_AREA:
				case DDSP_LOWER_AND_LEVEL_AREA:
				case DDSP_LEVEL_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
			}
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
	}
};

static const Widget _terraform_widgets[] = {
{ WWT_CLOSEBOX,   RESIZE_NONE,     7,   0,  10,   0,  13, STR_00C5,                STR_018B_CLOSE_WINDOW},             // TTW_CLOSEBOX
{  WWT_CAPTION,   RESIZE_NONE,     7,  11, 145,   0,  13, STR_LANDSCAPING_TOOLBAR, STR_018C_WINDOW_TITLE_DRAG_THIS},   // TTW_CAPTION
{WWT_STICKYBOX,   RESIZE_NONE,     7, 146, 157,   0,  13, STR_NULL,                STR_STICKY_BUTTON},                 // TTW_STICKY

{    WWT_PANEL,   RESIZE_NONE,     7,  66,  69,  14,  35, 0x0,                    STR_NULL},                           // TTW_SEPERATOR
{   WWT_IMGBTN,   RESIZE_NONE,     7,   0,  21,  14,  35, SPR_IMG_TERRAFORM_DOWN,  STR_018E_LOWER_A_CORNER_OF_LAND},   // TTW_LOWER_LAND
{   WWT_IMGBTN,   RESIZE_NONE,     7,  22,  43,  14,  35, SPR_IMG_TERRAFORM_UP,    STR_018F_RAISE_A_CORNER_OF_LAND},   // TTW_RAISE_LAND
{   WWT_IMGBTN,   RESIZE_NONE,     7,  44,  65,  14,  35, SPR_IMG_LEVEL_LAND,      STR_LEVEL_LAND_TOOLTIP},            // TTW_LEVEL_LAND
{   WWT_IMGBTN,   RESIZE_NONE,     7,  70,  91,  14,  35, SPR_IMG_DYNAMITE,        STR_018D_DEMOLISH_BUILDINGS_ETC},   // TTW_DEMOLISH
{   WWT_IMGBTN,   RESIZE_NONE,     7,  92, 113,  14,  35, SPR_IMG_BUY_LAND,        STR_0329_PURCHASE_LAND_FOR_FUTURE}, // TTW_BUY_LAND
{   WWT_IMGBTN,   RESIZE_NONE,     7, 114, 135,  14,  35, SPR_IMG_PLANTTREES,      STR_0185_PLANT_TREES_PLACE_SIGNS},  // TTW_PLANT_TREES
{   WWT_IMGBTN,   RESIZE_NONE,     7, 136, 157,  14,  35, SPR_IMG_SIGN,            STR_0289_PLACE_SIGN},               // TTW_PLACE_SIGN

{   WIDGETS_END},
};

static const WindowDesc _terraform_desc = {
	WDP_ALIGN_TBR, 22 + 36, 158, 36, 158, 36,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_terraform_widgets,
};

void ShowTerraformToolbar(Window *link)
{
	if (!IsValidPlayer(_current_player)) return;
	Window *w = AllocateWindowDescFront<TerraformToolbarWindow>(&_terraform_desc, 0);
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
{  WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                  STR_018B_CLOSE_WINDOW},                   // ETTW_CLOSEBOX
{   WWT_CAPTION,   RESIZE_NONE,     7,    11,   191,     0,    13, STR_0223_LAND_GENERATION,  STR_018C_WINDOW_TITLE_DRAG_THIS},         // ETTW_CAPTION
{ WWT_STICKYBOX,   RESIZE_NONE,     7,   192,   203,     0,    13, STR_NULL,                  STR_STICKY_BUTTON},                       // ETTW_STICKY
{     WWT_PANEL,   RESIZE_NONE,     7,     0,   203,    14,   102, 0x0,                       STR_NULL},                                // ETTW_BACKGROUND
{    WWT_IMGBTN,   RESIZE_NONE,    14,     2,    23,    16,    37, SPR_IMG_DYNAMITE,          STR_018D_DEMOLISH_BUILDINGS_ETC},         // ETTW_DEMOLISH
{    WWT_IMGBTN,   RESIZE_NONE,    14,    24,    45,    16,    37, SPR_IMG_TERRAFORM_DOWN,    STR_018E_LOWER_A_CORNER_OF_LAND},         // ETTW_LOWER_LAND
{    WWT_IMGBTN,   RESIZE_NONE,    14,    46,    67,    16,    37, SPR_IMG_TERRAFORM_UP,      STR_018F_RAISE_A_CORNER_OF_LAND},         // ETTW_RAISE_LAND
{    WWT_IMGBTN,   RESIZE_NONE,    14,    68,    89,    16,    37, SPR_IMG_LEVEL_LAND,        STR_LEVEL_LAND_TOOLTIP},                  // ETTW_LEVEL_LAND
{    WWT_IMGBTN,   RESIZE_NONE,    14,    90,   111,    16,    37, SPR_IMG_BUILD_CANAL,       STR_CREATE_LAKE},                         // ETTW_BUILD_CANAL
{    WWT_IMGBTN,   RESIZE_NONE,    14,   112,   133,    16,    37, SPR_IMG_BUILD_RIVER,       STR_CREATE_RIVER},                        // ETTW_BUILD_RIVER
{    WWT_IMGBTN,   RESIZE_NONE,    14,   134,   156,    16,    37, SPR_IMG_ROCKS,             STR_028C_PLACE_ROCKY_AREAS_ON_LANDSCAPE}, // ETTW_PLACE_ROCKS
{    WWT_IMGBTN,   RESIZE_NONE,    14,   157,   179,    16,    37, SPR_IMG_LIGHTHOUSE_DESERT, STR_NULL},                                // ETTW_PLACE_DESERT_LIGHTHOUSE XXX - dynamic
{    WWT_IMGBTN,   RESIZE_NONE,    14,   180,   201,    16,    37, SPR_IMG_TRANSMITTER,       STR_028E_PLACE_TRANSMITTER},              // ETTW_PLACE_TRANSMITTER
{    WWT_IMGBTN,   RESIZE_NONE,    14,   150,   161,    45,    56, SPR_ARROW_UP,              STR_0228_INCREASE_SIZE_OF_LAND_AREA},     // ETTW_INCREASE_SIZE
{    WWT_IMGBTN,   RESIZE_NONE,    14,   150,   161,    58,    69, SPR_ARROW_DOWN,            STR_0229_DECREASE_SIZE_OF_LAND_AREA},     // ETTW_DECREASE_SIZE
{   WWT_TEXTBTN,   RESIZE_NONE,    14,    24,   179,    76,    87, STR_SE_NEW_WORLD,          STR_022A_GENERATE_RANDOM_LAND},           // ETTW_NEW_SCENARIO
{   WWT_TEXTBTN,   RESIZE_NONE,    14,    24,   179,    89,   100, STR_022B_RESET_LANDSCAPE,  STR_RESET_LANDSCAPE_TOOLTIP},             // ETTW_RESET_LANDSCAPE
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

/** Enum referring to the widgets of the editor terraform toolbar */
enum EditorTerraformToolbarWidgets {
	ETTW_START = 0,                        ///< Used for iterations
	ETTW_CLOSEBOX = ETTW_START,            ///< Close window button
	ETTW_CAPTION,                          ///< Window caption
	ETTW_STICKY,                           ///< Sticky window button
	ETTW_BACKGROUND,                       ///< Background of the lower part of the window
	ETTW_BUTTONS_START,                    ///< Start of pushable buttons
	ETTW_DEMOLISH = ETTW_BUTTONS_START,    ///< Demolish aka dynamite button
	ETTW_LOWER_LAND,                       ///< Lower land button
	ETTW_RAISE_LAND,                       ///< Raise land button
	ETTW_LEVEL_LAND,                       ///< Level land button
	ETTW_BUILD_CANAL,                      ///< Build canal button
	ETTW_BUILD_RIVER,                      ///< Build river button
	ETTW_PLACE_ROCKS,                      ///< Place rocks button
	ETTW_PLACE_DESERT_LIGHTHOUSE,          ///< Place desert button (in tropical climate) / place lighthouse button (else)
	ETTW_PLACE_TRANSMITTER,                ///< Place transmitter button
	ETTW_BUTTONS_END,                      ///< End of pushable buttons
	ETTW_INCREASE_SIZE = ETTW_BUTTONS_END, ///< Upwards arrow button to increase terraforming size
	ETTW_DECREASE_SIZE,                    ///< Downwards arrow button to decrease terraforming size
	ETTW_NEW_SCENARIO,                     ///< Button for generating a new scenario
	ETTW_RESET_LANDSCAPE,                  ///< Button for removing all player-owned property
};

/**
 * @todo Merge with terraform_gui.cpp (move there) after I have cooled down at its braindeadness
 * and changed OnButtonClick to include the widget as well in the function declaration. Post 0.4.0 - Darkvater
 */
static void EditorTerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, ETTW_DEMOLISH, ANIMCURSOR_DEMOLISH, VHM_RECT, PlaceProc_DemolishArea);
}

static void EditorTerraformClick_LowerBigLand(Window *w)
{
	HandlePlacePushButton(w, ETTW_LOWER_LAND, ANIMCURSOR_LOWERLAND, VHM_POINT, PlaceProc_LowerBigLand);
}

static void EditorTerraformClick_RaiseBigLand(Window *w)
{
	HandlePlacePushButton(w, ETTW_RAISE_LAND, ANIMCURSOR_RAISELAND, VHM_POINT, PlaceProc_RaiseBigLand);
}

static void EditorTerraformClick_LevelLand(Window *w)
{
	HandlePlacePushButton(w, ETTW_LEVEL_LAND, SPR_CURSOR_LEVEL_LAND, VHM_POINT, PlaceProc_LevelLand);
}

static void EditorTerraformClick_WaterArea(Window *w)
{
	HandlePlacePushButton(w, ETTW_BUILD_CANAL, SPR_CURSOR_CANAL, VHM_RECT, PlaceProc_WaterArea);
}

static void EditorTerraformClick_RiverArea(Window *w)
{
	HandlePlacePushButton(w, ETTW_BUILD_RIVER, SPR_CURSOR_RIVER, VHM_RECT, PlaceProc_RiverArea);
}

static void EditorTerraformClick_RockyArea(Window *w)
{
	HandlePlacePushButton(w, ETTW_PLACE_ROCKS, SPR_CURSOR_ROCKY_AREA, VHM_RECT, PlaceProc_RockyArea);
}

static void EditorTerraformClick_DesertLightHouse(Window *w)
{
	HandlePlacePushButton(w, ETTW_PLACE_DESERT_LIGHTHOUSE, SPR_CURSOR_LIGHTHOUSE, VHM_RECT, (_opt.landscape == LT_TROPIC) ? PlaceProc_DesertArea : PlaceProc_LightHouse);
}

static void EditorTerraformClick_Transmitter(Window *w)
{
	HandlePlacePushButton(w, ETTW_PLACE_TRANSMITTER, SPR_CURSOR_TRANSMITTER, VHM_RECT, PlaceProc_Transmitter);
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

struct ScenarioEditorLandscapeGenerationWindow : Window {
	ScenarioEditorLandscapeGenerationWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->widget[ETTW_PLACE_DESERT_LIGHTHOUSE].tooltips = (_opt.landscape == LT_TROPIC) ? STR_028F_DEFINE_DESERT_AREA : STR_028D_PLACE_LIGHTHOUSE;
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint() {
		this->DrawWidgets();

		int n = _terraform_size * _terraform_size;
		const int8 *coords = &_multi_terraform_coords[0][0];

		assert(n != 0);
		do {
			DrawSprite(SPR_WHITE_POINT, PAL_NONE, 88 + coords[0], 55 + coords[1]);
			coords += 2;
		} while (--n);

		if (this->IsWidgetLowered(ETTW_LOWER_LAND) || this->IsWidgetLowered(ETTW_RAISE_LAND)) { // change area-size if raise/lower corner is selected
			SetTileSelectSize(_terraform_size, _terraform_size);
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		for (uint i = 0; i != lengthof(_editor_terraform_keycodes); i++) {
			if (keycode == _editor_terraform_keycodes[i]) {
				_editor_terraform_button_proc[i](this);
				return ES_HANDLED;
			}
		}
		return ES_NOT_HANDLED;
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (IsInsideMM(widget, ETTW_BUTTONS_START, ETTW_BUTTONS_END)) {
			_editor_terraform_button_proc[widget - ETTW_BUTTONS_START](this);
		} else {
			switch (widget) {
				case ETTW_INCREASE_SIZE:
				case ETTW_DECREASE_SIZE: { // Increase/Decrease terraform size
					int size = (widget == ETTW_INCREASE_SIZE) ? 1 : -1;
					this->HandleButtonClick(widget);
					size += _terraform_size;

					if (!IsInsideMM(size, 1, 8 + 1)) return;
					_terraform_size = size;

					SndPlayFx(SND_15_BEEP);
					this->SetDirty();
				} break;
				case ETTW_NEW_SCENARIO: // gen random land
					this->HandleButtonClick(widget);
					ShowCreateScenario();
					break;
				case ETTW_RESET_LANDSCAPE: // Reset landscape
					ShowQuery(
						STR_022C_RESET_LANDSCAPE,
						STR_RESET_LANDSCAPE_CONFIRMATION_TEXT,
						NULL,
						ResetLandscapeConfirmationCallback);
					break;
			}
		}
	}

	virtual void OnTimeout()
	{
		for (uint i = ETTW_START; i < this->widget_count; i++) {
			if (i == ETTW_BUTTONS_START) i = ETTW_BUTTONS_END; // skip the buttons
			if (this->IsWidgetLowered(i)) {
				this->RaiseWidget(i);
				this->InvalidateWidget(i);
			}
		}
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1) {
			switch (select_proc) {
				default: NOT_REACHED();
				case DDSP_CREATE_ROCKS:
				case DDSP_CREATE_DESERT:
				case DDSP_CREATE_WATER:
				case DDSP_CREATE_RIVER:
				case DDSP_RAISE_AND_LEVEL_AREA:
				case DDSP_LOWER_AND_LEVEL_AREA:
				case DDSP_LEVEL_AREA:
				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
			}
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
		this->SetDirty();
	}
};

static const WindowDesc _scen_edit_land_gen_desc = {
	WDP_AUTO, WDP_AUTO, 204, 103, 204, 103,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_scen_edit_land_gen_widgets,
};

void ShowEditorTerraformToolbar()
{
	AllocateWindowDescFront<ScenarioEditorLandscapeGenerationWindow>(&_scen_edit_land_gen_desc, 0);
}
