/* $Id$ */

/** @file ai_gui.cpp Window for configuring the AIs */

#include "../stdafx.h"
#include "../gui.h"
#include "../window_gui.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "../gfx_func.h"
#include "../command_func.h"
#include "../network/network.h"
#include "../string_func.h"
#include "../textbuf_gui.h"
#include "../settings_type.h"
#include "../settings_func.h"
#include "../network/network_content.h"

#include "ai.hpp"
#include "api/ai_log.hpp"
#include "ai_config.hpp"

#include "table/strings.h"

/**
 * Window that let you choose an available AI.
 */
struct AIListWindow : public Window {
	/** Enum referring to the widgets of the AI list window */
	enum AIListWindowWidgets {
		AIL_WIDGET_CLOSEBOX = 0,     ///< Close window button
		AIL_WIDGET_CAPTION,          ///< Window caption
		AIL_WIDGET_LIST,             ///< The matrix with all available AIs
		AIL_WIDGET_SCROLLBAR,        ///< Scrollbar next to the AI list
		AIL_WIDGET_INFO_BG,          ///< Panel to draw some AI information on
		AIL_WIDGET_ACCEPT,           ///< Accept button
		AIL_WIDGET_CANCEL,           ///< Cancel button
		AIL_WIDGET_CONTENT_DOWNLOAD, ///< Download content button
		AIL_WIDGET_RESIZE,           ///< Resize button
	};

	const AIInfoList *ai_info_list;
	int selected;
	CompanyID slot;

