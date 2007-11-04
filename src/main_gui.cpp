/* $Id$ */

/** @file main_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "currency.h"
#include "functions.h"
#include "spritecache.h"
#include "station.h"
#include "strings.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "player.h"
#include "command.h"
#include "news.h"
#include "town.h"
#include "vehicle.h"
#include "console.h"
#include "sound.h"
#include "network/network.h"
#include "signs.h"
#include "waypoint.h"
#include "variables.h"
#include "train.h"
#include "roadveh.h"
#include "bridge_map.h"
#include "unmovable_map.h"
#include "string.h"
#include "screenshot.h"
#include "genworld.h"
#include "settings.h"
#include "date.h"
#include "vehicle_gui.h"
#include "transparency_gui.h"
#include "newgrf_config.h"

#include "network/network_data.h"
#include "network/network_client.h"
#include "network/network_server.h"
#include "network/network_gui.h"
#include "industry.h"

static int _rename_id = 1;
static int _rename_what = -1;

static byte _terraform_size = 1;
RailType _last_built_railtype;
RoadType _last_built_roadtype;
static int _scengen_town_size = 1; // depress medium-sized towns per default

extern void GenerateIndustries();
extern bool GenerateTowns();

bool _draw_bounding_boxes = false;


void CcGiveMoney(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
#ifdef ENABLE_NETWORK
	if (!success || !_patches.give_money) return;

	char msg[20];
	/* Inform the player of this action */
	snprintf(msg, sizeof(msg), "%d", p1);

	if (!_network_server) {
		SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_GIVE_MONEY, DESTTYPE_TEAM, p2, msg);
	} else {
		NetworkServer_HandleChat(NETWORK_ACTION_GIVE_MONEY, DESTTYPE_TEAM, p2, msg, NETWORK_SERVER_INDEX);
	}
#endif /* ENABLE_NETWORK */
}

void HandleOnEditText(const char *str)
{
	int id = _rename_id;
	_cmd_text = str;

	switch (_rename_what) {
	case 1: // Rename a waypoint
		if (*str == '\0') return;
		DoCommandP(0, id, 0, NULL, CMD_RENAME_WAYPOINT | CMD_MSG(STR_CANT_CHANGE_WAYPOINT_NAME));
		break;
#ifdef ENABLE_NETWORK
	case 3: { // Give money, you can only give money in excess of loan
		const Player *p = GetPlayer(_current_player);
		Money money = min(p->player_money - p->current_loan, (Money)(atoi(str) / _currency->rate));

		uint32 money_c = clamp(ClampToI32(money), 0, 20000000); // Clamp between 20 million and 0

		/* Give 'id' the money, and substract it from ourself */
		DoCommandP(0, money_c, id, CcGiveMoney, CMD_GIVE_MONEY | CMD_MSG(STR_INSUFFICIENT_FUNDS));
	} break;
#endif /* ENABLE_NETWORK */
		default: NOT_REACHED();
	}

	_rename_id = _rename_what = -1;
}

/**
 * This code is shared for the majority of the pushbuttons.
 * Handles e.g. the pressing of a button (to build things), playing of click sound and sets certain parameters
 *
 * @param w Window which called the function
 * @param widget ID of the widget (=button) that called this function
 * @param cursor How should the cursor image change? E.g. cursor with depot image in it
 * @param mode Tile highlighting mode, e.g. drawing a rectangle or a dot on the ground
 * @param placeproc Procedure which will be called when someone clicks on the map
 * @return true if the button is clicked, false if it's unclicked
 */
bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, int mode, PlaceProc *placeproc)
{
	if (IsWindowWidgetDisabled(w, widget)) return false;

	SndPlayFx(SND_15_BEEP);
	SetWindowDirty(w);

	if (IsWindowWidgetLowered(w, widget)) {
		ResetObjectToPlace();
		return false;
	}

	SetObjectToPlace(cursor, PAL_NONE, mode, w->window_class, w->window_number);
	LowerWindowWidget(w, widget);
	_place_proc = placeproc;
	return true;
}


void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_12_EXPLOSION, tile);
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


/** Toggle all transparency options, except for signs */
static void ToggleTransparency()
{
	static byte trans_opt = ~0;

	if (GB(_transparent_opt, 1, 7) == 0) {
		SB(_transparent_opt, 1, 7, GB(trans_opt, 1, 7));
	} else {
		trans_opt = _transparent_opt;
		SB(_transparent_opt, 1, 7, 0);
	}

	MarkWholeScreenDirty();
}


static void MenuClickSettings(int index)
{
	switch (index) {
		case 0: ShowGameOptions();      return;
		case 1: ShowGameDifficulty();   return;
		case 2: ShowPatchesSelection(); return;
		case 3: ShowNewGRFSettings(!_networking, true, true, &_grfconfig);   return;
		case 4: ShowTransparencyToolbar(); break;

		case  6: TOGGLEBIT(_display_opt, DO_SHOW_TOWN_NAMES);    break;
		case  7: TOGGLEBIT(_display_opt, DO_SHOW_STATION_NAMES); break;
		case  8: TOGGLEBIT(_display_opt, DO_SHOW_SIGNS);         break;
		case  9: TOGGLEBIT(_display_opt, DO_WAYPOINTS);          break;
		case 10: TOGGLEBIT(_display_opt, DO_FULL_ANIMATION);     break;
		case 11: TOGGLEBIT(_display_opt, DO_FULL_DETAIL);        break;
		case 12: ToggleTransparency(); break;
		case 13: TOGGLEBIT(_transparent_opt, TO_SIGNS); break;
	}
	MarkWholeScreenDirty();
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

static void MenuClickMap(int index)
{
	switch (index) {
		case 0: ShowSmallMap();            break;
		case 1: ShowExtraViewPortWindow(); break;
		case 2: ShowSignList();            break;
	}
}

static void MenuClickTown(int index)
{
	ShowTownDirectory();
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

static void MenuClickSubsidies(int index)
{
	ShowSubsidiesList();
}

static void MenuClickStations(int index)
{
	ShowPlayerStations((PlayerID)index);
}

static void MenuClickFinances(int index)
{
	ShowPlayerFinances((PlayerID)index);
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

static void MenuClickLeague(int index)
{
	switch (index) {
		case 0: ShowCompanyLeagueTable();      break;
		case 1: ShowPerformanceRatingDetail(); break;
	}
}

static void MenuClickIndustry(int index)
{
	switch (index) {
		case 0: ShowIndustryDirectory();   break;
		case 1: ShowBuildIndustryWindow(); break;
	}
}

static void MenuClickShowTrains(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_TRAIN);
}

static void MenuClickShowRoad(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_ROAD);
}

static void MenuClickShowShips(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_SHIP);
}

static void MenuClickShowAir(int index)
{
	ShowVehicleListWindow((PlayerID)index, VEH_AIRCRAFT);
}

static void MenuClickBuildRail(int index)
{
	_last_built_railtype = (RailType)index;
	ShowBuildRailToolbar(_last_built_railtype, -1);
}

static void MenuClickBuildRoad(int index)
{
	_last_built_roadtype = (RoadType)index;
	ShowBuildRoadToolbar(_last_built_roadtype);
}

static void MenuClickBuildWater(int index)
{
	ShowBuildDocksToolbar();
}

static void MenuClickBuildAir(int index)
{
	ShowBuildAirToolbar();
}

#ifdef ENABLE_NETWORK
void ShowNetworkGiveMoneyWindow(PlayerID player)
{
	_rename_id = player;
	_rename_what = 3;
	ShowQueryString(STR_EMPTY, STR_NETWORK_GIVE_MONEY_CAPTION, 30, 180, NULL, CS_NUMERAL);
}
#endif /* ENABLE_NETWORK */

