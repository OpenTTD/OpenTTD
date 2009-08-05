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
#include "ai_instance.hpp"

#include "table/strings.h"

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

/**
 * Window that let you choose an available AI.
 */
struct AIListWindow : public Window {
	const AIInfoList *ai_info_list;
	int selected;
	CompanyID slot;
	int line_height; // Height of a row in the matrix widget.

	AIListWindow(const WindowDesc *desc, CompanyID slot) : Window(),
		slot(slot)
	{
		this->ai_info_list = AI::GetUniqueInfoList();

		this->InitNested(desc); // Initializes 'this->line_height' as side effect.

		this->vscroll.cap = this->nested_array[AIL_WIDGET_LIST]->current_y / this->line_height;
		this->nested_array[AIL_WIDGET_LIST]->widget_data = (this->vscroll.cap << MAT_ROW_START) + (1 << MAT_COL_START);
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
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget == AIL_WIDGET_LIST) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

			resize->width = 1;
			resize->height = this->line_height;
			size->height = GB(this->nested_array[widget]->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case AIL_WIDGET_LIST: {
				/* Draw a list of all available AIs. */
				int y = this->nested_array[AIL_WIDGET_LIST]->pos_y;
				/* First AI in the list is hardcoded to random */
				if (this->vscroll.pos == 0) {
					DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_LEFT, y + WD_MATRIX_TOP, STR_AI_CONFIG_RANDOM_AI, this->selected == -1 ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				AIInfoList::const_iterator it = this->ai_info_list->begin();
				for (int i = 1; it != this->ai_info_list->end(); i++, it++) {
					if (IsInsideBS(i, this->vscroll.pos, this->vscroll.cap)) {
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
		InvalidateWindow(WC_GAME_OPTIONS, 0);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case AIL_WIDGET_LIST: { // Select one of the AIs
				int sel = (pt.y - this->nested_array[AIL_WIDGET_LIST]->pos_y) / this->line_height + this->vscroll.pos - 1;
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
					ShowErrorMessage(INVALID_STRING_ID, STR_NETWORK_ERROR_NOTAVAILABLE, 0, 0);
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
				int sel = (pt.y - this->nested_array[AIL_WIDGET_LIST]->pos_y) / this->line_height + this->vscroll.pos - 1;
				if (sel < (int)this->ai_info_list->size()) {
					this->selected = sel;
					this->ChangeAI();
					delete this;
				}
				break;
			}
		}
	}

	virtual void OnResize(Point delta)
	{
		this->vscroll.cap += delta.y / this->line_height;
		SetVScrollCount(this, (int)this->ai_info_list->size() + 1);
		this->nested_array[AIL_WIDGET_LIST]->widget_data = (this->vscroll.cap << MAT_ROW_START) + (1 << MAT_COL_START);
	}
};

static const NWidgetPart _nested_ai_list_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE, AIL_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, AIL_WIDGET_CAPTION), SetMinimalSize(189, 14), SetDataTip(STR_AI_LIST_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, AIL_WIDGET_LIST), SetMinimalSize(188, 112), SetResize(1, 1), SetDataTip(0x501, STR_AI_LIST_TOOLTIP),
		NWidget(WWT_SCROLLBAR, COLOUR_MAUVE, AIL_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, AIL_WIDGET_INFO_BG), SetMinimalSize(200, 84), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIL_WIDGET_ACCEPT), SetMinimalSize(100, 12), SetResize(1, 0), SetDataTip(STR_AI_LIST_ACCEPT, STR_AI_LIST_ACCEPT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIL_WIDGET_CANCEL), SetMinimalSize(100, 12), SetResize(1, 0), SetDataTip(STR_AI_LIST_CANCEL, STR_AI_LIST_CANCEL_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIL_WIDGET_CONTENT_DOWNLOAD), SetMinimalSize(188, 12), SetResize(1, 0), SetDataTip(STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE, AIL_WIDGET_RESIZE),
	EndContainer(),
};

/* Window definition for the ai list window. */
static const WindowDesc _ai_list_desc(
	WDP_CENTER, WDP_CENTER, 200, 234, 200, 234,
	WC_AI_LIST, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	NULL, _nested_ai_list_widgets, lengthof(_nested_ai_list_widgets)
);

