/* $Id$ */

/** @file rail_gui.cpp File for dealing with rail construction user interface */

#include "stdafx.h"
#include "openttd.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "terraform_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "command_func.h"
#include "town_type.h"
#include "waypoint.h"
#include "debug.h"
#include "variables.h"
#include "newgrf_callbacks.h"
#include "newgrf_station.h"
#include "train.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "sound_func.h"
#include "player_func.h"
#include "settings_type.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "tunnelbridge.h"
#include "tilehighlight_func.h"

#include "bridge_map.h"
#include "rail_map.h"
#include "road_map.h"
#include "station_map.h"
#include "tunnel_map.h"
#include "tunnelbridge_map.h"

#include "table/sprites.h"
#include "table/strings.h"

static RailType _cur_railtype;               ///< Rail type of the current build-rail toolbar.
static bool _remove_button_clicked;          ///< Flag whether 'remove' toggle-button is currently enabled
static DiagDirection _build_depot_direction; ///< Currently selected depot direction
static byte _waypoint_count = 1;             ///< Number of waypoint types
static byte _cur_waypoint_type;              ///< Currently selected waypoint type
static bool _convert_signal_button;          ///< convert signal button in the signal GUI pressed
static SignalVariant _cur_signal_variant;    ///< set the signal variant (for signal GUI)
static SignalType _cur_signal_type;          ///< set the signal type (for signal GUI)

static struct {
	byte orientation;                 ///< Currently selected rail station orientation
	byte numtracks;                   ///< Currently selected number of tracks in station (if not \c dragdrop )
	byte platlength;                  ///< Currently selected platform length of station (if not \c dragdrop )
	bool dragdrop;                    ///< Use drag & drop to place a station

	bool newstations;                 ///< Are custom station definitions available?
	StationClassIDByte station_class; ///< Currently selected custom station class (if newstations is \c true )
	byte station_type;                ///< Station type within the currently selected custom station class (if newstations is \c true )
	byte station_count;               ///< Number of custom stations (if newstations is \c true )
} _railstation;


static void HandleStationPlacement(TileIndex start, TileIndex end);
static void ShowBuildTrainDepotPicker(Window *parent);
static void ShowBuildWaypointPicker(Window *parent);
static void ShowStationBuilder(Window *parent);
static void ShowSignalBuilder(Window *parent);

void CcPlaySound1E(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_20_SPLAT_2, tile);
}

static void GenericPlaceRail(TileIndex tile, int cmd)
{
	DoCommandP(tile, _cur_railtype, cmd, CcPlaySound1E,
		_remove_button_clicked ?
		CMD_REMOVE_SINGLE_RAIL | CMD_MSG(STR_1012_CAN_T_REMOVE_RAILROAD_TRACK) | CMD_NO_WATER :
		CMD_BUILD_SINGLE_RAIL | CMD_MSG(STR_1011_CAN_T_BUILD_RAILROAD_TRACK) | CMD_NO_WATER
	);
}

static void PlaceRail_N(TileIndex tile)
{
	int cmd = _tile_fract_coords.x > _tile_fract_coords.y ? 4 : 5;
	GenericPlaceRail(tile, cmd);
}

static void PlaceRail_NE(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_FIX_Y, DDSP_PLACE_RAIL_NE);
}

static void PlaceRail_E(TileIndex tile)
{
	int cmd = _tile_fract_coords.x + _tile_fract_coords.y <= 15 ? 2 : 3;
	GenericPlaceRail(tile, cmd);
}

static void PlaceRail_NW(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_FIX_X, DDSP_PLACE_RAIL_NW);
}

static void PlaceRail_AutoRail(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_RAILDIRS, DDSP_PLACE_AUTORAIL);
}

/**
 * Try to add an additional rail-track at the entrance of a depot
 * @param tile  Tile to use for adding the rail-track
 * @param extra Track to add
 * @see CcRailDepot()
 */
static void PlaceExtraDepotRail(TileIndex tile, uint16 extra)
{
	if (GetRailTileType(tile) != RAIL_TILE_NORMAL) return;
	if ((GetTrackBits(tile) & GB(extra, 8, 8)) == 0) return;

	DoCommandP(tile, _cur_railtype, extra & 0xFF, NULL, CMD_BUILD_SINGLE_RAIL | CMD_NO_WATER);
}

/** Additional pieces of track to add at the entrance of a depot. */
static const uint16 _place_depot_extra[12] = {
	0x0604, 0x2102, 0x1202, 0x0505,  // First additional track for directions 0..3
	0x2400, 0x2801, 0x1800, 0x1401,  // Second additional track
	0x2203, 0x0904, 0x0A05, 0x1103,  // Third additional track
};


void CcRailDepot(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		DiagDirection dir = (DiagDirection)p2;

		SndPlayTileFx(SND_20_SPLAT_2, tile);
		ResetObjectToPlace();

		tile += TileOffsByDiagDir(dir);

		if (IsTileType(tile, MP_RAILWAY)) {
			PlaceExtraDepotRail(tile, _place_depot_extra[dir]);
			PlaceExtraDepotRail(tile, _place_depot_extra[dir + 4]);
			PlaceExtraDepotRail(tile, _place_depot_extra[dir + 8]);
		}
	}
}

static void PlaceRail_Depot(TileIndex tile)
{
	DoCommandP(tile, _cur_railtype, _build_depot_direction, CcRailDepot,
		CMD_BUILD_TRAIN_DEPOT | CMD_NO_WATER | CMD_MSG(STR_100E_CAN_T_BUILD_TRAIN_DEPOT));
}

static void PlaceRail_Waypoint(TileIndex tile)
{
	if (_remove_button_clicked) {
		DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_REMOVE_TRAIN_WAYPOINT | CMD_MSG(STR_CANT_REMOVE_TRAIN_WAYPOINT));
	} else {
		DoCommandP(tile, _cur_waypoint_type, 0, CcPlaySound1E, CMD_BUILD_TRAIN_WAYPOINT | CMD_MSG(STR_CANT_BUILD_TRAIN_WAYPOINT));
	}
}

void CcStation(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_20_SPLAT_2, tile);
		/* Only close the station builder window if the default station is chosen. */
		if (_railstation.station_class == STAT_CLASS_DFLT && _railstation.station_type == 0) ResetObjectToPlace();
	}
}

static void PlaceRail_Station(TileIndex tile)
{
	if (_remove_button_clicked) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_REMOVE_STATION);
		VpSetPlaceSizingLimit(-1);
	} else if (_railstation.dragdrop) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_BUILD_STATION);
		VpSetPlaceSizingLimit(_settings.station.station_spread);
	} else {
		DoCommandP(tile,
				_railstation.orientation | (_railstation.numtracks << 8) | (_railstation.platlength << 16) | (_ctrl_pressed << 24),
				_cur_railtype | (_railstation.station_class << 8) | (_railstation.station_type << 16), CcStation,
				CMD_BUILD_RAILROAD_STATION | CMD_NO_WATER | CMD_MSG(STR_100F_CAN_T_BUILD_RAILROAD_STATION));
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
		DoCommandP(tile, track, 0, CcPlaySound1E,
			CMD_REMOVE_SIGNALS | CMD_MSG(STR_1013_CAN_T_REMOVE_SIGNALS_FROM));
	} else {
		const Window *w = FindWindowById(WC_BUILD_SIGNAL, 0);

		/* various bitstuffed elements for CmdBuildSingleSignal() */
		uint32 p1 = track;

		if (w != NULL) {
			/* signal GUI is used */
			SB(p1, 3, 1, _ctrl_pressed);
			SB(p1, 4, 1, _cur_signal_variant);
			SB(p1, 5, 2, _cur_signal_type);
			SB(p1, 7, 1, _convert_signal_button);
		} else {
			SB(p1, 3, 1, _ctrl_pressed);
			SB(p1, 4, 1, (_cur_year < _settings.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC));
			SB(p1, 5, 2, SIGTYPE_NORMAL);
			SB(p1, 7, 1, 0);
		}

		DoCommandP(tile, p1, 0, CcPlaySound1E, CMD_BUILD_SIGNALS |
			CMD_MSG((w != NULL && _convert_signal_button) ? STR_SIGNAL_CAN_T_CONVERT_SIGNALS_HERE : STR_1010_CAN_T_BUILD_SIGNALS_HERE));
	}
}

static void PlaceRail_Bridge(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
}

/** Command callback for building a tunnel */
void CcBuildRailTunnel(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_20_SPLAT_2, tile);
		ResetObjectToPlace();
	} else {
		SetRedErrorSquare(_build_tunnel_endtile);
	}
}

