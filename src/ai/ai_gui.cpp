/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_gui.cpp Window for configuring the AIs */

#include "../stdafx.h"
#include "../table/sprites.h"
#include "../error.h"
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
#include "../script/api/script_log.hpp"
#include "ai_config.hpp"
#include "ai_info.hpp"
#include "ai_instance.hpp"

#include "../widgets/ai_widget.h"

#include "table/strings.h"

#include <vector>

/**
 * Window that let you choose an available AI.
 */
struct AIListWindow : public Window {
	const ScriptInfoList *ai_info_list; ///< The list of AIs.
	int selected;                       ///< The currently selected AI.
	CompanyID slot;                     ///< The company we're selecting a new AI for.
	int line_height;                    ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;                 ///< Cache of the vertical scrollbar.

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param slot The company we're changing the AI for.
	 */
	AIListWindow(const WindowDesc *desc, CompanyID slot) : Window(),
		slot(slot)
	{
		this->ai_info_list = AI::GetUniqueInfoList();

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(WID_AIL_SCROLLBAR);
		this->FinishInitNested(desc); // Initializes 'this->line_height' as side effect.

		this->vscroll->SetCount((int)this->ai_info_list->size() + 1);

		/* Try if we can find the currently selected AI */
		this->selected = -1;
		if (AIConfig::GetConfig(slot)->HasScript()) {
			AIInfo *info = AIConfig::GetConfig(slot)->GetInfo();
			int i = 0;
			for (ScriptInfoList::const_iterator it = this->ai_info_list->begin(); it != this->ai_info_list->end(); it++, i++) {
				if ((*it).second == info) {
					this->selected = i;
					break;
				}
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_AIL_LIST) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

			resize->width = 1;
			resize->height = this->line_height;
			size->height = GB(this->GetWidget<NWidgetCore>(widget)->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_AIL_LIST: {
				/* Draw a list of all available AIs. */
				int y = this->GetWidget<NWidgetBase>(WID_AIL_LIST)->pos_y;
				/* First AI in the list is hardcoded to random */
				if (this->vscroll->IsVisible(0)) {
					DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_LEFT, y + WD_MATRIX_TOP, STR_AI_CONFIG_RANDOM_AI, this->selected == -1 ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				ScriptInfoList::const_iterator it = this->ai_info_list->begin();
				for (int i = 1; it != this->ai_info_list->end(); i++, it++) {
					if (this->vscroll->IsVisible(i)) {
						DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, (*it).second->GetName(), (this->selected == i - 1) ? TC_WHITE : TC_BLACK);
						y += this->line_height;
					}
				}
				break;
			}
			case WID_AIL_INFO_BG: {
				AIInfo *selected_info = NULL;
				ScriptInfoList::const_iterator it = this->ai_info_list->begin();
				for (int i = 1; selected_info == NULL && it != this->ai_info_list->end(); i++, it++) {
					if (this->selected == i - 1) selected_info = static_cast<AIInfo *>((*it).second);
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

	/**
	 * Changes the AI of the current slot.
	 */
	void ChangeAI()
	{
		if (this->selected == -1) {
			AIConfig::GetConfig(slot)->Change(NULL);
		} else {
			ScriptInfoList::const_iterator it = this->ai_info_list->begin();
			for (int i = 0; i < this->selected; i++) it++;
			AIConfig::GetConfig(slot)->Change((*it).second->GetName(), (*it).second->GetVersion());
		}
		SetWindowDirty(WC_GAME_OPTIONS, WN_GAME_OPTIONS_AI);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_AIL_LIST: { // Select one of the AIs
				int sel = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_AIL_LIST, 0, this->line_height) - 1;
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

			case WID_AIL_ACCEPT: {
				this->ChangeAI();
				delete this;
				break;
			}

			case WID_AIL_CANCEL:
				delete this;
				break;
		}
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_AIL_LIST);
		this->vscroll->SetCapacity(nwi->current_y / this->line_height);
		nwi->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot)) {
			delete this;
			return;
		}

		if (!gui_scope) return;

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
		NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIL_LIST), SetMinimalSize(188, 112), SetFill(1, 1), SetResize(1, 1), SetDataTip(0x501, STR_AI_LIST_TOOLTIP), SetScrollbar(WID_AIL_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_AIL_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_AIL_INFO_BG), SetMinimalTextLines(8, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_AIL_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_LIST_ACCEPT, STR_AI_LIST_ACCEPT_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_AIL_CANCEL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_LIST_CANCEL, STR_AI_LIST_CANCEL_TOOLTIP),
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

/**
 * Window for settings the parameters of an AI.
 */
struct AISettingsWindow : public Window {
	CompanyID slot;                       ///< The currently show company's setting.
	AIConfig *ai_config;                  ///< The configuration we're modifying.
	int clicked_button;                   ///< The button we clicked.
	bool clicked_increase;                ///< Whether we clicked the increase or decrease button.
	int timeout;                          ///< Timeout for unclicking the button.
	int clicked_row;                      ///< The clicked row of settings.
	int line_height;                      ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;                   ///< Cache of the vertical scrollbar.
	typedef std::vector<const ScriptConfigItem *> VisibleSettingsList;
	VisibleSettingsList visible_settings; ///< List of visible AI settings

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param slot The company we're changing the settings for.
	 */
	AISettingsWindow(const WindowDesc *desc, CompanyID slot) : Window(),
		slot(slot),
		clicked_button(-1),
		timeout(0)
	{
		this->ai_config = AIConfig::GetConfig(slot);
		this->RebuildVisibleSettings();

		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(WID_AIS_SCROLLBAR);
		this->FinishInitNested(desc, slot);  // Initializes 'this->line_height' as side effect.

		this->SetWidgetDisabledState(WID_AIS_RESET, _game_mode != GM_MENU && Company::IsValidID(this->slot));

		this->vscroll->SetCount((int)this->visible_settings.size());
	}

	/**
	 * Rebuilds the list of visible settings. AI settings with the flag
	 * AICONFIG_AI_DEVELOPER set will only be visible if the game setting
	 * gui.ai_developer_tools is enabled.
	 */
	void RebuildVisibleSettings()
	{
		visible_settings.clear();

		ScriptConfigItemList::const_iterator it = this->ai_config->GetConfigList()->begin();
		for (; it != this->ai_config->GetConfigList()->end(); it++) {
			bool no_hide = (it->flags & SCRIPTCONFIG_DEVELOPER) == 0;
			if (no_hide || _settings_client.gui.ai_developer_tools) {
				visible_settings.push_back(&(*it));
			}
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_AIS_BACKGROUND) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

			resize->width = 1;
			resize->height = this->line_height;
			size->height = GB(this->GetWidget<NWidgetCore>(widget)->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_AIS_BACKGROUND) return;

		AIConfig *config = this->ai_config;
		VisibleSettingsList::const_iterator it = this->visible_settings.begin();
		int i = 0;
		for (; !this->vscroll->IsVisible(i); i++) it++;

		bool rtl = _current_text_dir == TD_RTL;
		uint buttons_left = rtl ? r.right - 23 : r.left + 4;
		uint text_left    = r.left + (rtl ? WD_FRAMERECT_LEFT : 28);
		uint text_right   = r.right - (rtl ? 28 : WD_FRAMERECT_RIGHT);


		int y = r.top;
		for (; this->vscroll->IsVisible(i) && it != visible_settings.end(); i++, it++) {
			const ScriptConfigItem &config_item = **it;
			int current_value = config->GetSetting((config_item).name);
			bool editable = _game_mode == GM_MENU || !Company::IsValidID(this->slot) || (config_item.flags & SCRIPTCONFIG_INGAME) != 0;

			StringID str;
			TextColour colour;
			uint idx = 0;
			if (StrEmpty(config_item.description)) {
				if (!strcmp(config_item.name, "start_date")) {
					/* Build-in translation */
					str = STR_AI_SETTINGS_START_DELAY;
					colour = TC_LIGHT_BLUE;
				} else {
					str = STR_JUST_STRING;
					colour = TC_ORANGE;
				}
			} else {
				str = STR_AI_SETTINGS_SETTING;
				colour = TC_LIGHT_BLUE;
				SetDParamStr(idx++, config_item.description);
			}

			if ((config_item.flags & SCRIPTCONFIG_BOOLEAN) != 0) {
				DrawFrameRect(buttons_left, y  + 2, buttons_left + 19, y + 10, (current_value != 0) ? COLOUR_GREEN : COLOUR_RED, (current_value != 0) ? FR_LOWERED : FR_NONE);
				SetDParam(idx++, current_value == 0 ? STR_CONFIG_SETTING_OFF : STR_CONFIG_SETTING_ON);
			} else {
				DrawArrowButtons(buttons_left, y + 2, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + (this->clicked_increase != rtl) : 0, editable && current_value > config_item.min_value, editable && current_value < config_item.max_value);
				if (config_item.labels != NULL && config_item.labels->Contains(current_value)) {
					SetDParam(idx++, STR_JUST_RAW_STRING);
					SetDParamStr(idx++, config_item.labels->Find(current_value)->second);
				} else {
					SetDParam(idx++, STR_JUST_INT);
					SetDParam(idx++, current_value);
				}
			}

			DrawString(text_left, text_right, y + WD_MATRIX_TOP, str, colour);
			y += this->line_height;
		}
	}

	/**
	 * Check whether we modified the difficulty level or not.
	 */
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
			case WID_AIS_BACKGROUND: {
				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_AIS_BACKGROUND);
				int num = (pt.y - wid->pos_y) / this->line_height + this->vscroll->GetPosition();
				if (num >= (int)this->visible_settings.size()) break;

				VisibleSettingsList::const_iterator it = this->visible_settings.begin();
				for (int i = 0; i < num; i++) it++;
				const ScriptConfigItem config_item = **it;
				if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot) && (config_item.flags & SCRIPTCONFIG_INGAME) == 0) return;

