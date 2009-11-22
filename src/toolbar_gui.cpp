/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file toolbar_gui.cpp Code related to the (main) toolbar. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "viewport_func.h"
#include "command_func.h"
#include "variables.h"
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
#include "rail.h"
#include "widgets/dropdown_type.h"

#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"

#include "table/strings.h"
#include "table/sprites.h"

RailType _last_built_railtype;
static RoadType _last_built_roadtype;

enum ToolbarMode {
	TB_NORMAL,
	TB_UPPER,
	TB_LOWER
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
public:
	bool checked;

	DropDownListCheckedItem(StringID string, int result, bool masked, bool checked) : DropDownListStringItem(string, result, masked), checked(checked) {}

	virtual ~DropDownListCheckedItem() {}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		if (checked) {
			DrawString(left + 2, right - 2, top, STR_JUST_CHECKMARK, sel ? TC_WHITE : TC_BLACK);
		}
		DrawString(left + 2, right - 2, top, this->String(), sel ? TC_WHITE : TC_BLACK);
	}
};

/**
 * Drop down list entry for showing a company entry, with companies 'blob'.
 */
class DropDownListCompanyItem : public DropDownListItem {
public:
	bool greyed;

	DropDownListCompanyItem(int result, bool masked, bool greyed) : DropDownListItem(result, masked), greyed(greyed) {}

	virtual ~DropDownListCompanyItem() {}

	bool Selectable() const
	{
		return true;
	}

	uint Width() const
	{
		char buffer[512];
		CompanyID company = (CompanyID)result;
		SetDParam(0, company);
		SetDParam(1, company);
		GetString(buffer, STR_COMPANY_NAME_COMPANY_NUM, lastof(buffer));
		return GetStringBoundingBox(buffer).width + 19;
	}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		CompanyID company = (CompanyID)result;
		bool rtl = _dynlang.text_dir == TD_RTL;

		/* It's possible the company is deleted while the dropdown is open */
		if (!Company::IsValidID(company)) return;

		DrawCompanyIcon(company, rtl ? right - 16 : left + 2, top + 1);

		SetDParam(0, company);
		SetDParam(1, company);
		TextColour col;
		if (this->greyed) {
			col = TC_GREY;
		} else {
			col = sel ? TC_WHITE : TC_BLACK;
		}
		DrawString(rtl ? left + 2 : left + 19, rtl ? right - 19 : right - 2, top, STR_COMPANY_NAME_COMPANY_NUM, col);
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
enum {
	CTMN_CLIENT_LIST = -1, ///< Show the client list
	CTMN_NEW_COMPANY = -2, ///< Create a new company
	CTMN_SPECTATE    = -3, ///< Become spectator
};

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

static void SelectSignTool()
{
	if (_cursor.sprite == SPR_CURSOR_SIGN) {
		ResetObjectToPlace();
	} else {
		SetObjectToPlace(SPR_CURSOR_SIGN, PAL_NONE, HT_RECT, WC_MAIN_TOOLBAR, 0);
		_place_proc = PlaceProc_Sign;
	}
}

/* --- Pausing --- */

static void ToolbarPauseClick(Window *w)
{
	if (_networking && !_network_server) return; // only server can pause the game

	if (DoCommandP(0, PM_PAUSED_NORMAL, _pause_mode == PM_UNPAUSED, CMD_PAUSE)) SndPlayFx(SND_15_BEEP);
}

/* --- Fast forwarding --- */

static void ToolbarFastForwardClick(Window *w)
{
	_fast_forward ^= true;
	SndPlayFx(SND_15_BEEP);
}

/* --- Options button menu --- */

enum OptionMenuEntries {
	OME_GAMEOPTIONS,
	OME_DIFFICULTIES,
	OME_SETTINGS,
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

static void ToolbarOptionsClick(Window *w)
{
	DropDownList *list = new DropDownList();
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_GAME_OPTIONS,             OME_GAMEOPTIONS, false));
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_DIFFICULTY_SETTINGS,      OME_DIFFICULTIES, false));
	list->push_back(new DropDownListStringItem(STR_SETTINGS_MENU_CONFIG_SETTINGS,          OME_SETTINGS, false));
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
}

