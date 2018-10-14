/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settingsgen.cpp Tool to create computer-readable settings. */

#include "../stdafx.h"
#include "../string_func.h"
#include "../strings_type.h"
#include "../misc/getoptdata.h"
#include "../ini_type.h"
#include "../core/smallvec_type.hpp"

#include <stdarg.h>

#if (!defined(WIN32) && !defined(WIN64)) || defined(__CYGWIN__)
#include <unistd.h>
#include <sys/stat.h>
#endif

#ifdef __MORPHOS__
#ifdef stderr
#undef stderr
#endif
#define stderr stdout
#endif /* __MORPHOS__ */

#include "../safeguards.h"

/**
 * Report a fatal error.
 * @param s Format string.
 * @note Function does not return.
 */
void NORETURN CDECL error(const char *s, ...)
{
	char buf[1024];
	va_list va;
	va_start(va, s);
	vseprintf(buf, lastof(buf), s, va);
	va_end(va);
	fprintf(stderr, "FATAL: %s\n", buf);
	exit(1);
}

static const int OUTPUT_BLOCK_SIZE = 16000; ///< Block size of the buffer in #OutputBuffer.

/** Output buffer for a block of data. */
class OutputBuffer {
public:
	/** Prepare buffer for use. */
	void Clear()
	{
		this->size = 0;
	}

	/**
	 * Add text to the output buffer.
	 * @param text   Text to store.
	 * @param length Length of the text in bytes.
	 * @return Number of bytes actually stored.
	 */
	int Add(const char *text, int length)
	{
		int store_size = min(length, OUTPUT_BLOCK_SIZE - this->size);
		assert(store_size >= 0);
		assert(store_size <= OUTPUT_BLOCK_SIZE);
		MemCpyT(this->data + this->size, text, store_size);
		this->size += store_size;
		return store_size;
	}

	/**
	 * Dump buffer to the output stream.
	 * @param out_fp Stream to write the \a data to.
	 */
	void Write(FILE *out_fp) const
	{
		if (fwrite(this->data, 1, this->size, out_fp) != (size_t)this->size) {
			fprintf(stderr, "Error: Cannot write output\n");
		}
	}

	/**
	 * Does the block have room for more data?
	 * @return \c true if room is available, else \c false.
	 */
	bool HasRoom() const
	{
		return this->size < OUTPUT_BLOCK_SIZE;
	}

	int size;                     ///< Number of bytes stored in \a data.
	char data[OUTPUT_BLOCK_SIZE]; ///< Stored data.
};

/** Temporarily store output. */
class OutputStore {
public:
	OutputStore()
	{
		this->Clear();
	}

	/** Clear the temporary storage. */
	void Clear()
	{
		this->output_buffer.Clear();
	}

	/**
	 * Add text to the output storage.
	 * @param text   Text to store.
	 * @param length Length of the text in bytes, \c 0 means 'length of the string'.
	 */
	void Add(const char *text, int length = 0)
	{
		if (length == 0) length = strlen(text);

		if (length > 0 && this->BufferHasRoom()) {
			int stored_size = this->output_buffer[this->output_buffer.Length() - 1].Add(text, length);
			length -= stored_size;
			text += stored_size;
		}
		while (length > 0) {
			OutputBuffer *block = this->output_buffer.Append();
			block->Clear(); // Initialize the new block.
			int stored_size = block->Add(text, length);
			length -= stored_size;
			text += stored_size;
		}
	}

	/**
	 * Write all stored output to the output stream.
	 * @param out_fp Stream to write the \a data to.
	 */
	void Write(FILE *out_fp) const
	{
		for (const OutputBuffer *out_data = this->output_buffer.Begin(); out_data != this->output_buffer.End(); out_data++) {
			out_data->Write(out_fp);
		}
	}

private:
	/**
	 * Does the buffer have room without adding a new #OutputBuffer block?
	 * @return \c true if room is available, else \c false.
	 */
	bool BufferHasRoom() const
	{
		uint num_blocks = this->output_buffer.Length();
		return num_blocks > 0 && this->output_buffer[num_blocks - 1].HasRoom();
	}

