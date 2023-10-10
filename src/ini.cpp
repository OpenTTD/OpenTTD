/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ini.cpp Definition of the IniItem class, related to reading/writing '*.ini' files. */

#include "stdafx.h"
#include "debug.h"
#include "ini_type.h"
#include "string_func.h"
#include "fileio_func.h"
#include <fstream>
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#endif

#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500)
# include <unistd.h>
# include <fcntl.h>
#endif

#ifdef _WIN32
# include <windows.h>
# include <shellapi.h>
# include "core/mem_func.hpp"
#endif

#include "safeguards.h"

/**
 * Create a new ini file with given group names.
 * @param list_group_names A list with group names that should be loaded as lists instead of variables. @see IGT_LIST
 */
IniFile::IniFile(const IniGroupNameList &list_group_names) : IniLoadFile(list_group_names)
{
}

/**
 * Save the Ini file's data to the disk.
 * @param filename the file to save to.
 * @return true if saving succeeded.
 */
bool IniFile::SaveToDisk(const std::string &filename)
{
	/*
	 * First write the configuration to a (temporary) file and then rename
	 * that file. This to prevent that when OpenTTD crashes during the save
	 * you end up with a truncated configuration file.
	 */
	std::string file_new{ filename };
	file_new.append(".new");

	std::ofstream os(OTTD2FS(file_new).c_str());
	if (os.fail()) return false;

	for (const IniGroup &group : this->groups) {
		os << group.comment << "[" << group.name << "]\n";
		for (const IniItem &item : group.items) {
			os << item.comment;

			/* protect item->name with quotes if needed */
			if (item.name.find(' ') != std::string::npos ||
				item.name[0] == '[') {
				os << "\"" << item.name << "\"";
			} else {
				os << item.name;
			}

			os << " = " << item.value.value_or("") << "\n";
		}
	}
	os << this->comment;

	os.flush();
	os.close();
	if (os.fail()) return false;

/*
 * POSIX (and friends) do not guarantee that when a file is closed it is
 * flushed to the disk. So we manually flush it do disk if we have the
 * APIs to do so. We only need to flush the data as the metadata itself
 * (modification date etc.) is not important to us; only the real data is.
 */
#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
	int f = open(file_new.c_str(), O_RDWR);
	int ret = fdatasync(f);
	close(f);
	if (ret != 0) return false;
#endif

#if defined(_WIN32)
	/* Allocate space for one more \0 character. */
	wchar_t tfilename[MAX_PATH + 1], tfile_new[MAX_PATH + 1];
	wcsncpy(tfilename, OTTD2FS(filename).c_str(), MAX_PATH);
	wcsncpy(tfile_new, OTTD2FS(file_new).c_str(), MAX_PATH);
	/* SHFileOperation wants a double '\0' terminated string. */
	tfilename[MAX_PATH - 1] = '\0';
	tfile_new[MAX_PATH - 1] = '\0';
	tfilename[wcslen(tfilename) + 1] = '\0';
	tfile_new[wcslen(tfile_new) + 1] = '\0';

	/* Rename file without any user confirmation. */
	SHFILEOPSTRUCT shfopt;
	MemSetT(&shfopt, 0);
	shfopt.wFunc  = FO_MOVE;
	shfopt.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT;
	shfopt.pFrom  = tfile_new;
	shfopt.pTo    = tfilename;
	SHFileOperation(&shfopt);
#else
	if (rename(file_new.c_str(), filename.c_str()) < 0) {
		Debug(misc, 0, "Renaming {} to {} failed; configuration not saved", file_new, filename);
	}
#endif

#ifdef __EMSCRIPTEN__
	EM_ASM(if (window["openttd_syncfs"]) openttd_syncfs());
#endif

	return true;
}

/* virtual */ FILE *IniFile::OpenFile(const std::string &filename, Subdirectory subdir, size_t *size)
{
	/* Open the text file in binary mode to prevent end-of-line translations
	 * done by ftell() and friends, as defined by K&R. */
	return FioFOpenFile(filename, "rb", subdir, size);
}

/* virtual */ void IniFile::ReportFileError(const char * const pre, const char * const buffer, const char * const post)
{
	ShowInfo("{}{}{}", pre, buffer, post);
}
