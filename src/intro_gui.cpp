/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file intro_gui.cpp The main menu GUI. */

#include "stdafx.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "network/network.h"
#include "genworld.h"
#include "network/network_gui.h"
#include "network/network_content.h"
#include "landscape_type.h"
#include "strings_func.h"
#include "fios.h"
#include "ai/ai_gui.hpp"
#include "gfx_func.h"
#include "core/geometry_func.hpp"

#include "table/strings.h"
#include "table/sprites.h"

enum SelectGameIntroWidgets {
	SGI_GENERATE_GAME,
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
	SGI_SETTINGS_OPTIONS,
	SGI_GRF_SETTINGS,
	SGI_CONTENT_DOWNLOAD,
	SGI_AI_SETTINGS,
	SGI_EXIT,
};

struct SelectGameWindow : public Window {

	SelectGameWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc);
		this->OnInvalidateData();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		this->SetWidgetLoweredState(SGI_TEMPERATE_LANDSCAPE, _settings_newgame.game_creation.landscape == LT_TEMPERATE);
		this->SetWidgetLoweredState(SGI_ARCTIC_LANDSCAPE,    _settings_newgame.game_creation.landscape == LT_ARCTIC);
		this->SetWidgetLoweredState(SGI_TROPIC_LANDSCAPE,    _settings_newgame.game_creation.landscape == LT_TROPIC);
		this->SetWidgetLoweredState(SGI_TOYLAND_LANDSCAPE,   _settings_newgame.game_creation.landscape == LT_TOYLAND);
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == SGI_DIFFICULTIES) SetDParam(0, STR_DIFFICULTY_LEVEL_EASY + _settings_newgame.difficulty.diff_level);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != SGI_DIFFICULTIES) return;

		Dimension textdim = {0, 0};
		for (uint i = STR_DIFFICULTY_LEVEL_EASY; i <= STR_DIFFICULTY_LEVEL_CUSTOM; i++) {
			SetDParam(0, i);
			textdim = maxdim(textdim, GetStringBoundingBox(STR_INTRO_DIFFICULTY));
		}
		textdim.width += padding.width;
		textdim.height += padding.height;
		*size = maxdim(*size, textdim);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
#ifdef ENABLE_NETWORK
		/* Do not create a network server when you (just) have closed one of the game
		 * creation/load windows for the network server. */
		if (IsInsideMM(widget, SGI_GENERATE_GAME, SGI_EDIT_SCENARIO + 1)) _is_network_server = false;
#endif /* ENABLE_NETWORK */

		switch (widget) {
			case SGI_GENERATE_GAME:
				if (_ctrl_pressed) {
					StartNewGameWithoutGUI(GENERATE_NEW_SEED);
				} else {
					ShowGenerateLandscape();
				}
				break;

			case SGI_LOAD_GAME:      ShowSaveLoadDialog(SLD_LOAD_GAME); break;
			case SGI_PLAY_SCENARIO:  ShowSaveLoadDialog(SLD_LOAD_SCENARIO); break;
			case SGI_PLAY_HEIGHTMAP: ShowSaveLoadDialog(SLD_LOAD_HEIGHTMAP); break;
			case SGI_EDIT_SCENARIO:  StartScenarioEditor(); break;

			case SGI_PLAY_NETWORK:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
					ShowNetworkGameWindow();
				}
				break;

			case SGI_TEMPERATE_LANDSCAPE: case SGI_ARCTIC_LANDSCAPE:
			case SGI_TROPIC_LANDSCAPE: case SGI_TOYLAND_LANDSCAPE:
				SetNewLandscapeType(widget - SGI_TEMPERATE_LANDSCAPE);
				break;

			case SGI_OPTIONS:         ShowGameOptions(); break;
			case SGI_DIFFICULTIES:    ShowGameDifficulty(); break;
			case SGI_SETTINGS_OPTIONS:ShowGameSettings(); break;
			case SGI_GRF_SETTINGS:    ShowNewGRFSettings(true, true, false, &_grfconfig_newgame); break;
			case SGI_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
					ShowNetworkContentListWindow();
				}
				break;
			case SGI_AI_SETTINGS:     ShowAIConfigWindow(); break;
			case SGI_EXIT:            HandleExitGameRequest(); break;
		}
	}
};

