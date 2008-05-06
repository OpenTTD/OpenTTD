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
#include "console.h"
#include "news_gui.h"

#include "network/network.h"
#include "network/network_gui.h"

#include "table/strings.h"
#include "table/sprites.h"

static Window *PopupMainToolbMenu(Window *w, uint16 parent_button, StringID base_string, byte item_count, byte disabled_mask);
static Window *PopupMainPlayerToolbMenu(Window *w, int main_button, int gray);

RailType _last_built_railtype;
RoadType _last_built_roadtype;

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

	w = PopupMainToolbMenu(w, 2, STR_02C4_GAME_OPTIONS, 14, 0);

	if (HasBit(_display_opt, DO_SHOW_TOWN_NAMES))    SetBit(x,  6);
	if (HasBit(_display_opt, DO_SHOW_STATION_NAMES)) SetBit(x,  7);
	if (HasBit(_display_opt, DO_SHOW_SIGNS))         SetBit(x,  8);
	if (HasBit(_display_opt, DO_WAYPOINTS))          SetBit(x,  9);
	if (HasBit(_display_opt, DO_FULL_ANIMATION))     SetBit(x, 10);
	if (HasBit(_display_opt, DO_FULL_DETAIL))        SetBit(x, 11);
	if (IsTransparencySet(TO_HOUSES))                SetBit(x, 12);
	if (IsTransparencySet(TO_SIGNS))                 SetBit(x, 13);
	WP(w, menu_d).checked_items = x;
}

static void MenuClickSettings(int index)
{
	switch (index) {
		case 0: ShowGameOptions();      return;
		case 1: ShowGameDifficulty();   return;
		case 2: ShowPatchesSelection(); return;
		case 3: ShowNewGRFSettings(!_networking, true, true, &_grfconfig);   return;
		case 4: ShowTransparencyToolbar(); break;

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
	PopupMainToolbMenu(w, 3, STR_015C_SAVE_GAME, 4, 0);
}

static void ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, 3, STR_0292_SAVE_SCENARIO, 6, 0);
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
	PopupMainToolbMenu(w, 4, STR_02DE_MAP_OF_WORLD, 3, 0);
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
	PopupMainToolbMenu(w, 5, STR_02BB_TOWN_DIRECTORY, 1, 0);
}

static void MenuClickTown(int index)
{
	ShowTownDirectory();
}

/* --- Subidies button menu --- */

static void ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbMenu(w, 6, STR_02DD_SUBSIDIES, 1, 0);
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
	PopupMainToolbMenu(w, 10, STR_0154_OPERATING_PROFIT_GRAPH, 6, 0);
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
	}
}

/* --- League button menu --- */

static void ToolbarLeagueClick(Window *w)
{
	PopupMainToolbMenu(w, 11, STR_015A_COMPANY_LEAGUE_TABLE, 2, 0);
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
	PopupMainToolbMenu(w, 12, STR_INDUSTRY_DIR, 2, (_current_player == PLAYER_SPECTATOR) ? (1 << 1) : 0);
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
	Window *w2 = PopupMainToolbMenu(w, 19, STR_1015_RAILROAD_CONSTRUCTION, RAILTYPE_END, ~p->avail_railtypes);
	WP(w2, menu_d).sel_index = _last_built_railtype;
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
	Window *w2 = PopupMainToolbMenu(w, 20, STR_180A_ROAD_CONSTRUCTION, 2, ~(p->avail_roadtypes | ROADTYPES_ROAD));
	WP(w2, menu_d).sel_index = _last_built_roadtype;
}

static void MenuClickBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_roadtype);
}

/* --- Water button menu --- */

static void ToolbarBuildWaterClick(Window *w)
{
	PopupMainToolbMenu(w, 21, STR_9800_DOCK_CONSTRUCTION, 1, 0);
}

static void MenuClickBuildWater(int index)
{
	ShowBuildDocksToolbar();
}

/* --- Airport button menu --- */

static void ToolbarBuildAirClick(Window *w)
{
	PopupMainToolbMenu(w, 22, STR_A01D_AIRPORT_CONSTRUCTION, 1, 0);
}

