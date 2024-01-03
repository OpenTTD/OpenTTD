/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_widget.h Types related to the script widgets. */

#ifndef WIDGETS_SCRIPT_WIDGET_H
#define WIDGETS_SCRIPT_WIDGET_H

#include "../company_type.h"

/** Widgets of the #ScriptListWindow class. */
enum ScriptListWidgets : WidgetID {
	WID_SCRL_CAPTION,   ///< Caption of the window.
	WID_SCRL_LIST,      ///< The matrix with all available Scripts.
	WID_SCRL_SCROLLBAR, ///< Scrollbar next to the Script list.
	WID_SCRL_INFO_BG,   ///< Panel to draw some Script information on.
	WID_SCRL_ACCEPT,    ///< Accept button.
	WID_SCRL_CANCEL,    ///< Cancel button.
};

/** Widgets of the #ScriptSettingsWindow class. */
enum ScriptSettingsWidgets : WidgetID {
	WID_SCRS_CAPTION,    ///< Caption of the window.
	WID_SCRS_BACKGROUND, ///< Panel to draw the settings on.
	WID_SCRS_SCROLLBAR,  ///< Scrollbar to scroll through all settings.
	WID_SCRS_ACCEPT,     ///< Accept button.
	WID_SCRS_RESET,      ///< Reset button.

	WID_SCRS_SETTING_DROPDOWN = -1, ///< Dynamically created dropdown for changing setting value.
};

/** Widgets of the #ScriptDebugWindow class. */
enum ScriptDebugWidgets : WidgetID {
	WID_SCRD_VIEW,                 ///< The row of company buttons.
	WID_SCRD_NAME_TEXT,            ///< Name of the current selected.
	WID_SCRD_SETTINGS,             ///< Settings button.
	WID_SCRD_SCRIPT_GAME,          ///< Game Script button.
	WID_SCRD_RELOAD_TOGGLE,        ///< Reload button.
	WID_SCRD_LOG_PANEL,            ///< Panel where the log is in.
	WID_SCRD_VSCROLLBAR,           ///< Vertical scrollbar of the log panel.
	WID_SCRD_COMPANY_BUTTON_START, ///< Buttons in the VIEW.
	WID_SCRD_COMPANY_BUTTON_END = WID_SCRD_COMPANY_BUTTON_START + MAX_COMPANIES - 1, ///< Last possible button in the VIEW.
	WID_SCRD_BREAK_STRING_WIDGETS, ///< The panel to handle the breaking on string.
	WID_SCRD_BREAK_STR_ON_OFF_BTN, ///< Enable breaking on string.
	WID_SCRD_BREAK_STR_EDIT_BOX,   ///< Edit box for the string to break on.
	WID_SCRD_MATCH_CASE_BTN,       ///< Checkbox to use match caching or not.
	WID_SCRD_CONTINUE_BTN,         ///< Continue button.
	WID_SCRD_HSCROLLBAR,           ///< Horizontal scrollbar of the log panel.
};

#endif /* WIDGETS_SCRIPT_WIDGET_H */
