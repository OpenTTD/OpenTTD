/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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

/**
 * Reset read data.
 */
void LoadCheckData::Clear()
{
	this->checkable = false;
	this->error = INVALID_STRING_ID;
	this->error_msg.clear();

	this->map_size_x = this->map_size_y = 256; // Default for old savegames which do not store mapsize.
	this->current_date = 0;
	this->settings = {};

	companies.clear();

	this->gamelog.Reset();

	ClearGRFConfigList(&this->grfconfig);
}

/** Load game/scenario with optional content download */
static const NWidgetPart _nested_load_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),

	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		/* Left side : filter box and available files */
		NWidget(NWID_VERTICAL),
			/* Filter box with label */
			NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 1),
				NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
					SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
						NWidget(WWT_TEXT, COLOUR_GREY), SetFill(0, 1), SetDataTip(STR_SAVELOAD_FILTER_TITLE , STR_NULL),
						NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SL_FILTER), SetFill(1, 0), SetMinimalSize(50, 12), SetResize(1, 0),
							SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
			EndContainer(),
			/* Sort buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SL_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
			EndContainer(),
			/* Files */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_FILE_BACKGROUND),
					NWidget(WWT_INSET, COLOUR_GREY, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 2, 2, 2),
							SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
				EndContainer(),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SL_SCROLLBAR),
			EndContainer(),
			/* Online Content button */
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SL_CONTENT_DOWNLOAD_SEL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_CONTENT_DOWNLOAD), SetResize(1, 0),
						SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
			EndContainer(),
		EndContainer(),

		/* Right side : game details */
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_DETAILS), SetResize(1, 1), SetFill(1, 1),
			EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_MISSING_NEWGRFS), SetDataTip(STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_BUTTON, STR_NEWGRF_SETTINGS_FIND_MISSING_CONTENT_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_NEWGRF_INFO), SetDataTip(STR_INTRO_NEWGRF_SETTINGS, STR_NULL), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_LOAD_BUTTON), SetDataTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Load heightmap with content download */
static const NWidgetPart _nested_load_heightmap_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),

	/* Filter box with label */
	NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
			SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
				NWidget(WWT_TEXT, COLOUR_GREY), SetFill(0, 1), SetDataTip(STR_SAVELOAD_FILTER_TITLE , STR_NULL),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SL_FILTER), SetFill(1, 0), SetMinimalSize(50, 12), SetResize(1, 0),
					SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
		EndContainer(),
	EndContainer(),
	/* Sort Buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SL_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
	EndContainer(),
	/* Files */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_FILE_BACKGROUND),
			NWidget(WWT_INSET, COLOUR_GREY, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 2, 2, 2),
					SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SL_SCROLLBAR),
	EndContainer(),
	/* Online Content and Load button */
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_CONTENT_DOWNLOAD), SetResize(1, 0), SetFill(1, 0),
				SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_LOAD_BUTTON), SetResize(1, 0), SetFill(1, 0),
				SetDataTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_HEIGHTMAP_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/** Save game/scenario */
