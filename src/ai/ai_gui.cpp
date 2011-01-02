/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_gui.cpp Window for configuring the AIs */

#include "../stdafx.h"
#include "../openttd.h"
#include "../table/sprites.h"
#include "../gui.h"
#include "../querystring_gui.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "../gfx_func.h"
#include "../command_func.h"
#include "../network/network.h"
#include "../settings_func.h"
#include "../network/network_content.h"
#include "../core/backup_type.hpp"

#include "ai.hpp"
#include "api/ai_log.hpp"
#include "ai_config.hpp"
#include "ai_instance.hpp"

#include "table/strings.h"

/** Enum referring to the widgets of the AI list window */
enum AIListWindowWidgets {
	AIL_WIDGET_LIST,             ///< The matrix with all available AIs
	AIL_WIDGET_SCROLLBAR,        ///< Scrollbar next to the AI list
	AIL_WIDGET_INFO_BG,          ///< Panel to draw some AI information on
	AIL_WIDGET_ACCEPT,           ///< Accept button
	AIL_WIDGET_CANCEL,           ///< Cancel button
};

/**
 * Window that let you choose an available AI.
 */
struct AIListWindow : public Window {
	const AIInfoList *ai_info_list;
	int selected;
	CompanyID slot;
	int line_height; // Height of a row in the matrix widget.
	Scrollbar *vscroll;

	AIListWindow(const WindowDesc *desc, CompanyID slot) : Window(),
		slot(slot)
	{
		this->ai_info_list = AI::GetUniqueInfoList();

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(AIL_WIDGET_SCROLLBAR);
		this->FinishInitNested(desc); // Initializes 'this->line_height' as side effect.

		this->vscroll->SetCount((int)this->ai_info_list->size() + 1);

		/* Try if we can find the currently selected AI */
		this->selected = -1;
		if (AIConfig::GetConfig(slot)->HasAI()) {
			AIInfo *info = AIConfig::GetConfig(slot)->GetInfo();
			int i = 0;
			for (AIInfoList::const_iterator it = this->ai_info_list->begin(); it != this->ai_info_list->end(); it++, i++) {
				if ((*it).second == info) {
					this->selected = i;
					break;
				}
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == AIL_WIDGET_LIST) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

			resize->width = 1;
			resize->height = this->line_height;
			size->height = GB(this->GetWidget<NWidgetCore>(widget)->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case AIL_WIDGET_LIST: {
				/* Draw a list of all available AIs. */
				int y = this->GetWidget<NWidgetBase>(AIL_WIDGET_LIST)->pos_y;
				/* First AI in the list is hardcoded to random */
				if (this->vscroll->IsVisible(0)) {
					DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_LEFT, y + WD_MATRIX_TOP, STR_AI_CONFIG_RANDOM_AI, this->selected == -1 ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				AIInfoList::const_iterator it = this->ai_info_list->begin();
				for (int i = 1; it != this->ai_info_list->end(); i++, it++) {
					if (this->vscroll->IsVisible(i)) {
						DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, (*it).second->GetName(), (this->selected == i - 1) ? TC_WHITE : TC_BLACK);
						y += this->line_height;
					}
				}
				break;
			}
			case AIL_WIDGET_INFO_BG: {
				AIInfo *selected_info = NULL;
				AIInfoList::const_iterator it = this->ai_info_list->begin();
				for (int i = 1; selected_info == NULL && it != this->ai_info_list->end(); i++, it++) {
					if (this->selected == i - 1) selected_info = (*it).second;
				}
				/* Some info about the currently selected AI. */
				if (selected_info != NULL) {
					int y = r.top + WD_FRAMERECT_TOP;
					SetDParamStr(0, selected_info->GetAuthor());
					DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, STR_AI_LIST_AUTHOR);
					y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
					SetDParam(0, selected_info->GetVersion());
					DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, STR_AI_LIST_VERSION);
					y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
					if (selected_info->GetURL() != NULL) {
						SetDParamStr(0, selected_info->GetURL());
						DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, STR_AI_LIST_URL);
						y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
					}
					SetDParamStr(0, selected_info->GetDescription());
					DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, r.bottom - WD_FRAMERECT_BOTTOM, STR_JUST_RAW_STRING, TC_BLACK);
				}
				break;
			}
		}
	}

	void ChangeAI()
	{
		if (this->selected == -1) {
			AIConfig::GetConfig(slot)->ChangeAI(NULL);
		} else {
			AIInfoList::const_iterator it = this->ai_info_list->begin();
			for (int i = 0; i < this->selected; i++) it++;
			AIConfig::GetConfig(slot)->ChangeAI((*it).second->GetName(), (*it).second->GetVersion());
		}
		SetWindowDirty(WC_GAME_OPTIONS, 0);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case AIL_WIDGET_LIST: { // Select one of the AIs
				int sel = this->vscroll->GetScrolledRowFromWidget(pt.y, this, AIL_WIDGET_LIST, 0, this->line_height) - 1;
				if (sel < (int)this->ai_info_list->size()) {
					this->selected = sel;
					this->SetDirty();
					if (click_count > 1) {
						this->ChangeAI();
						delete this;
					}
				}
				break;
			}

			case AIL_WIDGET_ACCEPT: {
				this->ChangeAI();
				delete this;
				break;
			}

			case AIL_WIDGET_CANCEL:
				delete this;
				break;
		}
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(AIL_WIDGET_LIST);
		this->vscroll->SetCapacity(nwi->current_y / this->line_height);
		nwi->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnInvalidateData(int data)
	{
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot)) {
			delete this;
			return;
		}

		this->vscroll->SetCount((int)this->ai_info_list->size() + 1);

		/* selected goes from -1 .. length of ai list - 1. */
		this->selected = min(this->selected, this->vscroll->GetCount() - 2);
	}
};

