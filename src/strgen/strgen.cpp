/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen.cpp Tool to create computer readable (stand-alone) translation files. */

#include "../stdafx.h"
#include "../core/alloc_func.hpp"
#include "../core/endian_func.hpp"
#include "../core/math_func.hpp"
#include "../core/mem_func.hpp"
#include "../string_func.h"
#include "../strings_type.h"
#include "../language.h"
#include "../table/control_codes.h"

#include <stdarg.h>

#if (!defined(WIN32) && !defined(WIN64)) || defined(__CYGWIN__)
#include <unistd.h>
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

#include "../table/strgen_tables.h"

/* Compiles a list of strings into a compiled string list */

struct Case {
	int caseidx;
	char *string;
	Case *next;
};

static bool _masterlang;                     ///< Whether we are loading the master language
static bool _translated;                     ///< Whether the current language is not the master language
static bool _translation;                    ///< Is the current file actually a translation or not
static const char *_file = "(unknown file)"; ///< The filename of the input, so we can refer to it in errors/warnings
static FILE *_output_file = NULL;            ///< The file we are currently writing output to
static const char *_output_filename = NULL;  ///< The filename of the output, so we can delete it if compilation fails
static int _cur_line;                        ///< The current line we're parsing in the input file
static int _errors, _warnings, _show_todo;

static const ptrdiff_t MAX_COMMAND_PARAM_SIZE = 100; ///< Maximum size of every command block, not counting the name of the command itself

struct LangString {
	char *name;            // Name of the string
	char *english;         // English text
	char *translated;      // Translated text
	uint16 hash_next;      // next hash entry
	uint16 index;
	int line;              // line of string in source-file
	Case *translated_case; // cases for foreign
};

static LangString *_strings[65536];
static LanguagePackHeader _lang; ///< Header information about a language.


#define HASH_SIZE 32767
static uint16 _hash_head[HASH_SIZE];

static byte _put_buf[4096];
static uint _put_pos;
static int _next_string_id;

static uint32 _hash;

static const char *_cur_ident;

struct CmdPair {
	const CmdStruct *a;
	const char *v;
};

struct ParsedCommandStruct {
	uint np;
	CmdPair pairs[32];
	const CmdStruct *cmd[32]; // ordered by param #
};

/* Used when generating some advanced commands. */
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
		LangString *ls = _strings[idx];

		if (strcmp(ls->name, s) == 0) return ls;
		idx = ls->hash_next;
	}
	return NULL;
}

#ifdef _MSC_VER
# define LINE_NUM_FMT(s) "%s (%d): warning: %s (" s ")\n"
#else
# define LINE_NUM_FMT(s) "%s:%d: " s ": %s\n"
#endif

static void CDECL strgen_warning(const char *s, ...) WARN_FORMAT(1, 2);

static void CDECL strgen_warning(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("warning"), _file, _cur_line, buf);
	_warnings++;
}

static void CDECL strgen_error(const char *s, ...) WARN_FORMAT(1, 2);

static void CDECL strgen_error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("error"), _file, _cur_line, buf);
	_errors++;
}

void NORETURN CDECL error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("FATAL"), _file, _cur_line, buf);
#ifdef _MSC_VER
	fprintf(stderr, LINE_NUM_FMT("warning"), _file, _cur_line, "language is not compiled");
#endif
	/* We were writing output to a file, remove it. */
	if (_output_file != NULL) {
		fclose(_output_file);
		unlink(_output_filename);
	}
	exit(1);
}

static void PutByte(byte c)
{
	if (_put_pos >= lengthof(_put_buf)) error("Put buffer too small");
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
		strgen_warning("Invalid unicode value U+0x%X", value);
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
	if (*buf != '\0') strgen_warning("Ignoring trailing letters in command");
	PutUtf8(value);
}


/* The plural specifier looks like
 * {NUM} {PLURAL -1 passenger passengers} then it picks either passenger/passengers depending on the count in NUM */

/* This is encoded like
 *  CommandByte <ARG#> <NUM> {Length of each string} {each string} */

