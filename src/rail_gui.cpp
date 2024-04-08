/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_gui.cpp %File for dealing with rail construction user interface */

#include "stdafx.h"
#include "gui.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "window_gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "viewport_func.h"
#include "command_func.h"
#include "waypoint_func.h"
#include "newgrf_station.h"
#include "company_base.h"
#include "strings_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "tunnelbridge.h"
#include "tilehighlight_func.h"
#include "spritecache.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"
#include "engine_base.h"
#include "vehicle_func.h"
#include "zoom_func.h"
#include "rail_gui.h"
#include "station_cmd.h"
#include "tunnelbridge_cmd.h"
#include "waypoint_cmd.h"
#include "rail_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "picker_gui.h"
#include "depot_func.h"

#include "station_map.h"
#include "tunnelbridge_map.h"

#include "widgets/rail_widget.h"

#include "safeguards.h"


static RailType _cur_railtype;               ///< Rail type of the current build-rail toolbar.
static bool _remove_button_clicked;          ///< Flag whether 'remove' toggle-button is currently enabled
static DiagDirection _build_depot_direction; ///< Currently selected depot direction
static bool _convert_signal_button;          ///< convert signal button in the signal GUI pressed
static SignalVariant _cur_signal_variant;    ///< set the signal variant (for signal GUI)
static SignalType _cur_signal_type;          ///< set the signal type (for signal GUI)

struct WaypointPickerSelection {
	StationClassID sel_class; ///< Selected station class.
	uint16_t sel_type; ///< Selected station type within the class.
};
static WaypointPickerSelection _waypoint_gui; ///< Settings of the waypoint picker.

struct StationPickerSelection {
	StationClassID sel_class; ///< Selected station class.
	uint16_t sel_type; ///< Selected station type within the class.
	Axis axis; ///< Selected orientation of the station.
};
static StationPickerSelection _station_gui; ///< Settings of the station picker.


static void HandleStationPlacement(TileIndex start, TileIndex end);
static void ShowBuildTrainDepotPicker(Window *parent);
static void ShowBuildWaypointPicker(Window *parent);
static Window *ShowStationBuilder(Window *parent);
static void ShowSignalBuilder(Window *parent);

/**
 * Check whether a station type can be build.
 * @return true if building is allowed.
 */
static bool IsStationAvailable(const StationSpec *statspec)
{
	if (statspec == nullptr || !HasBit(statspec->callback_mask, CBM_STATION_AVAIL)) return true;

	uint16_t cb_res = GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, nullptr, INVALID_TILE);
	if (cb_res == CALLBACK_FAILED) return true;

	return Convert8bitBooleanCallback(statspec->grf_prop.grffile, CBID_STATION_AVAILABILITY, cb_res);
}

void CcPlaySound_CONSTRUCTION_RAIL(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Succeeded() && _settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, tile);
}

static void GenericPlaceRail(TileIndex tile, Track track)
{
	if (_remove_button_clicked) {
		Command<CMD_REMOVE_SINGLE_RAIL>::Post(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK, CcPlaySound_CONSTRUCTION_RAIL,
				tile, track);
	} else {
		Command<CMD_BUILD_SINGLE_RAIL>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, CcPlaySound_CONSTRUCTION_RAIL,
				tile, _cur_railtype, track, _settings_client.gui.auto_remove_signals);
	}
}

/**
 * Try to add an additional rail-track at the entrance of a depot
 * @param tile  Tile to use for adding the rail-track
 * @param dir   Direction to check for already present tracks
 * @param track Track to add
 * @see CcRailDepot()
 */
static void PlaceExtraDepotRail(TileIndex tile, DiagDirection dir, Track track)
{
	if (IsRailDepot(tile)) return;
	if (GetRailTileType(tile) == RAIL_TILE_SIGNALS && !_settings_client.gui.auto_remove_signals) return;
	if ((GetTrackBits(tile) & DiagdirReachesTracks(dir)) == 0) return;

	Command<CMD_BUILD_SINGLE_RAIL>::Post(tile, _cur_railtype, track, _settings_client.gui.auto_remove_signals);
}

/** Additional pieces of track to add at the entrance of a depot. */
static const Track _place_depot_extra_track[12] = {
	TRACK_LEFT,  TRACK_UPPER, TRACK_UPPER, TRACK_RIGHT, // First additional track for directions 0..3
	TRACK_X,     TRACK_Y,     TRACK_X,     TRACK_Y,     // Second additional track
	TRACK_LOWER, TRACK_LEFT,  TRACK_RIGHT, TRACK_LOWER, // Third additional track
};

/** Direction to check for existing track pieces. */
static const DiagDirection _place_depot_extra_dir[12] = {
	DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE, DIAGDIR_SW,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE, DIAGDIR_SE,
	DIAGDIR_NW, DIAGDIR_NE, DIAGDIR_NW, DIAGDIR_NE,
};

void CcRailDepot(Commands, const CommandCost &result, TileIndex start_tile, RailType, DiagDirection dir, bool, DepotID, TileIndex end_tile)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, start_tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();

	TileArea ta(start_tile, end_tile);
	for (TileIndex t : ta) {
		TileIndex tile = t + TileOffsByDiagDir(dir);

		if (IsTileType(tile, MP_RAILWAY)) {
			PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir], _place_depot_extra_track[dir]);

			Tile double_depot_tile = tile + TileOffsByDiagDir(dir);
			bool is_double_depot = IsValidTile(double_depot_tile) && IsRailDepotTile(double_depot_tile);
			if (!is_double_depot) PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir + 4], _place_depot_extra_track[dir + 4]);

			PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir + 8], _place_depot_extra_track[dir + 8]);
		}
	}
}

/**
 * Place a rail waypoint.
 * @param tile Position to start dragging a waypoint.
 */
static void PlaceRail_Waypoint(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_REMOVE_STATION);
		return;
	}

	Axis axis = GetAxisForNewRailWaypoint(tile);
	if (IsValidAxis(axis)) {
		/* Valid tile for waypoints */
		VpStartPlaceSizing(tile, axis == AXIS_X ? VPM_X_LIMITED : VPM_Y_LIMITED, DDSP_BUILD_STATION);
		VpSetPlaceSizingLimit(_settings_game.station.station_spread);
	} else {
		/* Tile where we can't build rail waypoints. This is always going to fail,
		 * but provides the user with a proper error message. */
		Command<CMD_BUILD_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_WAYPOINT, tile, AXIS_X, 1, 1, STAT_CLASS_WAYP, 0, INVALID_STATION, false);
	}
}

void CcStation(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, tile);
	/* Only close the station builder window if the default station and non persistent building is chosen. */
	if (_station_gui.sel_class == STAT_CLASS_DFLT && _station_gui.sel_type == 0 && !_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

/**
 * Place a rail station.
 * @param tile Position to place or start dragging a station.
 */
static void PlaceRail_Station(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_REMOVE_STATION);
		VpSetPlaceSizingLimit(-1);
	} else if (_settings_client.gui.station_dragdrop) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_BUILD_STATION);
		VpSetPlaceSizingLimit(_settings_game.station.station_spread);
	} else {
		int w = _settings_client.gui.station_numtracks;
		int h = _settings_client.gui.station_platlength;
		if (!_station_gui.axis) Swap(w, h);

		StationPickerSelection params = _station_gui;
		RailType rt = _cur_railtype;
		uint8_t numtracks = _settings_client.gui.station_numtracks;
		uint8_t platlength = _settings_client.gui.station_platlength;
		bool adjacent = _ctrl_pressed;

		auto proc = [=](bool test, StationID to_join) -> bool {
			if (test) {
				return Command<CMD_BUILD_RAIL_STATION>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_RAIL_STATION>()), tile, rt, params.axis, numtracks, platlength, params.sel_class, params.sel_type, INVALID_STATION, adjacent).Succeeded();
			} else {
				return Command<CMD_BUILD_RAIL_STATION>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, CcStation, tile, rt, params.axis, numtracks, platlength, params.sel_class, params.sel_type, to_join, adjacent);
			}
		};

		ShowSelectStationIfNeeded(TileArea(tile, w, h), proc);
	}
}

/**
 * Build a new signal or edit/remove a present signal, use CmdBuildSingleSignal() or CmdRemoveSingleSignal() in rail_cmd.cpp
 *
 * @param tile The tile where the signal will build or edit
 */
static void GenericPlaceSignals(TileIndex tile)
{
	TrackBits trackbits = TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_RAIL, 0));

	if (trackbits & TRACK_BIT_VERT) { // N-S direction
		trackbits = (_tile_fract_coords.x <= _tile_fract_coords.y) ? TRACK_BIT_RIGHT : TRACK_BIT_LEFT;
	}

	if (trackbits & TRACK_BIT_HORZ) { // E-W direction
		trackbits = (_tile_fract_coords.x + _tile_fract_coords.y <= 15) ? TRACK_BIT_UPPER : TRACK_BIT_LOWER;
	}

	Track track = FindFirstTrack(trackbits);

	if (_remove_button_clicked) {
		Command<CMD_REMOVE_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, CcPlaySound_CONSTRUCTION_RAIL, tile, track);
	} else {
		/* Which signals should we cycle through? */
		bool tile_has_signal = IsPlainRailTile(tile) && IsValidTrack(track) && HasSignalOnTrack(tile, track);
		SignalType cur_signal_on_tile = tile_has_signal ? GetSignalType(tile, track) : _cur_signal_type;
		SignalType cycle_start;
		SignalType cycle_end;

		/* Start with the least restrictive case: the player wants to cycle through all signals they can see. */
		if (_settings_client.gui.cycle_signal_types == SIGNAL_CYCLE_ALL) {
			cycle_start = _settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL ? SIGTYPE_BLOCK : SIGTYPE_PBS;
			cycle_end = SIGTYPE_LAST;
		} else {
			/* Only cycle through signals of the same group (block or path) as the current signal on the tile. */
			if (cur_signal_on_tile <= SIGTYPE_LAST_NOPBS) {
				/* Block signals only. */
				cycle_start = SIGTYPE_BLOCK;
				cycle_end = SIGTYPE_LAST_NOPBS;
			} else {
				/* Path signals only. */
				cycle_start = SIGTYPE_PBS;
				cycle_end = SIGTYPE_LAST;
			}
		}

		if (FindWindowById(WC_BUILD_SIGNAL, 0) != nullptr) {
			/* signal GUI is used */
			Command<CMD_BUILD_SINGLE_SIGNAL>::Post(_convert_signal_button ? STR_ERROR_SIGNAL_CAN_T_CONVERT_SIGNALS_HERE : STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcPlaySound_CONSTRUCTION_RAIL,
				tile, track, _cur_signal_type, _cur_signal_variant, _convert_signal_button, false, _ctrl_pressed, cycle_start, cycle_end, 0, 0);
		} else {
			SignalVariant sigvar = TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC;
			Command<CMD_BUILD_SINGLE_SIGNAL>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcPlaySound_CONSTRUCTION_RAIL,
				tile, track, _settings_client.gui.default_signal_type, sigvar, false, false, _ctrl_pressed, cycle_start, cycle_end, 0, 0);

		}
	}
}

