#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "sound.h"
#include "command.h"
#include "vehicle.h"
#include "station.h"

static uint _cur_railtype;
static bool _remove_button_clicked;
static byte _build_depot_direction;
static byte _waypoint_count=1;
static byte _cur_waypoint_type;

struct {
	byte orientation;
	byte numtracks;
	byte platlength;
	bool dragdrop;
} _railstation;


static void HandleStationPlacement(uint start, uint end);
static void ShowBuildTrainDepotPicker();
static void ShowBuildWaypointPicker();
static void ShowStationBuilder();

typedef void OnButtonClick(Window *w);

void CcPlaySound1E(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_20_SPLAT_2, tile);
}

static void GenericPlaceRail(uint tile, int cmd)
{
	DoCommandP(tile, _cur_railtype, cmd, CcPlaySound1E,
		_remove_button_clicked ?
		CMD_REMOVE_SINGLE_RAIL | CMD_MSG(STR_1012_CAN_T_REMOVE_RAILROAD_TRACK) | CMD_AUTO | CMD_NO_WATER :
		CMD_BUILD_SINGLE_RAIL | CMD_MSG(STR_1011_CAN_T_BUILD_RAILROAD_TRACK) | CMD_AUTO | CMD_NO_WATER
		);
}

static void PlaceRail_N(uint tile)
{
	int cmd = _tile_fract_coords.x > _tile_fract_coords.y ? 4 : 5;
	GenericPlaceRail(tile, cmd);
}

static void PlaceRail_NE(uint tile)
{
	VpStartPlaceSizing(tile, VPM_FIX_Y);
}

static void PlaceRail_E(uint tile)
{
	int cmd = _tile_fract_coords.x + _tile_fract_coords.y <= 15 ? 2 : 3;
	GenericPlaceRail(tile, cmd);
}

static void PlaceRail_NW(uint tile)
{
	VpStartPlaceSizing(tile, VPM_FIX_X);
}

static void PlaceRail_AutoRail(uint tile)
{
	VpStartPlaceSizing(tile, VPM_RAILDIRS);
}

static void PlaceExtraDepotRail(uint tile, uint16 extra)
{
	byte b = _map5[tile];

	if (b & 0xC0 || !(b & (extra >> 8)))
		return;

	DoCommandP(tile, _cur_railtype, extra & 0xFF, NULL, CMD_BUILD_SINGLE_RAIL | CMD_AUTO | CMD_NO_WATER);
}

static const uint16 _place_depot_extra[12] = {
	0x604,		0x2102,		0x1202,		0x505,
	0x2400,		0x2801,		0x1800,		0x1401,
	0x2203,		0x904,		0x0A05,		0x1103,
};


void CcRailDepot(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) {
		int dir = p2;

		SndPlayTileFx(SND_20_SPLAT_2, tile);
		ResetObjectToPlace();

		tile += TileOffsByDir(dir);

		if (IsTileType(tile, MP_RAILWAY)) {
			PlaceExtraDepotRail(tile, _place_depot_extra[dir]);
			PlaceExtraDepotRail(tile, _place_depot_extra[dir + 4]);
			PlaceExtraDepotRail(tile, _place_depot_extra[dir + 8]);
		}
	}
}

static void PlaceRail_Depot(uint tile)
{
	DoCommandP(tile, _cur_railtype, _build_depot_direction, CcRailDepot,
		CMD_BUILD_TRAIN_DEPOT | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_100E_CAN_T_BUILD_TRAIN_DEPOT));
}

static void PlaceRail_Waypoint(uint tile)
{
	if (!_remove_button_clicked) {
		DoCommandP(tile, _waypoint_count > 0 ? (0x100 + _cur_waypoint_type) : 0, 0, CcPlaySound1E, CMD_BUILD_TRAIN_WAYPOINT | CMD_MSG(STR_CANT_BUILD_TRAIN_WAYPOINT));
	} else {
		DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_REMOVE_TRAIN_WAYPOINT | CMD_MSG(STR_CANT_REMOVE_TRAIN_WAYPOINT));
	}
}

void CcStation(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_20_SPLAT_2, tile);
		ResetObjectToPlace();
	}
}

static void PlaceRail_Station(uint tile)
{
	if(_remove_button_clicked)
		DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_REMOVE_FROM_RAILROAD_STATION | CMD_MSG(STR_CANT_REMOVE_PART_OF_STATION));
	else if (_railstation.dragdrop) {
		VpStartPlaceSizing(tile, VPM_X_AND_Y_LIMITED);
		VpSetPlaceSizingLimit(_patches.station_spread);
	} else {
		// TODO: Custom station selector GUI. Now we just try using first custom station
		// (and fall back to normal stations if it isn't available).
		DoCommandP(tile, _railstation.orientation | (_railstation.numtracks<<8) | (_railstation.platlength<<16),_cur_railtype|1<<4, CcStation,
				CMD_BUILD_RAILROAD_STATION | CMD_NO_WATER | CMD_AUTO | CMD_MSG(STR_100F_CAN_T_BUILD_RAILROAD_STATION));
	}
}

