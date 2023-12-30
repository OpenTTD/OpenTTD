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
#include "engine_base.h"
#include "strings_func.h"
#include "core/geometry_func.hpp"
#include "station_cmd.h"
#include "road_cmd.h"
#include "tunnelbridge_cmd.h"
#include "newgrf_roadstop.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "string_func.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "widgets/road_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static void ShowRVStationPicker(Window *parent, RoadStopType rs);
static void ShowRoadDepotPicker(Window *parent);

static bool _remove_button_clicked;
static bool _one_way_button_clicked;

static Axis _place_road_dir;
static bool _place_road_start_half_x;
static bool _place_road_start_half_y;
static bool _place_road_end_half;

static RoadType _cur_roadtype;

static DiagDirection _road_depot_orientation;

struct RoadStopGUISettings {
	DiagDirection orientation;

	RoadStopClassID roadstop_class;
	uint16_t roadstop_type;
	uint16_t roadstop_count;
};
static RoadStopGUISettings _roadstop_gui_settings;

/**
 * Check whether a road stop type can be built.
 * @return true if building is allowed.
 */
static bool IsRoadStopAvailable(const RoadStopSpec *roadstopspec, StationType type)
{
	if (roadstopspec == nullptr) return true;

	if (HasBit(roadstopspec->flags, RSF_BUILD_MENU_ROAD_ONLY) && !RoadTypeIsRoad(_cur_roadtype)) return false;
	if (HasBit(roadstopspec->flags, RSF_BUILD_MENU_TRAM_ONLY) && !RoadTypeIsTram(_cur_roadtype)) return false;

	if (roadstopspec->stop_type != ROADSTOPTYPE_ALL) {
		switch (type) {
			case STATION_BUS:   if (roadstopspec->stop_type != ROADSTOPTYPE_PASSENGER) return false; break;
			case STATION_TRUCK: if (roadstopspec->stop_type != ROADSTOPTYPE_FREIGHT)   return false; break;
			default: break;
		}
	}

	if (!HasBit(roadstopspec->callback_mask, CBM_ROAD_STOP_AVAIL)) return true;

	uint16_t cb_res = GetRoadStopCallback(CBID_STATION_AVAILABILITY, 0, 0, roadstopspec, nullptr, INVALID_TILE, _cur_roadtype, type, 0);
	if (cb_res == CALLBACK_FAILED) return true;

	return Convert8bitBooleanCallback(roadstopspec->grf_prop.grffile, CBID_STATION_AVAILABILITY, cb_res);
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

void CcRoadDepot(Commands, const CommandCost &result, TileIndex tile, RoadType, DiagDirection dir)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_1F_CONSTRUCTION_OTHER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	ConnectRoadToStructure(tile, dir);
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
	DiagDirection ddir = _roadstop_gui_settings.orientation;
	bool drive_through = ddir >= DIAGDIR_END;
	if (drive_through) ddir = static_cast<DiagDirection>(ddir - DIAGDIR_END); // Adjust picker result to actual direction.
	RoadStopClassID spec_class = _roadstop_gui_settings.roadstop_class;
	uint16_t spec_index = _roadstop_gui_settings.roadstop_type;

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
 * Callback for placing a bus station.
 * @param tile Position to place the station.
 */
static void PlaceRoad_BusStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_REMOVE_BUSSTOP);
	} else {
		if (_roadstop_gui_settings.orientation < DIAGDIR_END) { // Not a drive-through stop.
			VpStartPlaceSizing(tile, (DiagDirToAxis(_roadstop_gui_settings.orientation) == AXIS_X) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_BUSSTOP);
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
		if (_roadstop_gui_settings.orientation < DIAGDIR_END) { // Not a drive-through stop.
			VpStartPlaceSizing(tile, (DiagDirToAxis(_roadstop_gui_settings.orientation) == AXIS_X) ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_TRUCKSTOP);
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

	BuildRoadToolbarWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->Initialize(_cur_roadtype);
		this->InitNested(window_number);
		this->SetupRoadToolbar();
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
			WID_ROT_BUS_STATION,
			WID_ROT_TRUCK_STATION);
		if (!can_build) {
			CloseWindowById(WC_BUS_STATION, TRANSPORT_ROAD);
			CloseWindowById(WC_TRUCK_STATION, TRANSPORT_ROAD);
			CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_ROAD);
		}

		if (_game_mode != GM_EDITOR) {
			if (!can_build) {
				/* Show in the tooltip why this button is disabled. */
				this->GetWidget<NWidgetCore>(WID_ROT_DEPOT)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_ROT_BUS_STATION)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_ROT_TRUCK_STATION)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
			} else {
				this->GetWidget<NWidgetCore>(WID_ROT_DEPOT)->SetToolTip(rtt == RTT_ROAD ? STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_VEHICLE_DEPOT : STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAM_VEHICLE_DEPOT);
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
				Command<CMD_BUILD_ROAD_DEPOT>::Post(this->rti->strings.err_depot, CcRoadDepot,
						tile, _cur_roadtype, _road_depot_orientation);
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
		CloseWindowById(WC_SELECT_STATION, 0);
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

				case DDSP_BUILD_BUSSTOP:
				case DDSP_REMOVE_BUSSTOP:
					if (this->IsWidgetLowered(WID_ROT_BUS_STATION) && GetIfClassHasNewStopsByType(RoadStopClass::Get(_roadstop_gui_settings.roadstop_class), ROADSTOP_BUS, _cur_roadtype)) {
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
					if (this->IsWidgetLowered(WID_ROT_TRUCK_STATION) && GetIfClassHasNewStopsByType(RoadStopClass::Get(_roadstop_gui_settings.roadstop_class), ROADSTOP_TRUCK, _cur_roadtype)) {
						if (_remove_button_clicked) {
							TileArea ta(start_tile, end_tile);
							Command<CMD_REMOVE_ROAD_STOP>::Post(this->rti->strings.err_remove_station[ROADSTOP_TRUCK], CcPlaySound_CONSTRUCTION_OTHER, ta.tile, ta.w, ta.h, ROADSTOP_TRUCK, _ctrl_pressed);
						} else {
							PlaceRoadStop(start_tile, end_tile, ROADSTOP_TRUCK, _ctrl_pressed, _cur_roadtype, this->rti->strings.err_build_station[ROADSTOP_TRUCK]);
						}
					}
					break;

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

	/**
	 * Handler for global hotkeys of the BuildRoadToolbarWindow.
	 * @param hotkey Hotkey
	 * @param last_build Last build road type
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState RoadTramToolbarGlobalHotkeys(int hotkey, RoadType last_build, RoadTramType rtt)
	{
		Window* w = nullptr;
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
		Hotkey('B', "bridge", WID_ROT_BUILD_BRIDGE),
		Hotkey('T', "tunnel", WID_ROT_BUILD_TUNNEL),
		Hotkey('R', "remove", WID_ROT_REMOVE),
		Hotkey('C', "convert", WID_ROT_CONVERT_ROAD),
	}, TramToolbarGlobalHotkeys};
};

static const NWidgetPart _nested_build_road_widgets[] = {
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

static WindowDesc _build_road_desc(__FILE__, __LINE__,
	WDP_ALIGN_TOOLBAR, "toolbar_road", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_road_widgets), std::end(_nested_build_road_widgets),
	&BuildRoadToolbarWindow::road_hotkeys
);

static const NWidgetPart _nested_build_tramway_widgets[] = {
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

static WindowDesc _build_tramway_desc(__FILE__, __LINE__,
	WDP_ALIGN_TOOLBAR, "toolbar_tramway", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_tramway_widgets), std::end(_nested_build_tramway_widgets),
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

	return AllocateWindowDescFront<BuildRoadToolbarWindow>(RoadTypeIsRoad(_cur_roadtype) ? &_build_road_desc : &_build_tramway_desc, TRANSPORT_ROAD);
}

static const NWidgetPart _nested_build_road_scen_widgets[] = {
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

static WindowDesc _build_road_scen_desc(__FILE__, __LINE__,
	WDP_AUTO, "toolbar_road_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_road_scen_widgets), std::end(_nested_build_road_scen_widgets),
	&BuildRoadToolbarWindow::road_hotkeys
);

static const NWidgetPart _nested_build_tramway_scen_widgets[] = {
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

static WindowDesc _build_tramway_scen_desc(__FILE__, __LINE__,
	WDP_AUTO, "toolbar_tram_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_tramway_scen_widgets), std::end(_nested_build_tramway_scen_widgets),
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

	return AllocateWindowDescFront<BuildRoadToolbarWindow>(RoadTypeIsRoad(_cur_roadtype) ? &_build_road_scen_desc : &_build_tramway_scen_desc, TRANSPORT_ROAD);
}

struct BuildRoadDepotWindow : public PickerWindowBase {
	BuildRoadDepotWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->CreateNestedTree();

		this->LowerWidget(_road_depot_orientation + WID_BROD_DEPOT_NE);
		if (RoadTypeIsTram(_cur_roadtype)) {
			this->GetWidget<NWidgetCore>(WID_BROD_CAPTION)->widget_data = STR_BUILD_DEPOT_TRAM_ORIENTATION_CAPTION;
			for (WidgetID i = WID_BROD_DEPOT_NE; i <= WID_BROD_DEPOT_NW; i++) {
				this->GetWidget<NWidgetCore>(i)->tool_tip = STR_BUILD_DEPOT_TRAM_ORIENTATION_SELECT_TOOLTIP;
			}
		}

		this->FinishInitNested(TRANSPORT_ROAD);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (!IsInsideMM(widget, WID_BROD_DEPOT_NE, WID_BROD_DEPOT_NW + 1)) return;

		size->width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
		size->height = ScaleGUITrad(48) + WidgetDimensions::scaled.fullbevel.Vertical();
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
				this->RaiseWidget(_road_depot_orientation + WID_BROD_DEPOT_NE);
				_road_depot_orientation = (DiagDirection)(widget - WID_BROD_DEPOT_NE);
				this->LowerWidget(_road_depot_orientation + WID_BROD_DEPOT_NE);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
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
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BROD_CAPTION), SetDataTip(STR_BUILD_DEPOT_ROAD_ORIENTATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_NW), SetMinimalSize(66, 50), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_SW), SetMinimalSize(66, 50), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_NE), SetMinimalSize(66, 50), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROD_DEPOT_SE), SetMinimalSize(66, 50), SetFill(0, 0), SetDataTip(0x0, STR_BUILD_DEPOT_ROAD_ORIENTATION_SELECT_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_road_depot_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_road_depot_widgets), std::end(_nested_build_road_depot_widgets)
);

static void ShowRoadDepotPicker(Window *parent)
{
	new BuildRoadDepotWindow(&_build_road_depot_desc, parent);
}

/** Enum referring to the Hotkeys in the build road stop window */
enum BuildRoadStopHotkeys {
	BROSHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
};

struct BuildRoadStationWindow : public PickerWindowBase {
private:
	RoadStopType roadStopType; ///< The RoadStopType for this Window.
	uint line_height; ///< Height of a single line in the newstation selection matrix.
	uint coverage_height; ///< Height of the coverage texts.
	Scrollbar *vscrollList; ///< Vertical scrollbar of the new station list.
	Scrollbar *vscrollMatrix; ///< Vertical scrollbar of the station picker matrix.

	typedef GUIList<RoadStopClassID, std::nullptr_t, StringFilter &> GUIRoadStopClassList; ///< Type definition for the list to hold available road stop classes.

	static const uint EDITBOX_MAX_SIZE = 16; ///< The maximum number of characters for the filter edit box.

	static Listing   last_sorting;           ///< Default sorting of #GUIRoadStopClassList.
	static Filtering last_filtering;         ///< Default filtering of #GUIRoadStopClassList.
	static GUIRoadStopClassList::SortFunction * const sorter_funcs[];   ///< Sort functions of the #GUIRoadStopClassList.
	static GUIRoadStopClassList::FilterFunction * const filter_funcs[]; ///< Filter functions of the #GUIRoadStopClassList.
	GUIRoadStopClassList roadstop_classes;   ///< Available road stop classes.
	StringFilter string_filter;              ///< Filter for available road stop classes.
	QueryString filter_editbox;              ///< Filter editbox.

	void EnsureSelectedClassIsVisible()
	{
		uint pos = 0;
		for (auto rs_class : this->roadstop_classes) {
			if (rs_class == _roadstop_gui_settings.roadstop_class) break;
			pos++;
		}
		this->vscrollList->SetCount(this->roadstop_classes.size());
		this->vscrollList->ScrollTowards(pos);
	}

	void CheckOrientationValid()
	{
		if (_roadstop_gui_settings.orientation >= DIAGDIR_END) return;
		const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpec(_roadstop_gui_settings.roadstop_type);
		if (spec != nullptr && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) {
			this->RaiseWidget(_roadstop_gui_settings.orientation + WID_BROS_STATION_NE);
			_roadstop_gui_settings.orientation = DIAGDIR_END;
			this->LowerWidget(_roadstop_gui_settings.orientation + WID_BROS_STATION_NE);
			this->SetDirty();
			CloseWindowById(WC_SELECT_STATION, 0);
		}
	}

public:
	BuildRoadStationWindow(WindowDesc *desc, Window *parent, RoadStopType rs) : PickerWindowBase(desc, parent), filter_editbox(EDITBOX_MAX_SIZE * MAX_CHAR_LENGTH, EDITBOX_MAX_SIZE)
	{
		this->coverage_height = 2 * GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
		this->vscrollList = nullptr;
		this->vscrollMatrix = nullptr;
		this->roadStopType = rs;
		bool newstops = GetIfNewStopsByType(rs, _cur_roadtype);

		this->CreateNestedTree();

		/* Hide the station class filter if no stations other than the default one are available. */
		this->GetWidget<NWidgetStacked>(WID_BROS_SHOW_NEWST_DEFSIZE)->SetDisplayedPlane(newstops ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BROS_FILTER_CONTAINER)->SetDisplayedPlane(newstops ? 0 : SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_BROS_SHOW_NEWST_ADDITIONS)->SetDisplayedPlane(newstops ? 0 : SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_BROS_SHOW_NEWST_ORIENTATION)->SetDisplayedPlane(newstops ? 0 : SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_BROS_SHOW_NEWST_TYPE_SEL)->SetDisplayedPlane(newstops ? 0 : SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_BROS_SHOW_NEWST_MATRIX)->SetDisplayedPlane(newstops ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BROS_SHOW_NEWST_RESIZE)->SetDisplayedPlane(newstops ? 0 : SZSP_NONE);
		if (newstops) {
			this->vscrollList = this->GetScrollbar(WID_BROS_NEWST_SCROLL);
			this->vscrollMatrix = this->GetScrollbar(WID_BROS_MATRIX_SCROLL);

			this->querystrings[WID_BROS_FILTER_EDITBOX] = &this->filter_editbox;
			this->roadstop_classes.SetListing(this->last_sorting);
			this->roadstop_classes.SetFiltering(this->last_filtering);
			this->roadstop_classes.SetSortFuncs(this->sorter_funcs);
			this->roadstop_classes.SetFilterFuncs(this->filter_funcs);
		}

		this->roadstop_classes.ForceRebuild();
		BuildRoadStopClassesAvailable();

		/* Trams don't have non-drivethrough stations */
		if (RoadTypeIsTram(_cur_roadtype) && _roadstop_gui_settings.orientation < DIAGDIR_END) {
			_roadstop_gui_settings.orientation = DIAGDIR_END;
		}
		const RoadTypeInfo *rti = GetRoadTypeInfo(_cur_roadtype);
		this->GetWidget<NWidgetCore>(WID_BROS_CAPTION)->widget_data = rti->strings.picker_title[rs];

		for (WidgetID i = RoadTypeIsTram(_cur_roadtype) ? WID_BROS_STATION_X : WID_BROS_STATION_NE; i < WID_BROS_LT_OFF; i++) {
			this->GetWidget<NWidgetCore>(i)->tool_tip = rti->strings.picker_tooltip[rs];
		}

		this->LowerWidget(_roadstop_gui_settings.orientation + WID_BROS_STATION_NE);
		this->LowerWidget(_settings_client.gui.station_show_coverage + WID_BROS_LT_OFF);

		this->FinishInitNested(TRANSPORT_ROAD);

		this->window_class = (rs == ROADSTOP_BUS) ? WC_BUS_STATION : WC_TRUCK_STATION;
		if (!newstops || _roadstop_gui_settings.roadstop_class >= (int)RoadStopClass::GetClassCount()) {
			/* There's no new stops available or the list has reduced in size.
			 * Now, set the default road stops as selected. */
			_roadstop_gui_settings.roadstop_class = ROADSTOP_CLASS_DFLT;
			_roadstop_gui_settings.roadstop_type = 0;
		}
		if (newstops) {
			/* The currently selected class doesn't have any stops for this RoadStopType, reset the selection. */
			if (!GetIfClassHasNewStopsByType(RoadStopClass::Get(_roadstop_gui_settings.roadstop_class), rs, _cur_roadtype)) {
				_roadstop_gui_settings.roadstop_class = ROADSTOP_CLASS_DFLT;
				_roadstop_gui_settings.roadstop_type = 0;
			}
			_roadstop_gui_settings.roadstop_count = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpecCount();
			_roadstop_gui_settings.roadstop_type = std::min((int)_roadstop_gui_settings.roadstop_type, _roadstop_gui_settings.roadstop_count - 1);

			/* Reset back to default class if the previously selected class is not available for this road stop type. */
			if (!GetIfClassHasNewStopsByType(RoadStopClass::Get(_roadstop_gui_settings.roadstop_class), roadStopType, _cur_roadtype)) {
				_roadstop_gui_settings.roadstop_class = ROADSTOP_CLASS_DFLT;
			}

			this->SelectFirstAvailableTypeIfUnavailable();

			NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BROS_MATRIX);
			matrix->SetScrollbar(this->vscrollMatrix);
			matrix->SetCount(_roadstop_gui_settings.roadstop_count);
			matrix->SetClicked(_roadstop_gui_settings.roadstop_type);

			this->EnsureSelectedClassIsVisible();
			this->CheckOrientationValid();
		}
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_STATION, 0);
		this->PickerWindowBase::Close();
	}

	/** Sort classes by RoadStopClassID. */
	static bool RoadStopClassIDSorter(RoadStopClassID const &a, RoadStopClassID const &b)
	{
		return a < b;
	}

	/** Filter classes by class name. */
	static bool CDECL TagNameFilter(RoadStopClassID const *sc, StringFilter &filter)
	{
		filter.ResetState();
		filter.AddLine(GetString(RoadStopClass::Get(*sc)->name));
		return filter.GetState();
	}

	inline bool ShowNewStops() const
	{
		return this->vscrollList != nullptr;
	}

	void BuildRoadStopClassesAvailable()
	{
		if (!this->roadstop_classes.NeedRebuild()) return;

		this->roadstop_classes.clear();

		for (uint i = 0; i < RoadStopClass::GetClassCount(); i++) {
			RoadStopClassID rs_id = (RoadStopClassID)i;
			if (rs_id == ROADSTOP_CLASS_WAYP) {
				// Skip waypoints.
				continue;
			}
			RoadStopClass *rs_class = RoadStopClass::Get(rs_id);
			if (GetIfClassHasNewStopsByType(rs_class, this->roadStopType, _cur_roadtype)) this->roadstop_classes.push_back(rs_id);
		}

		if (this->ShowNewStops()) {
			this->roadstop_classes.Filter(this->string_filter);
			this->roadstop_classes.shrink_to_fit();
			this->roadstop_classes.RebuildDone();
			this->roadstop_classes.Sort();

			this->vscrollList->SetCount(this->roadstop_classes.size());
		}
	}

	void SelectFirstAvailableTypeIfUnavailable()
	{
		const RoadStopClass *rs_class = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class);
		StationType st = GetRoadStationTypeByWindowClass(this->window_class);

		if (IsRoadStopAvailable(rs_class->GetSpec(_roadstop_gui_settings.roadstop_type), st)) return;
		for (uint i = 0; i < _roadstop_gui_settings.roadstop_count; i++) {
			if (IsRoadStopAvailable(rs_class->GetSpec(i), st)) {
				_roadstop_gui_settings.roadstop_type = i;
				break;
			}
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		this->BuildRoadStopClassesAvailable();
	}

	EventState OnHotkey(int hotkey) override
	{
		if (hotkey == BROSHK_FOCUS_FILTER_BOX) {
			this->SetFocusedWidget(WID_BROS_FILTER_EDITBOX);
			SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
			return ES_HANDLED;
		}

		return ES_NOT_HANDLED;
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (widget == WID_BROS_FILTER_EDITBOX) {
			string_filter.SetFilterTerm(this->filter_editbox.text.buf);
			this->roadstop_classes.SetFilterState(!string_filter.IsEmpty());
			this->roadstop_classes.ForceRebuild();
			this->InvalidateData();
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

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_BROS_NEWST_LIST: {
				Dimension d = { 0, 0 };
				for (auto rs_class : this->roadstop_classes) {
					d = maxdim(d, GetStringBoundingBox(RoadStopClass::Get(rs_class)->name));
				}
				size->width = std::max(size->width, d.width + padding.width);
				this->line_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.matrix.Vertical();
				size->height = 5 * this->line_height;
				resize->height = this->line_height;
				break;
			}

			case WID_BROS_SHOW_NEWST_TYPE: {
				Dimension d = {0, 0};
				StringID str = this->GetWidget<NWidgetCore>(widget)->widget_data;
				for (auto roadstop_class : this->roadstop_classes) {
					RoadStopClass *rs_class = RoadStopClass::Get(roadstop_class);
					for (uint j = 0; j < rs_class->GetSpecCount(); j++) {
						const RoadStopSpec *roadstopspec = rs_class->GetSpec(j);
						SetDParam(0, (roadstopspec != nullptr && roadstopspec->name != 0) ? roadstopspec->name : STR_STATION_CLASS_DFLT_ROADSTOP);
						d = maxdim(d, GetStringBoundingBox(str));
					}
				}
				size->width = std::max(size->width, d.width + padding.width);
				break;
			}

			case WID_BROS_STATION_NE:
			case WID_BROS_STATION_SE:
			case WID_BROS_STATION_SW:
			case WID_BROS_STATION_NW:
			case WID_BROS_STATION_X:
			case WID_BROS_STATION_Y:
			case WID_BROS_IMAGE:
				size->width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size->height = ScaleGUITrad(48) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;

			case WID_BROS_MATRIX:
				fill->height = 1;
				resize->height = 1;
				break;

			case WID_BROS_ACCEPTANCE:
				size->height = this->coverage_height;
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
				const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpec(_roadstop_gui_settings.roadstop_type);
				bool disabled = (spec != nullptr && widget < WID_BROS_STATION_X && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY));
				DrawPixelInfo tmp_dpi;
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
					int y = (ir.Height() + ScaleSpriteTrad(48)) / 2 - ScaleSpriteTrad(31);
					if (spec == nullptr || disabled) {
						StationPickerDrawSprite(x, y, st, INVALID_RAILTYPE, _cur_roadtype, widget - WID_BROS_STATION_NE);
					} else {
						DrawRoadStopTile(x, y, _cur_roadtype, spec, st, widget - WID_BROS_STATION_NE);
					}
				}
				if (disabled) GfxFillRect(ir, PC_BLACK, FILLRECT_CHECKER);
				break;
			}

			case WID_BROS_NEWST_LIST: {
				uint statclass = 0;
				uint row = 0;
				for (auto rs_class : this->roadstop_classes) {
					if (this->vscrollList->IsVisible(statclass)) {
						DrawString(r.left + WidgetDimensions::scaled.matrix.left, r.right, row * this->line_height + r.top + WidgetDimensions::scaled.matrix.top,
								RoadStopClass::Get(rs_class)->name,
								rs_class == _roadstop_gui_settings.roadstop_class ? TC_WHITE : TC_BLACK);
						row++;
					}
					statclass++;
				}
				break;
			}

			case WID_BROS_IMAGE: {
				uint16_t type = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
				assert(type < _roadstop_gui_settings.roadstop_count);

				const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpec(type);
				StationType st = GetRoadStationTypeByWindowClass(this->window_class);

				/* Set up a clipping area for the sprite preview. */
				DrawPixelInfo tmp_dpi;
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
					int y = (ir.Height() + ScaleSpriteTrad(48)) / 2 - ScaleSpriteTrad(31);
					if (spec == nullptr) {
						StationPickerDrawSprite(x, y, st, INVALID_RAILTYPE, _cur_roadtype, _roadstop_gui_settings.orientation);
					} else {
						DiagDirection orientation = _roadstop_gui_settings.orientation;
						if (orientation < DIAGDIR_END && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) orientation = DIAGDIR_END;
						DrawRoadStopTile(x, y, _cur_roadtype, spec, st, (uint8_t)orientation);
					}
				}
				if (!IsRoadStopAvailable(spec, st)) {
					GfxFillRect(ir, PC_BLACK, FILLRECT_CHECKER);
				}
				break;
			}
		}
	}

	void OnResize() override
	{
		if (this->vscrollList != nullptr) {
			this->vscrollList->SetCapacityFromWidget(this, WID_BROS_NEWST_LIST);
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_BROS_SHOW_NEWST_TYPE) {
			const RoadStopSpec *roadstopspec = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpec(_roadstop_gui_settings.roadstop_type);
			SetDParam(0, (roadstopspec != nullptr && roadstopspec->name != 0) ? roadstopspec->name : STR_STATION_CLASS_DFLT_ROADSTOP);
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
					const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpec(_roadstop_gui_settings.roadstop_type);
					if (spec != nullptr && HasBit(spec->flags, RSF_DRIVE_THROUGH_ONLY)) return;
				}
				this->RaiseWidget(_roadstop_gui_settings.orientation + WID_BROS_STATION_NE);
				_roadstop_gui_settings.orientation = (DiagDirection)(widget - WID_BROS_STATION_NE);
				this->LowerWidget(_roadstop_gui_settings.orientation + WID_BROS_STATION_NE);
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

			case WID_BROS_NEWST_LIST: {
				auto it = this->vscrollList->GetScrolledItemFromWidget(this->roadstop_classes, pt.y, this, WID_BROS_NEWST_LIST);
				if (it == this->roadstop_classes.end()) return;
				RoadStopClassID class_id = *it;
				if (_roadstop_gui_settings.roadstop_class != class_id && GetIfClassHasNewStopsByType(RoadStopClass::Get(class_id), roadStopType, _cur_roadtype)) {
					_roadstop_gui_settings.roadstop_class = class_id;
					RoadStopClass *rsclass = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class);
					_roadstop_gui_settings.roadstop_count = rsclass->GetSpecCount();
					_roadstop_gui_settings.roadstop_type = std::min((int)_roadstop_gui_settings.roadstop_type, std::max(0, (int)_roadstop_gui_settings.roadstop_count - 1));
					this->SelectFirstAvailableTypeIfUnavailable();

					NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BROS_MATRIX);
					matrix->SetCount(_roadstop_gui_settings.roadstop_count);
					matrix->SetClicked(_roadstop_gui_settings.roadstop_type);
					this->CheckOrientationValid();
				}
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;
			}

			case WID_BROS_IMAGE: {
				uint16_t y = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
				if (y >= _roadstop_gui_settings.roadstop_count) return;

				const RoadStopSpec *spec = RoadStopClass::Get(_roadstop_gui_settings.roadstop_class)->GetSpec(y);
				StationType st = GetRoadStationTypeByWindowClass(this->window_class);

				if (!IsRoadStopAvailable(spec, st)) return;

				_roadstop_gui_settings.roadstop_type = y;

				this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->SetClicked(_roadstop_gui_settings.roadstop_type);

				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				this->CheckOrientationValid();
				break;
			}

			default:
				break;
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawStationCoverage(this);
	}

	IntervalTimer<TimerGameCalendar> yearly_interval = {{TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [this](auto) {
		this->InvalidateData();
	}};

	static inline HotkeyList hotkeys{"buildroadstop", {
		Hotkey('F', "focus_filter_box", BROSHK_FOCUS_FILTER_BOX),
	}};
};

