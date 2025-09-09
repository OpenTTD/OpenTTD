/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_widget.h Types related to the engine widgets. */

#ifndef WIDGETS_ENGINE_WIDGET_H
#define WIDGETS_ENGINE_WIDGET_H

/** Widgets of the #EnginePreviewWindow class. */
enum EnginePreviewWidgets : WidgetID {
	WID_EP_CAPTION, ///< Caption of window.
	WID_EP_SORT_ASCENDING_DESCENDING, ///< Sort direction.
	WID_EP_VEH_TYPE_FILTER_DROPDOWN, ///< Cargo filter dropdown.
	WID_EP_SHOW_HIDDEN_ENGINES, ///< Toggle whether to display the hidden vehicles.
	WID_EP_CONFIGURE_BADGES, ///< Button to configure badges.
	WID_EP_SORT_DROPDOWN, ///< Criteria of sorting dropdown.
	WID_EP_BADGE_FILTER, ///< Container for dropdown badge filters.
	WID_EP_SCROLLBAR, ///< Scrollbar of list.
	WID_EP_BUILD_SEL, ///< Build button.
	WID_EP_QUESTION, ///< The container for the question.
	WID_EP_NO,       ///< No button.
	WID_EP_YES,      ///< Yes button.
	WID_EP_RENAME, ///< Rename button.
	WID_EP_FILTER, ///< Filter by name.
	WID_EP_LIST, ///< List of vehicles.
};

#endif /* WIDGETS_ENGINE_WIDGET_H */
