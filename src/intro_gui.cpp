/* $Id$ */

/** @file intro_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "network/network.h"
#include "variables.h"
#include "heightmap.h"
#include "genworld.h"
#include "network/network_gui.h"
#include "newgrf.h"
#include "strings_func.h"
#include "window_func.h"
#include "fios.h"
#include "gfx_func.h"
#include "settings_type.h"

#include "table/strings.h"
#include "table/sprites.h"

static const Widget _select_game_widgets[] = {
{    WWT_CAPTION, RESIZE_NONE, 13,   0, 335,   0,  13, STR_0307_OPENTTD,         STR_NULL},
{      WWT_PANEL, RESIZE_NONE, 13,   0, 335,  14, 194, 0x0,                      STR_NULL},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  22,  33, STR_0140_NEW_GAME,        STR_02FB_START_A_NEW_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  22,  33, STR_0141_LOAD_GAME,       STR_02FC_LOAD_A_SAVED_GAME},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  40,  51, STR_029A_PLAY_SCENARIO,   STR_0303_START_A_NEW_GAME_USING},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12, 168, 325,  40,  51, STR_PLAY_HEIGHTMAP,       STR_PLAY_HEIGHTMAP_HINT},
{ WWT_PUSHTXTBTN, RESIZE_NONE, 12,  10, 167,  58,  69, STR_SCENARIO_EDITOR,      STR_02FE_CREATE_A_CUSTOMIZED_GAME},
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

static inline void SetNewLandscapeType(byte landscape)
{
	_opt_newgame.landscape = landscape;
	InvalidateWindowClasses(WC_SELECT_GAME);
}

enum SelectGameIntroWidgets {
	SGI_GENERATE_GAME = 2,
	SGI_LOAD_GAME,
	SGI_PLAY_SCENARIO,
	SGI_PLAY_HEIGHTMAP,
	SGI_EDIT_SCENARIO,
	SGI_PLAY_NETWORK,
	SGI_TEMPERATE_LANDSCAPE,
	SGI_ARCTIC_LANDSCAPE,
	SGI_TROPIC_LANDSCAPE,
	SGI_TOYLAND_LANDSCAPE,
	SGI_OPTIONS,
	SGI_DIFFICULTIES,
	SGI_PATCHES_OPTIONS,
	SGI_GRF_SETTINGS,
	SGI_EXIT,
};

static void SelectGameWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE: w->LowerWidget(_opt_newgame.landscape + 8); break;

		case WE_PAINT:
			w->SetWidgetLoweredState(SGI_TEMPERATE_LANDSCAPE, _opt_newgame.landscape == LT_TEMPERATE);
			w->SetWidgetLoweredState(SGI_ARCTIC_LANDSCAPE, _opt_newgame.landscape == LT_ARCTIC);
			w->SetWidgetLoweredState(SGI_TROPIC_LANDSCAPE, _opt_newgame.landscape == LT_TROPIC);
			w->SetWidgetLoweredState(SGI_TOYLAND_LANDSCAPE, _opt_newgame.landscape == LT_TOYLAND);
			SetDParam(0, STR_6801_EASY + _opt_newgame.diff_level);
			DrawWindowWidgets(w);
			break;

		case WE_CLICK:
#ifdef ENABLE_NETWORK
			/* Do not create a network server when you (just) have closed one of the game
			 * creation/load windows for the network server. */
			if (SGI_GENERATE_GAME <= e->we.click.widget && e->we.click.widget <= SGI_EDIT_SCENARIO) _is_network_server = false;
#endif /* ENABLE_NETWORK */

			switch (e->we.click.widget) {
				case SGI_GENERATE_GAME:  ShowGenerateLandscape(); break;
				case SGI_LOAD_GAME:      ShowSaveLoadDialog(SLD_LOAD_GAME); break;
				case SGI_PLAY_SCENARIO:  ShowSaveLoadDialog(SLD_LOAD_SCENARIO); break;
				case SGI_PLAY_HEIGHTMAP: ShowSaveLoadDialog(SLD_LOAD_HEIGHTMAP); break;
				case SGI_EDIT_SCENARIO:  StartScenarioEditor(); break;

				case SGI_PLAY_NETWORK:
					if (!_network_available) {
						ShowErrorMessage(INVALID_STRING_ID, STR_NETWORK_ERR_NOTAVAILABLE, 0, 0);
					} else {
						ShowNetworkGameWindow();
					}
					break;

				case SGI_TEMPERATE_LANDSCAPE:	case SGI_ARCTIC_LANDSCAPE:
				case SGI_TROPIC_LANDSCAPE: case SGI_TOYLAND_LANDSCAPE:
					w->RaiseWidget(_opt_newgame.landscape + SGI_TEMPERATE_LANDSCAPE);
					SetNewLandscapeType(e->we.click.widget - SGI_TEMPERATE_LANDSCAPE);
					break;

				case SGI_OPTIONS:         ShowGameOptions(); break;
				case SGI_DIFFICULTIES:    ShowGameDifficulty(); break;
				case SGI_PATCHES_OPTIONS: ShowPatchesSelection(); break;
				case SGI_GRF_SETTINGS:    ShowNewGRFSettings(true, true, false, &_grfconfig_newgame); break;
				case SGI_EXIT:            HandleExitGameRequest(); break;
			}
			break;
	}
}

static const WindowDesc _select_game_desc = {
	WDP_CENTER, WDP_CENTER, 336, 195, 336, 195,
	WC_SELECT_GAME, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_game_widgets,
	SelectGameWndProc
};

void ShowSelectGameWindow()
{
	AllocateWindowDesc(&_select_game_desc);
}

static void AskExitGameCallback(Window *w, bool confirmed)
{
	if (confirmed) _exit_game = true;
}

void AskExitGame()
{
#if defined(_WIN32)
		SetDParam(0, STR_OSNAME_WINDOWS);
#elif defined(__APPLE__)
		SetDParam(0, STR_OSNAME_OSX);
#elif defined(__BEOS__)
		SetDParam(0, STR_OSNAME_BEOS);
#elif defined(__MORPHOS__)
		SetDParam(0, STR_OSNAME_MORPHOS);
#elif defined(__AMIGA__)
		SetDParam(0, STR_OSNAME_AMIGAOS);
#elif defined(__OS2__)
		SetDParam(0, STR_OSNAME_OS2);
#elif defined(SUNOS)
		SetDParam(0, STR_OSNAME_SUNOS);
#else
		SetDParam(0, STR_OSNAME_UNIX);
#endif
	ShowQuery(
		STR_00C7_QUIT,
		STR_00CA_ARE_YOU_SURE_YOU_WANT_TO,
		NULL,
		AskExitGameCallback
	);
}


static void AskExitToGameMenuCallback(Window *w, bool confirmed)
{
	if (confirmed) _switch_mode = SM_MENU;
}

void AskExitToGameMenu()
{
	ShowQuery(
		STR_0161_QUIT_GAME,
		(_game_mode != GM_EDITOR) ? STR_ABANDON_GAME_QUERY : STR_QUIT_SCENARIO_QUERY,
		NULL,
		AskExitToGameMenuCallback
	);
}
