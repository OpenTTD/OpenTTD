/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "road_type.h"
#include "road_cmd.h"
#include "road_map.h"
#include "station_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "company_func.h"
#include "tunnelbridge.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "settings_type.h"

#include "table/sprites.h"
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

static void PlaceRoad_Tunnel(TileIndex tile)
{
	DoCommandP(tile, 0x200 | RoadTypeToRoadTypes(_cur_roadtype), 0, CMD_BUILD_TUNNEL | CMD_MSG(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE), CcBuildRoadTunnel);
}

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

void CcRoadDepot(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		DiagDirection dir = (DiagDirection)GB(p1, 0, 2);
		SndPlayTileFx(SND_1F_SPLAT, tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
		BuildRoadOutsideStation(tile, dir);
		/* For a drive-through road stop build connecting road for other entrance */
		if (HasBit(p2, 1)) BuildRoadOutsideStation(tile, ReverseDiagDir(dir));
	}
}

static void PlaceRoad_Depot(TileIndex tile)
{
	DoCommandP(tile, _cur_roadtype << 2 | _road_depot_orientation, 0, CMD_BUILD_ROAD_DEPOT | CMD_MSG(_road_type_infos[_cur_roadtype].err_depot), CcRoadDepot);
}

static void PlaceRoadStop(TileIndex tile, uint32 p2, uint32 cmd)
{
	uint32 p1 = _road_station_picker_orientation;
	SB(p2, 16, 16, INVALID_STATION); // no station to join

	if (p1 >= DIAGDIR_END) {
		SetBit(p2, 1); // It's a drive-through stop
		p1 -= DIAGDIR_END; // Adjust picker result to actual direction
	}
	CommandContainer cmdcont = { tile, p1, p2, cmd, CcRoadDepot, "" };
	ShowSelectStationIfNeeded(cmdcont, TileArea(tile, 1, 1));
}

static void PlaceRoad_BusStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		DoCommandP(tile, 0, ROADSTOP_BUS, CMD_REMOVE_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_station[ROADSTOP_BUS]), CcPlaySound1D);
	} else {
		PlaceRoadStop(tile, (_ctrl_pressed << 5) | RoadTypeToRoadTypes(_cur_roadtype) << 2 | ROADSTOP_BUS, CMD_BUILD_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_station[ROADSTOP_BUS]));
	}
}

static void PlaceRoad_TruckStation(TileIndex tile)
{
	if (_remove_button_clicked) {
		DoCommandP(tile, 0, ROADSTOP_TRUCK, CMD_REMOVE_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_station[ROADSTOP_TRUCK]), CcPlaySound1D);
	} else {
		PlaceRoadStop(tile, (_ctrl_pressed << 5) | RoadTypeToRoadTypes(_cur_roadtype) << 2 | ROADSTOP_TRUCK, CMD_BUILD_ROAD_STOP | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_station[ROADSTOP_TRUCK]));
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
	w->SetWidgetDirty(RTW_REMOVE);
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
	HandlePlacePushButton(w, RTW_ROAD_X, _road_type_infos[_cur_roadtype].cursor_nwse, HT_RECT, PlaceRoad_X_Dir);
}

/**
 * Function that handles the click on the
 *  Y road placement button.
 *
 * @param w The current window
 */
static void BuildRoadClick_Y_Dir(Window *w)
{
	HandlePlacePushButton(w, RTW_ROAD_Y, _road_type_infos[_cur_roadtype].cursor_nesw, HT_RECT, PlaceRoad_Y_Dir);
}

/**
 * Function that handles the click on the
 *  autoroad placement button.
 *
 * @param w The current window
 */
static void BuildRoadClick_AutoRoad(Window *w)
{
	HandlePlacePushButton(w, RTW_AUTOROAD, _road_type_infos[_cur_roadtype].cursor_autoroad, HT_RECT, PlaceRoad_AutoRoad);
}

static void BuildRoadClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, RTW_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT, PlaceProc_DemolishArea);
}

