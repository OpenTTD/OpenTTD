/* $Id$ */

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


/** The header of an entry (regular file, link, etc) in a tar file. */
struct TarHeader {
	char name[100];      ///< Name of the file.
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];       ///< Size of the file, in ASCII.
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];    ///< Path of the file.

	char unused[12];
};

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

/**
 * Class for writing a tar file.
 * Work cycle:
 * - #StartWriteTar starts writing a tar file. It creates the file, and resets block counts.
 * - #StartWriteFile starts writing a file in the tar.
 * - #WriteFileData writes data into the tarfile belonging to the last opened file in the tar.
 * - #StopWriteFile ends writing the file in the tar. It updates the header of the file.
 * To write more files, do a #StartWriteFile again.
 * - # StopWriteTar ends writing of the tar file.
 */
class WriteTar {
public:
	WriteTar();
	~WriteTar();

	bool StartWriteTar(const char *tar_fname, const char *dir_name, const char *dir_entry);
	void StopWriteTar();

	void StartWriteFile(const char *fname);
	void WriteFileData(const char *data, size_t length);
	void StopWriteFile();

private:
	/** State of the tar writer, used for sane closedown of the writing process. */
	enum WriterState {
		TWS_TAR_CLOSED,  ///< No tar file opened.
		TWS_TAR_OPENED,  ///< Tar file opened, but not writing a file.
		TWS_FILE_OPENED, ///< Writing a file.
	};

	char *dir_name;     ///< Name of the directory where all the files are placed in.
	char *dir_entry;    ///< Directory entry in which the files will be placed.
	size_t tar_block;   ///< Block number after the last completely written file.
	TarHeader header;   ///< Header block of the opened file.
	FILE *fp;           ///< File handle of the tar file.
	size_t file_size;   ///< Length of the file data written so far.
	uint32 start_block; ///< Block-number containing the header of the current file.
	WriterState state;  ///< State of the tar writer object.

	void WriteZeroes(size_t length);

	void MakeOctalValue(char *start, uint32 value, uint length);
};

#endif /* TAR_TYPE_H */
