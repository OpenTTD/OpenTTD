/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fios.h Declarations for savegames operations */

#ifndef FIOS_H
#define FIOS_H

#include "gfx_type.h"
#include "company_base.h"
#include "newgrf_config.h"


typedef SmallMap<uint, CompanyProperties *> CompanyPropertiesMap;

/**
 * Container for loading in mode SL_LOAD_CHECK.
 */
struct LoadCheckData {
	bool checkable;     ///< True if the savegame could be checked by SL_LOAD_CHECK. (Old savegames are not checkable.)
	StringID error;     ///< Error message from loading. INVALID_STRING_ID if no error.
	char *error_data;   ///< Data to pass to SetDParamStr when displaying #error.

	uint32 map_size_x, map_size_y;
	Date current_date;

	GameSettings settings;

	CompanyPropertiesMap companies;               ///< Company information.

	GRFConfig *grfconfig;                         ///< NewGrf configuration from save.
	GRFListCompatibility grf_compatibility;       ///< Summary state of NewGrfs, whether missing files or only compatible found.

	LoadCheckData() : error_data(NULL), grfconfig(NULL)
	{
		this->Clear();
	}

	/**
	 * Don't leak memory at program exit
	 */
	~LoadCheckData()
	{
		this->Clear();
	}

	/**
	 * Check whether loading the game resulted in errors.
	 * @return true if errors were encountered.
	 */
	bool HasErrors()
	{
		return this->checkable && this->error != INVALID_STRING_ID;
	}

	/**
	 * Check whether the game uses any NewGrfs.
	 * @return true if NewGrfs are used.
	 */
	bool HasNewGrfs()
	{
		return this->checkable && this->error == INVALID_STRING_ID && this->grfconfig != NULL;
	}

	void Clear();
};

extern LoadCheckData _load_check_data;


enum FileSlots {
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

enum SaveLoadDialogMode {
	SLD_LOAD_GAME,
	SLD_LOAD_SCENARIO,
	SLD_SAVE_GAME,
	SLD_SAVE_SCENARIO,
	SLD_LOAD_HEIGHTMAP,
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
	char name[MAX_PATH];
};

/* Deals with the type of the savegame, independent of extension */
struct SmallFiosItem {
	int mode;             ///< savegame/scenario type (old, new)
	FileType filetype;    ///< what type of file are we dealing with
	char name[MAX_PATH];  ///< name
	char title[255];      ///< internal name of the game
};

enum SortingBits {
	SORT_ASCENDING  = 0,
	SORT_DESCENDING = 1,
	SORT_BY_DATE    = 0,
	SORT_BY_NAME    = 2
};
DECLARE_ENUM_AS_BIT_SET(SortingBits)

/* Variables to display file lists */
extern SmallVector<FiosItem, 32> _fios_items;
extern SmallFiosItem _file_to_saveload;
extern SaveLoadDialogMode _saveload_mode;
extern SortingBits _savegame_sort_order;

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
const char *FiosBrowseTo(const FiosItem *item);
/* Return path, free space and stringID */
StringID FiosGetDescText(const char **path, uint64 *total_free);
/* Delete a name */
bool FiosDelete(const char *name);
/* Make a filename from a name */
void FiosMakeSavegameName(char *buf, const char *name, size_t size);
/* Determines type of savegame (or tells it is not a savegame) */
FiosType FiosGetSavegameListCallback(SaveLoadDialogMode mode, const char *file, const char *ext, char *title, const char *last);

int CDECL CompareFiosItems(const FiosItem *a, const FiosItem *b);

/* FIOS_TYPE_FILE, FIOS_TYPE_OLDFILE etc. different colours */
extern const TextColour _fios_colours[];

void BuildFileList();
void SetFiosType(const byte fiostype);

#endif /* FIOS_H */
