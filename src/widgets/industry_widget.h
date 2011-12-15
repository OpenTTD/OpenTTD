/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_widget.h Types related to the industry widgets. */

#ifndef WIDGETS_INDUSTRY_WIDGET_H
#define WIDGETS_INDUSTRY_WIDGET_H

/** Widgets of the WC_BUILD_INDUSTRY. */
enum DynamicPlaceIndustriesWidgets {
	DPIW_MATRIX_WIDGET,
	DPIW_SCROLLBAR,
	DPIW_INFOPANEL,
	DPIW_DISPLAY_WIDGET,
	DPIW_FUND_WIDGET,
};

/** Widgets of the WC_INDUSTRY_VIEW. */
enum IndustryViewWidgets {
	IVW_CAPTION,
	IVW_VIEWPORT,
	IVW_INFO,
	IVW_GOTO,
	IVW_DISPLAY,
};

/** Widgets of the WC_INDUSTRY_DIRECTORY. */
enum IndustryDirectoryWidgets {
	IDW_DROPDOWN_ORDER,
	IDW_DROPDOWN_CRITERIA,
	IDW_INDUSTRY_LIST,
	IDW_SCROLLBAR,
};

/** Widgets of the WC_INDUSTRY_CARGOES */
enum IndustryCargoesWidgets {
	ICW_CAPTION,
	ICW_NOTIFY,
	ICW_PANEL,
	ICW_SCROLLBAR,
};


#endif /* WIDGETS_INDUSTRY_WIDGET_H */
