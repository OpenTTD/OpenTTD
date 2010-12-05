/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file company_gui.cpp Company related GUIs. */

#include "stdafx.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "company_func.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"
#include "vehicle_func.h"
#include "newgrf.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "date_func.h"
#include "widgets/dropdown_type.h"
#include "tilehighlight_func.h"
#include "sprite.h"
#include "company_base.h"
#include "core/geometry_func.hpp"
#include "economy_func.h"
#include "object_type.h"

#include "table/strings.h"

/** Company GUI constants. */
static const uint EXP_LINESPACE  = 2;      ///< Amount of vertical space for a horizontal (sub-)total line.
static const uint EXP_BLOCKSPACE = 10;     ///< Amount of vertical space between two blocks of numbers.

static void DoSelectCompanyManagerFace(Window *parent);

/** Standard unsorted list of expenses. */
static ExpensesType _expenses_list_1[] = {
	EXPENSES_CONSTRUCTION,
	EXPENSES_NEW_VEHICLES,
	EXPENSES_TRAIN_RUN,
	EXPENSES_ROADVEH_RUN,
	EXPENSES_AIRCRAFT_RUN,
	EXPENSES_SHIP_RUN,
	EXPENSES_PROPERTY,
	EXPENSES_TRAIN_INC,
	EXPENSES_ROADVEH_INC,
	EXPENSES_AIRCRAFT_INC,
	EXPENSES_SHIP_INC,
	EXPENSES_LOAN_INT,
	EXPENSES_OTHER,
};

/** Grouped list of expenses. */
static ExpensesType _expenses_list_2[] = {
	EXPENSES_TRAIN_INC,
	EXPENSES_ROADVEH_INC,
	EXPENSES_AIRCRAFT_INC,
	EXPENSES_SHIP_INC,
	INVALID_EXPENSES,
	EXPENSES_TRAIN_RUN,
	EXPENSES_ROADVEH_RUN,
	EXPENSES_AIRCRAFT_RUN,
	EXPENSES_SHIP_RUN,
	EXPENSES_PROPERTY,
	EXPENSES_LOAN_INT,
	INVALID_EXPENSES,
	EXPENSES_CONSTRUCTION,
	EXPENSES_NEW_VEHICLES,
	EXPENSES_OTHER,
	INVALID_EXPENSES,
};

/** Expense list container. */
struct ExpensesList {
	const ExpensesType *et;   ///< Expenses items.
	const uint length;        ///< Number of items in list.
	const uint num_subtotals; ///< Number of sub-totals in the list.

	ExpensesList(ExpensesType *et, int length, int num_subtotals) : et(et), length(length), num_subtotals(num_subtotals)
	{
	}

	uint GetHeight() const
	{
		/* heading + line + texts of expenses + sub-totals + total line + total text */
		return FONT_HEIGHT_NORMAL + EXP_LINESPACE + this->length * FONT_HEIGHT_NORMAL + num_subtotals * (EXP_BLOCKSPACE + EXP_LINESPACE) + EXP_LINESPACE + FONT_HEIGHT_NORMAL;
	}

	/** Compute width of the expenses categories in pixels. */
	uint GetCategoriesWidth() const
	{
		uint width = 0;
		bool invalid_expenses_measured = false; // Measure 'Total' width only once.
		for (uint i = 0; i < this->length; i++) {
			ExpensesType et = this->et[i];
			if (et == INVALID_EXPENSES) {
				if (!invalid_expenses_measured) {
					width = max(width, GetStringBoundingBox(STR_FINANCES_TOTAL_CAPTION).width);
					invalid_expenses_measured = true;
				}
			} else {
				width = max(width, GetStringBoundingBox(STR_FINANCES_SECTION_CONSTRUCTION + et).width);
			}
		}
		return width;
	}
};

static const ExpensesList _expenses_list_types[] = {
	ExpensesList(_expenses_list_1, lengthof(_expenses_list_1), 0),
	ExpensesList(_expenses_list_2, lengthof(_expenses_list_2), 3),
};

/** Widgets of the company finances windows. */
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
};

/**
 * Draw the expenses categories.
 * @param r Available space for drawing.
 * @note The environment must provide padding at the left and right of \a r.
 */
static void DrawCategories(const Rect &r)
{
	int y = r.top;

	DrawString(r.left, r.right, y, STR_FINANCES_EXPENDITURE_INCOME_TITLE, TC_FROMSTRING, SA_HOR_CENTER, true);
	y += FONT_HEIGHT_NORMAL + EXP_LINESPACE;

	int type = _settings_client.gui.expenses_layout;
	for (uint i = 0; i < _expenses_list_types[type].length; i++) {
		const ExpensesType et = _expenses_list_types[type].et[i];
		if (et == INVALID_EXPENSES) {
			y += EXP_LINESPACE;
			DrawString(r.left, r.right, y, STR_FINANCES_TOTAL_CAPTION, TC_FROMSTRING, SA_RIGHT);
			y += FONT_HEIGHT_NORMAL + EXP_BLOCKSPACE;
		} else {
			DrawString(r.left, r.right, y, STR_FINANCES_SECTION_CONSTRUCTION + et);
			y += FONT_HEIGHT_NORMAL;
		}
	}

	DrawString(r.left, r.right, y + EXP_LINESPACE, STR_FINANCES_TOTAL_CAPTION, TC_FROMSTRING, SA_RIGHT);
}

/**
 * Draw an amount of money.
 * @param amount Amount of money to draw,
 * @param left   Left coordinate of the space to draw in.
 * @param right  Right coordinate of the space to draw in.
 * @param top    Top coordinate of the space to draw in.
 */
static void DrawPrice(Money amount, int left, int right, int top)
{
	StringID str = STR_FINANCES_NEGATIVE_INCOME;
	if (amount < 0) {
		amount = -amount;
		str++;
	}
	SetDParam(0, amount);
	DrawString(left, right, top, str, TC_FROMSTRING, SA_RIGHT);
}

/**
 * Draw a column with prices.
 * @param r    Available space for drawing.
 * @param year Year being drawn.
 * @param tbl  Pointer to table of amounts for \a year.
 * @note The environment must provide padding at the left and right of \a r.
 */
static void DrawYearColumn(const Rect &r, int year, const Money (*tbl)[EXPENSES_END])
{
	int y = r.top;

	SetDParam(0, year);
	DrawString(r.left, r.right, y, STR_FINANCES_YEAR, TC_FROMSTRING, SA_RIGHT, true);
	y += FONT_HEIGHT_NORMAL + EXP_LINESPACE;

	Money sum = 0;
	Money subtotal = 0;
	int type = _settings_client.gui.expenses_layout;
	for (uint i = 0; i < _expenses_list_types[type].length; i++) {
		const ExpensesType et = _expenses_list_types[type].et[i];
		if (et == INVALID_EXPENSES) {
			Money cost = subtotal;
			subtotal = 0;
			GfxFillRect(r.left, y, r.right, y, 215);
			y += EXP_LINESPACE;
			DrawPrice(cost, r.left, r.right, y);
			y += FONT_HEIGHT_NORMAL + EXP_BLOCKSPACE;
		} else {
			Money cost = (*tbl)[et];
			subtotal += cost;
			sum += cost;
			if (cost != 0) DrawPrice(cost, r.left, r.right, y);
			y += FONT_HEIGHT_NORMAL;
		}
	}

	GfxFillRect(r.left, y, r.right, y, 215);
	y += EXP_LINESPACE;
	DrawPrice(sum, r.left, r.right, y);
}

