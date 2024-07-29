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
#include "waypoint_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "tunnelbridge.h"
#include "tunnelbridge_map.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "hotkeys.h"
#include "road_gui.h"
#include "zoom_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "engine_base.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "strings_func.h"
#include "core/geometry_func.hpp"
#include "station_cmd.h"
#include "waypoint_cmd.h"
#include "road_cmd.h"
#include "tunnelbridge_cmd.h"
#include "newgrf_roadstop.h"
#include "picker_gui.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "depot_func.h"

#include "widgets/road_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static void ShowRVStationPicker(Window *parent, RoadStopType rs);
static void ShowRoadDepotPicker(Window *parent);
static void ShowBuildRoadWaypointPicker(Window *parent);

static bool _remove_button_clicked;
static bool _one_way_button_clicked;

static Axis _place_road_dir;
static bool _place_road_start_half_x;
static bool _place_road_start_half_y;
static bool _place_road_end_half;

static RoadType _cur_roadtype;

static DiagDirection _road_depot_orientation;

struct RoadWaypointPickerSelection {
	RoadStopClassID sel_class; ///< Selected road waypoint class.
	uint16_t sel_type; ///< Selected road waypoint type within the class.
};
static RoadWaypointPickerSelection _waypoint_gui; ///< Settings of the road waypoint picker.

struct RoadStopPickerSelection {
	RoadStopClassID sel_class; ///< Selected road stop class.
	uint16_t sel_type; ///< Selected road stop type within the class.
	DiagDirection orientation; ///< Selected orientation of the road stop.
};
static RoadStopPickerSelection _roadstop_gui;

static bool IsRoadStopEverAvailable(const RoadStopSpec *spec, StationType type)
{
	if (spec == nullptr) return true;

	if (HasBit(spec->flags, RSF_BUILD_MENU_ROAD_ONLY) && !RoadTypeIsRoad(_cur_roadtype)) return false;
	if (HasBit(spec->flags, RSF_BUILD_MENU_TRAM_ONLY) && !RoadTypeIsTram(_cur_roadtype)) return false;

	switch (spec->stop_type) {
		case ROADSTOPTYPE_ALL: return true;
		case ROADSTOPTYPE_PASSENGER: return type == STATION_BUS;
		case ROADSTOPTYPE_FREIGHT: return type == STATION_TRUCK;
		default: NOT_REACHED();
	}
}

/**
 * Check whether a road stop type can be built.
 * @return true if building is allowed.
 */
static bool IsRoadStopAvailable(const RoadStopSpec *spec, StationType type)
{
	if (spec == nullptr) return true;
	if (IsRoadStopEverAvailable(spec, type)) return true;

	if (!HasBit(spec->callback_mask, CBM_ROAD_STOP_AVAIL)) return true;

	uint16_t cb_res = GetRoadStopCallback(CBID_STATION_AVAILABILITY, 0, 0, spec, nullptr, INVALID_TILE, _cur_roadtype, type, 0);
	if (cb_res == CALLBACK_FAILED) return true;

	return Convert8bitBooleanCallback(spec->grf_prop.grffile, CBID_STATION_AVAILABILITY, cb_res);
}

void CcPlaySound_CONSTRUCTION_OTHER(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Succeeded() && _settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, tile);
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
		w->OnPlaceMouseUp(VPM_X_OR_Y, DDSP_BUILD_BRIDGE, pt, other_tile, tile);
	} else {
		VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
	}
}

/**
 * Callback executed after a build road tunnel command has been called.
 *
 * @param result Whether the build succeeded.
 * @param start_tile Starting tile of the tunnel.
 */
void CcBuildRoadTunnel(Commands, const CommandCost &result, TileIndex start_tile)
{
	if (result.Succeeded()) {
		if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, start_tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();

		DiagDirection start_direction = ReverseDiagDir(GetTunnelBridgeDirection(start_tile));
		ConnectRoadToStructure(start_tile, start_direction);

		TileIndex end_tile = GetOtherTunnelBridgeEnd(start_tile);
		DiagDirection end_direction = ReverseDiagDir(GetTunnelBridgeDirection(end_tile));
		ConnectRoadToStructure(end_tile, end_direction);
	} else {
		SetRedErrorSquare(_build_tunnel_endtile);
	}
}

/**
 * If required, connects a new structure to an existing road or tram by building the missing roadbit.
 * @param tile Tile containing the structure to connect.
 * @param direction Direction to check.
 */
void ConnectRoadToStructure(TileIndex tile, DiagDirection direction)
{
	tile += TileOffsByDiagDir(direction);
	/* if there is a roadpiece just outside of the station entrance, build a connecting route */
	if (IsNormalRoadTile(tile)) {
		if (GetRoadBits(tile, GetRoadTramType(_cur_roadtype)) != ROAD_NONE) {
			Command<CMD_BUILD_ROAD>::Post(tile, DiagDirToRoadBits(ReverseDiagDir(direction)), _cur_roadtype, DRD_NONE, 0);
		}
	}
}

void CcRoadDepot(Commands, const CommandCost &result, TileIndex start_tile, RoadType, DiagDirection dir, bool, DepotID, TileIndex end_tile)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, start_tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();

	TileArea ta(start_tile, end_tile);
	for (TileIndex tile : ta) {
		ConnectRoadToStructure(tile, dir);
	}
}

/**
 * Command callback for building road stops.
 * @param result Result of the build road stop command.
 * @param tile Start tile.
 * @param width Width of the road stop.
 * @param length Length of the road stop.
 * @param is_drive_through False for normal stops, true for drive-through.
 * @param dir Entrance direction (#DiagDirection) for normal stops. Converted to the axis for drive-through stops.
 * @param spec_class Road stop spec class.
 * @param spec_index Road stop spec index.
 * @see CmdBuildRoadStop
 */
void CcRoadStop(Commands, const CommandCost &result, TileIndex tile, uint8_t width, uint8_t length, RoadStopType, bool is_drive_through,
		DiagDirection dir, RoadType, RoadStopClassID spec_class, uint16_t spec_index, StationID, bool)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();

	bool connect_to_road = true;
	if ((uint)spec_class < RoadStopClass::GetClassCount() && spec_index < RoadStopClass::Get(spec_class)->GetSpecCount()) {
		const RoadStopSpec *roadstopspec = RoadStopClass::Get(spec_class)->GetSpec(spec_index);
		if (roadstopspec != nullptr && HasBit(roadstopspec->flags, RSF_NO_AUTO_ROAD_CONNECTION)) connect_to_road = false;
	}

	if (connect_to_road) {
		TileArea roadstop_area(tile, width, length);
		for (TileIndex cur_tile : roadstop_area) {
			ConnectRoadToStructure(cur_tile, dir);
			/* For a drive-through road stop build connecting road for other entrance. */
			if (is_drive_through) ConnectRoadToStructure(cur_tile, ReverseDiagDir(dir));
		}
	}
}

/**
 * Place a new road stop.
 * @param start_tile First tile of the area.
 * @param end_tile Last tile of the area.
 * @param stop_type Type of stop (bus/truck).
 * @param adjacent Allow stations directly adjacent to other stations.
 * @param rt The roadtypes.
 * @param err_msg Error message to show.
 * @see CcRoadStop()
 */
static void PlaceRoadStop(TileIndex start_tile, TileIndex end_tile, RoadStopType stop_type, bool adjacent, RoadType rt, StringID err_msg)
{
	TileArea ta(start_tile, end_tile);
	DiagDirection ddir = _roadstop_gui.orientation;
	bool drive_through = ddir >= DIAGDIR_END;
	if (drive_through) ddir = static_cast<DiagDirection>(ddir - DIAGDIR_END); // Adjust picker result to actual direction.
	RoadStopClassID spec_class = _roadstop_gui.sel_class;
	uint16_t spec_index = _roadstop_gui.sel_type;

	auto proc = [=](bool test, StationID to_join) -> bool {
		if (test) {
			return Command<CMD_BUILD_ROAD_STOP>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_ROAD_STOP>()), ta.tile, ta.w, ta.h, stop_type, drive_through,
					ddir, rt, spec_class, spec_index, INVALID_STATION, adjacent).Succeeded();
		} else {
			return Command<CMD_BUILD_ROAD_STOP>::Post(err_msg, CcRoadStop, ta.tile, ta.w, ta.h, stop_type, drive_through,
					ddir, rt, spec_class, spec_index, to_join, adjacent);
		}
	};

	ShowSelectStationIfNeeded(ta, proc);
}

