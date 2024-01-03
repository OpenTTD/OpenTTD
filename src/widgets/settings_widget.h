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
enum GameOptionsWidgets : WidgetID {
	WID_GO_TAB_GENERAL,            ///< General tab.
	WID_GO_TAB_GRAPHICS,           ///< Graphics tab.
	WID_GO_TAB_SOUND,              ///< Sound tab.
	WID_GO_TAB_SELECTION,          ///< Background of the tab selection.
	WID_GO_CURRENCY_DROPDOWN,      ///< Currency dropdown.
	WID_GO_DISTANCE_DROPDOWN,      ///< Measuring unit dropdown.
	WID_GO_AUTOSAVE_DROPDOWN,      ///< Dropdown to say how often to autosave.
	WID_GO_LANG_DROPDOWN,          ///< Language dropdown.
	WID_GO_RESOLUTION_DROPDOWN,    ///< Dropdown for the resolution.
	WID_GO_FULLSCREEN_BUTTON,      ///< Toggle fullscreen.
	WID_GO_GUI_SCALE,              ///< GUI Scale slider.
	WID_GO_GUI_SCALE_AUTO,         ///< Autodetect GUI scale button.
	WID_GO_GUI_SCALE_BEVEL_BUTTON, ///< Toggle for chunky bevels.
	WID_GO_BASE_GRF_DROPDOWN,      ///< Use to select a base GRF.
	WID_GO_BASE_GRF_PARAMETERS,    ///< Base GRF parameters.
	WID_GO_BASE_GRF_OPEN_URL,      ///< Open base GRF URL.
	WID_GO_BASE_GRF_TEXTFILE,      ///< Open base GRF readme, changelog (+1) or license (+2).
	WID_GO_BASE_GRF_DESCRIPTION = WID_GO_BASE_GRF_TEXTFILE + TFT_CONTENT_END,     ///< Description of selected base GRF.
	WID_GO_BASE_SFX_DROPDOWN,      ///< Use to select a base SFX.
	WID_GO_TEXT_SFX_VOLUME,        ///< Sound effects volume label.
	WID_GO_BASE_SFX_VOLUME,        ///< Change sound effects volume.
	WID_GO_BASE_SFX_OPEN_URL,      ///< Open base SFX URL.
	WID_GO_BASE_SFX_TEXTFILE,      ///< Open base SFX readme, changelog (+1) or license (+2).
	WID_GO_BASE_SFX_DESCRIPTION = WID_GO_BASE_SFX_TEXTFILE + TFT_CONTENT_END,     ///< Description of selected base SFX.
	WID_GO_BASE_MUSIC_DROPDOWN,    ///< Use to select a base music set.
	WID_GO_TEXT_MUSIC_VOLUME,      ///< Music volume label.
	WID_GO_BASE_MUSIC_VOLUME,      ///< Change music volume.
	WID_GO_BASE_MUSIC_JUKEBOX,     ///< Open the jukebox.
	WID_GO_BASE_MUSIC_OPEN_URL,    ///< Open base music URL.
	WID_GO_BASE_MUSIC_TEXTFILE,    ///< Open base music readme, changelog (+1) or license (+2).
	WID_GO_BASE_MUSIC_DESCRIPTION = WID_GO_BASE_MUSIC_TEXTFILE + TFT_CONTENT_END, ///< Description of selected base music set.
	WID_GO_VIDEO_ACCEL_BUTTON,     ///< Toggle for video acceleration.
	WID_GO_VIDEO_VSYNC_BUTTON,     ///< Toggle for video vsync.
	WID_GO_REFRESH_RATE_DROPDOWN,  ///< Dropdown for all available refresh rates.
	WID_GO_VIDEO_DRIVER_INFO,      ///< Label showing details about the current video driver.
	WID_GO_SURVEY_SEL,             ///< Selection to hide survey if no JSON library is compiled in.
	WID_GO_SURVEY_PARTICIPATE_BUTTON, ///< Toggle for participating in the automated survey.
	WID_GO_SURVEY_LINK_BUTTON,     ///< Button to open browser to go to the survey website.
	WID_GO_SURVEY_PREVIEW_BUTTON,  ///< Button to open a preview window with the survey results
};

/** Widgets of the #GameSettingsWindow class. */
enum GameSettingsWidgets : WidgetID {
	WID_GS_FILTER,             ///< Text filter.
	WID_GS_OPTIONSPANEL,       ///< Panel widget containing the option lists.
	WID_GS_SCROLLBAR,          ///< Scrollbar.
	WID_GS_HELP_TEXT,          ///< Information area to display help text of the selected option.
	WID_GS_EXPAND_ALL,         ///< Expand all button.
	WID_GS_COLLAPSE_ALL,       ///< Collapse all button.
	WID_GS_RESET_ALL,          ///< Reset all button.
	WID_GS_RESTRICT_CATEGORY,  ///< Label upfront to the category drop-down box to restrict the list of settings to show
	WID_GS_RESTRICT_TYPE,      ///< Label upfront to the type drop-down box to restrict the list of settings to show
	WID_GS_RESTRICT_DROPDOWN,  ///< The drop down box to restrict the list of settings
	WID_GS_TYPE_DROPDOWN,      ///< The drop down box to choose client/game/company/all settings

	WID_GS_SETTING_DROPDOWN = -1, ///< Dynamically created dropdown for changing setting value.
};

/** Widgets of the #CustomCurrencyWindow class. */
enum CustomCurrencyWidgets : WidgetID {
	WID_CC_RATE_DOWN,      ///< Down button.
	WID_CC_RATE_UP,        ///< Up button.
	WID_CC_RATE,           ///< Rate of currency.
	WID_CC_SEPARATOR_EDIT, ///< Separator edit button.
	WID_CC_SEPARATOR,      ///< Current separator.
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