static void PlaceRail_Tunnel(TileIndex tile)
{
	DoCommandP(tile, _cur_railtype, 0, CcBuildRailTunnel,
		CMD_BUILD_TUNNEL | CMD_MSG(STR_5016_CAN_T_BUILD_TUNNEL_HERE));
}

static void PlaceRail_ConvertRail(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CONVERT_RAIL);
}

static void PlaceRail_AutoSignals(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_SIGNALDIRS, DDSP_BUILD_SIGNALS);
}


/** Enum referring to the widgets of the build rail toolbar */
enum RailToolbarWidgets {
	RTW_CLOSEBOX = 0,
	RTW_CAPTION,
	RTW_STICKY,
	RTW_SPACER,
	RTW_BUILD_NS,
	RTW_BUILD_X,
	RTW_BUILD_EW,
	RTW_BUILD_Y,
	RTW_AUTORAIL,
	RTW_DEMOLISH,
	RTW_BUILD_DEPOT,
	RTW_BUILD_WAYPOINT,
	RTW_BUILD_STATION,
	RTW_BUILD_SIGNALS,
	RTW_BUILD_BRIDGE,
	RTW_BUILD_TUNNEL,
	RTW_REMOVE,
	RTW_CONVERT_RAIL,
};


/** Toggles state of the Remove button of Build rail toolbar
 * @param w window the button belongs to
 */
static void ToggleRailButton_Remove(Window *w)
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
static bool RailToolbar_CtrlChanged(Window *w)
{
	if (w->IsWidgetDisabled(RTW_REMOVE)) return false;

	/* allow ctrl to switch remove mode only for these widgets */
	for (uint i = RTW_BUILD_NS; i <= RTW_BUILD_STATION; i++) {
		if ((i <= RTW_AUTORAIL || i >= RTW_BUILD_WAYPOINT) && w->IsWidgetLowered(i)) {
			ToggleRailButton_Remove(w);
			return true;
		}
	}

	return false;
}


/**
 * The "rail N"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_N(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_NS, GetRailTypeInfo(_cur_railtype)->cursor.rail_ns, VHM_RECT, PlaceRail_N);
}

/**
 * The "rail NE"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_NE(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_X, GetRailTypeInfo(_cur_railtype)->cursor.rail_swne, VHM_RECT, PlaceRail_NE);
}

/**
 * The "rail E"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_E(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_EW, GetRailTypeInfo(_cur_railtype)->cursor.rail_ew, VHM_RECT, PlaceRail_E);
}

/**
 * The "rail NW"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_NW(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_Y, GetRailTypeInfo(_cur_railtype)->cursor.rail_nwse, VHM_RECT, PlaceRail_NW);
}

/**
 * The "auto-rail"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_AutoRail(Window *w)
{
	HandlePlacePushButton(w, RTW_AUTORAIL, GetRailTypeInfo(_cur_railtype)->cursor.autorail, VHM_RAIL, PlaceRail_AutoRail);
}

/**
 * The "demolish"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, RTW_DEMOLISH, ANIMCURSOR_DEMOLISH, VHM_RECT, PlaceProc_DemolishArea);
}

/**
 * The "build depot"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Depot(Window *w)
{
	if (HandlePlacePushButton(w, RTW_BUILD_DEPOT, GetRailTypeInfo(_cur_railtype)->cursor.depot, VHM_RECT, PlaceRail_Depot)) {
		ShowBuildTrainDepotPicker(w);
	}
}

/**
 * The "build waypoint"-button click proc of the build-rail toolbar.
 * If there are newGRF waypoints, also open a window to pick the waypoint type.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Waypoint(Window *w)
{
	_waypoint_count = GetNumCustomStations(STAT_CLASS_WAYP);
	if (HandlePlacePushButton(w, RTW_BUILD_WAYPOINT, SPR_CURSOR_WAYPOINT, VHM_RECT, PlaceRail_Waypoint) &&
			_waypoint_count > 1) {
		ShowBuildWaypointPicker(w);
	}
}

/**
 * The "build station"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Station(Window *w)
{
	if (HandlePlacePushButton(w, RTW_BUILD_STATION, SPR_CURSOR_RAIL_STATION, VHM_RECT, PlaceRail_Station)) ShowStationBuilder(w);
}

/**
 * The "build signal"-button click proc of the build-rail toolbar.
 * Start ShowSignalBuilder() and/or HandleAutoSignalPlacement().
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_AutoSignals(Window *w)
{
	if (_settings.gui.enable_signal_gui != _ctrl_pressed) {
		if (HandlePlacePushButton(w, RTW_BUILD_SIGNALS, ANIMCURSOR_BUILDSIGNALS, VHM_RECT, PlaceRail_AutoSignals)) ShowSignalBuilder(w);
	} else {
		HandlePlacePushButton(w, RTW_BUILD_SIGNALS, ANIMCURSOR_BUILDSIGNALS, VHM_RECT, PlaceRail_AutoSignals);
	}
}

/**
 * The "build bridge"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Bridge(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, VHM_RECT, PlaceRail_Bridge);
}

/**
 * The "build tunnel"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Tunnel(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_TUNNEL, GetRailTypeInfo(_cur_railtype)->cursor.tunnel, VHM_SPECIAL, PlaceRail_Tunnel);
}

/**
 * The "remove"-button click proc of the build-rail toolbar.
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Remove(Window *w)
{
	if (w->IsWidgetDisabled(RTW_REMOVE)) return;
	ToggleRailButton_Remove(w);
	SndPlayFx(SND_15_BEEP);

	/* handle station builder */
	if (w->IsWidgetLowered(RTW_BUILD_STATION)) {
		if (_remove_button_clicked) {
			/* starting drag & drop remove */
			if (!_railstation.dragdrop) {
				SetTileSelectSize(1, 1);
			} else {
				VpSetPlaceSizingLimit(-1);
			}
		} else {
			/* starting station build mode */
			if (!_railstation.dragdrop) {
				int x = _railstation.numtracks;
				int y = _railstation.platlength;
				if (_railstation.orientation == 0) Swap(x, y);
				SetTileSelectSize(x, y);
			} else {
				VpSetPlaceSizingLimit(_settings.station.station_spread);
			}
		}
	}
}

/**
 * The "convert-rail"-button click proc of the build-rail toolbar.
 * Switches to 'convert-rail' mode
 * @param w Build-rail toolbar window
 * @see BuildRailToolbWndProc()
 */
static void BuildRailClick_Convert(Window *w)
{
	HandlePlacePushButton(w, RTW_CONVERT_RAIL, GetRailTypeInfo(_cur_railtype)->cursor.convert, VHM_RECT, PlaceRail_ConvertRail);
}


static void DoRailroadTrack(int mode)
{
	DoCommandP(TileVirtXY(_thd.selstart.x, _thd.selstart.y), TileVirtXY(_thd.selend.x, _thd.selend.y), _cur_railtype | (mode << 4), NULL,
		_remove_button_clicked ?
		CMD_REMOVE_RAILROAD_TRACK | CMD_NO_WATER | CMD_MSG(STR_1012_CAN_T_REMOVE_RAILROAD_TRACK) :
		CMD_BUILD_RAILROAD_TRACK  | CMD_NO_WATER | CMD_MSG(STR_1011_CAN_T_BUILD_RAILROAD_TRACK)
	);
}

