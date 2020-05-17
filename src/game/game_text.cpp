/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_text.cpp Implementation of handling translated strings. */

#include "../stdafx.h"
#include "../strgen/strgen.h"
#include "../debug.h"
#include "../fileio_func.h"
#include "../tar_type.h"
#include "../script/squirrel_class.hpp"
#include "../strings_func.h"
#include "game_text.hpp"
#include "game.hpp"
#include "game_info.hpp"

#include "table/strings.h"

#include <stdarg.h>
#include <memory>

#include "../safeguards.h"

void CDECL strgen_warning(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	DEBUG(script, 0, "%s:%d: warning: %s", _file, _cur_line, buf);
	_warnings++;
}

void CDECL strgen_error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	DEBUG(script, 0, "%s:%d: error: %s", _file, _cur_line, buf);
	_errors++;
}

void NORETURN CDECL strgen_fatal(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	DEBUG(script, 0, "%s:%d: FATAL: %s", _file, _cur_line, buf);
	throw std::exception();
}

/**
 * Read all the raw language strings from the given file.
 * @param file The file to read from.
 * @return The raw strings, or nullptr upon error.
 */
LanguageStrings ReadRawLanguageStrings(const std::string &file)
{
	size_t to_read;
	FILE *fh = FioFOpenFile(file.c_str(), "rb", GAME_DIR, &to_read);
	if (fh == nullptr) return LanguageStrings();

	FileCloser fhClose(fh);

	auto pos = file.rfind(PATHSEPCHAR);
	if (pos == std::string::npos) return LanguageStrings();
	std::string langname = file.substr(pos + 1);

	/* Check for invalid empty filename */
	if (langname.empty() || langname.front() == '.') return LanguageStrings();

	LanguageStrings ret(langname.substr(0, langname.find('.')));

	char buffer[2048];
	while (to_read != 0 && fgets(buffer, sizeof(buffer), fh) != nullptr) {
		size_t len = strlen(buffer);

		/* Remove trailing spaces/newlines from the string. */
		size_t i = len;
		while (i > 0 && (buffer[i - 1] == '\r' || buffer[i - 1] == '\n' || buffer[i - 1] == ' ')) i--;
		buffer[i] = '\0';

		ret.lines.emplace_back(buffer, i);

		if (len > to_read) {
			to_read = 0;
		} else {
			to_read -= len;
		}
	}

	return ret;
}


/** A reader that simply reads using fopen. */
struct StringListReader : StringReader {
	StringList::const_iterator p;   ///< The current location of the iteration.
	StringList::const_iterator end; ///< The end of the iteration.

	/**
	 * Create the reader.
	 * @param data        The data to fill during reading.
	 * @param strings     The language strings we are reading.
	 * @param master      Are we reading the master file?
	 * @param translation Are we reading a translation?
	 */
	StringListReader(StringData &data, const LanguageStrings &strings, bool master, bool translation) :
			StringReader(data, strings.language.c_str(), master, translation), p(strings.lines.begin()), end(strings.lines.end())
	{
	}

	char *ReadLine(char *buffer, const char *last) override
	{
		if (this->p == this->end) return nullptr;

		strecpy(buffer, this->p->c_str(), last);
		this->p++;

		return buffer;
	}
};

/** Class for writing an encoded language. */
struct TranslationWriter : LanguageWriter {
	StringList &strings; ///< The encoded strings.

	/**
	 * Writer for the encoded data.
	 * @param strings The string table to add the strings to.
	 */
	TranslationWriter(StringList &strings) : strings(strings)
	{
	}

	void WriteHeader(const LanguagePackHeader *header)
	{
		/* We don't use the header. */
	}

	void Finalise()
	{
		/* Nothing to do. */
	}

	void WriteLength(uint length)
	{
		/* We don't write the length. */
	}

	void Write(const byte *buffer, size_t length)
	{
		this->strings.emplace_back((const char *)buffer, length);
	}
};

/** Class for writing the string IDs. */
struct StringNameWriter : HeaderWriter {
	StringList &strings; ///< The string names.

	/**
	 * Writer for the string names.
	 * @param strings The string table to add the strings to.
	 */
	StringNameWriter(StringList &strings) : strings(strings)
	{
	}

	void WriteStringID(const char *name, int stringid)
	{
		if (stringid == (int)this->strings.size()) this->strings.emplace_back(name);
	}

	void Finalise(const StringData &data)
	{
		/* Nothing to do. */
	}
};

/**
 * Scanner to find language files in a GameScript directory.
 */
class LanguageScanner : protected FileScanner {
private:
	GameStrings *gs;
	std::string exclude;

public:
	/** Initialise */
	LanguageScanner(GameStrings *gs, const std::string &exclude) : gs(gs), exclude(exclude) {}

	/**
	 * Scan.
	 */
	void Scan(const char *directory)
	{
		this->FileScanner::Scan(".txt", directory, false);
	}