/**
 * Start placing a rail bridge.
 * @param tile Position of the first tile of the bridge.
 * @param w    Rail toolbar window.
 */
static void PlaceRail_Bridge(TileIndex tile, Window *w)
{
	if (IsBridgeTile(tile)) {
		TileIndex other_tile = GetOtherTunnelBridgeEnd(tile);
		Point pt = {0, 0};
		w->OnPlaceMouseUp(VPM_X_OR_Y, DDSP_BUILD_BRIDGE, pt, other_tile, tile);
	} else {
		VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
	}
}

/** Command callback for building a tunnel */
void CcBuildRailTunnel(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Succeeded()) {
		if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	} else {
		SetRedErrorSquare(_build_tunnel_endtile);
	}
}

/**
 * Toggles state of the Remove button of Build rail toolbar
 * @param w window the button belongs to
 */
static void ToggleRailButton_Remove(Window *w)
{
	CloseWindowById(WC_SELECT_STATION, 0);
	w->ToggleWidgetLoweredState(WID_RAT_REMOVE);
	w->SetWidgetDirty(WID_RAT_REMOVE);
	_remove_button_clicked = w->IsWidgetLowered(WID_RAT_REMOVE);
	SetSelectionRed(_remove_button_clicked);
}

/**
 * Updates the Remove button because of Ctrl state change
 * @param w window the button belongs to
 * @return true iff the remove button was changed
 */
static bool RailToolbar_CtrlChanged(Window *w)
{
	if (w->IsWidgetDisabled(WID_RAT_REMOVE)) return false;

	/* allow ctrl to switch remove mode only for these widgets */
	for (WidgetID i = WID_RAT_BUILD_NS; i <= WID_RAT_BUILD_STATION; i++) {
		if ((i <= WID_RAT_AUTORAIL || i >= WID_RAT_BUILD_WAYPOINT) && w->IsWidgetLowered(i)) {
			ToggleRailButton_Remove(w);
			return true;
		}
	}

	return false;
}


/**
 * The "remove"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbarWindow::OnClick()
 */
static void BuildRailClick_Remove(Window *w)
{
	if (w->IsWidgetDisabled(WID_RAT_REMOVE)) return;
	ToggleRailButton_Remove(w);
	if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);

	/* handle station builder */
	if (w->IsWidgetLowered(WID_RAT_BUILD_STATION)) {
		if (_remove_button_clicked) {
			/* starting drag & drop remove */
			if (!_settings_client.gui.station_dragdrop) {
				SetTileSelectSize(1, 1);
			} else {
				VpSetPlaceSizingLimit(-1);
			}
		} else {
			/* starting station build mode */
			if (!_settings_client.gui.station_dragdrop) {
				int x = _settings_client.gui.station_numtracks;
				int y = _settings_client.gui.station_platlength;
				if (_station_gui.axis == 0) Swap(x, y);
				SetTileSelectSize(x, y);
			} else {
				VpSetPlaceSizingLimit(_settings_game.station.station_spread);
			}
		}
	}
}

static void DoRailroadTrack(Track track)
{
	if (_remove_button_clicked) {
		Command<CMD_REMOVE_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_RAILROAD_TRACK, CcPlaySound_CONSTRUCTION_RAIL,
				TileVirtXY(_thd.selend.x, _thd.selend.y), TileVirtXY(_thd.selstart.x, _thd.selstart.y), track);
	} else {
		Command<CMD_BUILD_RAILROAD_TRACK>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, CcPlaySound_CONSTRUCTION_RAIL,
				TileVirtXY(_thd.selend.x, _thd.selend.y), TileVirtXY(_thd.selstart.x, _thd.selstart.y),  _cur_railtype, track, _settings_client.gui.auto_remove_signals, false);
	}
}

static void HandleAutodirPlacement()
{
	Track trackstat = static_cast<Track>( _thd.drawstyle & HT_DIR_MASK); // 0..5

	if (_thd.drawstyle & HT_RAIL) { // one tile case
		GenericPlaceRail(TileVirtXY(_thd.selend.x, _thd.selend.y), trackstat);
		return;
	}

	DoRailroadTrack(trackstat);
}

/**
 * Build new signals or remove signals or (if only one tile marked) edit a signal.
 *
 * If one tile marked abort and use GenericPlaceSignals()
 * else use CmdBuildSingleSignal() or CmdRemoveSingleSignal() in rail_cmd.cpp to build many signals
 */
static void HandleAutoSignalPlacement()
{
	Track track = (Track)GB(_thd.drawstyle, 0, 3); // 0..5

	if ((_thd.drawstyle & HT_DRAG_MASK) == HT_RECT) { // one tile case
		GenericPlaceSignals(TileVirtXY(_thd.selend.x, _thd.selend.y));
		return;
	}

	/* _settings_client.gui.drag_signals_density is given as a parameter such that each user
	 * in a network game can specify their own signal density */
	if (_remove_button_clicked) {
		Command<CMD_REMOVE_SIGNAL_TRACK>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, CcPlaySound_CONSTRUCTION_RAIL,
				TileVirtXY(_thd.selstart.x, _thd.selstart.y), TileVirtXY(_thd.selend.x, _thd.selend.y), track, _ctrl_pressed);
	} else {
		bool sig_gui = FindWindowById(WC_BUILD_SIGNAL, 0) != nullptr;
		SignalType sigtype = sig_gui ? _cur_signal_type : _settings_client.gui.default_signal_type;
		SignalVariant sigvar = sig_gui ? _cur_signal_variant : (TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC);
		Command<CMD_BUILD_SIGNAL_TRACK>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcPlaySound_CONSTRUCTION_RAIL,
				TileVirtXY(_thd.selstart.x, _thd.selstart.y), TileVirtXY(_thd.selend.x, _thd.selend.y), track, sigtype, sigvar, false, _ctrl_pressed, !_settings_client.gui.drag_signals_fixed_distance, _settings_client.gui.drag_signals_density);
	}
}


/** Rail toolbar management class. */
struct BuildRailToolbarWindow : Window {
	RailType railtype;    ///< Rail type to build.
	int last_user_action; ///< Last started user action.

