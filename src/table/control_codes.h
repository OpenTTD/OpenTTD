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

	/* This must be the first entry. It's encoded in strings that are saved. */
	SCC_ENCODED = SCC_CONTROL_START,

	/* Display control codes */
	SCC_SETX,
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

	SCC_CURRENCY_SHORT,
	SCC_CURRENCY_LONG,

	SCC_CARGO_LONG,
	SCC_CARGO_SHORT,
	SCC_CARGO_TINY,
	SCC_POWER,
	SCC_VOLUME_LONG,
	SCC_VOLUME_SHORT,
	SCC_WEIGHT_LONG,
	SCC_WEIGHT_SHORT,
	SCC_FORCE,
	SCC_VELOCITY,
	SCC_HEIGHT,

	SCC_DATE_TINY,
	SCC_DATE_SHORT,
	SCC_DATE_LONG,
	SCC_DATE_ISO,

	/* Must be consecutive */
	SCC_STRING1,
	SCC_STRING2,
	SCC_STRING3,
	SCC_STRING4,
	SCC_STRING5,
	SCC_STRING6,
	SCC_STRING7,


	SCC_STRING,
	SCC_COMMA,
	SCC_DECIMAL,
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
	SCC_SET_CASE,
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
	SCC_NEWGRF_PRINT_DWORD_SIGNED = SCC_NEWGRF_FIRST, ///< Read 4 bytes from the stack
	SCC_NEWGRF_PRINT_WORD_SIGNED,                     ///< Read 2 bytes from the stack as signed value
	SCC_NEWGRF_PRINT_BYTE_SIGNED,                     ///< Read 1 byte from the stack as signed value
	SCC_NEWGRF_PRINT_WORD_UNSIGNED,                   ///< Read 2 bytes from the stack as unsigned value
	SCC_NEWGRF_PRINT_DWORD_CURRENCY,                  ///< Read 4 bytes from the stack as currency
	SCC_NEWGRF_PRINT_WORD_STRING_ID,                  ///< Read 2 bytes from the stack as String ID
	SCC_NEWGRF_PRINT_WORD_DATE_LONG,                  ///< Read 2 bytes from the stack as base 1920 date
	SCC_NEWGRF_PRINT_WORD_DATE_SHORT,                 ///< Read 2 bytes from the stack as base 1920 date
	SCC_NEWGRF_PRINT_WORD_SPEED,                      ///< Read 2 bytes from the stack as signed speed
	SCC_NEWGRF_PRINT_WORD_VOLUME_LONG,                ///< Read 2 bytes from the stack as long signed volume
	SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG,                ///< Read 2 bytes from the stack as long unsigned weight
	SCC_NEWGRF_PRINT_WORD_STATION_NAME,               ///< Read 2 bytes from the stack as station name
	SCC_NEWGRF_PRINT_QWORD_CURRENCY,                  ///< Read 8 bytes from the stack as currency
	SCC_NEWGRF_PRINT_BYTE_HEX,                        ///< Read 1 byte from the stack and print it as hex
	SCC_NEWGRF_PRINT_WORD_HEX,                        ///< Read 2 bytes from the stack and print it as hex
	SCC_NEWGRF_PRINT_DWORD_HEX,                       ///< Read 4 bytes from the stack and print it as hex
	SCC_NEWGRF_PRINT_QWORD_HEX,                       ///< Read 8 bytes from the stack and print it as hex
	SCC_NEWGRF_PRINT_DWORD_DATE_LONG,                 ///< Read 4 bytes from the stack as base 0 date
	SCC_NEWGRF_PRINT_DWORD_DATE_SHORT,                ///< Read 4 bytes from the stack as base 0 date
	SCC_NEWGRF_PRINT_WORD_POWER,                      ///< Read 2 bytes from the stack as unsigned power
	SCC_NEWGRF_PRINT_WORD_VOLUME_SHORT,               ///< Read 2 bytes from the stack as short signed volume
	SCC_NEWGRF_PRINT_WORD_WEIGHT_SHORT,               ///< Read 2 bytes from the stack as short unsigned weight
	SCC_NEWGRF_PUSH_WORD,                             ///< Pushes 2 bytes onto the stack
	SCC_NEWGRF_UNPRINT,                               ///< "Unprints" the given number of bytes from the string
	SCC_NEWGRF_DISCARD_WORD,                          ///< Discard the next two bytes
	SCC_NEWGRF_ROTATE_TOP_4_WORDS,                    ///< Rotate the top 4 words of the stack (W4 W1 W2 W3)
	SCC_NEWGRF_LAST = SCC_NEWGRF_ROTATE_TOP_4_WORDS,

	SCC_NEWGRF_STRINL,                                ///< Inline another string at the current position, StringID is encoded in the string

	/* Special printable symbols.
	 * These are mapped to the original glyphs */
	SCC_LESS_THAN        = SCC_SPRITE_START + 0x3C,
	SCC_GREATER_THAN     = SCC_SPRITE_START + 0x3E,
	SCC_UP_ARROW         = SCC_SPRITE_START + 0xA0,
	SCC_DOWN_ARROW       = SCC_SPRITE_START + 0xAA,
	SCC_CHECKMARK        = SCC_SPRITE_START + 0xAC,
	SCC_CROSS            = SCC_SPRITE_START + 0xAD,
	SCC_RIGHT_ARROW      = SCC_SPRITE_START + 0xAF,
	SCC_TRAIN            = SCC_SPRITE_START + 0xB4,
	SCC_LORRY            = SCC_SPRITE_START + 0xB5,
	SCC_BUS              = SCC_SPRITE_START + 0xB6,
	SCC_PLANE            = SCC_SPRITE_START + 0xB7,
	SCC_SHIP             = SCC_SPRITE_START + 0xB8,
	SCC_SUPERSCRIPT_M1   = SCC_SPRITE_START + 0xB9,
	SCC_SMALL_UP_ARROW   = SCC_SPRITE_START + 0xBC,
	SCC_SMALL_DOWN_ARROW = SCC_SPRITE_START + 0xBD,
};

#endif /* CONTROL_CODES_H */
