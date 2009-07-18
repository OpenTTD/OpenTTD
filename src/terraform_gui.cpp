/* $Id$ */

/** @file terraform_gui.cpp GUI related to terraforming the map. */

#include "stdafx.h"
#include "openttd.h"
#include "clear_map.h"
#include "company_func.h"
#include "company_base.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
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
#include "tree_map.h"
#include "station_map.h"
#include "landscape_type.h"
#include "tilehighlight_func.h"
#include "settings_type.h"
#include "waypoint.h"

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
		SetTropicZone(tile, (_ctrl_pressed) ? TROPICZONE_NORMAL : TROPICZONE_DESERT);
		DoCommandP(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
		MarkTileDirtyByTile(tile);
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
	if (!_settings_game.construction.freeform_edges) {
		/* When end_tile is MP_VOID, the error tile will not be visible to the
		 * user. This happens when terraforming at the southern border. */
		if (TileX(end_tile) == MapMaxX()) end_tile += TileDiffXY(-1, 0);
		if (TileY(end_tile) == MapMaxY()) end_tile += TileDiffXY(0, -1);
	}

	switch (proc) {
		case DDSP_DEMOLISH_AREA:
			DoCommandP(end_tile, start_tile, 0, CMD_CLEAR_AREA | CMD_MSG(STR_ERROR_CAN_T_CLEAR_THIS_AREA), CcPlaySound10);
			break;
		case DDSP_RAISE_AND_LEVEL_AREA:
			DoCommandP(end_tile, start_tile, 1, CMD_LEVEL_LAND | CMD_MSG(STR_ERROR_CAN_T_RAISE_LAND_HERE), CcTerraform);
			break;
		case DDSP_LOWER_AND_LEVEL_AREA:
			DoCommandP(end_tile, start_tile, (uint32)-1, CMD_LEVEL_LAND | CMD_MSG(STR_ERROR_CAN_T_LOWER_LAND_HERE), CcTerraform);
			break;
		case DDSP_LEVEL_AREA:
			DoCommandP(end_tile, start_tile, 0, CMD_LEVEL_LAND | CMD_MSG(STR_ERROR_CAN_T_LEVEL_LAND_HERE), CcTerraform);
			break;
		case DDSP_CREATE_ROCKS:
			GenerateRockyArea(end_tile, start_tile);
			break;
		case DDSP_CREATE_DESERT:
			GenerateDesertArea(end_tile, start_tile);
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
	DoCommandP(tile, 0, 0, CMD_PURCHASE_LAND_AREA | CMD_MSG(STR_ERROR_CAN_T_PURCHASE_THIS_LAND), CcPlaySound1E);
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
	HandlePlacePushButton(w, TTW_LOWER_LAND, ANIMCURSOR_LOWERLAND, HT_POINT, PlaceProc_LowerLand);
}

static void TerraformClick_Raise(Window *w)
{
	HandlePlacePushButton(w, TTW_RAISE_LAND, ANIMCURSOR_RAISELAND, HT_POINT, PlaceProc_RaiseLand);
}

static void TerraformClick_Level(Window *w)
{
	HandlePlacePushButton(w, TTW_LEVEL_LAND, SPR_CURSOR_LEVEL_LAND, HT_POINT, PlaceProc_LevelLand);
}

static void TerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, TTW_DEMOLISH, ANIMCURSOR_DEMOLISH , HT_RECT, PlaceProc_DemolishArea);
}

static void TerraformClick_BuyLand(Window *w)
{
	HandlePlacePushButton(w, TTW_BUY_LAND, SPR_CURSOR_BUY_LAND, HT_RECT, PlaceProc_BuyLand);
}

static void TerraformClick_Trees(Window *w)
{
	/* This button is NOT a place-push-button, so don't treat it as such */
	ShowBuildTreesToolbar();
}

