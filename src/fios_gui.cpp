/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fios_gui.cpp GUIs for loading/saving games, scenarios, heightmaps, ... */

#include "stdafx.h"
#include "saveload/saveload.h"
#include "error.h"
#include "gui.h"
#include "gfx_func.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_content.h"
#include "strings_func.h"
#include "fileio_func.h"
#include "fios.h"
#include "window_func.h"
#include "tilehighlight_func.h"
#include "querystring_gui.h"
#include "engine_func.h"
#include "landscape_type.h"
#include "genworld.h"
#include "timer/timer_game_calendar.h"
#include "core/geometry_func.hpp"
#include "gamelog.h"
#include "stringfilter_type.h"
#include "misc_cmd.h"
#include "gamelog_internal.h"

#include "widgets/fios_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

LoadCheckData _load_check_data;    ///< Data loaded from save during SL_LOAD_CHECK.

static bool _fios_path_changed;
static bool _savegame_sort_dirty;

/** The available sorters for FiosItems. */
enum class SavegameSorter : uint8_t {
	Date, ///< Sort by date.
	Name, ///< Sort by name.
};

static SavegameSorter _savegame_sorter = SavegameSorter::Date; ///< Sorter for savegames.
static bool _savegame_sorter_ascending = false; ///< Sorter for savegames.

/** Sorts the FiosItems based on the savegame sorter and order. @copydoc GUIList::Sorter */
bool FiosItemSorter(const FiosItem &a, const FiosItem &b)
{
	switch (_savegame_sorter) {
		case SavegameSorter::Date:
			return _savegame_sorter_ascending ? FiosItemModificationDateSorter(a, b) : FiosItemModificationDateSorter(b, a);
		case SavegameSorter::Name:
			return _savegame_sorter_ascending ? FiosItemNameSorter(a, b) : FiosItemNameSorter(b, a);
		default:
			NOT_REACHED();
	}
}

/**
 * Reset read data.
 */
void LoadCheckData::Clear()
{
	this->checkable = false;
	this->error = INVALID_STRING_ID;
	this->error_msg.clear();

	this->map_size_x = this->map_size_y = 256; // Default for old savegames which do not store mapsize.
	this->current_date = CalendarTime::MIN_DATE;
	this->landscape = {};
	this->starting_year = {};

	companies.clear();

	this->gamelog.Reset();

	ClearGRFConfigList(this->grfconfig);
}

/** Load game/scenario with optional content download */
static constexpr std::initializer_list<NWidgetPart> _nested_load_dialog_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, Colours::Grey),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, Colours::Grey, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),

	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		/* Left side : filter box and available files */
		NWidget(NWID_VERTICAL),
			/* Filter box with label */
			NWidget(WWT_PANEL, Colours::Grey), SetFill(1, 1), SetResize(1, 1),
				NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
						SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
					NWidget(WWT_TEXT, Colours::Invalid), SetFill(0, 1), SetStringTip(STR_SAVELOAD_FILTER_TITLE),
					NWidget(WWT_EDITBOX, Colours::Grey, WID_SL_FILTER), SetFill(1, 0), SetResize(1, 0),
							SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
			EndContainer(),
			/* Sort buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYNAME), SetStringTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYDATE), SetStringTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, Colours::Grey, WID_SL_HOME_BUTTON), SetAspect(1), SetSpriteTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON_TOOLTIP),
			EndContainer(),
			/* Files */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, Colours::Grey, WID_SL_FILE_BACKGROUND),
					NWidget(WWT_INSET, Colours::Grey, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 2, 2, 2),
							SetToolTip(STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_VSCROLLBAR, Colours::Grey, WID_SL_SCROLLBAR),
			EndContainer(),
			/* Online Content button */
			NWidget(NWID_SELECTION, Colours::Invalid, WID_SL_CONTENT_DOWNLOAD_SEL),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_CONTENT_DOWNLOAD), SetResize(1, 0),
						SetStringTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
			EndContainer(),
		EndContainer(),

		/* Right side : game details */
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, Colours::Grey, WID_SL_DETAILS), SetResize(1, 1), SetFill(1, 1),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_MISSING_NEWGRFS), SetStringTip(STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON, STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_NEWGRF_INFO), SetStringTip(STR_MAPGEN_NEWGRF_SETTINGS, STR_MAPGEN_NEWGRF_SETTINGS_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_LOAD_BUTTON), SetStringTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_RESIZEBOX, Colours::Grey),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Load heightmap with content download */
static constexpr std::initializer_list<NWidgetPart> _nested_load_heightmap_dialog_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, Colours::Grey),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, Colours::Grey, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),

	/* Filter box with label */
	NWidget(WWT_PANEL, Colours::Grey), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
				SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
			NWidget(WWT_TEXT, Colours::Invalid), SetFill(0, 1), SetStringTip(STR_SAVELOAD_FILTER_TITLE),
			NWidget(WWT_EDITBOX, Colours::Grey, WID_SL_FILTER), SetFill(1, 0), SetResize(1, 0),
					SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
	EndContainer(),
	/* Sort Buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
			NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYNAME), SetStringTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYDATE), SetStringTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, Colours::Grey, WID_SL_HOME_BUTTON), SetAspect(1), SetSpriteTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON_TOOLTIP),
	EndContainer(),
	/* Files */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Grey, WID_SL_FILE_BACKGROUND),
			NWidget(WWT_INSET, Colours::Grey, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 2, 2, 2),
					SetToolTip(STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Grey, WID_SL_SCROLLBAR),
	EndContainer(),
	/* Online Content and Load button */
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_CONTENT_DOWNLOAD), SetResize(1, 0), SetFill(1, 0),
				SetStringTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_LOAD_BUTTON), SetResize(1, 0), SetFill(1, 0),
				SetStringTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_HEIGHTMAP_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Grey),
	EndContainer(),
};

