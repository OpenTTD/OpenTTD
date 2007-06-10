/* $Id$ */

/** @file rail_gui.cpp File for dealing with rail construction user interface */

#include "stdafx.h"
#include "openttd.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "landscape.h"
#include "date.h"
#include "map.h"
#include "tile.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "sound.h"
#include "command.h"
#include "vehicle.h"
#include "station.h"
#include "waypoint.h"
#include "debug.h"
#include "variables.h"
#include "newgrf_callbacks.h"
#include "newgrf_station.h"
#include "train.h"

static RailType _cur_railtype;
static bool _remove_button_clicked;
static DiagDirection _build_depot_direction;
static byte _waypoint_count = 1;
static byte _cur_waypoint_type;

static struct {
	byte orientation;
	byte numtracks;
	byte platlength;
	bool dragdrop;

	bool newstations;
	StationClassIDByte station_class;
	byte station_type;
	byte station_count;
} _railstation;


static void HandleStationPlacement(TileIndex start, TileIndex end);
static void ShowBuildTrainDepotPicker();
static void ShowBuildWaypointPicker();
static void ShowStationBuilder();

void CcPlaySound1E(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_20_SPLAT_2, tile);
}

static void GenericPlaceRail(TileIndex tile, int cmd)
{
	DoCommandP(tile, _cur_railtype, cmd, CcPlaySound1E,
		_remove_button_clicked ?
		CMD_REMOVE_SINGLE_RAIL | CMD_MSG(STR_1012_CAN_T_REMOVE_RAILROAD_TRACK) | CMD_AUTO | CMD_NO_WATER :
		CMD_BUILD_SINGLE_RAIL | CMD_MSG(STR_1011_CAN_T_BUILD_RAILROAD_TRACK) | CMD_AUTO | CMD_NO_WATER
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

static void PlaceExtraDepotRail(TileIndex tile, uint16 extra)
{
	if (GetRailTileType(tile) != RAIL_TILE_NORMAL) return;
	if ((GetTrackBits(tile) & GB(extra, 8, 8)) == 0) return;

	DoCommandP(tile, _cur_railtype, extra & 0xFF, NULL, CMD_BUILD_SINGLE_RAIL | CMD_AUTO | CMD_NO_WATER);
}

static const uint16 _place_depot_extra[12] = {
	0x0604, 0x2102, 0x1202, 0x0505,
	0x2400, 0x2801, 0x1800, 0x1401,
	0x2203, 0x0904, 0x0A05, 0x1103,
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
		CMD_BUILD_TRAIN_DEPOT | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_100E_CAN_T_BUILD_TRAIN_DEPOT));
}

static void PlaceRail_Waypoint(TileIndex tile)
{
	if (!_remove_button_clicked) {
		DoCommandP(tile, _cur_waypoint_type, 0, CcPlaySound1E, CMD_BUILD_TRAIN_WAYPOINT | CMD_MSG(STR_CANT_BUILD_TRAIN_WAYPOINT));
	} else {
		DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_REMOVE_TRAIN_WAYPOINT | CMD_MSG(STR_CANT_REMOVE_TRAIN_WAYPOINT));
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
		VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_REMOVE_STATION);
	} else if (_railstation.dragdrop) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED, DDSP_BUILD_STATION);
		VpSetPlaceSizingLimit(_patches.station_spread);
	} else {
		DoCommandP(tile,
				_railstation.orientation | (_railstation.numtracks << 8) | (_railstation.platlength << 16) | (_ctrl_pressed << 24),
				_cur_railtype | (_railstation.station_class << 8) | (_railstation.station_type << 16), CcStation,
				CMD_BUILD_RAILROAD_STATION | CMD_NO_WATER | CMD_AUTO | CMD_MSG(STR_100F_CAN_T_BUILD_RAILROAD_STATION));
	}
}

static void GenericPlaceSignals(TileIndex tile)
{
	TrackBits trackbits = (TrackBits)GB(GetTileTrackStatus(tile, TRANSPORT_RAIL, 0), 0, 6);

	if (trackbits & TRACK_BIT_VERT) { // N-S direction
		trackbits = (_tile_fract_coords.x <= _tile_fract_coords.y) ? TRACK_BIT_RIGHT : TRACK_BIT_LEFT;
	}

	if (trackbits & TRACK_BIT_HORZ) { // E-W direction
		trackbits = (_tile_fract_coords.x + _tile_fract_coords.y <= 15) ? TRACK_BIT_UPPER : TRACK_BIT_LOWER;
	}

	Track track = TrackBitsToTrack(trackbits);

	if (!_remove_button_clicked) {
		uint32 p1 = track;
		SB(p1, 3, 1, _ctrl_pressed);
		SB(p1, 4, 1, _cur_year < _patches.semaphore_build_before);

		DoCommandP(tile, p1, 0, CcPlaySound1E,
			CMD_BUILD_SIGNALS | CMD_AUTO | CMD_MSG(STR_1010_CAN_T_BUILD_SIGNALS_HERE));
	} else {
		DoCommandP(tile, track, 0, CcPlaySound1E,
			CMD_REMOVE_SIGNALS | CMD_AUTO | CMD_MSG(STR_1013_CAN_T_REMOVE_SIGNALS_FROM));
	}
}

