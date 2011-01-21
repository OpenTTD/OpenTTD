/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings.cpp Handling of translated strings. */

#include "stdafx.h"
#include "currency.h"
#include "station_base.h"
#include "town.h"
#include "screenshot.h"
#include "waypoint_base.h"
#include "depot_base.h"
#include "industry.h"
#include "newgrf_text.h"
#include "fileio_func.h"
#include "group.h"
#include "signs_base.h"
#include "cargotype.h"
#include "fontcache.h"
#include "gui.h"
#include "strings_func.h"
#include "rev.h"
#include "core/endian_func.hpp"
#include "date_func.h"
#include "vehicle_base.h"
#include "engine_base.h"
#include "language.h"
#include "townname_func.h"
#include "string_func.h"
#include "company_base.h"
#include "smallmap_gui.h"
#include "window_func.h"
#include "debug.h"
#include <stack>

#include "table/strings.h"
#include "table/control_codes.h"

char _config_language_file[MAX_PATH];             ///< The file (name) stored in the configuration.
LanguageList _languages;                          ///< The actual list of language meta data.
const LanguageMetadata *_current_language = NULL; ///< The currently loaded language.

TextDirection _current_text_dir; ///< Text direction of the currently selected language.
uint64 _decode_parameters[20];   ///< Global array of string parameters. To access, use #SetDParam.
WChar _parameter_type[20];       ///< Type of parameters stored in #_decode_parameters

#ifdef WITH_ICU
Collator *_current_collator = NULL;               ///< Collator for the language currently in use.
#endif /* WITH_ICU */

static char *StationGetSpecialString(char *buff, int x, const char *last);
static char *GetSpecialTownNameString(char *buff, int ind, uint32 seed, const char *last);
static char *GetSpecialNameString(char *buff, int ind, int64 *argv, const int64 *argve, const char *last, WChar *argt = NULL);

static char *FormatString(char *buff, const char *str, int64 *argv, const int64 *argve, uint casei, const char *last, WChar *argt = NULL, bool dry_run = false);

struct LanguagePack : public LanguagePackHeader {
	char data[]; // list of strings
};

static char **_langpack_offs;
static LanguagePack *_langpack;
static uint _langtab_num[32];   ///< Offset into langpack offs
static uint _langtab_start[32]; ///< Offset into langpack offs
static bool _keep_gender_data = false;  ///< Should we retain the gender data in the current string?


/**
 * Read an int64 from the argv array. The argv and argt arrays are incremented
 * so the next time GetInt64 is called the next value is read.
 * @param argv  Pointer to a position in the argv array.
 * @param argve Pointer just past the end of the argv array.
 * @param argt  Pointer to a position in the argument type array, or pointer to NULL.
 * @param type  Type of the argument to get. Must be equal to **argt if *argt is
 *              not NULL and **argt != 0.
 * @return The value from the current position in the argv array.
 */
static inline int64 GetInt64(int64 **argv, const int64 *argve, WChar **argt, WChar type = 0)
{
	assert(*argv != NULL);
	assert(*argv < argve);
	if (*argt != NULL) {
		assert(**argt == 0 || **argt == type);
		**argt = type;
		(*argt)++;
	}
	return *(*argv)++;
}

/** Read an int32 from the argv array. @see GetInt64 */
static inline int32 GetInt32(int64 **argv, const int64 *argve, WChar **argt, WChar type = 0)
{
	return (int32)GetInt64(argv, argve, argt, type);
}

/**
 * Read an array from the argv array. This is done by increasing *argv and *argt
 * by n and returning the original values.
 * @param argv  Pointer to a position in the argv array.
 * @param n     Number of parameters to skip.
 * @param argve Pointer just past the end of the argv array.
 * @param argt  Pointer to a position in the argument type array, or pointer to NULL.
 */
static inline int64 *GetArgvPtr(int64 **argv, int n, const int64 *argve, WChar **argt)
{
	int64 *result;
	assert(*argv != NULL);
	assert((*argv + n) <= argve);
	result = *argv;
	(*argv) += n;
	if (*argt != NULL) (*argt) += n;
	return result;
}


const char *GetStringPtr(StringID string)
{
	switch (GB(string, 11, 5)) {
		/* GetGRFStringPtr doesn't handle 0xD4xx ids, we need to convert those to 0xD0xx. */
		case 26: return GetStringPtr(GetGRFStringID(0, 0xD000 + GB(string, 0, 10)));
		case 28: return GetGRFStringPtr(GB(string, 0, 11));
		case 29: return GetGRFStringPtr(GB(string, 0, 11) + 0x0800);
		case 30: return GetGRFStringPtr(GB(string, 0, 11) + 0x1000);
		default: return _langpack_offs[_langtab_start[string >> 11] + (string & 0x7FF)];
	}
}

/**
 * The highest 8 bits of string contain the "case index".
 * These 8 bits will only be set when FormatString wants to print
 * the string in a different case. No one else except FormatString
 * should set those bits, therefore string CANNOT be StringID, but uint32.
 * @param buffr  Pointer to a string buffer where the formatted string should be written to.
 * @param string
 * @param argv   Array with arguments for the string.
 * @param argve  Pointer just past the end of argv.
 * @param last   Pointer just past the end of buffr.
 * @param argt   Array to be filled with the type of parameters.
 * @return       Pointer to the final zero byte of the formatted string.
 */
char *GetStringWithArgs(char *buffr, uint string, int64 *argv, const int64 *argve, const char *last, WChar *argt)
{
	if (GB(string, 0, 16) == 0) return GetStringWithArgs(buffr, STR_UNDEFINED, argv, argve, last, argt);

	uint index = GB(string,  0, 11);
	uint tab   = GB(string, 11,  5);

	switch (tab) {
		case 4:
			if (index >= 0xC0) {
				return GetSpecialTownNameString(buffr, index - 0xC0, GetInt32(&argv, argve, &argt), last);
			}
			break;

		case 14:
			if (index >= 0xE4) {
				return GetSpecialNameString(buffr, index - 0xE4, argv, argve, last, argt);
			}
			break;

		case 15:
			/* Old table for custom names. This is no longer used */
			error("Incorrect conversion of custom name string.");

		case 26:
			/* Include string within newgrf text (format code 81) */
			if (HasBit(index, 10)) {
				StringID string = GetGRFStringID(0, 0xD000 + GB(index, 0, 10));
				return GetStringWithArgs(buffr, string, argv, argve, last, argt);
			}
			break;

		case 28:
			return FormatString(buffr, GetGRFStringPtr(index), argv, argve, GB(string, 24, 8), last, argt);

		case 29:
			return FormatString(buffr, GetGRFStringPtr(index + 0x0800), argv, argve, GB(string, 24, 8), last, argt);

		case 30:
			return FormatString(buffr, GetGRFStringPtr(index + 0x1000), argv, argve, GB(string, 24, 8), last, argt);

		case 31:
			NOT_REACHED();
	}

	if (index >= _langtab_num[tab]) {
		error("String 0x%X is invalid. You are probably using an old version of the .lng file.\n", string);
	}

	return FormatString(buffr, GetStringPtr(GB(string, 0, 16)), argv, argve, GB(string, 24, 8), last, argt);
}

char *GetString(char *buffr, StringID string, const char *last)
{
	memset(_parameter_type, 0, sizeof(_parameter_type));
	return GetStringWithArgs(buffr, string, (int64*)_decode_parameters, (int64*)endof(_decode_parameters), last, _parameter_type);
}


char *InlineString(char *buf, StringID string)
{
	buf += Utf8Encode(buf, SCC_STRING_ID);
	buf += Utf8Encode(buf, string);
	return buf;
}


/**
 * This function is used to "bind" a C string to a OpenTTD dparam slot.
 * @param n slot of the string
 * @param str string to bind
 */
void SetDParamStr(uint n, const char *str)
{
	SetDParam(n, (uint64)(size_t)str);
}

/**
 * Shift the string parameters in the global string parameter array by \a amount positions, making room at the beginning.
 * @param amount Number of positions to shift.
 */
void InjectDParam(uint amount)
{
	assert((uint)amount < lengthof(_decode_parameters));
	memmove(_decode_parameters + amount, _decode_parameters, sizeof(_decode_parameters) - amount * sizeof(uint64));
}

