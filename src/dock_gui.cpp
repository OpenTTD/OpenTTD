/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
	auto [slope, z] = GetTileSlopeZ(tile_from);
	DiagDirection dir = GetInclinedSlopeDirection(slope);

	/* If the direction isn't right, just return the next tile so the command
	 * complains about the wrong slope instead of the ends not matching up.
	 * Make sure the coordinate is always a valid tile within the map, so we
	 * don't go "off" the map. That would cause the wrong error message. */
	if (!IsValidDiagDirection(dir)) return TileAddXY(tile_from, TileX(tile_from) > 2 ? -1 : 1, 0);

	/* Direction the aqueduct is built to. */
	TileIndexDiff offset = TileOffsByDiagDir(ReverseDiagDir(dir));
	/* The maximum length of the aqueduct. */
	int max_length = std::min<int>(_settings_game.construction.max_bridge_length, DistanceFromEdgeDir(tile_from, ReverseDiagDir(dir)) - 1);

	TileIndex endtile = tile_from;
	for (int length = 0; IsValidTile(endtile) && TileX(endtile) != 0 && TileY(endtile) != 0; length++) {
		endtile = TileAdd(endtile, offset);

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
	WidgetID last_clicked_widget = INVALID_WIDGET; ///< Contains the last widget that has been clicked on this toolbar.

	BuildDocksToolbarWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
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
				if (_game_mode == GM_EDITOR) {
					HandlePlacePushButton(this, WID_DT_CANAL, SPR_CURSOR_CANAL, HT_RECT);
				} else {
					HandlePlacePushButton(this, WID_DT_CANAL, SPR_CURSOR_CANAL, HT_RECT | HT_DIAGONAL);
				}
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
		this->last_clicked_widget = widget;
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (this->last_clicked_widget) {
			case WID_DT_CANAL: // Build canal button
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_WATER);
				break;

			case WID_DT_LOCK: // Build lock button
				Command<Commands::BuildLock>::Post(STR_ERROR_CAN_T_BUILD_LOCKS, CcBuildDocks, tile);
				break;

			case WID_DT_DEMOLISH: // Demolish aka dynamite button
				PlaceProc_DemolishArea(tile);
				break;

			case WID_DT_DEPOT: // Build depot button
				Command<Commands::BuildShipDepot>::Post(STR_ERROR_CAN_T_BUILD_SHIP_DEPOT, CcBuildDocks, tile, _ship_depot_direction);
				break;

			case WID_DT_STATION: { // Build station button
				/* Determine the watery part of the dock. */
				DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile));
				TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile, ReverseDiagDir(dir)) : tile);

				bool adjacent = _ctrl_pressed;
				auto proc = [=](bool test, StationID to_join) -> bool {
					if (test) {
						return Command<Commands::BuildDock>::Do(CommandFlagsToDCFlags(GetCommandFlags<Commands::BuildDock>()), tile, StationID::Invalid(), adjacent).Succeeded();
					} else {
						return Command<Commands::BuildDock>::Post(STR_ERROR_CAN_T_BUILD_DOCK_HERE, CcBuildDocks, tile, to_join, adjacent);
					}
				};

				ShowSelectStationIfNeeded(TileArea(tile, tile_to), proc);
				break;
			}

			case WID_DT_BUOY: // Build buoy button
				Command<Commands::BuildBuoy>::Post(STR_ERROR_CAN_T_POSITION_BUOY_HERE, CcBuildDocks, tile);
				break;

			case WID_DT_RIVER: // Build river button (in scenario editor)
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_RIVER);
				break;

			case WID_DT_BUILD_AQUEDUCT: // Build aqueduct button
				Command<Commands::BuildBridge>::Post(STR_ERROR_CAN_T_BUILD_AQUEDUCT_HERE, CcBuildBridge, tile, GetOtherAqueductEnd(tile), TRANSPORT_WATER, 0, 0);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	Point OnInitialPosition(int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		return AlignInitialConstructionToolbar(sm_width);
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x != -1) {
			switch (select_proc) {
				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
				case DDSP_CREATE_WATER:
					if (_game_mode == GM_EDITOR) {
						Command<Commands::BuildCanal>::Post(STR_ERROR_CAN_T_BUILD_CANALS, CcPlaySound_CONSTRUCTION_WATER, end_tile, start_tile, _ctrl_pressed ? WaterClass::Sea : WaterClass::Canal, false);
					} else {
						Command<Commands::BuildCanal>::Post(STR_ERROR_CAN_T_BUILD_CANALS, CcPlaySound_CONSTRUCTION_WATER, end_tile, start_tile, WaterClass::Canal, _ctrl_pressed);
					}
					break;
				case DDSP_CREATE_RIVER:
					Command<Commands::BuildCanal>::Post(STR_ERROR_CAN_T_PLACE_RIVERS, CcPlaySound_CONSTRUCTION_WATER, end_tile, start_tile, WaterClass::River, _ctrl_pressed);
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
static constexpr std::initializer_list<NWidgetPart> _nested_build_docks_toolbar_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_WATERWAYS_TOOLBAR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL_LTR),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_CANAL), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_BUILD_CANAL, STR_WATERWAYS_TOOLBAR_BUILD_CANALS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_LOCK), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_BUILD_LOCK, STR_WATERWAYS_TOOLBAR_BUILD_LOCKS_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetToolbarSpacerMinimalSize(), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEMOLISH), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEPOT), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_SHIP_DEPOT, STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_STATION), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_SHIP_DOCK, STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUOY), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_BUOY, STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUILD_AQUEDUCT), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_AQUEDUCT, STR_WATERWAYS_TOOLBAR_BUILD_AQUEDUCT_TOOLTIP),
	EndContainer(),
};

