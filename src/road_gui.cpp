/* $Id$ */

/** @file road_gui.cpp GUI for building roads. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "variables.h"
#include "road_type.h"
#include "road_cmd.h"
#include "road_map.h"
#include "station_map.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "player_func.h"
#include "settings_type.h"
#include "tunnelbridge.h"
#include "tilehighlight_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static void ShowRVStationPicker(RoadStopType rs);
static void ShowRoadDepotPicker();

static bool _remove_button_clicked;
static bool _one_way_button_clicked;

/**
 * Define the values of the RoadFlags
 * @see CmdBuildLongRoad
 */
enum RoadFlags {
	RF_NONE             = 0x00,
	RF_START_HALFROAD_Y = 0x01,    // The start tile in Y-dir should have only a half road
	RF_END_HALFROAD_Y   = 0x02,    // The end tile in Y-dir should have only a half road
	RF_DIR_Y            = 0x04,    // The direction is Y-dir
	RF_DIR_X            = RF_NONE, // Dummy; Dir X is set when RF_DIR_Y is not set
	RF_START_HALFROAD_X = 0x08,    // The start tile in X-dir should have only a half road
	RF_END_HALFROAD_X   = 0x10,    // The end tile in X-dir should have only a half road
};
DECLARE_ENUM_AS_BIT_SET(RoadFlags);

static RoadFlags _place_road_flag;

static RoadType _cur_roadtype;

static DiagDirection _road_depot_orientation;
static DiagDirection _road_station_picker_orientation;

void CcPlaySound1D(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_1F_SPLAT, tile);
}

/**
 * Set the initial flags for the road constuction.
 * The flags are:
 * @li The direction is the X-dir
 * @li The first tile has a partitial RoadBit (true or false)
 *
 * @param tile The start tile
 */
static void PlaceRoad_X_Dir(TileIndex tile)
{
	_place_road_flag = RF_DIR_X;
	if (_tile_fract_coords.x >= 8) _place_road_flag |= RF_START_HALFROAD_X;
	VpStartPlaceSizing(tile, VPM_FIX_Y, DDSP_PLACE_ROAD_X_DIR);
}

/**
 * Set the initial flags for the road constuction.
 * The flags are:
 * @li The direction is the Y-dir
 * @li The first tile has a partitial RoadBit (true or false)
 *
 * @param tile The start tile
 */
static void PlaceRoad_Y_Dir(TileIndex tile)
{
	_place_road_flag = RF_DIR_Y;
	if (_tile_fract_coords.y >= 8) _place_road_flag |= RF_START_HALFROAD_Y;
	VpStartPlaceSizing(tile, VPM_FIX_X, DDSP_PLACE_ROAD_Y_DIR);
}

/**
 * Set the initial flags for the road constuction.
 * The flags are:
 * @li The direction is not set.
 * @li The first tile has a partitial RoadBit (true or false)
 *
 * @param tile The start tile
 */
static void PlaceRoad_AutoRoad(TileIndex tile)
{
	_place_road_flag = RF_NONE;
	if (_tile_fract_coords.x >= 8) _place_road_flag |= RF_START_HALFROAD_X;
	if (_tile_fract_coords.y >= 8) _place_road_flag |= RF_START_HALFROAD_Y;
	VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_PLACE_AUTOROAD);
}

