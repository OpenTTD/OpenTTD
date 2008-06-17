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
#include "train.h"
#include "roadveh.h"
#include "vehicle_gui.h"
#include "rail_gui.h"
#include "road_gui.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "terraform_gui.h"
#include "transparency.h"
#include "strings_func.h"
#include "player_base.h"
#include "player_func.h"
#include "player_gui.h"
#include "settings_type.h"
#include "toolbar_gui.h"
#include "vehicle_base.h"
#include "gfx_func.h"
#include "cheat_func.h"
#include "transparency_gui.h"
#include "screenshot.h"
#include "newgrf_config.h"
#include "signs_func.h"
#include "fios.h"
#include "functions.h"
#include "console_gui.h"
#include "news_gui.h"
#include "tilehighlight_func.h"

#include "network/network.h"
#include "network/network_gui.h"

#include "table/strings.h"
#include "table/sprites.h"

static void PopupMainToolbMenu(Window *parent, uint16 parent_button, StringID base_string, byte item_count, byte disabled_mask = 0, int sel_index = 0, int checked_items = 0);
static void PopupMainPlayerToolbMenu(Window *parent, int main_button, int gray);
static void SplitToolbar(Window *w);

RailType _last_built_railtype;
RoadType _last_built_roadtype;

enum ToolbarMode {
	TB_NORMAL,
	TB_UPPER,
	TB_LOWER
};

enum ToolbarScenEditorWidgets {
	TBSE_PAUSE        = 0,
	TBSE_FASTFORWARD,
	TBSE_SPACERPANEL  = 4,
	TBSE_DATEBACKWARD = 6,
	TBSE_DATEFORWARD,
	TBSE_ZOOMIN       = 9,
	TBSE_ZOOMOUT,
	TBSE_LANDGENERATE,
	TBSE_TOWNGENERATE,
	TBSE_INDUSTRYGENERATE,
	TBSE_BUILDROAD,
	TBSE_PLANTTREES,
	TBSE_PLACESIGNS,
};

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

/** Returns the position where the toolbar wants the menu to appear.
 * Make sure the dropdown is fully visible within the window.
 * x + w->left because x is supposed to be the offset of the toolbar-button
 * we clicked on and w->left the toolbar window itself. So meaning that
 * the default position is aligned with the left side of the clicked button */
static Point GetToolbarDropdownPos(uint16 parent_button, int width, int height)
{
	const Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
	Point pos;
	pos.x = w->widget[GB(parent_button, 0, 8)].left;
	pos.x = w->left + Clamp(pos.x, 0, w->width - width);
	pos.y = w->height;

	return pos;
}

/**
 * Retrieve the menu item number from a position
 * @param w Window holding the menu items
 * @param x X coordinate of the position
 * @param y Y coordinate of the position
 * @return Index number of the menu item, or \c -1 if no valid selection under position
 */
static int GetMenuItemIndex(const Window *w, int item_count, int disabled_items)
{
	int x = _cursor.pos.x;
	int y = _cursor.pos.y;

	if ((x -= w->left) >= 0 && x < w->width && (y -= w->top + 1) >= 0) {
		y /= 10;

		if (y < item_count &&
				!HasBit(disabled_items, y)) {
			return y;
		}
	}
	return -1;
}

/* --- Pausing --- */

static void ToolbarPauseClick(Window *w)
{
	if (_networking && !_network_server) return; // only server can pause the game

	if (DoCommandP(0, _pause_game ? 0 : 1, 0, NULL, CMD_PAUSE)) SndPlayFx(SND_15_BEEP);
}

/* --- Fast forwarding --- */

static void ToolbarFastForwardClick(Window *w)
{
	_fast_forward ^= true;
	SndPlayFx(SND_15_BEEP);
}

/* --- Options button menu --- */

static void ToolbarOptionsClick(Window *w)
{
	uint16 x = 0;
	if (HasBit(_display_opt, DO_SHOW_TOWN_NAMES))    SetBit(x,  6);
	if (HasBit(_display_opt, DO_SHOW_STATION_NAMES)) SetBit(x,  7);
	if (HasBit(_display_opt, DO_SHOW_SIGNS))         SetBit(x,  8);
	if (HasBit(_display_opt, DO_WAYPOINTS))          SetBit(x,  9);
	if (HasBit(_display_opt, DO_FULL_ANIMATION))     SetBit(x, 10);
	if (HasBit(_display_opt, DO_FULL_DETAIL))        SetBit(x, 11);
	if (IsTransparencySet(TO_HOUSES))                SetBit(x, 12);
	if (IsTransparencySet(TO_SIGNS))                 SetBit(x, 13);

	PopupMainToolbMenu(w, 2, STR_02C4_GAME_OPTIONS, 14, 0, 0, x);
}