static void BuildRoadClick_Depot(Window *w)
{
	if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
	if (HandlePlacePushButton(w, RTW_DEPOT, SPR_CURSOR_ROAD_DEPOT, HT_RECT, PlaceRoad_Depot)) ShowRoadDepotPicker(w);
}

static void BuildRoadClick_BusStation(Window *w)
{
	if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
	if (HandlePlacePushButton(w, RTW_BUS_STATION, SPR_CURSOR_BUS_STATION, HT_RECT, PlaceRoad_BusStation)) ShowRVStationPicker(w, ROADSTOP_BUS);
}

static void BuildRoadClick_TruckStation(Window *w)
{
	if (_game_mode == GM_EDITOR || !CanBuildVehicleInfrastructure(VEH_ROAD)) return;
	if (HandlePlacePushButton(w, RTW_TRUCK_STATION, SPR_CURSOR_TRUCK_STATION, HT_RECT, PlaceRoad_TruckStation)) ShowRVStationPicker(w, ROADSTOP_TRUCK);
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
	HandlePlacePushButton(w, RTW_BUILD_BRIDGE, SPR_CURSOR_BRIDGE, HT_RECT, PlaceRoad_Bridge);
}

static void BuildRoadClick_Tunnel(Window *w)
{
	HandlePlacePushButton(w, RTW_BUILD_TUNNEL, SPR_CURSOR_ROAD_TUNNEL, HT_SPECIAL, PlaceRoad_Tunnel);
}

static void BuildRoadClick_Remove(Window *w)
{
	if (w->IsWidgetDisabled(RTW_REMOVE)) return;

	DeleteWindowById(WC_SELECT_STATION, 0);
	ToggleRoadButton_Remove(w);
	SndPlayFx(SND_15_BEEP);
}

/** Array with the handlers of the button-clicks for the road-toolbar */
static OnButtonClick * const _build_road_button_proc[] = {
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

struct BuildRoadToolbarWindow : Window {
	BuildRoadToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		this->SetWidgetsDisabledState(true,
			RTW_REMOVE,
			RTW_ONE_WAY,
			WIDGET_LIST_END);

		this->SetWidgetsDisabledState(!CanBuildVehicleInfrastructure(VEH_ROAD),
			RTW_DEPOT,
			RTW_BUS_STATION,
			RTW_TRUCK_STATION,
			WIDGET_LIST_END);

		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildRoadToolbarWindow()
	{
		if (_settings_client.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0, false);
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

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= RTW_ROAD_X) {
			_remove_button_clicked = false;
			_one_way_button_clicked = false;
			_build_road_button_proc[widget - RTW_ROAD_X](this);
		}
		this->UpdateOptionWidgetStatus((RoadToolbarWidgets)widget);
		if (_ctrl_pressed) RoadToolbar_CtrlChanged(this);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		for (uint i = 0; i != lengthof(_road_keycodes); i++) {
			if (keycode == _road_keycodes[i]) {
				_remove_button_clicked = false;
				_one_way_button_clicked = false;
				_build_road_button_proc[i](this);
				this->UpdateOptionWidgetStatus((RoadToolbarWidgets)(i + RTW_ROAD_X));
				if (_ctrl_pressed) RoadToolbar_CtrlChanged(this);
				state = ES_HANDLED;
				break;
			}
		}
		MarkTileDirtyByTile(TileVirtXY(_thd.pos.x, _thd.pos.y)); // redraw tile selection
		return state;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_remove_button_clicked = this->IsWidgetLowered(RTW_REMOVE);
		_one_way_button_clicked = this->IsWidgetLowered(RTW_ONE_WAY);
		_place_proc(tile);
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

					DoCommandP(end_tile, start_tile, _place_road_flag | (_cur_roadtype << 3) | (_one_way_button_clicked << 5),
						_remove_button_clicked ?
						CMD_REMOVE_LONG_ROAD | CMD_MSG(_road_type_infos[_cur_roadtype].err_remove_road) :
						CMD_BUILD_LONG_ROAD | CMD_MSG(_road_type_infos[_cur_roadtype].err_build_road), CcPlaySound1D);
					break;
			}
		}
	}

	virtual void OnPlacePresize(Point pt, TileIndex tile)
	{
		DoCommand(tile, 0x200 | RoadTypeToRoadTypes(_cur_roadtype), 0, DC_AUTO, CMD_BUILD_TUNNEL);
		VpSetPresizeRange(tile, _build_tunnel_endtile == 0 ? tile : _build_tunnel_endtile);
	}

	virtual EventState OnCTRLStateChange()
	{
		if (RoadToolbar_CtrlChanged(this)) return ES_HANDLED;
		return ES_NOT_HANDLED;
	}
};