	BuildRailToolbarWindow(WindowDesc &desc, RailType railtype) : Window(desc)
	{
		this->CreateNestedTree();
		this->SetupRailToolbar(railtype);
		this->FinishInitNested(TRANSPORT_RAIL);
		this->DisableWidget(WID_RAT_REMOVE);
		this->OnInvalidateData();
		this->last_user_action = INVALID_WID_RAT;

		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (this->IsWidgetLowered(WID_RAT_BUILD_STATION)) SetViewportCatchmentStation(nullptr, true);
		if (this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT)) SetViewportCatchmentWaypoint(nullptr, true);
		if (_settings_client.gui.link_terraform_toolbar) CloseWindowById(WC_SCEN_LAND_GEN, 0, false);
		CloseWindowById(WC_SELECT_STATION, 0);
		if (this->IsWidgetLowered(WID_RAT_BUILD_DEPOT)) SetViewportHighlightDepot(INVALID_DEPOT, true);
		this->Window::Close();
	}

	/** List of widgets to be disabled if infrastructure limit prevents building. */
	static inline const std::initializer_list<WidgetID> can_build_widgets = {
		WID_RAT_BUILD_NS, WID_RAT_BUILD_X, WID_RAT_BUILD_EW, WID_RAT_BUILD_Y, WID_RAT_AUTORAIL,
		WID_RAT_BUILD_DEPOT, WID_RAT_BUILD_WAYPOINT, WID_RAT_BUILD_STATION, WID_RAT_BUILD_SIGNALS,
		WID_RAT_BUILD_BRIDGE, WID_RAT_BUILD_TUNNEL, WID_RAT_CONVERT_RAIL,
	};

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		bool can_build = CanBuildVehicleInfrastructure(VEH_TRAIN);
		for (const WidgetID widget : can_build_widgets) this->SetWidgetDisabledState(widget, !can_build);
		if (!can_build) {
			CloseWindowById(WC_BUILD_SIGNAL, TRANSPORT_RAIL);
			CloseWindowById(WC_BUILD_STATION, TRANSPORT_RAIL);
			CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_RAIL);
			CloseWindowById(WC_BUILD_WAYPOINT, TRANSPORT_RAIL);
			CloseWindowById(WC_SELECT_STATION, 0);
		}
	}

	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override
	{
		bool can_build = CanBuildVehicleInfrastructure(VEH_TRAIN);
		if (can_build) return false;

		if (std::find(std::begin(can_build_widgets), std::end(can_build_widgets), widget) == std::end(can_build_widgets)) return false;

		GuiShowTooltips(this, STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE, close_cond);
		return true;
	}

	/**
	 * Configures the rail toolbar for railtype given
	 * @param railtype the railtype to display
	 */
	void SetupRailToolbar(RailType railtype)
	{
		this->railtype = railtype;
		const RailTypeInfo *rti = GetRailTypeInfo(railtype);

		assert(railtype < RAILTYPE_END);
		this->GetWidget<NWidgetCore>(WID_RAT_BUILD_NS)->widget_data     = rti->gui_sprites.build_ns_rail;
		this->GetWidget<NWidgetCore>(WID_RAT_BUILD_X)->widget_data      = rti->gui_sprites.build_x_rail;
		this->GetWidget<NWidgetCore>(WID_RAT_BUILD_EW)->widget_data     = rti->gui_sprites.build_ew_rail;
		this->GetWidget<NWidgetCore>(WID_RAT_BUILD_Y)->widget_data      = rti->gui_sprites.build_y_rail;
		this->GetWidget<NWidgetCore>(WID_RAT_AUTORAIL)->widget_data     = rti->gui_sprites.auto_rail;
		this->GetWidget<NWidgetCore>(WID_RAT_BUILD_DEPOT)->widget_data  = rti->gui_sprites.build_depot;
		this->GetWidget<NWidgetCore>(WID_RAT_CONVERT_RAIL)->widget_data = rti->gui_sprites.convert_rail;
		this->GetWidget<NWidgetCore>(WID_RAT_BUILD_TUNNEL)->widget_data = rti->gui_sprites.build_tunnel;
	}

	/**
	 * Switch to another rail type.
	 * @param railtype New rail type.
	 */
	void ModifyRailType(RailType railtype)
	{
		this->SetupRailToolbar(railtype);
		this->ReInit();
	}

	void UpdateRemoveWidgetStatus(WidgetID clicked_widget)
	{
		switch (clicked_widget) {
			case WID_RAT_REMOVE:
				/* If it is the removal button that has been clicked, do nothing,
				 * as it is up to the other buttons to drive removal status */
				return;

			case WID_RAT_BUILD_NS:
			case WID_RAT_BUILD_X:
			case WID_RAT_BUILD_EW:
			case WID_RAT_BUILD_Y:
			case WID_RAT_AUTORAIL:
			case WID_RAT_BUILD_WAYPOINT:
			case WID_RAT_BUILD_STATION:
			case WID_RAT_BUILD_SIGNALS:
				/* Removal button is enabled only if the rail/signal/waypoint/station
				 * button is still lowered.  Once raised, it has to be disabled */
				this->SetWidgetDisabledState(WID_RAT_REMOVE, !this->IsWidgetLowered(clicked_widget));
				break;

			default:
				/* When any other buttons than rail/signal/waypoint/station, raise and
				 * disable the removal button */
				this->DisableWidget(WID_RAT_REMOVE);
				this->RaiseWidget(WID_RAT_REMOVE);
				break;
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_RAT_CAPTION) {
			const RailTypeInfo *rti = GetRailTypeInfo(this->railtype);
			if (rti->max_speed > 0) {
				SetDParam(0, STR_TOOLBAR_RAILTYPE_VELOCITY);
				SetDParam(1, rti->strings.toolbar_caption);
				SetDParam(2, PackVelocity(rti->max_speed, VEH_TRAIN));
			} else {
				SetDParam(0, rti->strings.toolbar_caption);
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget < WID_RAT_BUILD_NS) return;

		_remove_button_clicked = false;
		switch (widget) {
			case WID_RAT_BUILD_NS:
				HandlePlacePushButton(this, WID_RAT_BUILD_NS, GetRailTypeInfo(_cur_railtype)->cursor.rail_ns, HT_LINE | HT_DIR_VL);
				this->last_user_action = widget;
				break;

			case WID_RAT_BUILD_X:
				HandlePlacePushButton(this, WID_RAT_BUILD_X, GetRailTypeInfo(_cur_railtype)->cursor.rail_swne, HT_LINE | HT_DIR_X);
				this->last_user_action = widget;
				break;

			case WID_RAT_BUILD_EW:
				HandlePlacePushButton(this, WID_RAT_BUILD_EW, GetRailTypeInfo(_cur_railtype)->cursor.rail_ew, HT_LINE | HT_DIR_HL);
				this->last_user_action = widget;
				break;

			case WID_RAT_BUILD_Y:
				HandlePlacePushButton(this, WID_RAT_BUILD_Y, GetRailTypeInfo(_cur_railtype)->cursor.rail_nwse, HT_LINE | HT_DIR_Y);
				this->last_user_action = widget;
				break;

			case WID_RAT_AUTORAIL:
				HandlePlacePushButton(this, WID_RAT_AUTORAIL, GetRailTypeInfo(_cur_railtype)->cursor.autorail, HT_RAIL);
				this->last_user_action = widget;
				break;

			case WID_RAT_DEMOLISH:
				HandlePlacePushButton(this, WID_RAT_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT | HT_DIAGONAL);
				this->last_user_action = widget;
				break;

			case WID_RAT_BUILD_DEPOT:
				if (HandlePlacePushButton(this, WID_RAT_BUILD_DEPOT, GetRailTypeInfo(_cur_railtype)->cursor.depot, HT_RECT)) {
					ShowBuildTrainDepotPicker(this);
					this->last_user_action = widget;
				}
				break;

			case WID_RAT_BUILD_WAYPOINT:
				this->last_user_action = widget;
				if (HandlePlacePushButton(this, WID_RAT_BUILD_WAYPOINT, SPR_CURSOR_WAYPOINT, HT_RECT)) {
					ShowBuildWaypointPicker(this);
				}
				break;

			case WID_RAT_BUILD_STATION:
				if (HandlePlacePushButton(this, WID_RAT_BUILD_STATION, SPR_CURSOR_RAIL_STATION, HT_RECT)) {
					ShowStationBuilder(this);
					this->last_user_action = widget;
				}
				break;

			case WID_RAT_BUILD_SIGNALS: {
				this->last_user_action = widget;
				bool started = HandlePlacePushButton(this, WID_RAT_BUILD_SIGNALS, ANIMCURSOR_BUILDSIGNALS, HT_RECT);
				if (started != _ctrl_pressed) {
					ShowSignalBuilder(this);
				}
				break;
			}

			case WID_RAT_BUILD_BRIDGE:
				HandlePlacePushButton(this, WID_RAT_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, HT_RECT);
				this->last_user_action = widget;
				break;

			case WID_RAT_BUILD_TUNNEL:
				HandlePlacePushButton(this, WID_RAT_BUILD_TUNNEL, GetRailTypeInfo(_cur_railtype)->cursor.tunnel, HT_SPECIAL);
				this->last_user_action = widget;
				break;

			case WID_RAT_REMOVE:
				BuildRailClick_Remove(this);
				break;

			case WID_RAT_CONVERT_RAIL:
				HandlePlacePushButton(this, WID_RAT_CONVERT_RAIL, GetRailTypeInfo(_cur_railtype)->cursor.convert, HT_RECT | HT_DIAGONAL);
				this->last_user_action = widget;
				break;

			default: NOT_REACHED();
		}
		this->UpdateRemoveWidgetStatus(widget);
		if (_ctrl_pressed) RailToolbar_CtrlChanged(this);
	}

	EventState OnHotkey(int hotkey) override
	{
		MarkTileDirtyByTile(TileVirtXY(_thd.pos.x, _thd.pos.y)); // redraw tile selection
		return Window::OnHotkey(hotkey);
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (this->last_user_action) {
			case WID_RAT_BUILD_NS:
				VpStartPlaceSizing(tile, VPM_FIX_VERTICAL | VPM_RAILDIRS, DDSP_PLACE_RAIL);
				break;

			case WID_RAT_BUILD_X:
				VpStartPlaceSizing(tile, VPM_FIX_Y | VPM_RAILDIRS, DDSP_PLACE_RAIL);
				break;

			case WID_RAT_BUILD_EW:
				VpStartPlaceSizing(tile, VPM_FIX_HORIZONTAL | VPM_RAILDIRS, DDSP_PLACE_RAIL);
				break;

			case WID_RAT_BUILD_Y:
				VpStartPlaceSizing(tile, VPM_FIX_X | VPM_RAILDIRS, DDSP_PLACE_RAIL);
				break;

			case WID_RAT_AUTORAIL:
				VpStartPlaceSizing(tile, VPM_RAILDIRS, DDSP_PLACE_RAIL);
				break;

			case WID_RAT_DEMOLISH:
				PlaceProc_DemolishArea(tile);
				break;

			case WID_RAT_BUILD_DEPOT: {
				CloseWindowById(WC_SELECT_DEPOT, VEH_TRAIN);

				ViewportPlaceMethod vpm = (DiagDirToAxis(_build_depot_direction) == 0) ? VPM_X_LIMITED : VPM_Y_LIMITED;
				VpStartPlaceSizing(tile, vpm, DDSP_BUILD_DEPOT);
				VpSetPlaceSizingLimit(_settings_game.depot.depot_spread);
				break;
			}

			case WID_RAT_BUILD_WAYPOINT:
				PlaceRail_Waypoint(tile);
				break;

			case WID_RAT_BUILD_STATION:
				PlaceRail_Station(tile);
				break;

			case WID_RAT_BUILD_SIGNALS:
				VpStartPlaceSizing(tile, VPM_SIGNALDIRS, DDSP_BUILD_SIGNALS);
				break;

			case WID_RAT_BUILD_BRIDGE:
				PlaceRail_Bridge(tile, this);
				break;

			case WID_RAT_BUILD_TUNNEL:
				Command<CMD_BUILD_TUNNEL>::Post(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE, CcBuildRailTunnel, tile, TRANSPORT_RAIL, _cur_railtype);
				break;

			case WID_RAT_CONVERT_RAIL:
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CONVERT_RAIL);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		/* no dragging if you have pressed the convert button */
		if (FindWindowById(WC_BUILD_SIGNAL, 0) != nullptr && _convert_signal_button && this->IsWidgetLowered(WID_RAT_BUILD_SIGNALS)) return;

		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x != -1) {
			switch (select_proc) {
				default: NOT_REACHED();
				case DDSP_BUILD_BRIDGE:
					if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
					ShowBuildBridgeWindow(start_tile, end_tile, TRANSPORT_RAIL, _cur_railtype);
					break;

				case DDSP_PLACE_RAIL:
					HandleAutodirPlacement();
					break;

				case DDSP_BUILD_SIGNALS:
					HandleAutoSignalPlacement();
					break;

				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;

				case DDSP_CONVERT_RAIL:
					Command<CMD_CONVERT_RAIL>::Post(STR_ERROR_CAN_T_CONVERT_RAIL, CcPlaySound_CONSTRUCTION_RAIL, end_tile, start_tile, _cur_railtype, _ctrl_pressed);
					break;

				case DDSP_REMOVE_STATION:
				case DDSP_BUILD_STATION:
					if (this->IsWidgetLowered(WID_RAT_BUILD_STATION)) {
						/* Station */
						if (_remove_button_clicked) {
							bool keep_rail = !_ctrl_pressed;
							Command<CMD_REMOVE_FROM_RAIL_STATION>::Post(STR_ERROR_CAN_T_REMOVE_PART_OF_STATION, CcPlaySound_CONSTRUCTION_RAIL, end_tile, start_tile, keep_rail);
						} else {
							HandleStationPlacement(start_tile, end_tile);
						}
					} else {
						/* Waypoint */
						if (_remove_button_clicked) {
							bool keep_rail = !_ctrl_pressed;
							Command<CMD_REMOVE_FROM_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_REMOVE_TRAIN_WAYPOINT, CcPlaySound_CONSTRUCTION_RAIL, end_tile, start_tile, keep_rail);
						} else {
							TileArea ta(start_tile, end_tile);
							Axis axis = select_method == VPM_X_LIMITED ? AXIS_X : AXIS_Y;
							bool adjacent = _ctrl_pressed;

							auto proc = [=](bool test, StationID to_join) -> bool {
								if (test) {
									return Command<CMD_BUILD_RAIL_WAYPOINT>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_RAIL_WAYPOINT>()), ta.tile, axis, ta.w, ta.h, _waypoint_gui.sel_class, _waypoint_gui.sel_type, INVALID_STATION, adjacent).Succeeded();
								} else {
									return Command<CMD_BUILD_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_WAYPOINT, CcPlaySound_CONSTRUCTION_RAIL, ta.tile, axis, ta.w, ta.h, _waypoint_gui.sel_class, _waypoint_gui.sel_type, to_join, adjacent);
								}
							};

							ShowSelectRailWaypointIfNeeded(ta, proc);
						}
					}
					break;

				case DDSP_BUILD_DEPOT: {
					bool adjacent = _ctrl_pressed;

					auto proc = [=](DepotID join_to) -> bool {
						return Command<CMD_BUILD_TRAIN_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_DEPOT, CcRailDepot, start_tile, _cur_railtype, _build_depot_direction, adjacent, join_to, end_tile);
					};

					ShowSelectDepotIfNeeded(TileArea(start_tile, end_tile), proc, VEH_TRAIN);
					break;
				}
			}
		}
	}

	void OnPlaceObjectAbort() override
	{
		if (this->IsWidgetLowered(WID_RAT_BUILD_STATION)) SetViewportCatchmentStation(nullptr, true);
		if (this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT)) SetViewportCatchmentWaypoint(nullptr, true);

		if (this->IsWidgetLowered(WID_RAT_BUILD_DEPOT)) SetViewportHighlightDepot(INVALID_DEPOT, true);

		this->RaiseButtons();
		this->DisableWidget(WID_RAT_REMOVE);
		this->SetWidgetDirty(WID_RAT_REMOVE);

		CloseWindowById(WC_BUILD_SIGNAL, TRANSPORT_RAIL);
		CloseWindowById(WC_BUILD_STATION, TRANSPORT_RAIL);
		CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_RAIL);
		CloseWindowById(WC_BUILD_WAYPOINT, TRANSPORT_RAIL);
		CloseWindowById(WC_SELECT_STATION, 0);
		CloseWindowById(WC_SELECT_DEPOT, VEH_TRAIN);
		CloseWindowByClass(WC_BUILD_BRIDGE);
	}

	void OnPlacePresize([[maybe_unused]] Point pt, TileIndex tile) override
	{
		Command<CMD_BUILD_TUNNEL>::Do(DC_AUTO, tile, TRANSPORT_RAIL, _cur_railtype);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
	}

	EventState OnCTRLStateChange() override
	{
		/* do not toggle Remove button by Ctrl when placing station */
		if (!this->IsWidgetLowered(WID_RAT_BUILD_STATION) && !this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT) && RailToolbar_CtrlChanged(this)) return ES_HANDLED;
		return ES_NOT_HANDLED;
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		if (this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT)) CheckRedrawRailWaypointCoverage(this);
	}

	/**
	 * Handler for global hotkeys of the BuildRailToolbarWindow.
	 * @param hotkey Hotkey
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState RailToolbarGlobalHotkeys(int hotkey)
	{
		if (_game_mode != GM_NORMAL) return ES_NOT_HANDLED;
		extern RailType _last_built_railtype;
		Window *w = ShowBuildRailToolbar(_last_built_railtype);
		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"railtoolbar", {
		Hotkey('1', "build_ns", WID_RAT_BUILD_NS),
		Hotkey('2', "build_x", WID_RAT_BUILD_X),
		Hotkey('3', "build_ew", WID_RAT_BUILD_EW),
		Hotkey('4', "build_y", WID_RAT_BUILD_Y),
		Hotkey({'5', 'A' | WKC_GLOBAL_HOTKEY}, "autorail", WID_RAT_AUTORAIL),
		Hotkey('6', "demolish", WID_RAT_DEMOLISH),
		Hotkey('7', "depot", WID_RAT_BUILD_DEPOT),
		Hotkey('8', "waypoint", WID_RAT_BUILD_WAYPOINT),
		Hotkey('9', "station", WID_RAT_BUILD_STATION),
		Hotkey('S', "signal", WID_RAT_BUILD_SIGNALS),
		Hotkey('B', "bridge", WID_RAT_BUILD_BRIDGE),
		Hotkey('T', "tunnel", WID_RAT_BUILD_TUNNEL),
		Hotkey('R', "remove", WID_RAT_REMOVE),
		Hotkey('C', "convert", WID_RAT_CONVERT_RAIL),
	}, RailToolbarGlobalHotkeys};
};

static constexpr NWidgetPart _nested_build_rail_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_RAT_CAPTION), SetDataTip(STR_JUST_STRING2, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetTextStyle(TC_WHITE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_NS),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_RAIL_NS, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TRACK),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_X),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_RAIL_NE, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TRACK),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_EW),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_RAIL_EW, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TRACK),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_Y),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_RAIL_NW, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TRACK),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_AUTORAIL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTORAIL, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_AUTORAIL),

		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetMinimalSize(4, 22), EndContainer(),

		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_DEMOLISH),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_DEPOT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DEPOT_RAIL, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_TRAIN_DEPOT_FOR_BUILDING),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_WAYPOINT),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_WAYPOINT, STR_RAIL_TOOLBAR_TOOLTIP_CONVERT_RAIL_TO_WAYPOINT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_STATION),
						SetFill(0, 1), SetMinimalSize(42, 22), SetDataTip(SPR_IMG_RAIL_STATION, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_SIGNALS),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_RAIL_SIGNALS, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_SIGNALS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_BRIDGE),
						SetFill(0, 1), SetMinimalSize(42, 22), SetDataTip(SPR_IMG_BRIDGE, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_BUILD_TUNNEL),
						SetFill(0, 1), SetMinimalSize(20, 22), SetDataTip(SPR_IMG_TUNNEL_RAIL, STR_RAIL_TOOLBAR_TOOLTIP_BUILD_RAILROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_REMOVE),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_RAIL_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_RAT_CONVERT_RAIL),
						SetFill(0, 1), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_CONVERT_RAIL, STR_RAIL_TOOLBAR_TOOLTIP_CONVERT_RAIL),
	EndContainer(),
};

static WindowDesc _build_rail_desc(
	WDP_ALIGN_TOOLBAR, "toolbar_rail", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_rail_widgets,
	&BuildRailToolbarWindow::hotkeys
);


/**
 * Open the build rail toolbar window for a specific rail type.
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @param railtype Rail type to open the window for
 * @return newly opened rail toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildRailToolbar(RailType railtype)
{
	if (!Company::IsValidID(_local_company)) return nullptr;
	if (!ValParamRailType(railtype)) return nullptr;

	CloseWindowByClass(WC_BUILD_TOOLBAR);
	_cur_railtype = railtype;
	_remove_button_clicked = false;
	return new BuildRailToolbarWindow(_build_rail_desc, railtype);
}

/* TODO: For custom stations, respect their allowed platforms/lengths bitmasks!
 * --pasky */

