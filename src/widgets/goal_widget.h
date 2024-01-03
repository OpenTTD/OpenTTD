
/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file goal_widget.h Types related to the goal widgets. */

#ifndef WIDGETS_GOAL_WIDGET_H
#define WIDGETS_GOAL_WIDGET_H

/** Widgets of the #GoalListWindow class. */
enum GoalListWidgets : WidgetID {
	WID_GOAL_CAPTION,         ///< Caption of the window.
	WID_GOAL_SELECT_BUTTONS,  ///< Selection widget for the title bar button.
	WID_GOAL_GLOBAL_BUTTON,   ///< Button to show global goals.
	WID_GOAL_COMPANY_BUTTON,  ///< Button to show company goals.
	WID_GOAL_LIST,            ///< Goal list.
	WID_GOAL_SCROLLBAR,       ///< Scrollbar of the goal list.
};

/** Widgets of the #GoalQuestionWindow class. */
enum GoalQuestionWidgets : WidgetID {
	WID_GQ_CAPTION,        ///< Caption of the window.
	WID_GQ_QUESTION,       ///< Question text.
	WID_GQ_BUTTONS,        ///< Buttons selection (between 1, 2 or 3).
	WID_GQ_BUTTON_1,       ///< First button.
	WID_GQ_BUTTON_2,       ///< Second button.
	WID_GQ_BUTTON_3,       ///< Third button.
};

#endif /* WIDGETS_GOAL_WIDGET_H */