static void PlaceRoad_Bridge(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
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

/** Structure holding information per roadtype for several functions */
struct RoadTypeInfo {
	StringID err_build_road;        ///< Building a normal piece of road
	StringID err_remove_road;       ///< Removing a normal piece of road
	StringID err_depot;             ///< Building a depot
	StringID err_build_station[2];  ///< Building a bus or truck station
	StringID err_remove_station[2]; ///< Removing of a bus or truck station

	StringID picker_title[2];       ///< Title for the station picker for bus or truck stations
	StringID picker_tooltip[2];     ///< Tooltip for the station picker for bus or truck stations

	SpriteID cursor_nesw;           ///< Cursor for building NE and SW bits
	SpriteID cursor_nwse;           ///< Cursor for building NW and SE bits
	SpriteID cursor_autoroad;       ///< Cursor for building autoroad
};

/** What errors/cursors must be shown for several types of roads */
static const RoadTypeInfo _road_type_infos[] = {
	{
		STR_1804_CAN_T_BUILD_ROAD_HERE,
		STR_1805_CAN_T_REMOVE_ROAD_FROM,
		STR_1807_CAN_T_BUILD_ROAD_VEHICLE,
		{ STR_1808_CAN_T_BUILD_BUS_STATION,        STR_1809_CAN_T_BUILD_TRUCK_STATION },
		{ STR_CAN_T_REMOVE_BUS_STATION,            STR_CAN_T_REMOVE_TRUCK_STATION     },
		{ STR_3042_BUS_STATION_ORIENTATION,        STR_3043_TRUCK_STATION_ORIENT      },
		{ STR_3051_SELECT_BUS_STATION_ORIENTATION, STR_3052_SELECT_TRUCK_LOADING_BAY  },

		SPR_CURSOR_ROAD_NESW,
		SPR_CURSOR_ROAD_NWSE,
		SPR_CURSOR_AUTOROAD,
	},
	{
		STR_CAN_T_BUILD_TRAMWAY_HERE,
		STR_CAN_T_REMOVE_TRAMWAY_FROM,
		STR_CAN_T_BUILD_TRAM_VEHICLE,
		{ STR_CAN_T_BUILD_PASSENGER_TRAM_STATION,        STR_CAN_T_BUILD_CARGO_TRAM_STATION        },
		{ STR_CAN_T_REMOVE_PASSENGER_TRAM_STATION,       STR_CAN_T_REMOVE_CARGO_TRAM_STATION       },
		{ STR_PASSENGER_TRAM_STATION_ORIENTATION,        STR_CARGO_TRAM_STATION_ORIENT             },
		{ STR_SELECT_PASSENGER_TRAM_STATION_ORIENTATION, STR_SELECT_CARGO_TRAM_STATION_ORIENTATION },

		SPR_CURSOR_TRAMWAY_NESW,
		SPR_CURSOR_TRAMWAY_NWSE,
		SPR_CURSOR_AUTOTRAM,
	},
};

static void PlaceRoad_Tunnel(TileIndex tile)
{
	DoCommandP(tile, 0x200 | RoadTypeToRoadTypes(_cur_roadtype), 0, CcBuildRoadTunnel, CMD_BUILD_TUNNEL | CMD_MSG(STR_5016_CAN_T_BUILD_TUNNEL_HERE));
}

static void BuildRoadOutsideStation(TileIndex tile, DiagDirection direction)
{
	tile += TileOffsByDiagDir(direction);
	/* if there is a roadpiece just outside of the station entrance, build a connecting route */
	if (IsNormalRoadTile(tile)) {
		if (GetRoadBits(tile, _cur_roadtype) != ROAD_NONE) {
			DoCommandP(tile, _cur_roadtype << 4 | DiagDirToRoadBits(ReverseDiagDir(direction)), 0, NULL, CMD_BUILD_ROAD);
		}
	}
}

void CcRoadDepot(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		DiagDirection dir = (DiagDirection)GB(p1, 0, 2);
		SndPlayTileFx(SND_1F_SPLAT, tile);
		ResetObjectToPlace();
		BuildRoadOutsideStation(tile, dir);
		/* For a drive-through road stop build connecting road for other entrance */
		if (HasBit(p2, 1)) BuildRoadOutsideStation(tile, ReverseDiagDir(dir));
	}
}

static void PlaceRoad_Depot(TileIndex tile)
{
	DoCommandP(tile, _cur_roadtype << 2 | _road_depot_orientation, 0, CcRoadDepot, CMD_BUILD_ROAD_DEPOT | CMD_NO_WATER | CMD_MSG(_road_type_infos[_cur_roadtype].err_depot));
}

static void PlaceRoadStop(TileIndex tile, uint32 p2, uint32 cmd)
{
	uint32 p1 = _road_station_picker_orientation;

	if (p1 >= DIAGDIR_END) {
		SetBit(p2, 1); // It's a drive-through stop
		p1 -= DIAGDIR_END; // Adjust picker result to actual direction
	}
	DoCommandP(tile, p1, p2, CcRoadDepot, cmd);
}

static void PlaceRoad_BusStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		DoCommandP(tile, 0, ROADSTOP_BUS, CcPlaySound1D, CMD_REMOVE_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_station[ROADSTOP_BUS]));
	} else {
		PlaceRoadStop(tile, (_ctrl_pressed << 5) | RoadTypeToRoadTypes(_cur_roadtype) << 2 | ROADSTOP_BUS, CMD_BUILD_ROAD_STOP | CMD_NO_WATER | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_station[ROADSTOP_BUS]));
	}
}

static void PlaceRoad_TruckStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		DoCommandP(tile, 0, ROADSTOP_TRUCK, CcPlaySound1D, CMD_REMOVE_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_station[ROADSTOP_TRUCK]));
	} else {
		PlaceRoadStop(tile, (_ctrl_pressed << 5) | RoadTypeToRoadTypes(_cur_roadtype) << 2 | ROADSTOP_TRUCK, CMD_BUILD_ROAD_STOP | CMD_NO_WATER | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_station[ROADSTOP_TRUCK]));
	}
}

/** Enum referring to the widgets of the build road toolbar */
enum RoadToolbarWidgets {
	RTW_CLOSEBOX = 0,
	RTW_CAPTION,
	RTW_STICKY,
	RTW_ROAD_X,
	RTW_ROAD_Y,
	RTW_AUTOROAD,
	RTW_DEMOLISH,
	RTW_DEPOT,
	RTW_BUS_STATION,
	RTW_TRUCK_STATION,
	RTW_ONE_WAY,
	RTW_BUILD_BRIDGE,
	RTW_BUILD_TUNNEL,
	RTW_REMOVE,
};

