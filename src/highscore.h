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
	/**
	 * The name of the company and president.
	 * The + 5 is for the comma and space or possibly other characters
	 * that join the two names in this single string and the '\0'.
	 */
	char company[(MAX_LENGTH_COMPANY_NAME_CHARS + MAX_LENGTH_PRESIDENT_NAME_CHARS + 5) * MAX_CHAR_LENGTH];
	StringID title; ///< NOSAVE, has troubles with changing string-numbers.
	uint16 score;   ///< The score for this high score. Do NOT change type, will break hs.dat
};

extern HighScore _highscore_table[SP_HIGHSCORE_END][5];

void SaveToHighScore();
void LoadFromHighScore();
int8 SaveHighScoreValue(const Company *c);
int8 SaveHighScoreValueNetwork();
StringID EndGameGetPerformanceTitleFromValue(uint value);
void ShowHighscoreTable(int difficulty = SP_CUSTOM, int8 rank = -1);

#endif /* HIGHSCORE_H */
