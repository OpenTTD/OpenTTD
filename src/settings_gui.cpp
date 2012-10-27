/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_gui.cpp GUI for settings. */

#include "stdafx.h"
#include "currency.h"
#include "error.h"
#include "settings_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "screenshot.h"
#include "network/network.h"
#include "town.h"
#include "settings_internal.h"
#include "newgrf_townname.h"
#include "strings_func.h"
#include "window_func.h"
#include "string_func.h"
#include "widgets/dropdown_type.h"
#include "widgets/dropdown_func.h"
#include "highscore.h"
#include "base_media_base.h"
#include "company_base.h"
#include "company_func.h"
#include "viewport_func.h"
#include "core/geometry_func.hpp"
#include "ai/ai.hpp"
#include "blitter/factory.hpp"
#include "language.h"
#include "textfile_gui.h"



static const StringID _units_dropdown[] = {
	STR_GAME_OPTIONS_MEASURING_UNITS_IMPERIAL,
	STR_GAME_OPTIONS_MEASURING_UNITS_METRIC,
	STR_GAME_OPTIONS_MEASURING_UNITS_SI,
	INVALID_STRING_ID
};

static const StringID _driveside_dropdown[] = {
	STR_GAME_OPTIONS_ROAD_VEHICLES_DROPDOWN_LEFT,
	STR_GAME_OPTIONS_ROAD_VEHICLES_DROPDOWN_RIGHT,
	INVALID_STRING_ID
};

static const StringID _autosave_dropdown[] = {
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_OFF,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_1_MONTH,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_3_MONTHS,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_6_MONTHS,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_12_MONTHS,
	INVALID_STRING_ID,
};

int _nb_orig_names = SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1; ///< Number of original town names.
static StringID *_grf_names = NULL; ///< Pointer to town names defined by NewGRFs.
static int _nb_grf_names = 0;       ///< Number of town names defined by NewGRFs.

/** Allocate memory for the NewGRF town names. */
void InitGRFTownGeneratorNames()
{
	free(_grf_names);
	_grf_names = GetGRFTownNameList();
	_nb_grf_names = 0;
	for (StringID *s = _grf_names; *s != INVALID_STRING_ID; s++) _nb_grf_names++;
}

/**
 * Get a town name.
 * @param town_name Number of the wanted town name.
 * @return Name of the town as string ID.
 */
static inline StringID TownName(int town_name)
{
	if (town_name < _nb_orig_names) return STR_GAME_OPTIONS_TOWN_NAME_ORIGINAL_ENGLISH + town_name;
	town_name -= _nb_orig_names;
	if (town_name < _nb_grf_names) return _grf_names[town_name];
	return STR_UNDEFINED;
}

/**
 * Get index of the current screen resolution.
 * @return Index of the current screen resolution if it is a known resolution, #_num_resolutions otherwise.
 */
static int GetCurRes()
{
	int i;

	for (i = 0; i != _num_resolutions; i++) {
		if ((int)_resolutions[i].width == _screen.width &&
				(int)_resolutions[i].height == _screen.height) {
			break;
		}
	}
	return i;
}

static void ShowCustCurrency();

template <class T>
static DropDownList *BuiltSetDropDownList(int *selected_index)
{
	int n = T::GetNumSets();
	*selected_index = T::GetIndexOfUsedSet();

	DropDownList *list = new DropDownList();
	for (int i = 0; i < n; i++) {
		list->push_back(new DropDownListCharStringItem(T::GetSet(i)->name, i, (_game_mode == GM_MENU) ? false : (*selected_index != i)));
	}

	return list;
}

/** Window for displaying the textfile of a BaseSet. */
template <class TBaseSet>
struct BaseSetTextfileWindow : public TextfileWindow {
	const TBaseSet* baseset; ///< View the textfile of this BaseSet.
	StringID content_type;   ///< STR_CONTENT_TYPE_xxx for title.

	BaseSetTextfileWindow(TextfileType file_type, const TBaseSet* baseset, StringID content_type) : TextfileWindow(file_type), baseset(baseset), content_type(content_type)
	{
		const char *textfile = this->baseset->GetTextfile(file_type);
		this->LoadTextfile(textfile, BASESET_DIR);
	}

	/* virtual */ void SetStringParameters(int widget) const
	{
		if (widget == WID_TF_CAPTION) {
			SetDParam(0, content_type);
			SetDParamStr(1, this->baseset->name);
		}
	}
};

/**
 * Open the BaseSet version of the textfile window.
 * @param file_type The type of textfile to display.
 * @param baseset The BaseSet to use.
 * @param content_type STR_CONTENT_TYPE_xxx for title.
 */
template <class TBaseSet>
void ShowBaseSetTextfileWindow(TextfileType file_type, const TBaseSet* baseset, StringID content_type)
{
	DeleteWindowByClass(WC_TEXTFILE);
	new BaseSetTextfileWindow<TBaseSet>(file_type, baseset, content_type);
}

struct GameOptionsWindow : Window {
	GameSettings *opt;
	bool reload;

	GameOptionsWindow(const WindowDesc *desc) : Window()
	{
		this->opt = &GetGameSettings();
		this->reload = false;

		this->InitNested(desc, WN_GAME_OPTIONS_GAME_OPTIONS);
		this->OnInvalidateData(0);
	}

	~GameOptionsWindow()
	{
		DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
		if (this->reload) _switch_mode = SM_MENU;
	}

	/**
	 * Build the dropdown list for a specific widget.
	 * @param widget         Widget to build list for
	 * @param selected_index Currently selected item
	 * @return the built dropdown list, or NULL if the widget has no dropdown menu.
	 */
	DropDownList *BuildDropDownList(int widget, int *selected_index) const
	{
		DropDownList *list = NULL;
		switch (widget) {
			case WID_GO_CURRENCY_DROPDOWN: { // Setup currencies dropdown
				list = new DropDownList();
				*selected_index = this->opt->locale.currency;
				StringID *items = BuildCurrencyDropdown();
				uint disabled = _game_mode == GM_MENU ? 0 : ~GetMaskOfAllowedCurrencies();
				int custom_index = -1;

				/* Add non-custom currencies; sorted naturally */
				for (uint i = 0; *items != INVALID_STRING_ID; items++, i++) {
					if (*items == STR_GAME_OPTIONS_CURRENCY_CUSTOM) {
						custom_index = i;
					} else {
						list->push_back(new DropDownListStringItem(*items, i, HasBit(disabled, i)));
					}
				}
				list->sort(DropDownListStringItem::NatSortFunc);

				/* Append custom currency at the end */
				if (custom_index >= 0) {
					list->push_back(new DropDownListItem(-1, false)); // separator line
					list->push_back(new DropDownListStringItem(STR_GAME_OPTIONS_CURRENCY_CUSTOM, custom_index, HasBit(disabled, custom_index)));
				}
				break;
			}

			case WID_GO_DISTANCE_DROPDOWN: { // Setup distance unit dropdown
				list = new DropDownList();
				*selected_index = this->opt->locale.units;
				const StringID *items = _units_dropdown;
				for (uint i = 0; *items != INVALID_STRING_ID; items++, i++) {
					list->push_back(new DropDownListStringItem(*items, i, false));
				}
				break;
			}

			case WID_GO_ROADSIDE_DROPDOWN: { // Setup road-side dropdown
				list = new DropDownList();
				*selected_index = this->opt->vehicle.road_side;
				const StringID *items = _driveside_dropdown;
				uint disabled = 0;

				/* You can only change the drive side if you are in the menu or ingame with
				 * no vehicles present. In a networking game only the server can change it */
				extern bool RoadVehiclesAreBuilt();
				if ((_game_mode != GM_MENU && RoadVehiclesAreBuilt()) || (_networking && !_network_server)) {
					disabled = ~(1 << this->opt->vehicle.road_side); // disable the other value
				}

				for (uint i = 0; *items != INVALID_STRING_ID; items++, i++) {
					list->push_back(new DropDownListStringItem(*items, i, HasBit(disabled, i)));
				}
				break;
			}

			case WID_GO_TOWNNAME_DROPDOWN: { // Setup townname dropdown
				list = new DropDownList();
				*selected_index = this->opt->game_creation.town_name;

				int enabled_item = (_game_mode == GM_MENU || Town::GetNumItems() == 0) ? -1 : *selected_index;

				/* Add and sort original townnames generators */
				for (int i = 0; i < _nb_orig_names; i++) {
					list->push_back(new DropDownListStringItem(STR_GAME_OPTIONS_TOWN_NAME_ORIGINAL_ENGLISH + i, i, enabled_item != i && enabled_item >= 0));
				}
				list->sort(DropDownListStringItem::NatSortFunc);

				/* Add and sort newgrf townnames generators */
				DropDownList newgrf_names;
				for (int i = 0; i < _nb_grf_names; i++) {
					int result = _nb_orig_names + i;
					newgrf_names.push_back(new DropDownListStringItem(_grf_names[i], result, enabled_item != result && enabled_item >= 0));
				}
				newgrf_names.sort(DropDownListStringItem::NatSortFunc);

				/* Insert newgrf_names at the top of the list */
				if (newgrf_names.size() > 0) {
					newgrf_names.push_back(new DropDownListItem(-1, false)); // separator line
					list->splice(list->begin(), newgrf_names);
				}
				break;
			}

			case WID_GO_AUTOSAVE_DROPDOWN: { // Setup autosave dropdown
				list = new DropDownList();
				*selected_index = _settings_client.gui.autosave;
				const StringID *items = _autosave_dropdown;
				for (uint i = 0; *items != INVALID_STRING_ID; items++, i++) {
					list->push_back(new DropDownListStringItem(*items, i, false));
				}
				break;
			}

			case WID_GO_LANG_DROPDOWN: { // Setup interface language dropdown
				list = new DropDownList();
				for (uint i = 0; i < _languages.Length(); i++) {
					if (&_languages[i] == _current_language) *selected_index = i;
					list->push_back(new DropDownListStringItem(SPECSTR_LANGUAGE_START + i, i, false));
				}
				list->sort(DropDownListStringItem::NatSortFunc);
				break;
			}

			case WID_GO_RESOLUTION_DROPDOWN: // Setup resolution dropdown
				list = new DropDownList();
				*selected_index = GetCurRes();
				for (int i = 0; i < _num_resolutions; i++) {
					list->push_back(new DropDownListStringItem(SPECSTR_RESOLUTION_START + i, i, false));
				}
				break;

			case WID_GO_SCREENSHOT_DROPDOWN: // Setup screenshot format dropdown
				list = new DropDownList();
				*selected_index = _cur_screenshot_format;
				for (uint i = 0; i < _num_screenshot_formats; i++) {
					if (!GetScreenshotFormatSupports_32bpp(i) && BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() == 32) continue;
					list->push_back(new DropDownListStringItem(SPECSTR_SCREENSHOT_START + i, i, false));
				}
				break;

			case WID_GO_BASE_GRF_DROPDOWN:
				list = BuiltSetDropDownList<BaseGraphics>(selected_index);
				break;

			case WID_GO_BASE_SFX_DROPDOWN:
				list = BuiltSetDropDownList<BaseSounds>(selected_index);
				break;

			case WID_GO_BASE_MUSIC_DROPDOWN:
				list = BuiltSetDropDownList<BaseMusic>(selected_index);
				break;

			default:
				return NULL;
		}

		return list;
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_GO_CURRENCY_DROPDOWN:   SetDParam(0, _currency_specs[this->opt->locale.currency].name); break;
			case WID_GO_DISTANCE_DROPDOWN:   SetDParam(0, STR_GAME_OPTIONS_MEASURING_UNITS_IMPERIAL + this->opt->locale.units); break;
			case WID_GO_ROADSIDE_DROPDOWN:   SetDParam(0, STR_GAME_OPTIONS_ROAD_VEHICLES_DROPDOWN_LEFT + this->opt->vehicle.road_side); break;
			case WID_GO_TOWNNAME_DROPDOWN:   SetDParam(0, TownName(this->opt->game_creation.town_name)); break;
			case WID_GO_AUTOSAVE_DROPDOWN:   SetDParam(0, _autosave_dropdown[_settings_client.gui.autosave]); break;
			case WID_GO_LANG_DROPDOWN:       SetDParamStr(0, _current_language->own_name); break;
			case WID_GO_RESOLUTION_DROPDOWN: SetDParam(0, GetCurRes() == _num_resolutions ? STR_GAME_OPTIONS_RESOLUTION_OTHER : SPECSTR_RESOLUTION_START + GetCurRes()); break;
			case WID_GO_SCREENSHOT_DROPDOWN: SetDParam(0, SPECSTR_SCREENSHOT_START + _cur_screenshot_format); break;
			case WID_GO_BASE_GRF_DROPDOWN:   SetDParamStr(0, BaseGraphics::GetUsedSet()->name); break;
			case WID_GO_BASE_GRF_STATUS:     SetDParam(0, BaseGraphics::GetUsedSet()->GetNumInvalid()); break;
			case WID_GO_BASE_SFX_DROPDOWN:   SetDParamStr(0, BaseSounds::GetUsedSet()->name); break;
			case WID_GO_BASE_MUSIC_DROPDOWN: SetDParamStr(0, BaseMusic::GetUsedSet()->name); break;
			case WID_GO_BASE_MUSIC_STATUS:   SetDParam(0, BaseMusic::GetUsedSet()->GetNumInvalid()); break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_GO_BASE_GRF_DESCRIPTION:
				SetDParamStr(0, BaseGraphics::GetUsedSet()->GetDescription(GetCurrentLanguageIsoCode()));
				DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, STR_BLACK_RAW_STRING);
				break;