static const NWidgetPart _nested_company_finances_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, CFW_CAPTION), SetDataTip(STR_FINANCES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, CFW_TOGGLE_SIZE), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_TOOLTIP_TOGGLE_LARGE_SMALL_WINDOW),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, CFW_SEL_PANEL),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT), SetPIP(0, 9, 0),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_CATEGORY), SetMinimalSize(120, 0), SetFill(0, 0),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_PRICE1), SetMinimalSize(86, 0), SetFill(0, 0),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_PRICE2), SetMinimalSize(86, 0), SetFill(0, 0),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_PRICE3), SetMinimalSize(86, 0), SetFill(0, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT),
			NWidget(NWID_VERTICAL), // Vertical column with 'bank balance', 'loan'
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_FINANCES_BANK_BALANCE_TITLE, STR_NULL), SetFill(1, 0),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_FINANCES_LOAN_TITLE, STR_NULL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetFill(0, 0), SetMinimalSize(30, 0),
			NWidget(NWID_VERTICAL), // Vertical column with bank balance amount, loan amount, and total.
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_BALANCE_VALUE), SetDataTip(STR_NULL, STR_NULL),
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_LOAN_VALUE), SetDataTip(STR_NULL, STR_NULL),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_LOAN_LINE), SetMinimalSize(0, 2), SetFill(1, 0),
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_TOTAL_VALUE), SetDataTip(STR_NULL, STR_NULL),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, CFW_SEL_MAXLOAN),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_SPACER), SetFill(0, 1), SetMinimalSize(25, 0),
					NWidget(NWID_VERTICAL), // Max loan information
						NWidget(WWT_EMPTY, COLOUR_GREY, CFW_MAXLOAN_GAP), SetFill(0, 0),
						NWidget(WWT_TEXT, COLOUR_GREY, CFW_MAXLOAN_VALUE), SetDataTip(STR_FINANCES_MAX_LOAN, STR_NULL),
						NWidget(NWID_SPACER), SetFill(0, 1),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, CFW_SEL_BUTTONS),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CFW_INCREASE_LOAN), SetFill(1, 0), SetDataTip(STR_FINANCES_BORROW_BUTTON, STR_FINANCES_BORROW_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CFW_REPAY_LOAN), SetFill(1, 0), SetDataTip(STR_FINANCES_REPAY_BUTTON, STR_FINANCES_REPAY_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

/**
 * Window class displaying the company finances.
 * @todo #money_width should be calculated dynamically.
 */
struct CompanyFinancesWindow : Window {
	static Money max_money; ///< The maximum amount of money a company has had this 'run'
	bool small;             ///< Window is toggled to 'small'.

	CompanyFinancesWindow(const WindowDesc *desc, CompanyID company) : Window()
	{
		this->small = false;
		this->CreateNestedTree(desc);
		this->SetupWidgets();
		this->FinishInitNested(desc, company);

		this->owner = (Owner)this->window_number;
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case CFW_CAPTION:
				SetDParam(0, (CompanyID)this->window_number);
				SetDParam(1, (CompanyID)this->window_number);
				break;

			case CFW_MAXLOAN_VALUE:
				SetDParam(0, _economy.max_loan);
				break;

			case CFW_INCREASE_LOAN:
			case CFW_REPAY_LOAN:
				SetDParam(0, LOAN_INTERVAL);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		int type = _settings_client.gui.expenses_layout;
		switch (widget) {
			case CFW_EXPS_CATEGORY:
				size->width  = _expenses_list_types[type].GetCategoriesWidth();
				size->height = _expenses_list_types[type].GetHeight();
				break;

			case CFW_EXPS_PRICE1:
			case CFW_EXPS_PRICE2:
			case CFW_EXPS_PRICE3:
				size->height = _expenses_list_types[type].GetHeight();
				/* FALL THROUGH */
			case CFW_BALANCE_VALUE:
			case CFW_LOAN_VALUE:
			case CFW_TOTAL_VALUE:
				SetDParam(0, CompanyFinancesWindow::max_money);
				size->width = max(GetStringBoundingBox(STR_FINANCES_NEGATIVE_INCOME).width, GetStringBoundingBox(STR_FINANCES_POSITIVE_INCOME).width) + padding.width;
				break;

			case CFW_MAXLOAN_GAP:
				size->height = FONT_HEIGHT_NORMAL;
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case CFW_EXPS_CATEGORY:
				DrawCategories(r);
				break;

			case CFW_EXPS_PRICE1:
			case CFW_EXPS_PRICE2:
			case CFW_EXPS_PRICE3: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				int age = min(_cur_year - c->inaugurated_year, 2);
				int wid_offset = widget - CFW_EXPS_PRICE1;
				if (wid_offset <= age) {
					DrawYearColumn(r, _cur_year - (age - wid_offset), c->yearly_expenses + (age - wid_offset));
				}
				break;
			}

			case CFW_BALANCE_VALUE: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				SetDParam(0, c->money);
				DrawString(r.left, r.right, r.top, STR_FINANCES_TOTAL_CURRENCY, TC_FROMSTRING, SA_RIGHT);
				break;
			}

			case CFW_LOAN_VALUE: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				SetDParam(0, c->current_loan);
				DrawString(r.left, r.right, r.top, STR_FINANCES_TOTAL_CURRENCY, TC_FROMSTRING, SA_RIGHT);
				break;
			}

			case CFW_TOTAL_VALUE: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				SetDParam(0, c->money - c->current_loan);
				DrawString(r.left, r.right, r.top, STR_FINANCES_TOTAL_CURRENCY, TC_FROMSTRING, SA_RIGHT);
				break;
			}

			case CFW_LOAN_LINE:
				GfxFillRect(r.left, r.top, r.right, r.top, 215);
				break;
		}
	}

	/**
	 * Setup the widgets in the nested tree, such that the finances window is displayed properly.
	 * @note After setup, the window must be (re-)initialized.
	 */
	void SetupWidgets()
	{
		int plane = this->small ? SZSP_NONE : 0;
		this->GetWidget<NWidgetStacked>(CFW_SEL_PANEL)->SetDisplayedPlane(plane);
		this->GetWidget<NWidgetStacked>(CFW_SEL_MAXLOAN)->SetDisplayedPlane(plane);

		CompanyID company = (CompanyID)this->window_number;
		plane = (company != _local_company) ? SZSP_NONE : 0;
		this->GetWidget<NWidgetStacked>(CFW_SEL_BUTTONS)->SetDisplayedPlane(plane);
	}

	virtual void OnPaint()
	{
		if (!this->IsShaded()) {
			if (!this->small) {
				/* Check that the expenses panel height matches the height needed for the layout. */
				int type = _settings_client.gui.expenses_layout;
				if (_expenses_list_types[type].GetHeight() != this->GetWidget<NWidgetBase>(CFW_EXPS_CATEGORY)->current_y) {
					this->SetupWidgets();
					this->ReInit();
					return;
				}
			}

			/* Check that the loan buttons are shown only when the user owns the company. */
			CompanyID company = (CompanyID)this->window_number;
			int req_plane = (company != _local_company) ? SZSP_NONE : 0;
			if (req_plane != this->GetWidget<NWidgetStacked>(CFW_SEL_BUTTONS)->shown_plane) {
				this->SetupWidgets();
				this->ReInit();
				return;
			}

			const Company *c = Company::Get(company);
			this->SetWidgetDisabledState(CFW_INCREASE_LOAN, c->current_loan == _economy.max_loan); // Borrow button only shows when there is any more money to loan.
			this->SetWidgetDisabledState(CFW_REPAY_LOAN, company != _local_company || c->current_loan == 0); // Repay button only shows when there is any more money to repay.
		}

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case CFW_TOGGLE_SIZE: // toggle size
				this->small = !this->small;
				this->SetupWidgets();
				if (this->IsShaded()) {
					/* Finances window is not resizable, so size hints given during unshading have no effect
					 * on the changed appearance of the window. */
					this->SetShaded(false);
				} else {
					this->ReInit();
				}
				break;

			case CFW_INCREASE_LOAN: // increase loan
				DoCommandP(0, 0, _ctrl_pressed, CMD_INCREASE_LOAN | CMD_MSG(STR_ERROR_CAN_T_BORROW_ANY_MORE_MONEY));
				break;

			case CFW_REPAY_LOAN: // repay loan
				DoCommandP(0, 0, _ctrl_pressed, CMD_DECREASE_LOAN | CMD_MSG(STR_ERROR_CAN_T_REPAY_LOAN));
				break;
		}
	}

	virtual void OnHundredthTick()
	{
		const Company *c = Company::Get((CompanyID)this->window_number);
		if (c->money > CompanyFinancesWindow::max_money) {
			CompanyFinancesWindow::max_money = max(c->money * 2, CompanyFinancesWindow::max_money * 4);
			this->SetupWidgets();
			this->ReInit();
		}
	}
};

/** First conservative estimate of the maximum amount of money */
Money CompanyFinancesWindow::max_money = INT32_MAX;

static const WindowDesc _company_finances_desc(
	WDP_AUTO, 0, 0,
	WC_FINANCES, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_company_finances_widgets, lengthof(_nested_company_finances_widgets)
);

/**
 * Open the finances window of a company.
 * @param company Company to show finances of.
 * @pre is company a valid company.
 */
void ShowCompanyFinances(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	if (BringWindowToFrontById(WC_FINANCES, company)) return;

	new CompanyFinancesWindow(&_company_finances_desc, company);
}

/* List of colours for the livery window */
static const StringID _colour_dropdown[] = {
	STR_COLOUR_DARK_BLUE,
	STR_COLOUR_PALE_GREEN,
	STR_COLOUR_PINK,
	STR_COLOUR_YELLOW,
	STR_COLOUR_RED,
	STR_COLOUR_LIGHT_BLUE,
	STR_COLOUR_GREEN,
	STR_COLOUR_DARK_GREEN,
	STR_COLOUR_BLUE,
	STR_COLOUR_CREAM,
	STR_COLOUR_MAUVE,
	STR_COLOUR_PURPLE,
	STR_COLOUR_ORANGE,
	STR_COLOUR_BROWN,
	STR_COLOUR_GREY,
	STR_COLOUR_WHITE,
};

/* Association of liveries to livery classes */
static const LiveryClass _livery_class[LS_END] = {
	LC_OTHER,
	LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL,
	LC_ROAD, LC_ROAD,
	LC_SHIP, LC_SHIP,
	LC_AIRCRAFT, LC_AIRCRAFT, LC_AIRCRAFT,
	LC_ROAD, LC_ROAD,
};

class DropDownListColourItem : public DropDownListItem {
public:
	DropDownListColourItem(int result, bool masked) : DropDownListItem(result, masked) {}

	virtual ~DropDownListColourItem() {}

	StringID String() const
	{
		return _colour_dropdown[this->result];
	}

	uint Height(uint width) const
	{
		return max(FONT_HEIGHT_NORMAL, (byte)14);
	}

	bool Selectable() const
	{
		return true;
	}

	void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		DrawSprite(SPR_VEH_BUS_SIDE_VIEW, PALETTE_RECOLOUR_START + this->result, rtl ? right - 16 : left + 16, top + 7);
		DrawString(rtl ? left + 2 : left + 32, rtl ? right - 32 : right - 2, top + max(0, 13 - FONT_HEIGHT_NORMAL), this->String(), sel ? TC_WHITE : TC_BLACK);
	}
};

/** Widgets of the select company livery window. */
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

/** Company livery colour scheme window. */
struct SelectCompanyLiveryWindow : public Window {
private:
	static const uint TEXT_INDENT = 15; ///< Number of pixels to indent the text in each column in the #SCLW_WIDGET_MATRIX to make room for the (coloured) rectangles.
	uint32 sel;
	LiveryClass livery_class;

	void ShowColourDropDownMenu(uint32 widget)
	{
		uint32 used_colours = 0;
		const Livery *livery;
		LiveryScheme scheme;

		/* Disallow other company colours for the primary colour */
		if (HasBit(this->sel, LS_DEFAULT) && widget == SCLW_WIDGET_PRI_COL_DROPDOWN) {
			const Company *c;
			FOR_ALL_COMPANIES(c) {
				if (c->index != _local_company) SetBit(used_colours, c->colour);
			}
		}

		/* Get the first selected livery to use as the default dropdown item */
		for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
			if (HasBit(this->sel, scheme)) break;
		}
		if (scheme == LS_END) scheme = LS_DEFAULT;
		livery = &Company::Get((CompanyID)this->window_number)->livery[scheme];

		DropDownList *list = new DropDownList();
		for (uint i = 0; i < lengthof(_colour_dropdown); i++) {
			list->push_back(new DropDownListColourItem(i, HasBit(used_colours, i)));
		}

		ShowDropDownList(this, list, widget == SCLW_WIDGET_PRI_COL_DROPDOWN ? livery->colour1 : livery->colour2, widget);
	}

