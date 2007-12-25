/* $Id$ */

#include "../stdafx.h"
#include "../string.h"
#include "../table/control_codes.h"
#include "../core/alloc_func.hpp"
#include "../core/endian_func.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#if (!defined(WIN32) && !defined(WIN64)) || defined(__CYGWIN__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#if defined WIN32 || defined __WATCOMC__
#include <direct.h>
#endif /* WIN32 || __WATCOMC__ */

#ifdef __MORPHOS__
#ifdef stderr
#undef stderr
#endif
#define stderr stdout
#endif /* __MORPHOS__ */

/* Compiles a list of strings into a compiled string list */

typedef void (*ParseCmdProc)(char *buf, int value);

struct LanguagePackHeader {
	uint32 ident;
	uint32 version;     // 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];      // the international name of this language
	char own_name[32];  // the localized name of this language
	char isocode[16];   // the ISO code for the language (not country code)
	uint16 offsets[32]; // the offsets
	byte plural_form;   // plural form index
	byte pad[3];        // pad header to be a multiple of 4
};

struct CmdStruct {
	const char *cmd;
	ParseCmdProc proc;
	long value;
	int8 consumes;
	byte flags;
};

enum {
	C_DONTCOUNT = 1,
	C_CASE      = 2,
};


struct Case {
	int caseidx;
	char *string;
	Case *next;
};

static bool _masterlang;
static bool _translated;
static const char* _file = "(unknown file)";
static int _cur_line;
static int _errors, _warnings, _show_todo;

struct LangString {
	char *name;            // Name of the string
	char *english;         // English text
	char *translated;      // Translated text
	uint16 hash_next;      // next hash entry
	uint16 index;
	int line;              // line of string in source-file
	Case *english_case;    // cases for english
	Case *translated_case; // cases for foreign
};

static LangString *_strings[65536];


#define HASH_SIZE 32767
static uint16 _hash_head[HASH_SIZE];

static byte _put_buf[4096];
static int _put_pos;
static int _next_string_id;

static uint32 _hash;
static char _lang_name[32], _lang_ownname[32], _lang_isocode[16];
static byte _lang_pluralform;
#define MAX_NUM_GENDER 8
static char _genders[MAX_NUM_GENDER][8];
static int _numgenders;

// contains the name of all cases.
#define MAX_NUM_CASES 50
static char _cases[MAX_NUM_CASES][16];
static int _numcases;

// for each plural value, this is the number of plural forms.
static const byte _plural_form_counts[] = { 2, 1, 2, 3, 3, 3, 3, 3, 4 };

static const char *_cur_ident;

struct CmdPair {
	const CmdStruct *a;
	const char *v;
};

struct ParsedCommandStruct {
	int np;
	CmdPair pairs[32];
	const CmdStruct *cmd[32]; // ordered by param #
};

// Used when generating some advanced commands.
static ParsedCommandStruct _cur_pcs;
static int _cur_argidx;

static uint HashStr(const char *s)
{
	uint hash = 0;
	for (; *s != '\0'; s++) hash = ROL(hash, 3) ^ *s;
	return hash % HASH_SIZE;
}

static void HashAdd(const char *s, LangString *ls)
{
	uint hash = HashStr(s);
	ls->hash_next = _hash_head[hash];
	_hash_head[hash] = ls->index + 1;
}

static LangString *HashFind(const char *s)
{
	int idx = _hash_head[HashStr(s)];

	while (--idx >= 0) {
		LangString* ls = _strings[idx];

		if (strcmp(ls->name, s) == 0) return ls;
		idx = ls->hash_next;
	}
	return NULL;
}

#ifdef _MSC_VER
# define LINE_NUM_FMT "(%d)"
#else
# define LINE_NUM_FMT ":%d"
#endif

static void CDECL warning(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);
	fprintf(stderr, "%s" LINE_NUM_FMT ": warning: %s\n", _file, _cur_line, buf);
	_warnings++;
}

void CDECL error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);
	fprintf(stderr, "%s" LINE_NUM_FMT ": error: %s\n", _file, _cur_line, buf);
	_errors++;
}


static void NORETURN CDECL fatal(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);
	fprintf(stderr, "%s" LINE_NUM_FMT ": FATAL: %s\n", _file, _cur_line, buf);
	exit(1);
}

static void PutByte(byte c)
{
	if (_put_pos == lengthof(_put_buf)) fatal("Put buffer too small");
	_put_buf[_put_pos++] = c;
}


static void PutUtf8(uint32 value)
{
	if (value < 0x80) {
		PutByte(value);
	} else if (value < 0x800) {
		PutByte(0xC0 + GB(value,  6, 5));
		PutByte(0x80 + GB(value,  0, 6));
	} else if (value < 0x10000) {
		PutByte(0xE0 + GB(value, 12, 4));
		PutByte(0x80 + GB(value,  6, 6));
		PutByte(0x80 + GB(value,  0, 6));
	} else if (value < 0x110000) {
		PutByte(0xF0 + GB(value, 18, 3));
		PutByte(0x80 + GB(value, 12, 6));
		PutByte(0x80 + GB(value,  6, 6));
		PutByte(0x80 + GB(value,  0, 6));
	} else {
		warning("Invalid unicode value U+0x%X", value);
	}
}


