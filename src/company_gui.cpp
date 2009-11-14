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
#include "gfx_func.h"
#include "company_func.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"
#include "sprite.h"
#include "economy_func.h"
#include "vehicle_base.h"
#include "newgrf.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "date_func.h"
#include "widgets/dropdown_type.h"
#include "tilehighlight_func.h"

#include "table/strings.h"

/** Company GUI constants. */
enum {
	FIRST_GUI_CALL = INT_MAX,  ///< default value to specify this is the first call of the resizable gui

	EXP_LINESPACE  = 2,        ///< Amount of vertical space for a horizontal (sub-)total line.
	EXP_BLOCKSPACE = 10,       ///< Amount of vertical space between two blocks of numbers.
};

static void DoSelectCompanyManagerFace(Window *parent, bool show_big, int top =  FIRST_GUI_CALL, int left = FIRST_GUI_CALL);

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
	CFW_CLOSEBOX = 0,  ///< Close the window
	CFW_CAPTION,       ///< Caption of the window
	CFW_TOGGLE_SIZE,   ///< Toggle windows size
	CFW_STICKY,        ///< Sticky button
	CFW_EXPS_PANEL,    ///< Panel for expenses
	CFW_SEL_PANEL,     ///< Select panel or nothing
	CFW_EXPS_CATEGORY, ///< Column for expenses category strings
	CFW_EXPS_PRICE1,   ///< Column for year Y-2 expenses
	CFW_EXPS_PRICE2,   ///< Column for year Y-1 expenses
	CFW_EXPS_PRICE3,   ///< Column for year Y expenses
	CFW_TOTAL_PANEL,   ///< Panel for totals
	CFW_SEL_MAXLOAN,   ///< Selection of maxloan column
	CFW_BALANCE_TITLE, ///< 'Bank balance' title
	CFW_LOAN_TITLE,    ///< 'Loan' title
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

/** Draw the expenses categories.
 * @param r Available space for drawing.
 * @note The environment must provide padding at the left and right of \a r.
 */
