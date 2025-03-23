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
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetStringTip(STR_AI_CONFIG_CAPTION_AI, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, WID_AIC_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_DECREASE_NUMBER), SetArrowWidgetTypeTip(AWV_DECREASE),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_INCREASE_NUMBER), SetArrowWidgetTypeTip(AWV_INCREASE),
					EndContainer(),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_AIC_NUMBER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_DECREASE_INTERVAL), SetArrowWidgetTypeTip(AWV_DECREASE),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_AIC_INCREASE_INTERVAL), SetArrowWidgetTypeTip(AWV_INCREASE),
					EndContainer(),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_AIC_INTERVAL), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_UP), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_AI_CONFIG_MOVE_UP, STR_AI_CONFIG_MOVE_UP_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_MOVE_DOWN), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_AI_CONFIG_MOVE_DOWN, STR_AI_CONFIG_MOVE_DOWN_TOOLTIP),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_FRAME, COLOUR_MAUVE), SetStringTip(STR_AI_CONFIG_AI), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_MAUVE, WID_AIC_LIST), SetMinimalSize(288, 112), SetFill(1, 0), SetMatrixDataTip(1, 8, STR_AI_CONFIG_AILIST_TOOLTIP), SetScrollbar(WID_AIC_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_AIC_SCROLLBAR),
				EndContainer(),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONFIGURE), SetFill(1, 0), SetStringTip(STR_AI_CONFIG_CONFIGURE, STR_AI_CONFIG_CONFIGURE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CHANGE), SetFill(1, 1), SetStringTip(STR_AI_CONFIG_CHANGE_AI, STR_AI_CONFIG_CHANGE_TOOLTIP),
					NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_CONTENT_DOWNLOAD), SetFill(1, 1), SetStringTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
				EndContainer(),
				NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_README), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_CHANGELOG), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WID_AIC_TEXTFILE + TFT_LICENSE), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
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
	{},
	_nested_ai_config_widgets
);

/**
 * Window to configure which AIs will start.
 */
struct AIConfigWindow : public Window {
	CompanyID selected_slot = CompanyID::Invalid(); ///< The currently selected AI slot or \c CompanyID::Invalid().
	int line_height = 0; ///< Height of a single AI-name line.
	Scrollbar *vscroll = nullptr; ///< Cache of the vertical scrollbar.

	AIConfigWindow() : Window(_ai_config_desc)
	{
		this->InitNested(WN_GAME_OPTIONS_AI); // Initializes 'this->line_height' as a side effect.
		this->vscroll = this->GetScrollbar(WID_AIC_SCROLLBAR);
		this->selected_slot = CompanyID::Invalid();
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

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_AIC_NUMBER:
				return GetString(STR_AI_CONFIG_MAX_COMPETITORS, GetGameSettings().difficulty.max_no_competitors);

			case WID_AIC_INTERVAL:
				return GetString(STR_AI_CONFIG_COMPETITORS_INTERVAL, GetGameSettings().difficulty.competitors_interval);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_AIC_DECREASE_NUMBER:
			case WID_AIC_INCREASE_NUMBER:
			case WID_AIC_DECREASE_INTERVAL:
			case WID_AIC_INCREASE_INTERVAL:
				size = maxdim(size, NWidgetScrollbar::GetHorizontalDimension());
				break;

			case WID_AIC_LIST:
				this->line_height = GetCharacterHeight(FS_NORMAL) + padding.height;
				resize.height = this->line_height;
				size.height = 8 * this->line_height;
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

	/**
	 * Get text to display for a given company slot.
	 * @param cid Company to display.
	 * @returns Text to display for company.
	 */
	std::string GetSlotText(CompanyID cid) const
	{
		if ((_game_mode != GM_NORMAL && cid == 0) || (_game_mode == GM_NORMAL && Company::IsValidHumanID(cid))) return GetString(STR_AI_CONFIG_HUMAN_PLAYER);
		if (const AIInfo *info = AIConfig::GetConfig(cid)->GetInfo(); info != nullptr) return info->GetName();
		return GetString(STR_AI_CONFIG_RANDOM_AI);
	}

	/**
	 * Get colour to display text in for a given company slot.
	 * @param cid Company to display.
	 * @param max_slot Maximum company ID that can be an AI.
	 * @returns Colour to display text for company.
	 */
	TextColour GetSlotColour(CompanyID cid, CompanyID max_slot) const
	{
		if (this->selected_slot == cid) return TC_WHITE;
		if (IsEditable(cid)) return cid < max_slot ? TC_ORANGE : TC_SILVER;
		if (Company::IsValidAiID(cid)) return TC_GREEN;
		return TC_SILVER;
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
					for (CompanyID cid = CompanyID::Begin(); cid < max_slot && cid < MAX_COMPANIES; ++cid) {
						if (Company::IsValidID(cid)) max_slot++;
					}
				} else {
					max_slot++; // Slot 0 is human
				}
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < MAX_COMPANIES; i++) {
					CompanyID cid = static_cast<CompanyID>(i);
					DrawString(tr, this->GetSlotText(cid), this->GetSlotColour(cid, static_cast<CompanyID>(max_slot)));
					tr.top += this->line_height;
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_AIC_TEXTFILE && widget < WID_AIC_TEXTFILE + TFT_CONTENT_END) {
			if (this->selected_slot == CompanyID::Invalid() || AIConfig::GetConfig(this->selected_slot) == nullptr) return;

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
					new_value = std::min<int>(MAX_COMPANIES - 1, GetGameSettings().difficulty.max_no_competitors + 1);
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
					std::swap(GetGameSettings().script_config.ai[this->selected_slot], GetGameSettings().script_config.ai[this->selected_slot - 1]);
					this->selected_slot = CompanyID(this->selected_slot - 1);
					this->vscroll->ScrollTowards(this->selected_slot.base());
					this->InvalidateData();
				}
				break;

