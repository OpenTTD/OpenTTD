/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_widget.h Types related to the settings widgets. */

#ifndef WIDGETS_SETTINGS_WIDGET_H
#define WIDGETS_SETTINGS_WIDGET_H

/** Widgets of the #GameOptionsWindow class. */
enum GameOptionsWidgets {
	WID_GO_BACKGROUND,             ///< Background of the window.
	WID_GO_CURRENCY_DROPDOWN,      ///< Currency dropdown.
	WID_GO_DISTANCE_DROPDOWN,      ///< Measuring unit dropdown.
	WID_GO_ROADSIDE_DROPDOWN,      ///< Dropdown to select the road side (to set the right side ;)).
	WID_GO_TOWNNAME_DROPDOWN,      ///< Town name dropdown.
	WID_GO_AUTOSAVE_DROPDOWN,      ///< Dropdown to say how often to autosave.
	WID_GO_LANG_DROPDOWN,          ///< Language dropdown.
	WID_GO_RESOLUTION_DROPDOWN,    ///< Dropdown for the resolution.
	WID_GO_FULLSCREEN_BUTTON,      ///< Toggle fullscreen.
	WID_GO_SCREENSHOT_DROPDOWN,    ///< Select the screenshot type... please use PNG!.
	WID_GO_BASE_GRF_DROPDOWN,      ///< Use to select a base GRF.
	WID_GO_BASE_GRF_STATUS,        ///< Info about missing files etc.
	WID_GO_BASE_GRF_DESCRIPTION,   ///< Description of selected base GRF.
	WID_GO_BASE_SFX_DROPDOWN,      ///< Use to select a base SFX.
	WID_GO_BASE_SFX_DESCRIPTION,   ///< Description of selected base SFX.
	WID_GO_BASE_MUSIC_DROPDOWN,    ///< Use to select a base music set.
	WID_GO_BASE_MUSIC_STATUS,      ///< Info about corrupted files etc.
	WID_GO_BASE_MUSIC_DESCRIPTION, ///< Description of selected base music set.
};

/** Widgets of the #GameDifficultyWindow class. */
enum GameDifficultyWidgets {
	WID_GD_LVL_EASY,      ///< Easy level button.
	WID_GD_LVL_MEDIUM,    ///< Medium level button.
	WID_GD_LVL_HARD,      ///< Hard level button.
	WID_GD_LVL_CUSTOM,    ///< Custom level button.
	WID_GD_HIGHSCORE,     ///< Highscore button.
	WID_GD_ACCEPT,        ///< Accept button.
	WID_GD_CANCEL,        ///< Cancel button.

	WID_GD_OPTIONS_START, ///< Start of the options.
};

/** Widgets of the #GameSettingsWindow class. */
enum GameSettingsWidgets {
	WID_GS_OPTIONSPANEL, ///< Panel widget containing the option lists.
	WID_GS_SCROLLBAR,    ///< Scrollbar.
};

/** Widgets of the #CustomCurrencyWindow class. */
enum CustomCurrencyWidgets {
	WID_CC_RATE_DOWN,      ///< Down button.
	WID_CC_RATE_UP,        ///< Up button.
	WID_CC_RATE,           ///< Rate of currency.
	WID_CC_SEPARATOR_EDIT, ///< Seperator edit button.
	WID_CC_SEPARATOR,      ///< Current seperator.
	WID_CC_PREFIX_EDIT,    ///< Prefix edit button.
	WID_CC_PREFIX,         ///< Current prefix.
	WID_CC_SUFFIX_EDIT,    ///< Suffix edit button.
	WID_CC_SUFFIX,         ///< Current suffix.
	WID_CC_YEAR_DOWN,      ///< Down button.
	WID_CC_YEAR_UP,        ///< Up button.
	WID_CC_YEAR,           ///< Year of introduction.
	WID_CC_PREVIEW,        ///< Preview.
};

#endif /* WIDGETS_SETTINGS_WIDGET_H */
