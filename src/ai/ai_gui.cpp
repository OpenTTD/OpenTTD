/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_gui.cpp %Window for configuring the AIs */

#include "../stdafx.h"
#include "../error.h"
#include "../company_base.h"
#include "../window_func.h"
#include "../network/network.h"
#include "../settings_func.h"
#include "../network/network_content.h"
#include "../core/geometry_func.hpp"

#include "ai.hpp"
#include "ai_gui.hpp"
#include "ai_config.hpp"
#include "ai_info.hpp"
#include "../script/script_gui.h"
#include "table/strings.h"

#include "../safeguards.h"


/** Widgets for the configure AI window. */
static constexpr NWidgetPart _nested_ai_config_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_CAPTION_AI, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_AIC_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_DECREASE_NUMBER), SetDataTip(AWV_DECREASE, STR_NULL),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_INCREASE_NUMBER), SetDataTip(AWV_INCREASE, STR_NULL),
					EndContainer(),
					NWidget(WWT_TEXT, COLOUR_MAUVE, WID_AIC_NUMBER), SetDataTip(STR_AI_CONFIG_MAX_COMPETITORS, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_DECREASE_INTERVAL), SetDataTip(AWV_DECREASE, STR_NULL),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_INCREASE_INTERVAL), SetDataTip(AWV_INCREASE, STR_NULL),
					EndContainer(),
					NWidget(WWT_TEXT, COLOUR_MAUVE, WID_AIC_INTERVAL), SetDataTip(STR_AI_CONFIG_COMPETITORS_INTERVAL, STR_NULL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_UP), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_UP, STR_AI_CONFIG_MOVE_UP_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_DOWN), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_MOVE_DOWN, STR_AI_CONFIG_MOVE_DOWN_TOOLTIP),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_FRAME, COLOUR_MAUVE), SetDataTip(STR_AI_CONFIG_AI, STR_NULL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIC_LIST), SetMinimalSize(288, 112), SetFill(1, 0), SetMatrixDataTip(1, 8, STR_AI_CONFIG_AILIST_TOOLTIP), SetScrollbar(WID_AIC_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_AIC_SCROLLBAR),
				EndContainer(),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONFIGURE), SetFill(1, 0), SetDataTip(STR_AI_CONFIG_CONFIGURE, STR_AI_CONFIG_CONFIGURE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CHANGE), SetFill(1, 1), SetDataTip(STR_AI_CONFIG_CHANGE_AI, STR_AI_CONFIG_CHANGE_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONTENT_DOWNLOAD), SetFill(1, 1), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
				NWidget(NWID_VERTICAL, NC_EQUALSIZE),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetDataTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_README), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_CHANGELOG), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_LICENSE), SetFill(1, 1), SetResize(1, 0), SetDataTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the configure AI window. */
