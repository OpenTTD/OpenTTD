/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_gui.cpp GUI for building roads. */

#include "stdafx.h"
#include "gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "viewport_func.h"
#include "command_func.h"
#include "road_cmd.h"
#include "station_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "tunnelbridge.h"
#include "tunnelbridge_map.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "hotkeys.h"

#include "table/strings.h"

static void ShowRVStationPicker(Window *parent, RoadStopType rs);
static void ShowRoadDepotPicker(Window *parent);

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
DECLARE_ENUM_AS_BIT_SET(RoadFlags)

static RoadFlags _place_road_flag;

static RoadType _cur_roadtype;

static DiagDirection _road_depot_orientation;
static DiagDirection _road_station_picker_orientation;

void CcPlaySound1D(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Succeeded()) SndPlayTileFx(SND_1F_SPLAT, tile);
}

/**
 * Callback to start placing a bridge.
 * @param tile Start tile of the bridge.
 */
static void PlaceRoad_Bridge(TileIndex tile, Window *w)
{
	if (IsBridgeTile(tile)) {
		TileIndex other_tile = GetOtherTunnelBridgeEnd(tile);
		Point pt = {0, 0};
		w->OnPlaceMouseUp(VPM_X_OR_Y, DDSP_BUILD_BRIDGE, pt, tile, other_tile);
	} else {
		VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
	}
}

void CcBuildRoadTunnel(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Succeeded()) {
		SndPlayTileFx(SND_20_SPLAT_2, tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
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
		STR_ERROR_CAN_T_BUILD_ROAD_HERE,
		STR_ERROR_CAN_T_REMOVE_ROAD_FROM,
		STR_ERROR_CAN_T_BUILD_ROAD_DEPOT,
		{ STR_ERROR_CAN_T_BUILD_BUS_STATION,         STR_ERROR_CAN_T_BUILD_TRUCK_STATION          },
		{ STR_ERROR_CAN_T_REMOVE_BUS_STATION,        STR_ERROR_CAN_T_REMOVE_TRUCK_STATION         },
		{ STR_STATION_BUILD_BUS_ORIENTATION,         STR_STATION_BUILD_TRUCK_ORIENTATION          },
		{ STR_STATION_BUILD_BUS_ORIENTATION_TOOLTIP, STR_STATION_BUILD_TRUCK_ORIENTATION_TOOLTIP  },

		SPR_CURSOR_ROAD_NESW,
		SPR_CURSOR_ROAD_NWSE,
		SPR_CURSOR_AUTOROAD,
	},
	{
		STR_ERROR_CAN_T_BUILD_TRAMWAY_HERE,
		STR_ERROR_CAN_T_REMOVE_TRAMWAY_FROM,
		STR_ERROR_CAN_T_BUILD_TRAM_DEPOT,
		{ STR_ERROR_CAN_T_BUILD_PASSENGER_TRAM_STATION,         STR_ERROR_CAN_T_BUILD_CARGO_TRAM_STATION         },
		{ STR_ERROR_CAN_T_REMOVE_PASSENGER_TRAM_STATION,        STR_ERROR_CAN_T_REMOVE_CARGO_TRAM_STATION        },
		{ STR_STATION_BUILD_PASSENGER_TRAM_ORIENTATION,         STR_STATION_BUILD_CARGO_TRAM_ORIENTATION         },
		{ STR_STATION_BUILD_PASSENGER_TRAM_ORIENTATION_TOOLTIP, STR_STATION_BUILD_CARGO_TRAM_ORIENTATION_TOOLTIP },

		SPR_CURSOR_TRAMWAY_NESW,
		SPR_CURSOR_TRAMWAY_NWSE,
		SPR_CURSOR_AUTOTRAM,
	},
};

static void BuildRoadOutsideStation(TileIndex tile, DiagDirection direction)
{
	tile += TileOffsByDiagDir(direction);
	/* if there is a roadpiece just outside of the station entrance, build a connecting route */
	if (IsNormalRoadTile(tile)) {
		if (GetRoadBits(tile, _cur_roadtype) != ROAD_NONE) {
			DoCommandP(tile, _cur_roadtype << 4 | DiagDirToRoadBits(ReverseDiagDir(direction)), 0, CMD_BUILD_ROAD);
		}
	}
}