public:
	SelectCompanyLiveryWindow(const WindowDesc *desc, CompanyID company) : Window()
	{
		this->livery_class = LC_OTHER;
		this->sel = 1;
		this->InitNested(desc, company);
		this->owner = company;
		this->LowerWidget(SCLW_WIDGET_CLASS_GENERAL);
		this->InvalidateData(1);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case SCLW_WIDGET_SPACER_DROPDOWN: {
				/* The matrix widget below needs enough room to print all the schemes. */
				Dimension d = {0, 0};
				for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					d = maxdim(d, GetStringBoundingBox(STR_LIVERY_DEFAULT + scheme));
				}
				size->width = max(size->width, TEXT_INDENT + d.width + WD_FRAMERECT_RIGHT);
				break;
			}

			case SCLW_WIDGET_MATRIX: {
				uint livery_height = 0;
				for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					if (_livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
						livery_height++;
					}
				}
				size->height = livery_height * (4 + FONT_HEIGHT_NORMAL);
				this->GetWidget<NWidgetCore>(SCLW_WIDGET_MATRIX)->widget_data = (livery_height << MAT_ROW_START) | (1 << MAT_COL_START);
				break;
			}

			case SCLW_WIDGET_SEC_COL_DROPDOWN:
				if (!_loaded_newgrf_features.has_2CC) {
					size->width = 0;
					break;
				}
				/* FALL THROUGH */
			case SCLW_WIDGET_PRI_COL_DROPDOWN: {
				for (const StringID *id = _colour_dropdown; id != endof(_colour_dropdown); id++) {
					size->width = max(size->width, GetStringBoundingBox(*id).width + 34);
				}
				break;
			}
		}
	}

	virtual void OnPaint()
	{
		/* Disable dropdown controls if no scheme is selected */
		this->SetWidgetDisabledState(SCLW_WIDGET_PRI_COL_DROPDOWN, this->sel == 0);
		this->SetWidgetDisabledState(SCLW_WIDGET_SEC_COL_DROPDOWN, this->sel == 0);

		this->DrawWidgets();
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case SCLW_WIDGET_PRI_COL_DROPDOWN:
			case SCLW_WIDGET_SEC_COL_DROPDOWN: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				LiveryScheme scheme = LS_DEFAULT;

				if (this->sel != 0) {
					for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
						if (HasBit(this->sel, scheme)) break;
					}
					if (scheme == LS_END) scheme = LS_DEFAULT;
				}
				SetDParam(0, STR_COLOUR_DARK_BLUE + ((widget == SCLW_WIDGET_PRI_COL_DROPDOWN) ? c->livery[scheme].colour1 : c->livery[scheme].colour2));
				break;
			}
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != SCLW_WIDGET_MATRIX) return;

		bool rtl = _current_text_dir == TD_RTL;

		/* Horizontal coordinates of scheme name column. */
		const NWidgetBase *nwi = this->GetWidget<NWidgetBase>(SCLW_WIDGET_SPACER_DROPDOWN);
		int sch_left = nwi->pos_x;
		int sch_right = sch_left + nwi->current_x - 1;
		/* Horizontal coordinates of first dropdown. */
		nwi = this->GetWidget<NWidgetBase>(SCLW_WIDGET_PRI_COL_DROPDOWN);
		int pri_left = nwi->pos_x;
		int pri_right = pri_left + nwi->current_x - 1;
		/* Horizontal coordinates of second dropdown. */
		nwi = this->GetWidget<NWidgetBase>(SCLW_WIDGET_SEC_COL_DROPDOWN);
		int sec_left = nwi->pos_x;
		int sec_right = sec_left + nwi->current_x - 1;

		int text_left  = (rtl ? (uint)WD_FRAMERECT_LEFT : TEXT_INDENT);
		int text_right = (rtl ? TEXT_INDENT : (uint)WD_FRAMERECT_RIGHT);

		int y = r.top + 3;
		const Company *c = Company::Get((CompanyID)this->window_number);
		for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
			if (_livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
				bool sel = HasBit(this->sel, scheme) != 0;

				/* Optional check box + scheme name. */
				if (scheme != LS_DEFAULT) {
					DrawSprite(c->livery[scheme].in_use ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, (rtl ? sch_right - TEXT_INDENT + WD_FRAMERECT_RIGHT : sch_left) + WD_FRAMERECT_LEFT, y);
				}
				DrawString(sch_left + text_left, sch_right - text_right, y, STR_LIVERY_DEFAULT + scheme, sel ? TC_WHITE : TC_BLACK);

				/* Text below the first dropdown. */
				DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOUR(c->livery[scheme].colour1), (rtl ? pri_right - TEXT_INDENT + WD_FRAMERECT_RIGHT : pri_left) + WD_FRAMERECT_LEFT, y);
				DrawString(pri_left + text_left, pri_right - text_right, y, STR_COLOUR_DARK_BLUE + c->livery[scheme].colour1, sel ? TC_WHITE : TC_GOLD);

				/* Text below the second dropdown. */
				if (sec_right > sec_left) { // Second dropdown has non-zero size.
					DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOUR(c->livery[scheme].colour2), (rtl ? sec_right - TEXT_INDENT + WD_FRAMERECT_RIGHT : sec_left) + WD_FRAMERECT_LEFT, y);
					DrawString(sec_left + text_left, sec_right - text_right, y, STR_COLOUR_DARK_BLUE + c->livery[scheme].colour2, sel ? TC_WHITE : TC_GOLD);
				}

				y += 4 + FONT_HEIGHT_NORMAL;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			/* Livery Class buttons */
			case SCLW_WIDGET_CLASS_GENERAL:
			case SCLW_WIDGET_CLASS_RAIL:
			case SCLW_WIDGET_CLASS_ROAD:
			case SCLW_WIDGET_CLASS_SHIP:
			case SCLW_WIDGET_CLASS_AIRCRAFT:
				this->RaiseWidget(this->livery_class + SCLW_WIDGET_CLASS_GENERAL);
				this->livery_class = (LiveryClass)(widget - SCLW_WIDGET_CLASS_GENERAL);
				this->LowerWidget(this->livery_class + SCLW_WIDGET_CLASS_GENERAL);

				/* Select the first item in the list */
				this->sel = 0;
				for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					if (_livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
						this->sel = 1 << scheme;
						break;
					}
				}

				this->ReInit();
				break;

			case SCLW_WIDGET_PRI_COL_DROPDOWN: // First colour dropdown
				ShowColourDropDownMenu(SCLW_WIDGET_PRI_COL_DROPDOWN);
				break;

			case SCLW_WIDGET_SEC_COL_DROPDOWN: // Second colour dropdown
				ShowColourDropDownMenu(SCLW_WIDGET_SEC_COL_DROPDOWN);
				break;

			case SCLW_WIDGET_MATRIX: {
				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(SCLW_WIDGET_MATRIX);
				LiveryScheme j = (LiveryScheme)((pt.y - wid->pos_y) / (4 + FONT_HEIGHT_NORMAL));

				for (LiveryScheme scheme = LS_BEGIN; scheme <= j; scheme++) {
					if (_livery_class[scheme] != this->livery_class || !HasBit(_loaded_newgrf_features.used_liveries, scheme)) j++;
					if (scheme >= LS_END) return;
				}
				if (j >= LS_END) return;

				/* If clicking on the left edge, toggle using the livery */
				if (_current_text_dir == TD_RTL ? pt.x - wid->pos_x > wid->current_x - TEXT_INDENT : pt.x - wid->pos_x < TEXT_INDENT) {
					DoCommandP(0, j | (2 << 8), !Company::Get((CompanyID)this->window_number)->livery[j].in_use, CMD_SET_COMPANY_COLOUR);
				}

				if (_ctrl_pressed) {
					ToggleBit(this->sel, j);
				} else {
					this->sel = 1 << j;
				}
				this->SetDirty();
				break;
			}
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
			if (HasBit(this->sel, scheme)) {
				DoCommandP(0, scheme | (widget == SCLW_WIDGET_PRI_COL_DROPDOWN ? 0 : 256), index, CMD_SET_COMPANY_COLOUR);
			}
		}
	}

	virtual void OnInvalidateData(int data = 0)
	{
		this->SetWidgetsDisabledState(true, SCLW_WIDGET_CLASS_RAIL, SCLW_WIDGET_CLASS_ROAD, SCLW_WIDGET_CLASS_SHIP, SCLW_WIDGET_CLASS_AIRCRAFT, WIDGET_LIST_END);

		bool current_class_valid = this->livery_class == LC_OTHER;
		if (_settings_client.gui.liveries == LIT_ALL || (_settings_client.gui.liveries == LIT_COMPANY && this->window_number == _local_company)) {
			for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
					if (_livery_class[scheme] == this->livery_class) current_class_valid = true;
					this->EnableWidget(SCLW_WIDGET_CLASS_GENERAL + _livery_class[scheme]);
				} else {
					ClrBit(this->sel, scheme);
				}
			}
		}

		if (!current_class_valid) {
			Point pt = {0, 0};
			this->OnClick(pt, SCLW_WIDGET_CLASS_GENERAL, 1);
		} else if (data == 0) {
			this->ReInit();
		}
	}
};

static const NWidgetPart _nested_select_company_livery_widgets [] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SCLW_WIDGET_CAPTION), SetDataTip(STR_LIVERY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_GENERAL), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_COMPANY_GENERAL, STR_LIVERY_GENERAL_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_RAIL), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRAINLIST, STR_LIVERY_TRAIN_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_ROAD), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_TRUCKLIST, STR_LIVERY_ROAD_VEHICLE_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_SHIP), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SHIPLIST, STR_LIVERY_SHIP_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_AIRCRAFT), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_AIRPLANESLIST, STR_LIVERY_AIRCRAFT_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(90, 22), SetFill(1, 1), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, SCLW_WIDGET_SPACER_DROPDOWN), SetMinimalSize(150, 12), SetFill(1, 1), EndContainer(),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, SCLW_WIDGET_PRI_COL_DROPDOWN), SetMinimalSize(125, 12), SetFill(0, 1), SetDataTip(STR_BLACK_STRING, STR_LIVERY_PRIMARY_TOOLTIP),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, SCLW_WIDGET_SEC_COL_DROPDOWN), SetMinimalSize(125, 12), SetFill(0, 1),
				SetDataTip(STR_BLACK_STRING, STR_LIVERY_SECONDARY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_MATRIX, COLOUR_GREY, SCLW_WIDGET_MATRIX), SetMinimalSize(275, 15), SetFill(1, 0), SetDataTip((1 << MAT_ROW_START) | (1 << MAT_COL_START), STR_LIVERY_PANEL_TOOLTIP),
};

static const WindowDesc _select_company_livery_desc(
	WDP_AUTO, 0, 0,
	WC_COMPANY_COLOUR, WC_NONE,
	0,
	_nested_select_company_livery_widgets, lengthof(_nested_select_company_livery_widgets)
);

/**
 * Draws the face of a company manager's face.
 * @param cmf   the company manager's face
 * @param colour the (background) colour of the gradient
 * @param x     x-position to draw the face
 * @param y     y-position to draw the face
 */
void DrawCompanyManagerFace(CompanyManagerFace cmf, int colour, int x, int y)
{
	GenderEthnicity ge = (GenderEthnicity)GetCompanyManagerFaceBits(cmf, CMFV_GEN_ETHN, GE_WM);

	bool has_moustache   = !HasBit(ge, GENDER_FEMALE) && GetCompanyManagerFaceBits(cmf, CMFV_HAS_MOUSTACHE,   ge) != 0;
	bool has_tie_earring = !HasBit(ge, GENDER_FEMALE) || GetCompanyManagerFaceBits(cmf, CMFV_HAS_TIE_EARRING, ge) != 0;
	bool has_glasses     = GetCompanyManagerFaceBits(cmf, CMFV_HAS_GLASSES, ge) != 0;
	PaletteID pal;

	/* Modify eye colour palette only if 2 or more valid values exist */
	if (_cmf_info[CMFV_EYE_COLOUR].valid_values[ge] < 2) {
		pal = PAL_NONE;
	} else {
		switch (GetCompanyManagerFaceBits(cmf, CMFV_EYE_COLOUR, ge)) {
			default: NOT_REACHED();
			case 0: pal = PALETTE_TO_BROWN; break;
			case 1: pal = PALETTE_TO_BLUE;  break;
			case 2: pal = PALETTE_TO_GREEN; break;
		}
	}

	/* Draw the gradient (background) */
	DrawSprite(SPR_GRADIENT, GENERAL_SPRITE_COLOUR(colour), x, y);

	for (CompanyManagerFaceVariable cmfv = CMFV_CHEEKS; cmfv < CMFV_END; cmfv++) {
		switch (cmfv) {
			case CMFV_MOUSTACHE:   if (!has_moustache)   continue; break;
			case CMFV_LIPS:        // FALL THROUGH
			case CMFV_NOSE:        if (has_moustache)    continue; break;
			case CMFV_TIE_EARRING: if (!has_tie_earring) continue; break;
			case CMFV_GLASSES:     if (!has_glasses)     continue; break;
			default: break;
		}
		DrawSprite(GetCompanyManagerFaceSprite(cmf, cmfv, ge), (cmfv == CMFV_EYEBROWS) ? pal : PAL_NONE, x, y);
	}
}

