/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file highscore.cpp Definition of functions used for highscore handling */

#include "stdafx.h"
#include "highscore.h"
#include "company_base.h"
#include "company_func.h"
#include "cheat_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "table/strings.h"
#include "core/sort_func.hpp"
#include "debug.h"

#include "safeguards.h"

HighScore _highscore_table[SP_HIGHSCORE_END][5]; ///< various difficulty-settings; top 5
char *_highscore_file; ///< The file to store the highscore data in.

static const StringID _endgame_perf_titles[] = {
	STR_HIGHSCORE_PERFORMANCE_TITLE_BUSINESSMAN,
	STR_HIGHSCORE_PERFORMANCE_TITLE_BUSINESSMAN,
	STR_HIGHSCORE_PERFORMANCE_TITLE_BUSINESSMAN,
	STR_HIGHSCORE_PERFORMANCE_TITLE_BUSINESSMAN,
	STR_HIGHSCORE_PERFORMANCE_TITLE_BUSINESSMAN,
	STR_HIGHSCORE_PERFORMANCE_TITLE_ENTREPRENEUR,
	STR_HIGHSCORE_PERFORMANCE_TITLE_ENTREPRENEUR,
	STR_HIGHSCORE_PERFORMANCE_TITLE_INDUSTRIALIST,
	STR_HIGHSCORE_PERFORMANCE_TITLE_INDUSTRIALIST,
	STR_HIGHSCORE_PERFORMANCE_TITLE_CAPITALIST,
	STR_HIGHSCORE_PERFORMANCE_TITLE_CAPITALIST,
	STR_HIGHSCORE_PERFORMANCE_TITLE_MAGNATE,
	STR_HIGHSCORE_PERFORMANCE_TITLE_MAGNATE,
	STR_HIGHSCORE_PERFORMANCE_TITLE_MOGUL,
	STR_HIGHSCORE_PERFORMANCE_TITLE_MOGUL,
	STR_HIGHSCORE_PERFORMANCE_TITLE_TYCOON_OF_THE_CENTURY
};

StringID EndGameGetPerformanceTitleFromValue(uint value)
{
	value = minu(value / 64, lengthof(_endgame_perf_titles) - 1);

	return _endgame_perf_titles[value];
}

/** Save the highscore for the company */
int8 SaveHighScoreValue(const Company *c)
{
	HighScore *hs = _highscore_table[SP_CUSTOM];
	uint i;
	uint16 score = c->old_economy[0].performance_history;

	/* Exclude cheaters from the honour of being in the highscore table */
	if (CheatHasBeenUsed()) return -1;

	for (i = 0; i < lengthof(_highscore_table[0]); i++) {
		/* You are in the TOP5. Move all values one down and save us there */
		if (hs[i].score <= score) {
			/* move all elements one down starting from the replaced one */
			memmove(&hs[i + 1], &hs[i], sizeof(HighScore) * (lengthof(_highscore_table[0]) - i - 1));
			SetDParam(0, c->index);
			SetDParam(1, c->index);
			GetString(hs[i].company, STR_HIGHSCORE_NAME, lastof(hs[i].company)); // get manager/company name string
			hs[i].score = score;
			hs[i].title = EndGameGetPerformanceTitleFromValue(score);
			return i;
		}
	}

	return -1; // too bad; we did not make it into the top5
}

/** Sort all companies given their performance */
static bool HighScoreSorter(const Company * const &a, const Company * const &b)
{
	return b->old_economy[0].performance_history < a->old_economy[0].performance_history;
}

/**
 * Save the highscores in a network game when it has ended
 * @return Position of the local company in the highscore list.
 */
int8 SaveHighScoreValueNetwork()
{
	const Company *c;
	const Company *cl[MAX_COMPANIES];
	uint count = 0;
	int8 company = -1;

	/* Sort all active companies with the highest score first */
	FOR_ALL_COMPANIES(c) cl[count++] = c;

	std::sort(std::begin(cl), std::begin(cl) + count, HighScoreSorter);

	{
		uint i;

		memset(_highscore_table[SP_MULTIPLAYER], 0, sizeof(_highscore_table[SP_MULTIPLAYER]));

		/* Copy over Top5 companies */
		for (i = 0; i < lengthof(_highscore_table[SP_MULTIPLAYER]) && i < count; i++) {
			HighScore *hs = &_highscore_table[SP_MULTIPLAYER][i];

			SetDParam(0, cl[i]->index);
			SetDParam(1, cl[i]->index);
			GetString(hs->company, STR_HIGHSCORE_NAME, lastof(hs->company)); // get manager/company name string
			hs->score = cl[i]->old_economy[0].performance_history;
			hs->title = EndGameGetPerformanceTitleFromValue(hs->score);

			/* get the ranking of the local company */
			if (cl[i]->index == _local_company) company = i;
		}
	}

	/* Add top5 companies to highscore table */
	return company;
}

/** Save HighScore table to file */
void SaveToHighScore()
{
	FILE *fp = fopen(_highscore_file, "wb");

	if (fp != nullptr) {
		uint i;
		HighScore *hs;

		for (i = 0; i < SP_SAVED_HIGHSCORE_END; i++) {
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				/* First character is a command character, so strlen will fail on that */
				byte length = min(sizeof(hs->company), StrEmpty(hs->company) ? 0 : (int)strlen(&hs->company[1]) + 1);

				if (fwrite(&length, sizeof(length), 1, fp)       != 1 || // write away string length
						fwrite(hs->company, length, 1, fp)           >  1 || // Yes... could be 0 bytes too
						fwrite(&hs->score, sizeof(hs->score), 1, fp) != 1 ||
						fwrite("  ", 2, 1, fp)                       != 1) { // XXX - placeholder for hs->title, not saved anymore; compatibility
					DEBUG(misc, 1, "Could not save highscore.");
					i = SP_SAVED_HIGHSCORE_END;
					break;
				}
			}
		}
		fclose(fp);
	}
}

/** Initialize the highscore table to 0 and if any file exists, load in values */
void LoadFromHighScore()
{
	FILE *fp = fopen(_highscore_file, "rb");

	memset(_highscore_table, 0, sizeof(_highscore_table));

	if (fp != nullptr) {
		uint i;
		HighScore *hs;

		for (i = 0; i < SP_SAVED_HIGHSCORE_END; i++) {
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				byte length;
				if (fread(&length, sizeof(length), 1, fp)                              !=  1 ||
						fread(hs->company, min<int>(lengthof(hs->company), length), 1, fp) >   1 || // Yes... could be 0 bytes too
						fread(&hs->score, sizeof(hs->score), 1, fp)                        !=  1 ||
						fseek(fp, 2, SEEK_CUR)                                             == -1) { // XXX - placeholder for hs->title, not saved anymore; compatibility
					DEBUG(misc, 1, "Highscore corrupted");
					i = SP_SAVED_HIGHSCORE_END;
					break;
				}
				str_validate(hs->company, lastof(hs->company), SVS_NONE);
				hs->title = EndGameGetPerformanceTitleFromValue(hs->score);
			}
		}
		fclose(fp);
	}
}
