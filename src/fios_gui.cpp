/* $Id$ */

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
#include "date_func.h"
#include "core/geometry_func.hpp"
#include "gamelog.h"

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
	free(this->error_data);
	this->error_data = NULL;

	this->map_size_x = this->map_size_y = 256; // Default for old savegames which do not store mapsize.
	this->current_date = 0;
	memset(&this->settings, 0, sizeof(this->settings));

	const CompanyPropertiesMap::iterator end = this->companies.End();
	for (CompanyPropertiesMap::iterator it = this->companies.Begin(); it != end; it++) {
		delete it->second;
	}
	companies.Clear();

	GamelogFree(this->gamelog_action, this->gamelog_actions);
	this->gamelog_action = NULL;
	this->gamelog_actions = 0;

	ClearGRFConfigList(&this->grfconfig);
}

/** Load game/scenario with optional content download */
static const NWidgetPart _nested_load_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SL_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_FILE_BACKGROUND),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_INSET, COLOUR_GREY, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 1, 2, 2),
							SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SL_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SL_CONTENT_DOWNLOAD_SEL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_CONTENT_DOWNLOAD), SetResize(1, 0),
							SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SL_DETAILS), SetResize(1, 1), SetFill(1, 1),
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
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SL_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_FILE_BACKGROUND),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_INSET, COLOUR_GREY, WID_SL_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 1, 2, 2),
						SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SL_SCROLLBAR),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_CONTENT_DOWNLOAD), SetResize(1, 0), SetFill(1, 0),
						SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_LOAD_BUTTON), SetResize(1, 0), SetFill(1, 0),
						SetDataTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_HEIGHTMAP_TOOLTIP),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Save game/scenario */
static const NWidgetPart _nested_save_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SL_CAPTION),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_SL_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_SL_FILE_BACKGROUND),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_INSET, COLOUR_GREY, WID_SL_DRIVES_DIRECTORIES_LIST), SetPadding(2, 1, 0, 2),
							SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(WID_SL_SCROLLBAR), EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SL_SCROLLBAR),
				EndContainer(),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SL_SAVE_OSK_TITLE), SetPadding(3, 2, 2, 2), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_SAVELOAD_OSKTITLE, STR_SAVELOAD_EDITBOX_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_DELETE_SELECTION), SetDataTip(STR_SAVELOAD_DELETE_BUTTON, STR_SAVELOAD_DELETE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SL_SAVE_GAME),        SetDataTip(STR_SAVELOAD_SAVE_BUTTON, STR_SAVELOAD_SAVE_TOOLTIP),     SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SL_DETAILS), SetResize(1, 1), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetResize(1, 0), SetFill(1, 1),
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
 * @param [inout] file_list List of save game files found in the directory.
 */
static void SortSaveGameList(FileList &file_list)
{
	uint sort_start = 0;
	uint sort_end = 0;

	/* Directories are always above the files (FIOS_TYPE_DIR)
	 * Drives (A:\ (windows only) are always under the files (FIOS_TYPE_DRIVE)
	 * Only sort savegames/scenarios, not directories
	 */
	for (const FiosItem *item = file_list.Begin(); item != file_list.End(); item++) {
		switch (item->type) {
			case FIOS_TYPE_DIR:    sort_start++; break;
			case FIOS_TYPE_PARENT: sort_start++; break;
			case FIOS_TYPE_DRIVE:  sort_end++;   break;
			default: break;
		}
	}

	uint s_amount = file_list.Length() - sort_start - sort_end;
	QSortT(file_list.Get(sort_start), s_amount, CompareFiosItems);
}

struct SaveLoadWindow : public Window {
private:
	QueryString filename_editbox; ///< Filename editbox.
	AbstractFileType abstract_filetype; /// Type of file to select.
	SaveLoadOperation fop;        ///< File operation to perform.
	FileList fios_items;          ///< Save game list.
	FiosItem o_dir;
	const FiosItem *selected;     ///< Selected game in #fios_items, or \c NULL.
	Scrollbar *vscroll;
public:

