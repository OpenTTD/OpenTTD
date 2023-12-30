/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_gui.cpp %File for dealing with rail construction user interface */

#include "stdafx.h"
#include "gui.h"
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
#include "widgets/dropdown_type.h"
#include "tunnelbridge.h"
#include "tilehighlight_func.h"
#include "spritecache.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"
#include "engine_base.h"
#include "vehicle_func.h"
#include "zoom_func.h"
#include "rail_gui.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "string_func.h"
#include "station_cmd.h"
#include "tunnelbridge_cmd.h"
#include "waypoint_cmd.h"
#include "rail_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "station_map.h"
#include "tunnelbridge_map.h"

#include "widgets/rail_widget.h"

#include "safeguards.h"


static RailType _cur_railtype;               ///< Rail type of the current build-rail toolbar.
static bool _remove_button_clicked;          ///< Flag whether 'remove' toggle-button is currently enabled
static DiagDirection _build_depot_direction; ///< Currently selected depot direction
static uint16_t _cur_waypoint_type;          ///< Currently selected waypoint type
static bool _convert_signal_button;          ///< convert signal button in the signal GUI pressed
static SignalVariant _cur_signal_variant;    ///< set the signal variant (for signal GUI)
static SignalType _cur_signal_type;          ///< set the signal type (for signal GUI)

struct RailStationGUISettings {
	Axis orientation;                 ///< Currently selected rail station orientation

	bool newstations;                 ///< Are custom station definitions available?
	StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
	uint16_t station_type;            ///< %Station type within the currently selected custom station class (if newstations is \c true )
	uint16_t station_count;           ///< Number of custom stations (if newstations is \c true )
};
static RailStationGUISettings _railstation; ///< Settings of the station builder GUI


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
	if (GetRailTileType(tile) == RAIL_TILE_DEPOT) return;
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

void CcRailDepot(Commands, const CommandCost &result, TileIndex tile, RailType, DiagDirection dir)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_20_CONSTRUCTION_RAIL, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();

	tile += TileOffsByDiagDir(dir);

	if (IsTileType(tile, MP_RAILWAY)) {
		PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir], _place_depot_extra_track[dir]);
		PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir + 4], _place_depot_extra_track[dir + 4]);
		PlaceExtraDepotRail(tile, _place_depot_extra_dir[dir + 8], _place_depot_extra_track[dir + 8]);
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

	Axis axis = GetAxisForNewWaypoint(tile);
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
	if (_railstation.station_class == STAT_CLASS_DFLT && _railstation.station_type == 0 && !_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
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
		if (!_railstation.orientation) Swap(w, h);

		RailStationGUISettings params = _railstation;
		RailType rt = _cur_railtype;
		byte numtracks = _settings_client.gui.station_numtracks;
		byte platlength = _settings_client.gui.station_platlength;
		bool adjacent = _ctrl_pressed;

		auto proc = [=](bool test, StationID to_join) -> bool {
			if (test) {
				return Command<CMD_BUILD_RAIL_STATION>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_RAIL_STATION>()), tile, rt, params.orientation, numtracks, platlength, params.station_class, params.station_type, INVALID_STATION, adjacent).Succeeded();
			} else {
				return Command<CMD_BUILD_RAIL_STATION>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, CcStation, tile, rt, params.orientation, numtracks, platlength, params.station_class, params.station_type, to_join, adjacent);
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
		Command<CMD_REMOVE_SIGNALS>::Post(STR_ERROR_CAN_T_REMOVE_SIGNALS_FROM, CcPlaySound_CONSTRUCTION_RAIL, tile, track);
	} else {
		/* Which signals should we cycle through? */
		SignalType cycle_start = _settings_client.gui.cycle_signal_types == SIGNAL_CYCLE_ALL && _settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL ? SIGTYPE_NORMAL : SIGTYPE_PBS;

		if (FindWindowById(WC_BUILD_SIGNAL, 0) != nullptr) {
			/* signal GUI is used */
			Command<CMD_BUILD_SIGNALS>::Post(_convert_signal_button ? STR_ERROR_SIGNAL_CAN_T_CONVERT_SIGNALS_HERE : STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcPlaySound_CONSTRUCTION_RAIL,
				tile, track, _cur_signal_type, _cur_signal_variant, _convert_signal_button, false, _ctrl_pressed, cycle_start, SIGTYPE_LAST, 0, 0);
		} else {
			SignalVariant sigvar = TimerGameCalendar::year < _settings_client.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC;
			Command<CMD_BUILD_SIGNALS>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcPlaySound_CONSTRUCTION_RAIL,
				tile, track, _settings_client.gui.default_signal_type, sigvar, false, false, _ctrl_pressed, cycle_start, SIGTYPE_LAST, 0, 0);

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
				if (_railstation.orientation == 0) Swap(x, y);
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

	BuildRailToolbarWindow(WindowDesc *desc, RailType railtype) : Window(desc)
	{
		this->InitNested(TRANSPORT_RAIL);
		this->SetupRailToolbar(railtype);
		this->DisableWidget(WID_RAT_REMOVE);
		this->last_user_action = INVALID_WID_RAT;

		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (this->IsWidgetLowered(WID_RAT_BUILD_STATION)) SetViewportCatchmentStation(nullptr, true);
		if (this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT)) SetViewportCatchmentWaypoint(nullptr, true);
		if (_settings_client.gui.link_terraform_toolbar) CloseWindowById(WC_SCEN_LAND_GEN, 0, false);
		CloseWindowById(WC_SELECT_STATION, 0);
		this->Window::Close();
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
				if (HandlePlacePushButton(this, WID_RAT_BUILD_WAYPOINT, SPR_CURSOR_WAYPOINT, HT_RECT) && StationClass::Get(STAT_CLASS_WAYP)->GetSpecCount() > 1) {
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

			case WID_RAT_BUILD_DEPOT:
				Command<CMD_BUILD_TRAIN_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_DEPOT, CcRailDepot, tile, _cur_railtype, _build_depot_direction);
				break;

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
							uint16_t waypoint_type = _cur_waypoint_type;

							auto proc = [=](bool test, StationID to_join) -> bool {
								if (test) {
									return Command<CMD_BUILD_RAIL_WAYPOINT>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_RAIL_WAYPOINT>()), ta.tile, axis, ta.w, ta.h, STAT_CLASS_WAYP, waypoint_type, INVALID_STATION, adjacent).Succeeded();
								} else {
									return Command<CMD_BUILD_RAIL_WAYPOINT>::Post(STR_ERROR_CAN_T_BUILD_TRAIN_WAYPOINT, CcPlaySound_CONSTRUCTION_RAIL, ta.tile, axis, ta.w, ta.h, STAT_CLASS_WAYP, waypoint_type, to_join, adjacent);
								}
							};

							ShowSelectWaypointIfNeeded(ta, proc);
						}
					}
					break;
			}
		}
	}

	void OnPlaceObjectAbort() override
	{
		if (this->IsWidgetLowered(WID_RAT_BUILD_STATION)) SetViewportCatchmentStation(nullptr, true);
		if (this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT)) SetViewportCatchmentWaypoint(nullptr, true);

		this->RaiseButtons();
		this->DisableWidget(WID_RAT_REMOVE);
		this->SetWidgetDirty(WID_RAT_REMOVE);

		CloseWindowById(WC_BUILD_SIGNAL, TRANSPORT_RAIL);
		CloseWindowById(WC_BUILD_STATION, TRANSPORT_RAIL);
		CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_RAIL);
		CloseWindowById(WC_BUILD_WAYPOINT, TRANSPORT_RAIL);
		CloseWindowById(WC_SELECT_STATION, 0);
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
		if (this->IsWidgetLowered(WID_RAT_BUILD_WAYPOINT)) CheckRedrawWaypointCoverage(this);
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

