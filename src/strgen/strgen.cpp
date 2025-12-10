/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file strgen.cpp Tool to create computer readable (stand-alone) translation files. */

#include "../stdafx.h"
#include "../error_func.h"
#include "../string_func.h"
#include "../strings_type.h"
#include "../misc/getoptdata.h"
#include "../table/control_codes.h"
#include "../3rdparty/fmt/std.h"

#include "strgen.h"

#include <filesystem>
#include <fstream>

#include "../table/strgen_tables.h"

#include "../safeguards.h"


#ifdef _MSC_VER
# define LINE_NUM_FMT(s) "{} ({}): warning: {} (" s ")\n"
#else
# define LINE_NUM_FMT(s) "{}:{}: " s ": {}\n"
#endif

void StrgenWarningI(const std::string &msg)
{
	if (_strgen.translation) {
		fmt::print(stderr, LINE_NUM_FMT("info"), _strgen.file, _strgen.cur_line, msg);
	} else {
		fmt::print(stderr, LINE_NUM_FMT("warning"), _strgen.file, _strgen.cur_line, msg);
	}
	_strgen.warnings++;
}

void StrgenErrorI(const std::string &msg)
{
	fmt::print(stderr, LINE_NUM_FMT("error"), _strgen.file, _strgen.cur_line, msg);
	_strgen.errors++;
}

[[noreturn]] void StrgenFatalI(const std::string &msg)
{
	fmt::print(stderr, LINE_NUM_FMT("FATAL"), _strgen.file, _strgen.cur_line, msg);
#ifdef _MSC_VER
	fmt::print(stderr, LINE_NUM_FMT("warning"), _strgen.file, _strgen.cur_line, "language is not compiled");
#endif
	throw std::exception();
}

[[noreturn]] void FatalErrorI(const std::string &msg)
{
	fmt::print(stderr, LINE_NUM_FMT("FATAL"), _strgen.file, _strgen.cur_line, msg);
#ifdef _MSC_VER
	fmt::print(stderr, LINE_NUM_FMT("warning"), _strgen.file, _strgen.cur_line, "language is not compiled");
#endif
	exit(2);
}

/** A reader that simply reads using fopen. */
struct FileStringReader : StringReader {
	std::ifstream input_stream;

	/**
	 * Create the reader.
	 * @param data        The data to fill during reading.
	 * @param file        The file we are reading.
	 * @param master      Are we reading the master file?
	 * @param translation Are we reading a translation?
	 */
	FileStringReader(StringData &data, const std::filesystem::path &file, bool master, bool translation) :
			StringReader(data, file.generic_string(), master, translation)
	{
		this->input_stream.open(file, std::ifstream::binary);
	}

	std::optional<std::string> ReadLine() override
	{
		std::string result;
		if (!std::getline(this->input_stream, result)) return std::nullopt;
		return result;
	}

	void HandlePragma(std::string_view str, LanguagePackHeader &lang) override;

	void ParseFile() override
	{
		this->StringReader::ParseFile();

		if (*_strgen.lang.name == '\0' || *_strgen.lang.own_name == '\0' || *_strgen.lang.isocode == '\0') {
			FatalError("Language must include ##name, ##ownname and ##isocode");
		}
	}
};

