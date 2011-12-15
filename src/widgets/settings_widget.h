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

/** Widgets of the WC_GAME_OPTIONS (WC_GAME_OPTIONS is also used in others). */
enum GameOptionsWidgets {
	GOW_BACKGROUND,             ///< Background of the window
	GOW_CURRENCY_DROPDOWN,      ///< Currency dropdown
	GOW_DISTANCE_DROPDOWN,      ///< Measuring unit dropdown
	GOW_ROADSIDE_DROPDOWN,      ///< Dropdown to select the road side (to set the right side ;))
	GOW_TOWNNAME_DROPDOWN,      ///< Town name dropdown
	GOW_AUTOSAVE_DROPDOWN,      ///< Dropdown to say how often to autosave
	GOW_LANG_DROPDOWN,          ///< Language dropdown
	GOW_RESOLUTION_DROPDOWN,    ///< Dropdown for the resolution
	GOW_FULLSCREEN_BUTTON,      ///< Toggle fullscreen
	GOW_SCREENSHOT_DROPDOWN,    ///< Select the screenshot type... please use PNG!
	GOW_BASE_GRF_DROPDOWN,      ///< Use to select a base GRF
	GOW_BASE_GRF_STATUS,        ///< Info about missing files etc.
	GOW_BASE_GRF_DESCRIPTION,   ///< Description of selected base GRF
	GOW_BASE_SFX_DROPDOWN,      ///< Use to select a base SFX
	GOW_BASE_SFX_DESCRIPTION,   ///< Description of selected base SFX
	GOW_BASE_MUSIC_DROPDOWN,    ///< Use to select a base music set
	GOW_BASE_MUSIC_STATUS,      ///< Info about corrupted files etc.
	GOW_BASE_MUSIC_DESCRIPTION, ///< Description of selected base music set
};

/** Widgets of the WC_GAME_OPTIONS (WC_GAME_OPTIONS is also used in others). */
enum GameDifficultyWidgets {
	GDW_LVL_EASY,
	GDW_LVL_MEDIUM,
	GDW_LVL_HARD,
	GDW_LVL_CUSTOM,
	GDW_HIGHSCORE,
	GDW_ACCEPT,
	GDW_CANCEL,

	GDW_OPTIONS_START,
};

/** Widgets of the WC_GAME_OPTIONS (WC_GAME_OPTIONS is also used in others). */
enum GameSettingsWidgets {
	SETTINGSEL_OPTIONSPANEL, ///< Panel widget containing the option lists
	SETTINGSEL_SCROLLBAR,    ///< Scrollbar
};

/** Widgets of the WC_CUSTOM_CURRENCY. */
enum CustomCurrencyWidgets {
	CUSTCURR_RATE_DOWN,
	CUSTCURR_RATE_UP,
	CUSTCURR_RATE,
	CUSTCURR_SEPARATOR_EDIT,
	CUSTCURR_SEPARATOR,
	CUSTCURR_PREFIX_EDIT,
	CUSTCURR_PREFIX,
	CUSTCURR_SUFFIX_EDIT,
	CUSTCURR_SUFFIX,
	CUSTCURR_YEAR_DOWN,
	CUSTCURR_YEAR_UP,
	CUSTCURR_YEAR,
	CUSTCURR_PREVIEW,
};

#endif /* WIDGETS_SETTINGS_WIDGET_H */
