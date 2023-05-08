/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file table/strgen_tables.h Tables of commands for strgen */

#include "../core/enum_type.hpp"

enum CmdFlags {
	C_NONE      = 0x0, ///< Nothing special about this command
	C_DONTCOUNT = 0x1, ///< These commands aren't counted for comparison
	C_CASE      = 0x2, ///< These commands support cases
	C_GENDER    = 0x4, ///< These commands support genders
};
DECLARE_ENUM_AS_BIT_SET(CmdFlags)

struct Buffer;
typedef void (*ParseCmdProc)(Buffer *buffer, char *buf, int value);

struct CmdStruct {
	const char *cmd;
	ParseCmdProc proc;
	long value;
	uint8_t consumes;
	int8_t default_plural_offset;
	CmdFlags flags;
};

extern void EmitSingleChar(Buffer *buffer, char *buf, int value);
extern void EmitPlural(Buffer *buffer, char *buf, int value);
extern void EmitGender(Buffer *buffer, char *buf, int value);

static const CmdStruct _cmd_structs[] = {
	/* Font size */
	{"NORMAL_FONT",       EmitSingleChar, SCC_NORMALFONT,         0, -1, C_NONE},
	{"TINY_FONT",         EmitSingleChar, SCC_TINYFONT,           0, -1, C_NONE},
	{"BIG_FONT",          EmitSingleChar, SCC_BIGFONT,            0, -1, C_NONE},
	{"MONO_FONT",         EmitSingleChar, SCC_MONOFONT,           0, -1, C_NONE},

	/* Colours */
	{"BLUE",              EmitSingleChar, SCC_BLUE,               0, -1, C_DONTCOUNT},
	{"SILVER",            EmitSingleChar, SCC_SILVER,             0, -1, C_DONTCOUNT},
	{"GOLD",              EmitSingleChar, SCC_GOLD,               0, -1, C_DONTCOUNT},
	{"RED",               EmitSingleChar, SCC_RED,                0, -1, C_DONTCOUNT},
	{"PURPLE",            EmitSingleChar, SCC_PURPLE,             0, -1, C_DONTCOUNT},
	{"LTBROWN",           EmitSingleChar, SCC_LTBROWN,            0, -1, C_DONTCOUNT},
	{"ORANGE",            EmitSingleChar, SCC_ORANGE,             0, -1, C_DONTCOUNT},
	{"GREEN",             EmitSingleChar, SCC_GREEN,              0, -1, C_DONTCOUNT},
	{"YELLOW",            EmitSingleChar, SCC_YELLOW,             0, -1, C_DONTCOUNT},
	{"DKGREEN",           EmitSingleChar, SCC_DKGREEN,            0, -1, C_DONTCOUNT},
	{"CREAM",             EmitSingleChar, SCC_CREAM,              0, -1, C_DONTCOUNT},
	{"BROWN",             EmitSingleChar, SCC_BROWN,              0, -1, C_DONTCOUNT},
	{"WHITE",             EmitSingleChar, SCC_WHITE,              0, -1, C_DONTCOUNT},
	{"LTBLUE",            EmitSingleChar, SCC_LTBLUE,             0, -1, C_DONTCOUNT},
	{"GRAY",              EmitSingleChar, SCC_GRAY,               0, -1, C_DONTCOUNT},
	{"DKBLUE",            EmitSingleChar, SCC_DKBLUE,             0, -1, C_DONTCOUNT},
	{"BLACK",             EmitSingleChar, SCC_BLACK,              0, -1, C_DONTCOUNT},
	{"COLOUR",            EmitSingleChar, SCC_COLOUR,             1, -1, C_NONE},
	{"PUSH_COLOUR",       EmitSingleChar, SCC_PUSH_COLOUR,        0, -1, C_DONTCOUNT},
	{"POP_COLOUR",        EmitSingleChar, SCC_POP_COLOUR,         0, -1, C_DONTCOUNT},

	{"REV",               EmitSingleChar, SCC_REVISION,           0, -1, C_NONE}, // openttd revision string

	{"STRING1",           EmitSingleChar, SCC_STRING1,            2, -1, C_CASE | C_GENDER}, // included string that consumes the string id and ONE argument
	{"STRING2",           EmitSingleChar, SCC_STRING2,            3, -1, C_CASE | C_GENDER}, // included string that consumes the string id and TWO arguments
	{"STRING3",           EmitSingleChar, SCC_STRING3,            4, -1, C_CASE | C_GENDER}, // included string that consumes the string id and THREE arguments
	{"STRING4",           EmitSingleChar, SCC_STRING4,            5, -1, C_CASE | C_GENDER}, // included string that consumes the string id and FOUR arguments
	{"STRING5",           EmitSingleChar, SCC_STRING5,            6, -1, C_CASE | C_GENDER}, // included string that consumes the string id and FIVE arguments
	{"STRING6",           EmitSingleChar, SCC_STRING6,            7, -1, C_CASE | C_GENDER}, // included string that consumes the string id and SIX arguments
	{"STRING7",           EmitSingleChar, SCC_STRING7,            8, -1, C_CASE | C_GENDER}, // included string that consumes the string id and SEVEN arguments

	{"STATION_FEATURES",  EmitSingleChar, SCC_STATION_FEATURES,   1, -1, C_NONE}, // station features string, icons of the features
	{"INDUSTRY",          EmitSingleChar, SCC_INDUSTRY_NAME,      1, -1, C_CASE | C_GENDER}, // industry, takes an industry #, can have cases
	{"CARGO_LONG",        EmitSingleChar, SCC_CARGO_LONG,         2,  1, C_NONE | C_GENDER},
	{"CARGO_SHORT",       EmitSingleChar, SCC_CARGO_SHORT,        2,  1, C_NONE | C_GENDER}, // short cargo description, only ### tons, or ### litres
	{"CARGO_TINY",        EmitSingleChar, SCC_CARGO_TINY,         2,  1, C_NONE}, // tiny cargo description with only the amount, not a specifier for the amount or the actual cargo name
	{"CARGO_LIST",        EmitSingleChar, SCC_CARGO_LIST,         1, -1, C_CASE},
	{"POWER",             EmitSingleChar, SCC_POWER,              1,  0, C_NONE},
	{"POWER_TO_WEIGHT",   EmitSingleChar, SCC_POWER_TO_WEIGHT,    1,  0, C_NONE},
	{"VOLUME_LONG",       EmitSingleChar, SCC_VOLUME_LONG,        1,  0, C_NONE},
	{"VOLUME_SHORT",      EmitSingleChar, SCC_VOLUME_SHORT,       1,  0, C_NONE},
	{"WEIGHT_LONG",       EmitSingleChar, SCC_WEIGHT_LONG,        1,  0, C_NONE},
	{"WEIGHT_SHORT",      EmitSingleChar, SCC_WEIGHT_SHORT,       1,  0, C_NONE},
	{"FORCE",             EmitSingleChar, SCC_FORCE,              1,  0, C_NONE},
	{"VELOCITY",          EmitSingleChar, SCC_VELOCITY,           1,  0, C_NONE},
	{"HEIGHT",            EmitSingleChar, SCC_HEIGHT,             1,  0, C_NONE},

	{"P",                 EmitPlural,     0,                      0, -1, C_DONTCOUNT}, // plural specifier
	{"G",                 EmitGender,     0,                      0, -1, C_DONTCOUNT}, // gender specifier

	{"DATE_TINY",         EmitSingleChar, SCC_DATE_TINY,          1, -1, C_NONE},
	{"DATE_SHORT",        EmitSingleChar, SCC_DATE_SHORT,         1, -1, C_CASE},
	{"DATE_LONG",         EmitSingleChar, SCC_DATE_LONG,          1, -1, C_CASE},
	{"DATE_ISO",          EmitSingleChar, SCC_DATE_ISO,           1, -1, C_NONE},

	{"STRING",            EmitSingleChar, SCC_STRING,             1, -1, C_CASE | C_GENDER},
	{"RAW_STRING",        EmitSingleChar, SCC_RAW_STRING_POINTER, 1, -1, C_NONE | C_GENDER},

	/* Numbers */
	{"COMMA",             EmitSingleChar, SCC_COMMA,              1,  0, C_NONE}, // Number with comma
	{"DECIMAL",           EmitSingleChar, SCC_DECIMAL,            2,  0, C_NONE}, // Number with comma and fractional part. Second parameter is number of fractional digits, first parameter is number times 10**(second parameter).
	{"NUM",               EmitSingleChar, SCC_NUM,                1,  0, C_NONE}, // Signed number
	{"ZEROFILL_NUM",      EmitSingleChar, SCC_ZEROFILL_NUM,       2,  0, C_NONE}, // Unsigned number with zero fill, e.g. "02". First parameter is number, second minimum length
	{"BYTES",             EmitSingleChar, SCC_BYTES,              1,  0, C_NONE}, // Unsigned number with "bytes", i.e. "1.02 MiB or 123 KiB"
	{"HEX",               EmitSingleChar, SCC_HEX,                1,  0, C_NONE}, // Hexadecimally printed number

	{"CURRENCY_LONG",     EmitSingleChar, SCC_CURRENCY_LONG,      1,  0, C_NONE},
	{"CURRENCY_SHORT",    EmitSingleChar, SCC_CURRENCY_SHORT,     1,  0, C_NONE}, // compact currency

	{"WAYPOINT",          EmitSingleChar, SCC_WAYPOINT_NAME,      1, -1, C_NONE | C_GENDER}, // waypoint name
	{"STATION",           EmitSingleChar, SCC_STATION_NAME,       1, -1, C_NONE | C_GENDER},
	{"DEPOT",             EmitSingleChar, SCC_DEPOT_NAME,         2, -1, C_NONE | C_GENDER},
	{"TOWN",              EmitSingleChar, SCC_TOWN_NAME,          1, -1, C_NONE | C_GENDER},
	{"GROUP",             EmitSingleChar, SCC_GROUP_NAME,         1, -1, C_NONE | C_GENDER},
	{"SIGN",              EmitSingleChar, SCC_SIGN_NAME,          1, -1, C_NONE | C_GENDER},
	{"ENGINE",            EmitSingleChar, SCC_ENGINE_NAME,        1, -1, C_NONE | C_GENDER},
	{"VEHICLE",           EmitSingleChar, SCC_VEHICLE_NAME,       1, -1, C_NONE | C_GENDER},
	{"COMPANY",           EmitSingleChar, SCC_COMPANY_NAME,       1, -1, C_NONE | C_GENDER},
	{"COMPANY_NUM",       EmitSingleChar, SCC_COMPANY_NUM,        1, -1, C_NONE},
	{"PRESIDENT_NAME",    EmitSingleChar, SCC_PRESIDENT_NAME,     1, -1, C_NONE | C_GENDER},

	{"",                  EmitSingleChar, '\n',                   0, -1, C_DONTCOUNT},
	{"{",                 EmitSingleChar, '{',                    0, -1, C_DONTCOUNT},
	{"UP_ARROW",          EmitSingleChar, SCC_UP_ARROW,           0, -1, C_DONTCOUNT},
	{"SMALL_UP_ARROW",    EmitSingleChar, SCC_SMALL_UP_ARROW,     0, -1, C_DONTCOUNT},
	{"SMALL_DOWN_ARROW",  EmitSingleChar, SCC_SMALL_DOWN_ARROW,   0, -1, C_DONTCOUNT},
	{"TRAIN",             EmitSingleChar, SCC_TRAIN,              0, -1, C_DONTCOUNT},
	{"LORRY",             EmitSingleChar, SCC_LORRY,              0, -1, C_DONTCOUNT},
	{"BUS",               EmitSingleChar, SCC_BUS,                0, -1, C_DONTCOUNT},
	{"PLANE",             EmitSingleChar, SCC_PLANE,              0, -1, C_DONTCOUNT},
	{"SHIP",              EmitSingleChar, SCC_SHIP,               0, -1, C_DONTCOUNT},
	{"NBSP",              EmitSingleChar, 0xA0,                   0, -1, C_DONTCOUNT},
	{"COPYRIGHT",         EmitSingleChar, 0xA9,                   0, -1, C_DONTCOUNT},
	{"DOWN_ARROW",        EmitSingleChar, SCC_DOWN_ARROW,         0, -1, C_DONTCOUNT},
	{"CHECKMARK",         EmitSingleChar, SCC_CHECKMARK,          0, -1, C_DONTCOUNT},
	{"CROSS",             EmitSingleChar, SCC_CROSS,              0, -1, C_DONTCOUNT},
	{"RIGHT_ARROW",       EmitSingleChar, SCC_RIGHT_ARROW,        0, -1, C_DONTCOUNT},
	{"SMALL_LEFT_ARROW",  EmitSingleChar, SCC_LESS_THAN,          0, -1, C_DONTCOUNT},
	{"SMALL_RIGHT_ARROW", EmitSingleChar, SCC_GREATER_THAN,       0, -1, C_DONTCOUNT},

	/* The following are directional formatting codes used to get the RTL strings right:
	 * http://www.unicode.org/unicode/reports/tr9/#Directional_Formatting_Codes */
	{"LRM",               EmitSingleChar, CHAR_TD_LRM,            0, -1, C_DONTCOUNT},
	{"RLM",               EmitSingleChar, CHAR_TD_RLM,            0, -1, C_DONTCOUNT},
	{"LRE",               EmitSingleChar, CHAR_TD_LRE,            0, -1, C_DONTCOUNT},
	{"RLE",               EmitSingleChar, CHAR_TD_RLE,            0, -1, C_DONTCOUNT},
	{"LRO",               EmitSingleChar, CHAR_TD_LRO,            0, -1, C_DONTCOUNT},
	{"RLO",               EmitSingleChar, CHAR_TD_RLO,            0, -1, C_DONTCOUNT},
	{"PDF",               EmitSingleChar, CHAR_TD_PDF,            0, -1, C_DONTCOUNT},
};