void FileStringReader::HandlePragma(std::string_view str, LanguagePackHeader &lang)
{
	StringConsumer consumer(str);
	auto name = consumer.ReadUntilChar(' ', StringConsumer::SKIP_ALL_SEPARATORS);
	if (name == "id") {
		this->data.next_string_id = consumer.ReadIntegerBase<uint32_t>(0);
	} else if (name == "name") {
		strecpy(lang.name, consumer.Read(StringConsumer::npos));
	} else if (name == "ownname") {
		strecpy(lang.own_name, consumer.Read(StringConsumer::npos));
	} else if (name == "isocode") {
		strecpy(lang.isocode, consumer.Read(StringConsumer::npos));
	} else if (name == "textdir") {
		auto dir = consumer.Read(StringConsumer::npos);
		if (dir == "ltr") {
			lang.text_dir = TD_LTR;
		} else if (dir == "rtl") {
			lang.text_dir = TD_RTL;
		} else {
			FatalError("Invalid textdir {}", dir);
		}
	} else if (name == "digitsep") {
		auto sep = consumer.Read(StringConsumer::npos);
		strecpy(lang.digit_group_separator, sep == "{NBSP}" ? NBSP : sep);
	} else if (name == "digitsepcur") {
		auto sep = consumer.Read(StringConsumer::npos);
		strecpy(lang.digit_group_separator_currency, sep == "{NBSP}" ? NBSP : sep);
	} else if (name == "decimalsep") {
		auto sep = consumer.Read(StringConsumer::npos);
		strecpy(lang.digit_decimal_separator, sep == "{NBSP}" ? NBSP : sep);
	} else if (name == "winlangid") {
		auto langid = consumer.ReadIntegerBase<int32_t>(0);
		if (langid > UINT16_MAX || langid < 0) {
			FatalError("Invalid winlangid {}", langid);
		}
		lang.winlangid = static_cast<uint16_t>(langid);
	} else if (name == "grflangid") {
		auto langid = consumer.ReadIntegerBase<int32_t>(0);
		if (langid >= 0x7F || langid < 0) {
			FatalError("Invalid grflangid {}", langid);
		}
		lang.newgrflangid = static_cast<uint8_t>(langid);
	} else if (name == "gender") {
		if (this->master) FatalError("Genders are not allowed in the base translation.");
		for (;;) {
			auto s = ParseWord(consumer);

			if (!s.has_value()) break;
			if (lang.num_genders >= MAX_NUM_GENDERS) FatalError("Too many genders, max {}", MAX_NUM_GENDERS);
			s->copy(lang.genders[lang.num_genders], CASE_GENDER_LEN - 1);
			lang.num_genders++;
		}
	} else if (name == "case") {
		if (this->master) FatalError("Cases are not allowed in the base translation.");
		for (;;) {
			auto s = ParseWord(consumer);

			if (!s.has_value()) break;
			if (lang.num_cases >= MAX_NUM_CASES) FatalError("Too many cases, max {}", MAX_NUM_CASES);
			s->copy(lang.cases[lang.num_cases], CASE_GENDER_LEN - 1);
			lang.num_cases++;
		}
	} else {
		StringReader::HandlePragma(str, lang);
	}
}

static bool CompareFiles(const std::filesystem::path &path1, const std::filesystem::path &path2)
{
	/* Check for equal size, but ignore the error code for cases when a file does not exist. */
	std::error_code error_code;
	if (std::filesystem::file_size(path1, error_code) != std::filesystem::file_size(path2, error_code)) return false;

	std::ifstream stream1(path1, std::ifstream::binary);
	std::ifstream stream2(path2, std::ifstream::binary);

	return std::equal(std::istreambuf_iterator<char>(stream1.rdbuf()),
			std::istreambuf_iterator<char>(),
			std::istreambuf_iterator<char>(stream2.rdbuf()));
}

/** Base class for writing data to disk. */
struct FileWriter {
	std::ofstream output_stream; ///< The stream to write all the output to.
	const std::filesystem::path path; ///< The file name we're writing to.

	/**
	 * Open a file to write to.
	 * @param path The path to the file to open.
	 * @param openmode The openmode flags for opening the file.
	 */
	FileWriter(const std::filesystem::path &path, std::ios_base::openmode openmode) : path(path)
	{
		this->output_stream.open(path, openmode);
	}

	/** Finalise the writing. */
	void Finalise()
	{
		this->output_stream.close();
	}

	/** Make sure the file is closed. */
	virtual ~FileWriter()
	{
		/* If we weren't closed an exception was thrown, so remove the temporary file. */
		if (this->output_stream.is_open()) {
			this->output_stream.close();
			std::filesystem::remove(this->path);
		}
	}
};

struct HeaderFileWriter : HeaderWriter, FileWriter {
	/** The real path we eventually want to write to. */
	const std::filesystem::path real_path;
	/** The previous string ID that was printed. */
	size_t prev;
	size_t total_strings;

