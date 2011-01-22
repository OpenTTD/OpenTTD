/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file toolbar_gui.cpp Code related to the (main) toolbar. */

#include "stdafx.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "viewport_func.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "rail_gui.h"
#include "road_gui.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "terraform_gui.h"
#include "transparency.h"
#include "strings_func.h"
#include "company_func.h"
#include "company_gui.h"
#include "vehicle_base.h"
#include "cheat_func.h"
#include "transparency_gui.h"
#include "screenshot.h"
#include "signs_func.h"
#include "fios.h"
#include "functions.h"
#include "console_gui.h"
#include "news_gui.h"
#include "ai/ai_gui.hpp"
#include "tilehighlight_func.h"
#include "smallmap_gui.h"
#include "graph_gui.h"
#include "textbuf_gui.h"
#include "newgrf_debug.h"
#include "hotkeys.h"
#include "engine_base.h"

#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"

#include "table/strings.h"
#include "table/sprites.h"

RailType _last_built_railtype;
RoadType _last_built_roadtype;

enum ToolbarMode {
	TB_NORMAL,
	TB_UPPER,
	TB_LOWER
};

/** Callback functions. */
enum CallBackFunction {
	CBF_NONE,
	CBF_PLACE_SIGN,
	CBF_PLACE_LANDINFO,
};

enum ToolbarNormalWidgets {
	TBN_PAUSE         = 0,
	TBN_FASTFORWARD,
	TBN_SETTINGS,
	TBN_SAVEGAME,
	TBN_SMALLMAP,
	TBN_TOWNDIRECTORY,
	TBN_SUBSIDIES,
	TBN_STATIONS,
	TBN_FINANCES,
	TBN_COMPANIES,
	TBN_GRAPHICS,
	TBN_LEAGUE,
	TBN_INDUSTRIES,
	TBN_VEHICLESTART,      ///< trains, actually.  So following are trucks, boats and planes
	TBN_TRAINS        = TBN_VEHICLESTART,
	TBN_ROADVEHS,
	TBN_SHIPS,
	TBN_AIRCRAFTS,
	TBN_ZOOMIN,
	TBN_ZOOMOUT,
	TBN_RAILS,
	TBN_ROADS,
	TBN_WATER,
	TBN_AIR,
	TBN_LANDSCAPE,
	TBN_MUSICSOUND,
	TBN_NEWSREPORT,
	TBN_HELP,
	TBN_SWITCHBAR,         ///< only available when toolbar has been split
	TBN_END                ///< The end marker
};

enum ToolbarScenEditorWidgets {
	TBSE_PAUSE        = 0,
	TBSE_FASTFORWARD,
	TBSE_SETTINGS,
	TBSE_SAVESCENARIO,
	TBSE_SPACERPANEL,
	TBSE_DATEPANEL,
	TBSE_DATEBACKWARD,
	TBSE_DATEFORWARD,
	TBSE_SMALLMAP,
	TBSE_ZOOMIN,
	TBSE_ZOOMOUT,
	TBSE_LANDGENERATE,
	TBSE_TOWNGENERATE,
	TBSE_INDUSTRYGENERATE,
	TBSE_BUILDROAD,
	TBSE_BUILDDOCKS,
	TBSE_PLANTTREES,
	TBSE_PLACESIGNS,
	TBSE_DATEPANEL_CONTAINER,
};

/**
 * Drop down list entry for showing a checked/unchecked toggle item.
 */
class DropDownListCheckedItem : public DropDownListStringItem {
	uint checkmark_width;
public:
	bool checked;

	DropDownListCheckedItem(StringID string, int result, bool masked, bool checked) : DropDownListStringItem(string, result, masked), checked(checked)
	{
		this->checkmark_width = GetStringBoundingBox(STR_JUST_CHECKMARK).width + 3;
	}

	virtual ~DropDownListCheckedItem() {}

	uint Width() const
	{
		return DropDownListStringItem::Width() + this->checkmark_width;
	}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		if (this->checked) {
			DrawString(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, STR_JUST_CHECKMARK, sel ? TC_WHITE : TC_BLACK);
		}
		DrawString(left + WD_FRAMERECT_LEFT + (rtl ? 0 : this->checkmark_width), right - WD_FRAMERECT_RIGHT - (rtl ? this->checkmark_width : 0), top, this->String(), sel ? TC_WHITE : TC_BLACK);
	}
};

/**
 * Drop down list entry for showing a company entry, with companies 'blob'.
 */
class DropDownListCompanyItem : public DropDownListItem {
	uint icon_width;
public:
	bool greyed;

	DropDownListCompanyItem(int result, bool masked, bool greyed) : DropDownListItem(result, masked), greyed(greyed)
	{
		this->icon_width = GetSpriteSize(SPR_COMPANY_ICON).width;
	}

	virtual ~DropDownListCompanyItem() {}

	bool Selectable() const
	{
		return true;
	}

	uint Width() const
	{
		CompanyID company = (CompanyID)this->result;
		SetDParam(0, company);
		SetDParam(1, company);
		return GetStringBoundingBox(STR_COMPANY_NAME_COMPANY_NUM).width + this->icon_width + 3;
	}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		CompanyID company = (CompanyID)this->result;
		bool rtl = _current_text_dir == TD_RTL;

		/* It's possible the company is deleted while the dropdown is open */
		if (!Company::IsValidID(company)) return;

		DrawCompanyIcon(company, rtl ? right - this->icon_width - WD_FRAMERECT_RIGHT : left + WD_FRAMERECT_LEFT, top + 1 + (FONT_HEIGHT_NORMAL - 10) / 2);

		SetDParam(0, company);
		SetDParam(1, company);
		TextColour col;
		if (this->greyed) {
			col = TC_GREY;
		} else {
			col = sel ? TC_WHITE : TC_BLACK;
		}
		DrawString(left + WD_FRAMERECT_LEFT + (rtl ? 0 : 3 + this->icon_width), right - WD_FRAMERECT_RIGHT - (rtl ? 3 + this->icon_width : 0), top, STR_COMPANY_NAME_COMPANY_NUM, col);
	}
};

/**
 * Pop up a generic text only menu.
 */
static void PopupMainToolbMenu(Window *w, int widget, StringID string, int count)
{
	DropDownList *list = new DropDownList();
	for (int i = 0; i < count; i++) {
		list->push_back(new DropDownListStringItem(string + i, i, false));
	}
	ShowDropDownList(w, list, 0, widget, 140, true, true);
	SndPlayFx(SND_15_BEEP);
}

/** Enum for the Company Toolbar's network related buttons */
static const int CTMN_CLIENT_LIST = -1; ///< Show the client list
static const int CTMN_NEW_COMPANY = -2; ///< Create a new company
static const int CTMN_SPECTATE    = -3; ///< Become spectator

/**
 * Pop up a generic company list menu.
 */
static void PopupMainCompanyToolbMenu(Window *w, int widget, int grey = 0)
{
	DropDownList *list = new DropDownList();

#ifdef ENABLE_NETWORK
	if (widget == TBN_COMPANIES && _networking) {
		/* Add the client list button for the companies menu */
		list->push_back(new DropDownListStringItem(STR_NETWORK_COMPANY_LIST_CLIENT_LIST, CTMN_CLIENT_LIST, false));

		if (_local_company == COMPANY_SPECTATOR) {
			list->push_back(new DropDownListStringItem(STR_NETWORK_COMPANY_LIST_NEW_COMPANY, CTMN_NEW_COMPANY, NetworkMaxCompaniesReached()));
		} else {
			list->push_back(new DropDownListStringItem(STR_NETWORK_COMPANY_LIST_SPECTATE, CTMN_SPECTATE, NetworkMaxSpectatorsReached()));
		}
	}
#endif /* ENABLE_NETWORK */

	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (!Company::IsValidID(c)) continue;
		list->push_back(new DropDownListCompanyItem(c, false, HasBit(grey, c)));
	}

	ShowDropDownList(w, list, _local_company == COMPANY_SPECTATOR ? CTMN_CLIENT_LIST : (int)_local_company, widget, 240, true, true);
	SndPlayFx(SND_15_BEEP);
}


static ToolbarMode _toolbar_mode;

static CallBackFunction SelectSignTool()
{
	if (_cursor.sprite == SPR_CURSOR_SIGN) {
		ResetObjectToPlace();
		return CBF_NONE;
	} else {
		SetObjectToPlace(SPR_CURSOR_SIGN, PAL_NONE, HT_RECT, WC_MAIN_TOOLBAR, 0);
		return CBF_PLACE_SIGN;
	}
}

/* --- Pausing --- */

static CallBackFunction ToolbarPauseClick(Window *w)
{
	if (_networking && !_network_server) return CBF_NONE; // only server can pause the game

	if (DoCommandP(0, PM_PAUSED_NORMAL, _pause_mode == PM_UNPAUSED, CMD_PAUSE)) SndPlayFx(SND_15_BEEP);
	return CBF_NONE;
}

/* --- Fast forwarding --- */

static CallBackFunction ToolbarFastForwardClick(Window *w)
{
	_fast_forward ^= true;
	SndPlayFx(SND_15_BEEP);
	return CBF_NONE;
}

/* --- Options button menu --- */

enum OptionMenuEntries {
	OME_GAMEOPTIONS,
	OME_DIFFICULTIES,
	OME_SETTINGS,
	OME_AI_SETTINGS,
	OME_NEWGRFSETTINGS,
	OME_TRANSPARENCIES,
	OME_SHOW_TOWNNAMES,
	OME_SHOW_STATIONNAMES,
	OME_SHOW_WAYPOINTNAMES,
	OME_SHOW_SIGNS,
	OME_FULL_ANIMATION,
	OME_FULL_DETAILS,
	OME_TRANSPARENTBUILDINGS,
	OME_SHOW_STATIONSIGNS,
};