static void GenericPlaceSignals(uint tile)
{
	uint trackstat;
	int i;

	trackstat = (byte)GetTileTrackStatus(tile, TRANSPORT_RAIL);

	if ((trackstat & 0x30) == 0x30) {
		trackstat = (_tile_fract_coords.x <= _tile_fract_coords.y) ? 0x20 : 0x10;
	}

	if ((trackstat & 0x0C) == 0x0C) {
		trackstat = (_tile_fract_coords.x + _tile_fract_coords.y <= 15) ? 4 : 8;
	}

	// Lookup the bit index
	i = 0;
	if (trackstat != 0) {	while (!(trackstat & 1)) { i++; trackstat >>= 1; }}

	if (!_remove_button_clicked) {
		DoCommandP(tile, i + (_ctrl_pressed ? 8 : 0), 0, CcPlaySound1E,
			CMD_BUILD_SIGNALS | CMD_AUTO | CMD_MSG(STR_1010_CAN_T_BUILD_SIGNALS_HERE));
	} else {
		DoCommandP(tile, i, 0, CcPlaySound1E,
			CMD_REMOVE_SIGNALS | CMD_AUTO | CMD_MSG(STR_1013_CAN_T_REMOVE_SIGNALS_FROM));
	}
}

static void PlaceRail_Bridge(uint tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y);
}

void CcBuildRailTunnel(bool success, uint tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_20_SPLAT_2, tile);
		ResetObjectToPlace();
	} else {
		SetRedErrorSquare(_build_tunnel_endtile);
	}
}

static void PlaceRail_Tunnel(uint tile)
{
	DoCommandP(tile, _cur_railtype, 0, CcBuildRailTunnel,
		CMD_BUILD_TUNNEL | CMD_AUTO | CMD_MSG(STR_5016_CAN_T_BUILD_TUNNEL_HERE));
}

void PlaceProc_BuyLand(uint tile)
{
	DoCommandP(tile, 0, 0, CcPlaySound1E, CMD_PURCHASE_LAND_AREA | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_5806_CAN_T_PURCHASE_THIS_LAND));
}

static void PlaceRail_ConvertRail(uint tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y | (1<<4));
}

static void PlaceRail_AutoSignals(uint tile)
{
	VpStartPlaceSizing(tile, VPM_SIGNALDIRS);
}

static void BuildRailClick_N(Window *w)
{
	HandlePlacePushButton(w, 4, _cur_railtype*4 + 0x4EF, 1, PlaceRail_N);
}

static void BuildRailClick_NE(Window *w)
{
	HandlePlacePushButton(w, 5, _cur_railtype*4 + 0x4F0, 1, PlaceRail_NE);
}

static void BuildRailClick_E(Window *w)
{
	HandlePlacePushButton(w, 6, _cur_railtype*4 + 0x4F1, 1, PlaceRail_E);
}

static void BuildRailClick_NW(Window *w)
{
	HandlePlacePushButton(w, 7, _cur_railtype*4 + 0x4F2, 1, PlaceRail_NW);
}

static void BuildRailClick_AutoRail(Window *w)
{
	HandlePlacePushButton(w, 8, _cur_railtype + SPR_CURSOR_AUTORAIL, VHM_RAIL, PlaceRail_AutoRail);
}

static void BuildRailClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, 9, ANIMCURSOR_DEMOLISH, 1, PlaceProc_DemolishArea);
}

static const SpriteID _depot_cursors[] = {
	0x510,
	SPR_OPENTTD_BASE + 14,
	SPR_OPENTTD_BASE + 15,
};

static void BuildRailClick_Depot(Window *w)
{
	if (HandlePlacePushButton(w, 10, _depot_cursors[_cur_railtype], 1, PlaceRail_Depot)) ShowBuildTrainDepotPicker();
}

static void BuildRailClick_Waypoint(Window *w)
{
	_waypoint_count = GetCustomStationsCount(STAT_CLASS_WAYP);
	if (HandlePlacePushButton(w, 11, SPR_OPENTTD_BASE + 7, 1, PlaceRail_Waypoint)
	    && _waypoint_count > 1)
		ShowBuildWaypointPicker();
}

static void BuildRailClick_Station(Window *w)
{
	if (HandlePlacePushButton(w, 12, 0x514, 1, PlaceRail_Station)) ShowStationBuilder();
}

static void BuildRailClick_AutoSignals(Window *w)
{
	HandlePlacePushButton(w, 13, ANIMCURSOR_BUILDSIGNALS, VHM_RECT, PlaceRail_AutoSignals);
}

