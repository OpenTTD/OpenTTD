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

#include "table/sprites.h"
#include "table/strings.h"

SaveLoadDialogMode _saveload_mode;
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

	ClearGRFConfigList(&this->grfconfig);
}


enum SaveLoadWindowWidgets {
	SLWW_WINDOWTITLE,
	SLWW_SORT_BYNAME,
	SLWW_SORT_BYDATE,
	SLWW_BACKGROUND,
	SLWW_FILE_BACKGROUND,
	SLWW_HOME_BUTTON,
	SLWW_DRIVES_DIRECTORIES_LIST,
	SLWW_SCROLLBAR,
	SLWW_CONTENT_DOWNLOAD,     ///< only available for play scenario/heightmap (content download)
	SLWW_SAVE_OSK_TITLE,       ///< only available for save operations
	SLWW_DELETE_SELECTION,     ///< same in here
	SLWW_SAVE_GAME,            ///< not to mention in here too
	SLWW_CONTENT_DOWNLOAD_SEL, ///< Selection 'stack' to 'hide' the content download
	SLWW_DETAILS,              ///< Panel with game details
	SLWW_NEWGRF_INFO,          ///< Button to open NewGgrf configuration
	SLWW_LOAD_BUTTON,          ///< Button to load game/scenario
};

/** Load game/scenario with optional content download */
static const NWidgetPart _nested_load_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLWW_WINDOWTITLE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SLWW_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, SLWW_FILE_BACKGROUND),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_INSET, COLOUR_GREY, SLWW_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 1, 2, 2),
							SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(SLWW_SCROLLBAR), EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, SLWW_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, SLWW_CONTENT_DOWNLOAD_SEL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_CONTENT_DOWNLOAD), SetResize(1, 0),
							SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_EMPTY, INVALID_COLOUR, SLWW_DETAILS), SetResize(1, 1), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_NEWGRF_INFO), SetDataTip(STR_INTRO_NEWGRF_SETTINGS, STR_NULL), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_LOAD_BUTTON), SetDataTip(STR_SAVELOAD_LOAD_BUTTON, STR_SAVELOAD_LOAD_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
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
		NWidget(WWT_CAPTION, COLOUR_GREY, SLWW_WINDOWTITLE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SLWW_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, SLWW_FILE_BACKGROUND),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_INSET, COLOUR_GREY, SLWW_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 1, 2, 2),
						SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(SLWW_SCROLLBAR), EndContainer(),
				NWidget(NWID_VSCROLLBAR, COLOUR_GREY, SLWW_SCROLLBAR),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_CONTENT_DOWNLOAD), SetResize(1, 0),
						SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Save game/scenario */