static CallBackFunction ToolbarOptionsClick(Window *w)
{
	DropDownList *list = new DropDownList();
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_GAME_OPTIONS,             OME_GAMEOPTIONS, false));
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_DIFFICULTY_SETTINGS,      OME_DIFFICULTIES, false));
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_CONFIG_SETTINGS,          OME_SETTINGS, false));
	/* Changes to the per-AI settings don't get send from the server to the clients. Clients get
	 * the settings once they join but never update it. As such don't show the window at all
	 * to network clients. */
	if (!_networking || _network_server) list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_AI_SETTINGS, OME_AI_SETTINGS, false));
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_NEWGRF_SETTINGS,          OME_NEWGRFSETTINGS, false));
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_TRANSPARENCY_OPTIONS,     OME_TRANSPARENCIES, false));
	list->push_back(new DropDownListItem(-1, false));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_TOWN_NAMES_DISPLAYED,    OME_SHOW_TOWNNAMES, false, HasBit(_display_opt, DO_SHOW_TOWN_NAMES)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_STATION_NAMES_DISPLAYED, OME_SHOW_STATIONNAMES, false, HasBit(_display_opt, DO_SHOW_STATION_NAMES)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_WAYPOINTS_DISPLAYED,     OME_SHOW_WAYPOINTNAMES, false, HasBit(_display_opt, DO_SHOW_WAYPOINT_NAMES)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_SIGNS_DISPLAYED,         OME_SHOW_SIGNS, false, HasBit(_display_opt, DO_SHOW_SIGNS)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_FULL_ANIMATION,          OME_FULL_ANIMATION, false, HasBit(_display_opt, DO_FULL_ANIMATION)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_FULL_DETAIL,             OME_FULL_DETAILS, false, HasBit(_display_opt, DO_FULL_DETAIL)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_TRANSPARENT_BUILDINGS,   OME_TRANSPARENTBUILDINGS, false, IsTransparencySet(TO_HOUSES)));
	list->push_back(new DropDownListCheckedItem(STR_SETTINGS_MENU_TRANSPARENT_SIGNS,       OME_SHOW_STATIONSIGNS, false, IsTransparencySet(TO_SIGNS)));

	ShowDropDownList(w, list, 0, TBN_SETTINGS, 140, true, true);
	SndPlayFx(SND_15_BEEP);
	return CBF_NONE;
}

static CallBackFunction MenuClickSettings(int index)
{
	switch (index) {
		case OME_GAMEOPTIONS:          ShowGameOptions();                               return CBF_NONE;
		case OME_DIFFICULTIES:         ShowGameDifficulty();                            return CBF_NONE;
		case OME_SETTINGS:             ShowGameSettings();                              return CBF_NONE;
		case OME_AI_SETTINGS:          ShowAIConfigWindow();                            return CBF_NONE;
		case OME_NEWGRFSETTINGS:       ShowNewGRFSettings(!_networking && _settings_client.gui.UserIsAllowedToChangeNewGRFs(), true, true, &_grfconfig); return CBF_NONE;
		case OME_TRANSPARENCIES:       ShowTransparencyToolbar();                       break;

		case OME_SHOW_TOWNNAMES:       ToggleBit(_display_opt, DO_SHOW_TOWN_NAMES);     break;
		case OME_SHOW_STATIONNAMES:    ToggleBit(_display_opt, DO_SHOW_STATION_NAMES);  break;
		case OME_SHOW_WAYPOINTNAMES:   ToggleBit(_display_opt, DO_SHOW_WAYPOINT_NAMES); break;
		case OME_SHOW_SIGNS:           ToggleBit(_display_opt, DO_SHOW_SIGNS);          break;
		case OME_FULL_ANIMATION:       ToggleBit(_display_opt, DO_FULL_ANIMATION);      break;
		case OME_FULL_DETAILS:         ToggleBit(_display_opt, DO_FULL_DETAIL);         break;
		case OME_TRANSPARENTBUILDINGS: ToggleTransparency(TO_HOUSES);                   break;
		case OME_SHOW_STATIONSIGNS:    ToggleTransparency(TO_SIGNS);                    break;
	}
	MarkWholeScreenDirty();
	return CBF_NONE;
}

/* --- Saving/loading button menu --- */

enum SaveLoadEditorMenuEntries {
	SLEME_SAVE_SCENARIO   = 0,
	SLEME_LOAD_SCENARIO,
	SLEME_LOAD_HEIGHTMAP,
	SLEME_EXIT_TOINTRO,
	SLEME_EXIT_GAME       = 5,
	SLEME_MENUCOUNT,
};

enum SaveLoadNormalMenuEntries {
	SLNME_SAVE_GAME   = 0,
	SLNME_LOAD_GAME,
	SLNME_EXIT_TOINTRO,
	SLNME_EXIT_GAME,
	SLNME_MENUCOUNT,
};

static CallBackFunction ToolbarSaveClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SAVEGAME, STR_FILE_MENU_SAVE_GAME, SLNME_MENUCOUNT);
	return CBF_NONE;
}

static CallBackFunction ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, TBSE_SAVESCENARIO, STR_SCENEDIT_FILE_MENU_SAVE_SCENARIO, SLEME_MENUCOUNT);
	return CBF_NONE;
}

static CallBackFunction MenuClickSaveLoad(int index = 0)
{
	if (_game_mode == GM_EDITOR) {
		switch (index) {
			case SLEME_SAVE_SCENARIO:  ShowSaveLoadDialog(SLD_SAVE_SCENARIO);  break;
			case SLEME_LOAD_SCENARIO:  ShowSaveLoadDialog(SLD_LOAD_SCENARIO);  break;
			case SLEME_LOAD_HEIGHTMAP: ShowSaveLoadDialog(SLD_LOAD_HEIGHTMAP); break;
			case SLEME_EXIT_TOINTRO:   AskExitToGameMenu();                    break;
			case SLEME_EXIT_GAME:      HandleExitGameRequest();                break;
		}
	} else {
		switch (index) {
			case SLNME_SAVE_GAME:      ShowSaveLoadDialog(SLD_SAVE_GAME); break;
			case SLNME_LOAD_GAME:      ShowSaveLoadDialog(SLD_LOAD_GAME); break;
			case SLNME_EXIT_TOINTRO:   AskExitToGameMenu();               break;
			case SLNME_EXIT_GAME:      HandleExitGameRequest();           break;
		}
	}
	return CBF_NONE;
}

/* --- Map button menu --- */

enum MapMenuEntries {
	MME_SHOW_SMALLMAP        = 0,
	MME_SHOW_EXTRAVIEWPORTS,
	MME_SHOW_SIGNLISTS,
	MME_SHOW_TOWNDIRECTORY,    ///< This entry is only used in Editor mode
	MME_MENUCOUNT_NORMAL     = 3,
	MME_MENUCOUNT_EDITOR     = 4,
};

static CallBackFunction ToolbarMapClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SMALLMAP, STR_MAP_MENU_MAP_OF_WORLD, MME_MENUCOUNT_NORMAL);
	return CBF_NONE;
}

static CallBackFunction ToolbarScenMapTownDir(Window *w)
{
	PopupMainToolbMenu(w, TBSE_SMALLMAP, STR_MAP_MENU_MAP_OF_WORLD, MME_MENUCOUNT_EDITOR);
	return CBF_NONE;
}

static CallBackFunction MenuClickMap(int index)
{
	switch (index) {
		case MME_SHOW_SMALLMAP:       ShowSmallMap();            break;
		case MME_SHOW_EXTRAVIEWPORTS: ShowExtraViewPortWindow(); break;
		case MME_SHOW_SIGNLISTS:      ShowSignList();            break;
		case MME_SHOW_TOWNDIRECTORY:  if (_game_mode == GM_EDITOR) ShowTownDirectory(); break;
	}
	return CBF_NONE;
}

/* --- Town button menu --- */

static CallBackFunction ToolbarTownClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_TOWNDIRECTORY, STR_TOWN_MENU_TOWN_DIRECTORY, (_settings_game.economy.found_town == TF_FORBIDDEN) ? 1 : 2);
	return CBF_NONE;
}

static CallBackFunction MenuClickTown(int index)
{
	switch (index) {
		case 0: ShowTownDirectory(); break;
		case 1: // setting could be changed when the dropdown was open
			if (_settings_game.economy.found_town != TF_FORBIDDEN) ShowFoundTownWindow();
			break;
	}
	return CBF_NONE;
}

/* --- Subidies button menu --- */

static CallBackFunction ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SUBSIDIES, STR_SUBSIDIES_MENU_SUBSIDIES, 1);
	return CBF_NONE;
}

static CallBackFunction MenuClickSubsidies(int index)
{
	ShowSubsidiesList();
	return CBF_NONE;
}

/* --- Stations button menu --- */

static CallBackFunction ToolbarStationsClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, TBN_STATIONS);
	return CBF_NONE;
}

static CallBackFunction MenuClickStations(int index)
{
	ShowCompanyStations((CompanyID)index);
	return CBF_NONE;
}

/* --- Finances button menu --- */

static CallBackFunction ToolbarFinancesClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, TBN_FINANCES);
	return CBF_NONE;
}

static CallBackFunction MenuClickFinances(int index)
{
	ShowCompanyFinances((CompanyID)index);
	return CBF_NONE;
}

/* --- Company's button menu --- */

static CallBackFunction ToolbarCompaniesClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, TBN_COMPANIES);
	return CBF_NONE;
}