	AIListWindow(const WindowDesc *desc, CompanyID slot) : Window(desc, 0),
		slot(slot)
	{
		this->ai_info_list = AI::GetUniqueInfoList();
		this->resize.step_height = 14;
		this->vscroll.cap = (this->widget[AIL_WIDGET_LIST].bottom - this->widget[AIL_WIDGET_LIST].top) / 14 + 1;
		this->widget[AIL_WIDGET_LIST].data = (this->vscroll.cap << 8) + 1;
		SetVScrollCount(this, (int)this->ai_info_list->size() + 1);

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

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		/* Draw a list of all available AIs. */
		int y = this->widget[AIL_WIDGET_LIST].top;
		/* First AI in the list is hardcoded to random */
		if (this->vscroll.pos == 0) {
			DrawStringTruncated(4, y + 3, STR_AI_RANDOM_AI, this->selected == -1 ? TC_WHITE : TC_BLACK, this->width - 8);
			y += 14;
		}
		AIInfo *selected_info = NULL;
		AIInfoList::const_iterator it = this->ai_info_list->begin();
		for (int i = 1; it != this->ai_info_list->end(); i++, it++) {
			if (this->selected == i - 1) selected_info = (*it).second;
			if (IsInsideBS(i, this->vscroll.pos, this->vscroll.cap)) {
				DoDrawStringTruncated((*it).second->GetName(), 4, y + 3, (this->selected == i - 1) ? TC_WHITE : TC_BLACK, this->width - 8);
				y += 14;
			}
		}

		/* Some info about the currently selected AI. */
		if (selected_info != NULL) {
			int y = this->widget[AIL_WIDGET_INFO_BG].top + 6;
			int x = DrawString(4, y, STR_AI_AUTHOR, TC_BLACK);
			DoDrawStringTruncated(selected_info->GetAuthor(), x + 5, y, TC_BLACK, this->width - x - 8);
			y += 13;
			x = DrawString(4, y, STR_AI_VERSION, TC_BLACK);
			static char buf[8];
			sprintf(buf, "%d", selected_info->GetVersion());
			DoDrawStringTruncated(buf, x + 5, y, TC_BLACK, this->width - x - 8);
			y += 13;
			SetDParamStr(0, selected_info->GetDescription());
			DrawStringMultiLine(4, y, STR_JUST_RAW_STRING, this->width - 8, this->widget[AIL_WIDGET_INFO_BG].bottom - y);
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
		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case AIL_WIDGET_LIST: { // Select one of the AIs
				int sel = (pt.y - this->widget[AIL_WIDGET_LIST].top) / 14 + this->vscroll.pos - 1;
				if (sel < (int)this->ai_info_list->size()) {
					this->selected = sel;
					this->SetDirty();
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

			case AIL_WIDGET_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(INVALID_STRING_ID, STR_NETWORK_ERR_NOTAVAILABLE, 0, 0);
				} else {
#if defined(ENABLE_NETWORK)
					ShowNetworkContentListWindow(NULL, CONTENT_TYPE_AI);
#endif
				}
				break;
		}
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		switch (widget) {
			case AIL_WIDGET_LIST: {
				int sel = (pt.y - this->widget[AIL_WIDGET_LIST].top) / 14 + this->vscroll.pos - 1;
				if (sel < (int)this->ai_info_list->size()) {
					this->selected = sel;
					this->ChangeAI();
					delete this;
				}
				break;
			}
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (delta.x != 0) {
			ResizeButtons(this, AIL_WIDGET_ACCEPT, AIL_WIDGET_CANCEL);
		}

		this->vscroll.cap += delta.y / 14;
		this->widget[AIL_WIDGET_LIST].data = (this->vscroll.cap << 8) + 1;
	}
};

/* Widget definition for the ai list window. */
static const Widget _ai_list_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_MAUVE,    0,   10,    0,   13,  STR_00C5,                 STR_018B_CLOSE_WINDOW},             // AIL_WIDGET_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_MAUVE,   11,  199,    0,   13,  STR_AI_LIST_CAPTION,      STR_018C_WINDOW_TITLE_DRAG_THIS},   // AIL_WIDGET_CAPTION
{     WWT_MATRIX,     RESIZE_RB,  COLOUR_MAUVE,    0,  187,   14,  125,  0x501,                    STR_AI_AILIST_TIP},                 // AIL_WIDGET_LIST
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_MAUVE,  188,  199,   14,  125,  0x0,                      STR_0190_SCROLL_BAR_SCROLLS_LIST }, // AIL_WIDGET_SCROLLBAR
{      WWT_PANEL,    RESIZE_RTB,  COLOUR_MAUVE,    0,  199,  126,  209,  0x0,                      STR_NULL},                          // AIL_WIDGET_INFO_BG
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_MAUVE,    0,   99,  210,  221,  STR_AI_ACCEPT,            STR_AI_ACCEPT_TIP},                 // AIL_WIDGET_ACCEPT
{ WWT_PUSHTXTBTN,    RESIZE_RTB,  COLOUR_MAUVE,  100,  199,  210,  221,  STR_AI_CANCEL,            STR_AI_CANCEL_TIP},                 // AIL_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,    RESIZE_RTB,  COLOUR_MAUVE,    0,  187,  222,  233,  STR_CONTENT_INTRO_BUTTON, STR_CONTENT_INTRO_BUTTON_TIP},      // AIL_WIDGET_DOWNLOAD_CONTENT
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_MAUVE,  188,  199,  222,  233,  STR_NULL,                 STR_RESIZE_BUTTON},                 // AIL_WIDGET_RESIZE
{   WIDGETS_END},
};

/* Window definition for the ai list window. */
static const WindowDesc _ai_list_desc(
	WDP_CENTER, WDP_CENTER, 200, 234, 200, 234,
	WC_AI_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_ai_list_widgets
);

void ShowAIListWindow(CompanyID slot)
{
	DeleteWindowByClass(WC_AI_LIST);
	new AIListWindow(&_ai_list_desc, slot);
}

/**
 * Window for settings the parameters of an AI.
 */