void ShowAIListWindow(CompanyID slot)
{
	DeleteWindowByClass(WC_AI_LIST);
	new AIListWindow(&_ai_list_desc, slot);
}

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

	AISettingsWindow(const WindowDesc *desc, CompanyID slot) : Window(),
		slot(slot),
		clicked_button(-1),
		timeout(0)
	{
		this->ai_config = AIConfig::GetConfig(slot);

		this->InitNested(desc);  // Initializes 'this->line_height' as side effect.

		this->vscroll.cap = this->nested_array[AIS_WIDGET_BACKGROUND]->current_y / this->line_height;
		this->nested_array[AIS_WIDGET_BACKGROUND]->widget_data = (this->vscroll.cap << MAT_ROW_START) + (1 << MAT_COL_START);
		SetVScrollCount(this, (int)this->ai_config->GetConfigList()->size());
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget == AIS_WIDGET_BACKGROUND) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;

			resize->width = 1;
			resize->height = this->line_height;
			size->height = GB(this->nested_array[widget]->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != AIS_WIDGET_BACKGROUND) return;

		AIConfig *config = this->ai_config;
		AIConfigItemList::const_iterator it = config->GetConfigList()->begin();
		int i = 0;
		for (; i < this->vscroll.pos; i++) it++;

		int y = r.top;
		for (; i < this->vscroll.pos + this->vscroll.cap && it != config->GetConfigList()->end(); i++, it++) {
			int current_value = config->GetSetting((*it).name);

			int x = 0;
			if (((*it).flags & AICONFIG_BOOLEAN) != 0) {
				DrawFrameRect(r.left + 4, y  + 2, r.left + 23, y + 10, (current_value != 0) ? COLOUR_GREEN : COLOUR_RED, (current_value != 0) ? FR_LOWERED : FR_NONE);
			} else {
				DrawArrowButtons(r.left + 4, y + 2, COLOUR_YELLOW, (this->clicked_button == i) ? 1 + !!this->clicked_increase : 0, current_value > (*it).min_value, current_value < (*it).max_value);
				if (it->labels != NULL && it->labels->Find(current_value) != it->labels->End()) {
					x = DrawString(r.left + 28, r.right - WD_MATRIX_LEFT, y + WD_MATRIX_TOP, it->labels->Find(current_value)->second, TC_ORANGE);
				} else {
					SetDParam(0, current_value);
					x = DrawString(r.left + 28, r.right - WD_MATRIX_LEFT, y + WD_MATRIX_TOP, STR_JUST_INT, TC_ORANGE);
				}
			}

			DrawString(max(x + 3, 54), r.right - WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, (*it).description, TC_LIGHT_BLUE);
			y += this->line_height;
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case AIS_WIDGET_BACKGROUND: {
				int num = (pt.y - this->nested_array[AIS_WIDGET_BACKGROUND]->pos_y) / this->line_height + this->vscroll.pos;
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
						ShowErrorMessage(INVALID_STRING_ID, STR_WARNING_DIFFICULTY_TO_CUSTOM, 0, 0);
					}
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

	virtual void OnResize(Point delta)
	{
		this->vscroll.cap += delta.y / this->line_height;
		this->nested_array[AIS_WIDGET_BACKGROUND]->widget_data = (this->vscroll.cap << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnTick()
	{
		if (--this->timeout == 0) {
			this->clicked_button = -1;
			this->SetDirty();
		}
	}
};

static const NWidgetPart _nested_ai_settings_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE, AIS_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, AIS_WIDGET_CAPTION), SetMinimalSize(189, 14), SetDataTip(STR_AI_SETTINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_MAUVE, AIS_WIDGET_BACKGROUND), SetMinimalSize(188, 182), SetResize(1, 1), SetDataTip(0x501, STR_NULL),
		NWidget(WWT_SCROLLBAR, COLOUR_MAUVE, AIS_WIDGET_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIS_WIDGET_ACCEPT), SetMinimalSize(94, 12), SetResize(1, 0), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, AIS_WIDGET_RESET), SetMinimalSize(94, 12), SetResize(1, 0), SetDataTip(STR_AI_SETTINGS_RESET, STR_NULL),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE, AIS_WIDGET_RESIZE),
	EndContainer(),
};

/* Window definition for the AI settings window. */
static const WindowDesc _ai_settings_desc(
	WDP_CENTER, WDP_CENTER, 200, 208, 500, 208,
	WC_AI_SETTINGS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_RESIZABLE,
	NULL, _nested_ai_settings_widgets, lengthof(_nested_ai_settings_widgets)
);