static void HandleAutodirPlacement()
{
	TileHighlightData *thd = &_thd;
	int trackstat = thd->drawstyle & 0xF; // 0..5

	if (thd->drawstyle & HT_RAIL) { // one tile case
		GenericPlaceRail(TileVirtXY(thd->selend.x, thd->selend.y), trackstat);
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
	TileHighlightData *thd = &_thd;
	uint32 p2 = GB(thd->drawstyle, 0, 3); // 0..5

	if (thd->drawstyle == HT_RECT) { // one tile case
		GenericPlaceSignals(TileVirtXY(thd->selend.x, thd->selend.y));
		return;
	}

	const Window *w = FindWindowById(WC_BUILD_SIGNAL, 0);

	if (w != NULL) {
		/* signal GUI is used */
		SB(p2,  3, 1, 0);
		SB(p2,  4, 1, _cur_signal_variant);
		SB(p2,  6, 1, _ctrl_pressed);
		SB(p2, 24, 8, _settings.gui.drag_signals_density);
	} else {
		SB(p2,  3, 1, 0);
		SB(p2,  4, 1, (_cur_year < _settings.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC));
		SB(p2,  6, 1, _ctrl_pressed);
		SB(p2, 24, 8, _settings.gui.drag_signals_density);
	}

	/* _settings.gui.drag_signals_density is given as a parameter such that each user
	 * in a network game can specify his/her own signal density */
	DoCommandP(
		TileVirtXY(thd->selstart.x, thd->selstart.y),
		TileVirtXY(thd->selend.x, thd->selend.y),
		p2,
		CcPlaySound1E,
		_remove_button_clicked ?
			CMD_REMOVE_SIGNAL_TRACK | CMD_NO_WATER | CMD_MSG(STR_1013_CAN_T_REMOVE_SIGNALS_FROM) :
			CMD_BUILD_SIGNAL_TRACK  | CMD_NO_WATER | CMD_MSG(STR_1010_CAN_T_BUILD_SIGNALS_HERE)
	);
}


typedef void OnButtonClick(Window *w);

static OnButtonClick * const _build_railroad_button_proc[] = {
	BuildRailClick_N,
	BuildRailClick_NE,
	BuildRailClick_E,
	BuildRailClick_NW,
	BuildRailClick_AutoRail,
	BuildRailClick_Demolish,
	BuildRailClick_Depot,
	BuildRailClick_Waypoint,
	BuildRailClick_Station,
	BuildRailClick_AutoSignals,
	BuildRailClick_Bridge,
	BuildRailClick_Tunnel,
	BuildRailClick_Remove,
	BuildRailClick_Convert
};

static const uint16 _rail_keycodes[] = {
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7', // depot
	'8', // waypoint
	'9', // station
	'S', // signals
	'B', // bridge
	'T', // tunnel
	'R', // remove
	'C', // convert rail
};


/**
 * Based on the widget clicked, update the status of the 'remove' button.
 * @param w              Rail toolbar window
 * @param clicked_widget Widget clicked in the toolbar
 */
struct BuildRailToolbarWindow : Window {
	BuildRailToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->DisableWidget(RTW_REMOVE);

		this->FindWindowPlacementAndResize(desc);
		if (_settings.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildRailToolbarWindow()
	{
		if (_settings.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0);
	}

	void UpdateRemoveWidgetStatus(int clicked_widget)
	{
		switch (clicked_widget) {
			case RTW_REMOVE:
				/* If it is the removal button that has been clicked, do nothing,
				* as it is up to the other buttons to drive removal status */
				return;
				break;
			case RTW_BUILD_NS:
			case RTW_BUILD_X:
			case RTW_BUILD_EW:
			case RTW_BUILD_Y:
			case RTW_AUTORAIL:
			case RTW_BUILD_WAYPOINT:
			case RTW_BUILD_STATION:
			case RTW_BUILD_SIGNALS:
				/* Removal button is enabled only if the rail/signal/waypoint/station
				* button is still lowered.  Once raised, it has to be disabled */
				this->SetWidgetDisabledState(RTW_REMOVE, !this->IsWidgetLowered(clicked_widget));
				break;

			default:
				/* When any other buttons than rail/signal/waypoint/station, raise and
				* disable the removal button */
				this->DisableWidget(RTW_REMOVE);
				this->RaiseWidget(RTW_REMOVE);
				break;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= RTW_BUILD_NS) {
			_remove_button_clicked = false;
			_build_railroad_button_proc[widget - RTW_BUILD_NS](this);
		}
		this->UpdateRemoveWidgetStatus(widget);
		if (_ctrl_pressed) RailToolbar_CtrlChanged(this);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		for (uint8 i = 0; i != lengthof(_rail_keycodes); i++) {
			if (keycode == _rail_keycodes[i]) {
				_remove_button_clicked = false;
				_build_railroad_button_proc[i](this);
				this->UpdateRemoveWidgetStatus(i + RTW_BUILD_NS);
				if (_ctrl_pressed) RailToolbar_CtrlChanged(this);
				state = ES_HANDLED;
				break;
			}
		}
		MarkTileDirty(_thd.pos.x, _thd.pos.y); // redraw tile selection
		return state;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		/* no dragging if you have pressed the convert button */
		if (FindWindowById(WC_BUILD_SIGNAL, 0) != NULL && _convert_signal_button && this->IsWidgetLowered(RTW_BUILD_SIGNALS)) return;

		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1) {
			switch (select_proc) {
				default: NOT_REACHED();
				case DDSP_BUILD_BRIDGE:
					ResetObjectToPlace();
					ShowBuildBridgeWindow(start_tile, end_tile, TRANSPORT_RAIL, _cur_railtype);
					break;

				case DDSP_PLACE_AUTORAIL:
					HandleAutodirPlacement();
					break;

				case DDSP_BUILD_SIGNALS:
					HandleAutoSignalPlacement();
					break;

				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;

				case DDSP_CONVERT_RAIL:
					DoCommandP(end_tile, start_tile, _cur_railtype, CcPlaySound10, CMD_CONVERT_RAIL | CMD_MSG(STR_CANT_CONVERT_RAIL));
					break;

				case DDSP_REMOVE_STATION:
				case DDSP_BUILD_STATION:
					if (_remove_button_clicked) {
						DoCommandP(end_tile, start_tile, 0, CcPlaySound1E, CMD_REMOVE_FROM_RAILROAD_STATION | CMD_MSG(STR_CANT_REMOVE_PART_OF_STATION));
						break;
					}
					HandleStationPlacement(start_tile, end_tile);
					break;

				case DDSP_PLACE_RAIL_NE:
				case DDSP_PLACE_RAIL_NW:
					DoRailroadTrack(select_proc == DDSP_PLACE_RAIL_NE ? TRACK_X : TRACK_Y);
					break;
			}
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
		this->DisableWidget(RTW_REMOVE);
		this->InvalidateWidget(RTW_REMOVE);

		delete FindWindowById(WC_BUILD_SIGNAL, 0);
		delete FindWindowById(WC_BUILD_STATION, 0);
		delete FindWindowById(WC_BUILD_DEPOT, 0);
	}

	virtual void OnPlacePresize(Point pt, TileIndex tile)
	{
		DoCommand(tile, 0, 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
	}

	virtual EventState OnCTRLStateChange()
	{
		/* do not toggle Remove button by Ctrl when placing station */
		if (!this->IsWidgetLowered(RTW_BUILD_STATION) && RailToolbar_CtrlChanged(this)) return ES_HANDLED;
		return ES_NOT_HANDLED;
	}
};

/** Widget definition for the rail toolbar */
static const Widget _build_rail_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},                   // RTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   337,     0,    13, STR_100A_RAILROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},         // RTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   338,   349,     0,    13, 0x0,                            STR_STICKY_BUTTON},                       // RTW_STICKY

{      WWT_PANEL,   RESIZE_NONE,     7,   110,   113,    14,    35, 0x0,                            STR_NULL},                                // RTW_SPACER

{     WWT_IMGBTN,   RESIZE_NONE,     7,    0,     21,    14,    35, SPR_IMG_RAIL_NS,                STR_1018_BUILD_RAILROAD_TRACK},           // RTW_BUILD_NS
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_RAIL_NE,                STR_1018_BUILD_RAILROAD_TRACK},           // RTW_BUILD_X
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_RAIL_EW,                STR_1018_BUILD_RAILROAD_TRACK},           // RTW_BUILD_EW
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,    87,    14,    35, SPR_IMG_RAIL_NW,                STR_1018_BUILD_RAILROAD_TRACK},           // RTW_BUILD_Y
{     WWT_IMGBTN,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_IMG_AUTORAIL,               STR_BUILD_AUTORAIL_TIP},                  // RTW_AUTORAIL

{     WWT_IMGBTN,   RESIZE_NONE,     7,   114,   135,    14,    35, SPR_IMG_DYNAMITE,               STR_018D_DEMOLISH_BUILDINGS_ETC},         // RTW_DEMOLISH
{     WWT_IMGBTN,   RESIZE_NONE,     7,   136,   157,    14,    35, SPR_IMG_DEPOT_RAIL,             STR_1019_BUILD_TRAIN_DEPOT_FOR_BUILDING}, // RTW_BUILD_DEPOT
{     WWT_IMGBTN,   RESIZE_NONE,     7,   158,   179,    14,    35, SPR_IMG_WAYPOINT,               STR_CONVERT_RAIL_TO_WAYPOINT_TIP},        // RTW_BUILD_WAYPOINT

