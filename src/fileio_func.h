/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fileio_func.h Functions for Standard In/Out file operations */

#ifndef FILEIO_FUNC_H
#define FILEIO_FUNC_H

#include "fileio_type.h"

void FioSeekTo(size_t pos, int mode);
void FioSeekToFile(uint8 slot, size_t pos);
size_t FioGetPos();
const char *FioGetFilename(uint8 slot);
byte FioReadByte();
uint16 FioReadWord();
uint32 FioReadDword();
void FioCloseAll();
void FioOpenFile(int slot, const char *filename);
void FioReadBlock(void *ptr, size_t size);
void FioSkipBytes(int n);

/**
 * The search paths OpenTTD could search through.
 * At least one of the slots has to be filled with a path.
 * NULL paths tell that there is no such path for the
 * current operating system.
 */
extern const char *_searchpaths[NUM_SEARCHPATHS];

/**
 * Checks whether the given search path is a valid search path
 * @param sp the search path to check
 * @return true if the search path is valid
 */
static inline bool IsValidSearchPath(Searchpath sp)
{
	return sp < NUM_SEARCHPATHS && _searchpaths[sp] != NULL;
}

/** Iterator for all the search paths */
#define FOR_ALL_SEARCHPATHS(sp) for (sp = SP_FIRST_DIR; sp < NUM_SEARCHPATHS; sp++) if (IsValidSearchPath(sp))

void FioFCloseFile(FILE *f);
FILE *FioFOpenFile(const char *filename, const char *mode = "rb", Subdirectory subdir = DATA_DIR, size_t *filesize = NULL);
bool FioCheckFileExists(const char *filename, Subdirectory subdir = DATA_DIR);
char *FioGetFullPath(char *buf, size_t buflen, Searchpath sp, Subdirectory subdir, const char *filename);
char *FioFindFullPath(char *buf, size_t buflen, Subdirectory subdir, const char *filename);
char *FioAppendDirectory(char *buf, size_t buflen, Searchpath sp, Subdirectory subdir);
char *FioGetDirectory(char *buf, size_t buflen, Subdirectory subdir);

void SanitizeFilename(char *filename);
bool AppendPathSeparator(char *buf, size_t buflen);
void DeterminePaths(const char *exe);
void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize);
bool FileExists(const char *filename);
const char *FioTarFirstDir(const char *tarname);
void FioTarAddLink(const char *src, const char *dest);
bool ExtractTar(const char *tar_filename);

extern char *_personal_dir; ///< custom directory for personal settings, saves, newgrf, etc.

/** Helper for scanning for files with a given name */
class FileScanner
{
public:
	/** Destruct the proper one... */
	virtual ~FileScanner() {}

	uint Scan(const char *extension, Subdirectory sd, bool tars = true, bool recursive = true);
	uint Scan(const char *extension, const char *directory, bool recursive = true);

	/**
	 * Add a file with the given filename.
	 * @param filename        the full path to the file to read
	 * @param basepath_length amount of characters to chop of before to get a
	 *                        filename relative to the search path.
	 * @return true if the file is added.
	 */
	virtual bool AddFile(const char *filename, size_t basepath_length) = 0;
};

/** Helper for scanning for files with tar as extension */
class TarScanner : FileScanner {
public:
	/* virtual */ bool AddFile(const char *filename, size_t basepath_length);

	/** Do the scan for Tars. */
	static uint DoScan();
};

/* Implementation of opendir/readdir/closedir for Windows */
#if defined(WIN32)
#include <windows.h>
struct DIR;

struct dirent { // XXX - only d_name implemented
	TCHAR *d_name; // name of found file
	/* little hack which will point to parent DIR struct which will
	 * save us a call to GetFileAttributes if we want information
	 * about the file (for example in function fio_bla) */
	DIR *dir;
};

struct DIR {
	HANDLE hFind;
	/* the dirent returned by readdir.
	 * note: having only one global instance is not possible because
	 * multiple independent opendir/readdir sequences must be supported. */
	dirent ent;
	WIN32_FIND_DATA fd;
	/* since opendir calls FindFirstFile, we need a means of telling the
	 * first call to readdir that we already have a file.
	 * that's the case iff this is true */
	bool at_first_entry;
};

DIR *opendir(const TCHAR *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#else
/* Use system-supplied opendir/readdir/closedir functions */
# include <sys/types.h>
# include <dirent.h>
#endif /* defined(WIN32) */

/**
 * A wrapper around opendir() which will convert the string from
 * OPENTTD encoding to that of the filesystem. For all purposes this
 * function behaves the same as the original opendir function
 * @param path string to open directory of
 * @return DIR pointer
 */
static inline DIR *ttd_opendir(const char *path)
{
	return opendir(OTTD2FS(path));
}

#endif /* FILEIO_FUNC_H */