static void DrawCategories(const Rect &r)
{
	int y = r.top;

	DrawString(r.left, r.right, y, STR_FINANCES_EXPENDITURE_INCOME_TITLE, TC_FROMSTRING, SA_CENTER, true);
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

/** Draw an amount of money.
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

/** Draw a column with prices.
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
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CFW_CLOSEBOX), SetFill(false, true),
		NWidget(WWT_CAPTION, COLOUR_GREY, CFW_CAPTION), SetDataTip(STR_FINANCES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, CFW_TOGGLE_SIZE), SetMinimalSize(14, 14), SetFill(false, true), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_TOOLTIP_TOGGLE_LARGE_SMALL_WINDOW),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, CFW_STICKY), SetFill(false, true),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, CFW_SEL_PANEL),
		NWidget(WWT_PANEL, COLOUR_GREY, CFW_EXPS_PANEL),
			NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT), SetPIP(0, 9, 0),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_CATEGORY), SetMinimalSize(120, 0), SetFill(false, false),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_PRICE1), SetMinimalSize(86, 0), SetFill(false, false),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_PRICE2), SetMinimalSize(86, 0), SetFill(false, false),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_EXPS_PRICE3), SetMinimalSize(86, 0), SetFill(false, false),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CFW_TOTAL_PANEL),
		NWidget(NWID_HORIZONTAL), SetPadding(WD_FRAMERECT_TOP, WD_FRAMERECT_RIGHT, WD_FRAMERECT_BOTTOM, WD_FRAMERECT_LEFT),
			NWidget(NWID_VERTICAL), // Vertical column with 'bank balance', 'loan'
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_BALANCE_TITLE), SetDataTip(STR_FINANCES_BANK_BALANCE_TITLE, STR_NULL), SetFill(true, false),
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_LOAN_TITLE), SetDataTip(STR_FINANCES_LOAN_TITLE, STR_NULL), SetFill(true, false),
				NWidget(NWID_SPACER), SetFill(false, true),
			EndContainer(),
			NWidget(NWID_SPACER), SetFill(false, false), SetMinimalSize(30, 0),
			NWidget(NWID_VERTICAL), // Vertical column with bank balance amount, loan amount, and total.
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_BALANCE_VALUE), SetDataTip(STR_NULL, STR_NULL),
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_LOAN_VALUE), SetDataTip(STR_NULL, STR_NULL),
				NWidget(WWT_EMPTY, COLOUR_GREY, CFW_LOAN_LINE), SetMinimalSize(0, 2), SetFill(true, false),
				NWidget(WWT_TEXT, COLOUR_GREY, CFW_TOTAL_VALUE), SetDataTip(STR_NULL, STR_NULL),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, CFW_SEL_MAXLOAN),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_SPACER), SetFill(false, true), SetMinimalSize(25, 0),
					NWidget(NWID_VERTICAL), // Max loan information
						NWidget(WWT_EMPTY, COLOUR_GREY, CFW_MAXLOAN_GAP), SetFill(false, false),
						NWidget(WWT_TEXT, COLOUR_GREY, CFW_MAXLOAN_VALUE), SetDataTip(STR_FINANCES_MAX_LOAN, STR_NULL),
						NWidget(NWID_SPACER), SetFill(false, true),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetFill(true, true),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, CFW_SEL_BUTTONS),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CFW_INCREASE_LOAN), SetFill(true, false), SetDataTip(STR_FINANCES_BORROW_BUTTON, STR_FINANCES_BORROW_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CFW_REPAY_LOAN), SetFill(true, false), SetDataTip(STR_FINANCES_REPAY_BUTTON, STR_FINANCES_REPAY_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

/** Window class displaying the company finances.
 * @todo #money_width should be calculated dynamically.
 */
struct CompanyFinancesWindow : Window {
	bool small;       ///< Window is toggled to 'small'.
	uint money_width; ///< Width needed for displaying all amounts.

	CompanyFinancesWindow(const WindowDesc *desc, CompanyID company) : Window()
	{
		this->small = false;
		this->money_width = 86;
		this->CreateNestedTree(desc);
		this->SetupWidgets();
		this->FinishInitNested(desc, company);

		this->owner = (Owner)this->window_number;
	}

	virtual void SetStringParameters(int widget) const
	{
		const Company *c = Company::Get((CompanyID)this->window_number);
		switch (widget) {
			case CFW_CAPTION:
				SetDParam(0, c->index);
				SetDParam(1, c->index);
				break;

			case CFW_MAXLOAN_VALUE:
				SetDParam(0, _economy.max_loan);
				break;

			case CFW_INCREASE_LOAN:
			case CFW_REPAY_LOAN:
				SetDParam(2, LOAN_INTERVAL);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
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
				size->width  = this->money_width;
				size->height = _expenses_list_types[type].GetHeight();
				break;

			case CFW_BALANCE_VALUE:
			case CFW_LOAN_VALUE:
			case CFW_TOTAL_VALUE:
				size->width  = this->money_width + padding.width;
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

	/** Setup the widgets in the nested tree, such that the finances window is displayed properly.
	 * @note After setup, the window must be (re-)initialized.
	 */
	void SetupWidgets()
	{
		int plane = small ? STACKED_SELECTION_ZERO_SIZE : 0;
		this->GetWidget<NWidgetStacked>(CFW_SEL_PANEL)->SetDisplayedPlane(plane);
		this->GetWidget<NWidgetStacked>(CFW_SEL_MAXLOAN)->SetDisplayedPlane(plane);

		CompanyID company = (CompanyID)this->window_number;
		plane = (company != _local_company) ? STACKED_SELECTION_ZERO_SIZE : 0;
		this->GetWidget<NWidgetStacked>(CFW_SEL_BUTTONS)->SetDisplayedPlane(plane);
	}

	virtual void OnPaint()
	{
		if (!small) {
			/* Check that the expenses panel height matches the height needed for the layout. */
			int type = _settings_client.gui.expenses_layout;
			if (_expenses_list_types[type].GetHeight() != this->GetWidget<NWidgetCore>(CFW_EXPS_CATEGORY)->current_y) {
				this->SetupWidgets();
				this->ReInit();
				return;
			}
		}

		/* Check that the loan buttons are shown only when the user owns the company. */
		CompanyID company = (CompanyID)this->window_number;
		int req_plane = (company != _local_company) ? STACKED_SELECTION_ZERO_SIZE : 0;
		if (req_plane != this->GetWidget<NWidgetStacked>(CFW_SEL_BUTTONS)->shown_plane) {
			this->SetupWidgets();
			this->ReInit();
			return;
		}

		const Company *c = Company::Get(company);
		this->SetWidgetDisabledState(CFW_INCREASE_LOAN, c->current_loan == _economy.max_loan); // Borrow button only shows when there is any more money to loan.
		this->SetWidgetDisabledState(CFW_REPAY_LOAN, company != _local_company || c->current_loan == 0); // Repay button only shows when there is any more money to repay.

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case CFW_TOGGLE_SIZE: // toggle size
				this->small = !this->small;
				this->SetupWidgets();
				this->ReInit();
				break;

			case CFW_INCREASE_LOAN: // increase loan
				DoCommandP(0, 0, _ctrl_pressed, CMD_INCREASE_LOAN | CMD_MSG(STR_ERROR_CAN_T_BORROW_ANY_MORE_MONEY));
				break;

			case CFW_REPAY_LOAN: // repay loan
				DoCommandP(0, 0, _ctrl_pressed, CMD_DECREASE_LOAN | CMD_MSG(STR_ERROR_CAN_T_REPAY_LOAN));
				break;
		}
	}
};

static const WindowDesc _company_finances_desc(
	WDP_AUTO, WDP_AUTO, 0, 0, 0, 0,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	NULL, _nested_company_finances_widgets, lengthof(_nested_company_finances_widgets)
);

/** Open the finances window of a company.
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
		DrawSprite(SPR_VEH_BUS_SIDE_VIEW, PALETTE_RECOLOUR_START + this->result, left + 16, top + 7);
		DrawString(left + 32, right - 2, top + max(0, 13 - FONT_HEIGHT_NORMAL), this->String(), sel ? TC_WHITE : TC_BLACK);
	}
};

/** Widgets of the select company livery window. */
enum SelectCompanyLiveryWindowWidgets {
	SCLW_WIDGET_CLOSE,
	SCLW_WIDGET_CAPTION,
	SCLW_WIDGET_CLASS_GENERAL,
	SCLW_WIDGET_CLASS_RAIL,
	SCLW_WIDGET_CLASS_ROAD,
	SCLW_WIDGET_CLASS_SHIP,
	SCLW_WIDGET_CLASS_AIRCRAFT,
	SCLW_WIDGET_SPACER_CLASS,
	SCLW_WIDGET_SPACER_DROPDOWN,
	SCLW_WIDGET_PRI_COL_DROPDOWN,
	SCLW_WIDGET_SEC_COL_DROPDOWN,
	SCLW_WIDGET_MATRIX,
};

/** Company livery colour scheme window. */
struct SelectCompanyLiveryWindow : public Window {
private:
	static const int TEXT_INDENT = 15; ///< Number of pixels to indent the text in each column in the #SCLW_WIDGET_MATRIX to make room for the (coloured) rectangles.
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
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		/* Number of liveries in each class, used to determine the height of the livery matrix widget. */
		static const byte livery_height[] = {
			1,
			13,
			4,
			2,
			3,
		};

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

			case SCLW_WIDGET_MATRIX:
				size->height = livery_height[this->livery_class] * (4 + FONT_HEIGHT_NORMAL);
				this->GetWidget<NWidgetCore>(SCLW_WIDGET_MATRIX)->widget_data = (livery_height[this->livery_class] << MAT_ROW_START) | (1 << MAT_COL_START);
				break;

			case SCLW_WIDGET_SEC_COL_DROPDOWN:
				if (!_loaded_newgrf_features.has_2CC) size->width = 0;
				break;
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

		int y = r.top + 3;
		const Company *c = Company::Get((CompanyID)this->window_number);
		for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
			if (_livery_class[scheme] == this->livery_class) {
				bool sel = HasBit(this->sel, scheme) != 0;

				/* Optional check box + scheme name. */
				if (scheme != LS_DEFAULT) {
					DrawSprite(c->livery[scheme].in_use ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, sch_left + WD_FRAMERECT_LEFT, y);
				}
				DrawString(sch_left + TEXT_INDENT, sch_right - WD_FRAMERECT_RIGHT, y, STR_LIVERY_DEFAULT + scheme, sel ? TC_WHITE : TC_BLACK);

				/* Text below the first dropdown. */
				DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOUR(c->livery[scheme].colour1), pri_left + WD_FRAMERECT_LEFT, y);
				DrawString(pri_left + TEXT_INDENT, pri_right - WD_FRAMERECT_RIGHT, y, STR_COLOUR_DARK_BLUE + c->livery[scheme].colour1, sel ? TC_WHITE : TC_GOLD);

				/* Text below the second dropdown. */
				if (sec_right > sec_left) { // Second dropdown has non-zero size.
					DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOUR(c->livery[scheme].colour2), sec_left + WD_FRAMERECT_LEFT, y);
					DrawString(sec_left + TEXT_INDENT, sec_right - WD_FRAMERECT_RIGHT, y, STR_COLOUR_DARK_BLUE + c->livery[scheme].colour2, sel ? TC_WHITE : TC_GOLD);
				}

				y += 4 + FONT_HEIGHT_NORMAL;
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
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
					if (_livery_class[scheme] == this->livery_class) {
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
				LiveryScheme j = (LiveryScheme)((pt.y - this->GetWidget<NWidgetBase>(SCLW_WIDGET_MATRIX)->pos_y) / (4 + FONT_HEIGHT_NORMAL));

				for (LiveryScheme scheme = LS_BEGIN; scheme <= j; scheme++) {
					if (_livery_class[scheme] != this->livery_class) j++;
					if (scheme >= LS_END) return;
				}
				if (j >= LS_END) return;

				/* If clicking on the left edge, toggle using the livery */
				if (pt.x < 10) {
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
		this->ReInit();
	}
};

static const NWidgetPart _nested_select_company_livery_widgets [] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, SCLW_WIDGET_CLOSE),
		NWidget(WWT_CAPTION, COLOUR_GREY, SCLW_WIDGET_CAPTION), SetMinimalSize(250, 14), SetDataTip(STR_LIVERY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_GENERAL), SetMinimalSize(22, 22), SetFill(false, true), SetDataTip(SPR_IMG_COMPANY_GENERAL, STR_LIVERY_GENERAL_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_RAIL), SetMinimalSize(22, 22), SetFill(false, true), SetDataTip(SPR_IMG_TRAINLIST, STR_LIVERY_TRAIN_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_ROAD), SetMinimalSize(22, 22), SetFill(false, true), SetDataTip(SPR_IMG_TRUCKLIST, STR_LIVERY_ROAD_VEHICLE_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_SHIP), SetMinimalSize(22, 22), SetFill(false, true), SetDataTip(SPR_IMG_SHIPLIST, STR_LIVERY_SHIP_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCLW_WIDGET_CLASS_AIRCRAFT), SetMinimalSize(22, 22), SetFill(false, true), SetDataTip(SPR_IMG_AIRPLANESLIST, STR_LIVERY_AIRCRAFT_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY, SCLW_WIDGET_SPACER_CLASS), SetMinimalSize(90, 22), SetFill(true, true), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, SCLW_WIDGET_SPACER_DROPDOWN), SetMinimalSize(150, 12), SetFill(true, true), EndContainer(),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, SCLW_WIDGET_PRI_COL_DROPDOWN), SetMinimalSize(125, 12), SetFill(false, true), SetDataTip(STR_BLACK_STRING, STR_LIVERY_PRIMARY_TOOLTIP),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, SCLW_WIDGET_SEC_COL_DROPDOWN), SetMinimalSize(125, 12), SetFill(false, true),
				SetDataTip(STR_BLACK_STRING, STR_LIVERY_SECONDARY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_MATRIX, COLOUR_GREY, SCLW_WIDGET_MATRIX), SetMinimalSize(275, 15), SetFill(true, false), SetDataTip((1 << MAT_ROW_START) | (1 << MAT_COL_START), STR_LIVERY_PANEL_TOOLTIP),
};

