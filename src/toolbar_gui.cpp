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

#include "network/network.h"

#include "table/strings.h"
#include "table/sprites.h"

extern void SelectSignTool();
extern RailType _last_built_railtype;
extern RoadType _last_built_roadtype;

/* Returns the position where the toolbar wants the menu to appear.
 * Make sure the dropdown is fully visible within the window.
 * x + w->left because x is supposed to be the offset of the toolbar-button
 * we clicked on and w->left the toolbar window itself. So meaning that
 * the default position is aligned with the left side of the clicked button */
Point GetToolbarDropdownPos(uint16 parent_button, int width, int height)
{
	const Window *w = FindWindowById(WC_MAIN_TOOLBAR, 0);
	Point pos;
	pos.x = w->widget[GB(parent_button, 0, 8)].left;
	pos.x = w->left + Clamp(pos.x, 0, w->width - width);
	pos.y = w->height;

	return pos;
}


static void ToolbarPauseClick(Window *w)
{
	if (_networking && !_network_server) return; // only server can pause the game

	if (DoCommandP(0, _pause_game ? 0 : 1, 0, NULL, CMD_PAUSE)) SndPlayFx(SND_15_BEEP);
}

static void ToolbarFastForwardClick(Window *w)
{
	_fast_forward ^= true;
	SndPlayFx(SND_15_BEEP);
}

static void ToolbarSaveClick(Window *w)
{
	PopupMainToolbMenu(w, 3, STR_015C_SAVE_GAME, 4, 0);
}

static void ToolbarMapClick(Window *w)
{
	PopupMainToolbMenu(w, 4, STR_02DE_MAP_OF_WORLD, 3, 0);
}

static void ToolbarTownClick(Window *w)
{
	PopupMainToolbMenu(w, 5, STR_02BB_TOWN_DIRECTORY, 1, 0);
}

static void ToolbarSubsidiesClick(Window *w)
{
	PopupMainToolbMenu(w, 6, STR_02DD_SUBSIDIES, 1, 0);
}

static void ToolbarStationsClick(Window *w)
{
	PopupMainPlayerToolbMenu(w, 7, 0);
}

static void ToolbarMoneyClick(Window *w)
{
	PopupMainPlayerToolbMenu(w, 8, 0);
}

static void ToolbarPlayersClick(Window *w)
{
	PopupMainPlayerToolbMenu(w, 9, 0);
}

static void ToolbarGraphsClick(Window *w)
{
	PopupMainToolbMenu(w, 10, STR_0154_OPERATING_PROFIT_GRAPH, 6, 0);
}

static void ToolbarLeagueClick(Window *w)
{
	PopupMainToolbMenu(w, 11, STR_015A_COMPANY_LEAGUE_TABLE, 2, 0);
}

static void ToolbarIndustryClick(Window *w)
{
	/* Disable build-industry menu if we are a spectator */
	PopupMainToolbMenu(w, 12, STR_INDUSTRY_DIR, 2, (_current_player == PLAYER_SPECTATOR) ? (1 << 1) : 0);
}

static void ToolbarTrainClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && IsFrontEngine(v)) ClrBit(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 13, dis);
}

static void ToolbarRoadClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_ROAD && IsRoadVehFront(v)) ClrBit(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 14, dis);
}

static void ToolbarShipClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_SHIP) ClrBit(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 15, dis);
}

static void ToolbarAirClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_AIRCRAFT) ClrBit(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 16, dis);
}


static void ToolbarZoomInClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(17);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarZoomOutClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		w->HandleButtonClick(18);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarBuildRailClick(Window *w)
{
	const Player *p = GetPlayer(_local_player);
	Window *w2 = PopupMainToolbMenu(w, 19, STR_1015_RAILROAD_CONSTRUCTION, RAILTYPE_END, ~p->avail_railtypes);
	WP(w2, menu_d).sel_index = _last_built_railtype;
}

static void ToolbarBuildRoadClick(Window *w)
{
	const Player *p = GetPlayer(_local_player);
	/* The standard road button is *always* available */
	Window *w2 = PopupMainToolbMenu(w, 20, STR_180A_ROAD_CONSTRUCTION, 2, ~(p->avail_roadtypes | ROADTYPES_ROAD));
	WP(w2, menu_d).sel_index = _last_built_roadtype;
}

static void ToolbarBuildWaterClick(Window *w)
{
	PopupMainToolbMenu(w, 21, STR_9800_DOCK_CONSTRUCTION, 1, 0);
}

static void ToolbarBuildAirClick(Window *w)
{
	PopupMainToolbMenu(w, 22, STR_A01D_AIRPORT_CONSTRUCTION, 1, 0);
}

static void ToolbarForestClick(Window *w)
{
	PopupMainToolbMenu(w, 23, STR_LANDSCAPING, 3, 0);
}

static void ToolbarMusicClick(Window *w)
{
	PopupMainToolbMenu(w, 24, STR_01D3_SOUND_MUSIC, 1, 0);
}

static void ToolbarNewspaperClick(Window *w)
{
	PopupMainToolbMenu(w, 25, STR_0200_LAST_MESSAGE_NEWS_REPORT, 3, 0);
}

static void ToolbarHelpClick(Window *w)
{
	PopupMainToolbMenu(w, 26, STR_02D5_LAND_BLOCK_INFO, 6, 0);
}

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


static void ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, 3, STR_0292_SAVE_SCENARIO, 6, 0);
}

static void ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
		w->HandleButtonClick(6);
		SetWindowDirty(w);

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
		SetWindowDirty(w);

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
	ToolbarMoneyClick,
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

extern void MenuClickSmallScreenshot();
extern void MenuClickWorldScreenshot();
extern void MenuClickSaveLoad(int index);

void MainToolbarWndProc(Window *w, WindowEvent *e)
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
			if (_game_mode != GM_MENU && !w->IsWidgetDisabled(e->we.click.widget))
				_toolbar_button_procs[e->we.click.widget](w);
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
			SetWindowDirty(w);
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

void ScenEditToolbarWndProc(Window *w, WindowEvent *e)
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
			SetWindowDirty(w);
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
				SetWindowDirty(w);
			}

			if (w->IsWidgetLowered(1) != !!_fast_forward) {
				w->ToggleWidgetLoweredState(1);
				SetWindowDirty(w);
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

Window *AllocateToolbar()
{
	return AllocateWindowDesc((_game_mode != GM_EDITOR) ? &_toolb_normal_desc : &_toolb_scen_desc);
}
