#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "window.h"
#include "gui.h"
#include "viewport.h"
#include "gfx.h"
#include "player.h"
#include "command.h"
#include "console.h"
#include "network.h"

extern void SwitchMode(int new_mode);

/*
static void ShowSelectTutorialWindow()
{
}
*/

/* We go from TOP to the BOTTOM and from LEFT to the RIGHT. I know this is
 * a highly unusual concept, guys, but I hope you'll manage to follow. --pasky */
static const Widget _select_game_widgets[] = {
{    WWT_CAPTION, RESIZE_NONE, 13,   0, 335,   0,  13, STR_0307_OPENTTD,       STR_NULL},
{     WWT_IMGBTN, RESIZE_NONE, 13,   0, 335,  14, 233, 0x0,                    STR_NULL},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  22,  33, STR_0140_NEW_GAME,      STR_02FB_START_A_NEW_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  22,  33, STR_0141_LOAD_GAME,     STR_02FC_LOAD_A_SAVED_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  40,  51, STR_0220_CREATE_SCENARIO,STR_02FE_CREATE_A_CUSTOMIZED_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  40,  51, STR_029A_PLAY_SCENARIO, STR_0303_START_A_NEW_GAME_USING},

{    WWT_PANEL_2, RESIZE_NONE, 12,  10,  85,  67, 120, 0x1312,                 STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{    WWT_PANEL_2, RESIZE_NONE, 12,  90, 165,  67, 120, 0x1314,                 STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{    WWT_PANEL_2, RESIZE_NONE, 12, 170, 245,  67, 120, 0x1316,                 STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{    WWT_PANEL_2, RESIZE_NONE, 12, 250, 325,  67, 120, 0x1318,                 STR_0311_SELECT_TOYLAND_LANDSCAPE},

/* string&0x1000 shows the number encoded in the lowest 12 bits. Using
 * just dparams gives some extremely weird results, so this is after
 * all the most elegant solution. --pasky */
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10,  57, 128, 139, 0x1040,                 STR_MAP_WIDTH_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  64, 111, 128, 139, 0x1080,                 STR_MAP_WIDTH_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 117, 164, 128, 139, 0x1100,                 STR_MAP_WIDTH_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 171, 218, 128, 139, 0x1200,                 STR_MAP_WIDTH_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 224, 271, 128, 139, 0x1400,                 STR_MAP_WIDTH_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 278, 325, 128, 139, 0x1800,                 STR_MAP_WIDTH_TIP},

{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10,  57, 144, 155, 0x1040,                 STR_MAP_HEIGHT_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  64, 111, 144, 155, 0x1080,                 STR_MAP_HEIGHT_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 117, 164, 144, 155, 0x1100,                 STR_MAP_HEIGHT_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 171, 218, 144, 155, 0x1200,                 STR_MAP_HEIGHT_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 224, 271, 144, 155, 0x1400,                 STR_MAP_HEIGHT_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 278, 325, 144, 155, 0x1800,                 STR_MAP_HEIGHT_TIP},

{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167, 173, 184, STR_SINGLE_PLAYER,      STR_02FF_SELECT_SINGLE_PLAYER_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325, 173, 184, STR_MULTIPLAYER,        STR_0300_SELECT_MULTIPLAYER_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167, 196, 207, STR_0148_GAME_OPTIONS,  STR_0301_DISPLAY_GAME_OPTIONS},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325, 196, 207, STR_01FE_DIFFICULTY,    STR_0302_DISPLAY_DIFFICULTY_OPTIONS},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167, 214, 225, STR_CONFIG_PATCHES,     STR_CONFIG_PATCHES_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325, 214, 225, STR_0304_QUIT,          STR_0305_QUIT_OPENTTD},
{    WIDGETS_END },
};

extern void HandleOnEditText(WindowEvent *e);
extern void HandleOnEditTextCancel(void);

