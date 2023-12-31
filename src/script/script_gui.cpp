/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file script_gui.cpp %Window for configuring the Scripts */

#include "../stdafx.h"
#include "../table/sprites.h"
#include "../error.h"
#include "../settings_gui.h"
#include "../querystring_gui.h"
#include "../stringfilter_type.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../window_func.h"
#include "../network/network.h"
#include "../widgets/dropdown_func.h"
#include "../hotkeys.h"
#include "../company_cmd.h"
#include "../misc_cmd.h"
#include "../timer/timer.h"
#include "../timer/timer_window.h"

#include "script_gui.h"
#include "script_log.hpp"
#include "script_scanner.hpp"
#include "script_config.hpp"
#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../ai/ai_info.hpp"
#include "../ai/ai_instance.hpp"
#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../game/game_info.hpp"
#include "../game/game_instance.hpp"
#include "table/strings.h"

#include "../safeguards.h"


static ScriptConfig *GetConfig(CompanyID slot)
{
	if (slot == OWNER_DEITY) return GameConfig::GetConfig();
	return AIConfig::GetConfig(slot);
}

/**
 * Window that let you choose an available Script.
 */
struct ScriptListWindow : public Window {
	const ScriptInfoList *info_list;    ///< The list of Scripts.
	int selected;                       ///< The currently selected Script.
	CompanyID slot;                     ///< The company we're selecting a new Script for.
	int line_height;                    ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;                 ///< Cache of the vertical scrollbar.
	bool show_all;                      ///< Whether to show all available versions.

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param slot The company we're changing the Script for.
	 * @param show_all Whether to show all available versions.
	 */
	ScriptListWindow(WindowDesc *desc, CompanyID slot, bool show_all) : Window(desc),
		slot(slot), show_all(show_all)
	{
		if (slot == OWNER_DEITY) {
			this->info_list = this->show_all ? Game::GetInfoList() : Game::GetUniqueInfoList();
		} else {
			this->info_list = this->show_all ? AI::GetInfoList() : AI::GetUniqueInfoList();
		}

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SCRL_SCROLLBAR);
		this->FinishInitNested(); // Initializes 'this->line_height' as side effect.

		this->vscroll->SetCount(this->info_list->size() + 1);

		/* Try if we can find the currently selected AI */
		this->selected = -1;
		if (GetConfig(slot)->HasScript()) {
			ScriptInfo *info = GetConfig(slot)->GetInfo();
			int i = 0;
			for (const auto &item : *this->info_list) {
				if (item.second == info) {
					this->selected = i;
					break;
				}

				i++;
			}
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget != WID_SCRL_CAPTION) return;

		SetDParam(0, (this->slot == OWNER_DEITY) ? STR_AI_LIST_CAPTION_GAMESCRIPT : STR_AI_LIST_CAPTION_AI);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_SCRL_LIST) return;

		this->line_height = GetCharacterHeight(FS_NORMAL) + padding.height;

		resize->width = 1;
		resize->height = this->line_height;
		size->height = 5 * this->line_height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SCRL_LIST: {
				/* Draw a list of all available Scripts. */
				Rect tr = r.Shrink(WidgetDimensions::scaled.matrix);
				/* First AI in the list is hardcoded to random */
				if (this->vscroll->IsVisible(0)) {
					DrawString(tr, this->slot == OWNER_DEITY ? STR_AI_CONFIG_NONE : STR_AI_CONFIG_RANDOM_AI, this->selected == -1 ? TC_WHITE : TC_ORANGE);
					tr.top += this->line_height;
				}
				StringID str = this->show_all ? STR_AI_CONFIG_NAME_VERSION : STR_JUST_RAW_STRING;
				int i = 0;
				for (const auto &item : *this->info_list) {
					i++;
					if (this->vscroll->IsVisible(i)) {
						SetDParamStr(0, item.second->GetName());
						SetDParam(1, item.second->GetVersion());
						DrawString(tr, str, (this->selected == i - 1) ? TC_WHITE : TC_ORANGE);
						tr.top += this->line_height;
					}
				}
				break;
			}
			case WID_SCRL_INFO_BG: {
				ScriptInfo *selected_info = nullptr;
				int i = 0;
				for (const auto &item : *this->info_list) {
					i++;
					if (this->selected == i - 1) selected_info = static_cast<ScriptInfo *>(item.second);
				}
				/* Some info about the currently selected Script. */
				if (selected_info != nullptr) {
					Rect tr = r.Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect);
					SetDParamStr(0, selected_info->GetAuthor());
					DrawString(tr, STR_AI_LIST_AUTHOR);
					tr.top += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
					SetDParam(0, selected_info->GetVersion());
					DrawString(tr, STR_AI_LIST_VERSION);
					tr.top += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
					if (!selected_info->GetURL().empty()) {
						SetDParamStr(0, selected_info->GetURL());
						DrawString(tr, STR_AI_LIST_URL);
						tr.top += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
					}
					SetDParamStr(0, selected_info->GetDescription());
					DrawStringMultiLine(tr, STR_JUST_RAW_STRING, TC_WHITE);
				}
				break;
			}
		}
	}

	/**
	 * Changes the Script of the current slot.
	 */
	void ChangeScript()
	{
		if (this->selected == -1) {
			GetConfig(slot)->Change(std::nullopt);
		} else {
			ScriptInfoList::const_iterator it = this->info_list->cbegin();
			std::advance(it, this->selected);
			GetConfig(slot)->Change(it->second->GetName(), it->second->GetVersion());
		}
		InvalidateWindowData(WC_GAME_OPTIONS, slot == OWNER_DEITY ? WN_GAME_OPTIONS_GS : WN_GAME_OPTIONS_AI);
		InvalidateWindowClassesData(WC_SCRIPT_SETTINGS);
		CloseWindowByClass(WC_QUERY_STRING);
		InvalidateWindowClassesData(WC_TEXTFILE);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SCRL_LIST: { // Select one of the Scripts
				int sel = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_SCRL_LIST) - 1;
				if (sel < (int)this->info_list->size()) {
					this->selected = sel;
					this->SetDirty();
					if (click_count > 1) {
						this->ChangeScript();
						this->Close();
					}
				}
				break;
			}

			case WID_SCRL_ACCEPT: {
				this->ChangeScript();
				this->Close();
				break;
			}

			case WID_SCRL_CANCEL:
				this->Close();
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SCRL_LIST);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot)) {
			this->Close();
			return;
		}

		if (!gui_scope) return;

		this->vscroll->SetCount(this->info_list->size() + 1);

		/* selected goes from -1 .. length of ai list - 1. */
		this->selected = std::min(this->selected, this->vscroll->GetCount() - 2);
	}
};

