/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bootstrap_widget.h Types related to the bootstrap widgets. */

#ifndef WIDGETS_BOOTSTRAP_WIDGET_H
#define WIDGETS_BOOTSTRAP_WIDGET_H

/** Widgets of the #BootstrapBackground class. */
enum BootstrapBackgroundWidgets : WidgetID {
	WID_BB_BACKGROUND, ///< Background of the window.
};

/** Widgets of the #BootstrapErrmsgWindow class. */
enum BootstrapErrorMessageWidgets : WidgetID {
	WID_BEM_CAPTION, ///< Caption of the window.
	WID_BEM_MESSAGE, ///< Error message.
	WID_BEM_QUIT,    ///< Quit button.
};

/** Widgets of the #BootstrapContentDownloadStatusWindow class. */
enum BootstrapAskForDownloadWidgets : WidgetID {
	WID_BAFD_QUESTION, ///< The question whether to download.
	WID_BAFD_YES,      ///< An affirmative answer to the question.
	WID_BAFD_NO,       ///< An negative answer to the question.
};

#endif /* WIDGETS_BOOTSTRAP_WIDGET_H */