static void MenuClickBuildAir(int index)
{
	ShowBuildAirToolbar();
}

/* --- Forest button menu --- */

static void ToolbarForestClick(Window *w)
{
	PopupMainToolbMenu(w, 23, STR_LANDSCAPING, 3, 0);
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
	PopupMainToolbMenu(w, 24, STR_01D3_SOUND_MUSIC, 1, 0);
}

static void MenuClickMusicWindow(int index)
{
	ShowMusicWindow();
}

/* --- Newspaper button menu --- */

static void ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbMenu(w, 25, STR_0200_LAST_MESSAGE_NEWS_REPORT, 3, 0);
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
	PopupMainToolbMenu(w, 26, STR_02D5_LAND_BLOCK_INFO, 6, 0);
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

/* --- Scenario editor specific handlers. */

static void ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
		w->HandleButtonClick(6);
		w->SetDirty();

		_patches_newgame.starting_year = Clamp(_patches_newgame.starting_year - 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenDateForward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
		w->HandleButtonClick(7);
		w->SetDirty();

		_patches_newgame.starting_year = Clamp(_patches_newgame.starting_year + 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenMapTownDir(Window *w)
{
	/* Scenario editor button, *hack*hack* use different button to activate */
	PopupMainToolbMenu(w, 8 | (17 << 8), STR_02DE_MAP_OF_WORLD, 4, 0);
}

static void ToolbarScenZoomIn(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(9);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarScenZoomOut(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(10);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarScenGenLand(Window *w)
{
	w->HandleButtonClick(11);
	SndPlayFx(SND_15_BEEP);

	ShowEditorTerraformToolbar();
}


static void ToolbarScenGenTown(Window *w)
{
	w->HandleButtonClick(12);
	SndPlayFx(SND_15_BEEP);
	ShowBuildTownWindow();
}

static void ToolbarScenGenIndustry(Window *w)
{
	w->HandleButtonClick(13);
	SndPlayFx(SND_15_BEEP);
	ShowBuildIndustryWindow();
}

static void ToolbarScenBuildRoad(Window *w)
{
	w->HandleButtonClick(14);
	SndPlayFx(SND_15_BEEP);
	ShowBuildRoadScenToolbar();
}

static void ToolbarScenPlantTrees(Window *w)
{
	w->HandleButtonClick(15);
	SndPlayFx(SND_15_BEEP);
	ShowBuildTreesScenToolbar();
}

static void ToolbarScenPlaceSign(Window *w)
{
	w->HandleButtonClick(16);
	SndPlayFx(SND_15_BEEP);
	SelectSignTool();
}

static void ToolbarBtn_NULL(Window *w)
{
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
};

static void MainToolbarWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			/* Draw brown-red toolbar bg. */
			GfxFillRect(0, 0, w->width - 1, w->height - 1, 0xB2);
			GfxFillRect(0, 0, w->width - 1, w->height - 1, 0xB4 | (1 << PALETTE_MODIFIER_GREYOUT));

			/* If spectator, disable all construction buttons
			* ie : Build road, rail, ships, airports and landscaping
			* Since enabled state is the default, just disable when needed */
			w->SetWidgetsDisabledState(_current_player == PLAYER_SPECTATOR, 19, 20, 21, 22, 23, WIDGET_LIST_END);
			/* disable company list drop downs, if there are no companies */
			w->SetWidgetsDisabledState(ActivePlayerCount() == 0, 7, 8, 13, 14, 15, 16, WIDGET_LIST_END);

			w->SetWidgetDisabledState(19, !CanBuildVehicleInfrastructure(VEH_TRAIN));
			w->SetWidgetDisabledState(22, !CanBuildVehicleInfrastructure(VEH_AIRCRAFT));

			DrawWindowWidgets(w);
			break;

		case WE_CLICK:
			if (_game_mode != GM_MENU && !w->IsWidgetDisabled(e->we.click.widget)) _toolbar_button_procs[e->we.click.widget](w);
			break;

		case WE_KEYPRESS:
			switch (e->we.keypress.keycode) {
				case WKC_F1: case WKC_PAUSE: ToolbarPauseClick(w); break;
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
				case WKC_SHIFT | WKC_F5: ToolbarZoomInClick(w); break;
				case WKC_NUM_MINUS: // Fall through
				case WKC_MINUS: // Fall through
				case WKC_SHIFT | WKC_MINUS: // Fall through
				case WKC_SHIFT | WKC_F6: ToolbarZoomOutClick(w); break;
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
				default: return;
			}
			e->we.keypress.cont = false;
			break;

		case WE_PLACE_OBJ:
			_place_proc(e->we.place.tile);
			break;

		case WE_ABORT_PLACE_OBJ:
			w->RaiseWidget(25);
			w->SetDirty();
			break;

		case WE_MOUSELOOP:
			if (w->IsWidgetLowered(0) != !!_pause_game) {
				w->ToggleWidgetLoweredState(0);
				w->InvalidateWidget(0);
			}

			if (w->IsWidgetLowered(1) != !!_fast_forward) {
				w->ToggleWidgetLoweredState(1);
				w->InvalidateWidget(1);
			}
			break;

		case WE_RESIZE: {
			/* There are 27 buttons plus some spacings if the space allows it */
			uint button_width;
			uint spacing;
			if (w->width >= 27 * 22) {
				button_width = 22;
				spacing = w->width - (27 * button_width);
			} else {
				button_width = w->width / 27;
				spacing = 0;
			}
			uint extra_spacing_at[] = { 4, 8, 13, 17, 19, 24, 0 };

			for (uint i = 0, x = 0, j = 0; i < 27; i++) {
				if (extra_spacing_at[j] == i) {
					j++;
					uint add = spacing / (lengthof(extra_spacing_at) - j);
					spacing -= add;
					x += add;
				}

				w->widget[i].left = x;
				x += (spacing != 0) ? button_width : (w->width - x) / (27 - i);
				w->widget[i].right = x - 1;
			}
		} break;

		case WE_TIMEOUT:
			for (uint i = 2; i < w->widget_count; i++) {
				if (w->IsWidgetLowered(i)) {
					w->RaiseWidget(i);
					w->InvalidateWidget(i);
				}
			}
			break;

		case WE_MESSAGE:
			if (FindWindowById(WC_MAIN_WINDOW, 0) != NULL) HandleZoomMessage(w, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, 17, 18);
			break;
	}
}

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
{   WIDGETS_END},
};

static const WindowDesc _toolb_normal_desc = {
	0, 0, 0, 22, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_toolb_normal_widgets,
	MainToolbarWndProc
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

static void ScenEditToolbarWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			w->SetWidgetDisabledState(6, _patches_newgame.starting_year <= MIN_YEAR);
			w->SetWidgetDisabledState(7, _patches_newgame.starting_year >= MAX_YEAR);

			/* Draw brown-red toolbar bg. */
			GfxFillRect(0, 0, w->width - 1, w->height - 1, 0xB2);
			GfxFillRect(0, 0, w->width - 1, w->height - 1, 0xB4 | (1 << PALETTE_MODIFIER_GREYOUT));

			DrawWindowWidgets(w);

			SetDParam(0, ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
			DrawStringCenteredTruncated(w->widget[6].right, w->widget[7].left, 6, STR_00AF, TC_FROMSTRING);

			/* We hide this panel when the toolbar space gets too small */
			if (w->widget[4].left != w->widget[4].right) {
				DrawStringCenteredTruncated(w->widget[4].left + 1, w->widget[4].right - 1,  1, STR_0221_OPENTTD, TC_FROMSTRING);
				DrawStringCenteredTruncated(w->widget[4].left + 1, w->widget[4].right - 1, 11, STR_0222_SCENARIO_EDITOR, TC_FROMSTRING);
			}

			break;

		case WE_CLICK:
			if (_game_mode == GM_MENU) return;
			_scen_toolbar_button_procs[e->we.click.widget](w);
			break;

		case WE_KEYPRESS:
			switch (e->we.keypress.keycode) {
				case WKC_F1: case WKC_PAUSE: ToolbarPauseClick(w); break;
				case WKC_F2: ShowGameOptions(); break;
				case WKC_F3: MenuClickSaveLoad(0); break;
				case WKC_F4: ToolbarScenGenLand(w); break;
				case WKC_F5: ToolbarScenGenTown(w); break;
				case WKC_F6: ToolbarScenGenIndustry(w); break;
				case WKC_F7: ToolbarScenBuildRoad(w); break;
				case WKC_F8: ToolbarScenPlantTrees(w); break;
				case WKC_F9: ToolbarScenPlaceSign(w); break;
				case WKC_F10: ShowMusicWindow(); break;
				case WKC_F11: PlaceLandBlockInfo(); break;
				case WKC_CTRL | 'S': MenuClickSmallScreenshot(); break;
				case WKC_CTRL | 'G': MenuClickWorldScreenshot(); break;

				/* those following are all fall through */
				case WKC_NUM_PLUS:
				case WKC_EQUALS:
				case WKC_SHIFT | WKC_EQUALS:
				case WKC_SHIFT | WKC_F5: ToolbarZoomInClick(w); break;

				/* those following are all fall through */
				case WKC_NUM_MINUS:
				case WKC_MINUS:
				case WKC_SHIFT | WKC_MINUS:
				case WKC_SHIFT | WKC_F6: ToolbarZoomOutClick(w); break;

				case 'L': ShowEditorTerraformToolbar(); break;
				case 'M': ShowSmallMap(); break;
				case 'V': ShowExtraViewPortWindow(); break;
				default: return;
			}
			e->we.keypress.cont = false;
			break;

		case WE_PLACE_OBJ:
			_place_proc(e->we.place.tile);
			break;

		case WE_ABORT_PLACE_OBJ:
			w->RaiseWidget(25);
			w->SetDirty();
			break;

		case WE_RESIZE: {
			/* There are 15 buttons plus some spacings if the space allows it.
			* Furthermore there are two panels of which one is non - essential
			* and that one can be removed is the space is too small. */
			uint buttons_width;
			uint spacing;

			static int normal_min_width = (15 * 22) + (2 * 130);
			static int one_less_panel_min_width = (15 * 22) + 130;

			if (w->width >= one_less_panel_min_width) {
				buttons_width = 15 * 22;
				spacing = w->width - ((w->width >= normal_min_width) ? normal_min_width : one_less_panel_min_width);
			} else {
				buttons_width = w->width - 130;
				spacing = 0;
			}
			uint extra_spacing_at[] = { 3, 4, 7, 8, 10, 16, 0 };

			/* Yes, it defines about 27 widgets for this toolbar */
			for (uint i = 0, x = 0, j = 0, b = 0; i < 27; i++) {
				switch (i) {
					case 4:
						w->widget[i].left = x;
						if (w->width < normal_min_width) {
							w->widget[i].right = x;
							j++;
							continue;
						}

						x += 130;
						w->widget[i].right = x - 1;
						break;

					case 5: {
						int offset = x - w->widget[i].left;
						w->widget[i + 1].left  += offset;
						w->widget[i + 1].right += offset;
						w->widget[i + 2].left  += offset;
						w->widget[i + 2].right += offset;
						w->widget[i].left = x;
						x += 130;
						w->widget[i].right = x - 1;
						i += 2;
					} break;

					default:
						if (w->widget[i].bottom == 0) continue;

						w->widget[i].left = x;
						x += buttons_width / (15 - b);
						w->widget[i].right = x - 1;
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
		} break;

		case WE_MOUSELOOP:
			if (w->IsWidgetLowered(0) != !!_pause_game) {
				w->ToggleWidgetLoweredState(0);
				w->SetDirty();
			}

			if (w->IsWidgetLowered(1) != !!_fast_forward) {
				w->ToggleWidgetLoweredState(1);
				w->SetDirty();
			}
			break;

		case WE_MESSAGE:
			HandleZoomMessage(w, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, 9, 10);
			break;
	}
}

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
	ScenEditToolbarWndProc
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

static void MenuWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			w->widget[0].right = w->width - 1;
			break;

		case WE_PAINT: {
			byte count = WP(w, menu_d).item_count;
			byte sel = WP(w, menu_d).sel_index;
			uint16 chk = WP(w, menu_d).checked_items;
			StringID string = WP(w, menu_d).string_id;
			byte dis = WP(w, menu_d).disabled_items;

			DrawWindowWidgets(w);

			int x = 1;
			int y = 1;

			for (; count != 0; count--, string++, sel--) {
				TextColour color = HasBit(dis, 0) ? TC_GREY : (sel == 0) ? TC_WHITE : TC_BLACK;
				if (sel == 0) GfxFillRect(x, y, x + w->width - 3, y + 9, 0);

				if (HasBit(chk, 0)) DrawString(x + 2, y, STR_CHECKMARK, color);
				DrawString(x + 2, y, string, color);

				y += 10;
				chk >>= 1;
				dis >>= 1;
			}
		} break;

		case WE_DESTROY: {
				Window *v = FindWindowById(WC_MAIN_TOOLBAR, 0);
				v->RaiseWidget(WP(w, menu_d).main_button);
				w->SetDirty();
				return;
			}

		case WE_POPUPMENU_SELECT: {
			int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);

			if (index < 0) {
				Window *w2 = FindWindowById(WC_MAIN_TOOLBAR,0);
				if (GetWidgetFromPos(w2, e->we.popupmenu.pt.x - w2->left, e->we.popupmenu.pt.y - w2->top) == WP(w, menu_d).main_button)
					index = WP(w, menu_d).sel_index;
			}

			int action_id = WP(w, menu_d).action_id;
			delete w;

			if (index >= 0) {
				assert((uint)index <= lengthof(_menu_clicked_procs));
				_menu_clicked_procs[action_id](index);
			}

		} break;

		case WE_POPUPMENU_OVER: {
			int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);

			if (index == -1 || index == WP(w, menu_d).sel_index) return;

			WP(w, menu_d).sel_index = index;
			w->SetDirty();
			return;
		}
	}
}

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
 * @param w Pointer to a window this dropdown menu belongs to. Has no effect
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
 * @return Return a pointer to the newly created dropdown window */