static void BuildRailClick_Bridge(Window *w)
{
	HandlePlacePushButton(w, 14, 0xA21, 1, PlaceRail_Bridge);
}

static void BuildRailClick_Tunnel(Window *w)
{
	HandlePlacePushButton(w, 15, 0x982 + _cur_railtype, 3, PlaceRail_Tunnel);
}

static void BuildRailClick_Remove(Window *w)
{
	if (w->disabled_state & (1<<16))
		return;
	SetWindowDirty(w);
	SndPlayFx(SND_15_BEEP);

	_thd.make_square_red = !!((w->click_state ^= (1 << 16)) & (1<<16));
	MarkTileDirty(_thd.pos.x, _thd.pos.y);
	_remove_button_clicked = (w->click_state & (1 << 16)) != 0;

	// handle station builder
	if( w->click_state & (1 << 16) )
	{
		if(_remove_button_clicked)
			SetTileSelectSize(1, 1);
		else
			BringWindowToFrontById(WC_BUILD_STATION, 0);
	}
}

static void BuildRailClick_Convert(Window *w)
{
	HandlePlacePushButton(w, 17, (SPR_OPENTTD_BASE + 26) + _cur_railtype * 2, 1, PlaceRail_ConvertRail);
}

static void BuildRailClick_Landscaping(Window *w)
{
	ShowTerraformToolbar();
}


static void DoRailroadTrack(int mode)
{
	DoCommandP(TILE_FROM_XY(_thd.selstart.x, _thd.selstart.y), PACK_POINT(_thd.selend.x, _thd.selend.y), (mode << 4) | _cur_railtype, NULL,
		_remove_button_clicked ?
		CMD_REMOVE_RAILROAD_TRACK | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1012_CAN_T_REMOVE_RAILROAD_TRACK) :
		CMD_BUILD_RAILROAD_TRACK | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1011_CAN_T_BUILD_RAILROAD_TRACK)
	);
}


// This function is more or less a hack because DoRailroadTrack() would otherwise screw up
static void SwapSelection()
{
	TileHighlightData *thd = &_thd;
	Point pt = thd->selstart;
	thd->selstart.x = thd->selend.x & ~0xF;
	thd->selstart.y = thd->selend.y & ~0xF;
	thd->selend = pt;
}


static void HandleAutodirPlacement()
{
	TileHighlightData *thd = &_thd;
	int bit;
	int dx = thd->selstart.x - (thd->selend.x&~0xF);
	int dy = thd->selstart.y - (thd->selend.y&~0xF);

	if (thd->drawstyle & HT_RAIL) { // one tile case
		bit = thd->drawstyle & 0xF;
		GenericPlaceRail(TILE_FROM_XY(thd->selend.x, thd->selend.y), bit);
	} else if ( !(thd->drawstyle & 0xE) ) { // x/y dir
		if (dx == 0) { // Y dir
			DoRailroadTrack(1);
		} else {
			DoRailroadTrack(2);
		}
	} else if (myabs(dx)+myabs(dy) >= 32) { // long line (more than 2 tiles)
		if(thd->drawstyle == (HT_LINE | HT_DIR_HU))
			DoRailroadTrack(0);
		if(thd->drawstyle == (HT_LINE | HT_DIR_HL))
			DoRailroadTrack(3);
		if(thd->drawstyle == (HT_LINE | HT_DIR_VL))
			DoRailroadTrack(3);
		if(thd->drawstyle == (HT_LINE | HT_DIR_VR))
			DoRailroadTrack(0);
	} else { // 2x1 pieces line
		if(thd->drawstyle == (HT_LINE | HT_DIR_HU)) {
			DoRailroadTrack(0);
		} else if (thd->drawstyle == (HT_LINE | HT_DIR_HL)) {
			SwapSelection();
			DoRailroadTrack(0);
			SwapSelection();
		} else if (thd->drawstyle == (HT_LINE | HT_DIR_VL)) {
			if(dx == 0) {
				SwapSelection();
				DoRailroadTrack(0);
				SwapSelection();
			} else {
				DoRailroadTrack(3);
			}
		} else if (thd->drawstyle == (HT_LINE | HT_DIR_VR)) {
			if(dx == 0) {
				DoRailroadTrack(0);
			} else {
				SwapSelection();
				DoRailroadTrack(3);
				SwapSelection();
			}
		}
	}
}