void ShowRenameWaypointWindow(const Waypoint *wp)
{
	int id = wp->index;

	/* Are we allowed to change the name of the waypoint? */
	if (!CheckTileOwnership(wp->xy)) {
		ShowErrorMessage(_error_message, STR_CANT_CHANGE_WAYPOINT_NAME,
			TileX(wp->xy) * TILE_SIZE, TileY(wp->xy) * TILE_SIZE);
		return;
	}

	_rename_id = id;
	_rename_what = 1;
	SetDParam(0, id);
	ShowQueryString(STR_WAYPOINT_RAW, STR_EDIT_WAYPOINT_NAME, 30, 180, NULL, CS_ALPHANUMERAL);
}

static void SelectSignTool()
{
	if (_cursor.sprite == SPR_CURSOR_SIGN) {
		ResetObjectToPlace();
	} else {
		SetObjectToPlace(SPR_CURSOR_SIGN, PAL_NONE, 1, WC_MAIN_TOOLBAR, 0);
		_place_proc = PlaceProc_Sign;
	}
}

static void MenuClickForest(int index)
{
	switch (index) {
		case 0: ShowTerraformToolbar();  break;
		case 1: ShowBuildTreesToolbar(); break;
		case 2: SelectSignTool();        break;
	}
}

static void MenuClickMusicWindow(int index)
{
	ShowMusicWindow();
}

static void MenuClickNewspaper(int index)
{
	switch (index) {
		case 0: ShowLastNewsMessage(); break;
		case 1: ShowMessageOptions();  break;
		case 2: ShowMessageHistory();  break;
	}
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
		case WE_CREATE: w->widget[0].right = w->width - 1; break;

	case WE_PAINT: {
		int x, y;

		byte count = WP(w, menu_d).item_count;
		byte sel = WP(w, menu_d).sel_index;
		uint16 chk = WP(w, menu_d).checked_items;
		StringID string = WP(w, menu_d).string_id;
		byte dis = WP(w, menu_d).disabled_items;

		DrawWindowWidgets(w);

		x = 1;
		y = 1;

		for (; count != 0; count--, string++, sel--) {
			TextColour color = HASBIT(dis, 0) ? TC_GREY : (sel == 0) ? TC_WHITE : TC_BLACK;
			if (sel == 0) GfxFillRect(x, y, x + w->width - 3, y + 9, 0);

			if (HASBIT(chk, 0)) DrawString(x + 2, y, STR_CHECKMARK, color);
			DrawString(x + 2, y, string, color);

			y += 10;
			chk >>= 1;
			dis >>= 1;
		}
	} break;

	case WE_DESTROY: {
			Window *v = FindWindowById(WC_MAIN_TOOLBAR, 0);
			RaiseWindowWidget(v, WP(w,menu_d).main_button);
			SetWindowDirty(v);
			return;
		}

	case WE_POPUPMENU_SELECT: {
		int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);
		int action_id;


		if (index < 0) {
			Window *w2 = FindWindowById(WC_MAIN_TOOLBAR,0);
			if (GetWidgetFromPos(w2, e->we.popupmenu.pt.x - w2->left, e->we.popupmenu.pt.y - w2->top) == WP(w,menu_d).main_button)
				index = WP(w,menu_d).sel_index;
		}

		action_id = WP(w,menu_d).action_id;
		DeleteWindow(w);

		if (index >= 0) {
			assert((uint)index <= lengthof(_menu_clicked_procs));
			_menu_clicked_procs[action_id](index);
		}

		break;
		}

	case WE_POPUPMENU_OVER: {
		int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);

		if (index == -1 || index == WP(w,menu_d).sel_index) return;

		WP(w,menu_d).sel_index = index;
		SetWindowDirty(w);
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


static const Widget _player_menu_widgets[] = {
{    WWT_PANEL, RESIZE_NONE, 14, 0, 240, 0, 81, 0x0, STR_NULL},
{ WIDGETS_END},
};


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
	if (_networking && WP(w,menu_d).main_button == 9) num++;

	if (WP(w,menu_d).item_count != num) {
		WP(w,menu_d).item_count = num;
		SetWindowDirty(w);
		num = num * 10 + 2;
		w->height = num;
		w->widget[0].bottom = w->widget[0].top + num - 1;
		SetWindowDirty(w);
	}
}

static void PlayerMenuWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int x,y;
		byte sel;
		TextColour color;
		Player *p;
		uint16 chk;

		UpdatePlayerMenuHeight(w);
		DrawWindowWidgets(w);

		x = 1;
		y = 1;
		sel = WP(w,menu_d).sel_index;
		chk = WP(w,menu_d).checked_items; // let this mean gray items.

		/* 9 = playerlist */
		if (_networking && WP(w,menu_d).main_button == 9) {
			if (sel == 0) {
				GfxFillRect(x, y, x + 238, y + 9, 0);
			}
			DrawString(x + 19, y, STR_NETWORK_CLIENT_LIST, TC_FROMSTRING);
			y += 10;
			sel--;
		}

		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				if (p->index == sel) {
					GfxFillRect(x, y, x + 238, y + 9, 0);
				}

				DrawPlayerIcon(p->index, x + 2, y + 1);

				SetDParam(0, p->index);
				SetDParam(1, p->index);

				color = (p->index == sel) ? TC_WHITE : TC_BLACK;
				if (chk&1) color = TC_GREY;
				DrawString(x + 19, y, STR_7021, color);

				y += 10;
			}
			chk >>= 1;
		}

		break;
		}

	case WE_DESTROY: {
		Window *v = FindWindowById(WC_MAIN_TOOLBAR, 0);
		RaiseWindowWidget(v, WP(w,menu_d).main_button);
		SetWindowDirty(v);
		return;
		}

	case WE_POPUPMENU_SELECT: {
		int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);
		int action_id = WP(w,menu_d).action_id;

		/* We have a new entry at the top of the list of menu 9 when networking
		 *  so keep that in count */
		if (_networking && WP(w,menu_d).main_button == 9) {
			if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
		} else {
			index = GetPlayerIndexFromMenu(index);
		}

		if (index < 0) {
			Window *w2 = FindWindowById(WC_MAIN_TOOLBAR,0);
			if (GetWidgetFromPos(w2, e->we.popupmenu.pt.x - w2->left, e->we.popupmenu.pt.y - w2->top) == WP(w,menu_d).main_button)
				index = WP(w,menu_d).sel_index;
		}

		DeleteWindow(w);

		if (index >= 0) {
			assert(index >= 0 && index < 30);
			_menu_clicked_procs[action_id](index);
		}
		break;
		}
	case WE_POPUPMENU_OVER: {
		int index;
		UpdatePlayerMenuHeight(w);
		index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);

		/* We have a new entry at the top of the list of menu 9 when networking
		 * so keep that in count */
		if (_networking && WP(w,menu_d).main_button == 9) {
			if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
		} else {
			index = GetPlayerIndexFromMenu(index);
		}

		if (index == -1 || index == WP(w,menu_d).sel_index) return;

		WP(w,menu_d).sel_index = index;
		SetWindowDirty(w);
		return;
		}
	}
}

/** Get the maximum length of a given string in a string-list. This is an
 * implicit string-list where the ID's are consecutive
 * @param base_string StringID of the first string in the list
 * @param count amount of StringID's in the list
 * @return the length of the longest string */
static int GetStringListMaxWidth(StringID base_string, byte count)
{
	char buffer[512];
	int width, max_width;
	byte i;

	max_width = 0;
	for (i = 0; i != count; i++) {
		GetString(buffer, base_string + i, lastof(buffer));
		width = GetStringBoundingBox(buffer).width;
		if (width > max_width) max_width = width;
	}

	return max_width;
}

