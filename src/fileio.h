/* $Id$ */

/** @file fileio.h Declarations for Standard In/Out file operations */

#ifndef FILEIO_H
#define FILEIO_H

#include <map>
#include <string>

void FioSeekTo(uint32 pos, int mode);
void FioSeekToFile(uint8 slot, uint32 pos);
uint32 FioGetPos();
const char *FioGetFilename();
byte FioReadByte();
uint16 FioReadWord();
uint32 FioReadDword();
void FioCloseAll();
void FioOpenFile(int slot, const char *filename);
void FioReadBlock(void *ptr, uint size);
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
 * The define of a TarList.
 */
struct TarListEntry {
	const char *filename;
};
struct TarFileListEntry {
	TarListEntry *tar;
	int size;
	int position;
};
typedef std::map<std::string, TarListEntry *> TarList;
typedef std::map<std::string, TarFileListEntry> TarFileList;
extern TarList _tar_list;
extern TarFileList _tar_filelist;

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
#define FOR_ALL_TARS(tar) for (tar = _tar_filelist.begin(); tar != _tar_filelist.end(); tar++)

typedef bool FioTarFileListCallback(const char *filename, int size, void *userdata);
FILE *FioTarFileList(const char *tar, const char *mode, size_t *filesize, FioTarFileListCallback *callback, void *userdata);

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

extern char *_personal_dir; ///< custom directory for personal settings, saves, newgrf, etc.

#endif /* FILEIO_H */