/** Widgets for the AI list window. */
static const NWidgetPart _nested_ai_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, AIL_WIDGET_LIST), SetMinimalSize(188, 112), SetFill(1, 1), SetResize(1, 1), SetDataTip(0x501, STR_AI_LIST_TOOLTIP), SetScrollbar(AIL_WIDGET_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, AIL_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, AIL_WIDGET_INFO_BG), SetMinimalTextLines(8, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIL_WIDGET_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_LIST_ACCEPT, STR_AI_LIST_ACCEPT_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIL_WIDGET_CANCEL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_LIST_CANCEL, STR_AI_LIST_CANCEL_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/** Window definition for the ai list window. */
static const WindowDesc _ai_list_desc(
	WDP_CENTER, 200, 234,
	WC_AI_LIST, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_ai_list_widgets, lengthof(_nested_ai_list_widgets)
);

/**
 * Open the AI list window to chose an AI for the given company slot.
 * @param slot The slot to change the AI of.
 */
static void ShowAIListWindow(CompanyID slot)
{
	DeleteWindowByClass(WC_AI_LIST);
	new AIListWindow(&_ai_list_desc, slot);
}

/** Enum referring to the widgets of the AI settings window */
enum AISettingsWindowWidgest {
	AIS_WIDGET_BACKGROUND,   ///< Panel to draw the settings on
	AIS_WIDGET_SCROLLBAR,    ///< Scrollbar to scroll through all settings
	AIS_WIDGET_ACCEPT,       ///< Accept button
	AIS_WIDGET_RESET,        ///< Reset button
};

/**
 * Window for settings the parameters of an AI.
 */
struct AISettingsWindow : public Window {
	CompanyID slot;
	AIConfig *ai_config;
	int clicked_button;
	bool clicked_increase;
	int timeout;
	int clicked_row;
	int line_height; // Height of a row in the matrix widget.
	Scrollbar *vscroll;

	AISettingsWindow(const WindowDesc *desc, CompanyID slot) : Window(),
		slot(slot),
		clicked_button(-1),
		timeout(0)
	{
		this->ai_config = AIConfig::GetConfig(slot);

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(AIS_WIDGET_SCROLLBAR);
		this->FinishInitNested(desc, slot);  // Initializes 'this->line_height' as side effect.

		this->SetWidgetDisabledState(AIS_WIDGET_RESET, _game_mode != GM_MENU && Company::IsValidID(this->slot));

		this->vscroll->SetCount((int)this->ai_config->GetConfigList()->size());
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == AIS_WIDGET_BACKGROUND) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

			resize->width = 1;
			resize->height = this->line_height;
			size->height = GB(this->GetWidget<NWidgetCore>(widget)->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != AIS_WIDGET_BACKGROUND) return;

		AIConfig *config = this->ai_config;
		AIConfigItemList::const_iterator it = config->GetConfigList()->begin();
		int i = 0;
		for (; !this->vscroll->IsVisible(i); i++) it++;

		bool rtl = _current_text_dir == TD_RTL;
		uint buttons_left = rtl ? r.right - 23 : r.left + 4;
		uint text_left    = r.left + (rtl ? WD_FRAMERECT_LEFT : 28);
		uint text_right   = r.right - (rtl ? 28 : WD_FRAMERECT_RIGHT);


		int y = r.top;
		for (; this->vscroll->IsVisible(i) && it != config->GetConfigList()->end(); i++, it++) {
			int current_value = config->GetSetting((*it).name);
			bool editable = _game_mode == GM_MENU || !Company::IsValidID(this->slot) || (it->flags & AICONFIG_INGAME) != 0;

			StringID str;
			TextColour colour;
			uint idx = 0;
			if (StrEmpty((*it).description)) {
				str = STR_JUST_STRING;
				colour = TC_ORANGE;
			} else {
				str = STR_AI_SETTINGS_SETTING;
				colour = TC_LIGHT_BLUE;
				SetDParamStr(idx++, (*it).description);
			}

			if (((*it).flags & AICONFIG_BOOLEAN) != 0) {
				DrawFrameRect(buttons_left, y  + 2, buttons_left + 19, y + 10, (current_value != 0) ? COLOUR_GREEN : COLOUR_RED, (current_value != 0) ? FR_LOWERED : FR_NONE);
				SetDParam(idx++, current_value == 0 ? STR_CONFIG_SETTING_OFF : STR_CONFIG_SETTING_ON);
			} else {
				DrawArrowButtons(buttons_left, y + 2, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + (this->clicked_increase != rtl) : 0, editable && current_value > (*it).min_value, editable && current_value < (*it).max_value);
				if (it->labels != NULL && it->labels->Contains(current_value)) {
					SetDParam(idx++, STR_JUST_RAW_STRING);
					SetDParamStr(idx++, it->labels->Find(current_value)->second);
				} else {
					SetDParam(idx++, STR_JUST_INT);
					SetDParam(idx++, current_value);
				}
			}

			DrawString(text_left, text_right, y + WD_MATRIX_TOP, str, colour);
			y += this->line_height;
		}
	}

	void CheckDifficultyLevel()
	{
		if (_game_mode == GM_MENU) {
			if (_settings_newgame.difficulty.diff_level != 3) {
				_settings_newgame.difficulty.diff_level = 3;
				ShowErrorMessage(STR_WARNING_DIFFICULTY_TO_CUSTOM, INVALID_STRING_ID, WL_WARNING);
			}
		} else if (_settings_game.difficulty.diff_level != 3) {
			IConsoleSetSetting("difficulty.diff_level", 3);
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case AIS_WIDGET_BACKGROUND: {
				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(AIS_WIDGET_BACKGROUND);
				int num = (pt.y - wid->pos_y) / this->line_height + this->vscroll->GetPosition();
				if (num >= (int)this->ai_config->GetConfigList()->size()) break;

				AIConfigItemList::const_iterator it = this->ai_config->GetConfigList()->begin();
				for (int i = 0; i < num; i++) it++;
				AIConfigItem config_item = *it;
				if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot) && (config_item.flags & AICONFIG_INGAME) == 0) return;

				bool bool_item = (config_item.flags & AICONFIG_BOOLEAN) != 0;

				int x = pt.x - wid->pos_x;
				if (_current_text_dir == TD_RTL) x = wid->current_x - x;
				x -= 4;
				/* One of the arrows is clicked (or green/red rect in case of bool value) */
				if (IsInsideMM(x, 0, 21)) {
					int new_val = this->ai_config->GetSetting(config_item.name);
					if (bool_item) {
						new_val = !new_val;
					} else if (x >= 10) {
						/* Increase button clicked */
						new_val += config_item.step_size;
						if (new_val > config_item.max_value) new_val = config_item.max_value;
						this->clicked_increase = true;
					} else {
						/* Decrease button clicked */
						new_val -= config_item.step_size;
						if (new_val < config_item.min_value) new_val = config_item.min_value;
						this->clicked_increase = false;
					}

					this->ai_config->SetSetting(config_item.name, new_val);
					this->clicked_button = num;
					this->timeout = 5;

					this->CheckDifficultyLevel();
				} else if (!bool_item) {
					/* Display a query box so users can enter a custom value. */
					this->clicked_row = num;
					SetDParam(0, this->ai_config->GetSetting(config_item.name));
					ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, 10, 100, this, CS_NUMERAL, QSF_NONE);
				}

				this->SetDirty();
				break;
			}

			case AIS_WIDGET_ACCEPT:
				delete this;
				break;

			case AIS_WIDGET_RESET:
				if (_game_mode == GM_MENU || !Company::IsValidID(this->slot)) {
					this->ai_config->ResetSettings();
					this->SetDirty();
				}
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (StrEmpty(str)) return;
		AIConfigItemList::const_iterator it = this->ai_config->GetConfigList()->begin();
		for (int i = 0; i < this->clicked_row; i++) it++;
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot) && (it->flags & AICONFIG_INGAME) == 0) return;
		int32 value = atoi(str);
		this->ai_config->SetSetting((*it).name, value);
		this->CheckDifficultyLevel();
		this->SetDirty();
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(AIS_WIDGET_BACKGROUND);
		this->vscroll->SetCapacity(nwi->current_y / this->line_height);
		nwi->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnTick()
	{
		if (--this->timeout == 0) {
			this->clicked_button = -1;
			this->SetDirty();
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot)) delete this;
	}
};