/** Widgets for the AI list window. */
static const NWidgetPart _nested_script_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_SCRL_CAPTION), SetDataTip(STR_AI_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_SCRL_LIST), SetMinimalSize(188, 112), SetFill(1, 1), SetResize(1, 1), SetMatrixDataTip(1, 0, STR_AI_LIST_TOOLTIP), SetScrollbar(WID_SCRL_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_SCRL_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_SCRL_INFO_BG), SetMinimalTextLines(8, WidgetDimensions::unscaled.framerect.Vertical() + WidgetDimensions::unscaled.vsep_normal * 3), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_SCRL_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_LIST_ACCEPT, STR_AI_LIST_ACCEPT_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_SCRL_CANCEL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_LIST_CANCEL, STR_AI_LIST_CANCEL_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/** Window definition for the ai list window. */
static WindowDesc _script_list_desc(__FILE__, __LINE__,
	WDP_CENTER, "settings_script_list", 200, 234,
	WC_SCRIPT_LIST, WC_NONE,
	0,
	std::begin(_nested_script_list_widgets), std::end(_nested_script_list_widgets)
);

/**
 * Open the Script list window to chose a script for the given company slot.
 * @param slot The slot to change the script of.
 * @param show_all Whether to show all available versions.
 */
void ShowScriptListWindow(CompanyID slot, bool show_all)
{
	CloseWindowByClass(WC_SCRIPT_LIST);
	new ScriptListWindow(&_script_list_desc, slot, show_all);
}


/**
 * Window for settings the parameters of an AI.
 */
struct ScriptSettingsWindow : public Window {
	CompanyID slot;                       ///< The currently show company's setting.
	ScriptConfig *script_config;          ///< The configuration we're modifying.
	int clicked_button;                   ///< The button we clicked.
	bool clicked_increase;                ///< Whether we clicked the increase or decrease button.
	bool clicked_dropdown;                ///< Whether the dropdown is open.
	bool closing_dropdown;                ///< True, if the dropdown list is currently closing.
	int clicked_row;                      ///< The clicked row of settings.
	int line_height;                      ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;                   ///< Cache of the vertical scrollbar.
	typedef std::vector<const ScriptConfigItem *> VisibleSettingsList; ///< typdef for a vector of script settings
	VisibleSettingsList visible_settings; ///< List of visible AI settings

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param slot The company we're changing the settings for.
	 */
	ScriptSettingsWindow(WindowDesc *desc, CompanyID slot) : Window(desc),
		slot(slot),
		clicked_button(-1),
		clicked_dropdown(false),
		closing_dropdown(false)
	{
		this->script_config = GetConfig(slot);

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SCRS_SCROLLBAR);
		this->FinishInitNested(slot);  // Initializes 'this->line_height' as side effect.

