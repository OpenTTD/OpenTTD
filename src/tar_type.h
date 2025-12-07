/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

using TarList = std::map<std::string, std::string, std::less<>>; ///< Map of tar file to tar directory.
using TarFileList = std::map<std::string, TarFileListEntry, std::less<>> ;
extern std::array<TarList, NUM_SUBDIRS> _tar_list;
extern TarFileList _tar_filelist[NUM_SUBDIRS];

#endif /* TAR_TYPE_H */
