/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file highscore_gui.cpp Definition of the HighScore and EndGame windows */

#include "stdafx.h"
#include "highscore.h"
#include "table/strings.h"
#include "gfx_func.h"
#include "table/sprites.h"
#include "window_gui.h"
#include "window_func.h"
#include "network/network.h"
#include "command_func.h"
#include "company_func.h"
#include "company_base.h"
#include "strings_func.h"
#include "hotkeys.h"

enum HighscoreWidgets {
	HSW_BACKGROUND,
};

struct EndGameHighScoreBaseWindow : Window {
	uint32 background_img;
	int8 rank;

	EndGameHighScoreBaseWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc);
		ResizeWindow(this, _screen.width - this->width, _screen.height - this->height);
	}

	/* Always draw a maximized window and within it the centered background */
	void SetupHighScoreEndWindow()
	{
		/* Resize window to "full-screen". */
		if (this->width != _screen.width || this->height != _screen.height) ResizeWindow(this, _screen.width - this->width, _screen.height - this->height);

		this->DrawWidgets();

		Point pt = this->GetTopLeft640x480();
		/* Center Highscore/Endscreen background */
		for (uint i = 0; i < 10; i++) { // the image is split into 10 50px high parts
			DrawSprite(this->background_img + i, PAL_NONE, pt.x, pt.y + (i * 50));
		}
	}

	/** Return the coordinate of the screen such that a window of 640x480 is centered at the screen. */
	Point GetTopLeft640x480()
	{
		Point pt = {max(0, (_screen.width  / 2) - (640 / 2)), max(0, (_screen.height / 2) - (480 / 2))};
		return pt;
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		delete this;
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		/* All keys are 'handled' by this window but we want to make
		 * sure that 'quit' still works correctly. Not handling the
		 * quit key is enough so the main toolbar can handle it. */
		if (IsQuitKey(keycode)) return ES_NOT_HANDLED;

		switch (keycode) {
			/* Keys for telling we want to go on */
			case WKC_RETURN:
			case WKC_ESC:
			case WKC_SPACE:
				delete this;
				return ES_HANDLED;

			default:
				/* We want to handle all keys; we don't want windows in
				 * the background to open. Especially the ones that do
				 * locate themselves based on the status-/toolbars. */
				return ES_HANDLED;
		}
	}
};

/** End game window shown at the end of the game */
struct EndGameWindow : EndGameHighScoreBaseWindow {
	EndGameWindow(const WindowDesc *desc) : EndGameHighScoreBaseWindow(desc)
	{
		/* Pause in single-player to have a look at the highscore at your own leisure */
		if (!_networking) DoCommandP(0, PM_PAUSED_NORMAL, 1, CMD_PAUSE);

		this->background_img = SPR_TYCOON_IMG1_BEGIN;

		if (_local_company != COMPANY_SPECTATOR) {
			const Company *c = Company::Get(_local_company);
			if (c->old_economy[0].performance_history == SCORE_MAX) {
				this->background_img = SPR_TYCOON_IMG2_BEGIN;
			}
		}

		/* In a network game show the endscores of the custom difficulty 'network' which is the last one
		 * as well as generate a TOP5 of that game, and not an all-time top5. */
		if (_networking) {
			this->window_number = lengthof(_highscore_table) - 1;
			this->rank = SaveHighScoreValueNetwork();
		} else {
			/* in single player _local company is always valid */
			const Company *c = Company::Get(_local_company);
			this->window_number = _settings_game.difficulty.diff_level;
			this->rank = SaveHighScoreValue(c);
		}

		MarkWholeScreenDirty();
	}

	~EndGameWindow()
	{
		if (!_networking) DoCommandP(0, PM_PAUSED_NORMAL, 0, CMD_PAUSE); // unpause
		ShowHighscoreTable(this->window_number, this->rank);
	}