static WindowDesc _build_docks_toolbar_desc(
	WDP_MANUAL, "toolbar_water", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_build_docks_toolbar_widgets,
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
	return AllocateWindowDescFront<BuildDocksToolbarWindow>(_build_docks_toolbar_desc, TRANSPORT_WATER);
}

/**
 * Nested widget parts of docks toolbar, scenario editor version.
 * Positions of #WID_DT_DEPOT, #WID_DT_STATION, and #WID_DT_BUOY widgets have changed.
 */
static constexpr std::initializer_list<NWidgetPart> _nested_build_docks_scen_toolbar_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_WATERWAYS_TOOLBAR_CAPTION_SE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_CANAL), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_BUILD_CANAL, STR_WATERWAYS_TOOLBAR_CREATE_LAKE_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_LOCK), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_BUILD_LOCK, STR_WATERWAYS_TOOLBAR_BUILD_LOCKS_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetToolbarSpacerMinimalSize(), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEMOLISH), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_RIVER), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_BUILD_RIVER, STR_WATERWAYS_TOOLBAR_CREATE_RIVER_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUILD_AQUEDUCT), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_AQUEDUCT, STR_WATERWAYS_TOOLBAR_BUILD_AQUEDUCT_TOOLTIP),
	EndContainer(),
};

/** Window definition for the build docks in scenario editor window. */
static WindowDesc _build_docks_scen_toolbar_desc(
	WDP_AUTO, "toolbar_water_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_build_docks_scen_toolbar_widgets
);