Listing BuildRoadStationWindow::last_sorting = { false, 0 };
Filtering BuildRoadStationWindow::last_filtering = { false, 0 };

BuildRoadStationWindow::GUIRoadStopClassList::SortFunction * const BuildRoadStationWindow::sorter_funcs[] = {
	&RoadStopClassIDSorter,
};

BuildRoadStationWindow::GUIRoadStopClassList::FilterFunction * const BuildRoadStationWindow::filter_funcs[] = {
	&TagNameFilter,
};

/** Widget definition of the build road station window */
static const NWidgetPart _nested_road_station_picker_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION,  COLOUR_DARK_GREEN, WID_BROS_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_DEFSIZE),
			NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_FILTER_CONTAINER),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
								NWidget(WWT_TEXT, COLOUR_DARK_GREEN), SetFill(0, 1), SetDataTip(STR_LIST_FILTER_TITLE, STR_NULL),
								NWidget(WWT_EDITBOX, COLOUR_GREY, WID_BROS_FILTER_EDITBOX), SetFill(1, 0), SetResize(1, 0),
										SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_ADDITIONS),
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_MATRIX, COLOUR_GREY, WID_BROS_NEWST_LIST), SetMinimalSize(122, 71), SetFill(1, 0),
									SetMatrixDataTip(1, 0, STR_STATION_BUILD_STATION_CLASS_TOOLTIP), SetScrollbar(WID_BROS_NEWST_SCROLL),
								NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BROS_NEWST_SCROLL),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_ORIENTATION),
							NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetDataTip(STR_STATION_BUILD_ORIENTATION, STR_NULL), SetFill(1, 0),
						EndContainer(),
						NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
								NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_NW), SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_NE), SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
								EndContainer(),
								NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_X),  SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
							EndContainer(),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
								NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_SW), SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
									NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_SE), SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
								EndContainer(),
								NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_Y),  SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_TYPE_SEL),
							NWidget(WWT_LABEL, COLOUR_DARK_GREEN, WID_BROS_SHOW_NEWST_TYPE), SetMinimalSize(144, 8), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_ORANGE), SetFill(1, 0),
						EndContainer(),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_OFF), SetMinimalSize(60, 12),
									SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_ON), SetMinimalSize(60, 12),
									SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_MATRIX),
						/* Hidden panel as NWID_MATRIX does not support SetScrollbar() */
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BROS_MATRIX_SCROLL),
							NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BROS_MATRIX), SetPIP(0, 2, 0),
								NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BROS_IMAGE), SetMinimalSize(66, 60),
										SetFill(0, 0), SetResize(0, 0), SetDataTip(0x0, STR_STATION_BUILD_STATION_TYPE_TOOLTIP), SetScrollbar(WID_BROS_MATRIX_SCROLL),
								EndContainer(),
							EndContainer(),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BROS_ACCEPTANCE), SetFill(1, 1), SetResize(1, 0), SetMinimalTextLines(2, WidgetDimensions::unscaled.vsep_normal),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_RESIZE),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BROS_MATRIX_SCROLL),
				NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _road_station_picker_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_station_road", 0, 0,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_road_station_picker_widgets), std::end(_nested_road_station_picker_widgets)
);

