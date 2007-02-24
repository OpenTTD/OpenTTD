/* $Id$ */

/** @file fios.c
 * This file contains functions for building file lists for the save/load dialogs.
 */

#include "stdafx.h"
#include "openttd.h"
#include "hal.h"
#include "string.h"
#include "variables.h"
#include "functions.h"
#include "heightmap.h"
#include "table/strings.h"
#include "fios.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
# include <io.h>
#else
# include <unistd.h>
#endif /* WIN32 */

/* Variables to display file lists */
int _fios_num;

static char *_fios_path;
static FiosItem *_fios_items;
static int _fios_count, _fios_alloc;

/* OS-specific functions are taken from their respective files (win32/unix/os2 .c) */
extern bool FiosIsRoot(const char *path);
extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);
extern void FiosGetDrives(void);
extern bool FiosGetDiskFreeSpace(const char *path, uint32 *tot);

/* get the name of an oldstyle savegame */
extern void GetOldSaveGameName(char *title, const char *path, const char *file);

/**
 * Allocate a new FiosItem.
 * @return A pointer to the newly allocated FiosItem.
 */
FiosItem *FiosAlloc(void)
{
	if (_fios_count == _fios_alloc) {
		_fios_alloc += 256;
		_fios_items = realloc(_fios_items, _fios_alloc * sizeof(FiosItem));
	}
	return &_fios_items[_fios_count++];
}

/**
 * Compare two FiosItem's. Used with qsort when sorting the file list.
 * @param a A pointer to the first FiosItem to compare.
 * @param a A pointer to the second FiosItem to compare.
 * @return -1, 0 or 1, depending on how the two items should be sorted.
 */
int CDECL compare_FiosItems(const void *a, const void *b)
{
	const FiosItem *da = (const FiosItem *)a;
	const FiosItem *db = (const FiosItem *)b;
	int r;

	if (_savegame_sort_order & SORT_BY_NAME) {
		r = strcasecmp(da->title, db->title);
	} else {
		r = da->mtime < db->mtime ? -1 : 1;
	}

	if (_savegame_sort_order & SORT_DESCENDING) r = -r;
	return r;
}

/**
 * Free the list of savegames
 */
void FiosFreeSavegameList(void)
{
	free(_fios_items);
	_fios_items = NULL;
	_fios_alloc = _fios_count = 0;
}

/**
 * Get descriptive texts. Returns the path and free space
 * left on the device
 * @param path string describing the path
 * @param total_free total free space in megabytes, optional (can be NULL)
 * @return StringID describing the path (free space or failure)
 */
StringID FiosGetDescText(const char **path, uint32 *total_free)
{
	*path = _fios_path;
	return FiosGetDiskFreeSpace(*path, total_free) ? STR_4005_BYTES_FREE : STR_4006_UNABLE_TO_READ_DRIVE;
}

/* Browse to a new path based on the passed FiosItem struct
 * @param *item FiosItem object telling us what to do
 * @return a string if we have given a file as a target, otherwise NULL */
char *FiosBrowseTo(const FiosItem *item)
{
	char *s;
	char *path = _fios_path;

	switch (item->type) {
#if defined(WIN32) || defined(__OS2__)
	case FIOS_TYPE_DRIVE: sprintf(path, "%c:" PATHSEP, item->title[0]); break;
#endif

	case FIOS_TYPE_PARENT:
		/* Check for possible NULL ptr (not required for UNIXes, but AmigaOS-alikes) */
		if ((s = strrchr(path, PATHSEPCHAR)) != NULL) {
			s[1] = '\0'; // go up a directory
			if (!FiosIsRoot(path)) s[0] = '\0';
		}
#if defined(__MORPHOS__) || defined(__AMIGAOS__)
		/* On MorphOS or AmigaOS paths look like: "Volume:directory/subdirectory" */
		else if ((s = strrchr(path, ':')) != NULL) s[1] = '\0';
#endif
		break;

	case FIOS_TYPE_DIR:
		if (!FiosIsRoot(path)) strcat(path, PATHSEP);
		strcat(path, item->name);
		break;

	case FIOS_TYPE_DIRECT:
		sprintf(path, "%s" PATHSEP, item->name);
		s = strrchr(path, PATHSEPCHAR);
		if (s != NULL && s[1] == '\0') s[0] = '\0'; // strip trailing slash
		break;

	case FIOS_TYPE_FILE:
	case FIOS_TYPE_OLDFILE:
	case FIOS_TYPE_SCENARIO:
	case FIOS_TYPE_OLD_SCENARIO:
	case FIOS_TYPE_PNG:
	case FIOS_TYPE_BMP:
	{
		static char str_buffr[512];

#if defined(__MORPHOS__) || defined(__AMIGAOS__)
		/* On MorphOS or AmigaOS paths look like: "Volume:directory/subdirectory" */
		if (FiosIsRoot(path)) {
			snprintf(str_buffr, lengthof(str_buffr), "%s:%s", path, item->name);
		} else // XXX - only next line!
#endif
		snprintf(str_buffr, lengthof(str_buffr), "%s" PATHSEP "%s", path, item->name);

		return str_buffr;
	}
	}

	return NULL;
}

