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
#include "waypoint_base.h"
#include "depot_base.h"
#include "industry.h"
#include "newgrf_text.h"
#include "fileio_func.h"
#include "signs_base.h"
#include "fontdetection.h"
#include "error.h"
#include "error_func.h"
#include "strings_func.h"
#include "rev.h"
#include "core/endian_func.hpp"
#include "timer/timer_game_calendar.h"
#include "vehicle_base.h"
#include "engine_base.h"
#include "language.h"
#include "townname_func.h"
#include "string_func.h"
#include "company_base.h"
#include "smallmap_gui.h"
#include "window_func.h"
#include "debug.h"
#include "game/game_text.hpp"
#include "network/network_content_gui.h"
#include "newgrf_engine.h"
#include "core/backup_type.hpp"
#include <stack>
#include <charconv>

#include "table/strings.h"
#include "table/control_codes.h"
#include "3rdparty/fmt/std.h"

#include "strings_internal.h"

#include "safeguards.h"

std::string _config_language_file;                ///< The file (name) stored in the configuration.
LanguageList _languages;                          ///< The actual list of language meta data.
const LanguageMetadata *_current_language = nullptr; ///< The currently loaded language.

TextDirection _current_text_dir; ///< Text direction of the currently selected language.

#ifdef WITH_ICU_I18N
std::unique_ptr<icu::Collator> _current_collator;    ///< Collator for the language currently in use.
#endif /* WITH_ICU_I18N */

ArrayStringParameters<20> _global_string_params;

/**
 * Prepare the string parameters for the next formatting run. This means
 * resetting the type information and resetting the offset to the begin.
 */
void StringParameters::PrepareForNextRun()
{
	for (auto &param : this->parameters) param.type = 0;
	this->offset = 0;
}


/**
 * Get the next parameter from our parameters.
 * This updates the offset, so the next time this is called the next parameter
 * will be read.
 * @return The pointer to the next parameter.
 */
StringParameter *StringParameters::GetNextParameterPointer()
{
	assert(this->next_type == 0 || (SCC_CONTROL_START <= this->next_type && this->next_type <= SCC_CONTROL_END));
	if (this->offset >= this->parameters.size()) {
		throw std::out_of_range("Trying to read invalid string parameter");
	}

	auto &param = this->parameters[this->offset++];
	if (param.type != 0 && param.type != this->next_type) {
		this->next_type = 0;
		throw std::out_of_range("Trying to read string parameter with wrong type");
	}
	param.type = this->next_type;
	this->next_type = 0;
	return &param;
}


/**
 * Set a string parameter \a v at index \a n in the global string parameter array.
 * @param n Index of the string parameter.
 * @param v Value of the string parameter.
 */
void SetDParam(size_t n, uint64_t v)
{
	_global_string_params.SetParam(n, v);
}

/**
 * Get the current string parameter at index \a n from the global string parameter array.
 * @param n Index of the string parameter.
 * @return Value of the requested string parameter.
 */
uint64_t GetDParam(size_t n)
{
	return _global_string_params.GetParam(n);
}

/**
 * Set DParam n to some number that is suitable for string size computations.
 * @param n Index of the string parameter.
 * @param max_value The biggest value which shall be displayed.
 *                  For the result only the number of digits of \a max_value matter.
 * @param min_count Minimum number of digits independent of \a max.
 * @param size  Font of the number
 */
void SetDParamMaxValue(size_t n, uint64_t max_value, uint min_count, FontSize size)
{
	uint num_digits = 1;
	while (max_value >= 10) {
		num_digits++;
		max_value /= 10;
	}
	SetDParamMaxDigits(n, std::max(min_count, num_digits), size);
}

/**
 * Set DParam n to some number that is suitable for string size computations.
 * @param n Index of the string parameter.
 * @param count Number of digits which shall be displayable.
 * @param size  Font of the number
 */
void SetDParamMaxDigits(size_t n, uint count, FontSize size)
{
	uint front = 0;
	uint next = 0;
	GetBroadestDigit(&front, &next, size);
	uint64_t val = count > 1 ? front : next;
	for (; count > 1; count--) {
		val = 10 * val + next;
	}
	SetDParam(n, val);
}

/**
 * Copy the parameters from the backup into the global string parameter array.
 * @param backup The backup to copy from.
 */
void CopyInDParam(const span<const StringParameterBackup> backup)
{
	for (size_t i = 0; i < backup.size(); i++) {
		auto &value = backup[i];
		if (value.string.has_value()) {
			_global_string_params.SetParam(i, value.string.value());
		} else {
			_global_string_params.SetParam(i, value.data);
		}
	}
}

/**
 * Copy \a num string parameters from the global string parameter array to the \a backup.
 * @param backup The backup to write to.
 * @param num Number of string parameters to copy.
 */
void CopyOutDParam(std::vector<StringParameterBackup> &backup, size_t num)
{
	backup.resize(num);
	for (size_t i = 0; i < backup.size(); i++) {
		const char *str = _global_string_params.GetParamStr(i);
		if (str != nullptr) {
			backup[i] = str;
		} else {
			backup[i] = _global_string_params.GetParam(i);
		}
	}
}

/**
 * Checks whether the global string parameters have changed compared to the given backup.
 * @param backup The backup to check against.
 * @return True when the parameters have changed, otherwise false.
 */
bool HaveDParamChanged(const std::vector<StringParameterBackup> &backup)
{
	bool changed = false;
	for (size_t i = 0; !changed && i < backup.size(); i++) {
		bool global_has_string = _global_string_params.GetParamStr(i) != nullptr;
		if (global_has_string != backup[i].string.has_value()) return true;

		if (global_has_string) {
			changed = backup[i].string.value() != _global_string_params.GetParamStr(i);
		} else {
			changed = backup[i].data != _global_string_params.GetParam(i);
		}
	}
	return changed;
}

static void StationGetSpecialString(StringBuilder &builder, StationFacility x);
static void GetSpecialTownNameString(StringBuilder &builder, int ind, uint32_t seed);
static void GetSpecialNameString(StringBuilder &builder, int ind, StringParameters &args);

static void FormatString(StringBuilder &builder, const char *str, StringParameters &args, uint case_index = 0, bool game_script = false, bool dry_run = false);

struct LanguagePack : public LanguagePackHeader {
	char data[]; // list of strings
};

struct LanguagePackDeleter {
	void operator()(LanguagePack *langpack)
	{
		/* LanguagePack is in fact reinterpreted char[], we need to reinterpret it back to free it properly. */
		delete[] reinterpret_cast<char*>(langpack);
	}
};

struct LoadedLanguagePack {
	std::unique_ptr<LanguagePack, LanguagePackDeleter> langpack;

	std::vector<char *> offsets;

	std::array<uint, TEXT_TAB_END> langtab_num;   ///< Offset into langpack offs
	std::array<uint, TEXT_TAB_END> langtab_start; ///< Offset into langpack offs
};

static LoadedLanguagePack _langpack;

static bool _scan_for_gender_data = false;  ///< Are we scanning for the gender of the current string? (instead of formatting it)


const char *GetStringPtr(StringID string)
{
	switch (GetStringTab(string)) {
		case TEXT_TAB_GAMESCRIPT_START: return GetGameStringPtr(GetStringIndex(string));
		/* 0xD0xx and 0xD4xx IDs have been converted earlier. */
		case TEXT_TAB_OLD_NEWGRF: NOT_REACHED();
		case TEXT_TAB_NEWGRF_START: return GetGRFStringPtr(GetStringIndex(string));
		default: return _langpack.offsets[_langpack.langtab_start[GetStringTab(string)] + GetStringIndex(string)];
	}
}

/**
 * Get a parsed string with most special stringcodes replaced by the string parameters.
 * @param builder     The builder of the string.
 * @param string      The ID of the string to parse.
 * @param args        Arguments for the string.
 * @param case_index  The "case index". This will only be set when FormatString wants to print the string in a different case.
 * @param game_script The string is coming directly from a game script.
 */
void GetStringWithArgs(StringBuilder &builder, StringID string, StringParameters &args, uint case_index, bool game_script)
{
	if (string == 0) {
		GetStringWithArgs(builder, STR_UNDEFINED, args);
		return;
	}

	uint index = GetStringIndex(string);
	StringTab tab = GetStringTab(string);

	switch (tab) {
		case TEXT_TAB_TOWN:
			if (index >= 0xC0 && !game_script) {
				GetSpecialTownNameString(builder, index - 0xC0, args.GetNextParameter<uint32_t>());
				return;
			}
			break;

		case TEXT_TAB_SPECIAL:
			if (index >= 0xE4 && !game_script) {
				GetSpecialNameString(builder, index - 0xE4, args);
				return;
			}
			break;

		case TEXT_TAB_OLD_CUSTOM:
			/* Old table for custom names. This is no longer used */
			if (!game_script) {
				FatalError("Incorrect conversion of custom name string.");
			}
			break;

		case TEXT_TAB_GAMESCRIPT_START: {
			FormatString(builder, GetGameStringPtr(index), args, case_index, true);
			return;
		}

		case TEXT_TAB_OLD_NEWGRF:
			NOT_REACHED();

		case TEXT_TAB_NEWGRF_START: {
			FormatString(builder, GetGRFStringPtr(index), args, case_index);
			return;
		}

		default:
			break;
	}

	if (index >= _langpack.langtab_num[tab]) {
		if (game_script) {
			return GetStringWithArgs(builder, STR_UNDEFINED, args);
		}
		FatalError("String 0x{:X} is invalid. You are probably using an old version of the .lng file.\n", string);
	}

	FormatString(builder, GetStringPtr(string), args, case_index);
}