			case WID_GO_BASE_SFX_DESCRIPTION:
				SetDParamStr(0, BaseSounds::GetUsedSet()->GetDescription(GetCurrentLanguageIsoCode()));
				DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, STR_BLACK_RAW_STRING);
				break;

			case WID_GO_BASE_MUSIC_DESCRIPTION:
				SetDParamStr(0, BaseMusic::GetUsedSet()->GetDescription(GetCurrentLanguageIsoCode()));
				DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, STR_BLACK_RAW_STRING);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_GO_BASE_GRF_DESCRIPTION:
				/* Find the biggest description for the default size. */
				for (int i = 0; i < BaseGraphics::GetNumSets(); i++) {
					SetDParamStr(0, BaseGraphics::GetSet(i)->GetDescription(GetCurrentLanguageIsoCode()));
					size->height = max(size->height, (uint)GetStringHeight(STR_BLACK_RAW_STRING, size->width));
				}
				break;

			case WID_GO_BASE_GRF_STATUS:
				/* Find the biggest description for the default size. */
				for (int i = 0; i < BaseGraphics::GetNumSets(); i++) {
					uint invalid_files = BaseGraphics::GetSet(i)->GetNumInvalid();
					if (invalid_files == 0) continue;

					SetDParam(0, invalid_files);
					*size = maxdim(*size, GetStringBoundingBox(STR_GAME_OPTIONS_BASE_GRF_STATUS));
				}
				break;

			case WID_GO_BASE_SFX_DESCRIPTION:
				/* Find the biggest description for the default size. */
				for (int i = 0; i < BaseSounds::GetNumSets(); i++) {
					SetDParamStr(0, BaseSounds::GetSet(i)->GetDescription(GetCurrentLanguageIsoCode()));
					size->height = max(size->height, (uint)GetStringHeight(STR_BLACK_RAW_STRING, size->width));
				}
				break;

			case WID_GO_BASE_MUSIC_DESCRIPTION:
				/* Find the biggest description for the default size. */
				for (int i = 0; i < BaseMusic::GetNumSets(); i++) {
					SetDParamStr(0, BaseMusic::GetSet(i)->GetDescription(GetCurrentLanguageIsoCode()));
					size->height = max(size->height, (uint)GetStringHeight(STR_BLACK_RAW_STRING, size->width));
				}
				break;

			case WID_GO_BASE_MUSIC_STATUS:
				/* Find the biggest description for the default size. */
				for (int i = 0; i < BaseMusic::GetNumSets(); i++) {
					uint invalid_files = BaseMusic::GetSet(i)->GetNumInvalid();
					if (invalid_files == 0) continue;

					SetDParam(0, invalid_files);
					*size = maxdim(*size, GetStringBoundingBox(STR_GAME_OPTIONS_BASE_MUSIC_STATUS));
				}
				break;

			default: {
				int selected;
				DropDownList *list = this->BuildDropDownList(widget, &selected);
				if (list != NULL) {
					/* Find the biggest item for the default size. */
					for (DropDownList::iterator it = list->begin(); it != list->end(); it++) {
						static const Dimension extra = {WD_DROPDOWNTEXT_LEFT + WD_DROPDOWNTEXT_RIGHT, WD_DROPDOWNTEXT_TOP + WD_DROPDOWNTEXT_BOTTOM};
						Dimension string_dim;
						int width = (*it)->Width();
						string_dim.width = width + extra.width;
						string_dim.height = (*it)->Height(width) + extra.height;
						*size = maxdim(*size, string_dim);
						delete *it;
					}
					delete list;
				}
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget >= WID_GO_BASE_GRF_TEXTFILE && widget < WID_GO_BASE_GRF_TEXTFILE + TFT_END) {
			if (BaseGraphics::GetUsedSet() == NULL) return;

			ShowBaseSetTextfileWindow((TextfileType)(widget - WID_GO_BASE_GRF_TEXTFILE), BaseGraphics::GetUsedSet(), STR_CONTENT_TYPE_BASE_GRAPHICS);
			return;
		}
		if (widget >= WID_GO_BASE_SFX_TEXTFILE && widget < WID_GO_BASE_SFX_TEXTFILE + TFT_END) {
			if (BaseSounds::GetUsedSet() == NULL) return;

			ShowBaseSetTextfileWindow((TextfileType)(widget - WID_GO_BASE_SFX_TEXTFILE), BaseSounds::GetUsedSet(), STR_CONTENT_TYPE_BASE_SOUNDS);
			return;
		}
		if (widget >= WID_GO_BASE_MUSIC_TEXTFILE && widget < WID_GO_BASE_MUSIC_TEXTFILE + TFT_END) {
			if (BaseMusic::GetUsedSet() == NULL) return;

			ShowBaseSetTextfileWindow((TextfileType)(widget - WID_GO_BASE_MUSIC_TEXTFILE), BaseMusic::GetUsedSet(), STR_CONTENT_TYPE_BASE_MUSIC);
			return;
		}
		switch (widget) {
			case WID_GO_FULLSCREEN_BUTTON: // Click fullscreen on/off
				/* try to toggle full-screen on/off */
				if (!ToggleFullScreen(!_fullscreen)) {
					ShowErrorMessage(STR_ERROR_FULLSCREEN_FAILED, INVALID_STRING_ID, WL_ERROR);
				}
				this->SetWidgetLoweredState(WID_GO_FULLSCREEN_BUTTON, _fullscreen);
				this->SetDirty();
				break;

			default: {
				int selected;
				DropDownList *list = this->BuildDropDownList(widget, &selected);
				if (list != NULL) {
					ShowDropDownList(this, list, selected, widget);
				}
				break;
			}
		}
	}

	/**
	 * Set the base media set.
	 * @param index the index of the media set
	 * @tparam T class of media set
	 */
	template <class T>
	void SetMediaSet(int index)
	{
		if (_game_mode == GM_MENU) {
			const char *name = T::GetSet(index)->name;

			free(T::ini_set);
			T::ini_set = strdup(name);

			T::SetSet(name);
			this->reload = true;
			this->InvalidateData();
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case WID_GO_CURRENCY_DROPDOWN: // Currency
				if (index == CUSTOM_CURRENCY_ID) ShowCustCurrency();
				this->opt->locale.currency = index;
				ReInitAllWindows();
				break;

			case WID_GO_DISTANCE_DROPDOWN: // Measuring units
				this->opt->locale.units = index;
				MarkWholeScreenDirty();
				break;

			case WID_GO_ROADSIDE_DROPDOWN: // Road side
				if (this->opt->vehicle.road_side != index) { // only change if setting changed
					uint i;
					if (GetSettingFromName("vehicle.road_side", &i) == NULL) NOT_REACHED();
					SetSettingValue(i, index);
					MarkWholeScreenDirty();
				}
				break;

			case WID_GO_TOWNNAME_DROPDOWN: // Town names
				if (_game_mode == GM_MENU || Town::GetNumItems() == 0) {
					this->opt->game_creation.town_name = index;
					SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GAME_OPTIONS);
				}
				break;

			case WID_GO_AUTOSAVE_DROPDOWN: // Autosave options
				_settings_client.gui.autosave = index;
				this->SetDirty();
				break;

			case WID_GO_LANG_DROPDOWN: // Change interface language
				ReadLanguagePack(&_languages[index]);
				DeleteWindowByClass(WC_QUERY_STRING);
				CheckForMissingGlyphs();
				UpdateAllVirtCoords();
				ReInitAllWindows();
				break;

			case WID_GO_RESOLUTION_DROPDOWN: // Change resolution
				if (index < _num_resolutions && ChangeResInGame(_resolutions[index].width, _resolutions[index].height)) {
					this->SetDirty();
				}
				break;

			case WID_GO_SCREENSHOT_DROPDOWN: // Change screenshot format
				SetScreenshotFormat(index);
				this->SetDirty();
				break;

			case WID_GO_BASE_GRF_DROPDOWN:
				this->SetMediaSet<BaseGraphics>(index);
				break;

			case WID_GO_BASE_SFX_DROPDOWN:
				this->SetMediaSet<BaseSounds>(index);
				break;

			case WID_GO_BASE_MUSIC_DROPDOWN:
				this->SetMediaSet<BaseMusic>(index);
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data. @see GameOptionsInvalidationData
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->SetWidgetLoweredState(WID_GO_FULLSCREEN_BUTTON, _fullscreen);

		bool missing_files = BaseGraphics::GetUsedSet()->GetNumMissing() == 0;
		this->GetWidget<NWidgetCore>(WID_GO_BASE_GRF_STATUS)->SetDataTip(missing_files ? STR_EMPTY : STR_GAME_OPTIONS_BASE_GRF_STATUS, STR_NULL);

		for (TextfileType tft = TFT_BEGIN; tft < TFT_END; tft++) {
			this->SetWidgetDisabledState(WID_GO_BASE_GRF_TEXTFILE + tft, BaseGraphics::GetUsedSet() == NULL || BaseGraphics::GetUsedSet()->GetTextfile(tft) == NULL);
			this->SetWidgetDisabledState(WID_GO_BASE_SFX_TEXTFILE + tft, BaseSounds::GetUsedSet() == NULL || BaseSounds::GetUsedSet()->GetTextfile(tft) == NULL);
			this->SetWidgetDisabledState(WID_GO_BASE_MUSIC_TEXTFILE + tft, BaseMusic::GetUsedSet() == NULL || BaseMusic::GetUsedSet()->GetTextfile(tft) == NULL);
		}

		missing_files = BaseMusic::GetUsedSet()->GetNumInvalid() == 0;
		this->GetWidget<NWidgetCore>(WID_GO_BASE_MUSIC_STATUS)->SetDataTip(missing_files ? STR_EMPTY : STR_GAME_OPTIONS_BASE_MUSIC_STATUS, STR_NULL);
	}
};