static void PlaceRail_Bridge(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
}

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
		CMD_BUILD_TUNNEL | CMD_AUTO | CMD_MSG(STR_5016_CAN_T_BUILD_TUNNEL_HERE));
}

void PlaceProc_BuyLand(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_PURCHASE_LAND_AREA | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_5806_CAN_T_PURCHASE_THIS_LAND));
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
enum {
	RTW_CAPTION        =  1,
	RTW_BUILD_NS       =  4,
	RTW_BUILD_X        =  5,
	RTW_BUILD_EW       =  6,
	RTW_BUILD_Y        =  7,
	RTW_AUTORAIL       =  8,
	RTW_DEMOLISH       =  9,
	RTW_BUILD_DEPOT    = 10,
	RTW_BUILD_WAYPOINT = 11,
	RTW_BUILD_STATION  = 12,
	RTW_BUILD_SIGNALS  = 13,
	RTW_BUILD_BRIDGE   = 14,
	RTW_BUILD_TUNNEL   = 15,
	RTW_REMOVE         = 16,
	RTW_CONVERT_RAIL   = 17
};


static void BuildRailClick_N(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_NS, GetRailTypeInfo(_cur_railtype)->cursor.rail_ns, 1, PlaceRail_N);
}

static void BuildRailClick_NE(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_X, GetRailTypeInfo(_cur_railtype)->cursor.rail_swne, 1, PlaceRail_NE);
}

static void BuildRailClick_E(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_EW, GetRailTypeInfo(_cur_railtype)->cursor.rail_ew, 1, PlaceRail_E);
}

static void BuildRailClick_NW(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_Y, GetRailTypeInfo(_cur_railtype)->cursor.rail_nwse, 1, PlaceRail_NW);
}

static void BuildRailClick_AutoRail(Window *w)
{
	HandlePlacePushButton(w, RTW_AUTORAIL, GetRailTypeInfo(_cur_railtype)->cursor.autorail, VHM_RAIL, PlaceRail_AutoRail);
}

static void BuildRailClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, RTW_DEMOLISH, ANIMCURSOR_DEMOLISH, 1, PlaceProc_DemolishArea);
}

static void BuildRailClick_Depot(Window *w)
{
	if (HandlePlacePushButton(w, RTW_BUILD_DEPOT, GetRailTypeInfo(_cur_railtype)->cursor.depot, 1, PlaceRail_Depot)) {
		ShowBuildTrainDepotPicker();
	}
}

static void BuildRailClick_Waypoint(Window *w)
{
	_waypoint_count = GetNumCustomStations(STAT_CLASS_WAYP);
	if (HandlePlacePushButton(w, RTW_BUILD_WAYPOINT, SPR_CURSOR_WAYPOINT, 1, PlaceRail_Waypoint) &&
			_waypoint_count > 1) {
		ShowBuildWaypointPicker();
	}
}

static void BuildRailClick_Station(Window *w)
{
	if (HandlePlacePushButton(w, RTW_BUILD_STATION, SPR_CURSOR_RAIL_STATION, 1, PlaceRail_Station)) ShowStationBuilder();
}

static void BuildRailClick_AutoSignals(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_SIGNALS, ANIMCURSOR_BUILDSIGNALS, VHM_RECT, PlaceRail_AutoSignals);
}

static void BuildRailClick_Bridge(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, 1, PlaceRail_Bridge);
}

static void BuildRailClick_Tunnel(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_TUNNEL, GetRailTypeInfo(_cur_railtype)->cursor.tunnel, 3, PlaceRail_Tunnel);
}

static void BuildRailClick_Remove(Window *w)
{
	if (IsWindowWidgetDisabled(w, RTW_REMOVE)) return;
	SetWindowDirty(w);
	SndPlayFx(SND_15_BEEP);

	ToggleWidgetLoweredState(w, RTW_REMOVE);
	_remove_button_clicked = IsWindowWidgetLowered(w, RTW_REMOVE);
	SetSelectionRed(_remove_button_clicked);

	// handle station builder
	if (_remove_button_clicked) {
		SetTileSelectSize(1, 1);
	}
}

static void BuildRailClick_Convert(Window *w)
{
	HandlePlacePushButton(w, RTW_CONVERT_RAIL, GetRailTypeInfo(_cur_railtype)->cursor.convert, 1, PlaceRail_ConvertRail);
}


