/* $Id$ */

/** @file highscore_gui.cpp Definition of the HighScore and EndGame windows */

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
#include "settings_type.h"
#include "strings_func.h"
#include "openttd.h"

struct EndGameHighScoreBaseWindow : Window {
	uint32 background_img;
	int8 rank;

	EndGameHighScoreBaseWindow(const WindowDesc *desc) : Window(desc)
	{
	}

	/* Always draw a maximized window and within there the centered background */
	void SetupHighScoreEndWindow(uint *x, uint *y)
	{
		/* resize window to "full-screen" */
		this->width = _screen.width;
		this->height = _screen.height;
		this->widget[0].right = this->width - 1;
		this->widget[0].bottom = this->height - 1;

		this->DrawWidgets();

		/* Center Highscore/Endscreen background */
		*x = max(0, (_screen.width  / 2) - (640 / 2));
		*y = max(0, (_screen.height / 2) - (480 / 2));
		for (uint i = 0; i < 10; i++) { // the image is split into 10 50px high parts
			DrawSprite(this->background_img + i, PAL_NONE, *x, *y + (i * 50));
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		delete this;
	}
};

/** End game window shown at the end of the game */
struct EndGameWindow : EndGameHighScoreBaseWindow {
	EndGameWindow(const WindowDesc *desc) : EndGameHighScoreBaseWindow(desc)
	{
		/* Pause in single-player to have a look at the highscore at your own leisure */
		if (!_networking) DoCommandP(0, 1, 0, CMD_PAUSE);

		this->background_img = SPR_TYCOON_IMG1_BEGIN;

		if (_local_company != COMPANY_SPECTATOR) {
			const Company *c = GetCompany(_local_company);
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
			const Company *c = GetCompany(_local_company);
			this->window_number = _settings_game.difficulty.diff_level;
			this->rank = SaveHighScoreValue(c);
		}

		MarkWholeScreenDirty();
	}

	~EndGameWindow()
	{
		if (!_networking) DoCommandP(0, 0, 0, CMD_PAUSE); // unpause
		ShowHighscoreTable(this->window_number, this->rank);
	}

	virtual void OnPaint()
	{
		const Company *c;
		uint x, y;

		this->SetupHighScoreEndWindow(&x, &y);

		if (!IsValidCompanyID(_local_company)) return;

		c = GetCompany(_local_company);
		/* We need to get performance from last year because the image is shown
		 * at the start of the new year when these things have already been copied */
		if (this->background_img == SPR_TYCOON_IMG2_BEGIN) { // Tycoon of the century \o/
			SetDParam(0, c->index);
			SetDParam(1, c->index);
			SetDParam(2, EndGameGetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 107, STR_021C_OF_ACHIEVES_STATUS, 640);
		} else {
			SetDParam(0, c->index);
			SetDParam(1, EndGameGetPerformanceTitleFromValue(c->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 157, STR_021B_ACHIEVES_STATUS, 640);
		}
	}
};

struct HighScoreWindow : EndGameHighScoreBaseWindow {
	HighScoreWindow(const WindowDesc *desc, int difficulty, int8 ranking) : EndGameHighScoreBaseWindow(desc)
	{
		/* pause game to show the chart */
		if (!_networking) DoCommandP(0, 1, 0, CMD_PAUSE);

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

		if (!_networking) DoCommandP(0, 0, 0, CMD_PAUSE); // unpause
	}

	virtual void OnPaint()
	{
		const HighScore *hs = _highscore_table[this->window_number];
		uint x, y;

		this->SetupHighScoreEndWindow(&x, &y);

		SetDParam(0, ORIGINAL_END_YEAR);
		SetDParam(1, this->window_number + STR_6801_EASY);
		DrawStringMultiCenter(x + (640 / 2), y + 62, !_networking ? STR_0211_TOP_COMPANIES_WHO_REACHED : STR_TOP_COMPANIES_NETWORK_GAME, 500);

		/* Draw Highscore peepz */
		for (uint8 i = 0; i < lengthof(_highscore_table[0]); i++) {
			SetDParam(0, i + 1);
			DrawString(x + 40, y + 140 + (i * 55), STR_0212, TC_BLACK);

			if (hs[i].company[0] != '\0') {
				TextColour colour = (this->rank == i) ? TC_RED : TC_BLACK; // draw new highscore in red

				DoDrawString(hs[i].company, x + 71, y + 140 + (i * 55), colour);
				SetDParam(0, hs[i].title);
				SetDParam(1, hs[i].score);
				DrawString(x + 71, y + 160 + (i * 55), STR_HIGHSCORE_STATS, colour);
			}
		}
	}
};

static const Widget _highscore_widgets[] = {
{      WWT_PANEL, RESIZE_NONE,  COLOUR_END, 0, 640, 0, 480, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _highscore_desc(
	0, 0, 641, 481, 641, 481,
	WC_HIGHSCORE, WC_NONE,
	0,
	_highscore_widgets
);

static const WindowDesc _endgame_desc(
	0, 0, 641, 481, 641, 481,
	WC_ENDSCREEN, WC_NONE,
	0,
	_highscore_widgets
);

/** Show the highscore table for a given difficulty. When called from
 * endgame ranking is set to the top5 element that was newly added
 * and is thus highlighted */
void ShowHighscoreTable(int difficulty, int8 ranking)
{
	DeleteWindowByClass(WC_HIGHSCORE);
	new HighScoreWindow(&_highscore_desc, difficulty, ranking);
}

/** Show the endgame victory screen in 2050. Update the new highscore
 * if it was high enough */
void ShowEndGameChart()
{
	/* Dedicated server doesn't need the highscore window and neither does -v null. */
	if (_network_dedicated || (!_networking && !IsValidCompanyID(_local_company))) return;

	HideVitalWindows();
	DeleteWindowByClass(WC_ENDSCREEN);
	new EndGameWindow(&_endgame_desc);
}
