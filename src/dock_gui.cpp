/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dock_gui.cpp GUI to create amazing water objects. */

#include "stdafx.h"
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
#include "hotkeys.h"
#include "gui.h"
#include "zoom_func.h"
#include "tunnelbridge_cmd.h"
#include "dock_cmd.h"
#include "station_cmd.h"
#include "water_cmd.h"
#include "waypoint_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "widgets/dock_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

static void ShowBuildDockStationPicker(Window *parent);
static void ShowBuildDocksDepotPicker(Window *parent);

static Axis _ship_depot_direction;

void CcBuildDocks(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_02_CONSTRUCTION_WATER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

void CcPlaySound_CONSTRUCTION_WATER(Commands, const CommandCost &result, TileIndex tile)
{
	if (result.Succeeded() && _settings_client.sound.confirm) SndPlayTileFx(SND_02_CONSTRUCTION_WATER, tile);
}


/**
 * Gets the other end of the aqueduct, if possible.
 * @param      tile_from The begin tile for the aqueduct.
 * @param[out] tile_to   The tile till where to show a selection for the aqueduct.
 * @return The other end of the aqueduct, or otherwise a tile in line with the aqueduct to cause the right error message.
 */
static TileIndex GetOtherAqueductEnd(TileIndex tile_from, TileIndex *tile_to = nullptr)
{
	int z;
	DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile_from, &z));

	/* If the direction isn't right, just return the next tile so the command
	 * complains about the wrong slope instead of the ends not matching up.
	 * Make sure the coordinate is always a valid tile within the map, so we
	 * don't go "off" the map. That would cause the wrong error message. */
	if (!IsValidDiagDirection(dir)) return TILE_ADDXY(tile_from, TileX(tile_from) > 2 ? -1 : 1, 0);

	/* Direction the aqueduct is built to. */
	TileIndexDiff offset = TileOffsByDiagDir(ReverseDiagDir(dir));
	/* The maximum length of the aqueduct. */
	int max_length = std::min<int>(_settings_game.construction.max_bridge_length, DistanceFromEdgeDir(tile_from, ReverseDiagDir(dir)) - 1);

	TileIndex endtile = tile_from;
	for (int length = 0; IsValidTile(endtile) && TileX(endtile) != 0 && TileY(endtile) != 0; length++) {
		endtile = TILE_ADD(endtile, offset);

		if (length > max_length) break;

		if (GetTileMaxZ(endtile) > z) {
			if (tile_to != nullptr) *tile_to = endtile;
			break;
		}
	}

	return endtile;
}

/** Toolbar window for constructing water infrastructure. */
struct BuildDocksToolbarWindow : Window {
	DockToolbarWidgets last_clicked_widget; ///< Contains the last widget that has been clicked on this toolbar.

	BuildDocksToolbarWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->last_clicked_widget = WID_DT_INVALID;
		this->InitNested(window_number);
		this->OnInvalidateData();
		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (_game_mode == GM_NORMAL && this->IsWidgetLowered(WID_DT_STATION)) SetViewportCatchmentStation(nullptr, true);
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

