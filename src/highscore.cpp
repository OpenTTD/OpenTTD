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
#include "fileio_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "table/strings.h"
#include "debug.h"

#include "safeguards.h"

HighScoresTable _highscore_table; ///< Table with all the high scores.
std::string _highscore_file; ///< The file to store the highscore data in.

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
	value = std::min<uint>(value / 64, lengthof(_endgame_perf_titles) - 1);

	return _endgame_perf_titles[value];
}

/**
 * Save the highscore for the company
 * @param c The company to insert.
 * @return The index the company got in the high score table, or -1 when it did not end up in the table.
 */
int8_t SaveHighScoreValue(const Company *c)
{
	/* Exclude cheaters from the honour of being in the highscore table */
	if (CheatHasBeenUsed()) return -1;

	auto &highscores = _highscore_table[SP_CUSTOM];
	uint16_t score = c->old_economy[0].performance_history;

	auto it = std::find_if(highscores.begin(), highscores.end(), [&score](auto &highscore) { return highscore.score <= score; });

	/* If we cannot find it, our score is not high enough. */
	if (it == highscores.end()) return -1;

	/* Move all elements one down starting from the replaced one */
	std::move_backward(it, highscores.end() - 1, highscores.end());

	/* Fill the elements. */
	SetDParam(0, c->index);
	SetDParam(1, c->index);
	it->name = GetString(STR_HIGHSCORE_NAME); // get manager/company name string
	it->score = score;
	it->title = EndGameGetPerformanceTitleFromValue(score);
	return std::distance(highscores.begin(), it);
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
int8_t SaveHighScoreValueNetwork()
{
	const Company *cl[MAX_COMPANIES];
	size_t count = 0;
	int8_t local_company_place = -1;

	/* Sort all active companies with the highest score first */
	for (const Company *c : Company::Iterate()) cl[count++] = c;

	std::sort(std::begin(cl), std::begin(cl) + count, HighScoreSorter);

	/* Clear the high scores from the previous network game. */
	auto &highscores = _highscore_table[SP_MULTIPLAYER];
	std::fill(highscores.begin(), highscores.end(), HighScore{});

	for (size_t i = 0; i < count && i < highscores.size(); i++) {
		const Company *c = cl[i];
		auto &highscore = highscores[i];
		SetDParam(0, c->index);
		SetDParam(1, c->index);
		highscore.name = GetString(STR_HIGHSCORE_NAME); // get manager/company name string
		highscore.score = c->old_economy[0].performance_history;
		highscore.title = EndGameGetPerformanceTitleFromValue(highscore.score);

		if (c->index == _local_company) local_company_place = static_cast<int8_t>(i);
	}

	return local_company_place;
}

/** Save HighScore table to file */
void SaveToHighScore()
{
	std::unique_ptr<FILE, FileDeleter> fp(fopen(_highscore_file.c_str(), "wb"));
	if (fp == nullptr) return;

	/* Does not iterate through the complete array!. */
	for (int i = 0; i < SP_SAVED_HIGHSCORE_END; i++) {
		for (HighScore &hs : _highscore_table[i]) {
			/* This code is weird and old fashioned to keep compatibility with the old high score files. */
			byte name_length = ClampTo<byte>(hs.name.size());
			if (fwrite(&name_length, sizeof(name_length), 1, fp.get()) != 1 || // Write the string length of the name
					fwrite(hs.name.data(), name_length, 1, fp.get()) > 1 || // Yes... could be 0 bytes too
					fwrite(&hs.score, sizeof(hs.score), 1, fp.get()) != 1 ||
					fwrite("  ", 2, 1, fp.get()) != 1) { // Used to be hs.title, not saved anymore; compatibility
				Debug(misc, 1, "Could not save highscore.");
				return;
			}
		}
	}
}

/** Initialize the highscore table to 0 and if any file exists, load in values */
void LoadFromHighScore()
{
	std::fill(_highscore_table.begin(), _highscore_table.end(), HighScores{});

	std::unique_ptr<FILE, FileDeleter> fp(fopen(_highscore_file.c_str(), "rb"));
	if (fp == nullptr) return;

	/* Does not iterate through the complete array!. */
	for (int i = 0; i < SP_SAVED_HIGHSCORE_END; i++) {
		for (HighScore &hs : _highscore_table[i]) {
			/* This code is weird and old fashioned to keep compatibility with the old high score files. */
			byte name_length;
			char buffer[std::numeric_limits<decltype(name_length)>::max() + 1];

			if (fread(&name_length, sizeof(name_length), 1, fp.get()) !=  1 ||
					fread(buffer, name_length, 1, fp.get()) > 1 || // Yes... could be 0 bytes too
					fread(&hs.score, sizeof(hs.score), 1, fp.get()) !=  1 ||
					fseek(fp.get(), 2, SEEK_CUR) == -1) { // Used to be hs.title, not saved anymore; compatibility
				Debug(misc, 1, "Highscore corrupted");
				return;
			}
			hs.name = StrMakeValid(std::string_view(buffer, name_length));
			hs.title = EndGameGetPerformanceTitleFromValue(hs.score);
		}
	}
}