static char *FormatNumber(char *buff, int64 number, const char *last, const char *separator, int zerofill_from = 19)
{
	uint64 divisor = 10000000000000000000ULL;

	if (number < 0) {
		buff += seprintf(buff, last, "-");
		number = -number;
	}

	uint64 num = number;
	uint64 tot = 0;
	for (int i = 0; i < 20; i++) {
		uint64 quot = 0;
		if (num >= divisor) {
			quot = num / divisor;
			num = num % divisor;
		}
		if (tot |= quot || i >= zerofill_from) {
			buff += seprintf(buff, last, "%i", (int)quot);
			if ((i % 3) == 1 && i != 19) buff = strecpy(buff, separator, last);
		}

		divisor /= 10;
	}

	*buff = '\0';

	return buff;
}

static char *FormatCommaNumber(char *buff, int64 number, const char *last)
{
	const char *separator = _settings_game.locale.digit_group_separator;
	if (separator == NULL) separator = _langpack->digit_group_separator;
	return FormatNumber(buff, number, last, separator);
}

static char *FormatNoCommaNumber(char *buff, int64 number, const char *last)
{
	return FormatNumber(buff, number, last, "");
}

static char *FormatZerofillNumber(char *buff, int64 number, int64 count, const char *last)
{
	return FormatNumber(buff, number, last, "", 20 - count);
}

static char *FormatHexNumber(char *buff, uint64 number, const char *last)
{
	return buff + seprintf(buff, last, "0x" OTTD_PRINTFHEX64, number);
}

/**
 * Format a given number as a number of bytes with the SI prefix.
 * @param buff   the buffer to write to
 * @param number the number of bytes to write down
 * @param last   the last element in the buffer
 * @return till where we wrote
 */
