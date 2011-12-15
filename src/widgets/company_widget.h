/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_widget.h Types related to the company widgets. */

#ifndef WIDGETS_COMPANY_WIDGET_H
#define WIDGETS_COMPANY_WIDGET_H

/** Widgets of the WC_COMPANY. */
enum CompanyWindowWidgets {
	CW_WIDGET_CAPTION,

	CW_WIDGET_FACE,
	CW_WIDGET_FACE_TITLE,

	CW_WIDGET_DESC_INAUGURATION,
	CW_WIDGET_DESC_COLOUR_SCHEME,
	CW_WIDGET_DESC_COLOUR_SCHEME_EXAMPLE,
	CW_WIDGET_DESC_VEHICLE,
	CW_WIDGET_DESC_VEHICLE_COUNTS,
	CW_WIDGET_DESC_COMPANY_VALUE,
	CW_WIDGET_DESC_INFRASTRUCTURE,
	CW_WIDGET_DESC_INFRASTRUCTURE_COUNTS,

	CW_WIDGET_SELECT_DESC_OWNERS,
	CW_WIDGET_DESC_OWNERS,

	CW_WIDGET_SELECT_BUTTONS,     ///< Selection widget for the button bar.
	CW_WIDGET_NEW_FACE,
	CW_WIDGET_COLOUR_SCHEME,
	CW_WIDGET_PRESIDENT_NAME,
	CW_WIDGET_COMPANY_NAME,
	CW_WIDGET_BUY_SHARE,
	CW_WIDGET_SELL_SHARE,

	CW_WIDGET_SELECT_VIEW_BUILD_HQ,
	CW_WIDGET_VIEW_HQ,
	CW_WIDGET_BUILD_HQ,

	CW_WIDGET_SELECT_RELOCATE,    ///< View/hide the 'Relocate HQ' button.
	CW_WIDGET_RELOCATE_HQ,

	CW_WIDGET_VIEW_INFRASTRUCTURE,

	CW_WIDGET_HAS_PASSWORD,       ///< Draw a lock when the company has a password
	CW_WIDGET_SELECT_MULTIPLAYER, ///< Multiplayer selection panel.
	CW_WIDGET_COMPANY_PASSWORD,
	CW_WIDGET_COMPANY_JOIN,
};

/** Widgets of the WC_FINANCES. */
enum CompanyFinancesWindowWidgets {
	CFW_CAPTION,       ///< Caption of the window
	CFW_TOGGLE_SIZE,   ///< Toggle windows size
	CFW_SEL_PANEL,     ///< Select panel or nothing
	CFW_EXPS_CATEGORY, ///< Column for expenses category strings
	CFW_EXPS_PRICE1,   ///< Column for year Y-2 expenses
	CFW_EXPS_PRICE2,   ///< Column for year Y-1 expenses
	CFW_EXPS_PRICE3,   ///< Column for year Y expenses
	CFW_TOTAL_PANEL,   ///< Panel for totals
	CFW_SEL_MAXLOAN,   ///< Selection of maxloan column
	CFW_BALANCE_VALUE, ///< Bank balance value
	CFW_LOAN_VALUE,    ///< Loan
	CFW_LOAN_LINE,     ///< Line for summing bank balance and loan
	CFW_TOTAL_VALUE,   ///< Total
	CFW_MAXLOAN_GAP,   ///< Gap above max loan widget
	CFW_MAXLOAN_VALUE, ///< Max loan widget
	CFW_SEL_BUTTONS,   ///< Selection of buttons
	CFW_INCREASE_LOAN, ///< Increase loan
	CFW_REPAY_LOAN,    ///< Decrease loan
	CFW_INFRASTRUCTURE,///< View company infrastructure
};


/** Widgets of the WC_COMPANY_COLOUR. */
enum SelectCompanyLiveryWindowWidgets {
	SCLW_WIDGET_CAPTION,
	SCLW_WIDGET_CLASS_GENERAL,
	SCLW_WIDGET_CLASS_RAIL,
	SCLW_WIDGET_CLASS_ROAD,
	SCLW_WIDGET_CLASS_SHIP,
	SCLW_WIDGET_CLASS_AIRCRAFT,
	SCLW_WIDGET_SPACER_DROPDOWN,
	SCLW_WIDGET_PRI_COL_DROPDOWN,
	SCLW_WIDGET_SEC_COL_DROPDOWN,
	SCLW_WIDGET_MATRIX,
};


/** Widgets of the WC_COMPANY_MANAGER_FACE.
 * Do not change the order of the widgets from SCMFW_WIDGET_HAS_MOUSTACHE_EARRING to SCMFW_WIDGET_GLASSES_R,
 * this order is needed for the WE_CLICK event of DrawFaceStringLabel().
 */