static void MenuClickSettings(int index)
{
	switch (index) {
		case  0: ShowGameOptions();                              return;
		case  1: ShowGameDifficulty();                           return;
		case  2: ShowPatchesSelection();                         return;
		case  3: ShowNewGRFSettings(!_networking, true, true, &_grfconfig);   return;
		case  4: ShowTransparencyToolbar();                      break;

		case  6: ToggleBit(_display_opt, DO_SHOW_TOWN_NAMES);    break;
		case  7: ToggleBit(_display_opt, DO_SHOW_STATION_NAMES); break;
		case  8: ToggleBit(_display_opt, DO_SHOW_SIGNS);         break;
		case  9: ToggleBit(_display_opt, DO_WAYPOINTS);          break;
		case 10: ToggleBit(_display_opt, DO_FULL_ANIMATION);     break;
		case 11: ToggleBit(_display_opt, DO_FULL_DETAIL);        break;
		case 12: ToggleTransparency(TO_HOUSES);                  break;
		case 13: ToggleTransparency(TO_SIGNS);                   break;
	}
	MarkWholeScreenDirty();
}

/* --- Saving/loading button menu --- */

static void ToolbarSaveClick(Window *w)
{
	PopupMainToolbMenu(w, 3, STR_015C_SAVE_GAME, 4);
}

static void ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, 3, STR_0292_SAVE_SCENARIO, 6);
}

static void MenuClickSaveLoad(int index)
{
	if (_game_mode == GM_EDITOR) {
		switch (index) {
			case 0: ShowSaveLoadDialog(SLD_SAVE_SCENARIO);  break;
			case 1: ShowSaveLoadDialog(SLD_LOAD_SCENARIO);  break;
			case 2: ShowSaveLoadDialog(SLD_LOAD_HEIGHTMAP); break;
			case 3: AskExitToGameMenu();                    break;
			case 5: HandleExitGameRequest();                break;
		}
	} else {
		switch (index) {
			case 0: ShowSaveLoadDialog(SLD_SAVE_GAME); break;
			case 1: ShowSaveLoadDialog(SLD_LOAD_GAME); break;
			case 2: AskExitToGameMenu();               break;
			case 3: HandleExitGameRequest();           break;
		}
	}
}

/* --- Map button menu --- */

static void ToolbarMapClick(Window *w)
{
	PopupMainToolbMenu(w, 4, STR_02DE_MAP_OF_WORLD, 3);
}

static void MenuClickMap(int index)
{
	switch (index) {
		case 0: ShowSmallMap();            break;
		case 1: ShowExtraViewPortWindow(); break;
		case 2: ShowSignList();            break;
	}
}

static void MenuClickScenMap(int index)
{
	switch (index) {
		case 0: ShowSmallMap();            break;
		case 1: ShowExtraViewPortWindow(); break;
		case 2: ShowSignList();            break;
		case 3: ShowTownDirectory();       break;
	}
}

/* --- Town button menu --- */

static void ToolbarTownClick(Window *w)
{
	PopupMainToolbMenu(w, 5, STR_02BB_TOWN_DIRECTORY, 1);
}

static void MenuClickTown(int index)
{
	ShowTownDirectory();
}

/* --- Subidies button menu --- */

static void ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbMenu(w, 6, STR_02DD_SUBSIDIES, 1);
}

static void MenuClickSubsidies(int index)
{
	ShowSubsidiesList();
}

/* --- Stations button menu --- */

static void ToolbarStationsClick(Window *w)
{
	PopupMainPlayerToolbMenu(w, 7, 0);
}

static void MenuClickStations(int index)
{
	ShowPlayerStations((PlayerID)index);
}

/* --- Finances button menu --- */

static void ToolbarFinancesClick(Window *w)
{
	PopupMainPlayerToolbMenu(w, 8, 0);
}

static void MenuClickFinances(int index)
{
	ShowPlayerFinances((PlayerID)index);
}

/* --- Company's button menu --- */

static void ToolbarPlayersClick(Window *w)
{
	PopupMainPlayerToolbMenu(w, 9, 0);
}

static void MenuClickCompany(int index)
{
	if (_networking && index == 0) {
		ShowClientList();
	} else {
		if (_networking) index--;
		ShowPlayerCompany((PlayerID)index);
	}
}

/* --- Graphs button menu --- */

static void ToolbarGraphsClick(Window *w)
{
	PopupMainToolbMenu(w, 10, STR_0154_OPERATING_PROFIT_GRAPH, (_toolbar_mode == TB_NORMAL) ? 6 : 8);
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
	PopupMainToolbMenu(w, 11, STR_015A_COMPANY_LEAGUE_TABLE, 2);
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
	PopupMainToolbMenu(w, 12, STR_INDUSTRY_DIR, 2, (_current_player == PLAYER_SPECTATOR) ? 2 : 0);
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
	PopupMainPlayerToolbMenu(w, 13 + veh, dis);
}


static void ToolbarTrainClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_TRAIN);
}

static void MenuClickShowTrains(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_TRAIN);
}

/* --- Road vehicle button menu --- */

static void ToolbarRoadClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_ROAD);
}