/** Widgets for the AI settings window. */
static const NWidgetPart _nested_ai_settings_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, AIS_WIDGET_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetFill(1, 0), SetDataTip(0x501, STR_NULL), SetScrollbar(AIS_WIDGET_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, AIS_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIS_WIDGET_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIS_WIDGET_RESET), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_SETTINGS_RESET, STR_NULL),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/** Window definition for the AI settings window. */
static const WindowDesc _ai_settings_desc(
	WDP_CENTER, 500, 208,
	WC_AI_SETTINGS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_ai_settings_widgets, lengthof(_nested_ai_settings_widgets)
);

/**
 * Open the AI settings window to change the AI settings for an AI.
 * @param slot The CompanyID of the AI to change the settings.
 */
static void ShowAISettingsWindow(CompanyID slot)
{
	DeleteWindowByClass(WC_AI_LIST);
	DeleteWindowByClass(WC_AI_SETTINGS);
	new AISettingsWindow(&_ai_settings_desc, slot);
}

/** Enum referring to the widgets of the AI config window */
enum AIConfigWindowWidgets {
	AIC_WIDGET_BACKGROUND,   ///< Window background
	AIC_WIDGET_DECREASE,     ///< Decrease the number of AIs
	AIC_WIDGET_INCREASE,     ///< Increase the number of AIs
	AIC_WIDGET_NUMBER,       ///< Number of AIs
	AIC_WIDGET_LIST,         ///< List with currently selected AIs
	AIC_WIDGET_SCROLLBAR,    ///< Scrollbar to scroll through the selected AIs
	AIC_WIDGET_MOVE_UP,      ///< Move up button
	AIC_WIDGET_MOVE_DOWN,    ///< Move down button
	AIC_WIDGET_CHANGE,       ///< Select another AI button
	AIC_WIDGET_CONFIGURE,    ///< Change AI settings button
	AIC_WIDGET_CLOSE,        ///< Close window button
	AIC_WIDGET_CONTENT_DOWNLOAD, ///< Download content button
};