/**
 * Names of the widgets. Keep them in the same order as in the widget array.
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

/** Nested widget description for the company manager face selection dialog */
static const NWidgetPart _nested_select_company_manager_face_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, SCMFW_WIDGET_CAPTION), SetDataTip(STR_FACE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCMFW_WIDGET_TOGGLE_LARGE_SMALL), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_FACE_ADVANCED_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SCMFW_WIDGET_SELECT_FACE),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 2, 2),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_SPACER), SetFill(1, 0),
					NWidget(WWT_EMPTY, COLOUR_GREY, SCMFM_WIDGET_FACE), SetMinimalSize(92, 119),
					NWidget(NWID_SPACER), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_RANDOM_NEW_FACE), SetFill(1, 0), SetDataTip(STR_FACE_NEW_FACE_BUTTON, STR_FACE_NEW_FACE_TOOLTIP),
				NWidget(NWID_SELECTION, INVALID_COLOUR, SCMFW_WIDGET_SEL_LOADSAVE), // Load/number/save buttons under the portrait in the advanced view.
					NWidget(NWID_VERTICAL),
						NWidget(NWID_SPACER), SetMinimalSize(0, 5), SetFill(0, 1),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_LOAD), SetFill(1, 0), SetDataTip(STR_FACE_LOAD, STR_FACE_LOAD_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_FACECODE), SetFill(1, 0), SetDataTip(STR_FACE_FACECODE, STR_FACE_FACECODE_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_SAVE), SetFill(1, 0), SetDataTip(STR_FACE_SAVE, STR_FACE_SAVE_TOOLTIP),
						NWidget(NWID_SPACER), SetMinimalSize(0, 5), SetFill(0, 1),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON), SetFill(1, 0), SetDataTip(STR_FACE_ADVANCED, STR_FACE_ADVANCED_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(NWID_SELECTION, INVALID_COLOUR, SCMFW_WIDGET_SEL_MALEFEMALE), // Simple male/female face setting.
					NWidget(NWID_VERTICAL),
						NWidget(NWID_SPACER), SetFill(0, 1),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_MALE), SetFill(1, 0), SetDataTip(STR_FACE_MALE_BUTTON, STR_FACE_MALE_TOOLTIP),
						NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_FEMALE), SetFill(1, 0), SetDataTip(STR_FACE_FEMALE_BUTTON, STR_FACE_FEMALE_TOOLTIP),
						NWidget(NWID_SPACER), SetFill(0, 1),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, SCMFW_WIDGET_SEL_PARTS), // Advanced face parts setting.
					NWidget(NWID_VERTICAL),
						NWidget(NWID_SPACER), SetMinimalSize(0, 2),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_MALE2), SetFill(1, 0), SetDataTip(STR_FACE_MALE_BUTTON, STR_FACE_MALE_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_FEMALE2), SetFill(1, 0), SetDataTip(STR_FACE_FEMALE_BUTTON, STR_FACE_FEMALE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SPACER), SetMinimalSize(0, 2),
						NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_ETHNICITY_EUR), SetFill(1, 0), SetDataTip(STR_FACE_EUROPEAN, STR_FACE_SELECT_EUROPEAN),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_ETHNICITY_AFR), SetFill(1, 0), SetDataTip(STR_FACE_AFRICAN, STR_FACE_SELECT_AFRICAN),
						EndContainer(),
						NWidget(NWID_SPACER), SetMinimalSize(0, 4),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_HAS_MOUSTACHE_EARRING), SetDataTip(STR_EMPTY, STR_FACE_MOUSTACHE_EARRING_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_HAS_GLASSES_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_HAS_GLASSES), SetDataTip(STR_EMPTY, STR_FACE_GLASSES_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SPACER), SetMinimalSize(0, 2), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_HAIR_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_HAIR_L), SetDataTip(AWV_DECREASE, STR_FACE_HAIR_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_HAIR), SetDataTip(STR_EMPTY, STR_FACE_HAIR_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_HAIR_R), SetDataTip(AWV_INCREASE, STR_FACE_HAIR_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_EYEBROWS_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_EYEBROWS_L), SetDataTip(AWV_DECREASE, STR_FACE_EYEBROWS_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_EYEBROWS), SetDataTip(STR_EMPTY, STR_FACE_EYEBROWS_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_EYEBROWS_R), SetDataTip(AWV_INCREASE, STR_FACE_EYEBROWS_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_EYECOLOUR_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_EYECOLOUR_L), SetDataTip(AWV_DECREASE, STR_FACE_EYECOLOUR_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_EYECOLOUR), SetDataTip(STR_EMPTY, STR_FACE_EYECOLOUR_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_EYECOLOUR_R), SetDataTip(AWV_INCREASE, STR_FACE_EYECOLOUR_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_GLASSES_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_GLASSES_L), SetDataTip(AWV_DECREASE, STR_FACE_GLASSES_TOOLTIP_2),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_GLASSES), SetDataTip(STR_EMPTY, STR_FACE_GLASSES_TOOLTIP_2),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_GLASSES_R), SetDataTip(AWV_INCREASE, STR_FACE_GLASSES_TOOLTIP_2),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_NOSE_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_NOSE_L), SetDataTip(AWV_DECREASE, STR_FACE_NOSE_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_NOSE), SetDataTip(STR_EMPTY, STR_FACE_NOSE_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_NOSE_R), SetDataTip(AWV_INCREASE, STR_FACE_NOSE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_LIPS_MOUSTACHE_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_LIPS_MOUSTACHE_L), SetDataTip(AWV_DECREASE, STR_FACE_LIPS_MOUSTACHE_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_LIPS_MOUSTACHE), SetDataTip(STR_EMPTY, STR_FACE_LIPS_MOUSTACHE_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_LIPS_MOUSTACHE_R), SetDataTip(AWV_INCREASE, STR_FACE_LIPS_MOUSTACHE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_CHIN_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_CHIN_L), SetDataTip(AWV_DECREASE, STR_FACE_CHIN_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_CHIN), SetDataTip(STR_EMPTY, STR_FACE_CHIN_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_CHIN_R), SetDataTip(AWV_INCREASE, STR_FACE_CHIN_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_JACKET_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_JACKET_L), SetDataTip(AWV_DECREASE, STR_FACE_JACKET_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_JACKET), SetDataTip(STR_EMPTY, STR_FACE_JACKET_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_JACKET_R), SetDataTip(AWV_INCREASE, STR_FACE_JACKET_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_COLLAR_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_COLLAR_L), SetDataTip(AWV_DECREASE, STR_FACE_COLLAR_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_COLLAR), SetDataTip(STR_EMPTY, STR_FACE_COLLAR_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_COLLAR_R), SetDataTip(AWV_INCREASE, STR_FACE_COLLAR_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, SCMFW_WIDGET_TIE_EARRING_TEXT), SetFill(1, 0),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_TIE_EARRING_L), SetDataTip(AWV_DECREASE, STR_FACE_TIE_EARRING_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_TIE_EARRING), SetDataTip(STR_EMPTY, STR_FACE_TIE_EARRING_TOOLTIP),
							NWidget(WWT_PUSHARROWBTN, COLOUR_GREY, SCMFW_WIDGET_TIE_EARRING_R), SetDataTip(AWV_INCREASE, STR_FACE_TIE_EARRING_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SPACER), SetFill(0, 1),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_CANCEL), SetFill(1, 0), SetDataTip(STR_BUTTON_CANCEL, STR_FACE_CANCEL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_ACCEPT), SetFill(1, 0), SetDataTip(STR_BUTTON_OK, STR_FACE_OK_TOOLTIP),
	EndContainer(),
};

/** Management class for customizing the face of the company manager. */
class SelectCompanyManagerFaceWindow : public Window
{
	CompanyManagerFace face; ///< company manager face bits
	bool advanced; ///< advanced company manager face selection window

	GenderEthnicity ge; ///< Gender and ethnicity.
	bool is_female;     ///< Female face.
	bool is_moust_male; ///< Male face with a moustache.

	Dimension yesno_dim;  ///< Dimension of a yes/no button of a part in the advanced face window.
	Dimension number_dim; ///< Dimension of a number widget of a part in the advanced face window.

	static const StringID PART_TEXTS_IS_FEMALE[]; ///< Strings depending on #is_female, used to describe parts (2 entries for a part).
	static const StringID PART_TEXTS[];           ///< Fixed strings to describe parts of the face.

	/**
	 * Draw dynamic a label to the left of the button and a value in the button
	 *
	 * @param widget_index   index of this widget in the window
	 * @param val            the value which will be draw
	 * @param is_bool_widget is it a bool button
	 */
	void DrawFaceStringLabel(byte widget_index, uint8 val, bool is_bool_widget) const
	{
		StringID str;
		const NWidgetCore *nwi_widget = this->GetWidget<NWidgetCore>(widget_index);
		if (!nwi_widget->IsDisabled()) {
			if (is_bool_widget) {
				/* if it a bool button write yes or no */
				str = (val != 0) ? STR_FACE_YES : STR_FACE_NO;
			} else {
				/* else write the value + 1 */
				SetDParam(0, val + 1);
				str = STR_JUST_INT;
			}

			/* Draw the value/bool in white (0xC). If the button clicked adds 1px to x and y text coordinates (IsWindowWidgetLowered()). */
			DrawString(nwi_widget->pos_x + nwi_widget->IsLowered(), nwi_widget->pos_x + nwi_widget->current_x - 1 - nwi_widget->IsLowered(),
					nwi_widget->pos_y + 1 + nwi_widget->IsLowered(), str, TC_WHITE, SA_HOR_CENTER);
		}
	}

	void UpdateData()
	{
		this->ge = (GenderEthnicity)GB(this->face, _cmf_info[CMFV_GEN_ETHN].offset, _cmf_info[CMFV_GEN_ETHN].length); // get the gender and ethnicity
		this->is_female = HasBit(this->ge, GENDER_FEMALE); // get the gender: 0 == male and 1 == female
		this->is_moust_male = !is_female && GetCompanyManagerFaceBits(this->face, CMFV_HAS_MOUSTACHE, this->ge) != 0; // is a male face with moustache
	}

public:
	SelectCompanyManagerFaceWindow(const WindowDesc *desc, Window *parent) : Window()
	{
		this->advanced = false;
		this->CreateNestedTree(desc);
		this->SelectDisplayPlanes(this->advanced);
		this->FinishInitNested(desc, parent->window_number);
		this->parent = parent;
		this->owner = (Owner)this->window_number;
		this->face = Company::Get((CompanyID)this->window_number)->face;

		this->UpdateData();
	}