typedef void OnButtonClick(Window *w);


/** Toogles state of the Remove button of Build road toolbar
 * @param w window the button belongs to
 */
static void ToggleRoadButton_Remove(Window *w)
{
	w->ToggleWidgetLoweredState(RTW_REMOVE);
	w->InvalidateWidget(RTW_REMOVE);
	_remove_button_clicked = w->IsWidgetLowered(RTW_REMOVE);
	SetSelectionRed(_remove_button_clicked);
}

/** Updates the Remove button because of Ctrl state change
 * @param w window the button belongs to
 * @return true iff the remove buton was changed
 */
static bool RoadToolbar_CtrlChanged(Window *w)
{
	if (w->IsWidgetDisabled(RTW_REMOVE)) return false;

	/* allow ctrl to switch remove mode only for these widgets */
	for (uint i = RTW_ROAD_X; i <= RTW_AUTOROAD; i++) {
		if (w->IsWidgetLowered(i)) {
			ToggleRoadButton_Remove(w);
			return true;
		}
	}

	return false;
}


/**
 * Function that handles the click on the
 *  X road placement button.
 *
 * @param w The current window
 */
static void BuildRoadClick_X_Dir(Window *w)
{
	HandlePlacePushButton(w, RTW_ROAD_X, _road_type_infos[_cur_roadtype].cursor_nwse, VHM_RECT, PlaceRoad_X_Dir);
}

/**
 * Function that handles the click on the
 *  Y road placement button.
 *
 * @param w The current window
 */
static void BuildRoadClick_Y_Dir(Window *w)
{
	HandlePlacePushButton(w, RTW_ROAD_Y, _road_type_infos[_cur_roadtype].cursor_nesw, VHM_RECT, PlaceRoad_Y_Dir);
}

/**
 * Function that handles the click on the
 *  autoroad placement button.
 *
 * @param w The current window
 */
static void BuildRoadClick_AutoRoad(Window *w)
{
	HandlePlacePushButton(w, RTW_AUTOROAD, _road_type_infos[_cur_roadtype].cursor_autoroad, VHM_RECT, PlaceRoad_AutoRoad);
}

static void BuildRoadClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, RTW_DEMOLISH, ANIMCURSOR_DEMOLISH, VHM_RECT, PlaceProc_DemolishArea);
}

static void BuildRoadClick_Depot(Window *w)
{
	if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
	if (HandlePlacePushButton(w, RTW_DEPOT, SPR_CURSOR_ROAD_DEPOT, VHM_RECT, PlaceRoad_Depot)) ShowRoadDepotPicker();
}

static void BuildRoadClick_BusStation(Window *w)
{
	if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
	if (HandlePlacePushButton(w, RTW_BUS_STATION, SPR_CURSOR_BUS_STATION, VHM_RECT, PlaceRoad_BusStation)) ShowRVStationPicker(ROADSTOP_BUS);
}

static void BuildRoadClick_TruckStation(Window *w)
{
	if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
	if (HandlePlacePushButton(w, RTW_TRUCK_STATION, SPR_CURSOR_TRUCK_STATION, VHM_RECT, PlaceRoad_TruckStation)) ShowRVStationPicker(ROADSTOP_TRUCK);
}

/**
 * Function that handles the click on the
 *  one way road button.
 *
 * @param w The current window
 */
static void BuildRoadClick_OneWay(Window *w)
{
	if (w->IsWidgetDisabled(RTW_ONE_WAY)) return;
	w->SetDirty();
	w->ToggleWidgetLoweredState(RTW_ONE_WAY);
	SetSelectionRed(false);
}

static void BuildRoadClick_Bridge(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, VHM_RECT, PlaceRoad_Bridge);
}

static void BuildRoadClick_Tunnel(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_TUNNEL, SPR_CURSOR_ROAD_TUNNEL, VHM_SPECIAL, PlaceRoad_Tunnel);
}

static void BuildRoadClick_Remove(Window *w)
{
	if (w->IsWidgetDisabled(RTW_REMOVE)) return;
	ToggleRoadButton_Remove(w);
	SndPlayFx(SND_15_BEEP);
}

/** Array with the handlers of the button-clicks for the road-toolbar */
static OnButtonClick* const _build_road_button_proc[] = {
	BuildRoadClick_X_Dir,
	BuildRoadClick_Y_Dir,
	BuildRoadClick_AutoRoad,
	BuildRoadClick_Demolish,
	BuildRoadClick_Depot,
	BuildRoadClick_BusStation,
	BuildRoadClick_TruckStation,
	BuildRoadClick_OneWay,
	BuildRoadClick_Bridge,
	BuildRoadClick_Tunnel,
	BuildRoadClick_Remove
};

/** Array with the keycode of the button-clicks for the road-toolbar */
static const uint16 _road_keycodes[] = {
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'B',
	'T',
	'R',
};

/**
 * Update the remove button lowered state of the road toolbar
 *
 * @param w The toolbar window
 * @param clicked_widget The widget which the player clicked just now
 */