		bool can_build = CanBuildVehicleInfrastructure(VEH_SHIP);
		this->SetWidgetsDisabledState(!can_build,
			WID_DT_DEPOT,
			WID_DT_STATION,
			WID_DT_BUOY);
		if (!can_build) {
			CloseWindowById(WC_BUILD_STATION, TRANSPORT_WATER);
			CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_WATER);
		}

		if (_game_mode != GM_EDITOR) {
			if (!can_build) {
				/* Show in the tooltip why this button is disabled. */
				this->GetWidget<NWidgetCore>(WID_DT_DEPOT)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_DT_STATION)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_DT_BUOY)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
			} else {
				this->GetWidget<NWidgetCore>(WID_DT_DEPOT)->SetToolTip(STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP);
				this->GetWidget<NWidgetCore>(WID_DT_STATION)->SetToolTip(STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP);
				this->GetWidget<NWidgetCore>(WID_DT_BUOY)->SetToolTip(STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP);
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_DT_CANAL: // Build canal button
				HandlePlacePushButton(this, WID_DT_CANAL, SPR_CURSOR_CANAL, HT_RECT);
				break;

			case WID_DT_LOCK: // Build lock button
				HandlePlacePushButton(this, WID_DT_LOCK, SPR_CURSOR_LOCK, HT_SPECIAL);
				break;

			case WID_DT_DEMOLISH: // Demolish aka dynamite button
				HandlePlacePushButton(this, WID_DT_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT | HT_DIAGONAL);
				break;

			case WID_DT_DEPOT: // Build depot button
				if (HandlePlacePushButton(this, WID_DT_DEPOT, SPR_CURSOR_SHIP_DEPOT, HT_RECT)) ShowBuildDocksDepotPicker(this);
				break;

			case WID_DT_STATION: // Build station button
				if (HandlePlacePushButton(this, WID_DT_STATION, SPR_CURSOR_DOCK, HT_SPECIAL)) ShowBuildDockStationPicker(this);
				break;

			case WID_DT_BUOY: // Build buoy button
				HandlePlacePushButton(this, WID_DT_BUOY, SPR_CURSOR_BUOY, HT_RECT);
				break;

			case WID_DT_RIVER: // Build river button (in scenario editor)
				if (_game_mode != GM_EDITOR) return;
				HandlePlacePushButton(this, WID_DT_RIVER, SPR_CURSOR_RIVER, HT_RECT | HT_DIAGONAL);
				break;

			case WID_DT_BUILD_AQUEDUCT: // Build aqueduct button
				HandlePlacePushButton(this, WID_DT_BUILD_AQUEDUCT, SPR_CURSOR_AQUEDUCT, HT_SPECIAL);
				break;

			default: return;
		}
		this->last_clicked_widget = (DockToolbarWidgets)widget;
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (this->last_clicked_widget) {
			case WID_DT_CANAL: // Build canal button
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_WATER);
				break;

			case WID_DT_LOCK: // Build lock button
				Command<CMD_BUILD_LOCK>::Post(STR_ERROR_CAN_T_BUILD_LOCKS, CcBuildDocks, tile);
				break;

			case WID_DT_DEMOLISH: // Demolish aka dynamite button
				PlaceProc_DemolishArea(tile);
				break;

			case WID_DT_DEPOT: // Build depot button
				Command<CMD_BUILD_SHIP_DEPOT>::Post(STR_ERROR_CAN_T_BUILD_SHIP_DEPOT, CcBuildDocks, tile, _ship_depot_direction);
				break;

			case WID_DT_STATION: { // Build station button
				/* Determine the watery part of the dock. */
				DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile));
				TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile, ReverseDiagDir(dir)) : tile);

				bool adjacent = _ctrl_pressed;
				auto proc = [=](bool test, StationID to_join) -> bool {
					if (test) {
						return Command<CMD_BUILD_DOCK>::Do(CommandFlagsToDCFlags(GetCommandFlags<CMD_BUILD_DOCK>()), tile, INVALID_STATION, adjacent).Succeeded();
					} else {
						return Command<CMD_BUILD_DOCK>::Post(STR_ERROR_CAN_T_BUILD_DOCK_HERE, CcBuildDocks, tile, to_join, adjacent);
					}
				};

				ShowSelectStationIfNeeded(TileArea(tile, tile_to), proc);
				break;
			}

			case WID_DT_BUOY: // Build buoy button
				Command<CMD_BUILD_BUOY>::Post(STR_ERROR_CAN_T_POSITION_BUOY_HERE, CcBuildDocks, tile);
				break;

			case WID_DT_RIVER: // Build river button (in scenario editor)
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_RIVER);
				break;

			case WID_DT_BUILD_AQUEDUCT: // Build aqueduct button
				Command<CMD_BUILD_BRIDGE>::Post(STR_ERROR_CAN_T_BUILD_AQUEDUCT_HERE, CcBuildBridge, tile, GetOtherAqueductEnd(tile), TRANSPORT_WATER, 0, 0);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x != -1) {
			switch (select_proc) {
				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
				case DDSP_CREATE_WATER:
					Command<CMD_BUILD_CANAL>::Post(STR_ERROR_CAN_T_BUILD_CANALS, CcPlaySound_CONSTRUCTION_WATER, end_tile, start_tile, (_game_mode == GM_EDITOR && _ctrl_pressed) ? WATER_CLASS_SEA : WATER_CLASS_CANAL, false);
					break;
				case DDSP_CREATE_RIVER:
					Command<CMD_BUILD_CANAL>::Post(STR_ERROR_CAN_T_PLACE_RIVERS, CcPlaySound_CONSTRUCTION_WATER, end_tile, start_tile, WATER_CLASS_RIVER, _ctrl_pressed);
					break;

				default: break;
			}
		}
	}

	void OnPlaceObjectAbort() override
	{
		if (_game_mode != GM_EDITOR && this->IsWidgetLowered(WID_DT_STATION)) SetViewportCatchmentStation(nullptr, true);

		this->RaiseButtons();

		CloseWindowById(WC_BUILD_STATION, TRANSPORT_WATER);
		CloseWindowById(WC_BUILD_DEPOT, TRANSPORT_WATER);
		CloseWindowById(WC_SELECT_STATION, 0);
		CloseWindowByClass(WC_BUILD_BRIDGE);
	}

	void OnPlacePresize([[maybe_unused]] Point pt, TileIndex tile_from) override
	{
		TileIndex tile_to = tile_from;

		if (this->last_clicked_widget == WID_DT_BUILD_AQUEDUCT) {
			GetOtherAqueductEnd(tile_from, &tile_to);
		} else {
			DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile_from));
			if (IsValidDiagDirection(dir)) {
				/* Locks and docks always select the tile "down" the slope. */
				tile_to = TileAddByDiagDir(tile_from, ReverseDiagDir(dir));
				/* Locks also select the tile "up" the slope. */
				if (this->last_clicked_widget == WID_DT_LOCK) tile_from = TileAddByDiagDir(tile_from, dir);
			}
		}

		VpSetPresizeRange(tile_from, tile_to);
	}

	/**
	 * Handler for global hotkeys of the BuildDocksToolbarWindow.
	 * @param hotkey Hotkey
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState DockToolbarGlobalHotkeys(int hotkey)
	{
		if (_game_mode != GM_NORMAL) return ES_NOT_HANDLED;
		Window *w = ShowBuildDocksToolbar();
		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"dockstoolbar", {
		Hotkey('1', "canal", WID_DT_CANAL),
		Hotkey('2', "lock", WID_DT_LOCK),
		Hotkey('3', "demolish", WID_DT_DEMOLISH),
		Hotkey('4', "depot", WID_DT_DEPOT),
		Hotkey('5', "dock", WID_DT_STATION),
		Hotkey('6', "buoy", WID_DT_BUOY),
		Hotkey('7', "river", WID_DT_RIVER),
		Hotkey({'B', '8'}, "aqueduct", WID_DT_BUILD_AQUEDUCT),
	}, DockToolbarGlobalHotkeys};
};

/**
 * Nested widget parts of docks toolbar, game version.
 * Position of #WID_DT_RIVER widget has changed.
 */
