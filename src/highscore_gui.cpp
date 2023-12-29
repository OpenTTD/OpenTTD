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
#include "zoom_func.h"
#include "misc_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "widgets/highscore_widget.h"

#include "safeguards.h"

struct EndGameHighScoreBaseWindow : Window {
	uint32_t background_img;
	int8_t rank;

	EndGameHighScoreBaseWindow(WindowDesc *desc) : Window(desc)
	{
		this->InitNested();
		CLRBITS(this->flags, WF_WHITE_BORDER);
		ResizeWindow(this, _screen.width - this->width, _screen.height - this->height);
	}

	/* Always draw a maximized window and within it the centered background */
	void SetupHighScoreEndWindow()
	{
		/* Resize window to "full-screen". */
		if (this->width != _screen.width || this->height != _screen.height) ResizeWindow(this, _screen.width - this->width, _screen.height - this->height);

		this->DrawWidgets();

		/* Standard background slices are 50 pixels high, but it's designed
		 * for 480 pixels total. 96% of 500 is 480. */
		Dimension dim = GetSpriteSize(this->background_img);
		Point pt = this->GetTopLeft(dim.width, dim.height * 96 / 10);
		/* Center Highscore/Endscreen background */
		for (uint i = 0; i < 10; i++) { // the image is split into 10 50px high parts
			DrawSprite(this->background_img + i, PAL_NONE, pt.x, pt.y + (i * dim.height));
		}
	}

	/** Return the coordinate of the screen such that a window of 640x480 is centered at the screen. */
	Point GetTopLeft(int x, int y)
	{
		Point pt = {std::max(0, (_screen.width / 2) - (x / 2)), std::max(0, (_screen.height / 2) - (y / 2))};
		return pt;
	}

	void OnClick([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget, [[maybe_unused]] int click_count) override
	{
		this->Close();
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
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
				this->Close();
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
	EndGameWindow(WindowDesc *desc) : EndGameHighScoreBaseWindow(desc)
	{
		/* Pause in single-player to have a look at the highscore at your own leisure */
		if (!_networking) Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, true);

		this->background_img = SPR_TYCOON_IMG1_BEGIN;

		if (_local_company != COMPANY_SPECTATOR) {
			const Company *c = Company::Get(_local_company);
			if (c->old_economy[0].performance_history == SCORE_MAX) {
				this->background_img = SPR_TYCOON_IMG2_BEGIN;
			}
		}

		/* In a network game show the endscores of the custom difficulty 'network' which is
		 * a TOP5 of that game, and not an all-time TOP5. */
		if (_networking) {
			this->window_number = SP_MULTIPLAYER;
			this->rank = SaveHighScoreValueNetwork();
		} else {
			/* in singleplayer mode _local company is always valid */
			const Company *c = Company::Get(_local_company);
			this->window_number = SP_CUSTOM;
			this->rank = SaveHighScoreValue(c);
		}

		MarkWholeScreenDirty();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (!_networking) Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, false); // unpause
		if (_game_mode != GM_MENU) ShowHighscoreTable(this->window_number, this->rank);
		this->EndGameHighScoreBaseWindow::Close();
	}

	void OnPaint() override
	{
		this->SetupHighScoreEndWindow();
		Point pt = this->GetTopLeft(ScaleSpriteTrad(640), ScaleSpriteTrad(480));

		const Company *c = Company::GetIfValid(_local_company);
		if (c == nullptr) return;

		/* We need to get performance from last year because the image is shown
		 * at the start of the new year when these things have already been copied */
		if (this->background_img == SPR_TYCOON_IMG2_BEGIN) { // Tycoon of the century \o/
			SetDParam(0, c->index);
			SetDParam(1, c->index);
			SetDParam(2, EndGameGetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawStringMultiLine(pt.x + ScaleSpriteTrad(15), pt.x + ScaleSpriteTrad(640) - ScaleSpriteTrad(25), pt.y + ScaleSpriteTrad(90), pt.y + ScaleSpriteTrad(160), STR_HIGHSCORE_PRESIDENT_OF_COMPANY_ACHIEVES_STATUS, TC_FROMSTRING, SA_CENTER);
		} else {
			SetDParam(0, c->index);
			SetDParam(1, EndGameGetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawStringMultiLine(pt.x + ScaleSpriteTrad(36), pt.x + ScaleSpriteTrad(640), pt.y + ScaleSpriteTrad(140), pt.y + ScaleSpriteTrad(206), STR_HIGHSCORE_COMPANY_ACHIEVES_STATUS, TC_FROMSTRING, SA_CENTER);
		}
	}
};

struct HighScoreWindow : EndGameHighScoreBaseWindow {
	bool game_paused_by_player; ///< True if the game was paused by the player when the highscore window was opened.

	HighScoreWindow(WindowDesc *desc, int difficulty, int8_t ranking) : EndGameHighScoreBaseWindow(desc)
	{
		/* pause game to show the chart */
		this->game_paused_by_player = _pause_mode == PM_PAUSED_NORMAL;
		if (!_networking && !this->game_paused_by_player) Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, true);

		/* Close all always on-top windows to get a clean screen */
		if (_game_mode != GM_MENU) HideVitalWindows();

		MarkWholeScreenDirty();
		this->window_number = difficulty; // show highscore chart for difficulty...
		this->background_img = SPR_HIGHSCORE_CHART_BEGIN; // which background to show
		this->rank = ranking;
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (_game_mode != GM_MENU) ShowVitalWindows();

		if (!_networking && !this->game_paused_by_player) Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, false); // unpause

		this->EndGameHighScoreBaseWindow::Close();
	}

