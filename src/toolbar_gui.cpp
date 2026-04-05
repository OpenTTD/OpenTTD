/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file toolbar_gui.cpp Code related to the (main) toolbar. */

#include "stdafx.h"
#include "core/geometry_func.hpp"
#include "gui.h"
#include "spritecache.h"
#include "window_gui.h"
#include "window_func.h"
#include "viewport_func.h"
#include "command_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "house.h"
#include "vehicle_gui.h"
#include "rail_gui.h"
#include "road.h"
#include "road_gui.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "terraform_gui.h"
#include "strings_func.h"
#include "company_func.h"
#include "company_gui.h"
#include "vehicle_base.h"
#include "cheat_func.h"
#include "transparency_gui.h"
#include "screenshot.h"
#include "signs_func.h"
#include "fios.h"
#include "console_gui.h"
#include "news_gui.h"
#include "ai/ai_gui.hpp"
#include "game/game_gui.hpp"
#include "script/script_gui.h"
#include "tilehighlight_func.h"
#include "smallmap_gui.h"
#include "graph_gui.h"
#include "textbuf_gui.h"
#include "linkgraph/linkgraph_gui.h"
#include "newgrf_debug.h"
#include "hotkeys.h"
#include "engine_base.h"
#include "highscore.h"
#include "game/game.hpp"
#include "goal_base.h"
#include "story_base.h"
#include "toolbar_gui.h"
#include "framerate_type.h"
#include "screenshot_gui.h"
#include "misc_cmd.h"
#include "league_gui.h"
#include "league_base.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "timer/timer_game_calendar.h"
#include "help_gui.h"
#include "core/string_consumer.hpp"

#include "widgets/toolbar_widget.h"

#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"


/** Width of the toolbar, shared by statusbar. */
uint _toolbar_width = 0;

RailType _last_built_railtype;
RoadType _last_built_roadtype;
RoadType _last_built_tramtype;

/** Toolbar modes */
enum class ToolbarMode : uint8_t {
	Normal, ///< Toolbar is in normal mode, in wich all 30 buttons are accessible.
	Upper, ///< Toolbar is in split mode and the first part is selected.
	Lower, ///< Toolbar is in split mode and the second part is selected.
};

/** Callback functions. */
enum class CallBackFunction : uint8_t {
	None, ///< No action will happen when tile is selected afterwards.
	PlaceSign, ///< A new sign will be placed when tile is selected afterwards.
	PlaceLandInfo, ///< Land info window will appear when tile is selected afterwards.
};

static CallBackFunction _last_started_action = CallBackFunction::None; ///< Last started user action.

/**
 * Company name list item, with company-colour icon, name, and lock components.
 */
class DropDownListCompanyItem : public DropDownIcon<DropDownIcon<DropDownString<DropDownListItem>, true>> {
public:
	DropDownListCompanyItem(CompanyID company, bool shaded) : DropDownIcon<DropDownIcon<DropDownString<DropDownListItem>, true>>(SPR_COMPANY_ICON, GetCompanyPalette(company), NetworkCanJoinCompany(company) ? SPR_EMPTY : SPR_LOCK, PAL_NONE, GetString(STR_COMPANY_NAME_COMPANY_NUM, company, company), company.base(), false, shaded)
	{
	}
};

/**
 * Get options for toolbar dropdown menus,
 * @param options Additional options to include.
 * @return DropDownOptions to use for toolbar dropdown menus.
 */
static DropDownOptions GetToolbarDropDownOptions(DropDownOptions options = {})
{
	if (_settings_client.gui.toolbar_dropdown_autoselect) options.Set(DropDownOption::InstantClose).Reset(DropDownOption::Filterable);
	return options;
}

/**
 * Pop up a generic text only menu.
 * @param w Toolbar
 * @param widget Toolbar button
 * @param list List of items
 * @param def Default item
 */
static void PopupMainToolbarMenu(Window *w, WidgetID widget, DropDownList &&list, int def)
{
	ShowDropDownList(w, std::move(list), def, widget, 0, GetToolbarDropDownOptions());
}

/**
 * Pop up a generic text only menu.
 * @param w Toolbar
 * @param widget Toolbar button
 * @param strings List of strings for each item in the menu
 */
static void PopupMainToolbarMenu(Window *w, WidgetID widget, const std::initializer_list<StringID> &strings)
{
	DropDownList list;
	int i = 0;
	for (StringID string : strings) {
		if (string == STR_NULL) {
			list.push_back(MakeDropDownListDividerItem());
		} else {
			list.push_back(MakeDropDownListStringItem(string, i));
			i++;
		}
	}
	PopupMainToolbarMenu(w, widget, std::move(list), 0);
}

/* Special values used in the dropdowns related to companies.
 * They cannot interfere with valid IDs for companies. */
static const int CTMN_CLIENT_LIST = MAX_COMPANIES; ///< Indicates the "all connected players" entry.
static const int CTMN_SPECTATE = COMPANY_SPECTATOR.base(); ///< Indicates the "become spectator" entry.
static const int CTMN_SPECTATOR = CompanyID::Invalid().base(); ///< Indicates that a window is being opened for the spectator.

/**
 * Pop up a generic company list menu.
 * @param w The toolbar window.
 * @param widget The button widget id.
 * @param grey A bitmask of which companies to mark as disabled.
 */
static void PopupMainCompanyToolbMenu(Window *w, WidgetID widget, CompanyMask grey = {})
{
	DropDownList list;

	switch (widget) {
		case WID_TN_COMPANIES:
			if (!_networking) break;

			/* Add the client list button for the companies menu */
			list.push_back(MakeDropDownListStringItem(STR_NETWORK_COMPANY_LIST_CLIENT_LIST, CTMN_CLIENT_LIST));

			if (_local_company != COMPANY_SPECTATOR) {
				list.push_back(MakeDropDownListStringItem(STR_NETWORK_COMPANY_LIST_SPECTATE, CTMN_SPECTATE));
			}
			break;
		case WID_TN_STORY:
			list.push_back(MakeDropDownListStringItem(STR_STORY_BOOK_SPECTATOR, CTMN_SPECTATOR));
			break;

		case WID_TN_GOAL:
			list.push_back(MakeDropDownListStringItem(STR_GOALS_SPECTATOR, CTMN_SPECTATOR));
			break;
	}

	for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
		if (!Company::IsValidID(c)) continue;
		list.push_back(std::make_unique<DropDownListCompanyItem>(c, grey.Test(c)));
	}

	PopupMainToolbarMenu(w, widget, std::move(list), _local_company == COMPANY_SPECTATOR ? (widget == WID_TN_COMPANIES ? CTMN_CLIENT_LIST : CTMN_SPECTATOR) : _local_company.base());
}

static ToolbarMode _toolbar_mode;

static CallBackFunction SelectSignTool()
{
	if (_last_started_action == CallBackFunction::PlaceSign) {
		ResetObjectToPlace();
		return CallBackFunction::None;
	} else {
		SetObjectToPlace(SPR_CURSOR_SIGN, PAL_NONE, HT_RECT, WC_MAIN_TOOLBAR, 0);
		return CallBackFunction::PlaceSign;
	}
}

/* --- Pausing --- */

static CallBackFunction ToolbarPauseClick(Window *)
{
	if (_networking && !_network_server) return CallBackFunction::None; // only server can pause the game

	if (Command<Commands::Pause>::Post(PauseMode::Normal, _pause_mode.None())) {
		SndConfirmBeep();
	}
	return CallBackFunction::None;
}

/**
 * Toggle fast forward mode.
 *
 * @return #CallBackFunction::None
 */
static CallBackFunction ToolbarFastForwardClick(Window *)
{
	if (_networking) return CallBackFunction::None; // no fast forward in network game

	ChangeGameSpeed(_game_speed == 100);

	SndClickBeep();
	return CallBackFunction::None;
}

/**
 * Game Option button menu entries.
 */
enum class OptionMenuEntries : uint8_t {
	GameOptions, ///< Open game options window.
	AISettings, ///< Open AI settings window.
	GameScriptSettings, ///< Open GS settings window.
	NewGRFSettings, ///< Open NewGRF settings window.
	SandboxOptions, ///< Open sandbox options window.
	Transparencies, ///< Open transparency options window.
	ShowTownNames, ///< Toggle visibility of town names.
	ShowStationNames, ///< Toggle visibility of station names.
	ShowTrainStationNames, ///< Toggle visibility of train station names.
	ShowLorryStationNames, ///< Toggle visibility of lorry station names.
	ShowBusStationNames, ///< Toggle visibility of bus station names.
	ShowDockNames, ///< Toggle visibility of dock names.
	ShowAirportNames, ///< Toggle visibility of airport names.
	ShowGhostStationNames, ///< Toggle visibility of ghost station names.
	ShowWaypointNames, ///< Toggle visibility of waypoint names.
	ShowSigns, ///< Toggle visibility of signs.
	ShowCompetitorSigns, ///< Toggle visibility of competitor signs and names.
	FullAnimation, ///< Toggle full animations.
	FullDetails, ///< Toggle full details.
	TransparentBuildings, ///< Toggle buildings transparency.
	TransparentStationSigns, ///< Toggle transparency of signs and names.
};

/**
 * Handle click on Options button in toolbar.
 *
 * @param w parent window the shown Drop down list is attached to.
 * @return #CallBackFunction::None
 */
static CallBackFunction ToolbarOptionsClick(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListStringItem(STR_SETTINGS_MENU_GAME_OPTIONS, OptionMenuEntries::GameOptions));
	/* Changes to the per-AI settings don't get send from the server to the clients. Clients get
	 * the settings once they join but never update it. As such don't show the window at all
	 * to network clients. */
	if (!_networking || _network_server) {
		list.push_back(MakeDropDownListStringItem(STR_SETTINGS_MENU_AI_SETTINGS, OptionMenuEntries::AISettings));
		list.push_back(MakeDropDownListStringItem(STR_SETTINGS_MENU_GAMESCRIPT_SETTINGS, OptionMenuEntries::GameScriptSettings));
	}
	list.push_back(MakeDropDownListStringItem(STR_SETTINGS_MENU_NEWGRF_SETTINGS, OptionMenuEntries::NewGRFSettings));
	if (_game_mode != GM_EDITOR && !_networking) {
		list.push_back(MakeDropDownListStringItem(STR_SETTINGS_MENU_SANDBOX_OPTIONS, OptionMenuEntries::SandboxOptions));
	}
	list.push_back(MakeDropDownListStringItem(STR_SETTINGS_MENU_TRANSPARENCY_OPTIONS, OptionMenuEntries::Transparencies));
	list.push_back(MakeDropDownListDividerItem());
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_SHOW_TOWN_NAMES), STR_SETTINGS_MENU_TOWN_NAMES_DISPLAYED, OptionMenuEntries::ShowTownNames));
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_SHOW_STATION_NAMES), STR_SETTINGS_MENU_STATION_NAMES_DISPLAYED, OptionMenuEntries::ShowStationNames));
	list.push_back(MakeDropDownListCheckedItem(_facility_display_opt.Test(StationFacility::Train), STR_SETTINGS_MENU_STATION_NAMES_TRAIN, OptionMenuEntries::ShowTrainStationNames, false, false, 1));
	list.push_back(MakeDropDownListCheckedItem(_facility_display_opt.Test(StationFacility::TruckStop), STR_SETTINGS_MENU_STATION_NAMES_LORRY, OptionMenuEntries::ShowLorryStationNames, false, false, 1));
	list.push_back(MakeDropDownListCheckedItem(_facility_display_opt.Test(StationFacility::BusStop), STR_SETTINGS_MENU_STATION_NAMES_BUS, OptionMenuEntries::ShowBusStationNames, false, false, 1));
	list.push_back(MakeDropDownListCheckedItem(_facility_display_opt.Test(StationFacility::Dock), STR_SETTINGS_MENU_STATION_NAMES_SHIP, OptionMenuEntries::ShowDockNames, false, false, 1));
	list.push_back(MakeDropDownListCheckedItem(_facility_display_opt.Test(StationFacility::Airport), STR_SETTINGS_MENU_STATION_NAMES_PLANE, OptionMenuEntries::ShowAirportNames, false, false, 1));
	list.push_back(MakeDropDownListCheckedItem(_facility_display_opt.Test(STATION_FACILITY_GHOST), STR_SETTINGS_MENU_STATION_NAMES_GHOST, OptionMenuEntries::ShowGhostStationNames, false, false, 1));
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_SHOW_WAYPOINT_NAMES), STR_SETTINGS_MENU_WAYPOINTS_DISPLAYED, OptionMenuEntries::ShowWaypointNames));
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_SHOW_SIGNS), STR_SETTINGS_MENU_SIGNS_DISPLAYED, OptionMenuEntries::ShowSigns));
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS), STR_SETTINGS_MENU_SHOW_COMPETITOR_SIGNS, OptionMenuEntries::ShowCompetitorSigns));
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_FULL_ANIMATION), STR_SETTINGS_MENU_FULL_ANIMATION, OptionMenuEntries::FullAnimation));
	list.push_back(MakeDropDownListCheckedItem(HasBit(_display_opt, DO_FULL_DETAIL), STR_SETTINGS_MENU_FULL_DETAIL, OptionMenuEntries::FullDetails));
	list.push_back(MakeDropDownListCheckedItem(IsTransparencySet(TO_HOUSES), STR_SETTINGS_MENU_TRANSPARENT_BUILDINGS, OptionMenuEntries::TransparentBuildings));
	list.push_back(MakeDropDownListCheckedItem(IsTransparencySet(TO_SIGNS), STR_SETTINGS_MENU_TRANSPARENT_SIGNS, OptionMenuEntries::TransparentStationSigns));

	ShowDropDownList(w, std::move(list), 0, WID_TN_SETTINGS, 140, GetToolbarDropDownOptions());
	return CallBackFunction::None;
}

