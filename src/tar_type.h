/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tar_type.h Structs, typedefs and macros used for TAR file handling. */

#ifndef TAR_TYPE_H
#define TAR_TYPE_H

#include <map>
#include <string>

#include "fileio_type.h"

/** The define of a TarList. */
struct TarListEntry {
	const char *filename;
	const char *dirname;

	/* MSVC goes copying around this struct after initialisation, so it tries
	 * to free filename, which isn't set at that moment... but because it
	 * initializes the variable with garbage, it's going to segfault. */
	TarListEntry() : filename(nullptr), dirname(nullptr) {}
	~TarListEntry() { free(this->filename); free(this->dirname); }
};

struct TarFileListEntry {
	const char *tar_filename;
	size_t size;
	size_t position;
};

typedef std::map<std::string, TarListEntry> TarList;
typedef std::map<std::string, TarFileListEntry> TarFileList;
extern TarList _tar_list[NUM_SUBDIRS];
extern TarFileList _tar_filelist[NUM_SUBDIRS];

#define FOR_ALL_TARS(tar, sd) for (tar = _tar_filelist[sd].begin(); tar != _tar_filelist[sd].end(); tar++)

#endif /* TAR_TYPE_H */
