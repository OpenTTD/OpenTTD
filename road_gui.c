#include "stdafx.h"
#include "ttd.h"

#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "sound.h"
#include "command.h"

static void ShowBusStationPicker();
static void ShowTruckStationPicker();
static void ShowRoadDepotPicker();

static bool _remove_button_clicked;
static bool _build_road_flag;

static byte _place_road_flag;

static byte _road_depot_orientation;
static byte _road_station_picker_orientation;

static void CcPlaySound1D(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) { SndPlayTileFx(0x1D, tile); }
}

static void PlaceRoad_NE(uint tile)
{
	_place_road_flag = (_tile_fract_coords.y >= 8) + 4;
	VpStartPlaceSizing(tile, VPM_FIX_X);
}

static void PlaceRoad_NW(uint tile)
{
	_place_road_flag = (_tile_fract_coords.x >= 8) + 0;
	VpStartPlaceSizing(tile, VPM_FIX_Y);
}

static void PlaceRoad_Bridge(uint tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y);
}


static void CcBuildTunnel(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(0x1E, tile);
		ResetObjectToPlace();
	} else {
		SetRedErrorSquare(_build_tunnel_endtile);
	}
}

static void PlaceRoad_Tunnel(uint tile)
{
	DoCommandP(tile, 0x200, 0, CcBuildTunnel, CMD_BUILD_TUNNEL | CMD_AUTO | CMD_MSG(STR_5016_CAN_T_BUILD_TUNNEL_HERE));
}

static void BuildRoadOutsideStation(uint tile, int direction)
{
	static const byte _roadbits_by_dir[4] = {2,1,8,4};
	tile += _tileoffs_by_dir[direction];
	// if there is a roadpiece just outside of the station entrance, build a connecting route
	if (IS_TILETYPE(tile, MP_STREET) && !(_map5[tile]&0x20)) {
		DoCommandP(tile, _roadbits_by_dir[direction], 0, NULL, CMD_BUILD_ROAD);
	}
}

static void CcDepot(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(0x1D, tile);
		ResetObjectToPlace();
		BuildRoadOutsideStation(tile, (int)p1);
	}
}

static void PlaceRoad_Depot(uint tile)
{
	DoCommandP(tile, _road_depot_orientation, 0, CcDepot, CMD_BUILD_ROAD_DEPOT | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1807_CAN_T_BUILD_ROAD_VEHICLE));
}

static void PlaceRoad_BusStation(uint tile)
{
	DoCommandP(tile, _road_station_picker_orientation, 0, CcDepot, CMD_BUILD_BUS_STATION | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1808_CAN_T_BUILD_BUS_STATION));
}

static void PlaceRoad_TruckStation(uint tile)
{
	DoCommandP(tile, _road_station_picker_orientation, 0, CcDepot, CMD_BUILD_TRUCK_STATION | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1809_CAN_T_BUILD_TRUCK_STATION));
}

static void PlaceRoad_DemolishArea(uint tile)
{
	VpStartPlaceSizing(tile, 4);
}

typedef void OnButtonClick(Window *w);

static void BuildRoadClick_NE(Window *w)
{
	_build_road_flag = 0;
	HandlePlacePushButton(w, 2, 0x51F, 1, PlaceRoad_NE);
}

static void BuildRoadClick_NW(Window *w)
{
	_build_road_flag = 0;
	HandlePlacePushButton(w, 3, 0x520, 1, PlaceRoad_NW);
}


static void BuildRoadClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, 4, ANIMCURSOR_DEMOLISH, 1, PlaceRoad_DemolishArea);
}

static void BuildRoadClick_Lower(Window *w)
{
	HandlePlacePushButton(w, 5, ANIMCURSOR_LOWERLAND, 2, PlaceProc_LowerLand);
}

static void BuildRoadClick_Raise(Window *w)
{
	HandlePlacePushButton(w, 6, ANIMCURSOR_RAISELAND, 2, PlaceProc_RaiseLand);
}