void ShowAISettingsWindow(CompanyID slot)
{
	DeleteWindowByClass(WC_AI_LIST);
	DeleteWindowByClass(WC_AI_SETTINGS);
	new AISettingsWindow(&_ai_settings_desc, slot);
}

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

static const NWidgetPart _nested_ai_config_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE, AIC_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_MAUVE, AIC_WIDGET_CAPTION), SetMinimalSize(289, 14), SetDataTip(STR_AI_CONFIG_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE, AIC_WIDGET_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 16),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_MATRIX, COLOUR_MAUVE, AIC_WIDGET_LIST), SetMinimalSize(288, 112), SetDataTip(0x801, STR_AI_CONFIG_LIST_TOOLTIP),
			NWidget(WWT_SCROLLBAR, COLOUR_MAUVE, AIC_WIDGET_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(5, 0, 5),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CHANGE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_CONFIG_CHANGE, STR_AI_CONFIG_CHANGE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CONFIGURE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_CONFIG_CONFIGURE, STR_AI_CONFIG_CONFIGURE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, AIC_WIDGET_CLOSE), SetFill(1, 0), SetMinimalSize(93, 12), SetDataTip(STR_AI_SETTINGS_CLOSE, STR_NULL),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 9),
	EndContainer(),
};

/* Window definition for the configure AI window. */
static const WindowDesc _ai_config_desc(
	WDP_CENTER, WDP_CENTER, 300, 172, 300, 172,
	WC_GAME_OPTIONS, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	NULL, _nested_ai_config_widgets, lengthof(_nested_ai_config_widgets)
);

/**
 * Window to configure which AIs will start.
 */
struct AIConfigWindow : public Window {
	CompanyID selected_slot;
	bool clicked_button;
	bool clicked_increase;
	int timeout;
	int line_height;

	AIConfigWindow() : Window(),
		clicked_button(false),
		timeout(0)
	{
		this->InitNested(&_ai_config_desc); // Initializes 'this->line_height' as a side effect.
		this->selected_slot = INVALID_COMPANY;
		this->vscroll.cap = this->nested_array[AIC_WIDGET_LIST]->current_y / this->line_height;
		this->nested_array[AIC_WIDGET_LIST]->widget_data = (this->vscroll.cap << MAT_ROW_START) + (1 << MAT_COL_START);
		SetVScrollCount(this, MAX_COMPANIES);
		this->OnInvalidateData(0);
	}