static const NWidgetPart _nested_save_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
	EndContainer(),
	/* Current directory and free space */
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		/* Left side : filter box and available files */
		NWidget(NWID_VERTICAL),
			/* Filter box with label */
			NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 1),
				NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect.top, 0, WidgetDimensions::unscaled.framerect.bottom, 0),
					SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.frametext.right, 0),
					NWidget(WWT_TEXT, COLOUR_GREY), SetFill(0, 1), SetDataTip(STR_SAVELOAD_FILTER_TITLE , STR_NULL),
					NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SL_FILTER), SetFill(1, 0), SetMinimalSize(50, 12), SetResize(1, 0),
						SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
				EndContainer(),
			EndContainer(),
			/* Sort buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SL_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
			EndContainer(),
			/* Files */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_FILE_BACKGROUND),
					NWidget(WWT_INSET, COLOUR_GREY, WID_SL_DRIVES_DIRECTORIES_LIST), SetPadding(2, 2, 2, 2),
							SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
				EndContainer(),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SL_SCROLLBAR),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SL_SAVE_OSK_TITLE), SetPadding(2, 2, 2, 2), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_SAVELOAD_OSKTITLE, STR_SAVELOAD_EDITBOX_TOOLTIP),
			EndContainer(),
			/* Save/delete buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_DELETE_SELECTION), SetDataTip(STR_SAVELOAD_DELETE_BUTTON, STR_SAVELOAD_DELETE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SAVE_GAME),        SetDataTip(STR_SAVELOAD_SAVE_BUTTON, STR_SAVELOAD_SAVE_TOOLTIP),     SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),

		/* Right side : game details */
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_DETAILS), SetResize(1, 1), SetFill(1, 1), EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Text colours of #DetailedFileType fios entries in the window. */
static const TextColour _fios_colours[] = {
	TC_LIGHT_BROWN,  // DFT_OLD_GAME_FILE
	TC_ORANGE,       // DFT_GAME_FILE
	TC_YELLOW,       // DFT_HEIGHTMAP_BMP
	TC_ORANGE,       // DFT_HEIGHTMAP_PNG
	TC_LIGHT_BLUE,   // DFT_FIOS_DRIVE
	TC_DARK_GREEN,   // DFT_FIOS_PARENT
	TC_DARK_GREEN,   // DFT_FIOS_DIR
	TC_ORANGE,       // DFT_FIOS_DIRECT
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
		switch (item.type) {
			case FIOS_TYPE_DIR:    sort_start++; break;
			case FIOS_TYPE_PARENT: sort_start++; break;
			case FIOS_TYPE_DRIVE:  sort_end++;   break;
			default: break;
		}
	}

	std::sort(file_list.begin() + sort_start, file_list.end() - sort_end);
}

struct SaveLoadWindow : public Window {
private:
	static const uint EDITBOX_MAX_SIZE   =  50;

	QueryString filename_editbox; ///< Filename editbox.
	AbstractFileType abstract_filetype; /// Type of file to select.
	SaveLoadOperation fop;        ///< File operation to perform.
	FileList fios_items;          ///< Save game list.
	FiosItem o_dir;               ///< Original dir (home dir for this browser)
	const FiosItem *selected;     ///< Selected game in #fios_items, or \c nullptr.
	const FiosItem *highlighted;  ///< Item in fios_items highlighted by mouse pointer, or \c nullptr.
	Scrollbar *vscroll;

	StringFilter string_filter; ///< Filter for available games.
	QueryString filter_editbox; ///< Filter editbox;
	std::vector<FiosItem *> display_list; ///< Filtered display list

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

public:

	/** Generate a default save filename. */
	void GenerateFileName()
	{
		this->filename_editbox.text.Assign(GenerateDefaultSaveName());
	}

