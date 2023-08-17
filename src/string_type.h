/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_type.h Types for strings. */

#ifndef STRING_TYPE_H
#define STRING_TYPE_H

#include "core/enum_type.hpp"

/** A non-breaking space. */
#define NBSP u8"\u00a0"

/** A left-to-right marker, marks the next character as left-to-right. */
#define LRM u8"\u200e"

/**
 * Valid filter types for IsValidChar.
 */
enum CharSetFilter {
	CS_ALPHANUMERAL,      ///< Both numeric and alphabetic and spaces and stuff
	CS_NUMERAL,           ///< Only numeric ones
	CS_NUMERAL_SPACE,     ///< Only numbers and spaces
	CS_NUMERAL_SIGNED,    ///< Only numbers and '-' for negative values
	CS_ALPHA,             ///< Only alphabetic values
	CS_HEXADECIMAL,       ///< Only hexadecimal characters
};

/* The following are directional formatting codes used to get the LTR and RTL strings right:
 * http://www.unicode.org/unicode/reports/tr9/#Directional_Formatting_Codes */
static const char32_t CHAR_TD_LRM = 0x200E; ///< The next character acts like a left-to-right character.
static const char32_t CHAR_TD_RLM = 0x200F; ///< The next character acts like a right-to-left character.
static const char32_t CHAR_TD_LRE = 0x202A; ///< The following text is embedded left-to-right.
static const char32_t CHAR_TD_RLE = 0x202B; ///< The following text is embedded right-to-left.
static const char32_t CHAR_TD_LRO = 0x202D; ///< Force the following characters to be treated as left-to-right characters.
static const char32_t CHAR_TD_RLO = 0x202E; ///< Force the following characters to be treated as right-to-left characters.
static const char32_t CHAR_TD_PDF = 0x202C; ///< Restore the text-direction state to before the last LRE, RLE, LRO or RLO.

/** Settings for the string validation. */
enum StringValidationSettings {
	SVS_NONE                       = 0,      ///< Allow nothing and replace nothing.
	SVS_REPLACE_WITH_QUESTION_MARK = 1 << 0, ///< Replace the unknown/bad bits with question marks.
	SVS_ALLOW_NEWLINE              = 1 << 1, ///< Allow newlines; replaces '\r\n' with '\n' during processing.
	SVS_ALLOW_CONTROL_CODE         = 1 << 2, ///< Allow the special control codes.
	/**
	 * Replace tabs ('\t'), carriage returns ('\r') and newlines ('\n') with spaces.
	 * When #SVS_ALLOW_NEWLINE is set, a '\n' or '\r\n' combination are not replaced with a space. A lone '\r' is replaced with a space.
	 * When #SVS_REPLACE_WITH_QUESTION_MARK is set, this replacement runs first.
	 */
	SVS_REPLACE_TAB_CR_NL_WITH_SPACE = 1 << 3,
};
DECLARE_ENUM_AS_BIT_SET(StringValidationSettings)


/** Type for a list of strings. */
typedef std::vector<std::string> StringList;

#endif /* STRING_TYPE_H */
