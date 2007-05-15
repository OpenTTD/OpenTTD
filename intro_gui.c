/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "network.h"
#include "variables.h"
#include "settings.h"
#include "heightmap.h"
#include "genworld.h"
#include "network_gui.h"
#include "newgrf.h"

static const Widget _select_game_widgets[] = {
{    WWT_CAPTION, RESIZE_NONE, 13,   0, 335,   0,  13, STR_0307_OPENTTD,         STR_NULL},
{      WWT_PANEL, RESIZE_NONE, 13,   0, 335,  14, 194, 0x0,                      STR_NULL},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  22,  33, STR_0140_NEW_GAME,        STR_02FB_START_A_NEW_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  22,  33, STR_0141_LOAD_GAME,       STR_02FC_LOAD_A_SAVED_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  40,  51, STR_029A_PLAY_SCENARIO,   STR_0303_START_A_NEW_GAME_USING},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  40,  51, STR_PLAY_HEIGHTMAP,       STR_PLAY_HEIGHTMAP_HINT},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  58,  69, STR_0220_CREATE_SCENARIO, STR_02FE_CREATE_A_CUSTOMIZED_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  58,  69, STR_MULTIPLAYER,          STR_0300_SELECT_MULTIPLAYER_GAME},

{   WWT_IMGBTN_2, RESIZE_NONE, 12,  10,  86,  77, 131, SPR_SELECT_TEMPERATE,     STR_030E_SELECT_TEMPERATE_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12,  90, 166,  77, 131, SPR_SELECT_SUB_ARCTIC,    STR_030F_SELECT_SUB_ARCTIC_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 170, 246,  77, 131, SPR_SELECT_SUB_TROPICAL,  STR_0310_SELECT_SUB_TROPICAL_LANDSCAPE},
{   WWT_IMGBTN_2, RESIZE_NONE, 12, 250, 326,  77, 131, SPR_SELECT_TOYLAND,       STR_0311_SELECT_TOYLAND_LANDSCAPE},

{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167, 139, 150, STR_0148_GAME_OPTIONS,    STR_0301_DISPLAY_GAME_OPTIONS},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325, 139, 150, STR_01FE_DIFFICULTY,      STR_0302_DISPLAY_DIFFICULTY_OPTIONS},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167, 157, 168, STR_CONFIG_PATCHES,       STR_CONFIG_PATCHES_TIP},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325, 157, 168, STR_NEWGRF_SETTINGS_BUTTON, STR_NULL},

{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 104, 231, 175, 186, STR_0304_QUIT,            STR_0305_QUIT_OPENTTD},
{   WIDGETS_END},
};

extern void HandleOnEditText(WindowEvent *e);

static inline void SetNewLandscapeType(byte landscape)
{
	_opt_newgame.landscape = landscape;
	InvalidateWindowClasses(WC_SELECT_GAME);
}