/**
 * Resolve the given StringID into a std::string with all the associated
 * DParam lookups and formatting.
 * @param string The unique identifier of the translatable string.
 * @return The std::string of the translated string.
 */
std::string GetString(StringID string)
{
	_global_string_params.PrepareForNextRun();
	return GetStringWithArgs(string, _global_string_params);
}

/**
 * Get a parsed string with most special stringcodes replaced by the string parameters.
 * @param string The ID of the string to parse.
 * @param args   Arguments for the string.
 * @return The parsed string.
 */
std::string GetStringWithArgs(StringID string, StringParameters &args)
{
	std::string result;
	StringBuilder builder(result);
	GetStringWithArgs(builder, string, args);
	return result;
}

/**
 * This function is used to "bind" a C string to a OpenTTD dparam slot.
 * @param n slot of the string
 * @param str string to bind
 */
void SetDParamStr(size_t n, const char *str)
{
	_global_string_params.SetParam(n, str);
}

/**
 * This function is used to "bind" the C string of a std::string to a OpenTTD dparam slot.
 * The caller has to ensure that the std::string reference remains valid while the string is shown.
 * @param n slot of the string
 * @param str string to bind
 */
void SetDParamStr(size_t n, const std::string &str)
{
	_global_string_params.SetParam(n, str);
}

/**
 * This function is used to "bind" the std::string to a OpenTTD dparam slot.
 * Contrary to the other \c SetDParamStr functions, this moves the string into
 * the parameter slot.
 * @param n slot of the string
 * @param str string to bind
 */
void SetDParamStr(size_t n, std::string &&str)
{
	_global_string_params.SetParam(n, std::move(str));
}

/**
 * Format a number into a string.
 * @param builder   the string builder to write to
 * @param number    the number to write down
 * @param last      the last element in the buffer
 * @param separator the thousands-separator to use
 * @param zerofill  minimum number of digits to print for the integer part. The number will be filled with zeros at the front if necessary.
 * @param fractional_digits number of fractional digits to display after a decimal separator. The decimal separator is inserted
 *                          in front of the \a fractional_digits last digit of \a number.
 */
static void FormatNumber(StringBuilder &builder, int64_t number, const char *separator, int zerofill = 1, int fractional_digits = 0)
{
	static const int max_digits = 20;
	uint64_t divisor = 10000000000000000000ULL;
	zerofill += fractional_digits;
	int thousands_offset = (max_digits - fractional_digits - 1) % 3;

	if (number < 0) {
		builder += '-';
		number = -number;
	}

	uint64_t num = number;
	uint64_t tot = 0;
	for (int i = 0; i < max_digits; i++) {
		if (i == max_digits - fractional_digits) {
			const char *decimal_separator = _settings_game.locale.digit_decimal_separator.c_str();
			if (StrEmpty(decimal_separator)) decimal_separator = _langpack.langpack->digit_decimal_separator;
			builder += decimal_separator;
		}

		uint64_t quot = 0;
		if (num >= divisor) {
			quot = num / divisor;
			num = num % divisor;
		}
		if ((tot |= quot) || i >= max_digits - zerofill) {
			builder += '0' + quot; // quot is a single digit
			if ((i % 3) == thousands_offset && i < max_digits - 1 - fractional_digits) builder += separator;
		}

		divisor /= 10;
	}
}

static void FormatCommaNumber(StringBuilder &builder, int64_t number, int fractional_digits = 0)
{
	const char *separator = _settings_game.locale.digit_group_separator.c_str();
	if (StrEmpty(separator)) separator = _langpack.langpack->digit_group_separator;
	FormatNumber(builder, number, separator, 1, fractional_digits);
}

static void FormatNoCommaNumber(StringBuilder &builder, int64_t number)
{
	FormatNumber(builder, number, "");
}

static void FormatZerofillNumber(StringBuilder &builder, int64_t number, int count)
{
	FormatNumber(builder, number, "", count);
}

static void FormatHexNumber(StringBuilder &builder, uint64_t number)
{
	fmt::format_to(builder, "0x{:X}", number);
}

/**
 * Format a given number as a number of bytes with the SI prefix.
 * @param builder the string builder to write to
 * @param number  the number of bytes to write down
 */
static void FormatBytes(StringBuilder &builder, int64_t number)
{
	assert(number >= 0);

	/*                                   1   2^10  2^20  2^30  2^40  2^50  2^60 */
	const char * const iec_prefixes[] = {"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei"};
	uint id = 1;
	while (number >= 1024 * 1024) {
		number /= 1024;
		id++;
	}

	const char *decimal_separator = _settings_game.locale.digit_decimal_separator.c_str();
	if (StrEmpty(decimal_separator)) decimal_separator = _langpack.langpack->digit_decimal_separator;

	if (number < 1024) {
		id = 0;
		fmt::format_to(builder, "{}", number);
	} else if (number < 1024 * 10) {
		fmt::format_to(builder, "{}{}{:02}", number / 1024, decimal_separator, (number % 1024) * 100 / 1024);
	} else if (number < 1024 * 100) {
		fmt::format_to(builder, "{}{}{:01}", number / 1024, decimal_separator, (number % 1024) * 10 / 1024);
	} else {
		assert(number < 1024 * 1024);
		fmt::format_to(builder, "{}", number / 1024);
	}

	assert(id < lengthof(iec_prefixes));
	fmt::format_to(builder, NBSP "{}B", iec_prefixes[id]);
}

static void FormatYmdString(StringBuilder &builder, TimerGameCalendar::Date date, uint case_index)
{
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(date);

	auto tmp_params = MakeParameters(ymd.day + STR_DAY_NUMBER_1ST - 1, STR_MONTH_ABBREV_JAN + ymd.month, ymd.year);
	FormatString(builder, GetStringPtr(STR_FORMAT_DATE_LONG), tmp_params, case_index);
}

static void FormatMonthAndYear(StringBuilder &builder, TimerGameCalendar::Date date, uint case_index)
{
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(date);

	auto tmp_params = MakeParameters(STR_MONTH_JAN + ymd.month, ymd.year);
	FormatString(builder, GetStringPtr(STR_FORMAT_DATE_SHORT), tmp_params, case_index);
}

static void FormatTinyOrISODate(StringBuilder &builder, TimerGameCalendar::Date date, StringID str)
{
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(date);

	/* Day and month are zero-padded with ZEROFILL_NUM, hence the two 2s. */
	auto tmp_params = MakeParameters(ymd.day, 2, ymd.month + 1, 2, ymd.year);
	FormatString(builder, GetStringPtr(str), tmp_params);
}

static void FormatGenericCurrency(StringBuilder &builder, const CurrencySpec *spec, Money number, bool compact)
{
	/* We are going to make number absolute for printing, so
	 * keep this piece of data as we need it later on */
	bool negative = number < 0;
	const char *multiplier = "";

	number *= spec->rate;

	/* convert from negative */
	if (number < 0) {
		builder.Utf8Encode(SCC_PUSH_COLOUR);
		builder.Utf8Encode(SCC_RED);
		builder += '-';
		number = -number;
	}

	/* Add prefix part, following symbol_pos specification.
	 * Here, it can can be either 0 (prefix) or 2 (both prefix and suffix).
	 * The only remaining value is 1 (suffix), so everything that is not 1 */
	if (spec->symbol_pos != 1) builder += spec->prefix;

	/* for huge numbers, compact the number into k or M */
	if (compact) {
		/* Take care of the 'k' rounding. Having 1 000 000 k
		 * and 1 000 M is inconsistent, so always use 1 000 M. */
		if (number >= 1000000000 - 500) {
			number = (number + 500000) / 1000000;
			multiplier = NBSP "M";
		} else if (number >= 1000000) {
			number = (number + 500) / 1000;
			multiplier = NBSP "k";
		}
	}

	const char *separator = _settings_game.locale.digit_group_separator_currency.c_str();
	if (StrEmpty(separator)) separator = _currency->separator.c_str();
	if (StrEmpty(separator)) separator = _langpack.langpack->digit_group_separator_currency;
	FormatNumber(builder, number, separator);
	builder += multiplier;

	/* Add suffix part, following symbol_pos specification.
	 * Here, it can can be either 1 (suffix) or 2 (both prefix and suffix).
	 * The only remaining value is 1 (prefix), so everything that is not 0 */
	if (spec->symbol_pos != 0) builder += spec->suffix;

	if (negative) {
		builder.Utf8Encode(SCC_POP_COLOUR);
	}
}

/**
 * Determine the "plural" index given a plural form and a number.
 * @param count       The number to get the plural index of.
 * @param plural_form The plural form we want an index for.
 * @return The plural index for the given form.
 */