static char *FormatBytes(char *buff, int64 number, const char *last)
{
	assert(number >= 0);

	/*                                    1   2^10  2^20  2^30  2^40  2^50  2^60 */
	const char * const iec_prefixes[] = { "", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei" };
	uint id = 1;
	while (number >= 1024 * 1024) {
		number /= 1024;
		id++;
	}

	const char *decimal_separator = _settings_game.locale.digit_decimal_separator;
	if (decimal_separator == NULL) decimal_separator = _langpack->digit_decimal_separator;

	if (number < 1024) {
		id = 0;
		buff += seprintf(buff, last, "%i", (int)number);
	} else if (number < 1024 * 10) {
		buff += seprintf(buff, last, "%i%s%02i", (int)number / 1024, decimal_separator, (int)(number % 1024) * 100 / 1024);
	} else if (number < 1024 * 100) {
		buff += seprintf(buff, last, "%i%s%01i", (int)number / 1024, decimal_separator, (int)(number % 1024) * 10 / 1024);
	} else {
		assert(number < 1024 * 1024);
		buff += seprintf(buff, last, "%i", (int)number / 1024);
	}

	assert(id < lengthof(iec_prefixes));
	buff += seprintf(buff, last, " %sB", iec_prefixes[id]);

	return buff;
}

static char *FormatYmdString(char *buff, Date date, uint modifier, const char *last)
{
	YearMonthDay ymd;
	ConvertDateToYMD(date, &ymd);

	int64 args[3] = { ymd.day + STR_ORDINAL_NUMBER_1ST - 1, STR_MONTH_ABBREV_JAN + ymd.month, ymd.year };
	return FormatString(buff, GetStringPtr(STR_FORMAT_DATE_LONG), args, endof(args), modifier >> 24, last);
}

static char *FormatMonthAndYear(char *buff, Date date, uint modifier, const char *last)
{
	YearMonthDay ymd;
	ConvertDateToYMD(date, &ymd);

	int64 args[2] = { STR_MONTH_JAN + ymd.month, ymd.year };
	return FormatString(buff, GetStringPtr(STR_FORMAT_DATE_SHORT), args, endof(args), modifier >> 24, last);
}

static char *FormatTinyOrISODate(char *buff, Date date, StringID str, const char *last)
{
	YearMonthDay ymd;
	ConvertDateToYMD(date, &ymd);

	char day[3];
	char month[3];
	/* We want to zero-pad the days and months */
	snprintf(day,   lengthof(day),   "%02i", ymd.day);
	snprintf(month, lengthof(month), "%02i", ymd.month + 1);

	int64 args[3] = { (int64)(size_t)day, (int64)(size_t)month, ymd.year };
	return FormatString(buff, GetStringPtr(str), args, endof(args), 0, last);
}

static char *FormatGenericCurrency(char *buff, const CurrencySpec *spec, Money number, bool compact, const char *last)
{
	/* We are going to make number absolute for printing, so
	 * keep this piece of data as we need it later on */
	bool negative = number < 0;
	const char *multiplier = "";

	number *= spec->rate;

	/* convert from negative */
	if (number < 0) {
		if (buff + Utf8CharLen(SCC_RED) > last) return buff;
		buff += Utf8Encode(buff, SCC_RED);
		buff = strecpy(buff, "-", last);
		number = -number;
	}

	/* Add prefix part, folowing symbol_pos specification.
	 * Here, it can can be either 0 (prefix) or 2 (both prefix anf suffix).
	 * The only remaining value is 1 (suffix), so everything that is not 1 */
	if (spec->symbol_pos != 1) buff = strecpy(buff, spec->prefix, last);

	/* for huge numbers, compact the number into k or M */
	if (compact) {
		/* Take care of the 'k' rounding. Having 1 000 000 k
		 * and 1 000 M is inconsistent, so always use 1 000 M. */
		if (number >= 1000000000 - 500) {
			number = (number + 500000) / 1000000;
			multiplier = "M";
		} else if (number >= 1000000) {
			number = (number + 500) / 1000;
			multiplier = "k";
		}
	}

	const char *separator = _settings_game.locale.digit_group_separator_currency;
	if (separator == NULL && !StrEmpty(_currency->separator)) separator = _currency->separator;
	if (separator == NULL) separator = _langpack->digit_group_separator_currency;
	buff = FormatNumber(buff, number, last, separator);
	buff = strecpy(buff, multiplier, last);

	/* Add suffix part, folowing symbol_pos specification.
	 * Here, it can can be either 1 (suffix) or 2 (both prefix anf suffix).
	 * The only remaining value is 1 (prefix), so everything that is not 0 */
	if (spec->symbol_pos != 0) buff = strecpy(buff, spec->suffix, last);

	if (negative) {
		if (buff + Utf8CharLen(SCC_PREVIOUS_COLOUR) > last) return buff;
		buff += Utf8Encode(buff, SCC_PREVIOUS_COLOUR);
		*buff = '\0';
	}

	return buff;
}

/**
 * Determine the "plural" index given a plural form and a number.
 * @param count       The number to get the plural index of.
 * @param plural_form The plural form we want an index for.
 * @return The plural index for the given form.
 */
static int DeterminePluralForm(int64 count, int plural_form)
{
	/* The absolute value determines plurality */
	uint64 n = abs(count);

	switch (plural_form) {
		default:
			NOT_REACHED();

		/* Two forms, singular used for one only
		 * Used in:
		 *   Danish, Dutch, English, German, Norwegian, Swedish, Estonian, Finnish,
		 *   Greek, Hebrew, Italian, Portuguese, Spanish, Esperanto */
		case 0:
			return n != 1;

		/* Only one form
		 * Used in:
		 *   Hungarian, Japanese, Korean, Turkish */
		case 1:
			return 0;

		/* Two forms, singular used for zero and one
		 * Used in:
		 *   French, Brazilian Portuguese */
		case 2:
			return n > 1;

		/* Three forms, special case for 0 and ending in 1, except those ending in 11
		 * Used in:
		 *   Latvian */
		case 3:
			return n % 10 == 1 && n % 100 != 11 ? 0 : n != 0 ? 1 : 2;

		/* Five forms, special case for one, two, 3 to 6 and 7 to 10
		 * Used in:
		 *   Gaelige (Irish) */
		case 4:
			return n == 1 ? 0 : n == 2 ? 1 : n < 7 ? 2 : n < 11 ? 3 : 4;

		/* Three forms, special case for numbers ending in 1[2-9]
		 * Used in:
		 *   Lithuanian */
		case 5:
			return n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;

		/* Three forms, special cases for numbers ending in 1 and 2, 3, 4, except those ending in 1[1-4]
		 * Used in:
		 *   Croatian, Russian, Ukrainian */
		case 6:
			return n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;

		/* Three forms, special case for one and some numbers ending in 2, 3, or 4
		 * Used in:
		 *   Polish */
		case 7:
			return n == 1 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;

		/* Four forms, special case for one and all numbers ending in 02, 03, or 04
		 * Used in:
		 *   Slovenian */
		case 8:
			return n % 100 == 1 ? 0 : n % 100 == 2 ? 1 : n % 100 == 3 || n % 100 == 4 ? 2 : 3;

		/* Two forms; singular used for everything ending in 1 but not in 11.
		 * Used in:
		 *   Icelandic */
		case 9:
			return n % 10 == 1 && n % 100 != 11 ? 0 : 1;

		/* Three forms, special cases for one and 2, 3, or 4
		 * Used in:
		 *   Czech, Slovak */
		case 10:
			return n == 1 ? 0 : n >= 2 && n <= 4 ? 1 : 2;

		/* Two forms, special 'hack' for Korean; singular for numbers ending
		 *   in a consonant and plural for numbers ending in a vowel.
		 * Korean doesn't have the concept of plural, but depending on how a
		 * number is pronounced it needs another version of a particle.
		 * As such the plural system is misused to give this distinction.
		 */
		case 11:
			switch (n % 10) {
				case 0: // yeong
				case 1: // il
				case 3: // sam
				case 6: // yuk
				case 7: // chil
				case 8: // pal
					return 0;

				case 2: // i
				case 4: // sa
				case 5: // o
				case 9: // gu
					return 1;

				default:
					NOT_REACHED();
			}

		/* Four forms: one, 0 and everything ending in 02..10, everything ending in 11..19.
		 * Used in:
		 *  Maltese */
		case 12:
			return (n == 1 ? 0 : n == 0 || (n % 100 > 1 && n % 100 < 11) ? 1 : (n % 100 > 10 && n % 100 < 20) ? 2 : 3);
	}
}

static const char *ParseStringChoice(const char *b, uint form, char **dst, const char *last)
{
	/* <NUM> {Length of each string} {each string} */
	uint n = (byte)*b++;
	uint pos, i, mypos = 0;

	for (i = pos = 0; i != n; i++) {
		uint len = (byte)*b++;
		if (i == form) mypos = pos;
		pos += len;
	}

	*dst += seprintf(*dst, last, "%s", b + mypos);
	return b + pos;
}

struct Units {
	int s_m;           ///< Multiplier for velocity
	int s_s;           ///< Shift for velocity
	StringID velocity; ///< String for velocity
	int p_m;           ///< Multiplier for power
	int p_s;           ///< Shift for power
	StringID power;    ///< String for velocity
	int w_m;           ///< Multiplier for weight
	int w_s;           ///< Shift for weight
	StringID s_weight; ///< Short string for weight
	StringID l_weight; ///< Long string for weight
	int v_m;           ///< Multiplier for volume
	int v_s;           ///< Shift for volume
	StringID s_volume; ///< Short string for volume
	StringID l_volume; ///< Long string for volume
	int f_m;           ///< Multiplier for force
	int f_s;           ///< Shift for force
	StringID force;    ///< String for force
	int h_m;           ///< Multiplier for height
	int h_s;           ///< Shift for height
	StringID height;   ///< String for height
};

/* Unit conversions */
static const Units units[] = {
	{ // Imperial (Original, mph, hp, metric ton, litre, kN, ft)
		   1,  0, STR_UNITS_VELOCITY_IMPERIAL,
		   1,  0, STR_UNITS_POWER_IMPERIAL,
		   1,  0, STR_UNITS_WEIGHT_SHORT_METRIC, STR_UNITS_WEIGHT_LONG_METRIC,
		1000,  0, STR_UNITS_VOLUME_SHORT_METRIC, STR_UNITS_VOLUME_LONG_METRIC,
		   1,  0, STR_UNITS_FORCE_SI,
		   3,  0, STR_UNITS_HEIGHT_IMPERIAL,
	},
	{ // Metric (km/h, hp, metric ton, litre, kN, metre)
		 103,  6, STR_UNITS_VELOCITY_METRIC,
		   1,  0, STR_UNITS_POWER_METRIC,
		   1,  0, STR_UNITS_WEIGHT_SHORT_METRIC, STR_UNITS_WEIGHT_LONG_METRIC,
		1000,  0, STR_UNITS_VOLUME_SHORT_METRIC, STR_UNITS_VOLUME_LONG_METRIC,
		   1,  0, STR_UNITS_FORCE_SI,
		   1,  0, STR_UNITS_HEIGHT_SI,
	},
	{ // SI (m/s, kilowatt, kilogram, cubic metre, kilonewton, metre)
		1831, 12, STR_UNITS_VELOCITY_SI,
		 764, 10, STR_UNITS_POWER_SI,
		1000,  0, STR_UNITS_WEIGHT_SHORT_SI, STR_UNITS_WEIGHT_LONG_SI,
		   1,  0, STR_UNITS_VOLUME_SHORT_SI, STR_UNITS_VOLUME_LONG_SI,
		   1,  0, STR_UNITS_FORCE_SI,
		   1,  0, STR_UNITS_HEIGHT_SI,
	},
};

/**
 * Convert the given (internal) speed to the display speed.
 * @param speed the speed to convert
 * @return the converted speed.
 */
uint ConvertSpeedToDisplaySpeed(uint speed)
{
	return (speed * units[_settings_game.locale.units].s_m) >> units[_settings_game.locale.units].s_s;
}

/**
 * Convert the given display speed to the (internal) speed.
 * @param speed the speed to convert
 * @return the converted speed.
 */
uint ConvertDisplaySpeedToSpeed(uint speed)
{
	return ((speed << units[_settings_game.locale.units].s_s) + units[_settings_game.locale.units].s_m / 2) / units[_settings_game.locale.units].s_m;
}

/**
 * Parse most format codes within a string and write the result to a buffer.
 * @param buff  The buffer to write the final string to.
 * @param str   The original string with format codes.
 * @param argv  Pointer to an array with extra arguments used by various string codes.
 * @param argve Pointer to just past the end of the argv array.
 * @param casei
 * @param last  Pointer to just past the end of the buff array.
 * @param argt  Pointer to an array with the string codes used to parse the argv array.
 * @param dry_run True when the argt array is not yet initialized.
 */
static char *FormatString(char *buff, const char *str_arg, int64 *argv, const int64 *argve, uint casei, const char *last, WChar *argt, bool dry_run)
{
	/* When there is no array with types there is no need to do a dry run. */
	if (argt == NULL) dry_run = true;
	if (UsingNewGRFTextStack() && !dry_run) {
		/* Values from the NewGRF text stack are only copied to the normal
		 * argv array at the time they are encountered. That means that if
		 * another string command references a value later in the string it
		 * would fail. We solve that by running FormatString twice. The first
		 * pass makes sure the argv array is correctly filled and the second
		 * pass can reference later values without problems. */
		struct TextRefStack *backup = CreateTextRefStackBackup();
		FormatString(buff, str_arg, argv, argve, casei, last, argt, true);
		RestoreTextRefStackBackup(backup);
	} else if (!dry_run) {
		FormatString(buff, str_arg, argv, argve, casei, last, argt, true);
	}
	WChar b;
	int64 *argv_orig = argv;
	WChar *argt_orig = argt;
	uint modifier = 0;
	char *buf_start = buff;
	std::stack<const char *> str_stack;
	str_stack.push(str_arg);

	while (true) {
		while (!str_stack.empty() && (b = Utf8Consume(&str_stack.top())) == '\0') {
			str_stack.pop();
		}
		if (str_stack.empty()) break;
		const char *&str = str_stack.top();

		if (SCC_NEWGRF_FIRST <= b && b <= SCC_NEWGRF_LAST) {
			/* We need to pass some stuff as it might be modified; oh boy. */
			//todo: should argve be passed here too?
			b = RemapNewGRFStringControlCode(b, buf_start, &buff, &str, argv);
			if (b == 0) continue;
		}

		switch (b) {
			case SCC_NEWGRF_STRINL: {
				StringID substr = Utf8Consume(&str);
				str_stack.push(GetStringPtr(substr));
				break;
			}

			case SCC_NEWGRF_PRINT_STRING_ID: {
				StringID substr = GetInt32(&argv, argve, &argt, SCC_NEWGRF_PRINT_STRING_ID);
				str_stack.push(GetStringPtr(substr));
				break;
			}


			case SCC_SETX: // {SETX}
				if (buff + Utf8CharLen(SCC_SETX) + 1 < last) {
					buff += Utf8Encode(buff, SCC_SETX);
					*buff++ = *str++;
				}
				break;

			case SCC_SETXY: // {SETXY}
				if (buff + Utf8CharLen(SCC_SETXY) + 2 < last) {
					buff += Utf8Encode(buff, SCC_SETXY);
					*buff++ = *str++;
					*buff++ = *str++;
				}
				break;

			case SCC_STRING_ID: // {STRINL}
				buff = GetStringWithArgs(buff, Utf8Consume(&str), argv, argve, last, argt);
				break;

			case SCC_RAW_STRING_POINTER: { // {RAW_STRING}
				const char *str = (const char*)(size_t)GetInt64(&argv, argve, &argt);
				buff = FormatString(buff, str, argv, argve, casei, last, argt);
				break;
			}

			case SCC_DATE_LONG: // {DATE_LONG}
				buff = FormatYmdString(buff, GetInt32(&argv, argve, &argt, SCC_DATE_LONG), modifier, last);
				break;

			case SCC_DATE_SHORT: // {DATE_SHORT}
				buff = FormatMonthAndYear(buff, GetInt32(&argv, argve, &argt, SCC_DATE_SHORT), modifier, last);
				break;

			case SCC_VELOCITY: { // {VELOCITY}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = ConvertSpeedToDisplaySpeed(GetInt32(&argv, argve, &argt, SCC_VELOCITY) * 10 / 16);
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].velocity), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_HEIGHT: { // {HEIGHT}
				int64 args[1] = {GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].h_m >> units[_settings_game.locale.units].h_s};
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].height), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_CURRENCY_COMPACT: // {CURRCOMPACT}
				buff = FormatGenericCurrency(buff, _currency, GetInt64(&argv, argve, &argt), true, last);
				break;

			case SCC_REVISION: // {REV}
				buff = strecpy(buff, _openttd_revision, last);
				break;

			case SCC_CARGO_SHORT: { // {SHORTCARGO}
				/* Short description of cargotypes. Layout:
				 * 8-bit = cargo type
				 * 16-bit = cargo count */
				StringID cargo_str = CargoSpec::Get(GetInt32(&argv, argve, &argt, SCC_CARGO_SHORT))->units_volume;
				switch (cargo_str) {
					case STR_TONS: {
						int64 args[1];
						assert(_settings_game.locale.units < lengthof(units));
						args[0] = GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].w_m >> units[_settings_game.locale.units].w_s;
						buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].l_weight), args, endof(args), modifier >> 24, last);
						modifier = 0;
						break;
					}

					case STR_LITERS: {
						int64 args[1];
						assert(_settings_game.locale.units < lengthof(units));
						args[0] = GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].v_m >> units[_settings_game.locale.units].v_s;
						buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].l_volume), args, endof(args), modifier >> 24, last);
						modifier = 0;
						break;
					}

					default:
						buff = GetStringWithArgs(buff, cargo_str, argv++, argve, last, argt++);
						break;
				}
				break;
			}

			case SCC_STRING1: { // {STRING1}
				/* String that consumes ONE argument */
				uint str = modifier + GetInt32(&argv, argve, &argt, SCC_STRING1);
				WChar *orig_argt = argt;
				int64 *args = GetArgvPtr(&argv, 1, argve, &argt);
				buff = GetStringWithArgs(buff, str, args, argve, last, orig_argt);
				modifier = 0;
				break;
			}

			case SCC_STRING2: { // {STRING2}
				/* String that consumes TWO arguments */
				uint str = modifier + GetInt32(&argv, argve, &argt, SCC_STRING2);
				WChar *orig_argt = argt;
				int64 *args = GetArgvPtr(&argv, 2, argve, &argt);
				buff = GetStringWithArgs(buff, str, args, argve, last, orig_argt);
				modifier = 0;
				break;
			}

			case SCC_STRING3: { // {STRING3}
				/* String that consumes THREE arguments */
				uint str = modifier + GetInt32(&argv, argve, &argt, SCC_STRING3);
				WChar *orig_argt = argt;
				int64 *args = GetArgvPtr(&argv, 3, argve, &argt);
				buff = GetStringWithArgs(buff, str, args, argve, last, orig_argt);
				modifier = 0;
				break;
			}

			case SCC_STRING4: { // {STRING4}
				/* String that consumes FOUR arguments */
				uint str = modifier + GetInt32(&argv, argve, &argt, SCC_STRING4);
				WChar *orig_argt = argt;
				int64 *args = GetArgvPtr(&argv, 4, argve, &argt);
				buff = GetStringWithArgs(buff, str, args, argve, last, orig_argt);
				modifier = 0;
				break;
			}

			case SCC_STRING5: { // {STRING5}
				/* String that consumes FIVE arguments */
				uint str = modifier + GetInt32(&argv, argve, &argt, SCC_STRING5);
				WChar *orig_argt = argt;
				int64 *args = GetArgvPtr(&argv, 5, argve, &argt);
				buff = GetStringWithArgs(buff, str, args, argve, last, orig_argt);
				modifier = 0;
				break;
			}

			case SCC_STATION_FEATURES: { // {STATIONFEATURES}
				buff = StationGetSpecialString(buff, GetInt32(&argv, argve, &argt, SCC_STATION_FEATURES), last);
				break;
			}

			case SCC_INDUSTRY_NAME: { // {INDUSTRY}
				const Industry *i = Industry::Get(GetInt32(&argv, argve, &argt, SCC_INDUSTRY_NAME));
				int64 args[2];

				/* industry not valid anymore? */
				assert(i != NULL);

				/* First print the town name and the industry type name. */
				args[0] = i->town->index;
				args[1] = GetIndustrySpec(i->type)->name;
				buff = FormatString(buff, GetStringPtr(STR_FORMAT_INDUSTRY_NAME), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_VOLUME: { // {VOLUME}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = GetInt32(&argv, argve, &argt, SCC_VOLUME) * units[_settings_game.locale.units].v_m >> units[_settings_game.locale.units].v_s;
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].l_volume), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_GENDER_LIST: { // {G 0 Der Die Das}
				/* First read the meta data from the language file. */
				byte offset = (byte)*str++;
				assert(argv_orig + offset < argve);
				int gender = 0;
				if (!dry_run && argt != NULL && argt_orig[offset] != 0) {
					/* Now we need to figure out what text to resolve, i.e.
					 * what do we need to draw? So get the actual raw string
					 * first using the control code to get said string. */
					char input[4 + 1];
					char *p = input + Utf8Encode(input, argt_orig[offset]);
					*p = '\0';

					/* Now do the string formatting. */
					char buf[256];
					bool old_kgd = _keep_gender_data;
					_keep_gender_data = true;
					p = FormatString(buf, input, argv_orig + offset, argve, 0, lastof(buf));
					_keep_gender_data = old_kgd;
					*p = '\0';

					/* And determine the string. */
					const char *s = buf;
					WChar c = Utf8Consume(&s);
					/* Does this string have a gender, if so, set it */
					if (c == SCC_GENDER_INDEX) gender = (byte)s[0];
				}
				str = ParseStringChoice(str, gender, &buff, last);
				break;
			}

			case SCC_DATE_TINY: { // {DATE_TINY}
				buff = FormatTinyOrISODate(buff, GetInt32(&argv, argve, &argt, SCC_DATE_TINY), STR_FORMAT_DATE_TINY, last);
				break;
			}

			case SCC_DATE_ISO: { // {DATE_ISO}
				buff = FormatTinyOrISODate(buff, GetInt32(&argv, argve, &argt), STR_FORMAT_DATE_ISO, last);
				break;
			}

			case SCC_CARGO: { // {CARGO}
				/* First parameter is cargo type, second parameter is cargo count */
				CargoID cargo = GetInt32(&argv, argve, &argt, SCC_CARGO);
				StringID cargo_str = (cargo == CT_INVALID) ? STR_QUANTITY_N_A : CargoSpec::Get(cargo)->quantifier;
				buff = GetStringWithArgs(buff, cargo_str, argv++, argve, last);
				break;
			}

			case SCC_POWER: { // {POWER}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].p_m >> units[_settings_game.locale.units].p_s;
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].power), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_VOLUME_SHORT: { // {VOLUME_S}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].v_m >> units[_settings_game.locale.units].v_s;
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].s_volume), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_WEIGHT: { // {WEIGHT}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = GetInt32(&argv, argve, &argt, SCC_WEIGHT) * units[_settings_game.locale.units].w_m >> units[_settings_game.locale.units].w_s;
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].l_weight), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_WEIGHT_SHORT: { // {WEIGHT_S}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].w_m >> units[_settings_game.locale.units].w_s;
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].s_weight), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			case SCC_FORCE: { // {FORCE}
				int64 args[1];
				assert(_settings_game.locale.units < lengthof(units));
				args[0] = GetInt32(&argv, argve, &argt) * units[_settings_game.locale.units].f_m >> units[_settings_game.locale.units].f_s;
				buff = FormatString(buff, GetStringPtr(units[_settings_game.locale.units].force), args, endof(args), modifier >> 24, last);
				modifier = 0;
				break;
			}

			/* This sets up the gender for the string.
			 * We just ignore this one. It's used in {G 0 Der Die Das} to determine the case. */
			case SCC_GENDER_INDEX: // {GENDER 0}
				if (_keep_gender_data) {
					buff += Utf8Encode(buff, SCC_GENDER_INDEX);
					*buff++ = *str++;
				} else {
					str++;
				}
				break;

			case SCC_STRING: {// {STRING}
				uint str = modifier + GetInt32(&argv, argve, &argt, SCC_STRING);
				/* WARNING. It's prohibited for the included string to consume any arguments.
				 * For included strings that consume argument, you should use STRING1, STRING2 etc.
				 * To debug stuff you can set argv to NULL and it will tell you */
				buff = GetStringWithArgs(buff, str, argv, argve, last);
				modifier = 0;
				break;
			}

			case SCC_COMMA: // {COMMA}
				buff = FormatCommaNumber(buff, GetInt64(&argv, argve, &argt, SCC_COMMA), last);
				break;

			case SCC_ARG_INDEX: { // Move argument pointer
				byte offset = (byte)*str++;
				argv = argv_orig + offset;
				if (argt_orig != NULL) argt = argt_orig + offset;
				break;
			}

			case SCC_PLURAL_LIST: { // {P}
				int plural_form = *str++;          // contains the plural form for this string
				byte idx = *str++;
				assert(argv_orig + idx < argve);
				int64 v = argv_orig[idx]; // contains the number that determines plural
				str = ParseStringChoice(str, DeterminePluralForm(v, plural_form), &buff, last);
				break;
			}

			case SCC_NUM: // {NUM}
				buff = FormatNoCommaNumber(buff, GetInt64(&argv, argve, &argt, SCC_NUM), last);
				break;

			case SCC_ZEROFILL_NUM: { // {ZEROFILL_NUM}
				int64 num = GetInt64(&argv, argve, &argt);
				buff = FormatZerofillNumber(buff, num, GetInt64(&argv, argve, &argt), last);
				break;
			}

			case SCC_HEX: // {HEX}
				buff = FormatHexNumber(buff, (uint64)GetInt64(&argv, argve, &argt, SCC_HEX), last);
				break;

			case SCC_BYTES: // {BYTES}
				buff = FormatBytes(buff, GetInt64(&argv, argve, &argt), last);
				break;

			case SCC_CURRENCY: // {CURRENCY}
				buff = FormatGenericCurrency(buff, _currency, GetInt64(&argv, argve, &argt, SCC_CURRENCY), false, last);
				break;

			case SCC_WAYPOINT_NAME: { // {WAYPOINT}
				Waypoint *wp = Waypoint::Get(GetInt32(&argv, argve, &argt, SCC_WAYPOINT_NAME));

				assert(wp != NULL);

				if (wp->name != NULL) {
					buff = strecpy(buff, wp->name, last);
				} else {
					int64 args[2];
					args[0] = wp->town->index;
					args[1] = wp->town_cn + 1;
					StringID str = ((wp->string_id == STR_SV_STNAME_BUOY) ? STR_FORMAT_BUOY_NAME : STR_FORMAT_WAYPOINT_NAME);
					if (wp->town_cn != 0) str++;
					buff = GetStringWithArgs(buff, str, args, endof(args), last);
				}
				break;
			}

			case SCC_STATION_NAME: { // {STATION}
				StationID sid = GetInt32(&argv, argve, &argt, SCC_STATION_NAME);
				const Station *st = Station::GetIfValid(sid);

				if (st == NULL) {
					/* The station doesn't exist anymore. The only place where we might
					 * be "drawing" an invalid station is in the case of cargo that is
					 * in transit. */
					buff = GetStringWithArgs(buff, STR_UNKNOWN_STATION, NULL, NULL, last);
					break;
				}

				if (st->name != NULL) {
					buff = strecpy(buff, st->name, last);
				} else {
					StringID str = st->string_id;
					if (st->indtype != IT_INVALID) {
						/* Special case where the industry provides the name for the station */
						const IndustrySpec *indsp = GetIndustrySpec(st->indtype);

						/* Industry GRFs can change which might remove the station name and
						 * thus cause very strange things. Here we check for that before we
						 * actually set the station name. */
						if (indsp->station_name != STR_NULL && indsp->station_name != STR_UNDEFINED) {
							str = indsp->station_name;
						}
					}

					int64 args[3];
					args[0] = STR_TOWN_NAME;
					args[1] = st->town->index;
					args[2] = st->index;
					buff = GetStringWithArgs(buff, str, args, endof(args), last);
				}
				break;
			}

			case SCC_DEPOT_NAME: { // {DEPOT}
				VehicleType vt = (VehicleType)GetInt32(&argv, argve, &argt, SCC_DEPOT_NAME);
				if (vt == VEH_AIRCRAFT) {
					int64 args[] = { GetInt32(&argv, argve, &argt) };
					buff = GetStringWithArgs(buff, STR_FORMAT_DEPOT_NAME_AIRCRAFT, args, endof(args), last);
					break;
				}

				const Depot *d = Depot::Get(GetInt32(&argv, argve, &argt));
				if (d->name != NULL) {
					buff = strecpy(buff, d->name, last);
				} else {
					int64 args[] = { d->town->index, d->town_cn + 1 };
					buff = GetStringWithArgs(buff, STR_FORMAT_DEPOT_NAME_TRAIN + 2 * vt + (d->town_cn == 0 ? 0 : 1), args, endof(args), last);
				}
				break;
			}

			case SCC_TOWN_NAME: { // {TOWN}
				const Town *t = Town::Get(GetInt32(&argv, argve, &argt, SCC_TOWN_NAME));

				assert(t != NULL);

				if (t->name != NULL) {
					buff = strecpy(buff, t->name, last);
				} else {
					buff = GetTownName(buff, t, last);
				}
				break;
			}

			case SCC_GROUP_NAME: { // {GROUP}
				const Group *g = Group::Get(GetInt32(&argv, argve, &argt));

				assert(g != NULL);

				if (g->name != NULL) {
					buff = strecpy(buff, g->name, last);
				} else {
					int64 args[1];

					args[0] = g->index;
					buff = GetStringWithArgs(buff, STR_FORMAT_GROUP_NAME, args, endof(args), last);
				}
				break;
			}

			case SCC_ENGINE_NAME: { // {ENGINE}
				EngineID engine = (EngineID)GetInt32(&argv, argve, &argt, SCC_ENGINE_NAME);
				const Engine *e = Engine::Get(engine);

				assert(e != NULL);

				if (e->name != NULL && e->info.string_id != STR_NEWGRF_INVALID_ENGINE) {
					buff = strecpy(buff, e->name, last);
				} else {
					buff = GetStringWithArgs(buff, e->info.string_id, NULL, NULL, last);
				}
				break;
			}

			case SCC_VEHICLE_NAME: { // {VEHICLE}
				const Vehicle *v = Vehicle::Get(GetInt32(&argv, argve, &argt, SCC_VEHICLE_NAME));

				assert(v != NULL);

				if (v->name != NULL) {
					buff = strecpy(buff, v->name, last);
				} else {
					int64 args[1];
					args[0] = v->unitnumber;

					StringID str;
					switch (v->type) {
						default: NOT_REACHED();
						case VEH_TRAIN:    str = STR_SV_TRAIN_NAME; break;
						case VEH_ROAD:     str = STR_SV_ROAD_VEHICLE_NAME; break;
						case VEH_SHIP:     str = STR_SV_SHIP_NAME; break;
						case VEH_AIRCRAFT: str = STR_SV_AIRCRAFT_NAME; break;
					}

					buff = GetStringWithArgs(buff, str, args, endof(args), last);
				}
				break;
			}

			case SCC_SIGN_NAME: { // {SIGN}
				const Sign *si = Sign::Get(GetInt32(&argv, argve, &argt));
				if (si->name != NULL) {
					buff = strecpy(buff, si->name, last);
				} else {
					buff = GetStringWithArgs(buff, STR_DEFAULT_SIGN_NAME, NULL, NULL, last);
				}
				break;
			}

			case SCC_COMPANY_NAME: { // {COMPANY}
				const Company *c = Company::Get((CompanyID)GetInt32(&argv, argve, &argt));

				if (c->name != NULL) {
					buff = strecpy(buff, c->name, last);
				} else {
					int64 args[1];
					args[0] = c->name_2;
					buff = GetStringWithArgs(buff, c->name_1, args, endof(args), last);
				}
				break;
			}

			case SCC_COMPANY_NUM: { // {COMPANYNUM}
				CompanyID company = (CompanyID)GetInt32(&argv, argve, &argt);

				/* Nothing is added for AI or inactive companies */
				if (Company::IsValidHumanID(company)) {
					int64 args[1];
					args[0] = company + 1;
					buff = GetStringWithArgs(buff, STR_FORMAT_COMPANY_NUM, args, endof(args), last);
				}
				break;
			}

			case SCC_PRESIDENT_NAME: { // {PRESIDENTNAME}
				const Company *c = Company::Get((CompanyID)GetInt32(&argv, argve, &argt, SCC_PRESIDENT_NAME));

				if (c->president_name != NULL) {
					buff = strecpy(buff, c->president_name, last);
				} else {
					int64 args[1];
					args[0] = c->president_name_2;
					buff = GetStringWithArgs(buff, c->president_name_1, args, endof(args), last);
				}
				break;
			}

			case SCC_SETCASE: { // {SETCASE}
				/* This is a pseudo command, it's outputted when someone does {STRING.ack}
				 * The modifier is added to all subsequent GetStringWithArgs that accept the modifier. */
				modifier = (byte)*str++ << 24;
				break;
			}

			case SCC_SWITCH_CASE: { // {Used to implement case switching}
				/* <0x9E> <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
				 * Each LEN is printed using 2 bytes in big endian order. */
				uint num = (byte)*str++;
				while (num) {
					if ((byte)str[0] == casei) {
						/* Found the case, adjust str pointer and continue */
						str += 3;
						break;
					}
					/* Otherwise skip to the next case */
					str += 3 + (str[1] << 8) + str[2];
					num--;
				}
				break;
			}

			default:
				if (buff + Utf8CharLen(b) < last) buff += Utf8Encode(buff, b);
				break;
		}
	}
	*buff = '\0';
	return buff;
}