	typedef SmallVector<OutputBuffer, 2> OutputBufferVector; ///< Vector type for output buffers.
	OutputBufferVector output_buffer; ///< Vector of blocks containing the stored output.
};


/** Derived class for loading INI files without going through Fio stuff. */
struct SettingsIniFile : IniLoadFile {
	/**
	 * Construct a new ini loader.
	 * @param list_group_names A \c NULL terminated list with group names that should be loaded as lists instead of variables. @see IGT_LIST
	 * @param seq_group_names  A \c NULL terminated list with group names that should be loaded as lists of names. @see IGT_SEQUENCE
	 */
	SettingsIniFile(const char * const *list_group_names = NULL, const char * const *seq_group_names = NULL) :
			IniLoadFile(list_group_names, seq_group_names)
	{
	}

	virtual FILE *OpenFile(const char *filename, Subdirectory subdir, size_t *size)
	{
		/* Open the text file in binary mode to prevent end-of-line translations
		 * done by ftell() and friends, as defined by K&R. */
		FILE *in = fopen(filename, "rb");
		if (in == NULL) return NULL;

		fseek(in, 0L, SEEK_END);
		*size = ftell(in);

		fseek(in, 0L, SEEK_SET); // Seek back to the start of the file.
		return in;
	}

	virtual void ReportFileError(const char * const pre, const char * const buffer, const char * const post)
	{
		error("%s%s%s", pre, buffer, post);
	}
};

OutputStore _stored_output; ///< Temporary storage of the output, until all processing is done.

static const char *PREAMBLE_GROUP_NAME  = "pre-amble";  ///< Name of the group containing the pre amble.
static const char *POSTAMBLE_GROUP_NAME = "post-amble"; ///< Name of the group containing the post amble.
static const char *TEMPLATES_GROUP_NAME = "templates";  ///< Name of the group containing the templates.
static const char *DEFAULTS_GROUP_NAME  = "defaults";   ///< Name of the group containing default values for the template variables.

/**
 * Load the INI file.
 * @param filename Name of the file to load.
 * @return         Loaded INI data.
 */
static IniLoadFile *LoadIniFile(const char *filename)
{
	static const char * const seq_groups[] = {PREAMBLE_GROUP_NAME, POSTAMBLE_GROUP_NAME, NULL};

	IniLoadFile *ini = new SettingsIniFile(NULL, seq_groups);
	ini->LoadFromDisk(filename, NO_DIRECTORY);
	return ini;
}

/**
 * Dump a #IGT_SEQUENCE group into #_stored_output.
 * @param ifile      Loaded INI data.
 * @param group_name Name of the group to copy.
 */
static void DumpGroup(IniLoadFile *ifile, const char * const group_name)
{
	IniGroup *grp = ifile->GetGroup(group_name, 0, false);
	if (grp != NULL && grp->type == IGT_SEQUENCE) {
		for (IniItem *item = grp->item; item != NULL; item = item->next) {
			if (item->name) {
				_stored_output.Add(item->name);
				_stored_output.Add("\n", 1);
			}
		}
	}
}

/**
 * Find the value of a template variable.
 * @param name Name of the item to find.
 * @param grp  Group currently being expanded (searched first).
 * @param defaults Fallback group to search, \c NULL skips the search.
 * @return Text of the item if found, else \c NULL.
 */
static const char *FindItemValue(const char *name, IniGroup *grp, IniGroup *defaults)
{
	IniItem *item = grp->GetItem(name, false);
	if (item == NULL && defaults != NULL) item = defaults->GetItem(name, false);
	if (item == NULL || item->value == NULL) return NULL;
	return item->value;
}

/**
 * Output all non-special sections through the template / template variable expansion system.
 * @param ifile Loaded INI data.
 */
