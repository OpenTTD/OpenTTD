/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen_base.cpp Tool to create computer readable (stand-alone) translation files. */

#include "../stdafx.h"
#include "../core/alloc_func.hpp"
#include "../core/endian_func.hpp"
#include "../core/mem_func.hpp"
#include "../error_func.h"
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
 */
Case::Case(int caseidx, const std::string &string) :
		caseidx(caseidx), string(string)
{
}

/**
 * Create a new string.
 * @param name    The name of the string.
 * @param english The english "translation" of the string.
 * @param index   The index in the string table.
 * @param line    The line this string was found on.
 */
LangString::LangString(const std::string &name, const std::string &english, size_t index, int line) :
		name(name), english(english), index(index), line(line)
{
}

/** Free all data related to the translation. */
void LangString::FreeTranslation()
{
	this->translated.clear();
	this->translated_cases.clear();
}

/**
 * Create a new string data container.
 * @param tabs The maximum number of strings.
 */
StringData::StringData(size_t tabs) : tabs(tabs), max_strings(tabs * TAB_SIZE)
{
	this->strings.resize(max_strings);
	this->next_string_id = 0;
}

/** Free all data related to the translation. */
void StringData::FreeTranslation()
{
	for (size_t i = 0; i < this->max_strings; i++) {
		LangString *ls = this->strings[i].get();
		if (ls != nullptr) ls->FreeTranslation();
	}
}

/**
 * Add a newly created LangString.
 * @param s  The name of the string.
 * @param ls The string to add.
 */
void StringData::Add(std::unique_ptr<LangString> ls)
{
	this->name_to_string[ls->name] = ls.get();
	this->strings[ls->index].swap(ls);
}

/**
 * Find a LangString based on the string name.
 * @param s The string name to search on.
 * @return The LangString or nullptr if it is not known.
 */
