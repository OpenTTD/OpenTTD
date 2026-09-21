/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoreplace_widget.h Types related to the autoreplace widgets. */

#ifndef WIDGETS_AUTOREPLACE_WIDGET_H
#define WIDGETS_AUTOREPLACE_WIDGET_H

/** Widgets of the #ReplaceVehicleWindow class. */
enum ReplaceVehicleWidgets : WidgetID {
	WID_RV_CAPTION,                  ///< Caption of the window.

	/* Sort dropdown at the right. */
	WID_RV_SORT_ASCENDING_DESCENDING, ///< Ascending/descending sort order button.
	WID_RV_SHOW_HIDDEN_ENGINES,       ///< Toggle whether to display the hidden vehicles.
	WID_RV_SORT_DROPDOWN,             ///< Dropdown for the sort criteria.

	/* Left and right matrix + details. */
	WID_RV_LEFT_MATRIX,              ///< The matrix on the left.
	WID_RV_LEFT_SCROLLBAR,           ///< The scrollbar for the matrix on the left.
	WID_RV_RIGHT_MATRIX,             ///< The matrix on the right.
	WID_RV_RIGHT_SCROLLBAR,          ///< The scrollbar for the matrix on the right.
	WID_RV_LEFT_DETAILS,             ///< Details of the entry on the left.
	WID_RV_RIGHT_DETAILS,            ///< Details of the entry on the right.

	/* Button row. */
	WID_RV_START_REPLACE,            ///< Start Replacing button.
	WID_RV_INFO_TAB,                 ///< Info tab.
	WID_RV_STOP_REPLACE,             ///< Stop Replacing button.

	/* Train only widgets. */
	WID_RV_RAIL_TYPE_DROPDOWN, ///< Dropdown to select railtype.
	WID_RV_TRAIN_ENGINEWAGON_DROPDOWN, ///< Dropdown to select engines and/or wagons.
	WID_RV_TRAIN_WAGONREMOVE_TOGGLE, ///< Button to toggle removing wagons.
	WID_RV_MASS_UPGRADE, ///< Button to open the mass railtype upgrade window.

	/* Road only widgets. */
	WID_RV_ROAD_TYPE_DROPDOWN, ///< Dropdown to select roadtype.
};

/** Widgets of the #MassUpgradeWindow class. */
enum MassUpgradeWidgets : WidgetID {
	WID_MU_CAPTION, ///< Caption of the window.

	/* Controls row. */
	WID_MU_GROUP_DROPDOWN, ///< Dropdown to select the group of trains to upgrade.
	WID_MU_RAIL_TYPE_DROPDOWN, ///< Dropdown to filter the upgrade target list by rail type.
	WID_MU_SORT_ASCENDING_DESCENDING, ///< Ascending/descending sort order button.
	WID_MU_SORT_DROPDOWN, ///< Dropdown for the sort criteria.
	WID_MU_SHOW_HIDDEN_ENGINES, ///< Toggle whether to display the hidden vehicles.

	/* Left and right matrix + details. */
	WID_MU_LEFT_MATRIX, ///< The matrix of locomotives currently in use by the group, on the left.
	WID_MU_LEFT_SCROLLBAR, ///< The scrollbar for the matrix on the left.
	WID_MU_RIGHT_MATRIX, ///< The matrix of upgrade target locomotives, on the right.
	WID_MU_RIGHT_SCROLLBAR, ///< The scrollbar for the matrix on the right.
	WID_MU_LEFT_DETAILS, ///< Details of the entry on the left.
	WID_MU_RIGHT_DETAILS, ///< Details of the entry on the right.

	/* Bottom row. */
	WID_MU_STATUS, ///< Status line showing how many trains are in depot.
	WID_MU_SEND_TO_DEPOT, ///< Button to send the selected group of trains to depot.
	WID_MU_UPGRADE, ///< Button to perform the upgrade.
};

#endif /* WIDGETS_AUTOREPLACE_WIDGET_H */
