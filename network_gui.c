#include "stdafx.h"
#include "ttd.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "command.h"
#include "network.h"

#define BGC 5
#define BTC 15
#define MAX_QUERYSTR_LEN 64
static byte _edit_str_buf[MAX_QUERYSTR_LEN*2];
static void ShowNetworkStartServerWindow();
#if 0
static void ShowNetworkLobbyWindow();
#endif

static byte _selected_field;

static const StringID _connection_types_dropdown[] = {
	STR_NETWORK_LAN,
	STR_NETWORK_INTERNET,
	INVALID_STRING_ID
};

/* Should be _network_game->players_max but since network is not yet really done
* we'll just use some dummy here
* network.c -->> static NetworkGameInfo _network_game;
*/
static byte _players_max;
/* Should be ??????????? (something) but since network is not yet really done
* we'll just use some dummy here
*/
static byte _network_connection;
static uint16 _network_game_count_last;

enum {
  NET_PRC__OFFSET_TOP_WIDGET	= 93,
	NET_PRC__SIZE_OF_ROW				= 14,
};

static NetworkGameList *selected_item = NULL;

static void NetworkGameWindowWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {

		SET_DPARAM16(0, 0x00);
		SET_DPARAM16(2, STR_NETWORK_LAN + _network_connection);
		DrawWindowWidgets(w);

		DrawEditBox(w, 6);

		DrawString(9, 43, STR_NETWORK_PLAYER_NAME, 2);
		DrawString(9, 63, STR_NETWORK_SELECT_CONNECTION, 2);

		DrawString(15, 82, STR_NETWORK_GAME_NAME, 2);
		DrawString(238, 82, STR_NETWORK_PLAYERS, 2);
		DrawString(288, 82, STR_NETWORK_MAP_SIZE, 2);

		{ // draw list of games
			uint16 y = NET_PRC__OFFSET_TOP_WIDGET + 3;
			int32 n = 0;
			NetworkGameList *cur_item = _network_game_list;
			while (cur_item != NULL) {
				if (cur_item == selected_item)
					GfxFillRect(11, y - 2, 380, y + 9, 10); // show highlighted item with a different colour

				DoDrawString(cur_item->item.server_name, 15, y, 16); // server name

				SET_DPARAM8(0, cur_item->item.players_on);
				SET_DPARAM8(1, cur_item->item.players_max);
				DrawString(238, y, STR_NETWORK_PLAYERS_VAL, 2); // #/#

				DoDrawString(cur_item->item.map_name, 288, y, 16); // map size
				cur_item = cur_item->_next;
				y += NET_PRC__SIZE_OF_ROW;
				if (++n == w->vscroll.cap) { break;} // max number of games in the window
			}
		}
	}	break;

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {
		case 0: case 15:  /* Close 'X' | Cancel button */
			DeleteWindowById(WC_NETWORK_WINDOW, 0);
			NetworkLobbyShutdown();
			break;
		case 3: { /* Find server automaticaly */
			NetworkCoreConnectGame("auto", _network_server_port);
		}	break;
		case 4: { /* Connect via direct ip */
				StringID str;
				str = AllocateName((byte*)_decode_parameters, 0);

				ShowQueryString(
				str,
				STR_NETWORK_ENTER_IP,
				50,  // maximum 50 characters OR
				250, // characters up to width 250 pixels, whichever is satisfied first
				w->window_class,
				w->window_number);
				DeleteName(str);
		} break;
		case 5: /* Start server */
			ShowNetworkStartServerWindow();
			break;
		case 7: case 8: /* Connection type */
			ShowDropDownMenu(w, _connection_types_dropdown, _network_connection, 8, 0); // do it for widget 8
			return;
		case 14: { /* Matrix to show networkgames */
			uint32 id_v = (e->click.pt.y - NET_PRC__OFFSET_TOP_WIDGET) / NET_PRC__SIZE_OF_ROW;

			if (id_v >= w->vscroll.cap) { return;} // click out of bounds

			id_v += w->vscroll.pos;

			{
				NetworkGameList *cur_item = _network_game_list;
				for (; id_v > 0 && cur_item != NULL; id_v--)
					cur_item = cur_item->_next;

				if (cur_item == NULL) { return;} // click out of vehicle bounds

				selected_item = cur_item;
			}
		} break;
		case 16: /* Join Game */
			if (selected_item != NULL)
				NetworkCoreConnectGameStruct(selected_item);
			break;
		}
		break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		_network_connection = e->dropdown.index;
		switch (_network_connection) {
		case 0: /* LAN */
			NetworkGameListFromLAN();
			break;
		case 1: /* Internet */
			NetworkGameListFromInternet();
			break;
		}

		_network_game_count_last = _network_game_count;
		SetWindowDirty(w);

		break;

	case WE_MOUSELOOP:
		if (_selected_field == 6)
			HandleEditBox(w, 6);

		break;

	case WE_KEYPRESS:
		if (_selected_field != 6)
			break;

		switch (HandleEditBoxKey(w, 6, e)) {
		case 1:
			HandleButtonClick(w, 9);
			break;
		}
		break;

	case WE_ON_EDIT_TEXT: {
		byte *b = e->edittext.str;
		if (*b != 0) {
			byte * ip = NULL;
			byte * port = NULL;
			byte * player = NULL;
			byte c;
			uint16 rport;

			rport = _network_server_port;
			c = 0;
			ip = b;

			while (b[c] != 0) {
				if (((char)b[c]) == '#') {
					player = &b[c+1];
					b[c] = 0;
					}
				if (((char)b[c]) == ':') {
					port = &b[c+1];
					b[c] = 0;
					}
				c++;
				}

			if (player!=NULL) _network_playas = atoi(player);
			if (port!=NULL) rport = atoi(port);

			NetworkCoreConnectGame(b, rport);
		}
	} break;

	case WE_TICK: {
		if (_network_game_count_last != _network_game_count)
			SetWindowDirty(w);
	} break;

	}
}