static const NWidgetPart _nested_build_rail_widgets[] = {
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

static WindowDesc _build_rail_desc(__FILE__, __LINE__,
	WDP_ALIGN_TOOLBAR, "toolbar_rail", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_rail_widgets), std::end(_nested_build_rail_widgets),
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
	return new BuildRailToolbarWindow(&_build_rail_desc, railtype);
}

/* TODO: For custom stations, respect their allowed platforms/lengths bitmasks!
 * --pasky */

static void HandleStationPlacement(TileIndex start, TileIndex end)
{
	TileArea ta(start, end);
	uint numtracks = ta.w;
	uint platlength = ta.h;

	if (_railstation.orientation == AXIS_X) Swap(numtracks, platlength);

	RailStationGUISettings params = _railstation;
	RailType rt = _cur_railtype;
	bool adjacent = _ctrl_pressed;

	auto proc = [=](bool test, StationID to_join) -> bool {
		if (test) {
			return Command<CMD_BUILD_RAIL_STATION>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_RAIL_STATION>()), ta.tile, rt, params.orientation, numtracks, platlength, params.station_class, params.station_type, INVALID_STATION, adjacent).Succeeded();
		} else {
			return Command<CMD_BUILD_RAIL_STATION>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_STATION, CcStation, ta.tile, rt, params.orientation, numtracks, platlength, params.station_class, params.station_type, to_join, adjacent);
		}
	};

	ShowSelectStationIfNeeded(ta, proc);
}

/** Enum referring to the Hotkeys in the build rail station window */
enum BuildRalStationHotkeys {
	BRASHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
};

struct BuildRailStationWindow : public PickerWindowBase {
private:
	uint line_height;     ///< Height of a single line in the newstation selection matrix (#WID_BRAS_NEWST_LIST widget).
	uint coverage_height; ///< Height of the coverage texts.
	Scrollbar *vscroll;   ///< Vertical scrollbar of the new station list.
	Scrollbar *vscroll2;  ///< Vertical scrollbar of the matrix with new stations.

	typedef GUIList<StationClassID, std::nullptr_t, StringFilter &> GUIStationClassList; ///< Type definition for the list to hold available station classes.

	static const uint EDITBOX_MAX_SIZE = 16; ///< The maximum number of characters for the filter edit box.

	static Listing   last_sorting;           ///< Default sorting of #GUIStationClassList.
	static Filtering last_filtering;         ///< Default filtering of #GUIStationClassList.
	static GUIStationClassList::SortFunction * const sorter_funcs[];   ///< Sort functions of the #GUIStationClassList.
	static GUIStationClassList::FilterFunction * const filter_funcs[]; ///< Filter functions of the #GUIStationClassList.
	GUIStationClassList station_classes;     ///< Available station classes.
	StringFilter string_filter;              ///< Filter for available station classes.
	QueryString filter_editbox;              ///< Filter editbox.

