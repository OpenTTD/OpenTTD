/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "road_cmd.h"
#include "road_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "tile.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "sound.h"
#include "command.h"
#include "variables.h"
//needed for catchments
#include "station.h"


static void ShowBusStationPicker(void);
static void ShowTruckStationPicker(void);
static void ShowRoadDepotPicker(void);

static bool _remove_button_clicked;

static byte _place_road_flag;

static DiagDirection _road_depot_orientation;
static DiagDirection _road_station_picker_orientation;

void CcPlaySound1D(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_1F_SPLAT, tile);
}

static void PlaceRoad_NE(TileIndex tile)
{
	_place_road_flag = (_tile_fract_coords.y >= 8) + 4;
	VpStartPlaceSizing(tile, VPM_FIX_X);
}

static void PlaceRoad_NW(TileIndex tile)
{
	_place_road_flag = (_tile_fract_coords.x >= 8) + 0;
	VpStartPlaceSizing(tile, VPM_FIX_Y);
}

static void PlaceRoad_Bridge(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y);
}


void CcBuildRoadTunnel(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_20_SPLAT_2, tile);
		ResetObjectToPlace();
	} else {
		SetRedErrorSquare(_build_tunnel_endtile);
	}
}

static void PlaceRoad_Tunnel(TileIndex tile)
{
	DoCommandP(tile, 0x200, 0, CcBuildRoadTunnel, CMD_BUILD_TUNNEL | CMD_AUTO | CMD_MSG(STR_5016_CAN_T_BUILD_TUNNEL_HERE));
}

static void BuildRoadOutsideStation(TileIndex tile, DiagDirection direction)
{
	tile += TileOffsByDiagDir(direction);
	// if there is a roadpiece just outside of the station entrance, build a connecting route
	if (IsTileType(tile, MP_STREET) && GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
		DoCommandP(tile, DiagDirToRoadBits(ReverseDiagDir(direction)), 0, NULL, CMD_BUILD_ROAD);
	}
}

void CcRoadDepot(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
		ResetObjectToPlace();
		BuildRoadOutsideStation(tile, (DiagDirection)p1);
	}
}

static void PlaceRoad_Depot(TileIndex tile)
{
	DoCommandP(tile, _road_depot_orientation, 0, CcRoadDepot, CMD_BUILD_ROAD_DEPOT | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1807_CAN_T_BUILD_ROAD_VEHICLE));
}

static void PlaceRoad_BusStation(TileIndex tile)
{
	DoCommandP(tile, _road_station_picker_orientation, RS_BUS, CcRoadDepot, CMD_BUILD_ROAD_STOP | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1808_CAN_T_BUILD_BUS_STATION));
}

static void PlaceRoad_TruckStation(TileIndex tile)
{
	DoCommandP(tile, _road_station_picker_orientation, RS_TRUCK, CcRoadDepot, CMD_BUILD_ROAD_STOP | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1809_CAN_T_BUILD_TRUCK_STATION));
}

static void PlaceRoad_DemolishArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, 4);
}


enum {
	RTW_ROAD_X        =  3,
	RTW_ROAD_Y        =  4,
	RTW_DEMOLISH      =  5,
	RTW_DEPOT         =  6,
	RTW_BUS_STATION   =  7,
	RTW_TRUCK_STATION =  8,
	RTW_BUILD_BRIDGE  =  9,
	RTW_BUILD_TUNNEL  = 10,
	RTW_REMOVE        = 11
};


typedef void OnButtonClick(Window *w);

static void BuildRoadClick_NE(Window *w)
{
	HandlePlacePushButton(w, RTW_ROAD_X, SPR_CURSOR_ROAD_NESW, 1, PlaceRoad_NE);
}

static void BuildRoadClick_NW(Window *w)
{
	HandlePlacePushButton(w, RTW_ROAD_Y, SPR_CURSOR_ROAD_NWSE, 1, PlaceRoad_NW);
}


static void BuildRoadClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, RTW_DEMOLISH, ANIMCURSOR_DEMOLISH, 1, PlaceRoad_DemolishArea);
}

static void BuildRoadClick_Depot(Window *w)
{
	if (_game_mode == GM_EDITOR) return;
	if (HandlePlacePushButton(w, RTW_DEPOT, SPR_CURSOR_ROAD_DEPOT, 1, PlaceRoad_Depot)) ShowRoadDepotPicker();
}

static void BuildRoadClick_BusStation(Window *w)
{
	if (_game_mode == GM_EDITOR) return;
	if (HandlePlacePushButton(w, RTW_BUS_STATION, SPR_CURSOR_BUS_STATION, 1, PlaceRoad_BusStation)) ShowBusStationPicker();
}

