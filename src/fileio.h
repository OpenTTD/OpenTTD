/* $Id$ */

/** @file fileio.h Declarations for Standard In/Out file operations */

#ifndef FILEIO_H
#define FILEIO_H

#include "core/enum_type.hpp"

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
void FioCreateDirectory(const char *filename);

/**
 * The different kinds of subdirectories OpenTTD uses
 */
enum Subdirectory {
	BASE_DIR,      ///< Base directory for all subdirectories
	SAVE_DIR,      ///< Base directory for all savegames
	AUTOSAVE_DIR,  ///< Subdirectory of save for autosaves
	SCENARIO_DIR,  ///< Base directory for all scenarios
	HEIGHTMAP_DIR, ///< Subdirectory of scenario for heightmaps
	GM_DIR,        ///< Subdirectory for all music
	DATA_DIR,      ///< Subdirectory for all data (GRFs, sample.cat, intro game)
	LANG_DIR,      ///< Subdirectory for all translation files
	NUM_SUBDIRS,   ///< Number of subdirectories
	NO_DIRECTORY,  ///< A path without any base directory
};

/**
 * Types of searchpaths OpenTTD might use
 */
enum Searchpath {
	SP_FIRST_DIR,
	SP_WORKING_DIR = SP_FIRST_DIR, ///< Search in the working directory
	SP_PERSONAL_DIR,               ///< Search in the personal directory
	SP_SHARED_DIR,                 ///< Search in the shared directory, like 'Shared Files' under Windows
	SP_BINARY_DIR,                 ///< Search in the directory where the binary resides
	SP_INSTALLATION_DIR,           ///< Search in the installation directory
	SP_APPLICATION_BUNDLE_DIR,     ///< Search within the application bundle
	NUM_SEARCHPATHS
};

DECLARE_POSTFIX_INCREMENT(Searchpath);

/**
 * The searchpaths OpenTTD could search through.
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

static inline const char *FioGetSubdirectory(Subdirectory subdir)
{
	extern const char *_subdirs[NUM_SUBDIRS];
	assert(subdir < NUM_SUBDIRS);
	return _subdirs[subdir];
}

void SanitizeFilename(char *filename);
void AppendPathSeparator(char *buf, size_t buflen);
void DeterminePaths(const char *exe);
void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize);
bool FileExists(const char *filename);

extern char *_personal_dir; ///< custom directory for personal settings, saves, newgrf, etc.

/** Helper for scanning for files with a given name */
class FileScanner
{
public:
	uint Scan(const char *extension, Subdirectory sd, bool tars = true);

	/**
	 * Add a file with the given filename.
	 * @param filename        the full path to the file to read
	 * @param basepath_length amount of characters to chop of before to get a
	 *                        filename relative to the search path.
	 * @return true if the file is added.
	 */
	virtual bool AddFile(const char *filename, size_t basepath_length) = 0;
};

#endif /* FILEIO_H */