static void HandleStationPlacement(TileIndex start, TileIndex end)
{
	TileArea ta(start, end);
	uint numtracks = ta.w;
	uint platlength = ta.h;

	if (_station_gui.axis == AXIS_X) Swap(numtracks, platlength);

	StationPickerSelection params = _station_gui;
	RailType rt = _cur_railtype;
	bool adjacent = _ctrl_pressed;

	auto proc = [=](bool test, StationID to_join) -> bool {
		if (test) {
			return Command<CMD_BUILD_RAIL_STATION>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_RAIL_STATION>()), ta.tile, rt, params.axis, numtracks, platlength, params.sel_class, params.sel_type, INVALID_STATION, adjacent).Succeeded();
		} else {
			return Command<CMD_BUILD_RAIL_STATION>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, CcStation, ta.tile, rt, params.axis, numtracks, platlength, params.sel_class, params.sel_type, to_join, adjacent);
		}
	};

	ShowSelectStationIfNeeded(ta, proc);
}

/**
 * Test if a station/waypoint uses the default graphics.
 * @param bst Station to test.
 * @return true if at least one of its rail station tiles uses the default graphics.
 */
static bool StationUsesDefaultType(const BaseStation *bst)
{
	for (TileIndex t : bst->train_station) {
		if (bst->TileBelongsToRailStation(t) && IsRailStation(t) && GetCustomStationSpecIndex(t) == 0) return true;
	}
	return false;
}