	SaveLoadWindow(WindowDesc *desc, AbstractFileType abstract_filetype, SaveLoadOperation fop)
			: Window(desc), filename_editbox(64), abstract_filetype(abstract_filetype), fop(fop), filter_editbox(EDITBOX_MAX_SIZE)
	{
		assert(this->fop == SLO_SAVE || this->fop == SLO_LOAD);

		/* For saving, construct an initial file name. */
		if (this->fop == SLO_SAVE) {
			switch (this->abstract_filetype) {
				case FT_SAVEGAME:
					this->GenerateFileName();
					break;

				case FT_SCENARIO:
				case FT_HEIGHTMAP:
					this->filename_editbox.text.Assign("UNNAMED");
					break;

				default:
					NOT_REACHED();
			}
		}
		this->querystrings[WID_SL_SAVE_OSK_TITLE] = &this->filename_editbox;
		this->filename_editbox.ok_button = WID_SL_SAVE_GAME;

		this->CreateNestedTree();
		if (this->fop == SLO_LOAD && this->abstract_filetype == FT_SAVEGAME) {
			this->GetWidget<NWidgetStacked>(WID_SL_CONTENT_DOWNLOAD_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		}

		/* Select caption string of the window. */
		StringID caption_string;
		switch (this->abstract_filetype) {
			case FT_SAVEGAME:
				caption_string = (this->fop == SLO_SAVE) ? STR_SAVELOAD_SAVE_CAPTION : STR_SAVELOAD_LOAD_CAPTION;
				break;

			case FT_SCENARIO:
				caption_string = (this->fop == SLO_SAVE) ? STR_SAVELOAD_SAVE_SCENARIO : STR_SAVELOAD_LOAD_SCENARIO;
				break;

			case FT_HEIGHTMAP:
				caption_string = (this->fop == SLO_SAVE) ? STR_SAVELOAD_SAVE_HEIGHTMAP : STR_SAVELOAD_LOAD_HEIGHTMAP;
				break;

			default:
				NOT_REACHED();
		}
		this->GetWidget<NWidgetCore>(WID_SL_CAPTION)->widget_data = caption_string;

		this->vscroll = this->GetScrollbar(WID_SL_SCROLLBAR);
		this->FinishInitNested(0);

		this->LowerWidget(WID_SL_DRIVES_DIRECTORIES_LIST);
		this->querystrings[WID_SL_FILTER] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;

		/* pause is only used in single-player, non-editor mode, non-menu mode. It
		 * will be unpaused in the WE_DESTROY event handler. */
		if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
			Command<CMD_PAUSE>::Post(PM_PAUSED_SAVELOAD, true);
		}
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

		this->OnInvalidateData(SLIWD_RESCAN_FILES);

		ResetObjectToPlace();

		/* Select the initial directory. */
		o_dir.type = FIOS_TYPE_DIRECT;
		switch (this->abstract_filetype) {
			case FT_SAVEGAME:
				o_dir.name = FioFindDirectory(SAVE_DIR);
				break;

			case FT_SCENARIO:
				o_dir.name = FioFindDirectory(SCENARIO_DIR);
				break;

			case FT_HEIGHTMAP:
				o_dir.name = FioFindDirectory(HEIGHTMAP_DIR);
				break;

			default:
				o_dir.name = _personal_dir;
		}

		switch (this->fop) {
			case SLO_SAVE:
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
			Command<CMD_PAUSE>::Post(PM_PAUSED_SAVELOAD, false);
		}
		this->Window::Close();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SL_SORT_BYNAME:
			case WID_SL_SORT_BYDATE:
				if (((_savegame_sort_order & SORT_BY_NAME) != 0) == (widget == WID_SL_SORT_BYNAME)) {
					this->DrawSortButtonState(widget, _savegame_sort_order & SORT_DESCENDING ? SBS_DOWN : SBS_UP);
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

				if (free_space.has_value()) SetDParam(0, free_space.value());
				DrawString(ir.left, ir.right, ir.top + GetCharacterHeight(FS_NORMAL), free_space.has_value() ? STR_SAVELOAD_BYTES_FREE : STR_ERROR_UNABLE_TO_READ_DRIVE);
				DrawString(ir.left, ir.right, ir.top, path, TC_BLACK);
				break;
			}

			case WID_SL_DRIVES_DIRECTORIES_LIST: {
				const Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
				GfxFillRect(br, PC_BLACK);

				Rect tr = r.Shrink(WidgetDimensions::scaled.inset).WithHeight(this->resize.step_height);
				uint scroll_pos = this->vscroll->GetPosition();
				for (auto it = this->display_list.begin() + scroll_pos; it != this->display_list.end() && tr.top < br.bottom; ++it) {
					const FiosItem *item = *it;

					if (item == this->selected) {
						GfxFillRect(br.left, tr.top, br.right, tr.bottom, PC_DARK_BLUE);
					} else if (item == this->highlighted) {
						GfxFillRect(br.left, tr.top, br.right, tr.bottom, PC_VERY_DARK_BLUE);
					}
					DrawString(tr, item->title, _fios_colours[GetDetailedFileType(item->type)]);
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
		int HEADER_HEIGHT = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.frametext.Vertical();

		Rect hr = r.WithHeight(HEADER_HEIGHT).Shrink(WidgetDimensions::scaled.frametext);
		Rect tr = r.Shrink(WidgetDimensions::scaled.frametext);
		tr.top += HEADER_HEIGHT;

		/* Create the nice grayish rectangle at the details top */
		GfxFillRect(r.WithHeight(HEADER_HEIGHT).Shrink(WidgetDimensions::scaled.bevel.left, WidgetDimensions::scaled.bevel.top, WidgetDimensions::scaled.bevel.right, 0), PC_GREY);
		DrawString(hr.left, hr.right, hr.top, STR_SAVELOAD_DETAIL_CAPTION, TC_FROMSTRING, SA_HOR_CENTER);

		if (this->selected == nullptr) return;

		/* Details panel */
		tr.bottom -= GetCharacterHeight(FS_NORMAL) - 1;
		if (tr.top > tr.bottom) return;

		if (!_load_check_data.checkable) {
			/* Old savegame, no information available */
			DrawString(tr, STR_SAVELOAD_DETAIL_NOT_AVAILABLE);
			tr.top += GetCharacterHeight(FS_NORMAL);
		} else if (_load_check_data.error != INVALID_STRING_ID) {
			/* Incompatible / broken savegame */
			SetDParamStr(0, _load_check_data.error_msg);
			tr.top = DrawStringMultiLine(tr, _load_check_data.error, TC_RED);
		} else {
			/* Mapsize */
			SetDParam(0, _load_check_data.map_size_x);
			SetDParam(1, _load_check_data.map_size_y);
			DrawString(tr, STR_NETWORK_SERVER_LIST_MAP_SIZE);
			tr.top += GetCharacterHeight(FS_NORMAL);
			if (tr.top > tr.bottom) return;

			/* Climate */
			byte landscape = _load_check_data.settings.game_creation.landscape;
			if (landscape < NUM_LANDSCAPE) {
				SetDParam(0, STR_CLIMATE_TEMPERATE_LANDSCAPE + landscape);
				DrawString(tr, STR_NETWORK_SERVER_LIST_LANDSCAPE);
				tr.top += GetCharacterHeight(FS_NORMAL);
			}

			tr.top += WidgetDimensions::scaled.vsep_normal;
			if (tr.top > tr.bottom) return;

			/* Start date (if available) */
			if (_load_check_data.settings.game_creation.starting_year != 0) {
				SetDParam(0, TimerGameCalendar::ConvertYMDToDate(_load_check_data.settings.game_creation.starting_year, 0, 1));
				DrawString(tr, STR_NETWORK_SERVER_LIST_START_DATE);
				tr.top += GetCharacterHeight(FS_NORMAL);
			}
			if (tr.top > tr.bottom) return;

			/* Hide current date for scenarios */
			if (this->abstract_filetype != FT_SCENARIO) {
				/* Current date */
				SetDParam(0, _load_check_data.current_date);
				DrawString(tr, STR_NETWORK_SERVER_LIST_CURRENT_DATE);
				tr.top += GetCharacterHeight(FS_NORMAL);
			}

			/* Hide the NewGRF stuff when saving. We also hide the button. */
			if (this->fop == SLO_LOAD && (this->abstract_filetype == FT_SAVEGAME || this->abstract_filetype == FT_SCENARIO)) {
				tr.top += WidgetDimensions::scaled.vsep_normal;
				if (tr.top > tr.bottom) return;

				/* NewGrf compatibility */
				SetDParam(0, _load_check_data.grfconfig == nullptr ? STR_NEWGRF_LIST_NONE :
						STR_NEWGRF_LIST_ALL_FOUND + _load_check_data.grf_compatibility);
				DrawString(tr, STR_SAVELOAD_DETAIL_GRFSTATUS);
				tr.top += GetCharacterHeight(FS_NORMAL);
			}
			if (tr.top > tr.bottom) return;

			/* Hide the company stuff for scenarios */
			if (this->abstract_filetype != FT_SCENARIO) {
				tr.top += WidgetDimensions::scaled.vsep_wide;
				if (tr.top > tr.bottom) return;

				/* Companies / AIs */
				for (auto &pair : _load_check_data.companies) {
					SetDParam(0, pair.first + 1);
					const CompanyProperties &c = *pair.second;
					if (!c.name.empty()) {
						SetDParam(1, STR_JUST_RAW_STRING);
						SetDParamStr(2, c.name);
					} else {
						SetDParam(1, c.name_1);
						SetDParam(2, c.name_2);
					}
					DrawString(tr, STR_SAVELOAD_DETAIL_COMPANY_INDEX);
					tr.top += GetCharacterHeight(FS_NORMAL);
					if (tr.top > tr.bottom) break;
				}
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_SL_BACKGROUND:
				size->height = 2 * GetCharacterHeight(FS_NORMAL) + padding.height;
				break;

			case WID_SL_DRIVES_DIRECTORIES_LIST:
				resize->height = GetCharacterHeight(FS_NORMAL);
				size->height = resize->height * 10 + padding.height;
				break;
			case WID_SL_SORT_BYNAME:
			case WID_SL_SORT_BYDATE: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
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
			case WID_SL_SORT_BYNAME: // Sort save names by name
				_savegame_sort_order = (_savegame_sort_order == SORT_BY_NAME) ?
					SORT_BY_NAME | SORT_DESCENDING : SORT_BY_NAME;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case WID_SL_SORT_BYDATE: // Sort save names by date
				_savegame_sort_order = (_savegame_sort_order == SORT_BY_DATE) ?
					SORT_BY_DATE | SORT_DESCENDING : SORT_BY_DATE;
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

				if (this->abstract_filetype == FT_HEIGHTMAP) {
					this->Close();
					ShowHeightmapLoad();
				} else if (!_load_check_data.HasNewGrfs() || _load_check_data.grf_compatibility != GLC_NOT_FOUND || _settings_client.gui.UserIsAllowedToChangeNewGRFs()) {
					_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD_GAME;
					ClearErrorMessages();
					this->Close();
				}
				break;
			}

			case WID_SL_NEWGRF_INFO:
				if (_load_check_data.HasNewGrfs()) {
					ShowNewGRFSettings(false, false, false, &_load_check_data.grfconfig);
				}
				break;

			case WID_SL_MISSING_NEWGRFS:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
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

						if (GetDetailedFileType(file->type) == DFT_GAME_FILE) {
							/* Other detailed file types cannot be checked before. */
							SaveOrLoad(file->name, SLO_CHECK, DFT_GAME_FILE, NO_DIRECTORY, false);
						}

						this->InvalidateData(SLIWD_SELECTION_CHANGES);
					}
					if (this->fop == SLO_SAVE) {
						/* Copy clicked name to editbox */
						this->filename_editbox.text.Assign(file->title);
						this->SetWidgetDirty(WID_SL_SAVE_OSK_TITLE);
					}
				} else if (!_load_check_data.HasErrors()) {
					this->selected = file;
					if (this->fop == SLO_LOAD) {
						if (this->abstract_filetype == FT_SAVEGAME || this->abstract_filetype == FT_SCENARIO) {
							this->OnClick(pt, WID_SL_LOAD_BUTTON, 1);
						} else {
							assert(this->abstract_filetype == FT_HEIGHTMAP);
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
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
					assert(this->fop == SLO_LOAD);
					switch (this->abstract_filetype) {
						default: NOT_REACHED();
						case FT_SCENARIO:  ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_SCENARIO);  break;
						case FT_HEIGHTMAP: ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_HEIGHTMAP); break;
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
		if (this->fop != SLO_SAVE) return;

		if (this->IsWidgetLowered(WID_SL_DELETE_SELECTION)) { // Delete button clicked
			if (!FiosDelete(this->filename_editbox.text.buf)) {
				ShowErrorMessage(STR_ERROR_UNABLE_TO_DELETE_FILE, INVALID_STRING_ID, WL_ERROR);
			} else {
				this->InvalidateData(SLIWD_RESCAN_FILES);
				/* Reset file name to current date on successful delete */
				if (this->abstract_filetype == FT_SAVEGAME) GenerateFileName();
			}
		} else if (this->IsWidgetLowered(WID_SL_SAVE_GAME)) { // Save button clicked
			if (this->abstract_filetype == FT_SAVEGAME || this->abstract_filetype == FT_SCENARIO) {
				_file_to_saveload.name = FiosMakeSavegameName(this->filename_editbox.text.buf);
				if (FioCheckFileExists(_file_to_saveload.name, Subdirectory::SAVE_DIR)) {
					ShowQuery(STR_SAVELOAD_OVERWRITE_TITLE, STR_SAVELOAD_OVERWRITE_WARNING, this, SaveLoadWindow::SaveGameConfirmationCallback);
				} else {
					_switch_mode = SM_SAVE_GAME;
				}
			} else {
				_file_to_saveload.name = FiosMakeHeightmapName(this->filename_editbox.text.buf);
				if (FioCheckFileExists(_file_to_saveload.name, Subdirectory::SAVE_DIR)) {
					ShowQuery(STR_SAVELOAD_OVERWRITE_TITLE, STR_SAVELOAD_OVERWRITE_WARNING, this, SaveLoadWindow::SaveHeightmapConfirmationCallback);
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
				this->string_filter.AddLine(it.title);
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
				this->fios_items.BuildFileList(this->abstract_filetype, this->fop);
				this->selected = nullptr;
				_load_check_data.Clear();

				/* We reset the files filtered */
				this->OnInvalidateData(SLIWD_FILTER_CHANGES);

				FALLTHROUGH;

			case SLIWD_SELECTION_CHANGES:
				/* Selection changes */
				if (!gui_scope) break;

				if (this->fop != SLO_LOAD) break;

				switch (this->abstract_filetype) {
					case FT_HEIGHTMAP:
						this->SetWidgetDisabledState(WID_SL_LOAD_BUTTON, this->selected == nullptr || _load_check_data.HasErrors());
						break;

					case FT_SAVEGAME:
					case FT_SCENARIO: {
						bool disabled = this->selected == nullptr || _load_check_data.HasErrors();
						if (!_settings_client.gui.UserIsAllowedToChangeNewGRFs()) {
							disabled |= _load_check_data.HasNewGrfs() && _load_check_data.grf_compatibility == GLC_NOT_FOUND;
						}
						this->SetWidgetDisabledState(WID_SL_LOAD_BUTTON, disabled);
						this->SetWidgetDisabledState(WID_SL_NEWGRF_INFO, !_load_check_data.HasNewGrfs());
						this->SetWidgetDisabledState(WID_SL_MISSING_NEWGRFS,
								!_load_check_data.HasNewGrfs() || _load_check_data.grf_compatibility == GLC_ALL_GOOD);
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
			this->string_filter.SetFilterTerm(this->filter_editbox.text.buf);
			this->InvalidateData(SLIWD_FILTER_CHANGES);
		}
	}
};

/** Load game/scenario */
static WindowDesc _load_dialog_desc(__FILE__, __LINE__,
	WDP_CENTER, "load_game", 500, 294,
	WC_SAVELOAD, WC_NONE,
	0,
	std::begin(_nested_load_dialog_widgets), std::end(_nested_load_dialog_widgets)
);

/** Load heightmap */
static WindowDesc _load_heightmap_dialog_desc(__FILE__, __LINE__,
	WDP_CENTER, "load_heightmap", 257, 320,
	WC_SAVELOAD, WC_NONE,
	0,
	std::begin(_nested_load_heightmap_dialog_widgets), std::end(_nested_load_heightmap_dialog_widgets)
);

/** Save game/scenario */
static WindowDesc _save_dialog_desc(__FILE__, __LINE__,
	WDP_CENTER, "save_game", 500, 294,
	WC_SAVELOAD, WC_NONE,
	0,
	std::begin(_nested_save_dialog_widgets), std::end(_nested_save_dialog_widgets)
);

/**
 * Launch save/load dialog in the given mode.
 * @param abstract_filetype Kind of file to handle.
 * @param fop File operation to perform (load or save).
 */
void ShowSaveLoadDialog(AbstractFileType abstract_filetype, SaveLoadOperation fop)
{
	CloseWindowById(WC_SAVELOAD, 0);

	WindowDesc *sld;
	if (fop == SLO_SAVE) {
		sld = &_save_dialog_desc;
	} else {
		/* Dialogue for loading a file. */
		sld = (abstract_filetype == FT_HEIGHTMAP) ? &_load_heightmap_dialog_desc : &_load_dialog_desc;
	}

	new SaveLoadWindow(sld, abstract_filetype, fop);
}