/**
 * Place a road waypoint.
 * @param tile Position to start dragging a waypoint.
 */
static void PlaceRoad_Waypoint(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_REMOVE_ROAD_WAYPOINT);
		return;
	}

	Axis axis = GetAxisForNewRoadWaypoint(tile);
	if (IsValidAxis(axis)) {
		/* Valid tile for waypoints */
		VpStartPlaceSizing(tile, axis == AXIS_X ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_ROAD_WAYPOINT);
		VpSetPlaceSizingLimit(_settings_game.station.station_spread);
	} else {
		/* Tile where we can't build road waypoints. This is always going to fail,
		 * but provides the user with a proper error message. */
		Command<CMD_BUILD_ROAD_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_ROAD_WAYPOINT, tile, AXIS_X, 1, 1, ROADSTOP_CLASS_WAYP, 0, INVALID_STATION, false);
	}
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
		if (_roadstop_gui.orientation < DIAGDIR_END) { // Not a drive-through stop.
			VpStartPlaceSizing(tile, (DiagDirToAxis(_roadstop_gui.orientation) == AXIS_X) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_BUSSTOP);
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
		if (_roadstop_gui.orientation < DIAGDIR_END) { // Not a drive-through stop.
			VpStartPlaceSizing(tile, (DiagDirToAxis(_roadstop_gui.orientation) == AXIS_X) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_TRUCKSTOP);
		} else {
			VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_BUILD_TRUCKSTOP);
		}
		VpSetPlaceSizingLimit(_settings_game.station.station_spread);
	}
}

typedef void OnButtonClick(Window *w);

/**
 * Toggles state of the Remove button of Build road toolbar
 * @param w window the button belongs to
 */
static void ToggleRoadButton_Remove(Window *w)
{
	w->ToggleWidgetLoweredState(WID_ROT_REMOVE);
	w->SetWidgetDirty(WID_ROT_REMOVE);
	_remove_button_clicked = w->IsWidgetLowered(WID_ROT_REMOVE);
	SetSelectionRed(_remove_button_clicked);
}

/**
 * Updates the Remove button because of Ctrl state change
 * @param w window the button belongs to
 * @return true iff the remove button was changed
 */
static bool RoadToolbar_CtrlChanged(Window *w)
{
	if (w->IsWidgetDisabled(WID_ROT_REMOVE)) return false;

	/* allow ctrl to switch remove mode only for these widgets */
	for (WidgetID i = WID_ROT_ROAD_X; i <= WID_ROT_AUTOROAD; i++) {
		if (w->IsWidgetLowered(i)) {
			ToggleRoadButton_Remove(w);
			return true;
		}
	}

	return false;
}

/** Road toolbar window handler. */
struct BuildRoadToolbarWindow : Window {
	RoadType roadtype;          ///< Road type to build.
	const RoadTypeInfo *rti;    ///< Information about current road type
	int last_started_action;    ///< Last started user action.

	BuildRoadToolbarWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->Initialize(_cur_roadtype);
		this->CreateNestedTree();
		this->SetupRoadToolbar();
		this->FinishInitNested(window_number);
		this->SetWidgetDisabledState(WID_ROT_REMOVE, true);

		if (RoadTypeIsRoad(this->roadtype)) {
			this->SetWidgetDisabledState(WID_ROT_ONE_WAY, true);
		}

		this->OnInvalidateData();
		this->last_started_action = INVALID_WID_ROT;

		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (_game_mode == GM_NORMAL && (this->IsWidgetLowered(WID_ROT_BUS_STATION) || this->IsWidgetLowered(WID_ROT_TRUCK_STATION))) SetViewportCatchmentStation(nullptr, true);
		if (_settings_client.gui.link_terraform_toolbar) CloseWindowById(WC_SCEN_LAND_GEN, 0, false);
		if (_game_mode == GM_NORMAL && this->IsWidgetLowered(WID_ROT_DEPOT)) SetViewportHighlightDepot(INVALID_DEPOT, true);
		this->Window::Close();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		RoadTramType rtt = GetRoadTramType(this->roadtype);