static const Widget _network_game_window_widgets[] = {
{   WWT_CLOSEBOX,   BGC,     0,    10,     0,    13, STR_00C5,										STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   BGC,    10,   399,     0,    13, STR_NETWORK_MULTIPLAYER,			STR_NULL},
{     WWT_IMGBTN,   BGC,     0,   399,    14,   199, 0x0,													STR_NULL},

{ WWT_PUSHTXTBTN,   BTC,    20,   130,    22,    33, STR_NETWORK_FIND_SERVER,			STR_NETWORK_FIND_SERVER_TIP},
{ WWT_PUSHTXTBTN,   BTC,   145,   255,    22,    33, STR_NETWORK_DIRECT_CONNECT,	STR_NETWORK_DIRECT_CONNECT_TIP},
{ WWT_PUSHTXTBTN,   BTC,   270,   380,    22,    33, STR_NETWORK_START_SERVER,		STR_NETWORK_START_SERVER_TIP},

{     WWT_IMGBTN,   BGC,   250,   394,    42,    53, 0x0,													STR_NETWORK_ENTER_NAME_TIP},

{          WWT_6,   BGC,   250,   393,    62,    73, STR_NETWORK_COMBO1,					STR_NETWORK_CONNECTION_TYPE_TIP},
{   WWT_CLOSEBOX,   BGC,   382,   392,    63,    72, STR_0225,										STR_NETWORK_CONNECTION_TYPE_TIP},

{  WWT_SCROLLBAR,   BGC,   382,   392,    81,   176, 0x0,													STR_0190_SCROLL_BAR_SCROLLS_LIST},

{     WWT_IMGBTN,   BTC,    10,   231,    81,    92, 0x0,													STR_NETWORK_GAME_NAME_TIP },
{     WWT_IMGBTN,   BTC,   232,   281,    81,    92, 0x0,													STR_NETWORK_PLAYERS_TIP },
{     WWT_IMGBTN,   BTC,   282,   331,    81,    92, 0x0,													STR_NETWORK_MAP_SIZE_TIP },
{     WWT_IMGBTN,   BTC,   332,   381,    81,    92, 0x0,													STR_NETWORK_INFO_ICONS_TIP },

{     WWT_MATRIX,   BGC,    10,   381,    93,   176, 0x601,												STR_NETWORK_CLICK_GAME_TO_SELECT},

{ WWT_PUSHTXTBTN,   BTC,   145,   255,   180,   191, STR_012E_CANCEL,							STR_NULL},
{ WWT_PUSHTXTBTN,   BTC,   270,   392,   180,   191, STR_NETWORK_JOIN_GAME,				STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_game_window_desc = {
	WDP_CENTER, WDP_CENTER, 400, 200,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_game_window_widgets,
	NetworkGameWindowWndProc,
};


void ShowNetworkGameWindow()
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	NetworkLobbyInit();

	w = AllocateWindowDesc(&_network_game_window_desc);
	strcpy(_edit_str_buf, "Your name");
	w->vscroll.cap = 6;
	w->disabled_state = (1<<6) | (1<<7) | (1<<8); // disable buttons not yet working
	NetworkGameListFromLAN(); // default dropdown item is LAN, so fill that array

	_network_game_count_last = _network_game_count;

	WP(w,querystr_d).caret = 1;
	WP(w,querystr_d).maxlen = MAX_QUERYSTR_LEN;
	WP(w,querystr_d).maxwidth = 240;
	WP(w,querystr_d).buf = _edit_str_buf;
}

static const StringID _players_dropdown[] = {
	STR_NETWORK_2_PLAYERS,
	STR_NETWORK_3_PLAYERS,
	STR_NETWORK_4_PLAYERS,
	STR_NETWORK_5_PLAYERS,
	STR_NETWORK_6_PLAYERS,
	STR_NETWORK_7_PLAYERS,
	STR_NETWORK_8_PLAYERS,
	INVALID_STRING_ID
};

static void NetworkStartServerWindowWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {

		SET_DPARAM16(7, STR_NETWORK_2_PLAYERS + _players_max);
		DrawWindowWidgets(w);

		GfxFillRect(11, 63, 237, 168, 0xD7);

		DrawEditBox(w, 3);
		DrawEditBox(w, 4);

		DrawString(10, 22, STR_NETWORK_NEW_GAME_NAME, 2);
		DrawString(210, 22, STR_NETWORK_PASSWORD, 2);

		DrawString(10, 43, STR_NETWORK_SELECT_MAP, 2);
		DrawString(260, 63, STR_NETWORK_NUMBER_OF_PLAYERS, 2);

	}	break;

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {
		case 0: case 12: /* Close 'X' | Cancel button */
			ShowNetworkGameWindow();
			break;
		case 7: case 8: /* Number of Players */
			ShowDropDownMenu(w, _players_dropdown, _players_max, 8, 0); // do it for widget 8
			return;
		case  9: /* Start game */
			NetworkCoreStartGame();
			strcpy(_network_game.server_name,	WP(w,querystr_d).buf);
			//ShowNetworkLobbyWindow();
			DoCommandP(0, 0, 0, NULL, CMD_START_NEW_GAME);
			break;
		case 10: /* Load game */
			NetworkCoreStartGame();
			strcpy(_network_game.server_name,	WP(w,querystr_d).buf);
			//ShowNetworkLobbyWindow();
			ShowSaveLoadDialog(SLD_LOAD_GAME);
			break;
		case 11: /* Load scenario */
			NetworkCoreStartGame();
			strcpy(_network_game.server_name,	WP(w,querystr_d).buf);
			//ShowNetworkLobbyWindow();
			ShowSaveLoadDialog(SLD_LOAD_SCENARIO);
			break;
		}
		break;

	case WE_DROPDOWN_SELECT: /* we have selected a dropdown item in the list */
		_players_max = e->dropdown.index;

		SetWindowDirty(w);
		break;

	case WE_MOUSELOOP:
		if(_selected_field == 3 || _selected_field == 4)
			HandleEditBox(w, _selected_field);

		break;

	case WE_KEYPRESS:
		if(_selected_field != 3 && _selected_field != 4)
			break;
		switch (HandleEditBoxKey(w, _selected_field, e)) {
		case 1:
			HandleButtonClick(w, 9);
			break;
		}
		break;

	}
}