static void MenuClickShowRoad(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_ROAD);
}

/* --- Ship button menu --- */

static void ToolbarShipClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_SHIP);
}

static void MenuClickShowShips(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_SHIP);
}

/* --- Aircraft button menu --- */

static void ToolbarAirClick(Window *w)
{
	ToolbarVehicleClick(w, VEH_AIRCRAFT);
}

static void MenuClickShowAir(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_AIRCRAFT);
}

/* --- Zoom in button --- */

static void ToolbarZoomInClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(17);
		SndPlayFx(SND_15_BEEP);
	}
}

/* --- Zoom out button --- */

static void ToolbarZoomOutClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(18);
		SndPlayFx(SND_15_BEEP);
	}
}

/* --- Rail button menu --- */

static void ToolbarBuildRailClick(Window *w)
{
	const Player *p = GetPlayer(_local_player);
	PopupMainToolbMenu(w, 19, STR_1015_RAILROAD_CONSTRUCTION, RAILTYPE_END, ~p->avail_railtypes, _last_built_railtype);
}

static void MenuClickBuildRail(int index)
{
	_last_built_railtype = (RailType)index;
	ShowBuildRailToolbar(_last_built_railtype, -1);
}

/* --- Road button menu --- */

static void ToolbarBuildRoadClick(Window *w)
{
	const Player *p = GetPlayer(_local_player);
	/* The standard road button is *always* available */
	PopupMainToolbMenu(w, 20, STR_180A_ROAD_CONSTRUCTION, 2, ~(p->avail_roadtypes | ROADTYPES_ROAD), _last_built_roadtype);
}

static void MenuClickBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_roadtype);
}

/* --- Water button menu --- */

static void ToolbarBuildWaterClick(Window *w)
{
	PopupMainToolbMenu(w, 21, STR_9800_DOCK_CONSTRUCTION, 1);
}

static void MenuClickBuildWater(int index)
{
	ShowBuildDocksToolbar();
}

/* --- Airport button menu --- */

static void ToolbarBuildAirClick(Window *w)
{
	PopupMainToolbMenu(w, 22, STR_A01D_AIRPORT_CONSTRUCTION, 1);
}

static void MenuClickBuildAir(int index)
{
	ShowBuildAirToolbar();
}

/* --- Forest button menu --- */

static void ToolbarForestClick(Window *w)
{
	PopupMainToolbMenu(w, 23, STR_LANDSCAPING, 3);
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
	PopupMainToolbMenu(w, 24, STR_01D3_SOUND_MUSIC, 1);
}

static void MenuClickMusicWindow(int index)
{
	ShowMusicWindow();
}

/* --- Newspaper button menu --- */

static void ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbMenu(w, 25, STR_0200_LAST_MESSAGE_NEWS_REPORT, 3);
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
	PopupMainToolbMenu(w, 26, STR_02D5_LAND_BLOCK_INFO, 6);
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
		case 3: MenuClickSmallScreenshot(); break;
		case 4: MenuClickWorldScreenshot(); break;
		case 5: ShowAboutWindow();          break;
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
	w->HandleButtonClick(27);
	SetWindowDirty(w);
	SndPlayFx(SND_15_BEEP);
}

/* --- Scenario editor specific handlers. */