struct AISettingsWindow : public Window {
	/** Enum referring to the widgets of the AI settings window */
	enum AISettingsWindowWidgest {
		AIS_WIDGET_CLOSEBOX = 0, ///< Close window button
		AIS_WIDGET_CAPTION,      ///< Window caption
		AIS_WIDGET_BACKGROUND,   ///< Panel to draw the settings on
		AIS_WIDGET_SCROLLBAR,    ///< Scrollbar to scroll through all settings
		AIS_WIDGET_ACCEPT,       ///< Accept button
		AIS_WIDGET_RESET,        ///< Reset button
		AIS_WIDGET_RESIZE,       ///< Resize button
	};

	CompanyID slot;
	AIConfig *ai_config;
	int clicked_button;
	bool clicked_increase;
	int timeout;
	int clicked_row;

	AISettingsWindow(const WindowDesc *desc, CompanyID slot) : Window(desc, 0),
		slot(slot),
		clicked_button(-1),
		timeout(0)
	{
		this->FindWindowPlacementAndResize(desc);
		this->ai_config = AIConfig::GetConfig(slot);
		this->resize.step_height = 14;
		this->vscroll.cap = (this->widget[AIS_WIDGET_BACKGROUND].bottom - this->widget[AIS_WIDGET_BACKGROUND].top) / 14 + 1;
		this->widget[AIS_WIDGET_BACKGROUND].data = (this->vscroll.cap << 8) + 1;
		SetVScrollCount(this, (int)this->ai_config->GetConfigList()->size());
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		AIConfig *config = this->ai_config;
		AIConfigItemList::const_iterator it = config->GetConfigList()->begin();
		int i = 0;
		for (; i < this->vscroll.pos; i++) it++;

		int y = this->widget[AIS_WIDGET_BACKGROUND].top;
		for (; i < this->vscroll.pos + this->vscroll.cap && it != config->GetConfigList()->end(); i++, it++) {
			int current_value = config->GetSetting((*it).name);

			int x = 0;
			if (((*it).flags & AICONFIG_BOOLEAN) != 0) {
				DrawFrameRect(4, y  + 2, 23, y + 10, (current_value != 0) ? COLOUR_GREEN : COLOUR_RED, (current_value != 0) ? FR_LOWERED : FR_NONE);
			} else {
				DrawArrowButtons(4, y + 2, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + !!this->clicked_increase : 0, current_value > (*it).min_value, current_value < (*it).max_value);
				if (it->labels != NULL && it->labels->Find(current_value) != it->labels->End()) {
					x = DoDrawStringTruncated(it->labels->Find(current_value)->second, 28, y + 3, TC_ORANGE, this->width - 32);
				} else {
					SetDParam(0, current_value);
					x = DrawStringTruncated(28, y + 3, STR_JUST_INT, TC_ORANGE, this->width - 32);
				}
			}

			DoDrawStringTruncated((*it).description, max(x + 3, 54), y + 3, TC_LIGHT_BLUE, this->width - (4 + max(x + 3, 54)));
			y += 14;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case AIS_WIDGET_BACKGROUND: {
				int num = (pt.y - this->widget[AIS_WIDGET_BACKGROUND].top) / 14 + this->vscroll.pos;
				if (num >= (int)this->ai_config->GetConfigList()->size()) break;

				AIConfigItemList::const_iterator it = this->ai_config->GetConfigList()->begin();
				for (int i = 0; i < num; i++) it++;
				AIConfigItem config_item = *it;
				bool bool_item = (config_item.flags & AICONFIG_BOOLEAN) != 0;

				const int x = pt.x - 4;
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

					if (_settings_newgame.difficulty.diff_level != 3) {
						_settings_newgame.difficulty.diff_level = 3;
						ShowErrorMessage(INVALID_STRING_ID, STR_DIFFICULTY_TO_CUSTOM, 0, 0);
					}
				} else if (!bool_item) {
					/* Display a query box so users can enter a custom value. */
					this->clicked_row = num;
					SetDParam(0, this->ai_config->GetSetting(config_item.name));
					ShowQueryString(STR_CONFIG_SETTING_INT32, STR_CONFIG_SETTING_QUERY_CAPT, 10, 100, this, CS_NUMERAL, QSF_NONE);
				}

				this->SetDirty();
				break;
			}

			case AIS_WIDGET_ACCEPT:
				delete this;
				break;

			case AIS_WIDGET_RESET:
				this->ai_config->ResetSettings();
				this->SetDirty();
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (StrEmpty(str)) return;
		AIConfigItemList::const_iterator it = this->ai_config->GetConfigList()->begin();
		for (int i = 0; i < this->clicked_row; i++) it++;
		int32 value = atoi(str);
		this->ai_config->SetSetting((*it).name, value);
		this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		if (delta.x != 0) {
			ResizeButtons(this, AIS_WIDGET_ACCEPT, AIS_WIDGET_RESET);
		}

		this->vscroll.cap += delta.y / 14;
		this->widget[AIS_WIDGET_BACKGROUND].data = (this->vscroll.cap << 8) + 1;
	}