static CallBackFunction MenuClickCompany(int index)
{
#ifdef ENABLE_NETWORK
	if (_networking) {
		switch (index) {
			case CTMN_CLIENT_LIST:
				ShowClientList();
				return CBF_NONE;

			case CTMN_NEW_COMPANY:
				if (_network_server) {
					DoCommandP(0, 0, _network_own_client_id, CMD_COMPANY_CTRL);
				} else {
					NetworkSendCommand(0, 0, 0, CMD_COMPANY_CTRL, NULL, NULL, _local_company);
				}
				return CBF_NONE;

			case CTMN_SPECTATE:
				if (_network_server) {
					NetworkServerDoMove(CLIENT_ID_SERVER, COMPANY_SPECTATOR);
					MarkWholeScreenDirty();
				} else {
					NetworkClientRequestMove(COMPANY_SPECTATOR);
				}
				return CBF_NONE;
		}
	}
#endif /* ENABLE_NETWORK */
	ShowCompany((CompanyID)index);
	return CBF_NONE;
}

/* --- Graphs button menu --- */

static CallBackFunction ToolbarGraphsClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_GRAPHICS, STR_GRAPH_MENU_OPERATING_PROFIT_GRAPH, (_toolbar_mode == TB_NORMAL) ? 6 : 8);
	return CBF_NONE;
}

static CallBackFunction MenuClickGraphs(int index)
{
	switch (index) {
		case 0: ShowOperatingProfitGraph();    break;
		case 1: ShowIncomeGraph();             break;
		case 2: ShowDeliveredCargoGraph();     break;
		case 3: ShowPerformanceHistoryGraph(); break;
		case 4: ShowCompanyValueGraph();       break;
		case 5: ShowCargoPaymentRates();       break;
		/* functions for combined graphs/league button */
		case 6: ShowCompanyLeagueTable();      break;
		case 7: ShowPerformanceRatingDetail(); break;
	}
	return CBF_NONE;
}

/* --- League button menu --- */

static CallBackFunction ToolbarLeagueClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_LEAGUE, STR_GRAPH_MENU_COMPANY_LEAGUE_TABLE, 2);
	return CBF_NONE;
}

static CallBackFunction MenuClickLeague(int index)
{
	switch (index) {
		case 0: ShowCompanyLeagueTable();      break;
		case 1: ShowPerformanceRatingDetail(); break;
	}
	return CBF_NONE;
}

/* --- Industries button menu --- */

static CallBackFunction ToolbarIndustryClick(Window *w)
{
	/* Disable build-industry menu if we are a spectator */
	PopupMainToolbMenu(w, TBN_INDUSTRIES, STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, (_local_company == COMPANY_SPECTATOR) ? 1 : 2);
	return CBF_NONE;
}

static CallBackFunction MenuClickIndustry(int index)
{
	switch (index) {
		case 0: ShowIndustryDirectory();   break;
		case 1: ShowBuildIndustryWindow(); break;
	}
	return CBF_NONE;
}

/* --- Trains button menu + 1 helper function for all vehicles. --- */

static void ToolbarVehicleClick(Window *w, VehicleType veh)
{
	const Vehicle *v;
	int dis = ~0;

	FOR_ALL_VEHICLES(v) {
		if (v->type == veh && v->IsPrimaryVehicle()) ClrBit(dis, v->owner);
	}
	PopupMainCompanyToolbMenu(w, TBN_VEHICLESTART + veh, dis);
}


static CallBackFunction ToolbarTrainClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_TRAIN);
	return CBF_NONE;
}

static CallBackFunction MenuClickShowTrains(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_TRAIN);
	return CBF_NONE;
}

/* --- Road vehicle button menu --- */

static CallBackFunction ToolbarRoadClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_ROAD);
	return CBF_NONE;
}

static CallBackFunction MenuClickShowRoad(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_ROAD);
	return CBF_NONE;
}

/* --- Ship button menu --- */

static CallBackFunction ToolbarShipClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_SHIP);
	return CBF_NONE;
}

static CallBackFunction MenuClickShowShips(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_SHIP);
	return CBF_NONE;
}

/* --- Aircraft button menu --- */

static CallBackFunction ToolbarAirClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_AIRCRAFT);
	return CBF_NONE;
}

static CallBackFunction MenuClickShowAir(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_AIRCRAFT);
	return CBF_NONE;
}

/* --- Zoom in button --- */

static CallBackFunction ToolbarZoomInClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick((_game_mode == GM_EDITOR) ? (byte)TBSE_ZOOMIN : (byte)TBN_ZOOMIN);
		SndPlayFx(SND_15_BEEP);
	}
	return CBF_NONE;
}

/* --- Zoom out button --- */

static CallBackFunction ToolbarZoomOutClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick((_game_mode == GM_EDITOR) ? (byte)TBSE_ZOOMOUT : (byte)TBN_ZOOMOUT);
		SndPlayFx(SND_15_BEEP);
	}
	return CBF_NONE;
}

/* --- Rail button menu --- */

static CallBackFunction ToolbarBuildRailClick(Window *w)
{
	ShowDropDownList(w, GetRailTypeDropDownList(), _last_built_railtype, TBN_RAILS, 140, true, true);
	SndPlayFx(SND_15_BEEP);
	return CBF_NONE;
}

static CallBackFunction MenuClickBuildRail(int index)
{
	_last_built_railtype = (RailType)index;
	ShowBuildRailToolbar(_last_built_railtype);
	return CBF_NONE;
}

/* --- Road button menu --- */

static CallBackFunction ToolbarBuildRoadClick(Window *w)
{
	const Company *c = Company::Get(_local_company);
	DropDownList *list = new DropDownList();

	/* Road is always visible and available. */
	list->push_back(new DropDownListStringItem(STR_ROAD_MENU_ROAD_CONSTRUCTION, ROADTYPE_ROAD, false));

	/* Tram is only visible when there will be a tram, and available when that has been introduced. */
	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
		if (!HasBit(e->info.climates, _settings_game.game_creation.landscape)) continue;
		if (!HasBit(e->info.misc_flags, EF_ROAD_TRAM)) continue;

		list->push_back(new DropDownListStringItem(STR_ROAD_MENU_TRAM_CONSTRUCTION, ROADTYPE_TRAM, !HasBit(c->avail_roadtypes, ROADTYPE_TRAM)));
		break;
	}
	ShowDropDownList(w, list, _last_built_roadtype, TBN_ROADS, 140, true, true);
	SndPlayFx(SND_15_BEEP);
	return CBF_NONE;
}

static CallBackFunction MenuClickBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_roadtype);
	return CBF_NONE;
}

/* --- Water button menu --- */

static CallBackFunction ToolbarBuildWaterClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_WATER, STR_WATERWAYS_MENU_WATERWAYS_CONSTRUCTION, 1);
	return CBF_NONE;
}

static CallBackFunction MenuClickBuildWater(int index)
{
	ShowBuildDocksToolbar();
	return CBF_NONE;
}

/* --- Airport button menu --- */

static CallBackFunction ToolbarBuildAirClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_AIR, STR_AIRCRAFT_MENU_AIRPORT_CONSTRUCTION, 1);
	return CBF_NONE;
}

static CallBackFunction MenuClickBuildAir(int index)
{
	ShowBuildAirToolbar();
	return CBF_NONE;
}

/* --- Forest button menu --- */

static CallBackFunction ToolbarForestClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_LANDSCAPE, STR_LANDSCAPING_MENU_LANDSCAPING, 3);
	return CBF_NONE;
}

static CallBackFunction MenuClickForest(int index)
{
	switch (index) {
		case 0: ShowTerraformToolbar();  break;
		case 1: ShowBuildTreesToolbar(); break;
		case 2: return SelectSignTool();
	}
	return CBF_NONE;
}

/* --- Music button menu --- */

static CallBackFunction ToolbarMusicClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_MUSICSOUND, STR_TOOLBAR_SOUND_MUSIC, 1);
	return CBF_NONE;
}

static CallBackFunction MenuClickMusicWindow(int index)
{
	ShowMusicWindow();
	return CBF_NONE;
}

/* --- Newspaper button menu --- */

static CallBackFunction ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_NEWSREPORT, STR_NEWS_MENU_LAST_MESSAGE_NEWS_REPORT, 3);
	return CBF_NONE;
}

static CallBackFunction MenuClickNewspaper(int index)
{
	switch (index) {
		case 0: ShowLastNewsMessage(); break;
		case 1: ShowMessageOptions();  break;
		case 2: ShowMessageHistory();  break;
	}
	return CBF_NONE;
}

/* --- Help button menu --- */

static CallBackFunction PlaceLandBlockInfo()
{
	if (_cursor.sprite == SPR_CURSOR_QUERY) {
		ResetObjectToPlace();
		return CBF_NONE;
	} else {
		SetObjectToPlace(SPR_CURSOR_QUERY, PAL_NONE, HT_RECT, WC_MAIN_TOOLBAR, 0);
		return CBF_PLACE_LANDINFO;
	}
}

static CallBackFunction ToolbarHelpClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_HELP, STR_ABOUT_MENU_LAND_BLOCK_INFO, _settings_client.gui.newgrf_developer_tools ? 9 : 8);
	return CBF_NONE;
}

static void MenuClickSmallScreenshot()
{
	MakeScreenshot(SC_VIEWPORT, NULL);
}

static void MenuClickZoomedInScreenshot()
{
	MakeScreenshot(SC_ZOOMEDIN, NULL);
}

static void MenuClickWorldScreenshot()
{
	MakeScreenshot(SC_WORLD, NULL);
}

