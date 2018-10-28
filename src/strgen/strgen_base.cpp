/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen_base.cpp Tool to create computer readable (stand-alone) translation files. */

#include "../stdafx.h"
#include "../core/endian_func.hpp"
#include "../string_func.h"
#include "../table/control_codes.h"

#include "strgen.h"


#include "../table/strgen_tables.h"

#include "../safeguards.h"

/* Compiles a list of strings into a compiled string list */

static bool _translated;              ///< Whether the current language is not the master language
static bool _translation;             ///< Is the current file actually a translation or not
const char *_file = "(unknown file)"; ///< The filename of the input, so we can refer to it in errors/warnings
int _cur_line;                        ///< The current line we're parsing in the input file
int _errors, _warnings, _show_todo;
LanguagePackHeader _lang;             ///< Header information about a language.

static const ptrdiff_t MAX_COMMAND_PARAM_SIZE = 100; ///< Maximum size of every command block, not counting the name of the command itself
static const CmdStruct *ParseCommandString(const char **str, char *param, int *argno, int *casei);

/**
 * Create a new case.
 * @param caseidx The index of the case.
 * @param string  The translation of the case.
 * @param next    The next chained case.
 */
Case::Case(int caseidx, const char *string, Case *next) :
		caseidx(caseidx), string(stredup(string)), next(next)
{
}

/** Free everything we allocated. */
Case::~Case()
{
	free(this->string);
	delete this->next;
}

/**
 * Create a new string.
 * @param name    The name of the string.
 * @param english The english "translation" of the string.
 * @param index   The index in the string table.
 * @param line    The line this string was found on.
 */
LangString::LangString(const char *name, const char *english, int index, int line) :
		name(stredup(name)), english(stredup(english)), translated(NULL),
		hash_next(0), index(index), line(line), translated_case(NULL)
{
}

/** Free everything we allocated. */
LangString::~LangString()
{
	free(this->name);
	free(this->english);
	free(this->translated);
	delete this->translated_case;
}

/** Free all data related to the translation. */
void LangString::FreeTranslation()
{
	free(this->translated);
	this->translated = NULL;

	delete this->translated_case;
	this->translated_case = NULL;
}

/**
 * Create a new string data container.
 * @param tabs The maximum number of strings.
 */
StringData::StringData(size_t tabs) : tabs(tabs), max_strings(tabs * TAB_SIZE)
{
	this->strings = CallocT<LangString *>(max_strings);
	this->hash_heads = CallocT<uint16>(max_strings);
	this->next_string_id = 0;
}

/** Free everything we allocated. */
StringData::~StringData()
{
	for (size_t i = 0; i < this->max_strings; i++) delete this->strings[i];
	free(this->strings);
	free(this->hash_heads);
}

/** Free all data related to the translation. */
void StringData::FreeTranslation()
{
	for (size_t i = 0; i < this->max_strings; i++) {
		LangString *ls = this->strings[i];
		if (ls != NULL) ls->FreeTranslation();
	}
}

/**
 * Create a hash of the string for finding them back quickly.
 * @param s The string to hash.
 * @return The hashed string.
 */
uint StringData::HashStr(const char *s) const
{
	uint hash = 0;
	for (; *s != '\0'; s++) hash = ROL(hash, 3) ^ *s;
	return hash % this->max_strings;
}

/**
 * Add a newly created LangString.
 * @param s  The name of the string.
 * @param ls The string to add.
 */
void StringData::Add(const char *s, LangString *ls)
{
	uint hash = this->HashStr(s);
	ls->hash_next = this->hash_heads[hash];
	/* Off-by-one for hash find. */
	this->hash_heads[hash] = ls->index + 1;
	this->strings[ls->index] = ls;
}

/**
 * Find a LangString based on the string name.
 * @param s The string name to search on.
 * @return The LangString or NULL if it is not known.
 */
LangString *StringData::Find(const char *s)
{
	int idx = this->hash_heads[this->HashStr(s)];

	while (--idx >= 0) {
		LangString *ls = this->strings[idx];

		if (strcmp(ls->name, s) == 0) return ls;
		idx = ls->hash_next;
	}
	return NULL;
}

/**
 * Create a compound hash.
 * @param hash The hash to add the string hash to.
 * @param s    The string hash.
 * @return The new hash.
 */
uint StringData::VersionHashStr(uint hash, const char *s) const
{
	for (; *s != '\0'; s++) {
		hash = ROL(hash, 3) ^ *s;
		hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
	}
	return hash;
}

/**
 * Make a hash of the file to get a unique "version number"
 * @return The version number.
 */