{     WWT_IMGBTN,   RESIZE_NONE,     7,   180,   221,    14,    35, SPR_IMG_RAIL_STATION,           STR_101A_BUILD_RAILROAD_STATION},         // RTW_BUILD_STATION
{     WWT_IMGBTN,   RESIZE_NONE,     7,   222,   243,    14,    35, SPR_IMG_RAIL_SIGNALS,           STR_101B_BUILD_RAILROAD_SIGNALS},         // RTW_BUILD_SIGNALS
{     WWT_IMGBTN,   RESIZE_NONE,     7,   244,   285,    14,    35, SPR_IMG_BRIDGE,                 STR_101C_BUILD_RAILROAD_BRIDGE},          // RTW_BUILD_BRIDGE
{     WWT_IMGBTN,   RESIZE_NONE,     7,   286,   305,    14,    35, SPR_IMG_TUNNEL_RAIL,            STR_101D_BUILD_RAILROAD_TUNNEL},          // RTW_BUILD_TUNNEL
{     WWT_IMGBTN,   RESIZE_NONE,     7,   306,   327,    14,    35, SPR_IMG_REMOVE,                 STR_101E_TOGGLE_BUILD_REMOVE_FOR},        // RTW_REMOVE
{     WWT_IMGBTN,   RESIZE_NONE,     7,   328,   349,    14,    35, SPR_IMG_CONVERT_RAIL,           STR_CONVERT_RAIL_TIP},                    // RTW_CONVERT_RAIL

{   WIDGETS_END},
};

static const WindowDesc _build_rail_desc = {
	WDP_ALIGN_TBR, 22, 350, 36, 350, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_rail_widgets,
};


/** Configures the rail toolbar for railtype given
 * @param railtype the railtype to display
 * @param w the window to modify
 */
static void SetupRailToolbar(RailType railtype, Window *w)
{
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);

	assert(railtype < RAILTYPE_END);
	w->widget[RTW_CAPTION].data = rti->strings.toolbar_caption;
	w->widget[RTW_BUILD_NS].data = rti->gui_sprites.build_ns_rail;
	w->widget[RTW_BUILD_X].data = rti->gui_sprites.build_x_rail;
	w->widget[RTW_BUILD_EW].data = rti->gui_sprites.build_ew_rail;
	w->widget[RTW_BUILD_Y].data = rti->gui_sprites.build_y_rail;
	w->widget[RTW_AUTORAIL].data = rti->gui_sprites.auto_rail;
	w->widget[RTW_BUILD_DEPOT].data = rti->gui_sprites.build_depot;
	w->widget[RTW_CONVERT_RAIL].data = rti->gui_sprites.convert_rail;
	w->widget[RTW_BUILD_TUNNEL].data = rti->gui_sprites.build_tunnel;
}

/**
 * Open the build rail toolbar window for a specific rail type.
 * The window may be opened in the 'normal' way by clicking at the rail icon in
 * the main toolbar, or by means of selecting one of the functions of the
 * toolbar. In the latter case, the corresponding widget is also selected.
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @param railtype Rail type to open the window for
 * @param button   Widget clicked (\c -1 means no button clicked)
 */
void ShowBuildRailToolbar(RailType railtype, int button)
{
	BuildRailToolbarWindow *w;

	if (!IsValidPlayer(_current_player)) return;
	if (!ValParamRailtype(railtype)) return;

	// don't recreate the window if we're clicking on a button and the window exists.
	if (button < 0 || !(w = dynamic_cast<BuildRailToolbarWindow*>(FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL)))) {
		DeleteWindowByClass(WC_BUILD_TOOLBAR);
		_cur_railtype = railtype;
		w = AllocateWindowDescFront<BuildRailToolbarWindow>(&_build_rail_desc, TRANSPORT_RAIL);
		SetupRailToolbar(railtype, w);
	}

	_remove_button_clicked = false;
	if (w != NULL && button >= RTW_CLOSEBOX) {
		_build_railroad_button_proc[button](w);
		w->UpdateRemoveWidgetStatus(button + RTW_BUILD_NS);
	}
}

/* TODO: For custom stations, respect their allowed platforms/lengths bitmasks!
 * --pasky */

static void HandleStationPlacement(TileIndex start, TileIndex end)
{
	uint sx = TileX(start);
	uint sy = TileY(start);
	uint ex = TileX(end);
	uint ey = TileY(end);
	uint w,h;

	if (sx > ex) Swap(sx, ex);
	if (sy > ey) Swap(sy, ey);
	w = ex - sx + 1;
	h = ey - sy + 1;
	if (!_railstation.orientation) Swap(w, h);

	DoCommandP(TileXY(sx, sy),
			_railstation.orientation | (w << 8) | (h << 16) | (_ctrl_pressed << 24),
			_cur_railtype | (_railstation.station_class << 8) | (_railstation.station_type << 16), CcStation,
			CMD_BUILD_RAILROAD_STATION | CMD_NO_WATER | CMD_MSG(STR_100F_CAN_T_BUILD_RAILROAD_STATION));
}

struct BuildRailStationWindow : public PickerWindowBase {
private:
	/** Enum referring to the widgets of the rail stations window */
	enum BuildRailStationWidgets {
		BRSW_CLOSEBOX = 0,
		BRSW_CAPTION,
		BRSW_BACKGROUND,

		BRSW_PLATFORM_DIR_X,
		BRSW_PLATFORM_DIR_Y,

		BRSW_PLATFORM_NUM_BEGIN = BRSW_PLATFORM_DIR_Y,
		BRSW_PLATFORM_NUM_1,
		BRSW_PLATFORM_NUM_2,
		BRSW_PLATFORM_NUM_3,
		BRSW_PLATFORM_NUM_4,
		BRSW_PLATFORM_NUM_5,
		BRSW_PLATFORM_NUM_6,
		BRSW_PLATFORM_NUM_7,

		BRSW_PLATFORM_LEN_BEGIN = BRSW_PLATFORM_NUM_7,
		BRSW_PLATFORM_LEN_1,
		BRSW_PLATFORM_LEN_2,
		BRSW_PLATFORM_LEN_3,
		BRSW_PLATFORM_LEN_4,
		BRSW_PLATFORM_LEN_5,
		BRSW_PLATFORM_LEN_6,
		BRSW_PLATFORM_LEN_7,

		BRSW_PLATFORM_DRAG_N_DROP,

		BRSW_HIGHLIGHT_OFF,
		BRSW_HIGHLIGHT_ON,

		BRSW_NEWST_DROPDOWN,
		BRSW_NEWST_LIST,
		BRSW_NEWST_SCROLL
	};

