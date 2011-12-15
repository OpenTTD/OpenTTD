/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallmap_widget.h Types related to the smallmap widgets. */

#ifndef WIDGETS_SMALLMAP_WIDGET_H
#define WIDGETS_SMALLMAP_WIDGET_H

/** Widgets of the WC_SMALLMAP. */
enum SmallMapWindowWidgets {
	SM_WIDGET_CAPTION,           ///< Caption widget.
	SM_WIDGET_MAP_BORDER,        ///< Border around the smallmap.
	SM_WIDGET_MAP,               ///< Panel containing the smallmap.
	SM_WIDGET_LEGEND,            ///< Bottom panel to display smallmap legends.
	SM_WIDGET_ZOOM_IN,           ///< Button to zoom in one step.
	SM_WIDGET_ZOOM_OUT,          ///< Button to zoom out one step.
	SM_WIDGET_CONTOUR,           ///< Button to select the contour view (height map).
	SM_WIDGET_VEHICLES,          ///< Button to select the vehicles view.
	SM_WIDGET_INDUSTRIES,        ///< Button to select the industries view.
	SM_WIDGET_ROUTES,            ///< Button to select the routes view.
	SM_WIDGET_VEGETATION,        ///< Button to select the vegetation view.
	SM_WIDGET_OWNERS,            ///< Button to select the owners view.
	SM_WIDGET_CENTERMAP,         ///< Button to move smallmap center to main window center.
	SM_WIDGET_TOGGLETOWNNAME,    ///< Toggle button to display town names.
	SM_WIDGET_SELECT_BUTTONS,    ///< Selection widget for the buttons present in some smallmap modes.
	SM_WIDGET_ENABLE_ALL,        ///< Button to enable display of all legend entries.
	SM_WIDGET_DISABLE_ALL,       ///< Button to disable display of all legend entries.
	SM_WIDGET_SHOW_HEIGHT,       ///< Show heightmap toggle button.
};

#endif /* WIDGETS_SMALLPAM_WIDGET_H */
