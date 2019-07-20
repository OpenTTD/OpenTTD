/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_widget.h Types related to the ai widgets. */

#ifndef WIDGETS_AI_WIDGET_H
#define WIDGETS_AI_WIDGET_H

#include "../company_type.h"
#include "../textfile_type.h"

/** Widgets of the #AIListWindow class. */
enum AIListWidgets {
	WID_AIL_CAPTION,   ///< Caption of the window.
	WID_AIL_LIST,      ///< The matrix with all available AIs.
	WID_AIL_SCROLLBAR, ///< Scrollbar next to the AI list.
	WID_AIL_INFO_BG,   ///< Panel to draw some AI information on.
	WID_AIL_ACCEPT,    ///< Accept button.
	WID_AIL_CANCEL,    ///< Cancel button.
};

/** Widgets of the #AISettingsWindow class. */
enum AISettingsWidgets {
	WID_AIS_CAPTION,    ///< Caption of the window.
	WID_AIS_BACKGROUND, ///< Panel to draw the settings on.
	WID_AIS_SCROLLBAR,  ///< Scrollbar to scroll through all settings.
	WID_AIS_ACCEPT,     ///< Accept button.
	WID_AIS_RESET,      ///< Reset button.
};

/** Widgets of the #AIConfigWindow class. */
enum AIConfigWidgets {
	WID_AIC_BACKGROUND,       ///< Window background.
	WID_AIC_SD_DECREASE,      ///< Decrease the start delay.
	WID_AIC_SD_INCREASE,      ///< Increase the start delay.
	WID_AIC_START_DELAY,      ///< Days to wait before starting an AI.
	WID_AIC_DECREASE,         ///< Decrease the number of AIs.
	WID_AIC_INCREASE,         ///< Increase the number of AIs.
	WID_AIC_NUMBER,           ///< Number of AIs.
	WID_AIC_GAMELIST,         ///< List with current selected GameScript.
	WID_AIC_LIST,             ///< List with currently selected AIs.
	WID_AIC_SCROLLBAR,        ///< Scrollbar to scroll through the selected AIs.
	WID_AIC_MOVE_UP,          ///< Move up button.
	WID_AIC_MOVE_DOWN,        ///< Move down button.
	WID_AIC_CHANGE,           ///< Select another AI button.
	WID_AIC_CONFIGURE,        ///< Change AI settings button.
	WID_AIC_CLOSE,            ///< Close window button.
	WID_AIC_TEXTFILE,         ///< Open AI readme, changelog (+1) or license (+2).
	WID_AIC_CONTENT_DOWNLOAD = WID_AIC_TEXTFILE + TFT_END, ///< Download content button.
};

/** Widgets of the #AIDebugWindow class. */
enum AIDebugWidgets {
	WID_AID_VIEW,                 ///< The row of company buttons.
	WID_AID_NAME_TEXT,            ///< Name of the current selected.
	WID_AID_SETTINGS,             ///< Settings button.
	WID_AID_SCRIPT_GAME,          ///< Game Script button.
	WID_AID_RELOAD_TOGGLE,        ///< Reload button.
	WID_AID_LOG_PANEL,            ///< Panel where the log is in.
	WID_AID_SCROLLBAR,            ///< Scrollbar of the log panel.
	WID_AID_COMPANY_BUTTON_START, ///< Buttons in the VIEW.
	WID_AID_COMPANY_BUTTON_END = WID_AID_COMPANY_BUTTON_START + MAX_COMPANIES - 1, ///< Last possible button in the VIEW.
	WID_AID_BREAK_STRING_WIDGETS, ///< The panel to handle the breaking on string.
	WID_AID_BREAK_STR_ON_OFF_BTN, ///< Enable breaking on string.
	WID_AID_BREAK_STR_EDIT_BOX,   ///< Edit box for the string to break on.
	WID_AID_MATCH_CASE_BTN,       ///< Checkbox to use match caching or not.
	WID_AID_CONTINUE_BTN,         ///< Continue button.
};

#endif /* WIDGETS_AI_WIDGET_H */
