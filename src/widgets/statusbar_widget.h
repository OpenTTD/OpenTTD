/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file statusbar_widget.h Types related to the statusbar widgets. */

#ifndef WIDGETS_STATUSBAR_WIDGET_H
#define WIDGETS_STATUSBAR_WIDGET_H

/** Widgets of the WC_STATUS_BAR. */
enum StatusbarWidget {
	SBW_LEFT,   ///< left part of the statusbar; date is shown there
	SBW_MIDDLE, ///< middle part; current news or company name or *** SAVING *** or *** PAUSED ***
	SBW_RIGHT,  ///< right part; bank balance
};

#endif /* WIDGETS_STATUSBAR_WIDGET_H */