/**
 * Handle click on one of the entries in the Options button menu.
 *
 * @param index Index being clicked.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickSettings(int index)
{
	switch (OptionMenuEntries(index)) {
		case OptionMenuEntries::GameOptions: ShowGameOptions(); return CallBackFunction::None;
		case OptionMenuEntries::AISettings: ShowAIConfigWindow(); return CallBackFunction::None;
		case OptionMenuEntries::GameScriptSettings: ShowGSConfigWindow(); return CallBackFunction::None;
		case OptionMenuEntries::NewGRFSettings: ShowNewGRFSettings(!_networking && _settings_client.gui.UserIsAllowedToChangeNewGRFs(), true, true, _grfconfig); return CallBackFunction::None;
		case OptionMenuEntries::SandboxOptions: ShowCheatWindow(); break;
		case OptionMenuEntries::Transparencies: ShowTransparencyToolbar(); break;

		case OptionMenuEntries::ShowTownNames: ToggleBit(_display_opt, DO_SHOW_TOWN_NAMES); break;
		case OptionMenuEntries::ShowStationNames: ToggleBit(_display_opt, DO_SHOW_STATION_NAMES); break;
		case OptionMenuEntries::ShowTrainStationNames: _facility_display_opt.Flip(StationFacility::Train); break;
		case OptionMenuEntries::ShowLorryStationNames: _facility_display_opt.Flip(StationFacility::TruckStop); break;
		case OptionMenuEntries::ShowBusStationNames: _facility_display_opt.Flip(StationFacility::BusStop); break;
		case OptionMenuEntries::ShowDockNames: _facility_display_opt.Flip(StationFacility::Dock); break;
		case OptionMenuEntries::ShowAirportNames: _facility_display_opt.Flip(StationFacility::Airport); break;
		case OptionMenuEntries::ShowGhostStationNames: _facility_display_opt.Flip(STATION_FACILITY_GHOST); break;
		case OptionMenuEntries::ShowWaypointNames: ToggleBit(_display_opt, DO_SHOW_WAYPOINT_NAMES); break;
		case OptionMenuEntries::ShowSigns: ToggleBit(_display_opt, DO_SHOW_SIGNS); break;
		case OptionMenuEntries::ShowCompetitorSigns:
			ToggleBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS);
			InvalidateWindowClassesData(WC_SIGN_LIST, -1);
			break;
		case OptionMenuEntries::FullAnimation: ToggleBit(_display_opt, DO_FULL_ANIMATION); CheckBlitter(); break;
		case OptionMenuEntries::FullDetails: ToggleBit(_display_opt, DO_FULL_DETAIL); break;
		case OptionMenuEntries::TransparentBuildings: ToggleTransparency(TO_HOUSES); break;
		case OptionMenuEntries::TransparentStationSigns: ToggleTransparency(TO_SIGNS); break;
	}
	MarkWholeScreenDirty();
	return CallBackFunction::None;
}

/**
 * SaveLoad entries in scenario editor mode.
 */
enum class SaveLoadEditorMenuEntries : uint8_t {
	SaveScenario = 0, ///< Save the scenario.
	LoadScenario, ///< Load a scenario.
	SaveHeightmap, ///< Save the heightmap.
	LoadHeightmap, ///< Load a heightmap.
	ExitToMainMenu, ///< Exit to main menu.
	ExitGame, ///< Exit to desktop.
};

/**
 * SaveLoad entries in normal game mode.
 */
enum class SaveLoadNormalMenuEntries : uint8_t {
	SaveGame = 0, ///< Save the game.
	LoadGame, ///< Load a save.
	ExitToMainMenu, ///< Exit to main menu.
	ExitGame, ///< Exit to desktop.
};

/**
 * Handle click on Save button in toolbar in normal game mode.
 *
 * @param w parent window the shown save dialogue is attached to.
 * @return #CallBackFunction::None
 */
static CallBackFunction ToolbarSaveClick(Window *w)
{
	PopupMainToolbarMenu(w, WID_TN_SAVE, {STR_FILE_MENU_SAVE_GAME, STR_FILE_MENU_LOAD_GAME, STR_FILE_MENU_QUIT_GAME,
			STR_NULL, STR_FILE_MENU_EXIT});
	return CallBackFunction::None;
}

/**
 * Handle click on SaveLoad button in toolbar in the scenario editor.
 *
 * @param w parent window the shown save dialogue is attached to.
 * @return #CallBackFunction::None
 */
static CallBackFunction ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbarMenu(w, WID_TE_SAVE, {STR_SCENEDIT_FILE_MENU_SAVE_SCENARIO, STR_SCENEDIT_FILE_MENU_LOAD_SCENARIO,
			STR_SCENEDIT_FILE_MENU_SAVE_HEIGHTMAP, STR_SCENEDIT_FILE_MENU_LOAD_HEIGHTMAP,
			STR_SCENEDIT_FILE_MENU_QUIT_EDITOR, STR_NULL, STR_SCENEDIT_FILE_MENU_QUIT});
	return CallBackFunction::None;
}

/**
 * Handle click on one of the entries in the SaveLoad menu.
 *
 * @param index Index being clicked.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickSaveLoad(int index = 0)
{
	if (_game_mode == GM_EDITOR) {
		switch (SaveLoadEditorMenuEntries(index)) {
			case SaveLoadEditorMenuEntries::SaveScenario: ShowSaveLoadDialog(AbstractFileType::Scenario, SaveLoadOperation::Save); break;
			case SaveLoadEditorMenuEntries::LoadScenario: ShowSaveLoadDialog(AbstractFileType::Scenario, SaveLoadOperation::Load); break;
			case SaveLoadEditorMenuEntries::SaveHeightmap: ShowSaveLoadDialog(AbstractFileType::Heightmap, SaveLoadOperation::Save); break;
			case SaveLoadEditorMenuEntries::LoadHeightmap: ShowSaveLoadDialog(AbstractFileType::Heightmap, SaveLoadOperation::Load); break;
			case SaveLoadEditorMenuEntries::ExitToMainMenu: AskExitToGameMenu(); break;
			case SaveLoadEditorMenuEntries::ExitGame: HandleExitGameRequest(); break;
		}
	} else {
		switch (SaveLoadNormalMenuEntries(index)) {
			case SaveLoadNormalMenuEntries::SaveGame: ShowSaveLoadDialog(AbstractFileType::Savegame, SaveLoadOperation::Save); break;
			case SaveLoadNormalMenuEntries::LoadGame: ShowSaveLoadDialog(AbstractFileType::Savegame, SaveLoadOperation::Load); break;
			case SaveLoadNormalMenuEntries::ExitToMainMenu: AskExitToGameMenu(); break;
			case SaveLoadNormalMenuEntries::ExitGame: HandleExitGameRequest(); break;
		}
	}
	return CallBackFunction::None;
}

/* --- Map button menu --- */

/** Map button menu entries. */
enum class MapMenuEntries : uint8_t {
	ShowSmallMap = 0, ///< Open small map window.
	ShowExtraViewport, ///< Open new extra viewport window.
	ShowLinkGraph, ///< Open cargo flow legend window.
	ShowSignList, ///< Open sign list window.
	ShowTownDirectory, ///< Open window with list of towns.
	ShowIndustryDirectory, ///< Open window with list of industries.
};

static CallBackFunction ToolbarMapClick(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_MAP_OF_WORLD, MapMenuEntries::ShowSmallMap));
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_EXTRA_VIEWPORT, MapMenuEntries::ShowExtraViewport));
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_LINGRAPH_LEGEND, MapMenuEntries::ShowLinkGraph));
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_SIGN_LIST, MapMenuEntries::ShowSignList));
	PopupMainToolbarMenu(w, WID_TN_SMALL_MAP, std::move(list), 0);
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenMapTownDir(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_MAP_OF_WORLD, MapMenuEntries::ShowSmallMap));
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_EXTRA_VIEWPORT, MapMenuEntries::ShowExtraViewport));
	list.push_back(MakeDropDownListStringItem(STR_MAP_MENU_SIGN_LIST, MapMenuEntries::ShowSignList));
	list.push_back(MakeDropDownListStringItem(STR_TOWN_MENU_TOWN_DIRECTORY, MapMenuEntries::ShowTownDirectory));
	list.push_back(MakeDropDownListStringItem(STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, MapMenuEntries::ShowIndustryDirectory));
	PopupMainToolbarMenu(w, WID_TE_SMALL_MAP, std::move(list), 0);
	return CallBackFunction::None;
}

/**
 * Handle click on one of the entries in the Map menu.
 *
 * @param index Index being clicked.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickMap(int index)
{
	switch (MapMenuEntries(index)) {
		case MapMenuEntries::ShowSmallMap: ShowSmallMap(); break;
		case MapMenuEntries::ShowExtraViewport: ShowExtraViewportWindow(); break;
		case MapMenuEntries::ShowLinkGraph: ShowLinkGraphLegend(); break;
		case MapMenuEntries::ShowSignList: ShowSignList(); break;
		case MapMenuEntries::ShowTownDirectory: ShowTownDirectory(); break;
		case MapMenuEntries::ShowIndustryDirectory: ShowIndustryDirectory(); break;
	}
	return CallBackFunction::None;
}

/* --- Town button menu --- */

/** Town button menu entries. */
enum class TownMenuEntries {
	ShowDirectory = 0, ///< Open window with list of towns.
	ShowFoundTown, ///< Open town generation window.
	ShowPlaceHouses, ///< Open house selection window.
};

static CallBackFunction ToolbarTownClick(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListStringItem(STR_TOWN_MENU_TOWN_DIRECTORY, TownMenuEntries::ShowDirectory));
	if (_settings_game.economy.found_town != TF_FORBIDDEN) list.push_back(MakeDropDownListStringItem(STR_TOWN_MENU_FOUND_TOWN, TownMenuEntries::ShowFoundTown));
	if (_settings_game.economy.place_houses != PlaceHouses::Forbidden) list.push_back(MakeDropDownListStringItem(STR_SCENEDIT_TOWN_MENU_PACE_HOUSE, TownMenuEntries::ShowPlaceHouses));

	PopupMainToolbarMenu(w, WID_TN_TOWNS, std::move(list), 0);

	return CallBackFunction::None;
}

/**
 * Handle click on one of the entries in the Town menu.
 *
 * @param index Index being clicked.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickTown(int index)
{
	switch (TownMenuEntries(index)) {
		case TownMenuEntries::ShowDirectory: ShowTownDirectory(); break;
		case TownMenuEntries::ShowFoundTown: // Setting could be changed when the dropdown was open
			if (_settings_game.economy.found_town != TF_FORBIDDEN) ShowFoundTownWindow();
			break;
		case TownMenuEntries::ShowPlaceHouses: // Setting could be changed when the dropdown was open
			if (_settings_game.economy.place_houses != PlaceHouses::Forbidden) ShowBuildHousePicker(nullptr);
			break;
	}
	return CallBackFunction::None;
}

/* --- Subidies button menu --- */

static CallBackFunction ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbarMenu(w, WID_TN_SUBSIDIES, {STR_SUBSIDIES_MENU_SUBSIDIES});
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Subsidies menu.
 *
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickSubsidies(int)
{
	ShowSubsidiesList();
	return CallBackFunction::None;
}

/* --- Stations button menu --- */

static CallBackFunction ToolbarStationsClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, WID_TN_STATIONS);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Stations menu
 *
 * @param index CompanyID to show station list for
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickStations(int index)
{
	ShowCompanyStations((CompanyID)index);
	return CallBackFunction::None;
}

/* --- Finances button menu --- */

static CallBackFunction ToolbarFinancesClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, WID_TN_FINANCES);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the finances overview menu.
 *
 * @param index CompanyID to show finances for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickFinances(int index)
{
	ShowCompanyFinances((CompanyID)index);
	return CallBackFunction::None;
}