				bool bool_item = (config_item.flags & SCRIPTCONFIG_BOOLEAN) != 0;

				int x = pt.x - wid->pos_x;
				if (_current_text_dir == TD_RTL) x = wid->current_x - x;
				x -= 4;
				/* One of the arrows is clicked (or green/red rect in case of bool value) */
				if (IsInsideMM(x, 0, 21)) {
					int new_val = this->ai_config->GetSetting(config_item.name);
					int old_val = new_val;
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

					if (new_val != old_val) {
						this->ai_config->SetSetting(config_item.name, new_val);
						this->clicked_button = num;
						this->timeout = 5;

						this->CheckDifficultyLevel();
					}
				} else if (!bool_item) {
					/* Display a query box so users can enter a custom value. */
					this->clicked_row = num;
					SetDParam(0, this->ai_config->GetSetting(config_item.name));
					ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, 10, this, CS_NUMERAL, QSF_NONE);
				}
				this->SetDirty();
				break;
			}

			case WID_AIS_ACCEPT:
				delete this;
				break;

			case WID_AIS_RESET:
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
		ScriptConfigItemList::const_iterator it = this->ai_config->GetConfigList()->begin();
		for (int i = 0; i < this->clicked_row; i++) it++;
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot) && (it->flags & SCRIPTCONFIG_INGAME) == 0) return;
		int32 value = atoi(str);
		this->ai_config->SetSetting((*it).name, value);
		this->CheckDifficultyLevel();
		this->SetDirty();
	}

	virtual void OnResize()
	{
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_AIS_BACKGROUND);
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

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot)) {
			delete this;
		} else {
			this->RebuildVisibleSettings();
		}
	}
};