static CallBackFunction MenuClickHelp(int index)
{
	switch (index) {
		case 0: return PlaceLandBlockInfo();
		case 2: IConsoleSwitch();              break;
		case 3: ShowAIDebugWindow();           break;
		case 4: MenuClickSmallScreenshot();    break;
		case 5: MenuClickZoomedInScreenshot(); break;
		case 6: MenuClickWorldScreenshot();    break;
		case 7: ShowAboutWindow();             break;
		case 8: ShowSpriteAlignerWindow();     break;
	}
	return CBF_NONE;
}

/* --- Switch toolbar button --- */

static CallBackFunction ToolbarSwitchClick(Window *w)
{
	if (_toolbar_mode != TB_LOWER) {
		_toolbar_mode = TB_LOWER;
	} else {
		_toolbar_mode = TB_UPPER;
	}

	w->ReInit();
	w->SetWidgetLoweredState(TBN_SWITCHBAR, _toolbar_mode == TB_LOWER);
	SndPlayFx(SND_15_BEEP);
	return CBF_NONE;
}

/* --- Scenario editor specific handlers. */

/**
 * Called when clicking at the date panel of the scenario editor toolbar.
 */
static CallBackFunction ToolbarScenDatePanel(Window *w)
{
	SetDParam(0, _settings_game.game_creation.starting_year);
	ShowQueryString(STR_JUST_INT, STR_MAPGEN_START_DATE_QUERY_CAPT, 8, 100, w, CS_NUMERAL, QSF_ENABLE_DEFAULT);
	_left_button_clicked = false;
	return CBF_NONE;
}

static CallBackFunction ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
		w->HandleButtonClick(TBSE_DATEBACKWARD);
		w->SetDirty();

		_settings_game.game_creation.starting_year = Clamp(_settings_game.game_creation.starting_year - 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1), 0);
	}
	_left_button_clicked = false;
	return CBF_NONE;
}

static CallBackFunction ToolbarScenDateForward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
		w->HandleButtonClick(TBSE_DATEFORWARD);
		w->SetDirty();

		_settings_game.game_creation.starting_year = Clamp(_settings_game.game_creation.starting_year + 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1), 0);
	}
	_left_button_clicked = false;
	return CBF_NONE;
}

static CallBackFunction ToolbarScenGenLand(Window *w)
{
	w->HandleButtonClick(TBSE_LANDGENERATE);
	SndPlayFx(SND_15_BEEP);

	ShowEditorTerraformToolbar();
	return CBF_NONE;
}


static CallBackFunction ToolbarScenGenTown(Window *w)
{
	w->HandleButtonClick(TBSE_TOWNGENERATE);
	SndPlayFx(SND_15_BEEP);
	ShowFoundTownWindow();
	return CBF_NONE;
}

static CallBackFunction ToolbarScenGenIndustry(Window *w)
{
	w->HandleButtonClick(TBSE_INDUSTRYGENERATE);
	SndPlayFx(SND_15_BEEP);
	ShowBuildIndustryWindow();
	return CBF_NONE;
}

static CallBackFunction ToolbarScenBuildRoad(Window *w)
{
	w->HandleButtonClick(TBSE_BUILDROAD);
	SndPlayFx(SND_15_BEEP);
	ShowBuildRoadScenToolbar();
	return CBF_NONE;
}

static CallBackFunction ToolbarScenBuildDocks(Window *w)
{
	w->HandleButtonClick(TBSE_BUILDDOCKS);
	SndPlayFx(SND_15_BEEP);
	ShowBuildDocksScenToolbar();
	return CBF_NONE;
}

static CallBackFunction ToolbarScenPlantTrees(Window *w)
{
	w->HandleButtonClick(TBSE_PLANTTREES);
	SndPlayFx(SND_15_BEEP);
	ShowBuildTreesToolbar();
	return CBF_NONE;
}

static CallBackFunction ToolbarScenPlaceSign(Window *w)
{
	w->HandleButtonClick(TBSE_PLACESIGNS);
	SndPlayFx(SND_15_BEEP);
	return SelectSignTool();
}

static CallBackFunction ToolbarBtn_NULL(Window *w)
{
	return CBF_NONE;
}

typedef CallBackFunction MenuClickedProc(int index);

static MenuClickedProc * const _menu_clicked_procs[] = {
	NULL,                 // 0
	NULL,                 // 1
	MenuClickSettings,    // 2
	MenuClickSaveLoad,    // 3
	MenuClickMap,         // 4
	MenuClickTown,        // 5
	MenuClickSubsidies,   // 6
	MenuClickStations,    // 7
	MenuClickFinances,    // 8
	MenuClickCompany,     // 9
	MenuClickGraphs,      // 10
	MenuClickLeague,      // 11
	MenuClickIndustry,    // 12
	MenuClickShowTrains,  // 13
	MenuClickShowRoad,    // 14
	MenuClickShowShips,   // 15
	MenuClickShowAir,     // 16
	MenuClickMap,         // 17
	NULL,                 // 18
	MenuClickBuildRail,   // 19
	MenuClickBuildRoad,   // 20
	MenuClickBuildWater,  // 21
	MenuClickBuildAir,    // 22
	MenuClickForest,      // 23
	MenuClickMusicWindow, // 24
	MenuClickNewspaper,   // 25
	MenuClickHelp,        // 26
};

int16 *_preferred_toolbar_size = NULL; ///< Pointer to the default size for the main toolbar.

/** Full blown container to make it behave exactly as we want :) */
class NWidgetToolbarContainer : public NWidgetContainer {
	bool visible[TBN_END]; ///< The visible headers
protected:
	uint spacers;          ///< Number of spacer widgets in this toolbar

public:
	NWidgetToolbarContainer() : NWidgetContainer(NWID_HORIZONTAL)
	{
	}

	/**
	 * Check whether the given widget type is a button for us.
	 * @param type the widget type to check
	 * @return true if it is a button for us
	 */
	bool IsButton(WidgetType type) const
	{
		return type == WWT_IMGBTN || type == WWT_IMGBTN_2 || type == WWT_PUSHIMGBTN;
	}

	void SetupSmallestSize(Window *w, bool init_array)
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
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			child_wid->SetupSmallestSize(w, init_array);
			this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
			if (this->IsButton(child_wid->type)) {
				nbuttons++;
				this->smallest_x = max(this->smallest_x, child_wid->smallest_x + child_wid->padding_left + child_wid->padding_right);
			} else if (child_wid->type == NWID_SPACER) {
				this->spacers++;
			}
		}

		/* ... then in a second pass make sure the 'current' heights are set. Won't change ever. */
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			child_wid->current_y = this->smallest_y;
			if (!this->IsButton(child_wid->type)) {
				child_wid->current_x = child_wid->smallest_x;
			}
		}
		*_preferred_toolbar_size = nbuttons * this->smallest_x;
	}

	void AssignSizePosition(SizingType sizing, uint x, uint y, uint given_width, uint given_height, bool rtl)
	{
		assert(given_width >= this->smallest_x && given_height >= this->smallest_y);

		this->pos_x = x;
		this->pos_y = y;
		this->current_x = given_width;
		this->current_y = given_height;

		/* Figure out what are the visible buttons */
		memset(this->visible, 0, sizeof(this->visible));
		uint arrangable_count, button_count, spacer_count;
		const byte *arrangement = GetButtonArrangement(given_width, arrangable_count, button_count, spacer_count);
		for (uint i = 0; i < arrangable_count; i++) {
			this->visible[arrangement[i]] = true;
		}

		/* Create us ourselves a quick lookup table */
		NWidgetBase *widgets[TBN_END];
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->type == NWID_SPACER) continue;
			widgets[((NWidgetCore*)child_wid)->index] = child_wid;
		}

		/* Now assign the widgets to their rightful place */
		uint position = 0; // Place to put next child relative to origin of the container.
		uint spacer_space = max(0, (int)given_width - (int)(button_count * this->smallest_x)); // Remaining spacing for 'spacer' widgets
		uint button_space = given_width - spacer_space; // Remaining spacing for the buttons
		uint spacer_i = 0;
		uint button_i = 0;

		/* Index into the arrangement indices. The macro lastof cannot be used here! */
		const byte *cur_wid = rtl ? &arrangement[arrangable_count - 1] : arrangement;
		for (uint i = 0; i < arrangable_count; i++) {
			NWidgetBase *child_wid = widgets[*cur_wid];
			/* If we have to give space to the spacers, do that */
			if (spacer_space != 0) {
				NWidgetBase *possible_spacer = rtl ? child_wid->next : child_wid->prev;
				if (possible_spacer != NULL && possible_spacer->type == NWID_SPACER) {
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
			}
			child_wid->AssignSizePosition(sizing, x + position, y, child_wid->current_x, this->current_y, rtl);
			position += child_wid->current_x;

			if (rtl) {
				cur_wid--;
			} else {
				cur_wid++;
			}
		}
	}

	/* virtual */ void Draw(const Window *w)
	{
		/* Draw brown-red toolbar bg. */
		GfxFillRect(this->pos_x, this->pos_y, this->pos_x + this->current_x - 1, this->pos_y + this->current_y - 1, 0xB2);
		GfxFillRect(this->pos_x, this->pos_y, this->pos_x + this->current_x - 1, this->pos_y + this->current_y - 1, 0xB4, FILLRECT_CHECKER);

		bool rtl = _current_text_dir == TD_RTL;
		for (NWidgetBase *child_wid = rtl ? this->tail : this->head; child_wid != NULL; child_wid = rtl ? child_wid->prev : child_wid->next) {
			if (child_wid->type == NWID_SPACER) continue;
			if (!this->visible[((NWidgetCore*)child_wid)->index]) continue;

			child_wid->Draw(w);
		}
	}

	/* virtual */ NWidgetCore *GetWidgetFromPos(int x, int y)
	{
		if (!IsInsideBS(x, this->pos_x, this->current_x) || !IsInsideBS(y, this->pos_y, this->current_y)) return NULL;

		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->type == NWID_SPACER) continue;
			if (!this->visible[((NWidgetCore*)child_wid)->index]) continue;

			NWidgetCore *nwid = child_wid->GetWidgetFromPos(x, y);
			if (nwid != NULL) return nwid;
		}
		return NULL;
	}

	/**
	 * Get the arrangement of the buttons for the toolbar.
	 * @param width the new width of the toolbar
	 * @param arrangable_count output of the number of visible items
	 * @param button_count output of the number of visible buttons
	 * @param spacer_count output of the number of spacers
	 * @return the button configuration
	 */
	virtual const byte *GetButtonArrangement(uint &width, uint &arrangable_count, uint &button_count, uint &spacer_count) const = 0;
};

