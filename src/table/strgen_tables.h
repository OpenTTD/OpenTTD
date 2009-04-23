/* $Id$ */

/** @file table/strgen_tables.h Tables of commands for strgen */

#include "../core/enum_type.hpp"

enum CmdFlags {
	C_NONE      = 0x0,
	C_DONTCOUNT = 0x1,
	C_CASE      = 0x2,
};
DECLARE_ENUM_AS_BIT_SET(CmdFlags);

typedef void (*ParseCmdProc)(char *buf, int value);

struct CmdStruct {
	const char *cmd;
	ParseCmdProc proc;
	long value;
	uint8 consumes;
	CmdFlags flags;
};

static void EmitSetX(char *buf, int value);
static void EmitSetXY(char *buf, int value);
static void EmitSingleChar(char *buf, int value);
static void EmitPlural(char *buf, int value);
static void EmitGender(char *buf, int value);

static const CmdStruct _cmd_structs[] = {
	/* Update position */
	{"SETX",            EmitSetX,       SCC_SETX,               0, C_NONE},
	{"SETXY",           EmitSetXY,      SCC_SETXY,              0, C_NONE},

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

	{"STRING1",         EmitSingleChar, SCC_STRING1,            2, C_CASE}, // included string that consumes the string id and ONE argument
	{"STRING2",         EmitSingleChar, SCC_STRING2,            3, C_CASE}, // included string that consumes the string id and TWO arguments
	{"STRING3",         EmitSingleChar, SCC_STRING3,            4, C_CASE}, // included string that consumes the string id and THREE arguments
	{"STRING4",         EmitSingleChar, SCC_STRING4,            5, C_CASE}, // included string that consumes the string id and FOUR arguments
	{"STRING5",         EmitSingleChar, SCC_STRING5,            6, C_CASE}, // included string that consumes the string id and FIVE arguments

	{"STATIONFEATURES", EmitSingleChar, SCC_STATION_FEATURES,   1, C_NONE}, // station features string, icons of the features
	{"INDUSTRY",        EmitSingleChar, SCC_INDUSTRY_NAME,      1, C_NONE}, // industry, takes an industry #
	{"CARGO",           EmitSingleChar, SCC_CARGO,              2, C_NONE},
	{"POWER",           EmitSingleChar, SCC_POWER,              1, C_NONE},
	{"VOLUME",          EmitSingleChar, SCC_VOLUME,             1, C_NONE},
	{"VOLUME_S",        EmitSingleChar, SCC_VOLUME_SHORT,       1, C_NONE},
	{"WEIGHT",          EmitSingleChar, SCC_WEIGHT,             1, C_NONE},
	{"WEIGHT_S",        EmitSingleChar, SCC_WEIGHT_SHORT,       1, C_NONE},
	{"FORCE",           EmitSingleChar, SCC_FORCE,              1, C_NONE},
	{"VELOCITY",        EmitSingleChar, SCC_VELOCITY,           1, C_NONE},

	{"P",               EmitPlural,     0,                      0, C_DONTCOUNT}, // plural specifier
	{"G",               EmitGender,     0,                      0, C_DONTCOUNT}, // gender specifier

	{"DATE_TINY",       EmitSingleChar, SCC_DATE_TINY,          1, C_NONE},
	{"DATE_SHORT",      EmitSingleChar, SCC_DATE_SHORT,         1, C_NONE},
	{"DATE_LONG",       EmitSingleChar, SCC_DATE_LONG,          1, C_NONE},
	{"DATE_ISO",        EmitSingleChar, SCC_DATE_ISO,           1, C_NONE},

	{"SKIP",            EmitSingleChar, SCC_SKIP,               1, C_NONE},

	{"STRING",          EmitSingleChar, SCC_STRING,             1, C_CASE},
	{"RAW_STRING",      EmitSingleChar, SCC_RAW_STRING_POINTER, 1, C_NONE},

	/* Numbers */
	{"COMMA",           EmitSingleChar, SCC_COMMA,              1, C_NONE}, // Number with comma
	{"NUM",             EmitSingleChar, SCC_NUM,                1, C_NONE}, // Signed number
	{"BYTES",           EmitSingleChar, SCC_BYTES,              1, C_NONE}, // Unsigned number with "bytes", i.e. "1.02 MiB or 123 KiB"

	{"CURRENCY",        EmitSingleChar, SCC_CURRENCY,           1, C_NONE},

	{"WAYPOINT",        EmitSingleChar, SCC_WAYPOINT_NAME,      1, C_NONE}, // waypoint name
	{"STATION",         EmitSingleChar, SCC_STATION_NAME,       1, C_NONE},
	{"TOWN",            EmitSingleChar, SCC_TOWN_NAME,          1, C_NONE},
	{"GROUP",           EmitSingleChar, SCC_GROUP_NAME,         1, C_NONE},
	{"SIGN",            EmitSingleChar, SCC_SIGN_NAME,          1, C_NONE},
	{"ENGINE",          EmitSingleChar, SCC_ENGINE_NAME,        1, C_NONE},
	{"VEHICLE",         EmitSingleChar, SCC_VEHICLE_NAME,       1, C_NONE},
	{"COMPANY",         EmitSingleChar, SCC_COMPANY_NAME,       1, C_NONE},
	{"COMPANYNUM",      EmitSingleChar, SCC_COMPANY_NUM,        1, C_NONE},
	{"PRESIDENTNAME",   EmitSingleChar, SCC_PRESIDENT_NAME,     1, C_NONE},

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
	{"LRM",             EmitSingleChar, 0x200E,                 0, C_DONTCOUNT},
	{"RLM",             EmitSingleChar, 0x200F,                 0, C_DONTCOUNT},
	{"LRE",             EmitSingleChar, 0x202A,                 0, C_DONTCOUNT},
	{"RLE",             EmitSingleChar, 0x202B,                 0, C_DONTCOUNT},
	{"LRO",             EmitSingleChar, 0x202D,                 0, C_DONTCOUNT},
	{"RLO",             EmitSingleChar, 0x202E,                 0, C_DONTCOUNT},
	{"PDF",             EmitSingleChar, 0x202C,                 0, C_DONTCOUNT},
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
	{ 3, "Three forms, special case for 1 and 2" },
	{ 3, "Three forms, special case for numbers ending in 1[2-9]" },
	{ 3, "Three forms, special cases for numbers ending in 1 and 2, 3, 4, except those ending in 1[1-4]" },
	{ 3, "Three forms, special case for 1 and some numbers ending in 2, 3, or 4" },
	{ 4, "Four forms, special case for 1 and all numbers ending in 02, 03, or 04" },
	{ 2, "Two forms, singular used for everything ending in 1 but not in 11" },
	{ 3, "Three forms, special case for 1 and 2, 3, or 4" },
};