static const NWidgetPart _nested_build_docks_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_WATERWAYS_TOOLBAR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL_LTR),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_CANAL), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_CANAL, STR_WATERWAYS_TOOLBAR_BUILD_CANALS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_LOCK), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_LOCK, STR_WATERWAYS_TOOLBAR_BUILD_LOCKS_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetMinimalSize(5, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEMOLISH), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEPOT), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SHIP_DEPOT, STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_STATION), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SHIP_DOCK, STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUOY), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUOY, STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUILD_AQUEDUCT), SetMinimalSize(23, 22), SetFill(0, 1), SetDataTip(SPR_IMG_AQUEDUCT, STR_WATERWAYS_TOOLBAR_BUILD_AQUEDUCT_TOOLTIP),
	EndContainer(),
};

static WindowDesc _build_docks_toolbar_desc(__FILE__, __LINE__,
	WDP_ALIGN_TOOLBAR, "toolbar_water", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_docks_toolbar_widgets), std::end(_nested_build_docks_toolbar_widgets),
	&BuildDocksToolbarWindow::hotkeys
);

/**
 * Open the build water toolbar window
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @return newly opened water toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildDocksToolbar()
{
	if (!Company::IsValidID(_local_company)) return nullptr;

	CloseWindowByClass(WC_BUILD_TOOLBAR);
	return AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_toolbar_desc, TRANSPORT_WATER);
}

/**
 * Nested widget parts of docks toolbar, scenario editor version.
 * Positions of #WID_DT_DEPOT, #WID_DT_STATION, and #WID_DT_BUOY widgets have changed.
 */
static const NWidgetPart _nested_build_docks_scen_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_WATERWAYS_TOOLBAR_CAPTION_SE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_CANAL), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_CANAL, STR_WATERWAYS_TOOLBAR_CREATE_LAKE_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_LOCK), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_LOCK, STR_WATERWAYS_TOOLBAR_BUILD_LOCKS_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetMinimalSize(5, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEMOLISH), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_RIVER), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_RIVER, STR_WATERWAYS_TOOLBAR_CREATE_RIVER_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUILD_AQUEDUCT), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_AQUEDUCT, STR_WATERWAYS_TOOLBAR_BUILD_AQUEDUCT_TOOLTIP),
	EndContainer(),
};

/** Window definition for the build docks in scenario editor window. */
static WindowDesc _build_docks_scen_toolbar_desc(__FILE__, __LINE__,
	WDP_AUTO, "toolbar_water_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_docks_scen_toolbar_widgets), std::end(_nested_build_docks_scen_toolbar_widgets)
);