static int DeterminePluralForm(int64_t count, int plural_form)
{
	/* The absolute value determines plurality */
	uint64_t n = abs(count);

	switch (plural_form) {
		default:
			NOT_REACHED();

		/* Two forms: singular used for one only.
		 * Used in:
		 *   Danish, Dutch, English, German, Norwegian, Swedish, Estonian, Finnish,
		 *   Greek, Hebrew, Italian, Portuguese, Spanish, Esperanto */
		case 0:
			return n != 1 ? 1 : 0;

		/* Only one form.
		 * Used in:
		 *   Hungarian, Japanese, Turkish */
		case 1:
			return 0;

		/* Two forms: singular used for 0 and 1.
		 * Used in:
		 *   French, Brazilian Portuguese */
		case 2:
			return n > 1 ? 1 : 0;

		/* Three forms: special cases for 0, and numbers ending in 1 except when ending in 11.
		 * Note: Cases are out of order for hysterical reasons. '0' is last.
		 * Used in:
		 *   Latvian */
		case 3:
			return n % 10 == 1 && n % 100 != 11 ? 0 : n != 0 ? 1 : 2;

		/* Five forms: special cases for 1, 2, 3 to 6, and 7 to 10.
		 * Used in:
		 *   Gaelige (Irish) */
		case 4:
			return n == 1 ? 0 : n == 2 ? 1 : n < 7 ? 2 : n < 11 ? 3 : 4;

		/* Three forms: special cases for numbers ending in 1 except when ending in 11, and 2 to 9 except when ending in 12 to 19.
		 * Used in:
		 *   Lithuanian */
		case 5:
			return n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;

		/* Three forms: special cases for numbers ending in 1 except when ending in 11, and 2 to 4 except when ending in 12 to 14.
		 * Used in:
		 *   Croatian, Russian, Ukrainian */
		case 6:
			return n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;

		/* Three forms: special cases for 1, and numbers ending in 2 to 4 except when ending in 12 to 14.
		 * Used in:
		 *   Polish */
		case 7:
			return n == 1 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2;

		/* Four forms: special cases for numbers ending in 01, 02, and 03 to 04.
		 * Used in:
		 *   Slovenian */
		case 8:
			return n % 100 == 1 ? 0 : n % 100 == 2 ? 1 : n % 100 == 3 || n % 100 == 4 ? 2 : 3;

		/* Two forms: singular used for numbers ending in 1 except when ending in 11.
		 * Used in:
		 *   Icelandic */
		case 9:
			return n % 10 == 1 && n % 100 != 11 ? 0 : 1;

		/* Three forms: special cases for 1, and 2 to 4
		 * Used in:
		 *   Czech, Slovak */
		case 10:
			return n == 1 ? 0 : n >= 2 && n <= 4 ? 1 : 2;

		/* Two forms: cases for numbers ending with a consonant, and with a vowel.
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

		/* Four forms: special cases for 1, 0 and numbers ending in 02 to 10, and numbers ending in 11 to 19.
		 * Used in:
		 *  Maltese */
		case 12:
			return (n == 1 ? 0 : n == 0 || (n % 100 > 1 && n % 100 < 11) ? 1 : (n % 100 > 10 && n % 100 < 20) ? 2 : 3);
		/* Four forms: special cases for 1 and 11, 2 and 12, 3 .. 10 and 13 .. 19, other
		 * Used in:
		 *  Scottish Gaelic */
		case 13:
			return ((n == 1 || n == 11) ? 0 : (n == 2 || n == 12) ? 1 : ((n > 2 && n < 11) || (n > 12 && n < 20)) ? 2 : 3);

		/* Three forms: special cases for 1, 0 and numbers ending in 01 to 19.
		 * Used in:
		 *   Romanian */
		case 14:
			return n == 1 ? 0 : (n == 0 || (n % 100 > 0 && n % 100 < 20)) ? 1 : 2;
	}
}

static const char *ParseStringChoice(const char *b, uint form, StringBuilder &builder)
{
	/* <NUM> {Length of each string} {each string} */
	uint n = (byte)*b++;
	uint pos, i, mypos = 0;

	for (i = pos = 0; i != n; i++) {
		uint len = (byte)*b++;
		if (i == form) mypos = pos;
		pos += len;
	}

	builder += b + mypos;
	return b + pos;
}

/** Helper for unit conversion. */
struct UnitConversion {
	double factor; ///< Amount to multiply or divide upon conversion.

	/**
	 * Convert value from OpenTTD's internal unit into the displayed value.
	 * @param input The input to convert.
	 * @param round Whether to round the value or not.
	 * @return The converted value.
	 */
	int64_t ToDisplay(int64_t input, bool round = true) const
	{
		return round
			? (int64_t)std::round(input * this->factor)
			: (int64_t)(input * this->factor);
	}

	/**
	 * Convert the displayed value back into a value of OpenTTD's internal unit.
	 * @param input The input to convert.
	 * @param round Whether to round the value up or not.
	 * @param divider Divide the return value by this.
	 * @return The converted value.
	 */
	int64_t FromDisplay(int64_t input, bool round = true, int64_t divider = 1) const
	{
		return round
			? (int64_t)std::round(input / this->factor / divider)
			: (int64_t)(input / this->factor / divider);
	}
};

/** Information about a specific unit system. */
struct Units {
	UnitConversion c; ///< Conversion
	StringID s;       ///< String for the unit
	unsigned int decimal_places; ///< Number of decimal places embedded in the value. For example, 1 if the value is in tenths, and 3 if the value is in thousandths.
};

/** Information about a specific unit system with a long variant. */
struct UnitsLong {
	UnitConversion c; ///< Conversion
	StringID s;       ///< String for the short variant of the unit
	StringID l;       ///< String for the long variant of the unit
	unsigned int decimal_places; ///< Number of decimal places embedded in the value. For example, 1 if the value is in tenths, and 3 if the value is in thousandths.
};

/** Unit conversions for velocity. */
static const Units _units_velocity[] = {
	{ { 1.0      }, STR_UNITS_VELOCITY_IMPERIAL,  0 },
	{ { 1.609344 }, STR_UNITS_VELOCITY_METRIC,    0 },
	{ { 0.44704  }, STR_UNITS_VELOCITY_SI,        0 },
	{ { 0.578125 }, STR_UNITS_VELOCITY_GAMEUNITS, 1 },
	{ { 0.868976 }, STR_UNITS_VELOCITY_KNOTS,     0 },
};

/** Unit conversions for power. */
static const Units _units_power[] = {
	{ { 1.0      }, STR_UNITS_POWER_IMPERIAL, 0 },
	{ { 1.01387  }, STR_UNITS_POWER_METRIC,   0 },
	{ { 0.745699 }, STR_UNITS_POWER_SI,       0 },
};

/** Unit conversions for power to weight. */
static const Units _units_power_to_weight[] = {
	{ { 0.907185 }, STR_UNITS_POWER_IMPERIAL_TO_WEIGHT_IMPERIAL, 1 },
	{ { 1.0      }, STR_UNITS_POWER_IMPERIAL_TO_WEIGHT_METRIC,   1 },
	{ { 1.0      }, STR_UNITS_POWER_IMPERIAL_TO_WEIGHT_SI,       1 },
	{ { 0.919768 }, STR_UNITS_POWER_METRIC_TO_WEIGHT_IMPERIAL,   1 },
	{ { 1.01387  }, STR_UNITS_POWER_METRIC_TO_WEIGHT_METRIC,     1 },
	{ { 1.01387  }, STR_UNITS_POWER_METRIC_TO_WEIGHT_SI,         1 },
	{ { 0.676487 }, STR_UNITS_POWER_SI_TO_WEIGHT_IMPERIAL,       1 },
	{ { 0.745699 }, STR_UNITS_POWER_SI_TO_WEIGHT_METRIC,         1 },
	{ { 0.745699 }, STR_UNITS_POWER_SI_TO_WEIGHT_SI,             1 },
};

/** Unit conversions for weight. */
static const UnitsLong _units_weight[] = {
	{ {    1.102311 }, STR_UNITS_WEIGHT_SHORT_IMPERIAL, STR_UNITS_WEIGHT_LONG_IMPERIAL, 0 },
	{ {    1.0      }, STR_UNITS_WEIGHT_SHORT_METRIC,   STR_UNITS_WEIGHT_LONG_METRIC,   0 },
	{ { 1000.0      }, STR_UNITS_WEIGHT_SHORT_SI,       STR_UNITS_WEIGHT_LONG_SI,       0 },
};

/** Unit conversions for volume. */
static const UnitsLong _units_volume[] = {
	{ {  264.172 }, STR_UNITS_VOLUME_SHORT_IMPERIAL, STR_UNITS_VOLUME_LONG_IMPERIAL, 0 },
	{ { 1000.0   }, STR_UNITS_VOLUME_SHORT_METRIC,   STR_UNITS_VOLUME_LONG_METRIC,   0 },
	{ {    1.0   }, STR_UNITS_VOLUME_SHORT_SI,       STR_UNITS_VOLUME_LONG_SI,       0 },
};

/** Unit conversions for force. */
static const Units _units_force[] = {
	{ { 0.224809 }, STR_UNITS_FORCE_IMPERIAL, 0 },
	{ { 0.101972 }, STR_UNITS_FORCE_METRIC,   0 },
	{ { 0.001    }, STR_UNITS_FORCE_SI,       0 },
};

/** Unit conversions for height. */
static const Units _units_height[] = {
	{ { 3.0 }, STR_UNITS_HEIGHT_IMPERIAL, 0 }, // "Wrong" conversion factor for more nicer GUI values
	{ { 1.0 }, STR_UNITS_HEIGHT_METRIC,   0 },
	{ { 1.0 }, STR_UNITS_HEIGHT_SI,       0 },
};

/**
 * Get index for velocity conversion units for a vehicle type.
 * @param type VehicleType to convert velocity for.
 * @return Index within velocity conversion units for vehicle type.
 */
static byte GetVelocityUnits(VehicleType type)
{
	if (type == VEH_SHIP || type == VEH_AIRCRAFT) return _settings_game.locale.units_velocity_nautical;

	return _settings_game.locale.units_velocity;
}

/**
 * Convert the given (internal) speed to the display speed.
 * @param speed the speed to convert
 * @return the converted speed.
 */
