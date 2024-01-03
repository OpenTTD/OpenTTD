/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_widget.h Types related to the viewport widgets. */

#ifndef WIDGETS_VIEWPORT_WIDGET_H
#define WIDGETS_VIEWPORT_WIDGET_H

/** Widgets of the #ExtraViewportWindow class. */
enum ExtraViewportWidgets : WidgetID {
	WID_EV_CAPTION,      ///< Caption of window.
	WID_EV_VIEWPORT,     ///< The viewport.
	WID_EV_ZOOM_IN,      ///< Zoom in.
	WID_EV_ZOOM_OUT,     ///< Zoom out.
	WID_EV_MAIN_TO_VIEW, ///< Center the view of this viewport on the main view.
	WID_EV_VIEW_TO_MAIN, ///< Center the main view on the view of this viewport.
};

#endif /* WIDGETS_VIEWPORT_WIDGET_H */