	void OnPaint() override
	{
		const auto &hs = _highscore_table[this->window_number];

		this->SetupHighScoreEndWindow();
		Point pt = this->GetTopLeft(ScaleSpriteTrad(640), ScaleSpriteTrad(480));

		/* Draw the title. */
		DrawStringMultiLine(pt.x + ScaleSpriteTrad(70), pt.x + ScaleSpriteTrad(570), pt.y, pt.y + ScaleSpriteTrad(140), STR_HIGHSCORE_TOP_COMPANIES, TC_FROMSTRING, SA_CENTER);

		/* Draw Highscore peepz */
		for (uint8_t i = 0; i < ClampTo<uint8_t>(hs.size()); i++) {
			SetDParam(0, i + 1);
			DrawString(pt.x + ScaleSpriteTrad(40), pt.x + ScaleSpriteTrad(600), pt.y + ScaleSpriteTrad(140 + i * 55), STR_HIGHSCORE_POSITION);

			if (!hs[i].name.empty()) {
				TextColour colour = (this->rank == i) ? TC_RED : TC_BLACK; // draw new highscore in red

				SetDParamStr(0, hs[i].name);
				DrawString(pt.x + ScaleSpriteTrad(71), pt.x + ScaleSpriteTrad(569), pt.y + ScaleSpriteTrad(140 + i * 55), STR_JUST_BIG_RAW_STRING, colour);
				SetDParam(0, hs[i].title);
				SetDParam(1, hs[i].score);
				DrawString(pt.x + ScaleSpriteTrad(71), pt.x + ScaleSpriteTrad(569), pt.y + ScaleSpriteTrad(140) + GetCharacterHeight(FS_LARGE) + ScaleSpriteTrad(i * 55), STR_HIGHSCORE_STATS, colour);
			}
		}
	}
};

static const NWidgetPart _nested_highscore_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_H_BACKGROUND), SetResize(1, 1), EndContainer(),
};

static WindowDesc _highscore_desc(__FILE__, __LINE__,
	WDP_MANUAL, nullptr, 0, 0,
	WC_HIGHSCORE, WC_NONE,
	0,
	std::begin(_nested_highscore_widgets), std::end(_nested_highscore_widgets)
);

static WindowDesc _endgame_desc(__FILE__, __LINE__,
	WDP_MANUAL, nullptr, 0, 0,
	WC_ENDSCREEN, WC_NONE,
	0,
	std::begin(_nested_highscore_widgets), std::end(_nested_highscore_widgets)
);

/**
 * Show the highscore table for a given difficulty. When called from
 * endgame ranking is set to the top5 element that was newly added
 * and is thus highlighted
 */
void ShowHighscoreTable(int difficulty, int8_t ranking)
{
	CloseWindowByClass(WC_HIGHSCORE);
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
	CloseWindowByClass(WC_ENDSCREEN);
	new EndGameWindow(&_endgame_desc);
}

static IntervalTimer<TimerGameCalendar> _check_end_game({TimerGameCalendar::YEAR, TimerGameCalendar::Priority::NONE}, [](auto)
{
	/* 0 = never */
	if (_settings_game.game_creation.ending_year == 0) return;

	/* Show the end-game chart at the end of the ending year (hence the + 1). */
	if (TimerGameCalendar::year == _settings_game.game_creation.ending_year + 1) {
		ShowEndGameChart();
	}
});