/** Widgets for the AI settings window. */
static const NWidgetPart _nested_ai_settings_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIS_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetFill(1, 0), SetDataTip(0x501, STR_NULL), SetScrollbar(WID_AIS_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_AIS_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_AIS_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_AIS_RESET), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_SETTINGS_RESET, STR_NULL),
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

/** Widgets for the configure AI window. */
static const NWidgetPart _nested_ai_config_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_AIC_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4),
			NWidget(NWID_HORIZONTAL), SetPIP(10, 0, 10),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_DECREASE), SetFill(0, 1), SetDataTip(AWV_DECREASE, STR_NULL),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_INCREASE), SetFill(0, 1), SetDataTip(AWV_INCREASE, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(6, 0),
				NWidget(WWT_TEXT, COLOUR_MAUVE, WID_AIC_NUMBER), SetDataTip(STR_DIFFICULTY_LEVEL_SETTING_MAXIMUM_NO_COMPETITORS, STR_NULL), SetFill(1, 0), SetPadding(1, 0, 0, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(10, 0, 10),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_UP), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_UP, STR_AI_CONFIG_MOVE_UP_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_DOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_DOWN, STR_AI_CONFIG_MOVE_DOWN_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIC_LIST), SetMinimalSize(288, 112), SetFill(1, 0), SetDataTip(0x801, STR_AI_CONFIG_LIST_TOOLTIP), SetScrollbar(WID_AIC_SCROLLBAR),
			NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_AIC_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(5, 0, 5),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CHANGE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_CONFIG_CHANGE, STR_AI_CONFIG_CHANGE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONFIGURE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_CONFIG_CONFIGURE, STR_AI_CONFIG_CONFIGURE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CLOSE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONTENT_DOWNLOAD), SetFill(1, 0), SetMinimalSize(279, 12), SetPadding(0, 5, 9, 5), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
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
	Scrollbar *vscroll;      ///< Cache of the vertical scrollbar.

	AIConfigWindow() : Window()
	{
		this->InitNested(&_ai_config_desc, WN_GAME_OPTIONS_AI); // Initializes 'this->line_height' as a side effect.
		this->vscroll = this->GetScrollbar(WID_AIC_SCROLLBAR);
		this->selected_slot = INVALID_COMPANY;
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_AIC_LIST);
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
			case WID_AIC_NUMBER:
				SetDParam(0, GetGameSettings().difficulty.max_no_competitors);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_AIC_LIST:
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
			case WID_AIC_LIST: {
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
			case WID_AIC_DECREASE:
			case WID_AIC_INCREASE: {
				int new_value;
				if (widget == WID_AIC_DECREASE) {
					new_value = max(0, GetGameSettings().difficulty.max_no_competitors - 1);
				} else {
					new_value = min(MAX_COMPANIES - 1, GetGameSettings().difficulty.max_no_competitors + 1);
				}
				IConsoleSetSetting("difficulty.max_no_competitors", new_value);
				this->InvalidateData();
				break;
			}

			case WID_AIC_LIST: { // Select a slot
				this->selected_slot = (CompanyID)this->vscroll->GetScrolledRowFromWidget(pt.y, this, widget, 0, this->line_height);
				this->InvalidateData();
				if (click_count > 1 && this->selected_slot != INVALID_COMPANY) ShowAIListWindow((CompanyID)this->selected_slot);
				break;
			}

			case WID_AIC_MOVE_UP:
				if (IsEditable(this->selected_slot) && IsEditable((CompanyID)(this->selected_slot - 1))) {
					Swap(GetGameSettings().ai_config[this->selected_slot], GetGameSettings().ai_config[this->selected_slot - 1]);
					this->selected_slot--;
					this->vscroll->ScrollTowards(this->selected_slot);
					this->InvalidateData();
				}
				break;

			case WID_AIC_MOVE_DOWN:
				if (IsEditable(this->selected_slot) && IsEditable((CompanyID)(this->selected_slot + 1))) {
					Swap(GetGameSettings().ai_config[this->selected_slot], GetGameSettings().ai_config[this->selected_slot + 1]);
					this->selected_slot++;
					this->vscroll->ScrollTowards(this->selected_slot);
					this->InvalidateData();
				}
				break;

			case WID_AIC_CHANGE:  // choose other AI
				ShowAIListWindow((CompanyID)this->selected_slot);
				break;

			case WID_AIC_CONFIGURE: // change the settings for an AI
				ShowAISettingsWindow((CompanyID)this->selected_slot);
				break;

			case WID_AIC_CLOSE:
				delete this;
				break;

			case WID_AIC_CONTENT_DOWNLOAD:
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

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!IsEditable(this->selected_slot)) {
			this->selected_slot = INVALID_COMPANY;
		}

		if (!gui_scope) return;

		this->SetWidgetDisabledState(WID_AIC_DECREASE, GetGameSettings().difficulty.max_no_competitors == 0);
		this->SetWidgetDisabledState(WID_AIC_INCREASE, GetGameSettings().difficulty.max_no_competitors == MAX_COMPANIES - 1);
		this->SetWidgetDisabledState(WID_AIC_CHANGE, this->selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(WID_AIC_CONFIGURE, this->selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(WID_AIC_MOVE_UP, this->selected_slot == INVALID_COMPANY || !IsEditable((CompanyID)(this->selected_slot - 1)));
		this->SetWidgetDisabledState(WID_AIC_MOVE_DOWN, this->selected_slot == INVALID_COMPANY || !IsEditable((CompanyID)(this->selected_slot + 1)));
	}
};

