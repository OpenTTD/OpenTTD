/* $Id$ */

/** @file highscore.h Declaration of functions and types defined in highscore.h and highscore_gui.h */

#ifndef HIGHSCORE_H
#define HIGHSCORE_H

#include "stdafx.h"
#include "strings_type.h"
#include "core/math_func.hpp"
#include "company_type.h"

struct HighScore {
	char company[100];
	StringID title; ///< NOSAVE, has troubles with changing string-numbers.
	uint16 score;   ///< do NOT change type, will break hs.dat
};

extern HighScore _highscore_table[5][5]; // 4 difficulty-settings (+ network); top 5

void SaveToHighScore();
void LoadFromHighScore();
int8 SaveHighScoreValue(const Company *c);
int8 SaveHighScoreValueNetwork();
StringID EndGameGetPerformanceTitleFromValue(uint value);
void ShowHighscoreTable(int difficulty, int8 rank);

#endif /* HIGHSCORE_H */