static void DoRailroadTrack(int mode)
{
	DoCommandP(TileVirtXY(_thd.selstart.x, _thd.selstart.y), TileVirtXY(_thd.selend.x, _thd.selend.y), _cur_railtype | (mode << 4), NULL,
		_remove_button_clicked ?
		CMD_REMOVE_RAILROAD_TRACK | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1012_CAN_T_REMOVE_RAILROAD_TRACK) :
		CMD_BUILD_RAILROAD_TRACK  | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1011_CAN_T_BUILD_RAILROAD_TRACK)
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

static void HandleAutoSignalPlacement()
{
	TileHighlightData *thd = &_thd;
	uint32 p2 = GB(thd->drawstyle, 0, 3); // 0..5

	if (thd->drawstyle == HT_RECT) { // one tile case
		GenericPlaceSignals(TileVirtXY(thd->selend.x, thd->selend.y));
		return;
	}

	SB(p2,  3, 1, _ctrl_pressed);
	SB(p2,  4, 1, _cur_year < _patches.semaphore_build_before);
	SB(p2, 24, 8, _patches.drag_signals_density);

	/* _patches.drag_signals_density is given as a parameter such that each user
	 * in a network game can specify his/her own signal density */
	DoCommandP(
		TileVirtXY(thd->selstart.x, thd->selstart.y),
		TileVirtXY(thd->selend.x, thd->selend.y),
		p2,
		CcPlaySound1E,
		_remove_button_clicked ?
			CMD_REMOVE_SIGNAL_TRACK | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1013_CAN_T_REMOVE_SIGNALS_FROM) :
			CMD_BUILD_SIGNAL_TRACK  | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1010_CAN_T_BUILD_SIGNALS_HERE)
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


static void UpdateRemoveWidgetStatus(Window *w, int clicked_widget)
{
	/* If it is the removal button that has been clicked, do nothing,
	 * as it is up to the other buttons to drive removal status */
	if (clicked_widget == RTW_REMOVE) return;

	switch (clicked_widget) {
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
			SetWindowWidgetDisabledState(w, RTW_REMOVE, !IsWindowWidgetLowered(w, clicked_widget));
			break;

		default:
			/* When any other buttons than rail/signal/waypoint/station, raise and
			 * disable the removal button*/
			DisableWindowWidget(w, RTW_REMOVE);
			RaiseWindowWidget(w, RTW_REMOVE);
			break;
	}
}

static void BuildRailToolbWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: DisableWindowWidget(w, RTW_REMOVE); break;

	case WE_PAINT: DrawWindowWidgets(w); break;

	case WE_CLICK:
		if (e->we.click.widget >= 4) {
			_remove_button_clicked = false;
			_build_railroad_button_proc[e->we.click.widget - 4](w);
		}
		UpdateRemoveWidgetStatus(w, e->we.click.widget);
		break;

	case WE_KEYPRESS: {
		uint i;

		for (i = 0; i != lengthof(_rail_keycodes); i++) {
			if (e->we.keypress.keycode == _rail_keycodes[i]) {
				e->we.keypress.cont = false;
				_remove_button_clicked = false;
				_build_railroad_button_proc[i](w);
				UpdateRemoveWidgetStatus(w, i + 4);
				break;
			}
		}
		MarkTileDirty(_thd.pos.x, _thd.pos.y); // redraw tile selection
		break;
	}

	case WE_PLACE_OBJ:
		_place_proc(e->we.place.tile);
		return;

	case WE_PLACE_DRAG: {
		VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
		return;
	}

	case WE_PLACE_MOUSEUP:
		if (e->we.place.pt.x != -1) {
			TileIndex start_tile = e->we.place.starttile;
			TileIndex end_tile = e->we.place.tile;

			switch (e->we.place.select_proc) {
				case DDSP_BUILD_BRIDGE:
					ResetObjectToPlace();
					ShowBuildBridgeWindow(start_tile, end_tile, _cur_railtype);
					break;

				case DDSP_PLACE_AUTORAIL: {
					bool old = _remove_button_clicked;
					if (_ctrl_pressed) _remove_button_clicked = true;
					HandleAutodirPlacement();
					_remove_button_clicked = old;
					break;
				}

				case DDSP_BUILD_SIGNALS:
					HandleAutoSignalPlacement();
					break;

				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(e);
					break;

				case DDSP_REMOVE_STATION:
					DoCommandP(end_tile, start_tile, 0, CcPlaySound1E, CMD_REMOVE_FROM_RAILROAD_STATION | CMD_MSG(STR_CANT_REMOVE_PART_OF_STATION));
					break;

				case DDSP_CONVERT_RAIL:
					DoCommandP(end_tile, start_tile, _cur_railtype, CcPlaySound10, CMD_CONVERT_RAIL | CMD_MSG(STR_CANT_CONVERT_RAIL));
					break;

				case DDSP_BUILD_STATION:
					HandleStationPlacement(start_tile, end_tile);
					break;

				case DDSP_PLACE_RAIL_NE:
				case DDSP_PLACE_RAIL_NW:
					DoRailroadTrack(e->we.place.select_proc == DDSP_PLACE_RAIL_NE ? TRACK_X : TRACK_Y);
					break;
			}
		}
		break;

	case WE_ABORT_PLACE_OBJ:
		RaiseWindowButtons(w);
		DisableWindowWidget(w, RTW_REMOVE);
		InvalidateWidget(w, RTW_REMOVE);

		w = FindWindowById(WC_BUILD_STATION, 0);
		if (w != NULL) WP(w,def_d).close = true;
		w = FindWindowById(WC_BUILD_DEPOT, 0);
		if (w != NULL) WP(w,def_d).close = true;
		break;

	case WE_PLACE_PRESIZE: {
		TileIndex tile = e->we.place.tile;

		DoCommand(tile, 0, 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
	} break;

	case WE_DESTROY:
		if (_patches.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0);
		break;
	}
}