	/**
	 * Verify whether the currently selected station size is allowed after selecting a new station class/type.
	 * If not, change the station size variables ( _railstation.numtracks and _railstation.platlength ).
	 * @param statspec Specification of the new station class/type
	 */
	void CheckSelectedSize(const StationSpec *statspec)
	{
		if (statspec == NULL || _railstation.dragdrop) return;

		/* If current number of tracks is not allowed, make it as big as possible (which is always less than currently selected) */
		if (HasBit(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
			this->RaiseWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
			_railstation.numtracks = 1;
			while (HasBit(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
				_railstation.numtracks++;
			}
			this->LowerWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
		}

		if (HasBit(statspec->disallowed_lengths, _railstation.platlength - 1)) {
			this->RaiseWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
			_railstation.platlength = 1;
			while (HasBit(statspec->disallowed_lengths, _railstation.platlength - 1)) {
				_railstation.platlength++;
			}
			this->LowerWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
		}
	}

	/** Build a dropdown list of available station classes */
	static DropDownList *BuildStationClassDropDown()
	{
		DropDownList *list = new DropDownList();

		for (uint i = 0; i < GetNumStationClasses(); i++) {
			if (i == STAT_CLASS_WAYP) continue;
			list->push_back(new DropDownListStringItem(GetStationClassName((StationClassID)i), i, false));
		}

		return list;
	}
/**
 * Window event handler of station build window.
 * @param w Staion build window
 * @param e Window event to handle
 */

public:
	BuildRailStationWindow(const WindowDesc *desc, Window *parent, bool newstation) : PickerWindowBase(desc, parent)
	{
		this->LowerWidget(_railstation.orientation + BRSW_PLATFORM_DIR_X);
		if (_railstation.dragdrop) {
			this->LowerWidget(BRSW_PLATFORM_DRAG_N_DROP);
		} else {
			this->LowerWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
			this->LowerWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
		}
		this->SetWidgetLoweredState(BRSW_HIGHLIGHT_OFF, !_station_show_coverage);
		this->SetWidgetLoweredState(BRSW_HIGHLIGHT_ON, _station_show_coverage);

		this->FindWindowPlacementAndResize(desc);

		_railstation.newstations = newstation;

		if (newstation) {
			_railstation.station_count = GetNumCustomStations(_railstation.station_class);

			this->vscroll.count = _railstation.station_count;
			this->vscroll.cap   = 5;
			this->vscroll.pos   = Clamp(_railstation.station_type - 2, 0, this->vscroll.count - this->vscroll.cap);
		}
	}

	virtual void OnPaint()
	{
		bool newstations = _railstation.newstations;
		DrawPixelInfo tmp_dpi, *old_dpi;
		const StationSpec *statspec = newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;

		if (_railstation.dragdrop) {
			SetTileSelectSize(1, 1);
		} else {
			int x = _railstation.numtracks;
			int y = _railstation.platlength;
			if (_railstation.orientation == 0) Swap(x, y);
			if (!_remove_button_clicked)
				SetTileSelectSize(x, y);
		}

		int rad = (_settings.station.modified_catchment) ? CA_TRAIN : CA_UNMODIFIED;

		if (_station_show_coverage)
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);

		for (uint bits = 0; bits < 7; bits++) {
			bool disable = bits >= _settings.station.station_spread;
			if (statspec == NULL) {
				this->SetWidgetDisabledState(bits + BRSW_PLATFORM_NUM_1, disable);
				this->SetWidgetDisabledState(bits + BRSW_PLATFORM_LEN_1, disable);
			} else {
				this->SetWidgetDisabledState(bits + BRSW_PLATFORM_NUM_1, HasBit(statspec->disallowed_platforms, bits) || disable);
				this->SetWidgetDisabledState(bits + BRSW_PLATFORM_LEN_1, HasBit(statspec->disallowed_lengths,   bits) || disable);
			}
		}

		SetDParam(0, GetStationClassName(_railstation.station_class));
		this->DrawWidgets();

		int y_offset = newstations ? 90 : 0;

		/* Set up a clipping area for the '/' station preview */
		if (FillDrawPixelInfo(&tmp_dpi, 7, 26 + y_offset, 66, 48)) {
			old_dpi = _cur_dpi;
			_cur_dpi = &tmp_dpi;
			if (!DrawStationTile(32, 16, _cur_railtype, AXIS_X, _railstation.station_class, _railstation.station_type)) {
				StationPickerDrawSprite(32, 16, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 2);
			}
			_cur_dpi = old_dpi;
		}

		/* Set up a clipping area for the '\' station preview */
		if (FillDrawPixelInfo(&tmp_dpi, 75, 26 + y_offset, 66, 48)) {
			old_dpi = _cur_dpi;
			_cur_dpi = &tmp_dpi;
			if (!DrawStationTile(32, 16, _cur_railtype, AXIS_Y, _railstation.station_class, _railstation.station_type)) {
				StationPickerDrawSprite(32, 16, STATION_RAIL, _cur_railtype, INVALID_ROADTYPE, 3);
			}
			_cur_dpi = old_dpi;
		}

		DrawStringCentered(74, 15 + y_offset, STR_3002_ORIENTATION, TC_FROMSTRING);
		DrawStringCentered(74, 76 + y_offset, STR_3003_NUMBER_OF_TRACKS, TC_FROMSTRING);
		DrawStringCentered(74, 101 + y_offset, STR_3004_PLATFORM_LENGTH, TC_FROMSTRING);
		DrawStringCentered(74, 141 + y_offset, STR_3066_COVERAGE_AREA_HIGHLIGHT, TC_FROMSTRING);

		int text_end = DrawStationCoverageAreaText(2, 166 + y_offset, SCT_ALL, rad, false);
		text_end = DrawStationCoverageAreaText(2, text_end + 4, SCT_ALL, rad, true) + 4;
		if (text_end != this->widget[BRSW_BACKGROUND].bottom) {
			this->SetDirty();
			ResizeWindowForWidget(this, BRSW_BACKGROUND, 0, text_end - this->widget[BRSW_BACKGROUND].bottom);
			this->SetDirty();
		}

		if (newstations) {
			uint y = 35;

			for (uint16 i = this->vscroll.pos; i < _railstation.station_count && i < (uint)(this->vscroll.pos + this->vscroll.cap); i++) {
				const StationSpec *statspec = GetCustomStationSpec(_railstation.station_class, i);

				if (statspec != NULL && statspec->name != 0) {
					if (HasBit(statspec->callbackmask, CBM_STATION_AVAIL) && GB(GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE), 0, 8) == 0) {
						GfxFillRect(8, y - 2, 127, y + 10, (1 << PALETTE_MODIFIER_GREYOUT));
					}

					DrawStringTruncated(9, y, statspec->name, i == _railstation.station_type ? TC_WHITE : TC_BLACK, 118);
				} else {
					DrawStringTruncated(9, y, STR_STAT_CLASS_DFLT, i == _railstation.station_type ? TC_WHITE : TC_BLACK, 118);
				}

				y += 14;
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BRSW_PLATFORM_DIR_X:
			case BRSW_PLATFORM_DIR_Y:
				this->RaiseWidget(_railstation.orientation + BRSW_PLATFORM_DIR_X);
				_railstation.orientation = widget - BRSW_PLATFORM_DIR_X;
				this->LowerWidget(_railstation.orientation + BRSW_PLATFORM_DIR_X);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			case BRSW_PLATFORM_NUM_1:
			case BRSW_PLATFORM_NUM_2:
			case BRSW_PLATFORM_NUM_3:
			case BRSW_PLATFORM_NUM_4:
			case BRSW_PLATFORM_NUM_5:
			case BRSW_PLATFORM_NUM_6:
			case BRSW_PLATFORM_NUM_7: {
				this->RaiseWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
				this->RaiseWidget(BRSW_PLATFORM_DRAG_N_DROP);

				_railstation.numtracks = widget - BRSW_PLATFORM_NUM_BEGIN;
				_railstation.dragdrop = false;

				const StationSpec *statspec = _railstation.newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;
				if (statspec != NULL && HasBit(statspec->disallowed_lengths, _railstation.platlength - 1)) {
					/* The previously selected number of platforms in invalid */
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_lengths, i)) {
							this->RaiseWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
							_railstation.platlength = i + 1;
							break;
						}
					}
				}

				this->LowerWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
				this->LowerWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
			}

			case BRSW_PLATFORM_LEN_1:
			case BRSW_PLATFORM_LEN_2:
			case BRSW_PLATFORM_LEN_3:
			case BRSW_PLATFORM_LEN_4:
			case BRSW_PLATFORM_LEN_5:
			case BRSW_PLATFORM_LEN_6:
			case BRSW_PLATFORM_LEN_7: {
				this->RaiseWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
				this->RaiseWidget(BRSW_PLATFORM_DRAG_N_DROP);

				_railstation.platlength = widget - BRSW_PLATFORM_LEN_BEGIN;
				_railstation.dragdrop = false;

				const StationSpec *statspec = _railstation.newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;
				if (statspec != NULL && HasBit(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
					/* The previously selected number of tracks in invalid */
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_platforms, i)) {
							this->RaiseWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
							_railstation.numtracks = i + 1;
							break;
						}
					}
				}

				this->LowerWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
				this->LowerWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
			}

			case BRSW_PLATFORM_DRAG_N_DROP: {
				_railstation.dragdrop ^= true;
				this->ToggleWidgetLoweredState(BRSW_PLATFORM_DRAG_N_DROP);

				/* get the first allowed length/number of platforms */
				const StationSpec *statspec = _railstation.newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;
				if (statspec != NULL && HasBit(statspec->disallowed_lengths, _railstation.platlength - 1)) {
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_lengths, i)) {
							this->RaiseWidget(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN);
							_railstation.platlength = i + 1;
							break;
						}
					}
				}
				if (statspec != NULL && HasBit(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
					for (uint i = 0; i < 7; i++) {
						if (!HasBit(statspec->disallowed_platforms, i)) {
							this->RaiseWidget(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN);
							_railstation.numtracks = i + 1;
							break;
						}
					}
				}

				this->SetWidgetLoweredState(_railstation.numtracks + BRSW_PLATFORM_NUM_BEGIN, !_railstation.dragdrop);
				this->SetWidgetLoweredState(_railstation.platlength + BRSW_PLATFORM_LEN_BEGIN, !_railstation.dragdrop);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
			} break;

			case BRSW_HIGHLIGHT_OFF:
			case BRSW_HIGHLIGHT_ON:
				_station_show_coverage = (widget != BRSW_HIGHLIGHT_OFF);
				this->SetWidgetLoweredState(BRSW_HIGHLIGHT_OFF, !_station_show_coverage);
				this->SetWidgetLoweredState(BRSW_HIGHLIGHT_ON, _station_show_coverage);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;

			case BRSW_NEWST_DROPDOWN:
				ShowDropDownList(this, BuildStationClassDropDown(), _railstation.station_class, BRSW_NEWST_DROPDOWN);
				break;

			case BRSW_NEWST_LIST: {
				const StationSpec *statspec;
				int y = (pt.y - 32) / 14;

				if (y >= this->vscroll.cap) return;
				y += this->vscroll.pos;
				if (y >= _railstation.station_count) return;

				/* Check station availability callback */
				statspec = GetCustomStationSpec(_railstation.station_class, y);
				if (statspec != NULL &&
					HasBit(statspec->callbackmask, CBM_STATION_AVAIL) &&
					GB(GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE), 0, 8) == 0) return;

				_railstation.station_type = y;

				this->CheckSelectedSize(statspec);

				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
			}
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (_railstation.station_class != index) {
			_railstation.station_class = (StationClassID)index;
			_railstation.station_type  = 0;
			_railstation.station_count = GetNumCustomStations(_railstation.station_class);

			this->CheckSelectedSize(GetCustomStationSpec(_railstation.station_class, _railstation.station_type));

			this->vscroll.count = _railstation.station_count;
			this->vscroll.pos   = _railstation.station_type;
		}

		SndPlayFx(SND_15_BEEP);
		this->SetDirty();
	}

	virtual void OnTick()
	{
		CheckRedrawStationCoverage(this);
	}
};

