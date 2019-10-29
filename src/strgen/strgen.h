/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen.h Structures related to strgen. */

#ifndef STRGEN_H
#define STRGEN_H

#include "../language.h"

/** Container for the different cases of a string. */
struct Case {
	int caseidx;  ///< The index of the case.
	char *string; ///< The translation of the case.
	Case *next;   ///< The next, chained, case.

	Case(int caseidx, const char *string, Case *next);
	~Case();
};

/** Information about a single string. */
struct LangString {
	char *name;            ///< Name of the string.
	char *english;         ///< English text.
	char *translated;      ///< Translated text.
	size_t hash_next;      ///< Next hash entry.
	size_t index;          ///< The index in the language file.
	int line;              ///< Line of string in source-file.
	Case *translated_case; ///< Cases of the translation.

	LangString(const char *name, const char *english, size_t index, int line);
	~LangString();
	void FreeTranslation();
};

/** Information about the currently known strings. */
struct StringData {
	LangString **strings; ///< Array of all known strings.
	size_t *hash_heads;   ///< Hash table for the strings.
	size_t tabs;          ///< The number of 'tabs' of strings.
	size_t max_strings;   ///< The maximum number of strings.
	size_t next_string_id;///< The next string ID to allocate.

	StringData(size_t tabs);
	~StringData();
	void FreeTranslation();
	uint HashStr(const char *s) const;
	void Add(const char *s, LangString *ls);
	LangString *Find(const char *s);
	uint VersionHashStr(uint hash, const char *s) const;
	uint Version() const;
	uint CountInUse(uint tab) const;
};

/** Helper for reading strings. */
struct StringReader {
	StringData &data; ///< The data to fill during reading.
	const char *file; ///< The file we are reading.
	bool master;      ///< Are we reading the master file?
	bool translation; ///< Are we reading a translation, implies !master. However, the base translation will have this false.

	StringReader(StringData &data, const char *file, bool master, bool translation);
	virtual ~StringReader();
	void HandleString(char *str);

	/**
	 * Read a single line from the source of strings.
	 * @param buffer The buffer to read the data in to.
	 * @param last   The last element in the buffer.
	 * @return The buffer, or nullptr if at the end of the file.
	 */
	virtual char *ReadLine(char *buffer, const char *last) = 0;

	/**
	 * Handle the pragma of the file.
	 * @param str    The pragma string to parse.
	 */
	virtual void HandlePragma(char *str);

	/**
	 * Start parsing the file.
	 */
	virtual void ParseFile();
};

/** Base class for writing the header, i.e. the STR_XXX to numeric value. */
struct HeaderWriter {
	/**
	 * Write the string ID.
	 * @param name     The name of the string.
	 * @param stringid The ID of the string.
	 */
	virtual void WriteStringID(const char *name, int stringid) = 0;

	/**
	 * Finalise writing the file.
	 * @param data The data about the string.
	 */
	virtual void Finalise(const StringData &data) = 0;

	/** Especially destroy the subclasses. */
	virtual ~HeaderWriter() {};

	void WriteHeader(const StringData &data);
};

/** Base class for all language writers. */
struct LanguageWriter {
	/**
	 * Write the header metadata. The multi-byte integers are already converted to
	 * the little endian format.
	 * @param header The header to write.
	 */
	virtual void WriteHeader(const LanguagePackHeader *header) = 0;

	/**
	 * Write a number of bytes.
	 * @param buffer The buffer to write.
	 * @param length The amount of byte to write.
	 */
	virtual void Write(const byte *buffer, size_t length) = 0;

	/**
	 * Finalise writing the file.
	 */
	virtual void Finalise() = 0;

	/** Especially destroy the subclasses. */
	virtual ~LanguageWriter() {}

	virtual void WriteLength(uint length);
	virtual void WriteLang(const StringData &data);
};

void CDECL strgen_warning(const char *s, ...) WARN_FORMAT(1, 2);
void CDECL strgen_error(const char *s, ...) WARN_FORMAT(1, 2);
void NORETURN CDECL strgen_fatal(const char *s, ...) WARN_FORMAT(1, 2);
char *ParseWord(char **buf);

extern const char *_file;
extern int _cur_line;
extern int _errors, _warnings, _show_todo;
extern LanguagePackHeader _lang;

#endif /* STRGEN_H */
