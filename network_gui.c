#include "stdafx.h"
#include "ttd.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "command.h"

#define BGC 5
#define BTC 15
#define MAX_QUERYSTR_LEN 64
static byte _edit_str_buf[MAX_QUERYSTR_LEN*2];
static void ShowNetworkStartServerWindow();
static void ShowNetworkLobbyWindow();

extern void ShowDropDownMenu(Window *w, const StringID *strings, int selected, int button, uint32 disabled_mask);
extern int HandleEditBoxKey(Window *w, int wid, WindowEvent *we);

void ShowQueryString(StringID str, StringID caption, int maxlen, int maxwidth, byte window_class, uint16 window_number);


static byte _selected_field;
char *direct_ip = NULL;


void ConnectToServer(byte* b)
{
	_networking = true;
	
	NetworkInitialize(b);
	DEBUG(misc, 1) ("Connecting to %s %d\n", b, _network_port);
	NetworkConnect(b, _network_port);
}

static const StringID _connection_types_dropdown[] = {
	STR_NETWORK_LAN,
	STR_NETWORK_INTERNET,
	INVALID_STRING_ID
};

static void NetworkGameWindowWndProc(Window *w, WindowEvent *e)
{
	switch(e->event) {
	case WE_PAINT: {
	
		SET_DPARAM16(0, 0x00);
		SET_DPARAM16(2, STR_NETWORK_LAN + _opt_mod_ptr->road_side);
		DrawWindowWidgets(w);
		
		DrawEditBox(w, 6);
		
		DrawString(9, 43, STR_NETWORK_PLAYER_NAME, 2);
		DrawString(9, 63, STR_NETWORK_SELECT_CONNECTION, 2);

		DrawString(15, 82, STR_NETWORK_GAME_NAME, 2);
		DrawString(238, 82, STR_NETWORK_PLAYERS, 2);
		DrawString(288, 82, STR_NETWORK_MAP_SIZE, 2);
		
		break;
	}

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {

		case 0:  // close X
		case 15: // cancel button
			DeleteWindowById(WC_NETWORK_WINDOW, 0);
			break;
		case 4: // connect via direct ip
			{
				StringID str;
				str = AllocateName((byte*)_decode_parameters, 0);
				
				ShowQueryString(
				str,
				STR_NETWORK_ENTER_IP,
				15,
				160,
				w->window_class,
				w->window_number);
				DeleteName(str);
			}
			break;
			
		case 5: // start server
			ShowNetworkStartServerWindow();
			break;

		case 8:
			ShowDropDownMenu(w, _connection_types_dropdown, _opt_mod_ptr->currency, e->click.widget, 0);
			return;
		}

	case WE_MOUSELOOP:
		if(_selected_field != 6)
			break;
		HandleEditBox(w, 6);
		break;

	case WE_KEYPRESS:
		if(_selected_field != 6)
			break;
		switch (HandleEditBoxKey(w, 6, e)) {
		case 1:
			HandleButtonClick(w, 9);
			break;
		}
		break;

	case WE_ON_EDIT_TEXT: {
		byte *b = e->edittext.str;
		if (*b == 0)
			return;
		ConnectToServer(b);
	} break;

	}
}

