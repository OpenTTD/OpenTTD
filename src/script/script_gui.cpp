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

#include "script_gui.h"
#include "script_log.hpp"
#include "script_scanner.hpp"
#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../ai/ai_info.hpp"
#include "../ai/ai_instance.hpp"
#include "../ai/ai_gui.hpp"
#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../game/game_info.hpp"
#include "../game/game_instance.hpp"
#include "../game/game_gui.hpp"
#include "table/strings.h"

#include "../safeguards.h"


/**
 * Window that let you choose an available Script.
 */
struct ScriptListWindow : public Window {
	const ScriptInfoList *info_list;    ///< The list of Scripts.
	ScriptConfig *script_config;        ///< The configuration we're modifying.
	int selected;                       ///< The currently selected Script.
	CompanyID slot;                     ///< The company we're selecting a new Script for.
	int line_height;                    ///< Height of a row in the matrix widget.
	Scrollbar *vscroll;                 ///< Cache of the vertical scrollbar.

	/**
	 * Constructor for the window.
	 * @param desc The description of the window.
	 * @param slot The company we're changing the Script for.
	 */
	ScriptListWindow(WindowDesc *desc, CompanyID slot) : Window(desc),
		slot(slot)
	{
		if (slot == OWNER_DEITY) {
			this->info_list = Game::GetUniqueInfoList();
			this->script_config = GameConfig::GetConfig();
		} else {
			this->info_list = AI::GetUniqueInfoList();
			this->script_config = AIConfig::GetConfig(slot);
		}

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SCRL_SCROLLBAR);
		this->FinishInitNested(); // Initializes 'this->line_height' as side effect.

		this->vscroll->SetCount((int)this->info_list->size() + 1);