static const WindowDesc _select_company_livery_desc(
	WDP_AUTO, WDP_AUTO, 0, 0, 0, 0,
	WC_COMPANY_COLOUR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	NULL, _nested_select_company_livery_widgets, lengthof(_nested_select_company_livery_widgets)
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
	SpriteID pal;

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
	SCMFW_WIDGET_CLOSEBOX = 0,
	SCMFW_WIDGET_CAPTION,
	SCMFW_WIDGET_TOGGLE_LARGE_SMALL,
	SCMFW_WIDGET_SELECT_FACE,
	SCMFW_WIDGET_CANCEL,
	SCMFW_WIDGET_ACCEPT,
	SCMFW_WIDGET_MALE,
	SCMFW_WIDGET_FEMALE,
	SCMFW_WIDGET_RANDOM_NEW_FACE,
	SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON,
	SCMFM_WIDGET_FACE,
	/* from here is the advanced company manager face selection window */
	SCMFW_WIDGET_LOAD,
	SCMFW_WIDGET_FACECODE,
	SCMFW_WIDGET_SAVE,
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
	SCMFW_WIDGET_LABELS,
};

static const NWidgetPart _nested_select_company_manager_face_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, SCMFW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, SCMFW_WIDGET_CAPTION), SetMinimalSize(164, 14), SetDataTip(STR_FACE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCMFW_WIDGET_TOGGLE_LARGE_SMALL), SetMinimalSize(15, 14), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_FACE_ADVANCED_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SCMFW_WIDGET_SELECT_FACE),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_EMPTY, COLOUR_GREY, SCMFM_WIDGET_FACE), SetMinimalSize(92, 119), SetPadding(2, 0, 2, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_RANDOM_NEW_FACE), SetMinimalSize(92, 12), SetDataTip(STR_FACE_NEW_FACE_BUTTON, STR_FACE_NEW_FACE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON), SetMinimalSize(93, 12), SetDataTip(STR_FACE_ADVANCED, STR_FACE_ADVANCED_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 47),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_MALE), SetMinimalSize(93, 12), SetDataTip(STR_FACE_MALE_BUTTON, STR_FACE_MALE_TOOLTIP),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_FEMALE), SetMinimalSize(93, 12), SetDataTip(STR_FACE_FEMALE_BUTTON, STR_FACE_FEMALE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 52),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_CANCEL), SetMinimalSize(95, 12), SetDataTip(STR_BUTTON_CANCEL, STR_FACE_CANCEL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_ACCEPT), SetMinimalSize(95, 12), SetDataTip(STR_BUTTON_OK, STR_FACE_OK_TOOLTIP),
	EndContainer(),
};

/** Widget description for the normal/simple company manager face selection dialog */
static const Widget _select_company_manager_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,          STR_TOOLTIP_CLOSE_WINDOW},           // SCMFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   174,     0,    13, STR_FACE_CAPTION,         STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS}, // SCMFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   189,     0,    13, SPR_LARGE_SMALL_WINDOW,   STR_FACE_ADVANCED_TOOLTIP},              // SCMFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   189,    14,   150, 0x0,                      STR_NULL},                           // SCMFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    94,   151,   162, STR_BUTTON_CANCEL,        STR_FACE_CANCEL_TOOLTIP},            // SCMFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   189,   151,   162, STR_BUTTON_OK,            STR_FACE_OK_TOOLTIP},                // SCMFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    75,    86, STR_FACE_MALE_BUTTON,     STR_FACE_MALE_TOOLTIP},              // SCMFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    87,    98, STR_FACE_FEMALE_BUTTON,   STR_FACE_FEMALE_TOOLTIP},            // SCMFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   137,   148, STR_FACE_NEW_FACE_BUTTON, STR_FACE_NEW_FACE_TOOLTIP},          // SCMFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    16,    27, STR_FACE_ADVANCED,        STR_FACE_ADVANCED_TOOLTIP},              // SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     2,    93,    16,   134, 0x0,                      STR_NULL},                           // SCMFW_WIDGET_FACE
{   WIDGETS_END},
};

