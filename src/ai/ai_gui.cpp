/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_gui.cpp %Window for configuring the AIs */

#include "../stdafx.h"
#include "../error.h"
#include "../settings_gui.h"
#include "../querystring_gui.h"
#include "../company_base.h"
#include "../window_func.h"
#include "../network/network.h"
#include "../settings_func.h"
#include "../network/network_content.h"
#include "../widgets/dropdown_func.h"
#include "../core/geometry_func.hpp"

#include "ai.hpp"
#include "ai_gui.hpp"
#include "ai_config.hpp"
#include "ai_info.hpp"
#include "../script/script_gui.h"
#include "table/strings.h"

#include "../safeguards.h"


/** Widgets for the configure AI window. */
static const NWidgetPart _nested_ai_config_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_CAPTION_AI, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_AIC_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4),
			NWidget(NWID_HORIZONTAL), SetPIP(7, 0, 7),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_DECREASE), SetDataTip(AWV_DECREASE, STR_NULL),
				NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_INCREASE), SetDataTip(AWV_INCREASE, STR_NULL),
				NWidget(NWID_SPACER), SetMinimalSize(6, 0),
				NWidget(WWT_TEXT, COLOUR_MAUVE, WID_AIC_NUMBER), SetDataTip(STR_AI_CONFIG_MAX_COMPETITORS, STR_NULL), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(7, 0, 7),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_UP), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_UP, STR_AI_CONFIG_MOVE_UP_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_DOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_DOWN, STR_AI_CONFIG_MOVE_DOWN_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_FRAME, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_AI, STR_NULL), SetPadding(0, 5, 0, 5),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIC_LIST), SetMinimalSize(288, 112), SetFill(1, 0), SetMatrixDataTip(1, 8, STR_AI_CONFIG_AILIST_TOOLTIP), SetScrollbar(WID_AIC_SCROLLBAR),
				NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_AIC_SCROLLBAR),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(7, 0, 7),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CHANGE), SetFill(1, 0), SetMinimalSize(93, 0), SetDataTip(STR_AI_CONFIG_CHANGE_AI, STR_AI_CONFIG_CHANGE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONFIGURE), SetFill(1, 0), SetMinimalSize(93, 0), SetDataTip(STR_AI_CONFIG_CONFIGURE, STR_AI_CONFIG_CONFIGURE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_NULL),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(7, 0, 7),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CLOSE), SetFill(1, 0), SetMinimalSize(93, 0), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_NULL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_NULL),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONTENT_DOWNLOAD), SetFill(1, 0), SetMinimalSize(279, 0), SetPadding(0, 7, 9, 7), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
	EndContainer(),
};

/** Window definition for the configure AI window. */
static WindowDesc _ai_config_desc(
	WDP_CENTER, "settings_ai_config", 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	_nested_ai_config_widgets, lengthof(_nested_ai_config_widgets)
);

/**
 * Window to configure which AIs will start.
 */
struct AIConfigWindow : public Window {
	CompanyID selected_slot; ///< The currently selected AI slot or \c INVALID_COMPANY.
	int line_height;         ///< Height of a single AI-name line.
	Scrollbar *vscroll;      ///< Cache of the vertical scrollbar.