static const NWidgetPart _nested_select_game_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_INTRO_CAPTION, STR_NULL),
	NWidget(WWT_PANEL, COLOUR_BROWN),
	NWidget(NWID_SPACER), SetMinimalSize(0, 8),

	/* 'generate game' and 'load game' buttons */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_GENERATE_GAME), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_NEW_GAME, STR_INTRO_TOOLTIP_NEW_GAME), SetPadding(0, 0, 0, 10), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_LOAD_GAME), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_LOAD_GAME, STR_INTRO_TOOLTIP_LOAD_GAME), SetPadding(0, 10, 0, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 6),

	/* 'play scenario' and 'play heightmap' buttons */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_PLAY_SCENARIO), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_PLAY_SCENARIO, STR_INTRO_TOOLTIP_PLAY_SCENARIO), SetPadding(0, 0, 0, 10), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_PLAY_HEIGHTMAP), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_PLAY_HEIGHTMAP, STR_INTRO_TOOLTIP_PLAY_HEIGHTMAP), SetPadding(0, 10, 0, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 6),

	/* 'edit scenario' and 'play multiplayer' buttons */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_EDIT_SCENARIO), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_SCENARIO_EDITOR, STR_INTRO_TOOLTIP_SCENARIO_EDITOR), SetPadding(0, 0, 0, 10), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_PLAY_NETWORK), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_MULTIPLAYER, STR_INTRO_TOOLTIP_MULTIPLAYER), SetPadding(0, 10, 0, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 7),

	/* climate selection buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetMinimalSize(10, 0), SetFill(1, 0),
		NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, SGI_TEMPERATE_LANDSCAPE), SetMinimalSize(77, 55),
							SetDataTip(SPR_SELECT_TEMPERATE, STR_INTRO_TOOLTIP_TEMPERATE),
		NWidget(NWID_SPACER), SetMinimalSize(3, 0), SetFill(1, 0),
		NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, SGI_ARCTIC_LANDSCAPE), SetMinimalSize(77, 55),
							SetDataTip(SPR_SELECT_SUB_ARCTIC, STR_INTRO_TOOLTIP_SUB_ARCTIC_LANDSCAPE),
		NWidget(NWID_SPACER), SetMinimalSize(3, 0), SetFill(1, 0),
		NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, SGI_TROPIC_LANDSCAPE), SetMinimalSize(77, 55),
							SetDataTip(SPR_SELECT_SUB_TROPICAL, STR_INTRO_TOOLTIP_SUB_TROPICAL_LANDSCAPE),
		NWidget(NWID_SPACER), SetMinimalSize(3, 0), SetFill(1, 0),
		NWidget(WWT_IMGBTN_2, COLOUR_ORANGE, SGI_TOYLAND_LANDSCAPE), SetMinimalSize(77, 55),
							SetDataTip(SPR_SELECT_TOYLAND, STR_INTRO_TOOLTIP_TOYLAND_LANDSCAPE),
		NWidget(NWID_SPACER), SetMinimalSize(10, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 7),

	/* 'game options' and 'difficulty options' buttons */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_OPTIONS), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_GAME_OPTIONS, STR_INTRO_TOOLTIP_GAME_OPTIONS), SetPadding(0, 0, 0, 10), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_DIFFICULTIES), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_DIFFICULTY, STR_INTRO_TOOLTIP_DIFFICULTY_OPTIONS), SetPadding(0, 10, 0, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 6),

	/* 'advanced settings' and 'newgrf settings' buttons */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_SETTINGS_OPTIONS), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_ADVANCED_SETTINGS, STR_INTRO_TOOLTIP_ADVANCED_SETTINGS), SetPadding(0, 0, 0, 10), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_GRF_SETTINGS), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_NEWGRF_SETTINGS, STR_INTRO_TOOLTIP_NEWGRF_SETTINGS), SetPadding(0, 10, 0, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 6),

	/* 'online content' and 'ai settings' buttons */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_CONTENT_DOWNLOAD), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT), SetPadding(0, 0, 0, 10), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_AI_SETTINGS), SetMinimalSize(158, 12),
							SetDataTip(STR_INTRO_AI_SETTINGS, STR_INTRO_TOOLTIP_AI_SETTINGS), SetPadding(0, 10, 0, 0), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 6),

	/* 'exit program' button */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, SGI_EXIT), SetMinimalSize(128, 12),
							SetDataTip(STR_INTRO_QUIT, STR_INTRO_TOOLTIP_QUIT),
		NWidget(NWID_SPACER), SetFill(1, 0),
	EndContainer(),

	NWidget(NWID_SPACER), SetMinimalSize(0, 8),

	EndContainer(),
};

static const WindowDesc _select_game_desc(
	WDP_CENTER, 0, 0,
	WC_SELECT_GAME, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_select_game_widgets, lengthof(_nested_select_game_widgets)
);

void ShowSelectGameWindow()
{
	new SelectGameWindow(&_select_game_desc);
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
#elif defined(__HAIKU__)
		SetDParam(0, STR_OSNAME_HAIKU);
#elif defined(__MORPHOS__)
		SetDParam(0, STR_OSNAME_MORPHOS);
#elif defined(__AMIGA__)
		SetDParam(0, STR_OSNAME_AMIGAOS);
#elif defined(__OS2__)
		SetDParam(0, STR_OSNAME_OS2);
#elif defined(SUNOS)
		SetDParam(0, STR_OSNAME_SUNOS);
#elif defined(DOS)
		SetDParam(0, STR_OSNAME_DOS);
#else
		SetDParam(0, STR_OSNAME_UNIX);
#endif
	ShowQuery(
		STR_QUIT_CAPTION,
		STR_QUIT_ARE_YOU_SURE_YOU_WANT_TO_EXIT_OPENTTD,
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
		STR_ABANDON_GAME_CAPTION,
		(_game_mode != GM_EDITOR) ? STR_ABANDON_GAME_QUERY : STR_ABANDON_SCENARIO_QUERY,
		NULL,
		AskExitToGameMenuCallback
	);
}
