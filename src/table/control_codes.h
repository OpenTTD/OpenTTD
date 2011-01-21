/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file control_codes.h Control codes that are embedded in the translation strings. */

#ifndef CONTROL_CODES_H
#define CONTROL_CODES_H

/**
 * List of string control codes used for string formatting, displaying, and
 * by strgen to generate the language files.
 */
enum StringControlCode {
	SCC_CONTROL_START = 0xE000,
	SCC_CONTROL_END   = 0xE1FF,

	SCC_SPRITE_START  = 0xE200,
	SCC_SPRITE_END    = SCC_SPRITE_START + 0xFF,

	/* Display control codes */
	SCC_SETX = SCC_CONTROL_START,
	SCC_SETXY,
	SCC_TINYFONT,  ///< Switch to small font
	SCC_BIGFONT,   ///< Switch to large font

	/* Formatting control codes */
	SCC_REVISION,
	SCC_COMPANY_NUM,
	SCC_STATION_FEATURES,
	SCC_INDUSTRY_NAME,
	SCC_WAYPOINT_NAME,
	SCC_STATION_NAME,
	SCC_DEPOT_NAME,
	SCC_TOWN_NAME,
	SCC_GROUP_NAME,
	SCC_VEHICLE_NAME,
	SCC_SIGN_NAME,
	SCC_COMPANY_NAME,
	SCC_PRESIDENT_NAME,
	SCC_ENGINE_NAME,

	SCC_CURRENCY_COMPACT,
	SCC_CURRENCY,

	SCC_CARGO,
	SCC_CARGO_SHORT,
	SCC_POWER,
	SCC_VOLUME,
	SCC_VOLUME_SHORT,
	SCC_WEIGHT,
	SCC_WEIGHT_SHORT,
	SCC_FORCE,
	SCC_VELOCITY,
	SCC_HEIGHT,

	SCC_DATE_TINY,
	SCC_DATE_SHORT,
	SCC_DATE_LONG,
	SCC_DATE_ISO,

	SCC_STRING1,
	SCC_STRING2,
	SCC_STRING3,
	SCC_STRING4,
	SCC_STRING5,

	SCC_STRING,
	SCC_COMMA,
	SCC_NUM,
	SCC_ZEROFILL_NUM,
	SCC_HEX,
	SCC_BYTES,

	SCC_STRING_ID,
	SCC_RAW_STRING_POINTER,
	SCC_PLURAL_LIST,
	SCC_GENDER_LIST,
	SCC_GENDER_INDEX,
	SCC_ARG_INDEX,
	SCC_SETCASE,
	SCC_SWITCH_CASE,

	/* Colour codes */
	SCC_BLUE,
	SCC_SILVER,
	SCC_GOLD,
	SCC_RED,
	SCC_PURPLE,
	SCC_LTBROWN,
	SCC_ORANGE,
	SCC_GREEN,
	SCC_YELLOW,
	SCC_DKGREEN,
	SCC_CREAM,
	SCC_BROWN,
	SCC_WHITE,
	SCC_LTBLUE,
	SCC_GRAY,
	SCC_DKBLUE,
	SCC_BLACK,
	SCC_PREVIOUS_COLOUR,

	/**
	 * The next variables are part of a NewGRF subsystem for creating text strings.
	 * It uses a "stack" of bytes and reads from there.
	 */
	SCC_NEWGRF_FIRST,
	SCC_NEWGRF_PRINT_DWORD = SCC_NEWGRF_FIRST, ///< Read 4 bytes from the stack
	SCC_NEWGRF_PRINT_SIGNED_WORD,              ///< Read 2 bytes from the stack as signed value
	SCC_NEWGRF_PRINT_SIGNED_BYTE,              ///< Read 1 byte from the stack as signed value
	SCC_NEWGRF_PRINT_UNSIGNED_WORD,            ///< Read 2 bytes from the stack as unsigned value
	SCC_NEWGRF_PRINT_DWORD_CURRENCY,           ///< Read 4 bytes from the stack as currency
	SCC_NEWGRF_PRINT_STRING_ID,                ///< Read 2 bytes from the stack as String ID
	SCC_NEWGRF_PRINT_DATE,                     ///< Read 2 bytes from the stack as base 1920 date
	SCC_NEWGRF_PRINT_MONTH_YEAR,               ///< Read 2 bytes from the stack as base 1920 date
	SCC_NEWGRF_PRINT_WORD_SPEED,               ///< Read 2 bytes from the stack as signed speed
	SCC_NEWGRF_PRINT_WORD_VOLUME,              ///< Read 2 bytes from the stack as signed volume
	SCC_NEWGRF_PRINT_WORD_WEIGHT,              ///< Read 2 bytes from the stack as signed weight
	SCC_NEWGRF_PRINT_WORD_STATION_NAME,        ///< Read 2 bytes from the stack as station name
	SCC_NEWGRF_PRINT_QWORD_CURRENCY,           ///< Read 8 bytes from the stack as currency
	SCC_NEWGRF_PRINT_HEX_BYTE,                 ///< Read 1 byte from the stack and print it as hex
	SCC_NEWGRF_PRINT_HEX_WORD,                 ///< Read 2 bytes from the stack and print it as hex
	SCC_NEWGRF_PRINT_HEX_DWORD,                ///< Read 4 bytes from the stack and print it as hex
	SCC_NEWGRF_PRINT_HEX_QWORD,                ///< Read 8 bytes from the stack and print it as hex
	SCC_NEWGRF_PUSH_WORD,                      ///< Pushes 2 bytes onto the stack
	SCC_NEWGRF_UNPRINT,                        ///< "Unprints" the given number of bytes from the string
	SCC_NEWGRF_DISCARD_WORD,                   ///< Discard the next two bytes
	SCC_NEWGRF_ROTATE_TOP_4_WORDS,             ///< Rotate the top 4 words of the stack (W4 W1 W2 W3)
	SCC_NEWGRF_LAST = SCC_NEWGRF_ROTATE_TOP_4_WORDS,

	SCC_NEWGRF_STRINL,                         ///< Inline another string at the current position, StringID is encoded in the string

	/* Special printable symbols.
	 * These are mapped to the original glyphs */
	SCC_LESSTHAN       = SCC_SPRITE_START + 0x3C,
	SCC_GREATERTHAN    = SCC_SPRITE_START + 0x3E,
	SCC_UPARROW        = SCC_SPRITE_START + 0xA0,
	SCC_DOWNARROW      = SCC_SPRITE_START + 0xAA,
	SCC_CHECKMARK      = SCC_SPRITE_START + 0xAC,
	SCC_CROSS          = SCC_SPRITE_START + 0xAD,
	SCC_RIGHTARROW     = SCC_SPRITE_START + 0xAF,
	SCC_TRAIN          = SCC_SPRITE_START + 0xB4,
	SCC_LORRY          = SCC_SPRITE_START + 0xB5,
	SCC_BUS            = SCC_SPRITE_START + 0xB6,
	SCC_PLANE          = SCC_SPRITE_START + 0xB7,
	SCC_SHIP           = SCC_SPRITE_START + 0xB8,
	SCC_SUPERSCRIPT_M1 = SCC_SPRITE_START + 0xB9,
	SCC_SMALLUPARROW   = SCC_SPRITE_START + 0xBC,
	SCC_SMALLDOWNARROW = SCC_SPRITE_START + 0xBD,
};

#endif /* CONTROL_CODES_H */