static void BuildRoadClick_Depot(Window *w)
{
	if (HandlePlacePushButton(w, 7, 0x511, 1, PlaceRoad_Depot)) ShowRoadDepotPicker();
}

static void BuildRoadClick_BusStation(Window *w)
{
	if (HandlePlacePushButton(w, 8, 0xAA5, 1, PlaceRoad_BusStation)) ShowBusStationPicker();
}

static void BuildRoadClick_TruckStation(Window *w)
{
	if (HandlePlacePushButton(w, 9, 0xAA6, 1, PlaceRoad_TruckStation)) ShowTruckStationPicker();
}

static void BuildRoadClick_Bridge(Window *w)
{
	_build_road_flag = 0;
	HandlePlacePushButton(w, 10, 0xA21, 1, PlaceRoad_Bridge);
}

static void BuildRoadClick_Tunnel(Window *w)
{
	_build_road_flag = 0;
	HandlePlacePushButton(w, 11, 0x981, 3, PlaceRoad_Tunnel);
}

static void BuildRoadClick_Remove(Window *w)
{
	if (w->disabled_state & (1<<12))
		return;
	SetWindowDirty(w);
	SndPlayFx(0x13);
	_thd.make_square_red = !!((w->click_state ^= (1 << 12)) & (1<<12));
}

static void BuildRoadClick_Purchase(Window *w)
{
	HandlePlacePushButton(w, 13, 0x12B8, 1, PlaceProc_BuyLand);
}

static OnButtonClick * const _build_road_button_proc[] = {
	BuildRoadClick_NE,
	BuildRoadClick_NW,
	BuildRoadClick_Demolish,
	BuildRoadClick_Lower,
	BuildRoadClick_Raise,
	BuildRoadClick_Depot,
	BuildRoadClick_BusStation,
	BuildRoadClick_TruckStation,
	BuildRoadClick_Bridge,
	BuildRoadClick_Tunnel,
	BuildRoadClick_Remove,
	BuildRoadClick_Purchase,
};

static void BuildRoadToolbWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		w->disabled_state &= ~(1 << 12);
		if (!(w->click_state & 12)) {
			w->disabled_state |= (1 << 12);
			w->click_state &= ~(1<<12);
		}
		DrawWindowWidgets(w);
		break;

	case WE_CLICK: {
		if (e->click.widget >= 2)
			_build_road_button_proc[e->click.widget - 2](w);
	}	break;

	case WE_KEYPRESS:
		switch(e->keypress.keycode) {
		case '1': BuildRoadClick_NE(w); break;
		case '2': BuildRoadClick_NW(w); break;
		case '3': BuildRoadClick_Demolish(w); break;
		case '4': BuildRoadClick_Lower(w); break;
		case '5': BuildRoadClick_Raise(w); break;
		case 'B': BuildRoadClick_Bridge(w); break;
		case 'T': BuildRoadClick_Tunnel(w); break;
		case 'R': BuildRoadClick_Remove(w); break;
		default:
			return;
		}
		e->keypress.cont = false;
		break;

	case WE_PLACE_OBJ:
		_remove_button_clicked = (w->click_state & (1 << 12)) != 0;
		_place_proc(e->place.tile);
		break;

	case WE_ABORT_PLACE_OBJ:
		w->click_state = 0;
		SetWindowDirty(w);

		w = FindWindowById(WC_BUS_STATION, 0);
		if (w != NULL) WP(w,def_d).close=true;
		w = FindWindowById(WC_TRUCK_STATION, 0);
		if (w != NULL) WP(w,def_d).close=true;
		w = FindWindowById(WC_BUILD_DEPOT, 0);
		if (w != NULL) WP(w,def_d).close=true;
		break;

	case WE_PLACE_DRAG: {
		int sel_method;
		if (e->place.userdata == 1) {
			sel_method = VPM_FIX_X;
			_place_road_flag = (_place_road_flag&~2) | ((e->place.pt.y&8)>>2);
		} else if (e->place.userdata == 2) {
			sel_method = VPM_FIX_Y;
			_place_road_flag = (_place_road_flag&~2) | ((e->place.pt.x&8)>>2);
		} else if (e->place.userdata == 4) {
			sel_method = VPM_X_AND_Y;
		} else {
			sel_method = VPM_X_OR_Y;
		}

		VpSelectTilesWithMethod(e->place.pt.x, e->place.pt.y, sel_method);
		return;
	}

	case WE_PLACE_MOUSEUP:
		if (e->place.pt.x != -1) {
			uint start_tile = e->place.starttile;
			uint end_tile = e->place.tile;
			if (e->place.userdata == 0) {
				ResetObjectToPlace();
				ShowBuildBridgeWindow(start_tile, end_tile, 0x80);
			} else if (e->place.userdata != 4) {
				DoCommandP(end_tile, start_tile, _place_road_flag, CcPlaySound1D,
					_remove_button_clicked ?
					CMD_REMOVE_LONG_ROAD | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1805_CAN_T_REMOVE_ROAD_FROM) :
					CMD_BUILD_LONG_ROAD | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1804_CAN_T_BUILD_ROAD_HERE));
			} else {
				DoCommandP(end_tile, start_tile, _place_road_flag, CcPlaySound10, CMD_CLEAR_AREA | CMD_MSG(STR_00B5_CAN_T_CLEAR_THIS_AREA));
			}
		}
		break;

	case WE_PLACE_PRESIZE: {
		uint tile = e->place.tile;
		DoCommandByTile(tile, 0x200, 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile==0?tile:_build_tunnel_endtile);
		break;
	}
	}
}

