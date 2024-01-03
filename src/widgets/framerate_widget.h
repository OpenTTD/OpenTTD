/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file framerate_widget.h Types related to the framerate windows widgets. */

#ifndef WIDGETS_FRAMERATE_WIDGET_H
#define WIDGETS_FRAMERATE_WIDGET_H

/** Widgets of the #FramerateWindow class. */
enum FramerateWindowWidgets : WidgetID {
	WID_FRW_CAPTION,
	WID_FRW_RATE_GAMELOOP,
	WID_FRW_RATE_DRAWING,
	WID_FRW_RATE_FACTOR,
	WID_FRW_INFO_DATA_POINTS,
	WID_FRW_TIMES_NAMES,
	WID_FRW_TIMES_CURRENT,
	WID_FRW_TIMES_AVERAGE,
	WID_FRW_ALLOCSIZE,
	WID_FRW_SEL_MEMORY,
	WID_FRW_SCROLLBAR,
};

/** Widgets of the #FrametimeGraphWindow class. */
enum FrametimeGraphWindowWidgets : WidgetID {
	WID_FGW_CAPTION,
	WID_FGW_GRAPH,
};

#endif /* WIDGETS_FRAMERATE_WIDGET_H */