static const NWidgetPart _nested_game_options_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_GO_BACKGROUND), SetPIP(6, 6, 10),
		NWidget(NWID_HORIZONTAL), SetPIP(10, 10, 10),
			NWidget(NWID_VERTICAL), SetPIP(0, 6, 0),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_CURRENCY_UNITS_FRAME, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_CURRENCY_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_CURRENCY_UNITS_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_ROAD_VEHICLES_FRAME, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_ROADSIDE_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_ROAD_VEHICLES_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_AUTOSAVE_FRAME, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_AUTOSAVE_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_RESOLUTION, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_RESOLUTION_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_RESOLUTION_TOOLTIP), SetFill(1, 0), SetPadding(0, 0, 3, 0),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_TEXT, COLOUR_GREY), SetMinimalSize(0, 12), SetFill(1, 0), SetDataTip(STR_GAME_OPTIONS_FULLSCREEN, STR_NULL),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_FULLSCREEN_BUTTON), SetMinimalSize(21, 9), SetDataTip(STR_EMPTY, STR_GAME_OPTIONS_FULLSCREEN_TOOLTIP),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_VERTICAL), SetPIP(0, 6, 0),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_MEASURING_UNITS_FRAME, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_DISTANCE_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_MEASURING_UNITS_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_TOWN_NAMES_FRAME, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_TOWNNAME_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_TOWN_NAMES_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_LANGUAGE, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_LANG_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_RAW_STRING, STR_GAME_OPTIONS_LANGUAGE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_SCREENSHOT_FORMAT, STR_NULL),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_SCREENSHOT_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_STRING, STR_GAME_OPTIONS_SCREENSHOT_FORMAT_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 0), SetFill(0, 1),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_BASE_GRF, STR_NULL), SetPadding(0, 10, 0, 10),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 30, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_BASE_GRF_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_RAW_STRING, STR_GAME_OPTIONS_BASE_GRF_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_GO_BASE_GRF_STATUS), SetMinimalSize(150, 12), SetDataTip(STR_EMPTY, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_GO_BASE_GRF_DESCRIPTION), SetMinimalSize(330, 0), SetDataTip(STR_EMPTY, STR_GAME_OPTIONS_BASE_GRF_DESCRIPTION_TOOLTIP), SetFill(1, 0), SetPadding(6, 0, 6, 0),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(7, 0, 7),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_NULL),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_BASE_SFX, STR_NULL), SetPadding(0, 10, 0, 10),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 30, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_BASE_SFX_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_RAW_STRING, STR_GAME_OPTIONS_BASE_SFX_TOOLTIP),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_GO_BASE_SFX_DESCRIPTION), SetMinimalSize(330, 0), SetDataTip(STR_EMPTY, STR_GAME_OPTIONS_BASE_SFX_DESCRIPTION_TOOLTIP), SetFill(1, 0), SetPadding(6, 0, 6, 0),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(7, 0, 7),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_NULL),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_FRAME, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_BASE_MUSIC, STR_NULL), SetPadding(0, 10, 0, 10),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 30, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_BASE_MUSIC_DROPDOWN), SetMinimalSize(150, 12), SetDataTip(STR_BLACK_RAW_STRING, STR_GAME_OPTIONS_BASE_MUSIC_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_GO_BASE_MUSIC_STATUS), SetMinimalSize(150, 12), SetDataTip(STR_EMPTY, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_GO_BASE_MUSIC_DESCRIPTION), SetMinimalSize(330, 0), SetDataTip(STR_EMPTY, STR_GAME_OPTIONS_BASE_MUSIC_DESCRIPTION_TOOLTIP), SetFill(1, 0), SetPadding(6, 0, 6, 0),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(7, 0, 7),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_NULL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _game_options_desc(
	WDP_CENTER, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_game_options_widgets, lengthof(_nested_game_options_widgets)
);

/** Open the game options window. */
void ShowGameOptions()
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new GameOptionsWindow(&_game_options_desc);
}

extern void StartupEconomy();

void SetDifficultyLevel(int mode, DifficultySettings *gm_opt);

class GameDifficultyWindow : public Window {
private:
	/* Temporary holding place of values in the difficulty window until 'Save' is clicked */
	GameSettings opt_mod_temp;

public:
	/** The number of difficulty settings */
	static const uint GAME_DIFFICULTY_NUM = 18;
	/** The number of widgets per difficulty setting */
	static const uint WIDGETS_PER_DIFFICULTY = 3;

	GameDifficultyWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc, WN_GAME_OPTIONS_GAME_DIFFICULTY);

		/* Setup disabled buttons when creating window
		 * disable all other difficulty buttons during gameplay except for 'custom' */
		this->SetWidgetsDisabledState(_game_mode != GM_MENU,
			WID_GD_LVL_EASY,
			WID_GD_LVL_MEDIUM,
			WID_GD_LVL_HARD,
			WID_GD_LVL_CUSTOM,
			WIDGET_LIST_END);
		this->SetWidgetDisabledState(WID_GD_HIGHSCORE, _game_mode == GM_EDITOR || _networking); // highscore chart in multiplayer
		this->SetWidgetDisabledState(WID_GD_ACCEPT, _networking && !_network_server); // Save-button in multiplayer (and if client)

		/* Read data */
		this->OnInvalidateData(GOID_DIFFICULTY_CHANGED);
	}

	virtual void SetStringParameters(int widget) const
	{
		widget -= WID_GD_OPTIONS_START;
		if (widget < 0 || (widget % 3) != 2) return;

		widget /= 3;

		uint i;
		const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i) + widget;
		int32 value = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
		SetDParam(0, sd->desc.str_val + value);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		/* Only for the 'descriptions' */
		int index = widget - WID_GD_OPTIONS_START;
		if (index < 0 || (index % 3) != 2) return;

		index /= 3;

		uint i;
		const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i) + index;
		const SettingDescBase *sdb = &sd->desc;

		/* Get the string and try all strings from the smallest to the highest value */
		StringID str = this->GetWidget<NWidgetCore>(widget)->widget_data;
		for (int32 value = sdb->min; (uint32)value <= sdb->max; value += sdb->interval) {
			SetDParam(0, sdb->str_val + value);
			*size = maxdim(*size, GetStringBoundingBox(str));
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (widget >= WID_GD_OPTIONS_START) {
			widget -= WID_GD_OPTIONS_START;
			if ((widget % 3) == 2) return;

			/* Don't allow clients to make any changes */
			if (_networking && !_network_server) return;

			uint i;
			const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i) + (widget / 3);
			const SettingDescBase *sdb = &sd->desc;

			int32 val = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
			if (widget % 3 == 1) {
				/* Increase button clicked */
				val = min(val + sdb->interval, (int32)sdb->max);
			} else {
				/* Decrease button clicked */
				val -= sdb->interval;
				val = max(val, sdb->min);
			}

			/* save value in temporary variable */
			WriteValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv, val);
			this->RaiseWidget(WID_GD_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
			SetDifficultyLevel(3, &this->opt_mod_temp.difficulty); // set difficulty level to custom
			this->LowerWidget(WID_GD_LVL_CUSTOM);
			this->InvalidateData();

			if (widget / 3 == 0 &&
					AI::GetInfoList()->size() == 0 &&
					this->opt_mod_temp.difficulty.max_no_competitors != 0) {
				ShowErrorMessage(STR_WARNING_NO_SUITABLE_AI, INVALID_STRING_ID, WL_CRITICAL);
			}
			return;
		}

		switch (widget) {
			case WID_GD_LVL_EASY:
			case WID_GD_LVL_MEDIUM:
			case WID_GD_LVL_HARD:
			case WID_GD_LVL_CUSTOM:
				/* temporarily change difficulty level */
				this->RaiseWidget(WID_GD_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
				SetDifficultyLevel(widget - WID_GD_LVL_EASY, &this->opt_mod_temp.difficulty);
				this->LowerWidget(WID_GD_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
				this->InvalidateData();
				break;

			case WID_GD_HIGHSCORE: // Highscore Table
				ShowHighscoreTable(this->opt_mod_temp.difficulty.diff_level, -1);
				break;

			case WID_GD_ACCEPT: { // Save button - save changes
				GameSettings *opt_ptr = &GetGameSettings();

				uint i;
				GetSettingFromName("difficulty.diff_level", &i);
				DoCommandP(0, i, this->opt_mod_temp.difficulty.diff_level, CMD_CHANGE_SETTING);

				const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i);
				for (uint btn = 0; btn != GAME_DIFFICULTY_NUM; btn++, sd++) {
					int32 new_val = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
					int32 cur_val = (int32)ReadValue(GetVariableAddress(opt_ptr, &sd->save), sd->save.conv);
					/* if setting has changed, change it */
					if (new_val != cur_val) {
						DoCommandP(0, i + btn, new_val, CMD_CHANGE_SETTING);
					}
				}
				delete this;
				/* If we are in the editor, we should reload the economy.
				 * This way when you load a game, the max loan and interest rate
				 * are loaded correctly. */
				if (_game_mode == GM_EDITOR) StartupEconomy();
				break;
			}

			case WID_GD_CANCEL: // Cancel button - close window, abandon changes
				delete this;
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data. @see GameOptionsInvalidationData
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;

		if (data == GOID_DIFFICULTY_CHANGED) {
			/* Window was created or settings were changed on server. Reread everything. */

			/* Copy current settings (ingame or in intro) to temporary holding place
			 * change that when setting stuff, copy back on clicking 'OK' */
			this->opt_mod_temp = GetGameSettings();

			this->LowerWidget(WID_GD_LVL_EASY + this->opt_mod_temp.difficulty.diff_level);
		}

		uint i;
		const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i);
		for (i = 0; i < GAME_DIFFICULTY_NUM; i++, sd++) {
			const SettingDescBase *sdb = &sd->desc;
			/* skip deprecated difficulty options */
			if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;
			int32 value = (int32)ReadValue(GetVariableAddress(&this->opt_mod_temp, &sd->save), sd->save.conv);
			bool disable = (sd->desc.flags & SGF_NEWGAME_ONLY) &&
					(_game_mode == GM_NORMAL ||
					(_game_mode == GM_EDITOR && (sd->desc.flags & SGF_SCENEDIT_TOO) == 0));

			this->SetWidgetDisabledState(WID_GD_OPTIONS_START + i * 3 + 0, disable || sdb->min == value);
			this->SetWidgetDisabledState(WID_GD_OPTIONS_START + i * 3 + 1, disable || sdb->max == (uint32)value);
		}
	}
};

static NWidgetBase *MakeDifficultyOptionsWidgets(int *biggest_index)
{
	NWidgetVertical *vert_desc = new NWidgetVertical;

	int widnum = WID_GD_OPTIONS_START;
	uint i, j;
	const SettingDesc *sd = GetSettingFromName("difficulty.max_no_competitors", &i);

	for (i = 0, j = 0; i < GameDifficultyWindow::GAME_DIFFICULTY_NUM; i++, sd++, widnum += GameDifficultyWindow::WIDGETS_PER_DIFFICULTY) {
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;

		NWidgetHorizontal *hor = new NWidgetHorizontal;

		/* [<] button. */
		NWidgetLeaf *leaf = new NWidgetLeaf(WWT_PUSHARROWBTN, COLOUR_YELLOW, widnum, AWV_DECREASE, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
		hor->Add(leaf);

		/* [>] button. */
		leaf = new NWidgetLeaf(WWT_PUSHARROWBTN, COLOUR_YELLOW, widnum + 1, AWV_INCREASE, STR_TOOLTIP_HSCROLL_BAR_SCROLLS_LIST);
		hor->Add(leaf);

		/* Some spacing between the text and the description */
		NWidgetSpacer *spacer = new NWidgetSpacer(5, 0);
		hor->Add(spacer);

		/* Descriptive text. */
		leaf = new NWidgetLeaf(WWT_TEXT, COLOUR_YELLOW, widnum + 2, STR_DIFFICULTY_LEVEL_SETTING_MAXIMUM_NO_COMPETITORS + (j++), STR_NULL);
		leaf->SetFill(1, 0);
		hor->Add(leaf);
		vert_desc->Add(hor);

		/* Space vertically */
		vert_desc->Add(new NWidgetSpacer(0, 2));
	}
	*biggest_index = widnum - 1;
	return vert_desc;
}


/** Widget definition for the game difficulty settings window */
static const NWidgetPart _nested_game_difficulty_widgets[] = {
	NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_DIFFICULTY_LEVEL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(NWID_VERTICAL), SetPIP(2, 0, 2),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 0, 10),
				NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GD_LVL_EASY), SetDataTip(STR_DIFFICULTY_LEVEL_EASY, STR_NULL), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GD_LVL_MEDIUM), SetDataTip(STR_DIFFICULTY_LEVEL_MEDIUM, STR_NULL), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GD_LVL_HARD), SetDataTip(STR_DIFFICULTY_LEVEL_HARD, STR_NULL), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GD_LVL_CUSTOM), SetDataTip(STR_DIFFICULTY_LEVEL_CUSTOM, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 10),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WID_GD_HIGHSCORE), SetDataTip(STR_DIFFICULTY_LEVEL_HIGH_SCORE_BUTTON, STR_NULL), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(NWID_VERTICAL), SetPIP(3, 0, 1),
			NWidget(NWID_HORIZONTAL), SetPIP(5, 0, 5),
				NWidgetFunction(MakeDifficultyOptionsWidgets),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(NWID_VERTICAL), SetPIP(2, 0, 2),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 0, 10),
				NWidget(NWID_SPACER), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GD_ACCEPT), SetDataTip(STR_DIFFICULTY_LEVEL_SAVE, STR_NULL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GD_CANCEL), SetDataTip(STR_BUTTON_CANCEL, STR_NULL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the game difficulty settings window */
static const WindowDesc _game_difficulty_desc(
	WDP_CENTER, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_game_difficulty_widgets, lengthof(_nested_game_difficulty_widgets)
);

/** Open the game-difficulty window. */
void ShowGameDifficulty()
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new GameDifficultyWindow(&_game_difficulty_desc);
}

static int SETTING_HEIGHT = 11;    ///< Height of a single setting in the tree view in pixels
static const int LEVEL_WIDTH = 15; ///< Indenting width of a sub-page in pixels

/**
 * Flags for #SettingEntry
 * @note The #SEF_BUTTONS_MASK matches expectations of the formal parameter 'state' of #DrawArrowButtons
 */