static const Widget _network_game_window_widgets[] = {
{   WWT_PUSHTXTBTN, BGC,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   BGC,    10,   399,     0,    13, STR_NETWORK_MULTIPLAYER},
{     WWT_IMGBTN,   BGC,     0,   399,    14,   199, 0x0},

{ WWT_PUSHTXTBTN,   BTC,    20,   130,    22,    33, STR_NETWORK_FIND_SERVER, STR_NETWORK_FIND_SERVER_TIP},
{ WWT_PUSHTXTBTN,   BTC,   145,   255,    22,    33, STR_NETWORK_DIRECT_CONNECT, STR_NETWORK_DIRECT_CONNECT_TIP},
{ WWT_PUSHTXTBTN,   BTC,   270,   380,    22,    33, STR_NETWORK_START_SERVER, STR_NETWORK_START_SERVER_TIP},

{     WWT_IMGBTN,   BGC,   250,   394,    42,    53, 0x0, STR_NETWORK_ENTER_NAME_TIP},

{          WWT_6,   BGC,   250,   393,    62,    73, STR_NETWORK_COMBO1, STR_NETWORK_CONNECTION_TYPE_TIP},
{   WWT_CLOSEBOX,   BGC,   382,   392,    63,    72, STR_0225, STR_NETWORK_CONNECTION_TYPE_TIP},

{  WWT_SCROLLBAR,   BGC,   382,   392,    81,   175, 0x0,  STR_0190_SCROLL_BAR_SCROLLS_LIST},

{    WWT_IMGBTN,    BTC,    10,   231,    81,    92, 0x0, STR_NETWORK_GAME_NAME_TIP },
{    WWT_IMGBTN,    BTC,   232,   281,    81,    92, 0x0, STR_NETWORK_PLAYERS_TIP },
{    WWT_IMGBTN,    BTC,   282,   331,    81,    92, 0x0, STR_NETWORK_MAP_SIZE_TIP },
{    WWT_IMGBTN,    BTC,   332,   381,    81,    92, 0x0, STR_NETWORK_INFO_ICONS_TIP },

{     WWT_MATRIX,   BGC,    10,   381,    93,   175, 0x601, STR_NETWORK_CLICK_GAME_TO_SELECT},

{ WWT_PUSHTXTBTN,   BTC,   145,   255,   180,   191, STR_012E_CANCEL, STR_NULL},
{ WWT_PUSHTXTBTN,   BTC,   270,   392,   180,   191, STR_NETWORK_JOIN_GAME, STR_NULL},

{      WWT_LAST},
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
	
	w = AllocateWindowDesc(&_network_game_window_desc);
	strcpy(_edit_str_buf, "Your name");

	
	WP(w,querystr_d).caret = 1;
	WP(w,querystr_d).maxlen = MAX_QUERYSTR_LEN;
	WP(w,querystr_d).maxwidth = 240;
	WP(w,querystr_d).buf = _edit_str_buf;
	
	
	ShowErrorMessage(-1, TEMP_STRING_NO_NETWORK, 0, 0);
}


void StartServer()
{
	_networking = true;
	NetworkInitialize(NULL);
	DEBUG(misc, 1) ("Listening on port %d\n", _network_port);
	NetworkListen(_network_port);
	_networking_server = true;
	DoCommandP(0, 0, 0, NULL, CMD_START_NEW_GAME);
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
	
		SET_DPARAM16(7, STR_NETWORK_2_PLAYERS + _opt_mod_ptr->road_side);
		DrawWindowWidgets(w);

		GfxFillRect(11, 63, 237, 168, 0xD7);

		DrawEditBox(w, 3);
		DrawEditBox(w, 4);
		
		DrawString(10, 22, STR_NETWORK_NEW_GAME_NAME, 2);
		DrawString(210, 22, STR_NETWORK_PASSWORD, 2);

		DrawString(10, 43, STR_NETWORK_SELECT_MAP, 2);
		DrawString(260, 63, STR_NETWORK_NUMBER_OF_PLAYERS, 2);
		
		break;
	}

	case WE_CLICK:
		_selected_field = e->click.widget;
		switch(e->click.widget) {

		case 0: // close X
		case 10: // cancel button
			ShowNetworkGameWindow();
			break;
		case 8:
			ShowDropDownMenu(w, _players_dropdown, _opt_mod_ptr->currency, e->click.widget, 0);
			return;
		case 9: // start game
			StartServer();
			ShowNetworkLobbyWindow();
			break;
		}

	case WE_MOUSELOOP:
		if(_selected_field == 3)
		{
			HandleEditBox(w, 3);
			break;
		}
		if(_selected_field == 4)
		{
			HandleEditBox(w, 4);
			break;
		}
			
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
{   WWT_PUSHTXTBTN, BGC,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   BGC,    10,   399,     0,    13, STR_NETWORK_START_GAME_WINDOW },
{     WWT_IMGBTN,   BGC,     0,   399,    14,   199, 0x0},

