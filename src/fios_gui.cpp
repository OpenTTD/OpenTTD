/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fios_gui.cpp GUIs for loading/saving games, scenarios, heightmaps, ... */

#include "stdafx.h"
#include "openttd.h"
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

#include "table/sprites.h"
#include "table/strings.h"

SaveLoadDialogMode _saveload_mode;

static bool _fios_path_changed;
static bool _savegame_sort_dirty;


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
};

static const NWidgetPart _nested_load_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLWW_WINDOWTITLE),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_FILE_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_INSET, COLOUR_GREY, SLWW_DRIVES_DIRECTORIES_LIST), SetFill(1, 1), SetPadding(2, 1, 2, 2),
										SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, SLWW_CONTENT_DOWNLOAD_SEL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_CONTENT_DOWNLOAD), SetResize(1, 0),
										SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SLWW_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
				NWidget(WWT_SCROLLBAR, COLOUR_GREY, SLWW_SCROLLBAR),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_save_dialog_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SLWW_WINDOWTITLE),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYNAME), SetDataTip(STR_SORT_BY_CAPTION_NAME, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SORT_BYDATE), SetDataTip(STR_SORT_BY_CAPTION_DATE, STR_TOOLTIP_SORT_ORDER), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_BACKGROUND), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SLWW_FILE_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_INSET, COLOUR_GREY, SLWW_DRIVES_DIRECTORIES_LIST), SetPadding(2, 1, 0, 2),
										SetDataTip(0x0, STR_SAVELOAD_LIST_TOOLTIP), SetResize(1, 10), EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SLWW_HOME_BUTTON), SetMinimalSize(12, 12), SetDataTip(SPR_HOUSE_ICON, STR_SAVELOAD_HOME_BUTTON),
				NWidget(WWT_SCROLLBAR, COLOUR_GREY, SLWW_SCROLLBAR),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_EDITBOX, COLOUR_GREY, SLWW_SAVE_OSK_TITLE), SetPadding(3, 2, 2, 2), SetFill(1, 0), SetResize(1, 0),
										SetDataTip(STR_SAVELOAD_OSKTITLE, STR_SAVELOAD_EDITBOX_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_DELETE_SELECTION), SetDataTip(STR_SAVELOAD_DELETE_BUTTON, STR_SAVELOAD_DELETE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SLWW_SAVE_GAME),        SetDataTip(STR_SAVELOAD_SAVE_BUTTON, STR_SAVELOAD_SAVE_TOOLTIP),     SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
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

		this->CreateNestedTree(desc);
		if (mode == SLD_LOAD_GAME) this->GetWidget<NWidgetStacked>(SLWW_CONTENT_DOWNLOAD_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
		this->GetWidget<NWidgetCore>(SLWW_WINDOWTITLE)->widget_data = saveload_captions[mode];

		this->FinishInitNested(desc, 0);

		this->LowerWidget(SLWW_DRIVES_DIRECTORIES_LIST);

		/* pause is only used in single-player, non-editor mode, non-menu mode. It
		 * will be unpaused in the WE_DESTROY event handler. */
		if (_game_mode != GM_MENU && !_networking && _game_mode != GM_EDITOR) {
			DoCommandP(0, PM_PAUSED_SAVELOAD, 1, CMD_PAUSE);
		}
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

		BuildFileList();

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
			} break;

			case SLWW_DRIVES_DIRECTORIES_LIST: {
				GfxFillRect(r.left + 1, r.top + 1, r.right, r.bottom, 0xD7);

				uint y = r.top + WD_FRAMERECT_TOP;
				for (uint pos = this->vscroll.GetPosition(); pos < _fios_items.Length(); pos++) {
					const FiosItem *item = _fios_items.Get(pos);

					DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, item->title, _fios_colours[item->type]);
					y += this->resize.step_height;
					if (y >= this->vscroll.GetCapacity() * this->resize.step_height + r.top + WD_FRAMERECT_TOP) break;
				}
			} break;
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
		}
	}

	virtual void OnPaint()
	{
		if (_savegame_sort_dirty) {
			_savegame_sort_dirty = false;
			MakeSortedSaveGameList();
		}

		this->vscroll.SetCount(_fios_items.Length());
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
				this->SetDirty();
				BuildFileList();
				break;

			case SLWW_DRIVES_DIRECTORIES_LIST: { // Click the listbox
				int y = (pt.y - this->GetWidget<NWidgetBase>(SLWW_DRIVES_DIRECTORIES_LIST)->pos_y - WD_FRAMERECT_TOP) / this->resize.step_height;

				if (y < 0 || (y += this->vscroll.GetPosition()) >= this->vscroll.GetCount()) return;

				const FiosItem *file = _fios_items.Get(y);

				const char *name = FiosBrowseTo(file);
				if (name != NULL) {
					if (_saveload_mode == SLD_LOAD_GAME || _saveload_mode == SLD_LOAD_SCENARIO) {
						_switch_mode = (_game_mode == GM_EDITOR) ? SM_LOAD_SCENARIO : SM_LOAD;

						SetFiosType(file->type);
						strecpy(_file_to_saveload.name, name, lastof(_file_to_saveload.name));
						strecpy(_file_to_saveload.title, file->title, lastof(_file_to_saveload.title));

						delete this;
					} else if (_saveload_mode == SLD_LOAD_HEIGHTMAP) {
						SetFiosType(file->type);
						strecpy(_file_to_saveload.name, name, lastof(_file_to_saveload.name));
						strecpy(_file_to_saveload.title, file->title, lastof(_file_to_saveload.title));

						delete this;
						ShowHeightmapLoad();
					} else {
						/* SLD_SAVE_GAME, SLD_SAVE_SCENARIO copy clicked name to editbox */
						ttd_strlcpy(this->text.buf, file->title, this->text.maxsize);
						UpdateTextBufferSize(&this->text);
						this->SetWidgetDirty(SLWW_SAVE_OSK_TITLE);
					}
				} else {
					/* Changed directory, need repaint. */
					this->SetDirty();
					BuildFileList();
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
				BuildFileList();
				/* Reset file name to current date on successful delete */
				if (_saveload_mode == SLD_SAVE_GAME) GenerateFileName();
			}

			UpdateTextBufferSize(&this->text);
			this->SetDirty();
		} else if (this->IsWidgetLowered(SLWW_SAVE_GAME)) { // Save button clicked
			_switch_mode = SM_SAVE;
			FiosMakeSavegameName(_file_to_saveload.name, this->text.buf, sizeof(_file_to_saveload.name));

			/* In the editor set up the vehicle engines correctly (date might have changed) */
			if (_game_mode == GM_EDITOR) StartupEngines();
		}
	}

	virtual void OnResize()
	{
		this->vscroll.SetCapacityFromWidget(this, SLWW_DRIVES_DIRECTORIES_LIST);
	}

	virtual void OnInvalidateData(int data)
	{
		BuildFileList();
	}
};

static const WindowDesc _load_dialog_desc(
	WDP_CENTER, 257, 294,
	WC_SAVELOAD, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_load_dialog_widgets, lengthof(_nested_load_dialog_widgets)
);

static const WindowDesc _save_dialog_desc(
	WDP_CENTER, 257, 320,
	WC_SAVELOAD, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_save_dialog_widgets, lengthof(_nested_save_dialog_widgets)
);

/** These values are used to convert the file/operations mode into a corresponding file type.
 * So each entry, as expressed by the related comment, is based on the enum   */
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
