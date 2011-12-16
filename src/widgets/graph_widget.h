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
enum GraphLegendWidgetNumbers {
	GLW_BACKGROUND,

	GLW_FIRST_COMPANY,
	GLW_LAST_COMPANY = GLW_FIRST_COMPANY + MAX_COMPANIES - 1,
};

/** Widgets of the WC_OPERATING_PROFIT / WC_DELIVERED_CARGO / WC_COMPANY_VALUE / WC_INCOME_GRAPH. */
enum CompanyValueWidgets {
	BGW_KEY_BUTTON,
	BGW_BACKGROUND,
	BGW_GRAPH,
	BGW_RESIZE,
};

/** Widget of the WC_PERFORMANCE_HISTORY. */
enum PerformanceHistoryGraphWidgets {
	PHW_KEY,
	PHW_DETAILED_PERFORMANCE,
	PHW_BACKGROUND,
	PHW_GRAPH,
	PHW_RESIZE,
};

/** Widget of the WC_PAYMENT_RATES. */
enum CargoPaymentRatesWidgets {
	CPW_BACKGROUND,
	CPW_HEADER,
	CPW_GRAPH,
	CPW_RESIZE,
	CPW_FOOTER,
	CPW_ENABLE_CARGOES,
	CPW_DISABLE_CARGOES,
	CPW_CARGO_FIRST,
};

/** Widget of the WC_COMPANY_LEAGUE. */
enum CompanyLeagueWidgets {
	CLW_BACKGROUND,
};

/** Widget of the WC_PERFORMANCE_DETAIL. */
enum PerformanceRatingDetailsWidgets {
	PRW_SCORE_FIRST,
	PRW_SCORE_LAST = PRW_SCORE_FIRST + (SCORE_END - SCORE_BEGIN) - 1,

	PRW_COMPANY_FIRST,
	PRW_COMPANY_LAST  = PRW_COMPANY_FIRST + MAX_COMPANIES - 1,
};

#endif /* WIDGETS_GRAPH_WIDGET_H */
