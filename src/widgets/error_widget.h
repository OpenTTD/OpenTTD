/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error_widget.h Types related to the error widgets. */

#ifndef WIDGETS_ERROR_WIDGET_H
#define WIDGETS_ERROR_WIDGET_H

/** Widgets of the #ErrmsgWindow class. */
enum ErrorMessageWidgets : WidgetID {
	WID_EM_CAPTION, ///< Caption of the window.
	WID_EM_FACE,    ///< Error title.
	WID_EM_MESSAGE, ///< Error message.
};

#endif /* WIDGETS_ERROR_WIDGET_H */