static void MenuClickSettings(int index)
{
	switch (index) {
		case OME_GAMEOPTIONS:          ShowGameOptions();                               return;
		case OME_DIFFICULTIES:         ShowGameDifficulty();                            return;
		case OME_SETTINGS:             ShowGameSettings();                              return;
		case OME_NEWGRFSETTINGS:       ShowNewGRFSettings(!_networking, true, true, &_grfconfig);   return;
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

static void ToolbarSaveClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SAVEGAME, STR_FILE_MENU_SAVE_GAME, SLNME_MENUCOUNT);
}

static void ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, TBSE_SAVESCENARIO, STR_SCENEDIT_FILE_MENU_SAVE_SCENARIO, SLEME_MENUCOUNT);
}

static void MenuClickSaveLoad(int index = 0)
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

static void ToolbarMapClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SMALLMAP, STR_MAP_MENU_MAP_OF_WORLD, MME_MENUCOUNT_NORMAL);
}

static void ToolbarScenMapTownDir(Window *w)
{
	PopupMainToolbMenu(w, TBSE_SMALLMAP, STR_MAP_MENU_MAP_OF_WORLD, MME_MENUCOUNT_EDITOR);
}

static void MenuClickMap(int index)
{
	switch (index) {
		case MME_SHOW_SMALLMAP:       ShowSmallMap();            break;
		case MME_SHOW_EXTRAVIEWPORTS: ShowExtraViewPortWindow(); break;
		case MME_SHOW_SIGNLISTS:      ShowSignList();            break;
		case MME_SHOW_TOWNDIRECTORY:  if (_game_mode == GM_EDITOR) ShowTownDirectory(); break;
	}
}

/* --- Town button menu --- */

static void ToolbarTownClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_TOWNDIRECTORY, STR_TOWN_MENU_TOWN_DIRECTORY, 1);
}

static void MenuClickTown(int index)
{
	ShowTownDirectory();
}

/* --- Subidies button menu --- */

static void ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SUBSIDIES, STR_SUBSIDIES_MENU_SUBSIDIES, 1);
}

static void MenuClickSubsidies(int index)
{
	ShowSubsidiesList();
}

/* --- Stations button menu --- */

static void ToolbarStationsClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, TBN_STATIONS);
}

static void MenuClickStations(int index)
{
	ShowCompanyStations((CompanyID)index);
}

/* --- Finances button menu --- */

static void ToolbarFinancesClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, TBN_FINANCES);
}

static void MenuClickFinances(int index)
{
	ShowCompanyFinances((CompanyID)index);
}

/* --- Company's button menu --- */

static void ToolbarCompaniesClick(Window *w)
{
	PopupMainCompanyToolbMenu(w, TBN_COMPANIES);
}

static void MenuClickCompany(int index)
{
#ifdef ENABLE_NETWORK
	if (_networking) {
		switch (index) {
			case CTMN_CLIENT_LIST:
				ShowClientList();
				return;

			case CTMN_NEW_COMPANY:
				if (_network_server) {
					DoCommandP(0, 0, _network_own_client_id, CMD_COMPANY_CTRL);
				} else {
					NetworkSend_Command(0, 0, 0, CMD_COMPANY_CTRL, NULL, NULL);
				}
				return;

			case CTMN_SPECTATE:
				if (_network_server) {
					NetworkServerDoMove(CLIENT_ID_SERVER, COMPANY_SPECTATOR);
					MarkWholeScreenDirty();
				} else {
					NetworkClientRequestMove(COMPANY_SPECTATOR);
				}
				return;
		}
	}
#endif /* ENABLE_NETWORK */
	ShowCompany((CompanyID)index);
}

/* --- Graphs button menu --- */

static void ToolbarGraphsClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_GRAPHICS, STR_GRAPH_MENU_OPERATING_PROFIT_GRAPH, (_toolbar_mode == TB_NORMAL) ? 6 : 8);
}

static void MenuClickGraphs(int index)
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
}

/* --- League button menu --- */

static void ToolbarLeagueClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_LEAGUE, STR_GRAPH_MENU_COMPANY_LEAGUE_TABLE, 2);
}

static void MenuClickLeague(int index)
{
	switch (index) {
		case 0: ShowCompanyLeagueTable();      break;
		case 1: ShowPerformanceRatingDetail(); break;
	}
}