	virtual void OnTick()
	{
		if (--this->timeout == 0) {
			this->clicked_button = -1;
			this->SetDirty();
		}
	}
};

/* Widget definition for the AI settings window. */
static const Widget _ai_settings_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_MAUVE,    0,   10,    0,   13,  STR_00C5,                 STR_018B_CLOSE_WINDOW},             // AIS_WIDGET_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_MAUVE,   11,  199,    0,   13,  STR_AI_SETTINGS_CAPTION,  STR_018C_WINDOW_TITLE_DRAG_THIS},   // AIS_WIDGET_CAPTION
{     WWT_MATRIX,     RESIZE_RB,  COLOUR_MAUVE,    0,  187,   14,  195,  0x501,                    STR_NULL},                          // AIS_WIDGET_BACKGROUND
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_MAUVE,  188,  199,   14,  195,  0x0,                      STR_0190_SCROLL_BAR_SCROLLS_LIST }, // AIS_WIDGET_SCROLLBAR
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_MAUVE,    0,   93,  196,  207,  STR_AI_CLOSE,             STR_NULL},                          // AIS_WIDGET_ACCEPT
{ WWT_PUSHTXTBTN,    RESIZE_RTB,  COLOUR_MAUVE,   94,  187,  196,  207,  STR_AI_RESET,             STR_NULL},                          // AIS_WIDGET_RESET
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_MAUVE,  188,  199,  196,  207,  STR_NULL,                 STR_RESIZE_BUTTON},                 // AIS_WIDGET_RESIZE
{   WIDGETS_END},
};

/* Window definition for the AI settings window. */
static const WindowDesc _ai_settings_desc(
	WDP_CENTER, WDP_CENTER, 200, 208, 500, 208,
	WC_AI_SETTINGS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	_ai_settings_widgets
);

void ShowAISettingsWindow(CompanyID slot)
{
	DeleteWindowByClass(WC_AI_LIST);
	DeleteWindowByClass(WC_AI_SETTINGS);
	new AISettingsWindow(&_ai_settings_desc, slot);
}

