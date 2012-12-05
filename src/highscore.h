/* $Id$ */

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
	char company[100];
	StringID title; ///< NOSAVE, has troubles with changing string-numbers.
	uint16 score;   ///< do NOT change type, will break hs.dat
};

extern HighScore _highscore_table[SP_HIGHSCORE_END][5];

void SaveToHighScore();
void LoadFromHighScore();
int8 SaveHighScoreValue(const Company *c);
int8 SaveHighScoreValueNetwork();
StringID EndGameGetPerformanceTitleFromValue(uint value);
void ShowHighscoreTable(int difficulty = SP_CUSTOM, int8 rank = -1);

#endif /* HIGHSCORE_H */
