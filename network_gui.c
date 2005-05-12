#include "stdafx.h"
#include "ttd.h"
#include "string.h"
#include "strings.h"
#include "table/sprites.h"
#include "network.h"
#include "saveload.h"

#include "hal.h" // for file list

#ifdef ENABLE_NETWORK

#include "table/strings.h"
#include "network_data.h"
#include "network_gamelist.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "command.h"
#include "functions.h"
#include "variables.h"
#include "network_server.h"
#include "network_udp.h"

#define BGC 5
#define BTC 15
#define MAX_QUERYSTR_LEN 64
static char _edit_str_buf[MAX_QUERYSTR_LEN*2];
static void ShowNetworkStartServerWindow(void);
static void ShowNetworkLobbyWindow(void);

static byte _selected_field;
static bool _first_time_show_network_game_window = true;

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

static StringID _str_map_name, _str_game_name, _str_server_version, _str_server_address;

enum {
	NET_PRC__OFFSET_TOP_WIDGET					= 74,
	NET_PRC__OFFSET_TOP_WIDGET_COMPANY	= 42,
	NET_PRC__SIZE_OF_ROW								= 14,
	NET_PRC__SIZE_OF_ROW_COMPANY				= 12,
};

static NetworkGameList *_selected_item = NULL;
static int8 _selected_company_item = -1;

// Truncates a string to max_width (via GetStringWidth) and adds 3 dots
//  at the end of the name.
static void NetworkTruncateString(char *name, const int max_width)
{
	char temp[NETWORK_NAME_LENGTH];
	char internal_name[NETWORK_NAME_LENGTH];

	ttd_strlcpy(internal_name, name, sizeof(internal_name));

	if (GetStringWidth(internal_name) > max_width) {
		// Servername is too long, trunc it!
		snprintf(temp, sizeof(temp), "%s...", internal_name);
		// Continue to delete 1 char of the string till it is in range
		while (GetStringWidth(temp) > max_width) {
			internal_name[strlen(internal_name) - 1] = '\0';
			snprintf(temp, sizeof(temp), "%s...", internal_name);
		}
		ttd_strlcpy(name, temp, sizeof(temp));
	}
}

extern const char _openttd_revision[];

static FiosItem *selected_map = NULL; // to highlight slected map

// called when a new server is found on the network
void UpdateNetworkGameWindow(bool unselect)
{
	Window *w;
	w = FindWindowById(WC_NETWORK_WINDOW, 0);
	if (w != NULL) {
		if (unselect)
			_selected_item = NULL;
		w->vscroll.count = _network_game_count;
		SetWindowDirty(w);
	}
}

