/* $Id$ */

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
#include "settings_type.h"
#include "newgrf_config.h"

#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"

#include "table/strings.h"
#include "table/sprites.h"

static void SplitToolbar(Window *w);

RailType _last_built_railtype;
RoadType _last_built_roadtype;

/** This enum gathers properties of both toolbars */
enum ToolBarProperties {
	TBP_BUTTONWIDTH        =  22,  ///< width of a button
	TBP_BUTTONHEIGHT       =  22,  ///< height of a button as well as the toolbars
	TBP_DATEPANELWIDTH     = 130,  ///< used in scenario editor to calculate width of the toolbar.

	TBP_TOOLBAR_MINBUTTON  =  14,  ///< references both toolbars
	TBP_NORMAL_MAXBUTTON   =  19,  ///< normal toolbar has this many buttons
	TBP_SCENARIO_MAXBUTTON =  16,  ///< while the scenario has these
};

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
};

enum ToolbarScenEditorWidgets {
	TBSE_PAUSE        = 0,
	TBSE_FASTFORWARD,
	TBSE_SAVESCENARIO = 3,
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
};

/**
 * Drop down list entry for showing a checked/unchecked toggle item.
 */
class DropDownListCheckedItem : public DropDownListStringItem {
public:
	bool checked;

	DropDownListCheckedItem(StringID string, int result, bool masked, bool checked) : DropDownListStringItem(string, result, masked), checked(checked) {}

	virtual ~DropDownListCheckedItem() {}

	void Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
	{
		if (checked) {
			DrawString(x + 2, y, STR_CHECKMARK, sel ? TC_WHITE : TC_BLACK);
		}
		DrawStringTruncated(x + 2, y, this->String(), sel ? TC_WHITE : TC_BLACK, width);
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
		GetString(buffer, STR_7021, lastof(buffer));
		return GetStringBoundingBox(buffer).width + 19;
	}

	void Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
	{
		CompanyID company = (CompanyID)result;
		DrawCompanyIcon(company, x + 2, y + 1);

		SetDParam(0, company);
		SetDParam(1, company);
		TextColour col;
		if (this->greyed) {
			col = TC_GREY;
		} else {
			col = sel ? TC_WHITE : TC_BLACK;
		}
		DrawStringTruncated(x + 19, y, STR_7021, col, width - 17);
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
		list->push_back(new DropDownListStringItem(STR_NETWORK_CLIENT_LIST, CTMN_CLIENT_LIST, false));

		if (_local_company == COMPANY_SPECTATOR) {
			list->push_back(new DropDownListStringItem(STR_NETWORK_COMPANY_LIST_NEW_COMPANY, CTMN_NEW_COMPANY, NetworkMaxCompaniesReached()));
		} else {
			list->push_back(new DropDownListStringItem(STR_NETWORK_COMPANY_LIST_SPECTATE, CTMN_SPECTATE, NetworkMaxSpectatorsReached()));
		}
	}
#endif /* ENABLE_NETWORK */

	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (!IsValidCompanyID(c)) continue;
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
		SetObjectToPlace(SPR_CURSOR_SIGN, PAL_NONE, VHM_RECT, WC_MAIN_TOOLBAR, 0);
		_place_proc = PlaceProc_Sign;
	}
}

/* --- Pausing --- */

static void ToolbarPauseClick(Window *w)
{
	if (_networking && !_network_server) return; // only server can pause the game

	if (DoCommandP(0, _pause_game ? 0 : 1, 0, CMD_PAUSE)) SndPlayFx(SND_15_BEEP);
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
	OME_SHOW_SIGNS,
	OME_SHOW_WAYPOINTNAMES,
	OME_FULL_ANIMATION,
	OME_FULL_DETAILS,
	OME_TRANSPARENTBUILDINGS,
	OME_SHOW_STATIONSIGNS,
};