static void HandleAutoSignalPlacement(void)
{
	TileHighlightData *thd = &_thd;
	int mode = 0;
	uint trackstat = 0;

	int dx = thd->selstart.x - (thd->selend.x&~0xF);
	int dy = thd->selstart.y - (thd->selend.y&~0xF);

	if (dx == 0 && dy == 0 ) // 1x1 tile signals
		GenericPlaceSignals(TILE_FROM_XY(thd->selend.x, thd->selend.y));
	else { // signals have been dragged
		if (!(thd->drawstyle & 0xE)) { // X,Y direction
			if (dx == 0)
				mode = VPM_FIX_X;
			else if (dy == 0)
				mode = VPM_FIX_Y;

			trackstat = 0xC0;
		}	else { // W-E or N-S direction
			if ((thd->drawstyle & 0xF) == 2 || (thd->drawstyle & 0xF) == 5)
				mode = 0;
			else
				mode = 3;

			if (dx == dy || abs(dx - dy) == 16) // North<->South track |
				trackstat = (thd->drawstyle & 1) ? 0x20 : 0x10;
			else if (dx == -dy || abs(dx + dy) == 16) // East<->West track --
				trackstat = (thd->drawstyle & 1) ? 8 : 4;
		}
		// _patches.drag_signals_density is given as a parameter such that each user in a network
		// game can specify his/her own signal density
		DoCommandP(TILE_FROM_XY(thd->selstart.x, thd->selstart.y), TILE_FROM_XY(thd->selend.x, thd->selend.y),
		(mode << 4) | (_remove_button_clicked + (_ctrl_pressed ? 8 : 0)) | (trackstat << 8) | (_patches.drag_signals_density << 24),
		CcPlaySound1E,
		(_remove_button_clicked ?	CMD_BUILD_MANY_SIGNALS | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1013_CAN_T_REMOVE_SIGNALS_FROM) :
															CMD_BUILD_MANY_SIGNALS | CMD_AUTO | CMD_NO_WATER | CMD_MSG(STR_1010_CAN_T_BUILD_SIGNALS_HERE) ) );
	}
}

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
	BuildRailClick_Convert,
	BuildRailClick_Landscaping,
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
	'L', // landscaping
};




static void BuildRailToolbWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT:
		w->disabled_state &= ~(1 << 16);
		if (!(w->click_state & ((1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8)|(1<<11)|(1<<12)|(1<<13)))) {
			w->disabled_state |= (1 << 16);
			w->click_state &= ~(1<<16);
		}
		DrawWindowWidgets(w);
		break;

	case WE_CLICK:
		if (e->click.widget >= 4) {
			_remove_button_clicked = false;
			_build_railroad_button_proc[e->click.widget - 4](w);
		}
	break;

	case WE_KEYPRESS: {
		int i;

		for(i=0; i!=lengthof(_rail_keycodes); i++) {
			if (e->keypress.keycode == _rail_keycodes[i]) {
				e->keypress.cont = false;
				_remove_button_clicked = false;
				_build_railroad_button_proc[i](w);
				break;
			}
		}
		MarkTileDirty(_thd.pos.x, _thd.pos.y); // redraw tile selection
		break;
	}

	case WE_PLACE_OBJ:
		_place_proc(e->place.tile);
		return;

	case WE_PLACE_DRAG: {
		VpSelectTilesWithMethod(e->place.pt.x, e->place.pt.y, e->place.userdata & 0xF);
		return;
	}

	case WE_PLACE_MOUSEUP:
		if (e->click.pt.x != -1) {
			uint start_tile = e->place.starttile;
			uint end_tile = e->place.tile;

			if (e->place.userdata == VPM_X_OR_Y) {
				ResetObjectToPlace();
				ShowBuildBridgeWindow(start_tile, end_tile, _cur_railtype);
			} else if (e->place.userdata == VPM_RAILDIRS) {
				bool old = _remove_button_clicked;
				if (_ctrl_pressed) _remove_button_clicked = true;
				HandleAutodirPlacement();
				_remove_button_clicked = old;
			} else if (e->place.userdata == VPM_SIGNALDIRS) {
				HandleAutoSignalPlacement();
			} else if (e->place.userdata == VPM_X_AND_Y) {
				DoCommandP(end_tile, start_tile, 0, CcPlaySound10, CMD_CLEAR_AREA | CMD_MSG(STR_00B5_CAN_T_CLEAR_THIS_AREA));
			} else if (e->place.userdata == (VPM_X_AND_Y | (1<<4))) {
				DoCommandP(end_tile, start_tile, _cur_railtype, CcPlaySound10, CMD_CONVERT_RAIL | CMD_MSG(STR_CANT_CONVERT_RAIL));
			} else if (e->place.userdata == (VPM_X_AND_Y | (2<<4))) {
				DoCommandP(end_tile, start_tile, _cur_railtype, CcPlaySound10, CMD_LEVEL_LAND | CMD_AUTO);
			} else if (e->place.userdata == VPM_X_AND_Y_LIMITED) {
				HandleStationPlacement(start_tile, end_tile);
			} else {
				DoRailroadTrack(e->place.userdata);
			}
		}
		break;

	case WE_ABORT_PLACE_OBJ:
		UnclickWindowButtons(w);
		SetWindowDirty(w);

		w = FindWindowById(WC_BUILD_STATION, 0);
		if (w != NULL) WP(w,def_d).close=true;
		w = FindWindowById(WC_BUILD_DEPOT, 0);
		if (w != NULL) WP(w,def_d).close=true;
		break;

	case WE_PLACE_PRESIZE: {
		uint tile = e->place.tile;
		DoCommandByTile(tile, 0, 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile==0?tile:_build_tunnel_endtile);
	} break;
	}
}