/* Widget definition for the configure AI window. */
static const Widget _ai_config_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_MAUVE,    0,   10,    0,   13,  STR_00C5,               STR_018B_CLOSE_WINDOW},            // AIC_WIDGET_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_MAUVE,   11,  299,    0,   13,  STR_AI_CONFIG_CAPTION,  STR_018C_WINDOW_TITLE_DRAG_THIS},  // AIC_WIDGET_CAPTION
{      WWT_PANEL,     RESIZE_RB,  COLOUR_MAUVE,    0,  299,   14,  171,  0x0,                    STR_NULL},                         // AIC_WIDGET_BACKGROUND
{     WWT_MATRIX,     RESIZE_RB,  COLOUR_MAUVE,    0,  287,   30,  141,  0x501,                  STR_AI_LIST_TIP},                  // AIC_WIDGET_LIST
{  WWT_SCROLLBAR,     RESIZE_LRB, COLOUR_MAUVE,  288,  299,   30,  141,  STR_NULL,               STR_0190_SCROLL_BAR_SCROLLS_LIST}, // AIC_WIDGET_SCROLLBAR
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_YELLOW,  10,  102,  151,  162,  STR_AI_CHANGE,          STR_AI_CHANGE_TIP},                // AIC_WIDGET_CHANGE
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_YELLOW, 103,  195,  151,  162,  STR_AI_CONFIGURE,       STR_AI_CONFIGURE_TIP},             // AIC_WIDGET_CONFIGURE
{ WWT_PUSHTXTBTN,     RESIZE_TB,  COLOUR_YELLOW, 196,  289,  151,  162,  STR_AI_CLOSE,           STR_NULL},                         // AIC_WIDGET_CLOSE
{   WIDGETS_END},
};

/* Window definition for the configure AI window. */
static const WindowDesc _ai_config_desc(
	WDP_CENTER, WDP_CENTER, 300, 172, 300, 172,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_ai_config_widgets
);

/**
 * Window to configure which AIs will start.
 */
struct AIConfigWindow : public Window {
	/** Enum referring to the widgets of the AI config window */
	enum AIConfigWindowWidgets {
		AIC_WIDGET_CLOSEBOX = 0, ///< Close window button
		AIC_WIDGET_CAPTION,      ///< Window caption
		AIC_WIDGET_BACKGROUND,   ///< Window background
		AIC_WIDGET_LIST,         ///< List with currently selected AIs
		AIC_WIDGET_SCROLLBAR,    ///< Scrollbar to scroll through the selected AIs
		AIC_WIDGET_CHANGE,       ///< Select another AI button
		AIC_WIDGET_CONFIGURE,    ///< Change AI settings button
		AIC_WIDGET_CLOSE,        ///< Close window button
		AIC_WIDGET_RESIZE,       ///< Resize button
	};

	CompanyID selected_slot;
	bool clicked_button;
	bool clicked_increase;
	int timeout;

	AIConfigWindow() : Window(&_ai_config_desc),
		clicked_button(false),
		timeout(0)
	{
		selected_slot = INVALID_COMPANY;
		this->resize.step_height = 14;
		this->vscroll.cap = (this->widget[AIC_WIDGET_LIST].bottom - this->widget[AIC_WIDGET_LIST].top) / 14 + 1;
		this->widget[AIC_WIDGET_LIST].data = (this->vscroll.cap << 8) + 1;
		SetVScrollCount(this, MAX_COMPANIES);
		this->FindWindowPlacementAndResize(&_ai_config_desc);
	}

	~AIConfigWindow()
	{
		DeleteWindowByClass(WC_AI_LIST);
		DeleteWindowByClass(WC_AI_SETTINGS);
	}