static void NetworkGameWindowWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: /* focus input box */
		_selected_field = 3;
		_selected_item = NULL;
		break;
	case WE_PAINT: {
		w->disabled_state = 0;

		if (_selected_item == NULL) {
			SETBIT(w->disabled_state, 17); SETBIT(w->disabled_state, 18);
		} else if (!_selected_item->online) {
			SETBIT(w->disabled_state, 17); // Server offline, join button disabled
		} else if (_selected_item->info.clients_on == _selected_item->info.clients_max) {
			SETBIT(w->disabled_state, 17); // Server full, join button disabled

			// revisions don't match, check if server has no revision; then allow connection
		} else if (strncmp(_selected_item->info.server_revision, _openttd_revision, NETWORK_REVISION_LENGTH - 1) != 0) {
			if (strncmp(_selected_item->info.server_revision, NOREV_STRING, sizeof(_selected_item->info.server_revision)) != 0)
				SETBIT(w->disabled_state, 17); // Revision mismatch, join button disabled
		}

		SetDParam(0, 0x00);
		SetDParam(7, _lan_internet_types_dropdown[_network_lan_internet]);
		DrawWindowWidgets(w);

		DrawEditBox(w, 3);

		DrawString(9, 23, STR_NETWORK_PLAYER_NAME, 2);
		DrawString(9, 43, STR_NETWORK_CONNECTION, 2);

		DrawString(15, 63, STR_NETWORK_GAME_NAME, 2);
		DrawString(135, 63, STR_NETWORK_CLIENTS_CAPTION, 2);

		{ // draw list of games
			uint16 y = NET_PRC__OFFSET_TOP_WIDGET + 3;
			int32 n = 0;
			int32 pos = w->vscroll.pos;
			char servername[NETWORK_NAME_LENGTH];
			const NetworkGameList *cur_item = _network_game_list;

			while (pos > 0 && cur_item != NULL) {
				pos--;
				cur_item = cur_item->next;
			}

			while (cur_item != NULL) {
				bool compatible = (strncmp(cur_item->info.server_revision, _openttd_revision, NETWORK_REVISION_LENGTH - 1) == 0);
				if (strncmp(cur_item->info.server_revision, NOREV_STRING, sizeof(cur_item->info.server_revision)) == 0)
					compatible = true;

				if (cur_item == _selected_item)
					GfxFillRect(11, y - 2, 218, y + 9, 10); // show highlighted item with a different colour

				snprintf(servername, sizeof(servername), "%s", cur_item->info.server_name);
				NetworkTruncateString(servername, 110);
				DoDrawString(servername, 15, y, 16); // server name

				SetDParam(0, cur_item->info.clients_on);
				SetDParam(1, cur_item->info.clients_max);
				DrawString(135, y, STR_NETWORK_CLIENTS_ONLINE, 2);

				// only draw icons if the server is online
				if (cur_item->online) {

					// draw a lock if the server is password protected.
					if(cur_item->info.use_password)
						DrawSprite(SPR_LOCK, 186, y-1);

					// draw red or green icon, depending on compatibility with server.
					DrawSprite(SPR_BLOT | (compatible?0x30d8000:0x30b8000), 195, y);

					// draw flag according to server language
					DrawSprite(SPR_FLAGS_BASE + cur_item->info.server_lang, 206, y);
				}

				cur_item = cur_item->next;
				y += NET_PRC__SIZE_OF_ROW;
				if (++n == w->vscroll.cap) { break;} // max number of games in the window
			}
		}

		// right menu
		GfxFillRect(252, 23, 478, 65, 157);
		if (_selected_item == NULL) {
			DrawStringMultiCenter(365, 40, STR_NETWORK_GAME_INFO, 0);
		} else if (!_selected_item->online) {
			SetDParam(0, _str_game_name);
			DrawStringMultiCenter(365, 42, STR_ORANGE, 2); // game name

			DrawStringMultiCenter(365, 110, STR_NETWORK_SERVER_OFFLINE, 2); // server offline
		} else { // show game info
			uint16 y = 70;

			DrawStringMultiCenter(365, 30, STR_NETWORK_GAME_INFO, 0);

			SetDParam(0, _str_game_name);
			DrawStringMultiCenter(365, 42, STR_ORANGE, 2); // game name

			SetDParam(0, _str_map_name);
			DrawStringMultiCenter(365, 54, STR_02BD, 2); // map name

			SetDParam(0, _selected_item->info.clients_on);
			SetDParam(1, _selected_item->info.clients_max);
			DrawString(260, y, STR_NETWORK_CLIENTS, 2); // clients on the server / maximum slots
			y+=10;

			SetDParam(0, STR_NETWORK_LANG_ANY+_selected_item->info.server_lang);
			DrawString(260, y, STR_NETWORK_LANGUAGE, 2); // server language
			y+=10;

			SetDParam(0, STR_TEMPERATE_LANDSCAPE+_selected_item->info.map_set);
			DrawString(260, y, STR_NETWORK_TILESET, 2); // tileset
			y+=10;

			SetDParam(0, _selected_item->info.map_width);
			SetDParam(1, _selected_item->info.map_height);
			DrawString(260, y, STR_NETWORK_MAP_SIZE, 2); // map size
			y+=10;

			SetDParam(0, _str_server_version);
			DrawString(260, y, STR_NETWORK_SERVER_VERSION, 2); // server version
			y+=10;

			SetDParam(0, _str_server_address);
			SetDParam(1, _selected_item->port);
			DrawString(260, y, STR_NETWORK_SERVER_ADDRESS, 2); // server address
			y+=10;

			SetDParam(0, _selected_item->info.start_date);
			DrawString(260, y, STR_NETWORK_START_DATE, 2); // start date
			y+=10;

			SetDParam(0, _selected_item->info.game_date);
			DrawString(260, y, STR_NETWORK_CURRENT_DATE, 2); // current date
			y+=10;

			y+=2;

			if (strncmp(_selected_item->info.server_revision, _openttd_revision, NETWORK_REVISION_LENGTH - 1) != 0) {
				if (strncmp(_selected_item->info.server_revision, NOREV_STRING, sizeof(_selected_item->info.server_revision)) != 0)
					DrawStringMultiCenter(365, y, STR_NETWORK_VERSION_MISMATCH, 2); // server mismatch
			} else if (_selected_item->info.clients_on == _selected_item->info.clients_max) {
				// Show: server full, when clients_on == clients_max
				DrawStringMultiCenter(365, y, STR_NETWORK_SERVER_FULL, 2); // server full
			} else if (_selected_item->info.use_password)
				DrawStringMultiCenter(365, y, STR_NETWORK_PASSWORD, 2); // password warning

			y+=10;
		}
	}	break;

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {
		case 0: case 14: /* Close 'X' | Cancel button */
			DeleteWindowById(WC_NETWORK_WINDOW, 0);
			break;
		case 4: case 5:
			ShowDropDownMenu(w, _lan_internet_types_dropdown, _network_lan_internet, 5, 0, 0); // do it for widget 5
			break;
		case 9: { /* Matrix to show networkgames */
			uint32 id_v = (e->click.pt.y - NET_PRC__OFFSET_TOP_WIDGET) / NET_PRC__SIZE_OF_ROW;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds
			id_v += w->vscroll.pos;

			{
				NetworkGameList *cur_item = _network_game_list;
				for (; id_v > 0 && cur_item != NULL; id_v--)
					cur_item = cur_item->next;

				if (cur_item == NULL) {
					// click out of vehicle bounds
					_selected_item = NULL;
					SetWindowDirty(w);
					return;
				}
				_selected_item = cur_item;

				DeleteName(_str_game_name);
				DeleteName(_str_map_name);
				DeleteName(_str_server_version);
				DeleteName(_str_server_address);
				if (_selected_item->info.server_name[0] != '\0')
					_str_game_name = AllocateName(_selected_item->info.server_name, 0);
				else
					_str_game_name = STR_EMPTY;

				if (_selected_item->info.map_name[0] != '\0')
					_str_map_name = AllocateName(_selected_item->info.map_name, 0);
				else
					_str_map_name = STR_EMPTY;

				if (_selected_item->info.server_revision[0] != '\0')
					_str_server_version = AllocateName(_selected_item->info.server_revision, 0);
				else
					_str_server_version = STR_EMPTY;

				if (_selected_item->info.hostname[0] != '\0')
					_str_server_address = AllocateName(_selected_item->info.hostname, 0);
				else
					_str_server_address = STR_EMPTY;
			}
			SetWindowDirty(w);
		} break;
		case 11: /* Find server automatically */
			switch (_network_lan_internet) {
				case 0: NetworkUDPSearchGame(); break;
				case 1: NetworkUDPQueryMasterServer(); break;
			}
			break;
		case 12: { // Add a server
				StringID str = AllocateName(_network_default_ip, 0);

				ShowQueryString(
				str,
				STR_NETWORK_ENTER_IP,
				31 | 0x1000,  // maximum number of characters OR
				250, // characters up to this width pixels, whichever is satisfied first
				w->window_class,
				w->window_number);
				DeleteName(str);
		} break;
		case 13: /* Start server */
			ShowNetworkStartServerWindow();
			break;
		case 17: /* Join Game */
			if (_selected_item != NULL) {
				memcpy(&_network_game_info, &_selected_item->info, sizeof(NetworkGameInfo));
				snprintf(_network_last_host, sizeof(_network_last_host), "%s", inet_ntoa(*(struct in_addr *)&_selected_item->ip));
				_network_last_port = _selected_item->port;
				ShowNetworkLobbyWindow();
			}
			break;
		case 18: // Refresh
			if (_selected_item != NULL) {
				NetworkQueryServer(_selected_item->info.hostname, _selected_item->port, true);
			}
			break;

	}	break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		switch(e->dropdown.button) {
			case 5:
				_network_lan_internet = e->dropdown.index;
				break;
		}

		SetWindowDirty(w);
		break;

	case WE_MOUSELOOP:
		if (_selected_field == 3)
			HandleEditBox(w, 3);

		break;

	case WE_KEYPRESS:
		if (_selected_field != 3) {
			if ( e->keypress.keycode == WKC_DELETE ) { // press 'delete' to remove servers
				if (_selected_item != NULL) {
					NetworkGameListRemoveItem(_selected_item);
					NetworkRebuildHostList();
					SetWindowDirty(w);
					_network_game_count--;
					// reposition scrollbar
					if (_network_game_count >= w->vscroll.cap && w->vscroll.pos > _network_game_count-w->vscroll.cap) w->vscroll.pos--;
					UpdateNetworkGameWindow(false);
					_selected_item = NULL;
				}
			}
			break;
		}

		if (HandleEditBoxKey(w, 3, e) == 1) break; // enter pressed

		// The name is only allowed when it starts with a letter!
		if (_edit_str_buf[0] != '\0' && _edit_str_buf[0] != ' ')
			ttd_strlcpy(_network_player_name, _edit_str_buf, lengthof(_network_player_name));
		else
			ttd_strlcpy(_network_player_name, "Player", lengthof(_network_player_name));

		break;

	case WE_ON_EDIT_TEXT: {
		NetworkAddServer(e->edittext.str);
		NetworkRebuildHostList();
	} break;
	}
}