size_t Utf8Validate(const char *s)
{
	uint32 c;

	if (!HasBit(s[0], 7)) {
		/* 1 byte */
		return 1;
	} else if (GB(s[0], 5, 3) == 6 && IsUtf8Part(s[1])) {
		/* 2 bytes */
		c = GB(s[0], 0, 5) << 6 | GB(s[1], 0, 6);
		if (c >= 0x80) return 2;
	} else if (GB(s[0], 4, 4) == 14 && IsUtf8Part(s[1]) && IsUtf8Part(s[2])) {
		/* 3 bytes */
		c = GB(s[0], 0, 4) << 12 | GB(s[1], 0, 6) << 6 | GB(s[2], 0, 6);
		if (c >= 0x800) return 3;
	} else if (GB(s[0], 3, 5) == 30 && IsUtf8Part(s[1]) && IsUtf8Part(s[2]) && IsUtf8Part(s[3])) {
		/* 4 bytes */
		c = GB(s[0], 0, 3) << 18 | GB(s[1], 0, 6) << 12 | GB(s[2], 0, 6) << 6 | GB(s[3], 0, 6);
		if (c >= 0x10000 && c <= 0x10FFFF) return 4;
	}

	return 0;
}


static void EmitSingleChar(char *buf, int value)
{
	if (*buf != '\0') warning("Ignoring trailing letters in command");
	PutUtf8(value);
}


static void EmitSetX(char *buf, int value)
{
	char *err;
	int x = strtol(buf, &err, 0);
	if (*err != 0) fatal("SetX param invalid");
	PutUtf8(SCC_SETX);
	PutByte((byte)x);
}


static void EmitSetXY(char *buf, int value)
{
	char *err;
	int x;
	int y;

	x = strtol(buf, &err, 0);
	if (*err != ' ') fatal("SetXY param invalid");
	y = strtol(err + 1, &err, 0);
	if (*err != 0) fatal("SetXY param invalid");

	PutUtf8(SCC_SETXY);
	PutByte((byte)x);
	PutByte((byte)y);
}

// The plural specifier looks like
// {NUM} {PLURAL -1 passenger passengers} then it picks either passenger/passengers depending on the count in NUM

// This is encoded like
//  CommandByte <ARG#> <NUM> {Length of each string} {each string}

bool ParseRelNum(char **buf, int *value)
{
	const char* s = *buf;
	char* end;
	bool rel = false;
	int v;

	while (*s == ' ' || *s == '\t') s++;
	if (*s == '+') {
		rel = true;
		s++;
	}
	v = strtol(s, &end, 0);
	if (end == s) return false;
	if (rel || v < 0) {
		*value += v;
	} else {
		*value = v;
	}
	*buf = end;
	return true;
}

// Parse out the next word, or NULL
char *ParseWord(char **buf)
{
	char *s = *buf, *r;

	while (*s == ' ' || *s == '\t') s++;
	if (*s == '\0') return NULL;

	if (*s == '"') {
		r = ++s;
		// parse until next " or NUL
		for (;;) {
			if (*s == '\0') break;
			if (*s == '"') {
				*s++ = '\0';
				break;
			}
			s++;
		}
	} else {
		// proceed until whitespace or NUL
		r = s;
		for (;;) {
			if (*s == '\0') break;
			if (*s == ' ' || *s == '\t') {
				*s++ = '\0';
				break;
			}
			s++;
		}
	}
	*buf = s;
	return r;
}

// Forward declaration
static int TranslateArgumentIdx(int arg);

static void EmitWordList(const char* const* words, uint nw)
{
	uint i;
	uint j;

	PutByte(nw);
	for (i = 0; i < nw; i++) PutByte(strlen(words[i]));
	for (i = 0; i < nw; i++) {
		for (j = 0; words[i][j] != '\0'; j++) PutByte(words[i][j]);
	}
}

static void EmitPlural(char *buf, int value)
{
	int argidx = _cur_argidx;
	const char* words[5];
	int nw = 0;

	// Parse out the number, if one exists. Otherwise default to prev arg.
	if (!ParseRelNum(&buf, &argidx)) argidx--;

	// Parse each string
	for (nw = 0; nw < 5; nw++) {
		words[nw] = ParseWord(&buf);
		if (words[nw] == NULL) break;
	}

	if (nw == 0)
		fatal("%s: No plural words", _cur_ident);

	if (_plural_form_counts[_lang_pluralform] != nw) {
		if (_translated) {
			fatal("%s: Invalid number of plural forms. Expecting %d, found %d.", _cur_ident,
				_plural_form_counts[_lang_pluralform], nw);
		} else {
			if ((_show_todo & 2) != 0) warning("'%s' is untranslated. Tweaking english string to allow compilation for plural forms", _cur_ident);
			if (nw > _plural_form_counts[_lang_pluralform]) {
				nw = _plural_form_counts[_lang_pluralform];
			} else {
				for (; nw < _plural_form_counts[_lang_pluralform]; nw++) {
					words[nw] = words[nw - 1];
				}
			}
		}
	}

	PutUtf8(SCC_PLURAL_LIST);
	PutByte(TranslateArgumentIdx(argidx));
	EmitWordList(words, nw);
}


static void EmitGender(char *buf, int value)
{
	int argidx = _cur_argidx;
	uint nw;

	if (buf[0] == '=') {
		buf++;

		// This is a {G=DER} command
		for (nw = 0; ; nw++) {
			if (nw >= 8) fatal("G argument '%s' invalid", buf);
			if (strcmp(buf, _genders[nw]) == 0) break;
		}
		// now nw contains the gender index
		PutUtf8(SCC_GENDER_INDEX);
		PutByte(nw);
	} else {
		const char* words[8];

		// This is a {G 0 foo bar two} command.
		// If no relative number exists, default to +0
		if (!ParseRelNum(&buf, &argidx)) {}

		for (nw = 0; nw < 8; nw++) {
			words[nw] = ParseWord(&buf);
			if (words[nw] == NULL) break;
		}
		if (nw != _numgenders) fatal("Bad # of arguments for gender command");
		PutUtf8(SCC_GENDER_LIST);
		PutByte(TranslateArgumentIdx(argidx));
		EmitWordList(words, nw);
	}
}


