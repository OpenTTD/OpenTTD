/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy_widget.h Types related to the subsidy widgets. */

#ifndef WIDGETS_SUBSIDY_WIDGET_H
#define WIDGETS_SUBSIDY_WIDGET_H

/** Widgets of the WC_SUBSIDIES_LIST. */
enum SubsidyListWidgets {
	/* Name starts with SU instead of S, becuase of collision with StationListWidgets */
	SULW_PANEL,
	SULW_SCROLLBAR,
};

#endif /* WIDGETS_SUBSIDY_WIDGET_H */