/** Widget definition of the standard build rail station window */
static const Widget _station_builder_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},               // BRSW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3000_RAIL_STATION_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},     // BRSW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,   199, 0x0,                             STR_NULL},                            // BRSW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,     7,    72,    26,    73, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},    // BRSW_PLATFORM_DIR_X
{      WWT_PANEL,   RESIZE_NONE,    14,    75,   140,    26,    73, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},    // BRSW_PLATFORM_DIR_Y

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,    87,    98, STR_00CB_1,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_1
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,    87,    98, STR_00CC_2,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_2
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,    87,    98, STR_00CD_3,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_3
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,    87,    98, STR_00CE_4,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_4
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,    87,    98, STR_00CF_5,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_5
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,    87,    98, STR_6,                           STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_6
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,    87,    98, STR_7,                           STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_7

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,   112,   123, STR_00CB_1,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_1
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,   112,   123, STR_00CC_2,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_2
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,   112,   123, STR_00CD_3,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_3
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,   112,   123, STR_00CE_4,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_4
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,   112,   123, STR_00CF_5,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_5
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,   112,   123, STR_6,                           STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_6
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,   112,   123, STR_7,                           STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_7

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,   111,   126,   137, STR_DRAG_DROP,                   STR_STATION_DRAG_DROP},               // BRSW_PLATFORM_DRAG_N_DROP
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    73,   152,   163, STR_02DB_OFF,                    STR_3065_DON_T_HIGHLIGHT_COVERAGE},   // BRSW_HIGHLIGHT_OFF
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    74,   133,   152,   163, STR_02DA_ON,                     STR_3064_HIGHLIGHT_COVERAGE_AREA},    // BRSW_HIGHLIGHT_ON
{   WIDGETS_END},
};

/** Widget definition of the build NewGRF rail station window */
static const Widget _newstation_builder_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},               // BRSW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3000_RAIL_STATION_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},     // BRSW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,   289, 0x0,                             STR_NULL},                            // BRSW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,     7,    72,   116,   163, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},    // BRSW_PLATFORM_DIR_X
{      WWT_PANEL,   RESIZE_NONE,    14,    75,   140,   116,   163, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},    // BRSW_PLATFORM_DIR_Y

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,   177,   188, STR_00CB_1,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_1
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,   177,   188, STR_00CC_2,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_2
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,   177,   188, STR_00CD_3,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_3
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,   177,   188, STR_00CE_4,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_4
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,   177,   188, STR_00CF_5,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_5
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,   177,   188, STR_6,                           STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_6
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,   177,   188, STR_7,                           STR_304F_SELECT_NUMBER_OF_PLATFORMS}, // BRSW_PLATFORM_NUM_7

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,   202,   213, STR_00CB_1,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_1
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,   202,   213, STR_00CC_2,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_2
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,   202,   213, STR_00CD_3,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_3
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,   202,   213, STR_00CE_4,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_4
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,   202,   213, STR_00CF_5,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_5
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,   202,   213, STR_6,                           STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_6
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,   202,   213, STR_7,                           STR_3050_SELECT_LENGTH_OF_RAILROAD},  // BRSW_PLATFORM_LEN_7

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,   111,   216,   227, STR_DRAG_DROP,                   STR_STATION_DRAG_DROP},               // BRSW_PLATFORM_DRAG_N_DROP
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    73,   242,   253, STR_02DB_OFF,                    STR_3065_DON_T_HIGHLIGHT_COVERAGE},   // BRSW_HIGHLIGHT_OFF
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    74,   133,   242,   253, STR_02DA_ON,                     STR_3064_HIGHLIGHT_COVERAGE_AREA},    // BRSW_HIGHLIGHT_ON

/* newstations gui additions */
{ WWT_DROPDOWNIN,   RESIZE_NONE,    14,     7,   140,    17,    28, STR_02BD,                        STR_SELECT_STATION_CLASS_TIP},        // BRSW_NEWST_DROPDOWN
{     WWT_MATRIX,   RESIZE_NONE,    14,     7,   128,    32,   102, 0x501,                           STR_SELECT_STATION_TYPE_TIP},         // BRSW_NEWST_LIST
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   129,   140,    32,   102, 0x0,                             STR_0190_SCROLL_BAR_SCROLLS_LIST},    // BRSW_NEWST_SCROLL
{   WIDGETS_END},
};

/** High level window description of the default station-build window */
static const WindowDesc _station_builder_desc = {
	WDP_AUTO, WDP_AUTO, 148, 200, 148, 200,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_station_builder_widgets,
};

/** High level window description of the newGRF station-build window */
static const WindowDesc _newstation_builder_desc = {
	WDP_AUTO, WDP_AUTO, 148, 290, 148, 290,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_newstation_builder_widgets,
};

/** Open station build window */
static void ShowStationBuilder(Window *parent)
{
	if (GetNumStationClasses() <= 2 && GetNumCustomStations(STAT_CLASS_DFLT) == 1) {
		new BuildRailStationWindow(&_station_builder_desc, parent, false);
	} else {
		new BuildRailStationWindow(&_newstation_builder_desc, parent, true);
	}
}

/** Enum referring to the widgets of the signal window */
enum BuildSignalWidgets {
	BSW_CLOSEBOX = 0,
	BSW_CAPTION,
	BSW_SEMAPHORE_NORM,
	BSW_SEMAPHORE_ENTRY,
	BSW_SEMAPHORE_EXIT,
	BSW_SEMAPHORE_COMBO,
	BSW_ELECTRIC_NORM,
	BSW_ELECTRIC_ENTRY,
	BSW_ELECTRIC_EXIT,
	BSW_ELECTRIC_COMBO,
	BSW_CONVERT,
	BSW_DRAG_SIGNALS_DENSITY,
	BSW_DRAG_SIGNALS_DENSITY_DECREASE,
	BSW_DRAG_SIGNALS_DENSITY_INCREASE,
};

