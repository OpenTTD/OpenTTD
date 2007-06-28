/* $Id$ */

#ifdef ENABLE_NETWORK
#include "stdafx.h"
#include "openttd.h"
#include "string.h"
#include "strings.h"
#include "table/sprites.h"
#include "network.h"
#include "date.h"

#include "fios.h"
#include "table/strings.h"
#include "functions.h"
#include "network_data.h"
#include "network_client.h"
#include "network_gui.h"
#include "network_gamelist.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "command.h"
#include "variables.h"
#include "network_server.h"
#include "network_udp.h"
#include "settings.h"
#include "string.h"
#include "town.h"
#include "newgrf.h"

#define BGC 5
#define BTC 15

typedef struct network_d {
	PlayerID company;        // select company in network lobby
	byte field;              // select text-field in start-server and game-listing
	NetworkGameList *server; // selected server in lobby and game-listing
	FiosItem *map;           // selected map in start-server
} network_d;
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(network_d));

typedef struct network_ql_d {
	network_d n;                 // see above; general stuff
	querystr_d q;                // text-input in start-server and game-listing
	NetworkGameList **sort_list; // list of games (sorted)
	list_d l;                    // accompanying list-administration
} network_ql_d;
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(network_ql_d));

/* Global to remember sorting after window has been closed */
static Listing _ng_sorting;

static char _edit_str_buf[150];
static bool _chat_tab_completion_active;

static void ShowNetworkStartServerWindow(void);
static void ShowNetworkLobbyWindow(NetworkGameList *ngl);
extern void SwitchMode(int new_mode);

static const StringID _connection_types_dropdown[] = {
	STR_NETWORK_LAN_INTERNET,
	STR_NETWORK_INTERNET_ADVERTISE,
	INVALID_STRING_ID
};

static const StringID _lan_internet_types_dropdown[] = {
	STR_NETWORK_LAN,
	STR_NETWORK_INTERNET,
	INVALID_STRING_ID
};

static const StringID _players_dropdown[] = {
	STR_NETWORK_0_PLAYERS,
	STR_NETWORK_1_PLAYERS,
	STR_NETWORK_2_PLAYERS,
	STR_NETWORK_3_PLAYERS,
	STR_NETWORK_4_PLAYERS,
	STR_NETWORK_5_PLAYERS,
	STR_NETWORK_6_PLAYERS,
	STR_NETWORK_7_PLAYERS,
	STR_NETWORK_8_PLAYERS,
	STR_NETWORK_9_PLAYERS,
	STR_NETWORK_10_PLAYERS,
	INVALID_STRING_ID
};

static const StringID _language_dropdown[] = {
	STR_NETWORK_LANG_ANY,
	STR_NETWORK_LANG_ENGLISH,
	STR_NETWORK_LANG_GERMAN,
	STR_NETWORK_LANG_FRENCH,
	INVALID_STRING_ID
};

enum {
	NET_PRC__OFFSET_TOP_WIDGET          = 54,
	NET_PRC__OFFSET_TOP_WIDGET_COMPANY  = 52,
	NET_PRC__SIZE_OF_ROW                = 14,
};

/** Update the network new window because a new server is
 * found on the network.
 * @param unselect unselect the currently selected item */
void UpdateNetworkGameWindow(bool unselect)
{
	SendWindowMessage(WC_NETWORK_WINDOW, 0, unselect, 0, 0);
}

static bool _internal_sort_order; // Used for Qsort order-flipping
typedef int CDECL NGameNameSortFunction(const void*, const void*);

/** Qsort function to sort by name. */
static int CDECL NGameNameSorter(const void *a, const void *b)
{
	const NetworkGameList *cmp1 = *(const NetworkGameList**)a;
	const NetworkGameList *cmp2 = *(const NetworkGameList**)b;
	int r = strcasecmp(cmp1->info.server_name, cmp2->info.server_name);

	return _internal_sort_order ? -r : r;
}

/** Qsort function to sort by the amount of clients online on a
 * server. If the two servers have the same amount, the one with the
 * higher maximum is preferred. */
static int CDECL NGameClientSorter(const void *a, const void *b)
{
	const NetworkGameList *cmp1 = *(const NetworkGameList**)a;
	const NetworkGameList *cmp2 = *(const NetworkGameList**)b;
	/* Reverse as per default we are interested in most-clients first */
	int r = cmp1->info.clients_on - cmp2->info.clients_on;

	if (r == 0) r = cmp1->info.clients_max - cmp2->info.clients_max;
	if (r == 0) r = strcasecmp(cmp1->info.server_name, cmp2->info.server_name);

	return _internal_sort_order ? -r : r;
}

/** Qsort function to sort by joinability. If both servers are the
 * same, prefer the non-passworded server first. */
static int CDECL NGameAllowedSorter(const void *a, const void *b)
{
	const NetworkGameList *cmp1 = *(const NetworkGameList**)a;
	const NetworkGameList *cmp2 = *(const NetworkGameList**)b;

	/* The servers we do not know anything about (the ones that did not reply) should be at the bottom) */
	int r = (cmp1->info.server_revision[0] == '\0') - (cmp2->info.server_revision[0] == '\0');

	/* Reverse default as we are interested in version-compatible clients first */
	if (r == 0) r = cmp2->info.version_compatible - cmp1->info.version_compatible;
	/* The version-compatible ones are then sorted with NewGRF compatible first, incompatible last */
	if (r == 0) r = cmp2->info.compatible - cmp1->info.compatible;
	/* Passworded servers should be below unpassworded servers */
	if (r == 0) r = cmp1->info.use_password - cmp2->info.use_password;
	/* Finally sort on the name of the server */
	if (r == 0) r = strcasecmp(cmp1->info.server_name, cmp2->info.server_name);

	return _internal_sort_order ? -r : r;
}

/** (Re)build the network game list as its amount has changed because
 * an item has been added or deleted for example
 * @param ngl list_d struct that contains all necessary information for sorting */
static void BuildNetworkGameList(network_ql_d *nqld)
{
	NetworkGameList *ngl_temp;
	uint n = 0;

	if (!(nqld->l.flags & VL_REBUILD)) return;

	/* Count the number of games in the list */
	for (ngl_temp = _network_game_list; ngl_temp != NULL; ngl_temp = ngl_temp->next) n++;
	if (n == 0) return;

	/* Create temporary array of games to use for listing */
	free(nqld->sort_list);
	nqld->sort_list = malloc(n * sizeof(nqld->sort_list[0]));
	if (nqld->sort_list == NULL) error("Could not allocate memory for the network-game-sorting-list");
	nqld->l.list_length = n;

	for (n = 0, ngl_temp = _network_game_list; ngl_temp != NULL; ngl_temp = ngl_temp->next) {
		nqld->sort_list[n++] = ngl_temp;
	}

	/* Force resort */
	nqld->l.flags &= ~VL_REBUILD;
	nqld->l.flags |= VL_RESORT;
}

static void SortNetworkGameList(network_ql_d *nqld)
{
	static NGameNameSortFunction * const ngame_sorter[] = {
		&NGameNameSorter,
		&NGameClientSorter,
		&NGameAllowedSorter
	};

	NetworkGameList *item;
	uint i;

	if (!(nqld->l.flags & VL_RESORT)) return;
	if (nqld->l.list_length == 0) return;

	_internal_sort_order = !!(nqld->l.flags & VL_DESC);
	qsort(nqld->sort_list, nqld->l.list_length, sizeof(nqld->sort_list[0]), ngame_sorter[nqld->l.sort_type]);

	/* After sorting ngl->sort_list contains the sorted items. Put these back
	 * into the original list. Basically nothing has changed, we are only
	 * shuffling the ->next pointers */
	_network_game_list = nqld->sort_list[0];
	for (item = _network_game_list, i = 1; i != nqld->l.list_length; i++) {
		item->next = nqld->sort_list[i];
		item = item->next;
	}
	item->next = NULL;

	nqld->l.flags &= ~VL_RESORT;
}