/** Load town data */
static constexpr std::initializer_list<NWidgetPart> _nested_load_town_data_dialog_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, Colours::Grey),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, Colours::Grey, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),

	/* Filter box with label */
	NWidget(WWT_PANEL, Colours::Grey), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
				SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
			NWidget(WWT_TEXT, Colours::Invalid), SetFill(0, 1), SetStringTip(STR_SAVELOAD_FILTER_TITLE),
			NWidget(WWT_EDITBOX, Colours::Grey, WID_SL_FILTER), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
	EndContainer(),
	/* Sort Buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
			NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYNAME), SetStringTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYDATE), SetStringTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, Colours::Grey, WID_SL_HOME_BUTTON), SetAspect(1), SetSpriteTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON_TOOLTIP),
	EndContainer(),
	/* Files */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Grey, WID_SL_FILE_BACKGROUND),
			NWidget(WWT_INSET, Colours::Grey, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 2, 2, 2),
					SetToolTip(STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Grey, WID_SL_SCROLLBAR),
	EndContainer(),
	/* Load button */
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_LOAD_BUTTON), SetResize(1, 0), SetFill(1, 0),
				SetStringTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_TOWN_DATA_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Grey),
	EndContainer(),
};

/** Save game/scenario */
static constexpr std::initializer_list<NWidgetPart> _nested_save_dialog_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, Colours::Grey),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, Colours::Grey, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),

	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		/* Left side : filter box and available files */
		NWidget(NWID_VERTICAL),
			/* Filter box with label */
			NWidget(WWT_PANEL, Colours::Grey), SetFill(1, 1), SetResize(1, 1),
				NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
						SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
					NWidget(WWT_TEXT, Colours::Invalid), SetFill(0, 1), SetStringTip(STR_SAVELOAD_FILTER_TITLE),
					NWidget(WWT_EDITBOX, Colours::Grey, WID_SL_FILTER), SetFill(1, 0), SetResize(1, 0),
							SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
			EndContainer(),
			/* Sort buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYNAME), SetStringTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SORT_BYDATE), SetStringTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, Colours::Grey, WID_SL_HOME_BUTTON), SetAspect(1), SetSpriteTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON_TOOLTIP),
			EndContainer(),
			/* Files */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, Colours::Grey, WID_SL_FILE_BACKGROUND),
					NWidget(WWT_INSET, Colours::Grey, WID_SL_DRIVES_DIRECTORIES_LIST), SetPadding(2, 2, 2, 2),
							SetToolTip(STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_VSCROLLBAR, Colours::Grey, WID_SL_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_PANEL, Colours::Grey),
				NWidget(WWT_EDITBOX, Colours::Grey, WID_SL_SAVE_OSK_TITLE), SetPadding(2, 2, 2, 2), SetFill(1, 0), SetResize(1, 0),
						SetStringTip(STR_SAVELOAD_OSKTITLE, STR_SAVELOAD_EDITBOX_TOOLTIP),
			EndContainer(),
			/* Save/delete buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_DELETE_SELECTION), SetStringTip(STR_SAVELOAD_DELETE_BUTTON, STR_SAVELOAD_DELETE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_SL_SAVE_GAME),        SetStringTip(STR_SAVELOAD_SAVE_BUTTON, STR_SAVELOAD_SAVE_TOOLTIP),     SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),

		/* Right side : game details */
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, Colours::Grey, WID_SL_DETAILS), SetResize(1, 1), SetFill(1, 1),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, Colours::Grey), SetResize(1, 0), SetFill(1, 1),
				EndContainer(),
				NWidget(WWT_RESIZEBOX, Colours::Grey),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Text colours of #DetailedFileType fios entries in the window. */