	virtual void OnPaint()
	{
		this->SetWidgetDisabledState(AIC_WIDGET_CHANGE, selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(AIC_WIDGET_CONFIGURE, selected_slot == INVALID_COMPANY);
		this->DrawWidgets();

		byte max_competitors = _settings_newgame.difficulty.max_no_competitors;
		DrawArrowButtons(10, 18, COLOUR_YELLOW, this->clicked_button ? 1 + !!this->clicked_increase : 0, max_competitors > 0, max_competitors < MAX_COMPANIES - 1);
		SetDParam(0, _settings_newgame.difficulty.max_no_competitors);
		DrawString(36, 18, STR_6805_MAXIMUM_NO_COMPETITORS, TC_FROMSTRING);

		int y = this->widget[AIC_WIDGET_LIST].top;
		for (int i = this->vscroll.pos; i < this->vscroll.pos + this->vscroll.cap && i < MAX_COMPANIES; i++) {
			StringID text;

			if (AIConfig::GetConfig((CompanyID)i)->GetInfo() != NULL) {
				SetDParamStr(0, AIConfig::GetConfig((CompanyID)i)->GetInfo()->GetName());
				text = STR_JUST_RAW_STRING;
			} else if (i == 0) {
				text = STR_AI_HUMAN_PLAYER;
			} else {
				text = STR_AI_RANDOM_AI;
			}
			DrawStringTruncated(10, y + 3, text, (this->selected_slot == i) ? TC_WHITE : ((i > _settings_newgame.difficulty.max_no_competitors || i == 0) ? TC_SILVER : TC_ORANGE), this->width - 20);
			y += 14;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case AIC_WIDGET_BACKGROUND: {
				/* Check if the user clicked on one of the arrows to configure the number of AIs */
				if (IsInsideBS(pt.x, 10, 20) && IsInsideBS(pt.y, 18, 10)) {
					int new_value;
					if (pt.x <= 20) {
						new_value = max(0, _settings_newgame.difficulty.max_no_competitors - 1);
					} else {
						new_value = min(MAX_COMPANIES - 1, _settings_newgame.difficulty.max_no_competitors + 1);
					}
					IConsoleSetSetting("difficulty.max_no_competitors", new_value);
					this->SetDirty();
				}
				break;
			}

			case AIC_WIDGET_LIST: { // Select a slot
				uint slot = (pt.y - this->widget[AIC_WIDGET_LIST].top) / 14 + this->vscroll.pos;

				if (slot == 0 || slot > _settings_newgame.difficulty.max_no_competitors) slot = INVALID_COMPANY;
				this->selected_slot = (CompanyID)slot;
				this->SetDirty();
				break;
			}

			case AIC_WIDGET_CHANGE:  // choose other AI
				ShowAIListWindow((CompanyID)this->selected_slot);
				break;

			case AIC_WIDGET_CONFIGURE: // change the settings for an AI
				ShowAISettingsWindow((CompanyID)this->selected_slot);
				break;

			case AIC_WIDGET_CLOSE:
				delete this;
				break;
		}
	}

	virtual void OnDoubleClick(Point pt, int widget)
	{
		switch (widget) {
			case AIC_WIDGET_LIST:
				this->OnClick(pt, widget);
				if (this->selected_slot != INVALID_COMPANY) ShowAIListWindow((CompanyID)this->selected_slot);
				break;
		}
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / 14;
		this->widget[AIC_WIDGET_LIST].data = (this->vscroll.cap << 8) + 1;
	}

	virtual void OnTick()
	{
		if (--this->timeout == 0) {
			this->clicked_button = false;
			this->SetDirty();
		}
	}
};

void ShowAIConfigWindow()
{
	DeleteWindowById(WC_GAME_OPTIONS, 0);
	new AIConfigWindow();
}

struct AIDebugWindow : public Window {
	enum AIDebugWindowWidgets {
		AID_WIDGET_CLOSEBOX = 0,
		AID_WIDGET_CAPTION,
		AID_WIDGET_VIEW,
		AID_WIDGET_NAME_TEXT,
		AID_WIDGET_RELOAD_TOGGLE,
		AID_WIDGET_LOG_PANEL,
		AID_WIDGET_SCROLLBAR,
		AID_WIDGET_UNUSED_1,
		AID_WIDGET_UNUSED_2,
		AID_WIDGET_UNUSED_3,
		AID_WIDGET_UNUSED_4,
		AID_WIDGET_UNUSED_5,
		AID_WIDGET_UNUSED_6,
		AID_WIDGET_UNUSED_7,

		AID_WIDGET_COMPANY_BUTTON_START,
		AID_WIDGET_COMPANY_BUTTON_END = AID_WIDGET_COMPANY_BUTTON_START + 14,
	};

	static CompanyID ai_debug_company;
	int redraw_timer;