	AIConfigWindow() : Window(&_ai_config_desc)
	{
		this->InitNested(WN_GAME_OPTIONS_AI); // Initializes 'this->line_height' as a side effect.
		this->vscroll = this->GetScrollbar(WID_AIC_SCROLLBAR);
		this->selected_slot = INVALID_COMPANY;
		NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_AIC_LIST);
		this->vscroll->SetCapacity(nwi->current_y / this->line_height);
		this->vscroll->SetCount(MAX_COMPANIES);
		this->OnInvalidateData(0);
	}

	void Close() override
	{
		CloseWindowByClass(WC_SCRIPT_LIST);
		CloseWindowByClass(WC_AI_SETTINGS);
		this->Window::Close();
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_AIC_NUMBER:
				SetDParam(0, GetGameSettings().difficulty.max_no_competitors);
				break;
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_AIC_DECREASE:
			case WID_AIC_INCREASE:
				*size = maxdim(*size, NWidgetScrollbar::GetHorizontalDimension());
				break;

			case WID_AIC_LIST:
				this->line_height = FONT_HEIGHT_NORMAL + padding.height;
				resize->height = this->line_height;
				size->height = 8 * this->line_height;
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
		if (Company::IsValidID(slot)) return false;

		int max_slot = GetGameSettings().difficulty.max_no_competitors;
		for (CompanyID cid = COMPANY_FIRST; cid < (CompanyID)max_slot && cid < MAX_COMPANIES; cid++) {
			if (Company::IsValidHumanID(cid)) max_slot++;
		}
		return slot < max_slot;
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (widget) {
			case WID_AIC_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.matrix);
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < MAX_COMPANIES; i++) {
					StringID text;

					if ((_game_mode != GM_NORMAL && i == 0) || (_game_mode == GM_NORMAL && Company::IsValidHumanID(i))) {
						text = STR_AI_CONFIG_HUMAN_PLAYER;
					} else if (AIConfig::GetConfig((CompanyID)i)->GetInfo() != nullptr) {
						SetDParamStr(0, AIConfig::GetConfig((CompanyID)i)->GetInfo()->GetName());
						text = STR_JUST_RAW_STRING;
					} else {
						text = STR_AI_CONFIG_RANDOM_AI;
					}
					DrawString(tr, text,
							(this->selected_slot == i) ? TC_WHITE : (IsEditable((CompanyID)i) ? TC_ORANGE : TC_SILVER));
					tr.top += this->line_height;
				}
				break;
			}
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		if (widget >= WID_AIC_TEXTFILE && widget < WID_AIC_TEXTFILE + TFT_END) {
			if (this->selected_slot == INVALID_COMPANY || AIConfig::GetConfig(this->selected_slot) == nullptr) return;

			ShowScriptTextfileWindow((TextfileType)(widget - WID_AIC_TEXTFILE), this->selected_slot);
			return;
		}

		switch (widget) {
			case WID_AIC_DECREASE:
			case WID_AIC_INCREASE: {
				int new_value;
				if (widget == WID_AIC_DECREASE) {
					new_value = std::max(0, GetGameSettings().difficulty.max_no_competitors - 1);
				} else {
					new_value = std::min(MAX_COMPANIES - 1, GetGameSettings().difficulty.max_no_competitors + 1);
				}
				IConsoleSetSetting("difficulty.max_no_competitors", new_value);
				break;
			}

			case WID_AIC_LIST: { // Select a slot
				this->selected_slot = (CompanyID)this->vscroll->GetScrolledRowFromWidget(pt.y, this, widget);
				this->InvalidateData();
				if (click_count > 1 && this->selected_slot != INVALID_COMPANY) ShowScriptListWindow((CompanyID)this->selected_slot);
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
				ShowScriptListWindow((CompanyID)this->selected_slot);
				break;

			case WID_AIC_CONFIGURE: // change the settings for an AI
				ShowAISettingsWindow((CompanyID)this->selected_slot);
				break;

			case WID_AIC_CLOSE:
				this->Close();
				break;

			case WID_AIC_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(STR_NETWORK_ERROR_NOTAVAILABLE, INVALID_STRING_ID, WL_ERROR);
				} else {
					ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_AI);
				}
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!IsEditable(this->selected_slot)) {
			this->selected_slot = INVALID_COMPANY;
		}

		if (!gui_scope) return;

		this->SetWidgetDisabledState(WID_AIC_DECREASE, GetGameSettings().difficulty.max_no_competitors == 0);
		this->SetWidgetDisabledState(WID_AIC_INCREASE, GetGameSettings().difficulty.max_no_competitors == MAX_COMPANIES - 1);
		this->SetWidgetDisabledState(WID_AIC_CHANGE, this->selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(WID_AIC_CONFIGURE, this->selected_slot == INVALID_COMPANY || AIConfig::GetConfig(this->selected_slot)->GetConfigList()->size() == 0);
		this->SetWidgetDisabledState(WID_AIC_MOVE_UP, this->selected_slot == INVALID_COMPANY || !IsEditable((CompanyID)(this->selected_slot - 1)));
		this->SetWidgetDisabledState(WID_AIC_MOVE_DOWN, this->selected_slot == INVALID_COMPANY || !IsEditable((CompanyID)(this->selected_slot + 1)));

		for (TextfileType tft = TFT_BEGIN; tft < TFT_END; tft++) {
			this->SetWidgetDisabledState(WID_AIC_TEXTFILE + tft, this->selected_slot == INVALID_COMPANY || (AIConfig::GetConfig(this->selected_slot)->GetTextfile(tft, this->selected_slot) == nullptr));
		}
	}
};