void CcRoadDepot(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	DiagDirection dir = (DiagDirection)GB(p1, 0, 2);
	SndPlayTileFx(SND_1F_SPLAT, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	BuildRoadOutsideStation(tile, dir);
}

/**
 * Command callback for building road stops.
 * @param result Result of the build road stop command.
 * @param tile Start tile.
 * @param p1 bit 0..7: Width of the road stop.
 *           bit 8..15: Length of the road stop.
 * @param p2 bit 0: 0 For bus stops, 1 for truck stops.
 *           bit 1: 0 For normal stops, 1 for drive-through.
 *           bit 2..3: The roadtypes.
 *           bit 5: Allow stations directly adjacent to other stations.
 *           bit 6..7: Entrance direction (#DiagDirection).
 *           bit 16..31: Station ID to join (NEW_STATION if build new one).
 * @see CmdBuildRoadStop
 */
void CcRoadStop(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;

	DiagDirection dir = (DiagDirection)GB(p2, 6, 2);
	SndPlayTileFx(SND_1F_SPLAT, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	TileArea roadstop_area(tile, GB(p1, 0, 8), GB(p1, 8, 8));
	TILE_AREA_LOOP(cur_tile, roadstop_area) {
		BuildRoadOutsideStation(cur_tile, dir);
		/* For a drive-through road stop build connecting road for other entrance. */
		if (HasBit(p2, 1)) BuildRoadOutsideStation(cur_tile, ReverseDiagDir(dir));
	}
}

/**
 * Place a new road stop.
 * @param start_tile First tile of the area.
 * @param end_tile Last tile of the area.
 * @param p2 bit 0: 0 For bus stops, 1 for truck stops.
 *           bit 2..3: The roadtypes.
 *           bit 5: Allow stations directly adjacent to other stations.
 * @param cmd Command to use.
 * @see CcRoadStop()
 */
static void PlaceRoadStop(TileIndex start_tile, TileIndex end_tile, uint32 p2, uint32 cmd)
{
	uint8 ddir = _road_station_picker_orientation;
	SB(p2, 16, 16, INVALID_STATION); // no station to join

	if (ddir >= DIAGDIR_END) {
		SetBit(p2, 1); // It's a drive-through stop.
		ddir -= DIAGDIR_END; // Adjust picker result to actual direction.
	}
	p2 |= ddir << 6; // Set the DiagDirecion into p2 bits 6 and 7.

	TileArea ta(start_tile, end_tile);
	CommandContainer cmdcont = { ta.tile, ta.w | ta.h << 8, p2, cmd, CcRoadStop, "" };
	ShowSelectStationIfNeeded(cmdcont, ta);
}

/**
 * Callback for placing a bus station.
 * @param tile Position to place the station.
 */
static void PlaceRoad_BusStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_REMOVE_BUSSTOP);
	} else {
		if (_road_station_picker_orientation < DIAGDIR_END) { // Not a drive-through stop.
			VpStartPlaceSizing(tile, (DiagDirToAxis(_road_station_picker_orientation) == AXIS_X) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_BUSSTOP);
		} else {
			VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_BUILD_BUSSTOP);
		}
		VpSetPlaceSizingLimit(_settings_game.station.station_spread);
	}
}

/**
 * Callback for placing a truck station.
 * @param tile Position to place the station.
 */
static void PlaceRoad_TruckStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_REMOVE_TRUCKSTOP);
	} else {
		if (_road_station_picker_orientation < DIAGDIR_END) { // Not a drive-through stop.
			VpStartPlaceSizing(tile, (DiagDirToAxis(_road_station_picker_orientation) == AXIS_X) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_TRUCKSTOP);
		} else {
			VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_BUILD_TRUCKSTOP);
		}
		VpSetPlaceSizingLimit(_settings_game.station.station_spread);
	}
}

