/* $Id$ */

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

typedef void (*ParseCmdProc)(char *buf, int value);

struct CmdStruct {
	const char *cmd;
	ParseCmdProc proc;
	long value;
	uint8 consumes;
	CmdFlags flags;
};

static void EmitSingleChar(char *buf, int value);
static void EmitPlural(char *buf, int value);
static void EmitGender(char *buf, int value);

static const CmdStruct _cmd_structs[] = {
	/* Font size */
	{"TINYFONT",        EmitSingleChar, SCC_TINYFONT,           0, C_NONE},
	{"BIGFONT",         EmitSingleChar, SCC_BIGFONT,            0, C_NONE},

	/* Colors */
	{"BLUE",            EmitSingleChar, SCC_BLUE,               0, C_NONE},
	{"SILVER",          EmitSingleChar, SCC_SILVER,             0, C_NONE},
	{"GOLD",            EmitSingleChar, SCC_GOLD,               0, C_NONE},
	{"RED",             EmitSingleChar, SCC_RED,                0, C_NONE},
	{"PURPLE",          EmitSingleChar, SCC_PURPLE,             0, C_NONE},
	{"LTBROWN",         EmitSingleChar, SCC_LTBROWN,            0, C_NONE},
	{"ORANGE",          EmitSingleChar, SCC_ORANGE,             0, C_NONE},
	{"GREEN",           EmitSingleChar, SCC_GREEN,              0, C_NONE},
	{"YELLOW",          EmitSingleChar, SCC_YELLOW,             0, C_NONE},
	{"DKGREEN",         EmitSingleChar, SCC_DKGREEN,            0, C_NONE},
	{"CREAM",           EmitSingleChar, SCC_CREAM,              0, C_NONE},
	{"BROWN",           EmitSingleChar, SCC_BROWN,              0, C_NONE},
	{"WHITE",           EmitSingleChar, SCC_WHITE,              0, C_NONE},
	{"LTBLUE",          EmitSingleChar, SCC_LTBLUE,             0, C_NONE},
	{"GRAY",            EmitSingleChar, SCC_GRAY,               0, C_NONE},
	{"DKBLUE",          EmitSingleChar, SCC_DKBLUE,             0, C_NONE},
	{"BLACK",           EmitSingleChar, SCC_BLACK,              0, C_NONE},

	{"CURRCOMPACT",     EmitSingleChar, SCC_CURRENCY_COMPACT,   1, C_NONE}, // compact currency
	{"REV",             EmitSingleChar, SCC_REVISION,           0, C_NONE}, // openttd revision string
	{"SHORTCARGO",      EmitSingleChar, SCC_CARGO_SHORT,        2, C_NONE}, // short cargo description, only ### tons, or ### litres

	{"STRING1",         EmitSingleChar, SCC_STRING1,            2, C_CASE | C_GENDER}, // included string that consumes the string id and ONE argument
	{"STRING2",         EmitSingleChar, SCC_STRING2,            3, C_CASE | C_GENDER}, // included string that consumes the string id and TWO arguments
	{"STRING3",         EmitSingleChar, SCC_STRING3,            4, C_CASE | C_GENDER}, // included string that consumes the string id and THREE arguments
	{"STRING4",         EmitSingleChar, SCC_STRING4,            5, C_CASE | C_GENDER}, // included string that consumes the string id and FOUR arguments
	{"STRING5",         EmitSingleChar, SCC_STRING5,            6, C_CASE | C_GENDER}, // included string that consumes the string id and FIVE arguments

	{"STATIONFEATURES", EmitSingleChar, SCC_STATION_FEATURES,   1, C_NONE}, // station features string, icons of the features
	{"INDUSTRY",        EmitSingleChar, SCC_INDUSTRY_NAME,      1, C_CASE | C_GENDER}, // industry, takes an industry #, can have cases
	{"CARGO",           EmitSingleChar, SCC_CARGO,              2, C_NONE | C_GENDER},
	{"POWER",           EmitSingleChar, SCC_POWER,              1, C_NONE},
	{"VOLUME",          EmitSingleChar, SCC_VOLUME,             1, C_NONE},
	{"VOLUME_S",        EmitSingleChar, SCC_VOLUME_SHORT,       1, C_NONE},
	{"WEIGHT",          EmitSingleChar, SCC_WEIGHT,             1, C_NONE},
	{"WEIGHT_S",        EmitSingleChar, SCC_WEIGHT_SHORT,       1, C_NONE},
	{"FORCE",           EmitSingleChar, SCC_FORCE,              1, C_NONE},
	{"VELOCITY",        EmitSingleChar, SCC_VELOCITY,           1, C_NONE},
	{"HEIGHT",          EmitSingleChar, SCC_HEIGHT,             1, C_NONE},

	{"P",               EmitPlural,     0,                      0, C_DONTCOUNT}, // plural specifier
	{"G",               EmitGender,     0,                      0, C_DONTCOUNT}, // gender specifier

	{"DATE_TINY",       EmitSingleChar, SCC_DATE_TINY,          1, C_NONE},
	{"DATE_SHORT",      EmitSingleChar, SCC_DATE_SHORT,         1, C_CASE},
	{"DATE_LONG",       EmitSingleChar, SCC_DATE_LONG,          1, C_CASE},
	{"DATE_ISO",        EmitSingleChar, SCC_DATE_ISO,           1, C_NONE},

	{"STRING",          EmitSingleChar, SCC_STRING,             1, C_CASE | C_GENDER},
	{"RAW_STRING",      EmitSingleChar, SCC_RAW_STRING_POINTER, 1, C_NONE | C_GENDER},

	/* Numbers */
	{"COMMA",           EmitSingleChar, SCC_COMMA,              1, C_NONE}, // Number with comma
	{"NUM",             EmitSingleChar, SCC_NUM,                1, C_NONE}, // Signed number
	{"ZEROFILL_NUM",    EmitSingleChar, SCC_ZEROFILL_NUM,       2, C_NONE}, // Unsigned number with zero fill, e.g. "02". First parameter is number, second minimum length
	{"BYTES",           EmitSingleChar, SCC_BYTES,              1, C_NONE}, // Unsigned number with "bytes", i.e. "1.02 MiB or 123 KiB"
	{"HEX",             EmitSingleChar, SCC_HEX,                1, C_NONE}, // Hexadecimally printed number

	{"CURRENCY",        EmitSingleChar, SCC_CURRENCY,           1, C_NONE},

	{"WAYPOINT",        EmitSingleChar, SCC_WAYPOINT_NAME,      1, C_NONE | C_GENDER}, // waypoint name
	{"STATION",         EmitSingleChar, SCC_STATION_NAME,       1, C_NONE | C_GENDER},
	{"DEPOT",           EmitSingleChar, SCC_DEPOT_NAME,         2, C_NONE | C_GENDER},
	{"TOWN",            EmitSingleChar, SCC_TOWN_NAME,          1, C_NONE | C_GENDER},
	{"GROUP",           EmitSingleChar, SCC_GROUP_NAME,         1, C_NONE | C_GENDER},
	{"SIGN",            EmitSingleChar, SCC_SIGN_NAME,          1, C_NONE | C_GENDER},
	{"ENGINE",          EmitSingleChar, SCC_ENGINE_NAME,        1, C_NONE | C_GENDER},
	{"VEHICLE",         EmitSingleChar, SCC_VEHICLE_NAME,       1, C_NONE | C_GENDER},
	{"COMPANY",         EmitSingleChar, SCC_COMPANY_NAME,       1, C_NONE | C_GENDER},
	{"COMPANYNUM",      EmitSingleChar, SCC_COMPANY_NUM,        1, C_NONE},
	{"PRESIDENTNAME",   EmitSingleChar, SCC_PRESIDENT_NAME,     1, C_NONE | C_GENDER},

	{"",                EmitSingleChar, '\n',                   0, C_DONTCOUNT},
	{"{",               EmitSingleChar, '{',                    0, C_DONTCOUNT},
	{"UPARROW",         EmitSingleChar, SCC_UPARROW,            0, C_DONTCOUNT},
	{"SMALLUPARROW",    EmitSingleChar, SCC_SMALLUPARROW,       0, C_DONTCOUNT},
	{"SMALLDOWNARROW",  EmitSingleChar, SCC_SMALLDOWNARROW,     0, C_DONTCOUNT},
	{"TRAIN",           EmitSingleChar, SCC_TRAIN,              0, C_DONTCOUNT},
	{"LORRY",           EmitSingleChar, SCC_LORRY,              0, C_DONTCOUNT},
	{"BUS",             EmitSingleChar, SCC_BUS,                0, C_DONTCOUNT},
	{"PLANE",           EmitSingleChar, SCC_PLANE,              0, C_DONTCOUNT},
	{"SHIP",            EmitSingleChar, SCC_SHIP,               0, C_DONTCOUNT},
	{"NBSP",            EmitSingleChar, 0xA0,                   0, C_DONTCOUNT},
	{"CENT",            EmitSingleChar, 0xA2,                   0, C_DONTCOUNT},
	{"POUNDSIGN",       EmitSingleChar, 0xA3,                   0, C_DONTCOUNT},
	{"EURO",            EmitSingleChar, 0x20AC,                 0, C_DONTCOUNT},
	{"YENSIGN",         EmitSingleChar, 0xA5,                   0, C_DONTCOUNT},
	{"COPYRIGHT",       EmitSingleChar, 0xA9,                   0, C_DONTCOUNT},
	{"DOWNARROW",       EmitSingleChar, SCC_DOWNARROW,          0, C_DONTCOUNT},
	{"CHECKMARK",       EmitSingleChar, SCC_CHECKMARK,          0, C_DONTCOUNT},
	{"CROSS",           EmitSingleChar, SCC_CROSS,              0, C_DONTCOUNT},
	{"REGISTERED",      EmitSingleChar, 0xAE,                   0, C_DONTCOUNT},
	{"RIGHTARROW",      EmitSingleChar, SCC_RIGHTARROW,         0, C_DONTCOUNT},
	{"SMALLLEFTARROW",  EmitSingleChar, SCC_LESSTHAN,           0, C_DONTCOUNT},
	{"SMALLRIGHTARROW", EmitSingleChar, SCC_GREATERTHAN,        0, C_DONTCOUNT},

	/* The following are directional formatting codes used to get the RTL strings right:
	 * http://www.unicode.org/unicode/reports/tr9/#Directional_Formatting_Codes */
	{"LRM",             EmitSingleChar, CHAR_TD_LRM,            0, C_DONTCOUNT},
	{"RLM",             EmitSingleChar, CHAR_TD_RLM,            0, C_DONTCOUNT},
	{"LRE",             EmitSingleChar, CHAR_TD_LRE,            0, C_DONTCOUNT},
	{"RLE",             EmitSingleChar, CHAR_TD_RLE,            0, C_DONTCOUNT},
	{"LRO",             EmitSingleChar, CHAR_TD_LRO,            0, C_DONTCOUNT},
	{"RLO",             EmitSingleChar, CHAR_TD_RLO,            0, C_DONTCOUNT},
	{"PDF",             EmitSingleChar, CHAR_TD_PDF,            0, C_DONTCOUNT},
};