uint StringData::Version() const
{
	uint hash = 0;

	for (size_t i = 0; i < this->max_strings; i++) {
		const LangString *ls = this->strings[i];

		if (ls != NULL) {
			const CmdStruct *cs;
			const char *s;
			char buf[MAX_COMMAND_PARAM_SIZE];
			int argno;
			int casei;

			s = ls->name;
			hash ^= i * 0x717239;
			hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
			hash = this->VersionHashStr(hash, s + 1);

			s = ls->english;
			while ((cs = ParseCommandString(&s, buf, &argno, &casei)) != NULL) {
				if (cs->flags & C_DONTCOUNT) continue;

				hash ^= (cs - _cmd_structs) * 0x1234567;
				hash = (hash & 1 ? hash >> 1 ^ 0xF00BAA4 : hash >> 1);
			}
		}
	}

	return hash;
}

/**
 * Count the number of tab elements that are in use.
 * @param tab The tab to count the elements of.
 */
uint StringData::CountInUse(uint tab) const
{
	int i;
	for (i = TAB_SIZE; --i >= 0;) if (this->strings[(tab * TAB_SIZE) + i] != NULL) break;
	return i + 1;
}

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

/** The buffer for writing a single string. */
struct Buffer : SmallVector<byte, 256> {
	/**
	 * Convenience method for adding a byte.
	 * @param value The value to add.
	 */
	void AppendByte(byte value)
	{
		*this->Append() = value;
	}

	/**
	 * Add an Unicode character encoded in UTF-8 to the buffer.
	 * @param value The character to add.
	 */
	void AppendUtf8(uint32 value)
	{
		if (value < 0x80) {
			*this->Append() = value;
		} else if (value < 0x800) {
			*this->Append() = 0xC0 + GB(value,  6, 5);
			*this->Append() = 0x80 + GB(value,  0, 6);
		} else if (value < 0x10000) {
			*this->Append() = 0xE0 + GB(value, 12, 4);
			*this->Append() = 0x80 + GB(value,  6, 6);
			*this->Append() = 0x80 + GB(value,  0, 6);
		} else if (value < 0x110000) {
			*this->Append() = 0xF0 + GB(value, 18, 3);
			*this->Append() = 0x80 + GB(value, 12, 6);
			*this->Append() = 0x80 + GB(value,  6, 6);
			*this->Append() = 0x80 + GB(value,  0, 6);
		} else {
			strgen_warning("Invalid unicode value U+0x%X", value);
		}
	}
};

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