/**
 * Open the build water toolbar window for the scenario editor.
 *
 * @return newly opened water toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildDocksScenToolbar()
{
	return AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_scen_toolbar_desc, TRANSPORT_WATER);
}

/** Widget numbers of the build-dock GUI. */
enum BuildDockStationWidgets {
	BDSW_BACKGROUND, ///< Background panel.
	BDSW_LT_OFF,     ///< 'Off' button of coverage high light.
	BDSW_LT_ON,      ///< 'On' button of coverage high light.
	BDSW_INFO,       ///< 'Coverage highlight' label.
	BDSW_ACCEPTANCE, ///< Acceptance info.
};

struct BuildDocksStationWindow : public PickerWindowBase {
public:
	BuildDocksStationWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_WATER);
		this->LowerWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_SELECT_STATION, 0);
		this->PickerWindowBase::Close();
	}

	void OnPaint() override
	{
		int rad = (_settings_game.station.modified_catchment) ? CA_DOCK : CA_UNMODIFIED;

		this->DrawWidgets();

		if (_settings_client.gui.station_show_coverage) {
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		/* strings such as 'Size' and 'Coverage Area' */
		Rect r = this->GetWidget<NWidgetBase>(BDSW_ACCEPTANCE)->GetCurrentRect();
		int top = r.top;
		top = DrawStationCoverageAreaText(r.left, r.right, top, SCT_ALL, rad, false) + WidgetDimensions::scaled.vsep_normal;
		top = DrawStationCoverageAreaText(r.left, r.right, top, SCT_ALL, rad, true);
		/* Resize background if the window is too small.
		 * Never make the window smaller to avoid oscillating if the size change affects the acceptance.
		 * (This is the case, if making the window bigger moves the mouse into the window.) */
		if (top > r.bottom) {
			ResizeWindow(this, 0, top - r.bottom, false);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case BDSW_LT_OFF:
			case BDSW_LT_ON:
				this->RaiseWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
				_settings_client.gui.station_show_coverage = (widget != BDSW_LT_OFF);
				this->LowerWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				SetViewportCatchmentStation(nullptr, true);
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
};

/** Nested widget parts of a build dock station window. */
static const NWidgetPart _nested_build_dock_station_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_DOCK_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BDSW_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
				NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BDSW_INFO), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(14, 0, 14),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, BDSW_LT_OFF), SetMinimalSize(60, 12), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, BDSW_LT_ON), SetMinimalSize(60, 12), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_EMPTY, COLOUR_GREY, BDSW_ACCEPTANCE), SetResize(0, 1), SetMinimalTextLines(2, WidgetDimensions::unscaled.vsep_normal),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_dock_station_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_dock_station_widgets), std::end(_nested_build_dock_station_widgets)
);

static void ShowBuildDockStationPicker(Window *parent)
{
	new BuildDocksStationWindow(&_build_dock_station_desc, parent);
}

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
	BuildDocksDepotWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_WATER);
		this->LowerWidget(_ship_depot_direction + WID_BDD_X);
		UpdateDocksDirection();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_BDD_X:
			case WID_BDD_Y:
				size->width  = ScaleGUITrad(96) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size->height = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		DrawPixelInfo tmp_dpi;

		switch (widget) {
			case WID_BDD_X:
			case WID_BDD_Y: {
				Axis axis = widget == WID_BDD_X ? AXIS_X : AXIS_Y;

				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(96)) / 2;
					int y = (ir.Height() - ScaleSpriteTrad(64)) / 2;
					int x1 = ScaleSpriteTrad(63);
					int x2 = ScaleSpriteTrad(31);
					DrawShipDepotSprite(x + (axis == AXIS_X ? x1 : x2), y + ScaleSpriteTrad(17), axis, DEPOT_PART_NORTH);
					DrawShipDepotSprite(x + (axis == AXIS_X ? x2 : x1), y + ScaleSpriteTrad(33), axis, DEPOT_PART_SOUTH);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BDD_X:
			case WID_BDD_Y:
				this->RaiseWidget(_ship_depot_direction + WID_BDD_X);
				_ship_depot_direction = (widget == WID_BDD_X ? AXIS_X : AXIS_Y);
				this->LowerWidget(_ship_depot_direction + WID_BDD_X);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				UpdateDocksDirection();
				this->SetDirty();
				break;
		}
	}
};

static const NWidgetPart _nested_build_docks_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_DEPOT_BUILD_SHIP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BDD_BACKGROUND),
		NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BDD_X), SetMinimalSize(98, 66), SetDataTip(0x0, STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BDD_Y), SetMinimalSize(98, 66), SetDataTip(0x0, STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_docks_depot_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_docks_depot_widgets), std::end(_nested_build_docks_depot_widgets)
);


static void ShowBuildDocksDepotPicker(Window *parent)
{
	new BuildDocksDepotWindow(&_build_docks_depot_desc, parent);
}


void InitializeDockGui()
{
	_ship_depot_direction = AXIS_X;
}
