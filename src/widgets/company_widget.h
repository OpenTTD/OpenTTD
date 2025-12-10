/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_widget.h Types related to the company widgets. */

#ifndef WIDGETS_COMPANY_WIDGET_H
#define WIDGETS_COMPANY_WIDGET_H

/** Widgets of the #CompanyWindow class. */
enum CompanyWidgets : WidgetID {
	WID_C_CAPTION,                    ///< Caption of the window.

	WID_C_FACE,                       ///< View of the face.
	WID_C_FACE_TITLE,                 ///< Title for the face.

	WID_C_DESC_INAUGURATION,          ///< Inauguration.
	WID_C_DESC_COLOUR_SCHEME,         ///< Colour scheme.
	WID_C_DESC_COLOUR_SCHEME_EXAMPLE, ///< Colour scheme example.
	WID_C_DESC_VEHICLE,               ///< Vehicles.
	WID_C_DESC_VEHICLE_COUNTS,        ///< Vehicle count.
	WID_C_DESC_COMPANY_VALUE,         ///< Company value.
	WID_C_DESC_INFRASTRUCTURE,        ///< Infrastructure.
	WID_C_DESC_INFRASTRUCTURE_COUNTS, ///< Infrastructure count.

	WID_C_SELECT_BUTTONS,             ///< Selection widget for the button bar.
	WID_C_NEW_FACE,                   ///< Button to make new face.
	WID_C_COLOUR_SCHEME,              ///< Button to change colour scheme.
	WID_C_PRESIDENT_NAME,             ///< Button to change president name.
	WID_C_COMPANY_NAME,               ///< Button to change company name.

	WID_C_SELECT_VIEW_BUILD_HQ,       ///< Panel about HQ.
	WID_C_VIEW_HQ,                    ///< Button to view the HQ.
	WID_C_BUILD_HQ,                   ///< Button to build the HQ.

	WID_C_SELECT_RELOCATE,            ///< Panel about 'Relocate HQ'.
	WID_C_RELOCATE_HQ,                ///< Button to relocate the HQ.

	WID_C_VIEW_INFRASTRUCTURE,        ///< Panel about infrastructure.

	WID_C_SELECT_GIVE_MONEY,          ///< Selection widget for the give money button.
	WID_C_GIVE_MONEY,                 ///< Button to give money.

	WID_C_SELECT_HOSTILE_TAKEOVER,    ///< Selection widget for the hostile takeover button.
	WID_C_HOSTILE_TAKEOVER,           ///< Button to hostile takeover another company.

	WID_C_SELECT_MULTIPLAYER,         ///< Multiplayer selection panel.
	WID_C_COMPANY_JOIN,               ///< Button to join company.
};

/** Widgets of the #CompanyFinancesWindow class. */
enum CompanyFinancesWidgets : WidgetID {
	WID_CF_CAPTION,        ///< Caption of the window.
	WID_CF_TOGGLE_SIZE,    ///< Toggle windows size.
	WID_CF_SEL_PANEL,      ///< Select panel or nothing.
	WID_CF_EXPS_CATEGORY,  ///< Column for expenses category strings.
	WID_CF_EXPS_PRICE1,    ///< Column for year Y-2 expenses.
	WID_CF_EXPS_PRICE2,    ///< Column for year Y-1 expenses.
	WID_CF_EXPS_PRICE3,    ///< Column for year Y expenses.
	WID_CF_TOTAL_PANEL,    ///< Panel for totals.
	WID_CF_SEL_MAXLOAN,    ///< Selection of maxloan column.
	WID_CF_BALANCE_VALUE,  ///< Bank balance value.
	WID_CF_LOAN_VALUE,     ///< Loan.
	WID_CF_BALANCE_LINE,   ///< Available cash.
	WID_CF_OWN_VALUE,      ///< Own funds, not including loan.
	WID_CF_INTEREST_RATE,  ///< Loan interest rate.
	WID_CF_MAXLOAN_VALUE,  ///< Max loan widget.
	WID_CF_SEL_BUTTONS,    ///< Selection of buttons.
	WID_CF_INCREASE_LOAN,  ///< Increase loan.
	WID_CF_REPAY_LOAN,     ///< Decrease loan..
	WID_CF_INFRASTRUCTURE, ///< View company infrastructure.
};


