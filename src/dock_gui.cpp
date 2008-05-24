/* $Id$ */

/** @file dock_gui.cpp GUI to create amazing water objects. */

#include "stdafx.h"
#include "openttd.h"
#include "tile_map.h"
#include "station_type.h"
#include "gui.h"
#include "terraform_gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "command_func.h"
#include "settings_type.h"
#include "water.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "player_func.h"
#include "slope_func.h"
#include "tilehighlight_func.h"

#include "table/sprites.h"
#include "table/strings.h"

static void ShowBuildDockStationPicker(Window *parent);
static void ShowBuildDocksDepotPicker(Window *parent);

static Axis _ship_depot_direction;

void CcBuildDocks(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_02_SPLAT, tile);
		ResetObjectToPlace();
	}
}

void CcBuildCanal(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_02_SPLAT, tile);
}


static void PlaceDocks_Dock(TileIndex tile)
{
	DoCommandP(tile, _ctrl_pressed, 0, CcBuildDocks, CMD_BUILD_DOCK | CMD_MSG(STR_9802_CAN_T_BUILD_DOCK_HERE));
}

static void PlaceDocks_Depot(TileIndex tile)
{
	DoCommandP(tile, _ship_depot_direction, 0, CcBuildDocks, CMD_BUILD_SHIP_DEPOT | CMD_MSG(STR_3802_CAN_T_BUILD_SHIP_DEPOT));
}

static void PlaceDocks_Buoy(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CcBuildDocks, CMD_BUILD_BUOY | CMD_MSG(STR_9835_CAN_T_POSITION_BUOY_HERE));
}

static void PlaceDocks_BuildCanal(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_CREATE_WATER);
}

static void PlaceDocks_BuildLock(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CcBuildDocks, CMD_BUILD_LOCK | CMD_MSG(STR_CANT_BUILD_LOCKS));
}

/** Enum referring to the widgets of the build dock toolbar */
enum DockToolbarWidgets {
	DTW_BEGIN = 0,                 ///< Start of toolbar widgets
	DTW_CLOSEBOX = DTW_BEGIN,      ///< Close window button
	DTW_CAPTION,                   ///< Window caption
	DTW_STICKY,                    ///< Sticky window button
	DTW_BUTTONS_BEGIN,             ///< Begin of clickable buttons (except seperating panel)
	DTW_CANAL = DTW_BUTTONS_BEGIN, ///< Build canal button
	DTW_LOCK,                      ///< Build lock button
	DTW_SEPERATOR,                 ///< Seperating panel between lock and demolish
	DTW_DEMOLISH,                  ///< Demolish aka dynamite button
	DTW_DEPOT,                     ///< Build depot button
	DTW_STATION,                   ///< Build station button
	DTW_BUOY,                      ///< Build buoy button
	DTW_END,                       ///< End of toolbar widgets
};


static void BuildDocksClick_Canal(Window *w)
{
	HandlePlacePushButton(w, DTW_CANAL, SPR_CURSOR_CANAL, VHM_RECT, PlaceDocks_BuildCanal);
}

static void BuildDocksClick_Lock(Window *w)
{
	HandlePlacePushButton(w, DTW_LOCK, SPR_CURSOR_LOCK, VHM_RECT, PlaceDocks_BuildLock);
}

static void BuildDocksClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, DTW_DEMOLISH, ANIMCURSOR_DEMOLISH, VHM_RECT, PlaceProc_DemolishArea);
}

static void BuildDocksClick_Depot(Window *w)
{
	if (!CanBuildVehicleInfrastructure(VEH_SHIP)) return;
	if (HandlePlacePushButton(w, DTW_DEPOT, SPR_CURSOR_SHIP_DEPOT, VHM_RECT, PlaceDocks_Depot)) ShowBuildDocksDepotPicker(w);
}

static void BuildDocksClick_Dock(Window *w)
{
	if (!CanBuildVehicleInfrastructure(VEH_SHIP)) return;
	if (HandlePlacePushButton(w, DTW_STATION, SPR_CURSOR_DOCK, VHM_SPECIAL, PlaceDocks_Dock)) ShowBuildDockStationPicker(w);
}

static void BuildDocksClick_Buoy(Window *w)
{
	if (!CanBuildVehicleInfrastructure(VEH_SHIP)) return;
	HandlePlacePushButton(w, DTW_BUOY, SPR_CURSOR_BOUY, VHM_RECT, PlaceDocks_Buoy);
}