/** Enum referring to the widgets of the build road toolbar */
enum RoadToolbarWidgets {
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


/**
 * Toogles state of the Remove button of Build road toolbar
 * @param w window the button belongs to
 */
static void ToggleRoadButton_Remove(Window *w)
{
	w->ToggleWidgetLoweredState(RTW_REMOVE);
	w->SetWidgetDirty(RTW_REMOVE);
	_remove_button_clicked = w->IsWidgetLowered(RTW_REMOVE);
	SetSelectionRed(_remove_button_clicked);
}

/**
 * Updates the Remove button because of Ctrl state change
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

/** Road toolbar window handler. */
struct BuildRoadToolbarWindow : Window {
	int last_started_action; ///< Last started user action.

	BuildRoadToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		this->SetWidgetsDisabledState(true,
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);

		this->OnInvalidateData();
		this->last_started_action = WIDGET_LIST_END;

		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildRoadToolbarWindow()
	{
		if (_settings_client.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0, false);
	}

	void OnInvalidateData(int data = 0)
	{
		this->SetWidgetsDisabledState(!CanBuildVehicleInfrastructure(VEH_ROAD),
				RTW_DEPOT,
				RTW_BUS_STATION,
				RTW_TRUCK_STATION,
				WIDGET_LIST_END);
	}