		this->RebuildVisibleSettings();
	}

	/**
	 * Rebuilds the list of visible settings. AI settings with the flag
	 * AICONFIG_AI_DEVELOPER set will only be visible if the game setting
	 * gui.ai_developer_tools is enabled.
	 */
	void RebuildVisibleSettings()
	{
		visible_settings.clear();

		for (const auto &item : *this->script_config->GetConfigList()) {
			bool no_hide = (item.flags & SCRIPTCONFIG_DEVELOPER) == 0;
			if (no_hide || _settings_client.gui.ai_developer_tools) {
				visible_settings.push_back(&item);
			}
		}

		this->vscroll->SetCount(this->visible_settings.size());
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget != WID_SCRS_CAPTION) return;

		SetDParam(0, (this->slot == OWNER_DEITY) ? STR_AI_SETTINGS_CAPTION_GAMESCRIPT : STR_AI_SETTINGS_CAPTION_AI);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_SCRS_BACKGROUND) return;

		this->line_height = std::max(SETTING_BUTTON_HEIGHT, GetCharacterHeight(FS_NORMAL)) + padding.height;

		resize->width = 1;
		resize->height = this->line_height;
		size->height = 5 * this->line_height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_SCRS_BACKGROUND) return;

		ScriptConfig *config = this->script_config;
		VisibleSettingsList::const_iterator it = this->visible_settings.begin();
		int i = 0;
		for (; !this->vscroll->IsVisible(i); i++) it++;

		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		bool rtl = _current_text_dir == TD_RTL;
		Rect br = ir.WithWidth(SETTING_BUTTON_WIDTH, rtl);
		Rect tr = ir.Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide, rtl);

		int y = r.top;
		int button_y_offset = (this->line_height - SETTING_BUTTON_HEIGHT) / 2;
		int text_y_offset = (this->line_height - GetCharacterHeight(FS_NORMAL)) / 2;
		for (; this->vscroll->IsVisible(i) && it != visible_settings.end(); i++, it++) {
			const ScriptConfigItem &config_item = **it;
			int current_value = config->GetSetting((config_item).name);
			bool editable = this->IsEditableItem(config_item);

			StringID str;
			TextColour colour;
			uint idx = 0;
			if (config_item.description.empty()) {
				str = STR_JUST_STRING1;
				colour = TC_ORANGE;
			} else {
				str = STR_AI_SETTINGS_SETTING;
				colour = TC_LIGHT_BLUE;
				SetDParamStr(idx++, config_item.description);
			}

			if ((config_item.flags & SCRIPTCONFIG_BOOLEAN) != 0) {
				DrawBoolButton(br.left, y + button_y_offset, current_value != 0, editable);
				SetDParam(idx++, current_value == 0 ? STR_CONFIG_SETTING_OFF : STR_CONFIG_SETTING_ON);
			} else {
				if (config_item.complete_labels) {
					DrawDropDownButton(br.left, y + button_y_offset, COLOUR_YELLOW, this->clicked_row == i && clicked_dropdown, editable);
				} else {
					DrawArrowButtons(br.left, y + button_y_offset, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + (this->clicked_increase != rtl) : 0, editable && current_value > config_item.min_value, editable && current_value < config_item.max_value);
				}

				auto config_iterator = config_item.labels.find(current_value);
				if (config_iterator != config_item.labels.end()) {
					SetDParam(idx++, STR_JUST_RAW_STRING);
					SetDParamStr(idx++, config_iterator->second);
				} else {
					SetDParam(idx++, STR_JUST_INT);
					SetDParam(idx++, current_value);
				}
			}

			DrawString(tr.left, tr.right, y + text_y_offset, str, colour);
			y += this->line_height;
		}
	}

	void OnPaint() override
	{
		if (this->closing_dropdown) {
			this->closing_dropdown = false;
			this->clicked_dropdown = false;
		}
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SCRS_BACKGROUND: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->visible_settings, pt.y, this, widget);
				if (it == this->visible_settings.end()) break;

				const ScriptConfigItem &config_item = **it;
				if (!this->IsEditableItem(config_item)) return;

				int num = it - this->visible_settings.begin();
				if (this->clicked_row != num) {
					this->CloseChildWindows(WC_QUERY_STRING);
					this->CloseChildWindows(WC_DROPDOWN_MENU);
					this->clicked_row = num;
					this->clicked_dropdown = false;
				}

				bool bool_item = (config_item.flags & SCRIPTCONFIG_BOOLEAN) != 0;

				Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.matrix, RectPadding::zero);
				int x = pt.x - r.left;
				if (_current_text_dir == TD_RTL) x = r.Width() - 1 - x;

				/* One of the arrows is clicked (or green/red rect in case of bool value) */
				int old_val = this->script_config->GetSetting(config_item.name);
				if (!bool_item && IsInsideMM(x, 0, SETTING_BUTTON_WIDTH) && config_item.complete_labels) {
					if (this->clicked_dropdown) {
						/* unclick the dropdown */
						this->CloseChildWindows(WC_DROPDOWN_MENU);
						this->clicked_dropdown = false;
						this->closing_dropdown = false;
					} else {
						int rel_y = (pt.y - r.top) % this->line_height;

						Rect wi_rect;
						wi_rect.left = pt.x - (_current_text_dir == TD_RTL ? SETTING_BUTTON_WIDTH - 1 - x : x);
						wi_rect.right = wi_rect.left + SETTING_BUTTON_WIDTH - 1;
						wi_rect.top = pt.y - rel_y + (this->line_height - SETTING_BUTTON_HEIGHT) / 2;
						wi_rect.bottom = wi_rect.top + SETTING_BUTTON_HEIGHT - 1;

						/* If the mouse is still held but dragged outside of the dropdown list, keep the dropdown open */
						if (pt.y >= wi_rect.top && pt.y <= wi_rect.bottom) {
							this->clicked_dropdown = true;
							this->closing_dropdown = false;

							DropDownList list;
							for (int i = config_item.min_value; i <= config_item.max_value; i++) {
								list.push_back(std::make_unique<DropDownListStringItem>(config_item.labels.find(i)->second, i, false));
							}

							ShowDropDownListAt(this, std::move(list), old_val, WID_SCRS_SETTING_DROPDOWN, wi_rect, COLOUR_ORANGE);
						}
					}
				} else if (IsInsideMM(x, 0, SETTING_BUTTON_WIDTH)) {
					int new_val = old_val;
					if (bool_item) {
						new_val = !new_val;
					} else if (x >= SETTING_BUTTON_WIDTH / 2) {
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
						this->script_config->SetSetting(config_item.name, new_val);
						this->clicked_button = num;
						this->unclick_timeout.Reset();
					}
				} else if (!bool_item && !config_item.complete_labels) {
					/* Display a query box so users can enter a custom value. */
					SetDParam(0, old_val);
					ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, INT32_DIGITS_WITH_SIGN_AND_TERMINATION, this, CS_NUMERAL_SIGNED, QSF_NONE);
				}
				this->SetDirty();
				break;
			}

			case WID_SCRS_ACCEPT:
				this->Close();
				break;

			case WID_SCRS_RESET:
				this->script_config->ResetEditableSettings(_game_mode == GM_MENU || ((this->slot != OWNER_DEITY) && !Company::IsValidID(this->slot)));
				this->SetDirty();
				break;
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (StrEmpty(str)) return;
		int32_t value = atoi(str);

		SetValue(value);
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		if (widget != WID_SCRS_SETTING_DROPDOWN) return;
		assert(this->clicked_dropdown);
		SetValue(index);
	}

	void OnDropdownClose(Point, WidgetID widget, int, bool) override
	{
		if (widget != WID_SCRS_SETTING_DROPDOWN) return;
		/* We cannot raise the dropdown button just yet. OnClick needs some hint, whether
		 * the same dropdown button was clicked again, and then not open the dropdown again.
		 * So, we only remember that it was closed, and process it on the next OnPaint, which is
		 * after OnClick. */
		assert(this->clicked_dropdown);
		this->closing_dropdown = true;
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SCRS_BACKGROUND);
	}

	/** When reset, unclick the button after a small timeout. */
	TimeoutTimer<TimerWindow> unclick_timeout = {std::chrono::milliseconds(150), [this]() {
		this->clicked_button = -1;
		this->SetDirty();
	}};

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->RebuildVisibleSettings();
		this->CloseChildWindows(WC_DROPDOWN_MENU);
		this->CloseChildWindows(WC_QUERY_STRING);
	}