class StationPickerCallbacks : public PickerCallbacksNewGRFClass<StationClass> {
public:
	StationPickerCallbacks() : PickerCallbacksNewGRFClass<StationClass>("fav_stations") {}

	StringID GetClassTooltip() const override { return STR_PICKER_STATION_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_STATION_TYPE_TOOLTIP; }

	bool IsActive() const override
	{
		for (const auto &cls : StationClass::Classes()) {
			if (IsWaypointClass(cls)) continue;
			for (const auto *spec : cls.Specs()) {
				if (spec != nullptr) return true;
			}
		}
		return false;
	}

	bool HasClassChoice() const override
	{
		return std::count_if(std::begin(StationClass::Classes()), std::end(StationClass::Classes()), std::not_fn(IsWaypointClass)) > 1;
	}

	int GetSelectedClass() const override { return _station_gui.sel_class; }
	void SetSelectedClass(int id) const override { _station_gui.sel_class = this->GetClassIndex(id); }

	StringID GetClassName(int id) const override
	{
		const auto *sc = GetClass(id);
		if (IsWaypointClass(*sc)) return INVALID_STRING_ID;
		return sc->name;
	}

	int GetSelectedType() const override { return _station_gui.sel_type; }
	void SetSelectedType(int id) const override { _station_gui.sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		return (spec == nullptr) ? STR_STATION_CLASS_DFLT_STATION : spec->name;
	}

	bool IsTypeAvailable(int cls_id, int id) const override
	{
		return IsStationAvailable(this->GetSpec(cls_id, id));
	}

	void DrawType(int x, int y, int cls_id, int id) const override
	{
		if (!DrawStationTile(x, y, _cur_railtype, _station_gui.axis, this->GetClassIndex(cls_id), id)) {
			StationPickerDrawSprite(x, y, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 2 + _station_gui.axis);
		}
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		bool default_added = false;
		for (const Station *st : Station::Iterate()) {
			if (st->owner != _local_company) continue;
			if (!default_added && StationUsesDefaultType(st)) {
				items.insert({0, 0, STAT_CLASS_DFLT, 0});
				default_added = true;
			}
			for (const auto &sm : st->speclist) {
				if (sm.spec == nullptr) continue;
				items.insert({sm.grfid, sm.localidx, sm.spec->class_index, sm.spec->index});
			}
		}
	}

	static StationPickerCallbacks instance;
};
/* static */ StationPickerCallbacks StationPickerCallbacks::instance;

struct BuildRailStationWindow : public PickerWindow {
private:
	uint coverage_height; ///< Height of the coverage texts.

	/**
	 * Verify whether the currently selected station size is allowed after selecting a new station class/type.
	 * If not, change the station size variables ( _settings_client.gui.station_numtracks and _settings_client.gui.station_platlength ).
	 * @param statspec Specification of the new station class/type
	 */
	void CheckSelectedSize(const StationSpec *statspec)
	{
		if (statspec == nullptr || _settings_client.gui.station_dragdrop) return;

		/* If current number of tracks is not allowed, make it as big as possible */
		if (HasBit(statspec->disallowed_platforms, _settings_client.gui.station_numtracks - 1)) {
			this->RaiseWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
			_settings_client.gui.station_numtracks = 1;
			if (statspec->disallowed_platforms != UINT8_MAX) {
				while (HasBit(statspec->disallowed_platforms, _settings_client.gui.station_numtracks - 1)) {
					_settings_client.gui.station_numtracks++;
				}
				this->LowerWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
			}
		}

		if (HasBit(statspec->disallowed_lengths, _settings_client.gui.station_platlength - 1)) {
			this->RaiseWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
			_settings_client.gui.station_platlength = 1;
			if (statspec->disallowed_lengths != UINT8_MAX) {
				while (HasBit(statspec->disallowed_lengths, _settings_client.gui.station_platlength - 1)) {
					_settings_client.gui.station_platlength++;
				}
				this->LowerWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
			}
		}
	}

public:
	BuildRailStationWindow(WindowDesc &desc, Window *parent) : PickerWindow(desc, parent, TRANSPORT_RAIL, StationPickerCallbacks::instance)
	{
		this->coverage_height = 2 * GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
		this->ConstructWindow();
		this->InvalidateData();
	}

	void OnInit() override
	{
		this->LowerWidget(WID_BRAS_PLATFORM_DIR_X + _station_gui.axis);
		if (_settings_client.gui.station_dragdrop) {
			this->LowerWidget(WID_BRAS_PLATFORM_DRAG_N_DROP);
		} else {
			this->LowerWidget(WID_BRAS_PLATFORM_NUM_BEGIN + _settings_client.gui.station_numtracks);
			this->LowerWidget(WID_BRAS_PLATFORM_LEN_BEGIN + _settings_client.gui.station_platlength);
		}
		this->SetWidgetLoweredState(WID_BRAS_HIGHLIGHT_OFF, !_settings_client.gui.station_show_coverage);
		this->SetWidgetLoweredState(WID_BRAS_HIGHLIGHT_ON, _settings_client.gui.station_show_coverage);

		this->PickerWindow::OnInit();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_STATION, 0);
		this->PickerWindow::Close();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (gui_scope) {
			const StationSpec *statspec = StationClass::Get(_station_gui.sel_class)->GetSpec(_station_gui.sel_type);
			this->CheckSelectedSize(statspec);
		}