/** Widgets for the configure AI window. */
static const NWidgetPart _nested_ai_config_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, AIC_WIDGET_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 10),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, AIC_WIDGET_DECREASE), SetFill(0, 1), SetDataTip(AWV_DECREASE, STR_NULL),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, AIC_WIDGET_INCREASE), SetFill(0, 1), SetDataTip(AWV_INCREASE, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(6, 0),
				NWidget(WWT_TEXT, COLOUR_MAUVE, AIC_WIDGET_NUMBER), SetDataTip(STR_DIFFICULTY_LEVEL_SETTING_MAXIMUM_NO_COMPETITORS, STR_NULL), SetFill(1, 0), SetPadding(1, 0, 0, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 0, 10),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_MOVE_UP), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_UP, STR_AI_CONFIG_MOVE_UP_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_MOVE_DOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_DOWN, STR_AI_CONFIG_MOVE_DOWN_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_MATRIX, COLOUR_MAUVE, AIC_WIDGET_LIST), SetMinimalSize(288, 112), SetFill(1, 0), SetDataTip(0x801, STR_AI_CONFIG_LIST_TOOLTIP), SetScrollbar(AIC_WIDGET_SCROLLBAR),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, AIC_WIDGET_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(5, 0, 5),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CHANGE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_CONFIG_CHANGE, STR_AI_CONFIG_CHANGE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CONFIGURE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_CONFIG_CONFIGURE, STR_AI_CONFIG_CONFIGURE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CLOSE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CONTENT_DOWNLOAD), SetFill(1, 0), SetMinimalSize(279, 12), SetPadding(0, 5, 9, 5), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
	EndContainer(),
};

/** Window definition for the configure AI window. */
static const WindowDesc _ai_config_desc(
	WDP_CENTER, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_ai_config_widgets, lengthof(_nested_ai_config_widgets)
);

/**
 * Window to configure which AIs will start.
 */
struct AIConfigWindow : public Window {
	CompanyID selected_slot; ///< The currently selected AI slot or \c INVALID_COMPANY.
	int line_height;         ///< Height of a single AI-name line.
	Scrollbar *vscroll;

	AIConfigWindow() : Window()
	{
		this->InitNested(&_ai_config_desc); // Initializes 'this->line_height' as a side effect.
		this->vscroll = this->GetScrollbar(AIC_WIDGET_SCROLLBAR);
		this->selected_slot = INVALID_COMPANY;
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(AIC_WIDGET_LIST);
		this->vscroll->SetCapacity(nwi->current_y / this->line_height);
		this->vscroll->SetCount(MAX_COMPANIES);
		nwi->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
		this->OnInvalidateData(0);
	}

	~AIConfigWindow()
	{
		DeleteWindowByClass(WC_AI_LIST);
		DeleteWindowByClass(WC_AI_SETTINGS);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case AIC_WIDGET_NUMBER:
				SetDParam(0, GetGameSettings().difficulty.max_no_competitors);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case AIC_WIDGET_LIST:
				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				size->height = GB(this->GetWidget<NWidgetCore>(widget)->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
				break;
		}
	}

	/**
	 * Can the AI config in the given company slot be edited?
	 * @param slot The slot to query.
	 * @return True if and only if the given AI Config slot can e edited.
	 */
	static bool IsEditable(CompanyID slot)
	{
		if (_game_mode != GM_NORMAL) {
			return slot > 0 && slot <= GetGameSettings().difficulty.max_no_competitors;
		}
		if (Company::IsValidID(slot) || slot < 0) return false;

		int max_slot = GetGameSettings().difficulty.max_no_competitors;
		for (CompanyID cid = COMPANY_FIRST; cid < (CompanyID)max_slot && cid < MAX_COMPANIES; cid++) {
			if (Company::IsValidHumanID(cid)) max_slot++;
		}
		return slot < max_slot;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case AIC_WIDGET_LIST: {
				int y = r.top;
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < MAX_COMPANIES; i++) {
					StringID text;

					if ((_game_mode != GM_NORMAL && i == 0) || (_game_mode == GM_NORMAL && Company::IsValidHumanID(i))) {
						text = STR_AI_CONFIG_HUMAN_PLAYER;
					} else if (AIConfig::GetConfig((CompanyID)i)->GetInfo() != NULL) {
						SetDParamStr(0, AIConfig::GetConfig((CompanyID)i)->GetInfo()->GetName());
						text = STR_JUST_RAW_STRING;
					} else {
						text = STR_AI_CONFIG_RANDOM_AI;
					}
					DrawString(r.left + 10, r.right - 10, y + WD_MATRIX_TOP, text,
							(this->selected_slot == i) ? TC_WHITE : (IsEditable((CompanyID)i) ? TC_ORANGE : TC_SILVER));
					y += this->line_height;
				}
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case AIC_WIDGET_DECREASE:
			case AIC_WIDGET_INCREASE: {
				int new_value;
				if (widget == AIC_WIDGET_DECREASE) {
					new_value = max(0, GetGameSettings().difficulty.max_no_competitors - 1);
				} else {
					new_value = min(MAX_COMPANIES - 1, GetGameSettings().difficulty.max_no_competitors + 1);
				}
				IConsoleSetSetting("difficulty.max_no_competitors", new_value);
				this->InvalidateData();
				break;
			}

			case AIC_WIDGET_LIST: { // Select a slot
				this->selected_slot = (CompanyID)this->vscroll->GetScrolledRowFromWidget(pt.y, this, widget, 0, this->line_height);
				this->InvalidateData();
				if (click_count > 1 && this->selected_slot != INVALID_COMPANY) ShowAIListWindow((CompanyID)this->selected_slot);
				break;
			}

			case AIC_WIDGET_MOVE_UP:
				if (IsEditable(this->selected_slot) && IsEditable((CompanyID)(this->selected_slot - 1))) {
					Swap(GetGameSettings().ai_config[this->selected_slot], GetGameSettings().ai_config[this->selected_slot - 1]);
					this->selected_slot--;
					this->vscroll->ScrollTowards(this->selected_slot);
					this->InvalidateData();
				}
				break;

			case AIC_WIDGET_MOVE_DOWN:
				if (IsEditable(this->selected_slot) && IsEditable((CompanyID)(this->selected_slot + 1))) {
					Swap(GetGameSettings().ai_config[this->selected_slot], GetGameSettings().ai_config[this->selected_slot + 1]);
					this->selected_slot++;
					this->vscroll->ScrollTowards(this->selected_slot);
					this->InvalidateData();
				}
				break;

			case AIC_WIDGET_CHANGE:  // choose other AI
				ShowAIListWindow((CompanyID)this->selected_slot);
				break;

			case AIC_WIDGET_CONFIGURE: // change the settings for an AI
				ShowAISettingsWindow((CompanyID)this->selected_slot);
				break;

			case AIC_WIDGET_CLOSE:
				delete this;
				break;

			case AIC_WIDGET_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
#if defined(ENABLE_NETWORK)
					ShowNetworkContentListWindow(NULL, CONTENT_TYPE_AI);
#endif
				}
				break;
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (!IsEditable(this->selected_slot)) {
			this->selected_slot = INVALID_COMPANY;
		}