enum SelectCompanyManagerFaceWidgets {
	SCMFW_WIDGET_CAPTION,
	SCMFW_WIDGET_TOGGLE_LARGE_SMALL,
	SCMFW_WIDGET_SELECT_FACE,
	SCMFW_WIDGET_CANCEL,
	SCMFW_WIDGET_ACCEPT,
	SCMFW_WIDGET_MALE,           ///< Male button in the simple view.
	SCMFW_WIDGET_FEMALE,         ///< Female button in the simple view.
	SCMFW_WIDGET_MALE2,          ///< Male button in the advanced view.
	SCMFW_WIDGET_FEMALE2,        ///< Female button in the advanced view.
	SCMFW_WIDGET_SEL_LOADSAVE,   ///< Selection to display the load/save/number buttons in the advanced view.
	SCMFW_WIDGET_SEL_MALEFEMALE, ///< Selection to display the male/female buttons in the simple view.
	SCMFW_WIDGET_SEL_PARTS,      ///< Selection to display the buttons for setting each part of the face in the advanced view.
	SCMFW_WIDGET_RANDOM_NEW_FACE,
	SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON,
	SCMFM_WIDGET_FACE,
	SCMFW_WIDGET_LOAD,
	SCMFW_WIDGET_FACECODE,
	SCMFW_WIDGET_SAVE,
	SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT,
	SCMFW_WIDGET_TIE_EARRING_TEXT,
	SCMFW_WIDGET_LIPS_MOUSTACHE_TEXT,
	SCMFW_WIDGET_HAS_GLASSES_TEXT,
	SCMFW_WIDGET_HAIR_TEXT,
	SCMFW_WIDGET_EYEBROWS_TEXT,
	SCMFW_WIDGET_EYECOLOUR_TEXT,
	SCMFW_WIDGET_GLASSES_TEXT,
	SCMFW_WIDGET_NOSE_TEXT,
	SCMFW_WIDGET_CHIN_TEXT,
	SCMFW_WIDGET_JACKET_TEXT,
	SCMFW_WIDGET_COLLAR_TEXT,
	SCMFW_WIDGET_ETHNICITY_EUR,
	SCMFW_WIDGET_ETHNICITY_AFR,
	SCMFW_WIDGET_HAS_MOUSTACHE_EARRING,
	SCMFW_WIDGET_HAS_GLASSES,
	SCMFW_WIDGET_EYECOLOUR_L,
	SCMFW_WIDGET_EYECOLOUR,
	SCMFW_WIDGET_EYECOLOUR_R,
	SCMFW_WIDGET_CHIN_L,
	SCMFW_WIDGET_CHIN,
	SCMFW_WIDGET_CHIN_R,
	SCMFW_WIDGET_EYEBROWS_L,
	SCMFW_WIDGET_EYEBROWS,
	SCMFW_WIDGET_EYEBROWS_R,
	SCMFW_WIDGET_LIPS_MOUSTACHE_L,
	SCMFW_WIDGET_LIPS_MOUSTACHE,
	SCMFW_WIDGET_LIPS_MOUSTACHE_R,
	SCMFW_WIDGET_NOSE_L,
	SCMFW_WIDGET_NOSE,
	SCMFW_WIDGET_NOSE_R,
	SCMFW_WIDGET_HAIR_L,
	SCMFW_WIDGET_HAIR,
	SCMFW_WIDGET_HAIR_R,
	SCMFW_WIDGET_JACKET_L,
	SCMFW_WIDGET_JACKET,
	SCMFW_WIDGET_JACKET_R,
	SCMFW_WIDGET_COLLAR_L,
	SCMFW_WIDGET_COLLAR,
	SCMFW_WIDGET_COLLAR_R,
	SCMFW_WIDGET_TIE_EARRING_L,
	SCMFW_WIDGET_TIE_EARRING,
	SCMFW_WIDGET_TIE_EARRING_R,
	SCMFW_WIDGET_GLASSES_L,
	SCMFW_WIDGET_GLASSES,
	SCMFW_WIDGET_GLASSES_R,
};

/** Widgets of the WC_COMPANY_INFRASTRUCTURE. */
enum CompanyInfrastructureWindowWidgets {
	CIW_WIDGET_CAPTION,
	CIW_WIDGET_RAIL_DESC,
	CIW_WIDGET_RAIL_COUNT,
	CIW_WIDGET_ROAD_DESC,
	CIW_WIDGET_ROAD_COUNT,
	CIW_WIDGET_WATER_DESC,
	CIW_WIDGET_WATER_COUNT,
	CIW_WIDGET_STATION_DESC,
	CIW_WIDGET_STATION_COUNT,
	CIW_WIDGET_TOTAL_DESC,
	CIW_WIDGET_TOTAL,
};

/** Widgets of the WC_BUY_COMPANY. */
enum BuyCompanyWidgets {
	BCW_CAPTION,
	BCW_FACE,
	BCW_QUESTION,
	BCW_NO,
	BCW_YES,
};

#endif /* WIDGETS_COMPANY_WIDGET_H */
