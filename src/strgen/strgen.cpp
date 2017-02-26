/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen.cpp Tool to create computer readable (stand-alone) translation files. */

#include "../stdafx.h"
#include "../core/endian_func.hpp"
#include "../string_func.h"
#include "../strings_type.h"
#include "../misc/getoptdata.h"
#include "../table/control_codes.h"

#include "strgen.h"

#include <stdarg.h>
#include <exception>

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

#include "../safeguards.h"


#ifdef _MSC_VER
# define LINE_NUM_FMT(s) "%s (%d): warning: %s (" s ")\n"
#else
# define LINE_NUM_FMT(s) "%s:%d: " s ": %s\n"
#endif

void CDECL strgen_warning(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("warning"), _file, _cur_line, buf);
	_warnings++;
}

void CDECL strgen_error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("error"), _file, _cur_line, buf);
	_errors++;
}

void NORETURN CDECL strgen_fatal(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("FATAL"), _file, _cur_line, buf);
#ifdef _MSC_VER
	fprintf(stderr, LINE_NUM_FMT("warning"), _file, _cur_line, "language is not compiled");
#endif
	throw std::exception();
}

void NORETURN CDECL error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	fprintf(stderr, LINE_NUM_FMT("FATAL"), _file, _cur_line, buf);
#ifdef _MSC_VER
	fprintf(stderr, LINE_NUM_FMT("warning"), _file, _cur_line, "language is not compiled");
#endif
	exit(2);
}

/** A reader that simply reads using fopen. */
struct FileStringReader : StringReader {
	FILE *fh; ///< The file we are reading.

	/**
	 * Create the reader.
	 * @param data        The data to fill during reading.
	 * @param file        The file we are reading.
	 * @param master      Are we reading the master file?
	 * @param translation Are we reading a translation?
	 */
	FileStringReader(StringData &data, const char *file, bool master, bool translation) :
			StringReader(data, file, master, translation)
	{
		this->fh = fopen(file, "rb");
		if (this->fh == NULL) error("Could not open %s", file);
	}

	/** Free/close the file. */
	virtual ~FileStringReader()
	{
		fclose(this->fh);
	}

	/* virtual */ char *ReadLine(char *buffer, const char *last)
	{
		return fgets(buffer, ClampToU16(last - buffer + 1), this->fh);
	}

	/* virtual */ void HandlePragma(char *str);

	/* virtual */ void ParseFile()
	{
		this->StringReader::ParseFile();

		if (StrEmpty(_lang.name) || StrEmpty(_lang.own_name) || StrEmpty(_lang.isocode)) {
			error("Language must include ##name, ##ownname and ##isocode");
		}
	}
};

void FileStringReader::HandlePragma(char *str)
{
	if (!memcmp(str, "id ", 3)) {
		this->data.next_string_id = strtoul(str + 3, NULL, 0);
	} else if (!memcmp(str, "name ", 5)) {
		strecpy(_lang.name, str + 5, lastof(_lang.name));
	} else if (!memcmp(str, "ownname ", 8)) {
		strecpy(_lang.own_name, str + 8, lastof(_lang.own_name));
	} else if (!memcmp(str, "isocode ", 8)) {
		strecpy(_lang.isocode, str + 8, lastof(_lang.isocode));
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
		if (this->master) error("Genders are not allowed in the base translation.");
		char *buf = str + 7;

		for (;;) {
			const char *s = ParseWord(&buf);

			if (s == NULL) break;
			if (_lang.num_genders >= MAX_NUM_GENDERS) error("Too many genders, max %d", MAX_NUM_GENDERS);
			strecpy(_lang.genders[_lang.num_genders], s, lastof(_lang.genders[_lang.num_genders]));
			_lang.num_genders++;
		}
	} else if (!memcmp(str, "case ", 5)) {
		if (this->master) error("Cases are not allowed in the base translation.");
		char *buf = str + 5;

		for (;;) {
			const char *s = ParseWord(&buf);

			if (s == NULL) break;
			if (_lang.num_cases >= MAX_NUM_CASES) error("Too many cases, max %d", MAX_NUM_CASES);
			strecpy(_lang.cases[_lang.num_cases], s, lastof(_lang.cases[_lang.num_cases]));
			_lang.num_cases++;
		}
	} else {
		StringReader::HandlePragma(str);
	}
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
	} while (l1 != 0);

	fclose(f2);
	fclose(f1);
	return true;
}

/** Base class for writing data to disk. */
struct FileWriter {
	FILE *fh;             ///< The file handle we're writing to.
	const char *filename; ///< The file name we're writing to.

	/**
	 * Open a file to write to.
	 * @param filename The file to open.
	 */
	FileWriter(const char *filename)
	{
		this->filename = stredup(filename);
		this->fh = fopen(this->filename, "wb");

		if (this->fh == NULL) {
			error("Could not open %s", this->filename);
		}
	}

	/** Finalise the writing. */
	void Finalise()
	{
		fclose(this->fh);
		this->fh = NULL;
	}