static const Widget _network_game_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   489,     0,    13, STR_NETWORK_MULTIPLAYER,			STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,     0,   489,    14,   214, 0x0,													STR_NULL},

/* LEFT SIDE */
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,    90,   231,    22,    33, 0x0,													STR_NETWORK_ENTER_NAME_TIP},

{          WWT_6,   RESIZE_NONE,   BGC,    90,   231,    42,    53, STR_NETWORK_COMBO1,					STR_NETWORK_CONNECTION_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,   220,   230,    43,    52, STR_0225,										STR_NETWORK_CONNECTION_TIP},

{     WWT_IMGBTN,   RESIZE_NONE,   BTC,    10,   130,    62,    73, 0x0,													STR_NETWORK_GAME_NAME_TIP },
{     WWT_IMGBTN,   RESIZE_NONE,   BTC,   131,   180,    62,    73, 0x0,													STR_NETWORK_CLIENTS_CAPTION_TIP },
{     WWT_IMGBTN,   RESIZE_NONE,   BTC,   181,   219,    62,    73, 0x0,													STR_NETWORK_INFO_ICONS_TIP },

{     WWT_MATRIX,   RESIZE_NONE,   BGC,    10,   219,    74,   185, 0x801,												STR_NETWORK_CLICK_GAME_TO_SELECT},
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   220,   231,    62,   185, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   115,   195,   206, STR_NETWORK_FIND_SERVER,			STR_NETWORK_FIND_SERVER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   125,   231,   195,   206, STR_NETWORK_ADD_SERVER,			STR_NETWORK_ADD_SERVER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   250,   360,   195,   206, STR_NETWORK_START_SERVER,		STR_NETWORK_START_SERVER_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   370,   480,   195,   206, STR_012E_CANCEL,							STR_NULL},

/* RIGHT SIDE */
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   250,   480,    22,   185, 0x0,					STR_NULL},
{          WWT_6,   RESIZE_NONE,   BGC,   251,   479,    23,   184, 0x0,					STR_NULL},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   257,   360,   164,   175, STR_NETWORK_JOIN_GAME,					STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   370,   473,   164,   175, STR_NETWORK_REFRESH,					STR_NETWORK_REFRESH_TIP},

{   WIDGETS_END},
};

static const WindowDesc _network_game_window_desc = {
	WDP_CENTER, WDP_CENTER, 490, 215,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESTORE_DPARAM,
	_network_game_window_widgets,
	NetworkGameWindowWndProc,
};

void ShowNetworkGameWindow(void)
{
	uint i;
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	/* Only show once */
	if (_first_time_show_network_game_window) {
		_first_time_show_network_game_window = false;
		// add all servers from the config file to our list
		for (i=0; i != lengthof(_network_host_list); i++) {
			if (_network_host_list[i] == NULL) break;
			NetworkAddServer(_network_host_list[i]);
		}
	}

	w = AllocateWindowDesc(&_network_game_window_desc);
	ttd_strlcpy(_edit_str_buf, _network_player_name, MAX_QUERYSTR_LEN);
	w->vscroll.cap = 8;

	WP(w, querystr_d).text.caret = true;
	WP(w, querystr_d).text.maxlength = MAX_QUERYSTR_LEN - 1;
	WP(w, querystr_d).text.maxwidth = 120;
	WP(w, querystr_d).text.buf = _edit_str_buf;
	UpdateTextBufferSize(&WP(w, querystr_d).text);

	UpdateNetworkGameWindow(true);
}

static const StringID _players_dropdown[] = {
	STR_NETWORK_2_CLIENTS,
	STR_NETWORK_3_CLIENTS,
	STR_NETWORK_4_CLIENTS,
	STR_NETWORK_5_CLIENTS,
	STR_NETWORK_6_CLIENTS,
	STR_NETWORK_7_CLIENTS,
	STR_NETWORK_8_CLIENTS,
	STR_NETWORK_9_CLIENTS,
	STR_NETWORK_10_CLIENTS,
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
	NSSWND_START = 64,
	NSSWND_ROWSIZE = 12
};