static Window *PopupMainToolbMenu(Window *w, uint16 parent_button, StringID base_string, byte item_count, byte disabled_mask)
{
	assert(disabled_mask == 0 || item_count <= 8);
	w->LowerWidget(parent_button);
	w->InvalidateWidget(parent_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);

	/* Extend the dropdown toolbar to the longest string in the list */
	int width = max(GetStringListMaxWidth(base_string, item_count) + 6, 140);
	int height = item_count * 10 + 2;

	Point pos = GetToolbarDropdownPos(parent_button, width, height);

	w = AllocateWindow(pos.x, pos.y, width, height, MenuWndProc, WC_TOOLBAR_MENU, _menu_widgets);
	w->widget[0].bottom = item_count * 10 + 1;
	w->flags4 &= ~WF_WHITE_BORDER_MASK;

	WP(w, menu_d).item_count = item_count;
	WP(w, menu_d).sel_index = 0;
	WP(w, menu_d).main_button = GB(parent_button, 0, 8);
	WP(w, menu_d).action_id = (GB(parent_button, 8, 8) != 0) ? GB(parent_button, 8, 8) : parent_button;
	WP(w, menu_d).string_id = base_string;
	WP(w, menu_d).checked_items = 0;
	WP(w, menu_d).disabled_items = disabled_mask;

	_popup_menu_active = true;

	SndPlayFx(SND_15_BEEP);
	return w;
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

static void UpdatePlayerMenuHeight(Window *w)
{
	byte num = ActivePlayerCount();

	/* Increase one to fit in PlayerList in the menu when in network */
	if (_networking && WP(w, menu_d).main_button == 9) num++;

	if (WP(w, menu_d).item_count != num) {
		WP(w, menu_d).item_count = num;
		w->SetDirty();
		num = num * 10 + 2;
		w->height = num;
		w->widget[0].bottom = w->widget[0].top + num - 1;
		w->top = GetToolbarDropdownPos(0, w->width, w->height).y;
		w->SetDirty();
	}
}

static void PlayerMenuWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			UpdatePlayerMenuHeight(w);
			DrawWindowWidgets(w);

			int x = 1;
			int y = 1;
			int sel = WP(w, menu_d).sel_index;
			int chk = WP(w, menu_d).checked_items; // let this mean gray items.

			/* 9 = playerlist */
			if (_networking && WP(w, menu_d).main_button == 9) {
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
					if (chk & 1) color = TC_GREY;
					DrawString(x + 19, y, STR_7021, color);

					y += 10;
				}
				chk >>= 1;
			}
		 } break;

		case WE_DESTROY: {
			Window *v = FindWindowById(WC_MAIN_TOOLBAR, 0);
			v->RaiseWidget(WP(w, menu_d).main_button);
			w->SetDirty();
			return;
		}

		case WE_POPUPMENU_SELECT: {
			int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);
			int action_id = WP(w, menu_d).action_id;

			/* We have a new entry at the top of the list of menu 9 when networking
			*  so keep that in count */
			if (_networking && WP(w, menu_d).main_button == 9) {
				if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
			} else {
				index = GetPlayerIndexFromMenu(index);
			}

			if (index < 0) {
				Window *w2 = FindWindowById(WC_MAIN_TOOLBAR,0);
				if (GetWidgetFromPos(w2, e->we.popupmenu.pt.x - w2->left, e->we.popupmenu.pt.y - w2->top) == WP(w, menu_d).main_button)
					index = WP(w, menu_d).sel_index;
			}

			delete w;

			if (index >= 0) {
				assert(index >= 0 && index < 30);
				_menu_clicked_procs[action_id](index);
			}
		} break;

		case WE_POPUPMENU_OVER: {
			int index;
			UpdatePlayerMenuHeight(w);
			index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);

			/* We have a new entry at the top of the list of menu 9 when networking
			* so keep that in count */
			if (_networking && WP(w, menu_d).main_button == 9) {
				if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
			} else {
				index = GetPlayerIndexFromMenu(index);
			}

			if (index == -1 || index == WP(w, menu_d).sel_index) return;

			WP(w, menu_d).sel_index = index;
			w->SetDirty();
			return;
		}
	}
}

