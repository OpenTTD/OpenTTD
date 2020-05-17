/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_text.hpp Base functions regarding game texts. */

#ifndef GAME_TEXT_HPP
#define GAME_TEXT_HPP

#include "../core/smallvec_type.hpp"

const char *GetGameStringPtr(uint id);
void RegisterGameTranslation(class Squirrel *engine);
void ReconsiderGameScriptLanguage();

/** Container for the raw (unencoded) language strings of a language. */
struct LanguageStrings {
	std::string language; ///< Name of the language (base filename). Empty string if invalid.
	StringList  lines;    ///< The lines of the file to pass into the parser/encoder.

	LanguageStrings() {}
	LanguageStrings(const std::string &lang) : language(lang) {}
	LanguageStrings(const LanguageStrings &other) : language(other.language), lines(other.lines) {}
	LanguageStrings(LanguageStrings &&other) : language(std::move(other.language)), lines(std::move(other.lines)) {}

	bool IsValid() const { return !this->language.empty(); }
};

/** Container for all the game strings. */
struct GameStrings {
	uint version;                  ///< The version of the language strings.
	LanguageStrings *cur_language; ///< The current (compiled) language.

	std::vector<LanguageStrings> raw_strings;      ///< The raw strings per language, first must be English/the master language!.
	std::vector<LanguageStrings> compiled_strings; ///< The compiled strings per language, first must be English/the master language!.
	StringList string_names;                       ///< The names of the compiled strings.

	void Compile();

	GameStrings() = default;

	GameStrings(const GameStrings &) = delete;
	GameStrings(GameStrings &&) = delete;
	GameStrings &operator=(const GameStrings &) = delete;
	GameStrings &operator=(GameStrings &&) = delete;
};

#endif /* GAME_TEXT_HPP */
