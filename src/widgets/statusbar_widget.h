/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file statusbar_widget.h Types related to the statusbar widgets. */

#ifndef WIDGETS_STATUSBAR_WIDGET_H
#define WIDGETS_STATUSBAR_WIDGET_H

/** Widgets of the #StatusBarWindow class. */
enum StatusbarWidgets : WidgetID {
	WID_S_LEFT,   ///< Left part of the statusbar; date is shown there.
	WID_S_MIDDLE, ///< Middle part; current news or company name or *** SAVING *** or *** PAUSED ***.
	WID_S_RIGHT,  ///< Right part; bank balance.
};

#endif /* WIDGETS_STATUSBAR_WIDGET_H */