			case WID_AIC_MOVE_DOWN:
				if (IsEditable(this->selected_slot) && IsEditable((CompanyID)(this->selected_slot + 1))) {
					std::swap(GetGameSettings().script_config.ai[this->selected_slot], GetGameSettings().script_config.ai[this->selected_slot + 1]);
					++this->selected_slot;
					this->vscroll->ScrollTowards(this->selected_slot.base());
					this->InvalidateData();
				}
				break;

			case WID_AIC_OPEN_URL: {
				const AIConfig *config = AIConfig::GetConfig(this->selected_slot);
				if (this->selected_slot == CompanyID::Invalid() || config == nullptr || config->GetInfo() == nullptr) return;
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
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
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
			this->selected_slot = CompanyID::Invalid();
		}

		if (!gui_scope) return;

		AIConfig *config = this->selected_slot == CompanyID::Invalid() ? nullptr : AIConfig::GetConfig(this->selected_slot);

		this->SetWidgetDisabledState(WID_AIC_DECREASE_NUMBER, GetGameSettings().difficulty.max_no_competitors == 0);
		this->SetWidgetDisabledState(WID_AIC_INCREASE_NUMBER, GetGameSettings().difficulty.max_no_competitors == MAX_COMPANIES - 1);
		this->SetWidgetDisabledState(WID_AIC_DECREASE_INTERVAL, GetGameSettings().difficulty.competitors_interval == MIN_COMPETITORS_INTERVAL);
		this->SetWidgetDisabledState(WID_AIC_INCREASE_INTERVAL, GetGameSettings().difficulty.competitors_interval == MAX_COMPETITORS_INTERVAL);
		this->SetWidgetDisabledState(WID_AIC_CHANGE, !IsEditable(this->selected_slot));
		this->SetWidgetDisabledState(WID_AIC_CONFIGURE, this->selected_slot == CompanyID::Invalid() || config->GetConfigList()->empty());
		this->SetWidgetDisabledState(WID_AIC_MOVE_UP, !IsEditable(this->selected_slot) || !IsEditable((CompanyID)(this->selected_slot - 1)));
		this->SetWidgetDisabledState(WID_AIC_MOVE_DOWN, !IsEditable(this->selected_slot) || !IsEditable((CompanyID)(this->selected_slot + 1)));

		this->SetWidgetDisabledState(WID_AIC_OPEN_URL, this->selected_slot == CompanyID::Invalid() || config->GetInfo() == nullptr || config->GetInfo()->GetURL().empty());
		for (TextfileType tft = TFT_CONTENT_BEGIN; tft < TFT_CONTENT_END; tft++) {
			this->SetWidgetDisabledState(WID_AIC_TEXTFILE + tft, this->selected_slot == CompanyID::Invalid() || !config->GetTextfile(tft, this->selected_slot).has_value());
		}
	}
};

/** Open the AI config window. */
void ShowAIConfigWindow()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new AIConfigWindow();
}