/* Uses network_ql_d (network_d, querystr_d and list_d) WP macro */
static void NetworkGameWindowWndProc(Window *w, WindowEvent *e)
{
	network_d *nd = &WP(w, network_ql_d).n;
	list_d *ld = &WP(w, network_ql_d).l;

	switch (e->event) {
	case WE_CREATE: /* Focus input box */
		nd->field = 3;
		nd->server = NULL;

		WP(w, network_ql_d).sort_list = NULL;
		ld->flags = VL_REBUILD | (_ng_sorting.order << (VL_DESC - 1));
		ld->sort_type = _ng_sorting.criteria;
		break;

	case WE_PAINT: {
		const NetworkGameList *sel = nd->server;
		const char *arrow = (ld->flags & VL_DESC) ? DOWNARROW : UPARROW;

		if (ld->flags & VL_REBUILD) {
			BuildNetworkGameList(&WP(w, network_ql_d));
			SetVScrollCount(w, ld->list_length);
		}
		if (ld->flags & VL_RESORT) SortNetworkGameList(&WP(w, network_ql_d));

		SetWindowWidgetDisabledState(w, 17, sel == NULL);
		/* Join Button disabling conditions */
		SetWindowWidgetDisabledState(w, 16, sel == NULL || // no Selected Server
				!sel->online || // Server offline
				sel->info.clients_on >= sel->info.clients_max || // Server full
				!sel->info.compatible); // Revision mismatch

		SetWindowWidgetHiddenState(w, 18, sel == NULL ||
				!sel->online ||
				sel->info.grfconfig == NULL);

		SetDParam(0, 0x00);
		SetDParam(7, _lan_internet_types_dropdown[_network_lan_internet]);
		DrawWindowWidgets(w);

		DrawEditBox(w, &WP(w, network_ql_d).q, 3);

		DrawString(9, 23, STR_NETWORK_CONNECTION, 2);
		DrawString(210, 23, STR_NETWORK_PLAYER_NAME, 2);

		/* Sort based on widgets: name, clients, compatibility */
		switch (ld->sort_type) {
			case 6 - 6: DoDrawString(arrow, w->widget[6].right - 10, 42, 0x10); break;
			case 7 - 6: DoDrawString(arrow, w->widget[7].right - 10, 42, 0x10); break;
			case 8 - 6: DoDrawString(arrow, w->widget[8].right - 10, 42, 0x10); break;
		}

		{ // draw list of games
			uint16 y = NET_PRC__OFFSET_TOP_WIDGET + 3;
			int32 n = 0;
			int32 pos = w->vscroll.pos;
			uint max_name_width = w->widget[6].right - w->widget[6].left - 5;
			const NetworkGameList *cur_item = _network_game_list;

			while (pos > 0 && cur_item != NULL) {
				pos--;
				cur_item = cur_item->next;
			}

			while (cur_item != NULL) {
				// show highlighted item with a different colour
				if (cur_item == sel) GfxFillRect(w->widget[6].left + 1, y - 2, w->widget[8].right - 1, y + 9, 10);

				SetDParamStr(0, cur_item->info.server_name);
				DrawStringTruncated(w->widget[6].left + 5, y, STR_02BD, 16, max_name_width);

				SetDParam(0, cur_item->info.clients_on);
				SetDParam(1, cur_item->info.clients_max);
				SetDParam(2, cur_item->info.companies_on);
				SetDParam(3, cur_item->info.companies_max);
				DrawStringCentered(210, y, STR_NETWORK_GENERAL_ONLINE, 2);

				// only draw icons if the server is online
				if (cur_item->online) {
					// draw a lock if the server is password protected.
					if (cur_item->info.use_password) DrawSprite(SPR_LOCK, w->widget[8].left + 5, y - 1);

					// draw red or green icon, depending on compatibility with server.
					DrawSprite(SPR_BLOT | (cur_item->info.compatible ? PALETTE_TO_GREEN : (cur_item->info.version_compatible ? PALETTE_TO_YELLOW : PALETTE_TO_RED)), w->widget[8].left + 15, y);

					// draw flag according to server language
					DrawSprite(SPR_FLAGS_BASE + cur_item->info.server_lang, w->widget[8].left + 25, y);
				}

				cur_item = cur_item->next;
				y += NET_PRC__SIZE_OF_ROW;
				if (++n == w->vscroll.cap) break; // max number of games in the window
			}
		}

		/* Draw the right menu */
		GfxFillRect(311, 43, 539, 92, 157);
		if (sel == NULL) {
			DrawStringCentered(425, 58, STR_NETWORK_GAME_INFO, 0);
		} else if (!sel->online) {
			SetDParamStr(0, sel->info.server_name);
			DrawStringCentered(425, 68, STR_ORANGE, 0); // game name

			DrawStringCentered(425, 132, STR_NETWORK_SERVER_OFFLINE, 0); // server offline
		} else { // show game info
			uint16 y = 100;
			const uint16 x = w->widget[15].left + 5;

			DrawStringCentered(425, 48, STR_NETWORK_GAME_INFO, 0);


			SetDParamStr(0, sel->info.server_name);
			DrawStringCenteredTruncated(w->widget[15].left, w->widget[15].right, 62, STR_ORANGE, 16); // game name

			SetDParamStr(0, sel->info.map_name);
			DrawStringCenteredTruncated(w->widget[15].left, w->widget[15].right, 74, STR_02BD, 16); // map name

			SetDParam(0, sel->info.clients_on);
			SetDParam(1, sel->info.clients_max);
			SetDParam(2, sel->info.companies_on);
			SetDParam(3, sel->info.companies_max);
			DrawString(x, y, STR_NETWORK_CLIENTS, 2);
			y += 10;

			SetDParam(0, _language_dropdown[sel->info.server_lang]);
			DrawString(x, y, STR_NETWORK_LANGUAGE, 2); // server language
			y += 10;

			SetDParam(0, STR_TEMPERATE_LANDSCAPE + sel->info.map_set);
			DrawString(x, y, STR_NETWORK_TILESET, 2); // tileset
			y += 10;

			SetDParam(0, sel->info.map_width);
			SetDParam(1, sel->info.map_height);
			DrawString(x, y, STR_NETWORK_MAP_SIZE, 2); // map size
			y += 10;

			SetDParamStr(0, sel->info.server_revision);
			DrawString(x, y, STR_NETWORK_SERVER_VERSION, 2); // server version
			y += 10;

			SetDParamStr(0, sel->info.hostname);
			SetDParam(1, sel->port);
			DrawString(x, y, STR_NETWORK_SERVER_ADDRESS, 2); // server address
			y += 10;

			SetDParam(0, sel->info.start_date);
			DrawString(x, y, STR_NETWORK_START_DATE, 2); // start date
			y += 10;

			SetDParam(0, sel->info.game_date);
			DrawString(x, y, STR_NETWORK_CURRENT_DATE, 2); // current date
			y += 10;

			y += 2;

			if (!sel->info.compatible) {
				DrawStringCentered(425, y, sel->info.version_compatible ? STR_NETWORK_GRF_MISMATCH : STR_NETWORK_VERSION_MISMATCH, 0); // server mismatch
			} else if (sel->info.clients_on == sel->info.clients_max) {
				// Show: server full, when clients_on == clients_max
				DrawStringCentered(425, y, STR_NETWORK_SERVER_FULL, 0); // server full
			} else if (sel->info.use_password) {
				DrawStringCentered(425, y, STR_NETWORK_PASSWORD, 0); // password warning
			}

			y += 10;
		}
	}	break;

	case WE_CLICK:
		nd->field = e->we.click.widget;
		switch (e->we.click.widget) {
		case 0: case 14: /* Close 'X' | Cancel button */
			DeleteWindowById(WC_NETWORK_WINDOW, 0);
			break;
		case 4: case 5:
			ShowDropDownMenu(w, _lan_internet_types_dropdown, _network_lan_internet, 5, 0, 0); // do it for widget 5
			break;
		case 6: /* Sort by name */
		case 7: /* Sort by connected clients */
		case 8: /* Connectivity (green dot) */
			if (ld->sort_type == e->we.click.widget - 6) ld->flags ^= VL_DESC;
			ld->flags |= VL_RESORT;
			ld->sort_type = e->we.click.widget - 6;

			_ng_sorting.order = !!(ld->flags & VL_DESC);
			_ng_sorting.criteria = ld->sort_type;
			SetWindowDirty(w);
			break;
		case 9: { /* Matrix to show networkgames */
			NetworkGameList *cur_item;
			uint32 id_v = (e->we.click.pt.y - NET_PRC__OFFSET_TOP_WIDGET) / NET_PRC__SIZE_OF_ROW;

			if (id_v >= w->vscroll.cap) return; // click out of bounds
			id_v += w->vscroll.pos;

			cur_item = _network_game_list;
			for (; id_v > 0 && cur_item != NULL; id_v--) cur_item = cur_item->next;

			nd->server = cur_item;
			SetWindowDirty(w);
		} break;
		case 11: /* Find server automatically */
			switch (_network_lan_internet) {
				case 0: NetworkUDPSearchGame(); break;
				case 1: NetworkUDPQueryMasterServer(); break;
			}
			break;
		case 12: { // Add a server
				ShowQueryString(
				BindCString(_network_default_ip),
				STR_NETWORK_ENTER_IP,
				31 | 0x1000,  // maximum number of characters OR
				250, // characters up to this width pixels, whichever is satisfied first
				w->window_class,
				w->window_number, CS_ALPHANUMERAL);
		} break;
		case 13: /* Start server */
			ShowNetworkStartServerWindow();
			break;
		case 16: /* Join Game */
			if (nd->server != NULL) {
				snprintf(_network_last_host, sizeof(_network_last_host), "%s", inet_ntoa(*(struct in_addr *)&nd->server->ip));
				_network_last_port = nd->server->port;
				ShowNetworkLobbyWindow(nd->server);
			}
			break;
		case 17: // Refresh
			if (nd->server != NULL)
				NetworkQueryServer(nd->server->info.hostname, nd->server->port, true);
			break;
		case 18: // NewGRF Settings
			if (nd->server != NULL) ShowNewGRFSettings(false, false, false, &nd->server->info.grfconfig);
			break;

	}	break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		switch (e->we.dropdown.button) {
			case 5:
				_network_lan_internet = e->we.dropdown.index;
				break;
		}

		SetWindowDirty(w);
		break;

	case WE_MOUSELOOP:
		if (nd->field == 3) HandleEditBox(w, &WP(w, network_ql_d).q, 3);
		break;

	case WE_MESSAGE:
		if (e->we.message.msg != 0) nd->server = NULL;
		ld->flags |= VL_REBUILD;
		SetWindowDirty(w);
		break;

	case WE_KEYPRESS:
		if (nd->field != 3) {
			if (nd->server != NULL) {
				if (e->we.keypress.keycode == WKC_DELETE) { /* Press 'delete' to remove servers */
					NetworkGameListRemoveItem(nd->server);
					NetworkRebuildHostList();
					nd->server = NULL;
				}
			}
			break;
		}

		if (HandleEditBoxKey(w, &WP(w, network_ql_d).q, 3, e) == 1) break; // enter pressed

		// The name is only allowed when it starts with a letter!
		if (_edit_str_buf[0] != '\0' && _edit_str_buf[0] != ' ') {
			ttd_strlcpy(_network_player_name, _edit_str_buf, lengthof(_network_player_name));
		} else {
			ttd_strlcpy(_network_player_name, "Player", lengthof(_network_player_name));
		}

		break;

	case WE_ON_EDIT_TEXT:
		NetworkAddServer(e->we.edittext.str);
		NetworkRebuildHostList();
		break;

	case WE_DESTROY: /* Nicely clean up the sort-list */
		free(WP(w, network_ql_d).sort_list);
		break;
	}
}

