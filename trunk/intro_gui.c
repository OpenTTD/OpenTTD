#include "stdafx.h"
#include "ttd.h"

#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "player.h"
#include "command.h"
#include "console.h"

extern void MakeNewGame();
extern void StartScenario();

/*
static void ShowSelectTutorialWindow()
{
}
*/

static const Widget _select_game_widgets[] = {
{    WWT_CAPTION,    13,     0,   335,     0,    13, STR_0307_OPENTTD,				STR_NULL},
{     WWT_IMGBTN,    13,     0,   335,    14,   196, 0x0,											STR_NULL},
{ WWT_PUSHTXTBTN,    12,    10,   167,    22,    33, STR_0140_NEW_GAME,				STR_02FB_START_A_NEW_GAME},
{ WWT_PUSHTXTBTN,    12,   168,   325,    22,    33, STR_0141_LOAD_GAME,			STR_02FC_LOAD_A_SAVED_GAME_FROM},
//{ WWT_PUSHTXTBTN,    12,    10,   167,   177,   188, STR_0142_TUTORIAL_DEMONSTRATION, STR_02FD_VIEW_DEMONSTRATIONS_TUTORIALS},
{ WWT_PUSHTXTBTN,    12,    10,   167,   177,   188, STR_CONFIG_PATCHES,			STR_CONFIG_PATCHES_TIP},
{ WWT_PUSHTXTBTN,    12,    10,   167,    40,    51, STR_0220_CREATE_SCENARIO,STR_02FE_CREATE_A_CUSTOMIZED_GAME},
{ WWT_PUSHTXTBTN,    12,    10,   167,   136,   147, STR_SINGLE_PLAYER,				STR_02FF_SELECT_SINGLE_PLAYER_GAME},
{ WWT_PUSHTXTBTN,    12,   168,   325,   136,   147, STR_MULTIPLAYER,					STR_0300_SELECT_TWO_PLAYER_GAME},
{ WWT_PUSHTXTBTN,    12,    10,   167,   159,   170, STR_0148_GAME_OPTIONS,		STR_0301_DISPLAY_GAME_OPTIONS},
{ WWT_PUSHTXTBTN,    12,   168,   325,   159,   170, STR_01FE_DIFFICULTY,			STR_0302_DISPLAY_DIFFICULTY_OPTIONS},
{ WWT_PUSHTXTBTN,    12,   168,   325,    40,    51, STR_029A_PLAY_SCENARIO,	STR_0303_START_A_NEW_GAME_USING},
{ WWT_PUSHTXTBTN,    12,   168,   325,   177,   188, STR_0304_QUIT,						STR_0305_LEAVE_OPENTTD},
{    WWT_PANEL_2,    12,    10,    85,    69,   122, 0x1312,									STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{    WWT_PANEL_2,    12,    90,   165,    69,   122, 0x1314,									STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{    WWT_PANEL_2,    12,   170,   245,    69,   122, 0x1316,									STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{    WWT_PANEL_2,    12,   250,   325,    69,   122, 0x1318,									STR_0311_SELECT_TOYLAND_LANDSCAPE},
{   WIDGETS_END},
};

static void SelectGameWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		w->click_state = (w->click_state & ~(0xC0) & ~(0xF << 12)) | (1 << (_new_opt.landscape+12)) | (!_networking?(1<<6):(1<<7));
		w->disabled_state = _networking ? 0x30 : 0;
		SET_DPARAM16(0, STR_6801_EASY + _new_opt.diff_level);
		DrawWindowWidgets(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: DoCommandP(0, 0, 0, NULL, CMD_START_NEW_GAME); break;
		case 3: ShowSaveLoadDialog(SLD_LOAD_GAME); break;
		case 4: ShowPatchesSelection(); break;
		case 5: DoCommandP(0, InteractiveRandom(), 0, NULL, CMD_CREATE_SCENARIO); break;
		case 6:
			if (_networking)
				DoCommandP(0, 0, 0, NULL, CMD_SET_SINGLE_PLAYER);
			break;
		case 7:
			if (!_network_available) {
				ShowErrorMessage(-1,STR_NETWORK_ERR_NOTAVAILABLE, 0, 0);
			} else {
				ShowNetworkGameWindow();
				ShowErrorMessage(-1, TEMP_STRING_NO_NETWORK, 0, 0);
			}
			break;
		case 8: ShowGameOptions(); break;
		case 9: ShowGameDifficulty(); break;
		case 10:ShowSaveLoadDialog(SLD_LOAD_SCENARIO); break;
		case 11:AskExitGame(); break;
		case 12: case 13: case 14: case 15:
			DoCommandP(0, e->click.widget - 12, 0, NULL, CMD_SET_NEW_LANDSCAPE_TYPE);
			break;
		}
	case WE_KEYPRESS:
		switch(e->keypress.keycode) {
		case WKC_BACKQUOTE: IConsoleSwitch(); break;
		}
		break;
	}
}

static const WindowDesc _select_game_desc = {
	WDP_CENTER, WDP_CENTER, 336, 197,
	WC_SELECT_GAME,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_game_widgets,
	SelectGameWndProc
};

void ShowSelectGameWindow()
{
	AllocateWindowDesc(&_select_game_desc);
}


// p1 = mode
//    0 - start new game
//    1 - close new game dialog

int32 CmdStartNewGame(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (!(flags & DC_EXEC))
		return 0;

	switch(p1) {
	case 0: // show select game window
		AskForNewGameToStart();
		break;
	case 1: // close select game window
		DeleteWindowById(WC_SAVELOAD, 0);
		break;
	}

	return 0;
}

int32 CmdGenRandomNewGame(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (!(flags & DC_EXEC))
		return 0;

	// this forces stuff into test mode.
	_docommand_recursive = 0;

	_random_seeds[0][0] = p1;
	_random_seeds[0][1] = p2;

	if (_networking) { NetworkStartSync(true); }

	MakeNewGame();
	return 0;
}

int32 CmdLoadGame(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (!(flags & DC_EXEC))
		return 0;

//	ShowSaveLoadDialog(0);
	return 0;
}

int32 CmdCreateScenario(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (!(flags & DC_EXEC))
		return 0;

	_switch_mode = SM_EDITOR;
	return 0;
}

int32 CmdSetSinglePlayer(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	printf("CmdSetSinglePlayer\n");
	return 0;
}

int32 CmdStartScenario(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (!(flags & DC_EXEC))
		return 0;

	// this forces stuff into test mode.
	_docommand_recursive = 0;

	_random_seeds[0][0] = p1;
	_random_seeds[0][1] = p2;

	if (_networking) { NetworkStartSync(true); }

	StartScenario();
	return 0;
}


static const Widget _ask_abandon_game_widgets[] = {
{    WWT_TEXTBTN,     4,     0,    10,     0,    13, STR_00C5,			STR_NULL},
{    WWT_CAPTION,     4,    11,   179,     0,    13, STR_00C7_QUIT,	STR_NULL},
{     WWT_IMGBTN,     4,     0,   179,    14,    91, 0x0,						STR_NULL},
{    WWT_TEXTBTN,    12,    25,    84,    72,    83, STR_00C9_NO,		STR_NULL},
{    WWT_TEXTBTN,    12,    95,   154,    72,    83, STR_00C8_YES,	STR_NULL},
{   WIDGETS_END},
};

static void AskAbandonGameWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		DrawWindowWidgets(w);
#if defined(_WIN32)
		SET_DPARAM16(0, STR_0133_WINDOWS);
#elif defined(__APPLE__)
		SET_DPARAM16(0, STR_0135_OSX);
#elif defined(__BEOS__)
		SET_DPARAM16(0, STR_OSNAME_BEOS);
#elif defined(__MORPHOS__)
		SET_DPARAM16(0, STR_OSNAME_MORPHOS);
#else
		SET_DPARAM16(0, STR_0134_UNIX);
#endif
		DrawStringMultiCenter(0x5A, 0x26, STR_00CA_ARE_YOU_SURE_YOU_WANT_TO, 178);
		return;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3:
			DeleteWindow(w);
			break;
		case 4:
			_exit_game = true;
			break;
		}
		break;
	case WE_KEYPRESS: /* Exit game on pressing 'Enter' */
		if (e->keypress.keycode == WKC_RETURN)
			_exit_game = true;
		break;
	}
}