static void ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
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
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
		w->HandleButtonClick(TBSE_DATEFORWARD);
		w->SetDirty();

		_settings_newgame.game_creation.starting_year = Clamp(_settings_newgame.game_creation.starting_year + 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_settings_newgame.game_creation.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenMapTownDir(Window *w)
{
	/* Scenario editor button, *hack*hack* use different button to activate */
	PopupMainToolbMenu(w, 8 | (17 << 8), STR_02DE_MAP_OF_WORLD, 4);
}

static void ToolbarScenZoomIn(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(TBSE_ZOOMIN);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarScenZoomOut(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(TBSE_ZOOMOUT);
		SndPlayFx(SND_15_BEEP);
	}
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
	if (w->width >= (int)w->widget_count * 22) {
		button_width = 22;
		spacing = w->width - (w->widget_count * button_width);
	} else {
		button_width = w->width / w->widget_count;
		spacing = 0;
	}

	uint extra_spacing_at[] = { 4, 8, 13, 17, 19, 24, 0 };
	uint i = 0;
	for (uint x = 0, j = 0; i < w->widget_count; i++) {
		if (extra_spacing_at[j] == i) {
			j++;
			uint add = spacing / (lengthof(extra_spacing_at) - j);
			spacing -= add;
			x += add;
		}

		w->widget[i].type = WWT_IMGBTN;
		w->widget[i].left = x;
		x += (spacing != 0) ? button_width : (w->width - x) / (w->widget_count - i);
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

	static const uint icon_size = 22;
	uint max_icons = max(14U, (w->width + icon_size / 2) / icon_size);

	assert(max_icons >= 14 && max_icons <= 19);

	/* first hide all icons */
	for (uint i = 0; i < w->widget_count; i++) {
		w->widget[i].type = WWT_EMPTY;
	}

	/* now activate them all on their proper positions */
	for (uint i = 0, x = 0, n = max_icons - 14; i < max_icons; i++) {
		uint icon = arrangements[n][i + ((_toolbar_mode == TB_LOWER) ? max_icons : 0)];
		w->widget[icon].type = WWT_IMGBTN;
		w->widget[icon].left = x;
		x += (w->width - x) / (max_icons - i);
		w->widget[icon].right = x - 1;
	}
}

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
	ToolbarPlayersClick,
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
		this->SetWidgetDisabledState(0, _networking && !_network_server); // if not server, disable pause button
		this->SetWidgetDisabledState(1, _networking); // if networking, disable fast-forward button

		CLRBITS(this->flags4, WF_WHITE_BORDER_MASK);

		this->FindWindowPlacementAndResize(desc);
		PositionMainToolbar(this);
		DoZoomInOutWindow(ZOOM_NONE, this);
	}

	virtual void OnPaint()
	{
		/* Draw brown-red toolbar bg. */
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB2);
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB4 | (1 << PALETTE_MODIFIER_GREYOUT));

		/* If spectator, disable all construction buttons
		* ie : Build road, rail, ships, airports and landscaping
		* Since enabled state is the default, just disable when needed */
		this->SetWidgetsDisabledState(_current_player == PLAYER_SPECTATOR, 19, 20, 21, 22, 23, WIDGET_LIST_END);
		/* disable company list drop downs, if there are no companies */
		this->SetWidgetsDisabledState(ActivePlayerCount() == 0, 7, 8, 13, 14, 15, 16, WIDGET_LIST_END);

		this->SetWidgetDisabledState(19, !CanBuildVehicleInfrastructure(VEH_TRAIN));
		this->SetWidgetDisabledState(22, !CanBuildVehicleInfrastructure(VEH_AIRCRAFT));

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (_game_mode != GM_MENU && !this->IsWidgetDisabled(widget)) _toolbar_button_procs[widget](this);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_F1: case WKC_PAUSE: ToolbarPauseClick(this); break;
			case WKC_F2: ShowGameOptions(); break;
			case WKC_F3: MenuClickSaveLoad(0); break;
			case WKC_F4: ShowSmallMap(); break;
			case WKC_F5: ShowTownDirectory(); break;
			case WKC_F6: ShowSubsidiesList(); break;
			case WKC_F7: ShowPlayerStations(_local_player); break;
			case WKC_F8: ShowPlayerFinances(_local_player); break;
			case WKC_F9: ShowPlayerCompany(_local_player); break;
			case WKC_F10: ShowOperatingProfitGraph(); break;
			case WKC_F11: ShowCompanyLeagueTable(); break;
			case WKC_F12: ShowBuildIndustryWindow(); break;
			case WKC_SHIFT | WKC_F1: ShowVehicleListWindow(_local_player, VEH_TRAIN); break;
			case WKC_SHIFT | WKC_F2: ShowVehicleListWindow(_local_player, VEH_ROAD); break;
			case WKC_SHIFT | WKC_F3: ShowVehicleListWindow(_local_player, VEH_SHIP); break;
			case WKC_SHIFT | WKC_F4: ShowVehicleListWindow(_local_player, VEH_AIRCRAFT); break;
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
		if (this->IsWidgetLowered(0) != !!_pause_game) {
			this->ToggleWidgetLoweredState(0);
			this->InvalidateWidget(0);
		}

		if (this->IsWidgetLowered(1) != !!_fast_forward) {
			this->ToggleWidgetLoweredState(1);
			this->InvalidateWidget(1);
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (this->width <= 19 * 22) {
			SplitToolbar(this);
		} else {
			ResizeToolbar(this);
		}
	}

	virtual void OnTimeout()
	{
		for (uint i = 2; i < this->widget_count; i++) {
			if (this->IsWidgetLowered(i)) {
				this->RaiseWidget(i);
				this->InvalidateWidget(i);
			}
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (FindWindowById(WC_MAIN_WINDOW, 0) != NULL) HandleZoomMessage(this, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, 17, 18);
	}
};

