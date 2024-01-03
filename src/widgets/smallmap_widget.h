/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallmap_widget.h Types related to the smallmap widgets. */

#ifndef WIDGETS_SMALLMAP_WIDGET_H
#define WIDGETS_SMALLMAP_WIDGET_H

/** Widgets of the #SmallMapWindow class. */
enum SmallMapWidgets : WidgetID {
	WID_SM_CAPTION,        ///< Caption of the window.
	WID_SM_MAP_BORDER,     ///< Border around the smallmap.
	WID_SM_MAP,            ///< Panel containing the smallmap.
	WID_SM_LEGEND,         ///< Bottom panel to display smallmap legends.
	WID_SM_BLANK,          ///< Empty button as placeholder.
	WID_SM_ZOOM_IN,        ///< Button to zoom in one step.
	WID_SM_ZOOM_OUT,       ///< Button to zoom out one step.
	WID_SM_CONTOUR,        ///< Button to select the contour view (height map).
	WID_SM_VEHICLES,       ///< Button to select the vehicles view.
	WID_SM_INDUSTRIES,     ///< Button to select the industries view.
	WID_SM_LINKSTATS,      ///< Button to select the link stats view.
	WID_SM_ROUTES,         ///< Button to select the routes view.
	WID_SM_VEGETATION,     ///< Button to select the vegetation view.
	WID_SM_OWNERS,         ///< Button to select the owners view.
	WID_SM_CENTERMAP,      ///< Button to move smallmap center to main window center.
	WID_SM_TOGGLETOWNNAME, ///< Toggle button to display town names.
	WID_SM_SELECT_BUTTONS, ///< Selection widget for the buttons present in some smallmap modes.
	WID_SM_ENABLE_ALL,     ///< Button to enable display of all legend entries.
	WID_SM_DISABLE_ALL,    ///< Button to disable display of all legend entries.
	WID_SM_SHOW_HEIGHT,    ///< Show heightmap toggle button.
};

#endif /* WIDGETS_SMALLMAP_WIDGET_H */