		this->SetWidgetDisabledState(AIC_WIDGET_DECREASE, GetGameSettings().difficulty.max_no_competitors == 0);
		this->SetWidgetDisabledState(AIC_WIDGET_INCREASE, GetGameSettings().difficulty.max_no_competitors == MAX_COMPANIES - 1);
		this->SetWidgetDisabledState(AIC_WIDGET_CHANGE, this->selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(AIC_WIDGET_CONFIGURE, this->selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(AIC_WIDGET_MOVE_UP, this->selected_slot == INVALID_COMPANY || !IsEditable((CompanyID)(this->selected_slot - 1)));
		this->SetWidgetDisabledState(AIC_WIDGET_MOVE_DOWN, this->selected_slot == INVALID_COMPANY || !IsEditable((CompanyID)(this->selected_slot + 1)));
	}
};

/** Open the AI config window. */
void ShowAIConfigWindow()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new AIConfigWindow();
}

/** Enum referring to the widgets of the AI debug window */
enum AIDebugWindowWidgets {
	AID_WIDGET_VIEW,
	AID_WIDGET_NAME_TEXT,
	AID_WIDGET_SETTINGS,
	AID_WIDGET_RELOAD_TOGGLE,
	AID_WIDGET_LOG_PANEL,
	AID_WIDGET_SCROLLBAR,
	AID_WIDGET_COMPANY_BUTTON_START,
	AID_WIDGET_COMPANY_BUTTON_END = AID_WIDGET_COMPANY_BUTTON_START + MAX_COMPANIES - 1,
	AID_BREAK_STRING_WIDGETS,
	AID_WIDGET_BREAK_STR_ON_OFF_BTN,
	AID_WIDGET_BREAK_STR_EDIT_BOX,
	AID_WIDGET_MATCH_CASE_BTN,
	AID_WIDGET_CONTINUE_BTN,
};

/**
 * Window with everything an AI prints via AILog.
 */
struct AIDebugWindow : public QueryStringBaseWindow {
	static const int top_offset;    ///< Offset of the text at the top of the ::AID_WIDGET_LOG_PANEL.
	static const int bottom_offset; ///< Offset of the text at the bottom of the ::AID_WIDGET_LOG_PANEL.

	static const unsigned int MAX_BREAK_STR_STRING_LENGTH = 256;

	static CompanyID ai_debug_company;                     ///< The AI that is (was last) being debugged.
	int redraw_timer;
	int last_vscroll_pos;
	bool autoscroll;
	bool show_break_box;
	static bool break_check_enabled;                       ///< Stop an AI when it prints a matching string
	static char break_string[MAX_BREAK_STR_STRING_LENGTH]; ///< The string to match to the AI output
	static bool case_sensitive_break_check;                ///< Is the matching done case-sensitive
	int highlight_row;                                     ///< The output row that matches the given string, or -1
	Scrollbar *vscroll;