static const Widget _player_menu_widgets[] = {
{    WWT_PANEL, RESIZE_NONE, 14, 0, 240, 0, 81, 0x0, STR_NULL},
{ WIDGETS_END},
};

static Window *PopupMainPlayerToolbMenu(Window *w, int main_button, int gray)
{
	w->LowerWidget(main_button);
	w->InvalidateWidget(main_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);
	Point pos = GetToolbarDropdownPos(main_button, 241, 82);
	w = AllocateWindow(pos.x, pos.y, 241, 82, PlayerMenuWndProc, WC_TOOLBAR_MENU, _player_menu_widgets);
	w->flags4 &= ~WF_WHITE_BORDER_MASK;
	WP(w, menu_d).item_count = 0;
	WP(w, menu_d).sel_index = (_local_player != PLAYER_SPECTATOR) ? _local_player : GetPlayerIndexFromMenu(0);
	if (_networking && main_button == 9) {
		if (_local_player != PLAYER_SPECTATOR) {
			WP(w, menu_d).sel_index++;
		} else {
			/* Select client list by default for spectators */
			WP(w, menu_d).sel_index = 0;
		}
	}
	WP(w, menu_d).action_id = main_button;
	WP(w, menu_d).main_button = main_button;
	WP(w, menu_d).checked_items = gray;
	WP(w, menu_d).disabled_items = 0;
	_popup_menu_active = true;
	SndPlayFx(SND_15_BEEP);
	return w;
}

/* --- Allocating the toolbar --- */

Window *AllocateToolbar()
{
	/* Clean old GUI values; railtype is (re)set by rail_gui.cpp */
	_last_built_roadtype = ROADTYPE_ROAD;

	Window *w = AllocateWindowDesc((_game_mode != GM_EDITOR) ? &_toolb_normal_desc : &_toolb_scen_desc);
	if (w == NULL) return NULL;

	CLRBITS(w->flags4, WF_WHITE_BORDER_MASK);

	w->SetWidgetDisabledState(0, _networking && !_network_server); // if not server, disable pause button
	w->SetWidgetDisabledState(1, _networking); // if networking, disable fast-forward button

	/* 'w' is for sure a WC_MAIN_TOOLBAR */
	PositionMainToolbar(w);

	return w;
}