	/**
	 * Scrolls #WID_BRAS_NEWST_SCROLL so that the selected station class is visible.
	 *
	 * Note that this method should be called shortly after SelectClassAndStation() which will ensure
	 * an actual existing station class is selected, or the one at position 0 which will always be
	 * the default TTD rail station.
	 */
	void EnsureSelectedStationClassIsVisible()
	{
		uint pos = 0;
		for (auto station_class : this->station_classes) {
			if (station_class == _railstation.station_class) break;
			pos++;
		}
		this->vscroll->SetCount(this->station_classes.size());
		this->vscroll->ScrollTowards(pos);
	}

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
	BuildRailStationWindow(WindowDesc *desc, Window *parent, bool newstation) : PickerWindowBase(desc, parent), filter_editbox(EDITBOX_MAX_SIZE * MAX_CHAR_LENGTH, EDITBOX_MAX_SIZE)
	{
		this->coverage_height = 2 * GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
		this->vscroll = nullptr;
		_railstation.newstations = newstation;

		this->CreateNestedTree();
		this->GetWidget<NWidgetStacked>(WID_BRAS_SHOW_NEWST_ADDITIONS)->SetDisplayedPlane(newstation ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BRAS_SHOW_NEWST_MATRIX)->SetDisplayedPlane(newstation ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BRAS_SHOW_NEWST_DEFSIZE)->SetDisplayedPlane(newstation ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BRAS_SHOW_NEWST_RESIZE)->SetDisplayedPlane(newstation ? 0 : SZSP_NONE);
		/* Hide the station class filter if no stations other than the default one are available. */
		this->GetWidget<NWidgetStacked>(WID_BRAS_FILTER_CONTAINER)->SetDisplayedPlane(newstation ? 0 : SZSP_NONE);
		if (newstation) {
			this->vscroll = this->GetScrollbar(WID_BRAS_NEWST_SCROLL);
			this->vscroll2 = this->GetScrollbar(WID_BRAS_MATRIX_SCROLL);

			this->querystrings[WID_BRAS_FILTER_EDITBOX] = &this->filter_editbox;
			this->station_classes.SetListing(this->last_sorting);
			this->station_classes.SetFiltering(this->last_filtering);
			this->station_classes.SetSortFuncs(this->sorter_funcs);
			this->station_classes.SetFilterFuncs(this->filter_funcs);
		}

		this->station_classes.ForceRebuild();

		BuildStationClassesAvailable();
		SelectClassAndStation();

		this->FinishInitNested(TRANSPORT_RAIL);

		this->LowerWidget(_railstation.orientation + WID_BRAS_PLATFORM_DIR_X);
		if (_settings_client.gui.station_dragdrop) {
			this->LowerWidget(WID_BRAS_PLATFORM_DRAG_N_DROP);
		} else {
			this->LowerWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
			this->LowerWidget(_settings_client.gui.station_platlength + WID_BRAS_PLATFORM_LEN_BEGIN);
		}
		this->SetWidgetLoweredState(WID_BRAS_HIGHLIGHT_OFF, !_settings_client.gui.station_show_coverage);
		this->SetWidgetLoweredState(WID_BRAS_HIGHLIGHT_ON, _settings_client.gui.station_show_coverage);

		if (!newstation) {
			_railstation.station_class = StationClassID::STAT_CLASS_DFLT;
			_railstation.station_type = 0;
			this->vscroll2 = nullptr;
		} else {
			_railstation.station_count = StationClass::Get(_railstation.station_class)->GetSpecCount();
			_railstation.station_type = std::min<int>(_railstation.station_type, _railstation.station_count - 1);

			NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BRAS_MATRIX);
			matrix->SetScrollbar(this->vscroll2);
			matrix->SetCount(_railstation.station_count);
			matrix->SetClicked(_railstation.station_type);

			EnsureSelectedStationClassIsVisible();
		}