/** Description of a plural form */
struct PluralForm {
	int plural_count;        ///< The number of plural forms
	const char *description; ///< Human readable description of the form
	const char *names;       ///< Plural names
};

/** The maximum number of plurals. */
static const int MAX_PLURALS = 5;

/** All plural forms used */
static const PluralForm _plural_forms[] = {
	{ 2, "Two forms: special case for 1.", "\"1\" \"other\"" },
	{ 1, "Only one form.", "\"other\"" },
	{ 2, "Two forms: special case for 0 to 1.", "\"0..1\" \"other\"" },
	{ 3, "Three forms: special cases for 0, and numbers ending in 1 except when ending in 11.", "\"1,21,31,...\" \"other\" \"0\"" },
	{ 5, "Five forms: special cases for 1, 2, 3 to 6, and 7 to 10.", "\"1\" \"2\" \"3..6\" \"7..10\" \"other\"" },
	{ 3, "Three forms: special cases for numbers ending in 1 except when ending in 11, and 2 to 9 except when ending in 12 to 19.", "\"1,21,31,...\" \"2..9,22..29,32..39,...\" \"other\"" },
	{ 3, "Three forms: special cases for numbers ending in 1 except when ending in 11, and 2 to 4 except when ending in 12 to 14.", "\"1,21,31,...\" \"2..4,22..24,32..34,...\" \"other\"" },
	{ 3, "Three forms: special cases for 1, and numbers ending in 2 to 4 except when ending in 12 to 14.", "\"1\" \"2..4,22..24,32..34,...\" \"other\"" },
	{ 4, "Four forms: special cases for numbers ending in 01, 02, and 03 to 04.", "\"1,101,201,...\" \"2,102,202,...\" \"3..4,103..104,203..204,...\" \"other\"" },
	{ 2, "Two forms: special case for numbers ending in 1 except when ending in 11.", "\"1,21,31,...\" \"other\"" },
	{ 3, "Three forms: special cases for 1, and 2 to 4.", "\"1\" \"2..4\" \"other\"" },
	{ 2, "Two forms: cases for numbers ending with a consonant, and with a vowel.", "\"yeong,il,sam,yuk,chil,pal\" \"i,sa,o,gu\"" },
	{ 4, "Four forms: special cases for 1, 0 and numbers ending in 02 to 10, and numbers ending in 11 to 19.", "\"1\" \"0,2..10,102..110,202..210,...\" \"11..19,111..119,211..219,...\" \"other\"" },
	{ 4, "Four forms: special cases for 1 and 11, 2 and 12, 3..10 and 13..19.", "\"1,11\" \"2,12\" \"3..10,13..19\" \"other\"" },
	{ 3, "Three forms: special cases for 1, 0 and numbers ending in 01 to 19.", "\"1\" \"0,2..19,101..119,201..219,...\" \"other\"" },
};