/* --- Company's button menu --- */

static CallBackFunction ToolbarCompaniesClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, WID_TN_COMPANIES);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Company menu.
 *
 * @param index Menu entry to handle.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickCompany(int index)
{
	if (_networking) {
		switch (index) {
			case CTMN_CLIENT_LIST:
				ShowClientList();
				return CallBackFunction::None;

			case CTMN_SPECTATE:
				if (_network_server) {
					NetworkServerDoMove(CLIENT_ID_SERVER, COMPANY_SPECTATOR);
					MarkWholeScreenDirty();
				} else {
					NetworkClientRequestMove(COMPANY_SPECTATOR);
				}
				return CallBackFunction::None;
		}
	}
	ShowCompany((CompanyID)index);
	return CallBackFunction::None;
}

/* --- Story button menu --- */

static CallBackFunction ToolbarStoryClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, WID_TN_STORY);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Story menu
 *
 * @param index CompanyID to show story book for
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickStory(int index)
{
	ShowStoryBook(CompanyID(index));
	return CallBackFunction::None;
}

/* --- Goal button menu --- */

static CallBackFunction ToolbarGoalClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, WID_TN_GOAL);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Goal menu
 *
 * @param index CompanyID to show story book for
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickGoal(int index)
{
	ShowGoalsList(CompanyID(index));
	return CallBackFunction::None;
}

/* --- Graphs and League Table button menu --- */

/**
 * Enum for the League Toolbar's and Graph Toolbar's related buttons.
 * Use continuous numbering as League Toolbar can be combined into the Graph Toolbar.
 */
static const int GRMN_OPERATING_PROFIT_GRAPH = -1;    ///< Show operating profit graph
static const int GRMN_INCOME_GRAPH = -2;              ///< Show income graph
static const int GRMN_DELIVERED_CARGO_GRAPH = -3;     ///< Show delivered cargo graph
static const int GRMN_PERFORMANCE_HISTORY_GRAPH = -4; ///< Show performance history graph
static const int GRMN_COMPANY_VALUE_GRAPH = -5;       ///< Show company value graph
static const int GRMN_CARGO_PAYMENT_RATES = -6;       ///< Show cargo payment rates graph
static const int LTMN_PERFORMANCE_LEAGUE = -7;        ///< Show default league table
static const int LTMN_PERFORMANCE_RATING = -8;        ///< Show detailed performance rating
static const int LTMN_HIGHSCORE          = -9;        ///< Show highscrore table

static void AddDropDownLeagueTableOptions(DropDownList &list)
{
	if (LeagueTable::GetNumItems() > 0) {
		for (LeagueTable *lt : LeagueTable::Iterate()) {
			list.push_back(MakeDropDownListStringItem(lt->title.GetDecodedString(), lt->index.base()));
		}
	} else {
		list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_COMPANY_LEAGUE_TABLE, LTMN_PERFORMANCE_LEAGUE));
		list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_DETAILED_PERFORMANCE_RATING, LTMN_PERFORMANCE_RATING));
		if (!_networking) {
			list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_HIGHSCORE, LTMN_HIGHSCORE));
		}
	}
}

static CallBackFunction ToolbarGraphsClick(Window *w)
{
	DropDownList list;

	list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_OPERATING_PROFIT_GRAPH, GRMN_OPERATING_PROFIT_GRAPH));
	list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_INCOME_GRAPH, GRMN_INCOME_GRAPH));
	list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_DELIVERED_CARGO_GRAPH, GRMN_DELIVERED_CARGO_GRAPH));
	list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_PERFORMANCE_HISTORY_GRAPH, GRMN_PERFORMANCE_HISTORY_GRAPH));
	list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_COMPANY_VALUE_GRAPH, GRMN_COMPANY_VALUE_GRAPH));
	list.push_back(MakeDropDownListStringItem(STR_GRAPH_MENU_CARGO_PAYMENT_RATES, GRMN_CARGO_PAYMENT_RATES));

	if (_toolbar_mode != ToolbarMode::Normal) AddDropDownLeagueTableOptions(list);

	ShowDropDownList(w, std::move(list), GRMN_OPERATING_PROFIT_GRAPH, WID_TN_GRAPHS, 140, GetToolbarDropDownOptions());
	return CallBackFunction::None;
}

static CallBackFunction ToolbarLeagueClick(Window *w)
{
	DropDownList list;

	AddDropDownLeagueTableOptions(list);

	int selected = list[0]->result;
	ShowDropDownList(w, std::move(list), selected, WID_TN_LEAGUE, 140, GetToolbarDropDownOptions());
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Graphs or CompanyLeague.
 *
 * @param index Graph to show.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickGraphsOrLeague(int index)
{
	switch (index) {
		case GRMN_OPERATING_PROFIT_GRAPH: ShowOperatingProfitGraph(); break;
		case GRMN_INCOME_GRAPH: ShowIncomeGraph(); break;
		case GRMN_DELIVERED_CARGO_GRAPH: ShowDeliveredCargoGraph(); break;
		case GRMN_PERFORMANCE_HISTORY_GRAPH: ShowPerformanceHistoryGraph(); break;
		case GRMN_COMPANY_VALUE_GRAPH: ShowCompanyValueGraph(); break;
		case GRMN_CARGO_PAYMENT_RATES: ShowCargoPaymentRates(); break;
		case LTMN_PERFORMANCE_LEAGUE: ShowPerformanceLeagueTable(); break;
		case LTMN_PERFORMANCE_RATING: ShowPerformanceRatingDetail(); break;
		case LTMN_HIGHSCORE: ShowHighscoreTable(); break;
		default: {
			if (LeagueTable::IsValidID(index)) {
				ShowScriptLeagueTable((LeagueTableID)index);
			}
		}
	}
	return CallBackFunction::None;
}



/* --- Industries button menu --- */

static CallBackFunction ToolbarIndustryClick(Window *w)
{
	/* Disable build-industry menu if we are a spectator */
	if (_local_company == COMPANY_SPECTATOR) {
		PopupMainToolbarMenu(w, WID_TN_INDUSTRIES, {STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, STR_INDUSTRY_MENU_INDUSTRY_CHAIN});
	} else {
		PopupMainToolbarMenu(w, WID_TN_INDUSTRIES, {STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, STR_INDUSTRY_MENU_INDUSTRY_CHAIN, STR_INDUSTRY_MENU_FUND_NEW_INDUSTRY});
	}
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Industry menu.
 *
 * @param index Menu entry number.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickIndustry(int index)
{
	switch (index) {
		case 0: ShowIndustryDirectory();     break;
		case 1: ShowIndustryCargoesWindow(); break;
		case 2: ShowBuildIndustryWindow();   break;
	}
	return CallBackFunction::None;
}

/* --- Trains button menu + 1 helper function for all vehicles. --- */

static void ToolbarVehicleClick(Window *w, VehicleType veh)
{
	CompanyMask dis{};

	for (const Company *c : Company::Iterate()) {
		if (c->group_all[veh].num_vehicle == 0) dis.Set(c->index);
	}
	PopupMainCompanyToolbMenu(w, WID_TN_VEHICLE_START + veh, dis);
}


static CallBackFunction ToolbarTrainClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_TRAIN);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Train menu.
 *
 * @param index CompanyID to show train list for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickShowTrains(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_TRAIN);
	return CallBackFunction::None;
}

/* --- Road vehicle button menu --- */

static CallBackFunction ToolbarRoadClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_ROAD);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Road Vehicles menu.
 *
 * @param index CompanyID to show road vehicles list for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickShowRoad(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_ROAD);
	return CallBackFunction::None;
}

/* --- Ship button menu --- */

static CallBackFunction ToolbarShipClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_SHIP);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Ships menu.
 *
 * @param index CompanyID to show ship list for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickShowShips(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_SHIP);
	return CallBackFunction::None;
}

/* --- Aircraft button menu --- */

static CallBackFunction ToolbarAirClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_AIRCRAFT);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Aircraft menu.
 *
 * @param index CompanyID to show aircraft list for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickShowAir(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_AIRCRAFT);
	return CallBackFunction::None;
}

/* --- Zoom in button --- */

static CallBackFunction ToolbarZoomInClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, GetMainWindow())) {
		w->HandleButtonClick((_game_mode == GM_EDITOR) ? (WidgetID)WID_TE_ZOOM_IN : (WidgetID)WID_TN_ZOOM_IN);
	}
	return CallBackFunction::None;
}

/* --- Zoom out button --- */

static CallBackFunction ToolbarZoomOutClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, GetMainWindow())) {
		w->HandleButtonClick((_game_mode == GM_EDITOR) ? (WidgetID)WID_TE_ZOOM_OUT : (WidgetID)WID_TN_ZOOM_OUT);
	}
	return CallBackFunction::None;
}

/* --- Rail button menu --- */

static std::string _railtype_filter; ///< Persistent filter text for railtype dropdown menu.
static std::string _roadtype_filter; ///< Persistent filter text for roadtype dropdown menu.
static std::string _tramtype_filter; ///< Persistent filter text for tramtype dropdown menu.

static CallBackFunction ToolbarBuildRailClick(Window *w)
{
	ShowDropDownList(w, GetRailTypeDropDownList(), _last_built_railtype, WID_TN_RAILS, 140, GetToolbarDropDownOptions(DropDownOption::Filterable), &_railtype_filter);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Rail menu.
 *
 * @param index RailType to show the build toolbar for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickBuildRail(int index)
{
	_last_built_railtype = (RailType)index;
	ShowBuildRailToolbar(_last_built_railtype);
	return CallBackFunction::None;
}

/* --- Road button menu --- */

static CallBackFunction ToolbarBuildRoadClick(Window *w)
{
	ShowDropDownList(w, GetRoadTypeDropDownList(RTTB_ROAD), _last_built_roadtype, WID_TN_ROADS, 140, GetToolbarDropDownOptions(DropDownOption::Filterable), &_roadtype_filter);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Road menu.
 *
 * @param index RoadType to show the build toolbar for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_roadtype);
	return CallBackFunction::None;
}

/* --- Tram button menu --- */

static CallBackFunction ToolbarBuildTramClick(Window *w)
{
	ShowDropDownList(w, GetRoadTypeDropDownList(RTTB_TRAM), _last_built_tramtype, WID_TN_TRAMS, 140, GetToolbarDropDownOptions(DropDownOption::Filterable), &_tramtype_filter);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Tram menu.
 *
 * @param index RoadType to show the build toolbar for.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickBuildTram(int index)
{
	_last_built_tramtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_tramtype);
	return CallBackFunction::None;
}

/* --- Water button menu --- */

static CallBackFunction ToolbarBuildWaterClick(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListIconItem(SPR_IMG_BUILD_CANAL, PAL_NONE, STR_WATERWAYS_MENU_WATERWAYS_CONSTRUCTION, 0));
	ShowDropDownList(w, std::move(list), 0, WID_TN_WATER, 140, GetToolbarDropDownOptions());
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Waterways menu.
 *
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickBuildWater(int)
{
	ShowBuildDocksToolbar();
	return CallBackFunction::None;
}

/* --- Airport button menu --- */

static CallBackFunction ToolbarBuildAirClick(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListIconItem(SPR_IMG_AIRPORT, PAL_NONE, STR_AIRCRAFT_MENU_AIRPORT_CONSTRUCTION, 0));
	ShowDropDownList(w, std::move(list), 0, WID_TN_AIR, 140, GetToolbarDropDownOptions());
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Air menu.
 *
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickBuildAir(int)
{
	ShowBuildAirToolbar();
	return CallBackFunction::None;
}

/* --- Forest button menu --- */

