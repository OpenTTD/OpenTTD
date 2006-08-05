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