	/** Make sure the file is closed. */
	virtual ~FileWriter()
	{
		/* If we weren't closed an exception was thrown, so remove the temporary file. */
		if (fh != NULL) {
			fclose(this->fh);
			unlink(this->filename);
		}
		free(this->filename);
	}
};

struct HeaderFileWriter : HeaderWriter, FileWriter {
	/** The real file name we eventually want to write to. */
	const char *real_filename;
	/** The previous string ID that was printed. */
	int prev;

	/**
	 * Open a file to write to.
	 * @param filename The file to open.
	 */
	HeaderFileWriter(const char *filename) : FileWriter("tmp.xxx"),
		real_filename(stredup(filename)), prev(0)
	{
		fprintf(this->fh, "/* This file is automatically generated. Do not modify */\n\n");
		fprintf(this->fh, "#ifndef TABLE_STRINGS_H\n");
		fprintf(this->fh, "#define TABLE_STRINGS_H\n");
	}

	/** Free the filename. */
	~HeaderFileWriter()
	{
		free(real_filename);
	}

	void WriteStringID(const char *name, int stringid)
	{
		if (prev + 1 != stringid) fprintf(this->fh, "\n");
		fprintf(this->fh, "static const StringID %s = 0x%X;\n", name, stringid);
		prev = stringid;
	}

	void Finalise(const StringData &data)
	{
		/* Find the plural form with the most amount of cases. */
		int max_plural_forms = 0;
		for (uint i = 0; i < lengthof(_plural_forms); i++) {
			max_plural_forms = max(max_plural_forms, _plural_forms[i].plural_count);
		}

		fprintf(this->fh,
			"\n"
			"static const uint LANGUAGE_PACK_VERSION     = 0x%X;\n"
			"static const uint LANGUAGE_MAX_PLURAL       = %d;\n"
			"static const uint LANGUAGE_MAX_PLURAL_FORMS = %d;\n\n",
			(uint)data.Version(), (uint)lengthof(_plural_forms), max_plural_forms
		);

		fprintf(this->fh, "#endif /* TABLE_STRINGS_H */\n");

		this->FileWriter::Finalise();

		if (CompareFiles(this->filename, this->real_filename)) {
			/* files are equal. tmp.xxx is not needed */
			unlink(this->filename);
		} else {
			/* else rename tmp.xxx into filename */
	#if defined(WIN32) || defined(WIN64)
			unlink(this->real_filename);
	#endif
			if (rename(this->filename, this->real_filename) == -1) error("rename() failed");
		}
	}
};

/** Class for writing a language to disk. */
struct LanguageFileWriter : LanguageWriter, FileWriter {
	/**
	 * Open a file to write to.
	 * @param filename The file to open.
	 */
	LanguageFileWriter(const char *filename) : FileWriter(filename)
	{
	}

	void WriteHeader(const LanguagePackHeader *header)
	{
		this->Write((const byte *)header, sizeof(*header));
	}

	void Finalise()
	{
		if (fputc(0, this->fh) == EOF) {
			error("Could not write to %s", this->filename);
		}
		this->FileWriter::Finalise();
	}

	void Write(const byte *buffer, size_t length)
	{
		if (fwrite(buffer, sizeof(*buffer), length, this->fh) != length) {
			error("Could not write to %s", this->filename);
		}
	}
};

/** Multi-OS mkdirectory function */
static inline void ottd_mkdir(const char *directory)
{
	/* Ignore directory creation errors; they'll surface later on, and most
	 * of the time they are 'directory already exists' errors anyhow. */
#if defined(WIN32) || defined(__WATCOMC__)
	mkdir(directory);
#else
	mkdir(directory, 0755);
#endif
}

/**
 * Create a path consisting of an already existing path, a possible
 * path separator and the filename. The separator is only appended if the path
 * does not already end with a separator
 */