enum SettingEntryFlags {
	SEF_LEFT_DEPRESSED  = 0x01, ///< Of a numeric setting entry, the left button is depressed
	SEF_RIGHT_DEPRESSED = 0x02, ///< Of a numeric setting entry, the right button is depressed
	SEF_BUTTONS_MASK = (SEF_LEFT_DEPRESSED | SEF_RIGHT_DEPRESSED), ///< Bit-mask for button flags

	SEF_LAST_FIELD = 0x04, ///< This entry is the last one in a (sub-)page

	/* Entry kind */
	SEF_SETTING_KIND = 0x10, ///< Entry kind: Entry is a setting
	SEF_SUBTREE_KIND = 0x20, ///< Entry kind: Entry is a sub-tree
	SEF_KIND_MASK    = (SEF_SETTING_KIND | SEF_SUBTREE_KIND), ///< Bit-mask for fetching entry kind
};

struct SettingsPage; // Forward declaration

/** Data fields for a sub-page (#SEF_SUBTREE_KIND kind)*/
struct SettingEntrySubtree {
	SettingsPage *page; ///< Pointer to the sub-page
	bool folded;        ///< Sub-page is folded (not visible except for its title)
	StringID title;     ///< Title of the sub-page
};

/** Data fields for a single setting (#SEF_SETTING_KIND kind) */
struct SettingEntrySetting {
	const char *name;           ///< Name of the setting
	const SettingDesc *setting; ///< Setting description of the setting
	uint index;                 ///< Index of the setting in the settings table
};

/** Data structure describing a single setting in a tab */
struct SettingEntry {
	byte flags; ///< Flags of the setting entry. @see SettingEntryFlags
	byte level; ///< Nesting level of this setting entry
	union {
		SettingEntrySetting entry; ///< Data fields if entry is a setting
		SettingEntrySubtree sub;   ///< Data fields if entry is a sub-page
	} d; ///< Data fields for each kind

	SettingEntry(const char *nm);
	SettingEntry(SettingsPage *sub, StringID title);

	void Init(byte level, bool last_field);
	void FoldAll();
	void UnFoldAll();
	void SetButtons(byte new_val);

	uint Length() const;
	void GetFoldingState(bool &all_folded, bool &all_unfolded) const;
	bool IsVisible(const SettingEntry *item) const;
	SettingEntry *FindEntry(uint row, uint *cur_row);
	uint GetMaxHelpHeight(int maxw);

	uint Draw(GameSettings *settings_ptr, int base_x, int base_y, int max_x, uint first_row, uint max_row, uint cur_row, uint parent_last, SettingEntry *selected);

	/**
	 * Get the help text of a single setting.
	 * @return The requested help text.
	 */
	inline StringID GetHelpText()
	{
		assert((this->flags & SEF_KIND_MASK) == SEF_SETTING_KIND);
		return this->d.entry.setting->desc.str_help;
	}

	void SetValueDParams(uint first_param, int32 value);

private:
	void DrawSetting(GameSettings *settings_ptr, int x, int y, int max_x, int state, bool highlight);
};

/** Data structure describing one page of settings in the settings window. */
struct SettingsPage {
	SettingEntry *entries; ///< Array of setting entries of the page.
	byte num;              ///< Number of entries on the page (statically filled).

	void Init(byte level = 0);
	void FoldAll();
	void UnFoldAll();

	uint Length() const;
	void GetFoldingState(bool &all_folded, bool &all_unfolded) const;
	bool IsVisible(const SettingEntry *item) const;
	SettingEntry *FindEntry(uint row, uint *cur_row) const;
	uint GetMaxHelpHeight(int maxw);

	uint Draw(GameSettings *settings_ptr, int base_x, int base_y, int max_x, uint first_row, uint max_row, SettingEntry *selected, uint cur_row = 0, uint parent_last = 0) const;
};


/* == SettingEntry methods == */

/**
 * Constructor for a single setting in the 'advanced settings' window
 * @param nm Name of the setting in the setting table
 */
SettingEntry::SettingEntry(const char *nm)
{
	this->flags = SEF_SETTING_KIND;
	this->level = 0;
	this->d.entry.name = nm;
	this->d.entry.setting = NULL;
	this->d.entry.index = 0;
}

/**
 * Constructor for a sub-page in the 'advanced settings' window
 * @param sub   Sub-page
 * @param title Title of the sub-page
 */
SettingEntry::SettingEntry(SettingsPage *sub, StringID title)
{
	this->flags = SEF_SUBTREE_KIND;
	this->level = 0;
	this->d.sub.page = sub;
	this->d.sub.folded = true;
	this->d.sub.title = title;
}

/**
 * Initialization of a setting entry
 * @param level      Page nesting level of this entry
 * @param last_field Boolean indicating this entry is the last at the (sub-)page
 */
void SettingEntry::Init(byte level, bool last_field)
{
	this->level = level;
	if (last_field) this->flags |= SEF_LAST_FIELD;

	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			this->d.entry.setting = GetSettingFromName(this->d.entry.name, &this->d.entry.index);
			assert(this->d.entry.setting != NULL);
			break;
		case SEF_SUBTREE_KIND:
			this->d.sub.page->Init(level + 1);
			break;
		default: NOT_REACHED();
	}
}

/** Recursively close all folds of sub-pages */
void SettingEntry::FoldAll()
{
	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			break;

		case SEF_SUBTREE_KIND:
			this->d.sub.folded = true;
			this->d.sub.page->FoldAll();
			break;

		default: NOT_REACHED();
	}
}

/** Recursively open all folds of sub-pages */
void SettingEntry::UnFoldAll()
{
	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			break;

		case SEF_SUBTREE_KIND:
			this->d.sub.folded = false;
			this->d.sub.page->UnFoldAll();
			break;

		default: NOT_REACHED();
	}
}

/**
 * Recursively accumulate the folding state of the tree.
 * @param[in,out] all_folded Set to false, if one entry is not folded.
 * @param[in,out] all_unfolded Set to false, if one entry is folded.
 */
void SettingEntry::GetFoldingState(bool &all_folded, bool &all_unfolded) const
{
	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			break;

		case SEF_SUBTREE_KIND:
			if (this->d.sub.folded) {
				all_unfolded = false;
			} else {
				all_folded = false;
			}
			this->d.sub.page->GetFoldingState(all_folded, all_unfolded);
			break;

		default: NOT_REACHED();
	}
}

/**
 * Check whether an entry is visible and not folded away.
 * Note: This does not consider the scrolling range; it might still require scrolling ot make the setting really visible.
 * @param item Entry to search for.
 * @return true if entry is visible.
 */
bool SettingEntry::IsVisible(const SettingEntry *item) const
{
	if (this == item) return true;

	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			return false;

		case SEF_SUBTREE_KIND:
			return !this->d.sub.folded && this->d.sub.page->IsVisible(item);

		default: NOT_REACHED();
	}
}

/**
 * Set the button-depressed flags (#SEF_LEFT_DEPRESSED and #SEF_RIGHT_DEPRESSED) to a specified value
 * @param new_val New value for the button flags
 * @see SettingEntryFlags
 */
void SettingEntry::SetButtons(byte new_val)
{
	assert((new_val & ~SEF_BUTTONS_MASK) == 0); // Should not touch any flags outside the buttons
	this->flags = (this->flags & ~SEF_BUTTONS_MASK) | new_val;
}

/** Return numbers of rows needed to display the entry */
uint SettingEntry::Length() const
{
	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			return 1;
		case SEF_SUBTREE_KIND:
			if (this->d.sub.folded) return 1; // Only displaying the title

			return 1 + this->d.sub.page->Length(); // 1 extra row for the title
		default: NOT_REACHED();
	}
}

/**
 * Find setting entry at row \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Current row number
 * @return The requested setting entry or \c NULL if it not found
 */
SettingEntry *SettingEntry::FindEntry(uint row_num, uint *cur_row)
{
	if (row_num == *cur_row) return this;

	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			(*cur_row)++;
			break;
		case SEF_SUBTREE_KIND:
			(*cur_row)++; // add one for row containing the title
			if (this->d.sub.folded) {
				break;
			}

			/* sub-page is visible => search it too */
			return this->d.sub.page->FindEntry(row_num, cur_row);
		default: NOT_REACHED();
	}
	return NULL;
}

/**
 * Get the biggest height of the help text(s), if the width is at least \a maxw. Help text gets wrapped if needed.
 * @param maxw Maximal width of a line help text.
 * @return Biggest height needed to display any help text of this node (and its descendants).
 */
uint SettingEntry::GetMaxHelpHeight(int maxw)
{
	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND: return GetStringHeight(this->GetHelpText(), maxw);
		case SEF_SUBTREE_KIND: return this->d.sub.page->GetMaxHelpHeight(maxw);
		default:               NOT_REACHED();
	}
}

/**
 * Draw a row in the settings panel.
 *
 * See SettingsPage::Draw() for an explanation about how drawing is performed.
 *
 * The \a parent_last parameter ensures that the vertical lines at the left are
 * only drawn when another entry follows, that it prevents output like
 * \verbatim
 *  |-- setting
 *  |-- (-) - Title
 *  |    |-- setting
 *  |    |-- setting
 * \endverbatim
 * The left-most vertical line is not wanted. It is prevented by setting the
 * appropiate bit in the \a parent_last parameter.
 *
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing \a first_row
 * @param right        Right-most x position to draw strings at.
 * @param base_y       Upper-most position in window/panel to start drawing \a first_row
 * @param first_row    First row number to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @param selected     Selected entry by the user.
 * @return Row number of the next row to draw
 */
uint SettingEntry::Draw(GameSettings *settings_ptr, int left, int right, int base_y, uint first_row, uint max_row, uint cur_row, uint parent_last, SettingEntry *selected)
{
	if (cur_row >= max_row) return cur_row;

	bool rtl = _current_text_dir == TD_RTL;
	int offset = rtl ? -4 : 4;
	int level_width = rtl ? -LEVEL_WIDTH : LEVEL_WIDTH;

	int x = rtl ? right : left;
	int y = base_y;
	if (cur_row >= first_row) {
		int colour = _colour_gradient[COLOUR_ORANGE][4];
		y = base_y + (cur_row - first_row) * SETTING_HEIGHT; // Compute correct y start position

		/* Draw vertical for parent nesting levels */
		for (uint lvl = 0; lvl < this->level; lvl++) {
			if (!HasBit(parent_last, lvl)) GfxDrawLine(x + offset, y, x + offset, y + SETTING_HEIGHT - 1, colour);
			x += level_width;
		}
		/* draw own |- prefix */
		int halfway_y = y + SETTING_HEIGHT / 2;
		int bottom_y = (flags & SEF_LAST_FIELD) ? halfway_y : y + SETTING_HEIGHT - 1;
		GfxDrawLine(x + offset, y, x + offset, bottom_y, colour);
		/* Small horizontal line from the last vertical line */
		GfxDrawLine(x + offset, halfway_y, x + level_width - offset, halfway_y, colour);
		x += level_width;
	}

	switch (this->flags & SEF_KIND_MASK) {
		case SEF_SETTING_KIND:
			if (cur_row >= first_row) {
				this->DrawSetting(settings_ptr, rtl ? left : x, rtl ? x : right, y, this->flags & SEF_BUTTONS_MASK,
						this == selected);
			}
			cur_row++;
			break;
		case SEF_SUBTREE_KIND:
			if (cur_row >= first_row) {
				DrawSprite((this->d.sub.folded ? SPR_CIRCLE_FOLDED : SPR_CIRCLE_UNFOLDED), PAL_NONE, rtl ? x - 8 : x, y + (SETTING_HEIGHT - 11) / 2);
				DrawString(rtl ? left : x + 12, rtl ? x - 12 : right, y, this->d.sub.title);
			}
			cur_row++;
			if (!this->d.sub.folded) {
				if (this->flags & SEF_LAST_FIELD) {
					assert(this->level < sizeof(parent_last));
					SetBit(parent_last, this->level); // Add own last-field state
				}

				cur_row = this->d.sub.page->Draw(settings_ptr, left, right, base_y, first_row, max_row, selected, cur_row, parent_last);
			}
			break;
		default: NOT_REACHED();
	}
	return cur_row;
}

static const void *ResolveVariableAddress(const GameSettings *settings_ptr, const SettingDesc *sd)
{
	if ((sd->desc.flags & SGF_PER_COMPANY) != 0) {
		if (Company::IsValidID(_local_company) && _game_mode != GM_MENU) {
			return GetVariableAddress(&Company::Get(_local_company)->settings, &sd->save);
		} else {
			return GetVariableAddress(&_settings_client.company, &sd->save);
		}
	} else {
		return GetVariableAddress(settings_ptr, &sd->save);
	}
}