		bool can_build = CanBuildVehicleInfrastructure(VEH_ROAD, rtt);
		this->SetWidgetsDisabledState(!can_build,
			WID_ROT_DEPOT,
			WID_ROT_BUILD_WAYPOINT,
			WID_ROT_BUS_STATION,
			WID_ROT_TRUCK_STATION);
		if (!can_build) {
			CloseWindowById(WC_BUS_STATION, TRANSPORT_ROAD);
			CloseWindowById(WC_TRUCK_STATION, TRANSPORT_ROAD);
			CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_ROAD);
			CloseWindowById(WC_BUILD_WAYPOINT, TRANSPORT_ROAD);
		}

		if (_game_mode != GM_EDITOR) {
			if (!can_build) {
				/* Show in the tooltip why this button is disabled. */
				this->GetWidget<NWidgetCore>(WID_ROT_DEPOT)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_ROT_BUILD_WAYPOINT)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_ROT_BUS_STATION)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_ROT_TRUCK_STATION)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
			} else {
				this->GetWidget<NWidgetCore>(WID_ROT_DEPOT)->SetToolTip(rtt == RTT_ROAD ? STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_VEHICLE_DEPOT : STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAM_VEHICLE_DEPOT);
				this->GetWidget<NWidgetCore>(WID_ROT_BUILD_WAYPOINT)->SetToolTip(rtt == RTT_ROAD ? STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_ROAD_TO_WAYPOINT : STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_TRAM_TO_WAYPOINT);
				this->GetWidget<NWidgetCore>(WID_ROT_BUS_STATION)->SetToolTip(rtt == RTT_ROAD ? STR_ROAD_TOOLBAR_TOOLTIP_BUILD_BUS_STATION : STR_ROAD_TOOLBAR_TOOLTIP_BUILD_PASSENGER_TRAM_STATION);
				this->GetWidget<NWidgetCore>(WID_ROT_TRUCK_STATION)->SetToolTip(rtt == RTT_ROAD ? STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRUCK_LOADING_BAY : STR_ROAD_TOOLBAR_TOOLTIP_BUILD_CARGO_TRAM_STATION);
			}
		}
	}

	void Initialize(RoadType roadtype)
	{
		assert(roadtype < ROADTYPE_END);
		this->roadtype = roadtype;
		this->rti = GetRoadTypeInfo(this->roadtype);
	}

	/**
	 * Configures the road toolbar for roadtype given
	 * @param roadtype the roadtype to display
	 */
	void SetupRoadToolbar()
	{
		this->GetWidget<NWidgetCore>(WID_ROT_ROAD_X)->widget_data = rti->gui_sprites.build_x_road;
		this->GetWidget<NWidgetCore>(WID_ROT_ROAD_Y)->widget_data = rti->gui_sprites.build_y_road;
		this->GetWidget<NWidgetCore>(WID_ROT_AUTOROAD)->widget_data = rti->gui_sprites.auto_road;
		if (_game_mode != GM_EDITOR) {
			this->GetWidget<NWidgetCore>(WID_ROT_DEPOT)->widget_data = rti->gui_sprites.build_depot;
		}
		this->GetWidget<NWidgetCore>(WID_ROT_CONVERT_ROAD)->widget_data = rti->gui_sprites.convert_road;
		this->GetWidget<NWidgetCore>(WID_ROT_BUILD_TUNNEL)->widget_data = rti->gui_sprites.build_tunnel;
	}

	/**
	 * Switch to another road type.
	 * @param roadtype New road type.
	 */
	void ModifyRoadType(RoadType roadtype)
	{
		this->Initialize(roadtype);
		this->SetupRoadToolbar();
		this->ReInit();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_ROT_CAPTION) {
			if (this->rti->max_speed > 0) {
				SetDParam(0, STR_TOOLBAR_RAILTYPE_VELOCITY);
				SetDParam(1, this->rti->strings.toolbar_caption);
				SetDParam(2, PackVelocity(this->rti->max_speed / 2, VEH_ROAD));
			} else {
				SetDParam(0, this->rti->strings.toolbar_caption);
			}
		}
	}

	/**
	 * Update the remove button lowered state of the road toolbar
	 *
	 * @param clicked_widget The widget which the client clicked just now
	 */
	void UpdateOptionWidgetStatus(RoadToolbarWidgets clicked_widget)
	{
		/* The remove and the one way button state is driven
		 * by the other buttons so they don't act on themselves.
		 * Both are only valid if they are able to apply as options. */
		switch (clicked_widget) {
			case WID_ROT_REMOVE:
				if (RoadTypeIsRoad(this->roadtype)) {
					this->RaiseWidget(WID_ROT_ONE_WAY);
					this->SetWidgetDirty(WID_ROT_ONE_WAY);
				}

				break;

			case WID_ROT_ONE_WAY:
				this->RaiseWidget(WID_ROT_REMOVE);
				this->SetWidgetDirty(WID_ROT_REMOVE);
				break;

			case WID_ROT_BUS_STATION:
			case WID_ROT_TRUCK_STATION:
			case WID_ROT_BUILD_WAYPOINT:
				if (RoadTypeIsRoad(this->roadtype)) this->DisableWidget(WID_ROT_ONE_WAY);
				this->SetWidgetDisabledState(WID_ROT_REMOVE, !this->IsWidgetLowered(clicked_widget));
				break;

			case WID_ROT_ROAD_X:
			case WID_ROT_ROAD_Y:
			case WID_ROT_AUTOROAD:
				this->SetWidgetDisabledState(WID_ROT_REMOVE, !this->IsWidgetLowered(clicked_widget));
				if (RoadTypeIsRoad(this->roadtype)) {
					this->SetWidgetDisabledState(WID_ROT_ONE_WAY, !this->IsWidgetLowered(clicked_widget));
				}
				break;

			default:
				/* When any other buttons than road/station, raise and
				 * disable the removal button */
				this->SetWidgetDisabledState(WID_ROT_REMOVE, true);
				this->SetWidgetLoweredState(WID_ROT_REMOVE, false);

				if (RoadTypeIsRoad(this->roadtype)) {
					this->SetWidgetDisabledState(WID_ROT_ONE_WAY, true);
					this->SetWidgetLoweredState(WID_ROT_ONE_WAY, false);
				}

				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		_remove_button_clicked = false;
		_one_way_button_clicked = false;
		switch (widget) {
			case WID_ROT_ROAD_X:
				HandlePlacePushButton(this, WID_ROT_ROAD_X, this->rti->cursor.road_nwse, HT_RECT);
				this->last_started_action = widget;
				break;

			case WID_ROT_ROAD_Y:
				HandlePlacePushButton(this, WID_ROT_ROAD_Y, this->rti->cursor.road_swne, HT_RECT);
				this->last_started_action = widget;
				break;

			case WID_ROT_AUTOROAD:
				HandlePlacePushButton(this, WID_ROT_AUTOROAD, this->rti->cursor.autoroad, HT_RECT);
				this->last_started_action = widget;
				break;

			case WID_ROT_DEMOLISH:
				HandlePlacePushButton(this, WID_ROT_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT | HT_DIAGONAL);
				this->last_started_action = widget;
				break;

			case WID_ROT_DEPOT:
				if (HandlePlacePushButton(this, WID_ROT_DEPOT, this->rti->cursor.depot, HT_RECT)) {
					ShowRoadDepotPicker(this);
					this->last_started_action = widget;
				}
				break;

			case WID_ROT_BUILD_WAYPOINT:
				this->last_started_action = widget;
				if (HandlePlacePushButton(this, WID_ROT_BUILD_WAYPOINT, SPR_CURSOR_WAYPOINT, HT_RECT)) {
					ShowBuildRoadWaypointPicker(this);
				}
				break;

			case WID_ROT_BUS_STATION:
				if (HandlePlacePushButton(this, WID_ROT_BUS_STATION, SPR_CURSOR_BUS_STATION, HT_RECT)) {
					ShowRVStationPicker(this, ROADSTOP_BUS);
					this->last_started_action = widget;
				}
				break;

			case WID_ROT_TRUCK_STATION:
				if (HandlePlacePushButton(this, WID_ROT_TRUCK_STATION, SPR_CURSOR_TRUCK_STATION, HT_RECT)) {
					ShowRVStationPicker(this, ROADSTOP_TRUCK);
					this->last_started_action = widget;
				}
				break;

			case WID_ROT_ONE_WAY:
				if (this->IsWidgetDisabled(WID_ROT_ONE_WAY)) return;
				this->SetDirty();
				this->ToggleWidgetLoweredState(WID_ROT_ONE_WAY);
				SetSelectionRed(false);
				break;

			case WID_ROT_BUILD_BRIDGE:
				HandlePlacePushButton(this, WID_ROT_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, HT_RECT);
				this->last_started_action = widget;
				break;

			case WID_ROT_BUILD_TUNNEL:
				HandlePlacePushButton(this, WID_ROT_BUILD_TUNNEL, this->rti->cursor.tunnel, HT_SPECIAL);
				this->last_started_action = widget;
				break;

			case WID_ROT_REMOVE:
				if (this->IsWidgetDisabled(WID_ROT_REMOVE)) return;

				CloseWindowById(WC_SELECT_STATION, 0);
				ToggleRoadButton_Remove(this);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				break;

			case WID_ROT_CONVERT_ROAD:
				HandlePlacePushButton(this, WID_ROT_CONVERT_ROAD, this->rti->cursor.convert_road, HT_RECT);
				this->last_started_action = widget;
				break;

			default: NOT_REACHED();
		}
		this->UpdateOptionWidgetStatus((RoadToolbarWidgets)widget);
		if (_ctrl_pressed) RoadToolbar_CtrlChanged(this);
	}

	EventState OnHotkey(int hotkey) override
	{
		MarkTileDirtyByTile(TileVirtXY(_thd.pos.x, _thd.pos.y)); // redraw tile selection
		return Window::OnHotkey(hotkey);
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		_remove_button_clicked = this->IsWidgetLowered(WID_ROT_REMOVE);
		_one_way_button_clicked = RoadTypeIsRoad(this->roadtype) ? this->IsWidgetLowered(WID_ROT_ONE_WAY) : false;
		switch (this->last_started_action) {
			case WID_ROT_ROAD_X:
				_place_road_dir = AXIS_X;
				_place_road_start_half_x = _tile_fract_coords.x >= 8;
				VpStartPlaceSizing(tile, VPM_FIX_Y, DDSP_PLACE_ROAD_X_DIR);
				break;

			case WID_ROT_ROAD_Y:
				_place_road_dir = AXIS_Y;
				_place_road_start_half_y = _tile_fract_coords.y >= 8;
				VpStartPlaceSizing(tile, VPM_FIX_X, DDSP_PLACE_ROAD_Y_DIR);
				break;

			case WID_ROT_AUTOROAD:
				_place_road_dir = INVALID_AXIS;
				_place_road_start_half_x = _tile_fract_coords.x >= 8;
				_place_road_start_half_y = _tile_fract_coords.y >= 8;
				VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_PLACE_AUTOROAD);
				break;

			case WID_ROT_DEMOLISH:
				PlaceProc_DemolishArea(tile);
				break;

			case WID_ROT_DEPOT:
				CloseWindowById(WC_SELECT_DEPOT, VEH_ROAD);

				VpSetPlaceSizingLimit(_settings_game.depot.depot_spread);
				VpStartPlaceSizing(tile, (DiagDirToAxis(_road_depot_orientation) == 0) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_DEPOT);
				break;

			case WID_ROT_BUILD_WAYPOINT:
				PlaceRoad_Waypoint(tile);
				break;

			case WID_ROT_BUS_STATION:
				PlaceRoad_BusStation(tile);
				break;

			case WID_ROT_TRUCK_STATION:
				PlaceRoad_TruckStation(tile);
				break;

			case WID_ROT_BUILD_BRIDGE:
				PlaceRoad_Bridge(tile, this);
				break;

			case WID_ROT_BUILD_TUNNEL:
				Command<CMD_BUILD_TUNNEL>::Post(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE, CcBuildRoadTunnel,
						tile, TRANSPORT_ROAD, _cur_roadtype);
				break;

			case WID_ROT_CONVERT_ROAD:
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CONVERT_ROAD);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceObjectAbort() override
	{
		if (_game_mode != GM_EDITOR && (this->IsWidgetLowered(WID_ROT_BUS_STATION) || this->IsWidgetLowered(WID_ROT_TRUCK_STATION))) SetViewportCatchmentStation(nullptr, true);

		if (_game_mode == GM_NORMAL && this->IsWidgetLowered(WID_ROT_DEPOT)) SetViewportHighlightDepot(INVALID_DEPOT, true);

		this->RaiseButtons();
		this->SetWidgetDisabledState(WID_ROT_REMOVE, true);
		this->SetWidgetDirty(WID_ROT_REMOVE);

		if (RoadTypeIsRoad(this->roadtype)) {
			this->SetWidgetDisabledState(WID_ROT_ONE_WAY, true);
			this->SetWidgetDirty(WID_ROT_ONE_WAY);
		}

		CloseWindowById(WC_BUS_STATION, TRANSPORT_ROAD);
		CloseWindowById(WC_TRUCK_STATION, TRANSPORT_ROAD);
		CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_ROAD);
		CloseWindowById(WC_BUILD_WAYPOINT, TRANSPORT_ROAD);
		CloseWindowById(WC_SELECT_STATION, 0);
		CloseWindowById(WC_SELECT_DEPOT, VEH_ROAD);
		CloseWindowByClass(WC_BUILD_BRIDGE);
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		/* Here we update the end tile flags
		 * of the road placement actions.
		 * At first we reset the end halfroad
		 * bits and if needed we set them again. */
		switch (select_proc) {
			case DDSP_PLACE_ROAD_X_DIR:
				_place_road_end_half = pt.x & 8;
				break;

			case DDSP_PLACE_ROAD_Y_DIR:
				_place_road_end_half = pt.y & 8;
				break;

			case DDSP_PLACE_AUTOROAD:
				/* For autoroad we need to update the
				 * direction of the road */
				if (_thd.size.x > _thd.size.y || (_thd.size.x == _thd.size.y &&
						( (_tile_fract_coords.x < _tile_fract_coords.y && (_tile_fract_coords.x + _tile_fract_coords.y) < 16) ||
						(_tile_fract_coords.x > _tile_fract_coords.y && (_tile_fract_coords.x + _tile_fract_coords.y) > 16) ))) {
					/* Set dir = X */
					_place_road_dir = AXIS_X;
					_place_road_end_half = pt.x & 8;
				} else {
					/* Set dir = Y */
					_place_road_dir = AXIS_Y;
					_place_road_end_half = pt.y & 8;
				}

				break;

			default:
				break;
		}

		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x != -1) {
			switch (select_proc) {
				default: NOT_REACHED();
				case DDSP_BUILD_BRIDGE:
					if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
					ShowBuildBridgeWindow(start_tile, end_tile, TRANSPORT_ROAD, _cur_roadtype);
					break;

				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;

				case DDSP_PLACE_ROAD_X_DIR:
				case DDSP_PLACE_ROAD_Y_DIR:
				case DDSP_PLACE_AUTOROAD: {
					bool start_half = _place_road_dir == AXIS_Y ? _place_road_start_half_y : _place_road_start_half_x;

					if (_remove_button_clicked) {
						Command<CMD_REMOVE_LONG_ROAD>::Post(this->rti->strings.err_remove_road, CcPlaySound_CONSTRUCTION_OTHER,
								end_tile, start_tile, _cur_roadtype, _place_road_dir, start_half, _place_road_end_half);
					} else {
						Command<CMD_BUILD_LONG_ROAD>::Post(this->rti->strings.err_build_road, CcPlaySound_CONSTRUCTION_OTHER,
								end_tile, start_tile, _cur_roadtype, _place_road_dir, _one_way_button_clicked ? DRD_NORTHBOUND : DRD_NONE, start_half, _place_road_end_half, false);
					}
					break;
				}

				case DDSP_BUILD_ROAD_WAYPOINT:
				case DDSP_REMOVE_ROAD_WAYPOINT:
					if (this->IsWidgetLowered(WID_ROT_BUILD_WAYPOINT)) {
						if (_remove_button_clicked) {
							Command<CMD_REMOVE_FROM_ROAD_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_ROAD_WAYPOINT, CcPlaySound_CONSTRUCTION_OTHER, end_tile, start_tile);
						} else {
							TileArea ta(start_tile, end_tile);
							Axis axis = select_method == VPM_X_LIMITED ? AXIS_X : AXIS_Y;
							bool adjacent = _ctrl_pressed;

							auto proc = [=](bool test, StationID to_join) -> bool {
								if (test) {
									return Command<CMD_BUILD_ROAD_WAYPOINT>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_ROAD_WAYPOINT>()), ta.tile, axis, ta.w, ta.h, _waypoint_gui.sel_class, _waypoint_gui.sel_type, INVALID_STATION, adjacent).Succeeded();
								} else {
									return Command<CMD_BUILD_ROAD_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_ROAD_WAYPOINT, CcPlaySound_CONSTRUCTION_OTHER, ta.tile, axis, ta.w, ta.h, _waypoint_gui.sel_class, _waypoint_gui.sel_type, to_join, adjacent);
								}
							};

							ShowSelectRoadWaypointIfNeeded(ta, proc);
						}
					}
					break;

				case DDSP_BUILD_BUSSTOP:
				case DDSP_REMOVE_BUSSTOP:
					if (this->IsWidgetLowered(WID_ROT_BUS_STATION) && GetIfClassHasNewStopsByType(RoadStopClass::Get(_roadstop_gui.sel_class), ROADSTOP_BUS, _cur_roadtype)) {
						if (_remove_button_clicked) {
							TileArea ta(start_tile, end_tile);
							Command<CMD_REMOVE_ROAD_STOP>::Post(this->rti->strings.err_remove_station[ROADSTOP_BUS], CcPlaySound_CONSTRUCTION_OTHER, ta.tile, ta.w, ta.h, ROADSTOP_BUS, _ctrl_pressed);
						} else {
							PlaceRoadStop(start_tile, end_tile, ROADSTOP_BUS, _ctrl_pressed, _cur_roadtype, this->rti->strings.err_build_station[ROADSTOP_BUS]);
						}
					}
					break;

				case DDSP_BUILD_TRUCKSTOP:
				case DDSP_REMOVE_TRUCKSTOP:
					if (this->IsWidgetLowered(WID_ROT_TRUCK_STATION) && GetIfClassHasNewStopsByType(RoadStopClass::Get(_roadstop_gui.sel_class), ROADSTOP_TRUCK, _cur_roadtype)) {
						if (_remove_button_clicked) {
							TileArea ta(start_tile, end_tile);
							Command<CMD_REMOVE_ROAD_STOP>::Post(this->rti->strings.err_remove_station[ROADSTOP_TRUCK], CcPlaySound_CONSTRUCTION_OTHER, ta.tile, ta.w, ta.h, ROADSTOP_TRUCK, _ctrl_pressed);
						} else {
							PlaceRoadStop(start_tile, end_tile, ROADSTOP_TRUCK, _ctrl_pressed, _cur_roadtype, this->rti->strings.err_build_station[ROADSTOP_TRUCK]);
						}
					}
					break;

				case DDSP_BUILD_DEPOT: {
					bool adjacent = _ctrl_pressed;
					StringID error_string = this->rti->strings.err_depot;

					auto proc = [=](DepotID join_to) -> bool {
						return Command<CMD_BUILD_ROAD_DEPOT>::Post(error_string, CcRoadDepot, start_tile, _cur_roadtype, _road_depot_orientation, adjacent, join_to, end_tile);
					};

					ShowSelectDepotIfNeeded(TileArea(start_tile, end_tile), proc, VEH_ROAD);
					break;
				}

				case DDSP_CONVERT_ROAD:
					Command<CMD_CONVERT_ROAD>::Post(rti->strings.err_convert_road, CcPlaySound_CONSTRUCTION_OTHER, end_tile, start_tile, _cur_roadtype);
					break;
			}
		}
	}

	void OnPlacePresize([[maybe_unused]] Point pt, TileIndex tile) override
	{
		Command<CMD_BUILD_TUNNEL>::Do(DC_AUTO, tile, TRANSPORT_ROAD, _cur_roadtype);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
	}

	EventState OnCTRLStateChange() override
	{
		if (RoadToolbar_CtrlChanged(this)) return ES_HANDLED;
		return ES_NOT_HANDLED;
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		if (_game_mode == GM_NORMAL && this->IsWidgetLowered(WID_ROT_BUILD_WAYPOINT)) CheckRedrawRoadWaypointCoverage(this);
	}

	/**
	 * Handler for global hotkeys of the BuildRoadToolbarWindow.
	 * @param hotkey Hotkey
	 * @param last_build Last build road type
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState RoadTramToolbarGlobalHotkeys(int hotkey, RoadType last_build, RoadTramType rtt)
	{
		Window *w = nullptr;
		switch (_game_mode) {
			case GM_NORMAL:
				w = ShowBuildRoadToolbar(last_build);
				break;

			case GM_EDITOR:
				if ((GetRoadTypes(true) & ((rtt == RTT_ROAD) ? ~_roadtypes_type : _roadtypes_type)) == ROADTYPES_NONE) return ES_NOT_HANDLED;
				w = ShowBuildRoadScenToolbar(last_build);
				break;

			default:
				break;
		}

		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static EventState RoadToolbarGlobalHotkeys(int hotkey)
	{
		extern RoadType _last_built_roadtype;
		return RoadTramToolbarGlobalHotkeys(hotkey, _last_built_roadtype, RTT_ROAD);
	}

	static EventState TramToolbarGlobalHotkeys(int hotkey)
	{
		extern RoadType _last_built_tramtype;
		return RoadTramToolbarGlobalHotkeys(hotkey, _last_built_tramtype, RTT_TRAM);
	}

	static inline HotkeyList road_hotkeys{"roadtoolbar", {
		Hotkey('1', "build_x", WID_ROT_ROAD_X),
		Hotkey('2', "build_y", WID_ROT_ROAD_Y),
		Hotkey('3', "autoroad", WID_ROT_AUTOROAD),
		Hotkey('4', "demolish", WID_ROT_DEMOLISH),
		Hotkey('5', "depot", WID_ROT_DEPOT),
		Hotkey('6', "bus_station", WID_ROT_BUS_STATION),
		Hotkey('7', "truck_station", WID_ROT_TRUCK_STATION),
		Hotkey('8', "oneway", WID_ROT_ONE_WAY),
		Hotkey('9', "waypoint", WID_ROT_BUILD_WAYPOINT),
		Hotkey('B', "bridge", WID_ROT_BUILD_BRIDGE),
		Hotkey('T', "tunnel", WID_ROT_BUILD_TUNNEL),
		Hotkey('R', "remove", WID_ROT_REMOVE),
		Hotkey('C', "convert", WID_ROT_CONVERT_ROAD),
	}, RoadToolbarGlobalHotkeys};

	static inline HotkeyList tram_hotkeys{"tramtoolbar", {
		Hotkey('1', "build_x", WID_ROT_ROAD_X),
		Hotkey('2', "build_y", WID_ROT_ROAD_Y),
		Hotkey('3', "autoroad", WID_ROT_AUTOROAD),
		Hotkey('4', "demolish", WID_ROT_DEMOLISH),
		Hotkey('5', "depot", WID_ROT_DEPOT),
		Hotkey('6', "bus_station", WID_ROT_BUS_STATION),
		Hotkey('7', "truck_station", WID_ROT_TRUCK_STATION),
		Hotkey('9', "waypoint", WID_ROT_BUILD_WAYPOINT),
		Hotkey('B', "bridge", WID_ROT_BUILD_BRIDGE),
		Hotkey('T', "tunnel", WID_ROT_BUILD_TUNNEL),
		Hotkey('R', "remove", WID_ROT_REMOVE),
		Hotkey('C', "convert", WID_ROT_CONVERT_ROAD),
	}, TramToolbarGlobalHotkeys};
};

static constexpr NWidgetPart _nested_build_road_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_ROT_CAPTION), SetDataTip(STR_JUST_STRING2, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetTextStyle(TC_WHITE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOROAD, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_DEPOT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_DEPOT, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_VEHICLE_DEPOT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_WAYPOINT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_WAYPOINT, STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_ROAD_TO_WAYPOINT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUS_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUS_STATION, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_BUS_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_TRUCK_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRUCK_BAY, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRUCK_LOADING_BAY),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ONE_WAY),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_ONE_WAY, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_ONE_WAY_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_CONVERT_ROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_CONVERT_ROAD, STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_ROAD),
	EndContainer(),
};

static WindowDesc _build_road_desc(
	WDP_ALIGN_TOOLBAR, "toolbar_road", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_road_widgets,
	&BuildRoadToolbarWindow::road_hotkeys
);

static constexpr NWidgetPart _nested_build_tramway_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_ROT_CAPTION), SetDataTip(STR_JUST_STRING2, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetTextStyle(TC_WHITE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOTRAM, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOTRAM),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_DEPOT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_DEPOT, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAM_VEHICLE_DEPOT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_WAYPOINT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_WAYPOINT, STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_TRAM_TO_WAYPOINT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUS_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUS_STATION, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_PASSENGER_TRAM_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_TRUCK_STATION),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRUCK_BAY, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_CARGO_TRAM_STATION),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_TRAMWAYS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_CONVERT_ROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_CONVERT_ROAD, STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_TRAM),
	EndContainer(),
};

static WindowDesc _build_tramway_desc(
	WDP_ALIGN_TOOLBAR, "toolbar_tramway", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_tramway_widgets,
	&BuildRoadToolbarWindow::tram_hotkeys
);

/**
 * Open the build road toolbar window
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @return newly opened road toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildRoadToolbar(RoadType roadtype)
{
	if (!Company::IsValidID(_local_company)) return nullptr;
	if (!ValParamRoadType(roadtype)) return nullptr;

	CloseWindowByClass(WC_BUILD_TOOLBAR);
	_cur_roadtype = roadtype;

	return AllocateWindowDescFront<BuildRoadToolbarWindow>(RoadTypeIsRoad(_cur_roadtype) ? _build_road_desc : _build_tramway_desc, TRANSPORT_ROAD);
}

static constexpr NWidgetPart _nested_build_road_scen_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_ROT_CAPTION), SetDataTip(STR_JUST_STRING2, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetTextStyle(TC_WHITE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOROAD, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ONE_WAY),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_ONE_WAY, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_ONE_WAY_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_CONVERT_ROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_CONVERT_ROAD, STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_ROAD),
	EndContainer(),
};

static WindowDesc _build_road_scen_desc(
	WDP_AUTO, "toolbar_road_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_road_scen_widgets,
	&BuildRoadToolbarWindow::road_hotkeys
);

static constexpr NWidgetPart _nested_build_tramway_scen_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_ROT_CAPTION), SetDataTip(STR_JUST_STRING2, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetTextStyle(TC_WHITE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_ROAD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_AUTOROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOTRAM, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOTRAM),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, -1), SetMinimalSize(0, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_TRAMWAYS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_ROT_CONVERT_ROAD),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_CONVERT_ROAD, STR_ROAD_TOOLBAR_TOOLTIP_CONVERT_TRAM),
	EndContainer(),
};

static WindowDesc _build_tramway_scen_desc(
	WDP_AUTO, "toolbar_tram_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_tramway_scen_widgets,
	&BuildRoadToolbarWindow::tram_hotkeys
);

/**
 * Show the road building toolbar in the scenario editor.
 * @return The just opened toolbar, or \c nullptr if the toolbar was already open.
 */