static const Widget _toolb_normal_widgets[] = {
{     WWT_IMGBTN,   RESIZE_LEFT,    14,     0,     0,     0,    21, SPR_IMG_PAUSE,           STR_0171_PAUSE_GAME},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_FASTFORWARD,     STR_FAST_FORWARD},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_SETTINGS,        STR_0187_OPTIONS},
{   WWT_IMGBTN_2,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_SAVE,            STR_0172_SAVE_GAME_ABANDON_GAME},

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_SMALLMAP,        STR_0174_DISPLAY_MAP},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_TOWN,            STR_0176_DISPLAY_TOWN_DIRECTORY},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_SUBSIDIES,       STR_02DC_DISPLAY_SUBSIDIES},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_COMPANY_LIST,    STR_0173_DISPLAY_LIST_OF_COMPANY},

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_COMPANY_FINANCE, STR_0177_DISPLAY_COMPANY_FINANCES},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_COMPANY_GENERAL, STR_0178_DISPLAY_COMPANY_GENERAL},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_GRAPHS,          STR_0179_DISPLAY_GRAPHS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_COMPANY_LEAGUE,  STR_017A_DISPLAY_COMPANY_LEAGUE},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_INDUSTRY,        STR_0312_FUND_CONSTRUCTION_OF_NEW},

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_TRAINLIST,       STR_017B_DISPLAY_LIST_OF_COMPANY},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_TRUCKLIST,       STR_017C_DISPLAY_LIST_OF_COMPANY},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_SHIPLIST,        STR_017D_DISPLAY_LIST_OF_COMPANY},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_AIRPLANESLIST,   STR_017E_DISPLAY_LIST_OF_COMPANY},

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_ZOOMIN,          STR_017F_ZOOM_THE_VIEW_IN},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_ZOOMOUT,         STR_0180_ZOOM_THE_VIEW_OUT},

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_BUILDRAIL,       STR_0181_BUILD_RAILROAD_TRACK},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_BUILDROAD,       STR_0182_BUILD_ROADS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_BUILDWATER,      STR_0183_BUILD_SHIP_DOCKS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_BUILDAIR,        STR_0184_BUILD_AIRPORTS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_LANDSCAPING,     STR_LANDSCAPING_TOOLBAR_TIP}, // tree icon is 0x2E6

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_MUSIC,           STR_01D4_SHOW_SOUND_MUSIC_WINDOW},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_MESSAGES,        STR_0203_SHOW_LAST_MESSAGE_NEWS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_IMG_QUERY,           STR_0186_LAND_BLOCK_INFORMATION},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,     0,     0,    21, SPR_SWITCH_TOOLBAR,      STR_EMPTY}, // switch toolbar button. only active when toolbar has been split
{   WIDGETS_END},
};

static const WindowDesc _toolb_normal_desc = {
	0, 0, 0, 22, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_toolb_normal_widgets,
};


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
	ToolbarScenZoomIn,
	ToolbarScenZoomOut,
	ToolbarScenGenLand,
	ToolbarScenGenTown,
	ToolbarScenGenIndustry,
	ToolbarScenBuildRoad,
	ToolbarScenPlantTrees,
	ToolbarScenPlaceSign,
	NULL,
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
		GfxFillRect(0, 0, this->width - 1, this->height - 1, 0xB4 | (1 << PALETTE_MODIFIER_GREYOUT));

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

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case WKC_F1: case WKC_PAUSE: ToolbarPauseClick(this); break;
			case WKC_F2: ShowGameOptions(); break;
			case WKC_F3: MenuClickSaveLoad(0); break;
			case WKC_F4: ToolbarScenGenLand(this); break;
			case WKC_F5: ToolbarScenGenTown(this); break;
			case WKC_F6: ToolbarScenGenIndustry(this); break;
			case WKC_F7: ToolbarScenBuildRoad(this); break;
			case WKC_F8: ToolbarScenPlantTrees(this); break;
			case WKC_F9: ToolbarScenPlaceSign(this); break;
			case WKC_F10: ShowMusicWindow(); break;
			case WKC_F11: PlaceLandBlockInfo(); break;
			case WKC_CTRL | 'S': MenuClickSmallScreenshot(); break;
			case WKC_CTRL | 'G': MenuClickWorldScreenshot(); break;

			/* those following are all fall through */
			case WKC_NUM_PLUS:
			case WKC_EQUALS:
			case WKC_SHIFT | WKC_EQUALS:
			case WKC_SHIFT | WKC_F5: ToolbarScenZoomIn(this); break;

			/* those following are all fall through */
			case WKC_NUM_MINUS:
			case WKC_MINUS:
			case WKC_SHIFT | WKC_MINUS:
			case WKC_SHIFT | WKC_F6: ToolbarScenZoomOut(this); break;

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
		/* There are 15 buttons plus some spacings if the space allows it.
		 * Furthermore there are two panels of which one is non - essential
		 * and that one can be removed is the space is too small. */
		uint buttons_width;
		uint spacing;

		static int normal_min_width = (15 * 22) + (2 * 130);
		static int one_less_panel_min_width = (15 * 22) + 130;

		if (this->width >= one_less_panel_min_width) {
			buttons_width = 15 * 22;
			spacing = this->width - ((this->width >= normal_min_width) ? normal_min_width : one_less_panel_min_width);
		} else {
			buttons_width = this->width - 130;
			spacing = 0;
		}
		uint extra_spacing_at[] = { 3, 4, 7, 8, 10, 16, 0 };

		for (uint i = 0, x = 0, j = 0, b = 0; i < this->widget_count; i++) {
			switch (i) {
				case 4:
					this->widget[i].left = x;
					if (this->width < normal_min_width) {
						this->widget[i].right = x;
						j++;
						continue;
					}

					x += 130;
					this->widget[i].right = x - 1;
					break;

				case 5: {
					int offset = x - this->widget[i].left;
					this->widget[i + 1].left  += offset;
					this->widget[i + 1].right += offset;
					this->widget[i + 2].left  += offset;
					this->widget[i + 2].right += offset;
					this->widget[i].left = x;
					x += 130;
					this->widget[i].right = x - 1;
					i += 2;
				} break;

				default:
					if (this->widget[i].bottom == 0) continue;

					this->widget[i].left = x;
					x += buttons_width / (15 - b);
					this->widget[i].right = x - 1;
					buttons_width -= buttons_width / (15 - b);
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
{  WWT_IMGBTN, RESIZE_LEFT, 14,   0,   0,  0, 21, SPR_IMG_PAUSE,       STR_0171_PAUSE_GAME},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_FASTFORWARD, STR_FAST_FORWARD},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_SETTINGS,    STR_0187_OPTIONS},
{WWT_IMGBTN_2, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_SAVE,        STR_0297_SAVE_SCENARIO_LOAD_SCENARIO},