static const Widget _build_railroad_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   359,     0,    13, STR_100A_RAILROAD_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   360,   371,     0,    13, 0x0,     STR_STICKY_BUTTON},

{      WWT_PANEL,   RESIZE_NONE,     7,   110,   113,    14,    35, 0x0,			STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,     7,    0,     21,    14,    35, 0x4E3,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    22,    43,    14,    35, 0x4E4,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    44,    65,    14,    35, 0x4E5,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    66,    87,    14,    35, 0x4E6,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_OPENTTD_BASE + 0, STR_BUILD_AUTORAIL_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   114,   135,    14,    35, 0x2BF,		STR_018D_DEMOLISH_BUILDINGS_ETC},
{      WWT_PANEL,   RESIZE_NONE,     7,   136,   157,    14,    35, 0x50E,		STR_1019_BUILD_TRAIN_DEPOT_FOR_BUILDING},
{      WWT_PANEL,   RESIZE_NONE,     7,   158,   179,    14,    35, SPR_OPENTTD_BASE + 3, STR_CONVERT_RAIL_TO_WAYPOINT_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   180,   221,    14,    35, 0x512,		STR_101A_BUILD_RAILROAD_STATION},
{      WWT_PANEL,   RESIZE_NONE,     7,   222,   243,    14,    35, 0x50B,		STR_101B_BUILD_RAILROAD_SIGNALS},
{      WWT_PANEL,   RESIZE_NONE,     7,   244,   285,    14,    35, 0xA22,		STR_101C_BUILD_RAILROAD_BRIDGE},
{      WWT_PANEL,   RESIZE_NONE,     7,   286,   305,    14,    35, 0x97E,		STR_101D_BUILD_RAILROAD_TUNNEL},
{      WWT_PANEL,   RESIZE_NONE,     7,   306,   327,    14,    35, 0x2CA,		STR_101E_TOGGLE_BUILD_REMOVE_FOR},
{      WWT_PANEL,   RESIZE_NONE,     7,   328,   349,    14,    35, SPR_OPENTTD_BASE + 25, STR_CONVERT_RAIL_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   350,   371,    14,    35, SPR_IMG_LANDSCAPING,	STR_LANDSCAPING_TOOLBAR_TIP},

{   WIDGETS_END},
};

static const WindowDesc _build_railroad_desc = {
	640-372, 22, 372, 36,
	WC_BUILD_TOOLBAR,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_railroad_widgets,
	BuildRailToolbWndProc
};

static const Widget _build_monorail_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   359,     0,    13, STR_100B_MONORAIL_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   360,   371,     0,    13, 0x0,     STR_STICKY_BUTTON},

{      WWT_PANEL,   RESIZE_NONE,     7,   110,   113,    14,    35, 0x0,			STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,     7,     0,    21,    14,    35, 0x4E7,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    22,    43,    14,    35, 0x4E8,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    44,    65,    14,    35, 0x4E9,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    66,    87,    14,    35, 0x4EA,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_OPENTTD_BASE + 1, STR_BUILD_AUTORAIL_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   114,   135,    14,    35, 0x2BF,		STR_018D_DEMOLISH_BUILDINGS_ETC},
{      WWT_PANEL,   RESIZE_NONE,     7,   136,   157,    14,    35, SPR_OPENTTD_BASE + 12, STR_1019_BUILD_TRAIN_DEPOT_FOR_BUILDING},
{      WWT_PANEL,   RESIZE_NONE,     7,   158,   179,    14,    35, SPR_OPENTTD_BASE + 3, STR_CONVERT_RAIL_TO_WAYPOINT_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   180,   221,    14,    35, 0x512,		STR_101A_BUILD_RAILROAD_STATION},
{      WWT_PANEL,   RESIZE_NONE,     7,   222,   243,    14,    35, 0x50B,		STR_101B_BUILD_RAILROAD_SIGNALS},
{      WWT_PANEL,   RESIZE_NONE,     7,   244,   285,    14,    35, 0xA22,		STR_101C_BUILD_RAILROAD_BRIDGE},
{      WWT_PANEL,   RESIZE_NONE,     7,   286,   305,    14,    35, 0x97F,		STR_101D_BUILD_RAILROAD_TUNNEL},
{      WWT_PANEL,   RESIZE_NONE,     7,   306,   327,    14,    35, 0x2CA,		STR_101E_TOGGLE_BUILD_REMOVE_FOR},
{      WWT_PANEL,   RESIZE_NONE,     7,   328,   349,    14,    35, SPR_OPENTTD_BASE + 27, STR_CONVERT_RAIL_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   350,   371,    14,    35, SPR_IMG_LANDSCAPING,	STR_LANDSCAPING_TOOLBAR_TIP},