		this->InvalidateData();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_STATION, 0);
		this->PickerWindowBase::Close();
	}

	/** Sort station classes by StationClassID. */
	static bool StationClassIDSorter(StationClassID const &a, StationClassID const &b)
	{
		return a < b;
	}

	/** Filter station classes by class name. */
	static bool CDECL TagNameFilter(StationClassID const * sc, StringFilter &filter)
	{
		filter.ResetState();
		filter.AddLine(GetString(StationClass::Get(*sc)->name));
		return filter.GetState();
	}

	/** Builds the filter list of available station classes. */
	void BuildStationClassesAvailable()
	{
		if (!this->station_classes.NeedRebuild()) return;

		this->station_classes.clear();

		for (uint i = 0; i < StationClass::GetClassCount(); i++) {
			StationClassID station_class_id = (StationClassID)i;
			if (station_class_id == StationClassID::STAT_CLASS_WAYP) {
				// Skip waypoints.
				continue;
			}
			StationClass *station_class = StationClass::Get(station_class_id);
			if (station_class->GetUISpecCount() == 0) continue;
			station_classes.push_back(station_class_id);
		}

		if (_railstation.newstations) {
			this->station_classes.Filter(this->string_filter);
			this->station_classes.shrink_to_fit();
			this->station_classes.RebuildDone();
			this->station_classes.Sort();

			this->vscroll->SetCount(this->station_classes.size());
		}
	}

	/**
	 * Checks if the previously selected current station class and station
	 * can be shown as selected to the user when the dialog is opened.
	 */
	void SelectClassAndStation()
	{
		if (_railstation.station_class == StationClassID::STAT_CLASS_DFLT) {
			/* This happens during the first time the window is open during the game life cycle. */
			this->SelectOtherClass(StationClassID::STAT_CLASS_DFLT);
		} else {
			/* Check if the previously selected station class is not available anymore as a
			 * result of starting a new game without the corresponding NewGRF. */
			bool available = _railstation.station_class < StationClass::GetClassCount();
			this->SelectOtherClass(available ? _railstation.station_class : StationClassID::STAT_CLASS_DFLT);
		}
	}

	/**
	 * Select the specified station class.
	 * @param station_class Station class select.
	 */
	void SelectOtherClass(StationClassID station_class)
	{
		_railstation.station_class = station_class;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		this->BuildStationClassesAvailable();
	}

	EventState OnHotkey(int hotkey) override
	{
		switch (hotkey) {
			case BRASHK_FOCUS_FILTER_BOX:
				this->SetFocusedWidget(WID_BRAS_FILTER_EDITBOX);
				SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
				break;

			default:
				return ES_NOT_HANDLED;
		}

		return ES_HANDLED;
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (widget == WID_BRAS_FILTER_EDITBOX) {
			string_filter.SetFilterTerm(this->filter_editbox.text.buf);
			this->station_classes.SetFilterState(!string_filter.IsEmpty());
			this->station_classes.ForceRebuild();
			this->InvalidateData();
		}
	}

	void OnPaint() override
	{
		bool newstations = _railstation.newstations;
		const StationSpec *statspec = newstations ? StationClass::Get(_railstation.station_class)->GetSpec(_railstation.station_type) : nullptr;

		if (_settings_client.gui.station_dragdrop) {
			SetTileSelectSize(1, 1);
		} else {
			int x = _settings_client.gui.station_numtracks;
			int y = _settings_client.gui.station_platlength;
			if (_railstation.orientation == AXIS_X) Swap(x, y);
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

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_BRAS_NEWST_LIST: {
				Dimension d = {0, 0};
				for (auto station_class : this->station_classes) {
					d = maxdim(d, GetStringBoundingBox(StationClass::Get(station_class)->name));
				}
				size->width = std::max(size->width, d.width + padding.width);
				this->line_height = GetCharacterHeight(FS_NORMAL) + padding.height;
				size->height = 5 * this->line_height;
				resize->height = this->line_height;
				break;
			}

			case WID_BRAS_SHOW_NEWST_TYPE: {
				if (!_railstation.newstations) {
					size->width = 0;
					size->height = 0;
					break;
				}

				/* If newstations exist, compute the non-zero minimal size. */
				Dimension d = {0, 0};
				StringID str = this->GetWidget<NWidgetCore>(widget)->widget_data;
				for (auto station_class : this->station_classes) {
					StationClass *stclass = StationClass::Get(station_class);
					for (uint j = 0; j < stclass->GetSpecCount(); j++) {
						const StationSpec *statspec = stclass->GetSpec(j);
						SetDParam(0, (statspec != nullptr && statspec->name != 0) ? statspec->name : STR_STATION_CLASS_DFLT_STATION);
						d = maxdim(d, GetStringBoundingBox(str));
					}
				}
				size->width = std::max(size->width, d.width + padding.width);
				break;
			}

			case WID_BRAS_PLATFORM_DIR_X:
			case WID_BRAS_PLATFORM_DIR_Y:
			case WID_BRAS_IMAGE:
				size->width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size->height = ScaleGUITrad(58) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;

			case WID_BRAS_COVERAGE_TEXTS:
				size->height = this->coverage_height;
				break;

			case WID_BRAS_MATRIX:
				fill->height = 1;
				resize->height = 1;
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
					int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
					int y = (ir.Height() + ScaleSpriteTrad(58)) / 2 - ScaleSpriteTrad(31);
					if (!DrawStationTile(x, y, _cur_railtype, AXIS_X, _railstation.station_class, _railstation.station_type)) {
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
					int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
					int y = (ir.Height() + ScaleSpriteTrad(58)) / 2 - ScaleSpriteTrad(31);
					if (!DrawStationTile(x, y, _cur_railtype, AXIS_Y, _railstation.station_class, _railstation.station_type)) {
						StationPickerDrawSprite(x, y, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 3);
					}
				}
				break;
			}

			case WID_BRAS_NEWST_LIST: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.matrix);
				uint statclass = 0;
				for (auto station_class : this->station_classes) {
					if (this->vscroll->IsVisible(statclass)) {
						DrawString(ir,
								StationClass::Get(station_class)->name,
								station_class == _railstation.station_class ? TC_WHITE : TC_BLACK);
						ir.top += this->line_height;
					}
					statclass++;
				}
				break;
			}

			case WID_BRAS_IMAGE: {
				uint16_t type = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
				assert(type < _railstation.station_count);
				/* Check station availability callback */
				const StationSpec *statspec = StationClass::Get(_railstation.station_class)->GetSpec(type);

				/* Set up a clipping area for the station preview. */
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
					int y = (ir.Height() + ScaleSpriteTrad(58)) / 2 - ScaleSpriteTrad(31);
					if (!DrawStationTile(x, y, _cur_railtype, _railstation.orientation, _railstation.station_class, type)) {
						StationPickerDrawSprite(x, y, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 2 + _railstation.orientation);
					}
				}
				if (!IsStationAvailable(statspec)) {
					GfxFillRect(ir, PC_BLACK, FILLRECT_CHECKER);
				}
				break;
			}
		}
	}

	void OnResize() override
	{
		if (this->vscroll != nullptr) { // New stations available.
			this->vscroll->SetCapacityFromWidget(this, WID_BRAS_NEWST_LIST);
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_BRAS_SHOW_NEWST_TYPE) {
			const StationSpec *statspec = StationClass::Get(_railstation.station_class)->GetSpec(_railstation.station_type);
			SetDParam(0, (statspec != nullptr && statspec->name != 0) ? statspec->name : STR_STATION_CLASS_DFLT_STATION);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BRAS_PLATFORM_DIR_X:
			case WID_BRAS_PLATFORM_DIR_Y:
				this->RaiseWidget(_railstation.orientation + WID_BRAS_PLATFORM_DIR_X);
				_railstation.orientation = (Axis)(widget - WID_BRAS_PLATFORM_DIR_X);
				this->LowerWidget(_railstation.orientation + WID_BRAS_PLATFORM_DIR_X);
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
				this->RaiseWidget(_settings_client.gui.station_numtracks + WID_BRAS_PLATFORM_NUM_BEGIN);
				this->RaiseWidget(WID_BRAS_PLATFORM_DRAG_N_DROP);

				_settings_client.gui.station_numtracks = widget - WID_BRAS_PLATFORM_NUM_BEGIN;
				_settings_client.gui.station_dragdrop = false;

				const StationSpec *statspec = _railstation.newstations ? StationClass::Get(_railstation.station_class)->GetSpec(_railstation.station_type) : nullptr;
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

				const StationSpec *statspec = _railstation.newstations ? StationClass::Get(_railstation.station_class)->GetSpec(_railstation.station_type) : nullptr;
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
				const StationSpec *statspec = _railstation.newstations ? StationClass::Get(_railstation.station_class)->GetSpec(_railstation.station_type) : nullptr;
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

			case WID_BRAS_NEWST_LIST: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->station_classes, pt.y, this, WID_BRAS_NEWST_LIST);
				if (it == this->station_classes.end()) return;
				StationClassID station_class_id = *it;
				if (_railstation.station_class != station_class_id) {
					StationClass *station_class = StationClass::Get(station_class_id);
					_railstation.station_class = station_class_id;
					_railstation.station_count = station_class->GetSpecCount();
					_railstation.station_type  = 0;

					this->CheckSelectedSize(station_class->GetSpec(_railstation.station_type));

					NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BRAS_MATRIX);
					matrix->SetCount(_railstation.station_count);
					matrix->SetClicked(_railstation.station_type);
				}
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;
			}

			case WID_BRAS_IMAGE: {
				uint16_t y = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
				if (y >= _railstation.station_count) return;

				/* Check station availability callback */
				const StationSpec *statspec = StationClass::Get(_railstation.station_class)->GetSpec(y);
				if (!IsStationAvailable(statspec)) return;

				_railstation.station_type = y;

				this->CheckSelectedSize(statspec);
				this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->SetClicked(_railstation.station_type);

				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				CloseWindowById(WC_SELECT_STATION, 0);
				break;
			}
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawStationCoverage(this);
	}

	IntervalTimer<TimerGameCalendar> yearly_interval = {{TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [this](auto) {
		this->SetDirty();
	}};

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
		Hotkey('F', "focus_filter_box", BRASHK_FOCUS_FILTER_BOX),
	}, BuildRailStationGlobalHotkeys};
};