static void ToolbarOptionsClick(Window *w)
{
	DropDownList *list = new DropDownList();
	list->push_back(new DropDownListStringItem(STR_02C4_GAME_OPTIONS,        OME_GAMEOPTIONS, false));
	list->push_back(new DropDownListStringItem(STR_02C6_DIFFICULTY_SETTINGS, OME_DIFFICULTIES, false));
	list->push_back(new DropDownListStringItem(STR_MENU_CONFIG_SETTINGS,      OME_SETTINGS, false));
	list->push_back(new DropDownListStringItem(STR_NEWGRF_SETTINGS,          OME_NEWGRFSETTINGS, false));
	list->push_back(new DropDownListStringItem(STR_TRANSPARENCY_OPTIONS,     OME_TRANSPARENCIES, false));
	list->push_back(new DropDownListItem(-1, false));
	list->push_back(new DropDownListCheckedItem(STR_02CA_TOWN_NAMES_DISPLAYED,    OME_SHOW_TOWNNAMES, false, HasBit(_display_opt, DO_SHOW_TOWN_NAMES)));
	list->push_back(new DropDownListCheckedItem(STR_02CC_STATION_NAMES_DISPLAYED, OME_SHOW_STATIONNAMES, false, HasBit(_display_opt, DO_SHOW_STATION_NAMES)));
	list->push_back(new DropDownListCheckedItem(STR_02CE_SIGNS_DISPLAYED,         OME_SHOW_SIGNS, false, HasBit(_display_opt, DO_SHOW_SIGNS)));
	list->push_back(new DropDownListCheckedItem(STR_WAYPOINTS_DISPLAYED2,         OME_SHOW_WAYPOINTNAMES, false, HasBit(_display_opt, DO_WAYPOINTS)));
	list->push_back(new DropDownListCheckedItem(STR_02D0_FULL_ANIMATION,          OME_FULL_ANIMATION, false, HasBit(_display_opt, DO_FULL_ANIMATION)));
	list->push_back(new DropDownListCheckedItem(STR_02D2_FULL_DETAIL,             OME_FULL_DETAILS, false, HasBit(_display_opt, DO_FULL_DETAIL)));
	list->push_back(new DropDownListCheckedItem(STR_02D4_TRANSPARENT_BUILDINGS,   OME_TRANSPARENTBUILDINGS, false, IsTransparencySet(TO_HOUSES)));
	list->push_back(new DropDownListCheckedItem(STR_TRANSPARENT_SIGNS,            OME_SHOW_STATIONSIGNS, false, IsTransparencySet(TO_SIGNS)));

	ShowDropDownList(w, list, 0, TBN_SETTINGS, 140, true, true);
	SndPlayFx(SND_15_BEEP);
}

static void MenuClickSettings(int index)
{
	switch (index) {
		case OME_GAMEOPTIONS:          ShowGameOptions();                              return;
		case OME_DIFFICULTIES:         ShowGameDifficulty();                           return;
		case OME_SETTINGS:             ShowGameSettings();                             return;
		case OME_NEWGRFSETTINGS:       ShowNewGRFSettings(!_networking, true, true, &_grfconfig);   return;
		case OME_TRANSPARENCIES:       ShowTransparencyToolbar();                      break;

		case OME_SHOW_TOWNNAMES:       ToggleBit(_display_opt, DO_SHOW_TOWN_NAMES);    break;
		case OME_SHOW_STATIONNAMES:    ToggleBit(_display_opt, DO_SHOW_STATION_NAMES); break;
		case OME_SHOW_SIGNS:           ToggleBit(_display_opt, DO_SHOW_SIGNS);         break;
		case OME_SHOW_WAYPOINTNAMES:   ToggleBit(_display_opt, DO_WAYPOINTS);          break;
		case OME_FULL_ANIMATION:       ToggleBit(_display_opt, DO_FULL_ANIMATION);     break;
		case OME_FULL_DETAILS:         ToggleBit(_display_opt, DO_FULL_DETAIL);        break;
		case OME_TRANSPARENTBUILDINGS: ToggleTransparency(TO_HOUSES);                  break;
		case OME_SHOW_STATIONSIGNS:    ToggleTransparency(TO_SIGNS);                   break;
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
	PopupMainToolbMenu(w, TBN_SAVEGAME, STR_015C_SAVE_GAME, SLNME_MENUCOUNT);
}

static void ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, TBSE_SAVESCENARIO, STR_0292_SAVE_SCENARIO, SLEME_MENUCOUNT);
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
	PopupMainToolbMenu(w, TBN_SMALLMAP, STR_02DE_MAP_OF_WORLD, MME_MENUCOUNT_NORMAL);
}