/** Open the AI config window. */
void ShowAIConfigWindow()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new AIConfigWindow();
}


/**
 * Window for settings the parameters of an AI.
 */
struct AISettingsWindow : public Window {
	CompanyID slot;                       ///< The currently show company's setting.
	ScriptConfig *ai_config;              ///< The configuration we're modifying.
	int clicked_button;                   ///< The button we clicked.
	bool clicked_increase;                ///< Whether we clicked the increase or decrease button.
	bool clicked_dropdown;                ///< Whether the dropdown is open.
	bool closing_dropdown;                ///< True, if the dropdown list is currently closing.
	GUITimer timeout;                     ///< Timeout for unclicking the button.
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
	AISettingsWindow(WindowDesc *desc, CompanyID slot) : Window(desc),
		slot(slot),
		clicked_button(-1),
		clicked_dropdown(false),
		closing_dropdown(false),
		timeout(0)
	{
		assert(slot != OWNER_DEITY);
		this->ai_config = AIConfig::GetConfig(slot);

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_AIS_SCROLLBAR);
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

		for (const auto &item : *this->ai_config->GetConfigList()) {
			bool no_hide = (item.flags & SCRIPTCONFIG_DEVELOPER) == 0;
			if (no_hide || _settings_client.gui.ai_developer_tools) {
				visible_settings.push_back(&item);
			}
		}

		this->vscroll->SetCount((int)this->visible_settings.size());
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		if (widget != WID_AIS_BACKGROUND) return;

		this->line_height = std::max(SETTING_BUTTON_HEIGHT, FONT_HEIGHT_NORMAL) + padding.height;

		resize->width = 1;
		resize->height = this->line_height;
		size->height = 5 * this->line_height;
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		if (widget != WID_AIS_BACKGROUND) return;