bool ParseRelNum(char **buf, int *value, int *offset)
{
	const char *s = *buf;
	char *end;
	bool rel = false;

	while (*s == ' ' || *s == '\t') s++;
	if (*s == '+') {
		rel = true;
		s++;
	}
	int v = strtol(s, &end, 0);
	if (end == s) return false;
	if (rel || v < 0) {
		*value += v;
	} else {
		*value = v;
	}
	if (offset != NULL && *end == ':') {
		/* Take the Nth within */
		s = end + 1;
		*offset = strtol(s, &end, 0);
		if (end == s) return false;
	}
	*buf = end;
	return true;
}

/* Parse out the next word, or NULL */
char *ParseWord(char **buf)
{
	char *s = *buf, *r;

	while (*s == ' ' || *s == '\t') s++;
	if (*s == '\0') return NULL;

	if (*s == '"') {
		r = ++s;
		/* parse until next " or NUL */
		for (;;) {
			if (*s == '\0') break;
			if (*s == '"') {
				*s++ = '\0';
				break;
			}
			s++;
		}
	} else {
		/* proceed until whitespace or NUL */
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

/* Forward declaration */
static int TranslateArgumentIdx(int arg, int offset = 0);

static void EmitWordList(const char * const *words, uint nw)
{
	PutByte(nw);
	for (uint i = 0; i < nw; i++) PutByte(strlen(words[i]) + 1);
	for (uint i = 0; i < nw; i++) {
		for (uint j = 0; words[i][j] != '\0'; j++) PutByte(words[i][j]);
		PutByte(0);
	}
}

static void EmitPlural(char *buf, int value)
{
	int argidx = _cur_argidx;
	int offset = 0;
	const char *words[5];
	int nw = 0;

	/* Parse out the number, if one exists. Otherwise default to prev arg. */
	if (!ParseRelNum(&buf, &argidx, &offset)) argidx--;

	/* Parse each string */
	for (nw = 0; nw < 5; nw++) {
		words[nw] = ParseWord(&buf);
		if (words[nw] == NULL) break;
	}

	if (nw == 0) {
		error("%s: No plural words", _cur_ident);
	}

	if (_plural_forms[_lang.plural_form].plural_count != nw) {
		if (_translated) {
			error("%s: Invalid number of plural forms. Expecting %d, found %d.", _cur_ident,
				_plural_forms[_lang.plural_form].plural_count, nw);
		} else {
			if ((_show_todo & 2) != 0) strgen_warning("'%s' is untranslated. Tweaking english string to allow compilation for plural forms", _cur_ident);
			if (nw > _plural_forms[_lang.plural_form].plural_count) {
				nw = _plural_forms[_lang.plural_form].plural_count;
			} else {
				for (; nw < _plural_forms[_lang.plural_form].plural_count; nw++) {
					words[nw] = words[nw - 1];
				}
			}
		}
	}

	PutUtf8(SCC_PLURAL_LIST);
	PutByte(_lang.plural_form);
	PutByte(TranslateArgumentIdx(argidx, offset));
	EmitWordList(words, nw);
}


static void EmitGender(char *buf, int value)
{
	int argidx = _cur_argidx;
	int offset = 0;
	uint nw;

	if (buf[0] == '=') {
		buf++;

		/* This is a {G=DER} command */
		nw = _lang.GetGenderIndex(buf);
		if (nw >= MAX_NUM_GENDERS) error("G argument '%s' invalid", buf);

		/* now nw contains the gender index */
		PutUtf8(SCC_GENDER_INDEX);
		PutByte(nw);
	} else {
		const char *words[MAX_NUM_GENDERS];

		/* This is a {G 0 foo bar two} command.
		 * If no relative number exists, default to +0 */
		if (!ParseRelNum(&buf, &argidx, &offset)) {}

		const CmdStruct *cmd = _cur_pcs.cmd[argidx];
		if (cmd == NULL || (cmd->flags & C_GENDER) == 0) {
			error("Command '%s' can't have a gender", cmd == NULL ? "<empty>" : cmd->cmd);
		}

		for (nw = 0; nw < MAX_NUM_GENDERS; nw++) {
			words[nw] = ParseWord(&buf);
			if (words[nw] == NULL) break;
		}
		if (nw != _lang.num_genders) error("Bad # of arguments for gender command");

		assert(IsInsideBS(cmd->value, SCC_CONTROL_START, UINT8_MAX));
		PutUtf8(SCC_GENDER_LIST);
		PutByte(TranslateArgumentIdx(argidx, offset));
		EmitWordList(words, nw);
	}
}

static const CmdStruct *FindCmd(const char *s, int len)
{
	for (const CmdStruct *cs = _cmd_structs; cs != endof(_cmd_structs); cs++) {
		if (strncmp(cs->cmd, s, len) == 0 && cs->cmd[len] == '\0') return cs;
	}
	return NULL;
}

static uint ResolveCaseName(const char *str, uint len)
{
	/* First get a clean copy of only the case name, then resolve it. */
	char case_str[CASE_GENDER_LEN];
	len = min(lengthof(case_str) - 1, len);
	memcpy(case_str, str, len);
	case_str[len] = '\0';

	uint8 case_idx = _lang.GetCaseIndex(case_str);
	if (case_idx >= MAX_NUM_CASES) error("Invalid case-name '%s'", case_str);
	return case_idx + 1;
}


/* returns NULL on eof
 * else returns command struct */
static const CmdStruct *ParseCommandString(const char **str, char *param, int *argno, int *casei)
{
	const char *s = *str, *start;
	char c;

	*argno = -1;
	*casei = -1;

	/* Scan to the next command, exit if there's no next command. */
	for (; *s != '{'; s++) {
		if (*s == '\0') return NULL;
	}
	s++; // Skip past the {

	if (*s >= '0' && *s <= '9') {
		char *end;

		*argno = strtoul(s, &end, 0);
		if (*end != ':') error("missing arg #");
		s = end + 1;
	}

	/* parse command name */
	start = s;
	do {
		c = *s++;
	} while (c != '}' && c != ' ' && c != '=' && c != '.' && c != 0);

	const CmdStruct *cmd = FindCmd(start, s - start - 1);
	if (cmd == NULL) {
		strgen_error("Undefined command '%.*s'", (int)(s - start - 1), start);
		return NULL;
	}

	if (c == '.') {
		const char *casep = s;

		if (!(cmd->flags & C_CASE)) {
			error("Command '%s' can't have a case", cmd->cmd);
		}

		do {
			c = *s++;
		} while (c != '}' && c != ' ' && c != '\0');
		*casei = ResolveCaseName(casep, s - casep - 1);
	}

	if (c == '\0') {
		strgen_error("Missing } from command '%s'", start);
		return NULL;
	}


	if (c != '}') {
		if (c == '=') s--;
		/* copy params */
		start = s;
		for (;;) {
			c = *s++;
			if (c == '}') break;
			if (c == '\0') {
				strgen_error("Missing } from command '%s'", start);
				return NULL;
			}
			if (s - start == MAX_COMMAND_PARAM_SIZE) error("param command too long");
			*param++ = c;
		}
	}
	*param = '\0';

	*str = s;

	return cmd;
}


static void HandlePragma(char *str, bool master)
{
	if (!memcmp(str, "id ", 3)) {
		_next_string_id = strtoul(str + 3, NULL, 0);
	} else if (!memcmp(str, "name ", 5)) {
		strecpy(_lang.name, str + 5, lastof(_lang.name));
	} else if (!memcmp(str, "ownname ", 8)) {
		strecpy(_lang.own_name, str + 8, lastof(_lang.own_name));
	} else if (!memcmp(str, "isocode ", 8)) {
		strecpy(_lang.isocode, str + 8, lastof(_lang.isocode));
	} else if (!memcmp(str, "plural ", 7)) {
		_lang.plural_form = atoi(str + 7);
		if (_lang.plural_form >= lengthof(_plural_forms)) {
			error("Invalid pluralform %d", _lang.plural_form);
		}
	} else if (!memcmp(str, "textdir ", 8)) {
		if (!memcmp(str + 8, "ltr", 3)) {
			_lang.text_dir = TD_LTR;
		} else if (!memcmp(str + 8, "rtl", 3)) {
			_lang.text_dir = TD_RTL;
		} else {
			error("Invalid textdir %s", str + 8);
		}
	} else if (!memcmp(str, "digitsep ", 9)) {
		str += 9;
		strecpy(_lang.digit_group_separator, strcmp(str, "{NBSP}") == 0 ? NBSP : str, lastof(_lang.digit_group_separator));
	} else if (!memcmp(str, "digitsepcur ", 12)) {
		str += 12;
		strecpy(_lang.digit_group_separator_currency, strcmp(str, "{NBSP}") == 0 ? NBSP : str, lastof(_lang.digit_group_separator_currency));
	} else if (!memcmp(str, "decimalsep ", 11)) {
		str += 11;
		strecpy(_lang.digit_decimal_separator, strcmp(str, "{NBSP}") == 0 ? NBSP : str, lastof(_lang.digit_decimal_separator));
	} else if (!memcmp(str, "winlangid ", 10)) {
		const char *buf = str + 10;
		long langid = strtol(buf, NULL, 16);
		if (langid > (long)UINT16_MAX || langid < 0) {
			error("Invalid winlangid %s", buf);
		}
		_lang.winlangid = (uint16)langid;
	} else if (!memcmp(str, "grflangid ", 10)) {
		const char *buf = str + 10;
		long langid = strtol(buf, NULL, 16);
		if (langid >= 0x7F || langid < 0) {
			error("Invalid grflangid %s", buf);
		}
		_lang.newgrflangid = (uint8)langid;
	} else if (!memcmp(str, "gender ", 7)) {
		if (master) error("Genders are not allowed in the base translation.");
		char *buf = str + 7;

		for (;;) {
			const char *s = ParseWord(&buf);

			if (s == NULL) break;
			if (_lang.num_genders >= MAX_NUM_GENDERS) error("Too many genders, max %d", MAX_NUM_GENDERS);
			strecpy(_lang.genders[_lang.num_genders], s, lastof(_lang.genders[_lang.num_genders]));
			_lang.num_genders++;
		}
	} else if (!memcmp(str, "case ", 5)) {
		if (master) error("Cases are not allowed in the base translation.");
		char *buf = str + 5;

		for (;;) {
			const char *s = ParseWord(&buf);

			if (s == NULL) break;
			if (_lang.num_cases >= MAX_NUM_CASES) error("Too many cases, max %d", MAX_NUM_CASES);
			strecpy(_lang.cases[_lang.num_cases], s, lastof(_lang.cases[_lang.num_cases]));
			_lang.num_cases++;
		}
	} else {
		error("unknown pragma '%s'", str);
	}
}

static void ExtractCommandString(ParsedCommandStruct *p, const char *s, bool warnings)
{
	char param[MAX_COMMAND_PARAM_SIZE];
	int argno;
	int argidx = 0;
	int casei;

	memset(p, 0, sizeof(*p));

	for (;;) {
		/* read until next command from a. */
		const CmdStruct *ar = ParseCommandString(&s, param, &argno, &casei);

		if (ar == NULL) break;

		/* Sanity checking */
		if (argno != -1 && ar->consumes == 0) error("Non consumer param can't have a paramindex");

		if (ar->consumes) {
			if (argno != -1) argidx = argno;
			if (argidx < 0 || (uint)argidx >= lengthof(p->cmd)) error("invalid param idx %d", argidx);
			if (p->cmd[argidx] != NULL && p->cmd[argidx] != ar) error("duplicate param idx %d", argidx);

			p->cmd[argidx++] = ar;
		} else if (!(ar->flags & C_DONTCOUNT)) { // Ignore some of them
			if (p->np >= lengthof(p->pairs)) error("too many commands in string, max " PRINTF_SIZE, lengthof(p->pairs));
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
			strcmp(a->cmd, "STRING5") == 0 ||
			strcmp(a->cmd, "RAW_STRING") == 0) {
		return FindCmd("STRING", 6);
	}

	return a;
}


static bool CheckCommandsMatch(char *a, char *b, const char *name)
{
	/* If we're not translating, i.e. we're compiling the base language,
	 * it is pointless to do all these checks as it'll always be correct.
	 * After all, all checks are based on the base language.
	 */
	if (!_translation) return true;

	ParsedCommandStruct templ;
	ParsedCommandStruct lang;
	bool result = true;

	ExtractCommandString(&templ, b, true);
	ExtractCommandString(&lang, a, true);

	/* For each string in templ, see if we find it in lang */
	if (templ.np != lang.np) {
		strgen_warning("%s: template string and language string have a different # of commands", name);
		result = false;
	}

	for (uint i = 0; i < templ.np; i++) {
		/* see if we find it in lang, and zero it out */
		bool found = false;
		for (uint j = 0; j < lang.np; j++) {
			if (templ.pairs[i].a == lang.pairs[j].a &&
					strcmp(templ.pairs[i].v, lang.pairs[j].v) == 0) {
				/* it was found in both. zero it out from lang so we don't find it again */
				lang.pairs[j].a = NULL;
				found = true;
				break;
			}
		}

		if (!found) {
			strgen_warning("%s: command '%s' exists in template file but not in language file", name, templ.pairs[i].a->cmd);
			result = false;
		}
	}

	/* if we reach here, all non consumer commands match up.
	 * Check if the non consumer commands match up also. */
	for (uint i = 0; i < lengthof(templ.cmd); i++) {
		if (TranslateCmdForCompare(templ.cmd[i]) != lang.cmd[i]) {
			strgen_warning("%s: Param idx #%d '%s' doesn't match with template command '%s'", name, i,
				lang.cmd[i]  == NULL ? "<empty>" : TranslateCmdForCompare(lang.cmd[i])->cmd,
				templ.cmd[i] == NULL ? "<empty>" : templ.cmd[i]->cmd);
			result = false;
		}
	}

	return result;
}

static void HandleString(char *str, bool master)
{
	if (*str == '#') {
		if (str[1] == '#' && str[2] != '#') HandlePragma(str + 2, master);
		return;
	}

	/* Ignore comments & blank lines */
	if (*str == ';' || *str == ' ' || *str == '\0') return;

	char *s = strchr(str, ':');
	if (s == NULL) {
		strgen_error("Line has no ':' delimiter");
		return;
	}

	char *t;
	/* Trim spaces.
	 * After this str points to the command name, and s points to the command contents */
	for (t = s; t > str && (t[-1] == ' ' || t[-1] == '\t'); t--) {}
	*t = 0;
	s++;

	/* Check string is valid UTF-8 */
	const char *tmp;
	for (tmp = s; *tmp != '\0';) {
		size_t len = Utf8Validate(tmp);
		if (len == 0) error("Invalid UTF-8 sequence in '%s'", s);

		WChar c;
		Utf8Decode(&c, tmp);
		if (c <= 0x001F || // ASCII control character range
				(c >= 0xE000 && c <= 0xF8FF) || // Private range
				(c >= 0xFFF0 && c <= 0xFFFF)) { // Specials range
			error("Unwanted UTF-8 character U+%04X in sequence '%s'", c, s);
		}

		tmp += len;
	}

	/* Check if the string has a case..
	 * The syntax for cases is IDENTNAME.case */
	char *casep = strchr(str, '.');
	if (casep) *casep++ = '\0';

	/* Check if this string already exists.. */
	LangString *ent = HashFind(str);

	if (master) {
		if (casep != NULL) {
			strgen_error("Cases in the base translation are not supported.");
			return;
		}

		if (ent != NULL) {
			strgen_error("String name '%s' is used multiple times", str);
			return;
		}

		if (_strings[_next_string_id]) {
			strgen_error("String ID 0x%X for '%s' already in use by '%s'", _next_string_id, str, _strings[_next_string_id]->name);
			return;
		}

		/* Allocate a new LangString */
		ent = CallocT<LangString>(1);
		_strings[_next_string_id] = ent;
		ent->index = _next_string_id++;
		ent->name = strdup(str);
		ent->line = _cur_line;

		HashAdd(str, ent);

		ent->english = strdup(s);
	} else {
		if (ent == NULL) {
			strgen_warning("String name '%s' does not exist in master file", str);
			return;
		}

		if (ent->translated && casep == NULL) {
			strgen_error("String name '%s' is used multiple times", str);
			return;
		}

		/* make sure that the commands match */
		if (!CheckCommandsMatch(s, ent->english, str)) return;

		if (casep != NULL) {
			Case *c = MallocT<Case>(1);

			c->caseidx = ResolveCaseName(casep, strlen(casep));
			c->string = strdup(s);
			c->next = ent->translated_case;
			ent->translated_case = c;
		} else {
			ent->translated = strdup(s);
			/* If the string was translated, use the line from the
			 * translated language so errors in the translated file
			 * are properly referenced to. */
			ent->line = _cur_line;
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

	/* Only look at the final filename to determine whether it's the base language or not */
	const char *cur_file = strrchr(_file, PATHSEPCHAR);
	const char *next_file = strrchr(file, PATHSEPCHAR);
	_translation = next_file != NULL && cur_file != NULL && strcmp(cur_file, next_file) != 0;
	_file = file;

	/* For each new file we parse, reset the genders, and language codes */
	MemSetT(&_lang, 0);
	strecpy(_lang.digit_group_separator, ",", lastof(_lang.digit_group_separator));
	strecpy(_lang.digit_group_separator_currency, ",", lastof(_lang.digit_group_separator_currency));
	strecpy(_lang.digit_decimal_separator, ".", lastof(_lang.digit_decimal_separator));

	in = fopen(file, "r");
	if (in == NULL) error("Cannot open file");
	_cur_line = 1;
	while (fgets(buf, sizeof(buf), in) != NULL) {
		rstrip(buf);
		HandleString(buf, english);
		_cur_line++;
	}
	fclose(in);

	if (StrEmpty(_lang.name) || StrEmpty(_lang.own_name) || StrEmpty(_lang.isocode)) {
		error("Language must include ##name, ##ownname and ##isocode");
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


/* make a hash of the file to get a unique "version number" */
static void MakeHashOfStrings()
{
	uint32 hash = 0;
	uint i;

	for (i = 0; i != lengthof(_strings); i++) {
		const LangString *ls = _strings[i];

		if (ls != NULL) {
			const CmdStruct *cs;
			const char *s;
			char buf[MAX_COMMAND_PARAM_SIZE];
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
	FILE *f2 = fopen(n2, "rb");
	if (f2 == NULL) return false;

	FILE *f1 = fopen(n1, "rb");
	if (f1 == NULL) error("can't open %s", n1);

	size_t l1, l2;
	do {
		char b1[4096];
		char b2[4096];
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
	int next = -1;

	_output_filename = "tmp.xxx";
	_output_file = fopen(_output_filename, "w");
	if (_output_file == NULL) error("can't open tmp.xxx");

	fprintf(_output_file, "/* This file is automatically generated. Do not modify */\n\n");
	fprintf(_output_file, "#ifndef TABLE_STRINGS_H\n");
	fprintf(_output_file, "#define TABLE_STRINGS_H\n");

	for (int i = 0; i != lengthof(_strings); i++) {
		if (_strings[i] != NULL) {
			if (next != i) fprintf(_output_file, "\n");
			fprintf(_output_file, "static const StringID %s = 0x%X;\n", _strings[i]->name, i);
			next = i + 1;
		}
	}

	fprintf(_output_file, "\nstatic const StringID STR_LAST_STRINGID = 0x%X;\n\n", next - 1);

	/* Find the plural form with the most amount of cases. */
	int max_plural_forms = 0;
	for (uint i = 0; i < lengthof(_plural_forms); i++) {
		max_plural_forms = max(max_plural_forms, _plural_forms[i].plural_count);
	}

	fprintf(_output_file,
		"static const uint LANGUAGE_PACK_VERSION     = 0x%X;\n"
		"static const uint LANGUAGE_MAX_PLURAL       = %d;\n"
		"static const uint LANGUAGE_MAX_PLURAL_FORMS = %d;\n\n",
		(uint)_hash, (uint)lengthof(_plural_forms), max_plural_forms
	);

	fprintf(_output_file, "#endif /* TABLE_STRINGS_H */\n");

	fclose(_output_file);
	_output_file = NULL;

	if (CompareFiles(_output_filename, filename)) {
		/* files are equal. tmp.xxx is not needed */
		unlink(_output_filename);
	} else {
		/* else rename tmp.xxx into filename */
#if defined(WIN32) || defined(WIN64)
		unlink(filename);
#endif
		if (rename(_output_filename, filename) == -1) error("rename() failed");
	}
	_output_filename = NULL;
}

static int TranslateArgumentIdx(int argidx, int offset)
{
	int sum;

	if (argidx < 0 || (uint)argidx >= lengthof(_cur_pcs.cmd)) {
		error("invalid argidx %d", argidx);
	}
	const CmdStruct *cs = _cur_pcs.cmd[argidx];
	if (cs != NULL && cs->consumes <= offset) {
		error("invalid argidx offset %d:%d", argidx, offset);
	}

	if (_cur_pcs.cmd[argidx] == NULL) {
		error("no command for this argidx %d", argidx);
	}

	for (int i = sum = 0; i < argidx; i++) {
		const CmdStruct *cs = _cur_pcs.cmd[i];

		sum += (cs != NULL) ? cs->consumes : 1;
	}

	return sum + offset;
}

static void PutArgidxCommand()
{
	PutUtf8(SCC_ARG_INDEX);
	PutByte(TranslateArgumentIdx(_cur_argidx));
}


static void PutCommandString(const char *str)
{
	_cur_argidx = 0;

	while (*str != '\0') {
		/* Process characters as they are until we encounter a { */
		if (*str != '{') {
			PutByte(*str++);
			continue;
		}

		char param[MAX_COMMAND_PARAM_SIZE];
		int argno;
		int casei;
		const CmdStruct *cs = ParseCommandString(&str, param, &argno, &casei);
		if (cs == NULL) break;

		if (casei != -1) {
			PutUtf8(SCC_SETCASE); // {SETCASE}
			PutByte(casei);
		}

		/* For params that consume values, we need to handle the argindex properly */
		if (cs->consumes > 0) {
			/* Check if we need to output a move-param command */
			if (argno != -1 && argno != _cur_argidx) {
				_cur_argidx = argno;
				PutArgidxCommand();
			}

			/* Output the one from the master string... it's always accurate. */
			cs = _cur_pcs.cmd[_cur_argidx++];
			if (cs == NULL) {
				error("%s: No argument exists at position %d", _cur_ident, _cur_argidx - 1);
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
		error("string too long");
	}
}


static void WriteLangfile(const char *filename)
{
	uint in_use[32];

	_output_filename = filename;
	_output_file = fopen(filename, "wb");
	if (_output_file == NULL) error("can't open %s", filename);

	for (int i = 0; i != 32; i++) {
		uint n = CountInUse(i);

		in_use[i] = n;
		_lang.offsets[i] = TO_LE16(n);
	}

	_lang.ident = TO_LE32(LanguagePackHeader::IDENT);
	_lang.version = TO_LE32(_hash);
	_lang.winlangid = TO_LE16(_lang.winlangid);

	fwrite(&_lang, sizeof(_lang), 1, _output_file);

	for (int i = 0; i != 32; i++) {
		for (uint j = 0; j != in_use[i]; j++) {
			const LangString *ls = _strings[(i << 11) + j];
			const Case *casep;
			const char *cmdp;

			/* For undefined strings, just set that it's an empty string */
			if (ls == NULL) {
				WriteLength(_output_file, 0);
				continue;
			}

			_cur_ident = ls->name;
			_cur_line = ls->line;

			/* Produce a message if a string doesn't have a translation. */
			if (_show_todo > 0 && ls->translated == NULL) {
				if ((_show_todo & 2) != 0) {
					strgen_warning("'%s' is untranslated", ls->name);
				}
				if ((_show_todo & 1) != 0) {
					const char *s = "<TODO> ";
					while (*s != '\0') PutByte(*s++);
				}
			}

			/* Extract the strings and stuff from the english command string */
			ExtractCommandString(&_cur_pcs, ls->english, false);

			if (ls->translated_case != NULL || ls->translated != NULL) {
				casep = ls->translated_case;
				cmdp = ls->translated;
			} else {
				casep = NULL;
				cmdp = ls->english;
			}

			_translated = _masterlang || (cmdp != ls->english);

			if (casep != NULL) {
				const Case *c;
				uint num;

				/* Need to output a case-switch.
				 * It has this format
				 * <0x9E> <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
				 * Each LEN is printed using 2 bytes in big endian order. */
				PutUtf8(SCC_SWITCH_CASE);
				/* Count the number of cases */
				for (num = 0, c = casep; c; c = c->next) num++;
				PutByte(num);

				/* Write each case */
				for (c = casep; c != NULL; c = c->next) {
					uint pos;

					PutByte(c->caseidx);
					/* Make some space for the 16-bit length */
					pos = _put_pos;
					PutByte(0);
					PutByte(0);
					/* Write string */
					PutCommandString(c->string);
					PutByte(0); // terminate with a zero
					/* Fill in the length */
					_put_buf[pos + 0] = GB(_put_pos - (pos + 2), 8, 8);
					_put_buf[pos + 1] = GB(_put_pos - (pos + 2), 0, 8);
				}
			}

			if (cmdp != NULL) PutCommandString(cmdp);

			WriteLength(_output_file, _put_pos);
			fwrite(_put_buf, 1, _put_pos, _output_file);
			_put_pos = 0;
		}
	}

	fputc(0, _output_file);
	fclose(_output_file);

	_output_file = NULL;
	_output_filename = NULL;
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

/**
 * Create a path consisting of an already existing path, a possible
 * path seperator and the filename. The seperator is only appended if the path
 * does not already end with a seperator
 */
static inline char *mkpath(char *buf, size_t buflen, const char *path, const char *file)
{
	ttd_strlcpy(buf, path, buflen); // copy directory into buffer

	char *p = strchr(buf, '\0'); // add path seperator if necessary
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
	for (char *c = s; *c != '\0'; c++) if (*c == '/') *c = '\\';
	return s;
}
#else
static inline char *replace_pathsep(char *s) { return s; }
#endif

int CDECL main(int argc, char *argv[])
{
	char pathbuf[MAX_PATH];
	const char *src_dir = ".";
	const char *dest_dir = NULL;

	while (argc > 1 && *argv[1] == '-') {
		if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
			puts("$Revision$");
			return 0;
		}

		if (strcmp(argv[1], "-export-commands") == 0) {
			printf("args\tflags\tcommand\treplacement\n");
			for (const CmdStruct *cs = _cmd_structs; cs < endof(_cmd_structs); cs++) {
				char flags;
				switch (cs->value) {
					case 0x200E: case 0x200F: // Implicit BIDI controls
					case 0x202A: case 0x202B: case 0x202C: case 0x202D: case 0x202E: // Explicit BIDI controls
					case 0xA0: // Non breaking space
					case '\n': // Newlines may be added too
					case '{':  // This special
						/* This command may be in the translation when it is not in base */
						flags = 'i';
						break;

					default:
						if (cs->proc == EmitGender) {
							flags = 'g'; // Command needs number of parameters defined by number of genders
						} else if (cs->proc == EmitPlural) {
							flags = 'p'; // Command needs number of parameters defined by plural value
						} else {
							flags = '0'; // Command needs no parameters
						}
				}
				printf("%i\t%c\t\"%s\"\t\"%s\"\n", cs->consumes, flags, cs->cmd, strstr(cs->cmd, "STRING") ? "STRING" : cs->cmd);
			}
			return 0;
		}

		if (strcmp(argv[1], "-export-plurals") == 0) {
			printf("count\tdescription\n");
			for (const PluralForm *pf = _plural_forms; pf < endof(_plural_forms); pf++) {
				printf("%i\t\"%s\"\n", pf->plural_count, pf->description);
			}
			return 0;
		}

		if (strcmp(argv[1], "-export-pragmas") == 0) {
			printf("name\tflags\tdefault\tdescription\n");
			for (size_t i = 0; i < lengthof(_pragmas); i++) {
				printf("\"%s\"\t%s\t\"%s\"\t\"%s\"\n",
						_pragmas[i][0], _pragmas[i][1], _pragmas[i][2], _pragmas[i][3]);
			}
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
				" -export-commands  export all commands and exit\n"
				" -export-plurals   export all plural forms and exit\n"
				" -export-pragmas   export all pragmas and exit\n"
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