static const NWidgetPart _nested_save_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLWW_WINDOWTITLE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
				EndContainer(),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SLWW_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, SLWW_FILE_BACKGROUND),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_INSET, COLOUR_GREY, SLWW_DRIVES_DIRECTORIES_LIST), SetPadding(2, 1, 0, 2),
							SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), SetScrollbar(SLWW_SCROLLBAR), EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, SLWW_SCROLLBAR),
				EndContainer(),
				NWidget(WWT_EDITBOX, COLOUR_GREY, SLWW_SAVE_OSK_TITLE), SetPadding(3, 2, 2, 2), SetFill(1, 0), SetResize(1, 0),
						SetDataTip(STR_SAVELOAD_OSKTITLE, STR_SAVELOAD_EDITBOX_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_DELETE_SELECTION), SetDataTip(STR_SAVELOAD_DELETE_BUTTON, STR_SAVELOAD_DELETE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SAVE_GAME),        SetDataTip(STR_SAVELOAD_SAVE_BUTTON, STR_SAVELOAD_SAVE_TOOLTIP),     SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_EMPTY, INVALID_COLOUR, SLWW_DETAILS), SetResize(1, 1), SetFill(1, 1),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetResize(1, 0), SetFill(1, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/* Colours for fios types */
const TextColour _fios_colours[] = {
	TC_LIGHT_BLUE, TC_DARK_GREEN,  TC_DARK_GREEN, TC_ORANGE, TC_LIGHT_BROWN,
	TC_ORANGE,     TC_LIGHT_BROWN, TC_ORANGE,     TC_ORANGE, TC_YELLOW
};

void BuildFileList()
{
	_fios_path_changed = true;
	FiosFreeSavegameList();

	switch (_saveload_mode) {
		case SLD_NEW_GAME:
		case SLD_LOAD_SCENARIO:
		case SLD_SAVE_SCENARIO:
			FiosGetScenarioList(_saveload_mode); break;
		case SLD_LOAD_HEIGHTMAP:
			FiosGetHeightmapList(_saveload_mode); break;

		default: FiosGetSavegameList(_saveload_mode); break;
	}
}

static void MakeSortedSaveGameList()
{
	uint sort_start = 0;
	uint sort_end = 0;

	/* Directories are always above the files (FIOS_TYPE_DIR)
	 * Drives (A:\ (windows only) are always under the files (FIOS_TYPE_DRIVE)
	 * Only sort savegames/scenarios, not directories
	 */
	for (const FiosItem *item = _fios_items.Begin(); item != _fios_items.End(); item++) {
		switch (item->type) {
			case FIOS_TYPE_DIR:    sort_start++; break;
			case FIOS_TYPE_PARENT: sort_start++; break;
			case FIOS_TYPE_DRIVE:  sort_end++;   break;
			default: break;
		}
	}

	uint s_amount = _fios_items.Length() - sort_start - sort_end;
	QSortT(_fios_items.Get(sort_start), s_amount, CompareFiosItems);
}

struct SaveLoadWindow : public QueryStringBaseWindow {
private:
	FiosItem o_dir;
	const FiosItem *selected;
	Scrollbar *vscroll;
public:

	void GenerateFileName()
	{
		GenerateDefaultSaveName(this->edit_str_buf, &this->edit_str_buf[this->edit_str_size - 1]);
	}

	SaveLoadWindow(const WindowDesc *desc, SaveLoadDialogMode mode) : QueryStringBaseWindow(64)
	{
		static const StringID saveload_captions[] = {
			STR_SAVELOAD_LOAD_CAPTION,
			STR_SAVELOAD_LOAD_SCENARIO,
			STR_SAVELOAD_SAVE_CAPTION,
			STR_SAVELOAD_SAVE_SCENARIO,
			STR_SAVELOAD_LOAD_HEIGHTMAP,
		};
		assert((uint)mode < lengthof(saveload_captions));

		/* Use an array to define what will be the current file type being handled
		 * by current file mode */
		switch (mode) {
			case SLD_SAVE_GAME:     this->GenerateFileName(); break;
			case SLD_SAVE_SCENARIO: strecpy(this->edit_str_buf, "UNNAMED", &this->edit_str_buf[edit_str_size - 1]); break;
			default:                break;
		}

		this->afilter = CS_ALPHANUMERAL;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, 240);

		this->CreateNestedTree(desc, true);
		if (mode == SLD_LOAD_GAME) this->GetWidget<NWidgetStacked>(SLWW_CONTENT_DOWNLOAD_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		this->GetWidget<NWidgetCore>(SLWW_WINDOWTITLE)->widget_data = saveload_captions[mode];
		this->vscroll = this->GetScrollbar(SLWW_SCROLLBAR);

		this->FinishInitNested(desc, 0);

		this->LowerWidget(SLWW_DRIVES_DIRECTORIES_LIST);

		/* pause is only used in single-player, non-editor mode, non-menu mode. It
		 * will be unpaused in the WE_DESTROY event handler. */
		if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
			DoCommandP(0, PM_PAUSED_SAVELOAD, 1, CMD_PAUSE);
		}
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

		this->OnInvalidateData(0);

		ResetObjectToPlace();

		o_dir.type = FIOS_TYPE_DIRECT;
		switch (_saveload_mode) {
			case SLD_SAVE_GAME:
			case SLD_LOAD_GAME:
				FioGetDirectory(o_dir.name, lengthof(o_dir.name), SAVE_DIR);
				break;

			case SLD_SAVE_SCENARIO:
			case SLD_LOAD_SCENARIO:
				FioGetDirectory(o_dir.name, lengthof(o_dir.name), SCENARIO_DIR);
				break;

			case SLD_LOAD_HEIGHTMAP:
				FioGetDirectory(o_dir.name, lengthof(o_dir.name), HEIGHTMAP_DIR);
				break;

			default:
				strecpy(o_dir.name, _personal_dir, lastof(o_dir.name));
		}

		/* Focus the edit box by default in the save windows */
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			this->SetFocusedWidget(SLWW_SAVE_OSK_TITLE);
		}
	}

	virtual ~SaveLoadWindow()
	{
		/* pause is only used in single-player, non-editor mode, non menu mode */
		if (!_networking && _game_mode != GM_EDITOR && _game_mode != GM_MENU) {
			DoCommandP(0, PM_PAUSED_SAVELOAD, 0, CMD_PAUSE);
		}
		FiosFreeSavegameList();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SLWW_SORT_BYNAME:
			case SLWW_SORT_BYDATE:
				if (((_savegame_sort_order & SORT_BY_NAME) != 0) == (widget == SLWW_SORT_BYNAME)) {
					this->DrawSortButtonState(widget, _savegame_sort_order & SORT_DESCENDING ? SBS_DOWN : SBS_UP);
				}
				break;

			case SLWW_BACKGROUND: {
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

			case SLWW_DRIVES_DIRECTORIES_LIST: {
				GfxFillRect(r.left + 1, r.top + 1, r.right, r.bottom, 0xD7);

				uint y = r.top + WD_FRAMERECT_TOP;
				for (uint pos = this->vscroll->GetPosition(); pos < _fios_items.Length(); pos++) {
					const FiosItem *item = _fios_items.Get(pos);

					if (item == this->selected) {
						GfxFillRect(r.left + 1, y, r.right, y + this->resize.step_height, 156);
					}
					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, item->title, _fios_colours[item->type]);
					y += this->resize.step_height;
					if (y >= this->vscroll->GetCapacity() * this->resize.step_height + r.top + WD_FRAMERECT_TOP) break;
				}
				break;
			}

			case SLWW_DETAILS: {
				GfxFillRect(r.left + WD_FRAMERECT_LEFT, r.top + WD_FRAMERECT_TOP,
						r.right - WD_FRAMERECT_RIGHT, r.top + FONT_HEIGHT_NORMAL * 2 + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM, 0x0A);
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
					if (_saveload_mode != SLD_LOAD_SCENARIO && _saveload_mode != SLD_SAVE_SCENARIO) {
						/* Current date */
						SetDParam(0, _load_check_data.current_date);
						DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_NETWORK_SERVER_LIST_CURRENT_DATE);
						y += FONT_HEIGHT_NORMAL;
					}

					/* Hide the NewGRF stuff when saving. We also hide the button. */
					if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
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
					if (_saveload_mode != SLD_LOAD_SCENARIO && _saveload_mode != SLD_SAVE_SCENARIO) {
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
			case SLWW_BACKGROUND:
				size->height = 2 * FONT_HEIGHT_NORMAL + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case SLWW_DRIVES_DIRECTORIES_LIST:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = resize->height * 10 + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
			case SLWW_SORT_BYNAME:
			case SLWW_SORT_BYDATE: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + WD_SORTBUTTON_ARROW_WIDTH * 2; // Doubled since the string is centred and it also looks better.
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
			MakeSortedSaveGameList();
		}

		this->vscroll->SetCount(_fios_items.Length());
		this->DrawWidgets();

		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			this->DrawEditBox(SLWW_SAVE_OSK_TITLE);
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case SLWW_SORT_BYNAME: // Sort save names by name
				_savegame_sort_order = (_savegame_sort_order == SORT_BY_NAME) ?
					SORT_BY_NAME | SORT_DESCENDING : SORT_BY_NAME;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case SLWW_SORT_BYDATE: // Sort save names by date
				_savegame_sort_order = (_savegame_sort_order == SORT_BY_DATE) ?
					SORT_BY_DATE | SORT_DESCENDING : SORT_BY_DATE;
				_savegame_sort_dirty = true;
				this->SetDirty();
				break;

			case SLWW_HOME_BUTTON: // OpenTTD 'button', jumps to OpenTTD directory
				FiosBrowseTo(&o_dir);
				this->InvalidateData();
				break;

			case SLWW_LOAD_BUTTON:
				if (this->selected != NULL && !_load_check_data.HasErrors() && (_load_check_data.grf_compatibility != GLC_NOT_FOUND || _settings_client.gui.UserIsAllowedToChangeNewGRFs())) {
					_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD;

					const char *name = FiosBrowseTo(this->selected);
					SetFiosType(this->selected->type);

					strecpy(_file_to_saveload.name, name, lastof(_file_to_saveload.name));
					strecpy(_file_to_saveload.title, this->selected->title, lastof(_file_to_saveload.title));

					delete this;
				}
				break;

			case SLWW_NEWGRF_INFO:
				if (_load_check_data.HasNewGrfs()) {
					ShowNewGRFSettings(false, false, false, &_load_check_data.grfconfig);
				}
				break;

			case SLWW_DRIVES_DIRECTORIES_LIST: { // Click the listbox
				int y = this->vscroll->GetScrolledRowFromWidget(pt.y, this, SLWW_DRIVES_DIRECTORIES_LIST, WD_FRAMERECT_TOP);
				if (y == INT_MAX) return;

				const FiosItem *file = _fios_items.Get(y);

				const char *name = FiosBrowseTo(file);
				if (name != NULL) {
					if (click_count == 1) {
						if (this->selected != file) {
							this->selected = file;
							_load_check_data.Clear();

							if (file->type == FIOS_TYPE_FILE || file->type == FIOS_TYPE_SCENARIO) {
								SaveOrLoad(name, SL_LOAD_CHECK, NO_DIRECTORY, false);
							}

							this->InvalidateData(1);
						}
						if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
							/* Copy clicked name to editbox */
							ttd_strlcpy(this->text.buf, file->title, this->text.max_bytes);
							UpdateTextBufferSize(&this->text);
							this->SetWidgetDirty(SLWW_SAVE_OSK_TITLE);
						}
					} else if (!_load_check_data.HasErrors()) {
						this->selected = file;
						if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
							this->OnClick(pt, SLWW_LOAD_BUTTON, 1);
						} else if (_saveload_mode == SLD_LOAD_HEIGHTMAP) {
							SetFiosType(file->type);
							strecpy(_file_to_saveload.name, name, lastof(_file_to_saveload.name));
							strecpy(_file_to_saveload.title, file->title, lastof(_file_to_saveload.title));

							delete this;
							ShowHeightmapLoad();
						}
					}
				} else {
					/* Changed directory, need refresh. */
					this->InvalidateData();
				}
				break;
			}

			case SLWW_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