static void TerraformClick_PlaceSign(Window *w)
{
	HandlePlacePushButton(w, TTW_PLACE_SIGN, SPR_CURSOR_SIGN, HT_RECT, PlaceProc_Sign);
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
	TerraformToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
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

static const NWidgetPart _nested_terraform_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, TTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, TTW_CAPTION), SetDataTip(STR_LANDSCAPING_TOOLBAR, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, TTW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_LOWER_LAND), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_TERRAFORM_DOWN, STR_TERRAFORM_TOOLTIP_LOWER_A_CORNER_OF_LAND),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_RAISE_LAND), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_TERRAFORM_UP, STR_TERRAFORM_TOOLTIP_RAISE_A_CORNER_OF_LAND),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_LEVEL_LAND), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_LEVEL_LAND, STR_LEVEL_LAND_TOOLTIP),

		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, TTW_SEPERATOR), SetMinimalSize(4, 22), EndContainer(),

		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_DEMOLISH), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_BUY_LAND), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_BUY_LAND, STR_TERRAFORM_TOOLTIP_PURCHASE_LAND),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_PLANT_TREES), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_PLANTTREES, STR_TOOLBAR_TOOLTIP_PLANT_TREES_PLACE_SIGNS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, TTW_PLACE_SIGN), SetMinimalSize(22,22),
								SetFill(0, 1), SetDataTip(SPR_IMG_SIGN, STR_SCENEDIT_TOOLBAR_PLACE_SIGN),
	EndContainer(),
};

static const WindowDesc _terraform_desc(
	WDP_ALIGN_TBR, 22 + 36, 158, 36, 158, 36,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	NULL, _nested_terraform_widgets, lengthof(_nested_terraform_widgets)
);

Window *ShowTerraformToolbar(Window *link)
{
	if (!Company::IsValidID(_local_company)) return NULL;

	Window *w = AllocateWindowDescFront<TerraformToolbarWindow>(&_terraform_desc, 0);
	if (link == NULL) return w;

	if (w == NULL) {
		w = FindWindowById(WC_SCEN_LAND_GEN, 0);
		if (w == NULL) return NULL;
	} else {
		w->top = 22;
		w->SetDirty();
	}

	/* Align the terraform toolbar under the main toolbar and put the linked
	 * toolbar to left of it
	 */
	link->top  = w->top;
	link->left = w->left - link->width;
	link->SetDirty();

	return w;
}

void ShowTerraformToolbarWithTool(uint16 key, uint16 keycode)
{
	Window *w = FindWindowById(WC_SCEN_LAND_GEN, 0);

	if (w == NULL) w = ShowTerraformToolbar(NULL);
	if (w == NULL) return;

	w->OnKeyPress(key, keycode);
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
			mode ? STR_ERROR_CAN_T_RAISE_LAND_HERE : STR_ERROR_CAN_T_LOWER_LAND_HERE;

		DoCommandP(tile, SLOPE_N, (uint32)mode, CMD_TERRAFORM_LAND | CMD_MSG(msg), CcTerraform);
	} else {
		assert(_terraform_size != 0);
		/* check out for map overflows */
		sizex = min(MapSizeX() - TileX(tile), _terraform_size);
		sizey = min(MapSizeY() - TileY(tile), _terraform_size);

		if (sizex == 0 || sizey == 0) return;

		SndPlayTileFx(SND_1F_SPLAT, tile);

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
				DoCommandP(tile2, SLOPE_N, (uint32)mode, CMD_TERRAFORM_LAND);
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
	ETTW_DOTS,                             ///< Invisible widget for rendering the terraform size on.
	ETTW_BUTTONS_START,                    ///< Start of pushable buttons
	ETTW_DEMOLISH = ETTW_BUTTONS_START,    ///< Demolish aka dynamite button
	ETTW_LOWER_LAND,                       ///< Lower land button
	ETTW_RAISE_LAND,                       ///< Raise land button
	ETTW_LEVEL_LAND,                       ///< Level land button
	ETTW_PLACE_ROCKS,                      ///< Place rocks button
	ETTW_PLACE_DESERT_LIGHTHOUSE,          ///< Place desert button (in tropical climate) / place lighthouse button (else)
	ETTW_PLACE_TRANSMITTER,                ///< Place transmitter button
	ETTW_BUTTONS_END,                      ///< End of pushable buttons
	ETTW_INCREASE_SIZE = ETTW_BUTTONS_END, ///< Upwards arrow button to increase terraforming size
	ETTW_DECREASE_SIZE,                    ///< Downwards arrow button to decrease terraforming size
	ETTW_NEW_SCENARIO,                     ///< Button for generating a new scenario
	ETTW_RESET_LANDSCAPE,                  ///< Button for removing all company-owned property
};