{   WWT_PANEL, RESIZE_NONE, 14,   0,   0,  0, 21, 0x0,                 STR_NULL},

{   WWT_PANEL, RESIZE_NONE, 14,   0, 129,  0, 21, 0x0,                 STR_NULL},
{  WWT_IMGBTN, RESIZE_NONE, 14,   3,  14,  5, 16, SPR_ARROW_DOWN,      STR_029E_MOVE_THE_STARTING_DATE},
{  WWT_IMGBTN, RESIZE_NONE, 14, 113, 125,  5, 16, SPR_ARROW_UP,        STR_029F_MOVE_THE_STARTING_DATE},

{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_SMALLMAP,    STR_0175_DISPLAY_MAP_TOWN_DIRECTORY},

{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_ZOOMIN,      STR_017F_ZOOM_THE_VIEW_IN},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_ZOOMOUT,     STR_0180_ZOOM_THE_VIEW_OUT},

{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_LANDSCAPING, STR_022E_LANDSCAPE_GENERATION},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_TOWN,        STR_022F_TOWN_GENERATION},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_INDUSTRY,    STR_0230_INDUSTRY_GENERATION},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_BUILDROAD,   STR_0231_ROAD_CONSTRUCTION},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_PLANTTREES,  STR_0288_PLANT_TREES},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_SIGN,        STR_0289_PLACE_SIGN},

{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_MUSIC,       STR_01D4_SHOW_SOUND_MUSIC_WINDOW},
{   WWT_EMPTY, RESIZE_NONE,  0,   0,   0,  0,  0, 0x0,                 STR_NULL},
{  WWT_IMGBTN, RESIZE_NONE, 14,   0,   0,  0, 21, SPR_IMG_QUERY,       STR_0186_LAND_BLOCK_INFORMATION},
{WIDGETS_END},
};

static const WindowDesc _toolb_scen_desc = {
	0, 0, 130, 22, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_toolb_scen_widgets,
};

/* --- Rendering/handling the drop down menus --- */

typedef void MenuClickedProc(int index);

static MenuClickedProc * const _menu_clicked_procs[] = {
	NULL,                 /* 0 */
	NULL,                 /* 1 */
	MenuClickSettings,    /* 2 */
	MenuClickSaveLoad,    /* 3 */
	MenuClickMap,         /* 4 */
	MenuClickTown,        /* 5 */
	MenuClickSubsidies,   /* 6 */
	MenuClickStations,    /* 7 */
	MenuClickFinances,    /* 8 */
	MenuClickCompany,     /* 9 */
	MenuClickGraphs,      /* 10 */
	MenuClickLeague,      /* 11 */
	MenuClickIndustry,    /* 12 */
	MenuClickShowTrains,  /* 13 */
	MenuClickShowRoad,    /* 14 */
	MenuClickShowShips,   /* 15 */
	MenuClickShowAir,     /* 16 */
	MenuClickScenMap,     /* 17 */
	NULL,                 /* 18 */
	MenuClickBuildRail,   /* 19 */
	MenuClickBuildRoad,   /* 20 */
	MenuClickBuildWater,  /* 21 */
	MenuClickBuildAir,    /* 22 */
	MenuClickForest,      /* 23 */
	MenuClickMusicWindow, /* 24 */
	MenuClickNewspaper,   /* 25 */
	MenuClickHelp,        /* 26 */
};

struct ToolbarMenuWindow : Window {
	int item_count;
	int sel_index;
	int main_button;
	int action_id;
	int checked_items;
	int disabled_items;
	StringID base_string;