#if defined(ENABLE_NETWORK)
					switch (_saveload_mode) {
						default: NOT_REACHED();
						case SLD_LOAD_SCENARIO:  ShowNetworkContentListWindow(NULL, CONTENT_TYPE_SCENARIO);  break;
						case SLD_LOAD_HEIGHTMAP: ShowNetworkContentListWindow(NULL, CONTENT_TYPE_HEIGHTMAP); break;
					}
#endif
				}
				break;

			case SLWW_DELETE_SELECTION: case SLWW_SAVE_GAME: // Delete, Save game
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		if (_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) {
			this->HandleEditBox(SLWW_SAVE_OSK_TITLE);
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		if (keycode == WKC_ESC) {
			delete this;
			return ES_HANDLED;
		}

		EventState state = ES_NOT_HANDLED;
		if ((_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO) &&
				this->HandleEditBoxKey(SLWW_SAVE_OSK_TITLE, key, keycode, state) == HEBR_CONFIRM) {
			this->HandleButtonClick(SLWW_SAVE_GAME);
		}

		return state;
	}

	virtual void OnTimeout()
	{
		/* This test protects against using widgets 11 and 12 which are only available
		 * in those two saveload mode */
		if (!(_saveload_mode == SLD_SAVE_GAME || _saveload_mode == SLD_SAVE_SCENARIO)) return;

		if (this->IsWidgetLowered(SLWW_DELETE_SELECTION)) { // Delete button clicked
			if (!FiosDelete(this->text.buf)) {
				ShowErrorMessage(STR_ERROR_UNABLE_TO_DELETE_FILE, INVALID_STRING_ID, WL_ERROR);
			} else {
				this->InvalidateData();
				/* Reset file name to current date on successful delete */
				if (_saveload_mode == SLD_SAVE_GAME) GenerateFileName();
			}

			UpdateTextBufferSize(&this->text);
		} else if (this->IsWidgetLowered(SLWW_SAVE_GAME)) { // Save button clicked
			_switch_mode = SM_SAVE;
			FiosMakeSavegameName(_file_to_saveload.name, this->text.buf, sizeof(_file_to_saveload.name));

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, SLWW_DRIVES_DIRECTORIES_LIST);
	}

	virtual void OnInvalidateData(int data)
	{
		switch (data) {
			case 0:
				/* Rescan files */
				this->selected = NULL;
				_load_check_data.Clear();
				BuildFileList();
				/* FALL THROUGH */
			case 1:
				/* Selection changes */
				if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
					this->SetWidgetDisabledState(SLWW_LOAD_BUTTON,
							this->selected == NULL || _load_check_data.HasErrors() || !(_load_check_data.grf_compatibility != GLC_NOT_FOUND || _settings_client.gui.UserIsAllowedToChangeNewGRFs()));
					this->SetWidgetDisabledState(SLWW_NEWGRF_INFO,
							!_load_check_data.HasNewGrfs());
				}
				break;
		}
	}
};