/** Show a general dropdown menu. The positioning of the dropdown menu
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
	int width;
	int x = w->widget[GB(parent_button, 0, 8)].left;

	assert(disabled_mask == 0 || item_count <= 8);
	LowerWindowWidget(w, parent_button);
	InvalidateWidget(w, parent_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);

	/* Extend the dropdown toolbar to the longest string in the list and
	 * also make sure the dropdown is fully visible within the window.
	 * x + w->left because x is supposed to be the offset of the toolbar-button
	 * we clicked on and w->left the toolbar window itself. So meaning that
	 * the default position is aligned with the left side of the clicked button */
	width = max(GetStringListMaxWidth(base_string, item_count) + 6, 140);
	x = w->left + clamp(x, 0, w->width - width); // or alternatively '_screen.width - width'

	w = AllocateWindow(x, 22, width, item_count * 10 + 2, MenuWndProc, WC_TOOLBAR_MENU, _menu_widgets);
	w->widget[0].bottom = item_count * 10 + 1;
	w->flags4 &= ~WF_WHITE_BORDER_MASK;

	WP(w,menu_d).item_count = item_count;
	WP(w,menu_d).sel_index = 0;
	WP(w,menu_d).main_button = GB(parent_button, 0, 8);
	WP(w,menu_d).action_id = (GB(parent_button, 8, 8) != 0) ? GB(parent_button, 8, 8) : parent_button;
	WP(w,menu_d).string_id = base_string;
	WP(w,menu_d).checked_items = 0;
	WP(w,menu_d).disabled_items = disabled_mask;

	_popup_menu_active = true;

	SndPlayFx(SND_15_BEEP);
	return w;
}

static Window *PopupMainPlayerToolbMenu(Window *w, int main_button, int gray)
{
	int x = w->widget[main_button].left + w->left;

	LowerWindowWidget(w, main_button);
	InvalidateWidget(w, main_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);
	w = AllocateWindow(x, 0x16, 0xF1, 0x52, PlayerMenuWndProc, WC_TOOLBAR_MENU, _player_menu_widgets);
	w->flags4 &= ~WF_WHITE_BORDER_MASK;
	WP(w,menu_d).item_count = 0;
	WP(w,menu_d).sel_index = (_local_player != PLAYER_SPECTATOR) ? _local_player : GetPlayerIndexFromMenu(0);
	if (_networking && main_button == 9) {
		if (_local_player != PLAYER_SPECTATOR) {
			WP(w,menu_d).sel_index++;
		} else {
			/* Select client list by default for spectators */
			WP(w,menu_d).sel_index = 0;
		}
	}
	WP(w,menu_d).action_id = main_button;
	WP(w,menu_d).main_button = main_button;
	WP(w,menu_d).checked_items = gray;
	WP(w,menu_d).disabled_items = 0;
	_popup_menu_active = true;
	SndPlayFx(SND_15_BEEP);
	return w;
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
		if (v->type == VEH_TRAIN && IsFrontEngine(v)) CLRBIT(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 13, dis);
}

static void ToolbarRoadClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_ROAD && IsRoadVehFront(v)) CLRBIT(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 14, dis);
}

static void ToolbarShipClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_SHIP) CLRBIT(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 15, dis);
}

static void ToolbarAirClick(Window *w)
{
	const Vehicle *v;
	int dis = -1;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_AIRCRAFT) CLRBIT(dis, v->owner);
	}
	PopupMainPlayerToolbMenu(w, 16, dis);
}

/* Zooms a viewport in a window in or out */
/* No button handling or what so ever */
bool DoZoomInOutWindow(int how, Window *w)
{
	ViewPort *vp;

	assert(w != NULL);
	vp = w->viewport;

	switch (how) {
		case ZOOM_IN:
			if (vp->zoom == ZOOM_LVL_MIN) return false;
			vp->zoom = (ZoomLevel)((byte)vp->zoom - 1);
			vp->virtual_width >>= 1;
			vp->virtual_height >>= 1;

			WP(w,vp_d).scrollpos_x += vp->virtual_width >> 1;
			WP(w,vp_d).scrollpos_y += vp->virtual_height >> 1;
			WP(w,vp_d).dest_scrollpos_x = WP(w,vp_d).scrollpos_x;
			WP(w,vp_d).dest_scrollpos_y = WP(w,vp_d).scrollpos_y;
			break;
		case ZOOM_OUT:
			if (vp->zoom == ZOOM_LVL_MAX) return false;
			vp->zoom = (ZoomLevel)((byte)vp->zoom + 1);

			WP(w,vp_d).scrollpos_x -= vp->virtual_width >> 1;
			WP(w,vp_d).scrollpos_y -= vp->virtual_height >> 1;
			WP(w,vp_d).dest_scrollpos_x = WP(w,vp_d).scrollpos_x;
			WP(w,vp_d).dest_scrollpos_y = WP(w,vp_d).scrollpos_y;

			vp->virtual_width <<= 1;
			vp->virtual_height <<= 1;
			break;
	}
	if (vp != NULL) { // the vp can be null when how == ZOOM_NONE
		vp->virtual_left = WP(w, vp_d).scrollpos_x;
		vp->virtual_top = WP(w, vp_d).scrollpos_y;
	}
	SetWindowDirty(w);
	/* Update the windows that have zoom-buttons to perhaps disable their buttons */
	SendWindowMessageClass(w->window_class, how, w->window_number, 0);
	return true;
}

static void ToolbarZoomInClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_IN, FindWindowById(WC_MAIN_WINDOW, 0))) {
		HandleButtonClick(w, 17);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarZoomOutClick(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT,FindWindowById(WC_MAIN_WINDOW, 0))) {
		HandleButtonClick(w, 18);
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
	Window *w2 = PopupMainToolbMenu(w, 20, STR_180A_ROAD_CONSTRUCTION, 2, ~(p->avail_roadtypes | 1));
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

	w = PopupMainToolbMenu(w, 2, STR_02C3_GAME_OPTIONS, 14, 0);

	if (HASBIT(_display_opt, DO_SHOW_TOWN_NAMES))    SETBIT(x,  6);
	if (HASBIT(_display_opt, DO_SHOW_STATION_NAMES)) SETBIT(x,  7);
	if (HASBIT(_display_opt, DO_SHOW_SIGNS))         SETBIT(x,  8);
	if (HASBIT(_display_opt, DO_WAYPOINTS))          SETBIT(x,  9);
	if (HASBIT(_display_opt, DO_FULL_ANIMATION))     SETBIT(x, 10);
	if (HASBIT(_display_opt, DO_FULL_DETAIL))        SETBIT(x, 11);
	if (GB(_transparent_opt, 1, 7) != 0)      SETBIT(x, 12);
	if (HASBIT(_transparent_opt, TO_SIGNS))   SETBIT(x, 13);
	WP(w,menu_d).checked_items = x;
}


static void ToolbarScenSaveOrLoad(Window *w)
{
	PopupMainToolbMenu(w, 3, STR_0292_SAVE_SCENARIO, 6, 0);
}

static void ToolbarScenDateBackward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
		HandleButtonClick(w, 6);
		SetWindowDirty(w);

		_patches_newgame.starting_year = clamp(_patches_newgame.starting_year - 1, MIN_YEAR, MAX_YEAR);
		SetDate(ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
	}
	_left_button_clicked = false;
}

static void ToolbarScenDateForward(Window *w)
{
	/* don't allow too fast scrolling */
	if ((w->flags4 & WF_TIMEOUT_MASK) <= 2 << WF_TIMEOUT_SHL) {
		HandleButtonClick(w, 7);
		SetWindowDirty(w);

		_patches_newgame.starting_year = clamp(_patches_newgame.starting_year + 1, MIN_YEAR, MAX_YEAR);
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
		HandleButtonClick(w, 9);
		SndPlayFx(SND_15_BEEP);
	}
}

static void ToolbarScenZoomOut(Window *w)
{
	if (DoZoomInOutWindow(ZOOM_OUT, FindWindowById(WC_MAIN_WINDOW, 0))) {
		HandleButtonClick(w, 10);
		SndPlayFx(SND_15_BEEP);
	}
}

