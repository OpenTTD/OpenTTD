#include "stdafx.h"
#include "ttd.h"
#include "string.h"
#include "strings.h"
#include "table/strings.h"
#include "namegen.h"
#include "station.h"
#include "town.h"
#include "vehicle.h"
#include "news.h"
#include "screenshot.h"

static char *StationGetSpecialString(char *buff);
static char *GetSpecialTownNameString(char *buff, int ind);
static char *GetSpecialPlayerNameString(char *buff, int ind);

static char *DecodeString(char *buff, const char *str);

static char **_langpack_offs;
static char *_langpack;
static uint _langtab_num[32]; // Offset into langpack offs
static uint _langtab_start[32]; // Offset into langpack offs

extern const char _openttd_revision[];

typedef struct {
	uint32 ident;
	uint32 version;			// 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];			// the international name of this language
	char own_name[32];	// the localized name of this language
	uint16 offsets[32];	// the offsets
} LanguagePackHeader;

const uint16 _currency_string_list[] = {
	STR_CURR_GBP,
	STR_CURR_USD,
	STR_CURR_EUR,
	STR_CURR_YEN,
	STR_CURR_ATS,
	STR_CURR_BEF,
	STR_CURR_CHF,
	STR_CURR_CZK,
	STR_CURR_DEM,
	STR_CURR_DKK,
	STR_CURR_ESP,
	STR_CURR_FIM,
	STR_CURR_FRF,
	STR_CURR_GRD,
	STR_CURR_HUF,
	STR_CURR_ISK,
	STR_CURR_ITL,
	STR_CURR_NLG,
	STR_CURR_NOK,
	STR_CURR_PLN,
	STR_CURR_ROL,
	STR_CURR_RUR,
	STR_CURR_SEK,
	STR_CURR_CUSTOM,
	INVALID_STRING_ID
};

static const uint16 _cargo_string_list[NUM_LANDSCAPE][NUM_CARGO] = {
	{ /* LT_NORMAL */
		STR_PASSENGERS,
		STR_TONS,
		STR_BAGS,
		STR_LITERS,
		STR_ITEMS,
		STR_CRATES,
		STR_TONS,
		STR_TONS,
		STR_TONS,
		STR_TONS,
		STR_BAGS,
		STR_RES_OTHER
	},

	{ /* LT_HILLY */
		STR_PASSENGERS,
		STR_TONS,
		STR_BAGS,
		STR_LITERS,
		STR_ITEMS,
		STR_CRATES,
		STR_TONS,
		STR_TONS,
		STR_RES_OTHER,
		STR_TONS,
		STR_BAGS,
		STR_TONS
	},

	{ /* LT_DESERT */
		STR_PASSENGERS,
		STR_LITERS,
		STR_BAGS,
		STR_LITERS,
		STR_TONS,
		STR_CRATES,
		STR_TONS,
		STR_TONS,
		STR_TONS,
		STR_LITERS,
		STR_BAGS,
		STR_TONS
	},

	{ /* LT_CANDY */
		STR_PASSENGERS,
		STR_TONS,
		STR_BAGS,
		STR_NOTHING,
		STR_NOTHING,
		STR_TONS,
		STR_TONS,
		STR_LITERS,
		STR_TONS,
		STR_NOTHING,
		STR_LITERS,
		STR_NOTHING
	}
};

static char *str_cat(char *dst, const char *src)
{
	while ((*dst++ = *src++) != '\0') {}
	return dst - 1;
}

static char *GetStringPtr(uint16 string)
{
	return _langpack_offs[_langtab_start[string >> 11] + (string & 0x7FF)];
}

char *GetString(char *buffr, uint16 string)
{
	uint index = string & 0x7FF;
	uint tab = string >> 11;

	if (string == 0) error("!invalid string id 0 in GetString");

	if (tab == 4 && index >= 0xC0)
		return GetSpecialTownNameString(buffr, index - 0xC0);

	if (tab == 6 && index == 0xD1)
		return StationGetSpecialString(buffr);

	if (tab == 14 && index >= 0xE4)
		return GetSpecialPlayerNameString(buffr, index - 0xE4);

	if (tab == 15)
		return GetName(index, buffr);

	// tab 31 is used for special or dynamic strings
	if (tab == 31) {
		return DecodeString(buffr, index == (STR_SPEC_SCREENSHOT_NAME & 0x7FF) ? _screenshot_name : _userstring);
	}

	if (index >= _langtab_num[tab])
		error("!String 0x%X is invalid. Probably because an old version of the .lng file.\n", string);

	return DecodeString(buffr, GetStringPtr(string));
}