static const EnumClassIndexContainer<std::array<TextColour, to_underlying(DetailedFileType::End)>, DetailedFileType> _fios_colours = {
	TC_LIGHT_BROWN, // DetailedFileType::OldGameFile
	TC_ORANGE, // DetailedFileType::GameFile
	TC_YELLOW, // DetailedFileType::HeightmapBmp
	TC_ORANGE, // DetailedFileType::HeightmapPng
	TC_LIGHT_BROWN, // DetailedFileType::TownDataJson
	TC_LIGHT_BLUE, // DetailedFileType::FiosDrive
	TC_DARK_GREEN, // DetailedFileType::FiosParent
	TC_DARK_GREEN, // DetailedFileType::FiosDirectory
	TC_ORANGE, // DetailedFileType::FiosDirect
};

/**
 * Sort the collected list save games prior to displaying it in the save/load gui.
 * @param[in,out] file_list List of save game files found in the directory.
 */
static void SortSaveGameList(FileList &file_list)
{
	size_t sort_start = 0;
	size_t sort_end = 0;

	/* Directories are always above the files (FIOS_TYPE_DIR)
	 * Drives (A:\ (windows only) are always under the files (FIOS_TYPE_DRIVE)
	 * Only sort savegames/scenarios, not directories
	 */
	for (const auto &item : file_list) {
		switch (item.type.detailed) {
			case DetailedFileType::FiosDirectory: sort_start++; break;
			case DetailedFileType::FiosParent: sort_start++; break;
			case DetailedFileType::FiosDrive: sort_end++; break;
			default: break;
		}
	}

	std::sort(file_list.begin() + sort_start, file_list.end() - sort_end, FiosItemSorter);
}

struct SaveLoadWindow : public Window {
private:
	static const uint EDITBOX_MAX_SIZE   =  50;

	QueryString filename_editbox; ///< Filename editbox.
	AbstractFileType abstract_filetype{}; ///< Type of file to select.
	SaveLoadOperation fop{}; ///< File operation to perform.
	FileList fios_items{}; ///< Item list.
	FiosItem o_dir{}; ///< Original dir (home dir for this browser)
	const FiosItem *selected = nullptr; ///< Selected item in #fios_items, or \c nullptr.
	const FiosItem *highlighted = nullptr; ///< Item in fios_items highlighted by mouse pointer, or \c nullptr.
	Scrollbar *vscroll = nullptr;

	StringFilter string_filter{}; ///< Filter for available items.
	QueryString filter_editbox; ///< Filter editbox;
	std::vector<FiosItem *> display_list{}; ///< Filtered display list

	static void SaveGameConfirmationCallback(Window *, bool confirmed)
	{
		/* File name has already been written to _file_to_saveload */
		if (confirmed) _switch_mode = SM_SAVE_GAME;
	}

	static void SaveHeightmapConfirmationCallback(Window *, bool confirmed)
	{
		/* File name has already been written to _file_to_saveload */
		if (confirmed) _switch_mode = SM_SAVE_HEIGHTMAP;
	}

	static void DeleteFileConfirmationCallback(Window *window, bool confirmed)
	{
		auto *save_load_window = static_cast<SaveLoadWindow*>(window);

		assert(save_load_window->selected != nullptr);

		if (confirmed) {
			if (!FioRemove(save_load_window->selected->name)) {
				ShowErrorMessage(GetEncodedString(STR_ERROR_UNABLE_TO_DELETE_FILE), {}, WL_ERROR);
			} else {
				save_load_window->InvalidateData(SLIWD_RESCAN_FILES);
				/* Reset file name to current date on successful delete */
				if (save_load_window->abstract_filetype == AbstractFileType::Savegame) save_load_window->GenerateFileName();
			}
		}
	}

public:

	/** Generate a default save filename. */
	void GenerateFileName()
	{
		this->filename_editbox.text.Assign(GenerateDefaultSaveName());
	}