static const Widget _build_rail_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   337,     0,    13, STR_100A_RAILROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   338,   349,     0,    13, 0x0,                            STR_STICKY_BUTTON},

{      WWT_PANEL,   RESIZE_NONE,     7,   110,   113,    14,    35, 0x0,                            STR_NULL},

{     WWT_IMGBTN,   RESIZE_NONE,     7,    0,     21,    14,    35, SPR_IMG_RAIL_NS,                STR_1018_BUILD_RAILROAD_TRACK},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_RAIL_NE,                STR_1018_BUILD_RAILROAD_TRACK},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    44,    65,    14,    35, SPR_IMG_RAIL_EW,                STR_1018_BUILD_RAILROAD_TRACK},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    66,    87,    14,    35, SPR_IMG_RAIL_NW,                STR_1018_BUILD_RAILROAD_TRACK},
{     WWT_IMGBTN,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_IMG_AUTORAIL,               STR_BUILD_AUTORAIL_TIP},

{     WWT_IMGBTN,   RESIZE_NONE,     7,   114,   135,    14,    35, SPR_IMG_DYNAMITE,               STR_018D_DEMOLISH_BUILDINGS_ETC},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   136,   157,    14,    35, SPR_IMG_DEPOT_RAIL,             STR_1019_BUILD_TRAIN_DEPOT_FOR_BUILDING},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   158,   179,    14,    35, SPR_IMG_WAYPOINT,               STR_CONVERT_RAIL_TO_WAYPOINT_TIP},

{     WWT_IMGBTN,   RESIZE_NONE,     7,   180,   221,    14,    35, SPR_IMG_RAIL_STATION,           STR_101A_BUILD_RAILROAD_STATION},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   222,   243,    14,    35, SPR_IMG_RAIL_SIGNALS,           STR_101B_BUILD_RAILROAD_SIGNALS},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   244,   285,    14,    35, SPR_IMG_BRIDGE,                 STR_101C_BUILD_RAILROAD_BRIDGE},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   286,   305,    14,    35, SPR_IMG_TUNNEL_RAIL,            STR_101D_BUILD_RAILROAD_TUNNEL},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   306,   327,    14,    35, SPR_IMG_REMOVE,                 STR_101E_TOGGLE_BUILD_REMOVE_FOR},
{     WWT_IMGBTN,   RESIZE_NONE,     7,   328,   349,    14,    35, SPR_IMG_CONVERT_RAIL,           STR_CONVERT_RAIL_TIP},

{   WIDGETS_END},
};

static const WindowDesc _build_rail_desc = {
	WDP_ALIGN_TBR, 22, 350, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_rail_widgets,
	BuildRailToolbWndProc
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

void ShowBuildRailToolbar(RailType railtype, int button)
{
	Window *w;

	if (!IsValidPlayer(_current_player)) return;
	if (!ValParamRailtype(railtype)) return;

	// don't recreate the window if we're clicking on a button and the window exists.
	if (button < 0 || !(w = FindWindowById(WC_BUILD_TOOLBAR, 0)) || w->wndproc != BuildRailToolbWndProc) {
		DeleteWindowById(WC_BUILD_TOOLBAR, 0);
		_cur_railtype = railtype;
		w = AllocateWindowDesc(&_build_rail_desc);
		SetupRailToolbar(railtype, w);
	}

	_remove_button_clicked = false;
	if (w != NULL && button >= 0) {
		_build_railroad_button_proc[button](w);
		UpdateRemoveWidgetStatus(w, button + 4);
	}
	if (_patches.link_terraform_toolbar) ShowTerraformToolbar(w);
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
			CMD_BUILD_RAILROAD_STATION | CMD_NO_WATER | CMD_AUTO | CMD_MSG(STR_100F_CAN_T_BUILD_RAILROAD_STATION));
}