void ZoomInOrOutToCursorWindow(bool in, Window *w)
{
	ViewPort *vp;
	Point pt;

	assert(w != 0);

	vp = w->viewport;

	if (_game_mode != GM_MENU) {
		if ((in && vp->zoom == ZOOM_LVL_MIN) || (!in && vp->zoom == ZOOM_LVL_MAX))
			return;

		pt = GetTileZoomCenterWindow(in,w);
		if (pt.x != -1) {
			ScrollWindowTo(pt.x, pt.y, w, true);

			DoZoomInOutWindow(in ? ZOOM_IN : ZOOM_OUT, w);
		}
	}
}

/**
 * Raise/Lower a bigger chunk of land at the same time in the editor. When
 * raising get the lowest point, when lowering the highest point, and set all
 * tiles in the selection to that height.
 * @todo : Incorporate into game itself to allow for ingame raising/lowering of
 *         larger chunks at the same time OR remove altogether, as we have 'level land' ?
 * @param tile The top-left tile where the terraforming will start
 * @param mode 1 for raising, 0 for lowering land
 */
static void CommonRaiseLowerBigLand(TileIndex tile, int mode)
{
	int sizex, sizey;
	uint h;

	_generating_world = true; // used to create green terraformed land

	if (_terraform_size == 1) {
		StringID msg =
			mode ? STR_0808_CAN_T_RAISE_LAND_HERE : STR_0809_CAN_T_LOWER_LAND_HERE;

		DoCommandP(tile, SLOPE_N, (uint32)mode, CcTerraform, CMD_TERRAFORM_LAND | CMD_MSG(msg));
	} else {
		SndPlayTileFx(SND_1F_SPLAT, tile);

		assert(_terraform_size != 0);
		/* check out for map overflows */
		sizex = min(MapSizeX() - TileX(tile) - 1, _terraform_size);
		sizey = min(MapSizeY() - TileY(tile) - 1, _terraform_size);

		if (sizex == 0 || sizey == 0) return;

		if (mode != 0) {
			/* Raise land */
			h = 15; // XXX - max height
			BEGIN_TILE_LOOP(tile2, sizex, sizey, tile) {
				h = min(h, TileHeight(tile2));
			} END_TILE_LOOP(tile2, sizex, sizey, tile)
		} else {
			/* Lower land */
			h = 0;
			BEGIN_TILE_LOOP(tile2, sizex, sizey, tile) {
				h = max(h, TileHeight(tile2));
			} END_TILE_LOOP(tile2, sizex, sizey, tile)
		}

		BEGIN_TILE_LOOP(tile2, sizex, sizey, tile) {
			if (TileHeight(tile2) == h) {
				DoCommandP(tile2, SLOPE_N, (uint32)mode, NULL, CMD_TERRAFORM_LAND);
			}
		} END_TILE_LOOP(tile2, sizex, sizey, tile)
	}

	_generating_world = false;
}

static void PlaceProc_RaiseBigLand(TileIndex tile)
{
	CommonRaiseLowerBigLand(tile, 1);
}

static void PlaceProc_LowerBigLand(TileIndex tile)
{
	CommonRaiseLowerBigLand(tile, 0);
}

static void PlaceProc_RockyArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_ROCKS);
}

static void PlaceProc_LightHouse(TileIndex tile)
{
	if (!IsTileType(tile, MP_CLEAR) || IsSteepSlope(GetTileSlope(tile, NULL)) || IsBridgeAbove(tile)) {
		return;
	}

	MakeLighthouse(tile);
	MarkTileDirtyByTile(tile);
	SndPlayTileFx(SND_1F_SPLAT, tile);
}

static void PlaceProc_Transmitter(TileIndex tile)
{
	if (!IsTileType(tile, MP_CLEAR) || IsSteepSlope(GetTileSlope(tile, NULL)) || IsBridgeAbove(tile)) {
		return;
	}

	MakeTransmitter(tile);
	MarkTileDirtyByTile(tile);
	SndPlayTileFx(SND_1F_SPLAT, tile);
}

static void PlaceProc_DesertArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_DESERT);
}

static void PlaceProc_WaterArea(TileIndex tile)
{
	VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_WATER);
}

static const Widget _scen_edit_land_gen_widgets[] = {
{  WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                  STR_018B_CLOSE_WINDOW},
{   WWT_CAPTION,   RESIZE_NONE,     7,    11,   169,     0,    13, STR_0223_LAND_GENERATION,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_STICKYBOX,   RESIZE_NONE,     7,   170,   181,     0,    13, STR_NULL,                  STR_STICKY_BUTTON},
{     WWT_PANEL,   RESIZE_NONE,     7,     0,   181,    14,   102, 0x0,                       STR_NULL},
{    WWT_IMGBTN,   RESIZE_NONE,    14,     2,    23,    16,    37, SPR_IMG_DYNAMITE,          STR_018D_DEMOLISH_BUILDINGS_ETC},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    24,    45,    16,    37, SPR_IMG_TERRAFORM_DOWN,    STR_018E_LOWER_A_CORNER_OF_LAND},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    46,    67,    16,    37, SPR_IMG_TERRAFORM_UP,      STR_018F_RAISE_A_CORNER_OF_LAND},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    68,    89,    16,    37, SPR_IMG_LEVEL_LAND,        STR_LEVEL_LAND_TOOLTIP},
{    WWT_IMGBTN,   RESIZE_NONE,    14,    90,   111,    16,    37, SPR_IMG_BUILD_CANAL,       STR_CREATE_LAKE},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   112,   134,    16,    37, SPR_IMG_ROCKS,             STR_028C_PLACE_ROCKY_AREAS_ON_LANDSCAPE},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   135,   157,    16,    37, SPR_IMG_LIGHTHOUSE_DESERT, STR_NULL}, // XXX - dynamic
{    WWT_IMGBTN,   RESIZE_NONE,    14,   158,   179,    16,    37, SPR_IMG_TRANSMITTER,       STR_028E_PLACE_TRANSMITTER},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   139,   150,    45,    56, SPR_ARROW_UP,              STR_0228_INCREASE_SIZE_OF_LAND_AREA},
{    WWT_IMGBTN,   RESIZE_NONE,    14,   139,   150,    58,    69, SPR_ARROW_DOWN,            STR_0229_DECREASE_SIZE_OF_LAND_AREA},
{   WWT_TEXTBTN,   RESIZE_NONE,    14,    24,   157,    76,    87, STR_SE_NEW_WORLD,          STR_022A_GENERATE_RANDOM_LAND},
{   WWT_TEXTBTN,   RESIZE_NONE,    14,    24,   157,    89,   100, STR_022B_RESET_LANDSCAPE,  STR_RESET_LANDSCAPE_TOOLTIP},
{   WIDGETS_END},
};

static const int8 _multi_terraform_coords[][2] = {
	{  0, -2},
	{  4,  0}, { -4,  0}, {  0,  2},
	{ -8,  2}, { -4,  4}, {  0,  6}, {  4,  4}, {  8,  2},
	{-12,  0}, { -8, -2}, { -4, -4}, {  0, -6}, {  4, -4}, {  8, -2}, { 12,  0},
	{-16,  2}, {-12,  4}, { -8,  6}, { -4,  8}, {  0, 10}, {  4,  8}, {  8,  6}, { 12,  4}, { 16,  2},
	{-20,  0}, {-16, -2}, {-12, -4}, { -8, -6}, { -4, -8}, {  0,-10}, {  4, -8}, {  8, -6}, { 12, -4}, { 16, -2}, { 20,  0},
	{-24,  2}, {-20,  4}, {-16,  6}, {-12,  8}, { -8, 10}, { -4, 12}, {  0, 14}, {  4, 12}, {  8, 10}, { 12,  8}, { 16,  6}, { 20,  4}, { 24,  2},
	{-28,  0}, {-24, -2}, {-20, -4}, {-16, -6}, {-12, -8}, { -8,-10}, { -4,-12}, {  0,-14}, {  4,-12}, {  8,-10}, { 12, -8}, { 16, -6}, { 20, -4}, { 24, -2}, { 28,  0},
};