static void BuildRoadClick_TruckStation(Window *w)
{
	if (_game_mode == GM_EDITOR) return;
	if (HandlePlacePushButton(w, RTW_TRUCK_STATION, SPR_CURSOR_TRUCK_STATION, 1, PlaceRoad_TruckStation)) ShowTruckStationPicker();
}

static void BuildRoadClick_Bridge(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, 1, PlaceRoad_Bridge);
}

static void BuildRoadClick_Tunnel(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_TUNNEL, SPR_CURSOR_ROAD_TUNNEL, 3, PlaceRoad_Tunnel);
}

static void BuildRoadClick_Remove(Window *w)
{
	if (IsWindowWidgetDisabled(w, RTW_REMOVE)) return;
	SetWindowDirty(w);
	SndPlayFx(SND_15_BEEP);
	ToggleWidgetLoweredState(w, RTW_REMOVE);
	SetSelectionRed(IsWindowWidgetLowered(w, RTW_REMOVE));
}


static OnButtonClick* const _build_road_button_proc[] = {
	BuildRoadClick_NE,
	BuildRoadClick_NW,
	BuildRoadClick_Demolish,
	BuildRoadClick_Depot,
	BuildRoadClick_BusStation,
	BuildRoadClick_TruckStation,
	BuildRoadClick_Bridge,
	BuildRoadClick_Tunnel,
	BuildRoadClick_Remove
};

static void BuildRoadToolbWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: DisableWindowWidget(w, RTW_REMOVE); break;

	case WE_PAINT:
		if (IsWindowWidgetLowered(w, RTW_ROAD_X) || IsWindowWidgetLowered(w, RTW_ROAD_Y)) {
			EnableWindowWidget(w, RTW_REMOVE);
		}
		DrawWindowWidgets(w);
		break;

	case WE_CLICK: {
		if (e->we.click.widget >= 3) _build_road_button_proc[e->we.click.widget - 3](w);
	}	break;

	case WE_KEYPRESS:
		switch (e->we.keypress.keycode) {
			case '1': BuildRoadClick_NE(w);           break;
			case '2': BuildRoadClick_NW(w);           break;
			case '3': BuildRoadClick_Demolish(w);     break;
			case '4': BuildRoadClick_Depot(w);        break;
			case '5': BuildRoadClick_BusStation(w);   break;
			case '6': BuildRoadClick_TruckStation(w); break;
			case 'B': BuildRoadClick_Bridge(w);       break;
			case 'T': BuildRoadClick_Tunnel(w);       break;
			case 'R': BuildRoadClick_Remove(w);       break;
			default: return;
		}
		MarkTileDirty(_thd.pos.x, _thd.pos.y); // redraw tile selection
		e->we.keypress.cont = false;
		break;

	case WE_PLACE_OBJ:
		_remove_button_clicked = IsWindowWidgetLowered(w, RTW_REMOVE);
		_place_proc(e->we.place.tile);
		break;

	case WE_ABORT_PLACE_OBJ:
		RaiseWindowButtons(w);
		DisableWindowWidget(w, RTW_REMOVE);
		InvalidateWidget(w, RTW_REMOVE);

		w = FindWindowById(WC_BUS_STATION, 0);
		if (w != NULL) WP(w,def_d).close = true;
		w = FindWindowById(WC_TRUCK_STATION, 0);
		if (w != NULL) WP(w,def_d).close = true;
		w = FindWindowById(WC_BUILD_DEPOT, 0);
		if (w != NULL) WP(w,def_d).close = true;
		break;

	case WE_PLACE_DRAG: {
		int sel_method;
		switch (e->we.place.userdata) {
			case 1:
				sel_method = VPM_FIX_X;
				_place_road_flag = (_place_road_flag & ~2) | ((e->we.place.pt.y & 8) >> 2);
				break;

			case 2:
				sel_method = VPM_FIX_Y;
				_place_road_flag = (_place_road_flag & ~2) | ((e->we.place.pt.x & 8) >> 2);
				break;

			case 4:
				sel_method = VPM_X_AND_Y;
				break;

			default:
				sel_method = VPM_X_OR_Y;
				break;
		}

		VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, sel_method);
		return;
	}

	case WE_PLACE_MOUSEUP:
		if (e->we.place.pt.x != -1) {
			TileIndex start_tile = e->we.place.starttile;
			TileIndex end_tile = e->we.place.tile;

			if (e->we.place.userdata == 0) {
				ResetObjectToPlace();
				ShowBuildBridgeWindow(start_tile, end_tile, 0x80);
			} else if (e->we.place.userdata != 4) {
				DoCommandP(end_tile, start_tile, _place_road_flag, CcPlaySound1D,
					_remove_button_clicked ?
					CMD_REMOVE_LONG_ROAD | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1805_CAN_T_REMOVE_ROAD_FROM) :
					CMD_BUILD_LONG_ROAD | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1804_CAN_T_BUILD_ROAD_HERE));
			} else {
				DoCommandP(end_tile, start_tile, 0, CcPlaySound10, CMD_CLEAR_AREA | CMD_MSG(STR_00B5_CAN_T_CLEAR_THIS_AREA));
			}
		}
		break;

	case WE_PLACE_PRESIZE: {
		TileIndex tile = e->we.place.tile;

		DoCommand(tile, 0x200, 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
		break;
	}

	case WE_DESTROY:
		if (_patches.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0);
		break;
	}
}