/** Load game/scenario */
static const WindowDesc _load_dialog_desc(
	WDP_CENTER, 500, 294,
	WC_SAVELOAD, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_load_dialog_widgets, lengthof(_nested_load_dialog_widgets)
);

/** Load heightmap */
static const WindowDesc _load_heightmap_dialog_desc(
	WDP_CENTER, 257, 320,
	WC_SAVELOAD, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_load_heightmap_dialog_widgets, lengthof(_nested_load_heightmap_dialog_widgets)
);

/** Save game/scenario */
static const WindowDesc _save_dialog_desc(
	WDP_CENTER, 500, 294,
	WC_SAVELOAD, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_save_dialog_widgets, lengthof(_nested_save_dialog_widgets)
);

/**
 * These values are used to convert the file/operations mode into a corresponding file type.
 * So each entry, as expressed by the related comment, is based on the enum
 */
static const FileType _file_modetotype[] = {
	FT_SAVEGAME,  ///< used for SLD_LOAD_GAME
	FT_SCENARIO,  ///< used for SLD_LOAD_SCENARIO
	FT_SAVEGAME,  ///< used for SLD_SAVE_GAME
	FT_SCENARIO,  ///< used for SLD_SAVE_SCENARIO
	FT_HEIGHTMAP, ///< used for SLD_LOAD_HEIGHTMAP
	FT_SAVEGAME,  ///< SLD_NEW_GAME
};