/** Container for the 'normal' main toolbar */
class NWidgetMainToolbarContainer : public NWidgetToolbarContainer {
	/* virtual */ const byte *GetButtonArrangement(uint &width, uint &arrangable_count, uint &button_count, uint &spacer_count) const
	{
		static const uint SMALLEST_ARRANGEMENT = 14;
		static const uint BIGGEST_ARRANGEMENT  = 19;
		static const byte arrange14[] = {
			0,  1, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 27,
			2,  3,  4,  5,  6,  7,  8,  9, 10, 12, 24, 25, 26, 27,
		};
		static const byte arrange15[] = {
			0,  1,  4, 13, 14, 15, 16, 19, 20, 21, 22, 23, 17, 18, 27,
			0,  2,  4,  3,  5,  6,  7,  8,  9, 10, 12, 24, 25, 26, 27,
		};
		static const byte arrange16[] = {
			0,  1,  2,  4, 13, 14, 15, 16, 19, 20, 21, 22, 23, 17, 18, 27,
			0,  1,  3,  5,  6,  7,  8,  9, 10, 12, 24, 25, 26, 17, 18, 27,
		};
		static const byte arrange17[] = {
			0,  1,  2,  4,  6, 13, 14, 15, 16, 19, 20, 21, 22, 23, 17, 18, 27,
			0,  1,  3,  4,  6,  5,  7,  8,  9, 10, 12, 24, 25, 26, 17, 18, 27,
		};
		static const byte arrange18[] = {
			0,  1,  2,  4,  5,  6,  7,  8,  9, 12, 19, 20, 21, 22, 23, 17, 18, 27,
			0,  1,  3,  4,  5,  6,  7, 10, 13, 14, 15, 16, 24, 25, 26, 17, 18, 27,
		};
		static const byte arrange19[] = {
			0,  1,  2,  4,  5,  6, 13, 14, 15, 16, 19, 20, 21, 22, 23, 24, 17, 18, 27,
			0,  1,  3,  4,  7,  8,  9, 10, 12, 25, 19, 20, 21, 22, 23, 26, 17, 18, 27,
		};
		static const byte arrange_all[] = {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
		};

		/* If at least BIGGEST_ARRANGEMENT fit, just spread all the buttons nicely */
		uint full_buttons = max(CeilDiv(width, this->smallest_x), SMALLEST_ARRANGEMENT);
		if (full_buttons > BIGGEST_ARRANGEMENT) {
			button_count = arrangable_count = lengthof(arrange_all);
			spacer_count = this->spacers;
			return arrange_all;
		}

		/* Introduce the split toolbar */
		static const byte * const arrangements[] = { arrange14, arrange15, arrange16, arrange17, arrange18, arrange19 };

		button_count = arrangable_count = full_buttons;
		spacer_count = this->spacers;
		return arrangements[full_buttons - SMALLEST_ARRANGEMENT] + ((_toolbar_mode == TB_LOWER) ? full_buttons : 0);
	}
};

/** Container for the scenario editor's toolbar */
class NWidgetScenarioToolbarContainer : public NWidgetToolbarContainer {
	uint panel_widths[2]; ///< The width of the two panels (the text panel and date panel)

	void SetupSmallestSize(Window *w, bool init_array)
	{
		this->NWidgetToolbarContainer::SetupSmallestSize(w, init_array);

		/* Find the size of panel_widths */
		uint i = 0;
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			if (child_wid->type == NWID_SPACER || this->IsButton(child_wid->type)) continue;

			assert(i < lengthof(this->panel_widths));
			this->panel_widths[i++] = child_wid->current_x;
			*_preferred_toolbar_size += child_wid->current_x;
		}
	}

	/* virtual */ const byte *GetButtonArrangement(uint &width, uint &arrangable_count, uint &button_count, uint &spacer_count) const
	{
		static const byte arrange_all[] = {
			0, 1, 2, 3, 4, 18, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 24, 26,
		};
		static const byte arrange_nopanel[] = {
			0, 1, 2, 3, 18, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 24, 26,
		};
		static const byte arrange_switch[] = {
			18,  8, 11, 12, 13, 14, 15, 16, 17, 27,
			 0,  1,  2,  3, 18,  9, 10, 24, 26, 27,
		};

		/* If we can place all buttons *and* the panels, show them. */
		uint min_full_width = (lengthof(arrange_all) - lengthof(this->panel_widths)) * this->smallest_x + this->panel_widths[0] + this->panel_widths[1];
		if (width >= min_full_width) {
			width -= this->panel_widths[0] + this->panel_widths[1];
			arrangable_count = lengthof(arrange_all);
			button_count = arrangable_count - 2;
			spacer_count = this->spacers;
			return arrange_all;
		}

		/* Otherwise don't show the date panel and if we can't fit half the buttons and the panels anymore, split the toolbar in two */
		uint min_small_width = (lengthof(arrange_switch) - lengthof(this->panel_widths)) * this->smallest_x / 2 + this->panel_widths[1];
		if (width > min_small_width) {
			width -= this->panel_widths[1];
			arrangable_count = lengthof(arrange_nopanel);
			button_count = arrangable_count - 1;
			spacer_count = this->spacers - 1;
			return arrange_nopanel;
		}

		/* Split toolbar */
		width -= this->panel_widths[1];
		arrangable_count = lengthof(arrange_switch) / 2;
		button_count = arrangable_count - 1;
		spacer_count = 0;
		return arrange_switch + ((_toolbar_mode == TB_LOWER) ? arrangable_count : 0);
	}
};

/* --- Toolbar handling for the 'normal' case */

typedef CallBackFunction ToolbarButtonProc(Window *w);

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
	ToolbarBuildWaterClick,
	ToolbarBuildAirClick,
	ToolbarForestClick,
	ToolbarMusicClick,
	ToolbarNewspaperClick,
	ToolbarHelpClick,
	ToolbarSwitchClick,
};

enum MainToolbarHotkeys {
	MTHK_PAUSE,
	MTHK_FASTFORWARD,
	MTHK_SETTINGS,
	MTHK_SAVEGAME,
	MTHK_LOADGAME,
	MTHK_SMALLMAP,
	MTHK_TOWNDIRECTORY,
	MTHK_SUBSIDIES,
	MTHK_STATIONS,
	MTHK_FINANCES,
	MTHK_COMPANIES,
	MTHK_GRAPHS,
	MTHK_LEAGUE,
	MTHK_INDUSTRIES,
	MTHK_TRAIN_LIST,
	MTHK_ROADVEH_LIST,
	MTHK_SHIP_LIST,
	MTHK_AIRCRAFT_LIST,
	MTHK_ZOOM_IN,
	MTHK_ZOOM_OUT,
	MTHK_BUILD_RAIL,
	MTHK_BUILD_ROAD,
	MTHK_BUILD_DOCKS,
	MTHK_BUILD_AIRPORT,
	MTHK_BUILD_TREES,
	MTHK_MUSIC,
	MTHK_AI_DEBUG,
	MTHK_SMALL_SCREENSHOT,
	MTHK_ZOOMEDIN_SCREENSHOT,
	MTHK_GIANT_SCREENSHOT,
	MTHK_CHEATS,
	MTHK_TERRAFORM,
	MTHK_EXTRA_VIEWPORT,
	MTHK_CLIENT_LIST,
	MTHK_SIGN_LIST,
};

/** Main toolbar. */
struct MainToolbarWindow : Window {
	CallBackFunction last_started_action; ///< Last started user action.

	MainToolbarWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc, 0);

		this->last_started_action = CBF_NONE;
		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);
		this->SetWidgetDisabledState(TBN_PAUSE, _networking && !_network_server); // if not server, disable pause button
		this->SetWidgetDisabledState(TBN_FASTFORWARD, _networking); // if networking, disable fast-forward button
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	virtual void OnPaint()
	{
		/* If spectator, disable all construction buttons
		 * ie : Build road, rail, ships, airports and landscaping
		 * Since enabled state is the default, just disable when needed */
		this->SetWidgetsDisabledState(_local_company == COMPANY_SPECTATOR, TBN_RAILS, TBN_ROADS, TBN_WATER, TBN_AIR, TBN_LANDSCAPE, WIDGET_LIST_END);
		/* disable company list drop downs, if there are no companies */
		this->SetWidgetsDisabledState(Company::GetNumItems() == 0, TBN_STATIONS, TBN_FINANCES, TBN_TRAINS, TBN_ROADVEHS, TBN_SHIPS, TBN_AIRCRAFTS, WIDGET_LIST_END);

		this->SetWidgetDisabledState(TBN_RAILS, !CanBuildVehicleInfrastructure(VEH_TRAIN));
		this->SetWidgetDisabledState(TBN_AIR, !CanBuildVehicleInfrastructure(VEH_AIRCRAFT));

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (_game_mode != GM_MENU && !this->IsWidgetDisabled(widget)) _toolbar_button_procs[widget](this);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		CallBackFunction cbf = _menu_clicked_procs[widget](index);
		if (cbf != CBF_NONE) this->last_started_action = cbf;
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (CheckHotkeyMatch(maintoolbar_hotkeys, keycode, this)) {
			case MTHK_PAUSE: ToolbarPauseClick(this); break;
			case MTHK_FASTFORWARD: ToolbarFastForwardClick(this); break;
			case MTHK_SETTINGS: ShowGameOptions(); break;
			case MTHK_SAVEGAME: MenuClickSaveLoad(); break;
			case MTHK_LOADGAME: ShowSaveLoadDialog(SLD_LOAD_GAME); break;
			case MTHK_SMALLMAP: ShowSmallMap(); break;
			case MTHK_TOWNDIRECTORY: ShowTownDirectory(); break;
			case MTHK_SUBSIDIES: ShowSubsidiesList(); break;
			case MTHK_STATIONS: ShowCompanyStations(_local_company); break;
			case MTHK_FINANCES: ShowCompanyFinances(_local_company); break;
			case MTHK_COMPANIES: ShowCompany(_local_company); break;
			case MTHK_GRAPHS: ShowOperatingProfitGraph(); break;
			case MTHK_LEAGUE: ShowCompanyLeagueTable(); break;
			case MTHK_INDUSTRIES: ShowBuildIndustryWindow(); break;
			case MTHK_TRAIN_LIST: ShowVehicleListWindow(_local_company, VEH_TRAIN); break;
			case MTHK_ROADVEH_LIST: ShowVehicleListWindow(_local_company, VEH_ROAD); break;
			case MTHK_SHIP_LIST: ShowVehicleListWindow(_local_company, VEH_SHIP); break;
			case MTHK_AIRCRAFT_LIST: ShowVehicleListWindow(_local_company, VEH_AIRCRAFT); break;
			case MTHK_ZOOM_IN: ToolbarZoomInClick(this); break;
			case MTHK_ZOOM_OUT: ToolbarZoomOutClick(this); break;
			case MTHK_BUILD_RAIL: if (CanBuildVehicleInfrastructure(VEH_TRAIN)) ShowBuildRailToolbar(_last_built_railtype); break;
			case MTHK_BUILD_ROAD: ShowBuildRoadToolbar(_last_built_roadtype); break;
			case MTHK_BUILD_DOCKS: ShowBuildDocksToolbar(); break;
			case MTHK_BUILD_AIRPORT: if (CanBuildVehicleInfrastructure(VEH_AIRCRAFT)) ShowBuildAirToolbar(); break;
			case MTHK_BUILD_TREES: ShowBuildTreesToolbar(); break;
			case MTHK_MUSIC: ShowMusicWindow(); break;
			case MTHK_AI_DEBUG: ShowAIDebugWindow(); break;
			case MTHK_SMALL_SCREENSHOT: MenuClickSmallScreenshot(); break;
			case MTHK_ZOOMEDIN_SCREENSHOT: MenuClickZoomedInScreenshot(); break;
			case MTHK_GIANT_SCREENSHOT: MenuClickWorldScreenshot(); break;
			case MTHK_CHEATS: if (!_networking) ShowCheatWindow(); break;
			case MTHK_TERRAFORM: ShowTerraformToolbar(); break;
			case MTHK_EXTRA_VIEWPORT: ShowExtraViewPortWindowForTileUnderCursor(); break;
#ifdef ENABLE_NETWORK
			case MTHK_CLIENT_LIST: if (_networking) ShowClientList(); break;
#endif
			case MTHK_SIGN_LIST: ShowSignList(); break;
			default: return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		switch (this->last_started_action) {
			case CBF_PLACE_SIGN:
				PlaceProc_Sign(tile);
				break;

			case CBF_PLACE_LANDINFO:
				ShowLandInfo(tile);
				break;

			default: NOT_REACHED();
		}
	}

	virtual void OnTick()
	{
		if (this->IsWidgetLowered(TBN_PAUSE) != !!_pause_mode) {
			this->ToggleWidgetLoweredState(TBN_PAUSE);
			this->SetWidgetDirty(TBN_PAUSE);
		}

		if (this->IsWidgetLowered(TBN_FASTFORWARD) != !!_fast_forward) {
			this->ToggleWidgetLoweredState(TBN_FASTFORWARD);
			this->SetWidgetDirty(TBN_FASTFORWARD);
		}
	}

	virtual void OnTimeout()
	{
		/* We do not want to automatically raise the pause, fast forward and
		 * switchbar buttons; they have to stay down when pressed etc. */
		for (uint i = TBN_SETTINGS; i < TBN_SWITCHBAR; i++) {
			if (this->IsWidgetLowered(i)) {
				this->RaiseWidget(i);
				this->SetWidgetDirty(i);
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (FindWindowById(WC_MAIN_WINDOW, 0) != NULL) HandleZoomMessage(this, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, TBN_ZOOMIN, TBN_ZOOMOUT);
	}

	static Hotkey<MainToolbarWindow> maintoolbar_hotkeys[];
};

const uint16 _maintoolbar_pause_keys[] = {WKC_F1, WKC_PAUSE, 0};
const uint16 _maintoolbar_zoomin_keys[] = {WKC_NUM_PLUS, WKC_EQUALS, WKC_SHIFT | WKC_EQUALS, WKC_SHIFT | WKC_F5, 0};
const uint16 _maintoolbar_zoomout_keys[] = {WKC_NUM_MINUS, WKC_MINUS, WKC_SHIFT | WKC_MINUS, WKC_SHIFT | WKC_F6, 0};
const uint16 _maintoolbar_smallmap_keys[] = {WKC_F4, 'M', 0};

Hotkey<MainToolbarWindow> MainToolbarWindow::maintoolbar_hotkeys[] = {
	Hotkey<MainToolbarWindow>(_maintoolbar_pause_keys, "pause", MTHK_PAUSE),
	Hotkey<MainToolbarWindow>((uint16)0, "fastforward", MTHK_FASTFORWARD),
	Hotkey<MainToolbarWindow>(WKC_F2, "settings", MTHK_SETTINGS),
	Hotkey<MainToolbarWindow>(WKC_F3, "saveload", MTHK_SAVEGAME),
	Hotkey<MainToolbarWindow>((uint16)0, "load_game", MTHK_LOADGAME),
	Hotkey<MainToolbarWindow>(_maintoolbar_smallmap_keys, "smallmap", MTHK_SMALLMAP),
	Hotkey<MainToolbarWindow>(WKC_F5, "town_list", MTHK_TOWNDIRECTORY),
	Hotkey<MainToolbarWindow>(WKC_F6, "subsidies", MTHK_SUBSIDIES),
	Hotkey<MainToolbarWindow>(WKC_F7, "station_list", MTHK_STATIONS),
	Hotkey<MainToolbarWindow>(WKC_F8, "finances", MTHK_FINANCES),
	Hotkey<MainToolbarWindow>(WKC_F9, "companies", MTHK_COMPANIES),
	Hotkey<MainToolbarWindow>(WKC_F10, "graphs", MTHK_GRAPHS),
	Hotkey<MainToolbarWindow>(WKC_F11, "league", MTHK_LEAGUE),
	Hotkey<MainToolbarWindow>(WKC_F12, "industry_list", MTHK_INDUSTRIES),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F1, "train_list", MTHK_TRAIN_LIST),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F2, "roadveh_list", MTHK_ROADVEH_LIST),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F3, "ship_list", MTHK_SHIP_LIST),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F4, "aircraft_list", MTHK_AIRCRAFT_LIST),
	Hotkey<MainToolbarWindow>(_maintoolbar_zoomin_keys, "zoomin", MTHK_ZOOM_IN),
	Hotkey<MainToolbarWindow>(_maintoolbar_zoomout_keys, "zoomout", MTHK_ZOOM_OUT),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F7, "build_rail", MTHK_BUILD_RAIL),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F8, "build_road", MTHK_BUILD_ROAD),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F9, "build_docks", MTHK_BUILD_DOCKS),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F10, "build_airport", MTHK_BUILD_AIRPORT),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F11, "build_trees", MTHK_BUILD_TREES),
	Hotkey<MainToolbarWindow>(WKC_SHIFT | WKC_F12, "music", MTHK_MUSIC),
	Hotkey<MainToolbarWindow>((uint16)0, "ai_debug", MTHK_AI_DEBUG),
	Hotkey<MainToolbarWindow>(WKC_CTRL  | 'S', "small_screenshot", MTHK_SMALL_SCREENSHOT),
	Hotkey<MainToolbarWindow>(WKC_CTRL  | 'P', "zoomedin_screenshot", MTHK_ZOOMEDIN_SCREENSHOT),
	Hotkey<MainToolbarWindow>((uint16)0, "giant_screenshot", MTHK_GIANT_SCREENSHOT),
	Hotkey<MainToolbarWindow>(WKC_CTRL | WKC_ALT | 'C', "cheats", MTHK_CHEATS),
	Hotkey<MainToolbarWindow>('L', "terraform", MTHK_TERRAFORM),
	Hotkey<MainToolbarWindow>('V', "extra_viewport", MTHK_EXTRA_VIEWPORT),
#ifdef ENABLE_NETWORK
	Hotkey<MainToolbarWindow>((uint16)0, "client_list", MTHK_CLIENT_LIST),