static const Widget _build_road_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   205,     0,    13, STR_1802_ROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   206,   217,     0,    13, 0x0,                        STR_STICKY_BUTTON},

{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    21,    14,    35, SPR_IMG_ROAD_NW,            STR_180B_BUILD_ROAD_SECTION},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_ROAD_NE,            STR_180B_BUILD_ROAD_SECTION},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_DYNAMITE,           STR_018D_DEMOLISH_BUILDINGS_ETC},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,    87,    14,    35, SPR_IMG_ROAD_DEPOT,         STR_180C_BUILD_ROAD_VEHICLE_DEPOT},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_IMG_BUS_STATION,        STR_180D_BUILD_BUS_STATION},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   110,   131,    14,    35, SPR_IMG_TRUCK_BAY,          STR_180E_BUILD_TRUCK_LOADING_BAY},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   132,   173,    14,    35, SPR_IMG_BRIDGE,             STR_180F_BUILD_ROAD_BRIDGE},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   174,   195,    14,    35, SPR_IMG_ROAD_TUNNEL,        STR_1810_BUILD_ROAD_TUNNEL},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   196,   217,    14,    35, SPR_IMG_REMOVE,             STR_1811_TOGGLE_BUILD_REMOVE_FOR},
{   WIDGETS_END},
};

static const WindowDesc _build_road_desc = {
	WDP_ALIGN_TBR, 22, 218, 36,
	WC_BUILD_TOOLBAR, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_road_widgets,
	BuildRoadToolbWndProc
};

void ShowBuildRoadToolbar(void)
{
	if (!IsValidPlayer(_current_player)) return;

	DeleteWindowById(WC_BUILD_TOOLBAR, 0);
	AllocateWindowDesc(&_build_road_desc);
	if (_patches.link_terraform_toolbar) ShowTerraformToolbar();
}

static const Widget _build_road_scen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   140,     0,    13, STR_1802_ROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   141,   152,     0,    13, 0x0,                        STR_STICKY_BUTTON},

{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    21,    14,    35, SPR_IMG_ROAD_NW,            STR_180B_BUILD_ROAD_SECTION},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_ROAD_NE,            STR_180B_BUILD_ROAD_SECTION},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_DYNAMITE,           STR_018D_DEMOLISH_BUILDINGS_ETC},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                        STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                        STR_NULL},
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                        STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,   107,    14,    35, SPR_IMG_BRIDGE,             STR_180F_BUILD_ROAD_BRIDGE},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   108,   129,    14,    35, SPR_IMG_ROAD_TUNNEL,        STR_1810_BUILD_ROAD_TUNNEL},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   130,   151,    14,    35, SPR_IMG_REMOVE,             STR_1811_TOGGLE_BUILD_REMOVE_FOR},
{   WIDGETS_END},
};

static const WindowDesc _build_road_scen_desc = {
	WDP_AUTO, WDP_AUTO, 152, 36,
	WC_SCEN_BUILD_ROAD, 0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_road_scen_widgets,
	BuildRoadToolbWndProc
};

void ShowBuildRoadScenToolbar(void)
{
	AllocateWindowDescFront(&_build_road_scen_desc, 0);
}

static void BuildRoadDepotWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: LowerWindowWidget(w, _road_depot_orientation + 3); break;

	case WE_PAINT:
		DrawWindowWidgets(w);

		DrawRoadDepotSprite(70, 17, DIAGDIR_NE);
		DrawRoadDepotSprite(70, 69, DIAGDIR_SE);
		DrawRoadDepotSprite( 2, 69, DIAGDIR_SW);
		DrawRoadDepotSprite( 2, 17, DIAGDIR_NW);
		break;

	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 3: case 4: case 5: case 6:
			RaiseWindowWidget(w, _road_depot_orientation + 3);
			_road_depot_orientation = (DiagDirection)(e->we.click.widget - 3);
			LowerWindowWidget(w, _road_depot_orientation + 3);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}
	}	break;

	case WE_MOUSELOOP:
		if (WP(w,def_d).close) DeleteWindow(w);
		break;

	case WE_DESTROY:
		if (!WP(w,def_d).close) ResetObjectToPlace();
		break;
	}
}

