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
#include "../core/mem_func.hpp"
#include "../error_func.h"

#if !defined(_WIN32) || defined(__CYGWIN__)
#include <unistd.h>
#include <sys/stat.h>
#endif

#include "../safeguards.h"

/**
 * Report a fatal error.
 * @param s Format string.
 * @note Function does not return.
 */
void NORETURN FatalErrorI(const std::string &msg)
{
	fmt::print(stderr, "settingsgen: FATAL: {}\n", msg);
	exit(1);
}

static const size_t OUTPUT_BLOCK_SIZE = 16000; ///< Block size of the buffer in #OutputBuffer.

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
	size_t Add(const char *text, size_t length)
	{
		size_t store_size = std::min(length, OUTPUT_BLOCK_SIZE - this->size);
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
		if (fwrite(this->data, 1, this->size, out_fp) != this->size) {
			FatalError("Cannot write output");
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

	size_t size;                  ///< Number of bytes stored in \a data.
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
		this->output_buffer.clear();
	}

	/**
	 * Add text to the output storage.
	 * @param text   Text to store.
	 * @param length Length of the text in bytes, \c 0 means 'length of the string'.
	 */
	void Add(const char *text, size_t length = 0)
	{
		if (length == 0) length = strlen(text);

		if (length > 0 && this->BufferHasRoom()) {
			size_t stored_size = this->output_buffer[this->output_buffer.size() - 1].Add(text, length);
			length -= stored_size;
			text += stored_size;
		}
		while (length > 0) {
			OutputBuffer &block = this->output_buffer.emplace_back();
			block.Clear(); // Initialize the new block.
			size_t stored_size = block.Add(text, length);
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
		for (const OutputBuffer &out_data : output_buffer) {
			out_data.Write(out_fp);
		}
	}

private:
	/**
	 * Does the buffer have room without adding a new #OutputBuffer block?
	 * @return \c true if room is available, else \c false.
	 */
	bool BufferHasRoom() const
	{
		size_t num_blocks = this->output_buffer.size();
		return num_blocks > 0 && this->output_buffer[num_blocks - 1].HasRoom();
	}

	typedef std::vector<OutputBuffer> OutputBufferVector; ///< Vector type for output buffers.
	OutputBufferVector output_buffer; ///< Vector of blocks containing the stored output.
};


/** Derived class for loading INI files without going through Fio stuff. */
struct SettingsIniFile : IniLoadFile {
	/**
	 * Construct a new ini loader.
	 * @param list_group_names A list with group names that should be loaded as lists instead of variables. @see IGT_LIST
	 * @param seq_group_names  A list with group names that should be loaded as lists of names. @see IGT_SEQUENCE
	 */
	SettingsIniFile(const IniGroupNameList &list_group_names = {}, const IniGroupNameList &seq_group_names = {}) :
			IniLoadFile(list_group_names, seq_group_names)
	{
	}

	FILE *OpenFile(const std::string &filename, Subdirectory, size_t *size) override
	{
		/* Open the text file in binary mode to prevent end-of-line translations
		 * done by ftell() and friends, as defined by K&R. */
		FILE *in = fopen(filename.c_str(), "rb");
		if (in == nullptr) return nullptr;

		fseek(in, 0L, SEEK_END);
		*size = ftell(in);

		fseek(in, 0L, SEEK_SET); // Seek back to the start of the file.
		return in;
	}

	void ReportFileError(const char * const pre, const char * const buffer, const char * const post) override
	{
		FatalError("{}{}{}", pre, buffer, post);
	}
};

OutputStore _stored_output; ///< Temporary storage of the output, until all processing is done.
OutputStore _post_amble_output; ///< Similar to _stored_output, but for the post amble.

static const char *PREAMBLE_GROUP_NAME  = "pre-amble"; ///< Name of the group containing the pre amble.
static const char *POSTAMBLE_GROUP_NAME = "post-amble"; ///< Name of the group containing the post amble.
static const char *TEMPLATES_GROUP_NAME = "templates"; ///< Name of the group containing the templates.
static const char *VALIDATION_GROUP_NAME = "validation"; ///< Name of the group containing the validation statements.
static const char *DEFAULTS_GROUP_NAME  = "defaults"; ///< Name of the group containing default values for the template variables.

/**
 * Dump a #IGT_SEQUENCE group into #_stored_output.
 * @param ifile      Loaded INI data.
 * @param group_name Name of the group to copy.
 */
static void DumpGroup(const IniLoadFile &ifile, const char * const group_name)
{
	const IniGroup *grp = ifile.GetGroup(group_name);
	if (grp != nullptr && grp->type == IGT_SEQUENCE) {
		for (const IniItem &item : grp->items) {
			if (!item.name.empty()) {
				_stored_output.Add(item.name.c_str());
				_stored_output.Add("\n", 1);
			}
		}
	}
}

/**
 * Find the value of a template variable.
 * @param name Name of the item to find.
 * @param grp  Group currently being expanded (searched first).
 * @param defaults Fallback group to search, \c nullptr skips the search.
 * @return Text of the item if found, else \c nullptr.
 */
static const char *FindItemValue(const char *name, const IniGroup *grp, const IniGroup *defaults)
{
	const IniItem *item = grp->GetItem(name);
	if (item == nullptr && defaults != nullptr) item = defaults->GetItem(name);
	if (item == nullptr || !item->value.has_value()) return nullptr;
	return item->value->c_str();
}

/**
 * Parse a single entry via a template and output this.
 * @param item The template to use for the output.
 * @param grp Group current being used for template rendering.
 * @param default_grp Default values for items not set in @grp.
 * @param output Output to use for result.
 */
static void DumpLine(const IniItem *item, const IniGroup *grp, const IniGroup *default_grp, OutputStore &output)
{
	static const int MAX_VAR_LENGTH = 64;

	/* Prefix with #if/#ifdef/#ifndef */
	static const auto pp_lines = {"if", "ifdef", "ifndef"};
	int count = 0;
	for (const auto &name : pp_lines) {
		const char *condition = FindItemValue(name, grp, default_grp);
		if (condition != nullptr) {
			output.Add("#", 1);
			output.Add(name);
			output.Add(" ", 1);
			output.Add(condition);
			output.Add("\n", 1);
			count++;
		}
	}

	/* Output text of the template, except template variables of the form '$[_a-z0-9]+' which get replaced by their value. */
	const char *txt = item->value->c_str();
	while (*txt != '\0') {
		if (*txt != '$') {
			output.Add(txt, 1);
			txt++;
			continue;
		}
		txt++;
		if (*txt == '$') { // Literal $
			output.Add(txt, 1);
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
			if (valitem != nullptr) output.Add(valitem);
		} else {
			output.Add("$", 1);
		}
	}
	output.Add("\n", 1); // \n after the expanded template.
	while (count > 0) {
		output.Add("#endif\n");
		count--;
	}
}

/**
 * Output all non-special sections through the template / template variable expansion system.
 * @param ifile Loaded INI data.
 */
static void DumpSections(const IniLoadFile &ifile)
{
	static const auto special_group_names = {PREAMBLE_GROUP_NAME, POSTAMBLE_GROUP_NAME, DEFAULTS_GROUP_NAME, TEMPLATES_GROUP_NAME, VALIDATION_GROUP_NAME};

	const IniGroup *default_grp = ifile.GetGroup(DEFAULTS_GROUP_NAME);
	const IniGroup *templates_grp = ifile.GetGroup(TEMPLATES_GROUP_NAME);
	const IniGroup *validation_grp = ifile.GetGroup(VALIDATION_GROUP_NAME);
	if (templates_grp == nullptr) return;

	/* Output every group, using its name as template name. */
	for (const IniGroup &grp : ifile.groups) {
		/* Exclude special group names. */
		if (std::find(std::begin(special_group_names), std::end(special_group_names), grp.name) != std::end(special_group_names)) continue;

		const IniItem *template_item = templates_grp->GetItem(grp.name); // Find template value.
		if (template_item == nullptr || !template_item->value.has_value()) {
			FatalError("Cannot find template {}", grp.name);
		}
		DumpLine(template_item, &grp, default_grp, _stored_output);

		if (validation_grp != nullptr) {
			const IniItem *validation_item = validation_grp->GetItem(grp.name); // Find template value.
			if (validation_item != nullptr && validation_item->value.has_value()) {
				DumpLine(validation_item, &grp, default_grp, _post_amble_output);
			}
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
	if (fname == nullptr) return;

	FILE *in_fp = fopen(fname, "r");
	if (in_fp == nullptr) {
		FatalError("Cannot open file {} for copying", fname);
	}

	char buffer[4096];
	size_t length;
	do {
		length = fread(buffer, 1, lengthof(buffer), in_fp);
		if (fwrite(buffer, 1, length, out_fp) != length) {
			FatalError("Cannot copy file");
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
	if (f2 == nullptr) return false;

	FILE *f1 = fopen(n1, "rb");
	if (f1 == nullptr) {
		fclose(f2);
		FatalError("can't open {}", n1);
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
	  GETOPT_NOVAL(     'h', "--help"),
	GETOPT_GENERAL('h', '?', nullptr, ODF_NO_VALUE),
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
	static const IniLoadFile::IniGroupNameList seq_groups = {PREAMBLE_GROUP_NAME, POSTAMBLE_GROUP_NAME};

	SettingsIniFile ini{{}, seq_groups};
	ini.LoadFromDisk(fname, NO_DIRECTORY);

	DumpGroup(ini, PREAMBLE_GROUP_NAME);
	DumpSections(ini);
	DumpGroup(ini, POSTAMBLE_GROUP_NAME);
}

/**
 * And the main program (what else?)
 * @param argc Number of command-line arguments including the program name itself.
 * @param argv Vector of the command-line arguments.
 */
int CDECL main(int argc, char *argv[])
{
	const char *output_file = nullptr;
	const char *before_file = nullptr;
	const char *after_file = nullptr;

	GetOptData mgo(argc - 1, argv + 1, _opts);
	for (;;) {
		int i = mgo.GetOpt();
		if (i == -1) break;

		switch (i) {
			case 'h':
				fmt::print("settingsgen\n"
						"Usage: settingsgen [options] ini-file...\n"
						"with options:\n"
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
				fmt::print(stderr, "Invalid arguments\n");
				return 1;
		}
	}

	_stored_output.Clear();
	_post_amble_output.Clear();

	for (int i = 0; i < mgo.numleft; i++) ProcessIniFile(mgo.argv[i]);

	/* Write output. */
	if (output_file == nullptr) {
		CopyFile(before_file, stdout);
		_stored_output.Write(stdout);
		_post_amble_output.Write(stdout);
		CopyFile(after_file, stdout);
	} else {
		static const char * const tmp_output = "tmp2.xxx";

		FILE *fp = fopen(tmp_output, "w");
		if (fp == nullptr) {
			FatalError("Cannot open file {}", tmp_output);
		}
		CopyFile(before_file, fp);
		_stored_output.Write(fp);
		_post_amble_output.Write(fp);
		CopyFile(after_file, fp);
		fclose(fp);

		if (CompareFiles(tmp_output, output_file)) {
			/* Files are equal. tmp2.xxx is not needed. */
			unlink(tmp_output);
		} else {
			/* Rename tmp2.xxx to output file. */
#if defined(_WIN32)
			unlink(output_file);
#endif
			if (rename(tmp_output, output_file) == -1) FatalError("rename() failed");
		}
	}
	return 0;
}
