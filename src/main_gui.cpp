/* $Id$ */

/** @file main_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "heightmap.h"
#include "currency.h"
#include "spritecache.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "command_func.h"
#include "news_func.h"
#include "town.h"
#include "console.h"
#include "signs.h"
#include "waypoint.h"
#include "variables.h"
#include "train.h"
#include "roadveh.h"
#include "bridge_map.h"
#include "screenshot.h"
#include "genworld.h"
#include "vehicle_gui.h"
#include "transparency_gui.h"
#include "newgrf_config.h"
#include "rail_gui.h"
#include "road_gui.h"
#include "date_func.h"
#include "functions.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "fios.h"
#include "terraform_gui.h"
#include "industry.h"
#include "transparency.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "string_func.h"
#include "player_base.h"
#include "player_func.h"
#include "player_gui.h"
#include "settings_type.h"
#include "toolbar_gui.h"

#include "network/network.h"
#include "network/network_data.h"
#include "network/network_client.h"
#include "network/network_server.h"
#include "network/network_gui.h"

#include "table/sprites.h"
#include "table/strings.h"

static int _rename_id = 1;
static int _rename_what = -1;

RailType _last_built_railtype;
RoadType _last_built_roadtype;
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

		uint32 money_c = Clamp(ClampToI32(money), 0, 20000000); // Clamp between 20 million and 0

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
bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, ViewportHighlightMode mode, PlaceProc *placeproc)
{
	if (w->IsWidgetDisabled(widget)) return false;

	SndPlayFx(SND_15_BEEP);
	SetWindowDirty(w);

	if (w->IsWidgetLowered(widget)) {
		ResetObjectToPlace();
		return false;
	}

	SetObjectToPlace(cursor, PAL_NONE, mode, w->window_class, w->window_number);
	w->LowerWidget(widget);
	_place_proc = placeproc;
	return true;
}


void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_12_EXPLOSION, tile);
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

void MenuClickSaveLoad(int index)
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

void SelectSignTool()
{
	if (_cursor.sprite == SPR_CURSOR_SIGN) {
		ResetObjectToPlace();
	} else {
		SetObjectToPlace(SPR_CURSOR_SIGN, PAL_NONE, VHM_RECT, WC_MAIN_TOOLBAR, 0);
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

void MenuClickSmallScreenshot()
{
	SetScreenshotType(SC_VIEWPORT);
}

void MenuClickWorldScreenshot()
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
			SetWindowDirty(v);
			return;
		}

	case WE_POPUPMENU_SELECT: {
		int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);
		int action_id;


		if (index < 0) {
			Window *w2 = FindWindowById(WC_MAIN_TOOLBAR,0);
			if (GetWidgetFromPos(w2, e->we.popupmenu.pt.x - w2->left, e->we.popupmenu.pt.y - w2->top) == WP(w, menu_d).main_button)
				index = WP(w, menu_d).sel_index;
		}

		action_id = WP(w, menu_d).action_id;
		DeleteWindow(w);

		if (index >= 0) {
			assert((uint)index <= lengthof(_menu_clicked_procs));
			_menu_clicked_procs[action_id](index);
		}

		break;
		}

	case WE_POPUPMENU_OVER: {
		int index = GetMenuItemIndex(w, e->we.popupmenu.pt.x, e->we.popupmenu.pt.y);

		if (index == -1 || index == WP(w, menu_d).sel_index) return;

		WP(w, menu_d).sel_index = index;
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
	if (_networking && WP(w, menu_d).main_button == 9) num++;

	if (WP(w, menu_d).item_count != num) {
		WP(w, menu_d).item_count = num;
		SetWindowDirty(w);
		num = num * 10 + 2;
		w->height = num;
		w->widget[0].bottom = w->widget[0].top + num - 1;
		w->top = GetToolbarDropdownPos(0, w->width, w->height).y;
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
		sel = WP(w, menu_d).sel_index;
		chk = WP(w, menu_d).checked_items; // let this mean gray items.

		/* 9 = playerlist */
		if (_networking && WP(w, menu_d).main_button == 9) {
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
		v->RaiseWidget(WP(w, menu_d).main_button);
		SetWindowDirty(v);
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
		if (_networking && WP(w, menu_d).main_button == 9) {
			if (index > 0) index = GetPlayerIndexFromMenu(index - 1) + 1;
		} else {
			index = GetPlayerIndexFromMenu(index);
		}

		if (index == -1 || index == WP(w, menu_d).sel_index) return;

		WP(w, menu_d).sel_index = index;
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
 Window *PopupMainToolbMenu(Window *w, uint16 parent_button, StringID base_string, byte item_count, byte disabled_mask)
{
	assert(disabled_mask == 0 || item_count <= 8);
	w->LowerWidget(parent_button);
	w->InvalidateWidget(parent_button);

	DeleteWindowById(WC_TOOLBAR_MENU, 0);

	// Extend the dropdown toolbar to the longest string in the list
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

Window *PopupMainPlayerToolbMenu(Window *w, int main_button, int gray)
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
			vp->zoom = (ZoomLevel)((int)vp->zoom - 1);
			vp->virtual_width >>= 1;
			vp->virtual_height >>= 1;

			WP(w, vp_d).scrollpos_x += vp->virtual_width >> 1;
			WP(w, vp_d).scrollpos_y += vp->virtual_height >> 1;
			WP(w, vp_d).dest_scrollpos_x = WP(w,vp_d).scrollpos_x;
			WP(w, vp_d).dest_scrollpos_y = WP(w,vp_d).scrollpos_y;
			break;
		case ZOOM_OUT:
			if (vp->zoom == ZOOM_LVL_MAX) return false;
			vp->zoom = (ZoomLevel)((int)vp->zoom + 1);

			WP(w, vp_d).scrollpos_x -= vp->virtual_width >> 1;
			WP(w, vp_d).scrollpos_y -= vp->virtual_height >> 1;
			WP(w, vp_d).dest_scrollpos_x = WP(w,vp_d).scrollpos_x;
			WP(w, vp_d).dest_scrollpos_y = WP(w,vp_d).scrollpos_y;

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

void ZoomInOrOutToCursorWindow(bool in, Window *w)
{
	assert(w != NULL);

	if (_game_mode != GM_MENU) {
		ViewPort *vp = w->viewport;
		if ((in && vp->zoom == ZOOM_LVL_MIN) || (!in && vp->zoom == ZOOM_LVL_MAX))
			return;

		Point pt = GetTileZoomCenterWindow(in,w);
		if (pt.x != -1) {
			ScrollWindowTo(pt.x, pt.y, w, true);

			DoZoomInOutWindow(in ? ZOOM_IN : ZOOM_OUT, w);
		}
	}
}


extern GetNewsStringCallbackProc * const _get_news_string_callback[];


static bool DrawScrollingStatusText(const NewsItem *ni, int pos, int width)
{
	char buf[512];
	StringID str;
	const char *s, *last;
	char *d;
	DrawPixelInfo tmp_dpi, *old_dpi;
	int x;
	char buffer[256];

	if (ni->display_mode == NM_CALLBACK) {
		str = _get_news_string_callback[ni->callback](ni);
	} else {
		CopyInDParam(0, ni->params, lengthof(ni->params));
		str = ni->string_id;
	}

	GetString(buf, str, lastof(buf));

	s = buf;
	d = buffer;
	last = lastof(buffer);

	for (;;) {
		WChar c = Utf8Consume(&s);
		if (c == 0) {
			break;
		} else if (c == 0x0D) {
			if (d + 4 >= last) break;
			d[0] = d[1] = d[2] = d[3] = ' ';
			d += 4;
		} else if (IsPrintable(c)) {
			if (d + Utf8CharLen(c) >= last) break;
			d += Utf8Encode(d, c);
		}
	}
	*d = '\0';

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
		} else if (WP(w, def_d).data_1 > -1280 && FindWindowById(WC_NEWS_WINDOW,0) == NULL && _statusbar_news_item.string_id != 0) {
			/* Draw the scrolling news text */
			if (!DrawScrollingStatusText(&_statusbar_news_item, WP(w, def_d).data_1, w->widget[1].right - w->widget[1].left - 2)) {
				WP(w, def_d).data_1 = -1280;
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
			w->InvalidateWidget(1);
		}

		if (WP(w, def_d).data_2 > 0) { // Red blot to show there are new unread newsmessages
			WP(w, def_d).data_2 -= 2;
		} else if (WP(w, def_d).data_2 < 0) {
			WP(w, def_d).data_2 = 0;
			w->InvalidateWidget(1);
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
					if (e->we.keypress.keycode == 'Z') MaxZoomInOut(ZOOM_IN, w);
					ScrollMainWindowTo(pt.x, pt.y);
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
			case '8' | WKC_CTRL:
			case '9' | WKC_CTRL:
				/* Transparency toggle hot keys */
				ToggleTransparency((TransparencyOption)(e->we.keypress.keycode - ('1' | WKC_CTRL)));
				MarkWholeScreenDirty();
				break;

			case 'X' | WKC_CTRL:
				ShowTransparencyToolbar();
				break;

			case 'X':
				ResetRestoreAllTransparency();
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
	Window *w = AllocateToolbar();
	DoZoomInOutWindow(ZOOM_NONE, w);

	CLRBITS(w->flags4, WF_WHITE_BORDER_MASK);

	w->SetWidgetDisabledState(0, _networking && !_network_server); // if not server, disable pause button
	w->SetWidgetDisabledState(1, _networking); // if networking, disable fast-forward button

	/* 'w' is for sure a WC_MAIN_TOOLBAR */
	PositionMainToolbar(w);

	/* Status bad only for normal games */
	if (_game_mode == GM_EDITOR) return;

	_main_status_desc.top = _screen.height - 12;
	w = AllocateWindowDesc(&_main_status_desc);
	CLRBITS(w->flags4, WF_WHITE_BORDER_MASK);

	WP(w, def_d).data_1 = -1280;
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