void ShowSaveLoadDialog(SaveLoadDialogMode mode)
{
	DeleteWindowById(WC_SAVELOAD, 0);

	const WindowDesc *sld;
	switch (mode) {
		case SLD_SAVE_GAME:
		case SLD_SAVE_SCENARIO:
			sld = &_save_dialog_desc; break;
		case SLD_LOAD_HEIGHTMAP:
			sld = &_load_heightmap_dialog_desc; break;
		default:
			sld = &_load_dialog_desc; break;
	}

	_saveload_mode = mode;
	_file_to_saveload.filetype = _file_modetotype[mode];

	new SaveLoadWindow(sld, mode);
}

void SetFiosType(const byte fiostype)
{
	switch (fiostype) {
		case FIOS_TYPE_FILE:
		case FIOS_TYPE_SCENARIO:
			_file_to_saveload.mode = SL_LOAD;
			break;

		case FIOS_TYPE_OLDFILE:
		case FIOS_TYPE_OLD_SCENARIO:
			_file_to_saveload.mode = SL_OLD_LOAD;
			break;

#ifdef WITH_PNG
		case FIOS_TYPE_PNG:
			_file_to_saveload.mode = SL_PNG;
			break;
#endif /* WITH_PNG */

		case FIOS_TYPE_BMP:
			_file_to_saveload.mode = SL_BMP;
			break;

		default:
			_file_to_saveload.mode = SL_INVALID;
			break;
	}
}