/** Open the AI config window. */
void ShowAIConfigWindow()
{
	DeleteWindowByClass(WC_GAME_OPTIONS);
	new AIConfigWindow();
}

/**
 * Window with everything an AI prints via ScriptLog.
 */
struct AIDebugWindow : public QueryStringBaseWindow {
	static const int top_offset;    ///< Offset of the text at the top of the WID_AID_LOG_PANEL.
	static const int bottom_offset; ///< Offset of the text at the bottom of the WID_AID_LOG_PANEL.

	static const unsigned int MAX_BREAK_STR_STRING_LENGTH = 256; ///< Maximum length of the break string.

	static CompanyID ai_debug_company;                     ///< The AI that is (was last) being debugged.
	int redraw_timer;                                      ///< Timer for redrawing the window, otherwise it'll happen every tick.
	int last_vscroll_pos;                                  ///< Last position of the scrolling.
	bool autoscroll;                                       ///< Whether automatically scrolling should be enabled or not.
	bool show_break_box;                                   ///< Whether the break/debug box is visible.
	static bool break_check_enabled;                       ///< Stop an AI when it prints a matching string
	static char break_string[MAX_BREAK_STR_STRING_LENGTH]; ///< The string to match to the AI output
	static bool case_sensitive_break_check;                ///< Is the matching done case-sensitive
	int highlight_row;                                     ///< The output row that matches the given string, or -1
	Scrollbar *vscroll;                                    ///< Cache of the vertical scrollbar.

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param number The window number (actually unused).
	 */
	AIDebugWindow(const WindowDesc *desc, WindowNumber number) : QueryStringBaseWindow(MAX_BREAK_STR_STRING_LENGTH)
	{
		this->CreateNestedTree(desc);
		this->vscroll = this->GetScrollbar(WID_AID_SCROLLBAR);
		this->show_break_box = _settings_client.gui.ai_developer_tools;
		this->GetWidget<NWidgetStacked>(WID_AID_BREAK_STRING_WIDGETS)->SetDisplayedPlane(this->show_break_box ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested(desc, number);

		if (!this->show_break_box) break_check_enabled = false;
		/* Disable the companies who are not active or not an AI */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + WID_AID_COMPANY_BUTTON_START, !Company::IsValidAiID(i));
		}
		this->DisableWidget(WID_AID_RELOAD_TOGGLE);
		this->DisableWidget(WID_AID_SETTINGS);
		this->DisableWidget(WID_AID_CONTINUE_BTN);