static void SelectGameWndProc(Window *w, WindowEvent *e) {
	switch (e->event) {
	case WE_PAINT:
		w->click_state &= ~(1<<22) & ~(0xF << 6) & ~(0xFFF << 11);
		w->click_state |=  (1<<22) | (1 << (_opt_newgame.landscape + 6));
		w->click_state |= (1 << (10 + _patches.map_x - 6)) | (1 << (16 + _patches.map_y - 6));
		SetDParam(0, STR_6801_EASY + _opt_newgame.diff_level);
		DrawWindowWidgets(w);
		break;

	case WE_CLICK:
		switch(e->click.widget) {
		case 2: DoCommandP(0, 0, 0, NULL, CMD_START_NEW_GAME); break;
		case 3: ShowSaveLoadDialog(SLD_LOAD_GAME); break;
		case 4: DoCommandP(0, InteractiveRandom(), 0, NULL, CMD_CREATE_SCENARIO); break;
		case 5: ShowSaveLoadDialog(SLD_LOAD_SCENARIO); break;
		case 6: case 7: case 8: case 9:
			// XXX: Useless usage of the CMD infrastructure?
			DoCommandP(0, e->click.widget - 6, 0, NULL, CMD_SET_NEW_LANDSCAPE_TYPE);
			break;
		case 10: case 11: case 12: case 13: case 14: case 15:
			_patches.map_x = 6 + e->click.widget - 10;
			InvalidateWindowClasses(WC_SELECT_GAME);
			break;
		case 16: case 17: case 18: case 19: case 20: case 21:
			_patches.map_y = 6 + e->click.widget - 16;
			InvalidateWindowClasses(WC_SELECT_GAME);
			break;
		case 23:
#ifdef ENABLE_NETWORK
			if (!_network_available) {
				ShowErrorMessage(-1, STR_NETWORK_ERR_NOTAVAILABLE, 0, 0);
			} else
				ShowNetworkGameWindow();
#else
			ShowErrorMessage(-1 ,STR_NETWORK_ERR_NOTAVAILABLE, 0, 0);
#endif
			break;
		case 24: ShowGameOptions(); break;
		case 25: ShowGameDifficulty(); break;
		case 26: ShowPatchesSelection(); break;
		case 27: AskExitGame(); break;
		}
		break;

	case WE_ON_EDIT_TEXT: HandleOnEditText(e); break;
	case WE_ON_EDIT_TEXT_CANCEL: HandleOnEditTextCancel(); break;
	}

}

static const WindowDesc _select_game_desc = {
	WDP_CENTER, WDP_CENTER, 336, 234,
	WC_SELECT_GAME,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_game_widgets,
	SelectGameWndProc
};

void ShowSelectGameWindow(void)
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

	SwitchMode(SM_NEWGAME);
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

	SwitchMode(SM_START_SCENARIO);
	return 0;
}


static const Widget _ask_abandon_game_widgets[] = {
{  WWT_TEXTBTN, RESIZE_NONE,  4,   0,  10,   0,  13, STR_00C5,      STR_NULL},
{  WWT_CAPTION, RESIZE_NONE,  4,  11, 179,   0,  13, STR_00C7_QUIT, STR_NULL},
{   WWT_IMGBTN, RESIZE_NONE,  4,   0, 179,  14,  91, 0x0,           STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  25,  84,  72,  83, STR_00C9_NO,   STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  95, 154,  72,  83, STR_00C8_YES,  STR_NULL},
{  WIDGETS_END },
};

static void AskAbandonGameWndProc(Window *w, WindowEvent *e) {
	switch(e->event) {
	case WE_PAINT:
		DrawWindowWidgets(w);
#if defined(_WIN32)
		SetDParam(0, STR_0133_WINDOWS);
#elif defined(__APPLE__)
		SetDParam(0, STR_0135_OSX);
#elif defined(__BEOS__)
		SetDParam(0, STR_OSNAME_BEOS);
#elif defined(__MORPHOS__)
		SetDParam(0, STR_OSNAME_MORPHOS);
#elif defined(__AMIGA__)
		SetDParam(0, STR_OSNAME_AMIGAOS);
#elif defined(__OS2__)
		SetDParam(0, STR_OSNAME_OS2);
#else
		SetDParam(0, STR_0134_UNIX);
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
		if (e->keypress.keycode == WKC_RETURN || e->keypress.keycode == WKC_NUM_ENTER)
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

void AskExitGame(void)
{
	AllocateWindowDescFront(&_ask_abandon_game_desc, 0);
}


static const Widget _ask_quit_game_widgets[] = {
{  WWT_TEXTBTN, RESIZE_NONE,  4,   0,  10,   0,  13, STR_00C5,           STR_NULL},
{  WWT_CAPTION, RESIZE_NONE,  4,  11, 179,   0,  13, STR_0161_QUIT_GAME, STR_NULL},
{   WWT_IMGBTN, RESIZE_NONE,  4,   0, 179,  14,  91, 0x0,                STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  25,  84,  72,  83, STR_00C9_NO,        STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  95, 154,  72,  83, STR_00C8_YES,       STR_NULL},
{  WIDGETS_END },
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

	case WE_KEYPRESS: /* Return to main menu on pressing 'Enter' */
		if (e->keypress.keycode == WKC_RETURN)
			_switch_mode = SM_MENU;
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


void AskExitToGameMenu(void)
{
	AllocateWindowDescFront(&_ask_quit_game_desc, 0);
}

int32 CmdSetNewLandscapeType(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		_opt_newgame.landscape = p1;
		InvalidateWindowClasses(WC_SELECT_GAME);
	}
	return 0;
}