	AIDebugWindow(const WindowDesc *desc, WindowNumber number) : Window(desc, number)
	{
		/* Disable the companies who are not active or not an AI */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + AID_WIDGET_COMPANY_BUTTON_START, !IsValidCompanyID(i) || !GetCompany(i)->is_ai);
		}
		this->DisableWidget(AID_WIDGET_RELOAD_TOGGLE);

		this->vscroll.cap = 14;
		this->vscroll.pos = 0;
		this->resize.step_height = 12;

		if (ai_debug_company != INVALID_COMPANY) this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* Check if the currently selected company is still active. */
		if (ai_debug_company == INVALID_COMPANY || !IsValidCompanyID(ai_debug_company)) {
			if (ai_debug_company != INVALID_COMPANY) {
				/* Raise and disable the widget for the previous selection. */
				this->RaiseWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
				this->DisableWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);

				ai_debug_company = INVALID_COMPANY;
			}

			for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
				if (IsValidCompanyID(i) && GetCompany(i)->is_ai) {
					/* Lower the widget corresponding to this company. */
					this->LowerWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

					ai_debug_company = i;
					break;
				}
			}
		}

		/* Update "Reload AI" button */
		this->SetWidgetDisabledState(AID_WIDGET_RELOAD_TOGGLE, ai_debug_company == INVALID_COMPANY);

		/* Draw standard stuff */
		this->DrawWidgets();

		/* If there are no active companies, don't display anything else. */
		if (ai_debug_company == INVALID_COMPANY) return;

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			if (!IsValidCompanyID(i) || !GetCompany(i)->is_ai) {
				/* Check if we have the company as an active company */
				if (!this->IsWidgetDisabled(i + AID_WIDGET_COMPANY_BUTTON_START)) {
					/* Bah, company gone :( */
					this->DisableWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

					/* We need a repaint */
					this->SetDirty();
				}
				continue;
			}

			/* Check if we have the company marked as inactive */
			if (this->IsWidgetDisabled(i + AID_WIDGET_COMPANY_BUTTON_START)) {
				/* New AI! Yippie :p */
				this->EnableWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

				/* We need a repaint */
				this->SetDirty();
			}

			byte x = (i == ai_debug_company) ? 1 : 0;
			DrawCompanyIcon(i, (i % 8) * 37 + 13 + x, (i < 8 ? 0 : 13) + 16 + x);
		}

		/* Draw the AI name */
		AIInfo *info = GetCompany(ai_debug_company)->ai_info;
		assert(info != NULL);
		char name[1024];
		snprintf(name, sizeof(name), "%s (v%d)", info->GetName(), info->GetVersion());
		DoDrawString(name, 7, 47, TC_BLACK);

		CompanyID old_company = _current_company;
		_current_company = ai_debug_company;
		AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
		_current_company = old_company;

		SetVScrollCount(this, (log == NULL) ? 0 : log->used);
		this->InvalidateWidget(AID_WIDGET_SCROLLBAR);
		if (log == NULL) return;

		int y = 6;
		for (int i = this->vscroll.pos; i < (this->vscroll.cap + this->vscroll.pos); i++) {
			uint pos = (log->count + log->pos - i) % log->count;
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

			DoDrawStringTruncated(log->lines[pos], 7, this->widget[AID_WIDGET_LOG_PANEL].top + y, colour, this->widget[AID_WIDGET_LOG_PANEL].right - this->widget[AID_WIDGET_LOG_PANEL].left - 14);
			y += 12;
		}
	}

	void ChangeToAI(CompanyID show_ai)
	{
		this->RaiseWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
		ai_debug_company = show_ai;
		this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
		this->SetDirty();
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, AID_WIDGET_COMPANY_BUTTON_START, AID_WIDGET_COMPANY_BUTTON_END + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				ChangeToAI((CompanyID)(widget - AID_WIDGET_COMPANY_BUTTON_START));
			}
		}
		if (widget == AID_WIDGET_RELOAD_TOGGLE && !this->IsWidgetDisabled(widget)) {
			/* First kill the company of the AI, then start a new one. This should start the current AI again */
			DoCommandP(0, 2, ai_debug_company, CMD_COMPANY_CTRL);
			DoCommandP(0, 1, 0, CMD_COMPANY_CTRL);
		}
	}

	virtual void OnTimeout()
	{
		this->RaiseWidget(AID_WIDGET_RELOAD_TOGGLE);
		this->SetDirty();
	}

	virtual void OnInvalidateData(int data = 0)
	{
		if (data == -1 || ai_debug_company == data) this->SetDirty();
	}

	virtual void OnResize(Point new_size, Point delta)
	{
		this->vscroll.cap += delta.y / (int)this->resize.step_height;
	}
};