static const CmdStruct _cmd_structs[] = {
	// Update position
	{"SETX",  EmitSetX,  SCC_SETX,  0, 0},
	{"SETXY", EmitSetXY, SCC_SETXY, 0, 0},

	// Font size
	{"TINYFONT", EmitSingleChar, SCC_TINYFONT, 0, 0},
	{"BIGFONT",  EmitSingleChar, SCC_BIGFONT,  0, 0},

	// Colors
	{"BLUE",    EmitSingleChar, SCC_BLUE,    0, 0},
	{"SILVER",  EmitSingleChar, SCC_SILVER,  0, 0},
	{"GOLD",    EmitSingleChar, SCC_GOLD,    0, 0},
	{"RED",     EmitSingleChar, SCC_RED,     0, 0},
	{"PURPLE",  EmitSingleChar, SCC_PURPLE,  0, 0},
	{"LTBROWN", EmitSingleChar, SCC_LTBROWN, 0, 0},
	{"ORANGE",  EmitSingleChar, SCC_ORANGE,  0, 0},
	{"GREEN",   EmitSingleChar, SCC_GREEN,   0, 0},
	{"YELLOW",  EmitSingleChar, SCC_YELLOW,  0, 0},
	{"DKGREEN", EmitSingleChar, SCC_DKGREEN, 0, 0},
	{"CREAM",   EmitSingleChar, SCC_CREAM,   0, 0},
	{"BROWN",   EmitSingleChar, SCC_BROWN,   0, 0},
	{"WHITE",   EmitSingleChar, SCC_WHITE,   0, 0},
	{"LTBLUE",  EmitSingleChar, SCC_LTBLUE,  0, 0},
	{"GRAY",    EmitSingleChar, SCC_GRAY,    0, 0},
	{"DKBLUE",  EmitSingleChar, SCC_DKBLUE,  0, 0},
	{"BLACK",   EmitSingleChar, SCC_BLACK,   0, 0},

	{"CURRCOMPACT",   EmitSingleChar, SCC_CURRENCY_COMPACT,    1, 0}, // compact currency
	{"REV",           EmitSingleChar, SCC_REVISION,            0, 0}, // openttd revision string
	{"SHORTCARGO",    EmitSingleChar, SCC_CARGO_SHORT,         2, 0}, // short cargo description, only ### tons, or ### litres

	{"STRING1", EmitSingleChar, SCC_STRING1, 1, C_CASE}, // included string that consumes ONE argument
	{"STRING2", EmitSingleChar, SCC_STRING2, 2, C_CASE}, // included string that consumes TWO arguments
	{"STRING3", EmitSingleChar, SCC_STRING3, 3, C_CASE}, // included string that consumes THREE arguments
	{"STRING4", EmitSingleChar, SCC_STRING4, 4, C_CASE}, // included string that consumes FOUR arguments
	{"STRING5", EmitSingleChar, SCC_STRING5, 5, C_CASE}, // included string that consumes FIVE arguments

	{"STATIONFEATURES", EmitSingleChar, SCC_STATION_FEATURES, 1, 0}, // station features string, icons of the features
	{"INDUSTRY",        EmitSingleChar, SCC_INDUSTRY_NAME,    1, 0}, // industry, takes an industry #
	{"CARGO",           EmitSingleChar, SCC_CARGO,            2, 0},
	{"POWER",           EmitSingleChar, SCC_POWER,            1, 0},
	{"VOLUME",          EmitSingleChar, SCC_VOLUME,           1, 0},
	{"VOLUME_S",        EmitSingleChar, SCC_VOLUME_SHORT,     1, 0},
	{"WEIGHT",          EmitSingleChar, SCC_WEIGHT,           1, 0},
	{"WEIGHT_S",        EmitSingleChar, SCC_WEIGHT_SHORT,     1, 0},
	{"FORCE",           EmitSingleChar, SCC_FORCE,            1, 0},
	{"VELOCITY",        EmitSingleChar, SCC_VELOCITY,         1, 0},

	{"P", EmitPlural, 0, 0, C_DONTCOUNT}, // plural specifier
	{"G", EmitGender, 0, 0, C_DONTCOUNT}, // gender specifier

	{"DATE_TINY",  EmitSingleChar, SCC_DATE_TINY, 1, 0},
	{"DATE_SHORT", EmitSingleChar, SCC_DATE_SHORT, 1, 0},
	{"DATE_LONG",  EmitSingleChar, SCC_DATE_LONG, 1, 0},

	{"SKIP", EmitSingleChar, SCC_SKIP, 1, 0},

	{"STRING", EmitSingleChar, SCC_STRING, 1, C_CASE},

	// Numbers
	{"COMMA", EmitSingleChar, SCC_COMMA, 1, 0}, // Number with comma
	{"NUM",   EmitSingleChar, SCC_NUM,   1, 0}, // Signed number

	{"CURRENCY",   EmitSingleChar, SCC_CURRENCY,    1, 0},

	{"WAYPOINT", EmitSingleChar, SCC_WAYPOINT_NAME, 1, 0}, // waypoint name
	{"STATION",  EmitSingleChar, SCC_STATION_NAME,  1, 0},
	{"TOWN",     EmitSingleChar, SCC_TOWN_NAME,     1, 0},
	{"GROUP",    EmitSingleChar, SCC_GROUP_NAME,    1, 0},
	{"SIGN",     EmitSingleChar, SCC_SIGN_NAME,     1, 0},
	{"ENGINE",   EmitSingleChar, SCC_ENGINE_NAME,   1, 0},
	{"VEHICLE",  EmitSingleChar, SCC_VEHICLE_NAME,  1, 0},
	{"COMPANY",  EmitSingleChar, SCC_COMPANY_NAME,  1, 0},
	{"COMPANYNUM", EmitSingleChar, SCC_COMPANY_NUM, 1, 0},
	{"PLAYERNAME", EmitSingleChar, SCC_PLAYER_NAME, 1, 0},

	// 0x9D is used for the pseudo command SETCASE
	// 0x9E is used for case switching

	{"",               EmitSingleChar, '\n',               0, C_DONTCOUNT},
	{"{",              EmitSingleChar, '{',                0, C_DONTCOUNT},
	{"UPARROW",        EmitSingleChar, SCC_UPARROW,        0, 0},
	{"SMALLUPARROW",   EmitSingleChar, SCC_SMALLUPARROW,   0, 0},
	{"SMALLDOWNARROW", EmitSingleChar, SCC_SMALLDOWNARROW, 0, 0},
	{"TRAIN",          EmitSingleChar, SCC_TRAIN,          0, 0},
	{"LORRY",          EmitSingleChar, SCC_LORRY,          0, 0},
	{"BUS",            EmitSingleChar, SCC_BUS,            0, 0},
	{"PLANE",          EmitSingleChar, SCC_PLANE,          0, 0},
	{"SHIP",           EmitSingleChar, SCC_SHIP,           0, 0},
	{"NBSP",           EmitSingleChar, 0xA0,               0, C_DONTCOUNT},
	{"CENT",           EmitSingleChar, 0xA2,               0, C_DONTCOUNT},
	{"POUNDSIGN",      EmitSingleChar, 0xA3,               0, C_DONTCOUNT},
	{"EURO",           EmitSingleChar, 0x20AC,             0, C_DONTCOUNT},
	{"YENSIGN",        EmitSingleChar, 0xA5,               0, C_DONTCOUNT},
	{"COPYRIGHT",      EmitSingleChar, 0xA9,               0, C_DONTCOUNT},
	{"DOWNARROW",      EmitSingleChar, SCC_DOWNARROW,      0, C_DONTCOUNT},
	{"CHECKMARK",      EmitSingleChar, SCC_CHECKMARK,      0, C_DONTCOUNT},
	{"CROSS",          EmitSingleChar, SCC_CROSS,          0, C_DONTCOUNT},
	{"REGISTERED",     EmitSingleChar, 0xAE,               0, C_DONTCOUNT},
	{"RIGHTARROW",     EmitSingleChar, SCC_RIGHTARROW,     0, C_DONTCOUNT},
	{"SMALLLEFTARROW", EmitSingleChar, SCC_LESSTHAN,       0, C_DONTCOUNT},
	{"SMALLRIGHTARROW",EmitSingleChar, SCC_GREATERTHAN,    0, C_DONTCOUNT},
};