/**
 * @todo Merge with terraform_gui.cpp (move there) after I have cooled down at its braindeadness
 * and changed OnButtonClick to include the widget as well in the function declaration. Post 0.4.0 - Darkvater
 */
static void EditorTerraformClick_Dynamite(Window *w)
{
	HandlePlacePushButton(w, 4, ANIMCURSOR_DEMOLISH, 1, PlaceProc_DemolishArea);
}

static void EditorTerraformClick_LowerBigLand(Window *w)
{
	HandlePlacePushButton(w, 5, ANIMCURSOR_LOWERLAND, 2, PlaceProc_LowerBigLand);
}

static void EditorTerraformClick_RaiseBigLand(Window *w)
{
	HandlePlacePushButton(w, 6, ANIMCURSOR_RAISELAND, 2, PlaceProc_RaiseBigLand);
}

static void EditorTerraformClick_LevelLand(Window *w)
{
	HandlePlacePushButton(w, 7, SPR_CURSOR_LEVEL_LAND, 2, PlaceProc_LevelLand);
}

static void EditorTerraformClick_WaterArea(Window *w)
{
	HandlePlacePushButton(w, 8, SPR_CURSOR_CANAL, 1, PlaceProc_WaterArea);
}

static void EditorTerraformClick_RockyArea(Window *w)
{
	HandlePlacePushButton(w, 9, SPR_CURSOR_ROCKY_AREA, 1, PlaceProc_RockyArea);
}

static void EditorTerraformClick_DesertLightHouse(Window *w)
{
	HandlePlacePushButton(w, 10, SPR_CURSOR_LIGHTHOUSE, 1, (_opt.landscape == LT_TROPIC) ? PlaceProc_DesertArea : PlaceProc_LightHouse);
}

static void EditorTerraformClick_Transmitter(Window *w)
{
	HandlePlacePushButton(w, 11, SPR_CURSOR_TRANSMITTER, 1, PlaceProc_Transmitter);
}

static const uint16 _editor_terraform_keycodes[] = {
	'D',
	'Q',
	'W',
	'E',
	'R',
	'T',
	'Y',
	'U'
};

typedef void OnButtonClick(Window *w);
static OnButtonClick * const _editor_terraform_button_proc[] = {
	EditorTerraformClick_Dynamite,
	EditorTerraformClick_LowerBigLand,
	EditorTerraformClick_RaiseBigLand,
	EditorTerraformClick_LevelLand,
	EditorTerraformClick_WaterArea,
	EditorTerraformClick_RockyArea,
	EditorTerraformClick_DesertLightHouse,
	EditorTerraformClick_Transmitter
};


/** Callback function for the scenario editor 'reset landscape' confirmation window
 * @param w Window unused
 * @param confirmed boolean value, true when yes was clicked, false otherwise */
static void ResetLandscapeConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		Player *p;

		/* Set generating_world to true to get instant-green grass after removing
		 * player property. */
		_generating_world = true;
		/* Delete all players */
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				ChangeOwnershipOfPlayerItems(p->index, PLAYER_SPECTATOR);
				p->is_active = false;
			}
		}
		_generating_world = false;

		/* Delete all stations owned by a player */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (IsValidPlayer(st->owner)) delete st;
		}
	}
}

static void ScenEditLandGenWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE:
		/* XXX - lighthouse button is widget 10!! Don't forget when changing */
		w->widget[10].tooltips = (_opt.landscape == LT_TROPIC) ? STR_028F_DEFINE_DESERT_AREA : STR_028D_PLACE_LIGHTHOUSE;
		break;

	case WE_PAINT:
		DrawWindowWidgets(w);

		{
			int n = _terraform_size * _terraform_size;
			const int8 *coords = &_multi_terraform_coords[0][0];

			assert(n != 0);
			do {
				DrawSprite(SPR_WHITE_POINT, PAL_NONE, 77 + coords[0], 55 + coords[1]);
				coords += 2;
			} while (--n);
		}

		if (IsWindowWidgetLowered(w, 5) || IsWindowWidgetLowered(w, 6)) // change area-size if raise/lower corner is selected
			SetTileSelectSize(_terraform_size, _terraform_size);

		break;

	case WE_KEYPRESS: {
		uint i;

		for (i = 0; i != lengthof(_editor_terraform_keycodes); i++) {
			if (e->we.keypress.keycode == _editor_terraform_keycodes[i]) {
				e->we.keypress.cont = false;
				_editor_terraform_button_proc[i](w);
				break;
			}
		}
	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11:
			_editor_terraform_button_proc[e->we.click.widget - 4](w);
			break;
		case 12: case 13: { // Increase/Decrease terraform size
			int size = (e->we.click.widget == 12) ? 1 : -1;
			HandleButtonClick(w, e->we.click.widget);
			size += _terraform_size;

			if (!IS_INT_INSIDE(size, 1, 8 + 1)) return;
			_terraform_size = size;

			SndPlayFx(SND_15_BEEP);
			SetWindowDirty(w);
		} break;
		case 14: // gen random land
			HandleButtonClick(w, 14);
			ShowCreateScenario();
			break;
		case 15: // Reset landscape
			ShowQuery(
			  STR_022C_RESET_LANDSCAPE,
			  STR_RESET_LANDSCAPE_CONFIRMATION_TEXT,
			  NULL,
			  ResetLandscapeConfirmationCallback);
			break;
		}
		break;

	case WE_TIMEOUT: {
		uint i;
		for (i = 0; i < w->widget_count; i++) {
			if (IsWindowWidgetLowered(w, i)) {
				RaiseWindowWidget(w, i);
				InvalidateWidget(w, i);
			}
			if (i == 3) i = 11;
		}
		break;
	}
	case WE_PLACE_OBJ:
		_place_proc(e->we.place.tile);
		break;
	case WE_PLACE_DRAG:
		VpSelectTilesWithMethod(e->we.place.pt.x, e->we.place.pt.y, e->we.place.select_method);
		break;

	case WE_PLACE_MOUSEUP:
		if (e->we.place.pt.x != -1) {
			switch (e->we.place.select_proc) {
				case DDSP_CREATE_ROCKS:
				case DDSP_CREATE_DESERT:
				case DDSP_CREATE_WATER:
				case DDSP_LEVEL_AREA:
				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(e);
					break;
			}
		}
		break;

	case WE_ABORT_PLACE_OBJ:
		RaiseWindowButtons(w);
		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _scen_edit_land_gen_desc = {
	WDP_AUTO, WDP_AUTO, 182, 103, 182, 103,
	WC_SCEN_LAND_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_scen_edit_land_gen_widgets,
	ScenEditLandGenWndProc,
};

static inline void ShowEditorTerraformToolBar()
{
	AllocateWindowDescFront(&_scen_edit_land_gen_desc, 0);
}

static void ToolbarScenGenLand(Window *w)
{
	HandleButtonClick(w, 11);
	SndPlayFx(SND_15_BEEP);

	ShowEditorTerraformToolBar();
}

void CcBuildTown(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) {
		SndPlayTileFx(SND_1F_SPLAT, tile);
		ResetObjectToPlace();
	}
}

static void PlaceProc_Town(TileIndex tile)
{
	uint32 size = min(_scengen_town_size, (int)TSM_CITY);
	uint32 mode = _scengen_town_size > TSM_CITY ? TSM_CITY : TSM_FIXED;
	DoCommandP(tile, size, mode, CcBuildTown, CMD_BUILD_TOWN | CMD_MSG(STR_0236_CAN_T_BUILD_TOWN_HERE));
}