void FiosMakeSavegameName(char *buf, const char *name, size_t size)
{
	const char *extension, *period;

	extension = (_game_mode == GM_EDITOR) ? ".scn" : ".sav";

	/* Don't append the extension if it is already there */
	period = strrchr(name, '.');
	if (period != NULL && strcasecmp(period, extension) == 0) extension = "";

	snprintf(buf, size, "%s" PATHSEP "%s%s", _fios_path, name, extension);
}

#if defined(WIN32) || defined(WIN64)
# define unlink _wunlink
#endif

bool FiosDelete(const char *name)
{
	char filename[512];

	FiosMakeSavegameName(filename, name, lengthof(filename));
	return unlink(OTTD2FS(filename)) == 0;
}

bool FileExists(const char *filename)
{
	return access(filename, 0) == 0;
}

typedef byte fios_getlist_callback_proc(int mode, const char *filename, const char *ext, char *title);

/** Create a list of the files in a directory, according to some arbitrary rule.
 *  @param num Will be filled with the amount of items.
 *  @param mode The mode we are in. Some modes don't allow 'parent'.
 *  @param callback The function that is called where you need to do the filtering.
 *  @return Return the list of files. */
static FiosItem *FiosGetFileList(int mode, fios_getlist_callback_proc *callback_proc)
{
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;
	FiosItem *fios;
	int sort_start;
	char d_name[sizeof(fios->name)];

	/* A parent directory link exists if we are not in the root directory */
	if (!FiosIsRoot(_fios_path) && mode != SLD_NEW_GAME) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		ttd_strlcpy(fios->name, "..", lengthof(fios->name));
		ttd_strlcpy(fios->title, ".. (Parent directory)", lengthof(fios->title));
	}

	/* Show subdirectories */
	if (mode != SLD_NEW_GAME && (dir = ttd_opendir(_fios_path)) != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			ttd_strlcpy(d_name, FS2OTTD(dirent->d_name), sizeof(d_name));

			/* found file must be directory, but not '.' or '..' */
			if (FiosIsValidFile(_fios_path, dirent, &sb) && (sb.st_mode & S_IFDIR) &&
				strcmp(d_name, ".") != 0 && strcmp(d_name, "..") != 0) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, d_name, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s" PATHSEP " (Directory)", d_name);
				str_validate(fios->title);
			}
		}
		closedir(dir);
	}

	/* Sort the subdirs always by name, ascending, remember user-sorting order */
	{
		byte order = _savegame_sort_order;
		_savegame_sort_order = SORT_BY_NAME | SORT_ASCENDING;
		qsort(_fios_items, _fios_count, sizeof(FiosItem), compare_FiosItems);
		_savegame_sort_order = order;
	}

	/* This is where to start sorting for the filenames */
	sort_start = _fios_count;

	/* Show files */
	dir = ttd_opendir(_fios_path);
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			char fios_title[64];
			char *t;
			byte type;
			ttd_strlcpy(d_name, FS2OTTD(dirent->d_name), sizeof(d_name));

			if (!FiosIsValidFile(_fios_path, dirent, &sb) || !(sb.st_mode & S_IFREG)) continue;

			/* File has no extension, skip it */
			if ((t = strrchr(d_name, '.')) == NULL) continue;
			fios_title[0] = '\0'; // reset the title;

			type = callback_proc(mode, d_name, t, fios_title);
			if (type != FIOS_TYPE_INVALID) {
				fios = FiosAlloc();
				fios->mtime = sb.st_mtime;
				fios->type = type;
				ttd_strlcpy(fios->name, d_name, lengthof(fios->name));

				/* Some callbacks want to lookup the title of the file. Allow that.
				 * If we just copy the title from the filename, strip the extension */
				t = (fios_title[0] == '\0') ? *t = '\0', d_name : fios_title;
				ttd_strlcpy(fios->title, t, lengthof(fios->title));
				str_validate(fios->title);
			}
		}
		closedir(dir);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	/* Show drives */
	if (mode != SLD_NEW_GAME) FiosGetDrives();

	_fios_num = _fios_count;
	return _fios_items;
}