		this->PickerWindow::OnInvalidateData(data, gui_scope);
	}

	void OnPaint() override
	{
		const StationSpec *statspec = StationClass::Get(_station_gui.sel_class)->GetSpec(_station_gui.sel_type);

		if (_settings_client.gui.station_dragdrop) {
			SetTileSelectSize(1, 1);
		} else {
			int x = _settings_client.gui.station_numtracks;
			int y = _settings_client.gui.station_platlength;
			if (_station_gui.axis == AXIS_X) Swap(x, y);
			if (!_remove_button_clicked) {
				SetTileSelectSize(x, y);
			}
		}

		int rad = (_settings_game.station.modified_catchment) ? CA_TRAIN : CA_UNMODIFIED;

		if (_settings_client.gui.station_show_coverage) SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);

		for (uint bits = 0; bits < 7; bits++) {
			bool disable = bits >= _settings_game.station.station_spread;
			if (statspec == nullptr) {
				this->SetWidgetDisabledState(bits + WID_BRAS_PLATFORM_NUM_1, disable);
				this->SetWidgetDisabledState(bits + WID_BRAS_PLATFORM_LEN_1, disable);
			} else {
				this->SetWidgetDisabledState(bits + WID_BRAS_PLATFORM_NUM_1, HasBit(statspec->disallowed_platforms, bits) || disable);
				this->SetWidgetDisabledState(bits + WID_BRAS_PLATFORM_LEN_1, HasBit(statspec->disallowed_lengths,   bits) || disable);
			}
		}

		this->DrawWidgets();

		if (this->IsShaded()) return;
		/* 'Accepts' and 'Supplies' texts. */
		Rect r = this->GetWidget<NWidgetBase>(WID_BRAS_COVERAGE_TEXTS)->GetCurrentRect();
		int top = r.top;
		top = DrawStationCoverageAreaText(r.left, r.right, top, SCT_ALL, rad, false) + WidgetDimensions::scaled.vsep_normal;
		top = DrawStationCoverageAreaText(r.left, r.right, top, SCT_ALL, rad, true);
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
			case WID_BRAS_PLATFORM_DIR_X:
			case WID_BRAS_PLATFORM_DIR_Y:
				size.width  = ScaleGUITrad(PREVIEW_WIDTH) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height = ScaleGUITrad(PREVIEW_HEIGHT) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;

			case WID_BRAS_COVERAGE_TEXTS:
				size.height = this->coverage_height;
				break;

			default:
				this->PickerWindow::UpdateWidgetSize(widget, size, padding, fill, resize);
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		DrawPixelInfo tmp_dpi;

		switch (widget) {
			case WID_BRAS_PLATFORM_DIR_X: {
				/* Set up a clipping area for the '/' station preview */
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(PREVIEW_WIDTH)) / 2 + ScaleSpriteTrad(PREVIEW_LEFT);
					int y = (ir.Height() + ScaleSpriteTrad(PREVIEW_HEIGHT)) / 2 - ScaleSpriteTrad(PREVIEW_BOTTOM);
					if (!DrawStationTile(x, y, _cur_railtype, AXIS_X, _station_gui.sel_class, _station_gui.sel_type)) {
						StationPickerDrawSprite(x, y, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 2);
					}
				}
				break;
			}

			case WID_BRAS_PLATFORM_DIR_Y: {
				/* Set up a clipping area for the '\' station preview */
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(PREVIEW_WIDTH)) / 2 + ScaleSpriteTrad(PREVIEW_LEFT);
					int y = (ir.Height() + ScaleSpriteTrad(PREVIEW_HEIGHT)) / 2 - ScaleSpriteTrad(PREVIEW_BOTTOM);
					if (!DrawStationTile(x, y, _cur_railtype, AXIS_Y, _station_gui.sel_class, _station_gui.sel_type)) {
						StationPickerDrawSprite(x, y, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 3);
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
			case WID_BRAS_PLATFORM_DIR_X:
			case WID_BRAS_PLATFORM_DIR_Y:
				this->RaiseWidget(WID_BRAS_PLATFORM_DIR_X + _station_gui.axis);
				_station_gui.axis = (Axis)(widget - WID_BRAS_PLATFORM_DIR_X);
				this->LowerWidget(WID_BRAS_PLATFORM_DIR_X + _station_gui.axis);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;

			case WID_BRAS_PLATFORM_NUM_1:
			case WID_BRAS_PLATFORM_NUM_2:
			case WID_BRAS_PLATFORM_NUM_3:
			case WID_BRAS_PLATFORM_NUM_4:
			case WID_BRAS_PLATFORM_NUM_5:
			case WID_BRAS_PLATFORM_NUM_6:
			case WID_BRAS_PLATFORM_NUM_7: {
				this->RaiseWidget(WID_BRAS_PLATFORM_NUM_BEGIN + _settings_client.gui.station_numtracks);
				this->RaiseWidget(WID_BRAS_PLATFORM_DRAG_N_DROP);

				_settings_client.gui.station_numtracks = widget - WID_BRAS_PLATFORM_NUM_BEGIN;
				_settings_client.gui.station_dragdrop = false;

				const StationSpec *statspec = StationClass::Get(_station_gui.sel_class)->GetSpec(_station_gui.sel_type);
				if (statspec != nullptr && HasBit(statspec->disallowed_lengths, _settings_client.gui.station_platlength - 1)) {
					/* The previously selected number of platforms in invalid */
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_lengths, i)) {
							this->RaiseWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
							_settings_client.gui.station_platlength = i + 1;
							break;
						}
					}
				}

				this->LowerWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
				this->LowerWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;
			}

			case WID_BRAS_PLATFORM_LEN_1:
			case WID_BRAS_PLATFORM_LEN_2:
			case WID_BRAS_PLATFORM_LEN_3:
			case WID_BRAS_PLATFORM_LEN_4:
			case WID_BRAS_PLATFORM_LEN_5:
			case WID_BRAS_PLATFORM_LEN_6:
			case WID_BRAS_PLATFORM_LEN_7: {
				this->RaiseWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
				this->RaiseWidget(WID_BRAS_PLATFORM_DRAG_N_DROP);

				_settings_client.gui.station_platlength = widget - WID_BRAS_PLATFORM_LEN_BEGIN;
				_settings_client.gui.station_dragdrop = false;

				const StationSpec *statspec = StationClass::Get(_station_gui.sel_class)->GetSpec(_station_gui.sel_type);
				if (statspec != nullptr && HasBit(statspec->disallowed_platforms, _settings_client.gui.station_numtracks - 1)) {
					/* The previously selected number of tracks in invalid */
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_platforms, i)) {
							this->RaiseWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
							_settings_client.gui.station_numtracks = i + 1;
							break;
						}
					}
				}

				this->LowerWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
				this->LowerWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;
			}

			case WID_BRAS_PLATFORM_DRAG_N_DROP: {
				_settings_client.gui.station_dragdrop ^= true;

				this->ToggleWidgetLoweredState(WID_BRAS_PLATFORM_DRAG_N_DROP);

				/* get the first allowed length/number of platforms */
				const StationSpec *statspec = StationClass::Get(_station_gui.sel_class)->GetSpec(_station_gui.sel_type);
				if (statspec != nullptr && HasBit(statspec->disallowed_lengths, _settings_client.gui.station_platlength - 1)) {
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_lengths, i)) {
							this->RaiseWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
							_settings_client.gui.station_platlength = i + 1;
							break;
						}
					}
				}
				if (statspec != nullptr && HasBit(statspec->disallowed_platforms, _settings_client.gui.station_numtracks - 1)) {
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_platforms, i)) {
							this->RaiseWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
							_settings_client.gui.station_numtracks = i + 1;
							break;
						}
					}
				}

				this->SetWidgetLoweredState(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN, !_settings_client.gui.station_dragdrop);
				this->SetWidgetLoweredState(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN, !_settings_client.gui.station_dragdrop);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;
			}

			case WID_BRAS_HIGHLIGHT_OFF:
			case WID_BRAS_HIGHLIGHT_ON:
				_settings_client.gui.station_show_coverage = (widget != WID_BRAS_HIGHLIGHT_OFF);

				this->SetWidgetLoweredState(WID_BRAS_HIGHLIGHT_OFF, !_settings_client.gui.station_show_coverage);
				this->SetWidgetLoweredState(WID_BRAS_HIGHLIGHT_ON, _settings_client.gui.station_show_coverage);
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

	/**
	 * Handler for global hotkeys of the BuildRailStationWindow.
	 * @param hotkey Hotkey
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState BuildRailStationGlobalHotkeys(int hotkey)
	{
		if (_game_mode == GM_MENU) return ES_NOT_HANDLED;
		Window *w = ShowStationBuilder(FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL));
		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"buildrailstation", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}, BuildRailStationGlobalHotkeys};
};

static constexpr NWidgetPart _nested_station_builder_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_RAIL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_ORIENTATION, STR_NULL),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
						NWidget(WWT_PANEL, COLOUR_GREY, WID_BRAS_PLATFORM_DIR_X), SetFill(0, 0), SetDataTip(0x0, STR_STATION_BUILD_RAILROAD_ORIENTATION_TOOLTIP), EndContainer(),
						NWidget(WWT_PANEL, COLOUR_GREY, WID_BRAS_PLATFORM_DIR_Y), SetFill(0, 0), SetDataTip(0x0, STR_STATION_BUILD_RAILROAD_ORIENTATION_TOOLTIP), EndContainer(),
					EndContainer(),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_NUMBER_OF_TRACKS, STR_NULL),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_1), SetAspect(1.25f), SetDataTip(STR_BLACK_1, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_2), SetAspect(1.25f), SetDataTip(STR_BLACK_2, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_3), SetAspect(1.25f), SetDataTip(STR_BLACK_3, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_4), SetAspect(1.25f), SetDataTip(STR_BLACK_4, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_5), SetAspect(1.25f), SetDataTip(STR_BLACK_5, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_6), SetAspect(1.25f), SetDataTip(STR_BLACK_6, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_7), SetAspect(1.25f), SetDataTip(STR_BLACK_7, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
					EndContainer(),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_PLATFORM_LENGTH, STR_NULL),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_1), SetAspect(1.25f), SetDataTip(STR_BLACK_1, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_2), SetAspect(1.25f), SetDataTip(STR_BLACK_2, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_3), SetAspect(1.25f), SetDataTip(STR_BLACK_3, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_4), SetAspect(1.25f), SetDataTip(STR_BLACK_4, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_5), SetAspect(1.25f), SetDataTip(STR_BLACK_5, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_6), SetAspect(1.25f), SetDataTip(STR_BLACK_6, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_7), SetAspect(1.25f), SetDataTip(STR_BLACK_7, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_DRAG_N_DROP), SetMinimalSize(75, 12), SetDataTip(STR_STATION_BUILD_DRAG_DROP, STR_STATION_BUILD_DRAG_DROP_TOOLTIP),
					EndContainer(),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_HIGHLIGHT_OFF), SetMinimalSize(60, 12), SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_HIGHLIGHT_ON), SetMinimalSize(60, 12), SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
					EndContainer(),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BRAS_COVERAGE_TEXTS), SetFill(1, 1), SetResize(1, 0), SetMinimalTextLines(2, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

/** High level window description of the station-build window (default & newGRF) */
static WindowDesc _station_builder_desc(
	WDP_AUTO, "build_station_rail", 0, 0,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_station_builder_widgets,
	&BuildRailStationWindow::hotkeys
);

/** Open station build window */
static Window *ShowStationBuilder(Window *parent)
{
	return new BuildRailStationWindow(_station_builder_desc, parent);
}

struct BuildSignalWindow : public PickerWindowBase {
private:
	Dimension sig_sprite_size;     ///< Maximum size of signal GUI sprites.
	int sig_sprite_bottom_offset;  ///< Maximum extent of signal GUI sprite from reference point towards bottom.

	/**
	 * Draw dynamic a signal-sprite in a button in the signal GUI
	 * @param image        the sprite to draw
	 */
	void DrawSignalSprite(const Rect &r, SpriteID image) const
	{
		Point offset;
		Dimension sprite_size = GetSpriteSize(image, &offset);
		Rect ir = r.Shrink(WidgetDimensions::scaled.imgbtn);
		int x = CenterBounds(ir.left, ir.right, sprite_size.width - offset.x) - offset.x; // centered
		int y = ir.top - sig_sprite_bottom_offset +
				(ir.Height() + sig_sprite_size.height) / 2; // aligned to bottom

		DrawSprite(image, PAL_NONE, x, y);
	}

