/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file league_widget.h Types related to the graph widgets. */

#ifndef WIDGETS_LEAGUE_WIDGET_H
#define WIDGETS_LEAGUE_WIDGET_H

/** Widget of the #PerformanceLeagueWindow class. */
enum PerformanceLeagueWidgets : WidgetID {
	WID_PLT_BACKGROUND, ///< Background of the window.
};

/** Widget of the #ScriptLeagueWindow class. */
enum ScriptLeagueWidgets : WidgetID {
	WID_SLT_CAPTION,    ///< Caption of the window.
	WID_SLT_BACKGROUND, ///< Background of the window.
};

#endif /* WIDGETS_LEAGUE_WIDGET_H */