	/**
	 * Update the remove button lowered state of the road toolbar
	 *
	 * @param clicked_widget The widget which the client clicked just now
	 */
	void UpdateOptionWidgetStatus(RoadToolbarWidgets clicked_widget)
	{
		/* The remove and the one way button state is driven
		 * by the other buttons so they don't act on themselfs.
		 * Both are only valid if they are able to apply as options. */
		switch (clicked_widget) {
			case RTW_REMOVE:
				this->RaiseWidget(RTW_ONE_WAY);
				this->SetWidgetDirty(RTW_ONE_WAY);
				break;

			case RTW_ONE_WAY:
				this->RaiseWidget(RTW_REMOVE);
				this->SetWidgetDirty(RTW_REMOVE);
				break;

			case RTW_BUS_STATION:
			case RTW_TRUCK_STATION:
				this->DisableWidget(RTW_ONE_WAY);
				this->SetWidgetDisabledState(RTW_REMOVE, !this->IsWidgetLowered(clicked_widget));
				break;

			case RTW_ROAD_X:
			case RTW_ROAD_Y:
			case RTW_AUTOROAD:
				this->SetWidgetsDisabledState(!this->IsWidgetLowered(clicked_widget),
						RTW_REMOVE,
						RTW_ONE_WAY,
						WIDGET_LIST_END);
				break;

			default:
				/* When any other buttons than road/station, raise and
				 * disable the removal button */
				this->SetWidgetsDisabledState(true,
						RTW_REMOVE,
						RTW_ONE_WAY,
						WIDGET_LIST_END);
				this->SetWidgetsLoweredState(false,
						RTW_REMOVE,
						RTW_ONE_WAY,
						WIDGET_LIST_END);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		_remove_button_clicked = false;
		_one_way_button_clicked = false;
		switch (widget) {
			case RTW_ROAD_X:
				HandlePlacePushButton(this, RTW_ROAD_X, _road_type_infos[_cur_roadtype].cursor_nwse, HT_RECT);
				this->last_started_action = widget;
				break;

			case RTW_ROAD_Y:
				HandlePlacePushButton(this, RTW_ROAD_Y, _road_type_infos[_cur_roadtype].cursor_nesw, HT_RECT);
				this->last_started_action = widget;
				break;

			case RTW_AUTOROAD:
				HandlePlacePushButton(this, RTW_AUTOROAD, _road_type_infos[_cur_roadtype].cursor_autoroad, HT_RECT);
				this->last_started_action = widget;
				break;

			case RTW_DEMOLISH:
				HandlePlacePushButton(this, RTW_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT | HT_DIAGONAL);
				this->last_started_action = widget;
				break;

			case RTW_DEPOT:
				if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
				if (HandlePlacePushButton(this, RTW_DEPOT, SPR_CURSOR_ROAD_DEPOT, HT_RECT)) {
					ShowRoadDepotPicker(this);
					this->last_started_action = widget;
				}
				break;

			case RTW_BUS_STATION:
				if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
				if (HandlePlacePushButton(this, RTW_BUS_STATION, SPR_CURSOR_BUS_STATION, HT_RECT)) {
					ShowRVStationPicker(this, ROADSTOP_BUS);
					this->last_started_action = widget;
				}
				break;

			case RTW_TRUCK_STATION:
				if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
				if (HandlePlacePushButton(this, RTW_TRUCK_STATION, SPR_CURSOR_TRUCK_STATION, HT_RECT)) {
					ShowRVStationPicker(this, ROADSTOP_TRUCK);
					this->last_started_action = widget;
				}
				break;

			case RTW_ONE_WAY:
				if (this->IsWidgetDisabled(RTW_ONE_WAY)) return;
				this->SetDirty();
				this->ToggleWidgetLoweredState(RTW_ONE_WAY);
				SetSelectionRed(false);
				break;

			case RTW_BUILD_BRIDGE:
				HandlePlacePushButton(this, RTW_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, HT_RECT);
				this->last_started_action = widget;
				break;

			case RTW_BUILD_TUNNEL:
				HandlePlacePushButton(this, RTW_BUILD_TUNNEL, SPR_CURSOR_ROAD_TUNNEL, HT_SPECIAL);
				this->last_started_action = widget;
				break;

			case RTW_REMOVE:
				if (this->IsWidgetDisabled(RTW_REMOVE)) return;

				DeleteWindowById(WC_SELECT_STATION, 0);
				ToggleRoadButton_Remove(this);
				SndPlayFx(SND_15_BEEP);
				break;

			default: NOT_REACHED();
		}
		this->UpdateOptionWidgetStatus((RoadToolbarWidgets)widget);
		if (_ctrl_pressed) RoadToolbar_CtrlChanged(this);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		int num = CheckHotkeyMatch(roadtoolbar_hotkeys, keycode, this);
		if (num == -1 || this->GetWidget<NWidgetBase>(num) == NULL) return ES_NOT_HANDLED;
		this->OnClick(Point(), num, 1);
		MarkTileDirtyByTile(TileVirtXY(_thd.pos.x, _thd.pos.y)); // redraw tile selection
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_remove_button_clicked = this->IsWidgetLowered(RTW_REMOVE);
		_one_way_button_clicked = this->IsWidgetLowered(RTW_ONE_WAY);
		switch (this->last_started_action) {
			case RTW_ROAD_X:
				_place_road_flag = RF_DIR_X;
				if (_tile_fract_coords.x >= 8) _place_road_flag |= RF_START_HALFROAD_X;
				VpStartPlaceSizing(tile, VPM_FIX_Y, DDSP_PLACE_ROAD_X_DIR);
				break;

			case RTW_ROAD_Y:
				_place_road_flag = RF_DIR_Y;
				if (_tile_fract_coords.y >= 8) _place_road_flag |= RF_START_HALFROAD_Y;
				VpStartPlaceSizing(tile, VPM_FIX_X, DDSP_PLACE_ROAD_Y_DIR);
				break;

			case RTW_AUTOROAD:
				_place_road_flag = RF_NONE;
				if (_tile_fract_coords.x >= 8) _place_road_flag |= RF_START_HALFROAD_X;
				if (_tile_fract_coords.y >= 8) _place_road_flag |= RF_START_HALFROAD_Y;
				VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_PLACE_AUTOROAD);
				break;

			case RTW_DEMOLISH:
				PlaceProc_DemolishArea(tile);
				break;

			case RTW_DEPOT:
				DoCommandP(tile, _cur_roadtype << 2 | _road_depot_orientation, 0,
						CMD_BUILD_ROAD_DEPOT | CMD_MSG(_road_type_infos[_cur_roadtype].err_depot), CcRoadDepot);
				break;

			case RTW_BUS_STATION:
				PlaceRoad_BusStation(tile);
				break;

			case RTW_TRUCK_STATION:
				PlaceRoad_TruckStation(tile);
				break;

			case RTW_BUILD_BRIDGE:
				PlaceRoad_Bridge(tile, this);
				break;

			case RTW_BUILD_TUNNEL:
				DoCommandP(tile, RoadTypeToRoadTypes(_cur_roadtype) | (TRANSPORT_ROAD << 8), 0,
						CMD_BUILD_TUNNEL | CMD_MSG(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE), CcBuildRoadTunnel);
				break;

			default: NOT_REACHED();
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
		this->SetWidgetsDisabledState(true,
				RTW_REMOVE,
				RTW_ONE_WAY,
				WIDGET_LIST_END);
		this->SetWidgetDirty(RTW_REMOVE);
		this->SetWidgetDirty(RTW_ONE_WAY);

		DeleteWindowById(WC_BUS_STATION, TRANSPORT_ROAD);
		DeleteWindowById(WC_TRUCK_STATION, TRANSPORT_ROAD);
		DeleteWindowById(WC_BUILD_DEPOT, TRANSPORT_ROAD);
		DeleteWindowById(WC_SELECT_STATION, 0);
		DeleteWindowByClass(WC_BUILD_BRIDGE);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		/* Here we update the end tile flags
		 * of the road placement actions.
		 * At first we reset the end halfroad
		 * bits and if needed we set them again. */
		switch (select_proc) {
			case DDSP_PLACE_ROAD_X_DIR:
				_place_road_flag &= ~RF_END_HALFROAD_X;
				if (pt.x & 8) _place_road_flag |= RF_END_HALFROAD_X;
				break;

			case DDSP_PLACE_ROAD_Y_DIR:
				_place_road_flag &= ~RF_END_HALFROAD_Y;
				if (pt.y & 8) _place_road_flag |= RF_END_HALFROAD_Y;
				break;

			case DDSP_PLACE_AUTOROAD:
				_place_road_flag &= ~(RF_END_HALFROAD_Y | RF_END_HALFROAD_X);
				if (pt.y & 8) _place_road_flag |= RF_END_HALFROAD_Y;
				if (pt.x & 8) _place_road_flag |= RF_END_HALFROAD_X;

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

		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1) {
			switch (select_proc) {
				default: NOT_REACHED();
				case DDSP_BUILD_BRIDGE:
					if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
					ShowBuildBridgeWindow(start_tile, end_tile, TRANSPORT_ROAD, RoadTypeToRoadTypes(_cur_roadtype));
					break;

				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;

				case DDSP_PLACE_ROAD_X_DIR:
				case DDSP_PLACE_ROAD_Y_DIR:
				case DDSP_PLACE_AUTOROAD:
					/* Flag description:
					 * Use the first three bits (0x07) if dir == Y
					 * else use the last 2 bits (X dir has
					 * not the 3rd bit set) */
					_place_road_flag = (RoadFlags)((_place_road_flag & RF_DIR_Y) ? (_place_road_flag & 0x07) : (_place_road_flag >> 3));

					DoCommandP(start_tile, end_tile, _place_road_flag | (_cur_roadtype << 3) | (_one_way_button_clicked << 5),
							_remove_button_clicked ?
							CMD_REMOVE_LONG_ROAD | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_road) :
							CMD_BUILD_LONG_ROAD | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_road), CcPlaySound1D);
					break;

				case DDSP_BUILD_BUSSTOP:
					PlaceRoadStop(start_tile, end_tile, (_ctrl_pressed << 5) | RoadTypeToRoadTypes(_cur_roadtype) << 2 | ROADSTOP_BUS, CMD_BUILD_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_station[ROADSTOP_BUS]));
					break;

				case DDSP_BUILD_TRUCKSTOP:
					PlaceRoadStop(start_tile, end_tile, (_ctrl_pressed << 5) | RoadTypeToRoadTypes(_cur_roadtype) << 2 | ROADSTOP_TRUCK, CMD_BUILD_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_station[ROADSTOP_TRUCK]));
					break;