LangString *StringData::Find(const std::string_view s)
{
	auto it = this->name_to_string.find(s);
	if (it == this->name_to_string.end()) return nullptr;

	return it->second;
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
		const LangString *ls = this->strings[i].get();

		if (ls != nullptr) {
			const CmdStruct *cs;
			const char *s;
			char buf[MAX_COMMAND_PARAM_SIZE];
			int argno;
			int casei;

			s = ls->name.c_str();
			hash ^= i * 0x717239;
			hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
			hash = this->VersionHashStr(hash, s + 1);

			s = ls->english.c_str();
			while ((cs = ParseCommandString(&s, buf, &argno, &casei)) != nullptr) {
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
	for (i = TAB_SIZE; --i >= 0;) if (this->strings[(tab * TAB_SIZE) + i] != nullptr) break;
	return i + 1;
}

static const char *_cur_ident;

/* Used when generating some advanced commands. */
static ParsedCommandStruct _cur_pcs;
static int _cur_argidx;

/** The buffer for writing a single string. */
struct Buffer : std::vector<byte> {
	/**
	 * Convenience method for adding a byte.
	 * @param value The value to add.
	 */
	void AppendByte(byte value)
	{
		this->push_back(value);
	}

	/**
	 * Add an Unicode character encoded in UTF-8 to the buffer.
	 * @param value The character to add.
	 */
	void AppendUtf8(uint32_t value)
	{
		if (value < 0x80) {
			this->push_back(value);
		} else if (value < 0x800) {
			this->push_back(0xC0 + GB(value,  6, 5));
			this->push_back(0x80 + GB(value,  0, 6));
		} else if (value < 0x10000) {
			this->push_back(0xE0 + GB(value, 12, 4));
			this->push_back(0x80 + GB(value,  6, 6));
			this->push_back(0x80 + GB(value,  0, 6));
		} else if (value < 0x110000) {
			this->push_back(0xF0 + GB(value, 18, 3));
			this->push_back(0x80 + GB(value, 12, 6));
			this->push_back(0x80 + GB(value,  6, 6));
			this->push_back(0x80 + GB(value,  0, 6));
		} else {
			StrgenWarning("Invalid unicode value U+0x{:X}", value);
		}
	}
};

size_t Utf8Validate(const char *s)
{
	uint32_t c;

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
	if (*buf != '\0') StrgenWarning("Ignoring trailing letters in command");
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
	int v = std::strtol(s, &end, 0);
	if (end == s) return false;
	if (rel || v < 0) {
		*value += v;
	} else {
		*value = v;
	}
	if (offset != nullptr && *end == ':') {
		/* Take the Nth within */
		s = end + 1;
		*offset = std::strtol(s, &end, 0);
		if (end == s) return false;
	}
	*buf = end;
	return true;
}

/* Parse out the next word, or nullptr */
char *ParseWord(char **buf)
{
	char *s = *buf, *r;

	while (*s == ' ' || *s == '\t') s++;
	if (*s == '\0') return nullptr;

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

static void EmitWordList(Buffer *buffer, const std::vector<const char *> &words, uint nw)
{
	buffer->AppendByte(nw);
	for (uint i = 0; i < nw; i++) buffer->AppendByte((byte)strlen(words[i]) + 1);
	for (uint i = 0; i < nw; i++) {
		for (uint j = 0; words[i][j] != '\0'; j++) buffer->AppendByte(words[i][j]);
		buffer->AppendByte(0);
	}
}

void EmitPlural(Buffer *buffer, char *buf, int)
{
	int argidx = _cur_argidx;
	int offset = -1;
	int expected = _plural_forms[_lang.plural_form].plural_count;
	std::vector<const char *> words(std::max(expected, MAX_PLURALS), nullptr);
	int nw = 0;

	/* Parse out the number, if one exists. Otherwise default to prev arg. */
	if (!ParseRelNum(&buf, &argidx, &offset)) argidx--;

	const CmdStruct *cmd = _cur_pcs.consuming_commands[argidx];
	if (offset == -1) {
		/* Use default offset */
		if (cmd == nullptr || cmd->default_plural_offset < 0) {
			StrgenFatal("Command '{}' has no (default) plural position", cmd == nullptr ? "<empty>" : cmd->cmd);
		}
		offset = cmd->default_plural_offset;
	}

	/* Parse each string */
	for (nw = 0; nw < MAX_PLURALS; nw++) {
		words[nw] = ParseWord(&buf);
		if (words[nw] == nullptr) break;
	}

	if (nw == 0) {
		StrgenFatal("{}: No plural words", _cur_ident);
	}

	if (expected != nw) {
		if (_translated) {
			StrgenFatal("{}: Invalid number of plural forms. Expecting {}, found {}.", _cur_ident,
				expected, nw);
		} else {
			if ((_show_todo & 2) != 0) StrgenWarning("'{}' is untranslated. Tweaking english string to allow compilation for plural forms", _cur_ident);
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


void EmitGender(Buffer *buffer, char *buf, int)
{
	int argidx = _cur_argidx;
	int offset = 0;
	uint nw;

	if (buf[0] == '=') {
		buf++;

		/* This is a {G=DER} command */
		nw = _lang.GetGenderIndex(buf);
		if (nw >= MAX_NUM_GENDERS) StrgenFatal("G argument '{}' invalid", buf);

		/* now nw contains the gender index */
		buffer->AppendUtf8(SCC_GENDER_INDEX);
		buffer->AppendByte(nw);
	} else {
		std::vector<const char *> words(MAX_NUM_GENDERS, nullptr);

		/* This is a {G 0 foo bar two} command.
		 * If no relative number exists, default to +0 */
		ParseRelNum(&buf, &argidx, &offset);

		const CmdStruct *cmd = _cur_pcs.consuming_commands[argidx];
		if (cmd == nullptr || (cmd->flags & C_GENDER) == 0) {
			StrgenFatal("Command '{}' can't have a gender", cmd == nullptr ? "<empty>" : cmd->cmd);
		}

		for (nw = 0; nw < MAX_NUM_GENDERS; nw++) {
			words[nw] = ParseWord(&buf);
			if (words[nw] == nullptr) break;
		}
		if (nw != _lang.num_genders) StrgenFatal("Bad # of arguments for gender command");

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
	return nullptr;
}

static uint ResolveCaseName(const char *str, size_t len)
{
	/* First get a clean copy of only the case name, then resolve it. */
	char case_str[CASE_GENDER_LEN];
	len = std::min(lengthof(case_str) - 1, len);
	memcpy(case_str, str, len);
	case_str[len] = '\0';

	uint8_t case_idx = _lang.GetCaseIndex(case_str);
	if (case_idx >= MAX_NUM_CASES) StrgenFatal("Invalid case-name '{}'", case_str);
	return case_idx + 1;
}


/* returns nullptr on eof
 * else returns command struct */
static const CmdStruct *ParseCommandString(const char **str, char *param, int *argno, int *casei)
{
	const char *s = *str, *start;
	char c;

	*argno = -1;
	*casei = -1;

	/* Scan to the next command, exit if there's no next command. */
	for (; *s != '{'; s++) {
		if (*s == '\0') return nullptr;
	}
	s++; // Skip past the {

	if (*s >= '0' && *s <= '9') {
		char *end;

		*argno = std::strtoul(s, &end, 0);
		if (*end != ':') StrgenFatal("missing arg #");
		s = end + 1;
	}

	/* parse command name */
	start = s;
	do {
		c = *s++;
	} while (c != '}' && c != ' ' && c != '=' && c != '.' && c != 0);

	const CmdStruct *cmd = FindCmd(start, s - start - 1);
	if (cmd == nullptr) {
		std::string command(start, s - start - 1);
		StrgenError("Undefined command '{}'", command);
		return nullptr;
	}

	if (c == '.') {
		const char *casep = s;

		if (!(cmd->flags & C_CASE)) {
			StrgenFatal("Command '{}' can't have a case", cmd->cmd);
		}

		do {
			c = *s++;
		} while (c != '}' && c != ' ' && c != '\0');
		*casei = ResolveCaseName(casep, s - casep - 1);
	}

	if (c == '\0') {
		StrgenError("Missing }} from command '{}'", start);
		return nullptr;
	}


	if (c != '}') {
		if (c == '=') s--;
		/* copy params */
		start = s;
		for (;;) {
			c = *s++;
			if (c == '}') break;
			if (c == '\0') {
				StrgenError("Missing }} from command '{}'", start);
				return nullptr;
			}
			if (s - start == MAX_COMMAND_PARAM_SIZE) FatalError("param command too long");
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
StringReader::StringReader(StringData &data, const std::string &file, bool master, bool translation) :
		data(data), file(file), master(master), translation(translation)
{
}

ParsedCommandStruct ExtractCommandString(const char *s, bool)
{
	char param[MAX_COMMAND_PARAM_SIZE];
	int argno;
	int argidx = 0;
	int casei;

	ParsedCommandStruct p;

	for (;;) {
		/* read until next command from a. */
		const CmdStruct *ar = ParseCommandString(&s, param, &argno, &casei);

		if (ar == nullptr) break;

		/* Sanity checking */
		if (argno != -1 && ar->consumes == 0) StrgenFatal("Non consumer param can't have a paramindex");

		if (ar->consumes) {
			if (argno != -1) argidx = argno;
			if (argidx < 0 || (uint)argidx >= p.consuming_commands.max_size()) StrgenFatal("invalid param idx {}", argidx);
			if (p.consuming_commands[argidx] != nullptr && p.consuming_commands[argidx] != ar) StrgenFatal("duplicate param idx {}", argidx);

			p.consuming_commands[argidx++] = ar;
		} else if (!(ar->flags & C_DONTCOUNT)) { // Ignore some of them
			p.non_consuming_commands.emplace_back(CmdPair{ar, param});
		}
	}

	return p;
}


const CmdStruct *TranslateCmdForCompare(const CmdStruct *a)
{
	if (a == nullptr) return nullptr;

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


static bool CheckCommandsMatch(const char *a, const char *b, const char *name)
{
	/* If we're not translating, i.e. we're compiling the base language,
	 * it is pointless to do all these checks as it'll always be correct.
	 * After all, all checks are based on the base language.
	 */
	if (!_translation) return true;

	bool result = true;

	ParsedCommandStruct templ = ExtractCommandString(b, true);
	ParsedCommandStruct lang = ExtractCommandString(a, true);

	/* For each string in templ, see if we find it in lang */
	if (templ.non_consuming_commands.max_size() != lang.non_consuming_commands.max_size()) {
		StrgenWarning("{}: template string and language string have a different # of commands", name);
		result = false;
	}

	for (auto &templ_nc : templ.non_consuming_commands) {
		/* see if we find it in lang, and zero it out */
		bool found = false;
		for (auto &lang_nc : lang.non_consuming_commands) {
			if (templ_nc.cmd == lang_nc.cmd && templ_nc.param == lang_nc.param) {
				/* it was found in both. zero it out from lang so we don't find it again */
				lang_nc.cmd = nullptr;
				found = true;
				break;
			}
		}

		if (!found) {
			StrgenWarning("{}: command '{}' exists in template file but not in language file", name, templ_nc.cmd->cmd);
			result = false;
		}
	}

	/* if we reach here, all non consumer commands match up.
	 * Check if the non consumer commands match up also. */
	for (uint i = 0; i < templ.consuming_commands.max_size(); i++) {
		if (TranslateCmdForCompare(templ.consuming_commands[i]) != lang.consuming_commands[i]) {
			StrgenWarning("{}: Param idx #{} '{}' doesn't match with template command '{}'", name, i,
				lang.consuming_commands[i]  == nullptr ? "<empty>" : TranslateCmdForCompare(lang.consuming_commands[i])->cmd,
				templ.consuming_commands[i] == nullptr ? "<empty>" : templ.consuming_commands[i]->cmd);
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
	if (s == nullptr) {
		StrgenError("Line has no ':' delimiter");
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
		if (len == 0) StrgenFatal("Invalid UTF-8 sequence in '{}'", s);

		char32_t c;
		Utf8Decode(&c, tmp);
		if (c <= 0x001F || // ASCII control character range
				c == 0x200B || // Zero width space
				(c >= 0xE000 && c <= 0xF8FF) || // Private range
				(c >= 0xFFF0 && c <= 0xFFFF)) { // Specials range
			StrgenFatal("Unwanted UTF-8 character U+{:04X} in sequence '{}'", (int)c, s);
		}

		tmp += len;
	}

	/* Check if the string has a case..
	 * The syntax for cases is IDENTNAME.case */
	char *casep = strchr(str, '.');
	if (casep != nullptr) *casep++ = '\0';

	/* Check if this string already exists.. */
	LangString *ent = this->data.Find(str);

	if (this->master) {
		if (casep != nullptr) {
			StrgenError("Cases in the base translation are not supported.");
			return;
		}

		if (ent != nullptr) {
			StrgenError("String name '{}' is used multiple times", str);
			return;
		}

		if (this->data.strings[this->data.next_string_id] != nullptr) {
			StrgenError("String ID 0x{:X} for '{}' already in use by '{}'", this->data.next_string_id, str, this->data.strings[this->data.next_string_id]->name);
			return;
		}

		/* Allocate a new LangString */
		this->data.Add(std::make_unique<LangString>(str, s, this->data.next_string_id++, _cur_line));
	} else {
		if (ent == nullptr) {
			StrgenWarning("String name '{}' does not exist in master file", str);
			return;
		}

		if (!ent->translated.empty() && casep == nullptr) {
			StrgenError("String name '{}' is used multiple times", str);
			return;
		}

		/* make sure that the commands match */
		if (!CheckCommandsMatch(s, ent->english.c_str(), str)) return;

		if (casep != nullptr) {
			ent->translated_cases.emplace_back(ResolveCaseName(casep, strlen(casep)), s);
		} else {
			ent->translated = s;
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
			StrgenFatal("Invalid pluralform {}", _lang.plural_form);
		}
	} else {
		StrgenFatal("unknown pragma '{}'", str);
	}
}

static void StripTrailingWhitespace(std::string &str)
{
	str.erase(str.find_last_not_of("\r\n ") + 1);
}

void StringReader::ParseFile()
{
	_warnings = _errors = 0;

	_translation = this->translation;
	_file = this->file.c_str();

	/* Abusing _show_todo to replace "warning" with "info" for translations. */
	_show_todo &= 3;
	if (!this->translation) _show_todo |= 4;

	/* For each new file we parse, reset the genders, and language codes. */
	MemSetT(&_lang, 0);
	strecpy(_lang.digit_group_separator, ",", lastof(_lang.digit_group_separator));
	strecpy(_lang.digit_group_separator_currency, ",", lastof(_lang.digit_group_separator_currency));
	strecpy(_lang.digit_decimal_separator, ".", lastof(_lang.digit_decimal_separator));

	_cur_line = 1;
	while (this->data.next_string_id < this->data.max_strings) {
		std::optional<std::string> line = this->ReadLine();
		if (!line.has_value()) return;

		StripTrailingWhitespace(line.value());
		this->HandleString(line.value().data());
		_cur_line++;
	}

	if (this->data.next_string_id == this->data.max_strings) {
		StrgenError("Too many strings, maximum allowed is {}", this->data.max_strings);
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
		if (data.strings[i] != nullptr) {
			this->WriteStringID(data.strings[i]->name, (int)i);
			last = (int)i;
		}
	}

	this->WriteStringID("STR_LAST_STRINGID", last);
}

static int TranslateArgumentIdx(int argidx, int offset)
{
	int sum;

	if (argidx < 0 || (uint)argidx >= _cur_pcs.consuming_commands.max_size()) {
		StrgenFatal("invalid argidx {}", argidx);
	}
	const CmdStruct *cs = _cur_pcs.consuming_commands[argidx];
	if (cs != nullptr && cs->consumes <= offset) {
		StrgenFatal("invalid argidx offset {}:{}", argidx, offset);
	}

	if (_cur_pcs.consuming_commands[argidx] == nullptr) {
		StrgenFatal("no command for this argidx {}", argidx);
	}

	for (int i = sum = 0; i < argidx; i++) {
		cs = _cur_pcs.consuming_commands[i];

		sum += (cs != nullptr) ? cs->consumes : 1;
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
		if (cs == nullptr) break;

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
			cs = _cur_pcs.consuming_commands[_cur_argidx++];
			if (cs == nullptr) {
				StrgenFatal("{}: No argument exists at position {}", _cur_ident, _cur_argidx - 1);
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
		StrgenFatal("string too long");
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
	std::vector<uint> in_use;
	for (size_t tab = 0; tab < data.tabs; tab++) {
		uint n = data.CountInUse((uint)tab);

		in_use.push_back(n);
		_lang.offsets[tab] = TO_LE16(n);

		for (uint j = 0; j != in_use[tab]; j++) {
			const LangString *ls = data.strings[(tab * TAB_SIZE) + j].get();
			if (ls != nullptr && ls->translated.empty()) _lang.missing++;
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
			const LangString *ls = data.strings[(tab * TAB_SIZE) + j].get();
			const std::string *cmdp;

			/* For undefined strings, just set that it's an empty string */
			if (ls == nullptr) {
				this->WriteLength(0);
				continue;
			}

			_cur_ident = ls->name.c_str();
			_cur_line = ls->line;

			/* Produce a message if a string doesn't have a translation. */
			if (_show_todo > 0 && ls->translated.empty()) {
				if ((_show_todo & 2) != 0) {
					StrgenWarning("'{}' is untranslated", ls->name);
				}
				if ((_show_todo & 1) != 0) {
					const char *s = "<TODO> ";
					while (*s != '\0') buffer.AppendByte(*s++);
				}
			}

			/* Extract the strings and stuff from the english command string */
			_cur_pcs = ExtractCommandString(ls->english.c_str(), false);

			if (!ls->translated_cases.empty() || !ls->translated.empty()) {
				cmdp = &ls->translated;
			} else {
				cmdp = &ls->english;
			}

			_translated = cmdp != &ls->english;

			if (!ls->translated_cases.empty()) {
				/* Need to output a case-switch.
				 * It has this format
				 * <0x9E> <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
				 * Each LEN is printed using 2 bytes in big endian order. */
				buffer.AppendUtf8(SCC_SWITCH_CASE);
				buffer.AppendByte((byte)ls->translated_cases.size());

				/* Write each case */
				for (const Case &c : ls->translated_cases) {
					buffer.AppendByte(c.caseidx);
					/* Make some space for the 16-bit length */
					uint pos = (uint)buffer.size();
					buffer.AppendByte(0);
					buffer.AppendByte(0);
					/* Write string */
					PutCommandString(&buffer, c.string.c_str());
					buffer.AppendByte(0); // terminate with a zero
					/* Fill in the length */
					uint size = (uint)buffer.size() - (pos + 2);
					buffer[pos + 0] = GB(size, 8, 8);
					buffer[pos + 1] = GB(size, 0, 8);
				}
			}

			if (!cmdp->empty()) PutCommandString(&buffer, cmdp->c_str());

			this->WriteLength((uint)buffer.size());
			this->Write(buffer.data(), buffer.size());
			buffer.clear();
		}
	}
}