static const NWidgetPart _nested_select_company_manager_face_adv_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, SCMFW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, SCMFW_WIDGET_CAPTION), SetMinimalSize(194, 14), SetDataTip(STR_FACE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, SCMFW_WIDGET_TOGGLE_LARGE_SMALL), SetMinimalSize(15, 14), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_FACE_SIMPLE_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, SCMFW_WIDGET_SELECT_FACE),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_EMPTY, COLOUR_GREY, SCMFM_WIDGET_FACE), SetMinimalSize(92, 119), SetPadding(2, 0, 2, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_RANDOM_NEW_FACE), SetMinimalSize(92, 12), SetDataTip(STR_MAPGEN_RANDOM, STR_FACE_NEW_FACE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 9),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_LOAD), SetMinimalSize(92, 12), SetDataTip(STR_FACE_LOAD, STR_FACE_LOAD_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_FACECODE), SetMinimalSize(92, 12), SetDataTip(STR_FACE_FACECODE, STR_FACE_FACECODE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_SAVE), SetMinimalSize(92, 12), SetDataTip(STR_FACE_SAVE, STR_FACE_SAVE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 14),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON), SetMinimalSize(123, 12), SetDataTip(STR_FACE_SIMPLE, STR_FACE_SIMPLE_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_SPACER), SetMinimalSize(1, 0),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_MALE), SetMinimalSize(61, 12), SetDataTip(STR_FACE_MALE_BUTTON, STR_FACE_MALE_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_FEMALE), SetMinimalSize(61, 12), SetDataTip(STR_FACE_FEMALE_BUTTON, STR_FACE_FEMALE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SPACER), SetMinimalSize(0, 2),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_ETHNICITY_EUR), SetMinimalSize(61, 12), SetDataTip(STR_FACE_EUROPEAN, STR_FACE_SELECT_EUROPEAN),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, SCMFW_WIDGET_ETHNICITY_AFR), SetMinimalSize(61, 12), SetDataTip(STR_FACE_AFRICAN, STR_FACE_SELECT_AFRICAN),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_EMPTY, COLOUR_GREY, SCMFW_WIDGET_LABELS), SetMinimalSize(75, 146), SetPadding(0, 4, 0, 1),
					NWidget(NWID_VERTICAL),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_HAS_MOUSTACHE_EARRING), SetMinimalSize(43, 12), SetDataTip(STR_EMPTY, STR_FACE_MOUSTACHE_EARRING_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_HAS_GLASSES), SetMinimalSize(43, 12), SetDataTip(STR_EMPTY, STR_FACE_GLASSES_TOOLTIP),
						NWidget(NWID_SPACER), SetMinimalSize(0, 2),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_HAIR_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_HAIR_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_HAIR), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_HAIR_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_HAIR_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_HAIR_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_EYEBROWS_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_EYEBROWS_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_EYEBROWS), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_EYEBROWS_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_EYEBROWS_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_EYEBROWS_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_EYECOLOUR_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_EYECOLOUR_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_EYECOLOUR), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_EYECOLOUR_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_EYECOLOUR_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_EYECOLOUR_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_GLASSES_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_GLASSES_TOOLTIP_2),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_GLASSES), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_GLASSES_TOOLTIP_2),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_GLASSES_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_GLASSES_TOOLTIP_2),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_NOSE_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_NOSE_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_NOSE), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_NOSE_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_NOSE_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_NOSE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_LIPS_MOUSTACHE_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_LIPS_MOUSTACHE_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_LIPS_MOUSTACHE), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_LIPS_MOUSTACHE_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_LIPS_MOUSTACHE_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_LIPS_MOUSTACHE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_CHIN_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_CHIN_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_CHIN), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_CHIN_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_CHIN_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_CHIN_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_JACKET_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_JACKET_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_JACKET), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_JACKET_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_JACKET_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_JACKET_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_COLLAR_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_COLLAR_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_COLLAR), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_COLLAR_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_COLLAR_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_COLLAR_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_TIE_EARRING_L), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_LEFT, STR_FACE_TIE_EARRING_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_TIE_EARRING), SetMinimalSize(25, 12), SetDataTip(STR_EMPTY, STR_FACE_TIE_EARRING_TOOLTIP),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, SCMFW_WIDGET_TIE_EARRING_R), SetMinimalSize(9, 12), SetDataTip(SPR_ARROW_RIGHT, STR_FACE_TIE_EARRING_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 2),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_CANCEL), SetMinimalSize(95, 12), SetDataTip(STR_BUTTON_CANCEL, STR_FACE_CANCEL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, SCMFW_WIDGET_ACCEPT), SetMinimalSize(125, 12), SetDataTip(STR_BUTTON_OK, STR_FACE_OK_TOOLTIP),
	EndContainer(),
};

/** Widget description for the advanced company manager face selection dialog */
static const Widget _select_company_manager_face_adv_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,         STR_TOOLTIP_CLOSE_WINDOW},           // SCMFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   204,     0,    13, STR_FACE_CAPTION,        STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS}, // SCMFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   205,   219,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_SIMPLE_TOOLTIP},                // SCMFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   219,    14,   207, 0x0,                     STR_NULL},                           // SCMFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    94,   208,   219, STR_BUTTON_CANCEL,       STR_FACE_CANCEL_TOOLTIP},            // SCMFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   219,   208,   219, STR_BUTTON_OK,           STR_FACE_OK_TOOLTIP},                // SCMFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    96,   156,    32,    43, STR_FACE_MALE_BUTTON,    STR_FACE_MALE_TOOLTIP},              // SCMFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   157,   217,    32,    43, STR_FACE_FEMALE_BUTTON,  STR_FACE_FEMALE_TOOLTIP},            // SCMFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   137,   148, STR_MAPGEN_RANDOM,       STR_FACE_NEW_FACE_TOOLTIP},          // SCMFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   217,    16,    27, STR_FACE_SIMPLE,         STR_FACE_SIMPLE_TOOLTIP},                // SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     2,    93,    16,   134, 0x0,                     STR_NULL},                           // SCMFW_WIDGET_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   158,   169, STR_FACE_LOAD,           STR_FACE_LOAD_TOOLTIP},                  // SCMFW_WIDGET_LOAD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   170,   181, STR_FACE_FACECODE,       STR_FACE_FACECODE_TOOLTIP},              // SCMFW_WIDGET_FACECODE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   182,   193, STR_FACE_SAVE,           STR_FACE_SAVE_TOOLTIP},                  // SCMFW_WIDGET_SAVE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    96,   156,    46,    57, STR_FACE_EUROPEAN,       STR_FACE_SELECT_EUROPEAN},           // SCMFW_WIDGET_ETHNICITY_EUR
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   157,   217,    46,    57, STR_FACE_AFRICAN,        STR_FACE_SELECT_AFRICAN},            // SCMFW_WIDGET_ETHNICITY_AFR
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   217,    60,    71, STR_EMPTY,               STR_FACE_MOUSTACHE_EARRING_TOOLTIP},     // SCMFW_WIDGET_HAS_MOUSTACHE_EARRING
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   217,    72,    83, STR_EMPTY,               STR_FACE_GLASSES_TOOLTIP},               // SCMFW_WIDGET_HAS_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   110,   121, SPR_ARROW_LEFT,          STR_FACE_EYECOLOUR_TOOLTIP},             // SCMFW_WIDGET_EYECOLOUR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   110,   121, STR_EMPTY,               STR_FACE_EYECOLOUR_TOOLTIP},             // SCMFW_WIDGET_EYECOLOUR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   110,   121, SPR_ARROW_RIGHT,         STR_FACE_EYECOLOUR_TOOLTIP},             // SCMFW_WIDGET_EYECOLOUR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   158,   169, SPR_ARROW_LEFT,          STR_FACE_CHIN_TOOLTIP},                  // SCMFW_WIDGET_CHIN_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   158,   169, STR_EMPTY,               STR_FACE_CHIN_TOOLTIP},                  // SCMFW_WIDGET_CHIN
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   158,   169, SPR_ARROW_RIGHT,         STR_FACE_CHIN_TOOLTIP},                  // SCMFW_WIDGET_CHIN_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,    98,   109, SPR_ARROW_LEFT,          STR_FACE_EYEBROWS_TOOLTIP},              // SCMFW_WIDGET_EYEBROWS_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,    98,   109, STR_EMPTY,               STR_FACE_EYEBROWS_TOOLTIP},              // SCMFW_WIDGET_EYEBROWS
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,    98,   109, SPR_ARROW_RIGHT,         STR_FACE_EYEBROWS_TOOLTIP},              // SCMFW_WIDGET_EYEBROWS_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   146,   157, SPR_ARROW_LEFT,          STR_FACE_LIPS_MOUSTACHE_TOOLTIP},        // SCMFW_WIDGET_LIPS_MOUSTACHE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   146,   157, STR_EMPTY,               STR_FACE_LIPS_MOUSTACHE_TOOLTIP},        // SCMFW_WIDGET_LIPS_MOUSTACHE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   146,   157, SPR_ARROW_RIGHT,         STR_FACE_LIPS_MOUSTACHE_TOOLTIP},        // SCMFW_WIDGET_LIPS_MOUSTACHE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   134,   145, SPR_ARROW_LEFT,          STR_FACE_NOSE_TOOLTIP},                  // SCMFW_WIDGET_NOSE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   134,   145, STR_EMPTY,               STR_FACE_NOSE_TOOLTIP},                  // SCMFW_WIDGET_NOSE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   134,   145, SPR_ARROW_RIGHT,         STR_FACE_NOSE_TOOLTIP},                  // SCMFW_WIDGET_NOSE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,    86,    97, SPR_ARROW_LEFT,          STR_FACE_HAIR_TOOLTIP},                  // SCMFW_WIDGET_HAIR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,    86,    97, STR_EMPTY,               STR_FACE_HAIR_TOOLTIP},                  // SCMFW_WIDGET_HAIR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,    86,    97, SPR_ARROW_RIGHT,         STR_FACE_HAIR_TOOLTIP},                  // SCMFW_WIDGET_HAIR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   170,   181, SPR_ARROW_LEFT,          STR_FACE_JACKET_TOOLTIP},                // SCMFW_WIDGET_JACKET_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   170,   181, STR_EMPTY,               STR_FACE_JACKET_TOOLTIP},                // SCMFW_WIDGET_JACKET
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   170,   181, SPR_ARROW_RIGHT,         STR_FACE_JACKET_TOOLTIP},                // SCMFW_WIDGET_JACKET_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   182,   193, SPR_ARROW_LEFT,          STR_FACE_COLLAR_TOOLTIP},                // SCMFW_WIDGET_COLLAR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   182,   193, STR_EMPTY,               STR_FACE_COLLAR_TOOLTIP},                // SCMFW_WIDGET_COLLAR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   182,   193, SPR_ARROW_RIGHT,         STR_FACE_COLLAR_TOOLTIP},                // SCMFW_WIDGET_COLLAR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   194,   205, SPR_ARROW_LEFT,          STR_FACE_TIE_EARRING_TOOLTIP},           // SCMFW_WIDGET_TIE_EARRING_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   194,   205, STR_EMPTY,               STR_FACE_TIE_EARRING_TOOLTIP},           // SCMFW_WIDGET_TIE_EARRING
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   194,   205, SPR_ARROW_RIGHT,         STR_FACE_TIE_EARRING_TOOLTIP},           // SCMFW_WIDGET_TIE_EARRING_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   183,   122,   133, SPR_ARROW_LEFT,          STR_FACE_GLASSES_TOOLTIP_2},             // SCMFW_WIDGET_GLASSES_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   184,   208,   122,   133, STR_EMPTY,               STR_FACE_GLASSES_TOOLTIP_2},             // SCMFW_WIDGET_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,   209,   217,   122,   133, SPR_ARROW_RIGHT,         STR_FACE_GLASSES_TOOLTIP_2},             // SCMFW_WIDGET_GLASSES_R
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,    96,   170,    60,   205, 0x0,                     STR_NULL},                           // SCMFW_WIDGET_LABELS
{   WIDGETS_END},
};