				case DDSP_REMOVE_BUSSTOP: {
					TileArea ta(start_tile, end_tile);
					DoCommandP(ta.tile, ta.w | ta.h << 8, ROADSTOP_BUS, CMD_REMOVE_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_station[ROADSTOP_BUS]), CcPlaySound1D);
					break;
				}

				case DDSP_REMOVE_TRUCKSTOP: {
					TileArea ta(start_tile, end_tile);
					DoCommandP(ta.tile, ta.w | ta.h << 8, ROADSTOP_TRUCK, CMD_REMOVE_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_station[ROADSTOP_TRUCK]), CcPlaySound1D);
					break;
				}
			}
		}
	}

	virtual void OnPlacePresize(Point pt, TileIndex tile)
	{
		DoCommand(tile, RoadTypeToRoadTypes(_cur_roadtype) | (TRANSPORT_ROAD << 8), 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
	}

	virtual EventState OnCTRLStateChange()
	{
		if (RoadToolbar_CtrlChanged(this)) return ES_HANDLED;
		return ES_NOT_HANDLED;
	}

	static Hotkey<BuildRoadToolbarWindow> roadtoolbar_hotkeys[];
};

Hotkey<BuildRoadToolbarWindow> BuildRoadToolbarWindow::roadtoolbar_hotkeys[] = {
	Hotkey<BuildRoadToolbarWindow>('1', "build_x", RTW_ROAD_X),
	Hotkey<BuildRoadToolbarWindow>('2', "build_y", RTW_ROAD_Y),
	Hotkey<BuildRoadToolbarWindow>('3', "autoroad", RTW_AUTOROAD),
	Hotkey<BuildRoadToolbarWindow>('4', "demolish", RTW_DEMOLISH),
	Hotkey<BuildRoadToolbarWindow>('5', "depot", RTW_DEPOT),
	Hotkey<BuildRoadToolbarWindow>('6', "bus_station", RTW_BUS_STATION),
	Hotkey<BuildRoadToolbarWindow>('7', "truck_station", RTW_TRUCK_STATION),
	Hotkey<BuildRoadToolbarWindow>('8', "oneway", RTW_ONE_WAY),
	Hotkey<BuildRoadToolbarWindow>('B', "bridge", RTW_BUILD_BRIDGE),
	Hotkey<BuildRoadToolbarWindow>('T', "tunnel", RTW_BUILD_TUNNEL),
	Hotkey<BuildRoadToolbarWindow>('R', "remove", RTW_REMOVE),
	HOTKEY_LIST_END(BuildRoadToolbarWindow)
};
Hotkey<BuildRoadToolbarWindow> *_roadtoolbar_hotkeys = BuildRoadToolbarWindow::roadtoolbar_hotkeys;