	/**
	 * Open a file to write to.
	 * @param path The path to the file to open.
	 */
	HeaderFileWriter(const std::filesystem::path &path) : FileWriter("tmp.xxx", std::ofstream::out),
		real_path(path), prev(0), total_strings(0)
	{
		this->output_stream << "/* This file is automatically generated. Do not modify */\n\n";
		this->output_stream << "#ifndef TABLE_STRINGS_H\n";
		this->output_stream << "#define TABLE_STRINGS_H\n";
	}

	void WriteStringID(const std::string &name, size_t stringid) override
	{
		if (prev + 1 != stringid) this->output_stream << "\n";
		fmt::print(this->output_stream, "static const StringID {} = 0x{:X};\n", name, stringid);
		prev = stringid;
		total_strings++;
	}

	void Finalise(const StringData &data) override
	{
		/* Find the plural form with the most amount of cases. */
		size_t max_plural_forms = 0;
		for (const auto &pf : _plural_forms) {
			max_plural_forms = std::max(max_plural_forms, pf.plural_count);
		}

		fmt::print(this->output_stream,
			"\n"
			"static const uint LANGUAGE_PACK_VERSION     = 0x{:X};\n"
			"static const uint LANGUAGE_MAX_PLURAL       = {};\n"
			"static const uint LANGUAGE_MAX_PLURAL_FORMS = {};\n"
			"static const uint LANGUAGE_TOTAL_STRINGS    = {};\n"
			"\n",
			data.Version(), std::size(_plural_forms), max_plural_forms, total_strings
		);

		this->output_stream << "#endif /* TABLE_STRINGS_H */\n";

		this->FileWriter::Finalise();

		std::error_code error_code;
		if (CompareFiles(this->path, this->real_path)) {
			/* files are equal. tmp.xxx is not needed */
			std::filesystem::remove(this->path, error_code); // Just ignore the error
		} else {
			/* else rename tmp.xxx into filename */
			std::filesystem::rename(this->path, this->real_path, error_code);
			if (error_code) FatalError("rename({}, {}) failed: {}", this->path, this->real_path, error_code.message());
		}
	}
};

/** Class for writing a language to disk. */
struct LanguageFileWriter : LanguageWriter, FileWriter {
	/**
	 * Open a file to write to.
	 * @param path The path to the file to open.
	 */
	LanguageFileWriter(const std::filesystem::path &path) : FileWriter(path, std::ofstream::binary | std::ofstream::out)
	{
	}

	void WriteHeader(const LanguagePackHeader *header) override
	{
		this->Write({reinterpret_cast<const char *>(header), sizeof(*header)});
	}

	void Finalise() override
	{
		this->output_stream.put(0);
		this->FileWriter::Finalise();
	}

	void Write(std::string_view buffer) override
	{
		this->output_stream.write(buffer.data(), buffer.size());
	}
};

/** Options of strgen. */
static const OptionData _opts[] = {
	{ .type = ODF_NO_VALUE, .id = 'C', .longname = "-export-commands" },
	{ .type = ODF_NO_VALUE, .id = 'L', .longname = "-export-plurals" },
	{ .type = ODF_NO_VALUE, .id = 'P', .longname = "-export-pragmas" },
	{ .type = ODF_NO_VALUE, .id = 't', .shortname = 't', .longname = "--todo" },
	{ .type = ODF_NO_VALUE, .id = 'w', .shortname = 'w', .longname = "--warning" },
	{ .type = ODF_NO_VALUE, .id = 'h', .shortname = 'h', .longname = "--help" },
	{ .type = ODF_NO_VALUE, .id = 'h', .shortname = '?' },
	{ .type = ODF_HAS_VALUE, .id = 's', .shortname = 's', .longname = "--source_dir" },
	{ .type = ODF_HAS_VALUE, .id = 'd', .shortname = 'd', .longname = "--dest_dir" },
};