		ScriptConfig *config = this->ai_config;
		VisibleSettingsList::const_iterator it = this->visible_settings.begin();
		int i = 0;
		for (; !this->vscroll->IsVisible(i); i++) it++;

		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		bool rtl = _current_text_dir == TD_RTL;
		Rect br = ir.WithWidth(SETTING_BUTTON_WIDTH, rtl);
		Rect tr = ir.Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide, rtl);

		int y = r.top;
		int button_y_offset = (this->line_height - SETTING_BUTTON_HEIGHT) / 2;
		int text_y_offset = (this->line_height - FONT_HEIGHT_NORMAL) / 2;
		for (; this->vscroll->IsVisible(i) && it != visible_settings.end(); i++, it++) {
			const ScriptConfigItem &config_item = **it;
			int current_value = config->GetSetting((config_item).name);
			bool editable = this->IsEditableItem(config_item);

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
				DrawBoolButton(br.left, y + button_y_offset, current_value != 0, editable);
				SetDParam(idx++, current_value == 0 ? STR_CONFIG_SETTING_OFF : STR_CONFIG_SETTING_ON);
			} else {
				if (config_item.complete_labels) {
					DrawDropDownButton(br.left, y + button_y_offset, COLOUR_YELLOW, this->clicked_row == i && clicked_dropdown, editable);
				} else {
					DrawArrowButtons(br.left, y + button_y_offset, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + (this->clicked_increase != rtl) : 0, editable && current_value > config_item.min_value, editable && current_value < config_item.max_value);
				}
				if (config_item.labels != nullptr && config_item.labels->Contains(current_value)) {
					SetDParam(idx++, STR_JUST_RAW_STRING);
					SetDParamStr(idx++, config_item.labels->Find(current_value)->second);
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

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_AIS_BACKGROUND: {
				Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.matrix, RectPadding::zero);
				int num = (pt.y - r.top) / this->line_height + this->vscroll->GetPosition();
				if (num >= (int)this->visible_settings.size()) break;

				VisibleSettingsList::const_iterator it = this->visible_settings.begin();
				for (int i = 0; i < num; i++) it++;
				const ScriptConfigItem config_item = **it;
				if (!this->IsEditableItem(config_item)) return;

				if (this->clicked_row != num) {
					this->CloseChildWindows(WC_QUERY_STRING);
					HideDropDownMenu(this);
					this->clicked_row = num;
					this->clicked_dropdown = false;
				}

				bool bool_item = (config_item.flags & SCRIPTCONFIG_BOOLEAN) != 0;

				int x = pt.x - r.left;
				if (_current_text_dir == TD_RTL) x = r.Width() - 1 - x;

				/* One of the arrows is clicked (or green/red rect in case of bool value) */
				int old_val = this->ai_config->GetSetting(config_item.name);
				if (!bool_item && IsInsideMM(x, 0, SETTING_BUTTON_WIDTH) && config_item.complete_labels) {
					if (this->clicked_dropdown) {
						/* unclick the dropdown */
						HideDropDownMenu(this);
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
								list.emplace_back(new DropDownListCharStringItem(config_item.labels->Find(i)->second, i, false));
							}

							ShowDropDownListAt(this, std::move(list), old_val, -1, wi_rect, COLOUR_ORANGE, true);
						}
					}
				} else if (IsInsideMM(x, 0, SETTING_BUTTON_WIDTH)) {
					int new_val = old_val;
					if (bool_item) {
						new_val = !new_val;
					} else {
						/* Increase/Decrease button clicked */
						this->clicked_increase = (x >= SETTING_BUTTON_WIDTH / 2);
						new_val = this->clicked_increase ?
							std::min(config_item.max_value, new_val + config_item.step_size) :
							std::max(config_item.min_value, new_val - config_item.step_size);
					}

					if (new_val != old_val) {
						this->ai_config->SetSetting(config_item.name, new_val);
						this->clicked_button = num;
						this->timeout.SetInterval(150);
					}
				} else if (!bool_item && !config_item.complete_labels) {
					/* Display a query box so users can enter a custom value. */
					SetDParam(0, old_val);
					ShowQueryString(STR_JUST_INT, STR_CONFIG_SETTING_QUERY_CAPTION, 10, this, CS_NUMERAL, QSF_NONE);
				}
				this->SetDirty();
				break;
			}

			case WID_AIS_ACCEPT:
				this->Close();
				break;

			case WID_AIS_RESET:
				this->ai_config->ResetEditableSettings(_game_mode == GM_MENU || !Company::IsValidID(this->slot));
				this->SetDirty();
				break;
		}
	}

	void OnQueryTextFinished(char *str) override
	{
		if (StrEmpty(str)) return;
		int32 value = atoi(str);

		SetValue(value);
	}

	void OnDropdownSelect(int widget, int index) override
	{
		assert(this->clicked_dropdown);
		SetValue(index);
	}

	void OnDropdownClose(Point pt, int widget, int index, bool instant_close) override
	{
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
		this->vscroll->SetCapacityFromWidget(this, WID_AIS_BACKGROUND);
	}

	void OnRealtimeTick(uint delta_ms) override
	{
		if (this->timeout.Elapsed(delta_ms)) {
			this->clicked_button = -1;
			this->SetDirty();
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		this->RebuildVisibleSettings();
		HideDropDownMenu(this);
		this->CloseChildWindows(WC_QUERY_STRING);
	}

private:
	bool IsEditableItem(const ScriptConfigItem &config_item) const
	{
		return _game_mode == GM_MENU
			|| _game_mode == GM_EDITOR
			|| !Company::IsValidID(this->slot)
			|| (config_item.flags & SCRIPTCONFIG_INGAME) != 0
			|| _settings_client.gui.ai_developer_tools;
	}

	void SetValue(int value)
	{
		VisibleSettingsList::const_iterator it = this->visible_settings.begin();
		for (int i = 0; i < this->clicked_row; i++) it++;
		const ScriptConfigItem config_item = **it;
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot) && (config_item.flags & SCRIPTCONFIG_INGAME) == 0) return;
		this->ai_config->SetSetting(config_item.name, value);
		this->SetDirty();
	}
};

/** Widgets for the Script settings window. */
static const NWidgetPart _nested_ai_settings_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, WID_AIS_CAPTION), SetDataTip(STR_AI_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIS_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_NULL), SetScrollbar(WID_AIS_SCROLLBAR),
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
static WindowDesc _ai_settings_desc(
	WDP_CENTER, "settings_ai", 500, 208,
	WC_AI_SETTINGS, WC_NONE,
	0,
	_nested_ai_settings_widgets, lengthof(_nested_ai_settings_widgets)
);

/**
 * Open the AI settings window to change the settings for a Script.
 * @param slot The CompanyID of the Script to change the settings.
 */
void ShowAISettingsWindow(CompanyID slot)
{
	CloseWindowByClass(WC_SCRIPT_LIST);
	CloseWindowByClass(WC_AI_SETTINGS);
	new AISettingsWindow(&_ai_settings_desc, slot);
}
