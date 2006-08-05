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
#include "table/strings.h"
#include "hal.h"
#include "fios.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
# include <io.h>
#else
# include <unistd.h>
# include <dirent.h>
#endif /* WIN32 */

char *_fios_path;
FiosItem *_fios_items;
int _fios_count, _fios_alloc;

/* OS-specific functions are taken from their respective files (win32/unix/os2 .c) */
extern bool FiosIsRoot(const char *path);
extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);
extern void FiosGetDrives(void);

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

void FiosMakeSavegameName(char *buf, const char *name, size_t size)
{
	const char *extension, *period;

	extension = (_game_mode == GM_EDITOR) ? ".scn" : ".sav";

	/* Don't append the extension if it is already there */
	period = strrchr(name, '.');
	if (period != NULL && strcasecmp(period, extension) == 0) extension = "";

	snprintf(buf, size, "%s" PATHSEP "%s%s", _fios_path, name, extension);
}

bool FiosDelete(const char *name)
{
	char filename[512];

	FiosMakeSavegameName(filename, name, lengthof(filename));
	return unlink(filename) == 0;
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
static FiosItem *FiosGetFileList(int *num, int mode, fios_getlist_callback_proc *callback_proc)
{
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;
	FiosItem *fios;
	int sort_start;

	/* A parent directory link exists if we are not in the root directory */
	if (!FiosIsRoot(_fios_path) && mode != SLD_NEW_GAME) {
		fios = FiosAlloc();
		fios->type = FIOS_TYPE_PARENT;
		fios->mtime = 0;
		ttd_strlcpy(fios->name, "..", lengthof(fios->name));
		ttd_strlcpy(fios->title, ".. (Parent directory)", lengthof(fios->title));
	}

	/* Show subdirectories */
	if (mode != SLD_NEW_GAME && (dir = opendir(_fios_path)) != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			/* found file must be directory, but not '.' or '..' */
			if (FiosIsValidFile(_fios_path, dirent, &sb) && (sb.st_mode & S_IFDIR) &&
				strcmp(dirent->d_name, ".") != 0 && strcmp(dirent->d_name, "..") != 0) {
				fios = FiosAlloc();
				fios->type = FIOS_TYPE_DIR;
				fios->mtime = 0;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));
				snprintf(fios->title, lengthof(fios->title), "%s" PATHSEP " (Directory)", FS2OTTD(dirent->d_name));
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
	dir = opendir(_fios_path);
	if (dir != NULL) {
		while ((dirent = readdir(dir)) != NULL) {
			char fios_title[64];
			char *t;
			byte type;

			if (!FiosIsValidFile(_fios_path, dirent, &sb) || !(sb.st_mode & S_IFREG)) continue;

			/* File has no extension, skip it */
			if ((t = strrchr(dirent->d_name, '.')) == NULL) continue;
			fios_title[0] = '\0'; // reset the title;

			type = callback_proc(mode, dirent->d_name, t, fios_title);
			if (type != FIOS_TYPE_INVALID) {
				fios = FiosAlloc();
				fios->mtime = sb.st_mtime;
				fios->type = type;
				ttd_strlcpy(fios->name, dirent->d_name, lengthof(fios->name));

				/* Some callbacks want to lookup the title of the file. Allow that.
				 * If we just copy the title from the filename, strip the extension */
				t = (fios_title[0] == '\0') ? *t = '\0', dirent->d_name : fios_title;
				ttd_strlcpy(fios->title, FS2OTTD(t), lengthof(fios->title));
				str_validate(fios->title);
			}
		}
		closedir(dir);
	}

	qsort(_fios_items + sort_start, _fios_count - sort_start, sizeof(FiosItem), compare_FiosItems);

	/* Show drives */
	if (mode != SLD_NEW_GAME) FiosGetDrives();

	*num = _fios_count;
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
FiosItem *FiosGetSavegameList(int *num, int mode)
{
	static char *_fios_save_path = NULL;

	if (_fios_save_path == NULL) {
		_fios_save_path = malloc(MAX_PATH);
		ttd_strlcpy(_fios_save_path, _path.save_dir, MAX_PATH);
	}

	_fios_path = _fios_save_path;

	return FiosGetFileList(num, mode, &FiosGetSavegameListCallback);
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
FiosItem *FiosGetScenarioList(int *num, int mode)
{
	static char *_fios_scn_path = NULL;

	if (_fios_scn_path == NULL) {
		_fios_scn_path = malloc(MAX_PATH);
		ttd_strlcpy(_fios_scn_path, _path.scenario_dir, MAX_PATH);
	}

	_fios_path = _fios_scn_path;

	return FiosGetFileList(num, mode, &FiosGetScenarioListCallback);
}