static void UpdateOptionWidgetStatus(Window *w, RoadToolbarWidgets clicked_widget)
{
	/* The remove and the one way button state is driven
	 * by the other buttons so they don't act on themselfs.
	 * Both are only valid if they are able to apply as options. */
	switch (clicked_widget) {
		case RTW_REMOVE:
			w->RaiseWidget(RTW_ONE_WAY);
			w->InvalidateWidget(RTW_ONE_WAY);
			break;

		case RTW_ONE_WAY:
			w->RaiseWidget(RTW_REMOVE);
			w->InvalidateWidget(RTW_REMOVE);
			break;

		case RTW_BUS_STATION:
		case RTW_TRUCK_STATION:
			w->DisableWidget(RTW_ONE_WAY);
			w->SetWidgetDisabledState(RTW_REMOVE, !w->IsWidgetLowered(clicked_widget));
			break;

		case RTW_ROAD_X:
		case RTW_ROAD_Y:
		case RTW_AUTOROAD:
			w->SetWidgetsDisabledState(!w->IsWidgetLowered(clicked_widget),
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);
			break;

		default:
			/* When any other buttons than road/station, raise and
			 * disable the removal button */
			w->SetWidgetsDisabledState(true,
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);
			w->SetWidgetsLoweredState (false,
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);
			break;
	}
}

static void BuildRoadToolbWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			w->SetWidgetsDisabledState(true,
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);
			break;

		case WE_PAINT:
			w->SetWidgetsDisabledState(!CanBuildVehicleInfrastructure(VEH_ROAD),
				RTW_DEPOT,
				RTW_BUS_STATION,
				RTW_TRUCK_STATION,
				WIDGET_LIST_END);
			w->DrawWidgets();
			break;

		case WE_CLICK:
			if (e->we.click.widget >= RTW_ROAD_X) {
				_remove_button_clicked = false;
				_one_way_button_clicked = false;
				_build_road_button_proc[e->we.click.widget - RTW_ROAD_X](w);
			}
			UpdateOptionWidgetStatus(w, (RoadToolbarWidgets)e->we.click.widget);
			if (_ctrl_pressed) RoadToolbar_CtrlChanged(w);
			break;

		case WE_KEYPRESS:
			for (uint i = 0; i != lengthof(_road_keycodes); i++) {
				if (e->we.keypress.keycode == _road_keycodes[i]) {
					e->we.keypress.cont = false;
					_remove_button_clicked = false;
					_one_way_button_clicked = false;
					_build_road_button_proc[i](w);
					UpdateOptionWidgetStatus(w, (RoadToolbarWidgets)(i + RTW_ROAD_X));
					if (_ctrl_pressed) RoadToolbar_CtrlChanged(w);
					break;
				}
			}
			MarkTileDirty(_thd.pos.x, _thd.pos.y); // redraw tile selection
			break;

		case WE_PLACE_OBJ:
			_remove_button_clicked = w->IsWidgetLowered(RTW_REMOVE);
			_one_way_button_clicked = w->IsWidgetLowered(RTW_ONE_WAY);
			_place_proc(e->we.place.tile);
			break;

		case WE_ABORT_PLACE_OBJ:
			w->RaiseButtons();
			w->SetWidgetsDisabledState(true,
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);
			w->InvalidateWidget(RTW_REMOVE);
			w->InvalidateWidget(RTW_ONE_WAY);

			delete FindWindowById(WC_BUS_STATION, 0);
			delete FindWindowById(WC_TRUCK_STATION, 0);
			delete FindWindowById(WC_BUILD_DEPOT, 0);
			break;

		case WE_PLACE_DRAG:
			/* Here we update the end tile flags
			* of the road placement actions.
			* At first we reset the end halfroad
			* bits and if needed we set them again. */
			switch (e->we.place.select_proc) {
				case DDSP_PLACE_ROAD_X_DIR:
					_place_road_flag &= ~RF_END_HALFROAD_X;
					if (e->we.place.pt.x & 8) _place_road_flag |= RF_END_HALFROAD_X;
					break;

				case DDSP_PLACE_ROAD_Y_DIR:
					_place_road_flag &= ~RF_END_HALFROAD_Y;
					if (e->we.place.pt.y & 8) _place_road_flag |= RF_END_HALFROAD_Y;
					break;

				case DDSP_PLACE_AUTOROAD:
					_place_road_flag &= ~(RF_END_HALFROAD_Y | RF_END_HALFROAD_X);
					if (e->we.place.pt.y & 8) _place_road_flag |= RF_END_HALFROAD_Y;
					if (e->we.place.pt.x & 8) _place_road_flag |= RF_END_HALFROAD_X;

					/* For autoroad we need to update the
					* direction of the road */
					if (_thd.size.x > _thd.size.y || (_thd.size.x == _thd.size.y &&
							( (_tile_fract_coords.x < _tile_fract_coords.y && (_tile_fract_coords.x + _tile_fract_coords.y) < 16) ||
							(_tile_fract_coords.x > _tile_fract_coords.y && (_tile_fract_coords.x + _tile_fract_coords.y) > 16) ))) {
						/* Set dir = X */
						_place_road_flag &= ~RF_DIR_Y;
					} else {
						/* Set dir = Y */
						_place_road_flag |= RF_DIR_Y;
					}

					break;

				default:
					break;
			}

			VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
			return;

		case WE_PLACE_MOUSEUP:
			if (e->we.place.pt.x != -1) {
				TileIndex start_tile = e->we.place.starttile;
				TileIndex end_tile = e->we.place.tile;

				switch (e->we.place.select_proc) {
					default: NOT_REACHED();
					case DDSP_BUILD_BRIDGE:
						ResetObjectToPlace();
						ShowBuildBridgeWindow(start_tile, end_tile, TRANSPORT_ROAD, RoadTypeToRoadTypes(_cur_roadtype));
						break;

					case DDSP_DEMOLISH_AREA:
						GUIPlaceProcDragXY(e->we.place.select_proc, e->we.place.starttile, e->we.place.tile);
						break;

					case DDSP_PLACE_ROAD_X_DIR:
					case DDSP_PLACE_ROAD_Y_DIR:
					case DDSP_PLACE_AUTOROAD:
						/* Flag description:
						* Use the first three bits (0x07) if dir == Y
						* else use the last 2 bits (X dir has
						* not the 3rd bit set) */
						_place_road_flag = (RoadFlags)((_place_road_flag & RF_DIR_Y) ? (_place_road_flag & 0x07) : (_place_road_flag >> 3));

						DoCommandP(end_tile, start_tile, _place_road_flag | (_cur_roadtype << 3) | (_one_way_button_clicked << 5), CcPlaySound1D,
							(_ctrl_pressed || _remove_button_clicked) ?
							CMD_REMOVE_LONG_ROAD | CMD_NO_WATER | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_road) :
							CMD_BUILD_LONG_ROAD | CMD_NO_WATER | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_road));
						break;
				}
			}
			break;

		case WE_PLACE_PRESIZE: {
			TileIndex tile = e->we.place.tile;

			DoCommand(tile, 0x200 | RoadTypeToRoadTypes(_cur_roadtype), 0, DC_AUTO, CMD_BUILD_TUNNEL);
			VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
		} break;

		case WE_DESTROY:
			if (_patches.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0);
			break;

		case WE_CTRL_CHANGED:
			if (RoadToolbar_CtrlChanged(w)) e->we.ctrl.cont = false;
			break;

		default:
			break;
	}
}

