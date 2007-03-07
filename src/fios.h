/* $Id$ */

/** @file fios.h Declarations for savegames operations */

#ifndef FIOS_H
#define FIOS_H

/* Deals with finding savegames */
typedef struct {
	byte type;
	uint64 mtime;
	char title[64];
	char name[256 - 12 - 64];
} FiosItem;

enum {
	FIOS_TYPE_DRIVE        =   0,
	FIOS_TYPE_PARENT       =   1,
	FIOS_TYPE_DIR          =   2,
	FIOS_TYPE_FILE         =   3,
	FIOS_TYPE_OLDFILE      =   4,
	FIOS_TYPE_SCENARIO     =   5,
	FIOS_TYPE_OLD_SCENARIO =   6,
	FIOS_TYPE_DIRECT       =   7,
	FIOS_TYPE_PNG          =   8,
	FIOS_TYPE_BMP          =   9,
	FIOS_TYPE_INVALID      = 255,
};

/* Variables to display file lists */
extern FiosItem *_fios_list; ///< defined in misc_gui.cpp
extern int _fios_num;        ///< defined in fios.cpp, read_only version of _fios_count
extern int _saveload_mode;   ///< defined in misc_gui.cpp

/* Get a list of savegames */
FiosItem *FiosGetSavegameList(int mode);
/* Get a list of scenarios */
FiosItem *FiosGetScenarioList(int mode);
/* Get a list of Heightmaps */
FiosItem *FiosGetHeightmapList(int mode);
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
/* Allocate a new FiosItem */
FiosItem *FiosAlloc();

int CDECL compare_FiosItems(const void *a, const void *b);

/* Implementation of opendir/readdir/closedir for Windows */
#if defined(WIN32)
#include <windows.h>
typedef struct DIR DIR;

typedef struct dirent { // XXX - only d_name implemented
	wchar_t *d_name; // name of found file
	/* little hack which will point to parent DIR struct which will
	 * save us a call to GetFileAttributes if we want information
	 * about the file (for example in function fio_bla) */
	DIR *dir;
} dirent;

struct DIR {
	HANDLE hFind;
	/* the dirent returned by readdir.
	 * note: having only one global instance is not possible because
	 * multiple independent opendir/readdir sequences must be supported. */
	dirent ent;
	WIN32_FIND_DATAW fd;
	/* since opendir calls FindFirstFile, we need a means of telling the
	 * first call to readdir that we already have a file.
	 * that's the case iff this is true */
	bool at_first_entry;
};

DIR *opendir(const wchar_t *path);
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