static char *StationGetSpecialString(char *buff, int x, const char *last)
{
	if ((x & FACIL_TRAIN)      && (buff + Utf8CharLen(SCC_TRAIN) < last)) buff += Utf8Encode(buff, SCC_TRAIN);
	if ((x & FACIL_TRUCK_STOP) && (buff + Utf8CharLen(SCC_LORRY) < last)) buff += Utf8Encode(buff, SCC_LORRY);
	if ((x & FACIL_BUS_STOP)   && (buff + Utf8CharLen(SCC_BUS)   < last)) buff += Utf8Encode(buff, SCC_BUS);
	if ((x & FACIL_DOCK)       && (buff + Utf8CharLen(SCC_SHIP)  < last)) buff += Utf8Encode(buff, SCC_SHIP);
	if ((x & FACIL_AIRPORT)    && (buff + Utf8CharLen(SCC_PLANE) < last)) buff += Utf8Encode(buff, SCC_PLANE);
	*buff = '\0';
	return buff;
}

static char *GetSpecialTownNameString(char *buff, int ind, uint32 seed, const char *last)
{
	return GenerateTownNameString(buff, last, ind, seed);
}

static const char * const _silly_company_names[] = {
	"Bloggs Brothers",
	"Tiny Transport Ltd.",
	"Express Travel",
	"Comfy-Coach & Co.",
	"Crush & Bump Ltd.",
	"Broken & Late Ltd.",
	"Sam Speedy & Son",
	"Supersonic Travel",
	"Mike's Motors",
	"Lightning International",
	"Pannik & Loozit Ltd.",
	"Inter-City Transport",
	"Getout & Pushit Ltd."
};

