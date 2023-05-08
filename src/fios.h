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
#include "gamelog.h"
#include "network/core/tcp_content_type.h"
#include "timer/timer_game_calendar.h"


/** Special values for save-load window for the data parameter of #InvalidateWindowData. */
enum SaveLoadInvalidateWindowData {
	SLIWD_RESCAN_FILES,          ///< Rescan all files (when changed directory, ...)
	SLIWD_SELECTION_CHANGES,     ///< File selection has changed (user click, ...)
	SLIWD_FILTER_CHANGES,        ///< The filename filter has changed (via the editbox)
};

using CompanyPropertiesMap = std::map<uint, std::unique_ptr<CompanyProperties>>;

/**
 * Container for loading in mode SL_LOAD_CHECK.
 */
struct LoadCheckData {
	bool checkable;     ///< True if the savegame could be checked by SL_LOAD_CHECK. (Old savegames are not checkable.)
	StringID error;     ///< Error message from loading. INVALID_STRING_ID if no error.
	std::string error_msg; ///< Data to pass to SetDParamStr when displaying #error.

	uint32_t map_size_x, map_size_y;
	TimerGameCalendar::Date current_date;

	GameSettings settings;

	CompanyPropertiesMap companies;               ///< Company information.

	GRFConfig *grfconfig;                         ///< NewGrf configuration from save.
	GRFListCompatibility grf_compatibility;       ///< Summary state of NewGrfs, whether missing files or only compatible found.

	Gamelog gamelog; ///< Gamelog actions

	LoadCheckData() : grfconfig(nullptr),
			grf_compatibility(GLC_NOT_FOUND)
	{
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
		return this->checkable && this->error == INVALID_STRING_ID && this->grfconfig != nullptr;
	}

	void Clear();
};

extern LoadCheckData _load_check_data;

/** Deals with finding savegames */
struct FiosItem {
	FiosType type;
	uint64_t mtime;
	std::string title;
	std::string name;
	bool operator< (const FiosItem &other) const;
};

/** List of file information. */
class FileList : public std::vector<FiosItem> {
public:
	void BuildFileList(AbstractFileType abstract_filetype, SaveLoadOperation fop);
	const FiosItem *FindItem(const std::string_view file);
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

bool FiosBrowseTo(const FiosItem *item);

std::string FiosGetCurrentPath();
std::optional<uint64_t> FiosGetDiskFreeSpace(const std::string &path);
bool FiosDelete(const char *name);
std::string FiosMakeHeightmapName(const char *name);
std::string FiosMakeSavegameName(const char *name);

std::tuple<FiosType, std::string> FiosGetSavegameListCallback(SaveLoadOperation fop, const std::string &file, const std::string_view ext);

void ScanScenarios();
const char *FindScenario(const ContentInfo *ci, bool md5sum);

/**
 * A savegame name automatically numbered.
 */
struct FiosNumberedSaveName {
	FiosNumberedSaveName(const std::string &prefix);
	std::string Filename();
	std::string Extension();
private:
	std::string prefix;
	int number;
};

#endif /* FIOS_H */
