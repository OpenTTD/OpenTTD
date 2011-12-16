/* $Id$ */

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

/** Widgets of the WC_GRAPH_LEGEND. */
enum GraphLegendWidgets {
	WID_GL_BACKGROUND,    ///< Background of the window.

	WID_GL_FIRST_COMPANY, ///< First company in the legend.
	WID_GL_LAST_COMPANY = WID_GL_FIRST_COMPANY + MAX_COMPANIES - 1, ///< Last company in the legend.
};

/** Widgets of the WC_OPERATING_PROFIT / WC_DELIVERED_CARGO / WC_COMPANY_VALUE / WC_INCOME_GRAPH. */
enum CompanyValueWidgets {
	WID_CV_KEY_BUTTON, ///< Key button.
	WID_CV_BACKGROUND, ///< Background of the window.
	WID_CV_GRAPH,      ///< Graph itself.
	WID_CV_RESIZE,     ///< Resize button.
};

/** Widget of the WC_PERFORMANCE_HISTORY. */
enum PerformanceHistoryGraphWidgets {
	WID_PHG_KEY,                  ///< Key button.
	WID_PHG_DETAILED_PERFORMANCE, ///< Detailed performance.
	WID_PHG_BACKGROUND,           ///< Background of the window.
	WID_PHG_GRAPH,                ///< Graph itself.
	WID_PHG_RESIZE,               ///< Resize button.
};

/** Widget of the WC_PAYMENT_RATES. */
enum CargoPaymentRatesWidgets {
	WID_CPR_BACKGROUND,      ///< Background of the window.
	WID_CPR_HEADER,          ///< Header.
	WID_CPR_GRAPH,           ///< Graph itself.
	WID_CPR_RESIZE,          ///< Resize button.
	WID_CPR_FOOTER,          ///< Footer.
	WID_CPR_ENABLE_CARGOES,  ///< Enable cargoes button.
	WID_CPR_DISABLE_CARGOES, ///< Disable cargoes button.
	WID_CPR_CARGO_FIRST,     ///< First cargo in the list.
};

/** Widget of the WC_COMPANY_LEAGUE. */
enum CompanyLeagueWidgets {
	WID_CL_BACKGROUND, ///< Background of the window.
};

/** Widget of the WC_PERFORMANCE_DETAIL. */
enum PerformanceRatingDetailsWidgets {
	WID_PRD_SCORE_FIRST, ///< First entry in the score list.
	WID_PRD_SCORE_LAST = WID_PRD_SCORE_FIRST + (SCORE_END - SCORE_BEGIN) - 1, ///< Last entry in the score list.

	WID_PRD_COMPANY_FIRST, ///< First company.
	WID_PRD_COMPANY_LAST  = WID_PRD_COMPANY_FIRST + MAX_COMPANIES - 1, ///< Last company.
};

#endif /* WIDGETS_GRAPH_WIDGET_H */