static const CmdStruct *FindCmd(const char *s, int len)
{
	const CmdStruct* cs;

	for (cs = _cmd_structs; cs != endof(_cmd_structs); cs++) {
		if (strncmp(cs->cmd, s, len) == 0 && cs->cmd[len] == '\0') return cs;
	}
	return NULL;
}

static uint ResolveCaseName(const char *str, uint len)
{
	uint i;

	for (i = 0; i < MAX_NUM_CASES; i++) {
		if (memcmp(_cases[i], str, len) == 0 && _cases[i][len] == 0) return i + 1;
	}
	fatal("Invalid case-name '%s'", str);
}


// returns NULL on eof
// else returns command struct
static const CmdStruct *ParseCommandString(const char **str, char *param, int *argno, int *casei)
{
	const char *s = *str, *start;
	const CmdStruct *cmd;
	byte c;

	*argno = -1;
	*casei = -1;

	// Scan to the next command, exit if there's no next command.
	for (; *s != '{'; s++) {
		if (*s == '\0') return NULL;
	}
	s++; // Skip past the {

	if (*s >= '0' && *s <= '9') {
		char *end;

		*argno = strtoul(s, &end, 0);
		if (*end != ':') fatal("missing arg #");
		s = end + 1;
	}

	// parse command name
	start = s;
	do {
		c = *s++;
	} while (c != '}' && c != ' ' && c != '=' && c != '.' && c != 0);

	cmd = FindCmd(start, s - start - 1);
	if (cmd == NULL) {
		error("Undefined command '%.*s'", s - start - 1, start);
		return NULL;
	}

	if (c == '.') {
		const char *casep = s;

		if (!(cmd->flags & C_CASE))
			fatal("Command '%s' can't have a case", cmd->cmd);

		do c = *s++; while (c != '}' && c != ' ' && c != '\0');
		*casei = ResolveCaseName(casep, s - casep - 1);
	}

	if (c == '\0') {
		error("Missing } from command '%s'", start);
		return NULL;
	}


	if (c != '}') {
		if (c == '=') s--;
		// copy params
		start = s;
		for (;;) {
			c = *s++;
			if (c == '}') break;
			if (c == '\0') {
				error("Missing } from command '%s'", start);
				return NULL;
			}
			if (s - start == 250) fatal("param command too long");
			*param++ = c;
		}
	}
	*param = '\0';

	*str = s;

	return cmd;
}