	bool AddFile(const char *filename, size_t basepath_length, const char *tar_filename) override
	{
		if (exclude == filename) return true;

		auto ls = ReadRawLanguageStrings(filename);
		if (!ls.IsValid()) return false;

		gs->raw_strings.push_back(std::move(ls));
		return true;
	}
};

/**
 * Load all translations that we know of.
 * @return Container with all (compiled) translations.
 */
GameStrings *LoadTranslations()
{
	const GameInfo *info = Game::GetInfo();
	std::string basename(info->GetMainScript());
	auto e = basename.rfind(PATHSEPCHAR);
	if (e == std::string::npos) return nullptr;
	basename.erase(e + 1);

	std::string filename = basename + "lang" PATHSEP "english.txt";
	if (!FioCheckFileExists(filename.c_str() , GAME_DIR)) return nullptr;

	auto ls = ReadRawLanguageStrings(filename);
	if (!ls.IsValid()) return nullptr;

	GameStrings *gs = new GameStrings();
	try {
		gs->raw_strings.push_back(std::move(ls));

		/* Scan for other language files */
		LanguageScanner scanner(gs, filename);
		std::string ldir = basename + "lang" PATHSEP;

		const char *tar_filename = info->GetTarFile();
		TarList::iterator iter;
		if (tar_filename != nullptr && (iter = _tar_list[GAME_DIR].find(tar_filename)) != _tar_list[GAME_DIR].end()) {
			/* The main script is in a tar file, so find all files that
			 * are in the same tar and add them to the langfile scanner. */
			TarFileList::iterator tar;
			FOR_ALL_TARS(tar, GAME_DIR) {
				/* Not in the same tar. */
				if (tar->second.tar_filename != iter->first) continue;

				/* Check the path and extension. */
				if (tar->first.size() <= ldir.size() || tar->first.compare(0, ldir.size(), ldir) != 0) continue;
				if (tar->first.compare(tar->first.size() - 4, 4, ".txt") != 0) continue;

				scanner.AddFile(tar->first.c_str(), 0, tar_filename);
			}
		} else {
			/* Scan filesystem */
			scanner.Scan(ldir.c_str());
		}

		gs->Compile();
		return gs;
	} catch (...) {
		delete gs;
		return nullptr;
	}
}

/** Compile the language. */
void GameStrings::Compile()
{
	StringData data(32);
	StringListReader master_reader(data, this->raw_strings[0], true, false);
	master_reader.ParseFile();
	if (_errors != 0) throw std::exception();

	this->version = data.Version();

	StringNameWriter id_writer(this->string_names);
	id_writer.WriteHeader(data);

	for (const auto &p : this->raw_strings) {
		data.FreeTranslation();
		StringListReader translation_reader(data, p, false, p.language != "english");
		translation_reader.ParseFile();
		if (_errors != 0) throw std::exception();

		this->compiled_strings.emplace_back(p.language);
		TranslationWriter writer(this->compiled_strings.back().lines);
		writer.WriteLang(data);
	}
}

/** The currently loaded game strings. */
GameStrings *_current_data = nullptr;

/**
 * Get the string pointer of a particular game string.
 * @param id The ID of the game string.
 * @return The encoded string.
 */
const char *GetGameStringPtr(uint id)
{
	if (id >= _current_data->cur_language->lines.size()) return GetStringPtr(STR_UNDEFINED);
	return _current_data->cur_language->lines[id].c_str();
}

/**
 * Register the current translation to the Squirrel engine.
 * @param engine The engine to update/
 */
void RegisterGameTranslation(Squirrel *engine)
{
	delete _current_data;
	_current_data = LoadTranslations();
	if (_current_data == nullptr) return;

	HSQUIRRELVM vm = engine->GetVM();
	sq_pushroottable(vm);
	sq_pushstring(vm, "GSText", -1);
	if (SQ_FAILED(sq_get(vm, -2))) return;

	int idx = 0;
	for (const auto &p : _current_data->string_names) {
		sq_pushstring(vm, p.c_str(), -1);
		sq_pushinteger(vm, idx);
		sq_rawset(vm, -3);
		idx++;
	}

	sq_pop(vm, 2);

	ReconsiderGameScriptLanguage();
}

/**
 * Reconsider the game script language, so we use the right one.
 */
void ReconsiderGameScriptLanguage()
{
	if (_current_data == nullptr) return;

	char temp[MAX_PATH];
	strecpy(temp, _current_language->file, lastof(temp));

	/* Remove the extension */
	char *l = strrchr(temp, '.');
	assert(l != nullptr);
	*l = '\0';

	/* Skip the path */
	char *language = strrchr(temp, PATHSEPCHAR);
	assert(language != nullptr);
	language++;

	for (auto &p : _current_data->compiled_strings) {
		if (p.language == language) {
			_current_data->cur_language = &p;
			return;
		}
	}

	_current_data->cur_language = &_current_data->compiled_strings[0];
}