static const NWidgetPart _nested_build_road_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, RTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, RTW_CAPTION), SetDataTip(STR_ROAD_TOOLBAR_ROAD_CONSTRUCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, RTW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_X),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_Y),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_AUTOROAD),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOROAD, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEMOLISH),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEPOT),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_DEPOT, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_VEHICLE_DEPOT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUS_STATION),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUS_STATION, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_BUS_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_TRUCK_STATION),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRUCK_BAY, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRUCK_LOADING_BAY),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ONE_WAY),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_ONE_WAY, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_ONE_WAY_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_BRIDGE),
						SetFill(false, true), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_TUNNEL),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_REMOVE),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_ROAD),
	EndContainer(),
};

static const WindowDesc _build_road_desc(
	WDP_ALIGN_TBR, 22, 263, 36, 263, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	NULL, _nested_build_road_widgets, lengthof(_nested_build_road_widgets)
);

static const NWidgetPart _nested_build_tramway_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, RTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, RTW_CAPTION), SetDataTip(STR_ROAD_TOOLBAR_TRAM_CONSTRUCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, RTW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_X),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_Y),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRAMWAY_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_AUTOROAD),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOTRAM, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOTRAM),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEMOLISH),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEPOT),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_DEPOT, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAM_VEHICLE_DEPOT),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUS_STATION),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUS_STATION, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_PASSENGER_TRAM_STATION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_TRUCK_STATION),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_TRUCK_BAY, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_CARGO_TRAM_STATION),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, RTW_ONE_WAY), // Stub so we don't have to litter the GUI code with checks whether it exists
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_BRIDGE),
						SetFill(false, true), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_TUNNEL),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_TRAMWAY_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_REMOVE),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_TRAMWAYS),
	EndContainer(),
};

static const WindowDesc _build_tramway_desc(
	WDP_ALIGN_TBR, 22, 241, 36, 241, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	NULL, _nested_build_tramway_widgets, lengthof(_nested_build_tramway_widgets)
);

void ShowBuildRoadToolbar(RoadType roadtype)
{
	if (!Company::IsValidID(_local_company)) return;
	_cur_roadtype = roadtype;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	AllocateWindowDescFront<BuildRoadToolbarWindow>(roadtype == ROADTYPE_ROAD ? &_build_road_desc : &_build_tramway_desc, TRANSPORT_ROAD);
}

static const NWidgetPart _nested_build_road_scen_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, RTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, RTW_CAPTION), SetDataTip(STR_ROAD_TOOLBAR_ROAD_CONSTRUCTION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, RTW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_X),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_X_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ROAD_Y),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_Y_DIR, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_SECTION),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_AUTOROAD),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AUTOROAD, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_AUTOROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_DEMOLISH),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_ONE_WAY),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_ONE_WAY, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_ONE_WAY_ROAD),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_BRIDGE),
						SetFill(false, true), SetMinimalSize(43, 22), SetDataTip(SPR_IMG_BRIDGE, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_BRIDGE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_BUILD_TUNNEL),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_ROAD_TUNNEL, STR_ROAD_TOOLBAR_TOOLTIP_BUILD_ROAD_TUNNEL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, RTW_REMOVE),
						SetFill(false, true), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_REMOVE, STR_ROAD_TOOLBAR_TOOLTIP_TOGGLE_BUILD_REMOVE_FOR_ROAD),
	EndContainer(),
};