static const Widget _network_game_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,                    STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   549,     0,    13, STR_NETWORK_MULTIPLAYER,     STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,   BGC,     0,   549,    14,   263, 0x0,                         STR_NULL},

/* LEFT SIDE */
{      WWT_PANEL,   RESIZE_NONE,   BGC,   310,   461,    22,    33, 0x0,                         STR_NETWORK_ENTER_NAME_TIP},

{      WWT_INSET,   RESIZE_NONE,   BGC,    90,   181,    22,    33, STR_NETWORK_COMBO1,          STR_NETWORK_CONNECTION_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,   BGC,   170,   180,    23,    32, STR_0225,                    STR_NETWORK_CONNECTION_TIP},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   170,    42,    53, STR_NETWORK_GAME_NAME,       STR_NETWORK_GAME_NAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   171,   250,    42,    53, STR_NETWORK_CLIENTS_CAPTION, STR_NETWORK_CLIENTS_CAPTION_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   251,   290,    42,    53, STR_EMPTY,                   STR_NETWORK_INFO_ICONS_TIP},

{     WWT_MATRIX,   RESIZE_NONE,   BGC,    10,   290,    54,   236, (13 << 8) + 1,               STR_NETWORK_CLICK_GAME_TO_SELECT},
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   291,   302,    42,   236, STR_NULL,                    STR_0190_SCROLL_BAR_SCROLLS_LIST},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    30,   130,   246,   257, STR_NETWORK_FIND_SERVER,     STR_NETWORK_FIND_SERVER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   180,   280,   246,   257, STR_NETWORK_ADD_SERVER,      STR_NETWORK_ADD_SERVER_TIP},

/* RIGHT SIDE */
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   315,   415,   246,   257, STR_NETWORK_START_SERVER,    STR_NETWORK_START_SERVER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   430,   535,   246,   257, STR_012E_CANCEL,             STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,   BGC,   310,   540,    42,   236, 0x0,                         STR_NULL},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   315,   415,   215,   226, STR_NETWORK_JOIN_GAME,       STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   430,   535,   215,   226, STR_NETWORK_REFRESH,         STR_NETWORK_REFRESH_TIP},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   430,   535,   197,   208, STR_NEWGRF_SETTINGS_BUTTON,  STR_NULL},

{   WIDGETS_END},
};

static const WindowDesc _network_game_window_desc = {
	WDP_CENTER, WDP_CENTER, 550, 264,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_game_window_widgets,
	NetworkGameWindowWndProc,
};

void ShowNetworkGameWindow(void)
{
	static bool first = true;
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	/* Only show once */
	if (first) {
		char* const *srv;

		first = false;
		// add all servers from the config file to our list
		for (srv = &_network_host_list[0]; srv != endof(_network_host_list) && *srv != NULL; srv++) {
			NetworkAddServer(*srv);
		}

		_ng_sorting.criteria = 2; // sort default by collectivity (green-dots on top)
		_ng_sorting.order = 0;    // sort ascending by default
	}

	w = AllocateWindowDesc(&_network_game_window_desc);
	if (w != NULL) {
		querystr_d *querystr = &WP(w, network_ql_d).q;

		ttd_strlcpy(_edit_str_buf, _network_player_name, lengthof(_edit_str_buf));
		w->vscroll.cap = 13;

		querystr->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&querystr->text, _edit_str_buf, lengthof(_edit_str_buf), 120);

		UpdateNetworkGameWindow(true);
	}
}

enum {
	NSSWND_START = 64,
	NSSWND_ROWSIZE = 12
};