void InjectDParam(int amount)
{
	memmove(_decode_parameters + amount, _decode_parameters, sizeof(_decode_parameters) - amount * sizeof(uint32));
}


int32 GetParamInt32(void)
{
	int32 result = GetDParam(0);
	memmove(&_decode_parameters[0], &_decode_parameters[1], sizeof(uint32) * (lengthof(_decode_parameters)-1));
	return result;
}

static int64 GetParamInt64(void)
{
	int64 result = GetDParam(0) + ((uint64)GetDParam(1) << 32);
	memmove(&_decode_parameters[0], &_decode_parameters[2], sizeof(uint32) * (lengthof(_decode_parameters)-2));
	return result;
}


int GetParamInt16(void)
{
	int result = (int16)GetDParam(0);
	memmove(&_decode_parameters[0], &_decode_parameters[1], sizeof(uint32) * (lengthof(_decode_parameters)-1));
	return result;
}

int GetParamInt8(void)
{
	int result = (int8)GetDParam(0);
	memmove(&_decode_parameters[0], &_decode_parameters[1], sizeof(uint32) * (lengthof(_decode_parameters)-1));
	return result;
}

int GetParamUint16(void)
{
	int result = GetDParam(0);
	memmove(&_decode_parameters[0], &_decode_parameters[1], sizeof(uint32) * (lengthof(_decode_parameters)-1));
	return result;
}

static const uint32 _divisor_table[] = {
	1000000000,
	100000000,
	10000000,
	1000000,

	100000,
	10000,
	1000,
	100,
	10,
	1
};

static char *FormatCommaNumber(char *buff, int32 number)
{
	uint32 quot,divisor;
	int i;
	uint32 tot;
	uint32 num;

	if (number < 0) {
		*buff++ = '-';
		number = -number;
	}

	num = number;

	tot = 0;
	for (i = 0; i != 10; i++) {
		divisor = _divisor_table[i];
		quot = 0;
		if (num >= divisor) {
			quot = num / _divisor_table[i];
			num = num % _divisor_table[i];
		}
		if (tot |= quot || i == 9) {
			*buff++ = '0' + quot;
			if (i == 0 || i == 3 || i == 6) *buff++ = ',';
		}
	}

	*buff = '\0';

	return buff;
}

static char *FormatNoCommaNumber(char *buff, int32 number)
{
	uint32 quot,divisor;
	int i;
	uint32 tot;
	uint32 num;

	if (number < 0) {
		*buff++ = '-';
		number = -number;
	}

	num = number;

	tot = 0;
	for (i = 0; i != 10; i++) {
		divisor = _divisor_table[i];
		quot = 0;
		if (num >= divisor) {
			quot = num / _divisor_table[i];
			num = num % _divisor_table[i];
		}
		if (tot |= quot || i == 9) {
			*buff++ = '0' + quot;
		}
	}

	*buff = '\0';

	return buff;
}


static char *FormatYmdString(char *buff, uint16 number)
{
	const char *src;
	YearMonthDay ymd;

	ConvertDayToYMD(&ymd, number);

	for (src = GetStringPtr(ymd.day + STR_01AC_1ST - 1); (*buff++ = *src++) != '\0';) {}

	buff[-1] = ' ';
	memcpy(buff, GetStringPtr(STR_0162_JAN + ymd.month), 4);
	buff[3] = ' ';

	return FormatNoCommaNumber(buff+4, ymd.year + MAX_YEAR_BEGIN_REAL);
}

static char *FormatMonthAndYear(char *buff, uint16 number)
{
	const char *src;
	YearMonthDay ymd;

	ConvertDayToYMD(&ymd, number);

	for (src = GetStringPtr(STR_MONTH_JAN + ymd.month); (*buff++ = *src++) != '\0';) {}
	buff[-1] = ' ';

	return FormatNoCommaNumber(buff, ymd.year + MAX_YEAR_BEGIN_REAL);
}