Window *ShowBuildRoadScenToolbar(RoadType roadtype)
{
	CloseWindowById(WC_SCEN_BUILD_TOOLBAR, TRANSPORT_ROAD);
	_cur_roadtype = roadtype;

	return AllocateWindowDescFront<BuildRoadToolbarWindow>(RoadTypeIsRoad(_cur_roadtype) ? _build_road_scen_desc : _build_tramway_scen_desc, TRANSPORT_ROAD);
}

struct BuildRoadDepotWindow : public PickerWindowBase {
	BuildRoadDepotWindow(WindowDesc &desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->CreateNestedTree();

		this->LowerWidget(WID_BROD_DEPOT_NE + _road_depot_orientation);
		if (RoadTypeIsTram(_cur_roadtype)) {
			this->GetWidget<NWidgetCore>(WID_BROD_CAPTION)->widget_data = STR_BUILD_DEPOT_TRAM_ORIENTATION_CAPTION;
			for (WidgetID i = WID_BROD_DEPOT_NE; i <= WID_BROD_DEPOT_NW; i++) {
				this->GetWidget<NWidgetCore>(i)->tool_tip = STR_BUILD_DEPOT_TRAM_ORIENTATION_SELECT_TOOLTIP;
			}
		}

		this->FinishInitNested(TRANSPORT_ROAD);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_DEPOT, VEH_ROAD);
		this->PickerWindowBase::Close();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (!IsInsideMM(widget, WID_BROD_DEPOT_NE, WID_BROD_DEPOT_NW + 1)) return;