/**
 * Set the DParams for drawing the value of a setting.
 * @param first_param First DParam to use
 * @param value Setting value to set params for.
 */
void SettingEntry::SetValueDParams(uint first_param, int32 value)
{
	assert((this->flags & SEF_KIND_MASK) == SEF_SETTING_KIND);
	const SettingDescBase *sdb = &this->d.entry.setting->desc;
	if (sdb->cmd == SDT_BOOLX) {
		SetDParam(first_param++, value != 0 ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
	} else {
		if ((sdb->flags & SGF_MULTISTRING) != 0) {
			SetDParam(first_param++, sdb->str_val - sdb->min + value);
		} else if ((sdb->flags & SGF_DISPLAY_ABS) != 0) {
			SetDParam(first_param++, sdb->str_val + ((value >= 0) ? 1 : 0));
			value = abs(value);
		} else {
			SetDParam(first_param++, sdb->str_val + ((value == 0 && (sdb->flags & SGF_0ISDISABLED) != 0) ? 1 : 0));
		}
		SetDParam(first_param++, value);
	}
}

/**
 * Private function to draw setting value (button + text + current value)
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing
 * @param right        Right-most position in window/panel to draw
 * @param y            Upper-most position in window/panel to start drawing
 * @param state        State of the left + right arrow buttons to draw for the setting
 * @param highlight    Highlight entry.
 */
void SettingEntry::DrawSetting(GameSettings *settings_ptr, int left, int right, int y, int state, bool highlight)
{
	const SettingDesc *sd = this->d.entry.setting;
	const SettingDescBase *sdb = &sd->desc;
	const void *var = ResolveVariableAddress(settings_ptr, sd);
	bool editable = true;

	bool rtl = _current_text_dir == TD_RTL;
	uint buttons_left = rtl ? right + 1 - SETTING_BUTTON_WIDTH : left;
	uint text_left  = left + (rtl ? 0 : SETTING_BUTTON_WIDTH + 5);
	uint text_right = right - (rtl ? SETTING_BUTTON_WIDTH + 5 : 0);
	uint button_y = y + (SETTING_HEIGHT - SETTING_BUTTON_HEIGHT) / 2;

	/* We do not allow changes of some items when we are a client in a networkgame */
	if (!(sd->save.conv & SLF_NO_NETWORK_SYNC) && _networking && !_network_server && !(sdb->flags & SGF_PER_COMPANY)) editable = false;
	if ((sdb->flags & SGF_NETWORK_ONLY) && !_networking) editable = false;
	if ((sdb->flags & SGF_NO_NETWORK) && _networking) editable = false;

	SetDParam(0, highlight ? STR_ORANGE_STRING1_WHITE : STR_ORANGE_STRING1_LTBLUE);
	int32 value = (int32)ReadValue(var, sd->save.conv);
	if (sdb->cmd == SDT_BOOLX) {
		/* Draw checkbox for boolean-value either on/off */
		DrawBoolButton(buttons_left, button_y, value != 0, editable);
	} else if ((sdb->flags & SGF_MULTISTRING) != 0) {
		/* Draw [v] button for settings of an enum-type */
		DrawDropDownButton(buttons_left, button_y, COLOUR_YELLOW, state != 0, editable);
	} else {
		/* Draw [<][>] boxes for settings of an integer-type */
		DrawArrowButtons(buttons_left, button_y, COLOUR_YELLOW, state,
				editable && value != (sdb->flags & SGF_0ISDISABLED ? 0 : sdb->min), editable && (uint32)value != sdb->max);
	}
	this->SetValueDParams(1, value);
	DrawString(text_left, text_right, y, sdb->str, highlight ? TC_WHITE : TC_LIGHT_BLUE);
}


/* == SettingsPage methods == */

/**
 * Initialization of an entire setting page
 * @param level Nesting level of this page (internal variable, do not provide a value for it when calling)
 */
void SettingsPage::Init(byte level)
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].Init(level, field + 1 == num);
	}
}

/** Recursively close all folds of sub-pages */
void SettingsPage::FoldAll()
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].FoldAll();
	}
}

/** Recursively open all folds of sub-pages */
void SettingsPage::UnFoldAll()
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].UnFoldAll();
	}
}

/**
 * Recursively accumulate the folding state of the tree.
 * @param[in,out] all_folded Set to false, if one entry is not folded.
 * @param[in,out] all_unfolded Set to false, if one entry is folded.
 */
void SettingsPage::GetFoldingState(bool &all_folded, bool &all_unfolded) const
{
	for (uint field = 0; field < this->num; field++) {
		this->entries[field].GetFoldingState(all_folded, all_unfolded);
	}
}

/**
 * Check whether an entry is visible and not folded away.
 * Note: This does not consider the scrolling range; it might still require scrolling ot make the setting really visible.
 * @param item Entry to search for.
 * @return true if entry is visible.
 */
bool SettingsPage::IsVisible(const SettingEntry *item) const
{
	for (uint field = 0; field < this->num; field++) {
		if (this->entries[field].IsVisible(item)) return true;
	}
	return false;
}

/** Return number of rows needed to display the whole page */
uint SettingsPage::Length() const
{
	uint length = 0;
	for (uint field = 0; field < this->num; field++) {
		length += this->entries[field].Length();
	}
	return length;
}

/**
 * Find the setting entry at row number \a row_num
 * @param row_num Index of entry to return
 * @param cur_row Variable used for keeping track of the current row number. Should point to memory initialized to \c 0 when first called.
 * @return The requested setting entry or \c NULL if it does not exist
 */
SettingEntry *SettingsPage::FindEntry(uint row_num, uint *cur_row) const
{
	SettingEntry *pe = NULL;

	for (uint field = 0; field < this->num; field++) {
		pe = this->entries[field].FindEntry(row_num, cur_row);
		if (pe != NULL) {
			break;
		}
	}
	return pe;
}

/**
 * Get the biggest height of the help texts, if the width is at least \a maxw. Help text gets wrapped if needed.
 * @param maxw Maximal width of a line help text.
 * @return Biggest height needed to display any help text of this (sub-)tree.
 */
uint SettingsPage::GetMaxHelpHeight(int maxw)
{
	uint biggest = 0;
	for (uint field = 0; field < this->num; field++) {
		biggest = max(biggest, this->entries[field].GetMaxHelpHeight(maxw));
	}
	return biggest;
}

/**
 * Draw a selected part of the settings page.
 *
 * The scrollbar uses rows of the page, while the page data strucure is a tree of #SettingsPage and #SettingEntry objects.
 * As a result, the drawing routing traverses the tree from top to bottom, counting rows in \a cur_row until it reaches \a first_row.
 * Then it enables drawing rows while traversing until \a max_row is reached, at which point drawing is terminated.
 *
 * @param settings_ptr Pointer to current values of all settings
 * @param left         Left-most position in window/panel to start drawing of each setting row
 * @param right        Right-most position in window/panel to draw at
 * @param base_y       Upper-most position in window/panel to start drawing of row number \a first_row
 * @param first_row    Number of first row to draw
 * @param max_row      Row-number to stop drawing (the row-number of the row below the last row to draw)
 * @param cur_row      Current row number (internal variable)
 * @param parent_last  Last-field booleans of parent page level (page level \e i sets bit \e i to 1 if it is its last field)
 * @param selected     Selected entry by the user.
 * @return Row number of the next row to draw
 */
uint SettingsPage::Draw(GameSettings *settings_ptr, int left, int right, int base_y, uint first_row, uint max_row, SettingEntry *selected, uint cur_row, uint parent_last) const
{
	if (cur_row >= max_row) return cur_row;

	for (uint i = 0; i < this->num; i++) {
		cur_row = this->entries[i].Draw(settings_ptr, left, right, base_y, first_row, max_row, cur_row, parent_last, selected);
		if (cur_row >= max_row) {
			break;
		}
	}
	return cur_row;
}


static SettingEntry _settings_ui_display[] = {
	SettingEntry("gui.date_format_in_default_names"),
	SettingEntry("gui.population_in_label"),
	SettingEntry("gui.measure_tooltip"),
	SettingEntry("gui.loading_indicators"),
	SettingEntry("gui.liveries"),
	SettingEntry("gui.show_track_reservation"),
	SettingEntry("gui.expenses_layout"),
	SettingEntry("gui.smallmap_land_colour"),
	SettingEntry("gui.zoom_min"),
	SettingEntry("gui.zoom_max"),
	SettingEntry("gui.graph_line_thickness"),
};
/** Display options sub-page */
static SettingsPage _settings_ui_display_page = {_settings_ui_display, lengthof(_settings_ui_display)};

static SettingEntry _settings_ui_interaction[] = {
	SettingEntry("gui.window_snap_radius"),
	SettingEntry("gui.window_soft_limit"),
	SettingEntry("gui.link_terraform_toolbar"),
	SettingEntry("gui.prefer_teamchat"),
	SettingEntry("gui.auto_scrolling"),
	SettingEntry("gui.reverse_scroll"),
	SettingEntry("gui.smooth_scroll"),
	SettingEntry("gui.left_mouse_btn_scrolling"),
	/* While the horizontal scrollwheel scrolling is written as general code, only
	 *  the cocoa (OSX) driver generates input for it.
	 *  Since it's also able to completely disable the scrollwheel will we display it on all platforms anyway */
	SettingEntry("gui.scrollwheel_scrolling"),
	SettingEntry("gui.scrollwheel_multiplier"),
#ifdef __APPLE__
	/* We might need to emulate a right mouse button on mac */
	SettingEntry("gui.right_mouse_btn_emulation"),
#endif
};
/** Interaction sub-page */
static SettingsPage _settings_ui_interaction_page = {_settings_ui_interaction, lengthof(_settings_ui_interaction)};

static SettingEntry _settings_ui[] = {
	SettingEntry(&_settings_ui_display_page, STR_CONFIG_SETTING_DISPLAY_OPTIONS),
	SettingEntry(&_settings_ui_interaction_page, STR_CONFIG_SETTING_INTERACTION),
	SettingEntry("gui.show_finances"),
	SettingEntry("gui.errmsg_duration"),
	SettingEntry("gui.hover_delay"),
	SettingEntry("gui.toolbar_pos"),
	SettingEntry("gui.statusbar_pos"),
	SettingEntry("gui.newgrf_default_palette"),
	SettingEntry("gui.pause_on_newgame"),
	SettingEntry("gui.advanced_vehicle_list"),
	SettingEntry("gui.timetable_in_ticks"),
	SettingEntry("gui.timetable_arrival_departure"),
	SettingEntry("gui.quick_goto"),
	SettingEntry("gui.default_rail_type"),
	SettingEntry("gui.disable_unsuitable_building"),
	SettingEntry("gui.persistent_buildingtools"),
	SettingEntry("gui.coloured_news_year"),
};
/** Interface subpage */
static SettingsPage _settings_ui_page = {_settings_ui, lengthof(_settings_ui)};

static SettingEntry _settings_construction_signals[] = {
	SettingEntry("construction.train_signal_side"),
	SettingEntry("gui.enable_signal_gui"),
	SettingEntry("gui.drag_signals_density"),
	SettingEntry("gui.drag_signals_fixed_distance"),
	SettingEntry("gui.semaphore_build_before"),
	SettingEntry("gui.default_signal_type"),
	SettingEntry("gui.cycle_signal_types"),
};
/** Signals subpage */
static SettingsPage _settings_construction_signals_page = {_settings_construction_signals, lengthof(_settings_construction_signals)};

static SettingEntry _settings_construction[] = {
	SettingEntry(&_settings_construction_signals_page, STR_CONFIG_SETTING_CONSTRUCTION_SIGNALS),
	SettingEntry("construction.build_on_slopes"),
	SettingEntry("construction.autoslope"),
	SettingEntry("construction.extra_dynamite"),
	SettingEntry("construction.max_bridge_length"),
	SettingEntry("construction.max_tunnel_length"),
	SettingEntry("station.never_expire_airports"),
	SettingEntry("construction.freeform_edges"),
	SettingEntry("construction.extra_tree_placement"),
	SettingEntry("construction.command_pause_level"),
};
/** Construction sub-page */
static SettingsPage _settings_construction_page = {_settings_construction, lengthof(_settings_construction)};

static SettingEntry _settings_stations_cargo[] = {
	SettingEntry("order.improved_load"),
	SettingEntry("order.gradual_loading"),
	SettingEntry("order.selectgoods"),
};
/** Cargo handling sub-page */
static SettingsPage _settings_stations_cargo_page = {_settings_stations_cargo, lengthof(_settings_stations_cargo)};

