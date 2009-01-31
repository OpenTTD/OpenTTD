/* $Id$ */

/** @file highscore.cpp Definition of functions used for highscore handling */

#include "highscore.h"
#include "settings_type.h"
#include "company_base.h"
#include "company_func.h"
#include "cheat_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "table/strings.h"
#include "core/sort_func.hpp"
#include "variables.h"
#include "debug.h"

HighScore _highscore_table[5][5]; // 4 difficulty-settings (+ network); top 5

static const StringID _endgame_perf_titles[] = {
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0213_BUSINESSMAN,
	STR_0214_ENTREPRENEUR,
	STR_0214_ENTREPRENEUR,
	STR_0215_INDUSTRIALIST,
	STR_0215_INDUSTRIALIST,
	STR_0216_CAPITALIST,
	STR_0216_CAPITALIST,
	STR_0217_MAGNATE,
	STR_0217_MAGNATE,
	STR_0218_MOGUL,
	STR_0218_MOGUL,
	STR_0219_TYCOON_OF_THE_CENTURY
};

StringID EndGameGetPerformanceTitleFromValue(uint value)
{
	value = minu(value / 64, lengthof(_endgame_perf_titles) - 1);

	return _endgame_perf_titles[value];
}

/** Save the highscore for the company */
int8 SaveHighScoreValue(const Company *c)
{
	HighScore *hs = _highscore_table[_settings_game.difficulty.diff_level];
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
static int CDECL HighScoreSorter(const Company * const *a, const Company * const *b)
{
	return (*b)->old_economy[0].performance_history - (*a)->old_economy[0].performance_history;
}

/* Save the highscores in a network game when it has ended */
#define LAST_HS_ITEM lengthof(_highscore_table) - 1
int8 SaveHighScoreValueNetwork()
{
	const Company *c;
	const Company *cl[MAX_COMPANIES];
	uint count = 0;
	int8 company = -1;

	/* Sort all active companies with the highest score first */
	FOR_ALL_COMPANIES(c) cl[count++] = c;

	QSortT(cl, count, &HighScoreSorter);

	{
		uint i;

		memset(_highscore_table[LAST_HS_ITEM], 0, sizeof(_highscore_table[0]));

		/* Copy over Top5 companies */
		for (i = 0; i < lengthof(_highscore_table[LAST_HS_ITEM]) && i < count; i++) {
			HighScore *hs = &_highscore_table[LAST_HS_ITEM][i];

			SetDParam(0, cl[i]->index);
			SetDParam(1, cl[i]->index);
			GetString(hs->company, STR_HIGHSCORE_NAME, lastof(hs->company)); // get manager/company name string
			hs->score = cl[i]->old_economy[0].performance_history;
			hs->title = EndGameGetPerformanceTitleFromValue(hs->score);

			/* get the ranking of the local company */
			if (cl[i]->index == _local_company) company = i;
		}
	}

	/* Add top5 companys to highscore table */
	return company;
}

/** Save HighScore table to file */
void SaveToHighScore()
{
	FILE *fp = fopen(_highscore_file, "wb");

	if (fp != NULL) {
		uint i;
		HighScore *hs;

		for (i = 0; i < LAST_HS_ITEM; i++) { // don't save network highscores
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				/* First character is a command character, so strlen will fail on that */
				byte length = min(sizeof(hs->company), StrEmpty(hs->company) ? 0 : (int)strlen(&hs->company[1]) + 1);

				if (fwrite(&length, sizeof(length), 1, fp)       != 1 || // write away string length
						fwrite(hs->company, length, 1, fp)           >  1 || // Yes... could be 0 bytes too
						fwrite(&hs->score, sizeof(hs->score), 1, fp) != 1 ||
						fwrite("  ", 2, 1, fp)                       != 1) { // XXX - placeholder for hs->title, not saved anymore; compatibility
					DEBUG(misc, 1, "Could not save highscore.");
					i = LAST_HS_ITEM;
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

	if (fp != NULL) {
		uint i;
		HighScore *hs;

		for (i = 0; i < LAST_HS_ITEM; i++) { // don't load network highscores
			for (hs = _highscore_table[i]; hs != endof(_highscore_table[i]); hs++) {
				byte length;
				if (fread(&length, sizeof(length), 1, fp)       !=  1 ||
						fread(hs->company, length, 1, fp)           >   1 || // Yes... could be 0 bytes too
						fread(&hs->score, sizeof(hs->score), 1, fp) !=  1 ||
						fseek(fp, 2, SEEK_CUR)                      == -1) { // XXX - placeholder for hs->title, not saved anymore; compatibility
					DEBUG(misc, 1, "Highscore corrupted");
					i = LAST_HS_ITEM;
					break;
				}
				*lastof(hs->company) = '\0';
				hs->title = EndGameGetPerformanceTitleFromValue(hs->score);
			}
		}
		fclose(fp);
	}
}