		this->last_vscroll_pos = 0;
		this->autoscroll = true;
		this->highlight_row = -1;
		InitializeTextBuffer(&this->text, this->edit_str_buf, this->edit_str_size, MAX_BREAK_STR_STRING_LENGTH);

		/* Restore the break string value from static variable */
		strecpy(this->edit_str_buf, this->break_string, this->edit_str_buf + MAX_BREAK_STR_STRING_LENGTH);
		UpdateTextBufferSize(&this->text);

		/* Restore button state from static class variables */
		if (ai_debug_company != INVALID_COMPANY) this->LowerWidget(ai_debug_company + WID_AID_COMPANY_BUTTON_START);
		this->SetWidgetLoweredState(WID_AID_BREAK_STR_ON_OFF_BTN, this->break_check_enabled);
		this->SetWidgetLoweredState(WID_AID_MATCH_CASE_BTN, this->case_sensitive_break_check);

	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_AID_LOG_PANEL) {
			resize->height = FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			size->height = 14 * resize->height + this->top_offset + this->bottom_offset;
		}
	}

	virtual void OnPaint()
	{
		/* Check if the currently selected company is still active. */
		if (ai_debug_company == INVALID_COMPANY || !Company::IsValidAiID(ai_debug_company)) {
			if (ai_debug_company != INVALID_COMPANY) {
				/* Raise the widget for the previous selection. */
				this->RaiseWidget(ai_debug_company + WID_AID_COMPANY_BUTTON_START);

				ai_debug_company = INVALID_COMPANY;
			}

			const Company *c;
			FOR_ALL_COMPANIES(c) {
				if (c->is_ai) {
					/* Lower the widget corresponding to this company. */
					this->LowerWidget(c->index + WID_AID_COMPANY_BUTTON_START);

					ai_debug_company = c->index;
					break;
				}
			}
		}

		/* Update "Reload AI" and "AI settings" buttons */
		this->SetWidgetsDisabledState(ai_debug_company == INVALID_COMPANY,
			WID_AID_RELOAD_TOGGLE,
			WID_AID_SETTINGS,
			WIDGET_LIST_END);

		/* Draw standard stuff */
		this->DrawWidgets();

		if (this->IsShaded()) return; // Don't draw anything when the window is shaded.

		if (this->show_break_box) this->DrawEditBox(WID_AID_BREAK_STR_EDIT_BOX);

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			NWidgetCore *button = this->GetWidget<NWidgetCore>(i + WID_AID_COMPANY_BUTTON_START);
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
			DrawCompanyIcon(i, button->pos_x + button->current_x / 2 - 7 + offset, this->GetWidget<NWidgetBase>(WID_AID_COMPANY_BUTTON_START + i)->pos_y + 2 + offset);
		}

		/* If there are no active companies, don't display anything else. */
		if (ai_debug_company == INVALID_COMPANY) return;

		ScriptLog::LogData *log = (ScriptLog::LogData *)Company::Get(ai_debug_company)->ai_instance->GetLogPointer();

		int scroll_count = (log == NULL) ? 0 : log->used;
		if (this->vscroll->GetCount() != scroll_count) {
			this->vscroll->SetCount(scroll_count);

			/* We need a repaint */
			this->SetWidgetDirty(WID_AID_SCROLLBAR);
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
				this->SetWidgetDirty(WID_AID_SCROLLBAR);
				this->SetWidgetDirty(WID_AID_LOG_PANEL);
			}
		}
		this->last_vscroll_pos = this->vscroll->GetPosition();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_AID_NAME_TEXT:
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
			case WID_AID_LOG_PANEL: {
				ScriptLog::LogData *log = (ScriptLog::LogData *)Company::Get(ai_debug_company)->ai_instance->GetLogPointer();
				if (log == NULL) return;

				int y = this->top_offset;
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < log->used; i++) {
					int pos = (i + log->pos + 1 - log->used + log->count) % log->count;
					if (log->lines[pos] == NULL) break;

					TextColour colour;
					switch (log->type[pos]) {
						case ScriptLog::LOG_SQ_INFO:  colour = TC_BLACK;  break;
						case ScriptLog::LOG_SQ_ERROR: colour = TC_RED;    break;
						case ScriptLog::LOG_INFO:     colour = TC_BLACK;  break;
						case ScriptLog::LOG_WARNING:  colour = TC_YELLOW; break;
						case ScriptLog::LOG_ERROR:    colour = TC_RED;    break;
						default:                  colour = TC_BLACK;  break;
					}

					/* Check if the current line should be highlighted */
					if (pos == this->highlight_row) {
						GfxFillRect(r.left + 1, r.top + y, r.right - 1, r.top + y + this->resize.step_height - WD_PAR_VSEP_NORMAL, PC_BLACK);
						if (colour == TC_BLACK) colour = TC_WHITE; // Make black text readable by inverting it to white.
					}

					DrawString(r.left + 7, r.right - 7, r.top + y, log->lines[pos], colour, SA_LEFT | SA_FORCE);
					y += this->resize.step_height;
				}
				break;
			}
		}
	}

	/**
	 * Change all settings to select another AI.
	 * @param show_ai The new AI to show.
	 */
	void ChangeToAI(CompanyID show_ai)
	{
		this->RaiseWidget(ai_debug_company + WID_AID_COMPANY_BUTTON_START);
		ai_debug_company = show_ai;

		ScriptLog::LogData *log = (ScriptLog::LogData *)Company::Get(ai_debug_company)->ai_instance->GetLogPointer();
		this->vscroll->SetCount((log == NULL) ? 0 : log->used);

		this->LowerWidget(ai_debug_company + WID_AID_COMPANY_BUTTON_START);
		this->autoscroll = true;
		this->last_vscroll_pos = this->vscroll->GetPosition();
		this->SetDirty();
		/* Close AI settings window to prevent confusion */
		DeleteWindowByClass(WC_AI_SETTINGS);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, WID_AID_COMPANY_BUTTON_START, WID_AID_COMPANY_BUTTON_END + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				ChangeToAI((CompanyID)(widget - WID_AID_COMPANY_BUTTON_START));
			}
		}

		switch (widget) {
			case WID_AID_RELOAD_TOGGLE:
				/* First kill the company of the AI, then start a new one. This should start the current AI again */
				DoCommandP(0, 2 | ai_debug_company << 16, CRR_MANUAL, CMD_COMPANY_CTRL);
				DoCommandP(0, 1 | ai_debug_company << 16, 0, CMD_COMPANY_CTRL);
				break;

			case WID_AID_SETTINGS:
				ShowAISettingsWindow(ai_debug_company);
				break;

			case WID_AID_BREAK_STR_ON_OFF_BTN:
				this->break_check_enabled = !this->break_check_enabled;
				this->SetWidgetLoweredState(WID_AID_BREAK_STR_ON_OFF_BTN, this->break_check_enabled);
				this->SetWidgetDirty(WID_AID_BREAK_STR_ON_OFF_BTN);
				break;

			case WID_AID_MATCH_CASE_BTN:
				this->case_sensitive_break_check = !this->case_sensitive_break_check;
				this->SetWidgetLoweredState(WID_AID_MATCH_CASE_BTN, this->case_sensitive_break_check);
				break;

			case WID_AID_CONTINUE_BTN:
				/* Unpause */
				DoCommandP(0, PM_PAUSED_NORMAL, 0, CMD_PAUSE);
				this->DisableWidget(WID_AID_CONTINUE_BTN);
				this->RaiseWidget(WID_AID_CONTINUE_BTN); // Disabled widgets don't raise themself
				break;
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(WID_AID_RELOAD_TOGGLE);
		this->RaiseWidget(WID_AID_SETTINGS);
		this->SetDirty();
	}

	virtual void OnMouseLoop()
	{
		this->HandleEditBox(WID_AID_BREAK_STR_EDIT_BOX);
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		EventState state = ES_NOT_HANDLED;
		if (this->HandleEditBoxKey(WID_AID_BREAK_STR_EDIT_BOX, key, keycode, state) != HEBR_NOT_FOCUSED) {
			/* Save the current string to static member so it can be restored next time the window is opened */
			strecpy(this->break_string, this->edit_str_buf, lastof(this->break_string));
		}
		return state;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (data == -1 || ai_debug_company == data) this->SetDirty();

		if (gui_scope && data == -2) {
			/* The continue button should be disabled when the game is unpaused and
			 * it was previously paused by the break string ( = a line in the log
			 * was highlighted )*/
			if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED && this->highlight_row != -1) {
				this->DisableWidget(WID_AID_CONTINUE_BTN);
				this->SetWidgetDirty(WID_AID_CONTINUE_BTN);
				this->SetWidgetDirty(WID_AID_LOG_PANEL);
				this->highlight_row = -1;
			}
		}

		/* If the log message is related to the active company tab, check the break string.
		 * This needs to be done in gameloop-scope, so the AI is suspended immediately. */
		if (!gui_scope && data == ai_debug_company && this->break_check_enabled && !StrEmpty(this->edit_str_buf)) {
			/* Get the log instance of the active company */
			ScriptLog::LogData *log = (ScriptLog::LogData *)Company::Get(ai_debug_company)->ai_instance->GetLogPointer();

			if (log != NULL && case_sensitive_break_check?
					strstr(log->lines[log->pos], this->edit_str_buf) != 0 :
					strcasestr(log->lines[log->pos], this->edit_str_buf) != 0) {

				AI::Suspend(ai_debug_company);
				if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED) {
					DoCommandP(0, PM_PAUSED_NORMAL, 1, CMD_PAUSE);
				}

				/* Make it possible to click on the continue button */
				this->EnableWidget(WID_AID_CONTINUE_BTN);
				this->SetWidgetDirty(WID_AID_CONTINUE_BTN);

				/* Highlight row that matched */
				this->highlight_row = log->pos;
			}
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_AID_LOG_PANEL);
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
	return MakeCompanyButtonRows(biggest_index, WID_AID_COMPANY_BUTTON_START, WID_AID_COMPANY_BUTTON_END, 8, STR_AI_DEBUG_SELECT_AI_TOOLTIP);
}

