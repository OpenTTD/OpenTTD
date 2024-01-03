/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_widget.h Types related to the newgrf widgets. */

#ifndef WIDGETS_NEWGRF_WIDGET_H
#define WIDGETS_NEWGRF_WIDGET_H

#include "../newgrf_config.h"
#include "../textfile_type.h"

/** Widgets of the #NewGRFParametersWindow class. */
enum NewGRFParametersWidgets : WidgetID {
	WID_NP_CAPTION,          ///< Caption of the window.
	WID_NP_SHOW_NUMPAR,      ///< #NWID_SELECTION to optionally display #WID_NP_NUMPAR.
	WID_NP_NUMPAR_DEC,       ///< Button to decrease number of parameters.
	WID_NP_NUMPAR_INC,       ///< Button to increase number of parameters.
	WID_NP_NUMPAR,           ///< Optional number of parameters.
	WID_NP_NUMPAR_TEXT,      ///< Text description.
	WID_NP_BACKGROUND,       ///< Panel to draw the settings on.
	WID_NP_SCROLLBAR,        ///< Scrollbar to scroll through all settings.
	WID_NP_ACCEPT,           ///< Accept button.
	WID_NP_RESET,            ///< Reset button.
	WID_NP_SHOW_DESCRIPTION, ///< #NWID_SELECTION to optionally display parameter descriptions.
	WID_NP_DESCRIPTION,      ///< Multi-line description of a parameter.

	WID_NP_SETTING_DROPDOWN = -1, ///< Dynamically created dropdown for changing setting value.
};

/** Widgets of the #NewGRFWindow class. */
enum NewGRFStateWidgets : WidgetID {
	WID_NS_PRESET_LIST,       ///< Active NewGRF preset.
	WID_NS_PRESET_SAVE,       ///< Save list of active NewGRFs as presets.
	WID_NS_PRESET_DELETE,     ///< Delete active preset.
	WID_NS_ADD,               ///< Add NewGRF to active list.
	WID_NS_REMOVE,            ///< Remove NewGRF from active list.
	WID_NS_MOVE_UP,           ///< Move NewGRF up in active list.
	WID_NS_MOVE_DOWN,         ///< Move NewGRF down in active list.
	WID_NS_UPGRADE,           ///< Upgrade NewGRFs that have a newer version available.
	WID_NS_FILTER,            ///< Filter list of available NewGRFs.
	WID_NS_FILE_LIST,         ///< List window of active NewGRFs.
	WID_NS_SCROLLBAR,         ///< Scrollbar for active NewGRF list.
	WID_NS_AVAIL_LIST,        ///< List window of available NewGRFs.
	WID_NS_SCROLL2BAR,        ///< Scrollbar for available NewGRF list.
	WID_NS_NEWGRF_INFO_TITLE, ///< Title for Info on selected NewGRF.
	WID_NS_NEWGRF_INFO,       ///< Panel for Info on selected NewGRF.
	WID_NS_OPEN_URL,          ///< Open URL of NewGRF.
	WID_NS_NEWGRF_TEXTFILE,   ///< Open NewGRF readme, changelog (+1) or license (+2).
	WID_NS_SET_PARAMETERS = WID_NS_NEWGRF_TEXTFILE + TFT_CONTENT_END,   ///< Open Parameters Window for selected NewGRF for editing parameters.
	WID_NS_VIEW_PARAMETERS,   ///< Open Parameters Window for selected NewGRF for viewing parameters.
	WID_NS_TOGGLE_PALETTE,    ///< Toggle Palette of selected, active NewGRF.
	WID_NS_APPLY_CHANGES,     ///< Apply changes to NewGRF config.
	WID_NS_RESCAN_FILES,      ///< Rescan files (available NewGRFs).
	WID_NS_RESCAN_FILES2,     ///< Rescan files (active NewGRFs).
	WID_NS_CONTENT_DOWNLOAD,  ///< Open content download (available NewGRFs).
	WID_NS_CONTENT_DOWNLOAD2, ///< Open content download (active NewGRFs).
	WID_NS_SHOW_REMOVE,       ///< Select active list buttons (0 = normal, 1 = simple layout).
	WID_NS_SHOW_APPLY,        ///< Select display of the buttons below the 'details'.
};

/** Widgets of the #SavePresetWindow class. */
enum SavePresetWidgets : WidgetID {
	WID_SVP_PRESET_LIST, ///< List with available preset names.
	WID_SVP_SCROLLBAR,   ///< Scrollbar for the list available preset names.
	WID_SVP_EDITBOX,     ///< Edit box for changing the preset name.
	WID_SVP_CANCEL,      ///< Button to cancel saving the preset.
	WID_SVP_SAVE,        ///< Button to save the preset.
};

/** Widgets of the #ScanProgressWindow class. */
enum ScanProgressWidgets : WidgetID {
	WID_SP_PROGRESS_BAR,  ///< Simple progress bar.
	WID_SP_PROGRESS_TEXT, ///< Text explaining what is happening.
};

#endif /* WIDGETS_NEWGRF_WIDGET_H */