		size.width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
		size.height = ScaleGUITrad(48) + WidgetDimensions::scaled.fullbevel.Vertical();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (!IsInsideMM(widget, WID_BROD_DEPOT_NE, WID_BROD_DEPOT_NW + 1)) return;

		DrawPixelInfo tmp_dpi;
		Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
		if (FillDrawPixelInfo(&tmp_dpi, ir)) {
			AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
			int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
			int y = (ir.Height() + ScaleSpriteTrad(48)) / 2 - ScaleSpriteTrad(31);
			DrawRoadDepotSprite(x, y, (DiagDirection)(widget - WID_BROD_DEPOT_NE + DIAGDIR_NE), _cur_roadtype);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BROD_DEPOT_NW:
			case WID_BROD_DEPOT_NE:
			case WID_BROD_DEPOT_SW:
			case WID_BROD_DEPOT_SE:
				CloseWindowById(WC_SELECT_DEPOT, VEH_ROAD);
				this->RaiseWidget(WID_BROD_DEPOT_NE + _road_depot_orientation);
				_road_depot_orientation = (DiagDirection)(widget - WID_BROD_DEPOT_NE);
				this->LowerWidget(WID_BROD_DEPOT_NE + _road_depot_orientation);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			default:
				break;
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawDepotHighlight(this, VEH_ROAD);
	}
};