/* Uses network_ql_d (network_d, querystr_d and list_d) WP macro */
static void NetworkStartServerWindowWndProc(Window *w, WindowEvent *e)
{
	network_d *nd = &WP(w, network_ql_d).n;

	switch (e->event) {
	case WE_CREATE: /* focus input box */
		nd->field = 3;
		_network_game_info.use_password = (_network_server_password[0] != '\0');
		break;

	case WE_PAINT: {
		int y = NSSWND_START, pos;
		const FiosItem *item;

		SetDParam( 7, _connection_types_dropdown[_network_advertise]);
		SetDParam( 9, _players_dropdown[_network_game_info.clients_max]);
		SetDParam(11, _players_dropdown[_network_game_info.companies_max]);
		SetDParam(13, _players_dropdown[_network_game_info.spectators_max]);
		SetDParam(15, _language_dropdown[_network_game_info.server_lang]);
		DrawWindowWidgets(w);

		GfxFillRect(11, 63, 258, 215, 0xD7);
		DrawEditBox(w, &WP(w, network_ql_d).q, 3);

		DrawString(10, 22, STR_NETWORK_NEW_GAME_NAME, 2);

		DrawString(10, 43, STR_NETWORK_SELECT_MAP, 2);

		DrawString(280,  63, STR_NETWORK_CONNECTION, 2);
		DrawString(280,  95, STR_NETWORK_NUMBER_OF_CLIENTS, 2);
		DrawString(280, 127, STR_NETWORK_NUMBER_OF_COMPANIES, 2);
		DrawString(280, 159, STR_NETWORK_NUMBER_OF_SPECTATORS, 2);
		DrawString(280, 191, STR_NETWORK_LANGUAGE_SPOKEN, 2);

		if (_network_game_info.use_password) DoDrawString("*", 408, 23, 3);

		// draw list of maps
		pos = w->vscroll.pos;
		while (pos < _fios_num + 1) {
			item = _fios_list + pos - 1;
			if (item == nd->map || (pos == 0 && nd->map == NULL))
				GfxFillRect(11, y - 1, 258, y + 10, 155); // show highlighted item with a different colour

			if (pos == 0) {
				DrawString(14, y, STR_4010_GENERATE_RANDOM_NEW_GAME, 9);
			} else {
				DoDrawString(item->title, 14, y, _fios_colors[item->type] );
			}
			pos++;
			y += NSSWND_ROWSIZE;

			if (y >= w->vscroll.cap * NSSWND_ROWSIZE + NSSWND_START) break;
		}
	}	break;

	case WE_CLICK:
		nd->field = e->we.click.widget;
		switch (e->we.click.widget) {
		case 0: /* Close 'X' */
		case 19: /* Cancel button */
			ShowNetworkGameWindow();
			break;

		case 4: /* Set password button */
			ShowQueryString(BindCString(_network_server_password),
				STR_NETWORK_SET_PASSWORD, 20, 250, w->window_class, w->window_number, CS_ALPHANUMERAL);
			break;

		case 5: { /* Select map */
			int y = (e->we.click.pt.y - NSSWND_START) / NSSWND_ROWSIZE;

			y += w->vscroll.pos;
			if (y >= w->vscroll.count) return;

			nd->map = (y == 0) ? NULL : _fios_list + y - 1;
			SetWindowDirty(w);
			} break;
		case 7: case 8: /* Connection type */
			ShowDropDownMenu(w, _connection_types_dropdown, _network_advertise, 8, 0, 0); // do it for widget 8
			break;
		case 9: case 10: /* Number of Players (hide 0 and 1 players) */
			ShowDropDownMenu(w, _players_dropdown, _network_game_info.clients_max, 10, 0, 3);
			break;
		case 11: case 12: /* Number of Companies (hide 0, 9 and 10 companies; max is 8) */
			ShowDropDownMenu(w, _players_dropdown, _network_game_info.companies_max, 12, 0, 1537);
			break;
		case 13: case 14: /* Number of Spectators */
			ShowDropDownMenu(w, _players_dropdown, _network_game_info.spectators_max, 14, 0, 0);
			break;
		case 15: case 16: /* Language */
			ShowDropDownMenu(w, _language_dropdown, _network_game_info.server_lang, 16, 0, 0);
			break;
		case 17: /* Start game */
			_is_network_server = true;

			if (nd->map == NULL) { // start random new game
				ShowGenerateLandscape();
			} else { // load a scenario
				char *name = FiosBrowseTo(nd->map);
				if (name != NULL) {
					SetFiosType(nd->map->type);
					ttd_strlcpy(_file_to_saveload.name, name, sizeof(_file_to_saveload.name));
					ttd_strlcpy(_file_to_saveload.title, nd->map->title, sizeof(_file_to_saveload.title));

					DeleteWindow(w);
					SwitchMode(SM_START_SCENARIO);
				}
			}
			break;
		case 18: /* Load game */
			_is_network_server = true;
			/* XXX - WC_NETWORK_WINDOW should stay, but if it stays, it gets
			 * copied all the elements of 'load game' and upon closing that, it segfaults */
			DeleteWindowById(WC_NETWORK_WINDOW, 0);
			ShowSaveLoadDialog(SLD_LOAD_GAME);
			break;
		}
		break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		switch (e->we.dropdown.button) {
			case  8: _network_advertise                = (e->we.dropdown.index != 0); break;
			case 10: _network_game_info.clients_max    = e->we.dropdown.index;        break;
			case 12: _network_game_info.companies_max  = e->we.dropdown.index;        break;
			case 14: _network_game_info.spectators_max = e->we.dropdown.index;        break;
			case 16: _network_game_info.server_lang    = e->we.dropdown.index;        break;
		}

		SetWindowDirty(w);
		break;

	case WE_MOUSELOOP:
		if (nd->field == 3) HandleEditBox(w, &WP(w, network_ql_d).q, 3);
		break;

	case WE_KEYPRESS:
		if (nd->field == 3) {
			if (HandleEditBoxKey(w, &WP(w, network_ql_d).q, 3, e) == 1) break; // enter pressed

			ttd_strlcpy(_network_server_name, WP(w, network_ql_d).q.text.buf, sizeof(_network_server_name));
			UpdateTextBufferSize(&WP(w, network_ql_d).q.text);
		}
		break;

	case WE_ON_EDIT_TEXT: {
		ttd_strlcpy(_network_server_password, e->we.edittext.str, lengthof(_network_server_password));
		_network_game_info.use_password = (_network_server_password[0] != '\0');
		SetWindowDirty(w);
	} break;
	}
}

static const Widget _network_start_server_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,                      STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   419,     0,    13, STR_NETWORK_START_GAME_WINDOW, STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,   BGC,     0,   419,    14,   243, 0x0,                           STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,   BGC,   100,   272,    22,    33, 0x0,                           STR_NETWORK_NEW_GAME_NAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   285,   405,    22,    33, STR_NETWORK_SET_PASSWORD,      STR_NETWORK_PASSWORD_TIP},

{      WWT_INSET,   RESIZE_NONE,   BGC,    10,   271,    62,   216, 0x0,                           STR_NETWORK_SELECT_MAP_TIP},
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   259,   270,    63,   215, 0x0,                           STR_0190_SCROLL_BAR_SCROLLS_LIST},
/* Combo boxes to control Connection Type / Max Clients / Max Companies / Max Observers / Language */
{      WWT_INSET,   RESIZE_NONE,   BGC,   280,   410,    77,    88, STR_NETWORK_COMBO1,            STR_NETWORK_CONNECTION_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,   BGC,   399,   409,    78,    87, STR_0225,                      STR_NETWORK_CONNECTION_TIP},
{      WWT_INSET,   RESIZE_NONE,   BGC,   280,   410,   109,   120, STR_NETWORK_COMBO2,            STR_NETWORK_NUMBER_OF_CLIENTS_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,   BGC,   399,   409,   110,   119, STR_0225,                      STR_NETWORK_NUMBER_OF_CLIENTS_TIP},
{      WWT_INSET,   RESIZE_NONE,   BGC,   280,   410,   141,   152, STR_NETWORK_COMBO3,            STR_NETWORK_NUMBER_OF_COMPANIES_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,   BGC,   399,   409,   142,   151, STR_0225,                      STR_NETWORK_NUMBER_OF_COMPANIES_TIP},
{      WWT_INSET,   RESIZE_NONE,   BGC,   280,   410,   173,   184, STR_NETWORK_COMBO4,            STR_NETWORK_NUMBER_OF_SPECTATORS_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,   BGC,   399,   409,   174,   183, STR_0225,                      STR_NETWORK_NUMBER_OF_SPECTATORS_TIP},
{      WWT_INSET,   RESIZE_NONE,   BGC,   280,   410,   205,   216, STR_NETWORK_COMBO5,            STR_NETWORK_LANGUAGE_TIP},
{    WWT_TEXTBTN,   RESIZE_NONE,   BGC,   399,   409,   206,   215, STR_0225,                      STR_NETWORK_LANGUAGE_TIP},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    40,   140,   224,   235, STR_NETWORK_START_GAME,        STR_NETWORK_START_GAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   150,   250,   224,   235, STR_NETWORK_LOAD_GAME,         STR_NETWORK_LOAD_GAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   260,   360,   224,   235, STR_012E_CANCEL,               STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_start_server_window_desc = {
	WDP_CENTER, WDP_CENTER, 420, 244,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_start_server_window_widgets,
	NetworkStartServerWindowWndProc,
};

static void ShowNetworkStartServerWindow(void)
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	w = AllocateWindowDesc(&_network_start_server_window_desc);
	ttd_strlcpy(_edit_str_buf, _network_server_name, lengthof(_edit_str_buf));

	_saveload_mode = SLD_NEW_GAME;
	BuildFileList();
	w->vscroll.cap = 12;
	w->vscroll.count = _fios_num+1;

	WP(w, network_ql_d).q.afilter = CS_ALPHANUMERAL;
	InitializeTextBuffer(&WP(w, network_ql_d).q.text, _edit_str_buf, lengthof(_edit_str_buf), 160);
}