/** Widget definition of the build road toolbar */
static const Widget _build_road_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},             // RTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   250,     0,    13, STR_1802_ROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},   // RTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   251,   262,     0,    13, 0x0,                        STR_STICKY_BUTTON},                 // RTW_STICKY

{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    21,    14,    35, SPR_IMG_ROAD_X_DIR,         STR_180B_BUILD_ROAD_SECTION},       // RTW_ROAD_X
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_ROAD_Y_DIR,         STR_180B_BUILD_ROAD_SECTION},       // RTW_ROAD_Y
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_AUTOROAD,           STR_BUILD_AUTOROAD_TIP},            // RTW_AUTOROAD
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,    87,    14,    35, SPR_IMG_DYNAMITE,           STR_018D_DEMOLISH_BUILDINGS_ETC},   // RTW_DEMOLISH
{     WWT_IMGBTN,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_IMG_ROAD_DEPOT,         STR_180C_BUILD_ROAD_VEHICLE_DEPOT}, // RTW_DEPOT
{     WWT_IMGBTN,   RESIZE_NONE,     7,   110,   131,    14,    35, SPR_IMG_BUS_STATION,        STR_180D_BUILD_BUS_STATION},        // RTW_BUS_STATION
{     WWT_IMGBTN,   RESIZE_NONE,     7,   132,   153,    14,    35, SPR_IMG_TRUCK_BAY,          STR_180E_BUILD_TRUCK_LOADING_BAY},  // RTW_TRUCK_STATION
{     WWT_IMGBTN,   RESIZE_NONE,     7,   154,   175,    14,    35, SPR_IMG_ROAD_ONE_WAY,       STR_TOGGLE_ONE_WAY_ROAD},           // RTW_ONE_WAY
{     WWT_IMGBTN,   RESIZE_NONE,     7,   176,   218,    14,    35, SPR_IMG_BRIDGE,             STR_180F_BUILD_ROAD_BRIDGE},        // RTW_BUILD_BRIDGE
{     WWT_IMGBTN,   RESIZE_NONE,     7,   219,   240,    14,    35, SPR_IMG_ROAD_TUNNEL,        STR_1810_BUILD_ROAD_TUNNEL},        // RTW_BUILD_TUNNEL
{     WWT_IMGBTN,   RESIZE_NONE,     7,   241,   262,    14,    35, SPR_IMG_REMOVE,             STR_1811_TOGGLE_BUILD_REMOVE_FOR},  // RTW_REMOVE