static const char * const _surname_list[] = {
	"Adams",
	"Allan",
	"Baker",
	"Bigwig",
	"Black",
	"Bloggs",
	"Brown",
	"Campbell",
	"Gordon",
	"Hamilton",
	"Hawthorn",
	"Higgins",
	"Green",
	"Gribble",
	"Jones",
	"McAlpine",
	"MacDonald",
	"McIntosh",
	"Muir",
	"Murphy",
	"Nelson",
	"O'Donnell",
	"Parker",
	"Phillips",
	"Pilkington",
	"Quigley",
	"Sharkey",
	"Thomson",
	"Watkins"
};

static const char * const _silly_surname_list[] = {
	"Grumpy",
	"Dozy",
	"Speedy",
	"Nosey",
	"Dribble",
	"Mushroom",
	"Cabbage",
	"Sniffle",
	"Fishy",
	"Swindle",
	"Sneaky",
	"Nutkins"
};

static const char _initial_name_letters[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
	'K', 'L', 'M', 'N', 'P', 'R', 'S', 'T', 'W',
};

static char *GenAndCoName(char *buff, uint32 arg, const char *last)
{
	const char * const *base;
	uint num;

	if (_settings_game.game_creation.landscape == LT_TOYLAND) {
		base = _silly_surname_list;
		num  = lengthof(_silly_surname_list);
	} else {
		base = _surname_list;
		num  = lengthof(_surname_list);
	}

	buff = strecpy(buff, base[num * GB(arg, 16, 8) >> 8], last);
	buff = strecpy(buff, " & Co.", last);

	return buff;
}