class SelectCompanyManagerFaceWindow : public Window
{
	CompanyManagerFace face; ///< company manager face bits
	bool advanced; ///< advanced company manager face selection window

	GenderEthnicity ge;
	bool is_female;
	bool is_moust_male;

	/**
	 * Draw dynamic a label to the left of the button and a value in the button
	 *
	 * @param widget_index   index of this widget in the window
	 * @param str            the label which will be draw
	 * @param val            the value which will be draw
	 * @param is_bool_widget is it a bool button
	 */
	void DrawFaceStringLabel(byte widget_index, StringID str, uint8 val, bool is_bool_widget)
	{
		/* Write the label in gold (0x2) to the left of the button. */
		DrawString(this->widget[SCMFW_WIDGET_LABELS].left, this->widget[SCMFW_WIDGET_LABELS].right, this->widget[widget_index].top + 1, str, TC_GOLD, SA_RIGHT);

		if (!this->IsWidgetDisabled(widget_index)) {
			if (is_bool_widget) {
				/* if it a bool button write yes or no */
				str = (val != 0) ? STR_FACE_YES : STR_FACE_NO;
			} else {
				/* else write the value + 1 */
				SetDParam(0, val + 1);
				str = STR_JUST_INT;
			}

			/* Draw the value/bool in white (0xC). If the button clicked adds 1px to x and y text coordinates (IsWindowWidgetLowered()). */
			DrawString(this->widget[widget_index].left + this->IsWidgetLowered(widget_index), this->widget[widget_index].right - this->IsWidgetLowered(widget_index),
				this->widget[widget_index].top + 1 + this->IsWidgetLowered(widget_index), str, TC_WHITE, SA_CENTER);
		}
	}