static byte NetworkLobbyFindCompanyIndex(byte pos)
{
	byte i;

	/* Scroll through all _network_player_info and get the 'pos' item
	    that is not empty */
	for (i = 0; i < MAX_PLAYERS; i++) {
		if (_network_player_info[i].company_name[0] != '\0') {
			if (pos-- == 0) return i;
		}
	}

	return 0;
}

/* uses network_d WP macro */
static void NetworkLobbyWindowWndProc(Window *w, WindowEvent *e)
{
	network_d *nd = &WP(w, network_d);

	switch (e->event) {
	case WE_CREATE:
		nd->company = (byte)-1;
		break;

	case WE_PAINT: {
		const NetworkGameInfo *gi = &nd->server->info;
		int y = NET_PRC__OFFSET_TOP_WIDGET_COMPANY, pos;

		SetWindowWidgetDisabledState(w, 7, nd->company == (byte)-1);
		SetWindowWidgetDisabledState(w, 8, gi->companies_on >= gi->companies_max);
		/* You can not join a server as spectator when it has no companies active..
		 * it causes some nasty crashes */
		SetWindowWidgetDisabledState(w, 9, gi->spectators_on >= gi->spectators_max ||
				gi->companies_on == 0);

		DrawWindowWidgets(w);

		SetDParamStr(0, gi->server_name);
		DrawString(10, 22, STR_NETWORK_PREPARE_TO_JOIN, 2);

		/* Draw company list */
		pos = w->vscroll.pos;
		while (pos < gi->companies_on) {
			byte company = NetworkLobbyFindCompanyIndex(pos);
			bool income = false;
			if (nd->company == company)
				GfxFillRect(11, y - 1, 154, y + 10, 10); // show highlighted item with a different colour

			DoDrawStringTruncated(_network_player_info[company].company_name, 13, y, 16, 135 - 13);
			if (_network_player_info[company].use_password != 0) DrawSprite(SPR_LOCK, 135, y);

			/* If the company's income was positive puts a green dot else a red dot */
			if (_network_player_info[company].income >= 0) income = true;
			DrawSprite(SPR_BLOT | (income ? PALETTE_TO_GREEN : PALETTE_TO_RED), 145, y);

			pos++;
			y += NET_PRC__SIZE_OF_ROW;
			if (pos >= w->vscroll.cap) break;
		}

		/* Draw info about selected company when it is selected in the left window */
		GfxFillRect(174, 39, 403, 75, 157);
		DrawStringCentered(290, 50, STR_NETWORK_COMPANY_INFO, 0);
		if (nd->company != (byte)-1) {
			const uint x = 183;
			const uint trunc_width = w->widget[6].right - x;
			y = 80;

			SetDParam(0, nd->server->info.clients_on);
			SetDParam(1, nd->server->info.clients_max);
			SetDParam(2, nd->server->info.companies_on);
			SetDParam(3, nd->server->info.companies_max);
			DrawString(x, y, STR_NETWORK_CLIENTS, 2);
			y += 10;

			SetDParamStr(0, _network_player_info[nd->company].company_name);
			DrawStringTruncated(x, y, STR_NETWORK_COMPANY_NAME, 2, trunc_width);
			y += 10;

			SetDParam(0, _network_player_info[nd->company].inaugurated_year);
			DrawString(x, y, STR_NETWORK_INAUGURATION_YEAR, 2); // inauguration year
			y += 10;

			SetDParam64(0, _network_player_info[nd->company].company_value);
			DrawString(x, y, STR_NETWORK_VALUE, 2); // company value
			y += 10;

			SetDParam64(0, _network_player_info[nd->company].money);
			DrawString(x, y, STR_NETWORK_CURRENT_BALANCE, 2); // current balance
			y += 10;

			SetDParam64(0, _network_player_info[nd->company].income);
			DrawString(x, y, STR_NETWORK_LAST_YEARS_INCOME, 2); // last year's income
			y += 10;

			SetDParam(0, _network_player_info[nd->company].performance);
			DrawString(x, y, STR_NETWORK_PERFORMANCE, 2); // performance
			y += 10;

			SetDParam(0, _network_player_info[nd->company].num_vehicle[0]);
			SetDParam(1, _network_player_info[nd->company].num_vehicle[1]);
			SetDParam(2, _network_player_info[nd->company].num_vehicle[2]);
			SetDParam(3, _network_player_info[nd->company].num_vehicle[3]);
			SetDParam(4, _network_player_info[nd->company].num_vehicle[4]);
			DrawString(x, y, STR_NETWORK_VEHICLES, 2); // vehicles
			y += 10;

			SetDParam(0, _network_player_info[nd->company].num_station[0]);
			SetDParam(1, _network_player_info[nd->company].num_station[1]);
			SetDParam(2, _network_player_info[nd->company].num_station[2]);
			SetDParam(3, _network_player_info[nd->company].num_station[3]);
			SetDParam(4, _network_player_info[nd->company].num_station[4]);
			DrawString(x, y, STR_NETWORK_STATIONS, 2); // stations
			y += 10;

			SetDParamStr(0, _network_player_info[nd->company].players);
			DrawStringTruncated(x, y, STR_NETWORK_PLAYERS, 2, trunc_width); // players
		}
	}	break;

	case WE_CLICK:
		switch (e->we.click.widget) {
		case 0: case 11: /* Close 'X' | Cancel button */
			ShowNetworkGameWindow();
			break;
		case 4: { /* Company list */
			uint32 id_v = (e->we.click.pt.y - NET_PRC__OFFSET_TOP_WIDGET_COMPANY) / NET_PRC__SIZE_OF_ROW;

			if (id_v >= w->vscroll.cap) return;

			id_v += w->vscroll.pos;
			nd->company = (id_v >= nd->server->info.companies_on) ? (byte)-1 : NetworkLobbyFindCompanyIndex(id_v);
			SetWindowDirty(w);
		}	break;
		case 7: /* Join company */
			if (nd->company != (byte)-1) {
				_network_playas = nd->company;
				NetworkClientConnectGame(_network_last_host, _network_last_port);
			}
			break;
		case 8: /* New company */
			_network_playas = PLAYER_NEW_COMPANY;
			NetworkClientConnectGame(_network_last_host, _network_last_port);
			break;
		case 9: /* Spectate game */
			_network_playas = PLAYER_SPECTATOR;
			NetworkClientConnectGame(_network_last_host, _network_last_port);
			break;
		case 10: /* Refresh */
			NetworkQueryServer(_network_last_host, _network_last_port, false); // company info
			NetworkUDPQueryServer(_network_last_host, _network_last_port);     // general data
			break;
		}	break;

	case WE_MESSAGE:
		SetWindowDirty(w);
		break;
	}
}

static const Widget _network_lobby_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,                  STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   419,     0,    13, STR_NETWORK_GAME_LOBBY,    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,   BGC,     0,   419,    14,   234, 0x0,                       STR_NULL},

// company list
{      WWT_PANEL,   RESIZE_NONE,   BTC,    10,   155,    38,    49, 0x0,                       STR_NULL},
{     WWT_MATRIX,   RESIZE_NONE,   BGC,    10,   155,    50,   190, (10 << 8) + 1,             STR_NETWORK_COMPANY_LIST_TIP},
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   156,   167,    38,   190, STR_NULL,                  STR_0190_SCROLL_BAR_SCROLLS_LIST},

// company/player info
{      WWT_PANEL,   RESIZE_NONE,   BGC,   173,   404,    38,   190, 0x0,                       STR_NULL},

// buttons
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   151,   200,   211, STR_NETWORK_JOIN_COMPANY,  STR_NETWORK_JOIN_COMPANY_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   151,   215,   226, STR_NETWORK_NEW_COMPANY,   STR_NETWORK_NEW_COMPANY_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   158,   268,   200,   211, STR_NETWORK_SPECTATE_GAME, STR_NETWORK_SPECTATE_GAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   158,   268,   215,   226, STR_NETWORK_REFRESH,       STR_NETWORK_REFRESH_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   278,   388,   200,   211, STR_012E_CANCEL,           STR_NULL},