static constexpr NWidgetPart _nested_build_road_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BROD_CAPTION), SetDataTip(STR_BUILD_DEPOT_ROAD_ORIENTATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_NW), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_SW), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_NE), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_SE), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_road_depot_desc(
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_road_depot_widgets
);

static void ShowRoadDepotPicker(Window *parent)
{
	new BuildRoadDepotWindow(_build_road_depot_desc, parent);
}

template <RoadStopType roadstoptype>
class RoadStopPickerCallbacks : public PickerCallbacksNewGRFClass<RoadStopClass> {
public:
	RoadStopPickerCallbacks(const std::string &ini_group) : PickerCallbacksNewGRFClass<RoadStopClass>(ini_group) {}

	StringID GetClassTooltip() const override;
	StringID GetTypeTooltip() const override;

	bool IsActive() const override
	{
		for (const auto &cls : RoadStopClass::Classes()) {
			if (IsWaypointClass(cls)) continue;
			for (const auto *spec : cls.Specs()) {
				if (spec == nullptr) continue;
				if (roadstoptype == ROADSTOP_TRUCK && spec->stop_type != ROADSTOPTYPE_FREIGHT && spec->stop_type != ROADSTOPTYPE_ALL) continue;
				if (roadstoptype == ROADSTOP_BUS && spec->stop_type != ROADSTOPTYPE_PASSENGER && spec->stop_type != ROADSTOPTYPE_ALL) continue;
				return true;
			}
		}
		return false;
	}

	static bool IsClassChoice(const RoadStopClass &cls)
	{
		return !IsWaypointClass(cls) && GetIfClassHasNewStopsByType(&cls, roadstoptype, _cur_roadtype);
	}

	bool HasClassChoice() const override
	{
		return std::count_if(std::begin(RoadStopClass::Classes()), std::end(RoadStopClass::Classes()), IsClassChoice);
	}

	int GetSelectedClass() const override { return _roadstop_gui.sel_class; }
	void SetSelectedClass(int id) const override { _roadstop_gui.sel_class = this->GetClassIndex(id); }

	StringID GetClassName(int id) const override
	{
		const auto *rsc = this->GetClass(id);
		if (!IsClassChoice(*rsc)) return INVALID_STRING_ID;
		return rsc->name;
	}

	int GetSelectedType() const override { return _roadstop_gui.sel_type; }
	void SetSelectedType(int id) const override { _roadstop_gui.sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		if (!IsRoadStopEverAvailable(spec, roadstoptype == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK)) return INVALID_STRING_ID;
		return (spec == nullptr) ? STR_STATION_CLASS_DFLT_ROADSTOP : spec->name;
	}

	bool IsTypeAvailable(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		return IsRoadStopAvailable(spec, roadstoptype == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK);
	}

	void DrawType(int x, int y, int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		if (spec == nullptr) {
			StationPickerDrawSprite(x, y, roadstoptype == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK, INVALID_RAILTYPE, _cur_roadtype, _roadstop_gui.orientation);
		} else {
			DiagDirection orientation = _roadstop_gui.orientation;
			if (orientation < DIAGDIR_END && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) orientation = DIAGDIR_END;
			DrawRoadStopTile(x, y, _cur_roadtype, spec, roadstoptype == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK, (uint8_t)orientation);
		}
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		for (const Station *st : Station::Iterate()) {
			if (st->owner != _local_company) continue;
			if (roadstoptype == ROADSTOP_TRUCK && !(st->facilities & FACIL_TRUCK_STOP)) continue;
			if (roadstoptype == ROADSTOP_BUS && !(st->facilities & FACIL_BUS_STOP)) continue;
			items.insert({0, 0, ROADSTOP_CLASS_DFLT, 0}); // We would need to scan the map to find out if default is used.
			for (const auto &sm : st->roadstop_speclist) {
				if (sm.spec == nullptr) continue;
				if (roadstoptype == ROADSTOP_TRUCK && sm.spec->stop_type != ROADSTOPTYPE_FREIGHT && sm.spec->stop_type != ROADSTOPTYPE_ALL) continue;
				if (roadstoptype == ROADSTOP_BUS && sm.spec->stop_type != ROADSTOPTYPE_PASSENGER && sm.spec->stop_type != ROADSTOPTYPE_ALL) continue;
				items.insert({sm.grfid, sm.localidx, sm.spec->class_index, sm.spec->index});
			}
		}
	}
};

template <> StringID RoadStopPickerCallbacks<ROADSTOP_BUS>::GetClassTooltip() const { return STR_PICKER_ROADSTOP_BUS_CLASS_TOOLTIP; }
template <> StringID RoadStopPickerCallbacks<ROADSTOP_BUS>::GetTypeTooltip() const { return STR_PICKER_ROADSTOP_BUS_TYPE_TOOLTIP; }

template <> StringID RoadStopPickerCallbacks<ROADSTOP_TRUCK>::GetClassTooltip() const { return STR_PICKER_ROADSTOP_TRUCK_CLASS_TOOLTIP; }
template <> StringID RoadStopPickerCallbacks<ROADSTOP_TRUCK>::GetTypeTooltip() const { return STR_PICKER_ROADSTOP_TRUCK_TYPE_TOOLTIP; }

static RoadStopPickerCallbacks<ROADSTOP_BUS> _bus_callback_instance("fav_passenger_roadstops");
static RoadStopPickerCallbacks<ROADSTOP_TRUCK> _truck_callback_instance("fav_freight_roadstops");

static PickerCallbacks &GetRoadStopPickerCallbacks(RoadStopType rs)
{
	return rs == ROADSTOP_BUS ? static_cast<PickerCallbacks &>(_bus_callback_instance) : static_cast<PickerCallbacks &>(_truck_callback_instance);
}

struct BuildRoadStationWindow : public PickerWindow {
private:
	uint coverage_height; ///< Height of the coverage texts.

	void CheckOrientationValid()
	{
		const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui.sel_class)->GetSpec(_roadstop_gui.sel_type);

		/* Raise and lower to ensure the correct widget is lowered after changing displayed orientation plane. */
		if (RoadTypeIsRoad(_cur_roadtype)) {
			this->RaiseWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
			this->GetWidget<NWidgetStacked>(WID_BROS_AVAILABLE_ORIENTATIONS)->SetDisplayedPlane((spec != nullptr && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) ? 1 : 0);
			this->LowerWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
		}

		if (_roadstop_gui.orientation >= DIAGDIR_END) return;

		if (spec != nullptr && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) {
			this->RaiseWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
			_roadstop_gui.orientation = DIAGDIR_END;
			this->LowerWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
			this->SetDirty();
			CloseWindowById(WC_SELECT_STATION, 0);
		}
	}