static const Widget _network_start_server_window_widgets[] = {
{   WWT_CLOSEBOX,   BGC,     0,    10,     0,    13, STR_00C5,											STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   BGC,    10,   399,     0,    13, STR_NETWORK_START_GAME_WINDOW,	STR_NULL},
{     WWT_IMGBTN,   BGC,     0,   399,    14,   199, 0x0,														STR_NULL},

{     WWT_IMGBTN,   BGC,    80,   190,    22,    33, 0x0,														STR_NETWORK_NEW_GAME_NAME_TIP},
{     WWT_IMGBTN,   BGC,   280,   390,    22,    33, 0x0,														STR_NETWORK_PASSWORD_TIP},

{     WWT_IMGBTN,   BGC,    10,   240,    62,   170, 0x0,														STR_NETWORK_SELECT_MAP_TIP},
{  WWT_SCROLLBAR,   BGC,   241,   251,    62,   170, 0x0,														STR_0190_SCROLL_BAR_SCROLLS_LIST},

{          WWT_6,   BGC,   260,   390,    81,    92, STR_NETWORK_COMBO2,						STR_NETWORK_NUMBER_OF_PLAYERS_TIP},
{   WWT_CLOSEBOX,   BGC,   379,   389,    82,    91, STR_0225,											STR_NETWORK_NUMBER_OF_PLAYERS_TIP},

{ WWT_PUSHTXTBTN,   BTC,    10,   100,   180,   191, STR_NETWORK_START_GAME,				STR_NETWORK_START_GAME_TIP},
{ WWT_PUSHTXTBTN,   BTC,   110,   200,   180,   191, STR_NETWORK_LOAD_GAME,					STR_NETWORK_LOAD_GAME_TIP},
{ WWT_PUSHTXTBTN,   BTC,   210,   300,   180,   191, STR_NETWORK_LOAD_SCENARIO,			STR_NETWORK_LOAD_SCENARIO_TIP},
{ WWT_PUSHTXTBTN,   BTC,   310,   390,   180,   191, STR_012E_CANCEL,								STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_start_server_window_desc = {
	WDP_CENTER, WDP_CENTER, 400, 200,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_start_server_window_widgets,
	NetworkStartServerWindowWndProc,
};

static void ShowNetworkStartServerWindow()
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	w = AllocateWindowDesc(&_network_start_server_window_desc);
	strcpy(_edit_str_buf, "");
	w->disabled_state = (1<<4) | (1<<5) | (1<<6) | (1<<7) | (1<<8); // disable buttons not yet working

	WP(w,querystr_d).caret = 1;
	WP(w,querystr_d).maxlen = MAX_QUERYSTR_LEN;
	WP(w,querystr_d).maxwidth = 240;
	WP(w,querystr_d).buf = _edit_str_buf;
}

#if 0
static void NetworkLobbyWindowWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {

		SET_DPARAM16(7, STR_NETWORK_2_PLAYERS + _opt_mod_ptr->road_side);
		DrawWindowWidgets(w);

		GfxFillRect( 11,  31, 239, 239, 0xD7);
		GfxFillRect(261,  31, 378, 220, 0xD7);

		DrawEditBox(w, 5);
		DrawEditBox(w, 7);

		DrawString(10, 255, STR_NETWORK_COMPANY_NAME, 2);

		break;
	}

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {

		case 0: // close X
		case 13: // cancel button
			ShowNetworkGameWindow();
			break;

		}

	case WE_MOUSELOOP:
		if(_selected_field == 5)
		{
			HandleEditBox(w, 5);
			break;
		}
		if(_selected_field == 7)
		{
			HandleEditBox(w, 7);
			break;
		}

		break;

	case WE_KEYPRESS:
		if(_selected_field != 5 && _selected_field != 7)
			break;
		switch (HandleEditBoxKey(w, _selected_field, e)) {
		case 1:
			HandleButtonClick(w, 12);
			break;
		}
		break;

	}
}