static void NetworkStartServerWindowWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: /* focus input box */
		_selected_field = 3;
		_network_game_info.use_password = (_network_server_password[0] == '\0') ? 0 : 1;
		break;

	case WE_PAINT: {
		int y = NSSWND_START, pos;
		const FiosItem *item;

		SetDParam(7, STR_NETWORK_LAN_INTERNET + _network_advertise);
		SetDParam(9, STR_NETWORK_2_CLIENTS + _network_game_info.clients_max - 2);
		SetDParam(11, STR_NETWORK_LANG_ANY + _network_game_info.server_lang);
		DrawWindowWidgets(w);

		GfxFillRect(11, 63, 259, 171, 0xD7);

		DrawEditBox(w, 3);

		DrawString(10, 22, STR_NETWORK_NEW_GAME_NAME, 2);

		DrawString(10, 43, STR_NETWORK_SELECT_MAP, 2);
		DrawString(280, 63, STR_NETWORK_CONNECTION, 2);
		DrawString(280, 95, STR_NETWORK_NUMBER_OF_CLIENTS, 2);
		DrawString(280, 127, STR_NETWORK_LANGUAGE_SPOKEN, 2);

		if (_network_game_info.use_password) DoDrawString("*", 408, 23, 3);

		// draw list of maps
		pos = w->vscroll.pos;
		while (pos < _fios_num + 1) {
			item = _fios_list + pos - 1;
			if (item == selected_map || (pos == 0 && selected_map == NULL))
				GfxFillRect(11, y - 1, 259, y + 10, 155); // show highlighted item with a different colour

			if (pos == 0) DrawString(14, y, STR_4010_GENERATE_RANDOM_NEW_GAME, 9);
			else DoDrawString(item->title[0] ? item->title : item->name, 14, y, _fios_colors[item->type] );
			pos++;
			y += NSSWND_ROWSIZE;

			if (y >= w->vscroll.cap * NSSWND_ROWSIZE + NSSWND_START) break;
		}
	}	break;

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {
		case 0: case 15: /* Close 'X' | Cancel button */
			ShowNetworkGameWindow();
			break;
		case 4: { /* Set password button */
			StringID str;
			str = AllocateName(_network_server_password, 0);
			ShowQueryString(str, STR_NETWORK_SET_PASSWORD, 20, 250, w->window_class, w->window_number);
			DeleteName(str);
			} break;
		case 5: { /* Select map */
			int y = (e->click.pt.y - NSSWND_START) / NSSWND_ROWSIZE;
			if ((y += w->vscroll.pos) >= w->vscroll.count)
				return;
			if (y == 0) selected_map = NULL;
			else selected_map = _fios_list + y-1;
			SetWindowDirty(w);
			} break;
		case 7: case 8: /* Connection type */
			ShowDropDownMenu(w, _connection_types_dropdown, _network_advertise, 8, 0, 0); // do it for widget 8
			break;
		case 9: case 10: /* Number of Players */
			ShowDropDownMenu(w, _players_dropdown, _network_game_info.clients_max - 2, 10, 0, 0); // do it for widget 10
			return;
		case 11: case 12: /* Language */
			ShowDropDownMenu(w, _language_dropdown, _network_game_info.server_lang, 12, 0, 0); // do it for widget 12
			return;
		case 13: /* Start game */
			_is_network_server = true;
			ttd_strlcpy(_network_server_name, WP(w, querystr_d).text.buf, sizeof(_network_server_name));
			UpdateTextBufferSize(&WP(w, querystr_d).text);
			if (selected_map == NULL) { // start random new game
				GenRandomNewGame(Random(), InteractiveRandom());
			} else { // load a scenario
				char *name;
				if ((name = FiosBrowseTo(selected_map)) != NULL) {
					SetFiosType(selected_map->type);
					strcpy(_file_to_saveload.name, name);
					snprintf(_network_game_info.map_name, sizeof(_network_game_info.map_name), "Loaded scenario");
					DeleteWindow(w);
					StartScenarioEditor(Random(), InteractiveRandom());
				}
			}
			break;
		case 14: /* Load game */
			_is_network_server = true;
			ttd_strlcpy(_network_server_name, WP(w, querystr_d).text.buf, sizeof(_network_server_name));
			UpdateTextBufferSize(&WP(w, querystr_d).text);
			snprintf(_network_game_info.map_name, sizeof(_network_game_info.map_name), "Loaded game");
			/* XXX - WC_NETWORK_WINDOW should stay, but if it stays, it gets
			 * copied all the elements of 'load game' and upon closing that, it segfaults */
			DeleteWindowById(WC_NETWORK_WINDOW, 0);
			ShowSaveLoadDialog(SLD_LOAD_GAME);
			break;
		}
		break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		switch(e->dropdown.button) {
			case 8:
				_network_advertise = (e->dropdown.index == 0) ? false : true;
				break;
			case 10:
				_network_game_info.clients_max = e->dropdown.index + 2;
				break;
			case 12:
				_network_game_info.server_lang = e->dropdown.index;
				break;
		}

		SetWindowDirty(w);
		break;

	case WE_MOUSELOOP:
		if (_selected_field == 3)
			HandleEditBox(w, 3);
		break;

	case WE_KEYPRESS:
		if (_selected_field == 3)
			HandleEditBoxKey(w, 3, e);
		break;

	case WE_ON_EDIT_TEXT: {
		const char *b = e->edittext.str;
		ttd_strlcpy(_network_server_password, b, sizeof(_network_server_password));
		_network_game_info.use_password = (_network_server_password[0] == '\0') ? 0 : 1;
		SetWindowDirty(w);
	} break;
	}
}

static const Widget _network_start_server_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,											STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   419,     0,    13, STR_NETWORK_START_GAME_WINDOW,	STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,     0,   419,    14,   199, 0x0,														STR_NULL},

{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   100,   272,    22,    33, 0x0,														STR_NETWORK_NEW_GAME_NAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   285,   405,    22,    33, STR_NETWORK_SET_PASSWORD,			STR_NETWORK_PASSWORD_TIP},

{          WWT_6,   RESIZE_NONE,   BGC,    10,   271,    62,   172, 0x0,														STR_NETWORK_SELECT_MAP_TIP},
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   260,   271,    63,   171, 0x0,														STR_0190_SCROLL_BAR_SCROLLS_LIST},

{          WWT_6,   RESIZE_NONE,   BGC,   280,   410,    77,    88, STR_NETWORK_COMBO1,						STR_NETWORK_CONNECTION_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,   399,   409,    78,    87, STR_0225,											STR_NETWORK_CONNECTION_TIP},