/**
 * Callback for FiosGetFileList. It tells if a file is a savegame or not.
 * @param mode Save/load mode.
 * @param file Name of the file to check.
 * @param ext A pointer to the extension identifier inside file
 * @param title Buffer if a callback wants to lookup the title of the file
 * @return a FIOS_TYPE_* type of the found file, FIOS_TYPE_INVALID if not a savegame
 * @see FiosGetFileList
 * @see FiosGetSavegameList
 */
static byte FiosGetSavegameListCallback(int mode, const char *file, const char *ext, char *title)
{
	/* Show savegame files
	 * .SAV OpenTTD saved game
	 * .SS1 Transport Tycoon Deluxe preset game
	 * .SV1 Transport Tycoon Deluxe (Patch) saved game
	 * .SV2 Transport Tycoon Deluxe (Patch) saved 2-player game */
	if (strcasecmp(ext, ".sav") == 0) return FIOS_TYPE_FILE;

	if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO) {
		if (strcasecmp(ext, ".ss1") == 0 || strcasecmp(ext, ".sv1") == 0 ||
				strcasecmp(ext, ".sv2") == 0) {
			GetOldSaveGameName(title, _fios_path, file);
			return FIOS_TYPE_OLDFILE;
		}
	}

	return FIOS_TYPE_INVALID;
}

/**
 * Get a list of savegames.
 * @param mode Save/load mode.
 * @return A pointer to an array of FiosItem representing all the files to be shown in the save/load dialog.
 * @see FiosGetFileList
 */
FiosItem *FiosGetSavegameList(int mode)
{
	static char *_fios_save_path = NULL;

	if (_fios_save_path == NULL) {
		_fios_save_path = malloc(MAX_PATH);
		ttd_strlcpy(_fios_save_path, _paths.save_dir, MAX_PATH);
	}

	_fios_path = _fios_save_path;

	return FiosGetFileList(mode, &FiosGetSavegameListCallback);
}

/**
 * Callback for FiosGetFileList. It tells if a file is a scenario or not.
 * @param mode Save/load mode.
 * @param file Name of the file to check.
 * @param ext A pointer to the extension identifier inside file
 * @param title Buffer if a callback wants to lookup the title of the file
 * @return a FIOS_TYPE_* type of the found file, FIOS_TYPE_INVALID if not a scenario
 * @see FiosGetFileList
 * @see FiosGetScenarioList
 */
static byte FiosGetScenarioListCallback(int mode, const char *file, const char *ext, char *title)
{
	/* Show scenario files
	 * .SCN OpenTTD style scenario file
	 * .SV0 Transport Tycoon Deluxe (Patch) scenario
	 * .SS0 Transport Tycoon Deluxe preset scenario */
	if (strcasecmp(ext, ".scn") == 0) return FIOS_TYPE_SCENARIO;

	if (mode == SLD_LOAD_GAME || mode == SLD_LOAD_SCENARIO || mode == SLD_NEW_GAME) {
		if (strcasecmp(ext, ".sv0") == 0 || strcasecmp(ext, ".ss0") == 0 ) {
			GetOldSaveGameName(title, _fios_path, file);
			return FIOS_TYPE_OLD_SCENARIO;
		}
	}

	return FIOS_TYPE_INVALID;
}

/**
 * Get a list of scenarios.
 * @param mode Save/load mode.
 * @return A pointer to an array of FiosItem representing all the files to be shown in the save/load dialog.
 * @see FiosGetFileList
 */
FiosItem *FiosGetScenarioList(int mode)
{
	static char *_fios_scn_path = NULL;

	if (_fios_scn_path == NULL) {
		_fios_scn_path = malloc(MAX_PATH);
		ttd_strlcpy(_fios_scn_path, _paths.scenario_dir, MAX_PATH);
	}

	_fios_path = _fios_scn_path;

	return FiosGetFileList(mode, &FiosGetScenarioListCallback);
}

static byte FiosGetHeightmapListCallback(int mode, const char *file, const char *ext, char *title)
{
	/* Show heightmap files
	 * .PNG PNG Based heightmap files
	 * .BMP BMP Based heightmap files
	 */

#ifdef WITH_PNG
	if (strcasecmp(ext, ".png") == 0) return FIOS_TYPE_PNG;
#endif /* WITH_PNG */

	if (strcasecmp(ext, ".bmp") == 0) return FIOS_TYPE_BMP;

	return FIOS_TYPE_INVALID;
}

// Get a list of Heightmaps
FiosItem *FiosGetHeightmapList(int mode)
{
	static char *_fios_hmap_path = NULL;

	if (_fios_hmap_path == NULL) {
		_fios_hmap_path = malloc(MAX_PATH);
		strcpy(_fios_hmap_path, _paths.heightmap_dir);
	}

	_fios_path = _fios_hmap_path;

	return FiosGetFileList(mode, &FiosGetHeightmapListCallback);
}