	/**
	 * Select planes to display to the user with the #NWID_SELECTION widgets #SCMFW_WIDGET_SEL_LOADSAVE, #SCMFW_WIDGET_SEL_MALEFEMALE, and #SCMFW_WIDGET_SEL_PARTS.
	 * @param advanced Display advanced face management window.
	 */
	void SelectDisplayPlanes(bool advanced)
	{
		this->GetWidget<NWidgetStacked>(SCMFW_WIDGET_SEL_LOADSAVE)->SetDisplayedPlane(advanced ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(SCMFW_WIDGET_SEL_PARTS)->SetDisplayedPlane(advanced ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(SCMFW_WIDGET_SEL_MALEFEMALE)->SetDisplayedPlane(advanced ? SZSP_NONE : 0);
		this->GetWidget<NWidgetCore>(SCMFW_WIDGET_RANDOM_NEW_FACE)->widget_data = advanced ? STR_MAPGEN_RANDOM : STR_FACE_NEW_FACE_BUTTON;

		NWidgetCore *wi = this->GetWidget<NWidgetCore>(SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON);
		if (advanced) {
			wi->SetDataTip(STR_FACE_SIMPLE, STR_FACE_SIMPLE_TOOLTIP);
		} else {
			wi->SetDataTip(STR_FACE_ADVANCED, STR_FACE_ADVANCED_TOOLTIP);
		}
	}

	virtual void OnInit()
	{
		/* Size of the boolean yes/no button. */
		Dimension yesno_dim = maxdim(GetStringBoundingBox(STR_FACE_YES), GetStringBoundingBox(STR_FACE_NO));
		yesno_dim.width  += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		yesno_dim.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		/* Size of the number button + arrows. */
		Dimension number_dim = {0, 0};
		for (int val = 1; val <= 12; val++) {
			SetDParam(0, val);
			number_dim = maxdim(number_dim, GetStringBoundingBox(STR_JUST_INT));
		}
		uint arrows_width = GetSpriteSize(SPR_ARROW_LEFT).width + GetSpriteSize(SPR_ARROW_RIGHT).width + 2 * (WD_IMGBTN_LEFT + WD_IMGBTN_RIGHT);
		number_dim.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT + arrows_width;
		number_dim.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		/* Compute width of both buttons. */
		yesno_dim.width = max(yesno_dim.width, number_dim.width);
		number_dim.width = yesno_dim.width - arrows_width;

		this->yesno_dim = yesno_dim;
		this->number_dim = number_dim;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT:
			case SCMFW_WIDGET_TIE_EARRING_TEXT: {
				int offset = (widget - SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT) * 2;
				*size = maxdim(GetStringBoundingBox(PART_TEXTS_IS_FEMALE[offset]), GetStringBoundingBox(PART_TEXTS_IS_FEMALE[offset + 1]));
				size->width  += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
			}

			case SCMFW_WIDGET_LIPS_MOUSTACHE_TEXT:
				*size = maxdim(GetStringBoundingBox(STR_FACE_LIPS), GetStringBoundingBox(STR_FACE_MOUSTACHE));
				size->width  += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case SCMFW_WIDGET_HAS_GLASSES_TEXT:
			case SCMFW_WIDGET_HAIR_TEXT:
			case SCMFW_WIDGET_EYEBROWS_TEXT:
			case SCMFW_WIDGET_EYECOLOUR_TEXT:
			case SCMFW_WIDGET_GLASSES_TEXT:
			case SCMFW_WIDGET_NOSE_TEXT:
			case SCMFW_WIDGET_CHIN_TEXT:
			case SCMFW_WIDGET_JACKET_TEXT:
			case SCMFW_WIDGET_COLLAR_TEXT:
				*size = GetStringBoundingBox(PART_TEXTS[widget - SCMFW_WIDGET_HAS_GLASSES_TEXT]);
				size->width  += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
				size->height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;

			case SCMFW_WIDGET_HAS_MOUSTACHE_EARRING:
			case SCMFW_WIDGET_HAS_GLASSES:
				*size = this->yesno_dim;
				break;

			case SCMFW_WIDGET_EYECOLOUR:
			case SCMFW_WIDGET_CHIN:
			case SCMFW_WIDGET_EYEBROWS:
			case SCMFW_WIDGET_LIPS_MOUSTACHE:
			case SCMFW_WIDGET_NOSE:
			case SCMFW_WIDGET_HAIR:
			case SCMFW_WIDGET_JACKET:
			case SCMFW_WIDGET_COLLAR:
			case SCMFW_WIDGET_TIE_EARRING:
			case SCMFW_WIDGET_GLASSES:
				*size = this->number_dim;
				break;
		}
	}

	virtual void OnPaint()
	{
		/* lower the non-selected gender button */
		this->SetWidgetsLoweredState(!this->is_female, SCMFW_WIDGET_MALE, SCMFW_WIDGET_MALE2, WIDGET_LIST_END);
		this->SetWidgetsLoweredState( this->is_female, SCMFW_WIDGET_FEMALE, SCMFW_WIDGET_FEMALE2, WIDGET_LIST_END);

		/* advanced company manager face selection window */

		/* lower the non-selected ethnicity button */
		this->SetWidgetLoweredState(SCMFW_WIDGET_ETHNICITY_EUR, !HasBit(this->ge, ETHNICITY_BLACK));
		this->SetWidgetLoweredState(SCMFW_WIDGET_ETHNICITY_AFR,  HasBit(this->ge, ETHNICITY_BLACK));


		/* Disable dynamically the widgets which CompanyManagerFaceVariable has less than 2 options
		 * (or in other words you haven't any choice).
		 * If the widgets depend on a HAS-variable and this is false the widgets will be disabled, too. */

		/* Eye colour buttons */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_EYE_COLOUR].valid_values[this->ge] < 2,
				SCMFW_WIDGET_EYECOLOUR, SCMFW_WIDGET_EYECOLOUR_L, SCMFW_WIDGET_EYECOLOUR_R, WIDGET_LIST_END);

