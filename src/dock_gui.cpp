/* $Id$ */

/** @file dock_gui.cpp GUI to create amazing water objects. */

#include "stdafx.h"
#include "openttd.h"
#include "tile_map.h"
#include "station_type.h"
#include "terraform_gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "command_func.h"
#include "water.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "company_func.h"
#include "slope_func.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "settings_type.h"

#include "table/sprites.h"
#include "table/strings.h"

static void ShowBuildDockStationPicker(Window *parent);
static void ShowBuildDocksDepotPicker(Window *parent);

static Axis _ship_depot_direction;

void CcBuildDocks(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_02_SPLAT, tile);
		if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	}
}

void CcBuildCanal(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_02_SPLAT, tile);
}


static void PlaceDocks_Dock(TileIndex tile)
{
	uint32 p2 = (uint32)INVALID_STATION << 16; // no station to join

	/* tile is always the land tile, so need to evaluate _thd.pos */
	CommandContainer cmdcont = { tile, _ctrl_pressed, p2, CMD_BUILD_DOCK | CMD_MSG(STR_ERROR_CAN_T_BUILD_DOCK_HERE), CcBuildDocks, "" };
	ShowSelectStationIfNeeded(cmdcont, _thd.size.x / TILE_SIZE, _thd.size.y / TILE_SIZE);
}

static void PlaceDocks_Depot(TileIndex tile)
{
	DoCommandP(tile, _ship_depot_direction, 0, CMD_BUILD_SHIP_DEPOT | CMD_MSG(STR_ERROR_CAN_T_BUILD_SHIP_DEPOT), CcBuildDocks);
}

static void PlaceDocks_Buoy(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CMD_BUILD_BUOY | CMD_MSG(STR_ERROR_CAN_T_POSITION_BUOY_HERE), CcBuildDocks);
}

static void PlaceDocks_BuildCanal(TileIndex tile)
{
	VpStartPlaceSizing(tile, (_game_mode == GM_EDITOR) ? VPM_X_AND_Y : VPM_X_OR_Y, DDSP_CREATE_WATER);
}

static void PlaceDocks_BuildLock(TileIndex tile)
{
	DoCommandP(tile, 0, 0, CMD_BUILD_LOCK | CMD_MSG(STR_CANT_BUILD_LOCKS), CcBuildDocks);
}

static void PlaceDocks_BuildRiver(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_RIVER);
}

static void PlaceDocks_Aqueduct(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_OR_Y, DDSP_BUILD_BRIDGE);
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
	DTW_RIVER,                     ///< Build river button (in scenario editor)
	DTW_BUILD_AQUEDUCT,            ///< Build aqueduct button
	DTW_END,                       ///< End of toolbar widgets
};


static void BuildDocksClick_Canal(Window *w)
{

	HandlePlacePushButton(w, DTW_CANAL, SPR_CURSOR_CANAL, HT_RECT, PlaceDocks_BuildCanal);
}

static void BuildDocksClick_Lock(Window *w)
{
	HandlePlacePushButton(w, DTW_LOCK, SPR_CURSOR_LOCK, HT_RECT, PlaceDocks_BuildLock);
}

static void BuildDocksClick_Demolish(Window *w)
{
	HandlePlacePushButton(w, DTW_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT, PlaceProc_DemolishArea);
}

static void BuildDocksClick_Depot(Window *w)
{
	if (!CanBuildVehicleInfrastructure(VEH_SHIP)) return;
	if (HandlePlacePushButton(w, DTW_DEPOT, SPR_CURSOR_SHIP_DEPOT, HT_RECT, PlaceDocks_Depot)) ShowBuildDocksDepotPicker(w);
}

static void BuildDocksClick_Dock(Window *w)
{
	if (!CanBuildVehicleInfrastructure(VEH_SHIP)) return;
	if (HandlePlacePushButton(w, DTW_STATION, SPR_CURSOR_DOCK, HT_SPECIAL, PlaceDocks_Dock)) ShowBuildDockStationPicker(w);
}

static void BuildDocksClick_Buoy(Window *w)
{
	if (!CanBuildVehicleInfrastructure(VEH_SHIP)) return;
	HandlePlacePushButton(w, DTW_BUOY, SPR_CURSOR_BOUY, HT_RECT, PlaceDocks_Buoy);
}

static void BuildDocksClick_River(Window *w)
{
	if (_game_mode != GM_EDITOR) return;
	HandlePlacePushButton(w, DTW_RIVER, SPR_CURSOR_RIVER, HT_RECT, PlaceDocks_BuildRiver);
}

