/* $Id$ */

/** @file fios.h Declarations for savegames operations */

#ifndef FIOS_H
#define FIOS_H

#include "strings_type.h"
#include "core/smallvec_type.hpp"

enum {
	/**
	 * Slot used for the GRF scanning and such. This slot cannot be reused
	 * as it will otherwise cause issues when pressing "rescan directories".
	 * It can furthermore not be larger than LAST_GRF_SLOT as that complicates
	 * the testing for "too much NewGRFs".
	 */
	CONFIG_SLOT    =  0,
	/** Slot for the sound. */
	SOUND_SLOT     =  1,
	/** First slot useable for (New)GRFs used during the game. */
	FIRST_GRF_SLOT =  2,
	/** Last slot useable for (New)GRFs used during the game. */
	LAST_GRF_SLOT  = 63,
	/** Maximum number of slots. */
	MAX_FILE_SLOTS = 64
};

enum SaveLoadDialogMode{
	SLD_LOAD_GAME,
	SLD_LOAD_SCENARIO,
	SLD_SAVE_GAME,
	SLD_SAVE_SCENARIO,
	SLD_LOAD_HEIGHTMAP,
	SLD_NEW_GAME,
};

/* The different types of files been handled by the system */
enum FileType {
	FT_NONE,      ///< nothing to do
	FT_SAVEGAME,  ///< old or new savegame
	FT_SCENARIO,  ///< old or new scenario
	FT_HEIGHTMAP, ///< heightmap file
};

enum FiosType {
	FIOS_TYPE_DRIVE,
	FIOS_TYPE_PARENT,
	FIOS_TYPE_DIR,
	FIOS_TYPE_FILE,
	FIOS_TYPE_OLDFILE,
	FIOS_TYPE_SCENARIO,
	FIOS_TYPE_OLD_SCENARIO,
	FIOS_TYPE_DIRECT,
	FIOS_TYPE_PNG,
	FIOS_TYPE_BMP,
	FIOS_TYPE_INVALID = 255,
};

/* Deals with finding savegames */
struct FiosItem {
	FiosType type;
	uint64 mtime;
	char title[64];
	char name[256 - 12 - 64];
};

/* Deals with the type of the savegame, independent of extension */
struct SmallFiosItem {
	int mode;             ///< savegame/scenario type (old, new)
	FileType filetype;    ///< what type of file are we dealing with
	char name[MAX_PATH];  ///< name
	char title[255];      ///< internal name of the game
};

enum {
	SORT_ASCENDING  = 0,
	SORT_DESCENDING = 1,
	SORT_BY_DATE    = 0,
	SORT_BY_NAME    = 2
};

/* Variables to display file lists */
extern SmallVector<FiosItem, 32> _fios_items; ///< defined in fios.cpp
extern SmallFiosItem _file_to_saveload;
extern SaveLoadDialogMode _saveload_mode;   ///< defined in misc_gui.cpp
extern byte _savegame_sort_order;

/* Launch save/load dialog */
void ShowSaveLoadDialog(SaveLoadDialogMode mode);

/* Get a list of savegames */
void FiosGetSavegameList(SaveLoadDialogMode mode);
/* Get a list of scenarios */
void FiosGetScenarioList(SaveLoadDialogMode mode);
/* Get a list of Heightmaps */
void FiosGetHeightmapList(SaveLoadDialogMode mode);
/* Free the list of savegames */
void FiosFreeSavegameList();
/* Browse to. Returns a filename w/path if we reached a file. */
char *FiosBrowseTo(const FiosItem *item);
/* Return path, free space and stringID */
StringID FiosGetDescText(const char **path, uint32 *total_free);
/* Delete a name */
bool FiosDelete(const char *name);
/* Make a filename from a name */
void FiosMakeSavegameName(char *buf, const char *name, size_t size);

int CDECL compare_FiosItems(const void *a, const void *b);

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

#endif /* FIOS_H */