static void HandlePragma(char *str)
{
	if (!memcmp(str, "id ", 3)) {
		_next_string_id = strtoul(str + 3, NULL, 0);
	} else if (!memcmp(str, "name ", 5)) {
		ttd_strlcpy(_lang_name, str + 5, sizeof(_lang_name));
	} else if (!memcmp(str, "ownname ", 8)) {
		ttd_strlcpy(_lang_ownname, str + 8, sizeof(_lang_ownname));
	} else if (!memcmp(str, "isocode ", 8)) {
		ttd_strlcpy(_lang_isocode, str + 8, sizeof(_lang_isocode));
	} else if (!memcmp(str, "plural ", 7)) {
		_lang_pluralform = atoi(str + 7);
		if (_lang_pluralform >= lengthof(_plural_form_counts))
			fatal("Invalid pluralform %d", _lang_pluralform);
	} else if (!memcmp(str, "gender ", 7)) {
		char* buf = str + 7;

		for (;;) {
			const char* s = ParseWord(&buf);

			if (s == NULL) break;
			if (_numgenders >= MAX_NUM_GENDER) fatal("Too many genders, max %d", MAX_NUM_GENDER);
			ttd_strlcpy(_genders[_numgenders], s, sizeof(_genders[_numgenders]));
			_numgenders++;
		}
	} else if (!memcmp(str, "case ", 5)) {
		char* buf = str + 5;

		for (;;) {
			const char* s = ParseWord(&buf);

			if (s == NULL) break;
			if (_numcases >= MAX_NUM_CASES) fatal("Too many cases, max %d", MAX_NUM_CASES);
			ttd_strlcpy(_cases[_numcases], s, sizeof(_cases[_numcases]));
			_numcases++;
		}
	} else {
		fatal("unknown pragma '%s'", str);
	}
}

static void ExtractCommandString(ParsedCommandStruct* p, const char* s, bool warnings)
{
	char param[100];
	int argno;
	int argidx = 0;
	int casei;

	memset(p, 0, sizeof(*p));

	for (;;) {
		// read until next command from a.
		const CmdStruct* ar = ParseCommandString(&s, param, &argno, &casei);

		if (ar == NULL) break;

		// Sanity checking
		if (argno != -1 && ar->consumes == 0) fatal("Non consumer param can't have a paramindex");

		if (ar->consumes) {
			if (argno != -1) argidx = argno;
			if (argidx < 0 || argidx >= lengthof(p->cmd)) fatal("invalid param idx %d", argidx);
			if (p->cmd[argidx] != NULL && p->cmd[argidx] != ar) fatal("duplicate param idx %d", argidx);

			p->cmd[argidx++] = ar;
		} else if (!(ar->flags & C_DONTCOUNT)) { // Ignore some of them
			if (p->np >= lengthof(p->pairs)) fatal("too many commands in string, max %d", lengthof(p->pairs));
			p->pairs[p->np].a = ar;
			p->pairs[p->np].v = param[0] != '\0' ? strdup(param) : "";
			p->np++;
		}
	}
}


static const CmdStruct *TranslateCmdForCompare(const CmdStruct *a)
{
	if (a == NULL) return NULL;

	if (strcmp(a->cmd, "STRING1") == 0 ||
			strcmp(a->cmd, "STRING2") == 0 ||
			strcmp(a->cmd, "STRING3") == 0 ||
			strcmp(a->cmd, "STRING4") == 0 ||
			strcmp(a->cmd, "STRING5") == 0) {
		return FindCmd("STRING", 6);
	}

	if (strcmp(a->cmd, "SKIP") == 0) return NULL;

	return a;
}


static bool CheckCommandsMatch(char *a, char *b, const char *name)
{
	ParsedCommandStruct templ;
	ParsedCommandStruct lang;
	int i, j;
	bool result = true;

	ExtractCommandString(&templ, b, true);
	ExtractCommandString(&lang, a, true);

	// For each string in templ, see if we find it in lang
	if (templ.np != lang.np) {
		warning("%s: template string and language string have a different # of commands", name);
		result = false;
	}

	for (i = 0; i < templ.np; i++) {
		// see if we find it in lang, and zero it out
		bool found = false;
		for (j = 0; j < lang.np; j++) {
			if (templ.pairs[i].a == lang.pairs[j].a &&
					strcmp(templ.pairs[i].v, lang.pairs[j].v) == 0) {
				// it was found in both. zero it out from lang so we don't find it again
				lang.pairs[j].a = NULL;
				found = true;
				break;
			}
		}

		if (!found) {
			warning("%s: command '%s' exists in template file but not in language file", name, templ.pairs[i].a->cmd);
			result = false;
		}
	}

	// if we reach here, all non consumer commands match up.
	// Check if the non consumer commands match up also.
	for (i = 0; i < lengthof(templ.cmd); i++) {
		if (TranslateCmdForCompare(templ.cmd[i]) != TranslateCmdForCompare(lang.cmd[i])) {
			warning("%s: Param idx #%d '%s' doesn't match with template command '%s'", name, i,
				lang.cmd[i]  == NULL ? "<empty>" : lang.cmd[i]->cmd,
				templ.cmd[i] == NULL ? "<empty>" : templ.cmd[i]->cmd);
			result = false;
		}
	}

	return result;
}