static const Widget _build_road_widgets[] = {
{   WWT_CLOSEBOX,     7,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,     7,    11,   283,     0,    13, STR_1802_ROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     7,     0,    21,    14,    35, 0x51D, STR_180B_BUILD_ROAD_SECTION},
{      WWT_PANEL,     7,    22,    43,    14,    35, 0x51E, STR_180B_BUILD_ROAD_SECTION},
{      WWT_PANEL,     7,    44,    65,    14,    35, 0x2BF, STR_018D_DEMOLISH_BUILDINGS_ETC},
{      WWT_PANEL,     7,    66,    87,    14,    35, 0x2B7, STR_018E_LOWER_A_CORNER_OF_LAND},
{      WWT_PANEL,     7,    88,   109,    14,    35, 0x2B6, STR_018F_RAISE_A_CORNER_OF_LAND},
{      WWT_PANEL,     7,   110,   131,    14,    35, 0x50F, STR_180C_BUILD_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,     7,   132,   153,    14,    35, 0x2ED, STR_180D_BUILD_BUS_STATION},
{      WWT_PANEL,     7,   154,   175,    14,    35, 0x2EE, STR_180E_BUILD_TRUCK_LOADING_BAY},
{      WWT_PANEL,     7,   176,   217,    14,    35, 0xA22, STR_180F_BUILD_ROAD_BRIDGE},
{      WWT_PANEL,     7,   218,   239,    14,    35, 0x97D, STR_1810_BUILD_ROAD_TUNNEL},
{      WWT_PANEL,     7,   240,   261,    14,    35, 0x2CA, STR_1811_TOGGLE_BUILD_REMOVE_FOR},
{      WWT_PANEL,     7,   262,   283,    14,    35, 0x12B7, STR_0329_PURCHASE_LAND_FOR_FUTURE},
{   WIDGETS_END},
};

static const WindowDesc _build_road_desc = {
	356, 22, 284, 36,
	WC_BUILD_TOOLBAR,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_widgets,
	BuildRoadToolbWndProc
};

void ShowBuildRoadToolbar()
{
	DeleteWindowById(WC_BUILD_TOOLBAR, 0);
	AllocateWindowDesc(&_build_road_desc);
}