static const NWidgetPart _nested_scen_edit_land_gen_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, ETTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, ETTW_CAPTION), SetDataTip(STR_TERRAFORM_TOOLBAR_LAND_GENERATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, ETTW_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, ETTW_BACKGROUND),
		NWidget(NWID_HORIZONTAL), SetPadding(2, 2, 7, 2),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_DEMOLISH), SetMinimalSize(22, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_LOWER_LAND), SetMinimalSize(22, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_TERRAFORM_DOWN, STR_TERRAFORM_TOOLTIP_LOWER_A_CORNER_OF_LAND),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_RAISE_LAND), SetMinimalSize(22, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_TERRAFORM_UP, STR_TERRAFORM_TOOLTIP_RAISE_A_CORNER_OF_LAND),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_LEVEL_LAND), SetMinimalSize(22, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_LEVEL_LAND, STR_LEVEL_LAND_TOOLTIP),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_PLACE_ROCKS), SetMinimalSize(22, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_ROCKS, STR_TERRAFORM_TOOLTIP_PLACE_ROCKY_AREAS_ON_LANDSCAPE),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_PLACE_DESERT_LIGHTHOUSE), SetMinimalSize(22, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_LIGHTHOUSE_DESERT, STR_NULL),
			NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_PLACE_TRANSMITTER), SetMinimalSize(23, 22),
										SetFill(0, 1), SetDataTip(SPR_IMG_TRANSMITTER, STR_TERRAFORM_TOOLTIP_PLACE_TRANSMITTER),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, ETTW_DOTS), SetMinimalSize(59, 31), SetDataTip(STR_EMPTY, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1),
				NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_INCREASE_SIZE), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_UP, STR_TERRAFORM_TOOLTIP_INCREASE_SIZE_OF_LAND_AREA),
				NWidget(NWID_SPACER), SetMinimalSize(0, 1),
				NWidget(WWT_IMGBTN, COLOUR_GREY, ETTW_DECREASE_SIZE), SetMinimalSize(12, 12), SetDataTip(SPR_ARROW_DOWN, STR_TERRAFORM_TOOLTIP_DECREASE_SIZE_OF_LAND_AREA),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 6),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, ETTW_NEW_SCENARIO), SetMinimalSize(160, 12),
								SetFill(1, 0), SetDataTip(STR_SE_NEW_WORLD, STR_TERRAFORM_TOOLTIP_GENERATE_RANDOM_LAND), SetPadding(0, 2, 0, 2),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, ETTW_RESET_LANDSCAPE), SetMinimalSize(160, 12),
								SetFill(1, 0), SetDataTip(STR_TERRAFORM_RESET_LANDSCAPE, STR_RESET_LANDSCAPE_TOOLTIP), SetPadding(1, 2, 2, 2),
	EndContainer(),
};

/**
 * @todo Merge with terraform_gui.cpp (move there) after I have cooled down at its braindeadness
 * and changed OnButtonClick to include the widget as well in the function declaration. Post 0.4.0 - Darkvater
 */
static void EditorTerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, ETTW_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT, PlaceProc_DemolishArea);
}

static void EditorTerraformClick_LowerBigLand(Window *w)
{
	HandlePlacePushButton(w, ETTW_LOWER_LAND, ANIMCURSOR_LOWERLAND, HT_POINT, PlaceProc_LowerBigLand);
}

static void EditorTerraformClick_RaiseBigLand(Window *w)
{
	HandlePlacePushButton(w, ETTW_RAISE_LAND, ANIMCURSOR_RAISELAND, HT_POINT, PlaceProc_RaiseBigLand);
}

static void EditorTerraformClick_LevelLand(Window *w)
{
	HandlePlacePushButton(w, ETTW_LEVEL_LAND, SPR_CURSOR_LEVEL_LAND, HT_POINT, PlaceProc_LevelLand);
}

static void EditorTerraformClick_RockyArea(Window *w)
{
	HandlePlacePushButton(w, ETTW_PLACE_ROCKS, SPR_CURSOR_ROCKY_AREA, HT_RECT, PlaceProc_RockyArea);
}