private:
	bool IsEditableItem(const ScriptConfigItem &config_item) const
	{
		return _game_mode == GM_MENU
			|| _game_mode == GM_EDITOR
			|| ((this->slot != OWNER_DEITY) && !Company::IsValidID(this->slot))
			|| (config_item.flags & SCRIPTCONFIG_INGAME) != 0
			|| _settings_client.gui.ai_developer_tools;
	}

	void SetValue(int value)
	{
		const ScriptConfigItem &config_item = *this->visible_settings[this->clicked_row];
		if (_game_mode == GM_NORMAL && ((this->slot == OWNER_DEITY) || Company::IsValidID(this->slot)) && (config_item.flags & SCRIPTCONFIG_INGAME) == 0) return;
		this->script_config->SetSetting(config_item.name, value);
		this->SetDirty();
	}
};

/** Widgets for the Script settings window. */
static const NWidgetPart _nested_script_settings_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_SCRS_CAPTION), SetDataTip(STR_AI_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_SCRS_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_SCRS_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_SCRS_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_SCRS_ACCEPT), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_SCRS_RESET), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_SETTINGS_RESET, STR_NULL),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

/** Window definition for the Script settings window. */
static WindowDesc _script_settings_desc(__FILE__, __LINE__,
	WDP_CENTER, "settings_script", 500, 208,
	WC_SCRIPT_SETTINGS, WC_NONE,
	0,
	std::begin(_nested_script_settings_widgets), std::end(_nested_script_settings_widgets)
);

/**
 * Open the Script settings window to change the Script settings for a Script.
 * @param slot The CompanyID of the Script to change the settings.
 */
void ShowScriptSettingsWindow(CompanyID slot)
{
	CloseWindowByClass(WC_SCRIPT_LIST);
	CloseWindowByClass(WC_SCRIPT_SETTINGS);
	new ScriptSettingsWindow(&_script_settings_desc, slot);
}


/** Window for displaying the textfile of a AI. */
struct ScriptTextfileWindow : public TextfileWindow {
	CompanyID slot; ///< View the textfile of this CompanyID slot.

	ScriptTextfileWindow(TextfileType file_type, CompanyID slot) : TextfileWindow(file_type), slot(slot)
	{
		this->OnInvalidateData();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget == WID_TF_CAPTION) {
			SetDParam(0, (slot == OWNER_DEITY) ? STR_CONTENT_TYPE_GAME_SCRIPT : STR_CONTENT_TYPE_AI);
			SetDParamStr(1, GetConfig(slot)->GetInfo()->GetName());
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		auto textfile = GetConfig(slot)->GetTextfile(file_type, slot);
		if (!textfile.has_value()) {
			this->Close();
		} else {
			this->LoadTextfile(textfile.value(), (slot == OWNER_DEITY) ? GAME_DIR : AI_DIR);
		}
	}
};

/**
 * Open the Script version of the textfile window.
 * @param file_type The type of textfile to display.
 * @param slot The slot the Script is using.
 */
void ShowScriptTextfileWindow(TextfileType file_type, CompanyID slot)
{
	CloseWindowById(WC_TEXTFILE, file_type);
	new ScriptTextfileWindow(file_type, slot);
}


/**
 * Set the widget colour of a button based on the
 * state of the script. (dead or alive)
 * @param button the button to update.
 * @param dead true if the script is dead, otherwise false.
 * @param paused true if the script is paused, otherwise false.
 * @return true if the colour was changed and the window need to be marked as dirty.
 */
static bool SetScriptButtonColour(NWidgetCore &button, bool dead, bool paused)
{
	/* Dead scripts are indicated with red background and
	 * paused scripts are indicated with yellow background. */
	Colours colour = dead ? COLOUR_RED :
		(paused ? COLOUR_YELLOW : COLOUR_GREY);
	if (button.colour != colour) {
		button.colour = colour;
		return true;
	}
	return false;
}

/**
 * Window with everything an AI prints via ScriptLog.
 */
struct ScriptDebugWindow : public Window {
	static const uint MAX_BREAK_STR_STRING_LENGTH = 256;   ///< Maximum length of the break string.

	struct FilterState {
		std::string break_string; ///< The string to match to the AI output
		CompanyID script_debug_company; ///< The AI that is (was last) being debugged.
		bool break_check_enabled; ///< Stop an AI when it prints a matching string
		bool case_sensitive_break_check; ///< Is the matching done case-sensitive
	};

	static inline FilterState initial_state = {
		"",
		INVALID_COMPANY,
		true,
		false,
	};

	int redraw_timer;                                      ///< Timer for redrawing the window, otherwise it'll happen every tick.
	int last_vscroll_pos;                                  ///< Last position of the scrolling.
	bool autoscroll;                                       ///< Whether automatically scrolling should be enabled or not.
	bool show_break_box;                                   ///< Whether the break/debug box is visible.
	QueryString break_editbox;                             ///< Break editbox
	StringFilter break_string_filter;                      ///< Log filter for break.
	int highlight_row;                                     ///< The output row that matches the given string, or -1
	Scrollbar *vscroll;                                    ///< Cache of the vertical scrollbar.
	Scrollbar *hscroll;                                    ///< Cache of the horizontal scrollbar.
	FilterState filter;

	ScriptLogTypes::LogData &GetLogData() const
	{
		if (this->filter.script_debug_company == OWNER_DEITY) return Game::GetInstance()->GetLogData();
		return Company::Get(this->filter.script_debug_company)->ai_instance->GetLogData();
	}

	/**
	 * Check whether the currently selected AI/GS is dead.
	 * @return true if dead.
	 */
	bool IsDead() const
	{
		if (this->filter.script_debug_company == OWNER_DEITY) {
			GameInstance *game = Game::GetInstance();
			return game == nullptr || game->IsDead();
		}
		return !Company::IsValidAiID(this->filter.script_debug_company) || Company::Get(this->filter.script_debug_company)->ai_instance->IsDead();
	}

	/**
	 * Check whether a company is a valid AI company or GS.
	 * @param company Company to check for validity.
	 * @return true if company is valid for debugging.
	 */
	bool IsValidDebugCompany(CompanyID company) const
	{
		switch (company) {
			case INVALID_COMPANY: return false;
			case OWNER_DEITY:     return Game::GetInstance() != nullptr;
			default:              return Company::IsValidAiID(company);
		}
	}