{   WIDGETS_END},
};

static const WindowDesc _network_lobby_window_desc = {
	WDP_CENTER, WDP_CENTER, 420, 235,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_lobby_window_widgets,
	NetworkLobbyWindowWndProc,
};

/* Show the networklobbywindow with the selected server
 * @param ngl Selected game pointer which is passed to the new window */
static void ShowNetworkLobbyWindow(NetworkGameList *ngl)
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	NetworkQueryServer(_network_last_host, _network_last_port, false); // company info
	NetworkUDPQueryServer(_network_last_host, _network_last_port);     // general data

	w = AllocateWindowDesc(&_network_lobby_window_desc);
	if (w != NULL) {
		WP(w, network_ql_d).n.server = ngl;
		strcpy(_edit_str_buf, "");
		w->vscroll.cap = 10;
	}
}

// The window below gives information about the connected clients
//  and also makes able to give money to them, kick them (if server)
//  and stuff like that.

extern void DrawPlayerIcon(PlayerID pid, int x, int y);

// Every action must be of this form
typedef void ClientList_Action_Proc(byte client_no);

// Max 10 actions per client
#define MAX_CLIENTLIST_ACTION 10

// Some standard bullshit.. defines variables ;)
static void ClientListWndProc(Window *w, WindowEvent *e);
static void ClientListPopupWndProc(Window *w, WindowEvent *e);
static byte _selected_clientlist_item = 255;
static byte _selected_clientlist_y = 0;
static char _clientlist_action[MAX_CLIENTLIST_ACTION][50];
static ClientList_Action_Proc *_clientlist_proc[MAX_CLIENTLIST_ACTION];

enum {
	CLNWND_OFFSET = 16,
	CLNWND_ROWSIZE = 10
};

static const Widget _client_list_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   237,     0,    13, STR_NETWORK_CLIENT_LIST,  STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX,   RESIZE_NONE,    14,   238,   249,     0,    13, STR_NULL,                 STR_STICKY_BUTTON},

{      WWT_PANEL,   RESIZE_NONE,    14,     0,   249,    14,    14 + CLNWND_ROWSIZE + 1, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const Widget _client_list_popup_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   99,     0,     0,     0, STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _client_list_desc = {
	WDP_AUTO, WDP_AUTO, 250, 1,
	WC_CLIENT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_client_list_widgets,
	ClientListWndProc
};

// Finds the Xth client-info that is active
static const NetworkClientInfo *NetworkFindClientInfo(byte client_no)
{
	const NetworkClientInfo *ci;

	FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
		if (client_no == 0) return ci;
		client_no--;
	}

	return NULL;
}

// Here we start to define the options out of the menu
static void ClientList_Kick(byte client_no)
{
	if (client_no < MAX_PLAYERS)
		SEND_COMMAND(PACKET_SERVER_ERROR)(DEREF_CLIENT(client_no), NETWORK_ERROR_KICKED);
}

static void ClientList_Ban(byte client_no)
{
	uint i;
	uint32 ip = NetworkFindClientInfo(client_no)->client_ip;

	for (i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] == NULL) {
			_network_ban_list[i] = strdup(inet_ntoa(*(struct in_addr *)&ip));
			break;
		}
	}

	if (client_no < MAX_PLAYERS)
		SEND_COMMAND(PACKET_SERVER_ERROR)(DEREF_CLIENT(client_no), NETWORK_ERROR_KICKED);
}

static void ClientList_GiveMoney(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL)
		ShowNetworkGiveMoneyWindow(NetworkFindClientInfo(client_no)->client_playas);
}

static void ClientList_SpeakToClient(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL)
		ShowNetworkChatQueryWindow(DESTTYPE_CLIENT, NetworkFindClientInfo(client_no)->client_index);
}

static void ClientList_SpeakToCompany(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL)
		ShowNetworkChatQueryWindow(DESTTYPE_TEAM, NetworkFindClientInfo(client_no)->client_playas);
}

static void ClientList_SpeakToAll(byte client_no)
{
	ShowNetworkChatQueryWindow(DESTTYPE_BROADCAST, 0);
}

static void ClientList_None(byte client_no)
{
	// No action ;)
}



// Help, a action is clicked! What do we do?
static void HandleClientListPopupClick(byte index, byte clientno) {
	// A click on the Popup of the ClientList.. handle the command
	if (index < MAX_CLIENTLIST_ACTION && _clientlist_proc[index] != NULL) {
		_clientlist_proc[index](clientno);
	}
}

// Finds the amount of clients and set the height correct
static bool CheckClientListHeight(Window *w)
{
	int num = 0;
	const NetworkClientInfo *ci;

	// Should be replaced with a loop through all clients
	FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
		num++;
	}

	num *= CLNWND_ROWSIZE;

	// If height is changed
	if (w->height != CLNWND_OFFSET + num + 1) {
		// XXX - magic unfortunately; (num + 2) has to be one bigger than heigh (num + 1)
		SetWindowDirty(w);
		w->widget[3].bottom = w->widget[3].top + num + 2;
		w->height = CLNWND_OFFSET + num + 1;
		SetWindowDirty(w);
		return false;
	}
	return true;
}

// Finds the amount of actions in the popup and set the height correct
static uint ClientListPopupHeigth(void) {
	int i, num = 0;

	// Find the amount of actions
	for (i = 0; i < MAX_CLIENTLIST_ACTION; i++) {
		if (_clientlist_action[i][0] == '\0') continue;
		if (_clientlist_proc[i] == NULL) continue;
		num++;
	}

	num *= CLNWND_ROWSIZE;

	return num + 1;
}

// Show the popup (action list)
static Window *PopupClientList(Window *w, int client_no, int x, int y)
{
	int i, h;
	const NetworkClientInfo *ci;
	DeleteWindowById(WC_TOOLBAR_MENU, 0);

	// Clean the current actions
	for (i = 0; i < MAX_CLIENTLIST_ACTION; i++) {
		_clientlist_action[i][0] = '\0';
		_clientlist_proc[i] = NULL;
	}

	// Fill the actions this client has
	// Watch is, max 50 chars long!

	ci = NetworkFindClientInfo(client_no);
	if (ci == NULL) return NULL;

	i = 0;
	if (_network_own_client_index != ci->client_index) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_CLIENT, lastof(_clientlist_action[i]));
		_clientlist_proc[i++] = &ClientList_SpeakToClient;
	}

	if (IsValidPlayer(ci->client_playas) || ci->client_playas == PLAYER_SPECTATOR) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_COMPANY, lastof(_clientlist_action[i]));
		_clientlist_proc[i++] = &ClientList_SpeakToCompany;
	}
	GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_ALL, lastof(_clientlist_action[i]));
	_clientlist_proc[i++] = &ClientList_SpeakToAll;

	if (_network_own_client_index != ci->client_index) {
		/* We are no spectator and the player we want to give money to is no spectator */
		if (IsValidPlayer(_network_playas) && IsValidPlayer(ci->client_playas)) {
			GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_GIVE_MONEY, lastof(_clientlist_action[i]));
			_clientlist_proc[i++] = &ClientList_GiveMoney;
		}
	}

	// A server can kick clients (but not himself)
	if (_network_server && _network_own_client_index != ci->client_index) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_KICK, lastof(_clientlist_action[i]));
		_clientlist_proc[i++] = &ClientList_Kick;

		sprintf(_clientlist_action[i],"Ban"); // XXX GetString?
		_clientlist_proc[i++] = &ClientList_Ban;
	}

	if (i == 0) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_NONE, lastof(_clientlist_action[i]));
		_clientlist_proc[i++] = &ClientList_None;
	}

	/* Calculate the height */
	h = ClientListPopupHeigth();

	// Allocate the popup
	w = AllocateWindow(x, y, 150, h + 1, ClientListPopupWndProc, WC_TOOLBAR_MENU, _client_list_popup_widgets);
	w->widget[0].bottom = w->widget[0].top + h;
	w->widget[0].right = w->widget[0].left + 150;

	w->flags4 &= ~WF_WHITE_BORDER_MASK;
	WP(w,menu_d).item_count = 0;
	// Save our client
	WP(w,menu_d).main_button = client_no;
	WP(w,menu_d).sel_index = 0;
	// We are a popup
	_popup_menu_active = true;

	return w;
}

/** Main handle for the client popup list
 * uses menu_d WP macro */