void EmitSingleChar(Buffer *buffer, char *buf, int value)
{
	if (*buf != '\0') strgen_warning("Ignoring trailing letters in command");
	buffer->AppendUtf8(value);
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

static void EmitWordList(Buffer *buffer, const char * const *words, uint nw)
{
	buffer->AppendByte(nw);
	for (uint i = 0; i < nw; i++) buffer->AppendByte((byte)strlen(words[i]) + 1);
	for (uint i = 0; i < nw; i++) {
		for (uint j = 0; words[i][j] != '\0'; j++) buffer->AppendByte(words[i][j]);
		buffer->AppendByte(0);
	}
}

void EmitPlural(Buffer *buffer, char *buf, int value)
{
	int argidx = _cur_argidx;
	int offset = -1;
	int expected = _plural_forms[_lang.plural_form].plural_count;
	const char **words = AllocaM(const char *, max(expected, MAX_PLURALS));
	int nw = 0;

	/* Parse out the number, if one exists. Otherwise default to prev arg. */
	if (!ParseRelNum(&buf, &argidx, &offset)) argidx--;

	const CmdStruct *cmd = _cur_pcs.cmd[argidx];
	if (offset == -1) {
		/* Use default offset */
		if (cmd == NULL || cmd->default_plural_offset < 0) {
			strgen_fatal("Command '%s' has no (default) plural position", cmd == NULL ? "<empty>" : cmd->cmd);
		}
		offset = cmd->default_plural_offset;
	}

	/* Parse each string */
	for (nw = 0; nw < MAX_PLURALS; nw++) {
		words[nw] = ParseWord(&buf);
		if (words[nw] == NULL) break;
	}

	if (nw == 0) {
		strgen_fatal("%s: No plural words", _cur_ident);
	}

	if (expected != nw) {
		if (_translated) {
			strgen_fatal("%s: Invalid number of plural forms. Expecting %d, found %d.", _cur_ident,
				expected, nw);
		} else {
			if ((_show_todo & 2) != 0) strgen_warning("'%s' is untranslated. Tweaking english string to allow compilation for plural forms", _cur_ident);
			if (nw > expected) {
				nw = expected;
			} else {
				for (; nw < expected; nw++) {
					words[nw] = words[nw - 1];
				}
			}
		}
	}

	buffer->AppendUtf8(SCC_PLURAL_LIST);
	buffer->AppendByte(_lang.plural_form);
	buffer->AppendByte(TranslateArgumentIdx(argidx, offset));
	EmitWordList(buffer, words, nw);
}


void EmitGender(Buffer *buffer, char *buf, int value)
{
	int argidx = _cur_argidx;
	int offset = 0;
	uint nw;

	if (buf[0] == '=') {
		buf++;

		/* This is a {G=DER} command */
		nw = _lang.GetGenderIndex(buf);
		if (nw >= MAX_NUM_GENDERS) strgen_fatal("G argument '%s' invalid", buf);

		/* now nw contains the gender index */
		buffer->AppendUtf8(SCC_GENDER_INDEX);
		buffer->AppendByte(nw);
	} else {
		const char *words[MAX_NUM_GENDERS];

		/* This is a {G 0 foo bar two} command.
		 * If no relative number exists, default to +0 */
		if (!ParseRelNum(&buf, &argidx, &offset)) {}

		const CmdStruct *cmd = _cur_pcs.cmd[argidx];
		if (cmd == NULL || (cmd->flags & C_GENDER) == 0) {
			strgen_fatal("Command '%s' can't have a gender", cmd == NULL ? "<empty>" : cmd->cmd);
		}

		for (nw = 0; nw < MAX_NUM_GENDERS; nw++) {
			words[nw] = ParseWord(&buf);
			if (words[nw] == NULL) break;
		}
		if (nw != _lang.num_genders) strgen_fatal("Bad # of arguments for gender command");

		assert(IsInsideBS(cmd->value, SCC_CONTROL_START, UINT8_MAX));
		buffer->AppendUtf8(SCC_GENDER_LIST);
		buffer->AppendByte(TranslateArgumentIdx(argidx, offset));
		EmitWordList(buffer, words, nw);
	}
}

static const CmdStruct *FindCmd(const char *s, int len)
{
	for (const CmdStruct *cs = _cmd_structs; cs != endof(_cmd_structs); cs++) {
		if (strncmp(cs->cmd, s, len) == 0 && cs->cmd[len] == '\0') return cs;
	}
	return NULL;
}

static uint ResolveCaseName(const char *str, size_t len)
{
	/* First get a clean copy of only the case name, then resolve it. */
	char case_str[CASE_GENDER_LEN];
	len = min(lengthof(case_str) - 1, len);
	memcpy(case_str, str, len);
	case_str[len] = '\0';

	uint8 case_idx = _lang.GetCaseIndex(case_str);
	if (case_idx >= MAX_NUM_CASES) strgen_fatal("Invalid case-name '%s'", case_str);
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
		if (*end != ':') strgen_fatal("missing arg #");
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
			strgen_fatal("Command '%s' can't have a case", cmd->cmd);
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

/**
 * Prepare reading.
 * @param data        The data to fill during reading.
 * @param file        The file we are reading.
 * @param master      Are we reading the master file?
 * @param translation Are we reading a translation?
 */
StringReader::StringReader(StringData &data, const char *file, bool master, bool translation) :
		data(data), file(stredup(file)), master(master), translation(translation)
{
}

/** Make sure the right reader gets freed. */
StringReader::~StringReader()
{
	free(file);
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
		if (argno != -1 && ar->consumes == 0) strgen_fatal("Non consumer param can't have a paramindex");

		if (ar->consumes) {
			if (argno != -1) argidx = argno;
			if (argidx < 0 || (uint)argidx >= lengthof(p->cmd)) strgen_fatal("invalid param idx %d", argidx);
			if (p->cmd[argidx] != NULL && p->cmd[argidx] != ar) strgen_fatal("duplicate param idx %d", argidx);

			p->cmd[argidx++] = ar;
		} else if (!(ar->flags & C_DONTCOUNT)) { // Ignore some of them
			if (p->np >= lengthof(p->pairs)) strgen_fatal("too many commands in string, max " PRINTF_SIZE, lengthof(p->pairs));
			p->pairs[p->np].a = ar;
			p->pairs[p->np].v = param[0] != '\0' ? stredup(param) : "";
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
			strcmp(a->cmd, "STRING6") == 0 ||
			strcmp(a->cmd, "STRING7") == 0 ||
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

void StringReader::HandleString(char *str)
{
	if (*str == '#') {
		if (str[1] == '#' && str[2] != '#') this->HandlePragma(str + 2);
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
		if (len == 0) strgen_fatal("Invalid UTF-8 sequence in '%s'", s);

		WChar c;
		Utf8Decode(&c, tmp);
		if (c <= 0x001F || // ASCII control character range
				c == 0x200B || // Zero width space
				(c >= 0xE000 && c <= 0xF8FF) || // Private range
				(c >= 0xFFF0 && c <= 0xFFFF)) { // Specials range
			strgen_fatal("Unwanted UTF-8 character U+%04X in sequence '%s'", c, s);
		}

		tmp += len;
	}

	/* Check if the string has a case..
	 * The syntax for cases is IDENTNAME.case */
	char *casep = strchr(str, '.');
	if (casep != NULL) *casep++ = '\0';

	/* Check if this string already exists.. */
	LangString *ent = this->data.Find(str);

	if (this->master) {
		if (casep != NULL) {
			strgen_error("Cases in the base translation are not supported.");
			return;
		}

		if (ent != NULL) {
			strgen_error("String name '%s' is used multiple times", str);
			return;
		}

		if (this->data.strings[this->data.next_string_id] != NULL) {
			strgen_error("String ID 0x%X for '%s' already in use by '%s'", this->data.next_string_id, str, this->data.strings[this->data.next_string_id]->name);
			return;
		}

		/* Allocate a new LangString */
		this->data.Add(str, new LangString(str, s, this->data.next_string_id++, _cur_line));
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
			ent->translated_case = new Case(ResolveCaseName(casep, strlen(casep)), s, ent->translated_case);
		} else {
			ent->translated = stredup(s);
			/* If the string was translated, use the line from the
			 * translated language so errors in the translated file
			 * are properly referenced to. */
			ent->line = _cur_line;
		}
	}
}

void StringReader::HandlePragma(char *str)
{
	if (!memcmp(str, "plural ", 7)) {
		_lang.plural_form = atoi(str + 7);
		if (_lang.plural_form >= lengthof(_plural_forms)) {
			strgen_fatal("Invalid pluralform %d", _lang.plural_form);
		}
	} else {
		strgen_fatal("unknown pragma '%s'", str);
	}
}

static void rstrip(char *buf)
{
	size_t i = strlen(buf);
	while (i > 0 && (buf[i - 1] == '\r' || buf[i - 1] == '\n' || buf[i - 1] == ' ')) i--;
	buf[i] = '\0';
}

void StringReader::ParseFile()
{
	char buf[2048];
	_warnings = _errors = 0;

	_translation = this->master || this->translation;
	_file = this->file;

	/* For each new file we parse, reset the genders, and language codes. */
	MemSetT(&_lang, 0);
	strecpy(_lang.digit_group_separator, ",", lastof(_lang.digit_group_separator));
	strecpy(_lang.digit_group_separator_currency, ",", lastof(_lang.digit_group_separator_currency));
	strecpy(_lang.digit_decimal_separator, ".", lastof(_lang.digit_decimal_separator));

	_cur_line = 1;
	while (this->ReadLine(buf, lastof(buf)) != NULL) {
		rstrip(buf);
		this->HandleString(buf);
		_cur_line++;
	}
}

/**
 * Write the header information.
 * @param data The data about the string.
 */
void HeaderWriter::WriteHeader(const StringData &data)
{
	int last = 0;
	for (size_t i = 0; i < data.max_strings; i++) {
		if (data.strings[i] != NULL) {
			this->WriteStringID(data.strings[i]->name, (int)i);
			last = (int)i;
		}
	}

	this->WriteStringID("STR_LAST_STRINGID", last);
}

static int TranslateArgumentIdx(int argidx, int offset)
{
	int sum;

	if (argidx < 0 || (uint)argidx >= lengthof(_cur_pcs.cmd)) {
		strgen_fatal("invalid argidx %d", argidx);
	}
	const CmdStruct *cs = _cur_pcs.cmd[argidx];
	if (cs != NULL && cs->consumes <= offset) {
		strgen_fatal("invalid argidx offset %d:%d", argidx, offset);
	}

	if (_cur_pcs.cmd[argidx] == NULL) {
		strgen_fatal("no command for this argidx %d", argidx);
	}

	for (int i = sum = 0; i < argidx; i++) {
		const CmdStruct *cs = _cur_pcs.cmd[i];

		sum += (cs != NULL) ? cs->consumes : 1;
	}

	return sum + offset;
}

static void PutArgidxCommand(Buffer *buffer)
{
	buffer->AppendUtf8(SCC_ARG_INDEX);
	buffer->AppendByte(TranslateArgumentIdx(_cur_argidx));
}


static void PutCommandString(Buffer *buffer, const char *str)
{
	_cur_argidx = 0;

	while (*str != '\0') {
		/* Process characters as they are until we encounter a { */
		if (*str != '{') {
			buffer->AppendByte(*str++);
			continue;
		}

		char param[MAX_COMMAND_PARAM_SIZE];
		int argno;
		int casei;
		const CmdStruct *cs = ParseCommandString(&str, param, &argno, &casei);
		if (cs == NULL) break;

		if (casei != -1) {
			buffer->AppendUtf8(SCC_SET_CASE); // {SET_CASE}
			buffer->AppendByte(casei);
		}

		/* For params that consume values, we need to handle the argindex properly */
		if (cs->consumes > 0) {
			/* Check if we need to output a move-param command */
			if (argno != -1 && argno != _cur_argidx) {
				_cur_argidx = argno;
				PutArgidxCommand(buffer);
			}

			/* Output the one from the master string... it's always accurate. */
			cs = _cur_pcs.cmd[_cur_argidx++];
			if (cs == NULL) {
				strgen_fatal("%s: No argument exists at position %d", _cur_ident, _cur_argidx - 1);
			}
		}

		cs->proc(buffer, param, cs->value);
	}
}

/**
 * Write the length as a simple gamma.
 * @param length The number to write.
 */
void LanguageWriter::WriteLength(uint length)
{
	char buffer[2];
	int offs = 0;
	if (length >= 0x4000) {
		strgen_fatal("string too long");
	}

	if (length >= 0xC0) {
		buffer[offs++] = (length >> 8) | 0xC0;
	}
	buffer[offs++] = length & 0xFF;
	this->Write((byte*)buffer, offs);
}

/**
 * Actually write the language.
 * @param data The data about the string.
 */
void LanguageWriter::WriteLang(const StringData &data)
{
	uint *in_use = AllocaM(uint, data.tabs);
	for (size_t tab = 0; tab < data.tabs; tab++) {
		uint n = data.CountInUse((uint)tab);

		in_use[tab] = n;
		_lang.offsets[tab] = TO_LE16(n);

		for (uint j = 0; j != in_use[tab]; j++) {
			const LangString *ls = data.strings[(tab * TAB_SIZE) + j];
			if (ls != NULL && ls->translated == NULL) _lang.missing++;
		}
	}

	_lang.ident = TO_LE32(LanguagePackHeader::IDENT);
	_lang.version = TO_LE32(data.Version());
	_lang.missing = TO_LE16(_lang.missing);
	_lang.winlangid = TO_LE16(_lang.winlangid);

	this->WriteHeader(&_lang);
	Buffer buffer;

	for (size_t tab = 0; tab < data.tabs; tab++) {
		for (uint j = 0; j != in_use[tab]; j++) {
			const LangString *ls = data.strings[(tab * TAB_SIZE) + j];
			const Case *casep;
			const char *cmdp;

			/* For undefined strings, just set that it's an empty string */
			if (ls == NULL) {
				this->WriteLength(0);
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
					while (*s != '\0') buffer.AppendByte(*s++);
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

			_translated = cmdp != ls->english;

			if (casep != NULL) {
				const Case *c;
				uint num;

				/* Need to output a case-switch.
				 * It has this format
				 * <0x9E> <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
				 * Each LEN is printed using 2 bytes in big endian order. */
				buffer.AppendUtf8(SCC_SWITCH_CASE);
				/* Count the number of cases */
				for (num = 0, c = casep; c; c = c->next) num++;
				buffer.AppendByte(num);

				/* Write each case */
				for (c = casep; c != NULL; c = c->next) {
					buffer.AppendByte(c->caseidx);
					/* Make some space for the 16-bit length */
					uint pos = buffer.Length();
					buffer.AppendByte(0);
					buffer.AppendByte(0);
					/* Write string */
					PutCommandString(&buffer, c->string);
					buffer.AppendByte(0); // terminate with a zero
					/* Fill in the length */
					uint size = buffer.Length() - (pos + 2);
					buffer[pos + 0] = GB(size, 8, 8);
					buffer[pos + 1] = GB(size, 0, 8);
				}
			}

			if (cmdp != NULL) PutCommandString(&buffer, cmdp);

			this->WriteLength(buffer.Length());
			this->Write(buffer.Begin(), buffer.Length());
			buffer.Clear();
		}
	}
}
