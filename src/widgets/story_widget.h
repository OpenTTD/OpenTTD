
/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file story_widget.h Types related to the story widgets. */

#ifndef WIDGETS_STORY_WIDGET_H
#define WIDGETS_STORY_WIDGET_H

/** Widgets of the #GoalListWindow class. */
enum StoryBookWidgets : WidgetID {
	WID_SB_CAPTION,   ///< Caption of the window.
	WID_SB_SEL_PAGE,  ///< Page selector.
	WID_SB_PAGE_PANEL,///< Page body.
	WID_SB_SCROLLBAR, ///< Scrollbar of the goal list.
	WID_SB_PREV_PAGE, ///< Prev button.
	WID_SB_NEXT_PAGE, ///< Next button.
};

#endif /* WIDGETS_STORY_WIDGET_H */