typedef void OnButtonClick(Window *w);
static OnButtonClick * const _build_docks_button_proc[] = {
	BuildDocksClick_Canal,
	BuildDocksClick_Lock,
	NULL,
	BuildDocksClick_Demolish,
	BuildDocksClick_Depot,
	BuildDocksClick_Dock,
	BuildDocksClick_Buoy
};

struct BuildDocksToolbarWindow : Window {
	BuildDocksToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
		if (_patches.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildDocksToolbarWindow()
	{
		if (_patches.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0);
	}

	virtual void OnPaint()
	{
		this->SetWidgetsDisabledState(!CanBuildVehicleInfrastructure(VEH_SHIP), DTW_DEPOT, DTW_STATION, DTW_BUOY, WIDGET_LIST_END);
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= DTW_BUTTONS_BEGIN && widget != DTW_SEPERATOR) _build_docks_button_proc[widget - DTW_BUTTONS_BEGIN](this);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case '1': BuildDocksClick_Canal(this); break;
			case '2': BuildDocksClick_Lock(this); break;
			case '3': BuildDocksClick_Demolish(this); break;
			case '4': BuildDocksClick_Depot(this); break;
			case '5': BuildDocksClick_Dock(this); break;
			case '6': BuildDocksClick_Buoy(this); break;
			default:  return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
	}

	virtual void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt)
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	virtual void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile)
	{
		if (pt.x != -1) {
			switch (select_proc) {
				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
				case DDSP_CREATE_WATER:
					DoCommandP(end_tile, start_tile, 0, CcBuildCanal, CMD_BUILD_CANAL | CMD_MSG(STR_CANT_BUILD_CANALS));
					break;

				default: break;
			}
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();

		delete FindWindowById(WC_BUILD_STATION, 0);
		delete FindWindowById(WC_BUILD_DEPOT, 0);
	}

	virtual void OnPlacePresize(Point pt, TileIndex tile_from)
	{
		DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile_from, NULL));
		TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile_from, ReverseDiagDir(dir)) : tile_from);

		VpSetPresizeRange(tile_from, tile_to);
	}
};

static const Widget _build_docks_toolb_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},                   // DTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   123,     0,    13, STR_9801_DOCK_CONSTRUCTION, STR_018C_WINDOW_TITLE_DRAG_THIS},         // DTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   124,   135,     0,    13, 0x0,                        STR_STICKY_BUTTON},                       // DTW_STICKY
{     WWT_IMGBTN,   RESIZE_NONE,     7,     0,    21,    14,    35, SPR_IMG_BUILD_CANAL,        STR_BUILD_CANALS_TIP},                    // DTW_CANAL
{     WWT_IMGBTN,   RESIZE_NONE,     7,    22,    43,    14,    35, SPR_IMG_BUILD_LOCK,         STR_BUILD_LOCKS_TIP},                     // DTW_LOCK

{      WWT_PANEL,   RESIZE_NONE,     7,    44,    47,    14,    35, 0x0,                        STR_NULL},                                // DTW_SEPERATOR

{     WWT_IMGBTN,   RESIZE_NONE,     7,    48,    69,    14,    35, SPR_IMG_DYNAMITE,           STR_018D_DEMOLISH_BUILDINGS_ETC},         // DTW_DEMOLISH
{     WWT_IMGBTN,   RESIZE_NONE,     7,    70,    91,    14,    35, SPR_IMG_SHIP_DEPOT,         STR_981E_BUILD_SHIP_DEPOT_FOR_BUILDING},  // DTW_DEPOT
{     WWT_IMGBTN,   RESIZE_NONE,     7,    92,   113,    14,    35, SPR_IMG_SHIP_DOCK,          STR_981D_BUILD_SHIP_DOCK},                // DTW_STATION
{     WWT_IMGBTN,   RESIZE_NONE,     7,   114,   135,    14,    35, SPR_IMG_BOUY,               STR_9834_POSITION_BUOY_WHICH_CAN},        // DTW_BUOY
{   WIDGETS_END},
};

static const WindowDesc _build_docks_toolbar_desc = {
	WDP_ALIGN_TBR, 22, 136, 36, 136, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_build_docks_toolb_widgets,
};

void ShowBuildDocksToolbar()
{
	if (!IsValidPlayer(_current_player)) return;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_toolbar_desc, TRANSPORT_WATER);
}

struct BuildDocksStationWindow : public PickerWindowBase {
private:
	enum BuildDockStationWidgets {
		BDSW_CLOSE,
		BDSW_CAPTION,
		BDSW_BACKGROUND,
		BDSW_LT_OFF,
		BDSW_LT_ON,
		BDSW_INFO,
	};

public:
	BuildDocksStationWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->LowerWidget(_station_show_coverage + BDSW_LT_OFF);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		int rad = (_patches.modified_catchment) ? CA_DOCK : CA_UNMODIFIED;