static SettingEntry _settings_stations[] = {
	SettingEntry(&_settings_stations_cargo_page, STR_CONFIG_SETTING_STATIONS_CARGOHANDLING),
	SettingEntry("station.adjacent_stations"),
	SettingEntry("station.distant_join_stations"),
	SettingEntry("station.station_spread"),
	SettingEntry("economy.station_noise_level"),
	SettingEntry("station.modified_catchment"),
	SettingEntry("construction.road_stop_on_town_road"),
	SettingEntry("construction.road_stop_on_competitor_road"),
};
/** Stations sub-page */
static SettingsPage _settings_stations_page = {_settings_stations, lengthof(_settings_stations)};

static SettingEntry _settings_economy_towns[] = {
	SettingEntry("economy.bribe"),
	SettingEntry("economy.exclusive_rights"),
	SettingEntry("economy.fund_roads"),
	SettingEntry("economy.fund_buildings"),
	SettingEntry("economy.town_layout"),
	SettingEntry("economy.allow_town_roads"),
	SettingEntry("economy.allow_town_level_crossings"),
	SettingEntry("economy.found_town"),
	SettingEntry("economy.mod_road_rebuild"),
	SettingEntry("economy.town_growth_rate"),
	SettingEntry("economy.larger_towns"),
	SettingEntry("economy.initial_city_size"),
};
/** Towns sub-page */
static SettingsPage _settings_economy_towns_page = {_settings_economy_towns, lengthof(_settings_economy_towns)};

static SettingEntry _settings_economy_industries[] = {
	SettingEntry("construction.raw_industry_construction"),
	SettingEntry("construction.industry_platform"),
	SettingEntry("economy.multiple_industry_per_town"),
	SettingEntry("game_creation.oil_refinery_limit"),
};
/** Industries sub-page */
static SettingsPage _settings_economy_industries_page = {_settings_economy_industries, lengthof(_settings_economy_industries)};

static SettingEntry _settings_economy_scripts[] = {
	SettingEntry("script.script_max_opcode_till_suspend"),
};
/** Scripts sub-page */
static SettingsPage _settings_economy_scripts_page = {_settings_economy_scripts, lengthof(_settings_economy_scripts)};

static SettingEntry _settings_economy[] = {
	SettingEntry(&_settings_economy_towns_page, STR_CONFIG_SETTING_ECONOMY_TOWNS),
	SettingEntry(&_settings_economy_industries_page, STR_CONFIG_SETTING_ECONOMY_INDUSTRIES),
	SettingEntry(&_settings_economy_scripts_page, STR_CONFIG_SETTING_ECONOMY_SCRIPTS),
	SettingEntry("economy.inflation"),
	SettingEntry("economy.smooth_economy"),
	SettingEntry("economy.feeder_payment_share"),
	SettingEntry("economy.infrastructure_maintenance"),
};
/** Economy sub-page */
static SettingsPage _settings_economy_page = {_settings_economy, lengthof(_settings_economy)};

static SettingEntry _settings_ai_npc[] = {
	SettingEntry("ai.ai_in_multiplayer"),
	SettingEntry("ai.ai_disable_veh_train"),
	SettingEntry("ai.ai_disable_veh_roadveh"),
	SettingEntry("ai.ai_disable_veh_aircraft"),
	SettingEntry("ai.ai_disable_veh_ship"),
};
/** Computer players sub-page */
static SettingsPage _settings_ai_npc_page = {_settings_ai_npc, lengthof(_settings_ai_npc)};

static SettingEntry _settings_ai[] = {
	SettingEntry(&_settings_ai_npc_page, STR_CONFIG_SETTING_AI_NPC),
	SettingEntry("economy.give_money"),
	SettingEntry("economy.allow_shares"),
};
/** AI sub-page */
static SettingsPage _settings_ai_page = {_settings_ai, lengthof(_settings_ai)};

static SettingEntry _settings_vehicles_routing[] = {
	SettingEntry("pf.pathfinder_for_trains"),
	SettingEntry("pf.forbid_90_deg"),
	SettingEntry("pf.pathfinder_for_roadvehs"),
	SettingEntry("pf.roadveh_queue"),
	SettingEntry("pf.pathfinder_for_ships"),
};
/** Autorenew sub-page */
static SettingsPage _settings_vehicles_routing_page = {_settings_vehicles_routing, lengthof(_settings_vehicles_routing)};

static SettingEntry _settings_vehicles_autorenew[] = {
	SettingEntry("company.engine_renew"),
	SettingEntry("company.engine_renew_months"),
	SettingEntry("company.engine_renew_money"),
};
/** Autorenew sub-page */
static SettingsPage _settings_vehicles_autorenew_page = {_settings_vehicles_autorenew, lengthof(_settings_vehicles_autorenew)};

static SettingEntry _settings_vehicles_servicing[] = {
	SettingEntry("vehicle.servint_ispercent"),
	SettingEntry("vehicle.servint_trains"),
	SettingEntry("vehicle.servint_roadveh"),
	SettingEntry("vehicle.servint_ships"),
	SettingEntry("vehicle.servint_aircraft"),
	SettingEntry("order.no_servicing_if_no_breakdowns"),
	SettingEntry("order.serviceathelipad"),
};
/** Servicing sub-page */
static SettingsPage _settings_vehicles_servicing_page = {_settings_vehicles_servicing, lengthof(_settings_vehicles_servicing)};

static SettingEntry _settings_vehicles_trains[] = {
	SettingEntry("pf.reverse_at_signals"),
	SettingEntry("vehicle.train_acceleration_model"),
	SettingEntry("vehicle.train_slope_steepness"),
	SettingEntry("vehicle.max_train_length"),
	SettingEntry("vehicle.wagon_speed_limits"),
	SettingEntry("vehicle.disable_elrails"),
	SettingEntry("vehicle.freight_trains"),
	SettingEntry("gui.stop_location"),
};
/** Trains sub-page */
static SettingsPage _settings_vehicles_trains_page = {_settings_vehicles_trains, lengthof(_settings_vehicles_trains)};

static SettingEntry _settings_vehicles[] = {
	SettingEntry(&_settings_vehicles_routing_page, STR_CONFIG_SETTING_VEHICLES_ROUTING),
	SettingEntry(&_settings_vehicles_autorenew_page, STR_CONFIG_SETTING_VEHICLES_AUTORENEW),
	SettingEntry(&_settings_vehicles_servicing_page, STR_CONFIG_SETTING_VEHICLES_SERVICING),
	SettingEntry(&_settings_vehicles_trains_page, STR_CONFIG_SETTING_VEHICLES_TRAINS),
	SettingEntry("gui.new_nonstop"),
	SettingEntry("gui.order_review_system"),
	SettingEntry("gui.vehicle_income_warn"),
	SettingEntry("gui.lost_vehicle_warn"),
	SettingEntry("vehicle.never_expire_vehicles"),
	SettingEntry("vehicle.max_trains"),
	SettingEntry("vehicle.max_roadveh"),
	SettingEntry("vehicle.max_aircraft"),
	SettingEntry("vehicle.max_ships"),
	SettingEntry("vehicle.plane_speed"),
	SettingEntry("vehicle.plane_crashes"),
	SettingEntry("vehicle.dynamic_engines"),
	SettingEntry("vehicle.roadveh_acceleration_model"),
	SettingEntry("vehicle.roadveh_slope_steepness"),
	SettingEntry("vehicle.smoke_amount"),
};
/** Vehicles sub-page */
static SettingsPage _settings_vehicles_page = {_settings_vehicles, lengthof(_settings_vehicles)};

static SettingEntry _settings_main[] = {
	SettingEntry(&_settings_ui_page,           STR_CONFIG_SETTING_GUI),
	SettingEntry(&_settings_construction_page, STR_CONFIG_SETTING_CONSTRUCTION),
	SettingEntry(&_settings_vehicles_page,     STR_CONFIG_SETTING_VEHICLES),
	SettingEntry(&_settings_stations_page,     STR_CONFIG_SETTING_STATIONS),
	SettingEntry(&_settings_economy_page,      STR_CONFIG_SETTING_ECONOMY),
	SettingEntry(&_settings_ai_page,           STR_CONFIG_SETTING_AI),
};

/** Main page, holding all advanced settings */
static SettingsPage _settings_main_page = {_settings_main, lengthof(_settings_main)};

struct GameSettingsWindow : Window {
	static const int SETTINGTREE_LEFT_OFFSET   = 5; ///< Position of left edge of setting values
	static const int SETTINGTREE_RIGHT_OFFSET  = 5; ///< Position of right edge of setting values
	static const int SETTINGTREE_TOP_OFFSET    = 5; ///< Position of top edge of setting values
	static const int SETTINGTREE_BOTTOM_OFFSET = 5; ///< Position of bottom edge of setting values

	static GameSettings *settings_ptr; ///< Pointer to the game settings being displayed and modified.

	SettingEntry *valuewindow_entry;   ///< If non-NULL, pointer to setting for which a value-entering window has been opened.
	SettingEntry *clicked_entry;       ///< If non-NULL, pointer to a clicked numeric setting (with a depressed left or right button).
	SettingEntry *last_clicked;        ///< If non-NULL, pointer to the last clicked setting.
	SettingEntry *valuedropdown_entry; ///< If non-NULL, pointer to the value for which a dropdown window is currently opened.
	bool closing_dropdown;             ///< True, if the dropdown list is currently closing.

	Scrollbar *vscroll;