static void HandleString(char *str, bool master)
{
	char *s,*t;
	LangString *ent;
	char *casep;

	if (*str == '#') {
		if (str[1] == '#' && str[2] != '#') HandlePragma(str + 2);
		return;
	}

	// Ignore comments & blank lines
	if (*str == ';' || *str == ' ' || *str == '\0') return;

	s = strchr(str, ':');
	if (s == NULL) {
		error("Line has no ':' delimiter");
		return;
	}

	// Trim spaces.
	// After this str points to the command name, and s points to the command contents
	for (t = s; t > str && (t[-1] == ' ' || t[-1] == '\t'); t--);
	*t = 0;
	s++;

	/* Check string is valid UTF-8 */
	{
		const char *tmp;
		for (tmp = s; *tmp != '\0';) {
			size_t len = Utf8Validate(tmp);
			if (len == 0) fatal("Invalid UTF-8 sequence in '%s'", s);
			tmp += len;
		}
	}

	// Check if the string has a case..
	// The syntax for cases is IDENTNAME.case
	casep = strchr(str, '.');
	if (casep) *casep++ = 0;

	// Check if this string already exists..
	ent = HashFind(str);

	if (master) {
		if (ent != NULL && casep == NULL) {
			error("String name '%s' is used multiple times", str);
			return;
		}

		if (ent == NULL && casep != NULL) {
			error("Base string name '%s' doesn't exist yet. Define it before defining a case.", str);
			return;
		}

		if (ent == NULL) {
			if (_strings[_next_string_id]) {
				error("String ID 0x%X for '%s' already in use by '%s'", ent, str, _strings[_next_string_id]->name);
				return;
			}

			// Allocate a new LangString
			ent = CallocT<LangString>(1);
			_strings[_next_string_id] = ent;
			ent->index = _next_string_id++;
			ent->name = strdup(str);
			ent->line = _cur_line;

			HashAdd(str, ent);
		}

		if (casep != NULL) {
			Case* c = MallocT<Case>(1);

			c->caseidx = ResolveCaseName(casep, strlen(casep));
			c->string = strdup(s);
			c->next = ent->english_case;
			ent->english_case = c;
		} else {
			ent->english = strdup(s);
		}

	} else {
		if (ent == NULL) {
			warning("String name '%s' does not exist in master file", str);
			return;
		}

		if (ent->translated && casep == NULL) {
			error("String name '%s' is used multiple times", str);
			return;
		}

		if (s[0] == ':' && s[1] == '\0' && casep == NULL) {
			// Special syntax :: means we should just inherit the master string
			ent->translated = strdup(ent->english);
		} else {
			// make sure that the commands match
			if (!CheckCommandsMatch(s, ent->english, str)) return;

			if (casep != NULL) {
				Case* c = MallocT<Case>(1);

				c->caseidx = ResolveCaseName(casep, strlen(casep));
				c->string = strdup(s);
				c->next = ent->translated_case;
				ent->translated_case = c;
			} else {
				ent->translated = strdup(s);
			}
		}
	}
}


static void rstrip(char *buf)
{
	int i = strlen(buf);
	while (i > 0 && (buf[i - 1] == '\r' || buf[i - 1] == '\n' || buf[i - 1] == ' ')) i--;
	buf[i] = '\0';
}


static void ParseFile(const char *file, bool english)
{
	FILE *in;
	char buf[2048];

	_file = file;

	/* For each new file we parse, reset the genders, and language codes */
	_numgenders = 0;
	_lang_name[0] = _lang_ownname[0] = _lang_isocode[0] = '\0';
	// TODO:!! We can't reset the cases. In case the translated strings
	// derive some strings from english....

	in = fopen(file, "r");
	if (in == NULL) fatal("Cannot open file");
	_cur_line = 1;
	while (fgets(buf, sizeof(buf), in) != NULL) {
		rstrip(buf);
		HandleString(buf, english);
		_cur_line++;
	}
	fclose(in);

	if (StrEmpty(_lang_name) || StrEmpty(_lang_ownname) || StrEmpty(_lang_isocode)) {
		fatal("Language must include ##name, ##ownname and ##isocode");
	}
}


static uint32 MyHashStr(uint32 hash, const char *s)
{
	for (; *s != '\0'; s++) {
		hash = ROL(hash, 3) ^ *s;
		hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
	}
	return hash;
}


// make a hash of the file to get a unique "version number"
static void MakeHashOfStrings()
{
	uint32 hash = 0;
	uint i;

	for (i = 0; i != lengthof(_strings); i++) {
		const LangString* ls = _strings[i];

		if (ls != NULL) {
			const CmdStruct* cs;
			const char* s;
			char buf[256];
			int argno;
			int casei;

			s = ls->name;
			hash ^= i * 0x717239;
			hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
			hash = MyHashStr(hash, s + 1);

			s = ls->english;
			while ((cs = ParseCommandString(&s, buf, &argno, &casei)) != NULL) {
				if (cs->flags & C_DONTCOUNT) continue;

				hash ^= (cs - _cmd_structs) * 0x1234567;
				hash = (hash & 1 ? hash >> 1 ^ 0xF00BAA4 : hash >> 1);
			}
		}
	}
	_hash = hash;
}


static uint CountInUse(uint grp)
{
	int i;

	for (i = 0x800; --i >= 0;) if (_strings[(grp << 11) + i] != NULL) break;
	return i + 1;
}


bool CompareFiles(const char *n1, const char *n2)
{
	FILE *f1, *f2;
	char b1[4096];
	char b2[4096];
	size_t l1, l2;

	f2 = fopen(n2, "rb");
	if (f2 == NULL) return false;

	f1 = fopen(n1, "rb");
	if (f1 == NULL) fatal("can't open %s", n1);

	do {
		l1 = fread(b1, 1, sizeof(b1), f1);
		l2 = fread(b2, 1, sizeof(b2), f2);

		if (l1 != l2 || memcmp(b1, b2, l1)) {
			fclose(f2);
			fclose(f1);
			return false;
		}
	} while (l1);

	fclose(f2);
	fclose(f1);
	return true;
}