static CallBackFunction ToolbarForestClick(Window *w)
{
	DropDownList list;
	list.push_back(MakeDropDownListIconItem(SPR_IMG_LANDSCAPING, PAL_NONE, STR_LANDSCAPING_MENU_LANDSCAPING, 0));
	list.push_back(MakeDropDownListIconItem(SPR_IMG_PLANTTREES, PAL_NONE, STR_LANDSCAPING_MENU_PLANT_TREES, 1));
	list.push_back(MakeDropDownListIconItem(SPR_IMG_SIGN, PAL_NONE, STR_LANDSCAPING_MENU_PLACE_SIGN, 2));
	ShowDropDownList(w, std::move(list), 0, WID_TN_LANDSCAPE, 100, GetToolbarDropDownOptions());
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the landscaping menu.
 *
 * @param index Menu entry clicked.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickForest(int index)
{
	switch (index) {
		case 0: ShowTerraformToolbar();  break;
		case 1: ShowBuildTreesToolbar(); break;
		case 2: return SelectSignTool();
	}
	return CallBackFunction::None;
}

/* --- Music button menu --- */

static CallBackFunction ToolbarMusicClick(Window *w)
{
	PopupMainToolbarMenu(w, _game_mode == GM_EDITOR ? (WidgetID)WID_TE_MUSIC_SOUND : (WidgetID)WID_TN_MUSIC_SOUND, {STR_TOOLBAR_SOUND_MUSIC});
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Music menu.
 *
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickMusicWindow(int)
{
	ShowMusicWindow();
	return CallBackFunction::None;
}

/* --- Newspaper button menu --- */

static CallBackFunction ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbarMenu(w, WID_TN_MESSAGES, {STR_NEWS_MENU_LAST_MESSAGE_NEWS_REPORT, STR_NEWS_MENU_MESSAGE_HISTORY_MENU, STR_NEWS_MENU_DELETE_ALL_MESSAGES});
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Newspaper menu.
 *
 * @param index Menu entry clicked.
 * @return #CallBackFunction::None
 */
static CallBackFunction MenuClickNewspaper(int index)
{
	switch (index) {
		case 0: ShowLastNewsMessage(); break;
		case 1: ShowMessageHistory();  break;
		case 2: DeleteAllMessages();   break;
	}
	return CallBackFunction::None;
}

/* --- Help button menu --- */

static CallBackFunction PlaceLandBlockInfo()
{
	if (_last_started_action == CallBackFunction::PlaceLandInfo) {
		ResetObjectToPlace();
		return CallBackFunction::None;
	} else {
		SetObjectToPlace(SPR_CURSOR_QUERY, PAL_NONE, HT_RECT, WC_MAIN_TOOLBAR, 0);
		return CallBackFunction::PlaceLandInfo;
	}
}

static CallBackFunction ToolbarHelpClick(Window *w)
{
	if (_settings_client.gui.newgrf_developer_tools) {
		PopupMainToolbarMenu(w, _game_mode == GM_EDITOR ? (WidgetID)WID_TE_HELP : (WidgetID)WID_TN_HELP, {STR_ABOUT_MENU_LAND_BLOCK_INFO,
				STR_ABOUT_MENU_HELP, STR_NULL, STR_ABOUT_MENU_TOGGLE_CONSOLE, STR_ABOUT_MENU_AI_DEBUG,
				STR_ABOUT_MENU_SCREENSHOT, STR_ABOUT_MENU_SHOW_FRAMERATE, STR_ABOUT_MENU_ABOUT_OPENTTD,
				STR_ABOUT_MENU_SPRITE_ALIGNER, STR_ABOUT_MENU_TOGGLE_BOUNDING_BOXES, STR_ABOUT_MENU_TOGGLE_DIRTY_BLOCKS,
				STR_ABOUT_MENU_TOGGLE_WIDGET_OUTLINES});
	} else {
		PopupMainToolbarMenu(w, _game_mode == GM_EDITOR ? (WidgetID)WID_TE_HELP : (WidgetID)WID_TN_HELP, {STR_ABOUT_MENU_LAND_BLOCK_INFO,
				STR_ABOUT_MENU_HELP, STR_NULL, STR_ABOUT_MENU_TOGGLE_CONSOLE, STR_ABOUT_MENU_AI_DEBUG,
				STR_ABOUT_MENU_SCREENSHOT, STR_ABOUT_MENU_SHOW_FRAMERATE, STR_ABOUT_MENU_ABOUT_OPENTTD});
	}
	return CallBackFunction::None;
}

/**
 * Toggle drawing of sprites' bounding boxes.
 * @note has only an effect when newgrf_developer_tools are active.
 *
 * Function is found here and not in viewport.cpp in order to avoid
 * importing the settings structs to there.
 */
void ToggleBoundingBoxes()
{
	extern bool _draw_bounding_boxes;
	/* Always allow to toggle them off */
	if (_settings_client.gui.newgrf_developer_tools || _draw_bounding_boxes) {
		_draw_bounding_boxes = !_draw_bounding_boxes;
		MarkWholeScreenDirty();
	}
}

/**
 * Toggle drawing of the dirty blocks.
 * @note has only an effect when newgrf_developer_tools are active.
 *
 * Function is found here and not in viewport.cpp in order to avoid
 * importing the settings structs to there.
 */
void ToggleDirtyBlocks()
{
	extern bool _draw_dirty_blocks;
	/* Always allow to toggle them off */
	if (_settings_client.gui.newgrf_developer_tools || _draw_dirty_blocks) {
		_draw_dirty_blocks = !_draw_dirty_blocks;
		MarkWholeScreenDirty();
	}
}

/**
 * Toggle drawing of widget outlines.
 * @note has only an effect when newgrf_developer_tools are active.
 */
void ToggleWidgetOutlines()
{
	extern bool _draw_widget_outlines;
	/* Always allow to toggle them off */
	if (_settings_client.gui.newgrf_developer_tools || _draw_widget_outlines) {
		_draw_widget_outlines = !_draw_widget_outlines;
		MarkWholeScreenDirty();
	}
}

/**
 * Set the starting year for a scenario.
 * @param year New starting year.
 */
void SetStartingYear(TimerGameCalendar::Year year)
{
	_settings_game.game_creation.starting_year = Clamp(year, CalendarTime::MIN_YEAR, CalendarTime::MAX_YEAR);
	TimerGameCalendar::Date new_calendar_date = TimerGameCalendar::ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1);
	TimerGameEconomy::Date new_economy_date{new_calendar_date.base()};

	/* We must set both Calendar and Economy dates to keep them in sync. Calendar first. */
	TimerGameCalendar::SetDate(new_calendar_date, 0);

	/* If you open a savegame as a scenario, there may already be link graphs and/or vehicles. These use economy date. */
	LinkGraphSchedule::instance.ShiftDates(new_economy_date - TimerGameEconomy::date);
	for (auto v : Vehicle::Iterate()) v->ShiftDates(new_economy_date - TimerGameEconomy::date);

	/* Only change the date after changing cached values above. */
	TimerGameEconomy::SetDate(new_economy_date, 0);
}

/**
 * Choose the proper callback function for the main toolbar's help menu.
 * @param index The menu index which was selected.
 * @return CallBackFunction::None
 */
static CallBackFunction MenuClickHelp(int index)
{
	switch (index) {
		case  0: return PlaceLandBlockInfo();
		case  1: ShowHelpWindow();                 break;
		case  2: IConsoleSwitch();                 break;
		case  3: ShowScriptDebugWindow(CompanyID::Invalid(), _ctrl_pressed); break;
		case  4: ShowScreenshotWindow();           break;
		case  5: ShowFramerateWindow();            break;
		case  6: ShowAboutWindow();                break;
		case  7: ShowSpriteAlignerWindow();        break;
		case  8: ToggleBoundingBoxes();            break;
		case  9: ToggleDirtyBlocks();              break;
		case 10: ToggleWidgetOutlines();           break;
	}
	return CallBackFunction::None;
}

/* --- Switch toolbar button --- */

static CallBackFunction ToolbarSwitchClick(Window *w)
{
	if (_toolbar_mode != ToolbarMode::Lower) {
		_toolbar_mode = ToolbarMode::Lower;
	} else {
		_toolbar_mode = ToolbarMode::Upper;
	}

	w->ReInit();
	w->SetWidgetLoweredState(_game_mode == GM_EDITOR ? (WidgetID)WID_TE_SWITCH_BAR : (WidgetID)WID_TN_SWITCH_BAR, _toolbar_mode == ToolbarMode::Lower);
	SndClickBeep();
	return CallBackFunction::None;
}

/* --- Scenario editor specific handlers. */

/**
 * Called when clicking at the date panel of the scenario editor toolbar.
 * @copydoc ToolbarButtonProc
 */
static CallBackFunction ToolbarScenDatePanel(Window *w)
{
	ShowQueryString(GetString(STR_JUST_INT, _settings_game.game_creation.starting_year), STR_MAPGEN_START_DATE_QUERY_CAPT, 8, w, CS_NUMERAL, QueryStringFlag::EnableDefault);
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if (!w->flags.Test(WindowFlag::Timeout) || w->timeout_timer <= 1) {
		w->HandleButtonClick(WID_TE_DATE_BACKWARD);
		w->SetDirty();

		SetStartingYear(_settings_game.game_creation.starting_year - 1);
	}
	_left_button_clicked = false;
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenDateForward(Window *w)
{
	/* don't allow too fast scrolling */
	if (!w->flags.Test(WindowFlag::Timeout) || w->timeout_timer <= 1) {
		w->HandleButtonClick(WID_TE_DATE_FORWARD);
		w->SetDirty();

		SetStartingYear(_settings_game.game_creation.starting_year + 1);
	}
	_left_button_clicked = false;
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenGenLand(Window *w)
{
	w->HandleButtonClick(WID_TE_LAND_GENERATE);

	ShowEditorTerraformToolbar();
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenGenTownClick(Window *w)
{
	PopupMainToolbarMenu(w, WID_TE_TOWN_GENERATE, {STR_SCENEDIT_TOWN_MENU_BUILD_TOWN, STR_SCENEDIT_TOWN_MENU_PACE_HOUSE});
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenGenTown(int index)
{
	switch (index) {
		case 0: ShowFoundTownWindow(); break;
		case 1: ShowBuildHousePicker(nullptr); break;
	}
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenGenIndustry(Window *w)
{
	w->HandleButtonClick(WID_TE_INDUSTRY);
	ShowBuildIndustryWindow();
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenBuildRoadClick(Window *w)
{
	ShowDropDownList(w, GetScenRoadTypeDropDownList(RTTB_ROAD), _last_built_roadtype, WID_TE_ROADS, 140, GetToolbarDropDownOptions(DropDownOption::Filterable), &_roadtype_filter);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Road menu.
 *
 * @param index RoadType to show the build toolbar for.
 * @return #CallBackFunction::None
 */
static CallBackFunction ToolbarScenBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadScenToolbar(_last_built_roadtype);
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenBuildTramClick(Window *w)
{
	ShowDropDownList(w, GetScenRoadTypeDropDownList(RTTB_TRAM), _last_built_tramtype, WID_TE_TRAMS, 140, GetToolbarDropDownOptions(DropDownOption::Filterable), &_tramtype_filter);
	return CallBackFunction::None;
}

/**
 * Handle click on the entry in the Build Tram menu.
 *
 * @param index RoadType to show the build toolbar for.
 * @return #CallBackFunction::None
 */
static CallBackFunction ToolbarScenBuildTram(int index)
{
	_last_built_tramtype = (RoadType)index;
	ShowBuildRoadScenToolbar(_last_built_tramtype);
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenBuildDocks(Window *w)
{
	w->HandleButtonClick(WID_TE_WATER);
	ShowBuildDocksScenToolbar();
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenPlantTrees(Window *w)
{
	w->HandleButtonClick(WID_TE_TREES);
	ShowBuildTreesToolbar();
	return CallBackFunction::None;
}

static CallBackFunction ToolbarScenPlaceSign(Window *w)
{
	w->HandleButtonClick(WID_TE_SIGNS);
	return SelectSignTool();
}

static CallBackFunction ToolbarBtn_NULL(Window *)
{
	return CallBackFunction::None;
}

typedef CallBackFunction MenuClickedProc(int index);

static MenuClickedProc * const _menu_clicked_procs[] = {
	nullptr,                 // 0
	nullptr,                 // 1
	MenuClickSettings,    // 2
	MenuClickSaveLoad,    // 3
	MenuClickMap,         // 4
	MenuClickTown,        // 5
	MenuClickSubsidies,   // 6
	MenuClickStations,    // 7
	MenuClickFinances,    // 8
	MenuClickCompany,     // 9
	MenuClickStory,       // 10
	MenuClickGoal,        // 11
	MenuClickGraphsOrLeague, // 12
	MenuClickGraphsOrLeague, // 13
	MenuClickIndustry,    // 14
	MenuClickShowTrains,  // 15
	MenuClickShowRoad,    // 16
	MenuClickShowShips,   // 17
	MenuClickShowAir,     // 18
	MenuClickMap,         // 19
	nullptr,                 // 20
	MenuClickBuildRail,   // 21
	MenuClickBuildRoad,   // 22
	MenuClickBuildTram,   // 23
	MenuClickBuildWater,  // 24
	MenuClickBuildAir,    // 25
	MenuClickForest,      // 26
	MenuClickMusicWindow, // 27
	MenuClickNewspaper,   // 28
	MenuClickHelp,        // 29
};

/** Full blown container to make it behave exactly as we want :) */
class NWidgetToolbarContainer : public NWidgetContainer {
protected:
	uint spacers = 0; ///< Number of spacer widgets in this toolbar

public:
	NWidgetToolbarContainer() : NWidgetContainer(NWID_HORIZONTAL)
	{
	}

	/**
	 * Check whether the given widget type is a button for us.
	 * @param type the widget type to check.
	 * @return true if it is a button for us.
	 */
	bool IsButton(WidgetType type) const
	{
		return type == WWT_IMGBTN || type == WWT_IMGBTN_2 || type == WWT_PUSHIMGBTN;
	}

	void SetupSmallestSize(Window *w) override
	{
		this->smallest_x = 0; // Biggest child
		this->smallest_y = 0; // Biggest child
		this->fill_x = 1;
		this->fill_y = 0;
		this->resize_x = 1; // We only resize in this direction
		this->resize_y = 0; // We never resize in this direction
		this->spacers = 0;

		uint nbuttons = 0;
		/* First initialise some variables... */
		for (const auto &child_wid : this->children) {
			child_wid->SetupSmallestSize(w);
			this->smallest_y = std::max(this->smallest_y, child_wid->smallest_y + child_wid->padding.Vertical());
			if (this->IsButton(child_wid->type)) {
				nbuttons++;
				this->smallest_x = std::max(this->smallest_x, child_wid->smallest_x + child_wid->padding.Horizontal());
			} else if (child_wid->type == NWID_SPACER) {
				this->spacers++;
			}
		}

		/* ... then in a second pass make sure the 'current' heights are set. Won't change ever. */
		for (const auto &child_wid : this->children) {
			child_wid->current_y = this->smallest_y;
			if (!this->IsButton(child_wid->type)) {
				child_wid->current_x = child_wid->smallest_x;
			}
		}

		/* Exclude the switcher button which is not displayed when the toolbar fits the screen. When the switch is
		 * displayed there will be no spacers anyway. */
		--nbuttons;

		/* Allow space for all buttons, and include spacers at quarter the width of buttons. */
		_toolbar_width = nbuttons * this->smallest_x + this->spacers * this->smallest_x / 4;
	}

	void AssignSizePosition(SizingType sizing, int x, int y, uint given_width, uint given_height, bool rtl) override
	{
		assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

		this->pos_x = x;
		this->pos_y = y;
		this->current_x = given_width;
		this->current_y = given_height;

		/* Figure out what are the visible buttons */
		uint arrangeable_count, button_count, spacer_count;
		const WidgetID *arrangement = GetButtonArrangement(given_width, arrangeable_count, button_count, spacer_count);

		/* Create us ourselves a quick lookup table from WidgetID to slot. */
		std::map<WidgetID, uint> lookup;
		for (auto it = std::begin(this->children); it != std::end(this->children); ++it) {
			NWidgetBase *nwid = it->get();
			nwid->current_x = 0; /* Hide widget, it will be revealed in the next step. */
			if (nwid->type == NWID_SPACER) continue;
			NWidgetCore *nwc = dynamic_cast<NWidgetCore *>(nwid);
			assert(nwc != nullptr);
			lookup[nwc->GetIndex()] = std::distance(this->children.begin(), it);
		}

		/* Now assign the widgets to their rightful place */
		uint position = 0; // Place to put next child relative to origin of the container.
		uint spacer_space = std::max(0, (int)given_width - (int)(button_count * this->smallest_x)); // Remaining spacing for 'spacer' widgets
		uint button_space = given_width - spacer_space; // Remaining spacing for the buttons
		uint spacer_i = 0;
		uint button_i = 0;

		/* Index into the arrangement indices. */
		const WidgetID *slotp = rtl ? &arrangement[arrangeable_count - 1] : arrangement;
		for (uint i = 0; i < arrangeable_count; i++) {
			uint slot = lookup[*slotp];
			auto &child_wid = this->children[slot];
			/* If we have space to give to the spacers, do that. */
			if (spacer_space > 0 && slot > 0 && slot < this->children.size() - 1) {
				const auto &possible_spacer = this->children[slot + (rtl ? 1 : -1)];
				if (possible_spacer != nullptr && possible_spacer->type == NWID_SPACER) {
					uint add = spacer_space / (spacer_count - spacer_i);
					position += add;
					spacer_space -= add;
					spacer_i++;
				}
			}

			/* Buttons can be scaled, the others not. */
			if (this->IsButton(child_wid->type)) {
				child_wid->current_x = button_space / (button_count - button_i);
				button_space -= child_wid->current_x;
				button_i++;
			} else {
				child_wid->current_x = child_wid->smallest_x;
			}
			child_wid->AssignSizePosition(sizing, x + position, y, child_wid->current_x, this->current_y, rtl);
			position += child_wid->current_x;

			if (rtl) {
				slotp--;
			} else {
				slotp++;
			}
		}
	}

	void Draw(const Window *w) override
	{
		/* Draw brown-red toolbar bg. */
		const Rect r = this->GetCurrentRect();
		GfxFillRect(r, PC_VERY_DARK_RED);
		GfxFillRect(r, PC_DARK_RED, FILLRECT_CHECKER);

		this->NWidgetContainer::Draw(w);
	}

	/**
	 * Get the arrangement of the buttons for the toolbar.
	 * @param width the new width of the toolbar.
	 * @param arrangeable_count output of the number of visible items.
	 * @param button_count output of the number of visible buttons.
	 * @param spacer_count output of the number of spacers.
	 * @return the button configuration.
	 */
	virtual const WidgetID *GetButtonArrangement(uint &width, uint &arrangeable_count, uint &button_count, uint &spacer_count) const = 0;
};

/** Container for the 'normal' main toolbar */
class NWidgetMainToolbarContainer : public NWidgetToolbarContainer {
	const WidgetID *GetButtonArrangement(uint &width, uint &arrangeable_count, uint &button_count, uint &spacer_count) const override
	{
		static const uint SMALLEST_ARRANGEMENT = 14;
		static const uint BIGGEST_ARRANGEMENT  = 20;

		/* The number of buttons of each row of the toolbar should match the number of items which we want to be visible.
		 * The total number of buttons should be equal to arrangeable_count * 2.
		 * No bad things happen, but we could see strange behaviours if we have buttons < (arrangeable_count * 2) like a
		 * pause button appearing on the right of the lower toolbar and weird resizing of the widgets even if there is
		 * enough space.
		 */
		static const WidgetID arrange14[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_SETTINGS,
			WID_TN_SAVE,
			WID_TN_SMALL_MAP,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_GRAPHS,
			WID_TN_INDUSTRIES,
			WID_TN_MUSIC_SOUND,
			WID_TN_MESSAGES,
			WID_TN_HELP,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange15[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SMALL_MAP,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_PAUSE,
			WID_TN_SETTINGS,
			WID_TN_SMALL_MAP,
			WID_TN_SAVE,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_GRAPHS,
			WID_TN_INDUSTRIES,
			WID_TN_MUSIC_SOUND,
			WID_TN_MESSAGES,
			WID_TN_HELP,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange16[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SETTINGS,
			WID_TN_SMALL_MAP,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SAVE,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_GRAPHS,
			WID_TN_INDUSTRIES,
			WID_TN_MUSIC_SOUND,
			WID_TN_MESSAGES,
			WID_TN_HELP,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange17[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SETTINGS,
			WID_TN_SMALL_MAP,
			WID_TN_SUBSIDIES,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SAVE,
			WID_TN_SMALL_MAP,
			WID_TN_SUBSIDIES,
			WID_TN_TOWNS,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_GRAPHS,
			WID_TN_INDUSTRIES,
			WID_TN_MUSIC_SOUND,
			WID_TN_MESSAGES,
			WID_TN_HELP,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange18[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SETTINGS,
			WID_TN_SMALL_MAP,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_INDUSTRIES,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SAVE,
			WID_TN_SMALL_MAP,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_STATIONS,
			WID_TN_GRAPHS,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_MUSIC_SOUND,
			WID_TN_MESSAGES,
			WID_TN_HELP,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange19[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SETTINGS,
			WID_TN_SMALL_MAP,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_MUSIC_SOUND,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SAVE,
			WID_TN_SMALL_MAP,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_GRAPHS,
			WID_TN_INDUSTRIES,
			WID_TN_MESSAGES,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_HELP,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange20[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SETTINGS,
			WID_TN_SMALL_MAP,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_MUSIC_SOUND,
			WID_TN_GOAL,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
			/* lower toolbar */
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SAVE,
			WID_TN_SMALL_MAP,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_GRAPHS,
			WID_TN_INDUSTRIES,
			WID_TN_MESSAGES,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_STORY,
			WID_TN_HELP,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_SWITCH_BAR,
		};
		static const WidgetID arrange_all[] = {
			WID_TN_PAUSE,
			WID_TN_FAST_FORWARD,
			WID_TN_SETTINGS,
			WID_TN_SAVE,
			WID_TN_SMALL_MAP,
			WID_TN_TOWNS,
			WID_TN_SUBSIDIES,
			WID_TN_STATIONS,
			WID_TN_FINANCES,
			WID_TN_COMPANIES,
			WID_TN_STORY,
			WID_TN_GOAL,
			WID_TN_GRAPHS,
			WID_TN_LEAGUE,
			WID_TN_INDUSTRIES,
			WID_TN_TRAINS,
			WID_TN_ROADVEHS,
			WID_TN_SHIPS,
			WID_TN_AIRCRAFT,
			WID_TN_ZOOM_IN,
			WID_TN_ZOOM_OUT,
			WID_TN_RAILS,
			WID_TN_ROADS,
			WID_TN_TRAMS,
			WID_TN_WATER,
			WID_TN_AIR,
			WID_TN_LANDSCAPE,
			WID_TN_MUSIC_SOUND,
			WID_TN_MESSAGES,
			WID_TN_HELP
		};

		/* If at least BIGGEST_ARRANGEMENT fit, just spread all the buttons nicely */
		uint full_buttons = std::max(CeilDiv(width, this->smallest_x), SMALLEST_ARRANGEMENT);
		if (full_buttons > BIGGEST_ARRANGEMENT) {
			_toolbar_mode = ToolbarMode::Normal;
			button_count = arrangeable_count = lengthof(arrange_all);
			spacer_count = this->spacers;
			return arrange_all;
		}

		/* Introduce the split toolbar */
		static const WidgetID * const arrangements[] = { arrange14, arrange15, arrange16, arrange17, arrange18, arrange19, arrange20 };

		button_count = arrangeable_count = full_buttons;
		spacer_count = this->spacers;
		return arrangements[full_buttons - SMALLEST_ARRANGEMENT] + ((_toolbar_mode == ToolbarMode::Lower) ? full_buttons : 0);
	}
};

/** Container for the scenario editor's toolbar */
class NWidgetScenarioToolbarContainer : public NWidgetToolbarContainer {
	std::array<uint, 2> panel_widths{}; ///< The width of the two panels (the text panel and date panel)

	void SetupSmallestSize(Window *w) override
	{
		this->NWidgetToolbarContainer::SetupSmallestSize(w);

		/* Find the size of panel_widths */
		auto it = this->panel_widths.begin();
		for (const auto &child_wid : this->children) {
			if (child_wid->type == NWID_SPACER || this->IsButton(child_wid->type)) continue;

			assert(it != this->panel_widths.end());
			*it = child_wid->current_x;
			_toolbar_width += child_wid->current_x;
			++it;
		}
	}

	const WidgetID *GetButtonArrangement(uint &width, uint &arrangeable_count, uint &button_count, uint &spacer_count) const override
	{
		static const WidgetID arrange_all[] = {
			WID_TE_PAUSE,
			WID_TE_FAST_FORWARD,
			WID_TE_SETTINGS,
			WID_TE_SAVE,
			WID_TE_SPACER,
			WID_TE_DATE_PANEL,
			WID_TE_SMALL_MAP,
			WID_TE_ZOOM_IN,
			WID_TE_ZOOM_OUT,
			WID_TE_LAND_GENERATE,
			WID_TE_TOWN_GENERATE,
			WID_TE_INDUSTRY,
			WID_TE_ROADS,
			WID_TE_TRAMS,
			WID_TE_WATER,
			WID_TE_TREES,
			WID_TE_SIGNS,
			WID_TE_MUSIC_SOUND,
			WID_TE_HELP,
		};
		static const WidgetID arrange_nopanel[] = {
			WID_TE_PAUSE,
			WID_TE_FAST_FORWARD,
			WID_TE_SETTINGS,
			WID_TE_SAVE,
			WID_TE_DATE_PANEL,
			WID_TE_SMALL_MAP,
			WID_TE_ZOOM_IN,
			WID_TE_ZOOM_OUT,
			WID_TE_LAND_GENERATE,
			WID_TE_TOWN_GENERATE,
			WID_TE_INDUSTRY,
			WID_TE_ROADS,
			WID_TE_TRAMS,
			WID_TE_WATER,
			WID_TE_TREES,
			WID_TE_SIGNS,
			WID_TE_MUSIC_SOUND,
			WID_TE_HELP,
		};
		static const WidgetID arrange_switch[] = {
			WID_TE_DATE_PANEL,
			WID_TE_SMALL_MAP,
			WID_TE_LAND_GENERATE,
			WID_TE_TOWN_GENERATE,
			WID_TE_INDUSTRY,
			WID_TE_ROADS,
			WID_TE_TRAMS,
			WID_TE_WATER,
			WID_TE_TREES,
			WID_TE_SIGNS,
			WID_TE_SWITCH_BAR,
			/* lower toolbar */
			WID_TE_PAUSE,
			WID_TE_FAST_FORWARD,
			WID_TE_SETTINGS,
			WID_TE_SAVE,
			WID_TE_DATE_PANEL,
			WID_TE_SMALL_MAP,
			WID_TE_ZOOM_IN,
			WID_TE_ZOOM_OUT,
			WID_TE_MUSIC_SOUND,
			WID_TE_HELP,
			WID_TE_SWITCH_BAR,
		};

		/* If we can place all buttons *and* the panels, show them. */
		size_t min_full_width = (lengthof(arrange_all) - std::size(this->panel_widths)) * this->smallest_x + this->panel_widths[0] + this->panel_widths[1];
		if (width >= min_full_width) {
			width -= this->panel_widths[0] + this->panel_widths[1];
			arrangeable_count = lengthof(arrange_all);
			button_count = arrangeable_count - 2;
			spacer_count = this->spacers;
			return arrange_all;
		}

		/* Otherwise don't show the date panel and if we can't fit half the buttons and the panels anymore, split the toolbar in two */
		size_t min_small_width = (lengthof(arrange_switch) - std::size(this->panel_widths)) * this->smallest_x / 2 + this->panel_widths[1];
		if (width > min_small_width) {
			width -= this->panel_widths[1];
			arrangeable_count = lengthof(arrange_nopanel);
			button_count = arrangeable_count - 1;
			spacer_count = this->spacers - 1;
			return arrange_nopanel;
		}

		/* Split toolbar */
		width -= this->panel_widths[1];
		arrangeable_count = lengthof(arrange_switch) / 2;
		button_count = arrangeable_count - 1;
		spacer_count = 0;
		return arrange_switch + ((_toolbar_mode == ToolbarMode::Lower) ? arrangeable_count : 0);
	}
};

/* --- Toolbar handling for the 'normal' case */

/**
 * Callback for when a button is clicked in the given window.
 * @param w The clicked window.
 * @return The callback function.
 */
using ToolbarButtonProc = CallBackFunction(Window *w);

static ToolbarButtonProc * const _toolbar_button_procs[] = {
	ToolbarPauseClick,
	ToolbarFastForwardClick,
	ToolbarOptionsClick,
	ToolbarSaveClick,
	ToolbarMapClick,
	ToolbarTownClick,
	ToolbarSubsidiesClick,
	ToolbarStationsClick,
	ToolbarFinancesClick,
	ToolbarCompaniesClick,
	ToolbarStoryClick,
	ToolbarGoalClick,
	ToolbarGraphsClick,
	ToolbarLeagueClick,
	ToolbarIndustryClick,
	ToolbarTrainClick,
	ToolbarRoadClick,
	ToolbarShipClick,
	ToolbarAirClick,
	ToolbarZoomInClick,
	ToolbarZoomOutClick,
	ToolbarBuildRailClick,
	ToolbarBuildRoadClick,
	ToolbarBuildTramClick,
	ToolbarBuildWaterClick,
	ToolbarBuildAirClick,
	ToolbarForestClick,
	ToolbarMusicClick,
	ToolbarNewspaperClick,
	ToolbarHelpClick,
	ToolbarSwitchClick,
};

/** Main toolbar. */
struct MainToolbarWindow : Window {
	MainToolbarWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested(0);

		_last_started_action = CallBackFunction::None;
		this->flags.Reset(WindowFlag::WhiteBorder);
		this->SetWidgetDisabledState(WID_TN_PAUSE, _networking && !_network_server); // if not server, disable pause button
		this->SetWidgetDisabledState(WID_TN_FAST_FORWARD, _networking); // if networking, disable fast-forward button
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	void FindWindowPlacementAndResize(int, int def_height, bool allow_resize) override
	{
		Window::FindWindowPlacementAndResize(_toolbar_width, def_height, allow_resize);
	}

	void OnPaint() override
	{
		/* If spectator, disable all construction buttons
		 * ie : Build road, rail, ships, airports and landscaping
		 * Since enabled state is the default, just disable when needed */
		this->SetWidgetsDisabledState(_local_company == COMPANY_SPECTATOR, WID_TN_RAILS, WID_TN_ROADS, WID_TN_TRAMS, WID_TN_WATER, WID_TN_AIR, WID_TN_LANDSCAPE);
		/* disable company list drop downs, if there are no companies */
		this->SetWidgetsDisabledState(Company::GetNumItems() == 0, WID_TN_STATIONS, WID_TN_FINANCES, WID_TN_TRAINS, WID_TN_ROADVEHS, WID_TN_SHIPS, WID_TN_AIRCRAFT);

		this->SetWidgetDisabledState(WID_TN_GOAL, Goal::GetNumItems() == 0);
		this->SetWidgetDisabledState(WID_TN_STORY, StoryPage::GetNumItems() == 0);

		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (_game_mode != GM_MENU && !this->IsWidgetDisabled(widget)) _toolbar_button_procs[widget](this);
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		CallBackFunction cbf = _menu_clicked_procs[widget](index);
		if (cbf != CallBackFunction::None) _last_started_action = cbf;
	}

	EventState OnHotkey(int hotkey) override
	{
		CallBackFunction cbf = CallBackFunction::None;
		switch (hotkey) {
			case MTHK_PAUSE: ToolbarPauseClick(this); break;
			case MTHK_FASTFORWARD: ToolbarFastForwardClick(this); break;
			case MTHK_SETTINGS: ShowGameOptions(); break;
			case MTHK_SAVEGAME: MenuClickSaveLoad(); break;
			case MTHK_LOADGAME: ShowSaveLoadDialog(AbstractFileType::Savegame, SaveLoadOperation::Load); break;
			case MTHK_SMALLMAP: ShowSmallMap(); break;
			case MTHK_TOWNDIRECTORY: ShowTownDirectory(); break;
			case MTHK_SUBSIDIES: ShowSubsidiesList(); break;
			case MTHK_STATIONS: ShowCompanyStations(_local_company); break;
			case MTHK_FINANCES: ShowCompanyFinances(_local_company); break;
			case MTHK_COMPANIES: ShowCompany(_local_company); break;
			case MTHK_STORY: ShowStoryBook(_local_company); break;
			case MTHK_GOAL: ShowGoalsList(_local_company); break;
			case MTHK_GRAPHS: ShowOperatingProfitGraph(); break;
			case MTHK_LEAGUE: ShowFirstLeagueTable(); break;
			case MTHK_INDUSTRIES: ShowBuildIndustryWindow(); break;
			case MTHK_TRAIN_LIST: ShowVehicleListWindow(_local_company, VEH_TRAIN); break;
			case MTHK_ROADVEH_LIST: ShowVehicleListWindow(_local_company, VEH_ROAD); break;
			case MTHK_SHIP_LIST: ShowVehicleListWindow(_local_company, VEH_SHIP); break;
			case MTHK_AIRCRAFT_LIST: ShowVehicleListWindow(_local_company, VEH_AIRCRAFT); break;
			case MTHK_ZOOM_IN: ToolbarZoomInClick(this); break;
			case MTHK_ZOOM_OUT: ToolbarZoomOutClick(this); break;
			case MTHK_BUILD_RAIL: ShowBuildRailToolbar(_last_built_railtype); break;
			case MTHK_BUILD_ROAD: ShowBuildRoadToolbar(_last_built_roadtype); break;
			case MTHK_BUILD_TRAM: ShowBuildRoadToolbar(_last_built_tramtype); break;
			case MTHK_BUILD_DOCKS: ShowBuildDocksToolbar(); break;
			case MTHK_BUILD_AIRPORT: ShowBuildAirToolbar(); break;
			case MTHK_BUILD_TREES: ShowBuildTreesToolbar(); break;
			case MTHK_MUSIC: ShowMusicWindow(); break;
			case MTHK_SCRIPT_DEBUG: ShowScriptDebugWindow(); break;
			case MTHK_SMALL_SCREENSHOT: MakeScreenshotWithConfirm(SC_VIEWPORT); break;
			case MTHK_ZOOMEDIN_SCREENSHOT: MakeScreenshotWithConfirm(SC_ZOOMEDIN); break;
			case MTHK_DEFAULTZOOM_SCREENSHOT: MakeScreenshotWithConfirm(SC_DEFAULTZOOM); break;
			case MTHK_GIANT_SCREENSHOT: MakeScreenshotWithConfirm(SC_WORLD); break;
			case MTHK_CHEATS: if (!_networking) ShowCheatWindow(); break;
			case MTHK_TERRAFORM: ShowTerraformToolbar(); break;
			case MTHK_EXTRA_VIEWPORT: ShowExtraViewportWindowForTileUnderCursor(); break;
			case MTHK_CLIENT_LIST: if (_networking) ShowClientList(); break;
			case MTHK_SIGN_LIST: ShowSignList(); break;
			case MTHK_LANDINFO: cbf = PlaceLandBlockInfo(); break;
			default: return ES_NOT_HANDLED;
		}
		if (cbf != CallBackFunction::None) _last_started_action = cbf;
		return ES_HANDLED;
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (_last_started_action) {
			case CallBackFunction::PlaceSign:
				PlaceProc_Sign(tile);
				break;

			case CallBackFunction::PlaceLandInfo:
				ShowLandInfo(tile);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceObjectAbort() override
	{
		_last_started_action = CallBackFunction::None;
	}

	/** Refresh the state of pause / game-speed on a regular interval.*/
	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::milliseconds(30), [this](auto) {
		if (this->IsWidgetLowered(WID_TN_PAUSE) != _pause_mode.Any()) {
			this->ToggleWidgetLoweredState(WID_TN_PAUSE);
			this->SetWidgetDirty(WID_TN_PAUSE);
		}

		if (this->IsWidgetLowered(WID_TN_FAST_FORWARD) != (_game_speed != 100)) {
			this->ToggleWidgetLoweredState(WID_TN_FAST_FORWARD);
			this->SetWidgetDirty(WID_TN_FAST_FORWARD);
		}
	}};

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		HandleZoomMessage(this, *GetMainWindow()->viewport, WID_TN_ZOOM_IN, WID_TN_ZOOM_OUT);
	}

	static inline HotkeyList hotkeys{"maintoolbar", {
		Hotkey({WKC_F1, WKC_PAUSE}, "pause", MTHK_PAUSE),
		Hotkey(0, "fastforward", MTHK_FASTFORWARD),
		Hotkey(WKC_F2, "settings", MTHK_SETTINGS),
		Hotkey(WKC_F3, "saveload", MTHK_SAVEGAME),
		Hotkey(0, "load_game", MTHK_LOADGAME),
		Hotkey({WKC_F4, 'M'}, "smallmap", MTHK_SMALLMAP),
		Hotkey(WKC_F5, "town_list", MTHK_TOWNDIRECTORY),
		Hotkey(WKC_F6, "subsidies", MTHK_SUBSIDIES),
		Hotkey(WKC_F7, "station_list", MTHK_STATIONS),
		Hotkey(WKC_F8, "finances", MTHK_FINANCES),
		Hotkey(WKC_F9, "companies", MTHK_COMPANIES),
		Hotkey(0, "story_book", MTHK_STORY),
		Hotkey(0, "goal_list", MTHK_GOAL),
		Hotkey(WKC_F10, "graphs", MTHK_GRAPHS),
		Hotkey(WKC_F11, "league", MTHK_LEAGUE),
		Hotkey(WKC_F12, "industry_list", MTHK_INDUSTRIES),
		Hotkey(WKC_SHIFT | WKC_F1, "train_list", MTHK_TRAIN_LIST),
		Hotkey(WKC_SHIFT | WKC_F2, "roadveh_list", MTHK_ROADVEH_LIST),
		Hotkey(WKC_SHIFT | WKC_F3, "ship_list", MTHK_SHIP_LIST),
		Hotkey(WKC_SHIFT | WKC_F4, "aircraft_list", MTHK_AIRCRAFT_LIST),
		Hotkey({WKC_NUM_PLUS, WKC_EQUALS, WKC_SHIFT | WKC_EQUALS, WKC_SHIFT | WKC_F5}, "zoomin", MTHK_ZOOM_IN),
		Hotkey({WKC_NUM_MINUS, WKC_MINUS, WKC_SHIFT | WKC_MINUS, WKC_SHIFT | WKC_F6}, "zoomout", MTHK_ZOOM_OUT),
		Hotkey(WKC_SHIFT | WKC_F7, "build_rail", MTHK_BUILD_RAIL),
		Hotkey(WKC_SHIFT | WKC_F8, "build_road", MTHK_BUILD_ROAD),
		Hotkey(0, "build_tram", MTHK_BUILD_TRAM),
		Hotkey(WKC_SHIFT | WKC_F9, "build_docks", MTHK_BUILD_DOCKS),
		Hotkey(WKC_SHIFT | WKC_F10, "build_airport", MTHK_BUILD_AIRPORT),
		Hotkey(WKC_SHIFT | WKC_F11, "build_trees", MTHK_BUILD_TREES),
		Hotkey(WKC_SHIFT | WKC_F12, "music", MTHK_MUSIC),
		Hotkey(0, "ai_debug", MTHK_SCRIPT_DEBUG),
		Hotkey(WKC_CTRL  | 'S', "small_screenshot", MTHK_SMALL_SCREENSHOT),
		Hotkey(WKC_CTRL  | 'P', "zoomedin_screenshot", MTHK_ZOOMEDIN_SCREENSHOT),
		Hotkey(WKC_CTRL  | 'D', "defaultzoom_screenshot", MTHK_DEFAULTZOOM_SCREENSHOT),
		Hotkey(0, "giant_screenshot", MTHK_GIANT_SCREENSHOT),
		Hotkey(WKC_CTRL | WKC_ALT | 'C', "cheats", MTHK_CHEATS),
		Hotkey('L', "terraform", MTHK_TERRAFORM),
		Hotkey('V', "extra_viewport", MTHK_EXTRA_VIEWPORT),
		Hotkey(0, "client_list", MTHK_CLIENT_LIST),
		Hotkey(0, "sign_list", MTHK_SIGN_LIST),
		Hotkey(0, "land_info", MTHK_LANDINFO),
	}};
};

/** Sprites to use for the different toolbar buttons */
static constexpr std::tuple<WidgetID, WidgetType, SpriteID> _toolbar_button_sprites[] = {
	{WID_TN_PAUSE,        WWT_IMGBTN,     SPR_IMG_PAUSE},
	{WID_TN_FAST_FORWARD, WWT_IMGBTN,     SPR_IMG_FASTFORWARD},
	{WID_TN_SETTINGS,     WWT_IMGBTN,     SPR_IMG_SETTINGS},
	{WID_TN_SAVE,         WWT_IMGBTN_2,   SPR_IMG_SAVE},
	{WID_TN_SMALL_MAP,    WWT_IMGBTN,     SPR_IMG_SMALLMAP},
	{WID_TN_TOWNS,        WWT_IMGBTN,     SPR_IMG_TOWN},
	{WID_TN_SUBSIDIES,    WWT_IMGBTN,     SPR_IMG_SUBSIDIES},
	{WID_TN_STATIONS,     WWT_IMGBTN,     SPR_IMG_COMPANY_LIST},
	{WID_TN_FINANCES,     WWT_IMGBTN,     SPR_IMG_COMPANY_FINANCE},
	{WID_TN_COMPANIES,    WWT_IMGBTN,     SPR_IMG_COMPANY_GENERAL},
	{WID_TN_STORY,        WWT_IMGBTN,     SPR_IMG_STORY_BOOK},
	{WID_TN_GOAL,         WWT_IMGBTN,     SPR_IMG_GOAL},
	{WID_TN_GRAPHS,       WWT_IMGBTN,     SPR_IMG_GRAPHS},
	{WID_TN_LEAGUE,       WWT_IMGBTN,     SPR_IMG_COMPANY_LEAGUE},
	{WID_TN_INDUSTRIES,   WWT_IMGBTN,     SPR_IMG_INDUSTRY},
	{WID_TN_TRAINS,       WWT_IMGBTN,     SPR_IMG_TRAINLIST},
	{WID_TN_ROADVEHS,     WWT_IMGBTN,     SPR_IMG_TRUCKLIST},
	{WID_TN_SHIPS,        WWT_IMGBTN,     SPR_IMG_SHIPLIST},
	{WID_TN_AIRCRAFT,     WWT_IMGBTN,     SPR_IMG_AIRPLANESLIST},
	{WID_TN_ZOOM_IN,      WWT_PUSHIMGBTN, SPR_IMG_ZOOMIN},
	{WID_TN_ZOOM_OUT,     WWT_PUSHIMGBTN, SPR_IMG_ZOOMOUT},
	{WID_TN_RAILS,        WWT_IMGBTN,     SPR_IMG_BUILDRAIL},
	{WID_TN_ROADS,        WWT_IMGBTN,     SPR_IMG_BUILDROAD},
	{WID_TN_TRAMS,        WWT_IMGBTN,     SPR_IMG_BUILDTRAMS},
	{WID_TN_WATER,        WWT_IMGBTN,     SPR_IMG_BUILDWATER},
	{WID_TN_AIR,          WWT_IMGBTN,     SPR_IMG_BUILDAIR},
	{WID_TN_LANDSCAPE,    WWT_IMGBTN,     SPR_IMG_LANDSCAPING},
	{WID_TN_MUSIC_SOUND,  WWT_IMGBTN,     SPR_IMG_MUSIC},
	{WID_TN_MESSAGES,     WWT_IMGBTN,     SPR_IMG_MESSAGES},
	{WID_TN_HELP,         WWT_IMGBTN,     SPR_IMG_QUERY},
	{WID_TN_SWITCH_BAR,   WWT_IMGBTN,     SPR_IMG_SWITCH_TOOLBAR},
};

/**
 * Get maximal square size of a toolbar image.
 * @return maximal toolbar image size.
 */
Dimension GetToolbarMaximalImageSize()
{
	Dimension d{};
	for (const auto &[widget, tp, sprite] : _toolbar_button_sprites) {
		if (!SpriteExists(sprite)) continue;
		d = maxdim(d, GetSquareScaledSpriteSize(sprite));
	}
	return d;
}

/**
 * Make widgets for the main toolbar.
 * @return widgets for the main toolbar.
 */
static std::unique_ptr<NWidgetBase> MakeMainToolbar()
{
	auto hor = std::make_unique<NWidgetMainToolbarContainer>();
	for (const auto &[widget, tp, sprite] : _toolbar_button_sprites) {
		switch (widget) {
			case WID_TN_SMALL_MAP:
			case WID_TN_FINANCES:
			case WID_TN_VEHICLE_START:
			case WID_TN_ZOOM_IN:
			case WID_TN_BUILDING_TOOLS_START:
			case WID_TN_MUSIC_SOUND:
				hor->Add(std::make_unique<NWidgetSpacer>(0, 0));
				break;
		}
		auto leaf = std::make_unique<NWidgetLeaf>(tp, COLOUR_GREY, widget, WidgetData{.sprite = sprite}, STR_TOOLBAR_TOOLTIP_PAUSE_GAME + widget);
		leaf->SetToolbarMinimalSize(1);
		hor->Add(std::move(leaf));
	}

	return hor;
}

static constexpr std::initializer_list<NWidgetPart> _nested_toolbar_normal_widgets = {
	NWidgetFunction(MakeMainToolbar),
};

static WindowDesc _toolb_normal_desc(
	WDP_MANUAL, {}, 0, 0,
	WC_MAIN_TOOLBAR, WC_NONE,
	{WindowDefaultFlag::NoFocus, WindowDefaultFlag::NoClose},
	_nested_toolbar_normal_widgets,
	&MainToolbarWindow::hotkeys
);


/* --- Toolbar handling for the scenario editor */

static MenuClickedProc * const _scen_toolbar_dropdown_procs[] = {
	nullptr,              // 0
	nullptr,              // 1
	MenuClickSettings,    // 2
	MenuClickSaveLoad,    // 3
	nullptr,              // 4
	nullptr,              // 5
	nullptr,              // 6
	nullptr,              // 7
	MenuClickMap,         // 8
	nullptr,              // 9
	nullptr,              // 10
	nullptr,              // 11
	ToolbarScenGenTown,   // 12
	nullptr,              // 13
	ToolbarScenBuildRoad, // 14
	ToolbarScenBuildTram, // 15
	nullptr,              // 16
	nullptr,              // 17
	nullptr,              // 18
	nullptr,              // 19
	MenuClickMusicWindow, // 20
	MenuClickHelp,        // 21
	nullptr,              // 22
};

static ToolbarButtonProc * const _scen_toolbar_button_procs[] = {
	ToolbarPauseClick,
	ToolbarFastForwardClick,
	ToolbarOptionsClick,
	ToolbarScenSaveOrLoad,
	ToolbarBtn_NULL,
	ToolbarScenDatePanel,
	ToolbarScenDateBackward,
	ToolbarScenDateForward,
	ToolbarScenMapTownDir,
	ToolbarZoomInClick,
	ToolbarZoomOutClick,
	ToolbarScenGenLand,
	ToolbarScenGenTownClick,
	ToolbarScenGenIndustry,
	ToolbarScenBuildRoadClick,
	ToolbarScenBuildTramClick,
	ToolbarScenBuildDocks,
	ToolbarScenPlantTrees,
	ToolbarScenPlaceSign,
	ToolbarBtn_NULL,
	ToolbarMusicClick,
	ToolbarHelpClick,
	ToolbarSwitchClick,
};

/**
 * List of hotkeys available in scenario editor.
 * @note Hotkeys that behave like toolbar buttons must have the same value as corresponding widget.
 *       Other hotkeys have to have SPECIAL_HOTKEY_BIT set.
 */
enum class MainToolbarEditorHotkeys : int32_t {
	Pause = WID_TE_PAUSE, ///< (Un)pause the game.
	FastForward = WID_TE_FAST_FORWARD, ///< Toggle the fast-forward mode.
	ZoomIn = WID_TE_ZOOM_IN, ///< Zoom in.
	ZoomOut = WID_TE_ZOOM_OUT, ///< Zoom out.
	GenerateLand = WID_TE_LAND_GENERATE, ///< Open land generation window.
	GenerateIndustry = WID_TE_INDUSTRY, ///< Open industry funding window.
	BuildWater = WID_TE_WATER, ///< Open window for building locks, canals, rivers and aqueducts.
	BuildTrees = WID_TE_TREES, ///< Open tree selection window.
	Sign = WID_TE_SIGNS, ///< Toggle sign creation tool.
	Settings = 1 << SPECIAL_HOTKEY_BIT, ///< Open game options window.
	SaveGame, ///< Open save scenario window.
	Music, ///< Open sound end music window.
	LandInfo, ///< Toggle land info tool.
	SmallScreenshot, ///< Take small screenshot.
	ZoomedInScreenshot, ///< Take zoomed in screenshot.
	DefaultZoomScreenshot, ///< Take screenshot with default zoom.
	GiantScreenshot, ///< Take detailed screenshot of whole map.
	Terraform, ///< Open land generation window.
	SmallMap, ///< Open small map window.
	ExtraViewport, ///< Open new extra viewport window.
	GenerateTown, ///< Open town generation window.
	BuildRoad, ///< Open toolbar window with tools for building roads.
	BuildTram, ///< Open toolbar window with tools for building tramways.
};

struct ScenarioEditorToolbarWindow : Window {
	ScenarioEditorToolbarWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested(0);

		_last_started_action = CallBackFunction::None;
		this->flags.Reset(WindowFlag::WhiteBorder);
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	void FindWindowPlacementAndResize(int, int def_height, bool allow_resize) override
	{
		Window::FindWindowPlacementAndResize(_toolbar_width, def_height, allow_resize);
	}

	void OnPaint() override
	{
		this->SetWidgetDisabledState(WID_TE_DATE_BACKWARD, _settings_game.game_creation.starting_year <= CalendarTime::MIN_YEAR);
		this->SetWidgetDisabledState(WID_TE_DATE_FORWARD, _settings_game.game_creation.starting_year >= CalendarTime::MAX_YEAR);
		this->SetWidgetDisabledState(WID_TE_ROADS, !GetRoadTypes(true).Any(GetMaskForRoadTramType(RTT_ROAD)));
		this->SetWidgetDisabledState(WID_TE_TRAMS, !GetRoadTypes(true).Any(GetMaskForRoadTramType(RTT_TRAM)));

		this->DrawWidgets();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_TE_DATE:
				return GetString(STR_JUST_DATE_LONG, TimerGameCalendar::ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TE_SPACER: {
				int height = r.Height();
				if (height > 2 * GetCharacterHeight(FS_NORMAL)) {
					DrawString(r.left, r.right, height / 2 - GetCharacterHeight(FS_NORMAL), STR_SCENEDIT_TOOLBAR_OPENTTD, TC_FROMSTRING, SA_HOR_CENTER);
					DrawString(r.left, r.right, height / 2, STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR, TC_FROMSTRING, SA_HOR_CENTER);
				} else {
					DrawString(r.left, r.right, (height - GetCharacterHeight(FS_NORMAL)) / 2, STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR, TC_FROMSTRING, SA_HOR_CENTER);
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TE_SPACER:
				size.width = std::max(GetStringBoundingBox(STR_SCENEDIT_TOOLBAR_OPENTTD).width, GetStringBoundingBox(STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR).width) + padding.width;
				break;

			case WID_TE_DATE:
				size = GetStringBoundingBox(GetString(STR_JUST_DATE_LONG, TimerGameCalendar::ConvertYMDToDate(CalendarTime::MAX_YEAR, 0, 1)));
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (_game_mode == GM_MENU) return;
		CallBackFunction cbf = _scen_toolbar_button_procs[widget](this);
		if (cbf != CallBackFunction::None) _last_started_action = cbf;
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		CallBackFunction cbf = _scen_toolbar_dropdown_procs[widget](index);
		if (cbf != CallBackFunction::None) _last_started_action = cbf;
		SndClickBeep();
	}

	EventState OnHotkey(int hotkey) override
	{
		if (IsSpecialHotkey(hotkey)) {
			CallBackFunction cbf = CallBackFunction::None;
			switch (MainToolbarEditorHotkeys(hotkey)) {
				case MainToolbarEditorHotkeys::Settings: ShowGameOptions(); break;
				case MainToolbarEditorHotkeys::SaveGame: MenuClickSaveLoad(); break;
				case MainToolbarEditorHotkeys::Music: ShowMusicWindow(); break;
				case MainToolbarEditorHotkeys::LandInfo: cbf = PlaceLandBlockInfo(); break;
				case MainToolbarEditorHotkeys::SmallScreenshot: MakeScreenshotWithConfirm(SC_VIEWPORT); break;
				case MainToolbarEditorHotkeys::ZoomedInScreenshot: MakeScreenshotWithConfirm(SC_ZOOMEDIN); break;
				case MainToolbarEditorHotkeys::DefaultZoomScreenshot: MakeScreenshotWithConfirm(SC_DEFAULTZOOM); break;
				case MainToolbarEditorHotkeys::GiantScreenshot: MakeScreenshotWithConfirm(SC_WORLD); break;
				case MainToolbarEditorHotkeys::Terraform: ShowEditorTerraformToolbar(); break;
				case MainToolbarEditorHotkeys::SmallMap: ShowSmallMap(); break;
				case MainToolbarEditorHotkeys::ExtraViewport: ShowExtraViewportWindowForTileUnderCursor(); break;
				case MainToolbarEditorHotkeys::GenerateTown: ShowFoundTownWindow(); break;
				case MainToolbarEditorHotkeys::BuildRoad: ShowBuildRoadScenToolbar(_last_built_roadtype); break;
				case MainToolbarEditorHotkeys::BuildTram: ShowBuildRoadScenToolbar(_last_built_tramtype); break;
				default: return ES_NOT_HANDLED;
			}
			if (cbf != CallBackFunction::None) _last_started_action = cbf;
		} else {
			this->OnClick({}, hotkey, 0);
		}
		return ES_HANDLED;
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		switch (_last_started_action) {
			case CallBackFunction::PlaceSign:
				PlaceProc_Sign(tile);
				break;

			case CallBackFunction::PlaceLandInfo:
				ShowLandInfo(tile);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceObjectAbort() override
	{
		_last_started_action = CallBackFunction::None;
	}

	void OnTimeout() override
	{
		this->SetWidgetsLoweredState(false, WID_TE_DATE_BACKWARD, WID_TE_DATE_FORWARD);
		this->SetWidgetDirty(WID_TE_DATE_BACKWARD);
		this->SetWidgetDirty(WID_TE_DATE_FORWARD);
	}

	/** Refresh the state of pause / game-speed on a regular interval.*/
	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::milliseconds(30), [this](auto) {
		if (this->IsWidgetLowered(WID_TE_PAUSE) != _pause_mode.Any()) {
			this->ToggleWidgetLoweredState(WID_TE_PAUSE);
			this->SetDirty();
		}

		if (this->IsWidgetLowered(WID_TE_FAST_FORWARD) != (_game_speed != 100)) {
			this->ToggleWidgetLoweredState(WID_TE_FAST_FORWARD);
			this->SetDirty();
		}
	}};

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		HandleZoomMessage(this, *GetMainWindow()->viewport, WID_TE_ZOOM_IN, WID_TE_ZOOM_OUT);
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		/* Was 'cancel' pressed? */
		if (!str.has_value()) return;

		TimerGameCalendar::Year value;
		if (!str->empty()) {
			auto val = ParseInteger(*str, 10, true);
			if (!val.has_value()) return;
			value = static_cast<TimerGameCalendar::Year>(*val);
		} else {
			/* An empty string means revert to the default */
			value = TimerGameCalendar::Year{CalendarTime::DEF_START_YEAR.base()};
		}
		SetStartingYear(value);

		this->SetDirty();
	}

	static inline HotkeyList hotkeys{"scenedit_maintoolbar", {
		Hotkey({WKC_F1, WKC_PAUSE}, "pause", MainToolbarEditorHotkeys::Pause),
		Hotkey(0, "fastforward", MainToolbarEditorHotkeys::FastForward),
		Hotkey(WKC_F2, "settings", MainToolbarEditorHotkeys::Settings),
		Hotkey(WKC_F3, "saveload", MainToolbarEditorHotkeys::SaveGame),
		Hotkey(WKC_F4, "gen_land", MainToolbarEditorHotkeys::GenerateLand),
		Hotkey(WKC_F5, "gen_town", MainToolbarEditorHotkeys::GenerateTown),
		Hotkey(WKC_F6, "gen_industry", MainToolbarEditorHotkeys::GenerateIndustry),
		Hotkey(WKC_F7, "build_road", MainToolbarEditorHotkeys::BuildRoad),
		Hotkey(0, "build_tram", MainToolbarEditorHotkeys::BuildTram),
		Hotkey(WKC_F8, "build_docks", MainToolbarEditorHotkeys::BuildWater),
		Hotkey(WKC_F9, "build_trees", MainToolbarEditorHotkeys::BuildTrees),
		Hotkey(WKC_F10, "build_sign", MainToolbarEditorHotkeys::Sign),
		Hotkey(WKC_F11, "music", MainToolbarEditorHotkeys::Music),
		Hotkey(WKC_F12, "land_info", MainToolbarEditorHotkeys::LandInfo),
		Hotkey(WKC_CTRL | 'S', "small_screenshot", MainToolbarEditorHotkeys::SmallScreenshot),
		Hotkey(WKC_CTRL | 'P', "zoomedin_screenshot", MainToolbarEditorHotkeys::ZoomedInScreenshot),
		Hotkey(WKC_CTRL | 'D', "defaultzoom_screenshot", MainToolbarEditorHotkeys::DefaultZoomScreenshot),
		Hotkey(0, "giant_screenshot", MainToolbarEditorHotkeys::GiantScreenshot),
		Hotkey({WKC_NUM_PLUS, WKC_EQUALS, WKC_SHIFT | WKC_EQUALS, WKC_SHIFT | WKC_F5}, "zoomin", MainToolbarEditorHotkeys::ZoomIn),
		Hotkey({WKC_NUM_MINUS, WKC_MINUS, WKC_SHIFT | WKC_MINUS, WKC_SHIFT | WKC_F6}, "zoomout", MainToolbarEditorHotkeys::ZoomOut),
		Hotkey('L', "terraform", MainToolbarEditorHotkeys::Terraform),
		Hotkey('M', "smallmap", MainToolbarEditorHotkeys::SmallMap),
		Hotkey('V', "extra_viewport", MainToolbarEditorHotkeys::ExtraViewport),
	}};
};

static constexpr std::initializer_list<NWidgetPart> _nested_toolb_scen_inner_widgets = {
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_PAUSE), SetSpriteTip(SPR_IMG_PAUSE, STR_TOOLBAR_TOOLTIP_PAUSE_GAME),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_FAST_FORWARD), SetSpriteTip(SPR_IMG_FASTFORWARD, STR_TOOLBAR_TOOLTIP_FORWARD),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_SETTINGS), SetSpriteTip(SPR_IMG_SETTINGS, STR_TOOLBAR_TOOLTIP_OPTIONS),
	NWidget(WWT_IMGBTN_2, COLOUR_GREY, WID_TE_SAVE), SetSpriteTip(SPR_IMG_SAVE, STR_SCENEDIT_TOOLBAR_SAVE_SCENARIO_LOAD_SCENARIO_TOOLTIP),
	NWidget(NWID_SPACER),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_TE_SPACER), EndContainer(),
	NWidget(NWID_SPACER),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_TE_DATE_PANEL),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 2, 2), SetPadding(1),
			NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_DATE_BACKWARD), SetSpriteTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_BACKWARD_TOOLTIP), SetFill(0, 1),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_TE_DATE), SetToolTip(STR_SCENEDIT_TOOLBAR_SET_DATE_TOOLTIP), SetTextStyle(TC_WHITE), SetAlignment(SA_CENTER), SetFill(0, 1),
			NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_DATE_FORWARD), SetSpriteTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_MOVE_THE_STARTING_DATE_FORWARD_TOOLTIP), SetFill(0, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_SPACER),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_SMALL_MAP), SetSpriteTip(SPR_IMG_SMALLMAP, STR_SCENEDIT_TOOLBAR_DISPLAY_MAP_TOWN_DIRECTORY_TOOLTIP),
	NWidget(NWID_SPACER),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_ZOOM_IN), SetSpriteTip(SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_ZOOM_OUT), SetSpriteTip(SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT),
	NWidget(NWID_SPACER),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_LAND_GENERATE), SetSpriteTip(SPR_IMG_LANDSCAPING, STR_SCENEDIT_TOOLBAR_LANDSCAPE_GENERATION_TOOLTIP),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_TOWN_GENERATE), SetSpriteTip(SPR_IMG_TOWN, STR_SCENEDIT_TOOLBAR_TOWN_GENERATION_TOOLTIP),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_INDUSTRY), SetSpriteTip(SPR_IMG_INDUSTRY, STR_SCENEDIT_TOOLBAR_INDUSTRY_GENERATION_TOOLTIP),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_ROADS), SetSpriteTip(SPR_IMG_BUILDROAD, STR_SCENEDIT_TOOLBAR_ROAD_CONSTRUCTION_TOOLTIP),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_TRAMS), SetSpriteTip(SPR_IMG_BUILDTRAMS, STR_SCENEDIT_TOOLBAR_TRAM_CONSTRUCTION_TOOLTIP),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_WATER), SetSpriteTip(SPR_IMG_BUILDWATER, STR_TOOLBAR_TOOLTIP_BUILD_SHIP_DOCKS),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_TREES), SetSpriteTip(SPR_IMG_PLANTTREES, STR_SCENEDIT_TOOLBAR_PLANT_TREES_TOOLTIP),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_TE_SIGNS), SetSpriteTip(SPR_IMG_SIGN, STR_SCENEDIT_TOOLBAR_PLACE_SIGN_TOOLTIP),
	NWidget(NWID_SPACER),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_MUSIC_SOUND), SetSpriteTip(SPR_IMG_MUSIC, STR_TOOLBAR_TOOLTIP_SHOW_SOUND_MUSIC_WINDOW),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_HELP), SetSpriteTip(SPR_IMG_QUERY, STR_TOOLBAR_TOOLTIP_LAND_BLOCK_INFORMATION),
	NWidget(WWT_IMGBTN, COLOUR_GREY, WID_TE_SWITCH_BAR), SetSpriteTip(SPR_IMG_SWITCH_TOOLBAR, STR_TOOLBAR_TOOLTIP_SWITCH_TOOLBAR),
};

static std::unique_ptr<NWidgetBase> MakeScenarioToolbar()
{
	return MakeNWidgets(_nested_toolb_scen_inner_widgets, std::make_unique<NWidgetScenarioToolbarContainer>());
}

static constexpr std::initializer_list<NWidgetPart> _nested_toolb_scen_widgets = {
	NWidgetFunction(MakeScenarioToolbar),
};

static WindowDesc _toolb_scen_desc(
	WDP_MANUAL, {}, 0, 0,
	WC_MAIN_TOOLBAR, WC_NONE,
	{WindowDefaultFlag::NoFocus, WindowDefaultFlag::NoClose},
	_nested_toolb_scen_widgets,
	&ScenarioEditorToolbarWindow::hotkeys
);

/** Allocate the toolbar. */
void AllocateToolbar()
{
	/* Clean old GUI values; railtype is (re)set by rail_gui.cpp */
	_last_built_roadtype = ROADTYPE_ROAD;
	_last_built_tramtype = ROADTYPE_TRAM;

	if (_game_mode == GM_EDITOR) {
		new ScenarioEditorToolbarWindow(_toolb_scen_desc);
	} else {
		new MainToolbarWindow(_toolb_normal_desc);
	}
}