		/* Try if we can find the currently selected AI */
		this->selected = -1;
		if (this->script_config->HasScript()) {
			ScriptInfo *info = this->script_config->GetInfo();
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

	void SetStringParameters(int widget) const override
	{
		if (widget != WID_SCRL_CAPTION) return;

		SetDParam(0, (this->slot == OWNER_DEITY) ? STR_AI_LIST_CAPTION_GAMESCRIPT : STR_AI_LIST_CAPTION_AI);
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		if (widget != WID_SCRL_LIST) return;

		this->line_height = FONT_HEIGHT_NORMAL + padding.height;

		resize->width = 1;
		resize->height = this->line_height;
		size->height = 5 * this->line_height;
	}

	void DrawWidget(const Rect &r, int widget) const override
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
				int i = 0;
				for (const auto &item : *this->info_list) {
					i++;
					if (this->vscroll->IsVisible(i)) {
						DrawString(tr, item.second->GetName(), (this->selected == i - 1) ? TC_WHITE : TC_ORANGE);
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
					tr.top += FONT_HEIGHT_NORMAL + WidgetDimensions::scaled.vsep_normal;
					SetDParam(0, selected_info->GetVersion());
					DrawString(tr, STR_AI_LIST_VERSION);
					tr.top += FONT_HEIGHT_NORMAL + WidgetDimensions::scaled.vsep_normal;
					if (selected_info->GetURL() != nullptr) {
						SetDParamStr(0, selected_info->GetURL());
						DrawString(tr, STR_AI_LIST_URL);
						tr.top += FONT_HEIGHT_NORMAL + WidgetDimensions::scaled.vsep_normal;
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
			this->script_config->Change(nullptr);
		} else {
			ScriptInfoList::const_iterator it = this->info_list->begin();
			for (int i = 0; i < this->selected; i++) it++;
			this->script_config->Change((*it).second->GetName(), (*it).second->GetVersion());
		}
		InvalidateWindowData(WC_GAME_OPTIONS, slot == OWNER_DEITY ? WN_GAME_OPTIONS_GS : WN_GAME_OPTIONS_AI);
		if (slot != OWNER_DEITY) InvalidateWindowClassesData(WC_AI_SETTINGS);
		CloseWindowByClass(WC_QUERY_STRING);
		InvalidateWindowClassesData(WC_TEXTFILE);
	}

	void OnClick(Point pt, int widget, int click_count) override
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
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (_game_mode == GM_NORMAL && Company::IsValidID(this->slot)) {
			this->Close();
			return;
		}

		if (!gui_scope) return;

		this->vscroll->SetCount((int)this->info_list->size() + 1);

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
static WindowDesc _script_list_desc(
	WDP_CENTER, "settings_script_list", 200, 234,
	WC_SCRIPT_LIST, WC_NONE,
	0,
	_nested_script_list_widgets, lengthof(_nested_script_list_widgets)
);

/**
 * Open the AI list window to chose an AI for the given company slot.
 * @param slot The slot to change the AI of.
 */
void ShowScriptListWindow(CompanyID slot)
{
	CloseWindowByClass(WC_SCRIPT_LIST);
	new ScriptListWindow(&_script_list_desc, slot);
}


/** Window for displaying the textfile of a AI. */
struct ScriptTextfileWindow : public TextfileWindow {
	CompanyID slot;              ///< View the textfile of this CompanyID slot.
	ScriptConfig *script_config; ///< The configuration we selected.

	ScriptTextfileWindow(TextfileType file_type, CompanyID slot) : TextfileWindow(file_type), slot(slot)
	{
		if (slot == OWNER_DEITY) {
			this->script_config = GameConfig::GetConfig();
		} else {
			this->script_config = AIConfig::GetConfig(slot);
		}
		this->OnInvalidateData();
	}

	void SetStringParameters(int widget) const override
	{
		if (widget == WID_TF_CAPTION) {
			SetDParam(0, (slot == OWNER_DEITY) ? STR_CONTENT_TYPE_GAME_SCRIPT : STR_CONTENT_TYPE_AI);
			SetDParamStr(1, script_config->GetInfo()->GetName());
		}
	}

	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		const char *textfile = script_config->GetTextfile(file_type, slot);
		if (textfile == nullptr) {
			this->Close();
		} else {
			this->LoadTextfile(textfile, (slot == OWNER_DEITY) ? GAME_DIR : AI_DIR);
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

	static CompanyID script_debug_company;                 ///< The AI that is (was last) being debugged.
	int redraw_timer;                                      ///< Timer for redrawing the window, otherwise it'll happen every tick.
	int last_vscroll_pos;                                  ///< Last position of the scrolling.
	bool autoscroll;                                       ///< Whether automatically scrolling should be enabled or not.
	bool show_break_box;                                   ///< Whether the break/debug box is visible.
	static bool break_check_enabled;                       ///< Stop an AI when it prints a matching string
	static char break_string[MAX_BREAK_STR_STRING_LENGTH]; ///< The string to match to the AI output
	QueryString break_editbox;                             ///< Break editbox
	static StringFilter break_string_filter;               ///< Log filter for break.
	static bool case_sensitive_break_check;                ///< Is the matching done case-sensitive
	int highlight_row;                                     ///< The output row that matches the given string, or -1
	Scrollbar *vscroll;                                    ///< Cache of the vertical scrollbar.

	ScriptLog::LogData *GetLogPointer() const
	{
		if (script_debug_company == OWNER_DEITY) return (ScriptLog::LogData *)Game::GetInstance()->GetLogPointer();
		return (ScriptLog::LogData *)Company::Get(script_debug_company)->ai_instance->GetLogPointer();
	}

	/**
	 * Check whether the currently selected AI/GS is dead.
	 * @return true if dead.
	 */
	bool IsDead() const
	{
		if (script_debug_company == OWNER_DEITY) {
			GameInstance *game = Game::GetInstance();
			return game == nullptr || game->IsDead();
		}
		return !Company::IsValidAiID(script_debug_company) || Company::Get(script_debug_company)->ai_instance->IsDead();
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
		if (this->IsValidDebugCompany(script_debug_company)) return;

		script_debug_company = INVALID_COMPANY;

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
	ScriptDebugWindow(WindowDesc *desc, WindowNumber number) : Window(desc), break_editbox(MAX_BREAK_STR_STRING_LENGTH)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SCRD_SCROLLBAR);
		this->show_break_box = _settings_client.gui.ai_developer_tools;
		this->GetWidget<NWidgetStacked>(WID_SCRD_BREAK_STRING_WIDGETS)->SetDisplayedPlane(this->show_break_box ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested(number);

		if (!this->show_break_box) break_check_enabled = false;

		this->last_vscroll_pos = 0;
		this->autoscroll = true;
		this->highlight_row = -1;

		this->querystrings[WID_SCRD_BREAK_STR_EDIT_BOX] = &this->break_editbox;

		SetWidgetsDisabledState(!this->show_break_box, WID_SCRD_BREAK_STR_ON_OFF_BTN, WID_SCRD_BREAK_STR_EDIT_BOX, WID_SCRD_MATCH_CASE_BTN, WIDGET_LIST_END);

		/* Restore the break string value from static variable */
		this->break_editbox.text.Assign(this->break_string);

		this->SelectValidDebugCompany();
		this->InvalidateData(-1);
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		if (widget == WID_SCRD_LOG_PANEL) {
			resize->height = FONT_HEIGHT_NORMAL + WidgetDimensions::scaled.vsep_normal;
			size->height = 14 * resize->height + WidgetDimensions::scaled.framerect.Vertical();
		}
	}

	void OnPaint() override
	{
		this->SelectValidDebugCompany();

		/* Draw standard stuff */
		this->DrawWidgets();

		if (this->IsShaded()) return; // Don't draw anything when the window is shaded.

		bool dirty = false;

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			NWidgetCore *button = this->GetWidget<NWidgetCore>(i + WID_SCRD_COMPANY_BUTTON_START);

			bool valid = Company::IsValidAiID(i);

			/* Check whether the validity of the company changed */
			dirty |= (button->IsDisabled() == valid);

			/* Mark dead/paused AIs by setting the background colour. */
			bool dead = valid && Company::Get(i)->ai_instance->IsDead();
			bool paused = valid && Company::Get(i)->ai_instance->IsPaused();
			/* Re-paint if the button was updated.
			 * (note that it is intentional that SetScriptButtonColour is always called) */
			dirty |= SetScriptButtonColour(*button, dead, paused);

			/* Draw company icon only for valid AI companies */
			if (!valid) continue;

			byte offset = (i == script_debug_company) ? 1 : 0;
			DrawCompanyIcon(i, button->pos_x + button->current_x / 2 - 7 + offset, this->GetWidget<NWidgetBase>(WID_SCRD_COMPANY_BUTTON_START + i)->pos_y + 2 + offset);
		}

		/* Set button colour for Game Script. */
		GameInstance *game = Game::GetInstance();
		bool valid = game != nullptr;
		bool dead = valid && game->IsDead();
		bool paused = valid && game->IsPaused();

		NWidgetCore *button = this->GetWidget<NWidgetCore>(WID_SCRD_SCRIPT_GAME);
		dirty |= (button->IsDisabled() == valid) || SetScriptButtonColour(*button, dead, paused);

		if (dirty) this->InvalidateData(-1);

		/* If there are no active companies, don't display anything else. */
		if (script_debug_company == INVALID_COMPANY) return;

		ScriptLog::LogData *log = this->GetLogPointer();

		int scroll_count = (log == nullptr) ? 0 : log->used;
		if (this->vscroll->GetCount() != scroll_count) {
			this->vscroll->SetCount(scroll_count);

			/* We need a repaint */
			this->SetWidgetDirty(WID_SCRD_SCROLLBAR);
		}

		if (log == nullptr) return;

		/* Detect when the user scrolls the window. Enable autoscroll when the
		 * bottom-most line becomes visible. */
		if (this->last_vscroll_pos != this->vscroll->GetPosition()) {
			this->autoscroll = this->vscroll->GetPosition() >= log->used - this->vscroll->GetCapacity();
		}
		if (this->autoscroll) {
			int scroll_pos = std::max(0, log->used - this->vscroll->GetCapacity());
			if (scroll_pos != this->vscroll->GetPosition()) {
				this->vscroll->SetPosition(scroll_pos);

				/* We need a repaint */
				this->SetWidgetDirty(WID_SCRD_SCROLLBAR);
				this->SetWidgetDirty(WID_SCRD_LOG_PANEL);
			}
		}
		this->last_vscroll_pos = this->vscroll->GetPosition();
	}

	void SetStringParameters(int widget) const override
	{
		if (widget != WID_SCRD_NAME_TEXT) return;

		if (script_debug_company == OWNER_DEITY) {
			const GameInfo *info = Game::GetInfo();
			assert(info != nullptr);
			SetDParam(0, STR_AI_DEBUG_NAME_AND_VERSION);
			SetDParamStr(1, info->GetName());
			SetDParam(2, info->GetVersion());
		} else if (script_debug_company == INVALID_COMPANY || !Company::IsValidAiID(script_debug_company)) {
			SetDParam(0, STR_EMPTY);
		} else {
			const AIInfo *info = Company::Get(script_debug_company)->ai_info;
			assert(info != nullptr);
			SetDParam(0, STR_AI_DEBUG_NAME_AND_VERSION);
			SetDParamStr(1, info->GetName());
			SetDParam(2, info->GetVersion());
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		if (script_debug_company == INVALID_COMPANY) return;

		if (widget != WID_SCRD_LOG_PANEL) return;

		ScriptLog::LogData *log = this->GetLogPointer();
		if (log == nullptr) return;

		Rect br = r.Shrink(WidgetDimensions::scaled.bevel);
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
		for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < log->used; i++) {
			int pos = (i + log->pos + 1 - log->used + log->count) % log->count;
			if (log->lines[pos] == nullptr) break;

			TextColour colour;
			switch (log->type[pos]) {
				case ScriptLog::LOG_SQ_INFO:  colour = TC_BLACK;  break;
				case ScriptLog::LOG_SQ_ERROR: colour = TC_WHITE;  break;
				case ScriptLog::LOG_INFO:     colour = TC_BLACK;  break;
				case ScriptLog::LOG_WARNING:  colour = TC_YELLOW; break;
				case ScriptLog::LOG_ERROR:    colour = TC_RED;    break;
				default:                      colour = TC_BLACK;  break;
			}

			/* Check if the current line should be highlighted */
			if (pos == this->highlight_row) {
				GfxFillRect(br.left, tr.top, br.right, tr.top + this->resize.step_height - 1, PC_BLACK);
				if (colour == TC_BLACK) colour = TC_WHITE; // Make black text readable by inverting it to white.
			}

			DrawString(tr, log->lines[pos], colour, SA_LEFT | SA_FORCE);
			tr.top += this->resize.step_height;
		}
	}

	/**
	 * Change all settings to select another Script.
	 * @param show_ai The new AI to show.
	 */
	void ChangeToScript(CompanyID show_script)
	{
		if (!this->IsValidDebugCompany(show_script)) return;

		script_debug_company = show_script;

		this->highlight_row = -1; // The highlight of one Script make little sense for another Script.

		/* Close script settings windows to prevent confusion */
		CloseWindowByClass(WC_AI_SETTINGS);
		CloseWindowById(WC_GAME_OPTIONS, WN_GAME_OPTIONS_GS);

		this->InvalidateData(-1);

		this->autoscroll = true;
		this->last_vscroll_pos = this->vscroll->GetPosition();
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		/* Also called for hotkeys, so check for disabledness */
		if (this->IsWidgetDisabled(widget)) return;

		/* Check which button is clicked */
		if (IsInsideMM(widget, WID_SCRD_COMPANY_BUTTON_START, WID_SCRD_COMPANY_BUTTON_END + 1)) {
			ChangeToScript((CompanyID)(widget - WID_SCRD_COMPANY_BUTTON_START));
		}

		switch (widget) {
			case WID_SCRD_SCRIPT_GAME:
				ChangeToScript(OWNER_DEITY);
				break;

			case WID_SCRD_RELOAD_TOGGLE:
				if (script_debug_company == OWNER_DEITY) break;
				/* First kill the company of the AI, then start a new one. This should start the current AI again */
				Command<CMD_COMPANY_CTRL>::Post(CCA_DELETE, script_debug_company, CRR_MANUAL, INVALID_CLIENT_ID);
				Command<CMD_COMPANY_CTRL>::Post(CCA_NEW_AI, script_debug_company, CRR_NONE, INVALID_CLIENT_ID);
				break;

			case WID_SCRD_SETTINGS:
				if (script_debug_company == OWNER_DEITY) {
					ShowGSConfigWindow();
				} else {
					ShowAISettingsWindow(script_debug_company);
				}
				break;

			case WID_SCRD_BREAK_STR_ON_OFF_BTN:
				this->break_check_enabled = !this->break_check_enabled;
				this->InvalidateData(-1);
				break;

			case WID_SCRD_MATCH_CASE_BTN:
				this->case_sensitive_break_check = !this->case_sensitive_break_check;
				this->InvalidateData(-1);
				break;

			case WID_SCRD_CONTINUE_BTN:
				/* Unpause current AI / game script and mark the corresponding script button dirty. */
				if (!this->IsDead()) {
					if (script_debug_company == OWNER_DEITY) {
						Game::Unpause();
					} else {
						AI::Unpause(script_debug_company);
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

	void OnEditboxChanged(int wid) override
	{
		if (wid != WID_SCRD_BREAK_STR_EDIT_BOX) return;

		/* Save the current string to static member so it can be restored next time the window is opened. */
		strecpy(this->break_string, this->break_editbox.text.buf, lastof(this->break_string));
		break_string_filter.SetFilterTerm(this->break_string);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 *             This is the company ID of the AI/GS which wrote a new log message, or -1 in other cases.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		/* If the log message is related to the active company tab, check the break string.
		 * This needs to be done in gameloop-scope, so the AI is suspended immediately. */
		if (!gui_scope && data == script_debug_company && this->IsValidDebugCompany(script_debug_company) && this->break_check_enabled && !this->break_string_filter.IsEmpty()) {
			/* Get the log instance of the active company */
			ScriptLog::LogData *log = this->GetLogPointer();

			if (log != nullptr) {
				this->break_string_filter.ResetState();
				this->break_string_filter.AddLine(log->lines[log->pos]);
				if (this->break_string_filter.GetState()) {
					/* Pause execution of script. */
					if (!this->IsDead()) {
						if (script_debug_company == OWNER_DEITY) {
							Game::Pause();
						} else {
							AI::Pause(script_debug_company);
						}
					}

					/* Pause the game. */
					if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED) {
						Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, true);
					}

					/* Highlight row that matched */
					this->highlight_row = log->pos;
				}
			}
		}

		if (!gui_scope) return;

		this->SelectValidDebugCompany();

		ScriptLog::LogData *log = script_debug_company != INVALID_COMPANY ? this->GetLogPointer() : nullptr;
		this->vscroll->SetCount((log == nullptr) ? 0 : log->used);

		/* Update company buttons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + WID_SCRD_COMPANY_BUTTON_START, !Company::IsValidAiID(i));
			this->SetWidgetLoweredState(i + WID_SCRD_COMPANY_BUTTON_START, script_debug_company == i);
		}

		this->SetWidgetDisabledState(WID_SCRD_SCRIPT_GAME, Game::GetGameInstance() == nullptr);
		this->SetWidgetLoweredState(WID_SCRD_SCRIPT_GAME, script_debug_company == OWNER_DEITY);

		this->SetWidgetLoweredState(WID_SCRD_BREAK_STR_ON_OFF_BTN, this->break_check_enabled);
		this->SetWidgetLoweredState(WID_SCRD_MATCH_CASE_BTN, this->case_sensitive_break_check);

		this->SetWidgetDisabledState(WID_SCRD_SETTINGS, script_debug_company == INVALID_COMPANY);
		extern CompanyID _local_company;
		this->SetWidgetDisabledState(WID_SCRD_RELOAD_TOGGLE, script_debug_company == INVALID_COMPANY || script_debug_company == OWNER_DEITY || script_debug_company == _local_company);
		this->SetWidgetDisabledState(WID_SCRD_CONTINUE_BTN, script_debug_company == INVALID_COMPANY ||
			(script_debug_company == OWNER_DEITY ? !Game::IsPaused() : !AI::IsPaused(script_debug_company)));
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SCRD_LOG_PANEL, WidgetDimensions::scaled.framerect.Vertical());
	}

	static HotkeyList hotkeys;
};

CompanyID ScriptDebugWindow::script_debug_company = INVALID_COMPANY;
char ScriptDebugWindow::break_string[MAX_BREAK_STR_STRING_LENGTH] = "";
bool ScriptDebugWindow::break_check_enabled = true;
bool ScriptDebugWindow::case_sensitive_break_check = false;
StringFilter ScriptDebugWindow::break_string_filter(&ScriptDebugWindow::case_sensitive_break_check);

/** Make a number of rows with buttons for each company for the Script debug window. */
NWidgetBase *MakeCompanyButtonRowsScriptDebug(int *biggest_index)
{
	return MakeCompanyButtonRows(biggest_index, WID_SCRD_COMPANY_BUTTON_START, WID_SCRD_COMPANY_BUTTON_END, COLOUR_GREY, 8, STR_AI_DEBUG_SELECT_AI_TOOLTIP);
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

static Hotkey scriptdebug_hotkeys[] = {
	Hotkey('1', "company_1", WID_SCRD_COMPANY_BUTTON_START),
	Hotkey('2', "company_2", WID_SCRD_COMPANY_BUTTON_START + 1),
	Hotkey('3', "company_3", WID_SCRD_COMPANY_BUTTON_START + 2),
	Hotkey('4', "company_4", WID_SCRD_COMPANY_BUTTON_START + 3),
	Hotkey('5', "company_5", WID_SCRD_COMPANY_BUTTON_START + 4),
	Hotkey('6', "company_6", WID_SCRD_COMPANY_BUTTON_START + 5),
	Hotkey('7', "company_7", WID_SCRD_COMPANY_BUTTON_START + 6),
	Hotkey('8', "company_8", WID_SCRD_COMPANY_BUTTON_START + 7),
	Hotkey('9', "company_9", WID_SCRD_COMPANY_BUTTON_START + 8),
	Hotkey((uint16)0, "company_10", WID_SCRD_COMPANY_BUTTON_START + 9),
	Hotkey((uint16)0, "company_11", WID_SCRD_COMPANY_BUTTON_START + 10),
	Hotkey((uint16)0, "company_12", WID_SCRD_COMPANY_BUTTON_START + 11),
	Hotkey((uint16)0, "company_13", WID_SCRD_COMPANY_BUTTON_START + 12),
	Hotkey((uint16)0, "company_14", WID_SCRD_COMPANY_BUTTON_START + 13),
	Hotkey((uint16)0, "company_15", WID_SCRD_COMPANY_BUTTON_START + 14),
	Hotkey('S', "settings", WID_SCRD_SETTINGS),
	Hotkey('0', "game_script", WID_SCRD_SCRIPT_GAME),
	Hotkey((uint16)0, "reload", WID_SCRD_RELOAD_TOGGLE),
	Hotkey('B', "break_toggle", WID_SCRD_BREAK_STR_ON_OFF_BTN),
	Hotkey('F', "break_string", WID_SCRD_BREAK_STR_EDIT_BOX),
	Hotkey('C', "match_case", WID_SCRD_MATCH_CASE_BTN),
	Hotkey(WKC_RETURN, "continue", WID_SCRD_CONTINUE_BTN),
	HOTKEY_LIST_END
};
HotkeyList ScriptDebugWindow::hotkeys("aidebug", scriptdebug_hotkeys, ScriptDebugGlobalHotkeys);

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
		NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_SCRD_NAME_TEXT), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_JUST_STRING, STR_AI_DEBUG_NAME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCRD_SETTINGS), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_SETTINGS, STR_AI_DEBUG_SETTINGS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCRD_RELOAD_TOGGLE), SetMinimalSize(100, 20), SetDataTip(STR_AI_DEBUG_RELOAD, STR_AI_DEBUG_RELOAD_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
		/* Log panel */
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SCRD_LOG_PANEL), SetMinimalSize(287, 180), SetResize(1, 1), SetScrollbar(WID_SCRD_SCROLLBAR),
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
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SCRD_SCROLLBAR),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
EndContainer(),
};

/** Window definition for the Script debug window. */
static WindowDesc _script_debug_desc(
	WDP_AUTO, "script_debug", 600, 450,
	WC_SCRIPT_DEBUG, WC_NONE,
	0,
	_nested_script_debug_widgets, lengthof(_nested_script_debug_widgets),
	&ScriptDebugWindow::hotkeys
);

/**
 * Open the Script debug window and select the given company.
 * @param show_company Display debug information about this AI company.
 */
Window *ShowScriptDebugWindow(CompanyID show_company)
{
	if (!_networking || _network_server) {
		ScriptDebugWindow *w = (ScriptDebugWindow *)BringWindowToFrontById(WC_SCRIPT_DEBUG, 0);
		if (w == nullptr) w = new ScriptDebugWindow(&_script_debug_desc, 0);
		if (show_company != INVALID_COMPANY) w->ChangeToScript(show_company);
		return w;
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
	ScriptDebugWindow::script_debug_company = INVALID_COMPANY;
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
