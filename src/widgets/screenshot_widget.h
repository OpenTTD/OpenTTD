/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file screenshot_widget.h Types related to the screenshot widgets. */

#ifndef WIDGETS_SCREENSHOT_WIDGET_H
#define WIDGETS_SCREENSHOT_WIDGET_H

/** Widgets of the #ScreenshotWindow class. */

enum ScreenshotWindowWidgets : WidgetID {
	WID_SC_TAKE,             ///< Button for taking a normal screenshot
	WID_SC_TAKE_ZOOMIN,      ///< Button for taking a zoomed in screenshot
	WID_SC_TAKE_DEFAULTZOOM, ///< Button for taking a screenshot at normal zoom
	WID_SC_TAKE_WORLD,       ///< Button for taking a screenshot of the whole world
	WID_SC_TAKE_HEIGHTMAP,   ///< Button for taking a heightmap "screenshot"
	WID_SC_TAKE_MINIMAP,     ///< Button for taking a minimap screenshot
};


#endif /* WIDGETS_SCREENSHOT_WIDGET_H */