	SaveLoadWindow(WindowDesc &desc, AbstractFileType abstract_filetype, SaveLoadOperation fop)
			: Window(desc), filename_editbox(64), abstract_filetype(abstract_filetype), fop(fop), filter_editbox(EDITBOX_MAX_SIZE)
	{
		assert(this->fop == SaveLoadOperation::Save || this->fop == SaveLoadOperation::Load);

		/* For saving, construct an initial file name. */
		if (this->fop == SaveLoadOperation::Save) {
			switch (this->abstract_filetype) {
				case AbstractFileType::Savegame:
					this->GenerateFileName();
					break;

				case AbstractFileType::Scenario:
				case AbstractFileType::Heightmap:
					this->filename_editbox.text.Assign("UNNAMED");
					break;

				default:
					/* It's not currently possible to save town data. */
					NOT_REACHED();
			}
		}
		this->querystrings[WID_SL_SAVE_OSK_TITLE] = &this->filename_editbox;
		this->filename_editbox.ok_button = WID_SL_SAVE_GAME;

		this->CreateNestedTree();
		if (this->fop == SaveLoadOperation::Load && this->abstract_filetype == AbstractFileType::Savegame) {
			this->GetWidget<NWidgetStacked>(WID_SL_CONTENT_DOWNLOAD_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		}

		/* Select caption string of the window. */
		StringID caption_string;
		switch (this->abstract_filetype) {
			case AbstractFileType::Savegame:
				caption_string = (this->fop == SaveLoadOperation::Save) ? STR_SAVELOAD_SAVE_CAPTION : STR_SAVELOAD_LOAD_CAPTION;
				break;

			case AbstractFileType::Scenario:
				caption_string = (this->fop == SaveLoadOperation::Save) ? STR_SAVELOAD_SAVE_SCENARIO : STR_SAVELOAD_LOAD_SCENARIO;
				break;

			case AbstractFileType::Heightmap:
				caption_string = (this->fop == SaveLoadOperation::Save) ? STR_SAVELOAD_SAVE_HEIGHTMAP : STR_SAVELOAD_LOAD_HEIGHTMAP;
				break;

			case AbstractFileType::TownData:
				caption_string = STR_SAVELOAD_LOAD_TOWN_DATA; // It's not currently possible to save town data.
				break;

			default:
				NOT_REACHED();
		}
		this->GetWidget<NWidgetCore>(WID_SL_CAPTION)->SetString(caption_string);

		this->vscroll = this->GetScrollbar(WID_SL_SCROLLBAR);
		this->FinishInitNested(0);

		this->LowerWidget(WID_SL_DRIVES_DIRECTORIES_LIST);
		this->querystrings[WID_SL_FILTER] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;

		/* pause is only used in single-player, non-editor mode, non-menu mode. It
		 * will be unpaused in the WE_DESTROY event handler. */
		if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
			Command<Commands::Pause>::Post(PauseMode::SaveLoad, true);
		}
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

		this->OnInvalidateData(SLIWD_RESCAN_FILES);

		ResetObjectToPlace();

		/* Select the initial directory. */
		o_dir.type = FIOS_TYPE_DIRECT;
		switch (this->abstract_filetype) {
			case AbstractFileType::Savegame:
				o_dir.name = FioFindDirectory(Subdirectory::Save);
				break;

			case AbstractFileType::Scenario:
				o_dir.name = FioFindDirectory(Subdirectory::Scenario);
				break;

			case AbstractFileType::Heightmap:
			case AbstractFileType::TownData:
				o_dir.name = FioFindDirectory(Subdirectory::Heightmap);
				break;

			default:
				o_dir.name = _personal_dir;
		}

		switch (this->fop) {
			case SaveLoadOperation::Save:
				/* Focus the edit box by default in the save window */
				this->SetFocusedWidget(WID_SL_SAVE_OSK_TITLE);
				break;

			default:
				this->SetFocusedWidget(WID_SL_FILTER);
		}
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		/* pause is only used in single-player, non-editor mode, non menu mode */
		if (!_networking && _game_mode != GM_EDITOR && _game_mode != GM_MENU) {
			Command<Commands::Pause>::Post(PauseMode::SaveLoad, false);
		}
		this->Window::Close();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SL_SORT_BYNAME:
			case WID_SL_SORT_BYDATE:
				if ((_savegame_sorter == SavegameSorter::Name) == (widget == WID_SL_SORT_BYNAME)) {
					this->DrawSortButtonState(widget, _savegame_sorter_ascending ? SBS_UP : SBS_DOWN);
				}
				break;

			case WID_SL_BACKGROUND: {
				static std::string path;
				static std::optional<uint64_t> free_space = std::nullopt;

				if (_fios_path_changed) {
					path = FiosGetCurrentPath();
					free_space = FiosGetDiskFreeSpace(path);
					_fios_path_changed = false;
				}

				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);

				if (free_space.has_value()) {
					DrawString(ir.left, ir.right, ir.top + GetCharacterHeight(FontSize::Normal), GetString(STR_SAVELOAD_BYTES_FREE, free_space.value()));
				} else {
					DrawString(ir.left, ir.right, ir.top + GetCharacterHeight(FontSize::Normal), STR_ERROR_UNABLE_TO_READ_DRIVE);
				}
				DrawString(ir.left, ir.right, ir.top, path, TC_BLACK);
				break;
			}