	/**
	 * Ensure that \c script_debug_company refers to a valid AI company or GS, or is set to #INVALID_COMPANY.
	 * If no valid company is selected, it selects the first valid AI or GS if any.
	 */
	void SelectValidDebugCompany()
	{
		/* Check if the currently selected company is still active. */
		if (this->IsValidDebugCompany(this->filter.script_debug_company)) return;

		this->filter.script_debug_company = INVALID_COMPANY;

		for (const Company *c : Company::Iterate()) {
			if (c->is_ai) {
				ChangeToScript(c->index);
				return;
			}
		}

		/* If no AI is available, see if there is a game script. */
		if (Game::GetInstance() != nullptr) ChangeToScript(OWNER_DEITY);
	}

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param number The window number (actually unused).
	 */
	ScriptDebugWindow(WindowDesc *desc, WindowNumber number, Owner show_company) : Window(desc), break_editbox(MAX_BREAK_STR_STRING_LENGTH)
	{
		this->filter = ScriptDebugWindow::initial_state;
		this->break_string_filter = {&this->filter.case_sensitive_break_check, false};

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SCRD_VSCROLLBAR);
		this->hscroll = this->GetScrollbar(WID_SCRD_HSCROLLBAR);
		this->FinishInitNested(number);

		this->last_vscroll_pos = 0;
		this->autoscroll = true;
		this->highlight_row = -1;

		this->querystrings[WID_SCRD_BREAK_STR_EDIT_BOX] = &this->break_editbox;

		this->hscroll->SetStepSize(10); // Speed up horizontal scrollbar

		/* Restore the break string value from static variable, and enable the filter. */
		this->break_editbox.text.Assign(this->filter.break_string);
		this->break_string_filter.SetFilterTerm(this->filter.break_string);