{     WWT_IMGBTN,   BGC,    80,   190,    22,    33, 0x0, STR_NETWORK_NEW_GAME_NAME_TIP},
{     WWT_IMGBTN,   BGC,   280,   390,    22,    33, 0x0, STR_NETWORK_PASSWORD_TIP},

{     WWT_IMGBTN,   BGC,    10,   240,    62,   170, 0x0, STR_NETWORK_SELECT_MAP_TIP},
{  WWT_SCROLLBAR,   BGC,   241,   251,    62,   170, 0x0,  STR_0190_SCROLL_BAR_SCROLLS_LIST},

{          WWT_6,   BGC,   260,   390,    81,    92, STR_NETWORK_COMBO2, STR_NETWORK_NUMBER_OF_PLAYERS_TIP},
{   WWT_CLOSEBOX,   BGC,   378,   388,    82,    91, STR_0225, STR_NETWORK_NUMBER_OF_PLAYERS_TIP},

{ WWT_PUSHTXTBTN,   BTC,    80,   180,   180,   191, STR_NETWORK_START_GAME, STR_NULL},
{ WWT_PUSHTXTBTN,   BTC,   220,   320,   180,   191, STR_012E_CANCEL, STR_NULL},

{      WWT_LAST},
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

	
	WP(w,querystr_d).caret = 1;
	WP(w,querystr_d).maxlen = MAX_QUERYSTR_LEN;
	WP(w,querystr_d).maxwidth = 240;
	WP(w,querystr_d).buf = _edit_str_buf;
}





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
{   WWT_PUSHTXTBTN, BGC,     0,    10,     0,    13, STR_00C5, STR_018B_CLOSE_WINDOW },
{    WWT_CAPTION,   BGC,    10,   399,     0,    13, STR_NETWORK_GAME_LOBBY },
{     WWT_IMGBTN,   BGC,     0,   399,    14,   299, 0x0},

// chat widget
{     WWT_IMGBTN,   BGC,    10,   240,    30,   240, 0x0},
{  WWT_SCROLLBAR,   BGC,   241,   251,    30,   240, 0x0,  STR_0190_SCROLL_BAR_SCROLLS_LIST},

// send message prompt
{     WWT_IMGBTN,   BGC,    10,   200,   241,   252, 0x0, STR_NETWORK_ENTER_NAME_TIP},
{ WWT_PUSHTXTBTN,   BTC,   201,   251,   241,   252, STR_NETWORK_SEND, STR_NETWORK_SEND_TIP},

// company name
{     WWT_IMGBTN,   BGC,   100,   251,   254,   265, 0x0, STR_NETWORK_COMPANY_NAME_TIP},

// player information
{     WWT_IMGBTN,   BGC,   260,   379,    30,   221, 0x0},
{  WWT_SCROLLBAR,   BGC,   380,   390,    30,   221, 0x1,  STR_0190_SCROLL_BAR_SCROLLS_LIST},

// buttons
{ WWT_PUSHTXTBTN,   BTC,   260,   390,   233,   244, STR_NETWORK_NEW_COMPANY, STR_NETWORK_NEW_COMPANY_TIP},
{ WWT_PUSHTXTBTN,   BTC,   260,   390,   254,   265, STR_NETWORK_SPECTATE_GAME, STR_NETWORK_SPECTATE_GAME_TIP},

{ WWT_PUSHTXTBTN,   BTC,    80,   180,   280,   291, STR_NETWORK_READY, STR_NULL},
{ WWT_PUSHTXTBTN,   BTC,   220,   320,   280,   291, STR_012E_CANCEL, STR_NULL},

{      WWT_LAST},
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