{          WWT_6,   RESIZE_NONE,   BGC,   280,   410,   109,   120, STR_NETWORK_COMBO2,						STR_NETWORK_NUMBER_OF_CLIENTS_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,   399,   409,   110,   119, STR_0225,											STR_NETWORK_NUMBER_OF_CLIENTS_TIP},

{          WWT_6,   RESIZE_NONE,   BGC,   280,   410,   141,   152, STR_NETWORK_COMBO3,						STR_NETWORK_LANGUAGE_TIP},
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,   399,   409,   142,   151, STR_0225,											STR_NETWORK_LANGUAGE_TIP},

{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    40,   140,   180,   191, STR_NETWORK_START_GAME,				STR_NETWORK_START_GAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   150,   250,   180,   191, STR_NETWORK_LOAD_GAME,					STR_NETWORK_LOAD_GAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   260,   360,   180,   191, STR_012E_CANCEL,								STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_start_server_window_desc = {
	WDP_CENTER, WDP_CENTER, 420, 200,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESTORE_DPARAM,
	_network_start_server_window_widgets,
	NetworkStartServerWindowWndProc,
};

static void ShowNetworkStartServerWindow(void)
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	w = AllocateWindowDesc(&_network_start_server_window_desc);
	ttd_strlcpy(_edit_str_buf, _network_server_name, MAX_QUERYSTR_LEN);

	_saveload_mode = SLD_NEW_GAME;
	BuildFileList();
	w->vscroll.cap = 9;
	w->vscroll.count = _fios_num+1;

	WP(w, querystr_d).text.caret = true;
	WP(w, querystr_d).text.maxlength = MAX_QUERYSTR_LEN - 1;
	WP(w, querystr_d).text.maxwidth = 160;
	WP(w, querystr_d).text.buf = _edit_str_buf;
	UpdateTextBufferSize(&WP(w, querystr_d).text);
}

static byte NetworkLobbyFindCompanyIndex(byte pos)
{
	byte i;
	/* Scroll through all _network_player_info and get the 'pos' item
	    that is not empty */
	for (i = 0; i < MAX_PLAYERS; i++) {
		if (_network_player_info[i].company_name[0] != '\0') {
			if (pos-- == 0)
				return i;
		}
	}

	return 0;
}

static void NetworkLobbyWindowWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		int y = NET_PRC__OFFSET_TOP_WIDGET_COMPANY, pos;
		StringID str;

		w->disabled_state = (_selected_company_item == -1) ? 1 << 7 : 0;

		if (_network_lobby_company_count == MAX_PLAYERS)
			SETBIT(w->disabled_state, 8);
		/* You can not join a server as spectator when it has no companies active..
		     it causes some nasty crashes */
		if (_network_lobby_company_count == 0)
			SETBIT(w->disabled_state, 9);

		DrawWindowWidgets(w);

		SetDParam(0, _str_game_name);
		DrawString(10, 22, STR_NETWORK_PREPARE_TO_JOIN, 2);

		// draw company list
		GfxFillRect(11, 41, 154, 165, 0xD7);
		pos = w->vscroll.pos;
		while (pos < _network_lobby_company_count) {
			byte index = NetworkLobbyFindCompanyIndex(pos);
			bool income = false;
			if (_selected_company_item == index)
				GfxFillRect(11, y - 1, 154, y + 10, 155); // show highlighted item with a different colour

			DoDrawString(_network_player_info[index].company_name, 13, y, 2);
			if(_network_player_info[index].use_password != 0)
				DrawSprite(SPR_LOCK, 135, y);

			/* If the company's income was positive puts a green dot else a red dot */
			if ((_network_player_info[index].income) >= 0)
				income = true;
			DrawSprite(SPR_BLOT | (income ? PALETTE_TO_GREEN : PALETTE_TO_RED), 145, y);

			pos++;
			y += NET_PRC__SIZE_OF_ROW_COMPANY;
			if (pos >= w->vscroll.cap)
				break;
		}

		// draw info about selected company
		DrawStringMultiCenter(290, 48, STR_NETWORK_COMPANY_INFO, 0);
		if (_selected_company_item != -1) { // if a company is selected...
			// show company info
			const uint x = 183;
			uint xm;
			y = 65;

			str = AllocateName(_network_player_info[_selected_company_item].company_name, 0);
			SetDParam(0, str);
			DrawString(x, y, STR_NETWORK_COMPANY_NAME, 2);
			DeleteName(str);
			y += 10;

			SetDParam(0, _network_player_info[_selected_company_item].inaugurated_year + MAX_YEAR_BEGIN_REAL);
			DrawString(x, y, STR_NETWORK_INAUGURATION_YEAR, 2); // inauguration year
			y += 10;

			SetDParam64(0, _network_player_info[_selected_company_item].company_value);
			DrawString(x, y, STR_NETWORK_VALUE, 2); // company value
			y += 10;

			SetDParam64(0, _network_player_info[_selected_company_item].money);
			DrawString(x, y, STR_NETWORK_CURRENT_BALANCE, 2); // current balance
			y += 10;

			SetDParam64(0, _network_player_info[_selected_company_item].income);
			DrawString(x, y, STR_NETWORK_LAST_YEARS_INCOME, 2); // last year's income
			y += 10;

			SetDParam(0, _network_player_info[_selected_company_item].performance);
			DrawString(x, y, STR_NETWORK_PERFORMANCE, 2); // performance
			y += 10;

			SetDParam(0, _network_player_info[_selected_company_item].num_vehicle[0]);
			SetDParam(1, _network_player_info[_selected_company_item].num_vehicle[1]);
			SetDParam(2, _network_player_info[_selected_company_item].num_vehicle[2]);
			SetDParam(3, _network_player_info[_selected_company_item].num_vehicle[3]);
			SetDParam(4, _network_player_info[_selected_company_item].num_vehicle[4]);
			DrawString(x, y, STR_NETWORK_VEHICLES, 2); // vehicles
			y += 10;

			SetDParam(0, _network_player_info[_selected_company_item].num_station[0]);
			SetDParam(1, _network_player_info[_selected_company_item].num_station[1]);
			SetDParam(2, _network_player_info[_selected_company_item].num_station[2]);
			SetDParam(3, _network_player_info[_selected_company_item].num_station[3]);
			SetDParam(4, _network_player_info[_selected_company_item].num_station[4]);
			DrawString(x, y, STR_NETWORK_STATIONS, 2); // stations
			y += 10;

			str = AllocateName(_network_player_info[_selected_company_item].players, 0);
			SetDParam(0, str);
			xm = DrawString(x, y, STR_NETWORK_PLAYERS, 2); // players
			DeleteName(str);
			y += 10;
		}
	}	break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 0: case 11: /* Close 'X' | Cancel button */
			ShowNetworkGameWindow();
			break;
		case 3: /* Company list */
			_selected_company_item = (e->click.pt.y - NET_PRC__OFFSET_TOP_WIDGET_COMPANY) / NET_PRC__SIZE_OF_ROW_COMPANY;

			if (_selected_company_item >= w->vscroll.cap) {
				// click out of bounds
				_selected_company_item = -1;
				SetWindowDirty(w);
				return;
			}
			_selected_company_item += w->vscroll.pos;
			if (_selected_company_item >= _network_lobby_company_count) {
				_selected_company_item = -1;
				SetWindowDirty(w);
				return;
			}

			_selected_company_item = NetworkLobbyFindCompanyIndex(_selected_company_item);

			SetWindowDirty(w);
			break;
		case 7: /* Join company */
			if (_selected_company_item != -1) {
				_network_playas = _selected_company_item + 1;
				NetworkClientConnectGame(_network_last_host, _network_last_port);
			}
			break;
		case 8: /* New company */
			_network_playas = 0;
			NetworkClientConnectGame(_network_last_host, _network_last_port);
			break;
		case 9: /* Spectate game */
			_network_playas = OWNER_SPECTATOR;
			NetworkClientConnectGame(_network_last_host, _network_last_port);
			break;
		case 10: /* Refresh */
			NetworkQueryServer(_network_last_host, _network_last_port, false);
			break;
		}	break;

	case WE_CREATE:
		_selected_company_item = -1;
	}
}

