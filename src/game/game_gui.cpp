/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_gui.cpp %Window for configuring the Game Script */

#include "../stdafx.h"
#include "../error.h"
#include "../settings_gui.h"
#include "../querystring_gui.h"
#include "../window_func.h"
#include "../network/network.h"
#include "../network/network_content.h"
#include "../widgets/dropdown_func.h"
#include "../timer/timer.h"
#include "../timer/timer_window.h"

#include "game.hpp"
#include "game_gui.hpp"
#include "game_config.hpp"
#include "game_info.hpp"
#include "../script/script_gui.h"
#include "../script_config.hpp"
#include "../table/strings.h"

#include "../safeguards.h"


/** Widgets for the configure GS window. */
static const NWidgetPart _nested_gs_config_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_CAPTION_GAMESCRIPT, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_GSC_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse_resize),
			NWidget(WWT_FRAME, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_GAMESCRIPT, STR_NULL), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_GSC_GSLIST), SetMinimalSize(288, 14), SetFill(1, 1), SetResize(1, 0), SetMatrixDataTip(1, 1, STR_AI_CONFIG_GAMELIST_TOOLTIP),
			EndContainer(),
			NWidget(WWT_FRAME, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_GAMESCRIPT_PARAM, STR_NULL), SetFill(1, 1), SetResize(1, 0), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_GSC_SETTINGS), SetFill(1, 0), SetResize(1, 1), SetMinimalSize(188, 182), SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_GSC_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_GSC_SCROLLBAR),
				EndContainer(),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_RESET), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_AI_SETTINGS_RESET, STR_NULL),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_CHANGE), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_AI_CONFIG_CHANGE_GAMESCRIPT, STR_AI_CONFIG_CHANGE_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_CONTENT_DOWNLOAD), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_TEXTFILE + TFT_README), SetFill(1, 1), SetResize(1, 0), SetMinimalSize(93, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_TEXTFILE + TFT_CHANGELOG), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_GSC_TEXTFILE + TFT_LICENSE), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_MAUVE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the configure GS window. */
static WindowDesc _gs_config_desc(__FILE__, __LINE__,
	WDP_CENTER, "settings_gs_config", 500, 350,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	std::begin(_nested_gs_config_widgets), std::end(_nested_gs_config_widgets)
);

/**
 * Window to configure which GSs will start.
 */
struct GSConfigWindow : public Window {
	ScriptConfig *gs_config; ///< The configuration we're modifying.
	int line_height;         ///< Height of a single GS-name line.
	int clicked_button;      ///< The button we clicked.
	bool clicked_increase;   ///< Whether we clicked the increase or decrease button.
	bool clicked_dropdown;   ///< Whether the dropdown is open.
	bool closing_dropdown;   ///< True, if the dropdown list is currently closing.
	int clicked_row;         ///< The clicked row of settings.
	Scrollbar *vscroll;      ///< Cache of the vertical scrollbar.
	typedef std::vector<const ScriptConfigItem *> VisibleSettingsList; ///< typdef for a vector of script settings
	VisibleSettingsList visible_settings; ///< List of visible GS settings

	GSConfigWindow() : Window(&_gs_config_desc),
		clicked_button(-1),
		clicked_dropdown(false),
		closing_dropdown(false)
	{
		this->gs_config = GameConfig::GetConfig();

		this->CreateNestedTree(); // Initializes 'this->line_height' as a side effect.
		this->vscroll = this->GetScrollbar(WID_GSC_SCROLLBAR);
		this->FinishInitNested(WN_GAME_OPTIONS_GS);
		this->OnInvalidateData(0);

		this->RebuildVisibleSettings();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowByClass(WC_SCRIPT_LIST);
		this->Window::Close();
	}