/* Check if the currently selected station size is allowed */
static void CheckSelectedSize(Window *w, const StationSpec *statspec)
{
	if (statspec == NULL || _railstation.dragdrop) return;

	if (HASBIT(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
		RaiseWindowWidget(w, _railstation.numtracks + 4);
		_railstation.numtracks = 1;
		while (HASBIT(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
			_railstation.numtracks++;
		}
		LowerWindowWidget(w, _railstation.numtracks + 4);
	}

	if (HASBIT(statspec->disallowed_lengths, _railstation.platlength - 1)) {
		RaiseWindowWidget(w, _railstation.platlength + 11);
		_railstation.platlength = 1;
		while (HASBIT(statspec->disallowed_lengths, _railstation.platlength - 1)) {
			_railstation.platlength++;
		}
		LowerWindowWidget(w, _railstation.platlength + 11);
	}
}

static void StationBuildWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE:
		LowerWindowWidget(w, _railstation.orientation + 3);
		if (_railstation.dragdrop) {
			LowerWindowWidget(w, 19);
		} else {
			LowerWindowWidget(w, _railstation.numtracks + 4);
			LowerWindowWidget(w, _railstation.platlength + 11);
		}
		SetWindowWidgetLoweredState(w, 20, !_station_show_coverage);
		SetWindowWidgetLoweredState(w, 21, _station_show_coverage);
		break;

	case WE_PAINT: {
		int rad;
		uint bits;
		bool newstations = _railstation.newstations;
		int y_offset;
		DrawPixelInfo tmp_dpi, *old_dpi;
		const StationSpec *statspec = newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;

		if (WP(w,def_d).close) return;

		if (_railstation.dragdrop) {
			SetTileSelectSize(1, 1);
		} else {
			int x = _railstation.numtracks;
			int y = _railstation.platlength;
			if (_railstation.orientation == 0) Swap(x, y);
			if (!_remove_button_clicked)
				SetTileSelectSize(x, y);
		}

		rad = (_patches.modified_catchment) ? CA_TRAIN : 4;

		if (_station_show_coverage)
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);

		for (bits = 0; bits < 7; bits++) {
			bool disable = bits >= _patches.station_spread;
			if (statspec == NULL) {
				SetWindowWidgetDisabledState(w, bits +  5, disable);
				SetWindowWidgetDisabledState(w, bits + 12, disable);
			} else {
				SetWindowWidgetDisabledState(w, bits +  5, HASBIT(statspec->disallowed_platforms, bits) || disable);
				SetWindowWidgetDisabledState(w, bits + 12, HASBIT(statspec->disallowed_lengths,   bits) || disable);
			}
		}

		SetDParam(0, GetStationClassName(_railstation.station_class));
		DrawWindowWidgets(w);

		y_offset = newstations ? 90 : 0;

		/* Set up a clipping area for the '/' station preview */
		if (FillDrawPixelInfo(&tmp_dpi, 7, 26 + y_offset, 66, 48)) {
			old_dpi = _cur_dpi;
			_cur_dpi = &tmp_dpi;
			if (!DrawStationTile(32, 16, _cur_railtype, AXIS_X, _railstation.station_class, _railstation.station_type)) {
				StationPickerDrawSprite(32, 16, _cur_railtype, INVALID_ROADTYPE, 2);
			}
			_cur_dpi = old_dpi;
		}

		/* Set up a clipping area for the '\' station preview */
		if (FillDrawPixelInfo(&tmp_dpi, 75, 26 + y_offset, 66, 48)) {
			old_dpi = _cur_dpi;
			_cur_dpi = &tmp_dpi;
			if (!DrawStationTile(32, 16, _cur_railtype, AXIS_Y, _railstation.station_class, _railstation.station_type)) {
				StationPickerDrawSprite(32, 16, _cur_railtype, INVALID_ROADTYPE, 3);
			}
			_cur_dpi = old_dpi;
		}

		DrawStringCentered(74, 15 + y_offset, STR_3002_ORIENTATION, 0);
		DrawStringCentered(74, 76 + y_offset, STR_3003_NUMBER_OF_TRACKS, 0);
		DrawStringCentered(74, 101 + y_offset, STR_3004_PLATFORM_LENGTH, 0);
		DrawStringCentered(74, 141 + y_offset, STR_3066_COVERAGE_AREA_HIGHLIGHT, 0);

		DrawStationCoverageAreaText(2, 166 + y_offset, (uint)-1, rad);

		if (newstations) {
			uint16 i;
			uint y = 35;

			for (i = w->vscroll.pos; i < _railstation.station_count && i < (uint)(w->vscroll.pos + w->vscroll.cap); i++) {
				const StationSpec *statspec = GetCustomStationSpec(_railstation.station_class, i);

				if (statspec != NULL && statspec->name != 0) {
					if (HASBIT(statspec->callbackmask, CBM_STATION_AVAIL) && GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE) == 0) {
						GfxFillRect(8, y - 2, 127, y + 10, (1 << PALETTE_MODIFIER_GREYOUT));
					}

					DrawStringTruncated(9, y, statspec->name, i == _railstation.station_type ? 12 : 16, 118);
				} else {
					DrawStringTruncated(9, y, STR_STAT_CLASS_DFLT, i == _railstation.station_type ? 12 : 16, 118);
				}

				y += 14;
			}
		}
	} break;

	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 3:
		case 4:
			RaiseWindowWidget(w, _railstation.orientation + 3);
			_railstation.orientation = e->we.click.widget - 3;
			LowerWindowWidget(w, _railstation.orientation + 3);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11: {
			RaiseWindowWidget(w, _railstation.numtracks + 4);
			RaiseWindowWidget(w, 19);

			_railstation.numtracks = (e->we.click.widget - 5) + 1;
			_railstation.dragdrop = false;

			const StationSpec *statspec = _railstation.newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;
			if (statspec != NULL && HASBIT(statspec->disallowed_lengths, _railstation.platlength - 1)) {
				/* The previously selected number of platforms in invalid */
				for (uint i = 0; i < 7; i++) {
					if (!HASBIT(statspec->disallowed_lengths, i)) {
						RaiseWindowWidget(w, _railstation.platlength + 11);
						_railstation.platlength = i + 1;
						break;
					}
				}
			}

			LowerWindowWidget(w, _railstation.platlength + 11);
			LowerWindowWidget(w, _railstation.numtracks + 4);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}

		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18: {
			RaiseWindowWidget(w, _railstation.platlength + 11);
			RaiseWindowWidget(w, 19);

			_railstation.platlength = (e->we.click.widget - 12) + 1;
			_railstation.dragdrop = false;

			const StationSpec *statspec = _railstation.newstations ? GetCustomStationSpec(_railstation.station_class, _railstation.station_type) : NULL;
			if (statspec != NULL && HASBIT(statspec->disallowed_platforms, _railstation.numtracks - 1)) {
				/* The previously selected number of tracks in invalid */
				for (uint i = 0; i < 7; i++) {
					if (!HASBIT(statspec->disallowed_platforms, i)) {
						RaiseWindowWidget(w, _railstation.numtracks + 4);
						_railstation.numtracks = i + 1;
						break;
					}
				}
			}

			LowerWindowWidget(w, _railstation.platlength + 11);
			LowerWindowWidget(w, _railstation.numtracks + 4);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}

		case 19:
			_railstation.dragdrop ^= true;
			ToggleWidgetLoweredState(w, 19);
			SetWindowWidgetLoweredState(w, _railstation.numtracks + 4, !_railstation.dragdrop);
			SetWindowWidgetLoweredState(w, _railstation.platlength + 11, !_railstation.dragdrop);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 20:
		case 21:
			_station_show_coverage = (e->we.click.widget != 20);
			SetWindowWidgetLoweredState(w, 20, !_station_show_coverage);
			SetWindowWidgetLoweredState(w, 21, _station_show_coverage);
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 22:
		case 23:
			ShowDropDownMenu(w, BuildStationClassDropdown(), _railstation.station_class, 23, 0, 1 << STAT_CLASS_WAYP);
			break;

		case 24: {
			const StationSpec *statspec;
			int y = (e->we.click.pt.y - 32) / 14;

			if (y >= w->vscroll.cap) return;
			y += w->vscroll.pos;
			if (y >= _railstation.station_count) return;

			/* Check station availability callback */
			statspec = GetCustomStationSpec(_railstation.station_class, y);
			if (statspec != NULL &&
				HASBIT(statspec->callbackmask, CBM_STATION_AVAIL) &&
				GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE) == 0) return;

			_railstation.station_type = y;

			CheckSelectedSize(w, statspec);

			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}
		}
	} break;

	case WE_DROPDOWN_SELECT:
		if (_railstation.station_class != e->we.dropdown.index) {
			_railstation.station_class = (StationClassID)e->we.dropdown.index;
			_railstation.station_type  = 0;
			_railstation.station_count = GetNumCustomStations(_railstation.station_class);

			CheckSelectedSize(w, GetCustomStationSpec(_railstation.station_class, _railstation.station_type));

			w->vscroll.count = _railstation.station_count;
			w->vscroll.pos   = _railstation.station_type;
		}

		SndPlayFx(SND_15_BEEP);
		SetWindowDirty(w);
		break;

	case WE_MOUSELOOP:
		if (WP(w,def_d).close) {
			DeleteWindow(w);
			return;
		}
		CheckRedrawStationCoverage(w);
		break;

	case WE_DESTROY:
		if (!WP(w,def_d).close) ResetObjectToPlace();
		break;
	}
}

