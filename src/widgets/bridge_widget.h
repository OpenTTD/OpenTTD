/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bridge_widget.h Types related to the bridge widgets. */

#ifndef WIDGETS_BRIDGE_WIDGET_H
#define WIDGETS_BRIDGE_WIDGET_H

/** Widgets of the #BuildBridgeWindow class. */
enum BuildBridgeSelectionWidgets : WidgetID {
	WID_BBS_CAPTION,           ///< Caption of the window.
	WID_BBS_DROPDOWN_ORDER,    ///< Direction of sort dropdown.
	WID_BBS_DROPDOWN_CRITERIA, ///< Criteria of sort dropdown.
	WID_BBS_BRIDGE_LIST,       ///< List of bridges.
	WID_BBS_SCROLLBAR,         ///< Scrollbar of the list.
};

#endif /* WIDGETS_BRIDGE_WIDGET_H */