		if (show_company == INVALID_COMPANY) {
			this->SelectValidDebugCompany();
		} else {
			this->ChangeToScript(show_company);
		}
	}

	void OnInit() override
	{
		this->show_break_box = _settings_client.gui.ai_developer_tools;
		this->GetWidget<NWidgetStacked>(WID_SCRD_BREAK_STRING_WIDGETS)->SetDisplayedPlane(this->show_break_box ? 0 : SZSP_HORIZONTAL);
		if (!this->show_break_box) this->filter.break_check_enabled = false;
		SetWidgetsDisabledState(!this->show_break_box, WID_SCRD_BREAK_STR_ON_OFF_BTN, WID_SCRD_BREAK_STR_EDIT_BOX, WID_SCRD_MATCH_CASE_BTN);

		this->InvalidateData(-1);
	}

	~ScriptDebugWindow()
	{
		ScriptDebugWindow::initial_state = this->filter;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget == WID_SCRD_LOG_PANEL) {
			resize->height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
			size->height = 14 * resize->height + WidgetDimensions::scaled.framerect.Vertical();
		}
	}

	void OnPaint() override
	{
		this->SelectValidDebugCompany();
		this->UpdateLogScroll();

		/* Draw standard stuff */
		this->DrawWidgets();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		if (widget != WID_SCRD_NAME_TEXT) return;

		if (this->filter.script_debug_company == OWNER_DEITY) {
			const GameInfo *info = Game::GetInfo();
			assert(info != nullptr);
			SetDParam(0, STR_AI_DEBUG_NAME_AND_VERSION);
			SetDParamStr(1, info->GetName());
			SetDParam(2, info->GetVersion());
		} else if (this->filter.script_debug_company == INVALID_COMPANY || !Company::IsValidAiID(this->filter.script_debug_company)) {
			SetDParam(0, STR_EMPTY);
		} else {
			const AIInfo *info = Company::Get(this->filter.script_debug_company)->ai_info;
			assert(info != nullptr);
			SetDParam(0, STR_AI_DEBUG_NAME_AND_VERSION);
			SetDParamStr(1, info->GetName());
			SetDParam(2, info->GetVersion());
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SCRD_LOG_PANEL:
				this->DrawWidgetLog(r);
				break;

			default:
				if (IsInsideBS(widget, WID_SCRD_COMPANY_BUTTON_START, MAX_COMPANIES)) {
					this->DrawWidgetCompanyButton(r, widget, WID_SCRD_COMPANY_BUTTON_START);
				}
				break;
		}
	}

	/**
	 * Draw a company button icon.
	 * @param r Rect area to draw within.
	 * @param widget Widget index to start.
	 * @param start Widget index of first company button.
	 */
	void DrawWidgetCompanyButton(const Rect &r, WidgetID widget, int start) const
	{
		if (this->IsWidgetDisabled(widget)) return;
		CompanyID cid = (CompanyID)(widget - start);
		Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
		DrawCompanyIcon(cid, CenterBounds(r.left, r.right, sprite_size.width), CenterBounds(r.top, r.bottom, sprite_size.height));
	}

	/**
	 * Draw the AI/GS log.
	 * @param r Rect area to draw within.
	 */
	void DrawWidgetLog(const Rect &r) const
	{
		if (this->filter.script_debug_company == INVALID_COMPANY) return;

		ScriptLogTypes::LogData &log = this->GetLogData();
		if (log.empty()) return;

		Rect fr = r.Shrink(WidgetDimensions::scaled.framerect);

		/* Setup a clipping rectangle... */
		DrawPixelInfo tmp_dpi;
		if (!FillDrawPixelInfo(&tmp_dpi, fr)) return;
		/* ...but keep coordinates relative to the window. */
		tmp_dpi.left += fr.left;
		tmp_dpi.top += fr.top;

		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

		fr.left -= this->hscroll->GetPosition();

		for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && (size_t)i < log.size(); i++) {
			const ScriptLogTypes::LogLine &line = log[i];

			TextColour colour;
			switch (line.type) {
				case ScriptLogTypes::LOG_SQ_INFO:  colour = TC_BLACK;  break;
				case ScriptLogTypes::LOG_SQ_ERROR: colour = TC_WHITE;  break;
				case ScriptLogTypes::LOG_INFO:     colour = TC_BLACK;  break;
				case ScriptLogTypes::LOG_WARNING:  colour = TC_YELLOW; break;
				case ScriptLogTypes::LOG_ERROR:    colour = TC_RED;    break;
				default:                           colour = TC_BLACK;  break;
			}

			/* Check if the current line should be highlighted */
			if (i == this->highlight_row) {
				fr.bottom = fr.top + this->resize.step_height - 1;
				GfxFillRect(fr, PC_BLACK);
				if (colour == TC_BLACK) colour = TC_WHITE; // Make black text readable by inverting it to white.
			}

			DrawString(fr, line.text, colour, SA_LEFT | SA_FORCE);
			fr.top += this->resize.step_height;
		}
	}

	/**
	 * Update the scrollbar and scroll position of the log panel.
	 */
	void UpdateLogScroll()
	{
		this->SetWidgetsDisabledState(this->filter.script_debug_company == INVALID_COMPANY, WID_SCRD_VSCROLLBAR, WID_SCRD_HSCROLLBAR);
		if (this->filter.script_debug_company == INVALID_COMPANY) return;

		ScriptLogTypes::LogData &log = this->GetLogData();

		int scroll_count = (int)log.size();
		if (this->vscroll->GetCount() != scroll_count) {
			this->vscroll->SetCount(scroll_count);

			/* We need a repaint */
			this->SetWidgetDirty(WID_SCRD_VSCROLLBAR);
		}

		if (log.empty()) return;

		/* Detect when the user scrolls the window. Enable autoscroll when the bottom-most line becomes visible. */
		if (this->last_vscroll_pos != this->vscroll->GetPosition()) {
			this->autoscroll = this->vscroll->GetPosition() + this->vscroll->GetCapacity() >= (int)log.size();
		}

		if (this->autoscroll && this->vscroll->SetPosition((int)log.size())) {
			/* We need a repaint */
			this->SetWidgetDirty(WID_SCRD_VSCROLLBAR);
			this->SetWidgetDirty(WID_SCRD_LOG_PANEL);
		}

		this->last_vscroll_pos = this->vscroll->GetPosition();
	}

	/**
	 * Update state of all Company (AI) buttons.
	 */
	void UpdateAIButtonsState()
	{
		/* Update company buttons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			/* Mark dead/paused AIs by setting the background colour. */
			bool valid = Company::IsValidAiID(i);
			bool dead = valid && Company::Get(i)->ai_instance->IsDead();
			bool paused = valid && Company::Get(i)->ai_instance->IsPaused();

			NWidgetCore *button = this->GetWidget<NWidgetCore>(i + WID_SCRD_COMPANY_BUTTON_START);
			button->SetDisabled(!valid);
			button->SetLowered(this->filter.script_debug_company == i);
			SetScriptButtonColour(*button, dead, paused);
		}
	}

	/**
	 * Update state of game script button.
	 */
	void UpdateGSButtonState()
	{
		GameInstance *game = Game::GetInstance();
		bool valid = game != nullptr;
		bool dead = valid && game->IsDead();
		bool paused = valid && game->IsPaused();

		NWidgetCore *button = this->GetWidget<NWidgetCore>(WID_SCRD_SCRIPT_GAME);
		button->SetDisabled(!valid);
		button->SetLowered(this->filter.script_debug_company == OWNER_DEITY);
		SetScriptButtonColour(*button, dead, paused);
	}

	/**
	 * Change all settings to select another Script.
	 * @param show_ai The new AI to show.
	 * @param new_window Open the script in a new window.
	 */
	void ChangeToScript(CompanyID show_script, bool new_window = false)
	{
		if (!this->IsValidDebugCompany(show_script)) return;

		if (new_window) {
			ScriptDebugWindow::initial_state = this->filter;
			ShowScriptDebugWindow(show_script, true);
			return;
		}

		this->filter.script_debug_company = show_script;

		this->highlight_row = -1; // The highlight of one Script make little sense for another Script.

		/* Close AI settings window to prevent confusion */
		CloseWindowByClass(WC_SCRIPT_SETTINGS);

		this->InvalidateData(-1);

		this->autoscroll = true;
		this->last_vscroll_pos = this->vscroll->GetPosition();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* Also called for hotkeys, so check for disabledness */
		if (this->IsWidgetDisabled(widget)) return;

		/* Check which button is clicked */
		if (IsInsideMM(widget, WID_SCRD_COMPANY_BUTTON_START, WID_SCRD_COMPANY_BUTTON_END + 1)) {
			ChangeToScript((CompanyID)(widget - WID_SCRD_COMPANY_BUTTON_START), _ctrl_pressed);
		}

		switch (widget) {
			case WID_SCRD_SCRIPT_GAME:
				ChangeToScript(OWNER_DEITY, _ctrl_pressed);
				break;

			case WID_SCRD_RELOAD_TOGGLE:
				if (this->filter.script_debug_company == OWNER_DEITY) break;
				/* First kill the company of the AI, then start a new one. This should start the current AI again */
				Command<CMD_COMPANY_CTRL>::Post(CCA_DELETE, this->filter.script_debug_company, CRR_MANUAL, INVALID_CLIENT_ID);
				Command<CMD_COMPANY_CTRL>::Post(CCA_NEW_AI, this->filter.script_debug_company, CRR_NONE, INVALID_CLIENT_ID);
				break;

			case WID_SCRD_SETTINGS:
				ShowScriptSettingsWindow(this->filter.script_debug_company);
				break;

			case WID_SCRD_BREAK_STR_ON_OFF_BTN:
				this->filter.break_check_enabled = !this->filter.break_check_enabled;
				this->InvalidateData(-1);
				break;

			case WID_SCRD_MATCH_CASE_BTN:
				this->filter.case_sensitive_break_check = !this->filter.case_sensitive_break_check;
				this->InvalidateData(-1);
				break;

			case WID_SCRD_CONTINUE_BTN:
				/* Unpause current AI / game script and mark the corresponding script button dirty. */
				if (!this->IsDead()) {
					if (this->filter.script_debug_company == OWNER_DEITY) {
						Game::Unpause();
					} else {
						AI::Unpause(this->filter.script_debug_company);
					}
				}

				/* If the last AI/Game Script is unpaused, unpause the game too. */
				if ((_pause_mode & PM_PAUSED_NORMAL) == PM_PAUSED_NORMAL) {
					bool all_unpaused = !Game::IsPaused();
					if (all_unpaused) {
						for (const Company *c : Company::Iterate()) {
							if (c->is_ai && AI::IsPaused(c->index)) {
								all_unpaused = false;
								break;
							}
						}
						if (all_unpaused) {
							/* All scripts have been unpaused => unpause the game. */
							Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, false);
						}
					}
				}

				this->highlight_row = -1;
				this->InvalidateData(-1);
				break;
		}
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid != WID_SCRD_BREAK_STR_EDIT_BOX) return;

		/* Save the current string to static member so it can be restored next time the window is opened. */
		this->filter.break_string = this->break_editbox.text.buf;
		this->break_string_filter.SetFilterTerm(this->filter.break_string);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 *             This is the company ID of the AI/GS which wrote a new log message, or -1 in other cases.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (this->show_break_box != _settings_client.gui.ai_developer_tools) this->ReInit();

		/* If the log message is related to the active company tab, check the break string.
		 * This needs to be done in gameloop-scope, so the AI is suspended immediately. */
		if (!gui_scope && data == this->filter.script_debug_company &&
				this->IsValidDebugCompany(this->filter.script_debug_company) &&
				this->filter.break_check_enabled && !this->break_string_filter.IsEmpty()) {
			/* Get the log instance of the active company */
			ScriptLogTypes::LogData &log = this->GetLogData();

			if (!log.empty()) {
				this->break_string_filter.ResetState();
				this->break_string_filter.AddLine(log.back().text);
				if (this->break_string_filter.GetState()) {
					/* Pause execution of script. */
					if (!this->IsDead()) {
						if (this->filter.script_debug_company == OWNER_DEITY) {
							Game::Pause();
						} else {
							AI::Pause(this->filter.script_debug_company);
						}
					}

					/* Pause the game. */
					if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED) {
						Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, true);
					}

					/* Highlight row that matched */
					this->highlight_row = (int)(log.size() - 1);
				}
			}
		}

		if (!gui_scope) return;

		this->SelectValidDebugCompany();

		uint max_width = 0;
		if (this->filter.script_debug_company != INVALID_COMPANY) {
			for (auto &line : this->GetLogData()) {
				if (line.width == 0 || data == -1) line.width = GetStringBoundingBox(line.text).width;
				max_width = std::max(max_width, line.width);
			}
		}

		this->vscroll->SetCount(this->filter.script_debug_company != INVALID_COMPANY ? this->GetLogData().size() : 0);
		this->hscroll->SetCount(max_width + WidgetDimensions::scaled.frametext.Horizontal());

		this->UpdateAIButtonsState();
		this->UpdateGSButtonState();

		this->SetWidgetLoweredState(WID_SCRD_BREAK_STR_ON_OFF_BTN, this->filter.break_check_enabled);
		this->SetWidgetLoweredState(WID_SCRD_MATCH_CASE_BTN, this->filter.case_sensitive_break_check);

		this->SetWidgetDisabledState(WID_SCRD_SETTINGS, this->filter.script_debug_company == INVALID_COMPANY);
		extern CompanyID _local_company;
		this->SetWidgetDisabledState(WID_SCRD_RELOAD_TOGGLE,
				this->filter.script_debug_company == INVALID_COMPANY ||
				this->filter.script_debug_company == OWNER_DEITY ||
				this->filter.script_debug_company == _local_company);
		this->SetWidgetDisabledState(WID_SCRD_CONTINUE_BTN, this->filter.script_debug_company == INVALID_COMPANY ||
			(this->filter.script_debug_company == OWNER_DEITY ? !Game::IsPaused() : !AI::IsPaused(this->filter.script_debug_company)));
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SCRD_LOG_PANEL, WidgetDimensions::scaled.framerect.Vertical());
		this->hscroll->SetCapacityFromWidget(this, WID_SCRD_LOG_PANEL);
	}

	/**
	 * Handler for global hotkeys of the ScriptDebugWindow.
	 * @param hotkey Hotkey
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState ScriptDebugGlobalHotkeys(int hotkey)
	{
		if (_game_mode != GM_NORMAL) return ES_NOT_HANDLED;
		Window *w = ShowScriptDebugWindow(INVALID_COMPANY);
		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"aidebug", {
		Hotkey('1', "company_1", WID_SCRD_COMPANY_BUTTON_START),
		Hotkey('2', "company_2", WID_SCRD_COMPANY_BUTTON_START + 1),
		Hotkey('3', "company_3", WID_SCRD_COMPANY_BUTTON_START + 2),
		Hotkey('4', "company_4", WID_SCRD_COMPANY_BUTTON_START + 3),
		Hotkey('5', "company_5", WID_SCRD_COMPANY_BUTTON_START + 4),
		Hotkey('6', "company_6", WID_SCRD_COMPANY_BUTTON_START + 5),
		Hotkey('7', "company_7", WID_SCRD_COMPANY_BUTTON_START + 6),
		Hotkey('8', "company_8", WID_SCRD_COMPANY_BUTTON_START + 7),
		Hotkey('9', "company_9", WID_SCRD_COMPANY_BUTTON_START + 8),
		Hotkey(0, "company_10", WID_SCRD_COMPANY_BUTTON_START + 9),
		Hotkey(0, "company_11", WID_SCRD_COMPANY_BUTTON_START + 10),
		Hotkey(0, "company_12", WID_SCRD_COMPANY_BUTTON_START + 11),
		Hotkey(0, "company_13", WID_SCRD_COMPANY_BUTTON_START + 12),
		Hotkey(0, "company_14", WID_SCRD_COMPANY_BUTTON_START + 13),
		Hotkey(0, "company_15", WID_SCRD_COMPANY_BUTTON_START + 14),
		Hotkey('S', "settings", WID_SCRD_SETTINGS),
		Hotkey('0', "game_script", WID_SCRD_SCRIPT_GAME),
		Hotkey(0, "reload", WID_SCRD_RELOAD_TOGGLE),
		Hotkey('B', "break_toggle", WID_SCRD_BREAK_STR_ON_OFF_BTN),
		Hotkey('F', "break_string", WID_SCRD_BREAK_STR_EDIT_BOX),
		Hotkey('C', "match_case", WID_SCRD_MATCH_CASE_BTN),
		Hotkey(WKC_RETURN, "continue", WID_SCRD_CONTINUE_BTN),
	}, ScriptDebugGlobalHotkeys};
};

/** Make a number of rows with buttons for each company for the Script debug window. */
std::unique_ptr<NWidgetBase> MakeCompanyButtonRowsScriptDebug()
{
	return MakeCompanyButtonRows(WID_SCRD_COMPANY_BUTTON_START, WID_SCRD_COMPANY_BUTTON_END, COLOUR_GREY, 8, STR_AI_DEBUG_SELECT_AI_TOOLTIP);
}