static inline char *mkpath(char *buf, const char *last, const char *path, const char *file)
{
	strecpy(buf, path, last); // copy directory into buffer

	char *p = strchr(buf, '\0'); // add path separator if necessary
	if (p[-1] != PATHSEPCHAR && p != last) *p++ = PATHSEPCHAR;
	strecpy(p, file, last); // concatenate filename at end of buffer
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

/** Options of strgen. */
static const OptionData _opts[] = {
	  GETOPT_NOVAL(     'v',  "--version"),
	GETOPT_GENERAL('C', '\0', "-export-commands", ODF_NO_VALUE),
	GETOPT_GENERAL('L', '\0', "-export-plurals",  ODF_NO_VALUE),
	GETOPT_GENERAL('P', '\0', "-export-pragmas",  ODF_NO_VALUE),
	  GETOPT_NOVAL(     't',  "--todo"),
	  GETOPT_NOVAL(     'w',  "--warning"),
	  GETOPT_NOVAL(     'h',  "--help"),
	GETOPT_GENERAL('h', '?',  NULL,               ODF_NO_VALUE),
	  GETOPT_VALUE(     's',  "--source_dir"),
	  GETOPT_VALUE(     'd',  "--dest_dir"),
	GETOPT_END(),
};

int CDECL main(int argc, char *argv[])
{
	char pathbuf[MAX_PATH];
	const char *src_dir = ".";
	const char *dest_dir = NULL;

	GetOptData mgo(argc - 1, argv + 1, _opts);
	for (;;) {
		int i = mgo.GetOpt();
		if (i == -1) break;

		switch (i) {
			case 'v':
				puts("$Revision$");
				return 0;

			case 'C':
				printf("args\tflags\tcommand\treplacement\n");
				for (const CmdStruct *cs = _cmd_structs; cs < endof(_cmd_structs); cs++) {
					char flags;
					if (cs->proc == EmitGender) {
						flags = 'g'; // Command needs number of parameters defined by number of genders
					} else if (cs->proc == EmitPlural) {
						flags = 'p'; // Command needs number of parameters defined by plural value
					} else if (cs->flags & C_DONTCOUNT) {
						flags = 'i'; // Command may be in the translation when it is not in base
					} else {
						flags = '0'; // Command needs no parameters
					}
					printf("%i\t%c\t\"%s\"\t\"%s\"\n", cs->consumes, flags, cs->cmd, strstr(cs->cmd, "STRING") ? "STRING" : cs->cmd);
				}
				return 0;

			case 'L':
				printf("count\tdescription\tnames\n");
				for (const PluralForm *pf = _plural_forms; pf < endof(_plural_forms); pf++) {
					printf("%i\t\"%s\"\t%s\n", pf->plural_count, pf->description, pf->names);
				}
				return 0;

			case 'P':
				printf("name\tflags\tdefault\tdescription\n");
				for (size_t i = 0; i < lengthof(_pragmas); i++) {
					printf("\"%s\"\t%s\t\"%s\"\t\"%s\"\n",
							_pragmas[i][0], _pragmas[i][1], _pragmas[i][2], _pragmas[i][3]);
				}
				return 0;

			case 't':
				_show_todo |= 1;
				break;

			case 'w':
				_show_todo |= 2;
				break;

			case 'h':
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

			case 's':
				src_dir = replace_pathsep(mgo.opt);
				break;

			case 'd':
				dest_dir = replace_pathsep(mgo.opt);
				break;

			case -2:
				fprintf(stderr, "Invalid arguments\n");
				return 0;
		}
	}

	if (dest_dir == NULL) dest_dir = src_dir; // if dest_dir is not specified, it equals src_dir

	try {
		/* strgen has two modes of operation. If no (free) arguments are passed
		 * strgen generates strings.h to the destination directory. If it is supplied
		 * with a (free) parameter the program will translate that language to destination
		 * directory. As input english.txt is parsed from the source directory */
		if (mgo.numleft == 0) {
			mkpath(pathbuf, lastof(pathbuf), src_dir, "english.txt");

			/* parse master file */
			StringData data(TEXT_TAB_END);
			FileStringReader master_reader(data, pathbuf, true, false);
			master_reader.ParseFile();
			if (_errors != 0) return 1;

			/* write strings.h */
			ottd_mkdir(dest_dir);
			mkpath(pathbuf, lastof(pathbuf), dest_dir, "strings.h");

			HeaderFileWriter writer(pathbuf);
			writer.WriteHeader(data);
			writer.Finalise(data);
		} else if (mgo.numleft >= 1) {
			char *r;

			mkpath(pathbuf, lastof(pathbuf), src_dir, "english.txt");

			StringData data(TEXT_TAB_END);
			/* parse master file and check if target file is correct */
			FileStringReader master_reader(data, pathbuf, true, false);
			master_reader.ParseFile();

			for (int i = 0; i < mgo.numleft; i++) {
				data.FreeTranslation();

				const char *translation = replace_pathsep(mgo.argv[i]);
				const char *file = strrchr(translation, PATHSEPCHAR);
				FileStringReader translation_reader(data, translation, false, file == NULL || strcmp(file + 1, "english.txt") != 0);
				translation_reader.ParseFile(); // target file
				if (_errors != 0) return 1;

				/* get the targetfile, strip any directories and append to destination path */
				r = strrchr(mgo.argv[i], PATHSEPCHAR);
				mkpath(pathbuf, lastof(pathbuf), dest_dir, (r != NULL) ? &r[1] : mgo.argv[i]);

				/* rename the .txt (input-extension) to .lng */
				r = strrchr(pathbuf, '.');
				if (r == NULL || strcmp(r, ".txt") != 0) r = strchr(pathbuf, '\0');
				strecpy(r, ".lng", lastof(pathbuf));

				LanguageFileWriter writer(pathbuf);
				writer.WriteLang(data);
				writer.Finalise();

				/* if showing warnings, print a summary of the language */
				if ((_show_todo & 2) != 0) {
					fprintf(stdout, "%d warnings and %d errors for %s\n", _warnings, _errors, pathbuf);
				}
			}
		}
	} catch (...) {
		return 2;
	}

	return 0;
}