public:
	BuildRoadStationWindow(WindowDesc &desc, Window *parent, RoadStopType rs) : PickerWindow(desc, parent, TRANSPORT_ROAD, GetRoadStopPickerCallbacks(rs))
	{
		this->coverage_height = 2 * GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

		/* Trams don't have non-drivethrough stations */
		if (RoadTypeIsTram(_cur_roadtype) && _roadstop_gui.orientation < DIAGDIR_END) {
			_roadstop_gui.orientation = DIAGDIR_END;
		}
		this->ConstructWindow();

		const RoadTypeInfo *rti = GetRoadTypeInfo(_cur_roadtype);
		this->GetWidget<NWidgetCore>(WID_BROS_CAPTION)->widget_data = rti->strings.picker_title[rs];

		for (WidgetID i = RoadTypeIsTram(_cur_roadtype) ? WID_BROS_STATION_X : WID_BROS_STATION_NE; i < WID_BROS_LT_OFF; i++) {
			this->GetWidget<NWidgetCore>(i)->tool_tip = rti->strings.picker_tooltip[rs];
		}

		this->LowerWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
		this->LowerWidget(WID_BROS_LT_OFF + _settings_client.gui.station_show_coverage);

		this->window_class = (rs == ROADSTOP_BUS) ? WC_BUS_STATION : WC_TRUCK_STATION;
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_STATION, 0);
		this->PickerWindow::Close();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->PickerWindow::OnInvalidateData(data, gui_scope);

		if (gui_scope) {
			this->CheckOrientationValid();
		}
	}

	void OnPaint() override
	{
		this->DrawWidgets();

		int rad = _settings_game.station.modified_catchment ? ((this->window_class == WC_BUS_STATION) ? CA_BUS : CA_TRUCK) : CA_UNMODIFIED;
		if (_settings_client.gui.station_show_coverage) {
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		if (this->IsShaded()) return;

		/* 'Accepts' and 'Supplies' texts. */
		StationCoverageType sct = (this->window_class == WC_BUS_STATION) ? SCT_PASSENGERS_ONLY : SCT_NON_PASSENGERS_ONLY;
		Rect r = this->GetWidget<NWidgetBase>(WID_BROS_ACCEPTANCE)->GetCurrentRect();
		int top = r.top;
		top = DrawStationCoverageAreaText(r.left, r.right, top, sct, rad, false) + WidgetDimensions::scaled.vsep_normal;
		top = DrawStationCoverageAreaText(r.left, r.right, top, sct, rad, true);
		/* Resize background if the window is too small.
		 * Never make the window smaller to avoid oscillating if the size change affects the acceptance.
		 * (This is the case, if making the window bigger moves the mouse into the window.) */
		if (top > r.bottom) {
			this->coverage_height += top - r.bottom;
			this->ReInit();
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_BROS_STATION_NE:
			case WID_BROS_STATION_SE:
			case WID_BROS_STATION_SW:
			case WID_BROS_STATION_NW:
			case WID_BROS_STATION_X:
			case WID_BROS_STATION_Y:
				size.width  = ScaleGUITrad(PREVIEW_WIDTH) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height = ScaleGUITrad(PREVIEW_HEIGHT) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;

			case WID_BROS_ACCEPTANCE:
				size.height = this->coverage_height;
				break;

			default:
				this->PickerWindow::UpdateWidgetSize(widget, size, padding, fill, resize);
				break;
		}
	}

	/**
	 * Simply to have a easier way to get the StationType for bus, truck and trams from the WindowClass.
	 */
	StationType GetRoadStationTypeByWindowClass(WindowClass window_class) const
	{
		switch (window_class) {
			case WC_BUS_STATION:          return STATION_BUS;
			case WC_TRUCK_STATION:        return STATION_TRUCK;
			default: NOT_REACHED();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BROS_STATION_NE:
			case WID_BROS_STATION_SE:
			case WID_BROS_STATION_SW:
			case WID_BROS_STATION_NW:
			case WID_BROS_STATION_X:
			case WID_BROS_STATION_Y: {
				StationType st = GetRoadStationTypeByWindowClass(this->window_class);
				const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui.sel_class)->GetSpec(_roadstop_gui.sel_type);
				DrawPixelInfo tmp_dpi;
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(PREVIEW_WIDTH)) / 2 + ScaleSpriteTrad(PREVIEW_LEFT);
					int y = (ir.Height() + ScaleSpriteTrad(PREVIEW_HEIGHT)) / 2 - ScaleSpriteTrad(PREVIEW_BOTTOM);
					if (spec == nullptr) {
						StationPickerDrawSprite(x, y, st, INVALID_RAILTYPE, _cur_roadtype, widget - WID_BROS_STATION_NE);
					} else {
						DrawRoadStopTile(x, y, _cur_roadtype, spec, st, widget - WID_BROS_STATION_NE);
					}
				}
				break;
			}

			default:
				this->PickerWindow::DrawWidget(r, widget);
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BROS_STATION_NE:
			case WID_BROS_STATION_SE:
			case WID_BROS_STATION_SW:
			case WID_BROS_STATION_NW:
			case WID_BROS_STATION_X:
			case WID_BROS_STATION_Y:
				if (widget < WID_BROS_STATION_X) {
					const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui.sel_class)->GetSpec(_roadstop_gui.sel_type);
					if (spec != nullptr && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) return;
				}
				this->RaiseWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
				_roadstop_gui.orientation = (DiagDirection)(widget - WID_BROS_STATION_NE);
				this->LowerWidget(WID_BROS_STATION_NE + _roadstop_gui.orientation);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;

			case WID_BROS_LT_OFF:
			case WID_BROS_LT_ON:
				this->RaiseWidget(_settings_client.gui.station_show_coverage + WID_BROS_LT_OFF);
				_settings_client.gui.station_show_coverage = (widget != WID_BROS_LT_OFF);
				this->LowerWidget(_settings_client.gui.station_show_coverage + WID_BROS_LT_OFF);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				SetViewportCatchmentStation(nullptr, true);
				break;

			default:
				this->PickerWindow::OnClick(pt, widget, click_count);
				break;
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawStationCoverage(this);
	}

	static inline HotkeyList road_hotkeys{"buildroadstop", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}};

	static inline HotkeyList tram_hotkeys{"buildtramstop", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}};
};