/** Widgets for the Script debug window. */
static const NWidgetPart _nested_script_debug_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_AI_DEBUG, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, WID_SCRD_VIEW),
		NWidgetFunction(MakeCompanyButtonRowsScriptDebug), SetPadding(0, 2, 1, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SCRD_SCRIPT_GAME), SetMinimalSize(100, 20), SetResize(1, 0), SetDataTip(STR_AI_GAME_SCRIPT, STR_AI_GAME_SCRIPT_TOOLTIP),
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SCRD_NAME_TEXT), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_STRING2, STR_AI_DEBUG_NAME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCRD_SETTINGS), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_SETTINGS, STR_AI_DEBUG_SETTINGS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCRD_RELOAD_TOGGLE), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_RELOAD, STR_AI_DEBUG_RELOAD_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
		/* Log panel */
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SCRD_LOG_PANEL), SetMinimalSize(287, 180), SetResize(1, 1), SetScrollbar(WID_SCRD_VSCROLLBAR),
		EndContainer(),
		/* Break string widgets */
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SCRD_BREAK_STRING_WIDGETS),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_IMGBTN_2, COLOUR_GREY, WID_SCRD_BREAK_STR_ON_OFF_BTN), SetFill(0, 1), SetDataTip(SPR_FLAG_VEH_STOPPED, STR_AI_DEBUG_BREAK_STR_ON_OFF_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_LABEL, COLOUR_GREY), SetPadding(2, 2, 2, 4), SetDataTip(STR_AI_DEBUG_BREAK_ON_LABEL, 0x0),
						NWidget(WWT_EDITBOX, COLOUR_GREY, WID_SCRD_BREAK_STR_EDIT_BOX), SetFill(1, 1), SetResize(1, 0), SetPadding(2, 2, 2, 2), SetDataTip(STR_AI_DEBUG_BREAK_STR_OSKTITLE, STR_AI_DEBUG_BREAK_STR_TOOLTIP),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SCRD_MATCH_CASE_BTN), SetMinimalSize(100, 0), SetFill(0, 1), SetDataTip(STR_AI_DEBUG_MATCH_CASE, STR_AI_DEBUG_MATCH_CASE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCRD_CONTINUE_BTN), SetMinimalSize(100, 0), SetFill(0, 1), SetDataTip(STR_AI_DEBUG_CONTINUE, STR_AI_DEBUG_CONTINUE_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HSCROLLBAR, COLOUR_GREY, WID_SCRD_HSCROLLBAR),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SCRD_VSCROLLBAR),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
