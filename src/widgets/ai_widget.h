/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_widget.h Types related to the ai widgets. */

#ifndef WIDGETS_AI_WIDGET_H
#define WIDGETS_AI_WIDGET_H

/** Widgets of the WC_AI_LIST. */
enum AIListWindowWidgets {
	AIL_WIDGET_LIST,             ///< The matrix with all available AIs
	AIL_WIDGET_SCROLLBAR,        ///< Scrollbar next to the AI list
	AIL_WIDGET_INFO_BG,          ///< Panel to draw some AI information on
	AIL_WIDGET_ACCEPT,           ///< Accept button
	AIL_WIDGET_CANCEL,           ///< Cancel button
};

/** Widgets of the WC_AI_SETTINGS. */
enum AISettingsWindowWidgets {
	AIS_WIDGET_BACKGROUND,   ///< Panel to draw the settings on
	AIS_WIDGET_SCROLLBAR,    ///< Scrollbar to scroll through all settings
	AIS_WIDGET_ACCEPT,       ///< Accept button
	AIS_WIDGET_RESET,        ///< Reset button
};

/** Widgets of the WC_GAME_OPTIONS (WC_GAME_OPTIONS is also used in others). */
enum AIConfigWindowWidgets {
	AIC_WIDGET_BACKGROUND,   ///< Window background
	AIC_WIDGET_DECREASE,     ///< Decrease the number of AIs
	AIC_WIDGET_INCREASE,     ///< Increase the number of AIs
	AIC_WIDGET_NUMBER,       ///< Number of AIs
	AIC_WIDGET_LIST,         ///< List with currently selected AIs
	AIC_WIDGET_SCROLLBAR,    ///< Scrollbar to scroll through the selected AIs
	AIC_WIDGET_MOVE_UP,      ///< Move up button
	AIC_WIDGET_MOVE_DOWN,    ///< Move down button
	AIC_WIDGET_CHANGE,       ///< Select another AI button
	AIC_WIDGET_CONFIGURE,    ///< Change AI settings button
	AIC_WIDGET_CLOSE,        ///< Close window button
	AIC_WIDGET_CONTENT_DOWNLOAD, ///< Download content button
};

/** Widgets of the WC_AI_DEBUG. */
enum AIDebugWindowWidgets {
	AID_WIDGET_VIEW,
	AID_WIDGET_NAME_TEXT,
	AID_WIDGET_SETTINGS,
	AID_WIDGET_RELOAD_TOGGLE,
	AID_WIDGET_LOG_PANEL,
	AID_WIDGET_SCROLLBAR,
	AID_WIDGET_COMPANY_BUTTON_START,
	AID_WIDGET_COMPANY_BUTTON_END = AID_WIDGET_COMPANY_BUTTON_START + MAX_COMPANIES - 1,
	AID_BREAK_STRING_WIDGETS,
	AID_WIDGET_BREAK_STR_ON_OFF_BTN,
	AID_WIDGET_BREAK_STR_EDIT_BOX,
	AID_WIDGET_MATCH_CASE_BTN,
	AID_WIDGET_CONTINUE_BTN,
};

#endif /* WIDGETS_AI_WIDGET_H */