/* Flags:
 * 0 = nothing
 * t = translator editable
 * l = ltr/rtl choice
 * p = plural choice
 * d = separator char (replace spaces with {NBSP})
 * x1 = hexadecimal number of 1 byte
 * x2 = hexadecimal number of 2 bytes
 * g = gender
 * c = cases
 * a = array, i.e. list of strings
 */
 /** All pragmas used */
static const char * const _pragmas[][4] = {
	/*  name         flags  default   description */
	{ "name",        "0",   "",       "English name for the language" },
	{ "ownname",     "t",   "",       "Localised name for the language" },
	{ "isocode",     "0",   "",       "ISO code for the language" },
	{ "plural",      "tp",  "0",      "Plural form to use" },
	{ "textdir",     "tl",  "ltr",    "Text direction. Either ltr (left-to-right) or rtl (right-to-left)" },
	{ "digitsep",    "td",  ",",      "Digit grouping separator for non-currency numbers" },
	{ "digitsepcur", "td",  ",",      "Digit grouping separator for currency numbers" },
	{ "decimalsep",  "td",  ".",      "Decimal separator" },
	{ "winlangid",   "x2",  "0x0000", "Language ID for Windows" },
	{ "grflangid",   "x1",  "0x00",   "Language ID for NewGRFs" },
	{ "gender",      "tag", "",       "List of genders" },
	{ "case",        "tac", "",       "List of cases" },
};