{   WIDGETS_END},
};

static const WindowDesc _build_monorail_desc = {
	640-372, 22, 372, 36,
	WC_BUILD_TOOLBAR,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_monorail_widgets,
	BuildRailToolbWndProc
};

static const Widget _build_maglev_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   359,     0,    13, STR_100C_MAGLEV_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   360,   371,     0,    13, 0x0,     STR_STICKY_BUTTON},

{      WWT_PANEL,   RESIZE_NONE,     7,   110,   113,    14,    35, 0x0,			STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,     7,     0,    21,    14,    35, 0x4EB,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    22,    43,    14,    35, 0x4EC,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    44,    65,    14,    35, 0x4EE,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    66,    87,    14,    35, 0x4ED,		STR_1018_BUILD_RAILROAD_TRACK},
{      WWT_PANEL,   RESIZE_NONE,     7,    88,   109,    14,    35, SPR_OPENTTD_BASE + 2, STR_BUILD_AUTORAIL_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   114,   135,    14,    35, 0x2BF,		STR_018D_DEMOLISH_BUILDINGS_ETC},
{      WWT_PANEL,   RESIZE_NONE,     7,   136,   157,    14,    35, SPR_OPENTTD_BASE + 13, STR_1019_BUILD_TRAIN_DEPOT_FOR_BUILDING},
{      WWT_PANEL,   RESIZE_NONE,     7,   158,   179,    14,    35, SPR_OPENTTD_BASE + 3, STR_CONVERT_RAIL_TO_WAYPOINT_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   180,   221,    14,    35, 0x512,		STR_101A_BUILD_RAILROAD_STATION},
{      WWT_PANEL,   RESIZE_NONE,     7,   222,   243,    14,    35, 0x50B,		STR_101B_BUILD_RAILROAD_SIGNALS},
{      WWT_PANEL,   RESIZE_NONE,     7,   244,   285,    14,    35, 0xA22,		STR_101C_BUILD_RAILROAD_BRIDGE},
{      WWT_PANEL,   RESIZE_NONE,     7,   286,   305,    14,    35, 0x980,		STR_101D_BUILD_RAILROAD_TUNNEL},
{      WWT_PANEL,   RESIZE_NONE,     7,   306,   327,    14,    35, 0x2CA,		STR_101E_TOGGLE_BUILD_REMOVE_FOR},
{      WWT_PANEL,   RESIZE_NONE,     7,   328,   349,    14,    35, SPR_OPENTTD_BASE + 29, STR_CONVERT_RAIL_TIP},

{      WWT_PANEL,   RESIZE_NONE,     7,   350,   371,    14,    35, SPR_IMG_LANDSCAPING,	STR_LANDSCAPING_TOOLBAR_TIP},

{   WIDGETS_END},
};

static const WindowDesc _build_maglev_desc = {
	640-372, 22, 372, 36,
	WC_BUILD_TOOLBAR,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_maglev_widgets,
	BuildRailToolbWndProc
};


static const WindowDesc * const _build_rr_desc[] = {
	&_build_railroad_desc,
	&_build_monorail_desc,
	&_build_maglev_desc,
};

void ShowBuildRailToolbar(int index, int button)
{
	Window *w;

	if (_current_player == OWNER_SPECTATOR) return;

	// don't recreate the window if we're clicking on a button and the window exists.
	if (button < 0 || !(w = FindWindowById(WC_BUILD_TOOLBAR, 0)) || w->wndproc != BuildRailToolbWndProc) {
		DeleteWindowById(WC_BUILD_TOOLBAR, 0);
		_cur_railtype = (byte)index;
		w = AllocateWindowDesc(_build_rr_desc[index]);
	}

	_remove_button_clicked = false;
	if (w != NULL && button >= 0) _build_railroad_button_proc[button](w);
}

/* TODO: For custom stations, respect their allowed platforms/lengths bitmasks!
 * --pasky */

static void HandleStationPlacement(uint start, uint end)
{
	uint sx = TileX(start);
	uint sy = TileY(start);
	uint ex = TileX(end);
	uint ey = TileY(end);
	uint w,h;

	if (sx > ex) intswap(sx,ex);
	if (sy > ey) intswap(sy,ey);
	w = ex - sx + 1;
	h = ey - sy + 1;
	if (!_railstation.orientation) intswap(w,h);

	// TODO: Custom station selector GUI. Now we just try using first custom station
	// (and fall back to normal stations if it isn't available).
	DoCommandP(TILE_XY(sx,sy), _railstation.orientation | (w<<8) | (h<<16),_cur_railtype|1<<4, CcStation,
		CMD_BUILD_RAILROAD_STATION | CMD_NO_WATER | CMD_AUTO | CMD_MSG(STR_100F_CAN_T_BUILD_RAILROAD_STATION));
}