			case WID_SL_DRIVES_DIRECTORIES_LIST: {
				const Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
				GfxFillRect(br, PC_BLACK);

				Rect tr = r.Shrink(WidgetDimensions::scaled.inset).WithHeight(this->resize.step_height);
				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->display_list);
				for (auto it = first; it != last; ++it) {
					const FiosItem *item = *it;

					if (item == this->selected) {
						GfxFillRect(br.WithY(tr), PC_DARK_BLUE);
					} else if (item == this->highlighted) {
						GfxFillRect(br.WithY(tr), PC_VERY_DARK_BLUE);
					}
					DrawString(tr, item->title.GetDecodedString(), _fios_colours[item->type.detailed]);
					tr = tr.Translate(0, this->resize.step_height);
				}
				break;
			}

			case WID_SL_DETAILS:
				this->DrawDetails(r);
				break;
		}
	}

	void DrawDetails(const Rect &r) const
	{
		/* Header panel */
		int HEADER_HEIGHT = GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.frametext.Vertical();

		Rect hr = r.WithHeight(HEADER_HEIGHT).Shrink(WidgetDimensions::scaled.frametext);
		Rect tr = r.Shrink(WidgetDimensions::scaled.frametext);
		tr.top += HEADER_HEIGHT;

		/* Create the nice lighter rectangle at the details top */
		GfxFillRect(r.WithHeight(HEADER_HEIGHT).Shrink(WidgetDimensions::scaled.bevel.left, WidgetDimensions::scaled.bevel.top, WidgetDimensions::scaled.bevel.right, 0), GetColourGradient(Colours::Grey, SHADE_LIGHTEST));
		DrawString(hr.left, hr.right, hr.top, STR_SAVELOAD_DETAIL_CAPTION, TC_FROMSTRING, SA_HOR_CENTER);

		if (this->selected == nullptr) return;

		/* Details panel */
		tr.bottom -= GetCharacterHeight(FontSize::Normal) - 1;
		if (tr.top > tr.bottom) return;

		if (!_load_check_data.checkable) {
			/* Old savegame, no information available */
			DrawString(tr, STR_SAVELOAD_DETAIL_NOT_AVAILABLE);
			tr.top += GetCharacterHeight(FontSize::Normal);
		} else if (_load_check_data.error != INVALID_STRING_ID) {
			/* Incompatible / broken savegame */
			tr.top = DrawStringMultiLine(tr, GetString(_load_check_data.error, _load_check_data.error_msg), TC_RED);
		} else {
			/* Mapsize */
			DrawString(tr, GetString(STR_NETWORK_SERVER_LIST_MAP_SIZE, _load_check_data.map_size_x, _load_check_data.map_size_y));
			tr.top += GetCharacterHeight(FontSize::Normal);
			if (tr.top > tr.bottom) return;

			/* Climate */
			if (to_underlying(_load_check_data.landscape) < NUM_LANDSCAPE) {
				DrawString(tr, GetString(STR_NETWORK_SERVER_LIST_LANDSCAPE, STR_CLIMATE_TEMPERATE_LANDSCAPE + to_underlying(_load_check_data.landscape)));
				tr.top += GetCharacterHeight(FontSize::Normal);
			}

			tr.top += WidgetDimensions::scaled.vsep_normal;
			if (tr.top > tr.bottom) return;

			/* Start date (if available) */
			if (_load_check_data.starting_year != 0) {
				DrawString(tr, GetString(STR_NETWORK_SERVER_LIST_START_DATE, TimerGameCalendar::ConvertYMDToDate(_load_check_data.starting_year, 0, 1)));
				tr.top += GetCharacterHeight(FontSize::Normal);
			}
			if (tr.top > tr.bottom) return;

			/* Hide current date for scenarios */
			if (this->abstract_filetype != AbstractFileType::Scenario) {
				/* Current date */
				DrawString(tr, GetString(STR_NETWORK_SERVER_LIST_CURRENT_DATE, _load_check_data.current_date));
				tr.top += GetCharacterHeight(FontSize::Normal);
			}

			/* Hide the NewGRF stuff when saving. We also hide the button. */
			if (this->fop == SaveLoadOperation::Load && (this->abstract_filetype == AbstractFileType::Savegame || this->abstract_filetype == AbstractFileType::Scenario)) {
				tr.top += WidgetDimensions::scaled.vsep_normal;
				if (tr.top > tr.bottom) return;

				/* NewGrf compatibility */
				DrawString(tr, GetString(STR_SAVELOAD_DETAIL_GRFSTATUS, _load_check_data.grfconfig.empty() ? STR_NEWGRF_LIST_NONE : STR_NEWGRF_LIST_ALL_FOUND + to_underlying(_load_check_data.grf_compatibility)));
				tr.top += GetCharacterHeight(FontSize::Normal);
			}
			if (tr.top > tr.bottom) return;

			/* Hide the company stuff for scenarios */
			if (this->abstract_filetype != AbstractFileType::Scenario) {
				tr.top += WidgetDimensions::scaled.vsep_wide;
				if (tr.top > tr.bottom) return;

				/* Companies / AIs */
				for (auto &pair : _load_check_data.companies) {
					const CompanyProperties &c = *pair.second;
					if (c.name.empty()) {
						AutoRestoreBackup landscape_backup(_settings_game.game_creation.landscape, _load_check_data.landscape);
						DrawString(tr, GetString(STR_SAVELOAD_DETAIL_COMPANY_INDEX, pair.first + 1, c.name_1, c.name_2));
					} else {
						DrawString(tr, GetString(STR_SAVELOAD_DETAIL_COMPANY_INDEX, pair.first + 1, STR_JUST_RAW_STRING, c.name));
					}

					tr.top += GetCharacterHeight(FontSize::Normal);
					if (tr.top > tr.bottom) break;
				}
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_SL_BACKGROUND:
				size.height = 2 * GetCharacterHeight(FontSize::Normal) + padding.height;
				break;

			case WID_SL_DRIVES_DIRECTORIES_LIST:
				fill.height = resize.height = GetCharacterHeight(FontSize::Normal);
				size.height = resize.height * 10 + padding.height;
				break;
			case WID_SL_SORT_BYNAME:
			case WID_SL_SORT_BYDATE: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	void OnPaint() override
	{
		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			SortSaveGameList(this->fios_items);
			this->OnInvalidateData(SLIWD_FILTER_CHANGES);
		}

		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SL_SORT_BYNAME: // Sort save games by name
				_savegame_sorter_ascending = _savegame_sorter != SavegameSorter::Name || !_savegame_sorter_ascending;
				_savegame_sorter = SavegameSorter::Name;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case WID_SL_SORT_BYDATE: // Sort save games by date
				_savegame_sorter_ascending = _savegame_sorter != SavegameSorter::Date || !_savegame_sorter_ascending;
				_savegame_sorter = SavegameSorter::Date;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case WID_SL_HOME_BUTTON: // OpenTTD 'button', jumps to OpenTTD directory
				FiosBrowseTo(&o_dir);
				this->InvalidateData(SLIWD_RESCAN_FILES);
				break;

			case WID_SL_LOAD_BUTTON: {
				if (this->selected == nullptr || _load_check_data.HasErrors()) break;

				_file_to_saveload.Set(*this->selected);

				if (this->abstract_filetype == AbstractFileType::Heightmap) {
					this->Close();
					ShowHeightmapLoad();
				} else if (this->abstract_filetype == AbstractFileType::TownData) {
					this->Close();
					LoadTownData();
				} else if (!_load_check_data.HasNewGrfs() || _load_check_data.grf_compatibility != GRFListCompatibility::NotFound || _settings_client.gui.UserIsAllowedToChangeNewGRFs()) {
					_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD_GAME;
					ClearErrorMessages();
					this->Close();
				}
				break;
			}

			case WID_SL_NEWGRF_INFO:
				if (_load_check_data.HasNewGrfs()) {
					ShowNewGRFSettings(false, false, false, _load_check_data.grfconfig);
				}
				break;

			case WID_SL_MISSING_NEWGRFS:
				if (!_network_available) {
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
				} else if (_load_check_data.HasNewGrfs()) {
					ShowMissingContentWindow(_load_check_data.grfconfig);
				}
				break;

			case WID_SL_DRIVES_DIRECTORIES_LIST: { // Click the listbox
				auto it = this->vscroll->GetScrolledItemFromWidget(this->display_list, pt.y, this, WID_SL_DRIVES_DIRECTORIES_LIST, WidgetDimensions::scaled.inset.top);
				if (it == this->display_list.end()) return;

				/* Get the corresponding non-filtered out item from the list */
				const FiosItem *file = *it;

				if (FiosBrowseTo(file)) {
					/* Changed directory, need refresh. */
					this->InvalidateData(SLIWD_RESCAN_FILES);
					break;
				}

				if (click_count == 1) {
					if (this->selected != file) {
						this->selected = file;
						_load_check_data.Clear();

						if (file->type.detailed == DetailedFileType::GameFile) {
							/* Other detailed file types cannot be checked before. */
							SaveOrLoad(file->name, SaveLoadOperation::Check, DetailedFileType::GameFile, Subdirectory::None, false);
						}

						this->InvalidateData(SLIWD_SELECTION_CHANGES);
					}
					if (this->fop == SaveLoadOperation::Save) {
						/* Copy clicked name to editbox */
						this->filename_editbox.text.Assign(file->title.GetDecodedString());
						this->SetWidgetDirty(WID_SL_SAVE_OSK_TITLE);
					}
				} else if (!_load_check_data.HasErrors()) {
					this->selected = file;
					if (this->fop == SaveLoadOperation::Load) {
						if (this->abstract_filetype == AbstractFileType::Savegame || this->abstract_filetype == AbstractFileType::Scenario || this->abstract_filetype == AbstractFileType::TownData) {
							this->OnClick(pt, WID_SL_LOAD_BUTTON, 1);
						} else {
							assert(this->abstract_filetype == AbstractFileType::Heightmap);
							_file_to_saveload.Set(*file);

							this->Close();
							ShowHeightmapLoad();
						}
					}
				}
				break;
			}

			case WID_SL_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
				} else {
					assert(this->fop == SaveLoadOperation::Load);
					switch (this->abstract_filetype) {
						default: NOT_REACHED();
						case AbstractFileType::Scenario: ShowNetworkContentListWindow(nullptr, ContentType::Scenario); break;
						case AbstractFileType::Heightmap: ShowNetworkContentListWindow(nullptr, ContentType::Heightmap); break;
					}
				}
				break;

			case WID_SL_DELETE_SELECTION: // Delete
				break;

			case WID_SL_SAVE_GAME: // Save game
				/* Note, this is also called via the OSK; and we need to lower the button. */
				this->HandleButtonClick(WID_SL_SAVE_GAME);
				break;
		}
	}

	void OnMouseOver([[maybe_unused]] Point pt, WidgetID widget) override
	{
		if (widget == WID_SL_DRIVES_DIRECTORIES_LIST) {
			auto it = this->vscroll->GetScrolledItemFromWidget(this->display_list, pt.y, this, WID_SL_DRIVES_DIRECTORIES_LIST, WidgetDimensions::scaled.inset.top);
			if (it == this->display_list.end()) return;

			/* Get the corresponding non-filtered out item from the list */
			const FiosItem *file = *it;

			if (file != this->highlighted) {
				this->highlighted = file;
				this->SetWidgetDirty(WID_SL_DRIVES_DIRECTORIES_LIST);
			}
		} else if (this->highlighted != nullptr) {
			this->highlighted = nullptr;
			this->SetWidgetDirty(WID_SL_DRIVES_DIRECTORIES_LIST);
		}
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		if (keycode == WKC_ESC) {
			this->Close();
			return ES_HANDLED;
		}

		return ES_NOT_HANDLED;
	}

	void OnTimeout() override
	{
		/* Widgets WID_SL_DELETE_SELECTION and WID_SL_SAVE_GAME only exist when saving to a file. */
		if (this->fop != SaveLoadOperation::Save) return;

		if (this->IsWidgetLowered(WID_SL_DELETE_SELECTION)) { // Delete button clicked
			ShowQuery(GetEncodedString(STR_SAVELOAD_DELETE_TITLE), GetEncodedString(STR_SAVELOAD_DELETE_WARNING),
					this, SaveLoadWindow::DeleteFileConfirmationCallback);
		} else if (this->IsWidgetLowered(WID_SL_SAVE_GAME)) { // Save button clicked
			if (this->abstract_filetype == AbstractFileType::Savegame || this->abstract_filetype == AbstractFileType::Scenario) {
				_file_to_saveload.name = FiosMakeSavegameName(this->filename_editbox.text.GetText());
				if (FioCheckFileExists(_file_to_saveload.name, Subdirectory::Save)) {
					ShowQuery(GetEncodedString(STR_SAVELOAD_OVERWRITE_TITLE), GetEncodedString(STR_SAVELOAD_OVERWRITE_WARNING),
							this, SaveLoadWindow::SaveGameConfirmationCallback);
				} else {
					_switch_mode = SM_SAVE_GAME;
				}
			} else {
				_file_to_saveload.name = FiosMakeHeightmapName(this->filename_editbox.text.GetText());
				if (FioCheckFileExists(_file_to_saveload.name, Subdirectory::Save)) {
					ShowQuery(GetEncodedString(STR_SAVELOAD_OVERWRITE_TITLE), GetEncodedString(STR_SAVELOAD_OVERWRITE_WARNING),
							this, SaveLoadWindow::SaveHeightmapConfirmationCallback);
				} else {
					_switch_mode = SM_SAVE_HEIGHTMAP;
				}
			}

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SL_DRIVES_DIRECTORIES_LIST);
	}

	void BuildDisplayList()
	{
		/* Filter changes */
		this->display_list.clear();
		this->display_list.reserve(this->fios_items.size());

		if (this->string_filter.IsEmpty()) {
			/* We don't filter anything out if the filter editbox is empty */
			for (auto &it : this->fios_items) {
				this->display_list.push_back(&it);
			}
		} else {
			for (auto &it : this->fios_items) {
				this->string_filter.ResetState();
				this->string_filter.AddLine(it.title.GetDecodedString());
				/* We set the vector to show this fios element as filtered depending on the result of the filter */
				if (this->string_filter.GetState()) {
					this->display_list.push_back(&it);
				} else if (&it == this->selected) {
					/* The selected element has been filtered out */
					this->selected = nullptr;
					this->OnInvalidateData(SLIWD_SELECTION_CHANGES);
				}
			}
		}

		this->vscroll->SetCount(this->display_list.size());
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		switch (data) {
			case SLIWD_RESCAN_FILES:
				/* Rescan files */
				this->selected = nullptr;
				_load_check_data.Clear();
				if (!gui_scope) break;

				_fios_path_changed = true;
				this->fios_items.BuildFileList(this->abstract_filetype, this->fop, true);
				this->selected = nullptr;
				_load_check_data.Clear();

				/* We reset the files filtered */
				this->OnInvalidateData(SLIWD_FILTER_CHANGES);

				[[fallthrough]];

			case SLIWD_SELECTION_CHANGES:
				/* Selection changes */
				if (!gui_scope) break;

				if (this->fop == SaveLoadOperation::Save) this->SetWidgetDisabledState(WID_SL_DELETE_SELECTION, this->selected == nullptr);

				if (this->fop != SaveLoadOperation::Load) break;

				switch (this->abstract_filetype) {
					case AbstractFileType::Heightmap:
					case AbstractFileType::TownData:
						this->SetWidgetDisabledState(WID_SL_LOAD_BUTTON, this->selected == nullptr || _load_check_data.HasErrors());
						break;

					case AbstractFileType::Savegame:
					case AbstractFileType::Scenario: {
						bool disabled = this->selected == nullptr || _load_check_data.HasErrors();
						if (!_settings_client.gui.UserIsAllowedToChangeNewGRFs()) {
							disabled |= _load_check_data.HasNewGrfs() && _load_check_data.grf_compatibility == GRFListCompatibility::NotFound;
						}
						this->SetWidgetDisabledState(WID_SL_LOAD_BUTTON, disabled);
						this->SetWidgetDisabledState(WID_SL_NEWGRF_INFO, !_load_check_data.HasNewGrfs());
						this->SetWidgetDisabledState(WID_SL_MISSING_NEWGRFS,
								!_load_check_data.HasNewGrfs() || _load_check_data.grf_compatibility == GRFListCompatibility::AllGood);
						break;
					}

					default:
						NOT_REACHED();
				}
				break;

			case SLIWD_FILTER_CHANGES:
				this->BuildDisplayList();
				break;
		}
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_SL_FILTER) {
			this->string_filter.SetFilterTerm(this->filter_editbox.text.GetText());
			this->InvalidateData(SLIWD_FILTER_CHANGES);
		}

		if (wid == WID_SL_SAVE_OSK_TITLE) {
			this->selected = nullptr;
			this->InvalidateData(SLIWD_SELECTION_CHANGES);
		}
	}
};

