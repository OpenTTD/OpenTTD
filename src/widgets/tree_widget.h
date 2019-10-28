/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tree_widget.h Types related to the tree widgets. */

#ifndef WIDGETS_TREE_WIDGET_H
#define WIDGETS_TREE_WIDGET_H

/** Widgets of the #BuildTreesWindow class. */
enum BuildTreesWidgets {
	WID_BT_TYPE_11,         ///< Tree 1st column 1st row.
	WID_BT_TYPE_12,         ///< Tree 1st column 2nd row.
	WID_BT_TYPE_13,         ///< Tree 1st column 3rd row.
	WID_BT_TYPE_14,         ///< Tree 1st column 4th row.
	WID_BT_TYPE_21,         ///< Tree 2st column 1st row.
	WID_BT_TYPE_22,         ///< Tree 2st column 2nd row.
	WID_BT_TYPE_23,         ///< Tree 2st column 3rd row.
	WID_BT_TYPE_24,         ///< Tree 2st column 4th row.
	WID_BT_TYPE_31,         ///< Tree 3st column 1st row.
	WID_BT_TYPE_32,         ///< Tree 3st column 2nd row.
	WID_BT_TYPE_33,         ///< Tree 3st column 3rd row.
	WID_BT_TYPE_34,         ///< Tree 3st column 4th row.
	WID_BT_TYPE_RANDOM,     ///< Button to build random type of tree.
	WID_BT_MANY_RANDOM,     ///< Button to build many random trees.
};

#endif /* WIDGETS_TREE_WIDGET_H */