static void StationBuildWndProc(Window *w, WindowEvent *e) {
	int rad;
	switch(e->event) {
	case WE_PAINT: {
		uint bits;

		if (WP(w,def_d).close)
			return;

		bits = (1<<3) << ( _railstation.orientation);
		if (_railstation.dragdrop) {
			bits |= (1<<19);
		} else {
			bits |= (1<<(5-1)) << (_railstation.numtracks);
			bits |= (1<<(12-1)) << (_railstation.platlength);
		}
		bits |= (1<<20) << (_station_show_coverage);
		w->click_state = bits;

		if (_railstation.dragdrop) {
			SetTileSelectSize(1, 1);
		} else {
			int x = _railstation.numtracks;
			int y = _railstation.platlength;
			if (_railstation.orientation == 0) intswap(x,y);
			if(!_remove_button_clicked)
				SetTileSelectSize(x, y);
		}

		rad = (_patches.modified_catchment) ? CA_TRAIN : 4;

		if (_station_show_coverage)
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);

		DrawWindowWidgets(w);

		StationPickerDrawSprite(39, 42, _cur_railtype, 2);
		StationPickerDrawSprite(107, 42, _cur_railtype, 3);

		DrawStringCentered(74, 15, STR_3002_ORIENTATION, 0);
		DrawStringCentered(74, 76, STR_3003_NUMBER_OF_TRACKS, 0);
		DrawStringCentered(74, 101, STR_3004_PLATFORM_LENGTH, 0);
		DrawStringCentered(74, 141, STR_3066_COVERAGE_AREA_HIGHLIGHT, 0);

		DrawStationCoverageAreaText(2, 166, (uint)-1, rad);
	} break;

	case WE_CLICK: {
		switch(e->click.widget) {
		case 0:
			ResetObjectToPlace();
			break;
		case 3:
		case 4:
			_railstation.orientation = e->click.widget - 3;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			_railstation.numtracks = (e->click.widget - 5) + 1;
			_railstation.dragdrop = false;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
			_railstation.platlength = (e->click.widget - 12) + 1;
			_railstation.dragdrop = false;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 19:
			_railstation.dragdrop ^= true;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;

		case 20:
		case 21:
			_station_show_coverage = e->click.widget - 20;
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
		ResetObjectToPlace();
		break;
	}
}

static const Widget _station_builder_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,		STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3000_RAIL_STATION_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,   199, 0x0,					STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     7,    72,    26,    73, 0x0,					STR_304E_SELECT_RAILROAD_STATION},
{      WWT_PANEL,   RESIZE_NONE,    14,    75,   140,    26,    73, 0x0,					STR_304E_SELECT_RAILROAD_STATION},

{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    22,    36,    87,    98, STR_00CB_1,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    37,    51,    87,    98, STR_00CC_2,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    52,    66,    87,    98, STR_00CD_3,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    67,    81,    87,    98, STR_00CE_4,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    82,    96,    87,    98, STR_00CF_5,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    97,   111,    87,    98, STR_0335_6,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   112,   126,    87,    98, STR_0336_7,	STR_304F_SELECT_NUMBER_OF_PLATFORMS},

{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    22,    36,   112,   123, STR_00CB_1,	STR_3050_SELECT_LENGTH_OF_RAILROAD},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    37,    51,   112,   123, STR_00CC_2,	STR_3050_SELECT_LENGTH_OF_RAILROAD},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    52,    66,   112,   123, STR_00CD_3,	STR_3050_SELECT_LENGTH_OF_RAILROAD},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    67,    81,   112,   123, STR_00CE_4,	STR_3050_SELECT_LENGTH_OF_RAILROAD},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    82,    96,   112,   123, STR_00CF_5,	STR_3050_SELECT_LENGTH_OF_RAILROAD},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    97,   111,   112,   123, STR_0335_6,	STR_3050_SELECT_LENGTH_OF_RAILROAD},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,   112,   126,   112,   123, STR_0336_7,	STR_3050_SELECT_LENGTH_OF_RAILROAD},

//{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    14,    73,   137,   148, STR_02DB_OFF, STR_3065_DON_T_HIGHLIGHT_COVERAGE},
//{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    74,   133,   137,   148, STR_02DA_ON, STR_3064_HIGHLIGHT_COVERAGE_AREA},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    37,   111,   126,   137, STR_DRAG_DROP, STR_STATION_DRAG_DROP},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    14,    73,   152,   163, STR_02DB_OFF, STR_3065_DON_T_HIGHLIGHT_COVERAGE},
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,    74,   133,   152,   163, STR_02DA_ON, STR_3064_HIGHLIGHT_COVERAGE_AREA},
{   WIDGETS_END},
};