/** Widgets of the #SelectCompanyLiveryWindow class. */
enum SelectCompanyLiveryWidgets : WidgetID {
	WID_SCL_CAPTION,          ///< Caption of window.
	WID_SCL_CLASS_GENERAL,    ///< Class general.
	WID_SCL_CLASS_RAIL,       ///< Class rail.
	WID_SCL_CLASS_ROAD,       ///< Class road.
	WID_SCL_CLASS_SHIP,       ///< Class ship.
	WID_SCL_CLASS_AIRCRAFT,   ///< Class aircraft.
	WID_SCL_GROUPS_RAIL,      ///< Rail groups.
	WID_SCL_GROUPS_ROAD,      ///< Road groups.
	WID_SCL_GROUPS_SHIP,      ///< Ship groups.
	WID_SCL_GROUPS_AIRCRAFT,  ///< Aircraft groups.
	WID_SCL_SPACER_DROPDOWN,  ///< Spacer for dropdown.
	WID_SCL_PRI_COL_DROPDOWN, ///< Dropdown for primary colour.
	WID_SCL_SEC_COL_DROPDOWN, ///< Dropdown for secondary colour.
	WID_SCL_SEC_COL_DROP_SEL, ///< Container for secondary color dropdown, which can be hidden.
	WID_SCL_MATRIX,           ///< Matrix.
	WID_SCL_MATRIX_SCROLLBAR, ///< Matrix scrollbar.
};


/**
 * Widgets of the #SelectCompanyManagerFaceWindow class.
 */
enum SelectCompanyManagerFaceWidgets : WidgetID {
	WID_SCMF_CAPTION,                    ///< Caption of window.
	WID_SCMF_TOGGLE_LARGE_SMALL,         ///< Toggle for large or small.
	WID_SCMF_SELECT_FACE,                ///< Select face.
	WID_SCMF_CANCEL,                     ///< Cancel.
	WID_SCMF_ACCEPT,                     ///< Accept.
	WID_SCMF_SEL_LOADSAVE,               ///< Selection to display the load/save/number buttons in the advanced view.
	WID_SCMF_SEL_PARTS, ///< Selection to display the buttons for setting each part of the face in the advanced view.
	WID_SCMF_SEL_RESIZE, ///< Selection to display the resize button.
	WID_SCMF_RANDOM_NEW_FACE,            ///< Create random new face.
	WID_SCMF_TOGGLE_LARGE_SMALL_BUTTON,  ///< Toggle for large or small.
	WID_SCMF_FACE,                       ///< Current face.
	WID_SCMF_LOAD,                       ///< Load face.
	WID_SCMF_FACECODE,                   ///< Get the face code.
	WID_SCMF_SAVE,                       ///< Save face.
	WID_SCMF_STYLE, ///< Style selector widget.
	WID_SCMF_PARTS, ///< Face configuration parts widget.
	WID_SCMF_PARTS_SCROLLBAR, ///< Scrollbar for configuration parts widget.
};

/** Widgets of the #CompanyInfrastructureWindow class. */
enum CompanyInfrastructureWidgets : WidgetID {
	WID_CI_CAPTION, ///< Caption of window.
	WID_CI_LIST, ///< Infrastructure list.
	WID_CI_SCROLLBAR, ///< Infrastructure list scrollbar.
};

/** Widgets of the #BuyCompanyWindow class. */
enum BuyCompanyWidgets : WidgetID {
	WID_BC_CAPTION,  ///< Caption of window.
	WID_BC_FACE,     ///< Face button.
	WID_BC_QUESTION, ///< Question text.
	WID_BC_NO,       ///< No button.
	WID_BC_YES,      ///< Yes button.
};

#endif /* WIDGETS_COMPANY_WIDGET_H */
