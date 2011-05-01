/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_type.h Types for strings. */

#ifndef STRING_TYPE_H
#define STRING_TYPE_H

/** A non-breaking space. */
#define NBSP "\xC2\xA0"

/** A left-to-right marker, marks the next character as left-to-right. */
#define LRM "\xE2\x80\x8E"

/**
 * Valid filter types for IsValidChar.
 */
enum CharSetFilter {
	CS_ALPHANUMERAL,      ///< Both numeric and alphabetic and spaces and stuff
	CS_NUMERAL,           ///< Only numeric ones
	CS_NUMERAL_SPACE,     ///< Only numbers and spaces
	CS_ALPHA,             ///< Only alphabetic values
	CS_HEXADECIMAL,       ///< Only hexadecimal characters
};

/** Type for wide characters, i.e. non-UTF8 encoded unicode characters. */
typedef uint32 WChar;

/* The following are directional formatting codes used to get the LTR and RTL strings right:
 * http://www.unicode.org/unicode/reports/tr9/#Directional_Formatting_Codes */
static const WChar CHAR_TD_LRM = 0x200E; ///< The next character acts like a left-to-right character.
static const WChar CHAR_TD_RLM = 0x200F; ///< The next character acts like a right-to-left character.
static const WChar CHAR_TD_LRE = 0x202A; ///< The following text is embedded left-to-right.
static const WChar CHAR_TD_RLE = 0x202B; ///< The following text is embedded right-to-left.
static const WChar CHAR_TD_LRO = 0x202D; ///< Force the following characters to be treated as left-to-right characters.
static const WChar CHAR_TD_RLO = 0x202E; ///< Force the following characters to be treated as right-to-left characters.
static const WChar CHAR_TD_PDF = 0x202C; ///< Restore the text-direction state to before the last LRE, RLE, LRO or RLO.

#endif /* STRING_TYPE_H */