	/** Generate a default save filename. */
	void GenerateFileName()
	{
		GenerateDefaultSaveName(this->filename_editbox.text.buf, &this->filename_editbox.text.buf[this->filename_editbox.text.max_bytes - 1]);
		this->filename_editbox.text.UpdateSize();
	}

	SaveLoadWindow(WindowDesc *desc, AbstractFileType abstract_filetype, SaveLoadOperation fop)
			: Window(desc), filename_editbox(64), abstract_filetype(abstract_filetype), fop(fop)
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

		this->CreateNestedTree(true);
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

		/* pause is only used in single-player, non-editor mode, non-menu mode. It
		 * will be unpaused in the WE_DESTROY event handler. */
		if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
			DoCommandP(0, PM_PAUSED_SAVELOAD, 1, CMD_PAUSE);
		}
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

		this->OnInvalidateData(0);

		ResetObjectToPlace();

		/* Select the initial directory. */
		o_dir.type = FIOS_TYPE_DIRECT;
		switch (this->abstract_filetype) {
			case FT_SAVEGAME:
				FioGetDirectory(o_dir.name, lastof(o_dir.name), SAVE_DIR);
				break;

			case FT_SCENARIO:
				FioGetDirectory(o_dir.name, lastof(o_dir.name), SCENARIO_DIR);
				break;

			case FT_HEIGHTMAP:
				FioGetDirectory(o_dir.name, lastof(o_dir.name), HEIGHTMAP_DIR);
				break;

			default:
				strecpy(o_dir.name, _personal_dir, lastof(o_dir.name));
		}

		/* Focus the edit box by default in the save windows */
		if (this->fop == SLO_SAVE) this->SetFocusedWidget(WID_SL_SAVE_OSK_TITLE);
	}

	virtual ~SaveLoadWindow()
	{
		/* pause is only used in single-player, non-editor mode, non menu mode */
		if (!_networking && _game_mode != GM_EDITOR && _game_mode != GM_MENU) {
			DoCommandP(0, PM_PAUSED_SAVELOAD, 0, CMD_PAUSE);
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_SL_SORT_BYNAME:
			case WID_SL_SORT_BYDATE:
				if (((_savegame_sort_order & SORT_BY_NAME) != 0) == (widget == WID_SL_SORT_BYNAME)) {
					this->DrawSortButtonState(widget, _savegame_sort_order & SORT_DESCENDING ? SBS_DOWN : SBS_UP);
				}
				break;

			case WID_SL_BACKGROUND: {
				static const char *path = NULL;
				static StringID str = STR_ERROR_UNABLE_TO_READ_DRIVE;
				static uint64 tot = 0;

				if (_fios_path_changed) {
					str = FiosGetDescText(&path, &tot);
					_fios_path_changed = false;
				}

				if (str != STR_ERROR_UNABLE_TO_READ_DRIVE) SetDParam(0, tot);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP, str);
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, path, TC_BLACK);
				break;
			}

			case WID_SL_DRIVES_DIRECTORIES_LIST: {
				GfxFillRect(r.left + 1, r.top + 1, r.right, r.bottom, PC_BLACK);

				uint y = r.top + WD_FRAMERECT_TOP;
				for (uint pos = this->vscroll->GetPosition(); pos < this->fios_items.Length(); pos++) {
					const FiosItem *item = this->fios_items.Get(pos);

					if (item == this->selected) {
						GfxFillRect(r.left + 1, y, r.right, y + this->resize.step_height, PC_DARK_BLUE);
					}
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, item->title, _fios_colours[GetDetailedFileType(item->type)]);
					y += this->resize.step_height;
					if (y >= this->vscroll->GetCapacity() * this->resize.step_height + r.top + WD_FRAMERECT_TOP) break;
				}
				break;
			}

			case WID_SL_DETAILS: {
				GfxFillRect(r.left + WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP,
						r.right - WD_FRAMERECT_RIGHT, r.top + FONT_HEIGHT_NORMAL * 2 + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM, PC_GREY);
				DrawString(r.left, r.right, r.top + FONT_HEIGHT_NORMAL / 2 + WD_FRAMERECT_TOP, STR_SAVELOAD_DETAIL_CAPTION, TC_FROMSTRING, SA_HOR_CENTER);

				if (this->selected == NULL) break;

				uint y = r.top + FONT_HEIGHT_NORMAL * 2 + WD_PAR_VSEP_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				uint y_max = r.bottom - FONT_HEIGHT_NORMAL - WD_FRAMERECT_BOTTOM;

				if (y > y_max) break;
				if (!_load_check_data.checkable) {
					/* Old savegame, no information available */
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SAVELOAD_DETAIL_NOT_AVAILABLE);
					y += FONT_HEIGHT_NORMAL;
				} else if (_load_check_data.error != INVALID_STRING_ID) {
					/* Incompatible / broken savegame */
					SetDParamStr(0, _load_check_data.error_data);
					y = DrawStringMultiLine(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT,
							y, r.bottom - WD_FRAMERECT_BOTTOM, _load_check_data.error, TC_RED);
				} else {
					/* Mapsize */
					SetDParam(0, _load_check_data.map_size_x);
					SetDParam(1, _load_check_data.map_size_y);
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_MAP_SIZE);
					y += FONT_HEIGHT_NORMAL;
					if (y > y_max) break;

					/* Climate */
					byte landscape = _load_check_data.settings.game_creation.landscape;
					if (landscape < NUM_LANDSCAPE) {
						SetDParam(0, STR_CHEAT_SWITCH_CLIMATE_TEMPERATE_LANDSCAPE + landscape);
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_LANDSCAPE);
						y += FONT_HEIGHT_NORMAL;
					}

					y += WD_PAR_VSEP_NORMAL;
					if (y > y_max) break;

					/* Start date (if available) */
					if (_load_check_data.settings.game_creation.starting_year != 0) {
						SetDParam(0, ConvertYMDToDate(_load_check_data.settings.game_creation.starting_year, 0, 1));
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_START_DATE);
						y += FONT_HEIGHT_NORMAL;
					}
					if (y > y_max) break;

					/* Hide current date for scenarios */
					if (this->abstract_filetype != FT_SCENARIO) {
						/* Current date */
						SetDParam(0, _load_check_data.current_date);
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_CURRENT_DATE);
						y += FONT_HEIGHT_NORMAL;
					}

					/* Hide the NewGRF stuff when saving. We also hide the button. */
					if (this->fop == SLO_LOAD && (this->abstract_filetype == FT_SAVEGAME || this->abstract_filetype == FT_SCENARIO)) {
						y += WD_PAR_VSEP_NORMAL;
						if (y > y_max) break;

						/* NewGrf compatibility */
						SetDParam(0, _load_check_data.grfconfig == NULL ? STR_NEWGRF_LIST_NONE :
								STR_NEWGRF_LIST_ALL_FOUND + _load_check_data.grf_compatibility);
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SAVELOAD_DETAIL_GRFSTATUS);
						y += FONT_HEIGHT_NORMAL;
					}
					if (y > y_max) break;

					/* Hide the company stuff for scenarios */
					if (this->abstract_filetype != FT_SCENARIO) {
						y += FONT_HEIGHT_NORMAL;
						if (y > y_max) break;

						/* Companies / AIs */
						CompanyPropertiesMap::const_iterator end = _load_check_data.companies.End();
						for (CompanyPropertiesMap::const_iterator it = _load_check_data.companies.Begin(); it != end; it++) {
							SetDParam(0, it->first + 1);
							const CompanyProperties &c = *it->second;
							if (c.name != NULL) {
								SetDParam(1, STR_JUST_RAW_STRING);
								SetDParamStr(2, c.name);
							} else {
								SetDParam(1, c.name_1);
								SetDParam(2, c.name_2);
							}
							DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_SAVELOAD_DETAIL_COMPANY_INDEX);
							y += FONT_HEIGHT_NORMAL;
							if (y > y_max) break;
						}
					}
				}
				break;
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_SL_BACKGROUND:
				size->height = 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case WID_SL_DRIVES_DIRECTORIES_LIST:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = resize->height * 10 + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
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

	virtual void OnPaint()
	{
		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			SortSaveGameList(this->fios_items);
		}

		this->vscroll->SetCount(this->fios_items.Length());
		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
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
				this->InvalidateData();
				break;

			case WID_SL_LOAD_BUTTON:
				if (this->selected != NULL && !_load_check_data.HasErrors()) {
					const char *name = FiosBrowseTo(this->selected);
					_file_to_saveload.SetMode(this->selected->type);
					_file_to_saveload.SetName(name);
					_file_to_saveload.SetTitle(this->selected->title);

					if (this->abstract_filetype == FT_HEIGHTMAP) {
						delete this;
						ShowHeightmapLoad();

					} else if (!_load_check_data.HasNewGrfs() || _load_check_data.grf_compatibility != GLC_NOT_FOUND || _settings_client.gui.UserIsAllowedToChangeNewGRFs()) {
						_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD_GAME;
						ClearErrorMessages();
						delete this;
					}
				}
				break;

			case WID_SL_NEWGRF_INFO:
				if (_load_check_data.HasNewGrfs()) {
					ShowNewGRFSettings(false, false, false, &_load_check_data.grfconfig);
				}
				break;

			case WID_SL_MISSING_NEWGRFS:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else if (_load_check_data.HasNewGrfs()) {
#if defined(ENABLE_NETWORK)
					ShowMissingContentWindow(_load_check_data.grfconfig);
#endif
				}
				break;

			case WID_SL_DRIVES_DIRECTORIES_LIST: { // Click the listbox
				int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_SL_DRIVES_DIRECTORIES_LIST, WD_FRAMERECT_TOP);
				if (y == INT_MAX) return;

				const FiosItem *file = this->fios_items.Get(y);

				const char *name = FiosBrowseTo(file);
				if (name != NULL) {
					if (click_count == 1) {
						if (this->selected != file) {
							this->selected = file;
							_load_check_data.Clear();

							if (GetDetailedFileType(file->type) == DFT_GAME_FILE) {
								/* Other detailed file types cannot be checked before. */
								SaveOrLoad(name, SLO_CHECK, DFT_GAME_FILE, NO_DIRECTORY, false);
							}

							this->InvalidateData(1);
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
								_file_to_saveload.SetMode(file->type);
								_file_to_saveload.SetName(name);
								_file_to_saveload.SetTitle(file->title);

								delete this;
								ShowHeightmapLoad();
							}
						}
					}
				} else {
					/* Changed directory, need refresh. */
					this->InvalidateData();
				}
				break;
			}

			case WID_SL_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