static const WindowDesc _ask_abandon_game_desc = {
	WDP_CENTER, WDP_CENTER, 180, 92,
	WC_ASK_ABANDON_GAME,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_ask_abandon_game_widgets,
	AskAbandonGameWndProc
};

void AskExitGame()
{
	AllocateWindowDescFront(&_ask_abandon_game_desc, 0);
}


static const Widget _ask_quit_game_widgets[] = {
{    WWT_TEXTBTN,     4,     0,    10,     0,    13, STR_00C5,						STR_NULL},
{    WWT_CAPTION,     4,    11,   179,     0,    13, STR_0161_QUIT_GAME,	STR_NULL},
{     WWT_IMGBTN,     4,     0,   179,    14,    91, 0x0,									STR_NULL},
{    WWT_TEXTBTN,    12,    25,    84,    72,    83, STR_00C9_NO,					STR_NULL},
{    WWT_TEXTBTN,    12,    95,   154,    72,    83, STR_00C8_YES,				STR_NULL},
{   WIDGETS_END},
};

static void AskQuitGameWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		DrawWindowWidgets(w);
		DrawStringMultiCenter(0x5A, 0x26,
			_game_mode != GM_EDITOR ? STR_0160_ARE_YOU_SURE_YOU_WANT_TO :
				STR_029B_ARE_YOU_SURE_YOU_WANT_TO,
			178);
		return;

	case WE_CLICK:
		switch(e->click.widget) {
		case 3:
			DeleteWindow(w);
			break;
		case 4:
			_switch_mode = SM_MENU;
			break;
		}
		break;
	}
}

static const WindowDesc _ask_quit_game_desc = {
	WDP_CENTER, WDP_CENTER, 180, 92,
	WC_QUIT_GAME,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_STD_BTN | WDF_UNCLICK_BUTTONS,
	_ask_quit_game_widgets,
	AskQuitGameWndProc
};


void AskExitToGameMenu()
{
	AllocateWindowDescFront(&_ask_quit_game_desc, 0);
}

int32 CmdSetNewLandscapeType(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		// XXX: some stuff
		_new_opt.landscape = p1;
		InvalidateWindowClasses(WC_SELECT_GAME);
	}
	return 0;
}
