/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_widget.h Types related to the autoreplace widgets. */

#ifndef WIDGETS_AUTOREPLACE_WIDGET_H
#define WIDGETS_AUTOREPLACE_WIDGET_H

/** Widgets of the WC_REPLACE_VEHICLE. */
enum ReplaceVehicleWindowWidgets {
	RVW_WIDGET_CAPTION,

	/* Left and right matrix + details. */
	RVW_WIDGET_LEFT_MATRIX,
	RVW_WIDGET_LEFT_SCROLLBAR,
	RVW_WIDGET_RIGHT_MATRIX,
	RVW_WIDGET_RIGHT_SCROLLBAR,
	RVW_WIDGET_LEFT_DETAILS,
	RVW_WIDGET_RIGHT_DETAILS,

	/* Button row. */
	RVW_WIDGET_START_REPLACE,
	RVW_WIDGET_INFO_TAB,
	RVW_WIDGET_STOP_REPLACE,

	/* Train only widgets. */
	RVW_WIDGET_TRAIN_ENGINEWAGON_TOGGLE,
	RVW_WIDGET_TRAIN_FLUFF_LEFT,
	RVW_WIDGET_TRAIN_RAILTYPE_DROPDOWN,
	RVW_WIDGET_TRAIN_FLUFF_RIGHT,
	RVW_WIDGET_TRAIN_WAGONREMOVE_TOGGLE,
};

#endif /* WIDGETS_AUTOREPLACE_WIDGET_H */