	~AIConfigWindow()
	{
		DeleteWindowByClass(WC_AI_LIST);
		DeleteWindowByClass(WC_AI_SETTINGS);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget == AIC_WIDGET_LIST) {
			this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
			size->height = GB(this->nested_array[widget]->widget_data, MAT_ROW_START, MAT_ROW_BITS) * this->line_height;
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case AIC_WIDGET_BACKGROUND: {
				byte max_competitors = _settings_newgame.difficulty.max_no_competitors;
				DrawArrowButtons(r.left + 10, r.top + 4, COLOUR_YELLOW, this->clicked_button ? 1 + !!this->clicked_increase : 0, max_competitors > 0, max_competitors < MAX_COMPANIES - 1);
				SetDParam(0, _settings_newgame.difficulty.max_no_competitors);
				DrawString(r.left + 36, r.right, r.top + 4, STR_DIFFICULTY_LEVEL_SETTING_MAXIMUM_NO_COMPETITORS);
				break;
			}
			case AIC_WIDGET_LIST: {
				int y = r.top;
				for (int i = this->vscroll.pos; i < this->vscroll.pos + this->vscroll.cap && i < MAX_COMPANIES; i++) {
					StringID text;

					if (AIConfig::GetConfig((CompanyID)i)->GetInfo() != NULL) {
						SetDParamStr(0, AIConfig::GetConfig((CompanyID)i)->GetInfo()->GetName());
						text = STR_JUST_RAW_STRING;
					} else if (i == 0) {
						text = STR_AI_CONFIG_HUMAN_PLAYER;
					} else {
						text = STR_AI_CONFIG_RANDOM_AI;
					}
					DrawString(r.left + 10, r.right - 10, y + WD_MATRIX_TOP, text,
							(this->selected_slot == i) ? TC_WHITE : ((i > _settings_newgame.difficulty.max_no_competitors || i == 0) ? TC_SILVER : TC_ORANGE));
					y += this->line_height;
				}
				break;
			}
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
				uint slot = (pt.y - this->nested_array[widget]->pos_y) / this->line_height + this->vscroll.pos;

				if (slot == 0 || slot > _settings_newgame.difficulty.max_no_competitors) slot = INVALID_COMPANY;
				this->selected_slot = (CompanyID)slot;
				this->OnInvalidateData(0);
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

	virtual void OnInvalidateData(int data)
	{
		this->SetWidgetDisabledState(AIC_WIDGET_CHANGE, this->selected_slot == INVALID_COMPANY);
		this->SetWidgetDisabledState(AIC_WIDGET_CONFIGURE, this->selected_slot == INVALID_COMPANY);
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

/** Enum referring to the widgets of the AI debug window */
enum AIDebugWindowWidgets {
	AID_WIDGET_CLOSEBOX = 0,
	AID_WIDGET_CAPTION,
	AID_WIDGET_STICKY,
	AID_WIDGET_VIEW,
	AID_WIDGET_NAME_TEXT,
	AID_WIDGET_RELOAD_TOGGLE,
	AID_WIDGET_LOG_PANEL,
	AID_WIDGET_SCROLLBAR,
	AID_WIDGET_COMPANY_BUTTON_START,
	AID_WIDGET_COMPANY_BUTTON_END = AID_WIDGET_COMPANY_BUTTON_START + 14,
	AID_WIDGET_RESIZE,
};

/**
 * Window with everything an AI prints via AILog.
 */
struct AIDebugWindow : public Window {
	static CompanyID ai_debug_company;
	int redraw_timer;
	int last_vscroll_pos;
	bool autoscroll;

	AIDebugWindow(const WindowDesc *desc, WindowNumber number) : Window()
	{
		this->vscroll.cap = 14;

		this->InitNested(desc, number);
		/* Disable the companies who are not active or not an AI */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + AID_WIDGET_COMPANY_BUTTON_START, !Company::IsValidAiID(i));
		}
		this->DisableWidget(AID_WIDGET_RELOAD_TOGGLE);

		this->vscroll.pos = 0;
		this->last_vscroll_pos = 0;
		this->autoscroll = true;

		if (ai_debug_company != INVALID_COMPANY) this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget == AID_WIDGET_LOG_PANEL) {
			resize->height = FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
			size->height = this->vscroll.cap * resize->height;
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

		/* Update "Reload AI" button */
		this->SetWidgetDisabledState(AID_WIDGET_RELOAD_TOGGLE, ai_debug_company == INVALID_COMPANY);

		/* Draw standard stuff */
		this->DrawWidgets();

		/* If there are no active companies, don't display anything else. */
		if (ai_debug_company == INVALID_COMPANY) return;

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			/* Background is grey by default, will be changed to red for dead AIs */
			this->nested_array[i + AID_WIDGET_COMPANY_BUTTON_START]->colour = COLOUR_GREY;

			const Company *c = Company::GetIfValid(i);
			if (c == NULL || !c->is_ai) {
				/* Check if we have the company as an active company */
				if (!this->IsWidgetDisabled(i + AID_WIDGET_COMPANY_BUTTON_START)) {
					/* Bah, company gone :( */
					this->DisableWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

					/* We need a repaint */
					this->SetDirty();
				}
				continue;
			}

			/* Mark dead AIs by red background */
			if (c->ai_instance->IsDead()) {
				this->nested_array[i + AID_WIDGET_COMPANY_BUTTON_START]->colour = COLOUR_RED;
			}

			/* Check if we have the company marked as inactive */
			if (this->IsWidgetDisabled(i + AID_WIDGET_COMPANY_BUTTON_START)) {
				/* New AI! Yippie :p */
				this->EnableWidget(i + AID_WIDGET_COMPANY_BUTTON_START);

				/* We need a repaint */
				this->SetDirty();
			}

			byte offset = (i == ai_debug_company) ? 1 : 0;
			DrawCompanyIcon(i, this->nested_array[AID_WIDGET_COMPANY_BUTTON_START + i]->pos_x + 11 + offset, this->nested_array[AID_WIDGET_COMPANY_BUTTON_START + i]->pos_y + 2 + offset);
		}

		CompanyID old_company = _current_company;
		_current_company = ai_debug_company;
		AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
		_current_company = old_company;

		int scroll_count = (log == NULL) ? 0 : log->used;
		if (this->vscroll.count != scroll_count) {
			SetVScrollCount(this, scroll_count);

			/* We need a repaint */
			this->InvalidateWidget(AID_WIDGET_SCROLLBAR);
		}

		if (log == NULL) return;

		/* Detect when the user scrolls the window. Enable autoscroll when the
		 * bottom-most line becomes visible. */
		if (this->last_vscroll_pos != this->vscroll.pos) {
			this->autoscroll = this->vscroll.pos >= log->used - this->vscroll.cap;
		}
		if (this->autoscroll) {
			int scroll_pos = max(0, log->used - this->vscroll.cap);
			if (scroll_pos != this->vscroll.pos) {
				this->vscroll.pos = scroll_pos;

				/* We need a repaint */
				this->InvalidateWidget(AID_WIDGET_SCROLLBAR);
			}
		}
		this->last_vscroll_pos = this->vscroll.pos;

	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (ai_debug_company == INVALID_COMPANY) return;

		switch (widget) {
			case AID_WIDGET_NAME_TEXT: {
				/* Draw the AI name */
				AIInfo *info = Company::Get(ai_debug_company)->ai_info;
				assert(info != NULL);
				char name[1024];
				snprintf(name, sizeof(name), "%s (v%d)", info->GetName(), info->GetVersion());
				DrawString(r.left + 7, r.right - 7, 47, name, TC_BLACK, SA_CENTER);
				break;
			}
			case AID_WIDGET_LOG_PANEL: {
				CompanyID old_company = _current_company;
				_current_company = ai_debug_company;
				AILog::LogData *log = (AILog::LogData *)AIObject::GetLogPointer();
				_current_company = old_company;
				if (log == NULL) return;

				int y = 6;
				for (int i = this->vscroll.pos; i < (this->vscroll.cap + this->vscroll.pos) && i < log->used; i++) {
					uint pos = (i + log->pos + 1 - log->used + log->count) % log->count;
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
		this->LowerWidget(ai_debug_company + AID_WIDGET_COMPANY_BUTTON_START);
		this->autoscroll = true;
		this->last_vscroll_pos = this->vscroll.pos;
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

	virtual void OnResize(Point delta)
	{
		this->vscroll.cap += delta.y / (int)this->resize.step_height;
		SetVScrollCount(this, this->vscroll.count); // vscroll.pos should be in a valid range
	}
};

CompanyID AIDebugWindow::ai_debug_company = INVALID_COMPANY;

static const NWidgetPart _nested_ai_debug_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, AID_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, AID_WIDGET_CAPTION), SetResize(1, 0), SetDataTip(STR_AI_DEBUG, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, AID_WIDGET_STICKY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_VIEW),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 1), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 2), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 3), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 4), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 5), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 6), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 7), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 8), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 9), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 10), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 11), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 12), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 13), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_COMPANY_BUTTON_START + 14), SetMinimalSize(37, 13), SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(39, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 1), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_NAME_TEXT), SetMinimalSize(150, 20), SetResize(1, 0), SetDataTip(0x0, STR_AI_DEBUG_NAME_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, AID_WIDGET_RELOAD_TOGGLE), SetMinimalSize(149, 20), SetDataTip(STR_AI_DEBUG_RELOAD, STR_AI_DEBUG_RELOAD_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, AID_WIDGET_LOG_PANEL), SetMinimalSize(287, 180), SetResize(1, 1),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_SCROLLBAR, COLOUR_GREY, AID_WIDGET_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY, AID_WIDGET_RESIZE),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _ai_debug_desc(
	WDP_AUTO, WDP_AUTO, 299, 241, 299, 241,
	WC_AI_DEBUG, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON | WDF_RESIZABLE,
	NULL, _nested_ai_debug_widgets, lengthof(_nested_ai_debug_widgets)
);

void ShowAIDebugWindow(CompanyID show_company)
{
	if (!_networking || _network_server) {
		AIDebugWindow *w = (AIDebugWindow *)BringWindowToFrontById(WC_AI_DEBUG, 0);
		if (w == NULL) w = new AIDebugWindow(&_ai_debug_desc, 0);
		if (show_company != INVALID_COMPANY) w->ChangeToAI(show_company);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_ERROR_AI_DEBUG_SERVER_ONLY, 0, 0);
	}
}