#if defined(ENABLE_NETWORK)
					assert(this->fop == SLO_LOAD);
					switch (this->abstract_filetype) {
						default: NOT_REACHED();
						case FT_SCENARIO:  ShowNetworkContentListWindow(NULL, CONTENT_TYPE_SCENARIO);  break;
						case FT_HEIGHTMAP: ShowNetworkContentListWindow(NULL, CONTENT_TYPE_HEIGHTMAP); break;
					}
#endif
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

	virtual EventState OnKeyPress(WChar key, uint16 keycode)
	{
		if (keycode == WKC_ESC) {
			delete this;
			return ES_HANDLED;
		}

		return ES_NOT_HANDLED;
	}

	virtual void OnTimeout()
	{
		/* Widgets WID_SL_DELETE_SELECTION and WID_SL_SAVE_GAME only exist when saving to a file. */
		if (this->fop != SLO_SAVE) return;

		if (this->IsWidgetLowered(WID_SL_DELETE_SELECTION)) { // Delete button clicked
			if (!FiosDelete(this->filename_editbox.text.buf)) {
				ShowErrorMessage(STR_ERROR_UNABLE_TO_DELETE_FILE, INVALID_STRING_ID, WL_ERROR);
			} else {
				this->InvalidateData();
				/* Reset file name to current date on successful delete */
				if (this->abstract_filetype == FT_SAVEGAME) GenerateFileName();
			}
		} else if (this->IsWidgetLowered(WID_SL_SAVE_GAME)) { // Save button clicked
			if (this->abstract_filetype == FT_SAVEGAME || this->abstract_filetype == FT_SCENARIO) {
				_switch_mode = SM_SAVE_GAME;
				FiosMakeSavegameName(_file_to_saveload.name, this->filename_editbox.text.buf, lastof(_file_to_saveload.name));
			} else {
				_switch_mode = SM_SAVE_HEIGHTMAP;
				FiosMakeHeightmapName(_file_to_saveload.name, this->filename_editbox.text.buf, lastof(_file_to_saveload.name));
			}

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SL_DRIVES_DIRECTORIES_LIST);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		switch (data) {
			case 0:
				/* Rescan files */
				this->selected = NULL;
				_load_check_data.Clear();
				if (!gui_scope) break;

				_fios_path_changed = true;
				this->fios_items.BuildFileList(this->abstract_filetype, this->fop);
				this->vscroll->SetCount(this->fios_items.Length());
				this->selected = NULL;
				_load_check_data.Clear();
				FALLTHROUGH;

			case 1:
				/* Selection changes */
				if (!gui_scope) break;

				if (this->fop != SLO_LOAD) break;

				switch (this->abstract_filetype) {
					case FT_HEIGHTMAP:
						this->SetWidgetDisabledState(WID_SL_LOAD_BUTTON, this->selected == NULL || _load_check_data.HasErrors());
						break;

					case FT_SAVEGAME:
					case FT_SCENARIO: {
						bool disabled = this->selected == NULL || _load_check_data.HasErrors();
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
		}
	}
};

/** Load game/scenario */
static WindowDesc _load_dialog_desc(
	WDP_CENTER, "load_game", 500, 294,
	WC_SAVELOAD, WC_NONE,
	0,
	_nested_load_dialog_widgets, lengthof(_nested_load_dialog_widgets)
);

/** Load heightmap */
static WindowDesc _load_heightmap_dialog_desc(
	WDP_CENTER, "load_heightmap", 257, 320,
	WC_SAVELOAD, WC_NONE,
	0,
	_nested_load_heightmap_dialog_widgets, lengthof(_nested_load_heightmap_dialog_widgets)
);

/** Save game/scenario */
static WindowDesc _save_dialog_desc(
	WDP_CENTER, "save_game", 500, 294,
	WC_SAVELOAD, WC_NONE,
	0,
	_nested_save_dialog_widgets, lengthof(_nested_save_dialog_widgets)
);

/**
 * Launch save/load dialog in the given mode.
 * @param abstract_filetype Kind of file to handle.
 * @param fop File operation to perform (load or save).
 */
void ShowSaveLoadDialog(AbstractFileType abstract_filetype, SaveLoadOperation fop)
{
	DeleteWindowById(WC_SAVELOAD, 0);

	WindowDesc *sld;
	if (fop == SLO_SAVE) {
		sld = &_save_dialog_desc;
	} else {
		/* Dialogue for loading a file. */
		sld = (abstract_filetype == FT_HEIGHTMAP) ? &_load_heightmap_dialog_desc : &_load_dialog_desc;
	}

	_file_to_saveload.abstract_ftype = abstract_filetype;

	new SaveLoadWindow(sld, abstract_filetype, fop);
}