		/* Chin buttons */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_CHIN].valid_values[this->ge] < 2,
				SCMFW_WIDGET_CHIN, SCMFW_WIDGET_CHIN_L, SCMFW_WIDGET_CHIN_R, WIDGET_LIST_END);

		/* Eyebrows buttons */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_EYEBROWS].valid_values[this->ge] < 2,
				SCMFW_WIDGET_EYEBROWS, SCMFW_WIDGET_EYEBROWS_L, SCMFW_WIDGET_EYEBROWS_R, WIDGET_LIST_END);

		/* Lips or (if it a male face with a moustache) moustache buttons */
		this->SetWidgetsDisabledState(_cmf_info[this->is_moust_male ? CMFV_MOUSTACHE : CMFV_LIPS].valid_values[this->ge] < 2,
				SCMFW_WIDGET_LIPS_MOUSTACHE, SCMFW_WIDGET_LIPS_MOUSTACHE_L, SCMFW_WIDGET_LIPS_MOUSTACHE_R, WIDGET_LIST_END);

		/* Nose buttons | male faces with moustache haven't any nose options */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_NOSE].valid_values[this->ge] < 2 || this->is_moust_male,
				SCMFW_WIDGET_NOSE, SCMFW_WIDGET_NOSE_L, SCMFW_WIDGET_NOSE_R, WIDGET_LIST_END);

		/* Hair buttons */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_HAIR].valid_values[this->ge] < 2,
				SCMFW_WIDGET_HAIR, SCMFW_WIDGET_HAIR_L, SCMFW_WIDGET_HAIR_R, WIDGET_LIST_END);

		/* Jacket buttons */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_JACKET].valid_values[this->ge] < 2,
				SCMFW_WIDGET_JACKET, SCMFW_WIDGET_JACKET_L, SCMFW_WIDGET_JACKET_R, WIDGET_LIST_END);

		/* Collar buttons */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_COLLAR].valid_values[this->ge] < 2,
				SCMFW_WIDGET_COLLAR, SCMFW_WIDGET_COLLAR_L, SCMFW_WIDGET_COLLAR_R, WIDGET_LIST_END);

		/* Tie/earring buttons | female faces without earring haven't any earring options */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_TIE_EARRING].valid_values[this->ge] < 2 ||
					(this->is_female && GetCompanyManagerFaceBits(this->face, CMFV_HAS_TIE_EARRING, this->ge) == 0),
				SCMFW_WIDGET_TIE_EARRING, SCMFW_WIDGET_TIE_EARRING_L, SCMFW_WIDGET_TIE_EARRING_R, WIDGET_LIST_END);

		/* Glasses buttons | faces without glasses haven't any glasses options */
		this->SetWidgetsDisabledState(_cmf_info[CMFV_GLASSES].valid_values[this->ge] < 2 || GetCompanyManagerFaceBits(this->face, CMFV_HAS_GLASSES, this->ge) == 0,
				SCMFW_WIDGET_GLASSES, SCMFW_WIDGET_GLASSES_L, SCMFW_WIDGET_GLASSES_R, WIDGET_LIST_END);

		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT:
			case SCMFW_WIDGET_TIE_EARRING_TEXT: {
				StringID str = PART_TEXTS_IS_FEMALE[(widget - SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT) * 2 + this->is_female];
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, str, TC_GOLD, SA_RIGHT);
				break;
			}

			case SCMFW_WIDGET_LIPS_MOUSTACHE_TEXT:
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, (this->is_moust_male) ? STR_FACE_MOUSTACHE : STR_FACE_LIPS, TC_GOLD, SA_RIGHT);
				break;

			case SCMFW_WIDGET_HAS_GLASSES_TEXT:
			case SCMFW_WIDGET_HAIR_TEXT:
			case SCMFW_WIDGET_EYEBROWS_TEXT:
			case SCMFW_WIDGET_EYECOLOUR_TEXT:
			case SCMFW_WIDGET_GLASSES_TEXT:
			case SCMFW_WIDGET_NOSE_TEXT:
			case SCMFW_WIDGET_CHIN_TEXT:
			case SCMFW_WIDGET_JACKET_TEXT:
			case SCMFW_WIDGET_COLLAR_TEXT:
				DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, r.top + WD_FRAMERECT_TOP, PART_TEXTS[widget - SCMFW_WIDGET_HAS_GLASSES_TEXT], TC_GOLD, SA_RIGHT);
				break;


			case SCMFW_WIDGET_HAS_MOUSTACHE_EARRING:
				if (this->is_female) { // Only for female faces
					this->DrawFaceStringLabel(SCMFW_WIDGET_HAS_MOUSTACHE_EARRING, GetCompanyManagerFaceBits(this->face, CMFV_HAS_TIE_EARRING, this->ge), true);
				} else { // Only for male faces
					this->DrawFaceStringLabel(SCMFW_WIDGET_HAS_MOUSTACHE_EARRING, GetCompanyManagerFaceBits(this->face, CMFV_HAS_MOUSTACHE,   this->ge), true);
				}
				break;

			case SCMFW_WIDGET_TIE_EARRING:
				if (this->is_female) { // Only for female faces
					this->DrawFaceStringLabel(SCMFW_WIDGET_TIE_EARRING, GetCompanyManagerFaceBits(this->face, CMFV_TIE_EARRING, this->ge), false);
				} else { // Only for male faces
					this->DrawFaceStringLabel(SCMFW_WIDGET_TIE_EARRING, GetCompanyManagerFaceBits(this->face, CMFV_TIE_EARRING, this->ge), false);
				}
				break;

			case SCMFW_WIDGET_LIPS_MOUSTACHE:
				if (this->is_moust_male) { // Only for male faces with moustache
					this->DrawFaceStringLabel(SCMFW_WIDGET_LIPS_MOUSTACHE, GetCompanyManagerFaceBits(this->face, CMFV_MOUSTACHE, this->ge), false);
				} else { // Only for female faces or male faces without moustache
					this->DrawFaceStringLabel(SCMFW_WIDGET_LIPS_MOUSTACHE, GetCompanyManagerFaceBits(this->face, CMFV_LIPS,      this->ge), false);
				}
				break;

			case SCMFW_WIDGET_HAS_GLASSES:
				this->DrawFaceStringLabel(SCMFW_WIDGET_HAS_GLASSES, GetCompanyManagerFaceBits(this->face, CMFV_HAS_GLASSES, this->ge), true );
				break;

			case SCMFW_WIDGET_HAIR:
				this->DrawFaceStringLabel(SCMFW_WIDGET_HAIR,        GetCompanyManagerFaceBits(this->face, CMFV_HAIR,        this->ge), false);
				break;

			case SCMFW_WIDGET_EYEBROWS:
				this->DrawFaceStringLabel(SCMFW_WIDGET_EYEBROWS,    GetCompanyManagerFaceBits(this->face, CMFV_EYEBROWS,    this->ge), false);
				break;

			case SCMFW_WIDGET_EYECOLOUR:
				this->DrawFaceStringLabel(SCMFW_WIDGET_EYECOLOUR,   GetCompanyManagerFaceBits(this->face, CMFV_EYE_COLOUR,  this->ge), false);
				break;

			case SCMFW_WIDGET_GLASSES:
				this->DrawFaceStringLabel(SCMFW_WIDGET_GLASSES,     GetCompanyManagerFaceBits(this->face, CMFV_GLASSES,     this->ge), false);
				break;

			case SCMFW_WIDGET_NOSE:
				this->DrawFaceStringLabel(SCMFW_WIDGET_NOSE,        GetCompanyManagerFaceBits(this->face, CMFV_NOSE,        this->ge), false);
				break;

			case SCMFW_WIDGET_CHIN:
				this->DrawFaceStringLabel(SCMFW_WIDGET_CHIN,        GetCompanyManagerFaceBits(this->face, CMFV_CHIN,        this->ge), false);
				break;

			case SCMFW_WIDGET_JACKET:
				this->DrawFaceStringLabel(SCMFW_WIDGET_JACKET,      GetCompanyManagerFaceBits(this->face, CMFV_JACKET,      this->ge), false);
				break;

			case SCMFW_WIDGET_COLLAR:
				this->DrawFaceStringLabel(SCMFW_WIDGET_COLLAR,      GetCompanyManagerFaceBits(this->face, CMFV_COLLAR,      this->ge), false);
				break;

			case SCMFM_WIDGET_FACE:
				DrawCompanyManagerFace(this->face, Company::Get((CompanyID)this->window_number)->colour, r.left, r.top);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			/* Toggle size, advanced/simple face selection */
			case SCMFW_WIDGET_TOGGLE_LARGE_SMALL:
			case SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON:
				this->advanced = !this->advanced;
				this->SelectDisplayPlanes(this->advanced);
				this->ReInit();
				break;

			/* OK button */
			case SCMFW_WIDGET_ACCEPT:
				DoCommandP(0, 0, this->face, CMD_SET_COMPANY_MANAGER_FACE);
				/* FALL THROUGH */

			/* Cancel button */
			case SCMFW_WIDGET_CANCEL:
				delete this;
				break;

			/* Load button */
			case SCMFW_WIDGET_LOAD:
				this->face = _company_manager_face;
				ScaleAllCompanyManagerFaceBits(this->face);
				ShowErrorMessage(STR_FACE_LOAD_DONE, INVALID_STRING_ID, WL_INFO);
				this->UpdateData();
				this->SetDirty();
				break;

			/* 'Company manager face number' button, view and/or set company manager face number */
			case SCMFW_WIDGET_FACECODE:
				SetDParam(0, this->face);
				ShowQueryString(STR_JUST_INT, STR_FACE_FACECODE_CAPTION, 10 + 1, 0, this, CS_NUMERAL, QSF_NONE);
				break;

			/* Save button */
			case SCMFW_WIDGET_SAVE:
				_company_manager_face = this->face;
				ShowErrorMessage(STR_FACE_SAVE_DONE, INVALID_STRING_ID, WL_INFO);
				break;

			/* Toggle gender (male/female) button */
			case SCMFW_WIDGET_MALE:
			case SCMFW_WIDGET_FEMALE:
			case SCMFW_WIDGET_MALE2:
			case SCMFW_WIDGET_FEMALE2:
				SetCompanyManagerFaceBits(this->face, CMFV_GENDER, this->ge, (widget == SCMFW_WIDGET_FEMALE || widget == SCMFW_WIDGET_FEMALE2));
				ScaleAllCompanyManagerFaceBits(this->face);
				this->UpdateData();
				this->SetDirty();
				break;

			/* Randomize face button */
			case SCMFW_WIDGET_RANDOM_NEW_FACE:
				RandomCompanyManagerFaceBits(this->face, this->ge, this->advanced);
				this->UpdateData();
				this->SetDirty();
				break;

			/* Toggle ethnicity (european/african) button */
			case SCMFW_WIDGET_ETHNICITY_EUR:
			case SCMFW_WIDGET_ETHNICITY_AFR:
				SetCompanyManagerFaceBits(this->face, CMFV_ETHNICITY, this->ge, widget - SCMFW_WIDGET_ETHNICITY_EUR);
				ScaleAllCompanyManagerFaceBits(this->face);
				this->UpdateData();
				this->SetDirty();
				break;

			default:
				/* Here all buttons from SCMFW_WIDGET_HAS_MOUSTACHE_EARRING to SCMFW_WIDGET_GLASSES_R are handled.
				 * First it checks which CompanyManagerFaceVariable is being changed, and then either
				 * a: invert the value for boolean variables, or
				 * b: it checks inside of IncreaseCompanyManagerFaceBits() if a left (_L) butten is pressed and then decrease else increase the variable */
				if (widget >= SCMFW_WIDGET_HAS_MOUSTACHE_EARRING && widget <= SCMFW_WIDGET_GLASSES_R) {
					CompanyManagerFaceVariable cmfv; // which CompanyManagerFaceVariable shall be edited

					if (widget < SCMFW_WIDGET_EYECOLOUR_L) { // Bool buttons
						switch (widget - SCMFW_WIDGET_HAS_MOUSTACHE_EARRING) {
							default: NOT_REACHED();
							case 0: cmfv = this->is_female ? CMFV_HAS_TIE_EARRING : CMFV_HAS_MOUSTACHE; break; // Has earring/moustache button
							case 1: cmfv = CMFV_HAS_GLASSES; break; // Has glasses button
						}
						SetCompanyManagerFaceBits(this->face, cmfv, this->ge, !GetCompanyManagerFaceBits(this->face, cmfv, this->ge));
						ScaleAllCompanyManagerFaceBits(this->face);
					} else { // Value buttons
						switch ((widget - SCMFW_WIDGET_EYECOLOUR_L) / 3) {
							default: NOT_REACHED();
							case 0: cmfv = CMFV_EYE_COLOUR; break;  // Eye colour buttons
							case 1: cmfv = CMFV_CHIN; break;        // Chin buttons
							case 2: cmfv = CMFV_EYEBROWS; break;    // Eyebrows buttons
							case 3: cmfv = this->is_moust_male ? CMFV_MOUSTACHE : CMFV_LIPS; break; // Moustache or lips buttons
							case 4: cmfv = CMFV_NOSE; break;        // Nose buttons
							case 5: cmfv = CMFV_HAIR; break;        // Hair buttons
							case 6: cmfv = CMFV_JACKET; break;      // Jacket buttons
							case 7: cmfv = CMFV_COLLAR; break;      // Collar buttons
							case 8: cmfv = CMFV_TIE_EARRING; break; // Tie/earring buttons
							case 9: cmfv = CMFV_GLASSES; break;     // Glasses buttons
						}
						/* 0 == left (_L), 1 == middle or 2 == right (_R) - button click */
						IncreaseCompanyManagerFaceBits(this->face, cmfv, this->ge, (((widget - SCMFW_WIDGET_EYECOLOUR_L) % 3) != 0) ? 1 : -1);
					}
					this->UpdateData();
					this->SetDirty();
				}
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;
		/* Set a new company manager face number */
		if (!StrEmpty(str)) {
			this->face = strtoul(str, NULL, 10);
			ScaleAllCompanyManagerFaceBits(this->face);
			ShowErrorMessage(STR_FACE_FACECODE_SET, INVALID_STRING_ID, WL_INFO);
			this->UpdateData();
			this->SetDirty();
		} else {
			ShowErrorMessage(STR_FACE_FACECODE_ERR, INVALID_STRING_ID, WL_INFO);
		}
	}
};

/** Both text values of parts of the face that depend on the #is_female boolean value. */
const StringID SelectCompanyManagerFaceWindow::PART_TEXTS_IS_FEMALE[] = {
	STR_FACE_MOUSTACHE, STR_FACE_EARRING, // SCMFW_WIDGET_HAS_MOUSTACHE_EARRING_TEXT
	STR_FACE_TIE,       STR_FACE_EARRING, // SCMFW_WIDGET_TIE_EARRING_TEXT
};

/** Textual names for parts of the face. */
const StringID SelectCompanyManagerFaceWindow::PART_TEXTS[] = {
	STR_FACE_GLASSES,   // SCMFW_WIDGET_HAS_GLASSES_TEXT
	STR_FACE_HAIR,      // SCMFW_WIDGET_HAIR_TEXT
	STR_FACE_EYEBROWS,  // SCMFW_WIDGET_EYEBROWS_TEXT
	STR_FACE_EYECOLOUR, // SCMFW_WIDGET_EYECOLOUR_TEXT
	STR_FACE_GLASSES,   // SCMFW_WIDGET_GLASSES_TEXT
	STR_FACE_NOSE,      // SCMFW_WIDGET_NOSE_TEXT
	STR_FACE_CHIN,      // SCMFW_WIDGET_CHIN_TEXT
	STR_FACE_JACKET,    // SCMFW_WIDGET_JACKET_TEXT
	STR_FACE_COLLAR,    // SCMFW_WIDGET_COLLAR_TEXT
};

/** Company manager face selection window description */
static const WindowDesc _select_company_manager_face_desc(
	WDP_AUTO, 0, 0,
	WC_COMPANY_MANAGER_FACE, WC_NONE,
	WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_nested_select_company_manager_face_widgets, lengthof(_nested_select_company_manager_face_widgets)
);

/**
 * Open the simple/advanced company manager face selection window
 *
 * @param parent the parent company window
 * @param adv    simple or advanced face selection window
 * @param top    previous top position of the window
 * @param left   previous left position of the window
 */
static void DoSelectCompanyManagerFace(Window *parent)
{
	if (!Company::IsValidID((CompanyID)parent->window_number)) return;

	if (BringWindowToFrontById(WC_COMPANY_MANAGER_FACE, parent->window_number)) return;
	new SelectCompanyManagerFaceWindow(&_select_company_manager_face_desc, parent);
}