static const Widget _scen_edit_town_gen_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,     7,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,     7,    11,   147,     0,    13, STR_0233_TOWN_GENERATION, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,     7,   148,   159,     0,    13, 0x0,                      STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,     7,     0,   159,    14,    94, 0x0,                      STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    16,    27, STR_0234_NEW_TOWN,        STR_0235_CONSTRUCT_NEW_TOWN},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    29,    40, STR_023D_RANDOM_TOWN,     STR_023E_BUILD_TOWN_IN_RANDOM_LOCATION},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    42,    53, STR_MANY_RANDOM_TOWNS,    STR_RANDOM_TOWNS_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,    53,    68,    79, STR_02A1_SMALL,           STR_02A4_SELECT_TOWN_SIZE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,    54,   105,    68,    79, STR_02A2_MEDIUM,          STR_02A4_SELECT_TOWN_SIZE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   106,   157,    68,    79, STR_02A3_LARGE,           STR_02A4_SELECT_TOWN_SIZE},
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     2,   157,    81,    92, STR_SCENARIO_EDITOR_CITY, STR_02A4_SELECT_TOWN_SIZE},
{      WWT_LABEL,   RESIZE_NONE,     7,     0,   147,    54,    67, STR_02A5_TOWN_SIZE,       STR_NULL},
{   WIDGETS_END},
};

static void ScenEditTownGenWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		DrawWindowWidgets(w);
		break;

	case WE_CREATE:
		LowerWindowWidget(w, _scengen_town_size + 7);
		break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 4: // new town
			HandlePlacePushButton(w, 4, SPR_CURSOR_TOWN, 1, PlaceProc_Town);
			break;
		case 5: {// random town
			Town *t;
			uint size = min(_scengen_town_size, (int)TSM_CITY);
			TownSizeMode mode = _scengen_town_size > TSM_CITY ? TSM_CITY : TSM_FIXED;

			HandleButtonClick(w, 5);
			_generating_world = true;
			t = CreateRandomTown(20, mode, size);
			_generating_world = false;

			if (t == NULL) {
				ShowErrorMessage(STR_NO_SPACE_FOR_TOWN, STR_CANNOT_GENERATE_TOWN, 0, 0);
			} else {
				ScrollMainWindowToTile(t->xy);
			}

			break;
		}
		case 6: {// many random towns
			HandleButtonClick(w, 6);

			_generating_world = true;
			if (!GenerateTowns()) ShowErrorMessage(STR_NO_SPACE_FOR_TOWN, STR_CANNOT_GENERATE_TOWN, 0, 0);
			_generating_world = false;
			break;
		}

		case 7: case 8: case 9: case 10:
			RaiseWindowWidget(w, _scengen_town_size + 7);
			_scengen_town_size = e->we.click.widget - 7;
			LowerWindowWidget(w, _scengen_town_size + 7);
			SetWindowDirty(w);
			break;
		}
		break;

	case WE_TIMEOUT:
		RaiseWindowWidget(w, 5);
		RaiseWindowWidget(w, 6);
		SetWindowDirty(w);
		break;
	case WE_PLACE_OBJ:
		_place_proc(e->we.place.tile);
		break;
	case WE_ABORT_PLACE_OBJ:
		RaiseWindowButtons(w);
		LowerWindowWidget(w, _scengen_town_size + 7);
		SetWindowDirty(w);
		break;
	}
}

static const WindowDesc _scen_edit_town_gen_desc = {
	WDP_AUTO, WDP_AUTO, 160, 95, 160, 95,
	WC_SCEN_TOWN_GEN, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_scen_edit_town_gen_widgets,
	ScenEditTownGenWndProc,
};

static void ToolbarScenGenTown(Window *w)
{
	HandleButtonClick(w, 12);
	SndPlayFx(SND_15_BEEP);

	AllocateWindowDescFront(&_scen_edit_town_gen_desc, 0);
}

static void ToolbarScenGenIndustry(Window *w)
{
	HandleButtonClick(w, 13);
	SndPlayFx(SND_15_BEEP);
	ShowBuildIndustryWindow();
}

static void ToolbarScenBuildRoad(Window *w)
{
	HandleButtonClick(w, 14);
	SndPlayFx(SND_15_BEEP);
	ShowBuildRoadScenToolbar();
}

static void ToolbarScenPlantTrees(Window *w)
{
	HandleButtonClick(w, 15);
	SndPlayFx(SND_15_BEEP);
	ShowBuildTreesScenToolbar();
}