static void WriteStringsH(const char *filename)
{
	FILE *out;
	int i;
	int next = -1;

	out = fopen("tmp.xxx", "w");
	if (out == NULL) fatal("can't open tmp.xxx");

	fprintf(out, "/* This file is automatically generated. Do not modify */\n\n");
	fprintf(out, "#ifndef TABLE_STRINGS_H\n");
	fprintf(out, "#define TABLE_STRINGS_H\n");

	for (i = 0; i != lengthof(_strings); i++) {
		if (_strings[i] != NULL) {
			if (next != i) fprintf(out, "\n");
			fprintf(out, "static const StringID %s = 0x%X;\n", _strings[i]->name, i);
			next = i + 1;
		}
	}

	fprintf(out, "\nstatic const StringID STR_LAST_STRINGID = 0x%X;\n", next - 1);

	fprintf(out,
		"\nenum {\n"
		"\tLANGUAGE_PACK_IDENT = 0x474E414C, // Big Endian value for 'LANG' (LE is 0x 4C 41 4E 47)\n"
		"\tLANGUAGE_PACK_VERSION = 0x%X,\n"
		"};\n", (uint)_hash
	);

	fprintf(out, "\n#endif /* TABLE_STRINGS_H */\n");

	fclose(out);

	if (CompareFiles("tmp.xxx", filename)) {
		// files are equal. tmp.xxx is not needed
		unlink("tmp.xxx");
	} else {
		// else rename tmp.xxx into filename
#if defined(WIN32) || defined(WIN64)
		unlink(filename);
#endif
		if (rename("tmp.xxx", filename) == -1) fatal("rename() failed");
	}
}

static int TranslateArgumentIdx(int argidx)
{
	int i, sum;

	if (argidx < 0 || argidx >= lengthof(_cur_pcs.cmd))
		fatal("invalid argidx %d", argidx);

	for (i = sum = 0; i < argidx; i++) {
		const CmdStruct *cs = _cur_pcs.cmd[i];
		sum += (cs != NULL) ? cs->consumes : 1;
	}

	return sum;
}

static void PutArgidxCommand()
{
	PutUtf8(SCC_ARG_INDEX);
	PutByte(TranslateArgumentIdx(_cur_argidx));
}


static void PutCommandString(const char *str)
{
	const CmdStruct *cs;
	char param[256];
	int argno;
	int casei;

	_cur_argidx = 0;

	while (*str != '\0') {
		// Process characters as they are until we encounter a {
		if (*str != '{') {
			PutByte(*str++);
			continue;
		}
		cs = ParseCommandString(&str, param, &argno, &casei);
		if (cs == NULL) break;

		if (casei != -1) {
			PutUtf8(SCC_SETCASE); // {SETCASE}
			PutByte(casei);
		}

		// For params that consume values, we need to handle the argindex properly
		if (cs->consumes > 0) {
			// Check if we need to output a move-param command
			if (argno != -1 && argno != _cur_argidx) {
				_cur_argidx = argno;
				PutArgidxCommand();
			}

			// Output the one from the master string... it's always accurate.
			cs = _cur_pcs.cmd[_cur_argidx++];
			if (cs == NULL) {
				fatal("%s: No argument exists at position %d", _cur_ident, _cur_argidx - 1);
			}
		}

		cs->proc(param, cs->value);
	}
}

static void WriteLength(FILE *f, uint length)
{
	if (length < 0xC0) {
		fputc(length, f);
	} else if (length < 0x4000) {
		fputc((length >> 8) | 0xC0, f);
		fputc(length & 0xFF, f);
	} else {
		fatal("string too long");
	}
}


static void WriteLangfile(const char *filename)
{
	FILE *f;
	uint in_use[32];
	LanguagePackHeader hdr;
	uint i;
	uint j;

	f = fopen(filename, "wb");
	if (f == NULL) fatal("can't open %s", filename);

	memset(&hdr, 0, sizeof(hdr));
	for (i = 0; i != 32; i++) {
		uint n = CountInUse(i);

		in_use[i] = n;
		hdr.offsets[i] = TO_LE16(n);
	}

	// see line 655: fprintf(..."\tLANGUAGE_PACK_IDENT = 0x474E414C,...)
	hdr.ident = TO_LE32(0x474E414C); // Big Endian value for 'LANG'
	hdr.version = TO_LE32(_hash);
	hdr.plural_form = _lang_pluralform;
	strcpy(hdr.name, _lang_name);
	strcpy(hdr.own_name, _lang_ownname);
	strcpy(hdr.isocode, _lang_isocode);

	fwrite(&hdr, sizeof(hdr), 1, f);

	for (i = 0; i != 32; i++) {
		for (j = 0; j != in_use[i]; j++) {
			const LangString* ls = _strings[(i << 11) + j];
			const Case* casep;
			const char* cmdp;

			// For undefined strings, just set that it's an empty string
			if (ls == NULL) {
				WriteLength(f, 0);
				continue;
			}

			_cur_ident = ls->name;
			_cur_line = ls->line;

			// Produce a message if a string doesn't have a translation.
			if (_show_todo > 0 && ls->translated == NULL) {
				if ((_show_todo & 2) != 0) {
					warning("'%s' is untranslated", ls->name);
				}
				if ((_show_todo & 1) != 0) {
					const char *s = "<TODO> ";
					while (*s != '\0') PutByte(*s++);
				}
			}

			// Extract the strings and stuff from the english command string
			ExtractCommandString(&_cur_pcs, ls->english, false);

			if (ls->translated_case != NULL || ls->translated != NULL) {
				casep = ls->translated_case;
				cmdp = ls->translated;
			} else {
				casep = ls->english_case;
				cmdp = ls->english;
			}

			_translated = _masterlang || (cmdp != ls->english);

			if (casep != NULL) {
				const Case* c;
				uint num;

				// Need to output a case-switch.
				// It has this format
				// <0x9E> <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
				// Each LEN is printed using 2 bytes in big endian order.
				PutUtf8(SCC_SWITCH_CASE);
				// Count the number of cases
				for (num = 0, c = casep; c; c = c->next) num++;
				PutByte(num);

				// Write each case
				for (c = casep; c != NULL; c = c->next) {
					int pos;

					PutByte(c->caseidx);
					// Make some space for the 16-bit length
					pos = _put_pos;
					PutByte(0);
					PutByte(0);
					// Write string
					PutCommandString(c->string);
					PutByte(0); // terminate with a zero
					// Fill in the length
					_put_buf[pos + 0] = GB(_put_pos - (pos + 2), 8, 8);
					_put_buf[pos + 1] = GB(_put_pos - (pos + 2), 0, 8);
				}
			}

			if (cmdp != NULL) PutCommandString(cmdp);

			WriteLength(f, _put_pos);
			fwrite(_put_buf, 1, _put_pos, f);
			_put_pos = 0;
		}
	}

	fputc(0, f);
	fclose(f);
}