static const Widget _station_builder_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3000_RAIL_STATION_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,   199, 0x0,                             STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     7,    72,    26,    73, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},
{      WWT_PANEL,   RESIZE_NONE,    14,    75,   140,    26,    73, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,    87,    98, STR_00CB_1,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,    87,    98, STR_00CC_2,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,    87,    98, STR_00CD_3,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,    87,    98, STR_00CE_4,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,    87,    98, STR_00CF_5,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,    87,    98, STR_0335_6,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,    87,    98, STR_0336_7,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,   112,   123, STR_00CB_1,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,   112,   123, STR_00CC_2,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,   112,   123, STR_00CD_3,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,   112,   123, STR_00CE_4,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,   112,   123, STR_00CF_5,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,   112,   123, STR_0335_6,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,   112,   123, STR_0336_7,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,   111,   126,   137, STR_DRAG_DROP,                   STR_STATION_DRAG_DROP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    73,   152,   163, STR_02DB_OFF,                    STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    74,   133,   152,   163, STR_02DA_ON,                     STR_3064_HIGHLIGHT_COVERAGE_AREA},
{   WIDGETS_END},
};

static const Widget _newstation_builder_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3000_RAIL_STATION_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,   289, 0x0,                             STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     7,    72,   116,   163, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},
{      WWT_PANEL,   RESIZE_NONE,    14,    75,   140,   116,   163, 0x0,                             STR_304E_SELECT_RAILROAD_STATION},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,   177,   188, STR_00CB_1,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,   177,   188, STR_00CC_2,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,   177,   188, STR_00CD_3,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,   177,   188, STR_00CE_4,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,   177,   188, STR_00CF_5,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,   177,   188, STR_0335_6,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,   177,   188, STR_0336_7,                      STR_304F_SELECT_NUMBER_OF_PLATFORMS},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    22,    36,   202,   213, STR_00CB_1,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,    51,   202,   213, STR_00CC_2,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    52,    66,   202,   213, STR_00CD_3,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    67,    81,   202,   213, STR_00CE_4,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    82,    96,   202,   213, STR_00CF_5,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    97,   111,   202,   213, STR_0335_6,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   112,   126,   202,   213, STR_0336_7,                      STR_3050_SELECT_LENGTH_OF_RAILROAD},