static void BuildDocksClick_Aqueduct(Window *w)
{
	HandlePlacePushButton(w, DTW_BUILD_AQUEDUCT, SPR_CURSOR_AQUEDUCT, HT_RECT, PlaceDocks_Aqueduct);
}


typedef void OnButtonClick(Window *w);
static OnButtonClick * const _build_docks_button_proc[] = {
	BuildDocksClick_Canal,
	BuildDocksClick_Lock,
	NULL,
	BuildDocksClick_Demolish,
	BuildDocksClick_Depot,
	BuildDocksClick_Dock,
	BuildDocksClick_Buoy,
	BuildDocksClick_River,
	BuildDocksClick_Aqueduct
};

struct BuildDocksToolbarWindow : Window {
	BuildDocksToolbarWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildDocksToolbarWindow()
	{
		if (_settings_client.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0, false);
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
			case '7': BuildDocksClick_River(this); break;
			case 'B':
			case '8': BuildDocksClick_Aqueduct(this); break;
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
				case DDSP_BUILD_BRIDGE:
					if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
					extern void CcBuildBridge(bool success, TileIndex tile, uint32 p1, uint32 p2);
					DoCommandP(end_tile, start_tile, TRANSPORT_WATER << 15, CMD_BUILD_BRIDGE | CMD_MSG(STR_CAN_T_BUILD_AQUEDUCT_HERE), CcBuildBridge);

				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
				case DDSP_CREATE_WATER:
					DoCommandP(end_tile, start_tile, (_game_mode == GM_EDITOR ? _ctrl_pressed : 0), CMD_BUILD_CANAL | CMD_MSG(STR_CANT_BUILD_CANALS), CcBuildCanal);
					break;
				case DDSP_CREATE_RIVER:
					DoCommandP(end_tile, start_tile, 2, CMD_BUILD_CANAL | CMD_MSG(STR_CANT_PLACE_RIVERS), CcBuildCanal);
					break;

				default: break;
			}
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();

		DeleteWindowById(WC_BUILD_STATION, 0);
		DeleteWindowById(WC_BUILD_DEPOT, 0);
		DeleteWindowById(WC_SELECT_STATION, 0);
		DeleteWindowById(WC_BUILD_BRIDGE, 0);
	}

	virtual void OnPlacePresize(Point pt, TileIndex tile_from)
	{
		DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile_from, NULL));
		TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile_from, ReverseDiagDir(dir)) : tile_from);

		VpSetPresizeRange(tile_from, tile_to);
	}
};

static const Widget _build_docks_toolb_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,    10,     0,    13, STR_BLACK_CROSS,               STR_TOOLTIP_CLOSE_WINDOW},                  // DTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_DARK_GREEN,    11,   147,     0,    13, STR_WATERWAYS_TOOLBAR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},        // DTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,   148,   159,     0,    13, 0x0,                           STR_STICKY_BUTTON},                         // DTW_STICKY
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,    21,    14,    35, SPR_IMG_BUILD_CANAL,           STR_BUILD_CANALS_TIP},                      // DTW_CANAL
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    22,    43,    14,    35, SPR_IMG_BUILD_LOCK,            STR_BUILD_LOCKS_TIP},                       // DTW_LOCK

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_DARK_GREEN,    44,    48,    14,    35, 0x0,                           STR_NULL},                                  // DTW_SEPERATOR

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    49,    70,    14,    35, SPR_IMG_DYNAMITE,              STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC},        // DTW_DEMOLISH
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    71,    92,    14,    35, SPR_IMG_SHIP_DEPOT,            STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP}, // DTW_DEPOT
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    93,   114,    14,    35, SPR_IMG_SHIP_DOCK,             STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP},  // DTW_STATION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,   115,   136,    14,    35, SPR_IMG_BOUY,                  STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP},        // DTW_BUOY
{     WWT_EMPTY,    RESIZE_NONE,  COLOUR_DARK_GREEN,     0,     0,     0,     0, 0x0,                           STR_NULL},                                  // DTW_RIVER
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,   137,   159,    14,    35, SPR_IMG_AQUEDUCT,              STR_BUILD_AQUEDUCT},                        // DTW_BUILD_AQUEDUCT
{   WIDGETS_END},
};

/**
 * Nested widget parts of docks toolbar, game version.
 * Position of #DTW_RIVER widget has changed.
 */