/** Names of the widgets of the #CompanyWindow. Keep them in the same order as in the widget array */
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

	CW_WIDGET_HAS_PASSWORD,       ///< Draw a lock when the company has a password
	CW_WIDGET_SELECT_MULTIPLAYER, ///< Multiplayer selection panel.
	CW_WIDGET_COMPANY_PASSWORD,
	CW_WIDGET_COMPANY_JOIN,
};

static const NWidgetPart _nested_company_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, CW_WIDGET_CAPTION), SetDataTip(STR_COMPANY_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL), SetPIP(4, 6, 4),
			NWidget(NWID_VERTICAL), SetPIP(4, 2, 4),
				NWidget(WWT_EMPTY, INVALID_COLOUR, CW_WIDGET_FACE), SetMinimalSize(92, 119), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, CW_WIDGET_FACE_TITLE), SetFill(1, 1), SetMinimalTextLines(2, 0),
			EndContainer(),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_VERTICAL), SetPIP(4, 5, 5),
						NWidget(WWT_TEXT, COLOUR_GREY, CW_WIDGET_DESC_INAUGURATION), SetDataTip(STR_COMPANY_VIEW_INAUGURATED_TITLE, STR_NULL), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL), SetPIP(0, 5, 0),
							NWidget(WWT_LABEL, COLOUR_GREY, CW_WIDGET_DESC_COLOUR_SCHEME), SetDataTip(STR_COMPANY_VIEW_COLOUR_SCHEME_TITLE, STR_NULL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, CW_WIDGET_DESC_COLOUR_SCHEME_EXAMPLE), SetMinimalSize(30, 0), SetFill(0, 1),
							NWidget(NWID_SPACER), SetFill(1, 0),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
							NWidget(NWID_VERTICAL),
								NWidget(WWT_TEXT, COLOUR_GREY, CW_WIDGET_DESC_VEHICLE), SetDataTip(STR_COMPANY_VIEW_VEHICLES_TITLE, STR_NULL),
								NWidget(NWID_SPACER), SetFill(0, 1),
							EndContainer(),
							NWidget(WWT_EMPTY, INVALID_COLOUR, CW_WIDGET_DESC_VEHICLE_COUNTS), SetMinimalTextLines(4, 0),
							NWidget(NWID_SPACER), SetFill(1, 0),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VERTICAL), SetPIP(4, 2, 4),
						NWidget(NWID_SELECTION, INVALID_COLOUR, CW_WIDGET_SELECT_VIEW_BUILD_HQ),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_VIEW_HQ), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_VIEW_HQ_BUTTON, STR_COMPANY_VIEW_VIEW_HQ_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, CW_WIDGET_BUILD_HQ), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_BUILD_HQ_BUTTON, STR_COMPANY_VIEW_BUILD_HQ_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, CW_WIDGET_SELECT_RELOCATE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, CW_WIDGET_RELOCATE_HQ), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_RELOCATE_HQ, STR_COMPANY_VIEW_RELOCATE_COMPANY_HEADQUARTERS),
							NWidget(NWID_SPACER), SetMinimalSize(90, 0),
						EndContainer(),
						NWidget(NWID_SPACER), SetFill(0, 1),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXT, COLOUR_GREY, CW_WIDGET_DESC_COMPANY_VALUE), SetDataTip(STR_COMPANY_VIEW_COMPANY_VALUE, STR_NULL), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_VERTICAL), SetPIP(5, 5, 4),
						NWidget(WWT_EMPTY, INVALID_COLOUR, CW_WIDGET_DESC_OWNERS), SetMinimalTextLines(3, 0),
						NWidget(NWID_SPACER), SetFill(0, 1),
					EndContainer(),
					NWidget(NWID_VERTICAL), SetPIP(4, 2, 4),
						NWidget(NWID_SPACER), SetMinimalSize(90, 0), SetFill(0, 1),
						/* Multi player buttons. */
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, COLOUR_GREY, CW_WIDGET_HAS_PASSWORD),
							NWidget(NWID_SELECTION, INVALID_COLOUR, CW_WIDGET_SELECT_MULTIPLAYER),
								NWidget(NWID_SPACER), SetFill(1, 0),
								NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COMPANY_PASSWORD), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_PASSWORD, STR_COMPANY_VIEW_PASSWORD_TOOLTIP),
								NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COMPANY_JOIN), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_JOIN, STR_COMPANY_VIEW_JOIN_TOOLTIP),
							EndContainer(),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	/* Button bars at the bottom. */
	NWidget(NWID_SELECTION, INVALID_COLOUR, CW_WIDGET_SELECT_BUTTONS),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_NEW_FACE), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_NEW_FACE_BUTTON, STR_COMPANY_VIEW_NEW_FACE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COLOUR_SCHEME), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_COLOUR_SCHEME_BUTTON, STR_COMPANY_VIEW_COLOUR_SCHEME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_PRESIDENT_NAME), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_PRESIDENT_NAME_BUTTON, STR_COMPANY_VIEW_PRESIDENT_NAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COMPANY_NAME), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_COMPANY_NAME_BUTTON, STR_COMPANY_VIEW_COMPANY_NAME_TOOLTIP),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_BUY_SHARE), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_BUY_SHARE_BUTTON, STR_COMPANY_VIEW_BUY_SHARE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_SELL_SHARE), SetFill(1, 0), SetDataTip(STR_COMPANY_VIEW_SELL_SHARE_BUTTON, STR_COMPANY_VIEW_SELL_SHARE_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

int GetAmountOwnedBy(const Company *c, Owner owner)
{
	return (c->share_owners[0] == owner) +
				 (c->share_owners[1] == owner) +
				 (c->share_owners[2] == owner) +
				 (c->share_owners[3] == owner);
}

/** Strings for the company vehicle counts */
static const StringID _company_view_vehicle_count_strings[] = {
	STR_COMPANY_VIEW_TRAINS, STR_COMPANY_VIEW_ROAD_VEHICLES, STR_COMPANY_VIEW_SHIPS, STR_COMPANY_VIEW_AIRCRAFT
};

/**
 * Window with general information about a company
 */
struct CompanyWindow : Window
{
	CompanyWindowWidgets query_widget;

	/** Display planes in the company window. */
	enum CompanyWindowPlanes {
		/* Display planes of the #CW_WIDGET_SELECT_MULTIPLAYER selection widget. */
		CWP_MP_EMPTY = 0, ///< Do not display any multiplayer button.
		CWP_MP_C_PWD,     ///< Display the company password button.
		CWP_MP_C_JOIN,    ///< Display the join company button.

		/* Display planes of the #CW_WIDGET_SELECT_VIEW_BUILD_HQ selection widget. */
		CWP_VB_VIEW = 0,  ///< Display the view button
		CWP_VB_BUILD,     ///< Display the build button

		/* Display planes of the #CW_WIDGET_SELECT_RELOCATE selection widget. */
		CWP_RELOCATE_SHOW = 0, ///< Show the relocate HQ button.
		CWP_RELOCATE_HIDE,     ///< Hide the relocate HQ button.

		/* Display planes of the #CW_WIDGET_SELECT_BUTTONS selection widget. */
		CWP_BUTTONS_LOCAL = 0, ///< Buttons of the local company.
		CWP_BUTTONS_OTHER,     ///< Buttons of the other companies.
	};

	CompanyWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		this->owner = (Owner)this->window_number;
	}

	virtual void OnPaint()
	{
		const Company *c = Company::Get((CompanyID)this->window_number);
		bool local = this->window_number == _local_company;

		if (!this->IsShaded()) {
			/* Button bar selection. */
			int plane = local ? CWP_BUTTONS_LOCAL : CWP_BUTTONS_OTHER;
			NWidgetStacked *wi = this->GetWidget<NWidgetStacked>(CW_WIDGET_SELECT_BUTTONS);
			if (plane != wi->shown_plane) {
				wi->SetDisplayedPlane(plane);
				this->SetDirty();
				return;
			}

			/* Build HQ button handling. */
			plane = (local && c->location_of_HQ == INVALID_TILE) ? CWP_VB_BUILD : CWP_VB_VIEW;
			wi = this->GetWidget<NWidgetStacked>(CW_WIDGET_SELECT_VIEW_BUILD_HQ);
			if (plane != wi->shown_plane) {
				wi->SetDisplayedPlane(plane);
				this->SetDirty();
				return;
			}

			this->SetWidgetDisabledState(CW_WIDGET_VIEW_HQ, c->location_of_HQ == INVALID_TILE);

			/* Enable/disable 'Relocate HQ' button. */
			plane = (!local || c->location_of_HQ == INVALID_TILE) ? CWP_RELOCATE_HIDE : CWP_RELOCATE_SHOW;
			wi = this->GetWidget<NWidgetStacked>(CW_WIDGET_SELECT_RELOCATE);
			if (plane != wi->shown_plane) {
				wi->SetDisplayedPlane(plane);
				this->SetDirty();
				return;
			}

			/* Multiplayer buttons. */
			plane = ((!_networking) ? CWP_MP_EMPTY : (local ? CWP_MP_C_PWD : CWP_MP_C_JOIN));
			wi = this->GetWidget<NWidgetStacked>(CW_WIDGET_SELECT_MULTIPLAYER);
			if (plane != wi->shown_plane) {
				wi->SetDisplayedPlane(plane);
				this->SetDirty();
				return;
			}
			this->SetWidgetDisabledState(CW_WIDGET_COMPANY_JOIN,   c->is_ai);
		}

		if (!local) {
			if (_settings_game.economy.allow_shares) { // Shares are allowed
				/* If all shares are owned by someone (none by nobody), disable buy button */
				this->SetWidgetDisabledState(CW_WIDGET_BUY_SHARE, GetAmountOwnedBy(c, INVALID_OWNER) == 0 ||
						/* Only 25% left to buy. If the company is human, disable buying it up.. TODO issues! */
						(GetAmountOwnedBy(c, INVALID_OWNER) == 1 && !c->is_ai) ||
						/* Spectators cannot do anything of course */
						_local_company == COMPANY_SPECTATOR);

				/* If the company doesn't own any shares, disable sell button */
				this->SetWidgetDisabledState(CW_WIDGET_SELL_SHARE, (GetAmountOwnedBy(c, _local_company) == 0) ||
						/* Spectators cannot do anything of course */
						_local_company == COMPANY_SPECTATOR);
			} else { // Shares are not allowed, disable buy/sell buttons
				this->DisableWidget(CW_WIDGET_BUY_SHARE);
				this->DisableWidget(CW_WIDGET_SELL_SHARE);
			}
		}

		this->DrawWidgets();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case CW_WIDGET_DESC_COMPANY_VALUE:
				SetDParam(0, INT64_MAX); // Arguably the maximum company value
				size->width = GetStringBoundingBox(STR_COMPANY_VIEW_COMPANY_VALUE).width;
				break;

			case CW_WIDGET_DESC_VEHICLE_COUNTS:
				SetDParam(0, 5000); // Maximum number of vehicles
				for (uint i = 0; i < lengthof(_company_view_vehicle_count_strings); i++) {
					size->width = max(size->width, GetStringBoundingBox(_company_view_vehicle_count_strings[i]).width);
				}
				break;

			case CW_WIDGET_DESC_OWNERS: {
				const Company *c2;

				FOR_ALL_COMPANIES(c2) {
					SetDParam(0, 25);
					SetDParam(1, c2->index);

					size->width = max(size->width, GetStringBoundingBox(STR_COMPANY_VIEW_SHARES_OWNED_BY).width);
				}
				break;
			}

#ifdef ENABLE_NETWORK
			case CW_WIDGET_HAS_PASSWORD:
				*size = maxdim(*size, GetSpriteSize(SPR_LOCK));
				break;
#endif /* ENABLE_NETWORK */
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		const Company *c = Company::Get((CompanyID)this->window_number);
		switch (widget) {
			case CW_WIDGET_FACE:
				DrawCompanyManagerFace(c->face, c->colour, r.left, r.top);
				break;

			case CW_WIDGET_FACE_TITLE:
				SetDParam(0, c->index);
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, STR_COMPANY_VIEW_PRESIDENT_MANAGER_TITLE, TC_FROMSTRING, SA_CENTER);
				break;

			case CW_WIDGET_DESC_COLOUR_SCHEME_EXAMPLE:
				DrawSprite(SPR_VEH_BUS_SW_VIEW, COMPANY_SPRITE_COLOUR(c->index), (r.left + r.right) / 2, r.top + FONT_HEIGHT_NORMAL / 10);
				break;

			case CW_WIDGET_DESC_VEHICLE_COUNTS: {
				uint amounts[4];
				CountCompanyVehicles((CompanyID)this->window_number, amounts);

				int y = r.top;
				if (amounts[0] + amounts[1] + amounts[2] + amounts[3] == 0) {
					DrawString(r.left, r.right, y, STR_COMPANY_VIEW_VEHICLES_NONE);
				} else {
					assert_compile(lengthof(amounts) == lengthof(_company_view_vehicle_count_strings));

					for (uint i = 0; i < lengthof(amounts); i++) {
						if (amounts[i] != 0) {
							SetDParam(0, amounts[i]);
							DrawString(r.left, r.right, y, _company_view_vehicle_count_strings[i]);
							y += FONT_HEIGHT_NORMAL;
						}
					}
				}
				break;
			}

			case CW_WIDGET_DESC_OWNERS: {
				const Company *c2;
				uint y = r.top;

				FOR_ALL_COMPANIES(c2) {
					uint amt = GetAmountOwnedBy(c, c2->index);
					if (amt != 0) {
						SetDParam(0, amt * 25);
						SetDParam(1, c2->index);

						DrawString(r.left, r.right, y, STR_COMPANY_VIEW_SHARES_OWNED_BY);
						y += FONT_HEIGHT_NORMAL;
					}
				}
				break;
			}

#ifdef ENABLE_NETWORK
			case CW_WIDGET_HAS_PASSWORD:
				if (_networking && NetworkCompanyIsPassworded(c->index)) {
					DrawSprite(SPR_LOCK, PAL_NONE, r.left, r.top);
				}
				break;
#endif /* ENABLE_NETWORK */
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case CW_WIDGET_CAPTION:
				SetDParam(0, (CompanyID)this->window_number);
				SetDParam(1, (CompanyID)this->window_number);
				break;

			case CW_WIDGET_DESC_INAUGURATION:
				SetDParam(0, Company::Get((CompanyID)this->window_number)->inaugurated_year);
				break;

			case CW_WIDGET_DESC_COMPANY_VALUE:
				SetDParam(0, CalculateCompanyValue(Company::Get((CompanyID)this->window_number)));
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case CW_WIDGET_NEW_FACE: DoSelectCompanyManagerFace(this); break;

			case CW_WIDGET_COLOUR_SCHEME:
				if (BringWindowToFrontById(WC_COMPANY_COLOUR, this->window_number)) break;
				new SelectCompanyLiveryWindow(&_select_company_livery_desc, (CompanyID)this->window_number);
				break;

			case CW_WIDGET_PRESIDENT_NAME:
				this->query_widget = CW_WIDGET_PRESIDENT_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_PRESIDENT_NAME, STR_COMPANY_VIEW_PRESIDENT_S_NAME_QUERY_CAPTION, MAX_LENGTH_PRESIDENT_NAME_CHARS, MAX_LENGTH_PRESIDENT_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case CW_WIDGET_COMPANY_NAME:
				this->query_widget = CW_WIDGET_COMPANY_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_COMPANY_NAME, STR_COMPANY_VIEW_COMPANY_NAME_QUERY_CAPTION, MAX_LENGTH_COMPANY_NAME_CHARS, MAX_LENGTH_COMPANY_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT | QSF_LEN_IN_CHARS);
				break;

			case CW_WIDGET_VIEW_HQ: {
				TileIndex tile = Company::Get((CompanyID)this->window_number)->location_of_HQ;
				if (_ctrl_pressed) {
					ShowExtraViewPortWindow(tile);
				} else {
					ScrollMainWindowToTile(tile);
				}
				break;
			}

			case CW_WIDGET_BUILD_HQ:
				if ((byte)this->window_number != _local_company) return;
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, HT_RECT, this);
				SetTileSelectSize(2, 2);
				this->LowerWidget(CW_WIDGET_BUILD_HQ);
				this->SetWidgetDirty(CW_WIDGET_BUILD_HQ);
				break;

			case CW_WIDGET_RELOCATE_HQ:
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, HT_RECT, this);
				SetTileSelectSize(2, 2);
				this->LowerWidget(CW_WIDGET_RELOCATE_HQ);
				this->SetWidgetDirty(CW_WIDGET_RELOCATE_HQ);
				break;

			case CW_WIDGET_BUY_SHARE:
				DoCommandP(0, this->window_number, 0, CMD_BUY_SHARE_IN_COMPANY | CMD_MSG(STR_ERROR_CAN_T_BUY_25_SHARE_IN_THIS));
				break;

			case CW_WIDGET_SELL_SHARE:
				DoCommandP(0, this->window_number, 0, CMD_SELL_SHARE_IN_COMPANY | CMD_MSG(STR_ERROR_CAN_T_SELL_25_SHARE_IN));
				break;