static const NWidgetPart _nested_build_road_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_ROAD_TOOLBAR_ROAD_CONSTRUCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOROAD, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEPOT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_DEPOT, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_VEHICLE_DEPOT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUS_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUS_STATION, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_BUS_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_TRUCK_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRUCK_BAY, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRUCK_LOADING_BAY),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ONE_WAY),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_ONE_WAY, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_ONE_WAY_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_ROAD),
	EndContainer(),
};

static const WindowDesc _build_road_desc(
	WDP_ALIGN_TOOLBAR, 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_road_widgets, lengthof(_nested_build_road_widgets)
);

static const NWidgetPart _nested_build_tramway_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_ROAD_TOOLBAR_TRAM_CONSTRUCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOTRAM, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOTRAM),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEPOT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_DEPOT, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAM_VEHICLE_DEPOT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUS_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUS_STATION, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_PASSENGER_TRAM_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_TRUCK_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRUCK_BAY, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_CARGO_TRAM_STATION),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, RTW_ONE_WAY), SetMinimalSize(0, 0),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_TRAMWAYS),
	EndContainer(),
};

static const WindowDesc _build_tramway_desc(
	WDP_ALIGN_TOOLBAR, 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_tramway_widgets, lengthof(_nested_build_tramway_widgets)
);

/**
 * Open the build road toolbar window
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @return newly opened road toolbar, or NULL if the toolbar could not be opened.
 */
Window *ShowBuildRoadToolbar(RoadType roadtype)
{
	if (!Company::IsValidID(_local_company)) return NULL;
	_cur_roadtype = roadtype;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	return AllocateWindowDescFront<BuildRoadToolbarWindow>(roadtype == ROADTYPE_ROAD ? &_build_road_desc : &_build_tramway_desc, TRANSPORT_ROAD);
}

EventState RoadToolbarGlobalHotkeys(uint16 key, uint16 keycode)
{
	extern RoadType _last_built_roadtype;
	int num = CheckHotkeyMatch<BuildRoadToolbarWindow>(_roadtoolbar_hotkeys, keycode, NULL, true);
	if (num == -1) return ES_NOT_HANDLED;
	Window *w = ShowBuildRoadToolbar(_last_built_roadtype);
	if (w == NULL) return ES_NOT_HANDLED;
	return w->OnKeyPress(key, keycode);
}