Listing BuildRailStationWindow::last_sorting = { false, 0 };
Filtering BuildRailStationWindow::last_filtering = { false, 0 };

BuildRailStationWindow::GUIStationClassList::SortFunction * const BuildRailStationWindow::sorter_funcs[] = {
	&StationClassIDSorter,
};

BuildRailStationWindow::GUIStationClassList::FilterFunction * const BuildRailStationWindow::filter_funcs[] = {
	&TagNameFilter,
};

static const NWidgetPart _nested_station_builder_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_RAIL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BRAS_SHOW_NEWST_DEFSIZE),
			NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BRAS_FILTER_CONTAINER),
							NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
								NWidget(WWT_TEXT, COLOUR_DARK_GREEN), SetFill(0, 1), SetDataTip(STR_LIST_FILTER_TITLE, STR_NULL),
								NWidget(WWT_EDITBOX, COLOUR_GREY, WID_BRAS_FILTER_EDITBOX), SetFill(1, 0), SetResize(1, 0),
										SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
							EndContainer(),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BRAS_SHOW_NEWST_ADDITIONS),
							NWidget(NWID_HORIZONTAL),
								NWidget(WWT_MATRIX, COLOUR_GREY, WID_BRAS_NEWST_LIST), SetMinimalSize(122, 71), SetFill(1, 0),
										SetMatrixDataTip(1, 0, STR_STATION_BUILD_STATION_CLASS_TOOLTIP), SetScrollbar(WID_BRAS_NEWST_SCROLL),
								NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BRAS_NEWST_SCROLL),
							EndContainer(),
						EndContainer(),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetDataTip(STR_STATION_BUILD_ORIENTATION, STR_NULL),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_BRAS_PLATFORM_DIR_X), SetMinimalSize(66, 60), SetFill(0, 0), SetDataTip(0x0, STR_STATION_BUILD_RAILROAD_ORIENTATION_TOOLTIP), EndContainer(),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_BRAS_PLATFORM_DIR_Y), SetMinimalSize(66, 60), SetFill(0, 0), SetDataTip(0x0, STR_STATION_BUILD_RAILROAD_ORIENTATION_TOOLTIP), EndContainer(),
						EndContainer(),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN, WID_BRAS_SHOW_NEWST_TYPE), SetMinimalSize(144, 11), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_ORANGE),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetDataTip(STR_STATION_BUILD_NUMBER_OF_TRACKS, STR_NULL),
						NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_1), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_1, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_2), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_2, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_3), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_3, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_4), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_4, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_5), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_5, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_6), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_6, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_NUM_7), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_7, STR_STATION_BUILD_NUMBER_OF_TRACKS_TOOLTIP),
						EndContainer(),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetMinimalSize(144, 11), SetDataTip(STR_STATION_BUILD_PLATFORM_LENGTH, STR_NULL),
						NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_1), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_1, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_2), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_2, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_3), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_3, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_4), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_4, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_5), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_5, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_6), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_6, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_LEN_7), SetMinimalSize(15, 12), SetDataTip(STR_BLACK_7, STR_STATION_BUILD_PLATFORM_LENGTH_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_PLATFORM_DRAG_N_DROP), SetMinimalSize(75, 12), SetDataTip(STR_STATION_BUILD_DRAG_DROP, STR_STATION_BUILD_DRAG_DROP_TOOLTIP),
						EndContainer(),
						NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_HIGHLIGHT_OFF), SetMinimalSize(60, 12),
									SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAS_HIGHLIGHT_ON), SetMinimalSize(60, 12),
									SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BRAS_SHOW_NEWST_MATRIX),
						/* We need an additional background for the matrix, as the matrix cannot handle the scrollbar due to not being an NWidgetCore. */
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BRAS_MATRIX_SCROLL),
							NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BRAS_MATRIX), SetPIP(0, 2, 0),
								NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BRAS_IMAGE), SetMinimalSize(66, 60),
										SetFill(0, 0), SetResize(0, 0), SetDataTip(0x0, STR_STATION_BUILD_STATION_TYPE_TOOLTIP), SetScrollbar(WID_BRAS_MATRIX_SCROLL),
								EndContainer(),
							EndContainer(),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BRAS_COVERAGE_TEXTS), SetFill(1, 1), SetResize(1, 0), SetMinimalTextLines(2, WidgetDimensions::unscaled.vsep_normal),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BRAS_SHOW_NEWST_RESIZE),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BRAS_MATRIX_SCROLL),
				NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** High level window description of the station-build window (default & newGRF) */