	GameSettingsWindow(const WindowDesc *desc) : Window()
	{
		static bool first_time = true;

		settings_ptr = &GetGameSettings();

		/* Build up the dynamic settings-array only once per OpenTTD session */
		if (first_time) {
			_settings_main_page.Init();
			first_time = false;
		} else {
			_settings_main_page.FoldAll(); // Close all sub-pages
		}

		this->valuewindow_entry = NULL; // No setting entry for which a entry window is opened
		this->clicked_entry = NULL; // No numeric setting buttons are depressed
		this->last_clicked = NULL;
		this->valuedropdown_entry = NULL;
		this->closing_dropdown = false;

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(WID_GS_SCROLLBAR);
		this->FinishInitNested(desc, WN_GAME_OPTIONS_GAME_SETTINGS);

		this->InvalidateData();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_GS_OPTIONSPANEL:
				resize->height = SETTING_HEIGHT = max(11, FONT_HEIGHT_NORMAL + 1);
				resize->width  = 1;

				size->height = 5 * resize->height + SETTINGTREE_TOP_OFFSET + SETTINGTREE_BOTTOM_OFFSET;
				break;

			case WID_GS_HELP_TEXT: {
				static const StringID setting_types[] = {
					STR_CONFIG_SETTING_TYPE_CLIENT,
					STR_CONFIG_SETTING_TYPE_COMPANY_MENU, STR_CONFIG_SETTING_TYPE_COMPANY_INGAME,
					STR_CONFIG_SETTING_TYPE_GAME_MENU, STR_CONFIG_SETTING_TYPE_GAME_INGAME,
				};
				for (uint i = 0; i < lengthof(setting_types); i++) {
					SetDParam(0, setting_types[i]);
					size->width = max(size->width, GetStringBoundingBox(STR_CONFIG_SETTING_TYPE).width);
				}
				size->height = 2 * FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL +
						max(size->height, _settings_main_page.GetMaxHelpHeight(size->width));
				break;
			}

			default:
				break;
		}
	}

	virtual void OnPaint()
	{
		if (this->closing_dropdown) {
			this->closing_dropdown = false;
			assert(this->valuedropdown_entry != NULL);
			this->valuedropdown_entry->SetButtons(0);
			this->valuedropdown_entry = NULL;
		}
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_GS_OPTIONSPANEL:
				_settings_main_page.Draw(settings_ptr, r.left + SETTINGTREE_LEFT_OFFSET, r.right - SETTINGTREE_RIGHT_OFFSET, r.top + SETTINGTREE_TOP_OFFSET,
						this->vscroll->GetPosition(), this->vscroll->GetPosition() + this->vscroll->GetCapacity(), this->last_clicked);
				break;

			case WID_GS_HELP_TEXT:
				if (this->last_clicked != NULL) {
					const SettingDesc *sd = this->last_clicked->d.entry.setting;

					int y = r.top;
					if (sd->desc.flags & SGF_PER_COMPANY) {
						SetDParam(0, _game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_COMPANY_MENU : STR_CONFIG_SETTING_TYPE_COMPANY_INGAME);
					} else if (sd->save.conv & SLF_NOT_IN_SAVE) {
						SetDParam(0, STR_CONFIG_SETTING_TYPE_CLIENT);
					} else {
						SetDParam(0, _game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_GAME_MENU : STR_CONFIG_SETTING_TYPE_GAME_INGAME);
					}
					DrawString(r.left, r.right, y, STR_CONFIG_SETTING_TYPE);
					y += FONT_HEIGHT_NORMAL;

					int32 default_value = ReadValue(&sd->desc.def, sd->save.conv);
					this->last_clicked->SetValueDParams(0, default_value);
					DrawString(r.left, r.right, y, STR_CONFIG_SETTING_DEFAULT_VALUE);
					y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;

					DrawStringMultiLine(r.left, r.right, y, r.bottom, this->last_clicked->GetHelpText(), TC_WHITE);
				}
				break;

			default:
				break;
		}
	}

	/**
	 * Set the entry that should have its help text displayed, and mark the window dirty so it gets repainted.
	 * @param pe Setting to display help text of, use \c NULL to stop displaying help of the currently displayed setting.
	 */
	void SetDisplayedHelpText(SettingEntry *pe)
	{
		if (this->last_clicked != pe) this->SetDirty();
		this->last_clicked = pe;
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_GS_EXPAND_ALL:
				_settings_main_page.UnFoldAll();
				this->InvalidateData();
				break;

			case WID_GS_COLLAPSE_ALL:
				_settings_main_page.FoldAll();
				this->InvalidateData();
				break;
		}

		if (widget != WID_GS_OPTIONSPANEL) return;

		uint btn = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GS_OPTIONSPANEL, SETTINGTREE_TOP_OFFSET);
		if (btn == INT_MAX) return;

		uint cur_row = 0;
		SettingEntry *pe = _settings_main_page.FindEntry(btn, &cur_row);

		if (pe == NULL) return;  // Clicked below the last setting of the page

		int x = (_current_text_dir == TD_RTL ? this->width - 1 - pt.x : pt.x) - SETTINGTREE_LEFT_OFFSET - (pe->level + 1) * LEVEL_WIDTH;  // Shift x coordinate
		if (x < 0) return;  // Clicked left of the entry

		if ((pe->flags & SEF_KIND_MASK) == SEF_SUBTREE_KIND) {
			this->SetDisplayedHelpText(NULL);
			pe->d.sub.folded = !pe->d.sub.folded; // Flip 'folded'-ness of the sub-page

			this->InvalidateData();
			return;
		}

		assert((pe->flags & SEF_KIND_MASK) == SEF_SETTING_KIND);
		const SettingDesc *sd = pe->d.entry.setting;

		/* return if action is only active in network, or only settable by server */
		if ((!(sd->save.conv & SLF_NO_NETWORK_SYNC) && _networking && !_network_server && !(sd->desc.flags & SGF_PER_COMPANY)) ||
				((sd->desc.flags & SGF_NETWORK_ONLY) && !_networking) || ((sd->desc.flags & SGF_NO_NETWORK) && _networking)) {
			this->SetDisplayedHelpText(pe);
			return;
		}

		const void *var = ResolveVariableAddress(settings_ptr, sd);
		int32 value = (int32)ReadValue(var, sd->save.conv);

		/* clicked on the icon on the left side. Either scroller, bool on/off or dropdown */
		if (x < SETTING_BUTTON_WIDTH && (sd->desc.flags & SGF_MULTISTRING)) {
			const SettingDescBase *sdb = &sd->desc;
			this->SetDisplayedHelpText(pe);

			if (this->valuedropdown_entry == pe) {
				/* unclick the dropdown */
				HideDropDownMenu(this);
				this->closing_dropdown = false;
				this->valuedropdown_entry->SetButtons(0);
				this->valuedropdown_entry = NULL;
			} else {
				if (this->valuedropdown_entry != NULL) this->valuedropdown_entry->SetButtons(0);
				this->closing_dropdown = false;

				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_GS_OPTIONSPANEL);
				int rel_y = (pt.y - (int)wid->pos_y - SETTINGTREE_TOP_OFFSET) % wid->resize_y;

				Rect wi_rect;
				wi_rect.left = pt.x - (_current_text_dir == TD_RTL ? SETTING_BUTTON_WIDTH - 1 - x : x);
				wi_rect.right = wi_rect.left + SETTING_BUTTON_WIDTH - 1;
				wi_rect.top = pt.y - rel_y + (SETTING_HEIGHT - SETTING_BUTTON_HEIGHT) / 2;
				wi_rect.bottom = wi_rect.top + SETTING_BUTTON_HEIGHT - 1;

				/* For dropdowns we also have to check the y position thoroughly, the mouse may not above the just opening dropdown */
				if (pt.y >= wi_rect.top && pt.y <= wi_rect.bottom) {
					this->valuedropdown_entry = pe;
					this->valuedropdown_entry->SetButtons(SEF_LEFT_DEPRESSED);

					DropDownList *list = new DropDownList();
					for (int i = sdb->min; i <= (int)sdb->max; i++) {
						list->push_back(new DropDownListStringItem(sdb->str_val + i - sdb->min, i, false));
					}

					ShowDropDownListAt(this, list, value, -1, wi_rect, COLOUR_ORANGE, true);
				}
			}
			this->SetDirty();
		} else if (x < SETTING_BUTTON_WIDTH) {
			this->SetDisplayedHelpText(pe);
			const SettingDescBase *sdb = &sd->desc;
			int32 oldvalue = value;

			switch (sdb->cmd) {
				case SDT_BOOLX: value ^= 1; break;
				case SDT_ONEOFMANY:
				case SDT_NUMX: {
					/* Add a dynamic step-size to the scroller. In a maximum of
					 * 50-steps you should be able to get from min to max,
					 * unless specified otherwise in the 'interval' variable
					 * of the current setting. */
					uint32 step = (sdb->interval == 0) ? ((sdb->max - sdb->min) / 50) : sdb->interval;
					if (step == 0) step = 1;

					/* don't allow too fast scrolling */
					if ((this->flags & WF_TIMEOUT) && this->timeout_timer > 1) {
						_left_button_clicked = false;
						return;
					}

					/* Increase or decrease the value and clamp it to extremes */
					if (x >= SETTING_BUTTON_WIDTH / 2) {
						value += step;
						if (sdb->min < 0) {
							assert((int32)sdb->max >= 0);
							if (value > (int32)sdb->max) value = (int32)sdb->max;
						} else {
							if ((uint32)value > sdb->max) value = (int32)sdb->max;
						}
						if (value < sdb->min) value = sdb->min; // skip between "disabled" and minimum
					} else {
						value -= step;
						if (value < sdb->min) value = (sdb->flags & SGF_0ISDISABLED) ? 0 : sdb->min;
					}

					/* Set up scroller timeout for numeric values */
					if (value != oldvalue) {
						if (this->clicked_entry != NULL) { // Release previous buttons if any
							this->clicked_entry->SetButtons(0);
						}
						this->clicked_entry = pe;
						this->clicked_entry->SetButtons((x >= SETTING_BUTTON_WIDTH / 2) != (_current_text_dir == TD_RTL) ? SEF_RIGHT_DEPRESSED : SEF_LEFT_DEPRESSED);
						this->SetTimeout();
						_left_button_clicked = false;
					}
					break;
				}

				default: NOT_REACHED();
			}

			if (value != oldvalue) {
				if ((sd->desc.flags & SGF_PER_COMPANY) != 0) {
					SetCompanySetting(pe->d.entry.index, value);
				} else {
					SetSettingValue(pe->d.entry.index, value);
				}
				this->SetDirty();
			}
		} else {
			/* Only open editbox if clicked for the second time, and only for types where it is sensible for. */
			if (this->last_clicked == pe && sd->desc.cmd != SDT_BOOLX && !(sd->desc.flags & SGF_MULTISTRING)) {
				/* Show the correct currency-translated value */
				if (sd->desc.flags & SGF_CURRENCY) value *= _currency->rate;

				this->valuewindow_entry = pe;
				SetDParam(0, value);
				ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, 10, this, CS_NUMERAL, QSF_ENABLE_DEFAULT);
			}
			this->SetDisplayedHelpText(pe);
		}
	}

	virtual void OnTimeout()
	{
		if (this->clicked_entry != NULL) { // On timeout, release any depressed buttons
			this->clicked_entry->SetButtons(0);
			this->clicked_entry = NULL;
			this->SetDirty();
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		/* The user pressed cancel */
		if (str == NULL) return;

		assert(this->valuewindow_entry != NULL);
		assert((this->valuewindow_entry->flags & SEF_KIND_MASK) == SEF_SETTING_KIND);
		const SettingDesc *sd = this->valuewindow_entry->d.entry.setting;

		int32 value;
		if (!StrEmpty(str)) {
			value = atoi(str);

			/* Save the correct currency-translated value */
			if (sd->desc.flags & SGF_CURRENCY) value /= _currency->rate;
		} else {
			value = (int32)(size_t)sd->desc.def;
		}

		if ((sd->desc.flags & SGF_PER_COMPANY) != 0) {
			SetCompanySetting(this->valuewindow_entry->d.entry.index, value);
		} else {
			SetSettingValue(this->valuewindow_entry->d.entry.index, value);
		}
		this->SetDirty();
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		assert(this->valuedropdown_entry != NULL);
		const SettingDesc *sd = this->valuedropdown_entry->d.entry.setting;
		assert(sd->desc.flags & SGF_MULTISTRING);

		if ((sd->desc.flags & SGF_PER_COMPANY) != 0) {
			SetCompanySetting(this->valuedropdown_entry->d.entry.index, index);
		} else {
			SetSettingValue(this->valuedropdown_entry->d.entry.index, index);
		}

		this->SetDirty();
	}

	virtual void OnDropdownClose(Point pt, int widget, int index, bool instant_close)
	{
		/* We cannot raise the dropdown button just yet. OnClick needs some hint, whether
		 * the same dropdown button was clicked again, and then not open the dropdown again.
		 * So, we only remember that it was closed, and process it on the next OnPaint, which is
		 * after OnClick. */
		assert(this->valuedropdown_entry != NULL);
		this->closing_dropdown = true;
		this->SetDirty();
	}

	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;

		this->vscroll->SetCount(_settings_main_page.Length());

		if (this->last_clicked != NULL && !_settings_main_page.IsVisible(this->last_clicked)) {
			this->SetDisplayedHelpText(NULL);
		}

		bool all_folded = true;
		bool all_unfolded = true;
		_settings_main_page.GetFoldingState(all_folded, all_unfolded);
		this->SetWidgetDisabledState(WID_GS_EXPAND_ALL, all_unfolded);
		this->SetWidgetDisabledState(WID_GS_COLLAPSE_ALL, all_folded);
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_GS_OPTIONSPANEL, SETTINGTREE_TOP_OFFSET + SETTINGTREE_BOTTOM_OFFSET);
	}
};

GameSettings *GameSettingsWindow::settings_ptr = NULL;

static const NWidgetPart _nested_settings_selection_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_CONFIG_SETTING_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_MAUVE, WID_GS_OPTIONSPANEL), SetMinimalSize(400, 174), SetScrollbar(WID_GS_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_GS_SCROLLBAR),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE), SetMinimalSize(400, 40),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GS_HELP_TEXT), SetMinimalSize(300, 25), SetFill(1, 1), SetResize(1, 0),
				SetPadding(WD_FRAMETEXT_TOP, WD_FRAMETEXT_RIGHT, WD_FRAMETEXT_BOTTOM, WD_FRAMETEXT_LEFT),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_MAUVE),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_GS_EXPAND_ALL), SetDataTip(STR_CONFIG_SETTING_EXPAND_ALL, STR_NULL),
					NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_GS_COLLAPSE_ALL), SetDataTip(STR_CONFIG_SETTING_COLLAPSE_ALL, STR_NULL),
					NWidget(NWID_SPACER, INVALID_COLOUR), SetFill(1, 1), SetResize(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _settings_selection_desc(
	WDP_CENTER, 510, 450,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_settings_selection_widgets, lengthof(_nested_settings_selection_widgets)
);

/** Open advanced settings window. */
void ShowGameSettings()
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new GameSettingsWindow(&_settings_selection_desc);
}


/**
 * Draw [<][>] boxes.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param button_colour the colour of the button
 * @param state 0 = none clicked, 1 = first clicked, 2 = second clicked
 * @param clickable_left is the left button clickable?
 * @param clickable_right is the right button clickable?
 */