	void UpdateData()
	{
		this->ge = (GenderEthnicity)GB(this->face, _cmf_info[CMFV_GEN_ETHN].offset, _cmf_info[CMFV_GEN_ETHN].length); // get the gender and ethnicity
		this->is_female = HasBit(this->ge, GENDER_FEMALE); // get the gender: 0 == male and 1 == female
		this->is_moust_male = !is_female && GetCompanyManagerFaceBits(this->face, CMFV_HAS_MOUSTACHE, this->ge) != 0; // is a male face with moustache
	}

public:
	SelectCompanyManagerFaceWindow(const WindowDesc *desc, Window *parent, bool advanced, int top, int left) : Window(desc, parent->window_number)
	{
		this->parent = parent;
		this->owner = (Owner)this->window_number;
		this->face = Company::Get((CompanyID)this->window_number)->face;
		this->advanced = advanced;

		this->UpdateData();

		/* Check if repositioning from default is required */
		if (top != FIRST_GUI_CALL && left != FIRST_GUI_CALL) {
			this->top = top;
			this->left = left;
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* lower the non-selected gender button */
		this->SetWidgetLoweredState(SCMFW_WIDGET_MALE,  !this->is_female);
		this->SetWidgetLoweredState(SCMFW_WIDGET_FEMALE, this->is_female);

		/* advanced company manager face selection window */
		if (this->advanced) {
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
		}

		this->DrawWidgets();

		/* Draw dynamic button value and labels for the advanced company manager face selection window */
		if (this->advanced) {
			if (this->is_female) {
				/* Only for female faces */
				this->DrawFaceStringLabel(SCMFW_WIDGET_HAS_MOUSTACHE_EARRING, STR_FACE_EARRING,   GetCompanyManagerFaceBits(this->face, CMFV_HAS_TIE_EARRING, this->ge), true );
				this->DrawFaceStringLabel(SCMFW_WIDGET_TIE_EARRING,           STR_FACE_EARRING,   GetCompanyManagerFaceBits(this->face, CMFV_TIE_EARRING,     this->ge), false);
			} else {
				/* Only for male faces */
				this->DrawFaceStringLabel(SCMFW_WIDGET_HAS_MOUSTACHE_EARRING, STR_FACE_MOUSTACHE, GetCompanyManagerFaceBits(this->face, CMFV_HAS_MOUSTACHE,   this->ge), true );
				this->DrawFaceStringLabel(SCMFW_WIDGET_TIE_EARRING,           STR_FACE_TIE,       GetCompanyManagerFaceBits(this->face, CMFV_TIE_EARRING,     this->ge), false);
			}
			if (this->is_moust_male) {
				/* Only for male faces with moustache */
				this->DrawFaceStringLabel(SCMFW_WIDGET_LIPS_MOUSTACHE,        STR_FACE_MOUSTACHE, GetCompanyManagerFaceBits(this->face, CMFV_MOUSTACHE,       this->ge), false);
			} else {
				/* Only for female faces or male faces without moustache */
				this->DrawFaceStringLabel(SCMFW_WIDGET_LIPS_MOUSTACHE,        STR_FACE_LIPS,      GetCompanyManagerFaceBits(this->face, CMFV_LIPS,            this->ge), false);
			}
			/* For all faces */
			this->DrawFaceStringLabel(SCMFW_WIDGET_HAS_GLASSES,           STR_FACE_GLASSES,     GetCompanyManagerFaceBits(this->face, CMFV_HAS_GLASSES,     this->ge), true );
			this->DrawFaceStringLabel(SCMFW_WIDGET_HAIR,                  STR_FACE_HAIR,        GetCompanyManagerFaceBits(this->face, CMFV_HAIR,            this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_EYEBROWS,              STR_FACE_EYEBROWS,    GetCompanyManagerFaceBits(this->face, CMFV_EYEBROWS,        this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_EYECOLOUR,             STR_FACE_EYECOLOUR,   GetCompanyManagerFaceBits(this->face, CMFV_EYE_COLOUR,      this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_GLASSES,               STR_FACE_GLASSES,     GetCompanyManagerFaceBits(this->face, CMFV_GLASSES,         this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_NOSE,                  STR_FACE_NOSE,        GetCompanyManagerFaceBits(this->face, CMFV_NOSE,            this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_CHIN,                  STR_FACE_CHIN,        GetCompanyManagerFaceBits(this->face, CMFV_CHIN,            this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_JACKET,                STR_FACE_JACKET,      GetCompanyManagerFaceBits(this->face, CMFV_JACKET,          this->ge), false);
			this->DrawFaceStringLabel(SCMFW_WIDGET_COLLAR,                STR_FACE_COLLAR,      GetCompanyManagerFaceBits(this->face, CMFV_COLLAR,          this->ge), false);
		}

		/* Draw the company manager face picture */
		DrawCompanyManagerFace(this->face, Company::Get((CompanyID)this->window_number)->colour, this->widget[SCMFM_WIDGET_FACE].left, this->widget[SCMFM_WIDGET_FACE].top);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			/* Toggle size, advanced/simple face selection */
			case SCMFW_WIDGET_TOGGLE_LARGE_SMALL:
			case SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON: {
				DoCommandP(0, 0, this->face, CMD_SET_COMPANY_MANAGER_FACE);

				/* Backup some data before deletion */
				int oldtop = this->top;     ///< current top position of the window before closing it
				int oldleft = this->left;   ///< current top position of the window before closing it
				bool adv = !this->advanced;
				Window *parent = this->parent;

				delete this;

				/* Open up the (toggled size) Face selection window at the same position as the previous */
				DoSelectCompanyManagerFace(parent, adv, oldtop, oldleft);
			} break;


			/* OK button */
			case SCMFW_WIDGET_ACCEPT:
				DoCommandP(0, 0, this->face, CMD_SET_COMPANY_MANAGER_FACE);
				/* Fall-Through */

			/* Cancel button */
			case SCMFW_WIDGET_CANCEL:
				delete this;
				break;

			/* Load button */
			case SCMFW_WIDGET_LOAD:
				this->face = _company_manager_face;
				ScaleAllCompanyManagerFaceBits(this->face);
				ShowErrorMessage(STR_FACE_LOAD_DONE, INVALID_STRING_ID, 0, 0);
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
				ShowErrorMessage(STR_FACE_SAVE_DONE, INVALID_STRING_ID, 0, 0);
				break;

			/* Toggle gender (male/female) button */
			case SCMFW_WIDGET_MALE:
			case SCMFW_WIDGET_FEMALE:
				SetCompanyManagerFaceBits(this->face, CMFV_GENDER, this->ge, widget - SCMFW_WIDGET_MALE);
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
				/* For all buttons from SCMFW_WIDGET_HAS_MOUSTACHE_EARRING to SCMFW_WIDGET_GLASSES_R is the same function.
				 * Therefor is this combined function.
				 * First it checks which CompanyManagerFaceVariable will be change and then
				 * a: invert the value for boolean variables
				 * or b: it checks inside of IncreaseCompanyManagerFaceBits() if a left (_L) butten is pressed and then decrease else increase the variable */
				if (this->advanced && widget >= SCMFW_WIDGET_HAS_MOUSTACHE_EARRING && widget <= SCMFW_WIDGET_GLASSES_R) {
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
			ShowErrorMessage(STR_FACE_FACECODE_SET, INVALID_STRING_ID, 0, 0);
			this->UpdateData();
			this->SetDirty();
		} else {
			ShowErrorMessage(STR_FACE_FACECODE_ERR, INVALID_STRING_ID, 0, 0);
		}
	}
};

/** normal/simple company manager face selection window description */
static const WindowDesc _select_company_manager_face_desc(
	WDP_AUTO, WDP_AUTO, 190, 163, 190, 163,
	WC_COMPANY_MANAGER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_select_company_manager_face_widgets, _nested_select_company_manager_face_widgets, lengthof(_nested_select_company_manager_face_widgets)
);

/** advanced company manager face selection window description */
static const WindowDesc _select_company_manager_face_adv_desc(
	WDP_AUTO, WDP_AUTO, 220, 220, 220, 220,
	WC_COMPANY_MANAGER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_select_company_manager_face_adv_widgets, _nested_select_company_manager_face_adv_widgets, lengthof(_nested_select_company_manager_face_adv_widgets)
);

/**
 * Open the simple/advanced company manager face selection window
 *
 * @param parent the parent company window
 * @param adv    simple or advanced face selection window
 * @param top    previous top position of the window
 * @param left   previous left position of the window
 */
static void DoSelectCompanyManagerFace(Window *parent, bool adv, int top, int left)
{
	if (!Company::IsValidID((CompanyID)parent->window_number)) return;

	if (BringWindowToFrontById(WC_COMPANY_MANAGER_FACE, parent->window_number)) return;
	new SelectCompanyManagerFaceWindow(adv ? &_select_company_manager_face_adv_desc : &_select_company_manager_face_desc, parent, adv, top, left); // simple or advanced window
}


/** Names of the widgets of the #CompanyWindow. Keep them in the same order as in the widget array */
enum CompanyWindowWidgets {
	CW_WIDGET_CLOSEBOX = 0,
	CW_WIDGET_CAPTION,
	CW_WIDGET_FACE,
	CW_WIDGET_NEW_FACE,
	CW_WIDGET_COLOUR_SCHEME,
	CW_WIDGET_PRESIDENT_NAME,
	CW_WIDGET_COMPANY_NAME,
	CW_WIDGET_BUILD_VIEW_HQ,
	CW_WIDGET_RELOCATE_HQ,
	CW_WIDGET_BUY_SHARE,
	CW_WIDGET_SELL_SHARE,
	CW_WIDGET_COMPANY_PASSWORD,
	CW_WIDGET_COMPANY_JOIN,
};

static const Widget _company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,                        STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   359,     0,    13, STR_COMPANY_VIEW_CAPTION,               STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   359,    14,   157, 0x0,                                    STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    89,   158,   169, STR_COMPANY_VIEW_NEW_FACE_BUTTON,       STR_COMPANY_VIEW_NEW_FACE_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    90,   179,   158,   169, STR_COMPANY_VIEW_COLOUR_SCHEME_BUTTON,  STR_COMPANY_VIEW_COLOUR_SCHEME_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   180,   269,   158,   169, STR_COMPANY_VIEW_PRESIDENT_NAME_BUTTON, STR_COMPANY_VIEW_PRESIDENT_NAME_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   270,   359,   158,   169, STR_COMPANY_VIEW_COMPANY_NAME_BUTTON,   STR_COMPANY_VIEW_COMPANY_NAME_TOOLTIP},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,    18,    29, STR_COMPANY_VIEW_VIEW_HQ_BUTTON,        STR_COMPANY_VIEW_BUILD_HQ_TOOLTIP},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,    32,    43, STR_COMPANY_VIEW_RELOCATE_HQ,                        STR_COMPANY_VIEW_RELOCATE_COMPANY_HEADQUARTERS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   179,   158,   169, STR_COMPANY_VIEW_BUY_SHARE_BUTTON,      STR_COMPANY_VIEW_BUY_SHARE_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   180,   359,   158,   169, STR_COMPANY_VIEW_SELL_SHARE_BUTTON,     STR_COMPANY_VIEW_SELL_SHARE_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,   138,   149, STR_COMPANY_VIEW_PASSWORD,                   STR_COMPANY_VIEW_PASSWORD_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,   138,   149, STR_COMPANY_VIEW_JOIN,                       STR_COMPANY_VIEW_JOIN_TOOLTIP},
{   WIDGETS_END},
};

static const NWidgetPart _nested_company_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CW_WIDGET_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, CW_WIDGET_CAPTION), SetDataTip(STR_COMPANY_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CW_WIDGET_FACE),
		NWidget(NWID_SPACER), SetMinimalSize(360, 4),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(true, false),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, CW_WIDGET_BUILD_VIEW_HQ), SetMinimalSize(90, 12), SetPadding(0, 4, 0, 0),
										SetDataTip(STR_COMPANY_VIEW_VIEW_HQ_BUTTON, STR_COMPANY_VIEW_BUILD_HQ_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(true, false),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, CW_WIDGET_RELOCATE_HQ), SetMinimalSize(90, 12), SetPadding(0, 4, 0, 0),
										SetDataTip(STR_COMPANY_VIEW_RELOCATE_HQ, STR_COMPANY_VIEW_RELOCATE_COMPANY_HEADQUARTERS),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 94),
		/* Multi player buttons. */
		NWidget(NWID_SELECTION, INVALID_COLOUR, -1),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(true, false),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COMPANY_PASSWORD), SetMinimalSize(90, 12), SetPadding(0, 4, 0, 0),
										SetDataTip(STR_COMPANY_VIEW_PASSWORD, STR_COMPANY_VIEW_PASSWORD_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(true, false),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COMPANY_JOIN), SetMinimalSize(90, 12), SetPadding(0, 4, 0, 0),
										SetDataTip(STR_COMPANY_VIEW_JOIN, STR_COMPANY_VIEW_JOIN_TOOLTIP),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 8),
	EndContainer(),
	/* Button bars at the bottom. */
	NWidget(NWID_SELECTION, INVALID_COLOUR, -1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_NEW_FACE), SetMinimalSize(90, 12),
										SetDataTip(STR_COMPANY_VIEW_NEW_FACE_BUTTON, STR_COMPANY_VIEW_NEW_FACE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COLOUR_SCHEME), SetMinimalSize(90, 12),
										SetDataTip(STR_COMPANY_VIEW_COLOUR_SCHEME_BUTTON, STR_COMPANY_VIEW_COLOUR_SCHEME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_PRESIDENT_NAME), SetMinimalSize(90, 12),
										SetDataTip(STR_COMPANY_VIEW_PRESIDENT_NAME_BUTTON, STR_COMPANY_VIEW_PRESIDENT_NAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_COMPANY_NAME), SetMinimalSize(90, 12),
										SetDataTip(STR_COMPANY_VIEW_COMPANY_NAME_BUTTON, STR_COMPANY_VIEW_COMPANY_NAME_TOOLTIP),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_BUY_SHARE), SetMinimalSize(180, 12),
										SetDataTip(STR_COMPANY_VIEW_BUY_SHARE_BUTTON, STR_COMPANY_VIEW_BUY_SHARE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CW_WIDGET_SELL_SHARE), SetMinimalSize(180, 12),
										SetDataTip(STR_COMPANY_VIEW_SELL_SHARE_BUTTON, STR_COMPANY_VIEW_SELL_SHARE_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

/**
 * Draws text "Vehicles:" and number of all vehicle types, or "(none)"
 * @param company ID of company to print statistics of
 * @param right the right most location to draw to
 */
static void DrawCompanyVehiclesAmount(CompanyID company, int right)
{
	const int x = 110;
	int y = 63;
	uint amounts[] = { 0, 0, 0, 0 };

	DrawString(x, right, y, STR_COMPANY_VIEW_VEHICLES_TITLE);

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == company) {
			if (v->IsPrimaryVehicle()) {
				assert((size_t)v->type < lengthof(amounts));
				amounts[v->type]++;
			}
		}
	}

	if (amounts[0] + amounts[1] + amounts[2] + amounts[3] == 0) {
		DrawString(x + 70, right, y, STR_COMPANY_VIEW_VEHICLES_NONE);
	} else {
		static const StringID strings[] = {
			STR_COMPANY_VIEW_TRAINS, STR_COMPANY_VIEW_ROAD_VEHICLES, STR_COMPANY_VIEW_SHIPS, STR_COMPANY_VIEW_AIRCRAFT
		};
		assert_compile(lengthof(amounts) == lengthof(strings));

		for (uint i = 0; i < lengthof(amounts); i++) {
			if (amounts[i] != 0) {
				SetDParam(0, amounts[i]);
				DrawString(x + 70, right, y, strings[i]);
				y += 10;
			}
		}
	}
}