/** Widget definition of the build tram station window */
static const NWidgetPart _nested_tram_station_picker_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION,  COLOUR_DARK_GREEN, WID_BROS_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_DEFSIZE),
			NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_FILTER_CONTAINER),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
								NWidget(WWT_TEXT, COLOUR_DARK_GREEN), SetFill(0, 1), SetDataTip(STR_LIST_FILTER_TITLE, STR_NULL),
								NWidget(WWT_EDITBOX, COLOUR_GREY, WID_BROS_FILTER_EDITBOX), SetFill(1, 0), SetResize(1, 0),
										SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_ADDITIONS),
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_MATRIX, COLOUR_GREY, WID_BROS_NEWST_LIST), SetMinimalSize(122, 71), SetFill(1, 0),
									SetMatrixDataTip(1, 0, STR_STATION_BUILD_STATION_CLASS_TOOLTIP), SetScrollbar(WID_BROS_NEWST_SCROLL),
								NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BROS_NEWST_SCROLL),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_ORIENTATION),
							NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetDataTip(STR_STATION_BUILD_ORIENTATION, STR_NULL), SetFill(1, 0),
						EndContainer(),
						NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_X),  SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_BROS_STATION_Y),  SetMinimalSize(66, 50), SetFill(0, 0), EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_TYPE_SEL),
							NWidget(WWT_LABEL, COLOUR_DARK_GREEN, WID_BROS_SHOW_NEWST_TYPE), SetMinimalSize(144, 8), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_ORANGE), SetFill(1, 0),
						EndContainer(),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_OFF), SetMinimalSize(60, 12),
									SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BROS_LT_ON), SetMinimalSize(60, 12),
									SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_MATRIX),
						/* Hidden panel as NWID_MATRIX does not support SetScrollbar() */
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BROS_MATRIX_SCROLL),
							NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BROS_MATRIX), SetPIP(0, 2, 0),
								NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BROS_IMAGE), SetMinimalSize(66, 60),
										SetFill(0, 0), SetResize(0, 0), SetDataTip(0x0, STR_STATION_BUILD_STATION_TYPE_TOOLTIP), SetScrollbar(WID_BROS_MATRIX_SCROLL),
								EndContainer(),
							EndContainer(),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BROS_ACCEPTANCE), SetFill(1, 1), SetResize(1, 0), SetMinimalTextLines(2, WidgetDimensions::unscaled.vsep_normal),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BROS_SHOW_NEWST_RESIZE),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BROS_MATRIX_SCROLL),
				NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _tram_station_picker_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_station_tram", 0, 0,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_tram_station_picker_widgets), std::end(_nested_tram_station_picker_widgets)
);