static WindowDesc _station_builder_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_station_rail", 350, 0,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_station_builder_widgets), std::end(_nested_station_builder_widgets),
	&BuildRailStationWindow::hotkeys
);

/** Open station build window */
static Window *ShowStationBuilder(Window *parent)
{
	bool newstations = StationClass::GetClassCount() > 2 || StationClass::Get(STAT_CLASS_DFLT)->GetSpecCount() != 1;
	return new BuildRailStationWindow(&_station_builder_desc, parent, newstations);
}

struct BuildSignalWindow : public PickerWindowBase {
private:
	Dimension sig_sprite_size;     ///< Maximum size of signal GUI sprites.
	int sig_sprite_bottom_offset;  ///< Maximum extent of signal GUI sprite from reference point towards bottom.

	/**
	 * Draw dynamic a signal-sprite in a button in the signal GUI
	 * Draw the sprite +1px to the right and down if the button is lowered
	 *
	 * @param widget_index index of this widget in the window
	 * @param image        the sprite to draw
	 */
	void DrawSignalSprite(const Rect &r, WidgetID widget_index, SpriteID image) const
	{
		Point offset;
		Dimension sprite_size = GetSpriteSize(image, &offset);
		Rect ir = r.Shrink(WidgetDimensions::scaled.imgbtn);
		int x = CenterBounds(ir.left, ir.right, sprite_size.width - offset.x) - offset.x; // centered
		int y = ir.top - sig_sprite_bottom_offset +
				(ir.Height() + sig_sprite_size.height) / 2; // aligned to bottom

		DrawSprite(image, PAL_NONE,
				x + this->IsWidgetLowered(widget_index),
				y + this->IsWidgetLowered(widget_index));
	}

	/** Show or hide buttons for non-path signals in the signal GUI */
	void SetSignalUIMode()
	{
		bool show_non_path_signals = (_settings_client.gui.signal_gui_mode == SIGNAL_GUI_ALL);

		this->GetWidget<NWidgetStacked>(WID_BS_SEMAPHORE_NORM_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_ELECTRIC_NORM_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_SEMAPHORE_ENTRY_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_ELECTRIC_ENTRY_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_SEMAPHORE_EXIT_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_ELECTRIC_EXIT_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_SEMAPHORE_COMBO_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_BS_ELECTRIC_COMBO_SEL)->SetDisplayedPlane(show_non_path_signals ? 0 : SZSP_NONE);
	}

public:
	BuildSignalWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
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
		for (uint type = SIGTYPE_NORMAL; type < SIGTYPE_END; type++) {
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

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_BS_DRAG_SIGNALS_DENSITY_LABEL) {
			/* Two digits for signals density. */
			size->width = std::max(size->width, 2 * GetDigitWidth() + padding.width + WidgetDimensions::scaled.framerect.Horizontal());
		} else if (IsInsideMM(widget, WID_BS_SEMAPHORE_NORM, WID_BS_ELECTRIC_PBS_OWAY + 1)) {
			size->width = std::max(size->width, this->sig_sprite_size.width + padding.width);
			size->height = std::max(size->height, this->sig_sprite_size.height + padding.height);
		} else if (widget == WID_BS_CAPTION) {
			size->width += WidgetDimensions::scaled.frametext.Horizontal();
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

			this->DrawSignalSprite(r, widget, sprite);
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
static const NWidgetPart _nested_signal_builder_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BS_CAPTION), SetDataTip(STR_BUILD_SIGNAL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_BS_TOGGLE_SIZE), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_BUILD_SIGNAL_TOGGLE_ADVANCED_SIGNAL_TOOLTIP),
	EndContainer(),
	NWidget(NWID_VERTICAL, NC_EQUALSIZE),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_SEMAPHORE_NORM_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_NORM), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_NORM_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_SEMAPHORE_ENTRY_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_ENTRY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_ENTRY_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_SEMAPHORE_EXIT_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_EXIT), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_EXIT_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_SEMAPHORE_COMBO_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_COMBO), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_COMBO_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_PBS), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_PBS_TOOLTIP), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_SEMAPHORE_PBS_OWAY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_SEMAPHORE_PBS_OWAY_TOOLTIP), EndContainer(),
			NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_BS_CONVERT), SetDataTip(SPR_IMG_SIGNAL_CONVERT, STR_BUILD_SIGNAL_CONVERT_TOOLTIP), SetFill(1, 1),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_ELECTRIC_NORM_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_NORM), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_NORM_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_ELECTRIC_ENTRY_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_ENTRY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_ENTRY_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_ELECTRIC_EXIT_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_EXIT), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_EXIT_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_BS_ELECTRIC_COMBO_SEL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_COMBO), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_COMBO_TOOLTIP), EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_PBS), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_PBS_TOOLTIP), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BS_ELECTRIC_PBS_OWAY), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_ELECTRIC_PBS_OWAY_TOOLTIP), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetDataTip(STR_NULL, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_TOOLTIP), SetFill(1, 1),
				NWidget(WWT_LABEL, COLOUR_DARK_GREEN, WID_BS_DRAG_SIGNALS_DENSITY_LABEL), SetDataTip(STR_JUST_INT, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_TOOLTIP), SetTextStyle(TC_ORANGE), SetFill(1, 1),
				NWidget(NWID_HORIZONTAL), SetPIP(2, 0, 2),
					NWidget(NWID_SPACER), SetFill(1, 0),
					NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_BS_DRAG_SIGNALS_DENSITY_DECREASE), SetMinimalSize(9, 12), SetDataTip(AWV_DECREASE, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_DECREASE_TOOLTIP),
					NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, WID_BS_DRAG_SIGNALS_DENSITY_INCREASE), SetMinimalSize(9, 12), SetDataTip(AWV_INCREASE, STR_BUILD_SIGNAL_DRAG_SIGNALS_DENSITY_INCREASE_TOOLTIP),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Signal selection window description */