static const Widget _network_lobby_window_widgets[] = {
{   WWT_CLOSEBOX,   BGC,     0,    10,     0,    13, STR_00C5,									STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   BGC,    10,   399,     0,    13, STR_NETWORK_GAME_LOBBY,		STR_NULL},
{     WWT_IMGBTN,   BGC,     0,   399,    14,   299, 0x0,												STR_NULL},

// chat widget
{     WWT_IMGBTN,   BGC,    10,   240,    30,   240, 0x0,												STR_NULL},
{  WWT_SCROLLBAR,   BGC,   241,   251,    30,   240, 0x0,												STR_0190_SCROLL_BAR_SCROLLS_LIST},

// send message prompt
{     WWT_IMGBTN,   BGC,    10,   200,   241,   252, 0x0,												STR_NETWORK_ENTER_NAME_TIP},
{ WWT_PUSHTXTBTN,   BTC,   201,   251,   241,   252, STR_NETWORK_SEND,					STR_NETWORK_SEND_TIP},

// company name
{     WWT_IMGBTN,   BGC,   100,   251,   254,   265, 0x0,												STR_NETWORK_COMPANY_NAME_TIP},

// player information
{     WWT_IMGBTN,   BGC,   260,   379,    30,   221, 0x0,												STR_NULL},
{  WWT_SCROLLBAR,   BGC,   380,   390,    30,   221, 0x1,												STR_0190_SCROLL_BAR_SCROLLS_LIST},

// buttons
{ WWT_PUSHTXTBTN,   BTC,   260,   390,   233,   244, STR_NETWORK_NEW_COMPANY,		STR_NETWORK_NEW_COMPANY_TIP},
{ WWT_PUSHTXTBTN,   BTC,   260,   390,   254,   265, STR_NETWORK_SPECTATE_GAME,	STR_NETWORK_SPECTATE_GAME_TIP},

{ WWT_PUSHTXTBTN,   BTC,    80,   180,   280,   291, STR_NETWORK_READY,					STR_NULL},
{ WWT_PUSHTXTBTN,   BTC,   220,   320,   280,   291, STR_012E_CANCEL,						STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _network_lobby_window_desc = {
	WDP_CENTER, WDP_CENTER, 400, 300,
	WC_NETWORK_WINDOW,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_network_lobby_window_widgets,
	NetworkLobbyWindowWndProc,
};


static void ShowNetworkLobbyWindow()
{
	Window *w;
	DeleteWindowById(WC_NETWORK_WINDOW, 0);

	w = AllocateWindowDesc(&_network_lobby_window_desc);
	strcpy(_edit_str_buf, "");


	WP(w,querystr_d).caret = 1;
	WP(w,querystr_d).maxlen = MAX_QUERYSTR_LEN;
	WP(w,querystr_d).maxwidth = 240;
	WP(w,querystr_d).buf = _edit_str_buf;
}
#endif