static void SelectGameWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
	case WE_CREATE: LowerWindowWidget(w, _opt_newgame.landscape + 8); break;

	case WE_PAINT:
		SetWindowWidgetLoweredState(w, 8,  _opt_newgame.landscape == LT_NORMAL);
		SetWindowWidgetLoweredState(w, 9,  _opt_newgame.landscape == LT_HILLY);
		SetWindowWidgetLoweredState(w, 10, _opt_newgame.landscape == LT_DESERT);
		SetWindowWidgetLoweredState(w, 11, _opt_newgame.landscape == LT_CANDY);
		SetDParam(0, STR_6801_EASY + _opt_newgame.diff_level);
		DrawWindowWidgets(w);
		break;

	case WE_CLICK:
		/* Do not create a network server when you (just) have closed one of the game
		 * creation/load windows for the network server. */
		if (2 <= e->we.click.widget && e->we.click.widget <= 6) _is_network_server = false;

		switch (e->we.click.widget) {
		case 2: ShowGenerateLandscape(); break;
		case 3: ShowSaveLoadDialog(SLD_LOAD_GAME); break;
		case 4: ShowSaveLoadDialog(SLD_LOAD_SCENARIO); break;
		case 5: ShowSaveLoadDialog(SLD_LOAD_HEIGHTMAP); break;
		case 6: ShowCreateScenario(); break;
		case 7:
			if (!_network_available) {
				ShowErrorMessage(INVALID_STRING_ID, STR_NETWORK_ERR_NOTAVAILABLE, 0, 0);
			} else {
				ShowNetworkGameWindow();
			}
			break;
		case 8: case 9: case 10: case 11:
			RaiseWindowWidget(w, _opt_newgame.landscape + 8);
			SetNewLandscapeType(e->we.click.widget - 8);
			break;
		case 12: ShowGameOptions(); break;
		case 13: ShowGameDifficulty(); break;
		case 14: ShowPatchesSelection(); break;
		case 15: ShowNewGRFSettings(true, true, false, &_grfconfig_newgame); break;
		case 16: HandleExitGameRequest(); break;
		}
		break;

		case WE_ON_EDIT_TEXT: HandleOnEditText(e); break;
	}
}

static const WindowDesc _select_game_desc = {
	WDP_CENTER, WDP_CENTER, 336, 195,
	WC_SELECT_GAME,0,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_game_widgets,
	SelectGameWndProc
};

void ShowSelectGameWindow(void)
{
	AllocateWindowDesc(&_select_game_desc);
}

static const Widget _ask_abandon_game_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE,  4,   0,  10,   0,  13, STR_00C5,      STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION, RESIZE_NONE,  4,  11, 179,   0,  13, STR_00C7_QUIT, STR_NULL},
{    WWT_PANEL, RESIZE_NONE,  4,   0, 179,  14,  91, 0x0,           STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  25,  84,  72,  83, STR_00C9_NO,   STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  95, 154,  72,  83, STR_00C8_YES,  STR_NULL},
{  WIDGETS_END },
};

static void AskAbandonGameWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
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
		DrawStringMultiCenter(90, 38, STR_00CA_ARE_YOU_SURE_YOU_WANT_TO, 178);
		return;

	case WE_CLICK:
		switch (e->we.click.widget) {
			case 3: DeleteWindow(w);   break;
			case 4: _exit_game = true; break;
		}
		break;

	case WE_KEYPRESS: /* Exit game on pressing 'Enter' */
		switch (e->we.keypress.keycode) {
			case WKC_RETURN:
			case WKC_NUM_ENTER:
				_exit_game = true;
				break;
		}
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
{ WWT_CLOSEBOX, RESIZE_NONE,  4,   0,  10,   0,  13, STR_00C5,           STR_018B_CLOSE_WINDOW},
{  WWT_CAPTION, RESIZE_NONE,  4,  11, 179,   0,  13, STR_0161_QUIT_GAME, STR_NULL},
{    WWT_PANEL, RESIZE_NONE,  4,   0, 179,  14,  91, 0x0,                STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  25,  84,  72,  83, STR_00C9_NO,        STR_NULL},
{  WWT_TEXTBTN, RESIZE_NONE, 12,  95, 154,  72,  83, STR_00C8_YES,       STR_NULL},
{  WIDGETS_END },
};

static void AskQuitGameWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			DrawWindowWidgets(w);
			DrawStringMultiCenter(
				90, 38,
				_game_mode != GM_EDITOR ?
					STR_0160_ARE_YOU_SURE_YOU_WANT_TO : STR_029B_ARE_YOU_SURE_YOU_WANT_TO,
				178
			);
			break;

		case WE_CLICK:
			switch (e->we.click.widget) {
				case 3: DeleteWindow(w);        break;
				case 4: _switch_mode = SM_MENU; break;
			}
			break;

		case WE_KEYPRESS: /* Return to main menu on pressing 'Enter' */
			if (e->we.keypress.keycode == WKC_RETURN) _switch_mode = SM_MENU;
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