static void ClientListPopupWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		int i, y, sel;
		byte colour;
		DrawWindowWidgets(w);

		// Draw the actions
		sel = WP(w,menu_d).sel_index;
		y = 1;
		for (i = 0; i < MAX_CLIENTLIST_ACTION; i++, y += CLNWND_ROWSIZE) {
			if (_clientlist_action[i][0] == '\0') continue;
			if (_clientlist_proc[i] == NULL) continue;

			if (sel-- == 0) { // Selected item, highlight it
				GfxFillRect(1, y, 150 - 2, y + CLNWND_ROWSIZE - 1, 0);
				colour = 0xC;
			} else {
				colour = 0x10;
			}

			DoDrawString(_clientlist_action[i], 4, y, colour);
		}
	}	break;

	case WE_POPUPMENU_SELECT: {
		// We selected an action
		int index = (e->we.popupmenu.pt.y - w->top) / CLNWND_ROWSIZE;

		if (index >= 0 && e->we.popupmenu.pt.y >= w->top)
			HandleClientListPopupClick(index, WP(w,menu_d).main_button);

		DeleteWindowById(WC_TOOLBAR_MENU, 0);
	}	break;

	case WE_POPUPMENU_OVER: {
		// Our mouse hoovers over an action? Select it!
		int index = (e->we.popupmenu.pt.y - w->top) / CLNWND_ROWSIZE;

		if (index == -1 || index == WP(w,menu_d).sel_index) return;

		WP(w,menu_d).sel_index = index;
		SetWindowDirty(w);
	} break;

	}
}

// Main handle for clientlist
static void ClientListWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		NetworkClientInfo *ci;
		int y, i = 0;
		byte colour;

		// Check if we need to reset the height
		if (!CheckClientListHeight(w)) break;

		DrawWindowWidgets(w);

		y = CLNWND_OFFSET;

		FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
			if (_selected_clientlist_item == i++) { // Selected item, highlight it
				GfxFillRect(1, y, 248, y + CLNWND_ROWSIZE - 1, 0);
				colour = 0xC;
			} else {
				colour = 0x10;
			}

			if (ci->client_index == NETWORK_SERVER_INDEX) {
				DrawString(4, y, STR_NETWORK_SERVER, colour);
			} else {
				DrawString(4, y, STR_NETWORK_CLIENT, colour);
			}

			// Filter out spectators
			if (IsValidPlayer(ci->client_playas)) DrawPlayerIcon(ci->client_playas, 64, y + 1);

			DoDrawString(ci->client_name, 81, y, colour);

			y += CLNWND_ROWSIZE;
		}
	}	break;

	case WE_CLICK:
		// Show the popup with option
		if (_selected_clientlist_item != 255) {
			PopupClientList(w, _selected_clientlist_item, e->we.click.pt.x + w->left, e->we.click.pt.y + w->top);
		}

		break;

	case WE_MOUSEOVER:
		// -1 means we left the current window
		if (e->we.mouseover.pt.y == -1) {
			_selected_clientlist_y = 0;
			_selected_clientlist_item = 255;
			SetWindowDirty(w);
			break;
		}
		// It did not change.. no update!
		if (e->we.mouseover.pt.y == _selected_clientlist_y) break;

		// Find the new selected item (if any)
		_selected_clientlist_y = e->we.mouseover.pt.y;
		if (e->we.mouseover.pt.y > CLNWND_OFFSET) {
			_selected_clientlist_item = (e->we.mouseover.pt.y - CLNWND_OFFSET) / CLNWND_ROWSIZE;
		} else {
			_selected_clientlist_item = 255;
		}

		// Repaint
		SetWindowDirty(w);
		break;

	case WE_DESTROY: case WE_CREATE:
		// When created or destroyed, data is reset
		_selected_clientlist_item = 255;
		_selected_clientlist_y = 0;
		break;
	}
}

void ShowClientList(void)
{
	AllocateWindowDescFront(&_client_list_desc, 0);
}


static NetworkPasswordType pw_type;


void ShowNetworkNeedPassword(NetworkPasswordType npt)
{
	StringID caption;

	pw_type = npt;
	switch (npt) {
		default: NOT_REACHED();
		case NETWORK_GAME_PASSWORD:    caption = STR_NETWORK_NEED_GAME_PASSWORD_CAPTION; break;
		case NETWORK_COMPANY_PASSWORD: caption = STR_NETWORK_NEED_COMPANY_PASSWORD_CAPTION; break;
	}
	ShowQueryString(STR_EMPTY, caption, 20, 180, WC_NETWORK_STATUS_WINDOW, 0, CS_ALPHANUMERAL);
}


static void NetworkJoinStatusWindowWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_PAINT: {
		uint8 progress; // used for progress bar
		DrawWindowWidgets(w);

		DrawStringCentered(125, 35, STR_NETWORK_CONNECTING_1 + _network_join_status, 14);
		switch (_network_join_status) {
			case NETWORK_JOIN_STATUS_CONNECTING: case NETWORK_JOIN_STATUS_AUTHORIZING:
			case NETWORK_JOIN_STATUS_GETTING_COMPANY_INFO:
				progress = 10; // first two stages 10%
				break;
			case NETWORK_JOIN_STATUS_WAITING:
				SetDParam(0, _network_join_waiting);
				DrawStringCentered(125, 46, STR_NETWORK_CONNECTING_WAITING, 14);
				progress = 15; // third stage is 15%
				break;
			case NETWORK_JOIN_STATUS_DOWNLOADING:
				SetDParam(0, _network_join_kbytes);
				SetDParam(1, _network_join_kbytes_total);
				DrawStringCentered(125, 46, STR_NETWORK_CONNECTING_DOWNLOADING, 14);
				/* Fallthrough */
			default: /* Waiting is 15%, so the resting receivement of map is maximum 70% */
				progress = 15 + _network_join_kbytes * (100 - 15) / (_network_join_kbytes_total+1);
		}

		/* Draw nice progress bar :) */
		DrawFrameRect(20, 18, (int)((w->width - 20) * progress / 100), 28, 10, 0);
	}	break;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 0: /* Close 'X' */
			case 3: /* Disconnect button */
				NetworkDisconnect();
				DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
				SwitchMode(SM_MENU);
				ShowNetworkGameWindow();
				break;
		}
		break;

		case WE_ON_EDIT_TEXT_CANCEL:
			NetworkDisconnect();
			ShowNetworkGameWindow();
			break;

		case WE_ON_EDIT_TEXT:
			SEND_COMMAND(PACKET_CLIENT_PASSWORD)(pw_type, e->we.edittext.str);
			break;
	}
}

static const Widget _network_join_status_window_widget[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   249,     0,    13, STR_NETWORK_CONNECTING, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   249,    14,    84, 0x0,                    STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    75,   175,    69,    80, STR_NETWORK_DISCONNECT, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_join_status_window_desc = {
	WDP_CENTER, WDP_CENTER, 250, 85,
	WC_NETWORK_STATUS_WINDOW, 0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_network_join_status_window_widget,
	NetworkJoinStatusWindowWndProc,
};

void ShowJoinStatusWindow(void)
{
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
	_network_join_status = NETWORK_JOIN_STATUS_CONNECTING;
	AllocateWindowDesc(&_network_join_status_window_desc);
}

void ShowJoinStatusWindowAfterJoin(void)
{
	/* This is a special instant of ShowJoinStatusWindow, because
	    it is opened after the map is loaded, but the client maybe is not
	    done registering itself to the server */
	DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
	_network_join_status = NETWORK_JOIN_STATUS_REGISTERING;
	AllocateWindowDesc(&_network_join_status_window_desc);
}

static void SendChat(const char *buf, DestType type, int dest)
{
	if (buf[0] == '\0') return;
	if (!_network_server) {
		SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT + type, type, dest, buf);
	} else {
		NetworkServer_HandleChat(NETWORK_ACTION_CHAT + type, type, dest, buf, NETWORK_SERVER_INDEX);
	}
}

/**
 * Find the next item of the list of things that can be auto-completed.
 * @param item The current indexed item to return. This function can, and most
 *     likely will, alter item, to skip empty items in the arrays.
 * @return Returns the char that matched to the index.
 */