static const Widget _build_road_scen_widgets[] = {
{    WWT_TEXTBTN,     7,     0,    10,     0,    13, STR_00C5,	STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,     7,    11,   195,     0,    13, STR_1802_ROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,     7,     0,    21,    14,    35, 0x51D,			STR_180B_BUILD_ROAD_SECTION},
{     WWT_IMGBTN,     7,    22,    43,    14,    35, 0x51E,			STR_180B_BUILD_ROAD_SECTION},
{     WWT_IMGBTN,     7,    44,    65,    14,    35, 0x2BF,			STR_018D_DEMOLISH_BUILDINGS_ETC},
{     WWT_IMGBTN,     7,    66,    87,    14,    35, 0x2B7,			STR_018E_LOWER_A_CORNER_OF_LAND},
{     WWT_IMGBTN,     7,    88,   109,    14,    35, 0x2B6,			STR_018F_RAISE_A_CORNER_OF_LAND},
{      WWT_EMPTY,     0,     0,     0,     0,     0, 0x0,				STR_NULL},
{      WWT_EMPTY,     0,     0,     0,     0,     0, 0x0,				STR_NULL},
{      WWT_EMPTY,     0,     0,     0,     0,     0, 0x0,				STR_NULL},
{     WWT_IMGBTN,     7,   110,   151,    14,    35, 0xA22,			STR_180F_BUILD_ROAD_BRIDGE},
{     WWT_IMGBTN,     7,   152,   173,    14,    35, 0x97D,			STR_1810_BUILD_ROAD_TUNNEL},
{     WWT_IMGBTN,     7,   174,   195,    14,    35, 0x2CA,			STR_1811_TOGGLE_BUILD_REMOVE_FOR},
{   WIDGETS_END},
};

static const WindowDesc _build_road_scen_desc = {
	-1, -1, 196, 36,
	WC_SCEN_BUILD_ROAD,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_scen_widgets,
	BuildRoadToolbWndProc
};

void ShowBuildRoadScenToolbar()
{
	AllocateWindowDescFront(&_build_road_scen_desc, 0);
}

static void BuildRoadDepotWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		w->click_state = (1<<3) << _road_depot_orientation;
		DrawWindowWidgets(w);

		DrawRoadDepotSprite(70, 17, 0);
		DrawRoadDepotSprite(70, 69, 1);
		DrawRoadDepotSprite(2, 69, 2);
		DrawRoadDepotSprite(2, 17, 3);
		break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 0:
			ResetObjectToPlace();
			break;
		case 3:
		case 4:
		case 5:
		case 6:
			_road_depot_orientation = e->click.widget - 3;
			SndPlayFx(0x13);
			SetWindowDirty(w);
			break;
		}
	}	break;

	case WE_MOUSELOOP:
		if (WP(w,def_d).close)
			DeleteWindow(w);
		break;
	}
}

static const Widget _build_road_depot_widgets[] = {
{   WWT_CLOSEBOX,     7,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,     7,    11,   139,     0,    13, STR_1806_ROAD_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     7,     0,   139,    14,   121, 0x0,			STR_NULL},
{      WWT_PANEL,    14,    71,   136,    17,    66, 0x0,			STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,    14,    71,   136,    69,   118, 0x0,			STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,    14,     3,    68,    69,   118, 0x0,			STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{      WWT_PANEL,    14,     3,    68,    17,    66, 0x0,			STR_1813_SELECT_ROAD_VEHICLE_DEPOT},
{   WIDGETS_END},
};

static const WindowDesc _build_road_depot_desc = {
	-1,-1, 140, 122,
	WC_BUILD_DEPOT,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_depot_widgets,
	BuildRoadDepotWndProc
};

static void ShowRoadDepotPicker()
{
	AllocateWindowDesc(&_build_road_depot_desc);
}

static void RoadStationPickerWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT: {
		int image;

		w->click_state = ((1<<3) << _road_station_picker_orientation)	|
										 ((1<<7) << _station_show_coverage);
		DrawWindowWidgets(w);

		SetTileSelectSize(1, 1);
		if (_station_show_coverage)
			SetTileSelectBigSize(-4, -4, 8, 8);

		image = (w->window_class == WC_BUS_STATION) ? 0x47 : 0x43;

		StationPickerDrawSprite(103, 35, 0, image);
		StationPickerDrawSprite(103, 85, 0, image+1);
		StationPickerDrawSprite(35, 85, 0, image+2);
		StationPickerDrawSprite(35, 35, 0, image+3);

		DrawStringCentered(70, 120, STR_3066_COVERAGE_AREA_HIGHLIGHT, 0);
		DrawStationCoverageAreaText(2, 146,
			((w->window_class == WC_BUS_STATION) ? (1<<CT_PASSENGERS) : ~(1<<CT_PASSENGERS))
		);

	} break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 0:
			ResetObjectToPlace();
			break;
		case 3:
		case 4:
		case 5:
		case 6:
			_road_station_picker_orientation = e->click.widget - 3;
			SndPlayFx(0x13);
			SetWindowDirty(w);
			break;
		case 7:
		case 8:
			_station_show_coverage = e->click.widget - 7;
			SndPlayFx(0x13);
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
	}
}