int GetAmountOwnedBy(const Company *c, Owner owner)
{
	return (c->share_owners[0] == owner) +
				 (c->share_owners[1] == owner) +
				 (c->share_owners[2] == owner) +
				 (c->share_owners[3] == owner);
}

/**
 * Draws list of all companies with shares
 * @param c pointer to the Company structure
 */
static void DrawCompanyOwnerText(const Company *c)
{
	const Company *c2;
	uint num = 0;
	const byte height = GetCharacterHeight(FS_NORMAL);

	FOR_ALL_COMPANIES(c2) {
		uint amt = GetAmountOwnedBy(c, c2->index);
		if (amt != 0) {
			SetDParam(0, amt * 25);
			SetDParam(1, c2->index);

			DrawString(120, 359, (num++) * height + 116, STR_COMPANY_VIEW_SHARES_OWNED_BY);
		}
	}
}

/**
 * Window with general information about a company
 */
struct CompanyWindow : Window
{
	CompanyWindowWidgets query_widget;

	CompanyWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->owner = (Owner)this->window_number;
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const Company *c = Company::Get((CompanyID)this->window_number);
		bool local = this->window_number == _local_company;

		this->SetWidgetHiddenState(CW_WIDGET_NEW_FACE,       !local);
		this->SetWidgetHiddenState(CW_WIDGET_COLOUR_SCHEME,   !local);
		this->SetWidgetHiddenState(CW_WIDGET_PRESIDENT_NAME, !local);
		this->SetWidgetHiddenState(CW_WIDGET_COMPANY_NAME,   !local);
		this->widget[CW_WIDGET_BUILD_VIEW_HQ].data = (local && c->location_of_HQ == INVALID_TILE) ? STR_COMPANY_VIEW_BUILD_HQ_BUTTON : STR_COMPANY_VIEW_VIEW_HQ_BUTTON;
		if (local && c->location_of_HQ != INVALID_TILE) this->widget[CW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; // HQ is already built.
		this->SetWidgetDisabledState(CW_WIDGET_BUILD_VIEW_HQ, !local && c->location_of_HQ == INVALID_TILE);
		this->SetWidgetHiddenState(CW_WIDGET_RELOCATE_HQ,      !local || c->location_of_HQ == INVALID_TILE);
		this->SetWidgetHiddenState(CW_WIDGET_BUY_SHARE,        local);
		this->SetWidgetHiddenState(CW_WIDGET_SELL_SHARE,       local);
		this->SetWidgetHiddenState(CW_WIDGET_COMPANY_PASSWORD, !local || !_networking);
		this->SetWidgetHiddenState(CW_WIDGET_COMPANY_JOIN,     local || !_networking);
		this->SetWidgetDisabledState(CW_WIDGET_COMPANY_JOIN,   c->is_ai);

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

		SetDParam(0, c->index);
		SetDParam(1, c->index);

		this->DrawWidgets();

#ifdef ENABLE_NETWORK
		if (_networking && NetworkCompanyIsPassworded(c->index)) {
			DrawSprite(SPR_LOCK, PAL_NONE, this->widget[CW_WIDGET_COMPANY_JOIN].left - 10, this->widget[CW_WIDGET_COMPANY_JOIN].top + 2);
		}
#endif /* ENABLE_NETWORK */

		/* Company manager's face */
		DrawCompanyManagerFace(c->face, c->colour, 2, 16);

		/* "xxx (Manager)" */
		SetDParam(0, c->index);
		DrawStringMultiLine(48 - MAX_LENGTH_PRESIDENT_NAME_PIXELS / 2, 48 + MAX_LENGTH_PRESIDENT_NAME_PIXELS / 2, 135, 157, STR_COMPANY_VIEW_PRESIDENT_MANAGER_TITLE, TC_FROMSTRING, SA_CENTER);

		/* "Inaugurated:" */
		SetDParam(0, c->inaugurated_year);
		DrawString(110, this->width, 23, STR_COMPANY_VIEW_INAUGURATED_TITLE);

		/* "Colour scheme:" */
		DrawString(110, this->width, 43, STR_COMPANY_VIEW_COLOUR_SCHEME_TITLE);
		/* Draw company-colour bus */
		DrawSprite(SPR_VEH_BUS_SW_VIEW, COMPANY_SPRITE_COLOUR(c->index), 215, 44);

		/* "Vehicles:" */
		DrawCompanyVehiclesAmount((CompanyID)this->window_number, this->width);

		/* "Company value:" */
		SetDParam(0, CalculateCompanyValue(c));
		DrawString(110, this->width, 106, STR_COMPANY_VIEW_COMPANY_VALUE);

		/* Shares list */
		DrawCompanyOwnerText(c);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case CW_WIDGET_NEW_FACE: DoSelectCompanyManagerFace(this, false); break;

			case CW_WIDGET_COLOUR_SCHEME:
				if (BringWindowToFrontById(WC_COMPANY_COLOUR, this->window_number)) break;
				new SelectCompanyLiveryWindow(&_select_company_livery_desc, (CompanyID)this->window_number);
				break;

			case CW_WIDGET_PRESIDENT_NAME:
				this->query_widget = CW_WIDGET_PRESIDENT_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_PRESIDENT_NAME, STR_COMPANY_VIEW_PRESIDENT_S_NAME_QUERY_CAPTION, MAX_LENGTH_PRESIDENT_NAME_BYTES, MAX_LENGTH_PRESIDENT_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case CW_WIDGET_COMPANY_NAME:
				this->query_widget = CW_WIDGET_COMPANY_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_COMPANY_NAME, STR_COMPANY_VIEW_COMPANY_NAME_QUERY_CAPTION, MAX_LENGTH_COMPANY_NAME_BYTES, MAX_LENGTH_COMPANY_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case CW_WIDGET_BUILD_VIEW_HQ: {
				TileIndex tile = Company::Get((CompanyID)this->window_number)->location_of_HQ;
				if (tile == INVALID_TILE) {
					if ((byte)this->window_number != _local_company) return;
					SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, HT_RECT, this);
					SetTileSelectSize(2, 2);
					this->LowerWidget(CW_WIDGET_BUILD_VIEW_HQ);
					this->SetWidgetDirty(CW_WIDGET_BUILD_VIEW_HQ);
				} else {
					if (_ctrl_pressed) {
						ShowExtraViewPortWindow(tile);
					} else {
						ScrollMainWindowToTile(tile);
					}
				}
				break;
			}

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
					ShowQueryString(STR_EMPTY, STR_NETWORK_NEED_COMPANY_PASSWORD_CAPTION, 20, 180, this, CS_ALPHANUMERAL, QSF_NONE);
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
		if (DoCommandP(tile, 0, 0, CMD_BUILD_COMPANY_HQ | CMD_MSG(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS)))
			ResetObjectToPlace();
			this->widget[CW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; // this button can now behave as a normal push button
			this->RaiseButtons();
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
	WDP_AUTO, WDP_AUTO, 360, 170, 360, 170,
	WC_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_company_widgets, _nested_company_widgets,lengthof(_nested_company_widgets)
);

void ShowCompany(CompanyID company)
{
	if (!Company::IsValidID(company)) return;

	AllocateWindowDescFront<CompanyWindow>(&_company_desc, company);
}

/** widget numbers of the #BuyCompanyWindow. */
enum BuyCompanyWidgets {
	BCW_CLOSEBOX,
	BCW_CAPTION,
	BCW_BACKGROUND,
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

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
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
			} break;

