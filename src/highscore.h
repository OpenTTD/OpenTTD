/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file highscore.h Declaration of functions and types defined in highscore.h and highscore_gui.h */

#ifndef HIGHSCORE_H
#define HIGHSCORE_H

#include "strings_type.h"
#include "company_type.h"
#include "settings_type.h"

struct HighScore {
	std::string name; ///< The name of the companyy and president.
	StringID title = INVALID_STRING_ID; ///< NOSAVE, has troubles with changing string-numbers.
	uint16_t score = 0; ///< The score for this high score. Do NOT change type, will break hs.dat
};

using HighScores = std::array<HighScore, 5>; ///< Record 5 high scores
using HighScoresTable = std::array<HighScores, SP_HIGHSCORE_END>; ///< Record high score for each of the difficulty levels
extern HighScoresTable _highscore_table;

void SaveToHighScore();
void LoadFromHighScore();
int8_t SaveHighScoreValue(const Company *c);
int8_t SaveHighScoreValueNetwork();
StringID EndGameGetPerformanceTitleFromValue(uint value);
void ShowHighscoreTable(int difficulty = SP_CUSTOM, int8_t rank = -1);

#endif /* HIGHSCORE_H */