#endif
	Hotkey<MainToolbarWindow>((uint16)0, "sign_list", MTHK_SIGN_LIST),
	HOTKEY_LIST_END(MainToolbarWindow)
};
Hotkey<MainToolbarWindow> *_maintoolbar_hotkeys = MainToolbarWindow::maintoolbar_hotkeys;

static NWidgetBase *MakeMainToolbar(int *biggest_index)
{
	/** Sprites to use for the different toolbar buttons */
	static const SpriteID toolbar_button_sprites[] = {
		SPR_IMG_PAUSE,           // TBN_PAUSE
		SPR_IMG_FASTFORWARD,     // TBN_FASTFORWARD
		SPR_IMG_SETTINGS,        // TBN_SETTINGS
		SPR_IMG_SAVE,            // TBN_SAVEGAME
		SPR_IMG_SMALLMAP,        // TBN_SMALLMAP
		SPR_IMG_TOWN,            // TBN_TOWNDIRECTORY
		SPR_IMG_SUBSIDIES,       // TBN_SUBSIDIES
		SPR_IMG_COMPANY_LIST,    // TBN_STATIONS
		SPR_IMG_COMPANY_FINANCE, // TBN_FINANCES
		SPR_IMG_COMPANY_GENERAL, // TBN_COMPANIES
		SPR_IMG_GRAPHS,          // TBN_GRAPHICS
		SPR_IMG_COMPANY_LEAGUE,  // TBN_LEAGUE
		SPR_IMG_INDUSTRY,        // TBN_INDUSTRIES
		SPR_IMG_TRAINLIST,       // TBN_TRAINS
		SPR_IMG_TRUCKLIST,       // TBN_ROADVEHS
		SPR_IMG_SHIPLIST,        // TBN_SHIPS
		SPR_IMG_AIRPLANESLIST,   // TBN_AIRCRAFTS
		SPR_IMG_ZOOMIN,          // TBN_ZOOMIN
		SPR_IMG_ZOOMOUT,         // TBN_ZOOMOUT
		SPR_IMG_BUILDRAIL,       // TBN_RAILS
		SPR_IMG_BUILDROAD,       // TBN_ROADS
		SPR_IMG_BUILDWATER,      // TBN_WATER
		SPR_IMG_BUILDAIR,        // TBN_AIR
		SPR_IMG_LANDSCAPING,     // TBN_LANDSCAPE
		SPR_IMG_MUSIC,           // TBN_MUSICSOUND
		SPR_IMG_MESSAGES,        // TBN_NEWSREPORT
		SPR_IMG_QUERY,           // TBN_HELP
		SPR_IMG_SWITCH_TOOLBAR,  // TBN_SWITCHBAR
	};

	NWidgetMainToolbarContainer *hor = new NWidgetMainToolbarContainer();
	for (uint i = 0; i < TBN_END; i++) {
		switch (i) {
			case 4: case 8: case 13: case 17: case 19: case 24: hor->Add(new NWidgetSpacer(0, 0)); break;
		}
		hor->Add(new NWidgetLeaf(i == TBN_SAVEGAME ? WWT_IMGBTN_2 : WWT_IMGBTN, COLOUR_GREY, i, toolbar_button_sprites[i], STR_TOOLBAR_TOOLTIP_PAUSE_GAME + i));
	}

	*biggest_index = max<int>(*biggest_index, TBN_SWITCHBAR);
	return hor;
}

static const NWidgetPart _nested_toolbar_normal_widgets[] = {
	NWidgetFunction(MakeMainToolbar),
};

static WindowDesc _toolb_normal_desc(
	WDP_MANUAL, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_NO_FOCUS,
	_nested_toolbar_normal_widgets, lengthof(_nested_toolbar_normal_widgets)
);


/* --- Toolbar handling for the scenario editor */

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
	ToolbarScenGenTown,
	ToolbarScenGenIndustry,
	ToolbarScenBuildRoad,
	ToolbarScenBuildDocks,
	ToolbarScenPlantTrees,
	ToolbarScenPlaceSign,
	ToolbarBtn_NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ToolbarMusicClick,
	NULL,
	ToolbarHelpClick,
	ToolbarSwitchClick,
};

enum MainToolbarEditorHotkeys {
	MTEHK_PAUSE,
	MTEHK_FASTFORWARD,
	MTEHK_SETTINGS,
	MTEHK_SAVEGAME,
	MTEHK_GENLAND,
	MTEHK_GENTOWN,
	MTEHK_GENINDUSTRY,
	MTEHK_BUILD_ROAD,
	MTEHK_BUILD_DOCKS,
	MTEHK_BUILD_TREES,
	MTEHK_SIGN,
	MTEHK_MUSIC,
	MTEHK_LANDINFO,
	MTEHK_SMALL_SCREENSHOT,
	MTEHK_ZOOMEDIN_SCREENSHOT,
	MTEHK_GIANT_SCREENSHOT,
	MTEHK_ZOOM_IN,
	MTEHK_ZOOM_OUT,
	MTEHK_TERRAFORM,
	MTEHK_SMALLMAP,
	MTEHK_EXTRA_VIEWPORT,
};

struct ScenarioEditorToolbarWindow : Window {
	CallBackFunction last_started_action; ///< Last started user action.

	ScenarioEditorToolbarWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc, 0);

		this->last_started_action = CBF_NONE;
		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	virtual void OnPaint()
	{
		this->SetWidgetDisabledState(TBSE_DATEBACKWARD, _settings_game.game_creation.starting_year <= MIN_YEAR);
		this->SetWidgetDisabledState(TBSE_DATEFORWARD, _settings_game.game_creation.starting_year >= MAX_YEAR);

		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case TBSE_DATEPANEL:
				SetDParam(0, ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));
				DrawString(r.left, r.right, (this->height - FONT_HEIGHT_NORMAL) / 2, STR_WHITE_DATE_LONG, TC_FROMSTRING, SA_HOR_CENTER);
				break;

			case TBSE_SPACERPANEL: {
				int height = r.bottom - r.top;
				if (height > 2 * FONT_HEIGHT_NORMAL) {
					DrawString(r.left, r.right, (height + 1) / 2 - FONT_HEIGHT_NORMAL, STR_SCENEDIT_TOOLBAR_OPENTTD, TC_FROMSTRING, SA_HOR_CENTER);
					DrawString(r.left, r.right, (height + 1) / 2, STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR, TC_FROMSTRING, SA_HOR_CENTER);
				} else {
					DrawString(r.left, r.right, (height - FONT_HEIGHT_NORMAL) / 2, STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR, TC_FROMSTRING, SA_HOR_CENTER);
				}
				break;
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case TBSE_SPACERPANEL:
				size->width = max(GetStringBoundingBox(STR_SCENEDIT_TOOLBAR_OPENTTD).width, GetStringBoundingBox(STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR).width) + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				break;

			case TBSE_DATEPANEL:
				SetDParam(0, ConvertYMDToDate(MAX_YEAR, 0, 1));
				*size = GetStringBoundingBox(STR_WHITE_DATE_LONG);
				size->height = max(size->height, GetSpriteSize(SPR_IMG_SAVE).height + WD_IMGBTN_TOP + WD_IMGBTN_BOTTOM);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (_game_mode == GM_MENU) return;
		CallBackFunction cbf = _scen_toolbar_button_procs[widget](this);
		if (cbf != CBF_NONE) this->last_started_action = cbf;
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		/* The map button is in a different location on the scenario
		 * editor toolbar, so we need to adjust for it. */
		if (widget == TBSE_SMALLMAP) widget = TBN_SMALLMAP;
		CallBackFunction cbf = _menu_clicked_procs[widget](index);
		if (cbf != CBF_NONE) this->last_started_action = cbf;
		SndPlayFx(SND_15_BEEP);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		CallBackFunction cbf = CBF_NONE;
		switch (CheckHotkeyMatch(scenedit_maintoolbar_hotkeys, keycode, this)) {
			case MTEHK_PAUSE:               ToolbarPauseClick(this); break;
			case MTEHK_FASTFORWARD:         ToolbarFastForwardClick(this); break;
			case MTEHK_SETTINGS:            ShowGameOptions(); break;
			case MTEHK_SAVEGAME:            MenuClickSaveLoad(); break;
			case MTEHK_GENLAND:             ToolbarScenGenLand(this); break;
			case MTEHK_GENTOWN:             ToolbarScenGenTown(this); break;
			case MTEHK_GENINDUSTRY:         ToolbarScenGenIndustry(this); break;
			case MTEHK_BUILD_ROAD:          ToolbarScenBuildRoad(this); break;
			case MTEHK_BUILD_DOCKS:         ToolbarScenBuildDocks(this); break;
			case MTEHK_BUILD_TREES:         ToolbarScenPlantTrees(this); break;
			case MTEHK_SIGN:                cbf = ToolbarScenPlaceSign(this); break;
			case MTEHK_MUSIC:               ShowMusicWindow(); break;
			case MTEHK_LANDINFO:            cbf = PlaceLandBlockInfo(); break;
			case MTEHK_SMALL_SCREENSHOT:    MenuClickSmallScreenshot(); break;
			case MTEHK_ZOOMEDIN_SCREENSHOT: MenuClickZoomedInScreenshot(); break;
			case MTEHK_GIANT_SCREENSHOT:    MenuClickWorldScreenshot(); break;
			case MTEHK_ZOOM_IN:             ToolbarZoomInClick(this); break;
			case MTEHK_ZOOM_OUT:            ToolbarZoomOutClick(this); break;
			case MTEHK_TERRAFORM:           ShowEditorTerraformToolbar(); break;
			case MTEHK_SMALLMAP:            ShowSmallMap(); break;
			case MTEHK_EXTRA_VIEWPORT:      ShowExtraViewPortWindowForTileUnderCursor(); break;
			default: return ES_NOT_HANDLED;
		}
		if (cbf != CBF_NONE) this->last_started_action = cbf;
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		switch (this->last_started_action) {
			case CBF_PLACE_SIGN:
				PlaceProc_Sign(tile);
				break;

			case CBF_PLACE_LANDINFO:
				ShowLandInfo(tile);
				break;

			default: NOT_REACHED();
		}
	}

	virtual void OnTimeout()
	{
		this->SetWidgetsLoweredState(false, TBSE_DATEBACKWARD, TBSE_DATEFORWARD, WIDGET_LIST_END);
		this->SetWidgetDirty(TBSE_DATEBACKWARD);
		this->SetWidgetDirty(TBSE_DATEFORWARD);
	}

	virtual void OnTick()
	{
		if (this->IsWidgetLowered(TBSE_PAUSE) != !!_pause_mode) {
			this->ToggleWidgetLoweredState(TBSE_PAUSE);
			this->SetDirty();
		}

		if (this->IsWidgetLowered(TBSE_FASTFORWARD) != !!_fast_forward) {
			this->ToggleWidgetLoweredState(TBSE_FASTFORWARD);
			this->SetDirty();
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (FindWindowById(WC_MAIN_WINDOW, 0) != NULL) HandleZoomMessage(this, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, TBSE_ZOOMIN, TBSE_ZOOMOUT);
	}

	virtual void OnQueryTextFinished(char *str)
	{
		/* Was 'cancel' pressed? */
		if (str == NULL) return;

		int32 value;
		if (!StrEmpty(str)) {
			value = atoi(str);
		} else {
			/* An empty string means revert to the default */
			value = DEF_START_YEAR;
		}
		_settings_game.game_creation.starting_year = Clamp(value, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1), 0);

		this->SetDirty();
	}

	static Hotkey<ScenarioEditorToolbarWindow> scenedit_maintoolbar_hotkeys[];
};