static const Widget _network_lobby_window_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,   BGC,     0,    10,     0,    13, STR_00C5,									STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   RESIZE_NONE,   BGC,    11,   419,     0,    13, STR_NETWORK_GAME_LOBBY,		STR_NULL},
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,     0,   419,    14,   209, 0x0,												STR_NULL},

// company list
{          WWT_6,   RESIZE_NONE,   BGC,    10,   167,    40,   166, 0x0,												STR_NETWORK_COMPANY_LIST_TIP},
{  WWT_SCROLLBAR,   RESIZE_NONE,   BGC,   155,   166,    41,   165, 0x1,												STR_0190_SCROLL_BAR_SCROLLS_LIST},

// company/player info
{     WWT_IMGBTN,   RESIZE_NONE,   BGC,   173,   404,    38,   166, 0x0,					STR_NULL},
{          WWT_6,   RESIZE_NONE,   BGC,   174,   403,    39,   165, 0x0,					STR_NULL},

// buttons
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   151,   175,   186, STR_NETWORK_JOIN_COMPANY,	STR_NETWORK_JOIN_COMPANY_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,    10,   151,   190,   201, STR_NETWORK_NEW_COMPANY,		STR_NETWORK_NEW_COMPANY_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   158,   268,   175,   186, STR_NETWORK_SPECTATE_GAME,	STR_NETWORK_SPECTATE_GAME_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   158,   268,   190,   201, STR_NETWORK_REFRESH,				STR_NETWORK_REFRESH_TIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,   BTC,   278,   388,   175,   186, STR_012E_CANCEL,						STR_NULL},

{   WIDGETS_END},
};

static const WindowDesc _network_lobby_window_desc = {
	WDP_CENTER, WDP_CENTER, 420, 210,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_lobby_window_widgets,
	NetworkLobbyWindowWndProc,
};


static void ShowNetworkLobbyWindow(void)
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	_network_lobby_company_count = 0;

	NetworkQueryServer(_network_last_host, _network_last_port, false);

	w = AllocateWindowDesc(&_network_lobby_window_desc);
	strcpy(_edit_str_buf, "");
	w->vscroll.pos = 0;
	w->vscroll.cap = 8;
}




// The window below gives information about the connected clients
//  and also makes able to give money to them, kick them (if server)
//  and stuff like that.

extern void DrawPlayerIcon(int p, int x, int y);

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
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                 STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   249,     0,    13, STR_NETWORK_CLIENT_LIST,  STR_018C_WINDOW_TITLE_DRAG_THIS},

{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   249,    14,    14 + CLNWND_ROWSIZE + 1, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const Widget _client_list_popup_widgets[] = {
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   99,     0,     0,     0,	STR_NULL},
{   WIDGETS_END},
};

static WindowDesc _client_list_desc = {
	-1, -1, 250, 1,
	WC_CLIENT_LIST,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_client_list_widgets,
	ClientListWndProc
};

// Finds the Xth client-info that is active
static NetworkClientInfo *NetworkFindClientInfo(byte client_no)
{
	NetworkClientInfo *ci;
	for (ci = _network_client_info; ci != &_network_client_info[MAX_CLIENT_INFO]; ci++) {
		// Skip non-active items
		if (ci->client_index == NETWORK_EMPTY_INDEX) continue;
		if (client_no == 0) return ci;
		client_no--;
	}

	return NULL;
}

// Here we start to define the options out of the menu
static void ClientList_Kick(byte client_no)
{
	if (client_no < MAX_PLAYERS)
		SEND_COMMAND(PACKET_SERVER_ERROR)(&_clients[client_no], NETWORK_ERROR_KICKED);
}

static void ClientList_Ban(byte client_no)
{
	uint i;
	uint32 ip = NetworkFindClientInfo(client_no)->client_ip;

	for (i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] == NULL || _network_ban_list[i][0] == '\0') {
			_network_ban_list[i] = strdup(inet_ntoa(*(struct in_addr *)&ip));
			break;
		}
	}

	if (client_no < MAX_PLAYERS)
		SEND_COMMAND(PACKET_SERVER_ERROR)(&_clients[client_no], NETWORK_ERROR_KICKED);
}