static void DumpSections(IniLoadFile *ifile)
{
	static const int MAX_VAR_LENGTH = 64;
	static const char * const special_group_names[] = {PREAMBLE_GROUP_NAME, POSTAMBLE_GROUP_NAME, DEFAULTS_GROUP_NAME, TEMPLATES_GROUP_NAME, NULL};

	IniGroup *default_grp = ifile->GetGroup(DEFAULTS_GROUP_NAME, 0, false);
	IniGroup *templates_grp  = ifile->GetGroup(TEMPLATES_GROUP_NAME, 0, false);
	if (templates_grp == NULL) return;

	/* Output every group, using its name as template name. */
	for (IniGroup *grp = ifile->group; grp != NULL; grp = grp->next) {
		const char * const *sgn;
		for (sgn = special_group_names; *sgn != NULL; sgn++) if (strcmp(grp->name, *sgn) == 0) break;
		if (*sgn != NULL) continue;

		IniItem *template_item = templates_grp->GetItem(grp->name, false); // Find template value.
		if (template_item == NULL || template_item->value == NULL) {
			fprintf(stderr, "settingsgen: Warning: Cannot find template %s\n", grp->name);
			continue;
		}

		/* Prefix with #if/#ifdef/#ifndef */
		static const char * const pp_lines[] = {"if", "ifdef", "ifndef", NULL};
		int count = 0;
		for (const char * const *name = pp_lines; *name != NULL; name++) {
			const char *condition = FindItemValue(*name, grp, default_grp);
			if (condition != NULL) {
				_stored_output.Add("#", 1);
				_stored_output.Add(*name);
				_stored_output.Add(" ", 1);
				_stored_output.Add(condition);
				_stored_output.Add("\n", 1);
				count++;
			}
		}

		/* Output text of the template, except template variables of the form '$[_a-z0-9]+' which get replaced by their value. */
		const char *txt = template_item->value;
		while (*txt != '\0') {
			if (*txt != '$') {
				_stored_output.Add(txt, 1);
				txt++;
				continue;
			}
			txt++;
			if (*txt == '$') { // Literal $
				_stored_output.Add(txt, 1);
				txt++;
				continue;
			}

			/* Read variable. */
			char variable[MAX_VAR_LENGTH];
			int i = 0;
			while (i < MAX_VAR_LENGTH - 1) {
				if (!(txt[i] == '_' || (txt[i] >= 'a' && txt[i] <= 'z') || (txt[i] >= '0' && txt[i] <= '9'))) break;
				variable[i] = txt[i];
				i++;
			}
			variable[i] = '\0';
			txt += i;

			if (i > 0) {
				/* Find the text to output. */
				const char *valitem = FindItemValue(variable, grp, default_grp);
				if (valitem != NULL) _stored_output.Add(valitem);
			} else {
				_stored_output.Add("$", 1);
			}
		}
		_stored_output.Add("\n", 1); // \n after the expanded template.
		while (count > 0) {
			_stored_output.Add("#endif\n");
			count--;
		}
	}
}

/**
 * Copy a file to the output.
 * @param fname Filename of file to copy.
 * @param out_fp Output stream to write to.
 */
static void CopyFile(const char *fname, FILE *out_fp)
{
	if (fname == NULL) return;

	FILE *in_fp = fopen(fname, "r");
	if (in_fp == NULL) {
		fprintf(stderr, "settingsgen: Warning: Cannot open file %s for copying\n", fname);
		return;
	}

	char buffer[4096];
	size_t length;
	do {
		length = fread(buffer, 1, lengthof(buffer), in_fp);
		if (fwrite(buffer, 1, length, out_fp) != length) {
			fprintf(stderr, "Error: Cannot copy file\n");
			break;
		}
	} while (length == lengthof(buffer));

	fclose(in_fp);
}

/**
 * Compare two files for identity.
 * @param n1 First file.
 * @param n2 Second file.
 * @return True if both files are identical.
 */
static bool CompareFiles(const char *n1, const char *n2)
{
	FILE *f2 = fopen(n2, "rb");
	if (f2 == NULL) return false;

	FILE *f1 = fopen(n1, "rb");
	if (f1 == NULL) {
		fclose(f2);
		error("can't open %s", n1);
	}

	size_t l1, l2;
	do {
		char b1[4096];
		char b2[4096];
		l1 = fread(b1, 1, sizeof(b1), f1);
		l2 = fread(b2, 1, sizeof(b2), f2);

		if (l1 != l2 || memcmp(b1, b2, l1) != 0) {
			fclose(f2);
			fclose(f1);
			return false;
		}
	} while (l1 != 0);

	fclose(f2);
	fclose(f1);
	return true;
}