/* --- Industries button menu --- */

static void ToolbarIndustryClick(Window *w)
{
	/* Disable build-industry menu if we are a spectator */
	PopupMainToolbMenu(w, TBN_INDUSTRIES, STR_INDUSTRY_MENU_INDUSTRY_DIRECTORY, (_local_company == COMPANY_SPECTATOR) ? 1 : 2);
}

static void MenuClickIndustry(int index)
{
	switch (index) {
		case 0: ShowIndustryDirectory();   break;
		case 1: ShowBuildIndustryWindow(); break;
	}
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


static void ToolbarTrainClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_TRAIN);
}

static void MenuClickShowTrains(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_TRAIN);
}

/* --- Road vehicle button menu --- */

static void ToolbarRoadClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_ROAD);
}

static void MenuClickShowRoad(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_ROAD);
}

/* --- Ship button menu --- */

static void ToolbarShipClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_SHIP);
}

static void MenuClickShowShips(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_SHIP);
}

/* --- Aircraft button menu --- */

static void ToolbarAirClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_AIRCRAFT);
}

static void MenuClickShowAir(int index)
{
	ShowVehicleListWindow((CompanyID)index, VEH_AIRCRAFT);
}

/* --- Zoom in button --- */

static void ToolbarZoomInClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick((_game_mode == GM_EDITOR) ? (byte)TBSE_ZOOMIN : (byte)TBN_ZOOMIN);
		SndPlayFx(SND_15_BEEP);
	}
}

/* --- Zoom out button --- */

static void ToolbarZoomOutClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick((_game_mode == GM_EDITOR) ? (byte)TBSE_ZOOMOUT : (byte)TBN_ZOOMOUT);
		SndPlayFx(SND_15_BEEP);
	}
}

/* --- Rail button menu --- */

static void ToolbarBuildRailClick(Window *w)
{
	const Company *c = Company::Get(_local_company);
	DropDownList *list = new DropDownList();
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		const RailtypeInfo *rti = GetRailTypeInfo(rt);
		/* Skip rail type if it has no label */
		if (rti->label == 0) continue;
		list->push_back(new DropDownListStringItem(rti->strings.menu_text, rt, !HasBit(c->avail_railtypes, rt)));
	}
	ShowDropDownList(w, list, _last_built_railtype, TBN_RAILS, 140, true, true);
	SndPlayFx(SND_15_BEEP);
}

static void MenuClickBuildRail(int index)
{
	_last_built_railtype = (RailType)index;
	ShowBuildRailToolbar(_last_built_railtype, -1);
}

/* --- Road button menu --- */

static void ToolbarBuildRoadClick(Window *w)
{
	const Company *c = Company::Get(_local_company);
	DropDownList *list = new DropDownList();
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		/* The standard road button is *always* available */
		list->push_back(new DropDownListStringItem(STR_ROAD_MENU_ROAD_CONSTRUCTION + rt, rt, !(HasBit(c->avail_roadtypes, rt) || rt == ROADTYPE_ROAD)));
	}
	ShowDropDownList(w, list, _last_built_roadtype, TBN_ROADS, 140, true, true);
	SndPlayFx(SND_15_BEEP);
}

static void MenuClickBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_roadtype);
}

/* --- Water button menu --- */

static void ToolbarBuildWaterClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_WATER, STR_WATERWAYS_MENU_WATERWAYS_CONSTRUCTION, 1);
}

static void MenuClickBuildWater(int index)
{
	ShowBuildDocksToolbar();
}

/* --- Airport button menu --- */

static void ToolbarBuildAirClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_AIR, STR_AIRCRAFT_MENU_AIRPORT_CONSTRUCTION, 1);
}

static void MenuClickBuildAir(int index)
{
	ShowBuildAirToolbar();
}

/* --- Forest button menu --- */

static void ToolbarForestClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_LANDSCAPE, STR_LANDSCAPING_MENU_LANDSCAPING, 3);
}

static void MenuClickForest(int index)
{
	switch (index) {
		case 0: ShowTerraformToolbar();  break;
		case 1: ShowBuildTreesToolbar(); break;
		case 2: SelectSignTool();        break;
	}
}

/* --- Music button menu --- */