static void ClientList_GiveMoney(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL)
		ShowNetworkGiveMoneyWindow(NetworkFindClientInfo(client_no)->client_playas - 1);
}

static void ClientList_SpeakToClient(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL)
		ShowNetworkChatQueryWindow(DESTTYPE_CLIENT, NetworkFindClientInfo(client_no)->client_index);
}

static void ClientList_SpeakToPlayer(byte client_no)
{
	if (NetworkFindClientInfo(client_no) != NULL)
		ShowNetworkChatQueryWindow(DESTTYPE_PLAYER, NetworkFindClientInfo(client_no)->client_playas);
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
	NetworkClientInfo *ci;

	// Should be replaced with a loop through all clients
	for (ci = _network_client_info; ci != &_network_client_info[MAX_CLIENT_INFO]; ci++) {
		// Skip non-active items
		if (ci->client_index == NETWORK_EMPTY_INDEX) continue;
		num++;
	}

	num *= CLNWND_ROWSIZE;

	// If height is changed
	if (w->height != CLNWND_OFFSET + num + 1) {
		// XXX - magic unfortunately; (num + 2) has to be one bigger than heigh (num + 1)
		SetWindowDirty(w);
		w->widget[2].bottom = w->widget[2].top + num + 2;
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
	NetworkClientInfo *ci;
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
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_CLIENT);
		_clientlist_proc[i++] = &ClientList_SpeakToClient;
	}

	if (ci->client_playas >= 1 && ci->client_playas <= MAX_PLAYERS) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_COMPANY);
		_clientlist_proc[i++] = &ClientList_SpeakToPlayer;
	}
	GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_SPEAK_TO_ALL);
	_clientlist_proc[i++] = &ClientList_SpeakToAll;

	if (_network_own_client_index != ci->client_index) {
		if (_network_playas >= 1 && _network_playas <= MAX_PLAYERS) {
			// We are no spectator
			if (ci->client_playas >= 1 && ci->client_playas <= MAX_PLAYERS) {
				GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_GIVE_MONEY);
				_clientlist_proc[i++] = &ClientList_GiveMoney;
			}
		}
	}

	// A server can kick clients (but not hisself)
	if (_network_server && _network_own_client_index != ci->client_index) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_KICK);
		_clientlist_proc[i++] = &ClientList_Kick;

		sprintf(_clientlist_action[i],"Ban");
		_clientlist_proc[i++] = &ClientList_Ban;
	}

	if (i == 0) {
		GetString(_clientlist_action[i], STR_NETWORK_CLIENTLIST_NONE);
		_clientlist_proc[i++] = &ClientList_None;
	}

	/* Calculate the height */
	h = ClientListPopupHeigth();

	// Allocate the popup
	w = AllocateWindow(x, y, 100, h + 1, ClientListPopupWndProc, WC_TOOLBAR_MENU, _client_list_popup_widgets);
	w->widget[0].bottom = w->widget[0].top + h;

	w->flags4 &= ~WF_WHITE_BORDER_MASK;
	WP(w,menu_d).item_count = 0;
	// Save our client
	WP(w,menu_d).main_button = client_no;
	WP(w,menu_d).sel_index = 0;
	// We are a popup
	_popup_menu_active = true;

	return w;
}

// Main handle for the popup
static void ClientListPopupWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
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
				GfxFillRect(1, y, 98, y + CLNWND_ROWSIZE - 1, 0);
				colour = 0xC;
			} else colour = 0x10;

			DoDrawString(_clientlist_action[i], 4, y, colour);
		}
	}	break;

	case WE_POPUPMENU_SELECT: {
		// We selected an action
		int index = (e->popupmenu.pt.y - w->top) / CLNWND_ROWSIZE;

		if (index >= 0 && e->popupmenu.pt.y >= w->top)
			HandleClientListPopupClick(index, WP(w,menu_d).main_button);

		// Sometimes, because of the bad DeleteWindow-proc, the 'w' pointer is
		//  invalid after the last functions (mostly because it kills a window
		//  that is in front of 'w', and because of a silly memmove, the address
		//  'w' was pointing to becomes invalid), so we need to refetch
		//  the right address...
		DeleteWindowById(WC_TOOLBAR_MENU, 0);
	}	break;

	case WE_POPUPMENU_OVER: {
		// Our mouse hoovers over an action? Select it!
		int index = (e->popupmenu.pt.y - w->top) / CLNWND_ROWSIZE;

		if (index == -1 || index == WP(w,menu_d).sel_index)
			return;

		WP(w,menu_d).sel_index = index;
		SetWindowDirty(w);
	} break;

	}
}

// Main handle for clientlist
static void ClientListWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
		NetworkClientInfo *ci;
		int y, i = 0;
		byte colour;

		// Check if we need to reset the height
		if (!CheckClientListHeight(w)) break;

		DrawWindowWidgets(w);

		y = CLNWND_OFFSET;

		for (ci = _network_client_info; ci != &_network_client_info[MAX_CLIENT_INFO]; ci++) {
			// Skip non-active items
			if (ci->client_index == NETWORK_EMPTY_INDEX) continue;

			if (_selected_clientlist_item == i++) { // Selected item, highlight it
				GfxFillRect(1, y, 248, y + CLNWND_ROWSIZE - 1, 0);
				colour = 0xC;
			} else
				colour = 0x10;

			if (ci->client_index == NETWORK_SERVER_INDEX) {
				DrawString(4, y, STR_NETWORK_SERVER, colour);
			} else
				DrawString(4, y, STR_NETWORK_CLIENT, colour);

			// Filter out spectators
			if (ci->client_playas > 0 && ci->client_playas <= MAX_PLAYERS)
				DrawPlayerIcon(ci->client_playas - 1, 44, y + 1);

			DoDrawString(ci->client_name, 61, y, colour);

			y += CLNWND_ROWSIZE;
		}
	}	break;

	case WE_CLICK:
		// Show the popup with option
		if (_selected_clientlist_item != 255) {
			PopupClientList(w, _selected_clientlist_item, e->click.pt.x + w->left, e->click.pt.y + w->top);
		}

		break;

	case WE_MOUSEOVER:
		// -1 means we left the current window
		if (e->mouseover.pt.y == -1) {
			_selected_clientlist_y = 0;
			_selected_clientlist_item = 255;
			SetWindowDirty(w);
			break;
		}
		// It did not change.. no update!
		if (e->mouseover.pt.y == _selected_clientlist_y) break;

		// Find the new selected item (if any)
		_selected_clientlist_y = e->mouseover.pt.y;
		if (e->mouseover.pt.y > CLNWND_OFFSET) {
			_selected_clientlist_item = (e->mouseover.pt.y - CLNWND_OFFSET) / CLNWND_ROWSIZE;
		} else
			_selected_clientlist_item = 255;

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
	Window *w = AllocateWindowDescFront(&_client_list_desc, 0);
	if (w)
		w->window_number = 0;
}