CompanyID AIDebugWindow::ai_debug_company = INVALID_COMPANY;

static const Widget _ai_debug_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                   STR_018B_CLOSE_WINDOW},                 // AID_WIDGET_CLOSEBOX
{    WWT_CAPTION,  RESIZE_RIGHT,  COLOUR_GREY,    11,   298,     0,    13, STR_AI_DEBUG,               STR_018C_WINDOW_TITLE_DRAG_THIS},       // AID_WIDGET_CAPTION
{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,     0,   298,    14,    40, 0x0,                        STR_NULL},                              // AID_WIDGET_VIEW

{      WWT_PANEL,  RESIZE_RIGHT,  COLOUR_GREY,     0,   149,    41,    60, 0x0,                        STR_AI_DEBUG_NAME_TIP},                 // AID_WIDGET_NAME_TEXT
{ WWT_PUSHTXTBTN,     RESIZE_LR,  COLOUR_GREY,   150,   298,    41,    60, STR_AI_DEBUG_RELOAD,        STR_AI_DEBUG_RELOAD_TIP},               // AID_WIDGET_RELOAD_TOGGLE
{      WWT_PANEL,     RESIZE_RB,  COLOUR_GREY,     0,   286,    61,   240, 0x0,                        STR_NULL},                              // AID_WIDGET_LOG_PANEL
{  WWT_SCROLLBAR,    RESIZE_LRB,  COLOUR_GREY,   287,   298,    61,   228, STR_NULL,                   STR_0190_SCROLL_BAR_SCROLLS_LIST},      // AID_WIDGET_SCROLLBAR
/* As this is WIP, leave the next few so we can work a bit with the GUI */
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   101,   120, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_1
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   121,   140, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_2
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   141,   160, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_3
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   161,   180, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_4
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   181,   200, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_5
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   201,   220, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_6
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   221,   240, 0x0,                        STR_NULL},                              // AID_WIDGET_UNUSED_7

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY}, // AID_WIDGET_COMPANY_BUTTON_START
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   261,   297,    14,    26, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    27,    39, 0x0,                        STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY}, // AID_WIDGET_COMPANY_BUTTON_END
{  WWT_RESIZEBOX,   RESIZE_LRTB,  COLOUR_GREY,   287,   298,   229,   240, STR_NULL,                   STR_RESIZE_BUTTON},
{   WIDGETS_END},
};

static const WindowDesc _ai_debug_desc(
	WDP_AUTO, WDP_AUTO, 299, 241, 299, 241,
	WC_AI_DEBUG, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_RESIZABLE,
	_ai_debug_widgets
);

void ShowAIDebugWindow(CompanyID show_company)
{
	if (!_networking || _network_server) {
		AIDebugWindow *w = (AIDebugWindow *)BringWindowToFrontById(WC_AI_DEBUG, 0);
		if (w == NULL) w = new AIDebugWindow(&_ai_debug_desc, 0);
		if (show_company != INVALID_COMPANY) w->ChangeToAI(show_company);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_AI_DEBUG_SERVER_ONLY, 0, 0);
	}
}