static void ToolbarMusicClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_MUSICSOUND, STR_TOOLBAR_SOUND_MUSIC, 1);
}

static void MenuClickMusicWindow(int index)
{
	ShowMusicWindow();
}

/* --- Newspaper button menu --- */

static void ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_NEWSREPORT, STR_NEWS_MENU_LAST_MESSAGE_NEWS_REPORT, 3);
}

static void MenuClickNewspaper(int index)
{
	switch (index) {
		case 0: ShowLastNewsMessage(); break;
		case 1: ShowMessageOptions();  break;
		case 2: ShowMessageHistory();  break;
	}
}

/* --- Help button menu --- */

static void ToolbarHelpClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_HELP, STR_ABOUT_MENU_LAND_BLOCK_INFO, 7);
}

static void MenuClickSmallScreenshot()
{
	RequestScreenshot(SC_VIEWPORT, NULL);
}

static void MenuClickWorldScreenshot()
{
	RequestScreenshot(SC_WORLD, NULL);
}

static void MenuClickHelp(int index)
{
	switch (index) {
		case 0: PlaceLandBlockInfo();       break;
		case 2: IConsoleSwitch();           break;
		case 3: ShowAIDebugWindow();        break;
		case 4: MenuClickSmallScreenshot(); break;
		case 5: MenuClickWorldScreenshot(); break;
		case 6: ShowAboutWindow();          break;
	}
}

/* --- Switch toolbar button --- */

static void ToolbarSwitchClick(Window *w)
{
	if (_toolbar_mode != TB_LOWER) {
		_toolbar_mode = TB_LOWER;
	} else {
		_toolbar_mode = TB_UPPER;
	}

	w->ReInit();
	w->SetWidgetLoweredState(TBN_SWITCHBAR, _toolbar_mode == TB_LOWER);
	SndPlayFx(SND_15_BEEP);
}

/* --- Scenario editor specific handlers. */