/** Options of settingsgen. */
static const OptionData _opts[] = {
	  GETOPT_NOVAL(     'v', "--version"),
	  GETOPT_NOVAL(     'h', "--help"),
	GETOPT_GENERAL('h', '?', NULL, ODF_NO_VALUE),
	  GETOPT_VALUE(     'o', "--output"),
	  GETOPT_VALUE(     'b', "--before"),
	  GETOPT_VALUE(     'a', "--after"),
	GETOPT_END(),
};

/**
 * Process a single INI file.
 * The file should have a [templates] group, where each item is one template.
 * Variables in a template have the form '\$[_a-z0-9]+' (a literal '$' followed
 * by one or more '_', lowercase letters, or lowercase numbers).
 *
 * After loading, the [pre-amble] group is copied verbatim if it exists.
 *
 * For every group with a name that matches a template name the template is written.
 * It starts with a optional \c \#if line if an 'if' item exists in the group. The item
 * value is used as condition. Similarly, \c \#ifdef and \c \#ifndef lines are also written.
 * Below the macro processor directives, the value of the template is written
 * at a line with its variables replaced by item values of the group being written.
 * If the group has no item for the variable, the [defaults] group is tried as fall back.
 * Finally, \c \#endif lines are written to match the macro processor lines.
 *
 * Last but not least, the [post-amble] group is copied verbatim.
 *
 * @param fname  Ini file to process. @return Exit status of the processing.
 */
static void ProcessIniFile(const char *fname)
{
	IniLoadFile *ini_data = LoadIniFile(fname);
	DumpGroup(ini_data, PREAMBLE_GROUP_NAME);
	DumpSections(ini_data);
	DumpGroup(ini_data, POSTAMBLE_GROUP_NAME);
	delete ini_data;
}

/**
 * And the main program (what else?)
 * @param argc Number of command-line arguments including the program name itself.
 * @param argv Vector of the command-line arguments.
 */
int CDECL main(int argc, char *argv[])
{
	const char *output_file = NULL;
	const char *before_file = NULL;
	const char *after_file = NULL;

	GetOptData mgo(argc - 1, argv + 1, _opts);
	for (;;) {
		int i = mgo.GetOpt();
		if (i == -1) break;

		switch (i) {
			case 'v':
				puts("$Revision$");
				return 0;

			case 'h':
				puts("settingsgen - $Revision$\n"
						"Usage: settingsgen [options] ini-file...\n"
						"with options:\n"
						"   -v, --version           Print version information and exit\n"
						"   -h, -?, --help          Print this help message and exit\n"
						"   -b FILE, --before FILE  Copy FILE before all settings\n"
						"   -a FILE, --after FILE   Copy FILE after all settings\n"
						"   -o FILE, --output FILE  Write output to FILE\n");
				return 0;

			case 'o':
				output_file = mgo.opt;
				break;

			case 'a':
				after_file = mgo.opt;
				break;

			case 'b':
				before_file = mgo.opt;
				break;

			case -2:
				fprintf(stderr, "Invalid arguments\n");
				return 1;
		}
	}

	_stored_output.Clear();

	for (int i = 0; i < mgo.numleft; i++) ProcessIniFile(mgo.argv[i]);

	/* Write output. */
	if (output_file == NULL) {
		CopyFile(before_file, stdout);
		_stored_output.Write(stdout);
		CopyFile(after_file, stdout);
	} else {
		static const char * const tmp_output = "tmp2.xxx";

		FILE *fp = fopen(tmp_output, "w");
		if (fp == NULL) {
			fprintf(stderr, "settingsgen: Warning: Cannot open file %s\n", tmp_output);
			return 1;
		}
		CopyFile(before_file, fp);
		_stored_output.Write(fp);
		CopyFile(after_file, fp);
		fclose(fp);

		if (CompareFiles(tmp_output, output_file)) {
			/* Files are equal. tmp2.xxx is not needed. */
			unlink(tmp_output);
		} else {
			/* Rename tmp2.xxx to output file. */
#if defined(WIN32) || defined(WIN64)
			unlink(output_file);
#endif
			if (rename(tmp_output, output_file) == -1) error("rename() failed");
		}
	}
	return 0;
}