static void ToolbarScenMapTownDir(Window *w)
{
	PopupMainToolbMenu(w, TBSE_SMALLMAP, STR_02DE_MAP_OF_WORLD, MME_MENUCOUNT_EDITOR);
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
	PopupMainToolbMenu(w, TBN_TOWNDIRECTORY, STR_02BB_TOWN_DIRECTORY, 1);
}

static void MenuClickTown(int index)
{
	ShowTownDirectory();
}

/* --- Subidies button menu --- */

static void ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_SUBSIDIES, STR_02DD_SUBSIDIES, 1);
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
	PopupMainToolbMenu(w, TBN_GRAPHICS, STR_0154_OPERATING_PROFIT_GRAPH, (_toolbar_mode == TB_NORMAL) ? 6 : 8);
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
	PopupMainToolbMenu(w, TBN_LEAGUE, STR_015A_COMPANY_LEAGUE_TABLE, 2);
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
	PopupMainToolbMenu(w, TBN_INDUSTRIES, STR_INDUSTRY_DIR, (_local_company == COMPANY_SPECTATOR) ? 1 : 2);
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
	const Company *c = GetCompany(_local_company);
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
	const Company *c = GetCompany(_local_company);
	DropDownList *list = new DropDownList();
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		/* The standard road button is *always* available */
		list->push_back(new DropDownListStringItem(STR_180A_ROAD_CONSTRUCTION + rt, rt, !(HasBit(c->avail_roadtypes, rt) || rt == ROADTYPE_ROAD)));
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
	PopupMainToolbMenu(w, TBN_WATER, STR_9800_WATERWAYS_CONSTRUCTION, 1);
}

static void MenuClickBuildWater(int index)
{
	ShowBuildDocksToolbar();
}

/* --- Airport button menu --- */

static void ToolbarBuildAirClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_AIR, STR_A01D_AIRPORT_CONSTRUCTION, 1);
}

static void MenuClickBuildAir(int index)
{
	ShowBuildAirToolbar();
}

/* --- Forest button menu --- */

static void ToolbarForestClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_LANDSCAPE, STR_LANDSCAPING, 3);
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
	PopupMainToolbMenu(w, TBN_MUSICSOUND, STR_01D3_SOUND_MUSIC, 1);
}

static void MenuClickMusicWindow(int index)
{
	ShowMusicWindow();
}

/* --- Newspaper button menu --- */

static void ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbMenu(w, TBN_NEWSREPORT, STR_0200_LAST_MESSAGE_NEWS_REPORT, 3);
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
	PopupMainToolbMenu(w, TBN_HELP, STR_02D5_LAND_BLOCK_INFO, 7);
}

static void MenuClickSmallScreenshot()
{
	SetScreenshotType(SC_VIEWPORT);
}

static void MenuClickWorldScreenshot()
{
	SetScreenshotType(SC_WORLD);
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

	SplitToolbar(w);
	w->HandleButtonClick(TBN_SWITCHBAR);
	SetWindowDirty(w);
	SndPlayFx(SND_15_BEEP);
}

/* --- Scenario editor specific handlers. */

static void ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
		w->HandleButtonClick(TBSE_DATEBACKWARD);
		w->SetDirty();

		_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year - 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenDateForward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= WF_TIMEOUT_TRIGGER) {
		w->HandleButtonClick(TBSE_DATEFORWARD);
		w->SetDirty();

		_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));
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
	ShowBuildTownWindow();
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