uint ConvertSpeedToDisplaySpeed(uint speed, VehicleType type)
{
	/* For historical reasons we don't want to mess with the
	 * conversion for speed. So, don't round it and keep the
	 * original conversion factors instead of the real ones. */
	return _units_velocity[GetVelocityUnits(type)].c.ToDisplay(speed, false);
}

/**
 * Convert the given display speed to the (internal) speed.
 * @param speed the speed to convert
 * @return the converted speed.
 */
uint ConvertDisplaySpeedToSpeed(uint speed, VehicleType type)
{
	return _units_velocity[GetVelocityUnits(type)].c.FromDisplay(speed);
}

/**
 * Convert the given km/h-ish speed to the display speed.
 * @param speed the speed to convert
 * @return the converted speed.
 */
uint ConvertKmhishSpeedToDisplaySpeed(uint speed, VehicleType type)
{
	return _units_velocity[GetVelocityUnits(type)].c.ToDisplay(speed * 10, false) / 16;
}

/**
 * Convert the given display speed to the km/h-ish speed.
 * @param speed the speed to convert
 * @return the converted speed.
 */
uint ConvertDisplaySpeedToKmhishSpeed(uint speed, VehicleType type)
{
	return _units_velocity[GetVelocityUnits(type)].c.FromDisplay(speed * 16, true, 10);
}

/**
 * Parse most format codes within a string and write the result to a buffer.
 * @param builder The string builder to write the final string to.
 * @param str_arg The original string with format codes.
 * @param args    Pointer to extra arguments used by various string codes.
 * @param dry_run True when the args' type data is not yet initialized.
 */
