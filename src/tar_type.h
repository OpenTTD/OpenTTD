/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tar_type.h Structs, typedefs and macros used for TAR file handling. */

#ifndef TAR_TYPE_H
#define TAR_TYPE_H

#include "fileio_type.h"


struct TarFileListEntry {
	std::string tar_filename;
	size_t size;
	size_t position;
};

typedef std::map<std::string, std::string> TarList; ///< Map of tar file to tar directory.
typedef std::map<std::string, TarFileListEntry> TarFileList;
extern std::array<TarList, NUM_SUBDIRS> _tar_list;
extern TarFileList _tar_filelist[NUM_SUBDIRS];

#endif /* TAR_TYPE_H */