/* --- Resizing the toolbar */

static void ResizeToolbar(Window *w)
{
	/* There are 27 buttons plus some spacings if the space allows it */
	uint button_width;
	uint spacing;
	uint widgetcount = w->widget_count - 1;

	if (w->width >= (int)widgetcount * TBP_BUTTONWIDTH) {
		button_width = TBP_BUTTONWIDTH;
		spacing = w->width - (widgetcount * button_width);
	} else {
		button_width = w->width / widgetcount;
		spacing = 0;
	}

	static const uint extra_spacing_at[] = { 4, 8, 13, 17, 19, 24, 0 };
	uint i = 0;
	for (uint x = 0, j = 0; i < widgetcount; i++) {
		if (extra_spacing_at[j] == i) {
			j++;
			uint add = spacing / (lengthof(extra_spacing_at) - j);
			spacing -= add;
			x += add;
		}

		w->widget[i].type = WWT_IMGBTN;
		w->widget[i].left = x;
		x += (spacing != 0) ? button_width : (w->width - x) / (widgetcount - i);
		w->widget[i].right = x - 1;
	}

	w->widget[i].type = WWT_EMPTY; // i now points to the last item
	_toolbar_mode = TB_NORMAL;
}

/* --- Split the toolbar */

static void SplitToolbar(Window *w)
{
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

	static const byte *arrangements[] = { arrange14, arrange15, arrange16, arrange17, arrange18, arrange19 };

	uint max_icons = max(TBP_TOOLBAR_MINBUTTON, (ToolBarProperties)((w->width + TBP_BUTTONWIDTH / 2) / TBP_BUTTONWIDTH));

	assert(max_icons >= TBP_TOOLBAR_MINBUTTON && max_icons <= TBP_NORMAL_MAXBUTTON);

	/* first hide all icons */
	for (uint i = 0; i < w->widget_count - 1; i++) {
		w->widget[i].type = WWT_EMPTY;
	}

	/* now activate them all on their proper positions */
	for (uint i = 0, x = 0, n = max_icons - TBP_TOOLBAR_MINBUTTON; i < max_icons; i++) {
		uint icon = arrangements[n][i + ((_toolbar_mode == TB_LOWER) ? max_icons : 0)];
		w->widget[icon].type = WWT_IMGBTN;
		w->widget[icon].left = x;
		x += (w->width - x) / (max_icons - i);
		w->widget[icon].right = x - 1;
	}
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
	MainToolbarWindow(const WindowDesc *desc) : Window(desc)
	{
		this->SetWidgetDisabledState(TBN_PAUSE, _networking && !_network_server); // if not server, disable pause button
		this->SetWidgetDisabledState(TBN_FASTFORWARD, _networking); // if networking, disable fast-forward button

		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);

		this->FindWindowPlacementAndResize(desc);
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	virtual void OnPaint()
	{
		/* Draw brown-red toolbar bg. */
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB2);
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB4, FILLRECT_CHECKER);

		/* If spectator, disable all construction buttons
		 * ie : Build road, rail, ships, airports and landscaping
		 * Since enabled state is the default, just disable when needed */
		this->SetWidgetsDisabledState(_local_company == COMPANY_SPECTATOR, TBN_RAILS, TBN_ROADS, TBN_WATER, TBN_AIR, TBN_LANDSCAPE, WIDGET_LIST_END);
		/* disable company list drop downs, if there are no companies */
		this->SetWidgetsDisabledState(ActiveCompanyCount() == TBN_PAUSE, TBN_STATIONS, TBN_FINANCES, TBN_TRAINS, TBN_ROADVEHS, TBN_SHIPS, TBN_AIRCRAFTS, WIDGET_LIST_END);

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
		if (this->IsWidgetLowered(TBN_PAUSE) != !!_pause_game) {
			this->ToggleWidgetLoweredState(TBN_PAUSE);
			this->InvalidateWidget(TBN_PAUSE);
		}

		if (this->IsWidgetLowered(TBN_FASTFORWARD) != !!_fast_forward) {
			this->ToggleWidgetLoweredState(TBN_FASTFORWARD);
			this->InvalidateWidget(TBN_FASTFORWARD);
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (this->width <= TBP_NORMAL_MAXBUTTON * TBP_BUTTONWIDTH) {
			SplitToolbar(this);
		} else {
			ResizeToolbar(this);
		}
	}

	virtual void OnTimeout()
	{
		for (uint i = TBN_SETTINGS; i < this->widget_count - 1; i++) {
			if (this->IsWidgetLowered(i)) {
				this->RaiseWidget(i);
				this->InvalidateWidget(i);
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (FindWindowById(WC_MAIN_WINDOW, 0) != NULL) HandleZoomMessage(this, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, TBN_ZOOMIN, TBN_ZOOMOUT);
	}
};

static const Widget _toolb_normal_widgets[] = {
{     WWT_IMGBTN,   RESIZE_LEFT,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_PAUSE,           STR_0171_PAUSE_GAME},               // TBN_PAUSE
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_FASTFORWARD,     STR_FAST_FORWARD},                  // TBN_FASTFORWARD
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_SETTINGS,        STR_0187_OPTIONS},                  // TBN_SETTINGS
{   WWT_IMGBTN_2,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_SAVE,            STR_0172_SAVE_GAME_ABANDON_GAME},   // TBN_SAVEGAME

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_SMALLMAP,        STR_0174_DISPLAY_MAP},              // TBN_SMALLMAP
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_TOWN,            STR_0176_DISPLAY_TOWN_DIRECTORY},   // TBN_TOWNDIRECTORY
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_SUBSIDIES,       STR_02DC_DISPLAY_SUBSIDIES},        // TBN_SUBSIDIES
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_COMPANY_LIST,    STR_0173_DISPLAY_LIST_OF_COMPANY},  // TBN_STATIONS

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_COMPANY_FINANCE, STR_0177_DISPLAY_COMPANY_FINANCES}, // TBN_FINANCES
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_COMPANY_GENERAL, STR_0178_DISPLAY_COMPANY_GENERAL},  // TBN_COMPANIES
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_GRAPHS,          STR_0179_DISPLAY_GRAPHS},           // TBN_GRAPHICS
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_COMPANY_LEAGUE,  STR_017A_DISPLAY_COMPANY_LEAGUE},   // TBN_LEAGUE
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_INDUSTRY,        STR_0312_FUND_CONSTRUCTION_OF_NEW}, // TBN_INDUSTRIES

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_TRAINLIST,       STR_017B_DISPLAY_LIST_OF_COMPANY},  // TBN_TRAINS
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_TRUCKLIST,       STR_017C_DISPLAY_LIST_OF_COMPANY},  // TBN_ROADVEHS
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_SHIPLIST,        STR_017D_DISPLAY_LIST_OF_COMPANY},  // TBN_SHIPS
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_AIRPLANESLIST,   STR_017E_DISPLAY_LIST_OF_COMPANY},  // TBN_AIRCRAFTS

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_ZOOMIN,          STR_017F_ZOOM_THE_VIEW_IN},         // TBN_ZOOMIN
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_ZOOMOUT,         STR_0180_ZOOM_THE_VIEW_OUT},        // TBN_ZOOMOUT

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_BUILDRAIL,       STR_0181_BUILD_RAILROAD_TRACK},     // TBN_RAILS
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_BUILDROAD,       STR_0182_BUILD_ROADS},              // TBN_ROADS
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_BUILDWATER,      STR_0183_BUILD_SHIP_DOCKS},         // TBN_WATER
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_BUILDAIR,        STR_0184_BUILD_AIRPORTS},           // TBN_AIR
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_LANDSCAPING,     STR_LANDSCAPING_TOOLBAR_TIP},       // TBN_LANDSCAPE tree icon is SPR_IMG_PLANTTREES