/** Widgets for the AI debug window. */
static const NWidgetPart _nested_ai_debug_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_AI_DEBUG, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_AID_VIEW),
		NWidgetFunction(MakeCompanyButtonRowsAIDebug), SetPadding(0, 2, 1, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_AID_NAME_TEXT), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_STRING, STR_AI_DEBUG_NAME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_AID_SETTINGS), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_SETTINGS, STR_AI_DEBUG_SETTINGS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_AID_RELOAD_TOGGLE), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_RELOAD, STR_AI_DEBUG_RELOAD_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			/* Log panel */
			NWidget(WWT_PANEL, COLOUR_GREY, WID_AID_LOG_PANEL), SetMinimalSize(287, 180), SetResize(1, 1), SetScrollbar(WID_AID_SCROLLBAR),
			EndContainer(),
			/* Break string widgets */
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_AID_BREAK_STRING_WIDGETS),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_IMGBTN_2, COLOUR_GREY, WID_AID_BREAK_STR_ON_OFF_BTN), SetFill(0, 1), SetDataTip(SPR_FLAG_VEH_STOPPED, STR_AI_DEBUG_BREAK_STR_ON_OFF_TOOLTIP),
					NWidget(WWT_PANEL, COLOUR_GREY),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_LABEL, COLOUR_GREY), SetPadding(2, 2, 2, 4), SetDataTip(STR_AI_DEBUG_BREAK_ON_LABEL, 0x0),
							NWidget(WWT_EDITBOX, COLOUR_WHITE, WID_AID_BREAK_STR_EDIT_BOX), SetFill(1, 1), SetResize(1, 0), SetPadding(2, 2, 2, 2), SetDataTip(STR_AI_DEBUG_BREAK_STR_OSKTITLE, STR_AI_DEBUG_BREAK_STR_TOOLTIP),
						EndContainer(),
					EndContainer(),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_AID_MATCH_CASE_BTN), SetMinimalSize(100, 0), SetFill(0, 1), SetDataTip(STR_AI_DEBUG_MATCH_CASE, STR_AI_DEBUG_MATCH_CASE_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_AID_CONTINUE_BTN), SetMinimalSize(100, 0), SetFill(0, 1), SetDataTip(STR_AI_DEBUG_CONTINUE, STR_AI_DEBUG_CONTINUE_TOOLTIP),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_AID_SCROLLBAR),
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
