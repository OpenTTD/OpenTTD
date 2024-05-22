/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file graph_widget.h Types related to the graph widgets. */

#ifndef WIDGETS_GRAPH_WIDGET_H
#define WIDGETS_GRAPH_WIDGET_H

#include "../economy_type.h"
#include "../company_type.h"

/** Widgets of the #GraphLegendWindow class. */
enum GraphLegendWidgets : WidgetID {
	WID_GL_BACKGROUND,    ///< Background of the window.

	WID_GL_FIRST_COMPANY, ///< First company in the legend.
	WID_GL_LAST_COMPANY = WID_GL_FIRST_COMPANY + MAX_COMPANIES - 1, ///< Last company in the legend.
};

/** Widgets of the #BaseGraphWindow class and derived classes. */
enum GraphWidgets : WidgetID {
	WID_GRAPH_KEY_BUTTON, ///< Key button.
	WID_GRAPH_BACKGROUND, ///< Background of the window.
	WID_GRAPH_GRAPH,      ///< Graph itself.
	WID_GRAPH_RESIZE,     ///< Resize button.
	WID_GRAPH_HEADER,     ///< Header.
	WID_GRAPH_FOOTER,     ///< Footer.

	WID_PHG_DETAILED_PERFORMANCE, ///< Detailed performance.

	WID_CPR_ENABLE_CARGOES,  ///< Enable cargoes button.
	WID_CPR_DISABLE_CARGOES, ///< Disable cargoes button.
	WID_CPR_MATRIX,          ///< Cargo list.
	WID_CPR_MATRIX_SCROLLBAR,///< Cargo list scrollbar.
};

/** Widget of the #PerformanceRatingDetailWindow class. */
enum PerformanceRatingDetailsWidgets : WidgetID {
	WID_PRD_SCORE_FIRST, ///< First entry in the score list.
	WID_PRD_SCORE_LAST = WID_PRD_SCORE_FIRST + (SCORE_END - SCORE_BEGIN) - 1, ///< Last entry in the score list.

	WID_PRD_COMPANY_FIRST, ///< First company.
	WID_PRD_COMPANY_LAST  = WID_PRD_COMPANY_FIRST + MAX_COMPANIES - 1, ///< Last company.
};

#endif /* WIDGETS_GRAPH_WIDGET_H */
