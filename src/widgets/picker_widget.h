/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file picker_widget.h Types related to the picker widgets. */

#ifndef WIDGETS_PICKER_WIDGET_H
#define WIDGETS_PICKER_WIDGET_H

/** Widgets of the #PickerWindow class. */
enum PickerClassWindowWidgets : WidgetID {
	WID_PW_START = 1 << 16, ///< Dummy to ensure widgets don't overlap.

	WID_PW_CLASS_SEL, ///< Stack to hide the class picker.
	WID_PW_CLASS_FILTER, ///< Editbox filter.
	WID_PW_CLASS_LIST, ///< List of classes.
	WID_PW_CLASS_SCROLL, ///< Scrollbar for list of classes.

	WID_PW_TYPE_SEL, ///< Stack to hide the type picker.
	WID_PW_TYPE_FILTER, ///< Text filter.
	WID_PW_TYPE_MATRIX, ///< Matrix with items.
	WID_PW_TYPE_ITEM, ///< A single item.
	WID_PW_TYPE_SCROLL, ///< Scrollbar for the matrix.
	WID_PW_TYPE_NAME, ///< Name of selected item.
	WID_PW_TYPE_RESIZE, ///< Type resize handle.
};

#endif /* WIDGETS_PICKER_WIDGET_H */
