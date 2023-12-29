/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_widget.h Types related to the GS widgets. */

#ifndef WIDGETS_GS_WIDGET_H
#define WIDGETS_GS_WIDGET_H

#include "../textfile_type.h"

/** Widgets of the #GSConfigWindow class. */
enum GSConfigWidgets : WidgetID {
	WID_GSC_BACKGROUND,       ///< Window background.
	WID_GSC_GSLIST,           ///< List with current selected Game Script.
	WID_GSC_SETTINGS,         ///< Panel to draw the Game Script settings on
	WID_GSC_SCROLLBAR,        ///< Scrollbar to scroll through the selected AIs.
	WID_GSC_CHANGE,           ///< Select another Game Script button.
	WID_GSC_OPEN_URL,         ///< Open GS URL.
	WID_GSC_TEXTFILE,         ///< Open GS readme, changelog (+1) or license (+2).
	WID_GSC_CONTENT_DOWNLOAD = WID_GSC_TEXTFILE + TFT_CONTENT_END, ///< Download content button.
	WID_GSC_RESET,            ///< Reset button.

	WID_GSC_SETTING_DROPDOWN = -1, ///< Dynamically created dropdown for changing setting value.
};

#endif /* WIDGETS_GS_WIDGET_H */
