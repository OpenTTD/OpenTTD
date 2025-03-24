/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen_base.cpp Tool to create computer readable (stand-alone) translation files. */

#include "../stdafx.h"
#include "../core/endian_func.hpp"
#include "../core/mem_func.hpp"
#include "../error_func.h"
#include "../string_func.h"
#include "../table/control_codes.h"

#include "strgen.h"

#include "../table/strgen_tables.h"

#include "../safeguards.h"

static bool _translated;              ///< Whether the current language is not the master language
bool _translation;                    ///< Is the current file actually a translation or not
const char *_file = "(unknown file)"; ///< The filename of the input, so we can refer to it in errors/warnings
size_t _cur_line;                     ///< The current line we're parsing in the input file
size_t _errors, _warnings;
bool _show_warnings = false, _annotate_todos = false;
LanguagePackHeader _lang;             ///< Header information about a language.
static const char *_cur_ident;
static ParsedCommandStruct _cur_pcs;
static size_t _cur_argidx;

struct ParsedCommandString {
	const CmdStruct *cmd = nullptr;
	std::string param;
	std::optional<size_t> argno;
	std::optional<uint8_t> casei;
};
static ParsedCommandString ParseCommandString(const char **str);
static size_t TranslateArgumentIdx(size_t arg, size_t offset = 0);

/**
 * Create a new case.
 * @param caseidx The index of the case.
 * @param string  The translation of the case.
 */
Case::Case(uint8_t caseidx, const std::string &string) :
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
LangString::LangString(const std::string &name, const std::string &english, size_t index, size_t line) :
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
void StringData::Add(std::shared_ptr<LangString> ls)
{
	this->name_to_string[ls->name] = ls;
	this->strings[ls->index] = std::move(ls);
}

/**
 * Find a LangString based on the string name.
 * @param s The string name to search on.
 * @return The LangString or nullptr if it is not known.
 */
LangString *StringData::Find(const std::string &s)
{
	auto it = this->name_to_string.find(s);
	if (it == this->name_to_string.end()) return nullptr;

	return it->second.get();
}

/**
 * Create a compound hash.
 * @param hash The hash to add the string hash to.
 * @param s    The string hash.
 * @return The new hash.
 */
static uint32_t VersionHashStr(uint32_t hash, std::string_view s)
{
	for (auto c : s) {
		hash = std::rotl(hash, 3) ^ c;
		hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
	}
	return hash;
}

/**
 * Make a hash of the file to get a unique "version number"
 * @return The version number.
 */