{   WIDGETS_END},
};

static const WindowDesc _build_road_desc = {
	WDP_ALIGN_TBR, 22, 263, 36, 263, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_road_widgets,
	BuildRoadToolbWndProc
};

/** Widget definition of the build tram toolbar */
static const Widget _build_tramway_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},                // RTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   228,     0,    13, STR_WHITE_TRAMWAY_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},      // RTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   229,   240,     0,    13, 0x0,                            STR_STICKY_BUTTON},                    // RTW_STICKY

{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    21,    14,    35, SPR_IMG_TRAMWAY_X_DIR,          STR_BUILD_TRAMWAY_SECTION},            // RTW_ROAD_X
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_TRAMWAY_Y_DIR,          STR_BUILD_TRAMWAY_SECTION},            // RTW_ROAD_Y
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_AUTOTRAM,               STR_BUILD_AUTOTRAM_TIP},               // RTW_AUTOROAD
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,    87,    14,    35, SPR_IMG_DYNAMITE,               STR_018D_DEMOLISH_BUILDINGS_ETC},      // RTW_DEMOLISH
{     WWT_IMGBTN,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_IMG_ROAD_DEPOT,             STR_BUILD_TRAM_VEHICLE_DEPOT},         // RTW_DEPOT
{     WWT_IMGBTN,   RESIZE_NONE,     7,   110,   131,    14,    35, SPR_IMG_BUS_STATION,            STR_BUILD_PASSENGER_TRAM_STATION},     // RTW_BUS_STATION
{     WWT_IMGBTN,   RESIZE_NONE,     7,   132,   153,    14,    35, SPR_IMG_TRUCK_BAY,              STR_BUILD_CARGO_TRAM_STATION},         // RTW_TRUCK_STATION
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                            STR_NULL},                             // RTW_ONE_WAY
{     WWT_IMGBTN,   RESIZE_NONE,     7,   154,   196,    14,    35, SPR_IMG_BRIDGE,                 STR_BUILD_TRAMWAY_BRIDGE},             // RTW_BUILD_BRIDGE
{     WWT_IMGBTN,   RESIZE_NONE,     7,   197,   218,    14,    35, SPR_IMG_ROAD_TUNNEL,            STR_BUILD_TRAMWAY_TUNNEL},             // RTW_BUILD_TUNNEL
{     WWT_IMGBTN,   RESIZE_NONE,     7,   219,   240,    14,    35, SPR_IMG_REMOVE,                 STR_TOGGLE_BUILD_REMOVE_FOR_TRAMWAYS}, // RTW_REMOVE

{   WIDGETS_END},
};

static const WindowDesc _build_tramway_desc = {
	WDP_ALIGN_TBR, 22, 241, 36, 241, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_tramway_widgets,
	BuildRoadToolbWndProc
};

void ShowBuildRoadToolbar(RoadType roadtype)
{
	if (!IsValidPlayer(_current_player)) return;
	_cur_roadtype = roadtype;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	Window *w = AllocateWindowDescFront<Window>(roadtype == ROADTYPE_ROAD ? &_build_road_desc : &_build_tramway_desc, TRANSPORT_ROAD);
	if (_patches.link_terraform_toolbar) ShowTerraformToolbar(w);
}

/** Widget definition of the build road toolbar in the scenario editor */
static const Widget _build_road_scen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},            // RTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   184,     0,    13, STR_1802_ROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},  // RTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   185,   196,     0,    13, 0x0,                        STR_STICKY_BUTTON},                // RTW_STICKY

{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    21,    14,    35, SPR_IMG_ROAD_X_DIR,         STR_180B_BUILD_ROAD_SECTION},      // RTW_ROAD_X
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_ROAD_Y_DIR,         STR_180B_BUILD_ROAD_SECTION},      // RTW_ROAD_Y
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_AUTOROAD,           STR_BUILD_AUTOROAD_TIP},           // RTW_AUTOROAD
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,    87,    14,    35, SPR_IMG_DYNAMITE,           STR_018D_DEMOLISH_BUILDINGS_ETC},  // RTW_DEMOLISH
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                        STR_NULL},                         // RTW_DEPOT
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                        STR_NULL},                         // RTW_BUS_STATION
{      WWT_EMPTY,   RESIZE_NONE,     0,     0,     0,     0,     0, 0x0,                        STR_NULL},                         // RTW_TRUCK_STATION
{     WWT_IMGBTN,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_IMG_ROAD_ONE_WAY,       STR_TOGGLE_ONE_WAY_ROAD},          // RTW_ONE_WAY
{     WWT_IMGBTN,   RESIZE_NONE,     7,   110,   152,    14,    35, SPR_IMG_BRIDGE,             STR_180F_BUILD_ROAD_BRIDGE},       // RTW_BUILD_BRIDGE
{     WWT_IMGBTN,   RESIZE_NONE,     7,   153,   174,    14,    35, SPR_IMG_ROAD_TUNNEL,        STR_1810_BUILD_ROAD_TUNNEL},       // RTW_BUILD_TUNNEL
{     WWT_IMGBTN,   RESIZE_NONE,     7,   175,   196,    14,    35, SPR_IMG_REMOVE,             STR_1811_TOGGLE_BUILD_REMOVE_FOR}, // RTW_REMOVE
{   WIDGETS_END},
};

