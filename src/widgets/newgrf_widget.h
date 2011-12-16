/* $Id$ */

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

/** Widgets of the WC_GRF_PARAMETERS. */
enum ShowNewGRFParametersWidgets {
	GRFPAR_WIDGET_SHOW_NUMPAR,      ///< #NWID_SELECTION to optionally display #GRFPAR_WIDGET_NUMPAR
	GRFPAR_WIDGET_NUMPAR_DEC,       ///< Button to decrease number of parameters
	GRFPAR_WIDGET_NUMPAR_INC,       ///< Button to increase number of parameters
	GRFPAR_WIDGET_NUMPAR,           ///< Optional number of parameters
	GRFPAR_WIDGET_NUMPAR_TEXT,      ///< Text description
	GRFPAR_WIDGET_BACKGROUND,       ///< Panel to draw the settings on
	GRFPAR_WIDGET_SCROLLBAR,        ///< Scrollbar to scroll through all settings
	GRFPAR_WIDGET_ACCEPT,           ///< Accept button
	GRFPAR_WIDGET_RESET,            ///< Reset button
	GRFPAR_WIDGET_SHOW_DESCRIPTION, ///< #NWID_SELECTION to optionally display parameter descriptions
	GRFPAR_WIDGET_DESCRIPTION,      ///< Multi-line description of a parameter
};

/** Widgets of the WC_NEWGRF_TEXTFILE. */
enum ShowNewGRFTextfileWidgets {
	GTW_WIDGET_CAPTION,    ///< The caption of the window.
	GTW_WIDGET_BACKGROUND, ///< Panel to draw the textfile on.
	GTW_WIDGET_VSCROLLBAR, ///< Vertical scrollbar to scroll through the textfile up-and-down.
	GTW_WIDGET_HSCROLLBAR, ///< Horizontal scrollbar to scroll through the textfile left-to-right.
};

/** Widgets of the WC_GAME_OPTIONS (WC_GAME_OPTIONS is also used in others). */
enum ShowNewGRFStateWidgets {
	SNGRFS_PRESET_LIST,
	SNGRFS_PRESET_SAVE,
	SNGRFS_PRESET_DELETE,
	SNGRFS_ADD,
	SNGRFS_REMOVE,
	SNGRFS_MOVE_UP,
	SNGRFS_MOVE_DOWN,
	SNGRFS_FILTER,
	SNGRFS_FILE_LIST,
	SNGRFS_SCROLLBAR,
	SNGRFS_AVAIL_LIST,
	SNGRFS_SCROLL2BAR,
	SNGRFS_NEWGRF_INFO_TITLE,
	SNGRFS_NEWGRF_INFO,
	SNGRFS_OPEN_URL,
	SNGRFS_NEWGRF_TEXTFILE,
	SNGRFS_SET_PARAMETERS = SNGRFS_NEWGRF_TEXTFILE + TFT_END,
	SNGRFS_TOGGLE_PALETTE,
	SNGRFS_APPLY_CHANGES,
	SNGRFS_RESCAN_FILES,
	SNGRFS_RESCAN_FILES2,
	SNGRFS_CONTENT_DOWNLOAD,
	SNGRFS_CONTENT_DOWNLOAD2,
	SNGRFS_SHOW_REMOVE, ///< Select active list buttons (0 = normal, 1 = simple layout).
	SNGRFS_SHOW_APPLY,  ///< Select display of the buttons below the 'details'.
};

/** Widgets of the WC_MODAL_PROGRESS (WC_MODAL_PROGRESS is also used in GenerationProgressWindowWidgets). */
enum ScanProgressWindowWidgets {
	SPWW_PROGRESS_BAR,  ///< Simple progress bar.
	SPWW_PROGRESS_TEXT, ///< Text explaining what is happening.
};

#endif /* WIDGETS_NEWGRF_WIDGET_H */