{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_MUSIC,           STR_01D4_SHOW_SOUND_MUSIC_WINDOW},  // TBN_MUSICSOUND
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_MESSAGES,        STR_0203_SHOW_LAST_MESSAGE_NEWS},   // TBN_NEWSREPORT
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_IMG_QUERY,           STR_0186_LAND_BLOCK_INFORMATION},   // TBN_HELP
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,    21, SPR_SWITCH_TOOLBAR,      STR_EMPTY},                         // TBN_SWITCHBAR
{   WIDGETS_END},
};

static const WindowDesc _toolb_normal_desc(
	0, 0, 0, TBP_BUTTONHEIGHT, 640, TBP_BUTTONHEIGHT,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_NO_FOCUS,
	_toolb_normal_widgets
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
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ToolbarMusicClick,
	NULL,
	ToolbarHelpClick,
};

struct ScenarioEditorToolbarWindow : Window {
public:
	ScenarioEditorToolbarWindow(const WindowDesc *desc) : Window(desc)
	{
		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);

		this->FindWindowPlacementAndResize(desc);
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	virtual void OnPaint()
	{
		this->SetWidgetDisabledState(TBSE_DATEBACKWARD, _settings_newgame.game_creation.starting_year <= MIN_YEAR);
		this->SetWidgetDisabledState(TBSE_DATEFORWARD, _settings_newgame.game_creation.starting_year >= MAX_YEAR);

		/* Draw brown-red toolbar bg. */
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB2);
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB4, FILLRECT_CHECKER);

		this->DrawWidgets();

		SetDParam(0, ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));
		DrawStringCenteredTruncated(this->widget[TBSE_DATEBACKWARD].right, this->widget[TBSE_DATEFORWARD].left, 6, STR_00AF, TC_FROMSTRING);

		/* We hide this panel when the toolbar space gets too small */
		const Widget *panel = &this->widget[TBSE_SPACERPANEL];
		if (panel->left != panel->right) {
			DrawStringCenteredTruncated(panel->left + 1, panel->right - 1,  1, STR_0221_OPENTTD, TC_FROMSTRING);
			DrawStringCenteredTruncated(panel->left + 1, panel->right - 1, 11, STR_0222_SCENARIO_EDITOR, TC_FROMSTRING);
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

	virtual void OnResize(Point new_size, Point delta)
	{
		/* There are 16 buttons plus some spacings if the space allows it.
		 * Furthermore there are two panels of which one is non - essential
		 * and that one can be removed if the space is too small. */
		uint buttons_width;
		uint spacing;

		static const int normal_min_width = (TBP_SCENARIO_MAXBUTTON * TBP_BUTTONWIDTH) + (2 * TBP_DATEPANELWIDTH);
		static const int one_less_panel_min_width = (TBP_SCENARIO_MAXBUTTON * TBP_BUTTONWIDTH) + TBP_DATEPANELWIDTH;

		if (this->width >= one_less_panel_min_width) {
			buttons_width = TBP_SCENARIO_MAXBUTTON * TBP_BUTTONWIDTH;
			spacing = this->width - ((this->width >= normal_min_width) ? normal_min_width : one_less_panel_min_width);
		} else {
			buttons_width = this->width - TBP_DATEPANELWIDTH;
			spacing = 0;
		}
		static const uint extra_spacing_at[] = { 3, 4, 7, 8, 10, 17, 0 };

		for (uint i = 0, x = 0, j = 0, b = 0; i < this->widget_count; i++) {
			switch (i) {
				case TBSE_SPACERPANEL:
					this->widget[i].left = x;
					if (this->width < normal_min_width) {
						this->widget[i].right = x;
						j++;
						continue;
					}

					x += TBP_DATEPANELWIDTH;
					this->widget[i].right = x - 1;
					break;

				case TBSE_DATEPANEL: {
					int offset = x - this->widget[i].left;
					this->widget[i + 1].left  += offset;
					this->widget[i + 1].right += offset;
					this->widget[i + 2].left  += offset;
					this->widget[i + 2].right += offset;
					this->widget[i].left = x;
					x += TBP_DATEPANELWIDTH;
					this->widget[i].right = x - 1;
					i += 2;
				} break;

				default:
					if (this->widget[i].bottom == 0) continue;

					this->widget[i].left = x;
					x += buttons_width / (TBP_SCENARIO_MAXBUTTON - b);
					this->widget[i].right = x - 1;
					buttons_width -= buttons_width / (TBP_SCENARIO_MAXBUTTON - b);
					b++;
					break;
			}

			if (extra_spacing_at[j] == i) {
				j++;
				uint add = spacing / (lengthof(extra_spacing_at) - j);
				spacing -= add;
				x += add;
			}
		}
	}

	virtual void OnTick()
	{
		if (this->IsWidgetLowered(TBSE_PAUSE) != !!_pause_game) {
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

static const Widget _toolb_scen_widgets[] = {
{  WWT_IMGBTN, RESIZE_LEFT,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_PAUSE,       STR_0171_PAUSE_GAME},                  // TBSE_PAUSE
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_FASTFORWARD, STR_FAST_FORWARD},                     // TBSE_FASTFORWARD
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_SETTINGS,    STR_0187_OPTIONS},
{WWT_IMGBTN_2, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_SAVE,        STR_0297_SAVE_SCENARIO_LOAD_SCENARIO}, // TBSE_SAVESCENARIO

{   WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, 0x0,                 STR_NULL},                             // TBSE_SPACERPANEL

{   WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0, 129,  0, 21, 0x0,                 STR_NULL},                             // TBSE_DATEPANEL
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   3,  14,  5, 16, SPR_ARROW_DOWN,      STR_029E_MOVE_THE_STARTING_DATE},      // TBSE_DATEBACKWARD
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY, 113, 125,  5, 16, SPR_ARROW_UP,        STR_029F_MOVE_THE_STARTING_DATE},      // TBSE_DATEFORWARD