static WindowDesc _signal_builder_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_SIGNAL, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_signal_builder_widgets), std::end(_nested_signal_builder_widgets)
);

/**
 * Open the signal selection window
 */
static void ShowSignalBuilder(Window *parent)
{
	new BuildSignalWindow(&_signal_builder_desc, parent);
}

struct BuildRailDepotWindow : public PickerWindowBase {
	BuildRailDepotWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_RAIL);
		this->LowerWidget(_build_depot_direction + WID_BRAD_DEPOT_NE);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (!IsInsideMM(widget, WID_BRAD_DEPOT_NE, WID_BRAD_DEPOT_NW + 1)) return;

		size->width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
		size->height = ScaleGUITrad(48) + WidgetDimensions::scaled.fullbevel.Vertical();
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
				this->RaiseWidget(_build_depot_direction + WID_BRAD_DEPOT_NE);
				_build_depot_direction = (DiagDirection)(widget - WID_BRAD_DEPOT_NE);
				this->LowerWidget(_build_depot_direction + WID_BRAD_DEPOT_NE);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
		}
	}
};

/** Nested widget definition of the build rail depot window */
static const NWidgetPart _nested_build_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_BUILD_DEPOT_TRAIN_ORIENTATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_NW), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_SW), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_NE), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BRAD_DEPOT_SE), SetMinimalSize(66, 50), SetDataTip(0x0, STR_BUILD_DEPOT_TRAIN_ORIENTATION_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_depot_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_depot_widgets), std::end(_nested_build_depot_widgets)
);

static void ShowBuildTrainDepotPicker(Window *parent)
{
	new BuildRailDepotWindow(&_build_depot_desc, parent);
}

struct BuildRailWaypointWindow : PickerWindowBase {
	using WaypointList = GUIList<uint>;
	static const uint FILTER_LENGTH = 20;

	const StationClass *waypoints;
	WaypointList list;
	StringFilter string_filter; ///< Filter for waypoint name
	static QueryString editbox; ///< Filter editbox

	BuildRailWaypointWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->waypoints = StationClass::Get(STAT_CLASS_WAYP);

		this->CreateNestedTree();

		NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BRW_WAYPOINT_MATRIX);
		matrix->SetScrollbar(this->GetScrollbar(WID_BRW_SCROLL));

		this->FinishInitNested(TRANSPORT_RAIL);

		this->querystrings[WID_BRW_FILTER] = &this->editbox;
		this->editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->string_filter.SetFilterTerm(this->editbox.text.buf);

		this->list.ForceRebuild();
		this->BuildPickerList();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_STATION, 0);
		this->PickerWindowBase::Close();
	}

	bool FilterByText(const StationSpec *statspec)
	{
		if (this->string_filter.IsEmpty()) return true;
		this->string_filter.ResetState();
		if (statspec == nullptr) {
			this->string_filter.AddLine(GetString(STR_STATION_CLASS_WAYP_WAYPOINT));
		} else {
			this->string_filter.AddLine(GetString(statspec->name));
			if (statspec->grf_prop.grffile != nullptr) {
				const GRFConfig *gc = GetGRFConfig(statspec->grf_prop.grffile->grfid);
				this->string_filter.AddLine(gc->GetName());
			}
		}
		return this->string_filter.GetState();
	}

	void BuildPickerList()
	{
		if (!this->list.NeedRebuild()) return;

		this->list.clear();
		this->list.reserve(this->waypoints->GetSpecCount());
		for (uint i = 0; i < this->waypoints->GetSpecCount(); i++) {
			const StationSpec *statspec = this->waypoints->GetSpec(i);
			if (!FilterByText(statspec)) continue;

			this->list.push_back(i);
		}
		this->list.RebuildDone();

		NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BRW_WAYPOINT_MATRIX);
		matrix->SetCount((int)this->list.size());
		matrix->SetClicked(this->UpdateSelection(_cur_waypoint_type));
	}

	uint UpdateSelection(uint type)
	{
		auto found = std::find(std::begin(this->list), std::end(this->list), type);
		if (found != std::end(this->list)) return found - std::begin(this->list);

		/* Selection isn't in the list, default to first */
		if (this->list.empty()) {
			_cur_waypoint_type = 0;
			return -1;
		} else {
			_cur_waypoint_type = this->list.front();
			return 0;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_BRW_WAYPOINT_MATRIX:
				/* Two blobs high and three wide. */
				size->width  += resize->width  * 2;
				size->height += resize->height * 1;

				/* Resizing in X direction only at blob size, but at pixel level in Y. */
				resize->height = 1;
				break;

			case WID_BRW_WAYPOINT:
				size->width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size->height = ScaleGUITrad(58) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_BRW_NAME) {
			if (!this->list.empty() && IsInsideBS(_cur_waypoint_type, 0, this->waypoints->GetSpecCount())) {
				const StationSpec *statspec = this->waypoints->GetSpec(_cur_waypoint_type);
				if (statspec == nullptr) {
					SetDParam(0, STR_STATION_CLASS_WAYP_WAYPOINT);
				} else {
					SetDParam(0, statspec->name);
				}
			} else {
				SetDParam(0, STR_EMPTY);
			}
		}
	}

	void OnPaint() override
	{
		this->BuildPickerList();
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BRW_WAYPOINT: {
				uint16_t type = this->list.at(this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement());
				const StationSpec *statspec = this->waypoints->GetSpec(type);

				DrawPixelInfo tmp_dpi;
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(64)) / 2 + ScaleSpriteTrad(31);
					int y = (ir.Height() + ScaleSpriteTrad(58)) / 2 - ScaleSpriteTrad(31);
					DrawWaypointSprite(x, y, type, _cur_railtype);
				}

				if (!IsStationAvailable(statspec)) {
					GfxFillRect(ir, PC_BLACK, FILLRECT_CHECKER);
				}
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BRW_WAYPOINT: {
				uint16_t sel = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
				assert(sel < this->list.size());
				uint16_t type = this->list.at(sel);

				/* Check station availability callback */
				const StationSpec *statspec = this->waypoints->GetSpec(type);
				if (!IsStationAvailable(statspec)) return;

				_cur_waypoint_type = type;
				this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->SetClicked(sel);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
			}
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->list.ForceRebuild();
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_BRW_FILTER) {
			this->string_filter.SetFilterTerm(this->editbox.text.buf);
			this->InvalidateData();
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawWaypointCoverage(this);
	}
};

/* static */ QueryString BuildRailWaypointWindow::editbox(BuildRailWaypointWindow::FILTER_LENGTH * MAX_CHAR_LENGTH, BuildRailWaypointWindow::FILTER_LENGTH);

/** Nested widget definition for the build NewGRF rail waypoint window */
static const NWidgetPart _nested_build_waypoint_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_WAYPOINT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(WWT_EDITBOX, COLOUR_DARK_GREEN, WID_BRW_FILTER), SetPadding(2), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BRW_SCROLL),
			NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BRW_WAYPOINT_MATRIX), SetPIP(0, 2, 0), SetPadding(WidgetDimensions::unscaled.picker),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_BRW_WAYPOINT), SetDataTip(0x0, STR_WAYPOINT_GRAPHICS_TOOLTIP), SetScrollbar(WID_BRW_SCROLL), EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BRW_SCROLL),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
			NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BRW_NAME), SetPadding(2), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_ORANGE), SetAlignment(SA_CENTER),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
};

static WindowDesc _build_waypoint_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_waypoint", 0, 0,
	WC_BUILD_WAYPOINT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_waypoint_widgets), std::end(_nested_build_waypoint_widgets)
);

static void ShowBuildWaypointPicker(Window *parent)
{
	new BuildRailWaypointWindow(&_build_waypoint_desc, parent);
}

/**
 * Initialize rail building GUI settings
 */
void InitializeRailGui()
{
	_build_depot_direction = DIAGDIR_NW;
	_railstation.station_class = StationClassID::STAT_CLASS_DFLT;
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
			FALLTHROUGH;
		}
		case 0: {
			/* Use first available type */
			std::vector<RailType>::const_iterator it = std::find_if(_sorted_railtypes.begin(), _sorted_railtypes.end(),
					[](RailType r){ return HasRailTypeAvail(_local_company, r); });
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
		list.push_back(std::make_unique<DropDownListStringItem>(STR_REPLACE_ALL_RAILTYPE, INVALID_RAILTYPE, false));
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
			list.push_back(std::make_unique<DropDownListStringItem>(rti->strings.replace_text, rt, !HasBit(avail_railtypes, rt)));
		} else {
			StringID str = rti->max_speed > 0 ? STR_TOOLBAR_RAILTYPE_VELOCITY : STR_JUST_STRING;
			list.push_back(std::make_unique<DropDownListIconItem>(d, rti->gui_sprites.build_x_rail, PAL_NONE, str, rt, !HasBit(avail_railtypes, rt)));
		}
	}

	if (list.empty()) {
		/* Empty dropdowns are not allowed */
		list.push_back(std::make_unique<DropDownListStringItem>(STR_NONE, INVALID_RAILTYPE, true));
	}

	return list;
}