struct BuildSignalWindow : public PickerWindowBase {
private:
	/**
	 * Draw dynamic a signal-sprite in a button in the signal GUI
	 * Draw the sprite +1px to the right and down if the button is lowered and change the sprite to sprite + 1 (red to green light)
	 *
	 * @param widget_index index of this widget in the window
	 * @param image        the sprite to draw
	 * @param xrel         the relativ x value of the sprite in the grf
	 * @param xsize        the width of the sprite
	 */
	void DrawSignalSprite(byte widget_index, SpriteID image, int8 xrel, uint8 xsize)
	{
		DrawSprite(image + this->IsWidgetLowered(widget_index), PAL_NONE,
				this->widget[widget_index].left + (this->widget[widget_index].right - this->widget[widget_index].left) / 2 - xrel - xsize / 2 +
				this->IsWidgetLowered(widget_index), this->widget[widget_index].bottom - 3 + this->IsWidgetLowered(widget_index));
	}

public:
	BuildSignalWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->FindWindowPlacementAndResize(desc);
	};

	virtual void OnPaint()
	{
		this->LowerWidget((_cur_signal_variant == SIG_ELECTRIC ? BSW_ELECTRIC_NORM : BSW_SEMAPHORE_NORM) + _cur_signal_type);

		this->SetWidgetLoweredState(BSW_CONVERT, _convert_signal_button);

		this->SetWidgetDisabledState(BSW_DRAG_SIGNALS_DENSITY_DECREASE, _settings.gui.drag_signals_density == 1);
		this->SetWidgetDisabledState(BSW_DRAG_SIGNALS_DENSITY_INCREASE, _settings.gui.drag_signals_density == 20);

		this->DrawWidgets();

		/* The 'hardcoded' off sets are needed because they are reused sprites. */
		this->DrawSignalSprite(BSW_SEMAPHORE_NORM,  SPR_IMG_SIGNAL_SEMAPHORE_NORM,   0, 12); // xsize of sprite + 1 ==  9
		this->DrawSignalSprite(BSW_SEMAPHORE_ENTRY, SPR_IMG_SIGNAL_SEMAPHORE_ENTRY, -1, 13); // xsize of sprite + 1 == 10
		this->DrawSignalSprite(BSW_SEMAPHORE_EXIT,  SPR_IMG_SIGNAL_SEMAPHORE_EXIT,   0, 12); // xsize of sprite + 1 ==  9
		this->DrawSignalSprite(BSW_SEMAPHORE_COMBO, SPR_IMG_SIGNAL_SEMAPHORE_COMBO,  0, 12); // xsize of sprite + 1 ==  9
		this->DrawSignalSprite(BSW_ELECTRIC_NORM,   SPR_IMG_SIGNAL_ELECTRIC_NORM,   -1,  4);
		this->DrawSignalSprite(BSW_ELECTRIC_ENTRY,  SPR_IMG_SIGNAL_ELECTRIC_ENTRY,  -2,  6);
		this->DrawSignalSprite(BSW_ELECTRIC_EXIT,   SPR_IMG_SIGNAL_ELECTRIC_EXIT,   -2,  6);
		this->DrawSignalSprite(BSW_ELECTRIC_COMBO,  SPR_IMG_SIGNAL_ELECTRIC_COMBO,  -2,  6);

		/* Draw dragging signal density value in the BSW_DRAG_SIGNALS_DENSITY widget */
		SetDParam(0, _settings.gui.drag_signals_density);
		DrawStringCentered(this->widget[BSW_DRAG_SIGNALS_DENSITY].left + (this->widget[BSW_DRAG_SIGNALS_DENSITY].right -
				this->widget[BSW_DRAG_SIGNALS_DENSITY].left) / 2 + 1,
				this->widget[BSW_DRAG_SIGNALS_DENSITY].top + 2, STR_JUST_INT, TC_ORANGE);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BSW_SEMAPHORE_NORM:
			case BSW_SEMAPHORE_ENTRY:
			case BSW_SEMAPHORE_EXIT:
			case BSW_SEMAPHORE_COMBO:
			case BSW_ELECTRIC_NORM:
			case BSW_ELECTRIC_ENTRY:
			case BSW_ELECTRIC_EXIT:
			case BSW_ELECTRIC_COMBO:
				this->RaiseWidget((_cur_signal_variant == SIG_ELECTRIC ? BSW_ELECTRIC_NORM : BSW_SEMAPHORE_NORM) + _cur_signal_type);

				_cur_signal_type = (SignalType)((uint)((widget - BSW_SEMAPHORE_NORM) % (SIGTYPE_COMBO + 1)));
				_cur_signal_variant = widget >= BSW_ELECTRIC_NORM ? SIG_ELECTRIC : SIG_SEMAPHORE;
				break;

			case BSW_CONVERT:
				_convert_signal_button = !_convert_signal_button;
				break;

			case BSW_DRAG_SIGNALS_DENSITY_DECREASE:
				if (_settings.gui.drag_signals_density > 1) {
					_settings.gui.drag_signals_density--;
					SetWindowDirty(FindWindowById(WC_GAME_OPTIONS, 0));
				}
				break;

			case BSW_DRAG_SIGNALS_DENSITY_INCREASE:
				if (_settings.gui.drag_signals_density < 20) {
					_settings.gui.drag_signals_density++;
					SetWindowDirty(FindWindowById(WC_GAME_OPTIONS, 0));
				}
				break;

			default: break;
		}

		this->SetDirty();
	}
};

/** Widget definition of the build signal window */
static const Widget _signal_builder_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  7,   0,  10,   0,  13, STR_00C5,               STR_018B_CLOSE_WINDOW},                 // BSW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  7,  11, 109,   0,  13, STR_SIGNAL_SELECTION,   STR_018C_WINDOW_TITLE_DRAG_THIS},       // BSW_CAPTION

{      WWT_PANEL,   RESIZE_NONE,  7,   0,  21,  14,  40, STR_NULL,               STR_BUILD_SIGNAL_SEMAPHORE_NORM_TIP},   // BSW_SEMAPHORE_NORM
{      WWT_PANEL,   RESIZE_NONE,  7,  22,  43,  14,  40, STR_NULL,               STR_BUILD_SIGNAL_SEMAPHORE_ENTRY_TIP},  // BSW_SEMAPHORE_ENTRY
{      WWT_PANEL,   RESIZE_NONE,  7,  44,  65,  14,  40, STR_NULL,               STR_BUILD_SIGNAL_SEMAPHORE_EXIT_TIP},   // BSW_SEMAPHORE_EXIT
{      WWT_PANEL,   RESIZE_NONE,  7,  66,  87,  14,  40, STR_NULL,               STR_BUILD_SIGNAL_SEMAPHORE_COMBO_TIP},  // BSW_SEMAPHORE_COMBO

{      WWT_PANEL,   RESIZE_NONE,  7,   0,  21,  41,  67, STR_NULL,               STR_BUILD_SIGNAL_ELECTRIC_NORM_TIP},    // BSW_ELECTRIC_NORM
{      WWT_PANEL,   RESIZE_NONE,  7,  22,  43,  41,  67, STR_NULL,               STR_BUILD_SIGNAL_ELECTRIC_ENTRY_TIP},   // BSW_ELECTRIC_ENTRY
{      WWT_PANEL,   RESIZE_NONE,  7,  44,  65,  41,  67, STR_NULL,               STR_BUILD_SIGNAL_ELECTRIC_EXIT_TIP},    // BSW_ELECTRIC_EXIT
{      WWT_PANEL,   RESIZE_NONE,  7,  66,  87,  41,  67, STR_NULL,               STR_BUILD_SIGNAL_ELECTRIC_COMBO_TIP},   // BSW_ELECTRIC_COMBO

{     WWT_IMGBTN,   RESIZE_NONE,  7,  88, 109,  14,  40, SPR_IMG_SIGNAL_CONVERT, STR_SIGNAL_CONVERT_TIP},                // BSW_CONVERT
{      WWT_PANEL,   RESIZE_NONE,  7,  88, 109,  41,  67, STR_NULL,               STR_DRAG_SIGNALS_DENSITY_TIP},          // BSW_DRAG_SIGNALS_DENSITY
{ WWT_PUSHIMGBTN,   RESIZE_NONE, 14,  90,  98,  54,  65, SPR_ARROW_LEFT,         STR_DRAG_SIGNALS_DENSITY_DECREASE_TIP}, // BSW_DRAG_SIGNALS_DENSITY_DECREASE
{ WWT_PUSHIMGBTN,   RESIZE_NONE, 14,  99, 107,  54,  65, SPR_ARROW_RIGHT,        STR_DRAG_SIGNALS_DENSITY_INCREASE_TIP}, // BSW_DRAG_SIGNALS_DENSITY_INCREASE

{   WIDGETS_END},
};

/** Signal selection window description */
static const WindowDesc _signal_builder_desc = {
	WDP_AUTO, WDP_AUTO, 110, 68, 110, 68,
	WC_BUILD_SIGNAL, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_signal_builder_widgets,
};

/**
 * Open the signal selection window
 */
static void ShowSignalBuilder(Window *parent)
{
	new BuildSignalWindow(&_signal_builder_desc, parent);
}

struct BuildRailDepotWindow : public PickerWindowBase {
private:
	/** Enum referring to the widgets of the build rail depot window */
	enum BuildRailDepotWidgets {
		BRDW_CLOSEBOX = 0,
		BRDW_CAPTION,
		BRDW_BACKGROUND,
		BRDW_DEPOT_NE,
		BRDW_DEPOT_SE,
		BRDW_DEPOT_SW,
		BRDW_DEPOT_NW,
	};

public:
	BuildRailDepotWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->LowerWidget(_build_depot_direction + BRDW_DEPOT_NE);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		DrawTrainDepotSprite(70, 17, DIAGDIR_NE, _cur_railtype);
		DrawTrainDepotSprite(70, 69, DIAGDIR_SE, _cur_railtype);
		DrawTrainDepotSprite( 2, 69, DIAGDIR_SW, _cur_railtype);
		DrawTrainDepotSprite( 2, 17, DIAGDIR_NW, _cur_railtype);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BRDW_DEPOT_NE:
			case BRDW_DEPOT_SE:
			case BRDW_DEPOT_SW:
			case BRDW_DEPOT_NW:
				this->RaiseWidget(_build_depot_direction + BRDW_DEPOT_NE);
				_build_depot_direction = (DiagDirection)(widget - BRDW_DEPOT_NE);
				this->LowerWidget(_build_depot_direction + BRDW_DEPOT_NE);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
		}
	}
};