static void FormatString(StringBuilder &builder, const char *str_arg, StringParameters &args, uint case_index, bool game_script, bool dry_run)
{
	size_t orig_offset = args.GetOffset();

	if (!dry_run) {
		/*
		 * This function is normally called with `dry_run` false, then we call this function again
		 * with `dry_run` being true. The dry run is required for the gender formatting. For the
		 * gender determination we need to format a sub string to get the gender, but for that we
		 * need to know as what string control code type the specific parameter is encoded. Since
		 * gendered words can be before the "parameter" words, this needs to be determined before
		 * the actual formatting.
		 */
		std::string buffer;
		StringBuilder dry_run_builder(buffer);
		if (UsingNewGRFTextStack()) {
			/* Values from the NewGRF text stack are only copied to the normal
			 * argv array at the time they are encountered. That means that if
			 * another string command references a value later in the string it
			 * would fail. We solve that by running FormatString twice. The first
			 * pass makes sure the argv array is correctly filled and the second
			 * pass can reference later values without problems. */
			struct TextRefStack *backup = CreateTextRefStackBackup();
			FormatString(dry_run_builder, str_arg, args, case_index, game_script, true);
			RestoreTextRefStackBackup(backup);
		} else {
			FormatString(dry_run_builder, str_arg, args, case_index, game_script, true);
		}
		/* We have to restore the original offset here to to read the correct values. */
		args.SetOffset(orig_offset);
	}
	char32_t b = '\0';
	uint next_substr_case_index = 0;
	std::stack<const char *, std::vector<const char *>> str_stack;
	str_stack.push(str_arg);

	for (;;) {
		try {
			while (!str_stack.empty() && (b = Utf8Consume(&str_stack.top())) == '\0') {
				str_stack.pop();
			}
			if (str_stack.empty()) break;
			const char *&str = str_stack.top();

			if (SCC_NEWGRF_FIRST <= b && b <= SCC_NEWGRF_LAST) {
				/* We need to pass some stuff as it might be modified. */
				StringParameters remaining = args.GetRemainingParameters();
				b = RemapNewGRFStringControlCode(b, &str, remaining, dry_run);
				if (b == 0) continue;
			}

			if (b < SCC_CONTROL_START || b > SCC_CONTROL_END) {
				builder.Utf8Encode(b);
				continue;
			}

			args.SetTypeOfNextParameter(b);
			switch (b) {
				case SCC_ENCODED: {
					ArrayStringParameters<20> sub_args;

					char *p;
					uint32_t stringid = std::strtoul(str, &p, 16);
					if (*p != ':' && *p != '\0') {
						while (*p != '\0') p++;
						str = p;
						builder += "(invalid SCC_ENCODED)";
						break;
					}
					if (stringid >= TAB_SIZE_GAMESCRIPT) {
						while (*p != '\0') p++;
						str = p;
						builder += "(invalid StringID)";
						break;
					}

					int i = 0;
					while (*p != '\0' && i < 20) {
						uint64_t param;
						const char *s = ++p;

						/* Find the next value */
						bool instring = false;
						bool escape = false;
						for (;; p++) {
							if (*p == '\\') {
								escape = true;
								continue;
							}
							if (*p == '"' && escape) {
								escape = false;
								continue;
							}
							escape = false;

							if (*p == '"') {
								instring = !instring;
								continue;
							}
							if (instring) {
								continue;
							}

							if (*p == ':') break;
							if (*p == '\0') break;
						}

						if (*s != '"') {
							/* Check if we want to look up another string */
							char32_t l;
							size_t len = Utf8Decode(&l, s);
							bool lookup = (l == SCC_ENCODED);
							if (lookup) s += len;

							param = std::strtoull(s, &p, 16);

							if (lookup) {
								if (param >= TAB_SIZE_GAMESCRIPT) {
									while (*p != '\0') p++;
									str = p;
									builder += "(invalid sub-StringID)";
									break;
								}
								param = MakeStringID(TEXT_TAB_GAMESCRIPT_START, param);
							}

							sub_args.SetParam(i++, param);
						} else {
							s++; // skip the leading \"
							sub_args.SetParam(i++, std::string(s, p - s - 1)); // also skip the trailing \".
						}
					}
					/* If we didn't error out, we can actually print the string. */
					if (*str != '\0') {
						str = p;
						GetStringWithArgs(builder, MakeStringID(TEXT_TAB_GAMESCRIPT_START, stringid), sub_args, true);
					}
					break;
				}

				case SCC_NEWGRF_STRINL: {
					StringID substr = Utf8Consume(&str);
					str_stack.push(GetStringPtr(substr));
					break;
				}

				case SCC_NEWGRF_PRINT_WORD_STRING_ID: {
					StringID substr = args.GetNextParameter<StringID>();
					str_stack.push(GetStringPtr(substr));
					case_index = next_substr_case_index;
					next_substr_case_index = 0;
					break;
				}


				case SCC_GENDER_LIST: { // {G 0 Der Die Das}
					/* First read the meta data from the language file. */
					size_t offset = orig_offset + (byte)*str++;
					int gender = 0;
					if (!dry_run && args.GetTypeAtOffset(offset) != 0) {
						/* Now we need to figure out what text to resolve, i.e.
						 * what do we need to draw? So get the actual raw string
						 * first using the control code to get said string. */
						char input[4 + 1];
						char *p = input + Utf8Encode(input, args.GetTypeAtOffset(offset));
						*p = '\0';

						/* The gender is stored at the start of the formatted string. */
						bool old_sgd = _scan_for_gender_data;
						_scan_for_gender_data = true;
						std::string buffer;
						StringBuilder tmp_builder(buffer);
						StringParameters tmp_params = args.GetRemainingParameters(offset);
						FormatString(tmp_builder, input, tmp_params);
						_scan_for_gender_data = old_sgd;

						/* And determine the string. */
						const char *s = buffer.c_str();
						char32_t c = Utf8Consume(&s);
						/* Does this string have a gender, if so, set it */
						if (c == SCC_GENDER_INDEX) gender = (byte)s[0];
					}
					str = ParseStringChoice(str, gender, builder);
					break;
				}

				/* This sets up the gender for the string.
				 * We just ignore this one. It's used in {G 0 Der Die Das} to determine the case. */
				case SCC_GENDER_INDEX: // {GENDER 0}
					if (_scan_for_gender_data) {
						builder.Utf8Encode(SCC_GENDER_INDEX);
						builder += *str++;
					} else {
						str++;
					}
					break;

				case SCC_PLURAL_LIST: { // {P}
					int plural_form = *str++;          // contains the plural form for this string
					size_t offset = orig_offset + (byte)*str++;
					int64_t v = args.GetParam(offset); // contains the number that determines plural
					str = ParseStringChoice(str, DeterminePluralForm(v, plural_form), builder);
					break;
				}

				case SCC_ARG_INDEX: { // Move argument pointer
					args.SetOffset(orig_offset + (byte)*str++);
					break;
				}

				case SCC_SET_CASE: { // {SET_CASE}
					/* This is a pseudo command, it's outputted when someone does {STRING.ack}
					 * The modifier is added to all subsequent GetStringWithArgs that accept the modifier. */
					next_substr_case_index = (byte)*str++;
					break;
				}

				case SCC_SWITCH_CASE: { // {Used to implement case switching}
					/* <0x9E> <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
					 * Each LEN is printed using 2 bytes in big endian order. */
					uint num = (byte)*str++;
					while (num) {
						if ((byte)str[0] == case_index) {
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

				case SCC_REVISION: // {REV}
					builder += _openttd_revision;
					break;

				case SCC_RAW_STRING_POINTER: { // {RAW_STRING}
					const char *raw_string = args.GetNextParameterString();
					/* raw_string can be nullptr. */
					if (raw_string == nullptr) {
						builder += "(invalid RAW_STRING parameter)";
						break;
					}
					FormatString(builder, raw_string, args);
					break;
				}

				case SCC_STRING: {// {STRING}
					StringID string_id = args.GetNextParameter<StringID>();
					if (game_script && GetStringTab(string_id) != TEXT_TAB_GAMESCRIPT_START) break;
					/* It's prohibited for the included string to consume any arguments. */
					StringParameters tmp_params(args, game_script ? args.GetDataLeft() : 0);
					GetStringWithArgs(builder, string_id, tmp_params, next_substr_case_index, game_script);
					next_substr_case_index = 0;
					break;
				}

				case SCC_STRING1:
				case SCC_STRING2:
				case SCC_STRING3:
				case SCC_STRING4:
				case SCC_STRING5:
				case SCC_STRING6:
				case SCC_STRING7: { // {STRING1..7}
					/* Strings that consume arguments */
					StringID string_id = args.GetNextParameter<StringID>();
					if (game_script && GetStringTab(string_id) != TEXT_TAB_GAMESCRIPT_START) break;
					uint size = b - SCC_STRING1 + 1;
					if (game_script && size > args.GetDataLeft()) {
						builder += "(too many parameters)";
					} else {
						StringParameters sub_args(args, game_script ? args.GetDataLeft() : size);
						GetStringWithArgs(builder, string_id, sub_args, next_substr_case_index, game_script);
						args.AdvanceOffset(size);
					}
					next_substr_case_index = 0;
					break;
				}

				case SCC_COMMA: // {COMMA}
					FormatCommaNumber(builder, args.GetNextParameter<int64_t>());
					break;

				case SCC_DECIMAL: { // {DECIMAL}
					int64_t number = args.GetNextParameter<int64_t>();
					int digits = args.GetNextParameter<int>();
					FormatCommaNumber(builder, number, digits);
					break;
				}

				case SCC_NUM: // {NUM}
					FormatNoCommaNumber(builder, args.GetNextParameter<int64_t>());
					break;

				case SCC_ZEROFILL_NUM: { // {ZEROFILL_NUM}
					int64_t num = args.GetNextParameter<int64_t>();
					FormatZerofillNumber(builder, num, args.GetNextParameter<int>());
					break;
				}

				case SCC_HEX: // {HEX}
					FormatHexNumber(builder, args.GetNextParameter<uint64_t>());
					break;

				case SCC_BYTES: // {BYTES}
					FormatBytes(builder, args.GetNextParameter<int64_t>());
					break;

				case SCC_CARGO_TINY: { // {CARGO_TINY}
					/* Tiny description of cargotypes. Layout:
					 * param 1: cargo type
					 * param 2: cargo count */
					CargoID cargo = args.GetNextParameter<CargoID>();
					if (cargo >= CargoSpec::GetArraySize()) break;

					StringID cargo_str = CargoSpec::Get(cargo)->units_volume;
					int64_t amount = 0;
					switch (cargo_str) {
						case STR_TONS:
							amount = _units_weight[_settings_game.locale.units_weight].c.ToDisplay(args.GetNextParameter<int64_t>());
							break;

						case STR_LITERS:
							amount = _units_volume[_settings_game.locale.units_volume].c.ToDisplay(args.GetNextParameter<int64_t>());
							break;

						default: {
							amount = args.GetNextParameter<int64_t>();
							break;
						}
					}

					FormatCommaNumber(builder, amount);
					break;
				}

				case SCC_CARGO_SHORT: { // {CARGO_SHORT}
					/* Short description of cargotypes. Layout:
					 * param 1: cargo type
					 * param 2: cargo count */
					CargoID cargo = args.GetNextParameter<CargoID>();
					if (cargo >= CargoSpec::GetArraySize()) break;

					StringID cargo_str = CargoSpec::Get(cargo)->units_volume;
					switch (cargo_str) {
						case STR_TONS: {
							assert(_settings_game.locale.units_weight < lengthof(_units_weight));
							const auto &x = _units_weight[_settings_game.locale.units_weight];
							auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
							FormatString(builder, GetStringPtr(x.l), tmp_params);
							break;
						}

						case STR_LITERS: {
							assert(_settings_game.locale.units_volume < lengthof(_units_volume));
							const auto &x = _units_volume[_settings_game.locale.units_volume];
							auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
							FormatString(builder, GetStringPtr(x.l), tmp_params);
							break;
						}

						default: {
							auto tmp_params = MakeParameters(args.GetNextParameter<int64_t>());
							GetStringWithArgs(builder, cargo_str, tmp_params);
							break;
						}
					}
					break;
				}

				case SCC_CARGO_LONG: { // {CARGO_LONG}
					/* First parameter is cargo type, second parameter is cargo count */
					CargoID cargo = args.GetNextParameter<CargoID>();
					if (IsValidCargoID(cargo) && cargo >= CargoSpec::GetArraySize()) break;

					StringID cargo_str = !IsValidCargoID(cargo) ? STR_QUANTITY_N_A : CargoSpec::Get(cargo)->quantifier;
					auto tmp_args = MakeParameters(args.GetNextParameter<int64_t>());
					GetStringWithArgs(builder, cargo_str, tmp_args);
					break;
				}

				case SCC_CARGO_LIST: { // {CARGO_LIST}
					CargoTypes cmask = args.GetNextParameter<CargoTypes>();
					bool first = true;

					for (const auto &cs : _sorted_cargo_specs) {
						if (!HasBit(cmask, cs->Index())) continue;

						if (first) {
							first = false;
						} else {
							/* Add a comma if this is not the first item */
							builder += ", ";
						}

						GetStringWithArgs(builder, cs->name, args, next_substr_case_index, game_script);
					}

					/* If first is still true then no cargo is accepted */
					if (first) GetStringWithArgs(builder, STR_JUST_NOTHING, args, next_substr_case_index, game_script);

					next_substr_case_index = 0;
					break;
				}

				case SCC_CURRENCY_SHORT: // {CURRENCY_SHORT}
					FormatGenericCurrency(builder, _currency, args.GetNextParameter<int64_t>(), true);
					break;

				case SCC_CURRENCY_LONG: // {CURRENCY_LONG}
					FormatGenericCurrency(builder, _currency, args.GetNextParameter<int64_t>(), false);
					break;

				case SCC_DATE_TINY: // {DATE_TINY}
					FormatTinyOrISODate(builder, args.GetNextParameter<TimerGameCalendar::Date>(), STR_FORMAT_DATE_TINY);
					break;

				case SCC_DATE_SHORT: // {DATE_SHORT}
					FormatMonthAndYear(builder, args.GetNextParameter<TimerGameCalendar::Date>(), next_substr_case_index);
					next_substr_case_index = 0;
					break;

				case SCC_DATE_LONG: // {DATE_LONG}
					FormatYmdString(builder, args.GetNextParameter<TimerGameCalendar::Date>(), next_substr_case_index);
					next_substr_case_index = 0;
					break;

				case SCC_DATE_ISO: // {DATE_ISO}
					FormatTinyOrISODate(builder, args.GetNextParameter<TimerGameCalendar::Date>(), STR_FORMAT_DATE_ISO);
					break;

				case SCC_FORCE: { // {FORCE}
					assert(_settings_game.locale.units_force < lengthof(_units_force));
					const auto &x = _units_force[_settings_game.locale.units_force];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_HEIGHT: { // {HEIGHT}
					assert(_settings_game.locale.units_height < lengthof(_units_height));
					const auto &x = _units_height[_settings_game.locale.units_height];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_POWER: { // {POWER}
					assert(_settings_game.locale.units_power < lengthof(_units_power));
					const auto &x = _units_power[_settings_game.locale.units_power];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_POWER_TO_WEIGHT: { // {POWER_TO_WEIGHT}
					auto setting = _settings_game.locale.units_power * 3u + _settings_game.locale.units_weight;
					assert(setting < lengthof(_units_power_to_weight));
					const auto &x = _units_power_to_weight[setting];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_VELOCITY: { // {VELOCITY}
					int64_t arg = args.GetNextParameter<int64_t>();
					// Unpack vehicle type from packed argument to get desired units.
					VehicleType vt = static_cast<VehicleType>(GB(arg, 56, 8));
					byte units = GetVelocityUnits(vt);
					assert(units < lengthof(_units_velocity));
					const auto &x = _units_velocity[units];
					auto tmp_params = MakeParameters(ConvertKmhishSpeedToDisplaySpeed(GB(arg, 0, 56), vt), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_VOLUME_SHORT: { // {VOLUME_SHORT}
					assert(_settings_game.locale.units_volume < lengthof(_units_volume));
					const auto &x = _units_volume[_settings_game.locale.units_volume];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_VOLUME_LONG: { // {VOLUME_LONG}
					assert(_settings_game.locale.units_volume < lengthof(_units_volume));
					const auto &x = _units_volume[_settings_game.locale.units_volume];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.l), tmp_params);
					break;
				}

				case SCC_WEIGHT_SHORT: { // {WEIGHT_SHORT}
					assert(_settings_game.locale.units_weight < lengthof(_units_weight));
					const auto &x = _units_weight[_settings_game.locale.units_weight];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.s), tmp_params);
					break;
				}

				case SCC_WEIGHT_LONG: { // {WEIGHT_LONG}
					assert(_settings_game.locale.units_weight < lengthof(_units_weight));
					const auto &x = _units_weight[_settings_game.locale.units_weight];
					auto tmp_params = MakeParameters(x.c.ToDisplay(args.GetNextParameter<int64_t>()), x.decimal_places);
					FormatString(builder, GetStringPtr(x.l), tmp_params);
					break;
				}

				case SCC_COMPANY_NAME: { // {COMPANY}
					const Company *c = Company::GetIfValid(args.GetNextParameter<CompanyID>());
					if (c == nullptr) break;

					if (!c->name.empty()) {
						auto tmp_params = MakeParameters(c->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						auto tmp_params = MakeParameters(c->name_2);
						GetStringWithArgs(builder, c->name_1, tmp_params);
					}
					break;
				}

				case SCC_COMPANY_NUM: { // {COMPANY_NUM}
					CompanyID company = args.GetNextParameter<CompanyID>();

					/* Nothing is added for AI or inactive companies */
					if (Company::IsValidHumanID(company)) {
						auto tmp_params = MakeParameters(company + 1);
						GetStringWithArgs(builder, STR_FORMAT_COMPANY_NUM, tmp_params);
					}
					break;
				}

				case SCC_DEPOT_NAME: { // {DEPOT}
					VehicleType vt = args.GetNextParameter<VehicleType>();
					if (vt == VEH_AIRCRAFT) {
						auto tmp_params = MakeParameters(args.GetNextParameter<StationID>());
						GetStringWithArgs(builder, STR_FORMAT_DEPOT_NAME_AIRCRAFT, tmp_params);
						break;
					}

					const Depot *d = Depot::Get(args.GetNextParameter<DepotID>());
					if (!d->name.empty()) {
						auto tmp_params = MakeParameters(d->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						auto tmp_params = MakeParameters(d->town->index, d->town_cn + 1);
						GetStringWithArgs(builder, STR_FORMAT_DEPOT_NAME_TRAIN + 2 * vt + (d->town_cn == 0 ? 0 : 1), tmp_params);
					}
					break;
				}

				case SCC_ENGINE_NAME: { // {ENGINE}
					int64_t arg = args.GetNextParameter<int64_t>();
					const Engine *e = Engine::GetIfValid(static_cast<EngineID>(arg));
					if (e == nullptr) break;

					if (!e->name.empty() && e->IsEnabled()) {
						auto tmp_params = MakeParameters(e->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
						break;
					}

					if (HasBit(e->info.callback_mask, CBM_VEHICLE_NAME)) {
						uint16_t callback = GetVehicleCallback(CBID_VEHICLE_NAME, static_cast<uint32_t>(arg >> 32), 0, e->index, nullptr);
						/* Not calling ErrorUnknownCallbackResult due to being inside string processing. */
						if (callback != CALLBACK_FAILED && callback < 0x400) {
							const GRFFile *grffile = e->GetGRF();
							assert(grffile != nullptr);

							StartTextRefStackUsage(grffile, 6);
							ArrayStringParameters<6> tmp_params;
							GetStringWithArgs(builder, GetGRFStringID(grffile->grfid, 0xD000 + callback), tmp_params);
							StopTextRefStackUsage();

							break;
						}
					}

					auto tmp_params = ArrayStringParameters<0>();
					GetStringWithArgs(builder, e->info.string_id, tmp_params);
					break;
				}

				case SCC_GROUP_NAME: { // {GROUP}
					const Group *g = Group::GetIfValid(args.GetNextParameter<GroupID>());
					if (g == nullptr) break;

					if (!g->name.empty()) {
						auto tmp_params = MakeParameters(g->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						auto tmp_params = MakeParameters(g->index);
						GetStringWithArgs(builder, STR_FORMAT_GROUP_NAME, tmp_params);
					}
					break;
				}

				case SCC_INDUSTRY_NAME: { // {INDUSTRY}
					const Industry *i = Industry::GetIfValid(args.GetNextParameter<IndustryID>());
					if (i == nullptr) break;

					static bool use_cache = true;
					if (use_cache) { // Use cached version if first call
						AutoRestoreBackup cache_backup(use_cache, false);
						builder += i->GetCachedName();
					} else if (_scan_for_gender_data) {
						/* Gender is defined by the industry type.
						 * STR_FORMAT_INDUSTRY_NAME may have the town first, so it would result in the gender of the town name */
						auto tmp_params = ArrayStringParameters<0>();
						FormatString(builder, GetStringPtr(GetIndustrySpec(i->type)->name), tmp_params, next_substr_case_index);
					} else {
						/* First print the town name and the industry type name. */
						auto tmp_params = MakeParameters(i->town->index, GetIndustrySpec(i->type)->name);
						FormatString(builder, GetStringPtr(STR_FORMAT_INDUSTRY_NAME), tmp_params, next_substr_case_index);
					}
					next_substr_case_index = 0;
					break;
				}

				case SCC_PRESIDENT_NAME: { // {PRESIDENT_NAME}
					const Company *c = Company::GetIfValid(args.GetNextParameter<CompanyID>());
					if (c == nullptr) break;

					if (!c->president_name.empty()) {
						auto tmp_params = MakeParameters(c->president_name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						auto tmp_params = MakeParameters(c->president_name_2);
						GetStringWithArgs(builder, c->president_name_1, tmp_params);
					}
					break;
				}

				case SCC_STATION_NAME: { // {STATION}
					StationID sid = args.GetNextParameter<StationID>();
					const Station *st = Station::GetIfValid(sid);

					if (st == nullptr) {
						/* The station doesn't exist anymore. The only place where we might
						 * be "drawing" an invalid station is in the case of cargo that is
						 * in transit. */
						auto tmp_params = ArrayStringParameters<0>();
						GetStringWithArgs(builder, STR_UNKNOWN_STATION, tmp_params);
						break;
					}

					static bool use_cache = true;
					if (use_cache) { // Use cached version if first call
						AutoRestoreBackup cache_backup(use_cache, false);
						builder += st->GetCachedName();
					} else if (!st->name.empty()) {
						auto tmp_params = MakeParameters(st->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						StringID string_id = st->string_id;
						if (st->indtype != IT_INVALID) {
							/* Special case where the industry provides the name for the station */
							const IndustrySpec *indsp = GetIndustrySpec(st->indtype);

							/* Industry GRFs can change which might remove the station name and
							 * thus cause very strange things. Here we check for that before we
							 * actually set the station name. */
							if (indsp->station_name != STR_NULL && indsp->station_name != STR_UNDEFINED) {
								string_id = indsp->station_name;
							}
						}

						auto tmp_params = MakeParameters(STR_TOWN_NAME, st->town->index, st->index);
						GetStringWithArgs(builder, string_id, tmp_params);
					}
					break;
				}

				case SCC_TOWN_NAME: { // {TOWN}
					const Town *t = Town::GetIfValid(args.GetNextParameter<TownID>());
					if (t == nullptr) break;

					static bool use_cache = true;
					if (use_cache) { // Use cached version if first call
						AutoRestoreBackup cache_backup(use_cache, false);
						builder += t->GetCachedName();
					} else if (!t->name.empty()) {
						auto tmp_params = MakeParameters(t->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						GetTownName(builder, t);
					}
					break;
				}

				case SCC_WAYPOINT_NAME: { // {WAYPOINT}
					Waypoint *wp = Waypoint::GetIfValid(args.GetNextParameter<StationID>());
					if (wp == nullptr) break;

					if (!wp->name.empty()) {
						auto tmp_params = MakeParameters(wp->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						auto tmp_params = MakeParameters(wp->town->index, wp->town_cn + 1);
						StringID string_id = ((wp->string_id == STR_SV_STNAME_BUOY) ? STR_FORMAT_BUOY_NAME : STR_FORMAT_WAYPOINT_NAME);
						if (wp->town_cn != 0) string_id++;
						GetStringWithArgs(builder, string_id, tmp_params);
					}
					break;
				}

				case SCC_VEHICLE_NAME: { // {VEHICLE}
					const Vehicle *v = Vehicle::GetIfValid(args.GetNextParameter<VehicleID>());
					if (v == nullptr) break;

					if (!v->name.empty()) {
						auto tmp_params = MakeParameters(v->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else if (v->group_id != DEFAULT_GROUP) {
						/* The vehicle has no name, but is member of a group, so print group name */
						auto tmp_params = MakeParameters(v->group_id, v->unitnumber);
						GetStringWithArgs(builder, STR_FORMAT_GROUP_VEHICLE_NAME, tmp_params);
					} else {
						auto tmp_params = MakeParameters(v->unitnumber);

						StringID string_id;
						switch (v->type) {
							default:           string_id = STR_INVALID_VEHICLE; break;
							case VEH_TRAIN:    string_id = STR_SV_TRAIN_NAME; break;
							case VEH_ROAD:     string_id = STR_SV_ROAD_VEHICLE_NAME; break;
							case VEH_SHIP:     string_id = STR_SV_SHIP_NAME; break;
							case VEH_AIRCRAFT: string_id = STR_SV_AIRCRAFT_NAME; break;
						}

						GetStringWithArgs(builder, string_id, tmp_params);
					}
					break;
				}

				case SCC_SIGN_NAME: { // {SIGN}
					const Sign *si = Sign::GetIfValid(args.GetNextParameter<SignID>());
					if (si == nullptr) break;

					if (!si->name.empty()) {
						auto tmp_params = MakeParameters(si->name);
						GetStringWithArgs(builder, STR_JUST_RAW_STRING, tmp_params);
					} else {
						auto tmp_params = ArrayStringParameters<0>();
						GetStringWithArgs(builder, STR_DEFAULT_SIGN_NAME, tmp_params);
					}
					break;
				}

				case SCC_STATION_FEATURES: { // {STATIONFEATURES}
					StationGetSpecialString(builder, args.GetNextParameter<StationFacility>());
					break;
				}

				case SCC_COLOUR: { // {COLOUR}
					StringControlCode scc = (StringControlCode)(SCC_BLUE + args.GetNextParameter<Colours>());
					if (IsInsideMM(scc, SCC_BLUE, SCC_COLOUR)) builder.Utf8Encode(scc);
					break;
				}

				default:
					builder.Utf8Encode(b);
					break;
			}
		} catch (std::out_of_range &e) {
			Debug(misc, 0, "FormatString: {}", e.what());
			builder += "(invalid parameter)";
		}
	}
}


static void StationGetSpecialString(StringBuilder &builder, StationFacility x)
{
	if ((x & FACIL_TRAIN) != 0) builder.Utf8Encode(SCC_TRAIN);
	if ((x & FACIL_TRUCK_STOP) != 0) builder.Utf8Encode(SCC_LORRY);
	if ((x & FACIL_BUS_STOP) != 0) builder.Utf8Encode(SCC_BUS);
	if ((x & FACIL_DOCK) != 0) builder.Utf8Encode(SCC_SHIP);
	if ((x & FACIL_AIRPORT) != 0) builder.Utf8Encode(SCC_PLANE);
}

static void GetSpecialTownNameString(StringBuilder &builder, int ind, uint32_t seed)
{
	GenerateTownNameString(builder, ind, seed);
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

static void GenAndCoName(StringBuilder &builder, uint32_t arg)
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

	builder += base[num * GB(arg, 16, 8) >> 8];
	builder += " & Co.";
}

static void GenPresidentName(StringBuilder &builder, uint32_t x)
{
	char initial[] = "?. ";
	const char * const *base;
	uint num;
	uint i;

	initial[0] = _initial_name_letters[sizeof(_initial_name_letters) * GB(x, 0, 8) >> 8];
	builder += initial;

	i = (sizeof(_initial_name_letters) + 35) * GB(x, 8, 8) >> 8;
	if (i < sizeof(_initial_name_letters)) {
		initial[0] = _initial_name_letters[i];
		builder += initial;
	}

	if (_settings_game.game_creation.landscape == LT_TOYLAND) {
		base = _silly_surname_list;
		num  = lengthof(_silly_surname_list);
	} else {
		base = _surname_list;
		num  = lengthof(_surname_list);
	}

	builder += base[num * GB(x, 16, 8) >> 8];
}

static void GetSpecialNameString(StringBuilder &builder, int ind, StringParameters &args)
{
	switch (ind) {
		case 1: // not used
			builder += _silly_company_names[std::min<uint>(args.GetNextParameter<uint16_t>(), lengthof(_silly_company_names) - 1)];
			return;

		case 2: // used for Foobar & Co company names
			GenAndCoName(builder, args.GetNextParameter<uint32_t>());
			return;

		case 3: // President name
			GenPresidentName(builder, args.GetNextParameter<uint32_t>());
			return;
	}

	/* town name? */
	if (IsInsideMM(ind - 6, 0, SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1)) {
		GetSpecialTownNameString(builder, ind - 6, args.GetNextParameter<uint32_t>());
		builder += " Transport";
		return;
	}

	NOT_REACHED();
}

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

/**
 * Check whether a translation is sufficiently finished to offer it to the public.
 */
bool LanguagePackHeader::IsReasonablyFinished() const
{
	/* "Less than 25% missing" is "sufficiently finished". */
	return 4 * this->missing < LANGUAGE_TOTAL_STRINGS;
}

/**
 * Read a particular language.
 * @param lang The metadata about the language.
 * @return Whether the loading went okay or not.
 */
bool ReadLanguagePack(const LanguageMetadata *lang)
{
	/* Current language pack */
	size_t len = 0;
	std::unique_ptr<LanguagePack, LanguagePackDeleter> lang_pack(reinterpret_cast<LanguagePack *>(ReadFileToMem(lang->file.string(), len, 1U << 20).release()));
	if (!lang_pack) return false;

	/* End of read data (+ terminating zero added in ReadFileToMem()) */
	const char *end = (char *)lang_pack.get() + len + 1;

	/* We need at least one byte of lang_pack->data */
	if (end <= lang_pack->data || !lang_pack->IsValid()) {
		return false;
	}

#if TTD_ENDIAN == TTD_BIG_ENDIAN
	for (uint i = 0; i < TEXT_TAB_END; i++) {
		lang_pack->offsets[i] = ReadLE16Aligned(&lang_pack->offsets[i]);
	}
#endif /* TTD_ENDIAN == TTD_BIG_ENDIAN */

	std::array<uint, TEXT_TAB_END> tab_start, tab_num;

	uint count = 0;
	for (uint i = 0; i < TEXT_TAB_END; i++) {
		uint16_t num = lang_pack->offsets[i];
		if (num > TAB_SIZE) return false;

		tab_start[i] = count;
		tab_num[i] = num;
		count += num;
	}

	/* Allocate offsets */
	std::vector<char *> offs(count);

	/* Fill offsets */
	char *s = lang_pack->data;
	len = (byte)*s++;
	for (uint i = 0; i < count; i++) {
		if (s + len >= end) return false;

		if (len >= 0xC0) {
			len = ((len & 0x3F) << 8) + (byte)*s++;
			if (s + len >= end) return false;
		}
		offs[i] = s;
		s += len;
		len = (byte)*s;
		*s++ = '\0'; // zero terminate the string
	}

	_langpack.langpack = std::move(lang_pack);
	_langpack.offsets = std::move(offs);
	_langpack.langtab_num = tab_num;
	_langpack.langtab_start = tab_start;

	_current_language = lang;
	_current_text_dir = (TextDirection)_current_language->text_dir;
	_config_language_file = _current_language->file.filename().string();
	SetCurrentGrfLangID(_current_language->newgrflangid);

#ifdef _WIN32
	extern void Win32SetCurrentLocaleName(std::string iso_code);
	Win32SetCurrentLocaleName(_current_language->isocode);
#endif

#ifdef WITH_COCOA
	extern void MacOSSetCurrentLocaleName(const char *iso_code);
	MacOSSetCurrentLocaleName(_current_language->isocode);
#endif

#ifdef WITH_ICU_I18N
	/* Create a collator instance for our current locale. */
	UErrorCode status = U_ZERO_ERROR;
	_current_collator.reset(icu::Collator::createInstance(icu::Locale(_current_language->isocode), status));
	/* Sort number substrings by their numerical value. */
	if (_current_collator) _current_collator->setAttribute(UCOL_NUMERIC_COLLATION, UCOL_ON, status);
	/* Avoid using the collator if it is not correctly set. */
	if (U_FAILURE(status)) {
		_current_collator.reset();
	}
#endif /* WITH_ICU_I18N */

	/* Some lists need to be sorted again after a language change. */
	ReconsiderGameScriptLanguage();
	InitializeSortedCargoSpecs();
	SortIndustryTypes();
	BuildIndustriesLegend();
	BuildContentTypeStringList();
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
#if !(defined(_WIN32) || defined(__APPLE__))
/**
 * Determine the current charset based on the environment
 * First check some default values, after this one we passed ourselves
 * and if none exist return the value for $LANG
 * @param param environment variable to check conditionally if default ones are not
 *        set. Pass nullptr if you don't want additional checks.
 * @return return string containing current charset, or nullptr if not-determinable
 */
const char *GetCurrentLocale(const char *param)
{
	const char *env;

	env = std::getenv("LANGUAGE");
	if (env != nullptr) return env;

	env = std::getenv("LC_ALL");
	if (env != nullptr) return env;

	if (param != nullptr) {
		env = std::getenv(param);
		if (env != nullptr) return env;
	}

	return std::getenv("LANG");
}
#else
const char *GetCurrentLocale(const char *param);
#endif /* !(defined(_WIN32) || defined(__APPLE__)) */

/**
 * Get the language with the given NewGRF language ID.
 * @param newgrflangid NewGRF languages ID to check.
 * @return The language's metadata, or nullptr if it is not known.
 */
const LanguageMetadata *GetLanguage(byte newgrflangid)
{
	for (const LanguageMetadata &lang : _languages) {
		if (newgrflangid == lang.newgrflangid) return &lang;
	}

	return nullptr;
}

/**
 * Reads the language file header and checks compatibility.
 * @param file the file to read
 * @param hdr  the place to write the header information to
 * @return true if and only if the language file is of a compatible version
 */
static bool GetLanguageFileHeader(const char *file, LanguagePackHeader *hdr)
{
	FILE *f = fopen(file, "rb");
	if (f == nullptr) return false;

	size_t read = fread(hdr, sizeof(*hdr), 1, f);
	fclose(f);

	bool ret = read == 1 && hdr->IsValid();

	/* Convert endianness for the windows language ID */
	if (ret) {
		hdr->missing = FROM_LE16(hdr->missing);
		hdr->winlangid = FROM_LE16(hdr->winlangid);
	}
	return ret;
}

/**
 * Search for the languages in the given directory and add them to the #_languages list.
 * @param path the base directory to search in
 */
static void FillLanguageList(const std::string &path)
{
	DIR *dir = ttd_opendir(path.c_str());
	if (dir != nullptr) {
		struct dirent *dirent;
		while ((dirent = readdir(dir)) != nullptr) {
			std::string d_name = FS2OTTD(dirent->d_name);
			const char *extension = strrchr(d_name.c_str(), '.');

			/* Not a language file */
			if (extension == nullptr || strcmp(extension, ".lng") != 0) continue;

			LanguageMetadata lmd;
			lmd.file = path + d_name;

			/* Check whether the file is of the correct version */
			if (!GetLanguageFileHeader(lmd.file.string().c_str(), &lmd)) {
				Debug(misc, 3, "{} is not a valid language file", lmd.file);
			} else if (GetLanguage(lmd.newgrflangid) != nullptr) {
				Debug(misc, 3, "{}'s language ID is already known", lmd.file);
			} else {
				_languages.push_back(lmd);
			}
		}
		closedir(dir);
	}
}

/**
 * Make a list of the available language packs. Put the data in
 * #_languages list.
 */
void InitializeLanguagePacks()
{
	for (Searchpath sp : _valid_searchpaths) {
		FillLanguageList(FioGetDirectory(sp, LANG_DIR));
	}
	if (_languages.empty()) UserError("No available language packs (invalid versions?)");

	/* Acquire the locale of the current system */
	const char *lang = GetCurrentLocale("LC_MESSAGES");
	if (lang == nullptr) lang = "en_GB";

	const LanguageMetadata *chosen_language   = nullptr; ///< Matching the language in the configuration file or the current locale
	const LanguageMetadata *language_fallback = nullptr; ///< Using pt_PT for pt_BR locale when pt_BR is not available
	const LanguageMetadata *en_GB_fallback    = _languages.data(); ///< Fallback when no locale-matching language has been found

	/* Find a proper language. */
	for (const LanguageMetadata &lng : _languages) {
		/* We are trying to find a default language. The priority is by
		 * configuration file, local environment and last, if nothing found,
		 * English. */
		if (_config_language_file == lng.file.filename()) {
			chosen_language = &lng;
			break;
		}

		if (strcmp (lng.isocode, "en_GB") == 0) en_GB_fallback    = &lng;

		/* Only auto-pick finished translations */
		if (!lng.IsReasonablyFinished()) continue;

		if (strncmp(lng.isocode, lang, 5) == 0) chosen_language   = &lng;
		if (strncmp(lng.isocode, lang, 2) == 0) language_fallback = &lng;
	}

	/* We haven't found the language in the config nor the one in the locale.
	 * Now we set it to one of the fallback languages */
	if (chosen_language == nullptr) {
		chosen_language = (language_fallback != nullptr) ? language_fallback : en_GB_fallback;
	}

	if (!ReadLanguagePack(chosen_language)) UserError("Can't read language pack '{}'", chosen_language->file);
}

/**
 * Get the ISO language code of the currently loaded language.
 * @return the ISO code.
 */
const char *GetCurrentLanguageIsoCode()
{
	return _langpack.langpack->isocode;
}

/**
 * Check whether there are glyphs missing in the current language.
 * @return If glyphs are missing, return \c true, else return \c false.
 */
bool MissingGlyphSearcher::FindMissingGlyphs()
{
	InitFontCache(this->Monospace());
	const Sprite *question_mark[FS_END];

	for (FontSize size = this->Monospace() ? FS_MONO : FS_BEGIN; size < (this->Monospace() ? FS_END : FS_MONO); size++) {
		question_mark[size] = GetGlyph(size, '?');
	}

	this->Reset();
	for (auto text = this->NextString(); text.has_value(); text = this->NextString()) {
		auto src = text->cbegin();

		FontSize size = this->DefaultSize();
		while (src != text->cend()) {
			char32_t c = Utf8Consume(src);

			if (c >= SCC_FIRST_FONT && c <= SCC_LAST_FONT) {
				size = (FontSize)(c - SCC_FIRST_FONT);
			} else if (!IsInsideMM(c, SCC_SPRITE_START, SCC_SPRITE_END) && IsPrintable(c) && !IsTextDirectionChar(c) && c != '?' && GetGlyph(size, c) == question_mark[size]) {
				/* The character is printable, but not in the normal font. This is the case we were testing for. */
				std::string size_name;

				switch (size) {
					case FS_NORMAL: size_name = "medium"; break;
					case FS_SMALL: size_name = "small"; break;
					case FS_LARGE: size_name = "large"; break;
					case FS_MONO: size_name = "mono"; break;
					default: NOT_REACHED();
				}

				Debug(fontcache, 0, "Font is missing glyphs to display char 0x{:X} in {} font size", (int)c, size_name);
				return true;
			}
		}
	}
	return false;
}

/** Helper for searching through the language pack. */
class LanguagePackGlyphSearcher : public MissingGlyphSearcher {
	uint i; ///< Iterator for the primary language tables.
	uint j; ///< Iterator for the secondary language tables.

	void Reset() override
	{
		this->i = 0;
		this->j = 0;
	}

	FontSize DefaultSize() override
	{
		return FS_NORMAL;
	}

	std::optional<std::string_view> NextString() override
	{
		if (this->i >= TEXT_TAB_END) return std::nullopt;

		const char *ret = _langpack.offsets[_langpack.langtab_start[this->i] + this->j];

		this->j++;
		while (this->i < TEXT_TAB_END && this->j >= _langpack.langtab_num[this->i]) {
			this->i++;
			this->j = 0;
		}

		return ret;
	}

	bool Monospace() override
	{
		return false;
	}

	void SetFontNames([[maybe_unused]] FontCacheSettings *settings, [[maybe_unused]] const char *font_name, [[maybe_unused]] const void *os_data) override
	{
#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
		settings->small.font = font_name;
		settings->medium.font = font_name;
		settings->large.font = font_name;

		settings->small.os_handle = os_data;
		settings->medium.os_handle = os_data;
		settings->large.os_handle = os_data;
#endif
	}
};

/**
 * Check whether the currently loaded language pack
 * uses characters that the currently loaded font
 * does not support. If this is the case an error
 * message will be shown in English. The error
 * message will not be localized because that would
 * mean it might use characters that are not in the
 * font, which is the whole reason this check has
 * been added.
 * @param base_font Whether to look at the base font as well.
 * @param searcher  The methods to use to search for strings to check.
 *                  If nullptr the loaded language pack searcher is used.
 */
void CheckForMissingGlyphs(bool base_font, MissingGlyphSearcher *searcher)
{
	static LanguagePackGlyphSearcher pack_searcher;
	if (searcher == nullptr) searcher = &pack_searcher;
	bool bad_font = !base_font || searcher->FindMissingGlyphs();
#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
	if (bad_font) {
		/* We found an unprintable character... lets try whether we can find
		 * a fallback font that can print the characters in the current language. */
		bool any_font_configured = !_fcsettings.medium.font.empty();
		FontCacheSettings backup = _fcsettings;

		_fcsettings.mono.os_handle = nullptr;
		_fcsettings.medium.os_handle = nullptr;

		bad_font = !SetFallbackFont(&_fcsettings, _langpack.langpack->isocode, _langpack.langpack->winlangid, searcher);

		_fcsettings = backup;

		if (!bad_font && any_font_configured) {
			/* If the user configured a bad font, and we found a better one,
			 * show that we loaded the better font instead of the configured one.
			 * The colour 'character' might change in the
			 * future, so for safety we just Utf8 Encode it into the string,
			 * which takes exactly three characters, so it replaces the "XXX"
			 * with the colour marker. */
			static std::string err_str("XXXThe current font is missing some of the characters used in the texts for this language. Using system fallback font instead.");
			Utf8Encode(err_str.data(), SCC_YELLOW);
			SetDParamStr(0, err_str);
			ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_WARNING);
		}

		if (bad_font && base_font) {
			/* Our fallback font does miss characters too, so keep the
			 * user chosen font as that is more likely to be any good than
			 * the wild guess we made */
			InitFontCache(searcher->Monospace());
		}
	}
#endif

	if (bad_font) {
		/* All attempts have failed. Display an error. As we do not want the string to be translated by
		 * the translators, we 'force' it into the binary and 'load' it via a BindCString. To do this
		 * properly we have to set the colour of the string, otherwise we end up with a lot of artifacts.
		 * The colour 'character' might change in the future, so for safety we just Utf8 Encode it into
		 * the string, which takes exactly three characters, so it replaces the "XXX" with the colour marker. */
		static std::string err_str("XXXThe current font is missing some of the characters used in the texts for this language. Read the readme to see how to solve this.");
		Utf8Encode(err_str.data(), SCC_YELLOW);
		SetDParamStr(0, err_str);
		ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_WARNING);

		/* Reset the font width */
		LoadStringWidthTable(searcher->Monospace());
		return;
	}

	/* Update the font with cache */
	LoadStringWidthTable(searcher->Monospace());

#if !(defined(WITH_ICU_I18N) && defined(WITH_HARFBUZZ)) && !defined(WITH_UNISCRIBE) && !defined(WITH_COCOA)
	/*
	 * For right-to-left languages we need the ICU library. If
	 * we do not have support for that library we warn the user
	 * about it with a message. As we do not want the string to
	 * be translated by the translators, we 'force' it into the
	 * binary and 'load' it via a BindCString. To do this
	 * properly we have to set the colour of the string,
	 * otherwise we end up with a lot of artifacts. The colour
	 * 'character' might change in the future, so for safety
	 * we just Utf8 Encode it into the string, which takes
	 * exactly three characters, so it replaces the "XXX" with
	 * the colour marker.
	 */
	if (_current_text_dir != TD_LTR) {
		static std::string err_str("XXXThis version of OpenTTD does not support right-to-left languages. Recompile with ICU + Harfbuzz enabled.");
		Utf8Encode(err_str.data(), SCC_YELLOW);
		SetDParamStr(0, err_str);
		ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
	}
#endif /* !(WITH_ICU_I18N && WITH_HARFBUZZ) && !WITH_UNISCRIBE && !WITH_COCOA */
}