static const WindowDesc _build_road_scen_desc = {
	WDP_AUTO, WDP_AUTO, 197, 36, 197, 36,
	WC_SCEN_BUILD_ROAD, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_road_scen_widgets,
	BuildRoadToolbWndProc
};

void ShowBuildRoadScenToolbar()
{
	_cur_roadtype = ROADTYPE_ROAD;
	AllocateWindowDescFront<Window>(&_build_road_scen_desc, 0);
}

struct BuildRoadDepotWindow : public PickerWindowBase {
private:
	/** Enum referring to the widgets of the build road depot window */
	enum BuildRoadDepotWidgets {
		BRDW_CLOSEBOX = 0,
		BRDW_CAPTION,
		BRDW_BACKGROUND,
		BRDW_DEPOT_NE,
		BRDW_DEPOT_SE,
		BRDW_DEPOT_SW,
		BRDW_DEPOT_NW,
	};

public:
	BuildRoadDepotWindow(const WindowDesc *desc) : PickerWindowBase(desc)
	{
		this->LowerWidget(_road_depot_orientation + BRDW_DEPOT_NE);
		if ( _cur_roadtype == ROADTYPE_TRAM) {
			this->widget[BRDW_CAPTION].data = STR_TRAM_DEPOT_ORIENTATION;
			for (int i = BRDW_DEPOT_NE; i <= BRDW_DEPOT_NW; i++) this->widget[i].tooltips = STR_SELECT_TRAM_VEHICLE_DEPOT;
		}
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		DrawRoadDepotSprite(70, 17, DIAGDIR_NE, _cur_roadtype);
		DrawRoadDepotSprite(70, 69, DIAGDIR_SE, _cur_roadtype);
		DrawRoadDepotSprite( 2, 69, DIAGDIR_SW, _cur_roadtype);
		DrawRoadDepotSprite( 2, 17, DIAGDIR_NW, _cur_roadtype);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BRDW_DEPOT_NW:
			case BRDW_DEPOT_NE:
			case BRDW_DEPOT_SW:
			case BRDW_DEPOT_SE:
				this->RaiseWidget(_road_depot_orientation + BRDW_DEPOT_NE);
				_road_depot_orientation = (DiagDirection)(widget - BRDW_DEPOT_NE);
				this->LowerWidget(_road_depot_orientation + BRDW_DEPOT_NE);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			default:
				break;
		}
	}
};

/** Widget definition of the build road depot window */
static const Widget _build_road_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},              // BRDW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_1806_ROAD_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // BRDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   121, 0x0,                             STR_NULL},                           // BRDW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT}, // BRDW_DEPOT_NE
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT}, // BRDW_DEPOT_SE
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT}, // BRDW_DEPOT_SW
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                             STR_1813_SELECT_ROAD_VEHICLE_DEPOT}, // BRDW_DEPOT_NW
{   WIDGETS_END},
};

static const WindowDesc _build_road_depot_desc = {
	WDP_AUTO, WDP_AUTO, 140, 122, 140, 122,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_road_depot_widgets,
	NULL
};

static void ShowRoadDepotPicker()
{
	new BuildRoadDepotWindow(&_build_road_depot_desc);
}