		this->DrawWidgets();

		if (_station_show_coverage) {
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		int text_end = DrawStationCoverageAreaText(4, 50, SCT_ALL, rad, false);
		text_end = DrawStationCoverageAreaText(4, text_end + 4, SCT_ALL, rad, true) + 4;
		if (text_end != this->widget[BDSW_BACKGROUND].bottom) {
			this->SetDirty();
			ResizeWindowForWidget(this, 2, 0, text_end - this->widget[BDSW_BACKGROUND].bottom);
			this->SetDirty();
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BDSW_LT_OFF:
			case BDSW_LT_ON:
				this->RaiseWidget(_station_show_coverage + BDSW_LT_OFF);
				_station_show_coverage = (widget != BDSW_LT_OFF);
				this->LowerWidget(_station_show_coverage + BDSW_LT_OFF);
				SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				break;
		}
	}

	virtual void OnTick()
	{
		CheckRedrawStationCoverage(this);
	}
};

static const Widget _build_dock_station_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                         STR_018B_CLOSE_WINDOW},             // BDSW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_3068_DOCK,                    STR_018C_WINDOW_TITLE_DRAG_THIS},   // BDSW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   147,    14,    74, 0x0,                              STR_NULL},                          // BDSW_BACKGROUND
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    14,    73,    30,    40, STR_02DB_OFF,                     STR_3065_DON_T_HIGHLIGHT_COVERAGE}, // BDSW_LT_OFF
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    74,   133,    30,    40, STR_02DA_ON,                      STR_3064_HIGHLIGHT_COVERAGE_AREA},  // BDSW_LT_ON
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,    17,    30, STR_3066_COVERAGE_AREA_HIGHLIGHT, STR_NULL},                          // BDSW_INFO
{   WIDGETS_END},
};

static const WindowDesc _build_dock_station_desc = {
	WDP_AUTO, WDP_AUTO, 148, 75, 148, 75,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_dock_station_widgets,
};

static void ShowBuildDockStationPicker(Window *parent)
{
	new BuildDocksStationWindow(&_build_dock_station_desc, parent);
}

struct BuildDocksDepotWindow : public PickerWindowBase {
private:
	enum BuildDockDepotWidgets {
		BDDW_CLOSE,
		BDDW_CAPTION,
		BDDW_BACKGROUND,
		BDDW_X,
		BDDW_Y,
	};

	static void UpdateDocksDirection()
	{
		if (_ship_depot_direction != AXIS_X) {
			SetTileSelectSize(1, 2);
		} else {
			SetTileSelectSize(2, 1);
		}
	}

public:
	BuildDocksDepotWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->LowerWidget(_ship_depot_direction + BDDW_X);
		UpdateDocksDirection();
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		DrawShipDepotSprite(67, 35, 0);
		DrawShipDepotSprite(35, 51, 1);
		DrawShipDepotSprite(135, 35, 2);
		DrawShipDepotSprite(167, 51, 3);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case BDDW_X:
			case BDDW_Y:
				this->RaiseWidget(_ship_depot_direction + BDDW_X);
				_ship_depot_direction = (widget == BDDW_X ? AXIS_X : AXIS_Y);
				this->LowerWidget(_ship_depot_direction + BDDW_X);
				SndPlayFx(SND_15_BEEP);
				UpdateDocksDirection();
				this->SetDirty();
				break;
		}
	}
};

static const Widget _build_docks_depot_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},                  // BDDW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   203,     0,    13, STR_3800_SHIP_DEPOT_ORIENTATION, STR_018C_WINDOW_TITLE_DRAG_THIS},        // BDDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   203,    14,    85, 0x0,                             STR_NULL},                               // BDDW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,    14,     3,   100,    17,    82, 0x0,                             STR_3803_SELECT_SHIP_DEPOT_ORIENTATION}, // BDDW_X
{      WWT_PANEL,   RESIZE_NONE,    14,   103,   200,    17,    82, 0x0,                             STR_3803_SELECT_SHIP_DEPOT_ORIENTATION}, // BDDW_Y
{   WIDGETS_END},
};

static const WindowDesc _build_docks_depot_desc = {
	WDP_AUTO, WDP_AUTO, 204, 86, 204, 86,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_build_docks_depot_widgets,
};


static void ShowBuildDocksDepotPicker(Window *parent)
{
	new BuildDocksDepotWindow(&_build_docks_depot_desc, parent);
}


void InitializeDockGui()
{
	_ship_depot_direction = AXIS_X;
}
