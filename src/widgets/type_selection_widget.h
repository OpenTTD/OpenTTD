/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file type_selection_widget.h Types related to the type_selection widgets. */

#ifndef WIDGETS_TYPE_SELECTION_WIDGET_H
#define WIDGETS_TYPE_SELECTION_WIDGET_H

/** Widgets of the #BuildVehicleWindow class. */
enum TypeSelectionWidgets : WidgetID {
	WID_TS_CAPTION, ///< Caption of window.
	WID_TS_SORT_ASCENDING_DESCENDING, ///< Sort direction.
	WID_TS_SORT_DROPDOWN, ///< Criteria of sorting dropdown.
	WID_TS_FILTER, ///< Filter by name.
	WID_TS_LIST, ///< List of types.
	WID_TS_SCROLLBAR, ///< Scrollbar of list.
	WID_TS_PANEL, ///< Button panel.
	WID_TS_CONFIGURE_BADGES, ///< Button to configure badges.
	WID_TS_BADGE_FILTER, ///< Container for dropdown badge filters.
};

#endif /* WIDGETS_TYPE_SELECTION_WIDGET_H */
