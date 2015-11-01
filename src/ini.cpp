/* $Id$ */

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

#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500)
# include <unistd.h>
#endif

#ifdef WIN32
# include <windows.h>
# include <shellapi.h>
# include "core/mem_func.hpp"
#endif

#include "safeguards.h"

/**
 * Create a new ini file with given group names.
 * @param list_group_names A \c NULL terminated list with group names that should be loaded as lists instead of variables. @see IGT_LIST
 */
IniFile::IniFile(const char * const *list_group_names) : IniLoadFile(list_group_names)
{
}

/**
 * Save the Ini file's data to the disk.
 * @param filename the file to save to.
 * @return true if saving succeeded.
 */
bool IniFile::SaveToDisk(const char *filename)
{
	/*
	 * First write the configuration to a (temporary) file and then rename
	 * that file. This to prevent that when OpenTTD crashes during the save
	 * you end up with a truncated configuration file.
	 */
	char file_new[MAX_PATH];

	strecpy(file_new, filename, lastof(file_new));
	strecat(file_new, ".new", lastof(file_new));
	FILE *f = fopen(file_new, "w");
	if (f == NULL) return false;

	for (const IniGroup *group = this->group; group != NULL; group = group->next) {
		if (group->comment) fputs(group->comment, f);
		fprintf(f, "[%s]\n", group->name);
		for (const IniItem *item = group->item; item != NULL; item = item->next) {
			if (item->comment != NULL) fputs(item->comment, f);

			/* protect item->name with quotes if needed */
			if (strchr(item->name, ' ') != NULL ||
					item->name[0] == '[') {
				fprintf(f, "\"%s\"", item->name);
			} else {
				fprintf(f, "%s", item->name);
			}

			fprintf(f, " = %s\n", item->value == NULL ? "" : item->value);
		}
	}
	if (this->comment) fputs(this->comment, f);

/*
 * POSIX (and friends) do not guarantee that when a file is closed it is
 * flushed to the disk. So we manually flush it do disk if we have the
 * APIs to do so. We only need to flush the data as the metadata itself
 * (modification date etc.) is not important to us; only the real data is.
 */
#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
	int ret = fdatasync(fileno(f));
	fclose(f);
	if (ret != 0) return false;
#else
	fclose(f);
#endif

#if defined(WIN32) || defined(WIN64)
	/* _tcsncpy = strcpy is TCHAR is char, but isn't when TCHAR is wchar. */
	#undef strncpy
	/* Allocate space for one more \0 character. */
	TCHAR tfilename[MAX_PATH + 1], tfile_new[MAX_PATH + 1];
	_tcsncpy(tfilename, OTTD2FS(filename), MAX_PATH);
	_tcsncpy(tfile_new, OTTD2FS(file_new), MAX_PATH);
	/* SHFileOperation wants a double '\0' terminated string. */
	tfilename[MAX_PATH - 1] = '\0';
	tfile_new[MAX_PATH - 1] = '\0';
	tfilename[_tcslen(tfilename) + 1] = '\0';
	tfile_new[_tcslen(tfile_new) + 1] = '\0';

	/* Rename file without any user confirmation. */
	SHFILEOPSTRUCT shfopt;
	MemSetT(&shfopt, 0);
	shfopt.wFunc  = FO_MOVE;
	shfopt.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT;
	shfopt.pFrom  = tfile_new;
	shfopt.pTo    = tfilename;
	SHFileOperation(&shfopt);
#else
	if (rename(file_new, filename) < 0) {
		DEBUG(misc, 0, "Renaming %s to %s failed; configuration not saved", file_new, filename);
	}
#endif

	return true;
}

/* virtual */ FILE *IniFile::OpenFile(const char *filename, Subdirectory subdir, size_t *size)
{
	/* Open the text file in binary mode to prevent end-of-line translations
	 * done by ftell() and friends, as defined by K&R. */
	return FioFOpenFile(filename, "rb", subdir, size);
}

/* virtual */ void IniFile::ReportFileError(const char * const pre, const char * const buffer, const char * const post)
{
	ShowInfoF("%s%s%s", pre, buffer, post);
}