static char *FormatTinyDate(char *buff, uint16 number)
{
	YearMonthDay ymd;

	ConvertDayToYMD(&ymd, number);
	buff += sprintf(buff, " %02i-%02i-%04i", ymd.day, ymd.month + 1, ymd.year + MAX_YEAR_BEGIN_REAL);

	return buff;
}

uint GetCurrentCurrencyRate(void)
{
	return _currency_specs[_opt.currency].rate;
}

static char *FormatGenericCurrency(char *buff, const CurrencySpec *spec, int64 number, bool compact)
{
	const char *s;
	char c;
	char buf[40], *p;
	int j;

	// multiply by exchange rate
	number *= spec->rate;

	// convert from negative
	if (number < 0) {
		*buff++ = '-';
		number = -number;
	}

	// add prefix part
	s = spec->prefix;
	while (s != spec->prefix + lengthof(spec->prefix) && (c = *s++) != '\0') *buff++ = c;

	// for huge numbers, compact the number into k or M
	if (compact) {
		compact = 0;
		if (number >= 1000000000) {
			number = (number + 500000) / 1000000;
			compact = 'M';
		} else if (number >= 1000000) {
			number = (number + 500) / 1000;
			compact = 'k';
		}
	}

	// convert to ascii number and add commas
	p = buf;
	j = 4;
	do {
		if (--j == 0) {
			*p++ = spec->separator;
			j = 3;
		}
		*p++ = '0' + number % 10;
	} while (number /= 10);
	do *buff++ = *--p; while (p != buf);

	if (compact) *buff++ = compact;

	// add suffix part
	s = spec->suffix;
	while (s != spec->suffix + lengthof(spec->suffix) && (c = *s++) != '\0') *buff++ = c;

	return buff;
}