static const WindowDesc _station_builder_desc = {
	-1, -1, 148, 200,
	WC_BUILD_STATION,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_station_builder_widgets,
	StationBuildWndProc
};

static void ShowStationBuilder()
{
	AllocateWindowDesc(&_station_builder_desc);
}

static void BuildTrainDepotWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		int r;

		w->click_state = (1 << 3) << _build_depot_direction;
		DrawWindowWidgets(w);

		r = _cur_railtype;
		DrawTrainDepotSprite(70, 17, 0, r);
		DrawTrainDepotSprite(70, 69, 1, r);
		DrawTrainDepotSprite(2, 69, 2, r);
		DrawTrainDepotSprite(2, 17, 3, r);
		break;
		}
	case WE_CLICK: {
		switch(e->click.widget) {
		case 0:
			ResetObjectToPlace();
			break;
		case 3:
		case 4:
		case 5:
		case 6:
			_build_depot_direction = e->click.widget - 3;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}
	} break;

	case WE_MOUSELOOP:
		if (WP(w,def_d).close)
			DeleteWindow(w);
		return;

	case WE_DESTROY:
		ResetObjectToPlace();
		break;
	}
}

static const Widget _build_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   139,     0,    13, STR_1014_TRAIN_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   139,    14,   121, 0x0,			STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    17,    66, 0x0,			STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{      WWT_PANEL,   RESIZE_NONE,    14,    71,   136,    69,   118, 0x0,			STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    69,   118, 0x0,			STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{      WWT_PANEL,   RESIZE_NONE,    14,     3,    68,    17,    66, 0x0,			STR_1020_SELECT_RAILROAD_DEPOT_ORIENTATIO},
{   WIDGETS_END},
};

static const WindowDesc _build_depot_desc = {
	-1,-1, 140, 122,
	WC_BUILD_DEPOT,WC_BUILD_TOOLBAR,
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
	switch(e->event) {
	case WE_PAINT: {
		int r;

		w->click_state = (1 << 3) << _cur_waypoint_type;
		DrawWindowWidgets(w);

		r = 4*w->hscroll.pos;
		if(r+0<=_waypoint_count) DrawWaypointSprite(2,   25, r + 0, _cur_railtype);
		if(r+1<=_waypoint_count) DrawWaypointSprite(70,  25, r + 1, _cur_railtype);
		if(r+2<=_waypoint_count) DrawWaypointSprite(138, 25, r + 2, _cur_railtype);
		if(r+3<=_waypoint_count) DrawWaypointSprite(206, 25, r + 3, _cur_railtype);
		if(r+4<=_waypoint_count) DrawWaypointSprite(274, 25, r + 4, _cur_railtype);
		break;
		}
	case WE_CLICK: {
		switch(e->click.widget) {
		case 0:
			ResetObjectToPlace();
			break;
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			_cur_waypoint_type = e->click.widget - 3;
			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
			break;
		}
		break;
	}

	case WE_MOUSELOOP:
		if (WP(w,def_d).close)
			DeleteWindow(w);
		return;

	case WE_DESTROY:
		ResetObjectToPlace();
		break;
	}
}

static const Widget _build_waypoint_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   343,     0,    13, STR_WAYPOINT,STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   343,    14,    91, 0x0, 0},

{      WWT_PANEL,   RESIZE_NONE,     7,     3,    68,    17,    76, 0x0, STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,    71,   136,    17,    76, 0x0, STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,   139,   204,    17,    76, 0x0, STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,   207,   272,    17,    76, 0x0, STR_WAYPOINT_GRAPHICS_TIP},
{      WWT_PANEL,   RESIZE_NONE,     7,   275,   340,    17,    76, 0x0, STR_WAYPOINT_GRAPHICS_TIP},

{ WWT_HSCROLLBAR,   RESIZE_NONE,    7,     1,   343,     80,    91, 0x0, STR_0190_SCROLL_BAR_SCROLLS_LIST},
{    WIDGETS_END},
};

static const WindowDesc _build_waypoint_desc = {
	-1,-1, 344, 92,
	WC_BUILD_DEPOT,WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_waypoint_widgets,
	BuildWaypointWndProc
};

static void ShowBuildWaypointPicker()
{
	Window *w = AllocateWindowDesc(&_build_waypoint_desc);
	w->hscroll.cap = 5;
	w->hscroll.count = (uint) (_waypoint_count+4) / 5;
}


void InitializeRailGui()
{
	_build_depot_direction = 3;
	_railstation.numtracks = 1;
	_railstation.platlength = 1;
	_railstation.dragdrop = true;
}