{    WWT_TEXTBTN,   RESIZE_NONE,    14,    37,   111,   216,   227, STR_DRAG_DROP,                   STR_STATION_DRAG_DROP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    73,   242,   253, STR_02DB_OFF,                    STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    74,   133,   242,   253, STR_02DA_ON,                     STR_3064_HIGHLIGHT_COVERAGE_AREA},

/* newstations gui additions */
{      WWT_INSET,   RESIZE_NONE,    14,     7,   140,    17,    28, STR_02BD,                        STR_SELECT_STATION_CLASS_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   129,   139,    18,    27, STR_0225,                        STR_SELECT_STATION_CLASS_TIP},
{     WWT_MATRIX,   RESIZE_NONE,    14,     7,   128,    32,   102, 0x501,                           STR_SELECT_STATION_TYPE_TIP},
{  WWT_SCROLLBAR,   RESIZE_NONE,    14,   129,   140,    32,   102, 0x0,                             STR_0190_SCROLL_BAR_SCROLLS_LIST},
{   WIDGETS_END},
};

static const WindowDesc _station_builder_desc = {
	WDP_AUTO, WDP_AUTO, 148, 200,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_station_builder_widgets,
	StationBuildWndProc
};

static const WindowDesc _newstation_builder_desc = {
	WDP_AUTO, WDP_AUTO, 148, 290,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_newstation_builder_widgets,
	StationBuildWndProc
};

static void ShowStationBuilder()
{
	Window *w;
	if (GetNumStationClasses() <= 2 && GetNumCustomStations(STAT_CLASS_DFLT) == 1) {
		w = AllocateWindowDesc(&_station_builder_desc);
		_railstation.newstations = false;
	} else {
		w = AllocateWindowDesc(&_newstation_builder_desc);
		_railstation.newstations = true;
		_railstation.station_count = GetNumCustomStations(_railstation.station_class);

		w->vscroll.count = _railstation.station_count;
		w->vscroll.cap   = 5;
		w->vscroll.pos   = clamp(_railstation.station_type - 2, 0, w->vscroll.count - w->vscroll.cap);
	}
}

