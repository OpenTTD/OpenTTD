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
#include "network/core/tcp_content.h"


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

	struct LoggedAction *gamelog_action;          ///< Gamelog actions
	uint gamelog_actions;                         ///< Number of gamelog actions

	LoadCheckData() : error_data(NULL), grfconfig(NULL),
			grf_compatibility(GLC_NOT_FOUND), gamelog_action(NULL), gamelog_actions(0)
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
	 * Slot used for the GRF scanning and such.
	 * This slot is used for all temporary accesses to files when scanning/testing files,
	 * and thus cannot be used for files, which are continuously accessed during a game.
	 */
	CONFIG_SLOT    =  0,
	/** Slot for the sound. */
	SOUND_SLOT     =  1,
	/** First slot usable for (New)GRFs used during the game. */
	FIRST_GRF_SLOT =  2,
	/** Maximum number of slots. */
	MAX_FILE_SLOTS = 128,
};

/** Deals with finding savegames */
struct FiosItem {
	FiosType type;
	uint64 mtime;
	char title[64];
	char name[MAX_PATH];
};

/** List of file information. */
class FileList {
public:
	~FileList();

	/**
	 * Construct a new entry in the file list.
	 * @return Pointer to the new items to be initialized.
	 */
	inline FiosItem *Append()
	{
		return this->files.Append();
	}

	/**
	 * Get the number of files in the list.
	 * @return The number of files stored in the list.
	 */
	inline uint Length() const
	{
		return this->files.Length();
	}

	/**
	 * Get a pointer to the first file information.
	 * @return Address of the first file information.
	 */
	inline const FiosItem *Begin() const
	{
		return this->files.Begin();
	}

	/**
	 * Get a pointer behind the last file information.
	 * @return Address behind the last file information.
	 */
	inline const FiosItem *End() const
	{
		return this->files.End();
	}

	/**
	 * Get a pointer to the indicated file information. File information must exist.
	 * @return Address of the indicated existing file information.
	 */
	inline const FiosItem *Get(uint index) const
	{
		return this->files.Get(index);
	}

	/**
	 * Get a pointer to the indicated file information. File information must exist.
	 * @return Address of the indicated existing file information.
	 */
	inline FiosItem *Get(uint index)
	{
		return this->files.Get(index);
	}

	inline const FiosItem &operator[](uint index) const
	{
		return this->files[index];
	}

	/**
	 * Get a reference to the indicated file information. File information must exist.
	 * @return The requested file information.
	 */
	inline FiosItem &operator[](uint index)
	{
		return this->files[index];
	}

	/** Remove all items from the list. */
	inline void Clear()
	{
		this->files.Clear();
	}

	/** Compact the list down to the smallest block size boundary. */
	inline void Compact()
	{
		this->files.Compact();
	}

	void BuildFileList(AbstractFileType abstract_filetype, SaveLoadOperation fop);
	const FiosItem *FindItem(const char *file);

	SmallVector<FiosItem, 32> files; ///< The list of files.
};

enum SortingBits {
	SORT_ASCENDING  = 0,
	SORT_DESCENDING = 1,
	SORT_BY_DATE    = 0,
	SORT_BY_NAME    = 2
};
DECLARE_ENUM_AS_BIT_SET(SortingBits)

/* Variables to display file lists */
extern SortingBits _savegame_sort_order;

void ShowSaveLoadDialog(AbstractFileType abstract_filetype, SaveLoadOperation fop);

void FiosGetSavegameList(SaveLoadOperation fop, FileList &file_list);
void FiosGetScenarioList(SaveLoadOperation fop, FileList &file_list);
void FiosGetHeightmapList(SaveLoadOperation fop, FileList &file_list);

const char *FiosBrowseTo(const FiosItem *item);

StringID FiosGetDescText(const char **path, uint64 *total_free);
bool FiosDelete(const char *name);
void FiosMakeHeightmapName(char *buf, const char *name, const char *last);
void FiosMakeSavegameName(char *buf, const char *name, const char *last);

FiosType FiosGetSavegameListCallback(SaveLoadOperation fop, const char *file, const char *ext, char *title, const char *last);

int CDECL CompareFiosItems(const FiosItem *a, const FiosItem *b);

#endif /* FIOS_H */