	AIDebugWindow(const WindowDesc *desc, WindowNumber number) : QueryStringBaseWindow(MAX_BREAK_STR_STRING_LENGTH)
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(AID_WIDGET_SCROLLBAR);
		this->show_break_box = _settings_client.gui.ai_developer_tools;
		this->GetWidget<NWidgetStacked>(AID_BREAK_STRING_WIDGETS)->SetDisplayedPlane(this->show_break_box ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested(desc, number);

		if (!this->show_break_box) break_check_enabled = false;
		/* Disable the companies who are not active or not an AI */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + AID_WIDGET_COMPANY_BUTTON_START, !Company::IsValidAiID(i));
		}
		this->DisableWidget(AID_WIDGET_RELOAD_TOGGLE);
		this->DisableWidget(AID_WIDGET_SETTINGS);
		this->DisableWidget(AID_WIDGET_CONTINUE_BTN);

		this->last_vscroll_pos = 0;
		this->autoscroll = true;
		this->highlight_row = -1;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, MAX_BREAK_STR_STRING_LENGTH);

		/* Restore the break string value from static variable */
		strecpy(this->edit_str_buf, this->break_string, this->edit_str_buf + MAX_BREAK_STR_STRING_LENGTH);
		UpdateTextBufferSize(&this->text);

		/* Restore button state from static class variables */
		if (ai_debug_company != INVALID_COMPANY) this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
		this->SetWidgetLoweredState(AID_WIDGET_BREAK_STR_ON_OFF_BTN, this->break_check_enabled);
		this->SetWidgetLoweredState(AID_WIDGET_MATCH_CASE_BTN, this->case_sensitive_break_check);

	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == AID_WIDGET_LOG_PANEL) {
			resize->height = FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			size->height = 14 * resize->height + this->top_offset + this->bottom_offset;
		}
	}

	virtual void OnPaint()
	{
		/* Check if the currently selected company is still active. */
		if (ai_debug_company == INVALID_COMPANY || !Company::IsValidAiID(ai_debug_company)) {
			if (ai_debug_company != INVALID_COMPANY) {
				/* Raise and disable the widget for the previous selection. */
				this->RaiseWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
				this->DisableWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);

				ai_debug_company = INVALID_COMPANY;
			}

			const Company *c;
			FOR_ALL_COMPANIES(c) {
				if (c->is_ai) {
					/* Lower the widget corresponding to this company. */
					this->LowerWidget(c->index + AID_WIDGET_COMPANY_BUTTON_START);

					ai_debug_company = c->index;
					break;
				}
			}
		}

		/* Update "Reload AI" and "AI settings" buttons */
		this->SetWidgetsDisabledState(ai_debug_company == INVALID_COMPANY,
			AID_WIDGET_RELOAD_TOGGLE,
			AID_WIDGET_SETTINGS,
			WIDGET_LIST_END);

		/* Draw standard stuff */
		this->DrawWidgets();

		if (this->IsShaded()) return; // Don't draw anything when the window is shaded.

		if (this->show_break_box) this->DrawEditBox(AID_WIDGET_BREAK_STR_EDIT_BOX);

		/* If there are no active companies, don't display anything else. */
		if (ai_debug_company == INVALID_COMPANY) return;

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			NWidgetCore *button = this->GetWidget<NWidgetCore>(i + AID_WIDGET_COMPANY_BUTTON_START);
			bool dirty = false;

			bool valid = Company::IsValidAiID(i);
			bool disabled = !valid;
			if (button->IsDisabled() != disabled) {
				/* Invalid/non-AI companies have button disabled */
				button->SetDisabled(disabled);
				dirty = true;
			}

			bool dead = valid && Company::Get(i)->ai_instance->IsDead();
			Colours colour = dead ? COLOUR_RED : COLOUR_GREY;
			if (button->colour != colour) {
				/* Mark dead AIs by red background */
				button->colour = colour;
				dirty = true;
			}

			/* Do we need a repaint? */
			if (dirty) this->SetDirty();
			/* Draw company icon only for valid AI companies */
			if (!valid) continue;

			byte offset = (i == ai_debug_company) ? 1 : 0;
			DrawCompanyIcon(i, button->pos_x + button->current_x / 2 - 7 + offset, this->GetWidget<NWidgetBase>(AID_WIDGET_COMPANY_BUTTON_START + i)->pos_y + 2 + offset);
		}

		Backup<CompanyByte> cur_company(_current_company, ai_debug_company, FILE_LINE);
		AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
		cur_company.Restore();

		int scroll_count = (log == NULL) ? 0 : log->used;
		if (this->vscroll->GetCount() != scroll_count) {
			this->vscroll->SetCount(scroll_count);

			/* We need a repaint */
			this->SetWidgetDirty(AID_WIDGET_SCROLLBAR);
		}

		if (log == NULL) return;

		/* Detect when the user scrolls the window. Enable autoscroll when the
		 * bottom-most line becomes visible. */
		if (this->last_vscroll_pos != this->vscroll->GetPosition()) {
			this->autoscroll = this->vscroll->GetPosition() >= log->used - this->vscroll->GetCapacity();
		}
		if (this->autoscroll) {
			int scroll_pos = max(0, log->used - this->vscroll->GetCapacity());
			if (scroll_pos != this->vscroll->GetPosition()) {
				this->vscroll->SetPosition(scroll_pos);

				/* We need a repaint */
				this->SetWidgetDirty(AID_WIDGET_SCROLLBAR);
				this->SetWidgetDirty(AID_WIDGET_LOG_PANEL);
			}
		}
		this->last_vscroll_pos = this->vscroll->GetPosition();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case AID_WIDGET_NAME_TEXT:
				if (ai_debug_company == INVALID_COMPANY || !Company::IsValidAiID(ai_debug_company)) {
					SetDParam(0, STR_EMPTY);
				} else {
					const AIInfo *info = Company::Get(ai_debug_company)->ai_info;
					assert(info != NULL);
					SetDParam(0, STR_AI_DEBUG_NAME_AND_VERSION);
					SetDParamStr(1, info->GetName());
					SetDParam(2, info->GetVersion());
				}
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (ai_debug_company == INVALID_COMPANY) return;

		switch (widget) {
			case AID_WIDGET_LOG_PANEL: {
				Backup<CompanyByte> cur_company(_current_company, ai_debug_company, FILE_LINE);
				AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
				cur_company.Restore();
				if (log == NULL) return;

				int y = this->top_offset;
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < log->used; i++) {
					int pos = (i + log->pos + 1 - log->used + log->count) % log->count;
					if (log->lines[pos] == NULL) break;

					TextColour colour;
					switch (log->type[pos]) {
						case AILog::LOG_SQ_INFO:  colour = TC_BLACK;  break;
						case AILog::LOG_SQ_ERROR: colour = TC_RED;    break;
						case AILog::LOG_INFO:     colour = TC_BLACK;  break;
						case AILog::LOG_WARNING:  colour = TC_YELLOW; break;
						case AILog::LOG_ERROR:    colour = TC_RED;    break;
						default:                  colour = TC_BLACK;  break;
					}

					/* Check if the current line should be highlighted */
					if (pos == this->highlight_row) {
						GfxFillRect(r.left + 1, r.top + y, r.right - 1, r.top + y + this->resize.step_height - WD_PAR_VSEP_NORMAL, 0);
						if (colour == TC_BLACK) colour = TC_WHITE; // Make black text readable by inverting it to white.
					}

					DrawString(r.left + 7, r.right - 7, r.top + y, log->lines[pos], colour, SA_LEFT | SA_FORCE);
					y += this->resize.step_height;
				}
				break;
			}
		}
	}

	void ChangeToAI(CompanyID show_ai)
	{
		this->RaiseWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
		ai_debug_company = show_ai;

		Backup<CompanyByte> cur_company(_current_company, ai_debug_company, FILE_LINE);
		AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
		cur_company.Restore();
		this->vscroll->SetCount((log == NULL) ? 0 : log->used);

		this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
		this->autoscroll = true;
		this->last_vscroll_pos = this->vscroll->GetPosition();
		this->SetDirty();
		/* Close AI settings window to prevent confusion */
		DeleteWindowByClass(WC_AI_SETTINGS);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, AID_WIDGET_COMPANY_BUTTON_START, AID_WIDGET_COMPANY_BUTTON_END + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				ChangeToAI((CompanyID)(widget - AID_WIDGET_COMPANY_BUTTON_START));
			}
		}

		switch (widget) {
			case AID_WIDGET_RELOAD_TOGGLE:
				/* First kill the company of the AI, then start a new one. This should start the current AI again */
				DoCommandP(0, 2 | ai_debug_company << 16, 0, CMD_COMPANY_CTRL);
				DoCommandP(0, 1 | ai_debug_company << 16, 0, CMD_COMPANY_CTRL);
				break;

			case AID_WIDGET_SETTINGS:
				ShowAISettingsWindow(ai_debug_company);
				break;

			case AID_WIDGET_BREAK_STR_ON_OFF_BTN:
				this->break_check_enabled = !this->break_check_enabled;
				this->SetWidgetLoweredState(AID_WIDGET_BREAK_STR_ON_OFF_BTN, this->break_check_enabled);
				this->SetWidgetDirty(AID_WIDGET_BREAK_STR_ON_OFF_BTN);
				break;

			case AID_WIDGET_MATCH_CASE_BTN:
				this->case_sensitive_break_check = !this->case_sensitive_break_check;
				this->SetWidgetLoweredState(AID_WIDGET_MATCH_CASE_BTN, this->case_sensitive_break_check);
				break;

			case AID_WIDGET_CONTINUE_BTN:
				/* Unpause */
				DoCommandP(0, PM_PAUSED_NORMAL, 0, CMD_PAUSE);
				this->DisableWidget(AID_WIDGET_CONTINUE_BTN);
				this->RaiseWidget(AID_WIDGET_CONTINUE_BTN); // Disabled widgets don't raise themself
				break;
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(AID_WIDGET_RELOAD_TOGGLE);
		this->RaiseWidget(AID_WIDGET_SETTINGS);
		this->SetDirty();
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(AID_WIDGET_BREAK_STR_EDIT_BOX);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		if (this->HandleEditBoxKey(AID_WIDGET_BREAK_STR_EDIT_BOX, key, keycode, state) != HEBR_NOT_FOCUSED) {
			/* Save the current string to static member so it can be restored next time the window is opened */
			strecpy(this->break_string, this->edit_str_buf, lastof(this->break_string));
		}
		return state;
	}

	virtual void OnInvalidateData(int data = 0)
	{
		if (data == -1 || ai_debug_company == data) this->SetDirty();

		if (data == -2) {
			/* The continue button should be disabled when the game is unpaused and
			 * it was previously paused by the break string ( = a line in the log
			 * was highlighted )*/
			if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED && this->highlight_row != -1) {
				this->DisableWidget(AID_WIDGET_CONTINUE_BTN);
				this->SetWidgetDirty(AID_WIDGET_CONTINUE_BTN);
				this->SetWidgetDirty(AID_WIDGET_LOG_PANEL);
				this->highlight_row = -1;
			}
		}

		/* If the log message is related to the active company tab, check the break string */
		if (data == ai_debug_company && this->break_check_enabled && !StrEmpty(this->edit_str_buf)) {
			/* Get the log instance of the active company */
			Backup<CompanyByte> cur_company(_current_company, ai_debug_company, FILE_LINE);
			AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();

			if (log != NULL && case_sensitive_break_check?
					strstr(log->lines[log->pos], this->edit_str_buf) != 0 :
					strcasestr(log->lines[log->pos], this->edit_str_buf) != 0) {

				AI::Suspend(ai_debug_company);
				if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED) {
					DoCommandP(0, PM_PAUSED_NORMAL, 1, CMD_PAUSE);
				}

				/* Make it possible to click on the continue button */
				this->EnableWidget(AID_WIDGET_CONTINUE_BTN);
				this->SetWidgetDirty(AID_WIDGET_CONTINUE_BTN);

				/* Highlight row that matched */
				this->highlight_row = log->pos;
			}

			cur_company.Restore();
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, AID_WIDGET_LOG_PANEL);
	}
};