static char *DecodeString(char *buff, const char *str)
{
	byte b;

	while ((b = *str++) != '\0') {
		switch (b) {
		case 0x1: // {SETX}
			*buff++ = b;
			*buff++ = *str++;
			break;
		case 0x2: // {SETXY}
			*buff++ = b;
			*buff++ = *str++;
			*buff++ = *str++;
			break;
		case 0x7B: // {COMMA32}
			buff = FormatCommaNumber(buff, GetParamInt32());
			break;
		case 0x7C: // {COMMA16}
			buff = FormatCommaNumber(buff, GetParamInt16());
			break;
		case 0x7D: // {COMMA8}
			buff = FormatCommaNumber(buff, GetParamInt8());
			break;
		case 0x7E: // {NUMU16}
			buff = FormatNoCommaNumber(buff, GetParamInt16());
			break;
		case 0x7F: // {CURRENCY}
			buff = FormatGenericCurrency(buff, &_currency_specs[_opt.currency], GetParamInt32(), false);
			break;
		// 0x80 is reserved for EURO
		case 0x81: // {STRINL}
			str += 2;
			buff = GetString(buff, READ_LE_UINT16(str-2));
			break;
		case 0x82: // {DATE_LONG}
			buff = FormatYmdString(buff, GetParamUint16());
			break;
		case 0x83: // {DATE_SHORT}
			buff = FormatMonthAndYear(buff, GetParamUint16());
			break;
		case 0x84: {// {VELOCITY}
			int value = GetParamInt16();
			if (_opt.kilometers) value = value * 1648 >> 10;
			buff = FormatCommaNumber(buff, value);
			if (_opt.kilometers) {
				memcpy(buff, " km/h", 5);
				buff += 5;
			} else {
				memcpy(buff, " mph", 4);
				buff += 4;
			}
			break;
		}

		// 0x85 is used as escape character..
		case 0x85:
			switch (*str++) {
			case 0: /* {CURRCOMPACT} */
				buff = FormatGenericCurrency(buff, &_currency_specs[_opt.currency], GetParamInt32(), true);
				break;
			case 1: /* {INT32} */
				buff = FormatNoCommaNumber(buff, GetParamInt32());
				break;
			case 2: /* {REV} */
				buff = str_cat(buff, _openttd_revision);
				break;
			case 3: { /* {SHORTCARGO} */
				// Short description of cargotypes. Layout:
				// 8-bit = cargo type
				// 16-bit = cargo count
				char *s;
				uint16 cargo_str = _cargo_string_list[_opt.landscape][(byte)GetParamInt8()];
				uint16 multiplier = (cargo_str == STR_LITERS) ? 1000 : 1;
				// liquid type of cargo is multiplied by 100 to get correct amount
				buff = FormatCommaNumber(buff, GetParamInt16() * multiplier);
				s = GetStringPtr(cargo_str);

				memcpy(buff++, " ", 1);
				while (*s) *buff++ = *s++;
			} break;
			case 4: /* {CURRCOMPACT64} */
				// 64 bit compact currency-unit
				buff = FormatGenericCurrency(buff, &_currency_specs[_opt.currency], GetParamInt64(), true);
				break;

			default:
				error("!invalid escape sequence in string");
			}
			break;

		case 0x86: // {SKIP}
			GetParamInt16();
			//assert(0);
			break;
		case 0x87: { // {VOLUME}
			char *s;
			buff = FormatCommaNumber(buff, GetParamInt16() * 1000);
			memcpy(buff++, " ", 1);
			s = GetStringPtr(STR_LITERS);
			while (*s) *buff++ = *s++;
			break;
		}

		case 0x88: // {STRING}
			buff = GetString(buff, (uint16)GetParamUint16());
			break;

		case 0x99: { // {CARGO}
			// Layout now is:
			//   8bit   - cargo type
			//   16-bit - cargo count
			int cargo_str = _cargoc.names_long_s[GetParamInt8()];
			// Now check if the cargo count is 1, if it is, increase string by 32.
			if (GetDParam(0) != 1) cargo_str += 32;
			buff = GetString(buff, cargo_str);
			break;
		}

		case 0x9A: { // {STATION}
			Station *st;
			InjectDParam(1);
			st = GetStation(GetDParam(1));
			if (st->xy == 0) { // station doesn't exist anymore
				buff = GetString(buff, STR_UNKNOWN_DESTINATION);
				break;
			}
			SetDParam(0, st->town->townnametype);
			SetDParam(1, st->town->townnameparts);
			buff = GetString(buff, st->string_id);
			break;
		}
		case 0x9B: { // {TOWN}
			Town *t;
			t = GetTown(GetDParam(0));
			assert(t->xy);
			SetDParam(0, t->townnameparts);
			buff = GetString(buff, t->townnametype);
			break;
		}

		case 0x9C: { // {CURRENCY64}
			buff = FormatGenericCurrency(buff, &_currency_specs[_opt.currency], GetParamInt64(), false);
			break;
		}

		case 0x9D: { // {WAYPOINT}
			Waypoint *cp = &_waypoints[GetDParam(0)];
			StringID str;
			int idx;
			if (~cp->town_or_string & 0xC000) {
				GetParamInt32(); // skip it
				str = cp->town_or_string;
			} else {
				idx = (cp->town_or_string >> 8) & 0x3F;
				if (idx == 0) {
					str = STR_WAYPOINTNAME_CITY;
				} else {
					InjectDParam(1);
					SetDParam(1, idx + 1);
					str = STR_WAYPOINTNAME_CITY_SERIAL;
				}
				SetDParam(0, cp->town_or_string & 0xFF);
			}

			buff = GetString(buff, str);
		} break;

		case 0x9E: { // {DATE_TINY}
			buff = FormatTinyDate(buff, GetParamUint16());
			break;
		}

		// case 0x88..0x98: // {COLORS}
		// case 0xE: // {TINYFONT}
		// case 0xF: // {BIGFONT}
		// 0x9E is the highest number that is available.
		default:
			*buff++ = b;
		}
	}
	*buff = '\0';
	return buff;
}


static char *StationGetSpecialString(char *buff)
{
	int x = GetParamInt8();
	if (x & 0x01) *buff++ = '\xB4';
	if (x & 0x02) *buff++ = '\xB5';
	if (x & 0x04) *buff++ = '\xB6';
	if (x & 0x08) *buff++ = '\xB7';
	if (x & 0x10) *buff++ = '\xB8';
	*buff = '\0';
	return buff;
}

static char *GetSpecialTownNameString(char *buff, int ind)
{
	uint32 x = GetParamInt32();

	_town_name_generators[ind](buff, x);

	while (*buff != '\0') buff++;
	return buff;
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
	"Getout & Pushit Ltd.",
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
	"Watkins",
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
	"Nutkins",
};

static const char _initial_name_letters[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'P', 'R', 'S', 'T', 'W',
};