void DrawArrowButtons(int x, int y, Colours button_colour, byte state, bool clickable_left, bool clickable_right)
{
	int colour = _colour_gradient[button_colour][2];

	DrawFrameRect(x,                            y, x + SETTING_BUTTON_WIDTH / 2 - 1, y + SETTING_BUTTON_HEIGHT - 1, button_colour, (state == 1) ? FR_LOWERED : FR_NONE);
	DrawFrameRect(x + SETTING_BUTTON_WIDTH / 2, y, x + SETTING_BUTTON_WIDTH     - 1, y + SETTING_BUTTON_HEIGHT - 1, button_colour, (state == 2) ? FR_LOWERED : FR_NONE);
	DrawSprite(SPR_ARROW_LEFT, PAL_NONE, x + WD_IMGBTN_LEFT, y + WD_IMGBTN_TOP);
	DrawSprite(SPR_ARROW_RIGHT, PAL_NONE, x + WD_IMGBTN_LEFT + SETTING_BUTTON_WIDTH / 2, y + WD_IMGBTN_TOP);

	/* Grey out the buttons that aren't clickable */
	bool rtl = _current_text_dir == TD_RTL;
	if (rtl ? !clickable_right : !clickable_left) {
		GfxFillRect(x                            + 1, y, x + SETTING_BUTTON_WIDTH / 2 - 1, y + SETTING_BUTTON_HEIGHT - 2, colour, FILLRECT_CHECKER);
	}
	if (rtl ? !clickable_left : !clickable_right) {
		GfxFillRect(x + SETTING_BUTTON_WIDTH / 2 + 1, y, x + SETTING_BUTTON_WIDTH     - 1, y + SETTING_BUTTON_HEIGHT - 2, colour, FILLRECT_CHECKER);
	}
}

/**
 * Draw a dropdown button.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param button_colour the colour of the button
 * @param state true = lowered
 * @param clickable is the button clickable?
 */
void DrawDropDownButton(int x, int y, Colours button_colour, bool state, bool clickable)
{
	static const char *DOWNARROW = "\xEE\x8A\xAA";

	int colour = _colour_gradient[button_colour][2];

	DrawFrameRect(x, y, x + SETTING_BUTTON_WIDTH - 1, y + SETTING_BUTTON_HEIGHT - 1, button_colour, state ? FR_LOWERED : FR_NONE);
	DrawString(x + (state ? 1 : 0), x + SETTING_BUTTON_WIDTH - (state ? 0 : 1), y + (state ? 2 : 1), DOWNARROW, TC_BLACK, SA_HOR_CENTER);

	if (!clickable) {
		GfxFillRect(x +  1, y, x + SETTING_BUTTON_WIDTH - 1, y + SETTING_BUTTON_HEIGHT - 2, colour, FILLRECT_CHECKER);
	}
}

/**
 * Draw a toggle button.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param state true = lowered
 * @param clickable is the button clickable?
 */
void DrawBoolButton(int x, int y, bool state, bool clickable)
{
	static const Colours _bool_ctabs[2][2] = {{COLOUR_CREAM, COLOUR_RED}, {COLOUR_DARK_GREEN, COLOUR_GREEN}};
	DrawFrameRect(x, y, x + SETTING_BUTTON_WIDTH - 1, y + SETTING_BUTTON_HEIGHT - 1, _bool_ctabs[state][clickable], state ? FR_LOWERED : FR_NONE);
}

struct CustomCurrencyWindow : Window {
	int query_widget;

	CustomCurrencyWindow(const WindowDesc *desc) : Window()
	{
		this->InitNested(desc);

		SetButtonState();
	}

	void SetButtonState()
	{
		this->SetWidgetDisabledState(WID_CC_RATE_DOWN, _custom_currency.rate == 1);
		this->SetWidgetDisabledState(WID_CC_RATE_UP, _custom_currency.rate == UINT16_MAX);
		this->SetWidgetDisabledState(WID_CC_YEAR_DOWN, _custom_currency.to_euro == CF_NOEURO);
		this->SetWidgetDisabledState(WID_CC_YEAR_UP, _custom_currency.to_euro == MAX_YEAR);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_CC_RATE:      SetDParam(0, 1); SetDParam(1, 1);            break;
			case WID_CC_SEPARATOR: SetDParamStr(0, _custom_currency.separator); break;
			case WID_CC_PREFIX:    SetDParamStr(0, _custom_currency.prefix);    break;
			case WID_CC_SUFFIX:    SetDParamStr(0, _custom_currency.suffix);    break;
			case WID_CC_YEAR:
				SetDParam(0, (_custom_currency.to_euro != CF_NOEURO) ? STR_CURRENCY_SWITCH_TO_EURO : STR_CURRENCY_SWITCH_TO_EURO_NEVER);
				SetDParam(1, _custom_currency.to_euro);
				break;

			case WID_CC_PREVIEW:
				SetDParam(0, 10000);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			/* Set the appropriate width for the edit 'buttons' */
			case WID_CC_SEPARATOR_EDIT:
			case WID_CC_PREFIX_EDIT:
			case WID_CC_SUFFIX_EDIT:
				size->width  = this->GetWidget<NWidgetBase>(WID_CC_RATE_DOWN)->smallest_x + this->GetWidget<NWidgetBase>(WID_CC_RATE_UP)->smallest_x;
				break;

			/* Make sure the window is wide enough for the widest exchange rate */
			case WID_CC_RATE:
				SetDParam(0, 1);
				SetDParam(1, INT32_MAX);
				*size = GetStringBoundingBox(STR_CURRENCY_EXCHANGE_RATE);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		int line = 0;
		int len = 0;
		StringID str = 0;
		CharSetFilter afilter = CS_ALPHANUMERAL;

		switch (widget) {
			case WID_CC_RATE_DOWN:
				if (_custom_currency.rate > 1) _custom_currency.rate--;
				if (_custom_currency.rate == 1) this->DisableWidget(WID_CC_RATE_DOWN);
				this->EnableWidget(WID_CC_RATE_UP);
				break;

			case WID_CC_RATE_UP:
				if (_custom_currency.rate < UINT16_MAX) _custom_currency.rate++;
				if (_custom_currency.rate == UINT16_MAX) this->DisableWidget(WID_CC_RATE_UP);
				this->EnableWidget(WID_CC_RATE_DOWN);
				break;

			case WID_CC_RATE:
				SetDParam(0, _custom_currency.rate);
				str = STR_JUST_INT;
				len = 5;
				line = WID_CC_RATE;
				afilter = CS_NUMERAL;
				break;

			case WID_CC_SEPARATOR_EDIT:
			case WID_CC_SEPARATOR:
				SetDParamStr(0, _custom_currency.separator);
				str = STR_JUST_RAW_STRING;
				len = 1;
				line = WID_CC_SEPARATOR;
				break;

			case WID_CC_PREFIX_EDIT:
			case WID_CC_PREFIX:
				SetDParamStr(0, _custom_currency.prefix);
				str = STR_JUST_RAW_STRING;
				len = 12;
				line = WID_CC_PREFIX;
				break;

			case WID_CC_SUFFIX_EDIT:
			case WID_CC_SUFFIX:
				SetDParamStr(0, _custom_currency.suffix);
				str = STR_JUST_RAW_STRING;
				len = 12;
				line = WID_CC_SUFFIX;
				break;

			case WID_CC_YEAR_DOWN:
				_custom_currency.to_euro = (_custom_currency.to_euro <= 2000) ? CF_NOEURO : _custom_currency.to_euro - 1;
				if (_custom_currency.to_euro == CF_NOEURO) this->DisableWidget(WID_CC_YEAR_DOWN);
				this->EnableWidget(WID_CC_YEAR_UP);
				break;

			case WID_CC_YEAR_UP:
				_custom_currency.to_euro = Clamp(_custom_currency.to_euro + 1, 2000, MAX_YEAR);
				if (_custom_currency.to_euro == MAX_YEAR) this->DisableWidget(WID_CC_YEAR_UP);
				this->EnableWidget(WID_CC_YEAR_DOWN);
				break;

			case WID_CC_YEAR:
				SetDParam(0, _custom_currency.to_euro);
				str = STR_JUST_INT;
				len = 7;
				line = WID_CC_YEAR;
				afilter = CS_NUMERAL;
				break;
		}

		if (len != 0) {
			this->query_widget = line;
			ShowQueryString(str, STR_CURRENCY_CHANGE_PARAMETER, len + 1, this, afilter, QSF_NONE);
		}

		this->SetTimeout();
		this->SetDirty();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		switch (this->query_widget) {
			case WID_CC_RATE:
				_custom_currency.rate = Clamp(atoi(str), 1, UINT16_MAX);
				break;

			case WID_CC_SEPARATOR: // Thousands seperator
				strecpy(_custom_currency.separator, str, lastof(_custom_currency.separator));
				break;

			case WID_CC_PREFIX:
				strecpy(_custom_currency.prefix, str, lastof(_custom_currency.prefix));
				break;

			case WID_CC_SUFFIX:
				strecpy(_custom_currency.suffix, str, lastof(_custom_currency.suffix));
				break;

			case WID_CC_YEAR: { // Year to switch to euro
				int val = atoi(str);

				_custom_currency.to_euro = (val < 2000 ? CF_NOEURO : min(val, MAX_YEAR));
				break;
			}
		}
		MarkWholeScreenDirty();
		SetButtonState();
	}

	virtual void OnTimeout()
	{
		this->SetDirty();
	}
};

static const NWidgetPart _nested_cust_currency_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_CURRENCY_WINDOW, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(7, 3, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 5),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_RATE_DOWN), SetDataTip(AWV_DECREASE, STR_CURRENCY_DECREASE_EXCHANGE_RATE_TOOLTIP),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_RATE_UP), SetDataTip(AWV_INCREASE, STR_CURRENCY_INCREASE_EXCHANGE_RATE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_TEXT, COLOUR_BLUE, WID_CC_RATE), SetDataTip(STR_CURRENCY_EXCHANGE_RATE, STR_CURRENCY_SET_EXCHANGE_RATE_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 5),
				NWidget(WWT_PUSHBTN, COLOUR_DARK_BLUE, WID_CC_SEPARATOR_EDIT), SetDataTip(0x0, STR_CURRENCY_SET_CUSTOM_CURRENCY_SEPARATOR_TOOLTIP), SetFill(0, 1),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_TEXT, COLOUR_BLUE, WID_CC_SEPARATOR), SetDataTip(STR_CURRENCY_SEPARATOR, STR_CURRENCY_SET_CUSTOM_CURRENCY_SEPARATOR_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 5),
				NWidget(WWT_PUSHBTN, COLOUR_DARK_BLUE, WID_CC_PREFIX_EDIT), SetDataTip(0x0, STR_CURRENCY_SET_CUSTOM_CURRENCY_PREFIX_TOOLTIP), SetFill(0, 1),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_TEXT, COLOUR_BLUE, WID_CC_PREFIX), SetDataTip(STR_CURRENCY_PREFIX, STR_CURRENCY_SET_CUSTOM_CURRENCY_PREFIX_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 5),
				NWidget(WWT_PUSHBTN, COLOUR_DARK_BLUE, WID_CC_SUFFIX_EDIT), SetDataTip(0x0, STR_CURRENCY_SET_CUSTOM_CURRENCY_SUFFIX_TOOLTIP), SetFill(0, 1),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_TEXT, COLOUR_BLUE, WID_CC_SUFFIX), SetDataTip(STR_CURRENCY_SUFFIX, STR_CURRENCY_SET_CUSTOM_CURRENCY_SUFFIX_TOOLTIP), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 5),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_YEAR_DOWN), SetDataTip(AWV_DECREASE, STR_CURRENCY_DECREASE_CUSTOM_CURRENCY_TO_EURO_TOOLTIP),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_YEAR_UP), SetDataTip(AWV_INCREASE, STR_CURRENCY_INCREASE_CUSTOM_CURRENCY_TO_EURO_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_TEXT, COLOUR_BLUE, WID_CC_YEAR), SetDataTip(STR_JUST_STRING, STR_CURRENCY_SET_CUSTOM_CURRENCY_TO_EURO_TOOLTIP), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_LABEL, COLOUR_BLUE, WID_CC_PREVIEW),
								SetDataTip(STR_CURRENCY_PREVIEW, STR_CURRENCY_CUSTOM_CURRENCY_PREVIEW_TOOLTIP), SetPadding(15, 1, 18, 2),
	EndContainer(),
};

static const WindowDesc _cust_currency_desc(
	WDP_CENTER, 0, 0,
	WC_CUSTOM_CURRENCY, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_cust_currency_widgets, lengthof(_nested_cust_currency_widgets)
);

/** Open custom currency window. */
static void ShowCustCurrency()
{
	DeleteWindowById(WC_CUSTOM_CURRENCY, 0);
	new CustomCurrencyWindow(&_cust_currency_desc);
}