	virtual void OnPaint()
	{
		this->SetupHighScoreEndWindow();
		Point pt = this->GetTopLeft640x480();

		const Company *c = Company::GetIfValid(_local_company);
		if (c == NULL) return;

		/* We need to get performance from last year because the image is shown
		 * at the start of the new year when these things have already been copied */
		if (this->background_img == SPR_TYCOON_IMG2_BEGIN) { // Tycoon of the century \o/
			SetDParam(0, c->index);
			SetDParam(1, c->index);
			SetDParam(2, EndGameGetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawStringMultiLine(pt.x + 15, pt.x + 640 - 25, pt.y + 90, pt.y + 160, STR_HIGHSCORE_PRESIDENT_OF_COMPANY_ACHIEVES_STATUS, TC_FROMSTRING, SA_CENTER);
		} else {
			SetDParam(0, c->index);
			SetDParam(1, EndGameGetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawStringMultiLine(pt.x + 36, pt.x + 640, pt.y + 140, pt.y + 206, STR_HIGHSCORE_COMPANY_ACHIEVES_STATUS, TC_FROMSTRING, SA_CENTER);
		}
	}
};

struct HighScoreWindow : EndGameHighScoreBaseWindow {
	HighScoreWindow(const WindowDesc *desc, int difficulty, int8 ranking) : EndGameHighScoreBaseWindow(desc)
	{
		/* pause game to show the chart */
		if (!_networking) DoCommandP(0, PM_PAUSED_NORMAL, 1, CMD_PAUSE);

		/* Close all always on-top windows to get a clean screen */
		if (_game_mode != GM_MENU) HideVitalWindows();

		MarkWholeScreenDirty();
		this->window_number = difficulty; // show highscore chart for difficulty...
		this->background_img = SPR_HIGHSCORE_CHART_BEGIN; // which background to show
		this->rank = ranking;
	}

	~HighScoreWindow()
	{
		if (_game_mode != GM_MENU) ShowVitalWindows();

		if (!_networking) DoCommandP(0, PM_PAUSED_NORMAL, 0, CMD_PAUSE); // unpause
	}

	virtual void OnPaint()
	{
		const HighScore *hs = _highscore_table[this->window_number];

		this->SetupHighScoreEndWindow();
		Point pt = this->GetTopLeft640x480();

		SetDParam(0, ORIGINAL_END_YEAR);
		SetDParam(1, this->window_number + STR_DIFFICULTY_LEVEL_EASY);
		DrawStringMultiLine(pt.x + 70, pt.x + 570, pt.y, pt.y + 140, !_networking ? STR_HIGHSCORE_TOP_COMPANIES_WHO_REACHED : STR_HIGHSCORE_TOP_COMPANIES_NETWORK_GAME, TC_FROMSTRING, SA_CENTER);

		/* Draw Highscore peepz */
		for (uint8 i = 0; i < lengthof(_highscore_table[0]); i++) {
			SetDParam(0, i + 1);
			DrawString(pt.x + 40, pt.x + 600, pt.y + 140 + (i * 55), STR_HIGHSCORE_POSITION);

			if (hs[i].company[0] != '\0') {
				TextColour colour = (this->rank == i) ? TC_RED : TC_BLACK; // draw new highscore in red

				DrawString(pt.x + 71, pt.x + 569, pt.y + 140 + (i * 55), hs[i].company, colour);
				SetDParam(0, hs[i].title);
				SetDParam(1, hs[i].score);
				DrawString(pt.x + 71, pt.x + 569, pt.y + 140 + FONT_HEIGHT_LARGE + (i * 55), STR_HIGHSCORE_STATS, colour);
			}
		}
	}
};

static const NWidgetPart _nested_highscore_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_END, HSW_BACKGROUND), SetMinimalSize(641, 481), SetResize(1, 1), EndContainer(),
};

static const WindowDesc _highscore_desc(
	WDP_MANUAL, 0, 0,
	WC_HIGHSCORE, WC_NONE,
	0,
	_nested_highscore_widgets, lengthof(_nested_highscore_widgets)
);

static const WindowDesc _endgame_desc(
	WDP_MANUAL, 0, 0,
	WC_ENDSCREEN, WC_NONE,
	0,
	_nested_highscore_widgets, lengthof(_nested_highscore_widgets)
);

/**
 * Show the highscore table for a given difficulty. When called from
 * endgame ranking is set to the top5 element that was newly added
 * and is thus highlighted
 */
void ShowHighscoreTable(int difficulty, int8 ranking)
{
	DeleteWindowByClass(WC_HIGHSCORE);
	new HighScoreWindow(&_highscore_desc, difficulty, ranking);
}

/**
 * Show the endgame victory screen in 2050. Update the new highscore
 * if it was high enough
 */
void ShowEndGameChart()
{
	/* Dedicated server doesn't need the highscore window and neither does -v null. */
	if (_network_dedicated || (!_networking && !Company::IsValidID(_local_company))) return;

	HideVitalWindows();
	DeleteWindowByClass(WC_ENDSCREEN);
	new EndGameWindow(&_endgame_desc);
}