extern void SwitchMode(int new_mode);

static void NetworkJoinStatusWindowWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
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
				progress = 15 + _network_join_kbytes * (100 - 15) / _network_join_kbytes_total;
		}

		/* Draw nice progress bar :) */
		DrawFrameRect(20, 18, (int)((w->width - 20) * progress / 100), 28, 10, 0);
	}	break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 0: case 3: /* Close 'X' | Disconnect button */
			NetworkDisconnect();
			DeleteWindowById(WC_NETWORK_STATUS_WINDOW, 0);
			SwitchMode(SM_MENU);
			ShowNetworkGameWindow();
			break;
		}
		break;

	}
}

static const Widget _network_join_status_window_widget[] = {
{    WWT_TEXTBTN,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   249,     0,    13, STR_NETWORK_CONNECTING, STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   249,    14,    84, 0x0,STR_NULL},
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



#define MAX_QUERYSTR_LEN 64

static void ChatWindowWndProc(Window *w, WindowEvent *e)
{
	static bool closed = false;
	switch (e->event) {
	case WE_CREATE:
		SendWindowMessage(WC_NEWS_WINDOW, 0, WE_CREATE, w->height, 0);
		closed = false;
		break;

	case WE_PAINT:
		DrawWindowWidgets(w);
		DrawEditBox(w, 1);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3: DeleteWindow(w); break; // Cancel
		case 2: // Send
press_ok:;
			if (strcmp(WP(w, querystr_d).text.buf, WP(w, querystr_d).text.buf + MAX_QUERYSTR_LEN) == 0) {
				DeleteWindow(w);
			} else {
				char *buf = WP(w, querystr_d).text.buf;
				WindowClass wnd_class = WP(w, querystr_d).wnd_class;
				WindowNumber wnd_num = WP(w, querystr_d).wnd_num;
				Window *parent;

				// Mask the edit-box as closed, so we don't send out a CANCEL
				closed = true;

				DeleteWindow(w);

				parent = FindWindowById(wnd_class, wnd_num);
				if (parent != NULL) {
					WindowEvent e;
					e.event = WE_ON_EDIT_TEXT;
					e.edittext.str = buf;
					parent->wndproc(parent, &e);
				}
			}
			break;
		}
		break;

	case WE_MOUSELOOP: {
		if (!FindWindowById(WP(w,querystr_d).wnd_class, WP(w,querystr_d).wnd_num)) {
			DeleteWindow(w);
			return;
		}
		HandleEditBox(w, 1);
	} break;

	case WE_KEYPRESS: {
		switch(HandleEditBoxKey(w, 1, e)) {
		case 1: // Return
			goto press_ok;
		case 2: // Escape
			DeleteWindow(w);
			break;
		}
	} break;

	case WE_DESTROY:
		SendWindowMessage(WC_NEWS_WINDOW, 0, WE_DESTROY, 0, 0);
		// If the window is not closed yet, it means it still needs to send a CANCEL
		if (!closed) {
			Window *parent = FindWindowById(WP(w,querystr_d).wnd_class, WP(w,querystr_d).wnd_num);
			if (parent != NULL) {
				WindowEvent e;
				e.event = WE_ON_EDIT_TEXT_CANCEL;
				parent->wndproc(parent, &e);
			}
		}
		break;
	}
}

static const Widget _chat_window_widgets[] = {
{     WWT_IMGBTN,   RESIZE_NONE,    14,     0,   639,     0,    13, STR_NULL,         STR_NULL}, // background
{     WWT_IMGBTN,   RESIZE_NONE,    14,     2,   399,     1,    12, STR_NULL,         STR_NULL}, // text box
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   400,   519,     1,    12, STR_NETWORK_SEND, STR_NULL}, // send button
{    WWT_TEXTBTN,   RESIZE_NONE,    14,   520,   639,     1,    12, STR_012E_CANCEL,  STR_NULL}, // cancel button
{   WIDGETS_END},
};

static const WindowDesc _chat_window_desc = {
	WDP_CENTER, -26, 640, 14, // x, y, width, height
	WC_SEND_NETWORK_MSG,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET,
	_chat_window_widgets,
	ChatWindowWndProc
};

void ShowChatWindow(StringID str, StringID caption, int maxlen, int maxwidth, byte window_class, uint16 window_number)
{
	Window *w;

#define _orig_edit_str_buf (_edit_str_buf+MAX_QUERYSTR_LEN)

	DeleteWindowById(WC_SEND_NETWORK_MSG, 0);

	GetString(_orig_edit_str_buf, str);

	_orig_edit_str_buf[maxlen] = '\0';

	memcpy(_edit_str_buf, _orig_edit_str_buf, MAX_QUERYSTR_LEN);

	w = AllocateWindowDesc(&_chat_window_desc);

	w->click_state = 1 << 1;
	WP(w,querystr_d).caption = caption;
	WP(w,querystr_d).wnd_class = window_class;
	WP(w,querystr_d).wnd_num = window_number;
	WP(w,querystr_d).text.caret = false;
	WP(w,querystr_d).text.maxlength = maxlen - 1;
	WP(w,querystr_d).text.maxwidth = maxwidth;
	WP(w,querystr_d).text.buf = _edit_str_buf;
	UpdateTextBufferSize(&WP(w, querystr_d).text);
}

#else
void ShowJoinStatusWindowAfterJoin(void) {}
#endif /* ENABLE_NETWORK */