static void BuildTrainDepotWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: LowerWindowWidget(w, _build_depot_direction + 3); break;

	case WE_PAINT: {
		RailType r;

		DrawWindowWidgets(w);

		r = _cur_railtype;
		DrawTrainDepotSprite(70, 17, 0, r);
		DrawTrainDepotSprite(70, 69, 1, r);
		DrawTrainDepotSprite( 2, 69, 2, r);
		DrawTrainDepotSprite( 2, 17, 3, r);
		break;
		}

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 3:
			case 4:
			case 5:
			case 6:
				RaiseWindowWidget(w, _build_depot_direction + 3);
				_build_depot_direction = (DiagDirection)(e->we.click.widget - 3);
				LowerWindowWidget(w, _build_depot_direction + 3);
				SndPlayFx(SND_15_BEEP);
				SetWindowDirty(w);
				break;
		}
		break;

	case WE_MOUSELOOP:
		if (WP(w,def_d).close) DeleteWindow(w);
		return;

	case WE_DESTROY:
		if (!WP(w,def_d).close) ResetObjectToPlace();
		break;
	}
}

static const Widget _build_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_1014_TRAIN_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   121, 0x0,                              STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,                              STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{   WIDGETS_END},
};

static const WindowDesc _build_depot_desc = {
	WDP_AUTO, WDP_AUTO, 140, 122,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_depot_widgets,
	BuildTrainDepotWndProc
};

static void ShowBuildTrainDepotPicker()
{
	AllocateWindowDesc(&_build_depot_desc);
}


static void BuildWaypointWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		uint i;

		for (i = 0; i < w->hscroll.cap; i++) {
			SetWindowWidgetLoweredState(w, i + 3, (w->hscroll.pos + i) == _cur_waypoint_type);
		}

		DrawWindowWidgets(w);

		for (i = 0; i < w->hscroll.cap; i++) {
			if (w->hscroll.pos + i < w->hscroll.count) {
				const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, w->hscroll.pos + i);

				DrawWaypointSprite(2 + i * 68, 25, w->hscroll.pos + i, _cur_railtype);

				if (statspec != NULL &&
						HASBIT(statspec->callbackmask, CBM_STATION_AVAIL) &&
						GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE) == 0) {
					GfxFillRect(4 + i * 68, 18, 67 + i * 68, 75, (1 << PALETTE_MODIFIER_GREYOUT));
				}
			}
		}
		break;
	}
	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 3: case 4: case 5: case 6: case 7: {
			byte type = e->we.click.widget - 3 + w->hscroll.pos;

			/* Check station availability callback */
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, type);
			if (statspec != NULL &&
					HASBIT(statspec->callbackmask, CBM_STATION_AVAIL) &&
					GetStationCallback(CBID_STATION_AVAILABILITY, 0, 0, statspec, NULL, INVALID_TILE) == 0) return;

			_cur_waypoint_type = type;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}
		}
		break;
	}

	case WE_MOUSELOOP:
		if (WP(w,def_d).close) DeleteWindow(w);
		break;

	case WE_DESTROY:
		if (!WP(w,def_d).close) ResetObjectToPlace();
		break;
	}
}

static const Widget _build_waypoint_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,     STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   343,     0,    13, STR_WAYPOINT, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   343,    14,    91, 0x0,          0},

{      WWT_PANEL,   RESIZE_NONE,     7,     3,    68,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,    71,   136,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,   139,   204,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,   207,   272,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,   275,   340,    17,    76, 0x0,          STR_WAYPOINT_GRAPHICS_TIP},

{ WWT_HSCROLLBAR,   RESIZE_NONE,    7,     1,   343,     80,    91, 0x0,          STR_0190_SCROLL_BAR_SCROLLS_LIST},
{    WIDGETS_END},
};

static const WindowDesc _build_waypoint_desc = {
	WDP_AUTO, WDP_AUTO, 344, 92,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_waypoint_widgets,
	BuildWaypointWndProc
};

static void ShowBuildWaypointPicker()
{
	Window *w = AllocateWindowDesc(&_build_waypoint_desc);
	w->hscroll.cap = 5;
	w->hscroll.count = _waypoint_count;
}


void InitializeRailGui()
{
	_build_depot_direction = DIAGDIR_NW;
	_railstation.numtracks = 1;
	_railstation.platlength = 1;
	_railstation.dragdrop = true;
}

void ReinitGuiAfterToggleElrail(bool disable)
{
	extern RailType _last_built_railtype;
	if (disable && _last_built_railtype == RAILTYPE_ELECTRIC) {
		Window *w;
		_last_built_railtype = _cur_railtype = RAILTYPE_RAIL;
		w = FindWindowById(WC_BUILD_TOOLBAR, 0);
		if (w != NULL && w->wndproc == BuildRailToolbWndProc) {
			SetupRailToolbar(_cur_railtype, w);
			SetWindowDirty(w);
		}
	}
	MarkWholeScreenDirty();
}