	/** Show or hide buttons for non-path signals in the signal GUI */
	void SetSignalUIMode()
	{
		bool show_non_path_signals = (_settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL);

		this->GetWidget<NWidgetStacked>(WID_BS_BLOCK_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_BLOCK_SPACER_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
	}

public:
	BuildSignalWindow(WindowDesc &desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->CreateNestedTree();
		this->SetSignalUIMode();
		this->FinishInitNested(TRANSPORT_RAIL);
		this->OnInvalidateData();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		_convert_signal_button = false;
		this->PickerWindowBase::Close();
	}

	void OnInit() override
	{
		/* Calculate maximum signal sprite size. */
		this->sig_sprite_size.width = 0;
		this->sig_sprite_size.height = 0;
		this->sig_sprite_bottom_offset = 0;
		const RailTypeInfo *rti = GetRailTypeInfo(_cur_railtype);
		for (uint type = SIGTYPE_BLOCK; type < SIGTYPE_END; type++) {
			for (uint variant = SIG_ELECTRIC; variant <= SIG_SEMAPHORE; variant++) {
				for (uint lowered = 0; lowered < 2; lowered++) {
					Point offset;
					Dimension sprite_size = GetSpriteSize(rti->gui_sprites.signals[type][variant][lowered], &offset);
					this->sig_sprite_bottom_offset = std::max<int>(this->sig_sprite_bottom_offset, sprite_size.height);
					this->sig_sprite_size.width = std::max<int>(this->sig_sprite_size.width, sprite_size.width - offset.x);
					this->sig_sprite_size.height = std::max<int>(this->sig_sprite_size.height, sprite_size.height - offset.y);
				}
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_BS_DRAG_SIGNALS_DENSITY_LABEL) {
			/* Two digits for signals density. */
			size.width = std::max(size.width, 2 * GetDigitWidth() + padding.width + WidgetDimensions::scaled.framerect.Horizontal());
		} else if (IsInsideMM(widget, WID_BS_SEMAPHORE_NORM, WID_BS_ELECTRIC_PBS_OWAY + 1)) {
			size.width = std::max(size.width, this->sig_sprite_size.width + padding.width);
			size.height = std::max(size.height, this->sig_sprite_size.height + padding.height);
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_BS_DRAG_SIGNALS_DENSITY_LABEL:
				SetDParam(0, _settings_client.gui.drag_signals_density);
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (IsInsideMM(widget, WID_BS_SEMAPHORE_NORM, WID_BS_ELECTRIC_PBS_OWAY + 1)) {
			/* Extract signal from widget number. */
			int type = (widget - WID_BS_SEMAPHORE_NORM) % SIGTYPE_END;
			int var = SIG_SEMAPHORE - (widget - WID_BS_SEMAPHORE_NORM) / SIGTYPE_END; // SignalVariant order is reversed compared to the widgets.
			SpriteID sprite = GetRailTypeInfo(_cur_railtype)->gui_sprites.signals[type][var][this->IsWidgetLowered(widget)];

			this->DrawSignalSprite(r, sprite);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BS_SEMAPHORE_NORM:
			case WID_BS_SEMAPHORE_ENTRY:
			case WID_BS_SEMAPHORE_EXIT:
			case WID_BS_SEMAPHORE_COMBO:
			case WID_BS_SEMAPHORE_PBS:
			case WID_BS_SEMAPHORE_PBS_OWAY:
			case WID_BS_ELECTRIC_NORM:
			case WID_BS_ELECTRIC_ENTRY:
			case WID_BS_ELECTRIC_EXIT:
			case WID_BS_ELECTRIC_COMBO:
			case WID_BS_ELECTRIC_PBS:
			case WID_BS_ELECTRIC_PBS_OWAY:
				this->RaiseWidget((_cur_signal_variant == SIG_ELECTRIC ? WID_BS_ELECTRIC_NORM : WID_BS_SEMAPHORE_NORM) + _cur_signal_type);

				_cur_signal_type = (SignalType)((uint)((widget - WID_BS_SEMAPHORE_NORM) % (SIGTYPE_LAST + 1)));
				_cur_signal_variant = widget >= WID_BS_ELECTRIC_NORM ? SIG_ELECTRIC : SIG_SEMAPHORE;

				/* Update default (last-used) signal type in config file. */
				_settings_client.gui.default_signal_type = _cur_signal_type;

				/* If 'remove' button of rail build toolbar is active, disable it. */
				if (_remove_button_clicked) {
					Window *w = FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
					if (w != nullptr) ToggleRailButton_Remove(w);
				}

				break;

			case WID_BS_CONVERT:
				_convert_signal_button = !_convert_signal_button;
				break;

			case WID_BS_DRAG_SIGNALS_DENSITY_DECREASE:
				if (_settings_client.gui.drag_signals_density > 1) {
					_settings_client.gui.drag_signals_density--;
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_SETTINGS);
				}
				break;

			case WID_BS_DRAG_SIGNALS_DENSITY_INCREASE:
				if (_settings_client.gui.drag_signals_density < 20) {
					_settings_client.gui.drag_signals_density++;
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_SETTINGS);
				}
				break;

			case WID_BS_TOGGLE_SIZE:
				_settings_client.gui.signal_gui_mode = (_settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL) ? SIGNAL_GUI_PATH : SIGNAL_GUI_ALL;
				this->SetSignalUIMode();
				this->ReInit();
				break;

			default: break;
		}

		this->InvalidateData();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->LowerWidget((_cur_signal_variant == SIG_ELECTRIC ? WID_BS_ELECTRIC_NORM : WID_BS_SEMAPHORE_NORM) + _cur_signal_type);

		this->SetWidgetLoweredState(WID_BS_CONVERT, _convert_signal_button);

		this->SetWidgetDisabledState(WID_BS_DRAG_SIGNALS_DENSITY_DECREASE, _settings_client.gui.drag_signals_density == 1);
		this->SetWidgetDisabledState(WID_BS_DRAG_SIGNALS_DENSITY_INCREASE, _settings_client.gui.drag_signals_density == 20);
	}
};

/** Nested widget definition of the build signal window */
static constexpr NWidgetPart _nested_signal_builder_widgets[] = {
	/* Title bar and buttons. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BS_CAPTION), SetDataTip(STR_BUILD_SIGNAL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_BS_TOGGLE_SIZE), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_BUILD_SIGNAL_TOGGLE_ADVANCED_SIGNAL_TOOLTIP), SetAspect(WidgetDimensions::ASPECT_TOGGLE_SIZE),
	EndContainer(),

	/* Container for both signal groups, spacers, and convert/autofill buttons. */
	NWidget(NWID_HORIZONTAL),
		/* Block signals (can be hidden). */
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_BLOCK_SEL),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE),
				/* Semaphore block signals. */
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_NORM), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_NORM_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_ENTRY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_ENTRY_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_EXIT), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_EXIT_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_COMBO), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_COMBO_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
				EndContainer(),
				/* Electric block signals. */
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_NORM), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_NORM_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_ENTRY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_ENTRY_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_EXIT), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_EXIT_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
					NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_COMBO), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_COMBO_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),

		/* Divider (only shown if block signals visible). */
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_BLOCK_SPACER_SEL),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(0, 0), SetMinimalSize(4, 0), EndContainer(),
		EndContainer(),

		/* Path signals. */
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			/* Semaphore path signals. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_PBS), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_PBS_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_PBS_OWAY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_PBS_OWAY_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
			EndContainer(),
			/* Electric path signals. */
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_PBS), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_PBS_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_PBS_OWAY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_PBS_OWAY_TOOLTIP), SetMinimalSize(22, 22), EndContainer(),
			EndContainer(),
		EndContainer(),

		/* Convert/autofill buttons. */
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_BS_CONVERT), SetDataTip(SPR_IMG_SIGNAL_CONVERT, STR_BUILD_SIGNAL_CONVERT_TOOLTIP), SetFill(0, 1),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_TOOLTIP), SetFill(0, 1),
				NWidget(NWID_VERTICAL), SetPadding(2), SetPIPRatio(1, 0, 1),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN, WID_BS_DRAG_SIGNALS_DENSITY_LABEL), SetDataTip(STR_JUST_INT, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_TOOLTIP), SetTextStyle(TC_ORANGE), SetFill(1, 1),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_BS_DRAG_SIGNALS_DENSITY_DECREASE), SetMinimalSize(9, 12), SetDataTip(AWV_DECREASE, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_DECREASE_TOOLTIP),
						NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_BS_DRAG_SIGNALS_DENSITY_INCREASE), SetMinimalSize(9, 12), SetDataTip(AWV_INCREASE, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_INCREASE_TOOLTIP),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Signal selection window description */
static WindowDesc _signal_builder_desc(
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_SIGNAL, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_signal_builder_widgets
);

/**
 * Open the signal selection window
 */
static void ShowSignalBuilder(Window *parent)
{
	new BuildSignalWindow(_signal_builder_desc, parent);
}

struct BuildRailDepotWindow : public PickerWindowBase {
	BuildRailDepotWindow(WindowDesc &desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_RAIL);
		this->LowerWidget(WID_BRAD_DEPOT_NE + _build_depot_direction);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_DEPOT, VEH_TRAIN);
		this->PickerWindowBase::Close();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (!IsInsideMM(widget, WID_BRAD_DEPOT_NE, WID_BRAD_DEPOT_NW + 1)) return;

		size.width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
		size.height = ScaleGUITrad(48) + WidgetDimensions::scaled.fullbevel.Vertical();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (!IsInsideMM(widget, WID_BRAD_DEPOT_NE, WID_BRAD_DEPOT_NW + 1)) return;

		DrawPixelInfo tmp_dpi;
		Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
		if (FillDrawPixelInfo(&tmp_dpi, ir)) {
			AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
			int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
			int y = (ir.Height() + ScaleSpriteTrad(48)) / 2 - ScaleSpriteTrad(31);
			DrawTrainDepotSprite(x, y, widget - WID_BRAD_DEPOT_NE + DIAGDIR_NE, _cur_railtype);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BRAD_DEPOT_NE:
			case WID_BRAD_DEPOT_SE:
			case WID_BRAD_DEPOT_SW:
			case WID_BRAD_DEPOT_NW:
				CloseWindowById(WC_SELECT_DEPOT, VEH_TRAIN);
				this->RaiseWidget(WID_BRAD_DEPOT_NE + _build_depot_direction);
				_build_depot_direction = (DiagDirection)(widget - WID_BRAD_DEPOT_NE);
				this->LowerWidget(WID_BRAD_DEPOT_NE + _build_depot_direction);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawDepotHighlight(this, VEH_TRAIN);
	}
};