/**
 * Open the build water toolbar window for the scenario editor.
 *
 * @return newly opened water toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildDocksScenToolbar()
{
	return AllocateWindowDescFront<BuildDocksToolbarWindow>(_build_docks_scen_toolbar_desc, TRANSPORT_WATER);
}

struct BuildDocksStationWindow : public PickerWindowBase {
public:
	BuildDocksStationWindow(WindowDesc &desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_WATER);
		this->LowerWidget(_settings_client.gui.station_show_coverage + WID_BDSW_LT_OFF);
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
		Rect r = this->GetWidget<NWidgetBase>(WID_BDSW_ACCEPTANCE)->GetCurrentRect();
		const int bottom = r.bottom;
		r.bottom = INT_MAX; // Allow overflow as we want to know the required height.
		r.top = DrawStationCoverageAreaText(r, SCT_ALL, rad, false) + WidgetDimensions::scaled.vsep_normal;
		r.top = DrawStationCoverageAreaText(r, SCT_ALL, rad, true);
		/* Resize background if the window is too small.
		 * Never make the window smaller to avoid oscillating if the size change affects the acceptance.
		 * (This is the case, if making the window bigger moves the mouse into the window.) */
		if (r.top > bottom) {
			ResizeWindow(this, 0, r.top - bottom, false);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BDSW_LT_OFF:
			case WID_BDSW_LT_ON:
				this->RaiseWidget(_settings_client.gui.station_show_coverage + WID_BDSW_LT_OFF);
				_settings_client.gui.station_show_coverage = (widget != WID_BDSW_LT_OFF);
				this->LowerWidget(_settings_client.gui.station_show_coverage + WID_BDSW_LT_OFF);
				SndClickBeep();
				this->SetDirty();
				SetViewportCatchmentStation(nullptr, true);
				break;
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		CheckRedrawStationCoverage(this);
	}

	const IntervalTimer<TimerGameCalendar> yearly_interval = {{TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [this](auto) {
		this->InvalidateData();
	}};
};

/** Nested widget parts of a build dock station window. */
static constexpr std::initializer_list<NWidgetPart> _nested_build_dock_station_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_STATION_BUILD_DOCK_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BDSW_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
				NWidget(WWT_LABEL, INVALID_COLOUR, WID_BDSW_INFO), SetStringTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(14, 0, 14),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BDSW_LT_OFF), SetMinimalSize(60, 12), SetFill(1, 0), SetStringTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
					NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BDSW_LT_ON), SetMinimalSize(60, 12), SetFill(1, 0), SetStringTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BDSW_ACCEPTANCE), SetResize(0, 1), SetMinimalTextLines(2, WidgetDimensions::unscaled.vsep_normal),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_dock_station_desc(
	WDP_AUTO, {}, 0, 0,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WindowDefaultFlag::Construction,
	_nested_build_dock_station_widgets
);

static void ShowBuildDockStationPicker(Window *parent)
{
	new BuildDocksStationWindow(_build_dock_station_desc, parent);
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
	BuildDocksDepotWindow(WindowDesc &desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_WATER);
		this->LowerWidget(WID_BDD_X + _ship_depot_direction);
		UpdateDocksDirection();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_BDD_X:
			case WID_BDD_Y:
				size.width  = ScaleGUITrad(96) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Vertical();
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
					DrawShipDepotSprite(x + (axis == AXIS_X ? x1 : x2), y + ScaleSpriteTrad(17), axis, DepotPart::North);
					DrawShipDepotSprite(x + (axis == AXIS_X ? x2 : x1), y + ScaleSpriteTrad(33), axis, DepotPart::South);
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
				this->RaiseWidget(WID_BDD_X + _ship_depot_direction);
				_ship_depot_direction = (widget == WID_BDD_X ? AXIS_X : AXIS_Y);
				this->LowerWidget(WID_BDD_X + _ship_depot_direction);
				SndClickBeep();
				UpdateDocksDirection();
				this->SetDirty();
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_build_docks_depot_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_DEPOT_BUILD_SHIP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BDD_BACKGROUND),
		NWidget(NWID_HORIZONTAL_LTR), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 1), SetPadding(WidgetDimensions::unscaled.picker),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BDD_X), SetToolTip(STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_BDD_Y), SetToolTip(STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_docks_depot_desc(
	WDP_AUTO, {}, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WindowDefaultFlag::Construction,
	_nested_build_docks_depot_widgets
);


static void ShowBuildDocksDepotPicker(Window *parent)
{
	new BuildDocksDepotWindow(_build_docks_depot_desc, parent);
}


void InitializeDockGui()
{
	_ship_depot_direction = AXIS_X;
}