#ifdef ENABLE_NETWORK
			case CW_WIDGET_COMPANY_PASSWORD:
				if (this->window_number == _local_company) ShowNetworkCompanyPasswordWindow(this);
				break;

			case CW_WIDGET_COMPANY_JOIN: {
				this->query_widget = CW_WIDGET_COMPANY_JOIN;
				CompanyID company = (CompanyID)this->window_number;
				if (_network_server) {
					NetworkServerDoMove(CLIENT_ID_SERVER, company);
					MarkWholeScreenDirty();
				} else if (NetworkCompanyIsPassworded(company)) {
					/* ask for the password */
					ShowQueryString(STR_EMPTY, STR_NETWORK_NEED_COMPANY_PASSWORD_CAPTION, NETWORK_PASSWORD_LENGTH, 180, this, CS_ALPHANUMERAL, QSF_NONE);
				} else {
					/* just send the join command */
					NetworkClientRequestMove(company);
				}
				break;
			}
#endif /* ENABLE_NETWORK */
		}
	}

	virtual void OnHundredthTick()
	{
		/* redraw the window every now and then */
		this->SetDirty();
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		if (DoCommandP(tile, OBJECT_HQ, 0, CMD_BUILD_OBJECT | CMD_MSG(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS))) {
			ResetObjectToPlace();
			this->RaiseButtons();
		}
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		switch (this->query_widget) {
			default: NOT_REACHED();

			case CW_WIDGET_PRESIDENT_NAME:
				DoCommandP(0, 0, 0, CMD_RENAME_PRESIDENT | CMD_MSG(STR_ERROR_CAN_T_CHANGE_PRESIDENT), NULL, str);
				break;

			case CW_WIDGET_COMPANY_NAME:
				DoCommandP(0, 0, 0, CMD_RENAME_COMPANY | CMD_MSG(STR_ERROR_CAN_T_CHANGE_COMPANY_NAME), NULL, str);
				break;

#ifdef ENABLE_NETWORK
			case CW_WIDGET_COMPANY_JOIN:
				NetworkClientRequestMove((CompanyID)this->window_number, str);
				break;
#endif /* ENABLE_NETWORK */
		}
	}
};

static const WindowDesc _company_desc(
	WDP_AUTO, 0, 0,
	WC_COMPANY, WC_NONE,
	WDF_UNCLICK_BUTTONS,
	_nested_company_widgets, lengthof(_nested_company_widgets)
);

void ShowCompany(CompanyID company)
{
	if (!Company::IsValidID(company)) return;

	AllocateWindowDescFront<CompanyWindow>(&_company_desc, company);
}

/** widget numbers of the #BuyCompanyWindow. */
enum BuyCompanyWidgets {
	BCW_CAPTION,
	BCW_FACE,
	BCW_QUESTION,
	BCW_NO,
	BCW_YES,
};

struct BuyCompanyWindow : Window {
	BuyCompanyWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case BCW_FACE:
				*size = GetSpriteSize(SPR_GRADIENT);
				break;

			case BCW_QUESTION:
				const Company *c = Company::Get((CompanyID)this->window_number);
				SetDParam(0, c->index);
				SetDParam(1, c->bankrupt_value);
				size->height = GetStringHeight(STR_BUY_COMPANY_MESSAGE, size->width);
				break;
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case BCW_CAPTION:
				SetDParam(0, STR_COMPANY_NAME);
				SetDParam(1, Company::Get((CompanyID)this->window_number)->index);
				break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case BCW_FACE: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				DrawCompanyManagerFace(c->face, c->colour, r.left, r.top);
				break;
			}

			case BCW_QUESTION: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				SetDParam(0, c->index);
				SetDParam(1, c->bankrupt_value);
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, STR_BUY_COMPANY_MESSAGE, TC_FROMSTRING, SA_CENTER);
				break;
			}
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case BCW_NO:
				delete this;
				break;

			case BCW_YES:
				DoCommandP(0, this->window_number, 0, CMD_BUY_COMPANY | CMD_MSG(STR_ERROR_CAN_T_BUY_COMPANY));
				break;
		}
	}
};

static const NWidgetPart _nested_buy_company_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, BCW_CAPTION), SetDataTip(STR_ERROR_MESSAGE_CAPTION_OTHER_COMPANY, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(NWID_VERTICAL), SetPIP(8, 8, 8),
			NWidget(NWID_HORIZONTAL), SetPIP(8, 10, 8),
				NWidget(WWT_EMPTY, INVALID_COLOUR, BCW_FACE), SetFill(0, 1),
				NWidget(WWT_EMPTY, INVALID_COLOUR, BCW_QUESTION), SetMinimalSize(240, 0), SetFill(1, 1),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(100, 10, 100),
				NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, BCW_NO), SetMinimalSize(60, 12), SetDataTip(STR_QUIT_NO, STR_NULL), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, BCW_YES), SetMinimalSize(60, 12), SetDataTip(STR_QUIT_YES, STR_NULL), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _buy_company_desc(
	WDP_AUTO, 0, 0,
	WC_BUY_COMPANY, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_buy_company_widgets, lengthof(_nested_buy_company_widgets)
);


void ShowBuyCompanyDialog(CompanyID company)
{
	AllocateWindowDescFront<BuyCompanyWindow>(&_buy_company_desc, company);
}