int CDECL main(int argc, char *argv[])
{
	std::filesystem::path src_dir(".");
	std::filesystem::path dest_dir;

	std::vector<std::string_view> params;
	for (int i = 1; i < argc; ++i) params.emplace_back(argv[i]);
	GetOptData mgo(params, _opts);
	for (;;) {
		int i = mgo.GetOpt();
		if (i == -1) break;

		switch (i) {
			case 'C':
				fmt::print("args\tflags\tcommand\treplacement\n");
				for (const auto &cs : _cmd_structs) {
					char flags;
					if (cs.proc == EmitGender) {
						flags = 'g'; // Command needs number of parameters defined by number of genders
					} else if (cs.proc == EmitPlural) {
						flags = 'p'; // Command needs number of parameters defined by plural value
					} else if (cs.flags.Test(CmdFlag::DontCount)) {
						flags = 'i'; // Command may be in the translation when it is not in base
					} else {
						flags = '0'; // Command needs no parameters
					}
					fmt::print("{}\t{:c}\t\"{}\"\t\"{}\"\n", cs.consumes, flags, cs.cmd, cs.cmd.find("STRING") != std::string::npos ? "STRING" : cs.cmd);
				}
				return 0;

			case 'L':
				fmt::print("count\tdescription\tnames\n");
				for (const auto &pf : _plural_forms) {
					fmt::print("{}\t\"{}\"\t{}\n", pf.plural_count, pf.description, pf.names);
				}
				return 0;

			case 'P':
				fmt::print("name\tflags\tdefault\tdescription\n");
				for (const auto &pragma : _pragmas) {
					fmt::print("\"{}\"\t{}\t\"{}\"\t\"{}\"\n",
							pragma[0], pragma[1], pragma[2], pragma[3]);
				}
				return 0;

			case 't':
				_strgen.annotate_todos = true;
				break;

			case 'w':
				_strgen.show_warnings = true;
				break;

			case 'h':
				fmt::print(
					"strgen\n"
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
					" file using english.txt as a reference and output <language>.lng.\n"
				);
				return 0;

			case 's':
				src_dir = mgo.opt;
				break;

			case 'd':
				dest_dir = mgo.opt;
				break;

			case -2:
				fmt::print(stderr, "Invalid arguments\n");
				return 0;
		}
	}

	if (dest_dir.empty()) dest_dir = src_dir; // if dest_dir is not specified, it equals src_dir

	try {
		/* strgen has two modes of operation. If no (free) arguments are passed
		 * strgen generates strings.h to the destination directory. If it is supplied
		 * with a (free) parameter the program will translate that language to destination
		 * directory. As input english.txt is parsed from the source directory */
		if (mgo.arguments.empty()) {
			std::filesystem::path input_path = std::move(src_dir);
			input_path /= "english.txt";

			/* parse master file */
			StringData data(TEXT_TAB_END);
			FileStringReader master_reader(data, input_path, true, false);
			master_reader.ParseFile();
			if (_strgen.errors != 0) return 1;

			/* write strings.h */
			std::filesystem::path output_path = dest_dir;
			std::filesystem::create_directories(dest_dir);
			output_path /= "strings.h";

			HeaderFileWriter writer(output_path);
			writer.WriteHeader(data);
			writer.Finalise(data);
			if (_strgen.errors != 0) return 1;
		} else {
			std::filesystem::path input_path = std::move(src_dir);
			input_path /= "english.txt";

			StringData data(TEXT_TAB_END);
			/* parse master file and check if target file is correct */
			FileStringReader master_reader(data, input_path, true, false);
			master_reader.ParseFile();

			for (auto &argument: mgo.arguments) {
				data.FreeTranslation();

				std::filesystem::path lang_file = argument;
				FileStringReader translation_reader(data, lang_file, false, lang_file.filename() != "english.txt");
				translation_reader.ParseFile(); // target file
				if (_strgen.errors != 0) return 1;

				/* get the targetfile, strip any directories and append to destination path */
				std::filesystem::path output_file = dest_dir;
				output_file /= lang_file.filename();
				output_file.replace_extension("lng");

				LanguageFileWriter writer(output_file);
				writer.WriteLang(data);
				writer.Finalise();

				/* if showing warnings, print a summary of the language */
				if (_strgen.show_warnings) {
					fmt::print("{} warnings and {} errors for {}\n", _strgen.warnings, _strgen.errors, output_file);
				}
			}
		}
	} catch (...) {
		return 2;
	}

	return 0;
}