	/**
	 * Rebuilds the list of visible settings. GS settings with the flag
	 * GSCONFIG_GS_DEVELOPER set will only be visible if the game setting
	 * gui.ai_developer_tools is enabled.
	 */
	void RebuildVisibleSettings()
	{
		visible_settings.clear();

		for (const auto &item : *this->gs_config->GetConfigList()) {
			bool no_hide = (item.flags & SCRIPTCONFIG_DEVELOPER) == 0;
			if (no_hide || _settings_client.gui.ai_developer_tools) {
				visible_settings.push_back(&item);
			}
		}

		this->vscroll->SetCount(this->visible_settings.size());
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_GSC_SETTINGS:
				this->line_height = std::max(SETTING_BUTTON_HEIGHT, GetCharacterHeight(FS_NORMAL)) + padding.height;
				resize->width = 1;
				resize->height = this->line_height;
				size->height = 5 * this->line_height;
				break;

			case WID_GSC_GSLIST:
				this->line_height = GetCharacterHeight(FS_NORMAL) + padding.height;
				size->height = 1 * this->line_height;
				break;
		}
	}

	/**
	 * Can the GS config be edited?
	 * @return True if the given GS Config slot can be edited, otherwise false.
	 */
	static bool IsEditable()
	{
		return _game_mode != GM_NORMAL || Game::GetInstance() != nullptr;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_GSC_GSLIST: {
				StringID text = STR_AI_CONFIG_NONE;

				if (GameConfig::GetConfig()->GetInfo() != nullptr) {
					SetDParamStr(0, GameConfig::GetConfig()->GetInfo()->GetName());
					text = STR_JUST_RAW_STRING;
				}

				/* There is only one slot, unlike with the GS GUI, so it should never be white */
				DrawString(r.Shrink(WidgetDimensions::scaled.matrix), text, (IsEditable() ? TC_ORANGE : TC_SILVER));
				break;
			}
			case WID_GSC_SETTINGS: {
				ScriptConfig *config = this->gs_config;
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
				break;
			}
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
		if (widget >= WID_GSC_TEXTFILE && widget < WID_GSC_TEXTFILE + TFT_CONTENT_END) {
			if (GameConfig::GetConfig() == nullptr) return;

			ShowScriptTextfileWindow((TextfileType)(widget - WID_GSC_TEXTFILE), (CompanyID)OWNER_DEITY);
			return;
		}

		switch (widget) {
			case WID_GSC_GSLIST: {
				this->InvalidateData();
				if (click_count > 1 && _game_mode != GM_NORMAL) ShowScriptListWindow((CompanyID)OWNER_DEITY, _ctrl_pressed);
				break;
			}

			case WID_GSC_CHANGE:  // choose other Game Script
				ShowScriptListWindow((CompanyID)OWNER_DEITY, _ctrl_pressed);
				break;

			case WID_GSC_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
					ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_GAME);
				}
				break;

			case WID_GSC_SETTINGS: {
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
				int old_val = this->gs_config->GetSetting(config_item.name);
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

							ShowDropDownListAt(this, std::move(list), old_val, WID_GSC_SETTING_DROPDOWN, wi_rect, COLOUR_ORANGE);
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
						this->gs_config->SetSetting(config_item.name, new_val);
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

			case WID_GSC_OPEN_URL: {
				const GameConfig *config = GameConfig::GetConfig();
				if (config == nullptr || config->GetInfo() == nullptr) return;
				OpenBrowser(config->GetInfo()->GetURL());
				break;
			}

			case WID_GSC_RESET:
				this->gs_config->ResetEditableSettings(_game_mode == GM_MENU);
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
		if (widget != WID_GSC_SETTING_DROPDOWN) return;
		assert(this->clicked_dropdown);
		SetValue(index);
	}

	void OnDropdownClose(Point, WidgetID widget, int, bool) override
	{
		if (widget != WID_GSC_SETTING_DROPDOWN) return;
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
		this->vscroll->SetCapacityFromWidget(this, WID_GSC_SETTINGS);
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
		if (!gui_scope) return;

		this->SetWidgetDisabledState(WID_GSC_CHANGE, (_game_mode == GM_NORMAL) || !IsEditable());

		const GameConfig *config = GameConfig::GetConfig();
		this->SetWidgetDisabledState(WID_GSC_OPEN_URL, config->GetInfo() == nullptr || config->GetInfo()->GetURL().empty());
		for (TextfileType tft = TFT_CONTENT_BEGIN; tft < TFT_CONTENT_END; tft++) {
			this->SetWidgetDisabledState(WID_GSC_TEXTFILE + tft, !config->GetTextfile(tft, (CompanyID)OWNER_DEITY).has_value());
		}
		this->RebuildVisibleSettings();
		this->CloseChildWindows(WC_DROPDOWN_MENU);
		this->CloseChildWindows(WC_QUERY_STRING);
	}
private:
	bool IsEditableItem(const ScriptConfigItem &config_item) const
	{
		return _game_mode == GM_MENU
		    || _game_mode == GM_EDITOR
		    || (config_item.flags & SCRIPTCONFIG_INGAME) != 0
		    || _settings_client.gui.ai_developer_tools;
	}

	void SetValue(int value)
	{
		const ScriptConfigItem &config_item = *this->visible_settings[this->clicked_row];
		if (_game_mode == GM_NORMAL && (config_item.flags & SCRIPTCONFIG_INGAME) == 0) return;
		this->gs_config->SetSetting(config_item.name, value);
		this->SetDirty();
	}
};

/** Open the GS config window. */
void ShowGSConfigWindow()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new GSConfigWindow();
}