	ToolbarMenuWindow(int x, int y, int width, int height, const Widget *widgets, int item_count,
										int sel_index, int parent_button, StringID base_string, int checked_items,
										int disabled_items) :
			Window(x, y, width, height, WC_TOOLBAR_MENU, widgets),
			item_count(item_count), sel_index(sel_index), main_button(GB(parent_button, 0, 8)),
			action_id((GB(parent_button, 8, 8) != 0) ? GB(parent_button, 8, 8) : parent_button),
			checked_items(checked_items), disabled_items(disabled_items), base_string(base_string)
	{
		this->widget[0].bottom = item_count * 10 + 1;
		this->widget[0].right = this->width - 1;
		this->flags4 &= ~WF_WHITE_BORDER_MASK;

		this->FindWindowPlacementAndResize(width, height);
	}

	~ToolbarMenuWindow()
	{
		Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
		w->RaiseWidget(this->main_button);
		w->SetDirty();
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		for (int i = 0, x = 1, y = 1; i != this->item_count; i++, y += 10) {
			TextColour color = HasBit(this->disabled_items, i) ? TC_GREY : (this->sel_index == i) ? TC_WHITE : TC_BLACK;
			if (this->sel_index == i) GfxFillRect(x, y, x + this->width - 3, y + 9, 0);

			if (HasBit(this->checked_items, i)) DrawString(x + 2, y, STR_CHECKMARK, color);
			DrawString(x + 2, y, this->base_string + i, color);
		}
	}

	virtual void OnMouseLoop()
	{
		int index = GetMenuItemIndex(this, this->item_count, this->disabled_items);

		if (_left_button_down) {
			if (index == -1 || index == this->sel_index) return;

			this->sel_index = index;
			this->SetDirty();
		} else {
			if (index < 0) {
				Window *w = FindWindowById(WC_MAIN_TOOLBAR,0);
				if (GetWidgetFromPos(w, _cursor.pos.x - w->left, _cursor.pos.y - w->top) == this->main_button) {
					index = this->sel_index;
				}
			}

			int action_id = this->action_id;
			delete this;

			if (index >= 0) {
				assert((uint)index <= lengthof(_menu_clicked_procs));
				_menu_clicked_procs[action_id](index);
			}
		}
	}
};

/* Dynamic widget length determined by toolbar-string length.
 * See PopupMainToolbMenu en MenuWndProc */
static const Widget _menu_widgets[] = {
{    WWT_PANEL, RESIZE_NONE, 14, 0,  0, 0, 0, 0x0, STR_NULL},
{ WIDGETS_END},
};


/**
 * Get the maximum length of a given string in a string-list. This is an
 * implicit string-list where the ID's are consecutive
 * @param base_string StringID of the first string in the list
 * @param count amount of StringID's in the list
 * @return the length of the longest string
 */
static int GetStringListMaxWidth(StringID base_string, byte count)
{
	char buffer[512];
	int width, max_width = 0;

	for (byte i = 0; i != count; i++) {
		GetString(buffer, base_string + i, lastof(buffer));
		width = GetStringBoundingBox(buffer).width;
		if (width > max_width) max_width = width;
	}

	return max_width;
}

/**
 * Show a general dropdown menu. The positioning of the dropdown menu
 * defaults to the left side of the parent_button, eg the button that caused
 * this window to appear. The only exceptions are when the right side of this
 * dropdown would fall outside the main toolbar window, in that case it is
 * aligned with the toolbar's right side.
 * Since the disable-mask is only 8 bits right now, these dropdowns are
 * restricted to 8 items max if any bits of disabled_mask are active.
 * @param parent Pointer to a window this dropdown menu belongs to. Has no effect
 * whatsoever, only graphically for positioning.
 * @param parent_button The widget identifier of the button that was clicked for
 * this dropdown. The created dropdown then knows what button to raise (button) on
 * action and whose function to execute (action).
 * It is possible to appoint another button for an action event by setting the
 * upper 8 bits of this parameter. If non is set, action is presumed to be the same
 * as button. So<br>
 * button bits 0 -  7 - widget clicked to get dropdown
 * action bits 8 - 15 - function of widget to execute on select (defaults to bits 0 - 7)
 * @param base_string The first StringID shown in the dropdown list. All others are
 * consecutive indeces from the language file. XXX - fix? Use ingame-string tables?
 * @param item_count Number of strings in the list, see previous parameter
 * @param disabled_mask Bitmask of disabled strings in the list
 * @param sel_index The selected toolbar item
 * @param check_items The items to have a checked mark in front of them.
 * @return Return a pointer to the newly created dropdown window
 */
static void PopupMainToolbMenu(Window *parent, uint16 parent_button, StringID base_string, byte item_count, byte disabled_mask, int sel_index, int checked_items)
{
	assert(disabled_mask == 0 || item_count <= 8);
	parent->LowerWidget(parent_button);
	parent->InvalidateWidget(parent_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);

	/* Extend the dropdown toolbar to the longest string in the list */
	int width = max(GetStringListMaxWidth(base_string, item_count) + 6, 140);
	int height = item_count * 10 + 2;

	Point pos = GetToolbarDropdownPos(parent_button, width, height);

	new ToolbarMenuWindow(pos.x, pos.y, width, height, _menu_widgets, item_count, sel_index, parent_button, base_string, checked_items, disabled_mask);

	SndPlayFx(SND_15_BEEP);
}