struct BuildRoadStationWindow : public PickerWindowBase {
private:
	/** Enum referring to the widgets of the build road station window */
	enum BuildRoadStationWidgets {
		BRSW_CLOSEBOX = 0,
		BRSW_CAPTION,
		BRSW_BACKGROUND,
		BRSW_STATION_NE,
		BRSW_STATION_SE,
		BRSW_STATION_SW,
		BRSW_STATION_NW,
		BRSW_STATION_X,
		BRSW_STATION_Y,
		BRSW_LT_OFF,
		BRSW_LT_ON,
		BRSW_INFO,
	};

public:
	BuildRoadStationWindow(const WindowDesc *desc, RoadStopType rs) : PickerWindowBase(desc)
	{
		/* Trams don't have non-drivethrough stations */
		if (_cur_roadtype == ROADTYPE_TRAM && _road_station_picker_orientation < DIAGDIR_END) {
			_road_station_picker_orientation = DIAGDIR_END;
		}
		this->SetWidgetsDisabledState(_cur_roadtype == ROADTYPE_TRAM,
			BRSW_STATION_NE,
			BRSW_STATION_SE,
			BRSW_STATION_SW,
			BRSW_STATION_NW,
			WIDGET_LIST_END);

		this->window_class = (rs == ROADSTOP_BUS) ? WC_BUS_STATION : WC_TRUCK_STATION;
		this->widget[BRSW_CAPTION].data = _road_type_infos[_cur_roadtype].picker_title[rs];
		for (uint i = BRSW_STATION_NE; i < BRSW_LT_OFF; i++) this->widget[i].tooltips = _road_type_infos[_cur_roadtype].picker_tooltip[rs];

		this->LowerWidget(_road_station_picker_orientation + BRSW_STATION_NE);
		this->LowerWidget(_station_show_coverage + BRSW_LT_OFF);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		if (_station_show_coverage) {
			int rad = _patches.modified_catchment ? CA_TRUCK /* = CA_BUS */ : CA_UNMODIFIED;
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		StationType st = (this->window_class == WC_BUS_STATION) ? STATION_BUS : STATION_TRUCK;

		StationPickerDrawSprite(103, 35, st, INVALID_RAILTYPE, ROADTYPE_ROAD, 0);
		StationPickerDrawSprite(103, 85, st, INVALID_RAILTYPE, ROADTYPE_ROAD, 1);
		StationPickerDrawSprite( 35, 85, st, INVALID_RAILTYPE, ROADTYPE_ROAD, 2);
		StationPickerDrawSprite( 35, 35, st, INVALID_RAILTYPE, ROADTYPE_ROAD, 3);

		StationPickerDrawSprite(171, 35, st, INVALID_RAILTYPE, _cur_roadtype, 4);
		StationPickerDrawSprite(171, 85, st, INVALID_RAILTYPE, _cur_roadtype, 5);

		int text_end = DrawStationCoverageAreaText(2, 146,
			(this->window_class == WC_BUS_STATION) ? SCT_PASSENGERS_ONLY : SCT_NON_PASSENGERS_ONLY,
			3, false);
		text_end = DrawStationCoverageAreaText(2, text_end + 4,
			(this->window_class == WC_BUS_STATION) ? SCT_PASSENGERS_ONLY : SCT_NON_PASSENGERS_ONLY,
			3, true) + 4;
		if (text_end > this->widget[BRSW_BACKGROUND].bottom) {
			this->SetDirty();
			ResizeWindowForWidget(this, BRSW_BACKGROUND, 0, text_end - this->widget[BRSW_BACKGROUND].bottom);
			this->SetDirty();
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BRSW_STATION_NE:
			case BRSW_STATION_SE:
			case BRSW_STATION_SW:
			case BRSW_STATION_NW:
			case BRSW_STATION_X:
			case BRSW_STATION_Y:
				this->RaiseWidget(_road_station_picker_orientation + BRSW_STATION_NE);
				_road_station_picker_orientation = (DiagDirection)(widget - BRSW_STATION_NE);
				this->LowerWidget(_road_station_picker_orientation + BRSW_STATION_NE);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			case BRSW_LT_OFF:
			case BRSW_LT_ON:
				this->RaiseWidget(_station_show_coverage + BRSW_LT_OFF);
				_station_show_coverage = (widget != BRSW_LT_OFF);
				this->LowerWidget(_station_show_coverage + BRSW_LT_OFF);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			default:
				break;
		}
	}

	virtual void OnTick()
	{
		CheckRedrawStationCoverage(this);
	}
};

/** Widget definition of the build raod station window */
static const Widget _rv_station_picker_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},             // BRSW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   206,     0,    13, STR_NULL,                         STR_018C_WINDOW_TITLE_DRAG_THIS},   // BRSW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   206,    14,   176, 0x0,                              STR_NULL},                          // BRSW_BACKGROUND

{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                              STR_NULL},                          // BRSW_STATION_NE
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                              STR_NULL},                          // BRSW_STATION_SE
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                              STR_NULL},                          // BRSW_STATION_SW
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                              STR_NULL},                          // BRSW_STATION_NW
{      WWT_PANEL,   RESIZE_NONE,    14,   139,   204,    17,    66, 0x0,                              STR_NULL},                          // BRSW_STATION_X
{      WWT_PANEL,   RESIZE_NONE,    14,   139,   204,    69,   118, 0x0,                              STR_NULL},                          // BRSW_STATION_Y

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    10,    69,   133,   144, STR_02DB_OFF,                     STR_3065_DON_T_HIGHLIGHT_COVERAGE}, // BRSW_LT_OFF
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    70,   129,   133,   144, STR_02DA_ON,                      STR_3064_HIGHLIGHT_COVERAGE_AREA},  // BRSW_LT_ON
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   139,   120,   133, STR_3066_COVERAGE_AREA_HIGHLIGHT, STR_NULL},                          // BRSW_INFO
{   WIDGETS_END},
};

static const WindowDesc _rv_station_picker_desc = {
	WDP_AUTO, WDP_AUTO, 207, 177, 207, 177,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_rv_station_picker_widgets,
	NULL
};

static void ShowRVStationPicker(RoadStopType rs)
{
	new BuildRoadStationWindow(&_rv_station_picker_desc, rs);
}

void InitializeRoadGui()
{
	_road_depot_orientation = DIAGDIR_NW;
	_road_station_picker_orientation = DIAGDIR_NW;
}