{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_SMALLMAP,    STR_0175_DISPLAY_MAP_TOWN_DIRECTORY},  // TBSE_SMALLMAP

{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_ZOOMIN,      STR_017F_ZOOM_THE_VIEW_IN},            // TBSE_ZOOMIN
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_ZOOMOUT,     STR_0180_ZOOM_THE_VIEW_OUT},           // TBSE_ZOOMOUT

{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_LANDSCAPING, STR_022E_LANDSCAPE_GENERATION},        // TBSE_LANDGENERATE
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_TOWN,        STR_022F_TOWN_GENERATION},             // TBSE_TOWNGENERATE
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_INDUSTRY,    STR_0230_INDUSTRY_GENERATION},         // TBSE_INDUSTRYGENERATE
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_BUILDROAD,   STR_0231_ROAD_CONSTRUCTION},           // TBSE_BUILDROAD
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_BUILDWATER,  STR_0183_BUILD_SHIP_DOCKS},            // TBSE_BUILDDOCKS
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_PLANTTREES,  STR_0288_PLANT_TREES},                 // TBSE_PLANTTREES
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_SIGN,        STR_0289_PLACE_SIGN},                  // TBSE_PLACESIGNS

{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_MUSIC,       STR_01D4_SHOW_SOUND_MUSIC_WINDOW},
{   WWT_EMPTY, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0,  0, 0x0,                 STR_NULL},
{  WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,   0,  0, 21, SPR_IMG_QUERY,       STR_0186_LAND_BLOCK_INFORMATION},
{WIDGETS_END},
};

static const WindowDesc _toolb_scen_desc(
	0, 0, 130, TBP_BUTTONHEIGHT, 640, TBP_BUTTONHEIGHT,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_NO_FOCUS,
	_toolb_scen_widgets
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