static void ShowRVStationPicker(Window *parent, RoadStopType rs)
{
	new BuildRoadStationWindow(RoadTypeIsRoad(_cur_roadtype) ? &_road_station_picker_desc : &_tram_station_picker_desc, parent, rs);
}

void InitializeRoadGui()
{
	_road_depot_orientation = DIAGDIR_NW;
	_roadstop_gui_settings.orientation = DIAGDIR_NW;
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
		list.push_back(std::make_unique<DropDownListStringItem>(STR_REPLACE_ALL_ROADTYPE, INVALID_ROADTYPE, false));
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
			list.push_back(std::make_unique<DropDownListStringItem>(rti->strings.replace_text, rt, !HasBit(avail_roadtypes, rt)));
		} else {
			StringID str = rti->max_speed > 0 ? STR_TOOLBAR_RAILTYPE_VELOCITY : STR_JUST_STRING;
			list.push_back(std::make_unique<DropDownListIconItem>(d, rti->gui_sprites.build_x_road, PAL_NONE, str, rt, !HasBit(avail_roadtypes, rt)));
		}
	}

	if (list.empty()) {
		/* Empty dropdowns are not allowed */
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NONE, INVALID_ROADTYPE, true));
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
		list.push_back(std::make_unique<DropDownListIconItem>(d, rti->gui_sprites.build_x_road, PAL_NONE, str, rt, !HasBit(avail_roadtypes, rt)));
	}

	if (list.empty()) {
		/* Empty dropdowns are not allowed */
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NONE, -1, true));
	}

	return list;
}