uint32_t StringData::Version() const
{
	uint32_t hash = 0;

	for (size_t i = 0; i < this->max_strings; i++) {
		const LangString *ls = this->strings[i].get();

		if (ls != nullptr) {
			hash ^= i * 0x717239;
			hash = (hash & 1 ? hash >> 1 ^ 0xDEADBEEF : hash >> 1);
			hash = VersionHashStr(hash, ls->name);

			const char *s = ls->english.c_str();
			ParsedCommandString cs;
			while ((cs = ParseCommandString(&s)).cmd != nullptr) {
				if (cs.cmd->flags.Test(CmdFlag::DontCount)) continue;

				hash ^= (cs.cmd - _cmd_structs) * 0x1234567;
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
size_t StringData::CountInUse(size_t tab) const
{
	size_t count = TAB_SIZE;
	while (count > 0 && this->strings[(tab * TAB_SIZE) + count - 1] == nullptr) --count;
	return count;
}

/** The buffer for writing a single string. */
struct Buffer : std::string {
	/**
	 * Convenience method for adding a byte.
	 * @param value The value to add.
	 */
	void AppendByte(uint8_t value)
	{
		this->push_back(static_cast<char>(value));
	}

	/**
	 * Add an Unicode character encoded in UTF-8 to the buffer.
	 * @param value The character to add.
	 */
	void AppendUtf8(char32_t value)
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
			StrgenWarning("Invalid unicode value U+{:04X}", static_cast<uint32_t>(value));
		}
	}
};

static size_t Utf8Validate(const char *s)
{
	char32_t c;

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

void EmitSingleChar(Buffer *buffer, const char *buf, char32_t value)
{
	if (*buf != '\0') StrgenWarning("Ignoring trailing letters in command");
	buffer->AppendUtf8(value);
}

/* The plural specifier looks like
 * {NUM} {PLURAL <ARG#> passenger passengers} then it picks either passenger/passengers depending on the count in NUM */
static std::pair<std::optional<size_t>, std::optional<size_t>> ParseRelNum(const char **buf)
{
	const char *s = *buf;
	char *end;

	while (*s == ' ' || *s == '\t') s++;
	size_t v = std::strtoul(s, &end, 0);
	if (end == s) return {};
	std::optional<size_t> offset;
	if (*end == ':') {
		/* Take the Nth within */
		s = end + 1;
		offset = std::strtoul(s, &end, 0);
		if (end == s) return {};
	}
	*buf = end;
	return {v, offset};
}

/* Parse out the next word, or nullptr */
std::optional<std::string_view> ParseWord(const char **buf)
{
	const char *s = *buf;

	while (*s == ' ' || *s == '\t') s++;
	if (*s == '\0') return {};

	if (*s == '"') {
		const char *begin = ++s;
		/* parse until next " or NUL */
		for (;;) {
			if (*s == '\0') StrgenFatal("Unterminated quotes");
			if (*s == '"') {
				*buf = s + 1;
				return std::string_view(begin, s - begin);
			}
			s++;
		}
	} else {
		/* proceed until whitespace or NUL */
		const char *begin = s;
		for (;;) {
			if (*s == '\0' || *s == ' ' || *s == '\t') {
				*buf = s;
				return std::string_view(begin, s - begin);
			}
			s++;
		}
	}
}

/* This is encoded like
 *  CommandByte <ARG#> <NUM> {Length of each string} {each string} */
static void EmitWordList(Buffer *buffer, const std::vector<std::string> &words)
{
	/* Maximum word length in bytes, excluding trailing NULL. */
	constexpr size_t MAX_WORD_LENGTH = UINT8_MAX - 2;

	buffer->AppendByte(static_cast<uint8_t>(words.size()));
	for (size_t i = 0; i < words.size(); i++) {
		size_t len = words[i].size() + 1;
		if (len >= UINT8_MAX) StrgenFatal("WordList {}/{} string '{}' too long, max bytes {}", i + 1, words.size(), words[i], MAX_WORD_LENGTH);
		buffer->AppendByte(static_cast<uint8_t>(len));
	}
	for (size_t i = 0; i < words.size(); i++) {
		buffer->append(words[i]);
		buffer->AppendByte(0);
	}
}

void EmitPlural(Buffer *buffer, const char *buf, char32_t)
{
	/* Parse out the number, if one exists. Otherwise default to prev arg. */
	auto [argidx, offset] = ParseRelNum(&buf);
	if (!argidx.has_value()) {
		if (_cur_argidx == 0) StrgenFatal("Plural choice needs positional reference");
		argidx = _cur_argidx - 1;
	}

	const CmdStruct *cmd = _cur_pcs.consuming_commands[*argidx];
	if (!offset.has_value()) {
		/* Use default offset */
		if (cmd == nullptr || !cmd->default_plural_offset.has_value()) {
			StrgenFatal("Command '{}' has no (default) plural position", cmd == nullptr ? "<empty>" : cmd->cmd);
		}
		offset = cmd->default_plural_offset;
	}

	/* Parse each string */
	std::vector<std::string> words;
	for (;;) {
		auto word = ParseWord(&buf);
		if (!word.has_value()) break;
		words.emplace_back(*word);
	}

	if (words.empty()) {
		StrgenFatal("{}: No plural words", _cur_ident);
	}

	size_t expected = _plural_forms[_lang.plural_form].plural_count;
	if (expected != words.size()) {
		if (_translated) {
			StrgenFatal("{}: Invalid number of plural forms. Expecting {}, found {}.", _cur_ident,
				expected, words.size());
		} else {
			if (_show_warnings) StrgenWarning("'{}' is untranslated. Tweaking english string to allow compilation for plural forms", _cur_ident);
			if (words.size() > expected) {
				words.resize(expected);
			} else {
				while (words.size() < expected) {
					words.push_back(words.back());
				}
			}
		}
	}

	buffer->AppendUtf8(SCC_PLURAL_LIST);
	buffer->AppendByte(_lang.plural_form);
	buffer->AppendByte(static_cast<uint8_t>(TranslateArgumentIdx(*argidx, *offset)));
	EmitWordList(buffer, words);
}

void EmitGender(Buffer *buffer, const char *buf, char32_t)
{
	if (buf[0] == '=') {
		buf++;

		/* This is a {G=DER} command */
		auto nw = _lang.GetGenderIndex(buf);
		if (nw >= MAX_NUM_GENDERS) StrgenFatal("G argument '{}' invalid", buf);

		/* now nw contains the gender index */
		buffer->AppendUtf8(SCC_GENDER_INDEX);
		buffer->AppendByte(nw);
	} else {
		/* This is a {G 0 foo bar two} command.
		 * If no relative number exists, default to +0 */
		auto [argidx, offset] = ParseRelNum(&buf);
		if (!argidx.has_value()) argidx = _cur_argidx;
		if (!offset.has_value()) offset = 0;

		const CmdStruct *cmd = _cur_pcs.consuming_commands[*argidx];
		if (cmd == nullptr || !cmd->flags.Test(CmdFlag::Gender)) {
			StrgenFatal("Command '{}' can't have a gender", cmd == nullptr ? "<empty>" : cmd->cmd);
		}

		std::vector<std::string> words;
		for (;;) {
			auto word = ParseWord(&buf);
			if (!word.has_value()) break;
			words.emplace_back(*word);
		}
		if (words.size() != _lang.num_genders) StrgenFatal("Bad # of arguments for gender command");

		assert(IsInsideBS(cmd->value, SCC_CONTROL_START, UINT8_MAX));
		buffer->AppendUtf8(SCC_GENDER_LIST);
		buffer->AppendByte(static_cast<uint8_t>(TranslateArgumentIdx(*argidx, *offset)));
		EmitWordList(buffer, words);
	}
}

static const CmdStruct *FindCmd(std::string_view s)
{
	for (const auto &cs : _cmd_structs) {
		if (cs.cmd == s) return &cs;
	}
	return nullptr;
}

static uint ResolveCaseName(std::string_view str)
{
	uint case_idx = _lang.GetCaseIndex(str);
	if (case_idx >= MAX_NUM_CASES) StrgenFatal("Invalid case-name '{}'", str);
	return case_idx + 1;
}

/* returns cmd == nullptr on eof */
static ParsedCommandString ParseCommandString(const char **str)
{
	ParsedCommandString result;
	const char *s = *str;

	/* Scan to the next command, exit if there's no next command. */
	for (; *s != '{'; s++) {
		if (*s == '\0') return {};
	}
	s++; // Skip past the {

	if (*s >= '0' && *s <= '9') {
		char *end;

		result.argno = std::strtoul(s, &end, 0);
		if (*end != ':') StrgenFatal("missing arg #");
		s = end + 1;
	}

	/* parse command name */
	const char *start = s;
	char c;
	do {
		c = *s++;
	} while (c != '}' && c != ' ' && c != '=' && c != '.' && c != 0);

	std::string_view command(start, s - start - 1);
	result.cmd = FindCmd(command);
	if (result.cmd == nullptr) {
		StrgenError("Undefined command '{}'", command);
		return {};
	}

	if (c == '.') {
		const char *casep = s;

		if (!result.cmd->flags.Test(CmdFlag::Case)) {
			StrgenFatal("Command '{}' can't have a case", result.cmd->cmd);
		}

		do {
			c = *s++;
		} while (c != '}' && c != ' ' && c != '\0');
		result.casei = ResolveCaseName(std::string_view(casep, s - casep - 1));
	}

	if (c == '\0') {
		StrgenError("Missing }} from command '{}'", start);
		return {};
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
				return {};
			}
			result.param += c;
		}
	}

	*str = s;

	return result;
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
	ParsedCommandStruct p;

	size_t argidx = 0;
	for (;;) {
		/* read until next command from a. */
		auto cs = ParseCommandString(&s);

		if (cs.cmd == nullptr) break;

		/* Sanity checking */
		if (cs.argno.has_value() && cs.cmd->consumes == 0) StrgenFatal("Non consumer param can't have a paramindex");

		if (cs.cmd->consumes > 0) {
			if (cs.argno.has_value()) argidx = *cs.argno;
			if (argidx >= p.consuming_commands.max_size()) StrgenFatal("invalid param idx {}", argidx);
			if (p.consuming_commands[argidx] != nullptr && p.consuming_commands[argidx] != cs.cmd) StrgenFatal("duplicate param idx {}", argidx);

			p.consuming_commands[argidx++] = cs.cmd;
		} else if (!cs.cmd->flags.Test(CmdFlag::DontCount)) { // Ignore some of them
			p.non_consuming_commands.emplace_back(cs.cmd, std::move(cs.param));
		}
	}

	return p;
}

const CmdStruct *TranslateCmdForCompare(const CmdStruct *a)
{
	if (a == nullptr) return nullptr;

	if (a->cmd == "STRING1" ||
			a->cmd == "STRING2" ||
			a->cmd == "STRING3" ||
			a->cmd == "STRING4" ||
			a->cmd == "STRING5" ||
			a->cmd == "STRING6" ||
			a->cmd == "STRING7" ||
			a->cmd == "RAW_STRING") {
		return FindCmd("STRING");
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
	for (size_t i = 0; i < templ.consuming_commands.max_size(); i++) {
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
			StrgenFatal("Unwanted UTF-8 character U+{:04X} in sequence '{}'", static_cast<uint32_t>(c), s);
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
			ent->translated_cases.emplace_back(ResolveCaseName(casep), s);
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

	/* For each new file we parse, reset the genders, and language codes. */
	MemSetT(&_lang, 0);
	strecpy(_lang.digit_group_separator, ",");
	strecpy(_lang.digit_group_separator_currency, ",");
	strecpy(_lang.digit_decimal_separator, ".");

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
	size_t last = 0;
	for (size_t i = 0; i < data.max_strings; i++) {
		if (data.strings[i] != nullptr) {
			this->WriteStringID(data.strings[i]->name, i);
			last = i;
		}
	}

	this->WriteStringID("STR_LAST_STRINGID", last);
}

static size_t TranslateArgumentIdx(size_t argidx, size_t offset)
{
	if (argidx >= _cur_pcs.consuming_commands.max_size()) {
		StrgenFatal("invalid argidx {}", argidx);
	}
	const CmdStruct *cs = _cur_pcs.consuming_commands[argidx];
	if (cs != nullptr && cs->consumes <= offset) {
		StrgenFatal("invalid argidx offset {}:{}", argidx, offset);
	}

	if (_cur_pcs.consuming_commands[argidx] == nullptr) {
		StrgenFatal("no command for this argidx {}", argidx);
	}

	size_t sum = 0;
	for (size_t i = 0; i < argidx; i++) {
		cs = _cur_pcs.consuming_commands[i];

		sum += (cs != nullptr) ? cs->consumes : 1;
	}

	return sum + offset;
}

static void PutArgidxCommand(Buffer *buffer)
{
	buffer->AppendUtf8(SCC_ARG_INDEX);
	buffer->AppendByte(static_cast<uint8_t>(TranslateArgumentIdx(_cur_argidx)));
}

static void PutCommandString(Buffer *buffer, const char *str)
{
	_cur_argidx = 0;

	while (*str != '\0') {
		/* Process characters as they are until we encounter a { */
		if (*str != '{') {
			buffer->append(1, *str++);
			continue;
		}

		auto cs = ParseCommandString(&str);
		auto *cmd = cs.cmd;
		if (cmd == nullptr) break;

		if (cs.casei.has_value()) {
			buffer->AppendUtf8(SCC_SET_CASE); // {SET_CASE}
			buffer->AppendByte(*cs.casei);
		}

		/* For params that consume values, we need to handle the argindex properly */
		if (cmd->consumes > 0) {
			/* Check if we need to output a move-param command */
			if (cs.argno.has_value() && *cs.argno != _cur_argidx) {
				_cur_argidx = *cs.argno;
				PutArgidxCommand(buffer);
			}

			/* Output the one from the master string... it's always accurate. */
			cmd = _cur_pcs.consuming_commands[_cur_argidx++];
			if (cmd == nullptr) {
				StrgenFatal("{}: No argument exists at position {}", _cur_ident, _cur_argidx - 1);
			}
		}

		cmd->proc(buffer, cs.param.c_str(), cmd->value);
	}
}

/**
 * Write the length as a simple gamma.
 * @param length The number to write.
 */
void LanguageWriter::WriteLength(size_t length)
{
	char buffer[2];
	size_t offs = 0;
	if (length >= 0x4000) {
		StrgenFatal("string too long");
	}

	if (length >= 0xC0) {
		buffer[offs++] = static_cast<char>(static_cast<uint8_t>((length >> 8) | 0xC0));
	}
	buffer[offs++] = static_cast<char>(static_cast<uint8_t>(length & 0xFF));
	this->Write(buffer, offs);
}

/**
 * Actually write the language.
 * @param data The data about the string.
 */
void LanguageWriter::WriteLang(const StringData &data)
{
	std::vector<size_t> in_use;
	for (size_t tab = 0; tab < data.tabs; tab++) {
		size_t n = data.CountInUse(tab);

		in_use.push_back(n);
		_lang.offsets[tab] = TO_LE16(static_cast<uint16_t>(n));

		for (size_t j = 0; j != in_use[tab]; j++) {
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
		for (size_t j = 0; j != in_use[tab]; j++) {
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
			if (ls->translated.empty()) {
				if (_show_warnings) {
					StrgenWarning("'{}' is untranslated", ls->name);
				}
				if (_annotate_todos) {
					buffer.append("<TODO> ");
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
				buffer.AppendByte(static_cast<uint8_t>(ls->translated_cases.size()));

				/* Write each case */
				for (const Case &c : ls->translated_cases) {
					buffer.AppendByte(c.caseidx);
					/* Make some space for the 16-bit length */
					size_t pos = buffer.size();
					buffer.AppendByte(0);
					buffer.AppendByte(0);
					/* Write string */
					PutCommandString(&buffer, c.string.c_str());
					buffer.AppendByte(0); // terminate with a zero
					/* Fill in the length */
					size_t size = buffer.size() - (pos + 2);
					buffer[pos + 0] = GB(size, 8, 8);
					buffer[pos + 1] = GB(size, 0, 8);
				}
			}

			if (!cmdp->empty()) PutCommandString(&buffer, cmdp->c_str());

			this->WriteLength(buffer.size());
			this->Write(buffer.data(), buffer.size());
			buffer.clear();
		}
	}
}