static const char *ChatTabCompletionNextItem(uint *item)
{
	static char chat_tab_temp_buffer[64];

	/* First, try clients */
	if (*item < MAX_CLIENT_INFO) {
		/* Skip inactive clients */
		while (_network_client_info[*item].client_index == NETWORK_EMPTY_INDEX && *item < MAX_CLIENT_INFO) (*item)++;
		if (*item < MAX_CLIENT_INFO) return _network_client_info[*item].client_name;
	}

	/* Then, try townnames */
	/* Not that the following assumes all town indices are adjacent, ie no
	 * towns have been deleted. */
	if (*item <= (uint)MAX_CLIENT_INFO + GetMaxTownIndex()) {
		const Town *t;

		FOR_ALL_TOWNS_FROM(t, *item - MAX_CLIENT_INFO) {
			/* Get the town-name via the string-system */
			SetDParam(0, t->townnameparts);
			GetString(chat_tab_temp_buffer, t->townnametype, lastof(chat_tab_temp_buffer));
			return &chat_tab_temp_buffer[0];
		}
	}

	return NULL;
}

/**
 * Find what text to complete. It scans for a space from the left and marks
 *  the word right from that as to complete. It also writes a \0 at the
 *  position of the space (if any). If nothing found, buf is returned.
 */
static char *ChatTabCompletionFindText(char *buf)
{
	char *p = strrchr(buf, ' ');
	if (p == NULL) return buf;

	*p = '\0';
	return p + 1;
}

/**
 * See if we can auto-complete the current text of the user.
 */
static void ChatTabCompletion(Window *w)
{
	static char _chat_tab_completion_buf[lengthof(_edit_str_buf)];
	Textbuf *tb = &WP(w, chatquerystr_d).text;
	uint len, tb_len;
	uint item;
	char *tb_buf, *pre_buf;
	const char *cur_name;
	bool second_scan = false;

	item = 0;

	/* Copy the buffer so we can modify it without damaging the real data */
	pre_buf = (_chat_tab_completion_active) ? strdup(_chat_tab_completion_buf) : strdup(tb->buf);

	tb_buf  = ChatTabCompletionFindText(pre_buf);
	tb_len  = strlen(tb_buf);

	while ((cur_name = ChatTabCompletionNextItem(&item)) != NULL) {
		item++;

		if (_chat_tab_completion_active) {
			/* We are pressing TAB again on the same name, is there an other name
			 *  that starts with this? */
			if (!second_scan) {
				uint offset;
				uint length;

				/* If we are completing at the begin of the line, skip the ': ' we added */
				if (tb_buf == pre_buf) {
					offset = 0;
					length = tb->length - 2;
				} else {
					/* Else, find the place we are completing at */
					offset = strlen(pre_buf) + 1;
					length = tb->length - offset;
				}

				/* Compare if we have a match */
				if (strlen(cur_name) == length && strncmp(cur_name, tb->buf + offset, length) == 0) second_scan = true;

				continue;
			}

			/* Now any match we make on _chat_tab_completion_buf after this, is perfect */
		}

		len = strlen(cur_name);
		if (tb_len < len && strncasecmp(cur_name, tb_buf, tb_len) == 0) {
			/* Save the data it was before completion */
			if (!second_scan) snprintf(_chat_tab_completion_buf, lengthof(_chat_tab_completion_buf), "%s", tb->buf);
			_chat_tab_completion_active = true;

			/* Change to the found name. Add ': ' if we are at the start of the line (pretty) */
			if (pre_buf == tb_buf) {
				snprintf(tb->buf, lengthof(_edit_str_buf), "%s: ", cur_name);
			} else {
				snprintf(tb->buf, lengthof(_edit_str_buf), "%s %s", pre_buf, cur_name);
			}

			/* Update the textbuffer */
			UpdateTextBufferSize(&WP(w, chatquerystr_d).text);

			SetWindowDirty(w);
			free(pre_buf);
			return;
		}
	}

	if (second_scan) {
		/* We walked all posibilities, and the user presses tab again.. revert to original text */
		strcpy(tb->buf, _chat_tab_completion_buf);
		_chat_tab_completion_active = false;

		/* Update the textbuffer */
		UpdateTextBufferSize(&WP(w, chatquerystr_d).text);

		SetWindowDirty(w);
	}
	free(pre_buf);
}

/*
 * uses chatquerystr_d WP macro
 * uses chatquerystr_d->caption to store type of chat message (Private/Team/All)
 */
static void ChatWindowWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE:
		SendWindowMessage(WC_NEWS_WINDOW, 0, WE_CREATE, w->height, 0);
		SETBIT(_no_scroll, SCROLL_CHAT); // do not scroll the game with the arrow-keys
		break;

	case WE_PAINT: {
		static const StringID chat_captions[] = {
			STR_NETWORK_CHAT_ALL_CAPTION,
			STR_NETWORK_CHAT_COMPANY_CAPTION,
			STR_NETWORK_CHAT_CLIENT_CAPTION
		};
		StringID msg;

		DrawWindowWidgets(w);

		assert(WP(w, chatquerystr_d).caption < lengthof(chat_captions));
		msg = chat_captions[WP(w, chatquerystr_d).caption];
		DrawStringRightAligned(w->widget[2].left - 2, w->widget[2].top + 1, msg, 16);
		DrawEditBox(w, &WP(w, querystr_d), 2);
	} break;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 3: { /* Send */
				DestType type = (DestType)WP(w, chatquerystr_d).caption;
				int dest = WP(w, chatquerystr_d).dest;
				SendChat(WP(w, chatquerystr_d).text.buf, type, dest);
			} /* FALLTHROUGH */
			case 0: /* Cancel */ DeleteWindow(w); break;
		}
		break;

	case WE_MOUSELOOP:
		HandleEditBox(w, &WP(w, querystr_d), 2);
		break;

	case WE_KEYPRESS:
		if (e->we.keypress.keycode == WKC_TAB) {
			ChatTabCompletion(w);
		} else {
			_chat_tab_completion_active = false;
			switch (HandleEditBoxKey(w, &WP(w, querystr_d), 2, e)) {
				case 1: { /* Return */
				DestType type = (DestType)WP(w, chatquerystr_d).caption;
				int dest = WP(w, chatquerystr_d).dest;
				SendChat(WP(w, chatquerystr_d).text.buf, type, dest);
			} /* FALLTHROUGH */
				case 2: /* Escape */ DeleteWindow(w); break;
			}
		}
		break;

	case WE_DESTROY:
		SendWindowMessage(WC_NEWS_WINDOW, 0, WE_DESTROY, 0, 0);
		CLRBIT(_no_scroll, SCROLL_CHAT);
		break;
	}
}

static const Widget _chat_window_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, 14,   0,  10,  0, 13, STR_00C5,         STR_018B_CLOSE_WINDOW},
{      WWT_PANEL, RESIZE_NONE, 14,  11, 639,  0, 13, 0x0,              STR_NULL}, // background
{      WWT_PANEL, RESIZE_NONE, 14,  75, 577,  1, 12, 0x0,              STR_NULL}, // text box
{ WWT_PUSHTXTBTN, RESIZE_NONE, 14, 578, 639,  1, 12, STR_NETWORK_SEND, STR_NULL}, // send button
{   WIDGETS_END},
};

static const WindowDesc _chat_window_desc = {
	WDP_CENTER, -26, 640, 14, // x, y, width, height
	WC_SEND_NETWORK_MSG,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_chat_window_widgets,
	ChatWindowWndProc
};

void ShowNetworkChatQueryWindow(DestType type, int dest)
{
	Window *w;

	DeleteWindowById(WC_SEND_NETWORK_MSG, 0);

	_edit_str_buf[0] = '\0';
	_chat_tab_completion_active = false;

	w = AllocateWindowDesc(&_chat_window_desc);

	LowerWindowWidget(w, 2);
	WP(w, chatquerystr_d).caption   = type; // Misuse of caption
	WP(w, chatquerystr_d).dest      = dest;
	WP(w, chatquerystr_d).afilter   = CS_ALPHANUMERAL;
	WP(w, chatquerystr_d).wnd_class = WC_MAIN_TOOLBAR;
	WP(w, chatquerystr_d).wnd_num   = 0;
	InitializeTextBuffer(&WP(w, chatquerystr_d).text, _edit_str_buf, lengthof(_edit_str_buf), 0);
}

#endif /* ENABLE_NETWORK */