Hotkey<ScenarioEditorToolbarWindow> ScenarioEditorToolbarWindow::scenedit_maintoolbar_hotkeys[] = {
	Hotkey<ScenarioEditorToolbarWindow>(_maintoolbar_pause_keys, "pause", MTEHK_PAUSE),
	Hotkey<ScenarioEditorToolbarWindow>((uint16)0, "fastforward", MTEHK_FASTFORWARD),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F2, "settings", MTEHK_SETTINGS),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F3, "saveload", MTEHK_SAVEGAME),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F4, "town_list", MTEHK_GENLAND),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F5, "subsidies", MTEHK_GENTOWN),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F6, "station_list", MTEHK_GENINDUSTRY),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F7, "finances", MTEHK_BUILD_ROAD),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F8, "companies", MTEHK_BUILD_DOCKS),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F9, "graphs", MTEHK_BUILD_TREES),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F10, "league", MTEHK_SIGN),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F11, "industry_list", MTEHK_MUSIC),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_F12, "train_list", MTEHK_LANDINFO),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_CTRL  | 'S', "small_screenshot", MTEHK_SMALL_SCREENSHOT),
	Hotkey<ScenarioEditorToolbarWindow>(WKC_CTRL  | 'P', "zoomedin_screenshot", MTEHK_ZOOMEDIN_SCREENSHOT),
	Hotkey<ScenarioEditorToolbarWindow>((uint16)0, "giant_screenshot", MTEHK_GIANT_SCREENSHOT),
	Hotkey<ScenarioEditorToolbarWindow>(_maintoolbar_zoomin_keys, "zoomin", MTEHK_ZOOM_IN),
	Hotkey<ScenarioEditorToolbarWindow>(_maintoolbar_zoomout_keys, "zoomout", MTEHK_ZOOM_OUT),
	Hotkey<ScenarioEditorToolbarWindow>('L', "terraform", MTEHK_TERRAFORM),
	Hotkey<ScenarioEditorToolbarWindow>('M', "smallmap", MTEHK_SMALLMAP),
	Hotkey<ScenarioEditorToolbarWindow>('V', "extra_viewport", MTEHK_EXTRA_VIEWPORT),
	HOTKEY_LIST_END(ScenarioEditorToolbarWindow)
};
Hotkey<ScenarioEditorToolbarWindow> *_scenedit_maintoolbar_hotkeys = ScenarioEditorToolbarWindow::scenedit_maintoolbar_hotkeys;

static const NWidgetPart _nested_toolb_scen_inner_widgets[] = {
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBSE_PAUSE), SetDataTip(SPR_IMG_PAUSE, STR_TOOLBAR_TOOLTIP_PAUSE_GAME),
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBSE_FASTFORWARD), SetDataTip(SPR_IMG_FASTFORWARD, STR_TOOLBAR_TOOLTIP_FORWARD),
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBSE_SETTINGS), SetDataTip(SPR_IMG_SETTINGS, STR_TOOLBAR_TOOLTIP_OPTIONS),
	NWidget(WWT_IMGBTN_2, COLOUR_GREY, TBSE_SAVESCENARIO), SetDataTip(SPR_IMG_SAVE, STR_SCENEDIT_TOOLBAR_TOOLTIP_SAVE_SCENARIO_LOAD_SCENARIO),
	NWidget(NWID_SPACER),
	NWidget(WWT_PANEL, COLOUR_GREY, TBSE_SPACERPANEL), EndContainer(),
	NWidget(NWID_SPACER),
	NWidget(WWT_PANEL, COLOUR_GREY, TBSE_DATEPANEL_CONTAINER),
		NWidget(NWID_HORIZONTAL), SetPIP(3, 2, 3),
			NWidget(WWT_IMGBTN, COLOUR_GREY, TBSE_DATEBACKWARD), SetDataTip(SPR_ARROW_DOWN, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_BACKWARD),
			NWidget(WWT_EMPTY, COLOUR_GREY, TBSE_DATEPANEL), SetDataTip(STR_NULL, STR_SCENEDIT_TOOLBAR_TOOLTIP_SET_DATE),
			NWidget(WWT_IMGBTN, COLOUR_GREY, TBSE_DATEFORWARD), SetDataTip(SPR_ARROW_UP, STR_SCENEDIT_TOOLBAR_TOOLTIP_MOVE_THE_STARTING_DATE_FORWARD),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_SPACER),
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBSE_SMALLMAP), SetDataTip(SPR_IMG_SMALLMAP, STR_SCENEDIT_TOOLBAR_TOOLTIP_DISPLAY_MAP_TOWN_DIRECTORY),
	NWidget(NWID_SPACER),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_ZOOMIN), SetDataTip(SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_ZOOMOUT), SetDataTip(SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT),
	NWidget(NWID_SPACER),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_LANDGENERATE), SetDataTip(SPR_IMG_LANDSCAPING, STR_SCENEDIT_TOOLBAR_LANDSCAPE_GENERATION),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_TOWNGENERATE), SetDataTip(SPR_IMG_TOWN, STR_SCENEDIT_TOOLBAR_TOWN_GENERATION),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_INDUSTRYGENERATE), SetDataTip(SPR_IMG_INDUSTRY, STR_SCENEDIT_TOOLBAR_INDUSTRY_GENERATION),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_BUILDROAD), SetDataTip(SPR_IMG_BUILDROAD, STR_SCENEDIT_TOOLBAR_ROAD_CONSTRUCTION),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_BUILDDOCKS), SetDataTip(SPR_IMG_BUILDWATER, STR_TOOLBAR_TOOLTIP_BUILD_SHIP_DOCKS),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_PLANTTREES), SetDataTip(SPR_IMG_PLANTTREES, STR_SCENEDIT_TOOLBAR_PLANT_TREES),
	NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, TBSE_PLACESIGNS), SetDataTip(SPR_IMG_SIGN, STR_SCENEDIT_TOOLBAR_PLACE_SIGN),
	NWidget(NWID_SPACER),
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBN_MUSICSOUND), SetDataTip(SPR_IMG_MUSIC, STR_TOOLBAR_TOOLTIP_SHOW_SOUND_MUSIC_WINDOW),
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBN_HELP), SetDataTip(SPR_IMG_QUERY, STR_TOOLBAR_TOOLTIP_LAND_BLOCK_INFORMATION),
	NWidget(WWT_IMGBTN, COLOUR_GREY, TBN_SWITCHBAR), SetDataTip(SPR_IMG_SWITCH_TOOLBAR, STR_TOOLBAR_TOOLTIP_SWITCH_TOOLBAR),
};

static NWidgetBase *MakeScenarioToolbar(int *biggest_index)
{
	return MakeNWidgets(_nested_toolb_scen_inner_widgets, lengthof(_nested_toolb_scen_inner_widgets), biggest_index, new NWidgetScenarioToolbarContainer());
}

static const NWidgetPart _nested_toolb_scen_widgets[] = {
	NWidgetFunction(MakeScenarioToolbar),
};

static WindowDesc _toolb_scen_desc(
	WDP_MANUAL, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_UNCLICK_BUTTONS | WDF_NO_FOCUS,
	_nested_toolb_scen_widgets, lengthof(_nested_toolb_scen_widgets)
);

/* --- Allocating the toolbar --- */

void AllocateToolbar()
{
	/* Clean old GUI values; railtype is (re)set by rail_gui.cpp */
	_last_built_roadtype = ROADTYPE_ROAD;

	if (_game_mode == GM_EDITOR) {
		_preferred_toolbar_size = &_toolb_scen_desc.default_width;
		new ScenarioEditorToolbarWindow(&_toolb_scen_desc);
	} else {
		_preferred_toolbar_size = &_toolb_normal_desc.default_width;
		new MainToolbarWindow(&_toolb_normal_desc);
	}
}