/** Widget definition of the build road station window */
static constexpr NWidgetPart _nested_road_station_picker_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION,  COLOUR_DARK_GREEN, WID_BROS_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_AVAILABLE_ORIENTATIONS),
						/* 6-orientation plane. */
						NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
								NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_NW), SetFill(0, 0), EndContainer(),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_NE), SetFill(0, 0), EndContainer(),
								EndContainer(),
								NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_X), SetFill(0, 0), EndContainer(),
							EndContainer(),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
								NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_SW), SetFill(0, 0), EndContainer(),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_SE), SetFill(0, 0), EndContainer(),
								EndContainer(),
								NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_Y), SetFill(0, 0), EndContainer(),
							EndContainer(),
						EndContainer(),
						/* 2-orientation plane. */
						NWidget(NWID_VERTICAL), SetPIPRatio(0, 0, 1),
							NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
								NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_X), SetFill(0, 0), EndContainer(),
								NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_Y), SetFill(0, 0), EndContainer(),
							EndContainer(),
						EndContainer(),
					EndContainer(),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_OFF), SetMinimalSize(60, 12),
								SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_ON), SetMinimalSize(60, 12),
								SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
					EndContainer(),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BROS_ACCEPTANCE), SetFill(1, 1), SetResize(1, 0), SetMinimalTextLines(2, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

static WindowDesc _road_station_picker_desc(
	WDP_AUTO, "build_station_road", 0, 0,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_road_station_picker_widgets,
	&BuildRoadStationWindow::road_hotkeys
);

/** Widget definition of the build tram station window */
static constexpr NWidgetPart _nested_tram_station_picker_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION,  COLOUR_DARK_GREEN, WID_BROS_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
						NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_X), SetFill(0, 0), EndContainer(),
						NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_Y), SetFill(0, 0), EndContainer(),
					EndContainer(),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_OFF), SetMinimalSize(60, 12),
								SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_ON), SetMinimalSize(60, 12),
								SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
					EndContainer(),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BROS_ACCEPTANCE), SetFill(1, 1), SetResize(1, 0), SetMinimalTextLines(2, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

static WindowDesc _tram_station_picker_desc(
	WDP_AUTO, "build_station_tram", 0, 0,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_tram_station_picker_widgets,
	&BuildRoadStationWindow::tram_hotkeys
);

static void ShowRVStationPicker(Window *parent, RoadStopType rs)
{
	new BuildRoadStationWindow(RoadTypeIsRoad(_cur_roadtype) ? _road_station_picker_desc : _tram_station_picker_desc, parent, rs);
}

class RoadWaypointPickerCallbacks : public PickerCallbacksNewGRFClass<RoadStopClass> {
public:
	RoadWaypointPickerCallbacks() : PickerCallbacksNewGRFClass<RoadStopClass>("fav_road_waypoints") {}

	StringID GetClassTooltip() const override { return STR_PICKER_WAYPOINT_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_WAYPOINT_TYPE_TOOLTIP; }

	bool IsActive() const override
	{
		for (const auto &cls : RoadStopClass::Classes()) {
			if (!IsWaypointClass(cls)) continue;
			for (const auto *spec : cls.Specs()) {
				if (spec != nullptr) return true;
			}
		}
		return false;
	}

	bool HasClassChoice() const override
	{
		return std::count_if(std::begin(RoadStopClass::Classes()), std::end(RoadStopClass::Classes()), IsWaypointClass) > 1;
	}

	void Close(int) override { ResetObjectToPlace(); }
	int GetSelectedClass() const override { return _waypoint_gui.sel_class; }
	void SetSelectedClass(int id) const override { _waypoint_gui.sel_class = this->GetClassIndex(id); }

	StringID GetClassName(int id) const override
	{
		const auto *sc = GetClass(id);
		if (!IsWaypointClass(*sc)) return INVALID_STRING_ID;
		return sc->name;
	}

	int GetSelectedType() const override { return _waypoint_gui.sel_type; }
	void SetSelectedType(int id) const override { _waypoint_gui.sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		return (spec == nullptr) ? STR_STATION_CLASS_WAYP_WAYPOINT : spec->name;
	}

	bool IsTypeAvailable(int cls_id, int id) const override
	{
		return IsRoadStopAvailable(this->GetSpec(cls_id, id), STATION_ROADWAYPOINT);
	}

	void DrawType(int x, int y, int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		if (spec == nullptr) {
			StationPickerDrawSprite(x, y, STATION_ROADWAYPOINT, INVALID_RAILTYPE, _cur_roadtype, RSV_DRIVE_THROUGH_X);
		} else {
			DrawRoadStopTile(x, y, _cur_roadtype, spec, STATION_ROADWAYPOINT, RSV_DRIVE_THROUGH_X);
		}
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		for (const Waypoint *wp : Waypoint::Iterate()) {
			if (wp->owner != _local_company || !HasBit(wp->waypoint_flags, WPF_ROAD)) continue;
			items.insert({0, 0, ROADSTOP_CLASS_WAYP, 0}); // We would need to scan the map to find out if default is used.
			for (const auto &sm : wp->roadstop_speclist) {
				if (sm.spec == nullptr) continue;
				items.insert({sm.grfid, sm.localidx, sm.spec->class_index, sm.spec->index});
			}
		}
	}

	static RoadWaypointPickerCallbacks instance;
};
/* static */ RoadWaypointPickerCallbacks RoadWaypointPickerCallbacks::instance;

struct BuildRoadWaypointWindow : public PickerWindow {
	BuildRoadWaypointWindow(WindowDesc &desc, Window *parent) : PickerWindow(desc, parent, TRANSPORT_ROAD, RoadWaypointPickerCallbacks::instance)
	{
		this->ConstructWindow();
		this->InvalidateData();
	}

	static inline HotkeyList hotkeys{"buildroadwaypoint", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}};
};

/** Nested widget definition for the build NewGRF road waypoint window */
static constexpr NWidgetPart _nested_build_road_waypoint_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_WAYPOINT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidgetFunction(MakePickerClassWidgets),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

static WindowDesc _build_road_waypoint_desc(
	WDP_AUTO, "build_road_waypoint", 0, 0,
	WC_BUILD_WAYPOINT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_road_waypoint_widgets,
	&BuildRoadWaypointWindow::hotkeys
);

static void ShowBuildRoadWaypointPicker(Window *parent)
{
	if (!RoadWaypointPickerCallbacks::instance.IsActive()) return;
	new BuildRoadWaypointWindow(_build_road_waypoint_desc, parent);
}

void InitializeRoadGui()
{
	_road_depot_orientation = DIAGDIR_NW;
	_roadstop_gui.orientation = DIAGDIR_NW;
	_waypoint_gui.sel_class = RoadStopClassID::ROADSTOP_CLASS_WAYP;
	_waypoint_gui.sel_type = 0;
}

/**
 * I really don't know why rail_gui.cpp has this too, shouldn't be included in the other one?
 */
void InitializeRoadGUI()
{
	BuildRoadToolbarWindow *w = dynamic_cast<BuildRoadToolbarWindow *>(FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_ROAD));
	if (w != nullptr) w->ModifyRoadType(_cur_roadtype);
}

DropDownList GetRoadTypeDropDownList(RoadTramTypes rtts, bool for_replacement, bool all_option)
{
	RoadTypes used_roadtypes;
	RoadTypes avail_roadtypes;

	const Company *c = Company::Get(_local_company);

	/* Find the used roadtypes. */
	if (for_replacement) {
		avail_roadtypes = GetCompanyRoadTypes(c->index, false);
		used_roadtypes  = GetRoadTypes(false);
	} else {
		avail_roadtypes = c->avail_roadtypes;
		used_roadtypes  = GetRoadTypes(true);
	}

	/* Filter listed road types */
	if (!HasBit(rtts, RTT_ROAD)) used_roadtypes &= _roadtypes_type;
	if (!HasBit(rtts, RTT_TRAM)) used_roadtypes &= ~_roadtypes_type;

	DropDownList list;

	if (all_option) {
		list.push_back(MakeDropDownListStringItem(STR_REPLACE_ALL_ROADTYPE, INVALID_ROADTYPE));
	}

	Dimension d = { 0, 0 };
	/* Get largest icon size, to ensure text is aligned on each menu item. */
	if (!for_replacement) {
		for (const auto &rt : _sorted_roadtypes) {
			if (!HasBit(used_roadtypes, rt)) continue;
			const RoadTypeInfo *rti = GetRoadTypeInfo(rt);
			d = maxdim(d, GetSpriteSize(rti->gui_sprites.build_x_road));
		}
	}

	for (const auto &rt : _sorted_roadtypes) {
		/* If it's not used ever, don't show it to the user. */
		if (!HasBit(used_roadtypes, rt)) continue;

		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);

		SetDParam(0, rti->strings.menu_text);
		SetDParam(1, rti->max_speed / 2);
		if (for_replacement) {
			list.push_back(MakeDropDownListStringItem(rti->strings.replace_text, rt, !HasBit(avail_roadtypes, rt)));
		} else {
			StringID str = rti->max_speed > 0 ? STR_TOOLBAR_RAILTYPE_VELOCITY : STR_JUST_STRING;
			list.push_back(MakeDropDownListIconItem(d, rti->gui_sprites.build_x_road, PAL_NONE, str, rt, !HasBit(avail_roadtypes, rt)));
		}
	}

	if (list.empty()) {
		/* Empty dropdowns are not allowed */
		list.push_back(MakeDropDownListStringItem(STR_NONE, INVALID_ROADTYPE, true));
	}

	return list;
}

DropDownList GetScenRoadTypeDropDownList(RoadTramTypes rtts)
{
	RoadTypes avail_roadtypes = GetRoadTypes(false);
	avail_roadtypes = AddDateIntroducedRoadTypes(avail_roadtypes, TimerGameCalendar::date);
	RoadTypes used_roadtypes = GetRoadTypes(true);

	/* Filter listed road types */
	if (!HasBit(rtts, RTT_ROAD)) used_roadtypes &= _roadtypes_type;
	if (!HasBit(rtts, RTT_TRAM)) used_roadtypes &= ~_roadtypes_type;

	DropDownList list;

	/* If it's not used ever, don't show it to the user. */
	Dimension d = { 0, 0 };
	for (const auto &rt : _sorted_roadtypes) {
		if (!HasBit(used_roadtypes, rt)) continue;
		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);
		d = maxdim(d, GetSpriteSize(rti->gui_sprites.build_x_road));
	}
	for (const auto &rt : _sorted_roadtypes) {
		if (!HasBit(used_roadtypes, rt)) continue;

		const RoadTypeInfo *rti = GetRoadTypeInfo(rt);

		SetDParam(0, rti->strings.menu_text);
		SetDParam(1, rti->max_speed / 2);
		StringID str = rti->max_speed > 0 ? STR_TOOLBAR_RAILTYPE_VELOCITY : STR_JUST_STRING;
		list.push_back(MakeDropDownListIconItem(d, rti->gui_sprites.build_x_road, PAL_NONE, str, rt, !HasBit(avail_roadtypes, rt)));
	}

	if (list.empty()) {
		/* Empty dropdowns are not allowed */
		list.push_back(MakeDropDownListStringItem(STR_NONE, -1, true));
	}

	return list;
}