const int AIDebugWindow::top_offset = WD_FRAMERECT_TOP + 2;
const int AIDebugWindow::bottom_offset = WD_FRAMERECT_BOTTOM;
CompanyID AIDebugWindow::ai_debug_company = INVALID_COMPANY;
char AIDebugWindow::break_string[MAX_BREAK_STR_STRING_LENGTH] = "";
bool AIDebugWindow::break_check_enabled = true;
bool AIDebugWindow::case_sensitive_break_check = false;

/** Make a number of rows with buttons for each company for the AI debug window. */
NWidgetBase *MakeCompanyButtonRowsAIDebug(int *biggest_index)
{
	return MakeCompanyButtonRows(biggest_index, AID_WIDGET_COMPANY_BUTTON_START, AID_WIDGET_COMPANY_BUTTON_END, 8, STR_AI_DEBUG_SELECT_AI_TOOLTIP);
}

/** Widgets for the AI debug window. */
static const NWidgetPart _nested_ai_debug_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_AI_DEBUG, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_VIEW),
		NWidgetFunction(MakeCompanyButtonRowsAIDebug), SetPadding(0, 2, 1, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, AID_WIDGET_NAME_TEXT), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_STRING, STR_AI_DEBUG_NAME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, AID_WIDGET_SETTINGS), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_SETTINGS, STR_AI_DEBUG_SETTINGS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, AID_WIDGET_RELOAD_TOGGLE), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_RELOAD, STR_AI_DEBUG_RELOAD_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			/* Log panel */
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_LOG_PANEL), SetMinimalSize(287, 180), SetResize(1, 1), SetScrollbar(AID_WIDGET_SCROLLBAR),
			EndContainer(),
			/* Break string widgets */
			NWidget(NWID_SELECTION, INVALID_COLOUR, AID_BREAK_STRING_WIDGETS),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_IMGBTN_2, COLOUR_GREY, AID_WIDGET_BREAK_STR_ON_OFF_BTN), SetFill(0, 1), SetDataTip(SPR_FLAG_VEH_STOPPED, STR_AI_DEBUG_BREAK_STR_ON_OFF_TOOLTIP),
					NWidget(WWT_PANEL, COLOUR_GREY),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_LABEL, COLOUR_GREY), SetPadding(2, 2, 2, 4), SetDataTip(STR_AI_DEBUG_BREAK_ON_LABEL, 0x0),
							NWidget(WWT_EDITBOX, COLOUR_WHITE, AID_WIDGET_BREAK_STR_EDIT_BOX), SetFill(1, 1), SetResize(1, 0), SetPadding(2, 2, 2, 2), SetDataTip(STR_AI_DEBUG_BREAK_STR_OSKTITLE, STR_AI_DEBUG_BREAK_STR_TOOLTIP),
						EndContainer(),
					EndContainer(),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, AID_WIDGET_MATCH_CASE_BTN), SetMinimalSize(100, 0), SetFill(0, 1), SetDataTip(STR_AI_DEBUG_MATCH_CASE, STR_AI_DEBUG_MATCH_CASE_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, AID_WIDGET_CONTINUE_BTN), SetMinimalSize(100, 0), SetFill(0, 1), SetDataTip(STR_AI_DEBUG_CONTINUE, STR_AI_DEBUG_CONTINUE_TOOLTIP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, AID_WIDGET_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the AI debug window. */
static const WindowDesc _ai_debug_desc(
	WDP_AUTO, 600, 450,
	WC_AI_DEBUG, WC_NONE,
	0,
	_nested_ai_debug_widgets, lengthof(_nested_ai_debug_widgets)
);

/**
 * Open the AI debug window and select the given company.
 * @param show_company Display debug information about this AI company.
 */
void ShowAIDebugWindow(CompanyID show_company)
{
	if (!_networking || _network_server) {
		AIDebugWindow *w = (AIDebugWindow *)BringWindowToFrontById(WC_AI_DEBUG, 0);
		if (w == NULL) w = new AIDebugWindow(&_ai_debug_desc, 0);
		if (show_company != INVALID_COMPANY) w->ChangeToAI(show_company);
	} else {
		ShowErrorMessage(STR_ERROR_AI_DEBUG_SERVER_ONLY, INVALID_STRING_ID, WL_INFO);
	}
}

/**
 * Reset the AI windows to their initial state.
 */
void InitializeAIGui()
{
	AIDebugWindow::ai_debug_company = INVALID_COMPANY;
}

/** Open the AI debug window if one of the AI scripts has crashed. */
void ShowAIDebugWindowIfAIError()
{
	/* Network clients can't debug AIs. */
	if (_networking && !_network_server) return;

	Company *c;
	FOR_ALL_COMPANIES(c) {
		if (c->is_ai && c->ai_instance->IsDead()) {
			ShowAIDebugWindow(c->index);
			break;
		}
	}
}