/** Load game/scenario */
static WindowDesc _load_dialog_desc(
	WindowPosition::Center, "load_game", 500, 294,
	WC_SAVELOAD, WC_NONE,
	{},
	_nested_load_dialog_widgets
);

/** Load heightmap */
static WindowDesc _load_heightmap_dialog_desc(
	WindowPosition::Center, "load_heightmap", 257, 320,
	WC_SAVELOAD, WC_NONE,
	{},
	_nested_load_heightmap_dialog_widgets
);

/** Load town data */
static WindowDesc _load_town_data_dialog_desc(
	WindowPosition::Center, "load_town_data", 257, 320,
	WC_SAVELOAD, WC_NONE,
	{},
	_nested_load_town_data_dialog_widgets
);

/** Save game/scenario */
static WindowDesc _save_dialog_desc(
	WindowPosition::Center, "save_game", 500, 294,
	WC_SAVELOAD, WC_NONE,
	{},
	_nested_save_dialog_widgets
);

/**
 * Launch save/load dialog in the given mode.
 * @param abstract_filetype Kind of file to handle.
 * @param fop File operation to perform (load or save).
 */
void ShowSaveLoadDialog(AbstractFileType abstract_filetype, SaveLoadOperation fop)
{
	CloseWindowById(WC_SAVELOAD, 0);

	if (fop == SaveLoadOperation::Save) {
		new SaveLoadWindow(_save_dialog_desc, abstract_filetype, fop);
	} else {
		/* Dialogue for loading a file. */
		switch (abstract_filetype) {
			case AbstractFileType::Heightmap:
				new SaveLoadWindow(_load_heightmap_dialog_desc, abstract_filetype, fop);
				break;

			case AbstractFileType::TownData:
				new SaveLoadWindow(_load_town_data_dialog_desc, abstract_filetype, fop);
				break;

			default:
				new SaveLoadWindow(_load_dialog_desc, abstract_filetype, fop);
		}
	}
}