static char *GenPresidentName(char *buff, uint32 x, const char *last)
{
	char initial[] = "?. ";
	const char * const *base;
	uint num;
	uint i;

	initial[0] = _initial_name_letters[sizeof(_initial_name_letters) * GB(x, 0, 8) >> 8];
	buff = strecpy(buff, initial, last);

	i = (sizeof(_initial_name_letters) + 35) * GB(x, 8, 8) >> 8;
	if (i < sizeof(_initial_name_letters)) {
		initial[0] = _initial_name_letters[i];
		buff = strecpy(buff, initial, last);
	}

	if (_settings_game.game_creation.landscape == LT_TOYLAND) {
		base = _silly_surname_list;
		num  = lengthof(_silly_surname_list);
	} else {
		base = _surname_list;
		num  = lengthof(_surname_list);
	}

	buff = strecpy(buff, base[num * GB(x, 16, 8) >> 8], last);

	return buff;
}

static char *GetSpecialNameString(char *buff, int ind, int64 *argv, const int64 *argve, const char *last, WChar *argt)
{
	switch (ind) {
		case 1: // not used
			return strecpy(buff, _silly_company_names[GetInt32(&argv, argve, &argt) & 0xFFFF], last);

		case 2: // used for Foobar & Co company names
			return GenAndCoName(buff, GetInt32(&argv, argve, &argt), last);

		case 3: // President name
			return GenPresidentName(buff, GetInt32(&argv, argve, &argt), last);
	}

	/* town name? */
	if (IsInsideMM(ind - 6, 0, SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1)) {
		buff = GetSpecialTownNameString(buff, ind - 6, GetInt32(&argv, argve, &argt), last);
		return strecpy(buff, " Transport", last);
	}

	/* language name? */
	if (IsInsideMM(ind, (SPECSTR_LANGUAGE_START - 0x70E4), (SPECSTR_LANGUAGE_END - 0x70E4) + 1)) {
		int i = ind - (SPECSTR_LANGUAGE_START - 0x70E4);
		return strecpy(buff,
			&_languages[i] == _current_language ? _current_language->own_name : _languages[i].name, last);
	}

	/* resolution size? */
	if (IsInsideMM(ind, (SPECSTR_RESOLUTION_START - 0x70E4), (SPECSTR_RESOLUTION_END - 0x70E4) + 1)) {
		int i = ind - (SPECSTR_RESOLUTION_START - 0x70E4);
		buff += seprintf(
			buff, last, "%ux%u", _resolutions[i].width, _resolutions[i].height
		);
		return buff;
	}

	/* screenshot format name? */
	if (IsInsideMM(ind, (SPECSTR_SCREENSHOT_START - 0x70E4), (SPECSTR_SCREENSHOT_END - 0x70E4) + 1)) {
		int i = ind - (SPECSTR_SCREENSHOT_START - 0x70E4);
		return strecpy(buff, GetScreenshotFormatDesc(i), last);
	}

	NOT_REACHED();
}