/** Multi-OS mkdirectory function */
static inline void ottd_mkdir(const char *directory)
{
#if defined(WIN32) || defined(__WATCOMC__)
		mkdir(directory);
#else
		mkdir(directory, 0755);
#endif
}

/** Create a path consisting of an already existing path, a possible
 * path seperator and the filename. The seperator is only appended if the path
 * does not already end with a seperator */
static inline char *mkpath(char *buf, size_t buflen, const char *path, const char *file)
{
	char *p;
	ttd_strlcpy(buf, path, buflen); // copy directory into buffer

	p = strchr(buf, '\0'); // add path seperator if necessary
	if (p[-1] != PATHSEPCHAR && (size_t)(p - buf) + 1 < buflen) *p++ = PATHSEPCHAR;
	ttd_strlcpy(p, file, buflen - (size_t)(p - buf)); // catenate filename at end of buffer
	return buf;
}

#if defined(__MINGW32__)
/**
 * On MingW, it is common that both / as \ are accepted in the
 * params. To go with those flow, we rewrite all incoming /
 * simply to \, so internally we can safely assume \.
 */
static inline char *replace_pathsep(char *s)
{
	char *c;

	for (c = s; *c != '\0'; c++) if (*c == '/') *c = '\\';
	return s;
}
#else
static inline char *replace_pathsep(char *s) { return s; }
#endif

int CDECL main(int argc, char* argv[])
{
	char pathbuf[256];
	const char *src_dir = ".";
	const char *dest_dir = NULL;

	while (argc > 1 && *argv[1] == '-') {
		if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
			puts("$Revision$");
			return 0;
		}

		if (strcmp(argv[1], "-t") == 0 || strcmp(argv[1], "--todo") == 0) {
			_show_todo |= 1;
			argc--, argv++;
			continue;
		}

		if (strcmp(argv[1], "-w") == 0 || strcmp(argv[1], "--warning") == 0) {
			_show_todo |= 2;
			argc--, argv++;
			continue;
		}

		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0) {
			puts(
				"strgen - $Revision$\n"
				" -v | --version    print version information and exit\n"
				" -t | --todo       replace any untranslated strings with '<TODO>'\n"
				" -w | --warning    print a warning for any untranslated strings\n"
				" -h | -? | --help  print this help message and exit\n"
				" -s | --source_dir search for english.txt in the specified directory\n"
				" -d | --dest_dir   put output file in the specified directory, create if needed\n"
				" Run without parameters and strgen will search for english.txt and parse it,\n"
				" creating strings.h. Passing an argument, strgen will translate that language\n"
				" file using english.txt as a reference and output <language>.lng."
			);
			return 0;
		}

		if (argc > 2 && (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--source_dir") == 0)) {
			src_dir = replace_pathsep(argv[2]);
			argc -= 2, argv += 2;
			continue;
		}

		if (argc > 2 && (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--dest_dir") == 0)) {
			dest_dir = replace_pathsep(argv[2]);
			argc -= 2, argv += 2;
			continue;
		}

		fprintf(stderr, "Invalid arguments\n");
		return 0;
	}

	if (dest_dir == NULL) dest_dir = src_dir; // if dest_dir is not specified, it equals src_dir

	/* strgen has two modes of operation. If no (free) arguments are passed
	 * strgen generates strings.h to the destination directory. If it is supplied
	 * with a (free) parameter the program will translate that language to destination
	 * directory. As input english.txt is parsed from the source directory */
	if (argc == 1) {
		mkpath(pathbuf, lengthof(pathbuf), src_dir, "english.txt");

		/* parse master file */
		_masterlang = true;
		ParseFile(pathbuf, true);
		MakeHashOfStrings();
		if (_errors) return 1;

		/* write strings.h */
		ottd_mkdir(dest_dir);
		mkpath(pathbuf, lengthof(pathbuf), dest_dir, "strings.h");
		WriteStringsH(pathbuf);
	} else if (argc == 2) {
		char *r;

		mkpath(pathbuf, lengthof(pathbuf), src_dir, "english.txt");

		/* parse master file and check if target file is correct */
		_masterlang = false;
		ParseFile(pathbuf, true);
		MakeHashOfStrings();
		ParseFile(replace_pathsep(argv[1]), false); // target file
		if (_errors) return 1;

		/* get the targetfile, strip any directories and append to destination path */
		r = strrchr(argv[1], PATHSEPCHAR);
		mkpath(pathbuf, lengthof(pathbuf), dest_dir, (r != NULL) ? &r[1] : argv[1]);

		/* rename the .txt (input-extension) to .lng */
		r = strrchr(pathbuf, '.');
		if (r == NULL || strcmp(r, ".txt") != 0) r = strchr(pathbuf, '\0');
		ttd_strlcpy(r, ".lng", (size_t)(r - pathbuf));
		WriteLangfile(pathbuf);

		/* if showing warnings, print a summary of the language */
		if ((_show_todo & 2) != 0) {
			fprintf(stdout, "%d warnings and %d errors for %s\n", _warnings, _errors, pathbuf);
		}
	} else {
		fprintf(stderr, "Invalid arguments\n");
	}

	return 0;
}