static const NWidgetPart _nested_build_docks_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, DTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, DTW_CAPTION), SetMinimalSize(80, 14), SetFill(1, 0), SetDataTip(STR_WATERWAYS_TOOLBAR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, DTW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL_LTR),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_CANAL), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUILD_CANAL, STR_BUILD_CANALS_TIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_LOCK), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUILD_LOCK, STR_BUILD_LOCKS_TIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, DTW_SEPERATOR), SetMinimalSize(5, 22), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_DEMOLISH), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_DEPOT), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SHIP_DEPOT, STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_STATION), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_SHIP_DOCK, STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_BUOY), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BOUY, STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, DTW_RIVER), SetMinimalSize(0, 0),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_BUILD_AQUEDUCT), SetMinimalSize(23, 22), SetDataTip(SPR_IMG_AQUEDUCT, STR_BUILD_AQUEDUCT),
	EndContainer(),
};

static const WindowDesc _build_docks_toolbar_desc(
	WDP_ALIGN_TBR, 22, 160, 36, 160, 36,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	_build_docks_toolb_widgets, _nested_build_docks_toolbar_widgets, lengthof(_nested_build_docks_toolbar_widgets)
);

void ShowBuildDocksToolbar()
{
	if (!IsValidCompanyID(_local_company)) return;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_toolbar_desc, TRANSPORT_WATER);
}

/** Widget definition for the build docks in scenario editor window. */
static const Widget _build_docks_scen_toolb_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,    10,     0,    13, STR_BLACK_CROSS,                  STR_TOOLTIP_CLOSE_WINDOW},           // DTW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_DARK_GREEN,    11,   102,     0,    13, STR_WATERWAYS_TOOLBAR_CAPTION_SE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS}, // DTW_CAPTION
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,   103,   114,     0,    13, 0x0,                              STR_STICKY_BUTTON},                  // DTW_STICKY
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,    21,    14,    35, SPR_IMG_BUILD_CANAL,              STR_CREATE_LAKE},                    // DTW_CANAL
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    22,    43,    14,    35, SPR_IMG_BUILD_LOCK,               STR_BUILD_LOCKS_TIP},                // DTW_LOCK

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_DARK_GREEN,    44,    48,    14,    35, 0x0,                              STR_NULL},                           // DTW_SEPERATOR

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    49,    70,    14,    35, SPR_IMG_DYNAMITE,                 STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC}, // DTW_DEMOLISH
{     WWT_EMPTY,    RESIZE_NONE,  COLOUR_DARK_GREEN,     0,     0,     0,     0, 0x0,                              STR_NULL},                           // DTW_DEPOT
{     WWT_EMPTY,    RESIZE_NONE,  COLOUR_DARK_GREEN,     0,     0,     0,     0, 0x0,                              STR_NULL},                           // DTW_STATION
{     WWT_EMPTY,    RESIZE_NONE,  COLOUR_DARK_GREEN,     0,     0,     0,     0, 0x0,                              STR_NULL},                           // DTW_BUOY
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    71,    92,    14,    35, SPR_IMG_BUILD_RIVER,              STR_CREATE_RIVER},                   // DTW_RIVER
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_DARK_GREEN,    93,   114,    14,    35, SPR_IMG_AQUEDUCT,                 STR_BUILD_AQUEDUCT},                 // DTW_BUILD_AQUEDUCT
{   WIDGETS_END},
};

/**
 * Nested widget parts of docks toolbar, scenario editor version.
 * Positions of #DTW_DEPOT, #DTW_STATION, and #DTW_BUOY widgets have changed.
 */
static const NWidgetPart _nested_build_docks_scen_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, DTW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, DTW_CAPTION), SetMinimalSize(80, 14), SetFill(1, 0), SetDataTip(STR_WATERWAYS_TOOLBAR_CAPTION_SE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN, DTW_STICKY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_CANAL), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUILD_CANAL, STR_CREATE_LAKE),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_LOCK), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUILD_LOCK, STR_BUILD_LOCKS_TIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, DTW_SEPERATOR), SetMinimalSize(5, 22), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_DEMOLISH), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, DTW_DEPOT), SetMinimalSize(0, 0),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, DTW_STATION), SetMinimalSize(0, 0),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, DTW_BUOY), SetMinimalSize(0, 0),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_RIVER), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_BUILD_RIVER, STR_CREATE_RIVER),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, DTW_BUILD_AQUEDUCT), SetMinimalSize(22, 22), SetDataTip(SPR_IMG_AQUEDUCT, STR_BUILD_AQUEDUCT),
	EndContainer(),
};

/** Window definition for the build docks in scenario editor window. */
static const WindowDesc _build_docks_scen_toolbar_desc(
	WDP_AUTO, WDP_AUTO, 115, 36, 115, 36,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_CONSTRUCTION,
	_build_docks_scen_toolb_widgets, _nested_build_docks_scen_toolbar_widgets, lengthof(_nested_build_docks_scen_toolbar_widgets)
);

void ShowBuildDocksScenToolbar()
{
	AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_scen_toolbar_desc, TRANSPORT_WATER);
}