/* --- Rendering/drawing the player menu --- */
static int GetPlayerIndexFromMenu(int index)
{
	if (index >= 0) {
		const Player *p;

		FOR_ALL_PLAYERS(p) {
			if (p->is_active && --index < 0) return p->index;
		}
	}
	return -1;
}

struct ToolbarPlayerMenuWindow : Window {
	int item_count;
	int sel_index;
	int main_button;
	int action_id;
	int gray_items;

	ToolbarPlayerMenuWindow(int x, int y, int width, int height, const Widget *widgets, int main_button, int gray) :
			Window(x, y, width, height, WC_TOOLBAR_MENU, widgets),
			item_count(0), main_button(main_button), action_id(main_button), gray_items(gray)
	{
		this->flags4 &= ~WF_WHITE_BORDER_MASK;
		this->sel_index = (_local_player != PLAYER_SPECTATOR) ? _local_player : GetPlayerIndexFromMenu(0);
		if (_networking && main_button == 9) {
			if (_local_player != PLAYER_SPECTATOR) {
				this->sel_index++;
			} else {
				/* Select client list by default for spectators */
				this->sel_index = 0;
			}
		}

		this->FindWindowPlacementAndResize(width, height);
	}

	~ToolbarPlayerMenuWindow()
	{
		Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
		w->RaiseWidget(this->main_button);
		w->SetDirty();
	}

	void UpdatePlayerMenuHeight()
	{
		byte num = ActivePlayerCount();

		/* Increase one to fit in PlayerList in the menu when in network */
		if (_networking && this->main_button == 9) num++;

		if (this->item_count != num) {
			this->item_count = num;
			this->SetDirty();
			num = num * 10 + 2;
			this->height = num;
			this->widget[0].bottom = this->widget[0].top + num - 1;
			this->top = GetToolbarDropdownPos(0, this->width, this->height).y;
			this->SetDirty();
		}
	}

	virtual void OnPaint()
	{
		this->UpdatePlayerMenuHeight();
		this->DrawWidgets();

		int x = 1;
		int y = 1;
		int sel = this->sel_index;
		int gray = this->gray_items;

		/* 9 = playerlist */
		if (_networking && this->main_button == 9) {
			if (sel == 0) {
				GfxFillRect(x, y, x + 238, y + 9, 0);
			}
			DrawString(x + 19, y, STR_NETWORK_CLIENT_LIST, TC_FROMSTRING);
			y += 10;
			sel--;
		}

		const Player *p;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				if (p->index == sel) {
					GfxFillRect(x, y, x + 238, y + 9, 0);
				}

				DrawPlayerIcon(p->index, x + 2, y + 1);

				SetDParam(0, p->index);
				SetDParam(1, p->index);

				TextColour color = (p->index == sel) ? TC_WHITE : TC_BLACK;
				if (gray & 1) color = TC_GREY;
				DrawString(x + 19, y, STR_7021, color);

				y += 10;
			}
			gray >>= 1;
		}
	}

	virtual void OnMouseLoop()
	{
		int index = GetMenuItemIndex(this, this->item_count, 0);

		if (_left_button_down) {
			this->UpdatePlayerMenuHeight();
			/* We have a new entry at the top of the list of menu 9 when networking
				* so keep that in count */
			if (_networking && this->main_button == 9) {
				if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
			} else {
				index = GetPlayerIndexFromMenu(index);
			}

			if (index == -1 || index == this->sel_index) return;

			this->sel_index = index;
			this->SetDirty();
		} else {
			int action_id = this->action_id;

			/* We have a new entry at the top of the list of menu 9 when networking
				* so keep that in count */
			if (_networking && this->main_button == 9) {
				if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
			} else {
				index = GetPlayerIndexFromMenu(index);
			}

			if (index < 0) {
				Window *w = FindWindowById(WC_MAIN_TOOLBAR,0);
				if (GetWidgetFromPos(w, _cursor.pos.x - w->left, _cursor.pos.y - w->top) == this->main_button) {
					index = this->sel_index;
				}
			}

			delete this;

			if (index >= 0) {
				assert(index >= 0 && index < 30);
				_menu_clicked_procs[action_id](index);
			}
		}
	}
};

static const Widget _player_menu_widgets[] = {
{    WWT_PANEL, RESIZE_NONE, 14, 0, 240, 0, 81, 0x0, STR_NULL},
{ WIDGETS_END},
};

static void PopupMainPlayerToolbMenu(Window *parent, int main_button, int gray)
{
	parent->LowerWidget(main_button);
	parent->InvalidateWidget(main_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);
	Point pos = GetToolbarDropdownPos(main_button, 241, 82);
	new ToolbarPlayerMenuWindow(pos.x, pos.y, 241, 82, _player_menu_widgets, main_button, gray);

	SndPlayFx(SND_15_BEEP);
}

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