static const NWidgetPart _nested_build_road_scen_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_ROAD_TOOLBAR_ROAD_CONSTRUCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOROAD, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ONE_WAY),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_ONE_WAY, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_ONE_WAY_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_ROAD),
	EndContainer(),
};

static const WindowDesc _build_road_scen_desc(
	WDP_AUTO, 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_road_scen_widgets, lengthof(_nested_build_road_scen_widgets)
);

Window *ShowBuildRoadScenToolbar()
{
	_cur_roadtype = ROADTYPE_ROAD;
	return AllocateWindowDescFront<BuildRoadToolbarWindow>(&_build_road_scen_desc, 0);
}

EventState RoadToolbarEditorGlobalHotkeys(uint16 key, uint16 keycode)
{
	int num = CheckHotkeyMatch<BuildRoadToolbarWindow>(_roadtoolbar_hotkeys, keycode, NULL, true);
	if (num == -1) return ES_NOT_HANDLED;
	Window *w = ShowBuildRoadScenToolbar();
	if (w == NULL) return ES_NOT_HANDLED;
	return w->OnKeyPress(key, keycode);
}

/** Enum referring to the widgets of the build road depot window */
enum BuildRoadDepotWidgets {
	BRDW_CAPTION,
	BRDW_DEPOT_NE,
	BRDW_DEPOT_SE,
	BRDW_DEPOT_SW,
	BRDW_DEPOT_NW,
};

struct BuildRoadDepotWindow : public PickerWindowBase {
	BuildRoadDepotWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(parent)
	{
		this->CreateNestedTree(desc);

		this->LowerWidget(_road_depot_orientation + BRDW_DEPOT_NE);
		if ( _cur_roadtype == ROADTYPE_TRAM) {
			this->GetWidget<NWidgetCore>(BRDW_CAPTION)->widget_data = STR_BUILD_DEPOT_TRAM_ORIENTATION_CAPTION;
			for (int i = BRDW_DEPOT_NE; i <= BRDW_DEPOT_NW; i++) this->GetWidget<NWidgetCore>(i)->tool_tip = STR_BUILD_DEPOT_TRAM_ORIENTATION_SELECT_TOOLTIP;
		}

		this->FinishInitNested(desc, TRANSPORT_ROAD);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (!IsInsideMM(widget, BRDW_DEPOT_NE, BRDW_DEPOT_NW + 1)) return;

		DrawRoadDepotSprite(r.left - 1, r.top, (DiagDirection)(widget - BRDW_DEPOT_NE + DIAGDIR_NE), _cur_roadtype);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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

static const NWidgetPart _nested_build_road_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, BRDW_CAPTION), SetDataTip(STR_BUILD_DEPOT_ROAD_ORIENTATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL_LTR),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0), SetFill(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_GREY, BRDW_DEPOT_NW), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(WWT_PANEL, COLOUR_GREY, BRDW_DEPOT_SW), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_GREY, BRDW_DEPOT_NE), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(WWT_PANEL, COLOUR_GREY, BRDW_DEPOT_SE), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
	EndContainer(),
};

static const WindowDesc _build_road_depot_desc(
	WDP_AUTO, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_road_depot_widgets, lengthof(_nested_build_road_depot_widgets)
);

static void ShowRoadDepotPicker(Window *parent)
{
	new BuildRoadDepotWindow(&_build_road_depot_desc, parent);
}

/** Enum referring to the widgets of the build road station window */
enum BuildRoadStationWidgets {
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

struct BuildRoadStationWindow : public PickerWindowBase {
	BuildRoadStationWindow(const WindowDesc *desc, Window *parent, RoadStopType rs) : PickerWindowBase(parent)
	{
		this->CreateNestedTree(desc);

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

		this->GetWidget<NWidgetCore>(BRSW_CAPTION)->widget_data = _road_type_infos[_cur_roadtype].picker_title[rs];
		for (uint i = BRSW_STATION_NE; i < BRSW_LT_OFF; i++) this->GetWidget<NWidgetCore>(i)->tool_tip = _road_type_infos[_cur_roadtype].picker_tooltip[rs];

		this->LowerWidget(_road_station_picker_orientation + BRSW_STATION_NE);
		this->LowerWidget(_settings_client.gui.station_show_coverage + BRSW_LT_OFF);

		this->FinishInitNested(desc, TRANSPORT_ROAD);

		this->window_class = (rs == ROADSTOP_BUS) ? WC_BUS_STATION : WC_TRUCK_STATION;
	}