static void ToolbarScenPlaceSign(Window *w)
{
	HandleButtonClick(w, 16);
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

static void MainToolbarWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT:
		/* Draw brown-red toolbar bg. */
		GfxFillRect(0, 0, w->width-1, w->height-1, 0xB2);
		GfxFillRect(0, 0, w->width-1, w->height-1, 0xB4 | (1 << PALETTE_MODIFIER_GREYOUT));

		/* If spectator, disable all construction buttons
		 * ie : Build road, rail, ships, airports and landscaping
		 * Since enabled state is the default, just disable when needed */
		SetWindowWidgetsDisabledState(w, _current_player == PLAYER_SPECTATOR, 19, 20, 21, 22, 23, WIDGET_LIST_END);
		/* disable company list drop downs, if there are no companies */
		SetWindowWidgetsDisabledState(w, ActivePlayerCount() == 0, 7, 8, 13, 14, 15, 16, WIDGET_LIST_END);

		SetWindowWidgetDisabledState(w, 19, !CanBuildVehicleInfrastructure(VEH_TRAIN));
		SetWindowWidgetDisabledState(w, 22, !CanBuildVehicleInfrastructure(VEH_AIRCRAFT));

		DrawWindowWidgets(w);
		break;

	case WE_CLICK: {
		if (_game_mode != GM_MENU && !IsWindowWidgetDisabled(w, e->we.click.widget))
			_toolbar_button_procs[e->we.click.widget](w);
	} break;

	case WE_KEYPRESS: {
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
	} break;

	case WE_PLACE_OBJ: {
		_place_proc(e->we.place.tile);
	} break;

	case WE_ABORT_PLACE_OBJ: {
		RaiseWindowWidget(w, 25);
		SetWindowDirty(w);
	} break;

	case WE_MOUSELOOP:
		if (IsWindowWidgetLowered(w, 0) != !!_pause_game) {
			ToggleWidgetLoweredState(w, 0);
			InvalidateWidget(w, 0);
		}

		if (IsWindowWidgetLowered(w, 1) != !!_fast_forward) {
			ToggleWidgetLoweredState(w, 1);
			InvalidateWidget(w, 1);
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

	case WE_TIMEOUT: {
		uint i;
		for (i = 2; i < w->widget_count; i++) {
			if (IsWindowWidgetLowered(w, i)) {
				RaiseWindowWidget(w, i);
				InvalidateWidget(w, i);
			}
		}
		break;
	}

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
		SetWindowWidgetDisabledState(w, 6, _patches_newgame.starting_year <= MIN_YEAR);
		SetWindowWidgetDisabledState(w, 7, _patches_newgame.starting_year >= MAX_YEAR);

		/* Draw brown-red toolbar bg. */
		GfxFillRect(0, 0, w->width-1, w->height-1, 0xB2);
		GfxFillRect(0, 0, w->width-1, w->height-1, 0xB4 | (1 << PALETTE_MODIFIER_GREYOUT));

		DrawWindowWidgets(w);

		SetDParam(0, ConvertYMDToDate(_patches_newgame.starting_year, 0, 1));
		DrawStringCenteredTruncated(w->widget[6].right, w->widget[7].left, 6, STR_00AF, TC_FROMSTRING);

		/* We hide this panel when the toolbar space gets too small */
		if (w->widget[4].left != w->widget[4].right) {
			DrawStringCenteredTruncated(w->widget[4].left + 1, w->widget[4].right - 1,  1, STR_0221_OPENTTD, TC_FROMSTRING);
			DrawStringCenteredTruncated(w->widget[4].left + 1, w->widget[4].right - 1, 11, STR_0222_SCENARIO_EDITOR, TC_FROMSTRING);
		}

		break;

	case WE_CLICK: {
		if (_game_mode == GM_MENU) return;
		_scen_toolbar_button_procs[e->we.click.widget](w);
	} break;

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
			case 'L': ShowEditorTerraformToolBar(); break;
			default: return;
		}
		e->we.keypress.cont = false;
		break;

	case WE_PLACE_OBJ: {
		_place_proc(e->we.place.tile);
	} break;

	case WE_ABORT_PLACE_OBJ: {
		RaiseWindowWidget(w, 25);
		SetWindowDirty(w);
	} break;

	case WE_RESIZE: {
		/* There are 15 buttons plus some spacings if the space allows it.
		 * Furthermore there are two panels of which one is non-essential
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
		if (IsWindowWidgetLowered(w, 0) != !!_pause_game) {
			ToggleWidgetLoweredState(w, 0);
			SetWindowDirty(w);
		}

		if (IsWindowWidgetLowered(w, 1) != !!_fast_forward) {
			ToggleWidgetLoweredState(w, 1);
			SetWindowDirty(w);
		}
		break;

		case WE_MESSAGE:
			HandleZoomMessage(w, FindWindowById(WC_MAIN_WINDOW, 0)->viewport, 9, 10);
			break;
	}
}

static const WindowDesc _toolb_scen_desc = {
	0, 0, 130, 22, 640, 22,
	WC_MAIN_TOOLBAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_toolb_scen_widgets,
	ScenEditToolbarWndProc
};

extern GetNewsStringCallbackProc * const _get_news_string_callback[];


static bool DrawScrollingStatusText(const NewsItem *ni, int pos, int width)
{
	char buf[512];
	StringID str;
	const char *s;
	char *d;
	DrawPixelInfo tmp_dpi, *old_dpi;
	int x;
	char buffer[256];

	if (ni->display_mode == 3) {
		str = _get_news_string_callback[ni->callback](ni);
	} else {
		CopyInDParam(0, ni->params, lengthof(ni->params));
		str = ni->string_id;
	}

	GetString(buf, str, lastof(buf));

	s = buf;
	d = buffer;

	for (;;) {
		WChar c = Utf8Consume(&s);
		if (c == 0) {
			*d = '\0';
			break;
		} else if (*s == 0x0D) {
			d[0] = d[1] = d[2] = d[3] = ' ';
			d += 4;
		} else if (IsPrintable(c)) {
			d += Utf8Encode(d, c);
		}
	}

	if (!FillDrawPixelInfo(&tmp_dpi, 141, 1, width, 11)) return true;

	old_dpi = _cur_dpi;
	_cur_dpi = &tmp_dpi;

	x = DoDrawString(buffer, pos, 0, TC_LIGHT_BLUE);
	_cur_dpi = old_dpi;

	return x > 0;
}

static void StatusBarWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		const Player *p = (_local_player == PLAYER_SPECTATOR) ? NULL : GetPlayer(_local_player);

		DrawWindowWidgets(w);
		SetDParam(0, _date);
		DrawStringCentered(
			70, 1, (_pause_game || _patches.status_long_date) ? STR_00AF : STR_00AE, TC_FROMSTRING
		);

		if (p != NULL) {
			/* Draw player money */
			SetDParam(0, p->player_money);
			DrawStringCentered(w->widget[2].left + 70, 1, STR_0004, TC_FROMSTRING);
		}

		/* Draw status bar */
		if (w->message.msg) { // true when saving is active
			DrawStringCenteredTruncated(w->widget[1].left + 1, w->widget[1].right - 1, 1, STR_SAVING_GAME, TC_FROMSTRING);
		} else if (_do_autosave) {
			DrawStringCenteredTruncated(w->widget[1].left + 1, w->widget[1].right - 1, 1, STR_032F_AUTOSAVE, TC_FROMSTRING);
		} else if (_pause_game) {
			DrawStringCenteredTruncated(w->widget[1].left + 1, w->widget[1].right - 1, 1, STR_0319_PAUSED, TC_FROMSTRING);
		} else if (WP(w,def_d).data_1 > -1280 && FindWindowById(WC_NEWS_WINDOW,0) == NULL && _statusbar_news_item.string_id != 0) {
			/* Draw the scrolling news text */
			if (!DrawScrollingStatusText(&_statusbar_news_item, WP(w,def_d).data_1, w->widget[1].right - w->widget[1].left - 2)) {
				WP(w,def_d).data_1 = -1280;
				if (p != NULL) {
					/* This is the default text */
					SetDParam(0, p->index);
					DrawStringCenteredTruncated(w->widget[1].left + 1, w->widget[1].right - 1, 1, STR_02BA, TC_FROMSTRING);
				}
			}
		} else {
			if (p != NULL) {
				/* This is the default text */
				SetDParam(0, p->index);
				DrawStringCenteredTruncated(w->widget[1].left + 1, w->widget[1].right - 1, 1, STR_02BA, TC_FROMSTRING);
			}
		}

		if (WP(w, def_d).data_2 > 0) DrawSprite(SPR_BLOT, PALETTE_TO_RED, w->widget[1].right - 11, 2);
	} break;

	case WE_MESSAGE:
		w->message.msg = e->we.message.msg;
		SetWindowDirty(w);
		break;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 1: ShowLastNewsMessage(); break;
			case 2: if (_local_player != PLAYER_SPECTATOR) ShowPlayerFinances(_local_player); break;
			default: ResetObjectToPlace();
		}
		break;

	case WE_TICK: {
		if (_pause_game) return;

		if (WP(w, def_d).data_1 > -1280) { // Scrolling text
			WP(w, def_d).data_1 -= 2;
			InvalidateWidget(w, 1);
		}

		if (WP(w, def_d).data_2 > 0) { // Red blot to show there are new unread newsmessages
			WP(w, def_d).data_2 -= 2;
		} else if (WP(w, def_d).data_2 < 0) {
			WP(w, def_d).data_2 = 0;
			InvalidateWidget(w, 1);
		}

		break;
	}
	}
}