static const WindowDesc _build_road_scen_desc(
	WDP_AUTO, WDP_AUTO, 197, 36, 197, 36,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	NULL, _nested_build_road_scen_widgets, lengthof(_nested_build_road_scen_widgets)
);

void ShowBuildRoadScenToolbar()
{
	_cur_roadtype = ROADTYPE_ROAD;
	AllocateWindowDescFront<BuildRoadToolbarWindow>(&_build_road_scen_desc, 0);
}

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

struct BuildRoadDepotWindow : public PickerWindowBase {
	BuildRoadDepotWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(parent)
	{
		this->CreateNestedTree(desc);

		this->LowerWidget(_road_depot_orientation + BRDW_DEPOT_NE);
		if ( _cur_roadtype == ROADTYPE_TRAM) {
			this->nested_array[BRDW_CAPTION]->widget_data = STR_BUILD_DEPOT_TRAM_ORIENTATION_CAPTION;
			for (int i = BRDW_DEPOT_NE; i <= BRDW_DEPOT_NW; i++) this->nested_array[i]->tool_tip = STR_BUILD_DEPOT_TRAM_ORIENTATION_SELECT_TOOLTIP;
		}

		this->FinishInitNested(desc, TRANSPORT_ROAD);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (!IsInsideMM(widget, BRDW_DEPOT_NE, BRDW_DEPOT_NW + 1)) return;

		DrawRoadDepotSprite(r.left - 1, r.top, (DiagDirection)(widget - BRDW_DEPOT_NE + DIAGDIR_NE), _cur_roadtype);
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

static const NWidgetPart _nested_build_road_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, BRDW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, BRDW_CAPTION), SetMinimalSize(129, 14), SetDataTip(STR_BUILD_DEPOT_ROAD_ORIENTATION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BRDW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL_LTR),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
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
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
	EndContainer(),
};

static const WindowDesc _build_road_depot_desc(
	WDP_AUTO, WDP_AUTO, 140, 122, 140, 122,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	NULL, _nested_build_road_depot_widgets, lengthof(_nested_build_road_depot_widgets)
);

static void ShowRoadDepotPicker(Window *parent)
{
	new BuildRoadDepotWindow(&_build_road_depot_desc, parent);
}

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

		this->nested_array[BRSW_CAPTION]->widget_data = _road_type_infos[_cur_roadtype].picker_title[rs];
		for (uint i = BRSW_STATION_NE; i < BRSW_LT_OFF; i++) this->nested_array[i]->tool_tip = _road_type_infos[_cur_roadtype].picker_tooltip[rs];

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
		int top = this->nested_array[BRSW_LT_ON]->pos_y + this->nested_array[BRSW_LT_ON]->current_y + WD_PAR_VSEP_NORMAL;
		NWidgetCore *back_nwi = this->nested_array[BRSW_BACKGROUND];
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
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, BRSW_CLOSEBOX),
		NWidget(WWT_CAPTION,  COLOUR_DARK_GREEN, BRSW_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BRSW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL), SetPIP(3, 2, 2),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_NW), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_NE), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_X),  SetMinimalSize(66, 50), EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL), SetPIP(3, 2, 2),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_SW), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_SE), SetMinimalSize(66, 50), EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, BRSW_STATION_Y),  SetMinimalSize(66, 50), EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BRSW_INFO), SetMinimalSize(140, 14), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL),
			NWidget(NWID_SPACER), SetFill(true, false),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BRSW_LT_OFF), SetMinimalSize(60, 12), SetPadding(0, 0, 0, 10),
											SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BRSW_LT_ON), SetMinimalSize(60, 12),
											SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
			NWidget(NWID_SPACER), SetFill(true, false),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 10), SetResize(0, 1),
	EndContainer(),
};

static const WindowDesc _rv_station_picker_desc(
	WDP_AUTO, WDP_AUTO, 207, 178, 207, 178,
	WC_BUS_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	NULL, _nested_rv_station_picker_widgets, lengthof(_nested_rv_station_picker_widgets)
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