/** Widget definition of the build rail depot window */
static const Widget _build_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},                     // BRDW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_1014_TRAIN_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},           // BRDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   121, 0x0,                              STR_NULL},                                  // BRDW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO}, // BRDW_DEPOT_NE
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO}, // BRDW_DEPOT_SE
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO}, // BRDW_DEPOT_SW
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO}, // BRDW_DEPOT_NW
{   WIDGETS_END},
};

static const WindowDesc _build_depot_desc = {
	WDP_AUTO, WDP_AUTO, 140, 122, 140, 122,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_depot_widgets,
};

static void ShowBuildTrainDepotPicker(Window *parent)
{
	new BuildRailDepotWindow(&_build_depot_desc, parent);
}

struct BuildRailWaypointWindow : PickerWindowBase {
private:
	/** Enum referring to the widgets of the build NewGRF rail waypoint window */
	enum BuildRailWaypointWidgets {
		BRWW_CLOSEBOX = 0,
		BRWW_CAPTION,
		BRWW_BACKGROUND,
		BRWW_WAYPOINT_1,
		BRWW_WAYPOINT_2,
		BRWW_WAYPOINT_3,
		BRWW_WAYPOINT_4,
		BRWW_WAYPOINT_5,
		BRWW_SCROLL,
	};

public:
	BuildRailWaypointWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->hscroll.cap = 5;
		this->hscroll.count = _waypoint_count;
		this->FindWindowPlacementAndResize(desc);
	};

	virtual void OnPaint()
	{
		uint i;

		for (i = 0; i < this->hscroll.cap; i++) {
			this->SetWidgetLoweredState(i + BRWW_WAYPOINT_1, (this->hscroll.pos + i) == _cur_waypoint_type);
		}

		this->DrawWidgets();

		for (i = 0; i < this->hscroll.cap; i++) {
			if (this->hscroll.pos + i < this->hscroll.count) {
				const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, this->hscroll.pos + i);

				DrawWaypointSprite(2 + i * 68, 25, this->hscroll.pos + i, _cur_railtype);

				if (statspec != NULL &&
						HasBit(statspec->callbackmask, CBM_STATION_AVAIL) &&
						GB(GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE), 0, 8) == 0) {
					GfxFillRect(4 + i * 68, 18, 67 + i * 68, 75, (1 << PALETTE_MODIFIER_GREYOUT));
				}
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BRWW_WAYPOINT_1:
			case BRWW_WAYPOINT_2:
			case BRWW_WAYPOINT_3:
			case BRWW_WAYPOINT_4:
			case BRWW_WAYPOINT_5: {
				byte type = widget - BRWW_WAYPOINT_1 + this->hscroll.pos;

				/* Check station availability callback */
				const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, type);
				if (statspec != NULL &&
						HasBit(statspec->callbackmask, CBM_STATION_AVAIL) &&
						GB(GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE), 0, 8) == 0) return;

				_cur_waypoint_type = type;
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
			}
		}
	}
};

/** Widget definition for the build NewGRF rail waypoint window */
static const Widget _build_waypoint_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,     STR_018B_CLOSE_WINDOW},            // BRWW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   343,     0,    13, STR_WAYPOINT, STR_018C_WINDOW_TITLE_DRAG_THIS},  // BRWW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   343,    14,    91, 0x0,          STR_NULL},                         // BRWW_BACKGROUND

{      WWT_PANEL,   RESIZE_NONE,     7,     3,    68,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},        // BRWW_WAYPOINT_1
{      WWT_PANEL,   RESIZE_NONE,     7,    71,   136,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},        // BRWW_WAYPOINT_2
{      WWT_PANEL,   RESIZE_NONE,     7,   139,   204,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},        // BRWW_WAYPOINT_3
{      WWT_PANEL,   RESIZE_NONE,     7,   207,   272,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},        // BRWW_WAYPOINT_4
{      WWT_PANEL,   RESIZE_NONE,     7,   275,   340,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},        // BRWW_WAYPOINT_5

{ WWT_HSCROLLBAR,   RESIZE_NONE,    7,     1,   343,     80,    91, 0x0,          STR_0190_SCROLL_BAR_SCROLLS_LIST}, // BRWW_SCROLL
{    WIDGETS_END},
};

static const WindowDesc _build_waypoint_desc = {
	WDP_AUTO, WDP_AUTO, 344, 92, 344, 92,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_waypoint_widgets,
};

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
	_railstation.numtracks = 1;
	_railstation.platlength = 1;
	_railstation.dragdrop = true;
}

/**
 * Re-initialize rail-build toolbar after toggling support for electric trains
 * @param disable Boolean whether electric trains are disabled (removed from the game)
 */
void ReinitGuiAfterToggleElrail(bool disable)
{
	extern RailType _last_built_railtype;
	if (disable && _last_built_railtype == RAILTYPE_ELECTRIC) {
		Window *w;
		_last_built_railtype = _cur_railtype = RAILTYPE_RAIL;
		w = FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
		if (w != NULL) {
			SetupRailToolbar(_cur_railtype, w);
			w->SetDirty();
		}
	}
	MarkWholeScreenDirty();
}

/** Set the initial (default) railtype to use */
static void SetDefaultRailGui()
{
	if (_local_player == PLAYER_SPECTATOR || !IsValidPlayer(_local_player)) return;

	extern RailType _last_built_railtype;
	RailType rt = (RailType)_settings.gui.default_rail_type;
	if (rt >= RAILTYPE_END) {
		if (rt == RAILTYPE_END + 2) {
			/* Find the most used rail type */
			RailType count[RAILTYPE_END];
			memset(count, 0, sizeof(count));
			for (TileIndex t = 0; t < MapSize(); t++) {
				if (IsTileType(t, MP_RAILWAY) || IsLevelCrossingTile(t) || IsRailwayStationTile(t) ||
						(IsTileType(t, MP_TUNNELBRIDGE) && GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL)) {
					count[GetRailType(t)]++;
				}
			}

			rt = RAILTYPE_RAIL;
			for (RailType r = RAILTYPE_ELECTRIC; r < RAILTYPE_END; r++) {
				if (count[r] >= count[rt]) rt = r;
			}

			/* No rail, just get the first available one */
			if (count[rt] == 0) rt = RAILTYPE_END;
		}
		switch (rt) {
			case RAILTYPE_END + 0:
				rt = RAILTYPE_RAIL;
				while (rt < RAILTYPE_END && !HasRailtypeAvail(_local_player, rt)) rt++;
				break;

			case RAILTYPE_END + 1:
				rt = GetBestRailtype(_local_player);
				break;

			default:
				break;
		}
	}

	_last_built_railtype = _cur_railtype = rt;
	Window *w = FindWindowById(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
	if (w != NULL) {
		SetupRailToolbar(_cur_railtype, w);
		w->SetDirty();
	}
}

/**
 * Updates the current signal variant used in the signal GUI
 * to the one adequate to current year.
 * @param 0 needed to be called when a patch setting changes
 * @return success, needed for patch settings
 */
int32 ResetSignalVariant(int32 = 0)
{
	SignalVariant new_variant = (_cur_year < _settings.gui.semaphore_build_before ? SIG_SEMAPHORE : SIG_ELECTRIC);

	if (new_variant != _cur_signal_variant) {
		Window *w = FindWindowById(WC_BUILD_SIGNAL, 0);
		if (w != NULL) {
			w->SetDirty();
			w->RaiseWidget((_cur_signal_variant == SIG_ELECTRIC ? BSW_ELECTRIC_NORM : BSW_SEMAPHORE_NORM) + _cur_signal_type);
		}
		_cur_signal_variant = new_variant;
	}

	return 0;
}

/** Resets the rail GUI - sets default railtype to build
 * and resets the signal GUI
 */
void InitializeRailGUI()
{
	SetDefaultRailGui();

	_convert_signal_button = false;
	_cur_signal_type = SIGTYPE_NORMAL;
	ResetSignalVariant();
}