static const Widget _bus_station_picker_widgets[] = {
{   WWT_CLOSEBOX,     7,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,     7,    11,   139,     0,    13, STR_3042_BUS_STATION_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     7,     0,   139,    14,   176, 0x0,					STR_NULL},
{      WWT_PANEL,    14,    71,   136,    17,    66, 0x0,					STR_3051_SELECT_BUS_STATION_ORIENTATION},
{      WWT_PANEL,    14,    71,   136,    69,   118, 0x0,					STR_3051_SELECT_BUS_STATION_ORIENTATION},
{      WWT_PANEL,    14,     3,    68,    69,   118, 0x0,					STR_3051_SELECT_BUS_STATION_ORIENTATION},
{      WWT_PANEL,    14,     3,    68,    17,    66, 0x0,					STR_3051_SELECT_BUS_STATION_ORIENTATION},
{   WWT_CLOSEBOX,    14,    10,    69,   133,   144, STR_02DB_OFF,STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{   WWT_CLOSEBOX,    14,    70,   129,   133,   144, STR_02DA_ON,	STR_3064_HIGHLIGHT_COVERAGE_AREA},
{   WIDGETS_END},
};

static const WindowDesc _bus_station_picker_desc = {
	-1,-1, 140, 177,
	WC_BUS_STATION,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_bus_station_picker_widgets,
	RoadStationPickerWndProc
};

static void ShowBusStationPicker()
{
	AllocateWindowDesc(&_bus_station_picker_desc);
}

static const Widget _truck_station_picker_widgets[] = {
{   WWT_CLOSEBOX,     7,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,     7,    11,   139,     0,    13, STR_3043_TRUCK_STATION_ORIENT, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,     7,     0,   139,    14,   176, 0x0,					STR_NULL},
{      WWT_PANEL,    14,    71,   136,    17,    66, 0x0,					STR_3052_SELECT_TRUCK_LOADING_BAY},
{      WWT_PANEL,    14,    71,   136,    69,   118, 0x0,					STR_3052_SELECT_TRUCK_LOADING_BAY},
{      WWT_PANEL,    14,     3,    68,    69,   118, 0x0,					STR_3052_SELECT_TRUCK_LOADING_BAY},
{      WWT_PANEL,    14,     3,    68,    17,    66, 0x0,					STR_3052_SELECT_TRUCK_LOADING_BAY},
{   WWT_CLOSEBOX,    14,    10,    69,   133,   144, STR_02DB_OFF,STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{   WWT_CLOSEBOX,    14,    70,   129,   133,   144, STR_02DA_ON,	STR_3064_HIGHLIGHT_COVERAGE_AREA},
{   WIDGETS_END},
};

static const WindowDesc _truck_station_picker_desc = {
	-1,-1, 140, 177,
	WC_TRUCK_STATION,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_truck_station_picker_widgets,
	RoadStationPickerWndProc
};

static void ShowTruckStationPicker()
{
	AllocateWindowDesc(&_truck_station_picker_desc);
}

void InitializeRoadGui()
{
	_road_depot_orientation = 3;
	_road_station_picker_orientation = 3;
}