EndContainer(),
};

/** Window definition for the Script debug window. */
static WindowDesc _script_debug_desc(__FILE__, __LINE__,
	WDP_AUTO, "script_debug", 600, 450,
	WC_SCRIPT_DEBUG, WC_NONE,
	0,
	std::begin(_nested_script_debug_widgets), std::end(_nested_script_debug_widgets),
	&ScriptDebugWindow::hotkeys
);

/**
 * Open the Script debug window and select the given company.
 * @param show_company Display debug information about this AI company.
 * @param new_window Show in new window instead of existing window.
 */
Window *ShowScriptDebugWindow(CompanyID show_company, bool new_window)
{
	if (!_networking || _network_server) {
		int i = 0;
		if (new_window) {
			/* find next free window number for script debug */
			while (FindWindowById(WC_SCRIPT_DEBUG, i) != nullptr) i++;
		} else {
			/* Find existing window showing show_company. */
			for (Window *w : Window::Iterate()) {
				if (w->window_class == WC_SCRIPT_DEBUG && static_cast<ScriptDebugWindow *>(w)->filter.script_debug_company == show_company) {
					return BringWindowToFrontById(w->window_class, w->window_number);
				}
			}

			/* Maybe there's a window showing a different company which can be switched. */
			ScriptDebugWindow *w = static_cast<ScriptDebugWindow *>(FindWindowByClass(WC_SCRIPT_DEBUG));
			if (w != nullptr) {
				BringWindowToFrontById(w->window_class, w->window_number);
				w->ChangeToScript(show_company);
				return w;
			}
		}
		return new ScriptDebugWindow(&_script_debug_desc, i, show_company);
	} else {
		ShowErrorMessage(STR_ERROR_AI_DEBUG_SERVER_ONLY, INVALID_STRING_ID, WL_INFO);
	}

	return nullptr;
}

/**
 * Reset the Script windows to their initial state.
 */
void InitializeScriptGui()
{
	ScriptDebugWindow::initial_state.script_debug_company = INVALID_COMPANY;
}

/** Open the AI debug window if one of the AI scripts has crashed. */
void ShowScriptDebugWindowIfScriptError()
{
	/* Network clients can't debug AIs. */
	if (_networking && !_network_server) return;

	for (const Company *c : Company::Iterate()) {
		if (c->is_ai && c->ai_instance->IsDead()) {
			ShowScriptDebugWindow(c->index);
			break;
		}
	}

	GameInstance *g = Game::GetGameInstance();
	if (g != nullptr && g->IsDead()) {
		ShowScriptDebugWindow(OWNER_DEITY);
	}
}