static const Widget _build_road_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_1806_ROAD_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   121, 0x0,                             STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{   WIDGETS_END},
};

static const WindowDesc _build_road_depot_desc = {
	WDP_AUTO, WDP_AUTO, 140, 122,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_depot_widgets,
	BuildRoadDepotWndProc
};

static void ShowRoadDepotPicker(void)
{
	AllocateWindowDesc(&_build_road_depot_desc);
}

static void RoadStationPickerWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE:
		LowerWindowWidget(w, _road_station_picker_orientation + 3);
		LowerWindowWidget(w, _station_show_coverage + 7);
		break;

	case WE_PAINT: {
		int image;

		if (WP(w,def_d).close) return;

		DrawWindowWidgets(w);

		if (_station_show_coverage) {
			int rad = _patches.modified_catchment ? CA_TRUCK /* = CA_BUS */ : 4;
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		image = (w->window_class == WC_BUS_STATION) ? 0x47 : 0x43;

		StationPickerDrawSprite(103, 35, RAILTYPE_BEGIN, image);
		StationPickerDrawSprite(103, 85, RAILTYPE_BEGIN, image+1);
		StationPickerDrawSprite(35, 85, RAILTYPE_BEGIN, image+2);
		StationPickerDrawSprite(35, 35, RAILTYPE_BEGIN, image+3);

		DrawStationCoverageAreaText(2, 146,
			((w->window_class == WC_BUS_STATION) ? (1<<CT_PASSENGERS) : ~(1<<CT_PASSENGERS)),
			3);

	} break;

	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 3: case 4: case 5: case 6:
			RaiseWindowWidget(w, _road_station_picker_orientation + 3);
			_road_station_picker_orientation = (DiagDirection)(e->we.click.widget - 3);
			LowerWindowWidget(w, _road_station_picker_orientation + 3);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		case 7: case 8:
			RaiseWindowWidget(w, _station_show_coverage + 7);
			_station_show_coverage = (e->we.click.widget != 7);
			LowerWindowWidget(w, _station_show_coverage + 7);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}
	} break;

	case WE_MOUSELOOP: {
		if (WP(w,def_d).close) {
			DeleteWindow(w);
			return;
		}

		CheckRedrawStationCoverage(w);
	} break;

	case WE_DESTROY:
		if (!WP(w,def_d).close) ResetObjectToPlace();
		break;
	}
}

static const Widget _bus_station_picker_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_3042_BUS_STATION_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   176, 0x0,                              STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                              STR_3051_SELECT_BUS_STATION_ORIENTATION},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                              STR_3051_SELECT_BUS_STATION_ORIENTATION},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                              STR_3051_SELECT_BUS_STATION_ORIENTATION},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                              STR_3051_SELECT_BUS_STATION_ORIENTATION},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    10,    69,   133,   144, STR_02DB_OFF,                     STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    70,   129,   133,   144, STR_02DA_ON,                      STR_3064_HIGHLIGHT_COVERAGE_AREA},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   139,   120,   133, STR_3066_COVERAGE_AREA_HIGHLIGHT, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _bus_station_picker_desc = {
	WDP_AUTO, WDP_AUTO, 140, 177,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_bus_station_picker_widgets,
	RoadStationPickerWndProc
};

static void ShowBusStationPicker(void)
{
	AllocateWindowDesc(&_bus_station_picker_desc);
}

static const Widget _truck_station_picker_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_3043_TRUCK_STATION_ORIENT,    STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   176, 0x0,                              STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                              STR_3052_SELECT_TRUCK_LOADING_BAY},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                              STR_3052_SELECT_TRUCK_LOADING_BAY},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                              STR_3052_SELECT_TRUCK_LOADING_BAY},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                              STR_3052_SELECT_TRUCK_LOADING_BAY},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    10,    69,   133,   144, STR_02DB_OFF,                     STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    70,   129,   133,   144, STR_02DA_ON,                      STR_3064_HIGHLIGHT_COVERAGE_AREA},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   139,   120,   133, STR_3066_COVERAGE_AREA_HIGHLIGHT, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _truck_station_picker_desc = {
	WDP_AUTO, WDP_AUTO, 140, 177,
	WC_TRUCK_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_truck_station_picker_widgets,
	RoadStationPickerWndProc
};

static void ShowTruckStationPicker(void)
{
	AllocateWindowDesc(&_truck_station_picker_desc);
}

void InitializeRoadGui(void)
{
	_road_depot_orientation = DIAGDIR_NW;
	_road_station_picker_orientation = DIAGDIR_NW;
}