/** Nested widget definition of the build rail depot window */
static constexpr NWidgetPart _nested_build_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_BUILD_DEPOT_TRAIN_ORIENTATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_NW), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_SW), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_NE), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_SE), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_depot_desc(
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_depot_widgets
);

static void ShowBuildTrainDepotPicker(Window *parent)
{
	new BuildRailDepotWindow(_build_depot_desc, parent);
}

class WaypointPickerCallbacks : public PickerCallbacksNewGRFClass<StationClass> {
public:
	WaypointPickerCallbacks() : PickerCallbacksNewGRFClass<StationClass>("fav_waypoints") {}

	StringID GetClassTooltip() const override { return STR_PICKER_WAYPOINT_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_WAYPOINT_TYPE_TOOLTIP; }

	bool IsActive() const override
	{
		for (const auto &cls : StationClass::Classes()) {
			if (!IsWaypointClass(cls)) continue;
			for (const auto *spec : cls.Specs()) {
				if (spec != nullptr) return true;
			}
		}
		return false;
	}

	bool HasClassChoice() const override
	{
		return std::count_if(std::begin(StationClass::Classes()), std::end(StationClass::Classes()), IsWaypointClass) > 1;
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
		return IsStationAvailable(this->GetSpec(cls_id, id));
	}

	void DrawType(int x, int y, int cls_id, int id) const override
	{
		DrawWaypointSprite(x, y, this->GetClassIndex(cls_id), id, _cur_railtype);
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		bool default_added = false;
		for (const Waypoint *wp : Waypoint::Iterate()) {
			if (wp->owner != _local_company || HasBit(wp->waypoint_flags, WPF_ROAD)) continue;
			if (!default_added && StationUsesDefaultType(wp)) {
				items.insert({0, 0, STAT_CLASS_WAYP, 0});
				default_added = true;
			}
			for (const auto &sm : wp->speclist) {
				if (sm.spec == nullptr) continue;
				items.insert({0, 0, sm.spec->class_index, sm.spec->index});
			}
		}
	}

	static WaypointPickerCallbacks instance;
};
/* static */ WaypointPickerCallbacks WaypointPickerCallbacks::instance;

struct BuildRailWaypointWindow : public PickerWindow {
	BuildRailWaypointWindow(WindowDesc &desc, Window *parent) : PickerWindow(desc, parent, TRANSPORT_RAIL, WaypointPickerCallbacks::instance)
	{
		this->ConstructWindow();
		this->InvalidateData();
	}

	static inline HotkeyList hotkeys{"buildrailwaypoint", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}};
};

/** Nested widget definition for the build NewGRF rail waypoint window */
static constexpr NWidgetPart _nested_build_waypoint_widgets[] = {
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

static WindowDesc _build_waypoint_desc(
	WDP_AUTO, "build_waypoint", 0, 0,
	WC_BUILD_WAYPOINT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_waypoint_widgets,
	&BuildRailWaypointWindow::hotkeys
);

static void ShowBuildWaypointPicker(Window *parent)
{
	if (!WaypointPickerCallbacks::instance.IsActive()) return;
	new BuildRailWaypointWindow(_build_waypoint_desc, parent);
}

/**
 * Initialize rail building GUI settings
 */
void InitializeRailGui()
{
	_build_depot_direction = DIAGDIR_NW;
	_station_gui.sel_class = StationClassID::STAT_CLASS_DFLT;
	_station_gui.sel_type = 0;
	_waypoint_gui.sel_class = StationClassID::STAT_CLASS_WAYP;
	_waypoint_gui.sel_type = 0;
}

/**
 * Re-initialize rail-build toolbar after toggling support for electric trains
 * @param disable Boolean whether electric trains are disabled (removed from the game)
 */
void ReinitGuiAfterToggleElrail(bool disable)
{
	extern RailType _last_built_railtype;
	if (disable && _last_built_railtype == RAILTYPE_ELECTRIC) {
		_last_built_railtype = _cur_railtype = RAILTYPE_RAIL;
		BuildRailToolbarWindow *w = dynamic_cast<BuildRailToolbarWindow *>(FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL));
		if (w != nullptr) w->ModifyRailType(_cur_railtype);
	}
	MarkWholeScreenDirty();
}

/** Set the initial (default) railtype to use */
static void SetDefaultRailGui()
{
	if (_local_company == COMPANY_SPECTATOR || !Company::IsValidID(_local_company)) return;

	extern RailType _last_built_railtype;
	RailType rt;
	switch (_settings_client.gui.default_rail_type) {
		case 2: {
			/* Find the most used rail type */
			uint count[RAILTYPE_END];
			memset(count, 0, sizeof(count));
			for (TileIndex t = 0; t < Map::Size(); t++) {
				if (IsTileType(t, MP_RAILWAY) || IsLevelCrossingTile(t) || HasStationTileRail(t) ||
						(IsTileType(t, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL)) {
					count[GetRailType(t)]++;
				}
			}

			rt = static_cast<RailType>(std::max_element(count + RAILTYPE_BEGIN, count + RAILTYPE_END) - count);
			if (count[rt] > 0) break;

			/* No rail, just get the first available one */
			[[fallthrough]];
		}
		case 0: {
			/* Use first available type */
			std::vector<RailType>::const_iterator it = std::find_if(_sorted_railtypes.begin(), _sorted_railtypes.end(),
					[](RailType r) { return HasRailTypeAvail(_local_company, r); });
			rt = it != _sorted_railtypes.end() ? *it : RAILTYPE_BEGIN;
			break;
		}
		case 1: {
			/* Use last available type */
			std::vector<RailType>::const_reverse_iterator it = std::find_if(_sorted_railtypes.rbegin(), _sorted_railtypes.rend(),
					[](RailType r){ return HasRailTypeAvail(_local_company, r); });
			rt = it != _sorted_railtypes.rend() ? *it : RAILTYPE_BEGIN;
			break;
		}
		default:
			NOT_REACHED();
	}

	_last_built_railtype = _cur_railtype = rt;
	BuildRailToolbarWindow *w = dynamic_cast<BuildRailToolbarWindow *>(FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL));
	if (w != nullptr) w->ModifyRailType(_cur_railtype);
}

/**
 * Updates the current signal variant used in the signal GUI
 * to the one adequate to current year.
 */
void ResetSignalVariant(int32_t)
{
	SignalVariant new_variant = (TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC);

	if (new_variant != _cur_signal_variant) {
		Window *w = FindWindowById(WC_BUILD_SIGNAL, 0);
		if (w != nullptr) {
			w->SetDirty();
			w->RaiseWidget((_cur_signal_variant == SIG_ELECTRIC ? WID_BS_ELECTRIC_NORM : WID_BS_SEMAPHORE_NORM) + _cur_signal_type);
		}
		_cur_signal_variant = new_variant;
	}
}

static IntervalTimer<TimerGameCalendar> _check_reset_signal({TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [](auto)
{
	if (TimerGameCalendar::year != _settings_client.gui.semaphore_build_before) return;

	ResetSignalVariant();
});

/**
 * Resets the rail GUI - sets default railtype to build
 * and resets the signal GUI
 */
void InitializeRailGUI()
{
	SetDefaultRailGui();

	_convert_signal_button = false;
	_cur_signal_type = _settings_client.gui.default_signal_type;
	ResetSignalVariant();
}

/**
 * Create a drop down list for all the rail types of the local company.
 * @param for_replacement Whether this list is for the replacement window.
 * @param all_option Whether to add an 'all types' item.
 * @return The populated and sorted #DropDownList.
 */
DropDownList GetRailTypeDropDownList(bool for_replacement, bool all_option)
{
	RailTypes used_railtypes;
	RailTypes avail_railtypes;

	const Company *c = Company::Get(_local_company);

	/* Find the used railtypes. */
	if (for_replacement) {
		avail_railtypes = GetCompanyRailTypes(c->index, false);
		used_railtypes  = GetRailTypes(false);
	} else {
		avail_railtypes = c->avail_railtypes;
		used_railtypes  = GetRailTypes(true);
	}

	DropDownList list;

	if (all_option) {
		list.push_back(MakeDropDownListStringItem(STR_REPLACE_ALL_RAILTYPE, INVALID_RAILTYPE));
	}

	Dimension d = { 0, 0 };
	/* Get largest icon size, to ensure text is aligned on each menu item. */
	if (!for_replacement) {
		for (const auto &rt : _sorted_railtypes) {
			if (!HasBit(used_railtypes, rt)) continue;
			const RailTypeInfo *rti = GetRailTypeInfo(rt);
			d = maxdim(d, GetSpriteSize(rti->gui_sprites.build_x_rail));
		}
	}

	for (const auto &rt : _sorted_railtypes) {
		/* If it's not used ever, don't show it to the user. */
		if (!HasBit(used_railtypes, rt)) continue;

		const RailTypeInfo *rti = GetRailTypeInfo(rt);

		SetDParam(0, rti->strings.menu_text);
		SetDParam(1, rti->max_speed);
		if (for_replacement) {
			list.push_back(MakeDropDownListStringItem(rti->strings.replace_text, rt, !HasBit(avail_railtypes, rt)));
		} else {
			StringID str = rti->max_speed > 0 ? STR_TOOLBAR_RAILTYPE_VELOCITY : STR_JUST_STRING;
			list.push_back(MakeDropDownListIconItem(d, rti->gui_sprites.build_x_rail, PAL_NONE, str, rt, !HasBit(avail_railtypes, rt)));
		}
	}

	if (list.empty()) {
		/* Empty dropdowns are not allowed */
		list.push_back(MakeDropDownListStringItem(STR_NONE, INVALID_RAILTYPE, true));
	}

	return list;
}