static char *GenAndCoName(char *buff)
{
	uint32 x = GetParamInt32();
	uint base,num;

	base = 0;
	num = 29;
	if (_opt.landscape == LT_CANDY) {
		base = num;
		num = 12;
	}

	buff = str_cat(buff, _surname_list[base + ((num * (byte)(x >> 16)) >> 8)]);
	buff = str_cat(buff, " & Co.");

	return buff;
}

static char *GenPlayerName_4(char *buff)
{
	uint32 x = GetParamInt32();
	uint i, base, num;

	buff[0] = _initial_name_letters[(sizeof(_initial_name_letters) * (byte)x) >> 8];
	buff[1] = '.';
	buff[2] = ' '; // Insert a space after initial and period "I. Firstname" instead of "I.Firstname"
	buff += 3;

	i = ((sizeof(_initial_name_letters) + 35) * (byte)(x >> 8)) >> 8;
	if (i < sizeof(_initial_name_letters)) {
		buff[0] = _initial_name_letters[i];
		buff[1] = '.';
		buff[2] = ' '; // Insert a space after initial and period "I. J. Firstname" instead of "I.J.Firstname"
		buff += 3;
	}

	base = 0;
	num = 29;
	if (_opt.landscape == LT_CANDY) {
		base = num;
		num = 12;
	}

	buff = str_cat(buff, _surname_list[base + ((num * (byte)(x >> 16)) >> 8)]);

	return buff;
}

static const char * const _song_names[] = {
	"Tycoon DELUXE Theme",
	"Easy Driver",
	"Little Red Diesel",
	"Cruise Control",
	"Don't Walk!",
	"Fell Apart On Me",
	"City Groove",
	"Funk Central",
	"Stoke It",
	"Road Hog",
	"Aliens Ate My Railway",
	"Snarl Up",
	"Stroll On",
	"Can't Get There From Here",
	"Sawyer's Tune",
	"Hold That Train!",
	"Movin' On",
	"Goss Groove",
	"Small Town",
	"Broomer's Oil Rag",
	"Jammit",
	"Hard Drivin'"
};

static char *GetSpecialPlayerNameString(char *buff, int ind)
{
	switch (ind) {
	// not used
	case 1: {
		int i = GetParamInt32() & 0xFFFF;
		return str_cat(buff, _silly_company_names[i]);
	}

	case 2: // used for Foobar & Co company names
		return GenAndCoName(buff);

	case 3: // President name
		return GenPlayerName_4(buff);

	// song names
	case 4: {
		const char *song = _song_names[GetParamUint16() - 1];
		return str_cat(buff, song);
	}
	}

	// town name?
	if (IS_INT_INSIDE(ind - 6, 0, SPECSTR_TOWNNAME_LAST-SPECSTR_TOWNNAME_START + 1)) {
		buff = GetSpecialTownNameString(buff, ind - 6);
		return str_cat(buff, " Transport");
	}

	// language name?
	if (IS_INT_INSIDE(ind, (SPECSTR_LANGUAGE_START - 0x70E4), (SPECSTR_LANGUAGE_END - 0x70E4) + 1)) {
		int i = ind - (SPECSTR_LANGUAGE_START - 0x70E4);
		return str_cat(buff, i == _dynlang.curr ? ((LanguagePackHeader*)_langpack)->own_name : _dynlang.ent[i].name);
	}

	// resolution size?
	if (IS_INT_INSIDE(ind, (SPECSTR_RESOLUTION_START - 0x70E4), (SPECSTR_RESOLUTION_END - 0x70E4) + 1)) {
		int i = ind - (SPECSTR_RESOLUTION_START - 0x70E4);
		return buff + sprintf(buff, "%dx%d", _resolutions[i][0], _resolutions[i][1]);
	}

	// screenshot format name?
	if (IS_INT_INSIDE(ind, (SPECSTR_SCREENSHOT_START - 0x70E4), (SPECSTR_SCREENSHOT_END - 0x70E4) + 1)) {
		int i = ind - (SPECSTR_SCREENSHOT_START - 0x70E4);
		return str_cat(buff, GetScreenshotFormatDesc(i));
	}

	assert(0);
	return NULL;
}