	virtual ~BuildRoadStationWindow()
	{
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		int rad = _settings_game.station.modified_catchment ? CA_TRUCK /* = CA_BUS */ : CA_UNMODIFIED;
		if (_settings_client.gui.station_show_coverage) {
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		/* 'Accepts' and 'Supplies' texts. */
		StationCoverageType sct = (this->window_class == WC_BUS_STATION) ? SCT_PASSENGERS_ONLY : SCT_NON_PASSENGERS_ONLY;
		int top = this->GetWidget<NWidgetBase>(BRSW_LT_ON)->pos_y + this->GetWidget<NWidgetBase>(BRSW_LT_ON)->current_y + WD_PAR_VSEP_NORMAL;
		NWidgetBase *back_nwi = this->GetWidget<NWidgetBase>(BRSW_BACKGROUND);
		int right = back_nwi->pos_x +  back_nwi->current_x;
		int bottom = back_nwi->pos_y +  back_nwi->current_y;
		top = DrawStationCoverageAreaText(back_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, sct, rad, false) + WD_PAR_VSEP_NORMAL;
		top = DrawStationCoverageAreaText(back_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, sct, rad, true) + WD_PAR_VSEP_NORMAL;
		/* Resize background if the text is not equally long as the window. */
		if (top > bottom || (top < bottom && back_nwi->current_y > back_nwi->smallest_y)) {
			ResizeWindow(this, 0, top - bottom);
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (!IsInsideMM(widget, BRSW_STATION_NE, BRSW_STATION_Y + 1)) return;

		StationType st = (this->window_class == WC_BUS_STATION) ? STATION_BUS : STATION_TRUCK;
		StationPickerDrawSprite(r.left + TILE_PIXELS, r.bottom - TILE_PIXELS, st, INVALID_RAILTYPE, widget < BRSW_STATION_X ? ROADTYPE_ROAD : _cur_roadtype, widget - BRSW_STATION_NE);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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
				DeleteWindowById(WC_SELECT_STATION, 0);
				break;

			case BRSW_LT_OFF:
			case BRSW_LT_ON:
				this->RaiseWidget(_settings_client.gui.station_show_coverage + BRSW_LT_OFF);
				_settings_client.gui.station_show_coverage = (widget != BRSW_LT_OFF);
				this->LowerWidget(_settings_client.gui.station_show_coverage + BRSW_LT_OFF);
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

/** Widget definition of the build road station window */
static const NWidgetPart _nested_rv_station_picker_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION,  COLOUR_DARK_GREEN, BRSW_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BRSW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL), SetPIP(0, 2, 0),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_NW), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_NE), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_X),  SetMinimalSize(66, 50), EndContainer(),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL), SetPIP(0, 2, 0),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_SW), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_SE), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_Y),  SetMinimalSize(66, 50), EndContainer(),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BRSW_INFO), SetMinimalSize(140, 14), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
			NWidget(NWID_SPACER), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BRSW_LT_OFF), SetMinimalSize(60, 12),
											SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BRSW_LT_ON), SetMinimalSize(60, 12),
											SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
			NWidget(NWID_SPACER), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10), SetResize(0, 1),
	EndContainer(),
};

static const WindowDesc _rv_station_picker_desc(
	WDP_AUTO, 0, 0,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_rv_station_picker_widgets, lengthof(_nested_rv_station_picker_widgets)
);

static void ShowRVStationPicker(Window *parent, RoadStopType rs)
{
	new BuildRoadStationWindow(&_rv_station_picker_desc, parent, rs);
}

void InitializeRoadGui()
{
	_road_depot_orientation = DIAGDIR_NW;
	_road_station_picker_orientation = DIAGDIR_NW;
}