#ifdef ENABLE_NETWORK
extern void SortNetworkLanguages();
#else /* ENABLE_NETWORK */
static inline void SortNetworkLanguages() {}
#endif /* ENABLE_NETWORK */

/**
 * Check whether the header is a valid header for OpenTTD.
 * @return true iff the header is deemed valid.
 */
bool LanguagePackHeader::IsValid() const
{
	return this->ident        == TO_LE32(LanguagePackHeader::IDENT) &&
	       this->version      == TO_LE32(LANGUAGE_PACK_VERSION) &&
	       this->plural_form  <  LANGUAGE_MAX_PLURAL &&
	       this->text_dir     <= 1 &&
	       this->newgrflangid < MAX_LANG &&
	       this->num_genders  < MAX_NUM_GENDERS &&
	       this->num_cases    < MAX_NUM_CASES &&
	       StrValid(this->name,                           lastof(this->name)) &&
	       StrValid(this->own_name,                       lastof(this->own_name)) &&
	       StrValid(this->isocode,                        lastof(this->isocode)) &&
	       StrValid(this->digit_group_separator,          lastof(this->digit_group_separator)) &&
	       StrValid(this->digit_group_separator_currency, lastof(this->digit_group_separator_currency)) &&
	       StrValid(this->digit_decimal_separator,        lastof(this->digit_decimal_separator));
}

bool ReadLanguagePack(const LanguageMetadata *lang)
{
	/* Current language pack */
	size_t len;
	LanguagePack *lang_pack = (LanguagePack *)ReadFileToMem(lang->file, &len, 200000);
	if (lang_pack == NULL) return false;

	/* End of read data (+ terminating zero added in ReadFileToMem()) */
	const char *end = (char *)lang_pack + len + 1;

	/* We need at least one byte of lang_pack->data */
	if (end <= lang_pack->data || !lang_pack->IsValid()) {
		free(lang_pack);
		return false;
	}

#if TTD_ENDIAN == TTD_BIG_ENDIAN
	for (uint i = 0; i < 32; i++) {
		lang_pack->offsets[i] = ReadLE16Aligned(&lang_pack->offsets[i]);
	}
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */

	uint count = 0;
	for (uint i = 0; i < 32; i++) {
		uint num = lang_pack->offsets[i];
		_langtab_start[i] = count;
		_langtab_num[i] = num;
		count += num;
	}

	/* Allocate offsets */
	char **langpack_offs = MallocT<char *>(count);

	/* Fill offsets */
	char *s = lang_pack->data;
	len = (byte)*s++;
	for (uint i = 0; i < count; i++) {
		if (s + len >= end) {
			free(lang_pack);
			free(langpack_offs);
			return false;
		}
		if (len >= 0xC0) {
			len = ((len & 0x3F) << 8) + (byte)*s++;
			if (s + len >= end) {
				free(lang_pack);
				free(langpack_offs);
				return false;
			}
		}
		langpack_offs[i] = s;
		s += len;
		len = (byte)*s;
		*s++ = '\0'; // zero terminate the string
	}

	free(_langpack);
	_langpack = lang_pack;

	free(_langpack_offs);
	_langpack_offs = langpack_offs;

	_current_language = lang;
	_current_text_dir = (TextDirection)_current_language->text_dir;
	const char *c_file = strrchr(_current_language->file, PATHSEPCHAR) + 1;
	strecpy(_config_language_file, c_file, lastof(_config_language_file));
	SetCurrentGrfLangID(_current_language->newgrflangid);

#ifdef WITH_ICU
	/* Delete previous collator. */
	if (_current_collator != NULL) {
		delete _current_collator;
		_current_collator = NULL;
	}

	/* Create a collator instance for our current locale. */
	UErrorCode status = U_ZERO_ERROR;
	_current_collator = Collator::createInstance(Locale(_current_language->isocode), status);
	/* Sort number substrings by their numerical value. */
	if (_current_collator != NULL) _current_collator->setAttribute(UCOL_NUMERIC_COLLATION, UCOL_ON, status);
	/* Avoid using the collator if it is not correctly set. */
	if (U_FAILURE(status)) {
		delete _current_collator;
		_current_collator = NULL;
	}
#endif /* WITH_ICU */

	/* Some lists need to be sorted again after a language change. */
	InitializeSortedCargoSpecs();
	SortIndustryTypes();
	BuildIndustriesLegend();
	SortNetworkLanguages();
	InvalidateWindowClassesData(WC_BUILD_VEHICLE);      // Build vehicle window.
	InvalidateWindowClassesData(WC_TRAINS_LIST);        // Train group window.
	InvalidateWindowClassesData(WC_ROADVEH_LIST);       // Road vehicle group window.
	InvalidateWindowClassesData(WC_SHIPS_LIST);         // Ship group window.
	InvalidateWindowClassesData(WC_AIRCRAFT_LIST);      // Aircraft group window.
	InvalidateWindowClassesData(WC_INDUSTRY_DIRECTORY); // Industry directory window.
	InvalidateWindowClassesData(WC_STATION_LIST);       // Station list window.

	return true;
}

/* Win32 implementation in win32.cpp.
 * OS X implementation in os/macosx/macos.mm. */
#if !(defined(WIN32) || defined(__APPLE__))
/**
 * Determine the current charset based on the environment
 * First check some default values, after this one we passed ourselves
 * and if none exist return the value for $LANG
 * @param param environment variable to check conditionally if default ones are not
 *        set. Pass NULL if you don't want additional checks.
 * @return return string containing current charset, or NULL if not-determinable
 */
const char *GetCurrentLocale(const char *param)
{
	const char *env;

	env = getenv("LANGUAGE");
	if (env != NULL) return env;

	env = getenv("LC_ALL");
	if (env != NULL) return env;

	if (param != NULL) {
		env = getenv(param);
		if (env != NULL) return env;
	}

	return getenv("LANG");
}
#else
const char *GetCurrentLocale(const char *param);
#endif /* !(defined(WIN32) || defined(__APPLE__)) */

int CDECL StringIDSorter(const StringID *a, const StringID *b)
{
	char stra[512];
	char strb[512];
	GetString(stra, *a, lastof(stra));
	GetString(strb, *b, lastof(strb));

	return strcmp(stra, strb);
}

/**
 * Get the language with the given NewGRF language ID.
 * @param newgrflangid NewGRF languages ID to check.
 * @return The language's metadata, or NULL if it is not known.
 */
const LanguageMetadata *GetLanguage(byte newgrflangid)
{
	for (const LanguageMetadata *lang = _languages.Begin(); lang != _languages.End(); lang++) {
		if (newgrflangid == lang->newgrflangid) return lang;
	}

	return NULL;
}

/**
 * Reads the language file header and checks compatability.
 * @param file the file to read
 * @param hdr  the place to write the header information to
 * @return true if and only if the language file is of a compatible version
 */