// remap a string ID from the old format to the new format
StringID RemapOldStringID(StringID s)
{
	if (s == 0x7000) s = STR_SV_UNNAMED;
	else if (s == 0x0006) s = STR_SV_EMPTY;
	else if (s == 0x8864) s = STR_SV_TRAIN_NAME;
	else if (s == 0x902B) s = STR_SV_ROADVEH_NAME;
	else if (s == 0x9830) s = STR_SV_SHIP_NAME;
	else if (s == 0xA02F) s = STR_SV_AIRCRAFT_NAME;
	else if (IS_INT_INSIDE(s, 0x300F, 0x3030)) s = s - 0x300F + STR_SV_STNAME;
	else if (s == 0x70E4 || s == 0x70E9) s = SPECSTR_PLAYERNAME_ENGLISH;
	return s;
}

bool ReadLanguagePack(int lang_index)
{
	int tot_count, i;
	char *lang_pack;
	size_t len;
	char **langpack_offs;
	char *s;

#define HDR ((LanguagePackHeader*)lang_pack)
	{
		char *lang = str_fmt("%s%s", _path.lang_dir, _dynlang.ent[lang_index].file);
		lang_pack = ReadFileToMem(lang, &len, 100000);
		free(lang);
	}
	if (lang_pack == NULL) return false;
	if (len < sizeof(LanguagePackHeader) ||
			HDR->ident != TO_LE32(LANGUAGE_PACK_IDENT) ||
			HDR->version != TO_LE32(LANGUAGE_PACK_VERSION)) {
		free(lang_pack);
		return false;
	}
#undef HDR

#if defined(TTD_BIG_ENDIAN)
	for (i = 0; i != 32; i++) {
		((LanguagePackHeader*)lang_pack)->offsets[i] = READ_LE_UINT16(&((LanguagePackHeader*)lang_pack)->offsets[i]);
	}
#endif

	tot_count = 0;
	for (i = 0; i != 32; i++) {
		uint num = ((LanguagePackHeader*)lang_pack)->offsets[i];
		_langtab_start[i] = tot_count;
		_langtab_num[i] = num;
		tot_count += num;
	}

	// Allocate offsets
	langpack_offs = malloc(tot_count * sizeof(*langpack_offs));

	// Fill offsets
	s = lang_pack + sizeof(LanguagePackHeader);
	for (i = 0; i != tot_count; i++) {
		len = (byte)*s;
		*s++ = '\0'; // zero terminate the string before.
		if (len >= 0xC0) len = ((len & 0x3F) << 8) + (byte)*s++;
		langpack_offs[i] = s;
		s += len;
	}

	if (_langpack) free(_langpack);
	_langpack = lang_pack;

	if (_langpack_offs) free(_langpack_offs);
	_langpack_offs = langpack_offs;

	ttd_strlcpy(_dynlang.curr_file, _dynlang.ent[lang_index].file, sizeof(_dynlang.curr_file));


	_dynlang.curr = lang_index;
	return true;
}

// make a list of the available language packs. put the data in _dynlang struct.
void InitializeLanguagePacks(void)
{
	DynamicLanguages *dl = &_dynlang;
	int i, j, n, m,def;
	LanguagePackHeader hdr;
	FILE *in;
	char *files[32];

	n = GetLanguageList(files, lengthof(files));

	def = 0; // default language file

	// go through the language files and make sure that they are valid.
	for (i = m = 0; i != n; i++) {
		char *s = str_fmt("%s%s", _path.lang_dir, files[i]);
		in = fopen(s, "rb");
		free(s);
		if (in == NULL ||
				(j = fread(&hdr, sizeof(hdr), 1, in), fclose(in), j) != 1 ||
				hdr.ident != TO_LE32(LANGUAGE_PACK_IDENT) ||
				hdr.version != TO_LE32(LANGUAGE_PACK_VERSION)) {
			free(files[i]);
			continue;
		}

		dl->ent[m].file = files[i];
		dl->ent[m].name = strdup(hdr.name);

		if (strcmp(hdr.name, "English") == 0) def = m;

		m++;
	}

	if (m == 0)
		error(n == 0 ? "No available language packs" : "Invalid version of language packs");

	dl->num = m;
	for (i = 0; i != dl->num; i++)
		dl->dropdown[i] = SPECSTR_LANGUAGE_START + i;
	dl->dropdown[i] = INVALID_STRING_ID;

	for (i = 0; i != dl->num; i++)
		if (strcmp(dl->ent[i].file, dl->curr_file) == 0) {
			def = i;
			break;
		}

	if (!ReadLanguagePack(def))
		error("can't read language pack '%s'", dl->ent[def].file);
}