static WindowDesc _ai_config_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	std::begin(_nested_ai_config_widgets), std::end(_nested_ai_config_widgets)
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

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowByClass(WC_SCRIPT_LIST);
		CloseWindowByClass(WC_SCRIPT_SETTINGS);
		this->Window::Close();
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_AIC_NUMBER:
				SetDParam(0, GetGameSettings().difficulty.max_no_competitors);
				break;

			case WID_AIC_INTERVAL:
				SetDParam(0, GetGameSettings().difficulty.competitors_interval);
				break;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_AIC_DECREASE_NUMBER:
			case WID_AIC_INCREASE_NUMBER:
			case WID_AIC_DECREASE_INTERVAL:
			case WID_AIC_INCREASE_INTERVAL:
				*size = maxdim(*size, NWidgetScrollbar::GetHorizontalDimension());
				break;

			case WID_AIC_LIST:
				this->line_height = GetCharacterHeight(FS_NORMAL) + padding.height;
				resize->height = this->line_height;
				size->height = 8 * this->line_height;
				break;
		}
	}

	/**
	 * Can the AI config in the given company slot be edited?
	 * @param slot The slot to query.
	 * @return True if and only if the given AI Config slot can be edited.
	 */
	static bool IsEditable(CompanyID slot)
	{
		if (_game_mode != GM_NORMAL) {
			return slot > 0 && slot < MAX_COMPANIES;
		}
		return slot < MAX_COMPANIES && !Company::IsValidID(slot);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_AIC_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.matrix);
				int max_slot = GetGameSettings().difficulty.max_no_competitors;
				if (_game_mode == GM_NORMAL) {
					for (const Company *c : Company::Iterate()) {
						if (c->is_ai) max_slot--;
					}
					for (CompanyID cid = COMPANY_FIRST; cid < (CompanyID)max_slot && cid < MAX_COMPANIES; cid++) {
						if (Company::IsValidID(cid)) max_slot++;
					}
				} else {
					max_slot++; // Slot 0 is human
				}
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

					TextColour tc = TC_SILVER;
					if (this->selected_slot == i) {
						tc = TC_WHITE;
					} else if (IsEditable((CompanyID)i)) {
						if (i < max_slot) tc = TC_ORANGE;
					} else if (Company::IsValidAiID(i)) {
						tc = TC_GREEN;
					}
					DrawString(tr, text, tc);
					tr.top += this->line_height;
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_AIC_TEXTFILE && widget < WID_AIC_TEXTFILE + TFT_CONTENT_END) {
			if (this->selected_slot == INVALID_COMPANY || AIConfig::GetConfig(this->selected_slot) == nullptr) return;

			ShowScriptTextfileWindow((TextfileType)(widget - WID_AIC_TEXTFILE), this->selected_slot);
			return;
		}

		switch (widget) {
			case WID_AIC_DECREASE_NUMBER:
			case WID_AIC_INCREASE_NUMBER: {
				int new_value;
				if (widget == WID_AIC_DECREASE_NUMBER) {
					new_value = std::max(0, GetGameSettings().difficulty.max_no_competitors - 1);
				} else {
					new_value = std::min(MAX_COMPANIES - 1, GetGameSettings().difficulty.max_no_competitors + 1);
				}
				IConsoleSetSetting("difficulty.max_no_competitors", new_value);
				this->InvalidateData();
				break;
			}

			case WID_AIC_DECREASE_INTERVAL:
			case WID_AIC_INCREASE_INTERVAL: {
				int new_value;
				if (widget == WID_AIC_DECREASE_INTERVAL) {
					new_value = std::max(static_cast<int>(MIN_COMPETITORS_INTERVAL), GetGameSettings().difficulty.competitors_interval - 1);
				} else {
					new_value = std::min(static_cast<int>(MAX_COMPETITORS_INTERVAL), GetGameSettings().difficulty.competitors_interval + 1);
				}
				IConsoleSetSetting("difficulty.competitors_interval", new_value);
				this->InvalidateData();
				break;
			}

			case WID_AIC_LIST: { // Select a slot
				this->selected_slot = (CompanyID)this->vscroll->GetScrolledRowFromWidget(pt.y, this, widget);
				this->InvalidateData();
				if (click_count > 1 && IsEditable(this->selected_slot)) ShowScriptListWindow((CompanyID)this->selected_slot, _ctrl_pressed);
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

			case WID_AIC_OPEN_URL: {
				const AIConfig *config = AIConfig::GetConfig(this->selected_slot);
				if (this->selected_slot == INVALID_COMPANY || config == nullptr || config->GetInfo() == nullptr) return;
				OpenBrowser(config->GetInfo()->GetURL());
				break;
			}

			case WID_AIC_CHANGE:  // choose other AI
				if (IsEditable(this->selected_slot)) ShowScriptListWindow((CompanyID)this->selected_slot, _ctrl_pressed);
				break;

			case WID_AIC_CONFIGURE: // change the settings for an AI
				ShowScriptSettingsWindow((CompanyID)this->selected_slot);
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
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!IsEditable(this->selected_slot) && !Company::IsValidAiID(this->selected_slot)) {
			this->selected_slot = INVALID_COMPANY;
		}

		if (!gui_scope) return;

		AIConfig *config = this->selected_slot == INVALID_COMPANY ? nullptr : AIConfig::GetConfig(this->selected_slot);

		this->SetWidgetDisabledState(WID_AIC_DECREASE_NUMBER, GetGameSettings().difficulty.max_no_competitors == 0);
		this->SetWidgetDisabledState(WID_AIC_INCREASE_NUMBER, GetGameSettings().difficulty.max_no_competitors == MAX_COMPANIES - 1);
		this->SetWidgetDisabledState(WID_AIC_DECREASE_INTERVAL, GetGameSettings().difficulty.competitors_interval == MIN_COMPETITORS_INTERVAL);
		this->SetWidgetDisabledState(WID_AIC_INCREASE_INTERVAL, GetGameSettings().difficulty.competitors_interval == MAX_COMPETITORS_INTERVAL);
		this->SetWidgetDisabledState(WID_AIC_CHANGE, !IsEditable(this->selected_slot));
		this->SetWidgetDisabledState(WID_AIC_CONFIGURE, this->selected_slot == INVALID_COMPANY || config->GetConfigList()->empty());
		this->SetWidgetDisabledState(WID_AIC_MOVE_UP, !IsEditable(this->selected_slot) || !IsEditable((CompanyID)(this->selected_slot - 1)));
		this->SetWidgetDisabledState(WID_AIC_MOVE_DOWN, !IsEditable(this->selected_slot) || !IsEditable((CompanyID)(this->selected_slot + 1)));

		this->SetWidgetDisabledState(WID_AIC_OPEN_URL, this->selected_slot == INVALID_COMPANY || config->GetInfo() == nullptr || config->GetInfo()->GetURL().empty());
		for (TextfileType tft = TFT_CONTENT_BEGIN; tft < TFT_CONTENT_END; tft++) {
			this->SetWidgetDisabledState(WID_AIC_TEXTFILE + tft, this->selected_slot == INVALID_COMPANY || !config->GetTextfile(tft, this->selected_slot).has_value());
		}
	}
};

/** Open the AI config window. */
void ShowAIConfigWindow()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new AIConfigWindow();
}