			case BCW_QUESTION: {
				const Company *c = Company::Get((CompanyID)this->window_number);
				SetDParam(0, c->index);
				SetDParam(1, c->bankrupt_value);
				DrawStringMultiLine(r.left, r.right, r.top, r.bottom, STR_BUY_COMPANY_MESSAGE, TC_FROMSTRING, SA_CENTER);
			} break;
		}
	}

	virtual void OnClick(Point pt, int widget)
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
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE, BCW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, BCW_CAPTION), SetDataTip(STR_ERROR_MESSAGE_CAPTION_OTHER_COMPANY, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE, BCW_BACKGROUND),
		NWidget(NWID_VERTICAL), SetPIP(8, 8, 8),
			NWidget(NWID_HORIZONTAL), SetPIP(8, 10, 8),
				NWidget(WWT_EMPTY, INVALID_COLOUR, BCW_FACE), SetFill(false, true),
				NWidget(WWT_EMPTY, INVALID_COLOUR, BCW_QUESTION), SetMinimalSize(240, 0), SetFill(true, true),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(100, 10, 100),
				NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, BCW_NO), SetMinimalSize(60, 12), SetDataTip(STR_QUIT_NO, STR_NULL), SetFill(true, false),
				NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, BCW_YES), SetMinimalSize(60, 12), SetDataTip(STR_QUIT_YES, STR_NULL), SetFill(true, false),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _buy_company_desc(
	153, 171, 334, 137, 334, 137,
	WC_BUY_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	NULL, _nested_buy_company_widgets, lengthof(_nested_buy_company_widgets)
);


void ShowBuyCompanyDialog(CompanyID company)
{
	AllocateWindowDescFront<BuyCompanyWindow>(&_buy_company_desc, company);
}