/** Description of a plural form */
struct PluralForm {
	int plural_count;        ///< The number of plural forms
	const char *description; ///< Human readable description of the form
};

/** All plural forms used */
static const PluralForm _plural_forms[] = {
	{ 2, "Two forms, singular used for 1 only" },
	{ 1, "Only one form" },
	{ 2, "Two forms, singular used for zero and 1" },
	{ 3, "Three forms, special case for 0 and ending in 1, except those ending in 11" },
	{ 5, "Five forms, special case for one, two, 3 to 6 and 7 to 10" },
	{ 3, "Three forms, special case for numbers ending in 1[2-9]" },
	{ 3, "Three forms, special cases for numbers ending in 1 and 2, 3, 4, except those ending in 1[1-4]" },
	{ 3, "Three forms, special case for 1 and some numbers ending in 2, 3, or 4" },
	{ 4, "Four forms, special case for 1 and all numbers ending in 02, 03, or 04" },
	{ 2, "Two forms, singular used for everything ending in 1 but not in 11" },
	{ 3, "Three forms, special case for 1 and 2, 3, or 4" },
	{ 2, "Two forms, cases for numbers ending with a consonant and with a vowel" },
	{ 4, "Four forms: one, 0 and everything ending in 02..10, everything ending in 11..19" },
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
	{ "digitsepcur", "td",  ",",      "Digit grouping seprarator for currency numbers" },
	{ "decimalsep",  "td",  ".",      "Decimal separator" },
	{ "winlangid",   "x2",  "0x0000", "Language ID for Windows" },
	{ "grflangid",   "x1",  "0x00",   "Language ID for NewGRFs" },
	{ "gender",      "tag", "",       "List of genders" },
	{ "case",        "tac", "",       "List of cases" },
};