static void EditorTerraformClick_DesertLightHouse(Window *w)
{
	HandlePlacePushButton(w, ETTW_PLACE_DESERT_LIGHTHOUSE, SPR_CURSOR_LIGHTHOUSE, HT_RECT, (_settings_game.game_creation.landscape == LT_TROPIC) ? PlaceProc_DesertArea : PlaceProc_LightHouse);
}

static void EditorTerraformClick_Transmitter(Window *w)
{
	HandlePlacePushButton(w, ETTW_PLACE_TRANSMITTER, SPR_CURSOR_TRANSMITTER, HT_RECT, PlaceProc_Transmitter);
}

static const uint16 _editor_terraform_keycodes[] = {
	'D',
	'Q',
	'W',
	'E',
	'R',
	'T',
	'Y'
};

typedef void OnButtonClick(Window *w);
static OnButtonClick * const _editor_terraform_button_proc[] = {
	EditorTerraformClick_Dynamite,
	EditorTerraformClick_LowerBigLand,
	EditorTerraformClick_RaiseBigLand,
	EditorTerraformClick_LevelLand,
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
		/* Set generating_world to true to get instant-green grass after removing
		 * company property. */
		_generating_world = true;

		/* Delete all companies */
		Company *c;
		FOR_ALL_COMPANIES(c) {
			ChangeOwnershipOfCompanyItems(c->index, INVALID_OWNER);
			delete c;
		}

		_generating_world = false;

		/* Delete all station signs */
		Station *st;
		FOR_ALL_STATIONS(st) {
			/* There can be buoys, remove them */
			if (st->IsBuoy() && IsBuoyTile(st->xy)) DoCommand(st->xy, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);
			if (st->facilities == 0) delete st;
		}

		/* The same for waypoints */
		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			delete wp;
		}

		MarkWholeScreenDirty();
	}
}

struct ScenarioEditorLandscapeGenerationWindow : Window {
	ScenarioEditorLandscapeGenerationWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		this->nested_array[ETTW_PLACE_DESERT_LIGHTHOUSE]->tool_tip = (_settings_game.game_creation.landscape == LT_TROPIC) ? STR_TERRAFORM_TOOLTIP_DEFINE_DESERT_AREA : STR_TERRAFORM_TOOLTIP_PLACE_LIGHTHOUSE;
	}

	virtual void OnPaint() {
		this->DrawWidgets();

		if (this->IsWidgetLowered(ETTW_LOWER_LAND) || this->IsWidgetLowered(ETTW_RAISE_LAND)) { // change area-size if raise/lower corner is selected
			SetTileSelectSize(_terraform_size, _terraform_size);
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != ETTW_DOTS) return;

		int center_x = (r.left + r.right + 1) / 2;
		int center_y = (r.top + r.bottom + 1) / 2;

		int n = _terraform_size * _terraform_size;
		const int8 *coords = &_multi_terraform_coords[0][0];

		assert(n != 0);
		do {
			DrawSprite(SPR_WHITE_POINT, PAL_NONE, center_x + coords[0], center_y + coords[1]);
			coords += 2;
		} while (--n);
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
						STR_QUERY_RESET_LANDSCAPE_CAPTION,
						STR_RESET_LANDSCAPE_CONFIRMATION_TEXT,
						NULL,
						ResetLandscapeConfirmationCallback);
					break;
			}
		}
	}

	virtual void OnTimeout()
	{
		for (uint i = ETTW_START; i < this->nested_array_size; i++) {
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

static const WindowDesc _scen_edit_land_gen_desc(
	WDP_AUTO, WDP_AUTO, 163, 103, 163, 103,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	NULL, _nested_scen_edit_land_gen_widgets, lengthof(_nested_scen_edit_land_gen_widgets)
);

Window *ShowEditorTerraformToolbar()
{
	return AllocateWindowDescFront<ScenarioEditorLandscapeGenerationWindow>(&_scen_edit_land_gen_desc, 0);
}

void ShowEditorTerraformToolbarWithTool(uint16 key, uint16 keycode)
{
	Window *w = FindWindowById(WC_SCEN_LAND_GEN, 0);

	if (w == NULL) w = ShowEditorTerraformToolbar();
	if (w == NULL) return;

	w->OnKeyPress(key, keycode);
}
