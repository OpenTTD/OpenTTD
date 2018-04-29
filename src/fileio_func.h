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

#include "core/enum_type.hpp"
#include "fileio_type.h"

void FioSeekTo(size_t pos, int mode);
void FioSeekToFile(uint8 slot, size_t pos);
size_t FioGetPos();
const char *FioGetFilename(uint8 slot);
byte FioReadByte();
uint16 FioReadWord();
uint32 FioReadDword();
void FioCloseAll();
void FioOpenFile(int slot, const char *filename, Subdirectory subdir);
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
FILE *FioFOpenFile(const char *filename, const char *mode, Subdirectory subdir, size_t *filesize = NULL);
bool FioCheckFileExists(const char *filename, Subdirectory subdir);
char *FioGetFullPath(char *buf, const char *last, Searchpath sp, Subdirectory subdir, const char *filename);
char *FioFindFullPath(char *buf, const char *last, Subdirectory subdir, const char *filename);
char *FioAppendDirectory(char *buf, const char *last, Searchpath sp, Subdirectory subdir);
char *FioGetDirectory(char *buf, const char *last, Subdirectory subdir);
void FioCreateDirectory(const char *name);

const char *FiosGetScreenshotDir();

void SanitizeFilename(char *filename);
bool AppendPathSeparator(char *buf, const char *last);
void DeterminePaths(const char *exe);
void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize);
bool FileExists(const char *filename);
const char *FioTarFirstDir(const char *tarname, Subdirectory subdir);
void FioTarAddLink(const char *src, const char *dest, Subdirectory subdir);
bool ExtractTar(const char *tar_filename, Subdirectory subdir);

extern const char *_personal_dir; ///< custom directory for personal settings, saves, newgrf, etc.

/** Helper for scanning for files with a given name */
class FileScanner {
protected:
	Subdirectory subdir; ///< The current sub directory we are searching through
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
	 * @param tar_filename    the name of the tar file the file is read from.
	 * @return true if the file is added.
	 */
	virtual bool AddFile(const char *filename, size_t basepath_length, const char *tar_filename) = 0;
};

/** Helper for scanning for files with tar as extension */
class TarScanner : FileScanner {
	uint DoScan(Subdirectory sd);
public:
	/** The mode of tar scanning. */
	enum Mode {
		NONE     = 0,      ///< Scan nothing.
		BASESET  = 1 << 0, ///< Scan for base sets.
		NEWGRF   = 1 << 1, ///< Scan for non-base sets.
		AI       = 1 << 2, ///< Scan for AIs and its libraries.
		SCENARIO = 1 << 3, ///< Scan for scenarios and heightmaps.
		GAME     = 1 << 4, ///< Scan for game scripts.
		ALL      = BASESET | NEWGRF | AI | SCENARIO | GAME, ///< Scan for everything.
	};

	/* virtual */ bool AddFile(const char *filename, size_t basepath_length, const char *tar_filename = NULL);

	bool AddFile(Subdirectory sd, const char *filename);

	/** Do the scan for Tars. */
	static uint DoScan(TarScanner::Mode mode);
};

DECLARE_ENUM_AS_BIT_SET(TarScanner::Mode)

/* Implementation of opendir/readdir/closedir for Windows */
#if defined(WIN32)
struct DIR;

struct dirent { // XXX - only d_name implemented
	TCHAR *d_name; // name of found file
	/* little hack which will point to parent DIR struct which will
	 * save us a call to GetFileAttributes if we want information
	 * about the file (for example in function fio_bla) */
	DIR *dir;
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


/** Auto-close a file upon scope exit. */
class FileCloser {
	FILE *f;

public:
	FileCloser(FILE *_f) : f(_f) {}
	~FileCloser()
	{
		fclose(f);
	}
};

#endif /* FILEIO_FUNC_H */