static const Widget _main_status_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   139,     0,    11, 0x0, STR_NULL},
{    WWT_PUSHBTN,   RESIZE_RIGHT,   14,   140,   179,     0,    11, 0x0, STR_02B7_SHOW_LAST_MESSAGE_OR_NEWS},
{    WWT_PUSHBTN,   RESIZE_LR,      14,   180,   319,     0,    11, 0x0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _main_status_desc = {
	WDP_CENTER, 0, 320, 12, 640, 12,
	WC_STATUS_BAR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_main_status_widgets,
	StatusBarWndProc
};

extern void UpdateAllStationVirtCoord();

static void MainWindowWndProc(Window *w, WindowEvent *e)
{
	int off_x;

	switch (e->event) {
	case WE_PAINT:
		DrawWindowViewport(w);
		if (_game_mode == GM_MENU) {
			off_x = _screen.width / 2;

			DrawSprite(SPR_OTTD_O, PAL_NONE, off_x - 120, 50);
			DrawSprite(SPR_OTTD_P, PAL_NONE, off_x -  86, 50);
			DrawSprite(SPR_OTTD_E, PAL_NONE, off_x -  53, 50);
			DrawSprite(SPR_OTTD_N, PAL_NONE, off_x -  22, 50);

			DrawSprite(SPR_OTTD_T, PAL_NONE, off_x +  34, 50);
			DrawSprite(SPR_OTTD_T, PAL_NONE, off_x +  65, 50);
			DrawSprite(SPR_OTTD_D, PAL_NONE, off_x +  96, 50);

			/*
			DrawSprite(SPR_OTTD_R, off_x + 119, 50);
			DrawSprite(SPR_OTTD_A, off_x + 148, 50);
			DrawSprite(SPR_OTTD_N, off_x + 181, 50);
			DrawSprite(SPR_OTTD_S, off_x + 215, 50);
			DrawSprite(SPR_OTTD_P, off_x + 246, 50);
			DrawSprite(SPR_OTTD_O, off_x + 275, 50);
			DrawSprite(SPR_OTTD_R, off_x + 307, 50);
			DrawSprite(SPR_OTTD_T, off_x + 337, 50);

			DrawSprite(SPR_OTTD_T, off_x + 390, 50);
			DrawSprite(SPR_OTTD_Y, off_x + 417, 50);
			DrawSprite(SPR_OTTD_C, off_x + 447, 50);
			DrawSprite(SPR_OTTD_O, off_x + 478, 50);
			DrawSprite(SPR_OTTD_O, off_x + 509, 50);
			DrawSprite(SPR_OTTD_N, off_x + 541, 50);
			*/
		}
		break;

	case WE_KEYPRESS:
		switch (e->we.keypress.keycode) {
			case 'Q' | WKC_CTRL:
			case 'Q' | WKC_META:
				HandleExitGameRequest();
				break;
		}

		/* Disable all key shortcuts, except quit shortcuts when
		 * generating the world, otherwise they create threading
		 * problem during the generating, resulting in random
		 * assertions that are hard to trigger and debug */
		if (IsGeneratingWorld()) break;

		if (e->we.keypress.keycode == WKC_BACKQUOTE) {
			IConsoleSwitch();
			e->we.keypress.cont = false;
			break;
		}

		if (e->we.keypress.keycode == ('B' | WKC_CTRL)) {
			e->we.keypress.cont = false;
			_draw_bounding_boxes = !_draw_bounding_boxes;
			MarkWholeScreenDirty();
			break;
		}

		if (_game_mode == GM_MENU) break;

		switch (e->we.keypress.keycode) {
			case 'C':
			case 'Z': {
				Point pt = GetTileBelowCursor();
				if (pt.x != -1) {
					ScrollMainWindowTo(pt.x, pt.y);
					if (e->we.keypress.keycode == 'Z') MaxZoomInOut(ZOOM_IN, w);
				}
				break;
			}

			case WKC_ESC: ResetObjectToPlace(); break;
			case WKC_DELETE: DeleteNonVitalWindows(); break;
			case WKC_DELETE | WKC_SHIFT: DeleteAllNonVitalWindows(); break;
			case 'R' | WKC_CTRL: MarkWholeScreenDirty(); break;

#if defined(_DEBUG)
			case '0' | WKC_ALT: // Crash the game
				*(byte*)0 = 0;
				break;

			case '1' | WKC_ALT: // Gimme money
				/* Server can not cheat in advertise mode either! */
				if (!_networking || !_network_server || !_network_advertise)
					DoCommandP(0, 10000000, 0, NULL, CMD_MONEY_CHEAT);
				break;

			case '2' | WKC_ALT: // Update the coordinates of all station signs
				UpdateAllStationVirtCoord();
				break;
#endif

			case '1' | WKC_CTRL:
			case '2' | WKC_CTRL:
			case '3' | WKC_CTRL:
			case '4' | WKC_CTRL:
			case '5' | WKC_CTRL:
			case '6' | WKC_CTRL:
			case '7' | WKC_CTRL:
				/* Transparency toggle hot keys */
				TOGGLEBIT(_transparent_opt, e->we.keypress.keycode - ('1' | WKC_CTRL));
				MarkWholeScreenDirty();
				break;

			case 'X' | WKC_CTRL:
				ShowTransparencyToolbar();
				break;

			case 'X':
				ToggleTransparency();
				break;

#ifdef ENABLE_NETWORK
			case WKC_RETURN: case 'T': // smart chat; send to team if any, otherwise to all
				if (_networking) {
					const NetworkClientInfo *cio = NetworkFindClientInfoFromIndex(_network_own_client_index);
					bool teamchat = false;

					if (cio == NULL) break;

					/* Only players actually playing can speak to team. Eg spectators cannot */
					if (_patches.prefer_teamchat && IsValidPlayer(cio->client_playas)) {
						const NetworkClientInfo *ci;
						FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
							if (ci->client_playas == cio->client_playas && ci != cio) {
								teamchat = true;
								break;
							}
						}
					}

					ShowNetworkChatQueryWindow(teamchat ? DESTTYPE_TEAM : DESTTYPE_BROADCAST, cio->client_playas);
				}
				break;

			case WKC_SHIFT | WKC_RETURN: case WKC_SHIFT | 'T': // send text message to all players
				if (_networking) ShowNetworkChatQueryWindow(DESTTYPE_BROADCAST, 0);
				break;

			case WKC_CTRL | WKC_RETURN: case WKC_CTRL | 'T': // send text to all team mates
				if (_networking) {
					const NetworkClientInfo *cio = NetworkFindClientInfoFromIndex(_network_own_client_index);
					if (cio == NULL) break;

					ShowNetworkChatQueryWindow(DESTTYPE_TEAM, cio->client_playas);
				}
				break;
#endif

			default: return;
		}
		e->we.keypress.cont = false;
		break;

		case WE_SCROLL: {
			ViewPort *vp = IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y);

			if (vp == NULL) {
				_cursor.fix_at = false;
				_scrolling_viewport = false;
			}

			WP(w, vp_d).scrollpos_x += ScaleByZoom(e->we.scroll.delta.x, vp->zoom);
			WP(w, vp_d).scrollpos_y += ScaleByZoom(e->we.scroll.delta.y, vp->zoom);
			WP(w, vp_d).dest_scrollpos_x = WP(w, vp_d).scrollpos_x;
			WP(w, vp_d).dest_scrollpos_y = WP(w, vp_d).scrollpos_y;
		} break;

		case WE_MOUSEWHEEL:
			ZoomInOrOutToCursorWindow(e->we.wheel.wheel < 0, w);
			break;

		case WE_MESSAGE:
			/* Forward the message to the appropiate toolbar (ingame or scenario editor) */
			SendWindowMessage(WC_MAIN_TOOLBAR, 0, e->we.message.msg, e->we.message.wparam, e->we.message.lparam);
			break;
	}
}


void ShowSelectGameWindow();

void SetupColorsAndInitialWindow()
{
	uint i;
	Window *w;
	int width, height;

	for (i = 0; i != 16; i++) {
		const byte *b = GetNonSprite(PALETTE_RECOLOR_START + i);

		assert(b);
		memcpy(_colour_gradient[i], b + 0xC6, sizeof(_colour_gradient[i]));
	}

	width = _screen.width;
	height = _screen.height;

	w = AllocateWindow(0, 0, width, height, MainWindowWndProc, WC_MAIN_WINDOW, NULL);
	AssignWindowViewport(w, 0, 0, width, height, TileXY(32, 32), ZOOM_LVL_VIEWPORT);

	/* XXX: these are not done */
	switch (_game_mode) {
		default: NOT_REACHED();
		case GM_MENU:
			ShowSelectGameWindow();
			break;

		case GM_NORMAL:
		case GM_EDITOR:
			ShowVitalWindows();
			break;
	}
}

void ShowVitalWindows()
{
	Window *w;

	w = AllocateWindowDesc((_game_mode != GM_EDITOR) ? &_toolb_normal_desc : &_toolb_scen_desc);
	DoZoomInOutWindow(ZOOM_NONE, w);

	CLRBITS(w->flags4, WF_WHITE_BORDER_MASK);

	SetWindowWidgetDisabledState(w, 0, _networking && !_network_server); // if not server, disable pause button
	SetWindowWidgetDisabledState(w, 1, _networking); // if networking, disable fast-forward button

	/* 'w' is for sure a WC_MAIN_TOOLBAR */
	PositionMainToolbar(w);

	/* Status bad only for normal games */
	if (_game_mode == GM_EDITOR) return;

	_main_status_desc.top = _screen.height - 12;
	w = AllocateWindowDesc(&_main_status_desc);
	CLRBITS(w->flags4, WF_WHITE_BORDER_MASK);

	WP(w,def_d).data_1 = -1280;
}

void GameSizeChanged()
{
	_cur_resolution[0] = _screen.width;
	_cur_resolution[1] = _screen.height;
	RelocateAllWindows(_screen.width, _screen.height);
	ScreenSizeChanged();
	MarkWholeScreenDirty();
}

void InitializeMainGui()
{
	/* Clean old GUI values */
	_last_built_railtype = RAILTYPE_RAIL;
	_last_built_roadtype = ROADTYPE_ROAD;
}