/** Widget numbers of the build-dock GUI. */
enum BuildDockStationWidgets {
	BDSW_CLOSE,      ///< Closebox.
	BDSW_CAPTION,    ///< Titlebar.
	BDSW_BACKGROUND, ///< Background panel.
	BDSW_LT_OFF,     ///< 'Off' button of coverage high light.
	BDSW_LT_ON,      ///< 'On' button of coverage high light.
	BDSW_INFO,       ///< 'Coverage highlight' label.
};

struct BuildDocksStationWindow : public PickerWindowBase {
public:
	BuildDocksStationWindow(const WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->LowerWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual ~BuildDocksStationWindow()
	{
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	virtual void OnPaint()
	{
		int rad = (_settings_game.station.modified_catchment) ? CA_DOCK : CA_UNMODIFIED;

		this->DrawWidgets();

		if (_settings_client.gui.station_show_coverage) {
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
				this->RaiseWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
				_settings_client.gui.station_show_coverage = (widget != BDSW_LT_OFF);
				this->LowerWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,    10,     0,    13, STR_BLACK_CROSS,                       STR_TOOLTIP_CLOSE_WINDOW},                    // BDSW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_DARK_GREEN,    11,   147,     0,    13, STR_STATION_BUILD_DOCK_CAPTION,        STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},          // BDSW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,   147,    14,    74, 0x0,                                   STR_NULL},                                    // BDSW_BACKGROUND
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,          14,    73,    31,    42, STR_STATION_BUILD_COVERAGE_OFF,        STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP}, // BDSW_LT_OFF
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,          74,   133,    31,    42, STR_STATION_BUILD_COVERAGE_ON,         STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP},  // BDSW_LT_ON
{      WWT_LABEL,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,   147,    17,    30, STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL},                                    // BDSW_INFO
{   WIDGETS_END},
};

/** Nested widget parts of a build dock station window. */
static const NWidgetPart _nested_build_dock_station_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, BDSW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, BDSW_CAPTION), SetDataTip(STR_STATION_BUILD_DOCK_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BDSW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BDSW_INFO), SetMinimalSize(148, 14), SetDataTip( STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL),
		NWidget(NWID_HORIZONTAL), SetPIP(14, 0, 14),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BDSW_LT_OFF), SetMinimalSize(40, 12), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BDSW_LT_ON), SetMinimalSize(40, 12), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 32),
	EndContainer(),
};

static const WindowDesc _build_dock_station_desc(
	WDP_AUTO, WDP_AUTO, 148, 75, 148, 75,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	_build_dock_station_widgets, _nested_build_dock_station_widgets, lengthof(_nested_build_dock_station_widgets)
);

static void ShowBuildDockStationPicker(Window *parent)
{
	new BuildDocksStationWindow(&_build_dock_station_desc, parent);
}

/** Widgets for the build ship depot window. */
enum BuildDockDepotWidgets {
	BDDW_CLOSE,
	BDDW_CAPTION,
	BDDW_BACKGROUND,
	BDDW_X,
	BDDW_Y,
};

struct BuildDocksDepotWindow : public PickerWindowBase {
private:
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,    10,     0,    13, STR_BLACK_CROSS,              STR_TOOLTIP_CLOSE_WINDOW},                 // BDDW_CLOSE
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_DARK_GREEN,    11,   203,     0,    13, STR_DEPOT_BUILD_SHIP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},       // BDDW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_DARK_GREEN,     0,   203,    14,    85, 0x0,                          STR_NULL},                                 // BDDW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,           3,   100,    17,    82, 0x0,                          STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP}, // BDDW_X
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,         103,   200,    17,    82, 0x0,                          STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP}, // BDDW_Y
{   WIDGETS_END},
};

static const NWidgetPart _nested_build_docks_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN, BDDW_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, BDDW_CAPTION), SetMinimalSize(193, 14), SetDataTip(STR_DEPOT_BUILD_SHIP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BDDW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL_LTR),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, BDDW_X), SetMinimalSize(98, 66), SetDataTip(0x0, STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, BDDW_Y), SetMinimalSize(98, 66), SetDataTip(0x0, STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
	EndContainer(),
};

static const WindowDesc _build_docks_depot_desc(
	WDP_AUTO, WDP_AUTO, 204, 86, 204, 86,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	_build_docks_depot_widgets, _nested_build_docks_depot_widgets, lengthof(_nested_build_docks_depot_widgets)
);


static void ShowBuildDocksDepotPicker(Window *parent)
{
	new BuildDocksDepotWindow(&_build_docks_depot_desc, parent);
}


void InitializeDockGui()
{
	_ship_depot_direction = AXIS_X;
}