static void ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
		w->HandleButtonClick(TBSE_DATEBACKWARD);
		w->SetDirty();

		_settings_game.game_creation.starting_year = Clamp(_settings_game.game_creation.starting_year - 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenDateForward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
		w->HandleButtonClick(TBSE_DATEFORWARD);
		w->SetDirty();

		_settings_game.game_creation.starting_year = Clamp(_settings_game.game_creation.starting_year + 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenGenLand(Window *w)
{
	w->HandleButtonClick(TBSE_LANDGENERATE);
	SndPlayFx(SND_15_BEEP);

	ShowEditorTerraformToolbar();
}


static void ToolbarScenGenTown(Window *w)
{
	w->HandleButtonClick(TBSE_TOWNGENERATE);
	SndPlayFx(SND_15_BEEP);
	ShowFoundTownWindow();
}

static void ToolbarScenGenIndustry(Window *w)
{
	w->HandleButtonClick(TBSE_INDUSTRYGENERATE);
	SndPlayFx(SND_15_BEEP);
	ShowBuildIndustryWindow();
}

static void ToolbarScenBuildRoad(Window *w)
{
	w->HandleButtonClick(TBSE_BUILDROAD);
	SndPlayFx(SND_15_BEEP);
	ShowBuildRoadScenToolbar();
}

static void ToolbarScenBuildDocks(Window *w)
{
	w->HandleButtonClick(TBSE_BUILDDOCKS);
	SndPlayFx(SND_15_BEEP);
	ShowBuildDocksScenToolbar();
}

static void ToolbarScenPlantTrees(Window *w)
{
	w->HandleButtonClick(TBSE_PLANTTREES);
	SndPlayFx(SND_15_BEEP);
	ShowBuildTreesToolbar();
}

static void ToolbarScenPlaceSign(Window *w)
{
	w->HandleButtonClick(TBSE_PLACESIGNS);
	SndPlayFx(SND_15_BEEP);
	SelectSignTool();
}

static void ToolbarBtn_NULL(Window *w)
{
}

typedef void MenuClickedProc(int index);

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

		/* First initialise some variables... */
		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
			child_wid->SetupSmallestSize(w, init_array);
			this->smallest_y = max(this->smallest_y, child_wid->smallest_y + child_wid->padding_top + child_wid->padding_bottom);
			if (this->IsButton(child_wid->type)) {
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

		for (NWidgetBase *child_wid = this->head; child_wid != NULL; child_wid = child_wid->next) {
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
		uint full_buttons = max((width + this->smallest_x - 1) / this->smallest_x, SMALLEST_ARRANGEMENT);
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

typedef void ToolbarButtonProc(Window *w);

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

struct MainToolbarWindow : Window {
	MainToolbarWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc, 0);

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

	virtual void OnClick(Point pt, int widget)
	{
		if (_game_mode != GM_MENU && !this->IsWidgetDisabled(widget)) _toolbar_button_procs[widget](this);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		_menu_clicked_procs[widget](index);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_F1: case WKC_PAUSE: ToolbarPauseClick(this); break;
			case WKC_F2: ShowGameOptions(); break;
			case WKC_F3: MenuClickSaveLoad(); break;
			case WKC_F4: ShowSmallMap(); break;
			case WKC_F5: ShowTownDirectory(); break;
			case WKC_F6: ShowSubsidiesList(); break;
			case WKC_F7: ShowCompanyStations(_local_company); break;
			case WKC_F8: ShowCompanyFinances(_local_company); break;
			case WKC_F9: ShowCompany(_local_company); break;
			case WKC_F10: ShowOperatingProfitGraph(); break;
			case WKC_F11: ShowCompanyLeagueTable(); break;
			case WKC_F12: ShowBuildIndustryWindow(); break;
			case WKC_SHIFT | WKC_F1: ShowVehicleListWindow(_local_company, VEH_TRAIN); break;
			case WKC_SHIFT | WKC_F2: ShowVehicleListWindow(_local_company, VEH_ROAD); break;
			case WKC_SHIFT | WKC_F3: ShowVehicleListWindow(_local_company, VEH_SHIP); break;
			case WKC_SHIFT | WKC_F4: ShowVehicleListWindow(_local_company, VEH_AIRCRAFT); break;
			case WKC_NUM_PLUS: // Fall through
			case WKC_EQUALS: // Fall through
			case WKC_SHIFT | WKC_EQUALS: // Fall through
			case WKC_SHIFT | WKC_F5: ToolbarZoomInClick(this); break;
			case WKC_NUM_MINUS: // Fall through
			case WKC_MINUS: // Fall through
			case WKC_SHIFT | WKC_MINUS: // Fall through
			case WKC_SHIFT | WKC_F6: ToolbarZoomOutClick(this); break;
			case WKC_SHIFT | WKC_F7: if (CanBuildVehicleInfrastructure(VEH_TRAIN)) ShowBuildRailToolbar(_last_built_railtype, -1); break;
			case WKC_SHIFT | WKC_F8: ShowBuildRoadToolbar(_last_built_roadtype); break;
			case WKC_SHIFT | WKC_F9: ShowBuildDocksToolbar(); break;
			case WKC_SHIFT | WKC_F10: if (CanBuildVehicleInfrastructure(VEH_AIRCRAFT)) ShowBuildAirToolbar(); break;
			case WKC_SHIFT | WKC_F11: ShowBuildTreesToolbar(); break;
			case WKC_SHIFT | WKC_F12: ShowMusicWindow(); break;
			case WKC_CTRL  | 'S': MenuClickSmallScreenshot(); break;
			case WKC_CTRL  | 'G': MenuClickWorldScreenshot(); break;
			case WKC_CTRL | WKC_ALT | 'C': if (!_networking) ShowCheatWindow(); break;
			case 'A': if (CanBuildVehicleInfrastructure(VEH_TRAIN)) ShowBuildRailToolbar(_last_built_railtype, 4); break; // Invoke Autorail
			case 'L': ShowTerraformToolbar(); break;
			case 'Q': case 'W': case 'E': case 'D': ShowTerraformToolbarWithTool(key, keycode); break;
			case 'M': ShowSmallMap(); break;
			case 'V': ShowExtraViewPortWindow(); break;
			default: return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
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
};

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

static const WindowDesc _toolb_normal_desc(
	0, 0, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_NO_FOCUS,
	_nested_toolbar_normal_widgets, lengthof(_nested_toolbar_normal_widgets)
);


/* --- Toolbar handling for the scenario editor */

static ToolbarButtonProc * const _scen_toolbar_button_procs[] = {
	ToolbarPauseClick,
	ToolbarFastForwardClick,
	ToolbarOptionsClick,
	ToolbarScenSaveOrLoad,
	ToolbarBtn_NULL,
	ToolbarBtn_NULL,
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

struct ScenarioEditorToolbarWindow : Window {
public:
	ScenarioEditorToolbarWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc, 0);

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
				DrawString(r.left, r.right, (this->height - FONT_HEIGHT_NORMAL) / 2, STR_WHITE_DATE_LONG, TC_FROMSTRING, SA_CENTER);
				break;

			case TBSE_SPACERPANEL: {
				int height = r.bottom - r.top;
				if (height > 2 * FONT_HEIGHT_NORMAL) {
					DrawString(r.left, r.right, (height + 1) / 2 - FONT_HEIGHT_NORMAL, STR_SCENEDIT_TOOLBAR_OPENTTD, TC_FROMSTRING, SA_CENTER);
					DrawString(r.left, r.right, (height + 1) / 2, STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR, TC_FROMSTRING, SA_CENTER);
				} else {
					DrawString(r.left, r.right, (height - FONT_HEIGHT_NORMAL) / 2, STR_SCENEDIT_TOOLBAR_SCENARIO_EDITOR, TC_FROMSTRING, SA_CENTER);
				}
			} break;
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

	virtual void OnClick(Point pt, int widget)
	{
		if (_game_mode == GM_MENU) return;
		_scen_toolbar_button_procs[widget](this);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		/* The map button is in a different location on the scenario
		 * editor toolbar, so we need to adjust for it. */
		if (widget == TBSE_SMALLMAP) widget = TBN_SMALLMAP;
		_menu_clicked_procs[widget](index);
		SndPlayFx(SND_15_BEEP);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_F1: case WKC_PAUSE: ToolbarPauseClick(this); break;
			case WKC_F2: ShowGameOptions(); break;
			case WKC_F3: MenuClickSaveLoad(); break;
			case WKC_F4: ToolbarScenGenLand(this); break;
			case WKC_F5: ToolbarScenGenTown(this); break;
			case WKC_F6: ToolbarScenGenIndustry(this); break;
			case WKC_F7: ToolbarScenBuildRoad(this); break;
			case WKC_F8: ToolbarScenBuildDocks(this); break;
			case WKC_F9: ToolbarScenPlantTrees(this); break;
			case WKC_F10: ToolbarScenPlaceSign(this); break;
			case WKC_F11: ShowMusicWindow(); break;
			case WKC_F12: PlaceLandBlockInfo(); break;
			case WKC_CTRL | 'S': MenuClickSmallScreenshot(); break;
			case WKC_CTRL | 'G': MenuClickWorldScreenshot(); break;

			/* those following are all fall through */
			case WKC_NUM_PLUS:
			case WKC_EQUALS:
			case WKC_SHIFT | WKC_EQUALS:
			case WKC_SHIFT | WKC_F5: ToolbarZoomInClick(this); break;

			/* those following are all fall through */
			case WKC_NUM_MINUS:
			case WKC_MINUS:
			case WKC_SHIFT | WKC_MINUS:
			case WKC_SHIFT | WKC_F6: ToolbarZoomOutClick(this); break;

			case 'L': ShowEditorTerraformToolbar(); break;
			case 'Q': case 'W': case 'E': case 'D': ShowEditorTerraformToolbarWithTool(key, keycode); break;
			case 'M': ShowSmallMap(); break;
			case 'V': ShowExtraViewPortWindow(); break;
			default: return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		_place_proc(tile);
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
};

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
			NWidget(WWT_EMPTY, COLOUR_GREY, TBSE_DATEPANEL),
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

static const WindowDesc _toolb_scen_desc(
	0, 0, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_NO_FOCUS,
	_nested_toolb_scen_widgets, lengthof(_nested_toolb_scen_widgets)
);

/* --- Allocating the toolbar --- */

void AllocateToolbar()
{
	/* Clean old GUI values; railtype is (re)set by rail_gui.cpp */
	_last_built_roadtype = ROADTYPE_ROAD;

	if (_game_mode == GM_EDITOR) {
		new ScenarioEditorToolbarWindow(&_toolb_scen_desc);;
	} else {
		new MainToolbarWindow(&_toolb_normal_desc);
	}
}
