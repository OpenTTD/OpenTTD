/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_widget.h Types related to the group widgets. */

#ifndef WIDGETS_GROUP_WIDGET_H
#define WIDGETS_GROUP_WIDGET_H

/** Widgets of the WC_TRAINS_LIST / WC_ROADVEH_LIST / WC_SHIPS_LIST / WC_AIRCRAFT_LIST. */
enum GroupListWidgets {
	GRP_WIDGET_CAPTION,
	GRP_WIDGET_SORT_BY_ORDER,
	GRP_WIDGET_SORT_BY_DROPDOWN,
	GRP_WIDGET_LIST_VEHICLE,
	GRP_WIDGET_LIST_VEHICLE_SCROLLBAR,
	GRP_WIDGET_AVAILABLE_VEHICLES,
	GRP_WIDGET_MANAGE_VEHICLES_DROPDOWN,
	GRP_WIDGET_STOP_ALL,
	GRP_WIDGET_START_ALL,

	GRP_WIDGET_ALL_VEHICLES,
	GRP_WIDGET_DEFAULT_VEHICLES,
	GRP_WIDGET_LIST_GROUP,
	GRP_WIDGET_LIST_GROUP_SCROLLBAR,
	GRP_WIDGET_CREATE_GROUP,
	GRP_WIDGET_DELETE_GROUP,
	GRP_WIDGET_RENAME_GROUP,
	GRP_WIDGET_REPLACE_PROTECTION,
};

#endif /* WIDGETS_GROUP_WIDGET_H */
