/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tutorial_widget.h Types related to the tutorial widgets. */

#ifndef WIDGETS_TUTORIAL_WIDGET_H
#define WIDGETS_TUTORIAL_WIDGET_H

#include "../window_type.h"

/** Widgets of the tutorial window. */
enum TutorialWidgets : WidgetID {
	WID_TUT_CAPTION,           ///< Caption of the window
	WID_TUT_PANEL,             ///< Main panel
	WID_TUT_CONTENT,           ///< Content area
	WID_TUT_PAGE_INDICATOR,    ///< Page indicator (e.g., "Page 2/6")
	WID_TUT_PREVIOUS,          ///< Previous page button
	WID_TUT_NEXT,              ///< Next page button
	WID_TUT_CLOSE,             ///< Close button
	WID_TUT_FINISH,            ///< Finish button
	WID_TUT_DONT_SHOW,         ///< Don't show again checkbox
	WID_TUT_SCROLLBAR,         ///< Vertical scrollbar
};

#endif /* WIDGETS_TUTORIAL_WIDGET_H */