static bool GetLanguageFileHeader(const char *file, LanguagePackHeader *hdr)
{
	FILE *f = fopen(file, "rb");
	if (f == NULL) return false;

	size_t read = fread(hdr, sizeof(*hdr), 1, f);
	fclose(f);

	bool ret = read == 1 && hdr->IsValid();

	/* Convert endianness for the windows language ID */
	if (ret) hdr->winlangid = FROM_LE16(hdr->winlangid);
	return ret;
}

/**
 * Gets a list of languages from the given directory.
 * @param path  the base directory to search in
 */
static void GetLanguageList(const char *path)
{
	DIR *dir = ttd_opendir(path);
	if (dir != NULL) {
		struct dirent *dirent;
		while ((dirent = readdir(dir)) != NULL) {
			const char *d_name    = FS2OTTD(dirent->d_name);
			const char *extension = strrchr(d_name, '.');

			/* Not a language file */
			if (extension == NULL || strcmp(extension, ".lng") != 0) continue;

			LanguageMetadata lmd;
			seprintf(lmd.file, lastof(lmd.file), "%s%s", path, d_name);

			/* Check whether the file is of the correct version */
			if (!GetLanguageFileHeader(lmd.file, &lmd)) {
				DEBUG(misc, 3, "%s is not a valid language file", lmd.file);
			} else if (GetLanguage(lmd.newgrflangid) != NULL) {
				DEBUG(misc, 3, "%s's language ID is already known", lmd.file);
			} else {
				*_languages.Append() = lmd;
			}
		}
		closedir(dir);
	}
}

/**
 * Make a list of the available language packs. put the data in
 * _dynlang struct.
 */
void InitializeLanguagePacks()
{
	Searchpath sp;

	FOR_ALL_SEARCHPATHS(sp) {
		char path[MAX_PATH];
		FioAppendDirectory(path, lengthof(path), sp, LANG_DIR);
		GetLanguageList(path);
	}
	if (_languages.Length() == 0) usererror("No available language packs (invalid versions?)");

	/* Acquire the locale of the current system */
	const char *lang = GetCurrentLocale("LC_MESSAGES");
	if (lang == NULL) lang = "en_GB";

	const LanguageMetadata *chosen_language   = NULL; ///< Matching the language in the configuartion file or the current locale
	const LanguageMetadata *language_fallback = NULL; ///< Using pt_PT for pt_BR locale when pt_BR is not available
	const LanguageMetadata *en_GB_fallback    = _languages.Begin(); ///< Fallback when no locale-matching language has been found

	/* Find a proper language. */
	for (const LanguageMetadata *lng = _languages.Begin(); lng != _languages.End(); lng++) {
		/* We are trying to find a default language. The priority is by
		 * configuration file, local environment and last, if nothing found,
		 * english. */
		const char *lang_file = strrchr(lng->file, PATHSEPCHAR) + 1;
		if (strcmp(lang_file, _config_language_file) == 0) {
			chosen_language = lng;
			break;
		}

		if (strcmp (lng->isocode, "en_GB") == 0) en_GB_fallback    = lng;
		if (strncmp(lng->isocode, lang, 5) == 0) chosen_language   = lng;
		if (strncmp(lng->isocode, lang, 2) == 0) language_fallback = lng;
	}

	/* We haven't found the language in the config nor the one in the locale.
	 * Now we set it to one of the fallback languages */
	if (chosen_language == NULL) {
		chosen_language = (language_fallback != NULL) ? language_fallback : en_GB_fallback;
	}

	if (!ReadLanguagePack(chosen_language)) usererror("Can't read language pack '%s'", chosen_language->file);
}

/**
 * Get the ISO language code of the currently loaded language.
 * @return the ISO code.
 */
const char *GetCurrentLanguageIsoCode()
{
	return _langpack->isocode;
}

/**
 * Check whether there are glyphs missing in the current language.
 * @param Pointer to an address for storing the text pointer.
 * @return If glyphs are missing, return \c true, else return \false.
 * @post If \c true is returned and str is not NULL, *str points to a string that is found to contain at least one missing glyph.
 */
static bool FindMissingGlyphs(const char **str)
{
#ifdef WITH_FREETYPE
	UninitFreeType();
	InitFreeType();
#endif
	const Sprite *question_mark[FS_END];
	FontSize size;

	for (size = FS_BEGIN; size < FS_END; size++) {
		question_mark[size] = GetGlyph(size, '?');
	}

	for (uint i = 0; i != 32; i++) {
		for (uint j = 0; j < _langtab_num[i]; j++) {
			size = FS_NORMAL;
			const char *text = _langpack_offs[_langtab_start[i] + j];
			if (str != NULL) *str = text;
			for (WChar c = Utf8Consume(&text); c != '\0'; c = Utf8Consume(&text)) {
				if (c == SCC_SETX) {
					/* SetX is, together with SetXY as special character that
					 * uses the next (two) characters as data points. We have
					 * to skip those, otherwise the UTF8 reading will go haywire. */
					text++;
				} else if (c == SCC_SETXY) {
					text += 2;
				} else if (c == SCC_TINYFONT) {
					size = FS_SMALL;
				} else if (c == SCC_BIGFONT) {
					size = FS_LARGE;
				} else if (IsPrintable(c) && !IsTextDirectionChar(c) && c != '?' && GetGlyph(size, c) == question_mark[size]) {
					/* The character is printable, but not in the normal font. This is the case we were testing for. */
					return true;
				}
			}
		}
	}
	return false;
}

/**
 * Check whether the currently loaded language pack
 * uses characters that the currently loaded font
 * does not support. If this is the case an error
 * message will be shown in English. The error
 * message will not be localized because that would
 * mean it might use characters that are not in the
 * font, which is the whole reason this check has
 * been added.
 */
void CheckForMissingGlyphsInLoadedLanguagePack()
{
	bool bad_font = FindMissingGlyphs(NULL);
#ifdef WITH_FREETYPE
	if (bad_font) {
		/* We found an unprintable character... lets try whether we can find
		 * a fallback font that can print the characters in the current language. */
		FreeTypeSettings backup;
		memcpy(&backup, &_freetype, sizeof(backup));

		bad_font = !SetFallbackFont(&_freetype, _langpack->isocode, _langpack->winlangid, &FindMissingGlyphs);

		memcpy(&_freetype, &backup, sizeof(backup));

		if (bad_font) {
			/* Our fallback font does miss characters too, so keep the
			 * user chosen font as that is more likely to be any good than
			 * the wild guess we made */
			UninitFreeType();
			InitFreeType();
		}
	}
#endif

	if (bad_font) {
		/* All attempts have failed. Display an error. As we do not want the string to be translated by
		 * the translators, we 'force' it into the binary and 'load' it via a BindCString. To do this
		 * properly we have to set the colour of the string, otherwise we end up with a lot of artefacts.
		 * The colour 'character' might change in the future, so for safety we just Utf8 Encode it into
		 * the string, which takes exactly three characters, so it replaces the "XXX" with the colour marker. */
		static char *err_str = strdup("XXXThe current font is missing some of the characters used in the texts for this language. Read the readme to see how to solve this.");
		Utf8Encode(err_str, SCC_YELLOW);
		SetDParamStr(0, err_str);
		ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_WARNING);

		/* Reset the font width */
		LoadStringWidthTable();
		return;
	}

	/* Update the font with cache */
	LoadStringWidthTable();

#if !defined(WITH_ICU)
	/*
	 * For right-to-left languages we need the ICU library. If
	 * we do not have support for that library we warn the user
	 * about it with a message. As we do not want the string to
	 * be translated by the translators, we 'force' it into the
	 * binary and 'load' it via a BindCString. To do this
	 * properly we have to set the colour of the string,
	 * otherwise we end up with a lot of artefacts. The colour
	 * 'character' might change in the future, so for safety
	 * we just Utf8 Encode it into the string, which takes
	 * exactly three characters, so it replaces the "XXX" with
	 * the colour marker.
	 */
	if (_current_text_dir != TD_LTR) {
		static char *err_str = strdup("XXXThis version of OpenTTD does not support right-to-left languages. Recompile with icu enabled.");
		Utf8Encode(err_str, SCC_YELLOW);
		SetDParamStr(0, err_str);
		ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
	}
#endif
}
