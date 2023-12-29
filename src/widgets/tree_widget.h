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
enum BuildTreesWidgets : WidgetID {
	WID_BT_TYPE_RANDOM,     ///< Button to build random type of tree.
	WID_BT_SE_PANE,         ///< Selection pane to show/hide scenario editor tools.
	WID_BT_MODE_NORMAL,     ///< Select normal/rectangle planting mode.
	WID_BT_MODE_FOREST_SM,  ///< Select small forest planting mode.
	WID_BT_MODE_FOREST_LG,  ///< Select large forest planting mode.
	WID_BT_MANY_RANDOM,     ///< Button to build many random trees.
	WID_BT_TYPE_BUTTON_FIRST, ///< First tree type selection button. (This must be last in the enum.)
};

#endif /* WIDGETS_TREE_WIDGET_H */
