/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_gui.cpp %Company related GUIs. */

#include "stdafx.h"
#include "currency.h"
#include "error.h"
#include "gui.h"
#include "settings_gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "company_func.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_gui.h"
#include "network/network_func.h"
#include "newgrf.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "timer/timer_game_economy.h"
#include "dropdown_type.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "core/geometry_func.hpp"
#include "object_type.h"
#include "rail.h"
#include "road.h"
#include "engine_base.h"
#include "window_func.h"
#include "road_func.h"
#include "water.h"
#include "station_func.h"
#include "zoom_func.h"
#include "sortlist_type.h"
#include "company_cmd.h"
#include "economy_cmd.h"
#include "group_cmd.h"
#include "group_gui.h"
#include "misc_cmd.h"
#include "object_cmd.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "core/string_consumer.hpp"

#include "widgets/company_widget.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"


/** Company GUI constants. */
static void DoSelectCompanyManagerFace(Window *parent);
static void ShowCompanyInfrastructure(CompanyID company);

/** List of revenues. */
static const std::initializer_list<ExpensesType> _expenses_list_revenue = {
	EXPENSES_TRAIN_REVENUE,
	EXPENSES_ROADVEH_REVENUE,
	EXPENSES_AIRCRAFT_REVENUE,
	EXPENSES_SHIP_REVENUE,
};

/** List of operating expenses. */
static const std::initializer_list<ExpensesType> _expenses_list_operating_costs = {
	EXPENSES_TRAIN_RUN,
	EXPENSES_ROADVEH_RUN,
	EXPENSES_AIRCRAFT_RUN,
	EXPENSES_SHIP_RUN,
	EXPENSES_PROPERTY,
	EXPENSES_LOAN_INTEREST,
};

/** List of capital expenses. */
static const std::initializer_list<ExpensesType> _expenses_list_capital_costs = {
	EXPENSES_CONSTRUCTION,
	EXPENSES_NEW_VEHICLES,
	EXPENSES_OTHER,
};

/** Expense list container. */
struct ExpensesList {
	const StringID title; ///< StringID of list title.
	const std::initializer_list<ExpensesType> &items; ///< List of expenses types.

	ExpensesList(StringID title, const std::initializer_list<ExpensesType> &list) : title(title), items(list)
	{
	}

	uint GetHeight() const
	{
		/* Add up the height of all the lines.  */
		return static_cast<uint>(this->items.size()) * GetCharacterHeight(FS_NORMAL);
	}

	/** Compute width of the expenses categories in pixels. */
	uint GetListWidth() const
	{
		uint width = 0;
		for (const ExpensesType &et : this->items) {
			width = std::max(width, GetStringBoundingBox(STR_FINANCES_SECTION_CONSTRUCTION + et).width);
		}
		return width;
	}
};

/** Types of expense lists */
static const std::initializer_list<ExpensesList> _expenses_list_types = {
	{ STR_FINANCES_REVENUE_TITLE,            _expenses_list_revenue },
	{ STR_FINANCES_OPERATING_EXPENSES_TITLE, _expenses_list_operating_costs },
	{ STR_FINANCES_CAPITAL_EXPENSES_TITLE,   _expenses_list_capital_costs },
};

/**
 * Get the total height of the "categories" column.
 * @return The total height in pixels.
 */
static uint GetTotalCategoriesHeight()
{
	/* There's an empty line and blockspace on the year row */
	uint total_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;

	for (const ExpensesList &list : _expenses_list_types) {
		/* Title + expense list + total line + total + blockspace after category */
		total_height += GetCharacterHeight(FS_NORMAL) + list.GetHeight() + WidgetDimensions::scaled.vsep_normal + GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;
	}

	/* Total income */
	total_height += WidgetDimensions::scaled.vsep_normal + GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;

	return total_height;
}

/**
 * Get the required width of the "categories" column, equal to the widest element.
 * @return The required width in pixels.
 */
static uint GetMaxCategoriesWidth()
{
	uint max_width = GetStringBoundingBox(TimerGameEconomy::UsingWallclockUnits() ? STR_FINANCES_PERIOD_CAPTION : STR_FINANCES_YEAR_CAPTION).width;

	/* Loop through categories to check max widths. */
	for (const ExpensesList &list : _expenses_list_types) {
		/* Title of category */
		max_width = std::max(max_width, GetStringBoundingBox(list.title).width);
		/* Entries in category */
		max_width = std::max(max_width, list.GetListWidth() + WidgetDimensions::scaled.hsep_indent);
	}

	return max_width;
}

/**
 * Draw a category of expenses (revenue, operating expenses, capital expenses).
 */
static void DrawCategory(const Rect &r, int start_y, const ExpensesList &list)
{
	Rect tr = r.Indent(WidgetDimensions::scaled.hsep_indent, _current_text_dir == TD_RTL);

	tr.top = start_y;

	for (const ExpensesType &et : list.items) {
		DrawString(tr, STR_FINANCES_SECTION_CONSTRUCTION + et);
		tr.top += GetCharacterHeight(FS_NORMAL);
	}
}

/**
 * Draw the expenses categories.
 * @param r Available space for drawing.
 * @note The environment must provide padding at the left and right of \a r.
 */
static void DrawCategories(const Rect &r)
{
	int y = r.top;
	/* Draw description of 12-minute economic period. */
	DrawString(r.left, r.right, y, (TimerGameEconomy::UsingWallclockUnits() ? STR_FINANCES_PERIOD_CAPTION : STR_FINANCES_YEAR_CAPTION), TC_FROMSTRING, SA_LEFT, true);
	y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;

	for (const ExpensesList &list : _expenses_list_types) {
		/* Draw category title and advance y */
		DrawString(r.left, r.right, y, list.title, TC_FROMSTRING, SA_LEFT);
		y += GetCharacterHeight(FS_NORMAL);

		/* Draw category items and advance y */
		DrawCategory(r, y, list);
		y += list.GetHeight();

		/* Advance y by the height of the horizontal line between amounts and subtotal */
		y += WidgetDimensions::scaled.vsep_normal;

		/* Draw category total and advance y */
		DrawString(r.left, r.right, y, STR_FINANCES_TOTAL_CAPTION, TC_FROMSTRING, SA_RIGHT);
		y += GetCharacterHeight(FS_NORMAL);

		/* Advance y by a blockspace after this category block */
		y += WidgetDimensions::scaled.vsep_wide;
	}

	/* Draw total profit/loss */
	y += WidgetDimensions::scaled.vsep_normal;
	DrawString(r.left, r.right, y, STR_FINANCES_PROFIT, TC_FROMSTRING, SA_LEFT);
}

/**
 * Draw an amount of money.
 * @param amount Amount of money to draw,
 * @param left   Left coordinate of the space to draw in.
 * @param right  Right coordinate of the space to draw in.
 * @param top    Top coordinate of the space to draw in.
 * @param colour The TextColour of the string.
 */
static void DrawPrice(Money amount, int left, int right, int top, TextColour colour)
{
	StringID str = STR_FINANCES_NEGATIVE_INCOME;
	if (amount == 0) {
		str = STR_FINANCES_ZERO_INCOME;
	} else if (amount < 0) {
		amount = -amount;
		str = STR_FINANCES_POSITIVE_INCOME;
	}
	DrawString(left, right, top, GetString(str, amount), colour, SA_RIGHT | SA_FORCE);
}

/**
 * Draw a category of expenses/revenues in the year column.
 * @return The income sum of the category.
 */
static Money DrawYearCategory(const Rect &r, int start_y, const ExpensesList &list, const Expenses &tbl)
{
	int y = start_y;
	Money sum = 0;

	for (const ExpensesType &et : list.items) {
		Money cost = tbl[et];
		sum += cost;
		if (cost != 0) DrawPrice(cost, r.left, r.right, y, TC_BLACK);
		y += GetCharacterHeight(FS_NORMAL);
	}

	/* Draw the total at the bottom of the category. */
	GfxFillRect(r.left, y, r.right, y + WidgetDimensions::scaled.bevel.top - 1, PC_BLACK);
	y += WidgetDimensions::scaled.vsep_normal;
	if (sum != 0) DrawPrice(sum, r.left, r.right, y, TC_WHITE);

	/* Return the sum for the yearly total. */
	return sum;
}


/**
 * Draw a column with prices.
 * @param r    Available space for drawing.
 * @param year Year being drawn.
 * @param tbl  Reference to table of amounts for \a year.
 * @note The environment must provide padding at the left and right of \a r.
 */
static void DrawYearColumn(const Rect &r, TimerGameEconomy::Year year, const Expenses &tbl)
{
	int y = r.top;
	Money sum;

	/* Year header */
	DrawString(r.left, r.right, y, GetString(STR_FINANCES_YEAR, year), TC_FROMSTRING, SA_RIGHT | SA_FORCE, true);
	y += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;

	/* Categories */
	for (const ExpensesList &list : _expenses_list_types) {
		y += GetCharacterHeight(FS_NORMAL);
		sum += DrawYearCategory(r, y, list, tbl);
		/* Expense list + expense category title + expense category total + blockspace after category */
		y += list.GetHeight() + WidgetDimensions::scaled.vsep_normal + GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_wide;
	}

	/* Total income. */
	GfxFillRect(r.left, y, r.right, y + WidgetDimensions::scaled.bevel.top - 1, PC_BLACK);
	y += WidgetDimensions::scaled.vsep_normal;
	DrawPrice(sum, r.left, r.right, y, TC_WHITE);
}

static constexpr std::initializer_list<NWidgetPart> _nested_company_finances_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_CF_CAPTION),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_CF_TOGGLE_SIZE), SetSpriteTip(SPR_LARGE_SMALL_WINDOW, STR_TOOLTIP_TOGGLE_LARGE_SMALL_WINDOW), SetAspect(WidgetDimensions::ASPECT_TOGGLE_SIZE),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CF_SEL_PANEL),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CF_EXPS_CATEGORY), SetMinimalSize(120, 0), SetFill(0, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CF_EXPS_PRICE1), SetMinimalSize(86, 0), SetFill(0, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CF_EXPS_PRICE2), SetMinimalSize(86, 0), SetFill(0, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CF_EXPS_PRICE3), SetMinimalSize(86, 0), SetFill(0, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.framerect), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPIPRatio(0, 1, 2),
			NWidget(NWID_VERTICAL), // Vertical column with 'bank balance', 'loan'
				NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_FINANCES_OWN_FUNDS_TITLE),
				NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_FINANCES_LOAN_TITLE),
				NWidget(WWT_TEXT, INVALID_COLOUR), SetStringTip(STR_FINANCES_BANK_BALANCE_TITLE), SetPadding(WidgetDimensions::unscaled.vsep_normal, 0, 0, 0),
			EndContainer(),
			NWidget(NWID_VERTICAL), // Vertical column with bank balance amount, loan amount, and total.
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_CF_OWN_VALUE), SetAlignment(SA_VERT_CENTER | SA_RIGHT | SA_FORCE),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_CF_LOAN_VALUE), SetAlignment(SA_VERT_CENTER | SA_RIGHT | SA_FORCE),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CF_BALANCE_LINE), SetMinimalSize(0, WidgetDimensions::unscaled.vsep_normal),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_CF_BALANCE_VALUE), SetAlignment(SA_VERT_CENTER | SA_RIGHT | SA_FORCE),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CF_SEL_MAXLOAN),
				NWidget(NWID_VERTICAL), SetPIPRatio(0, 0, 1), // Max loan information
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CF_INTEREST_RATE),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CF_MAXLOAN_VALUE),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CF_SEL_BUTTONS),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_CF_INCREASE_LOAN), SetFill(1, 0), SetToolTip(STR_FINANCES_BORROW_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_CF_REPAY_LOAN), SetFill(1, 0), SetToolTip(STR_FINANCES_REPAY_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_CF_INFRASTRUCTURE), SetFill(1, 0), SetStringTip(STR_FINANCES_INFRASTRUCTURE_BUTTON, STR_COMPANY_VIEW_INFRASTRUCTURE_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

/** Window class displaying the company finances. */
struct CompanyFinancesWindow : Window {
	static constexpr int NUM_PERIODS = WID_CF_EXPS_PRICE3 - WID_CF_EXPS_PRICE1 + 1;

	static Money max_money; ///< The maximum amount of money a company has had this 'run'
	bool small = false; ///< Window is toggled to 'small'.
	uint8_t first_visible = NUM_PERIODS - 1; ///< First visible expenses column. The last column (current) is always visible.

	CompanyFinancesWindow(WindowDesc &desc, CompanyID company) : Window(desc)
	{
		this->CreateNestedTree();
		this->SetupWidgets();
		this->FinishInitNested(company);

		this->owner = this->window_number;
		this->InvalidateData();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_CF_CAPTION:
				return GetString(STR_FINANCES_CAPTION, this->window_number, this->window_number);

			case WID_CF_BALANCE_VALUE: {
				const Company *c = Company::Get(this->window_number);
				return GetString(STR_FINANCES_BANK_BALANCE, c->money);
			}

			case WID_CF_LOAN_VALUE: {
				const Company *c = Company::Get(this->window_number);
				return GetString(STR_FINANCES_TOTAL_CURRENCY, c->current_loan);
			}

			case WID_CF_OWN_VALUE: {
				const Company *c = Company::Get(this->window_number);
				return GetString(STR_FINANCES_TOTAL_CURRENCY, c->money - c->current_loan);
			}

			case WID_CF_INTEREST_RATE:
				return GetString(STR_FINANCES_INTEREST_RATE, _settings_game.difficulty.initial_interest);

			case WID_CF_MAXLOAN_VALUE: {
				const Company *c = Company::Get(this->window_number);
				return GetString(STR_FINANCES_MAX_LOAN, c->GetMaxLoan());
			}

			case WID_CF_INCREASE_LOAN:
				return GetString(STR_FINANCES_BORROW_BUTTON, LOAN_INTERVAL);

			case WID_CF_REPAY_LOAN:
				return GetString(STR_FINANCES_REPAY_BUTTON, LOAN_INTERVAL);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_CF_EXPS_CATEGORY:
				size.width  = GetMaxCategoriesWidth();
				size.height = GetTotalCategoriesHeight();
				break;

			case WID_CF_EXPS_PRICE1:
			case WID_CF_EXPS_PRICE2:
			case WID_CF_EXPS_PRICE3:
				size.height = GetTotalCategoriesHeight();
				[[fallthrough]];

			case WID_CF_BALANCE_VALUE:
			case WID_CF_LOAN_VALUE:
			case WID_CF_OWN_VALUE: {
				uint64_t max_value = GetParamMaxValue(CompanyFinancesWindow::max_money);
				size.width = std::max(GetStringBoundingBox(GetString(STR_FINANCES_NEGATIVE_INCOME, max_value)).width, GetStringBoundingBox(GetString(STR_FINANCES_POSITIVE_INCOME, max_value)).width);
				size.width += padding.width;
				break;
			}

			case WID_CF_INTEREST_RATE:
				size.height = GetCharacterHeight(FS_NORMAL);
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_CF_EXPS_CATEGORY:
				DrawCategories(r);
				break;

			case WID_CF_EXPS_PRICE1:
			case WID_CF_EXPS_PRICE2:
			case WID_CF_EXPS_PRICE3: {
				int period = widget - WID_CF_EXPS_PRICE1;
				if (period < this->first_visible) break;

				const Company *c = Company::Get(this->window_number);
				const auto &expenses = c->yearly_expenses[NUM_PERIODS - period - 1];
				DrawYearColumn(r, TimerGameEconomy::year - (NUM_PERIODS - period - 1), expenses);
				break;
			}

			case WID_CF_BALANCE_LINE:
				GfxFillRect(r.left, r.top, r.right, r.top + WidgetDimensions::scaled.bevel.top - 1, PC_BLACK);
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
		this->GetWidget<NWidgetStacked>(WID_CF_SEL_PANEL)->SetDisplayedPlane(plane);
		this->GetWidget<NWidgetStacked>(WID_CF_SEL_MAXLOAN)->SetDisplayedPlane(plane);

		CompanyID company = this->window_number;
		plane = (company != _local_company) ? SZSP_NONE : 0;
		this->GetWidget<NWidgetStacked>(WID_CF_SEL_BUTTONS)->SetDisplayedPlane(plane);
	}

	void OnPaint() override
	{
		if (!this->IsShaded()) {
			if (!this->small) {
				/* Check that the expenses panel height matches the height needed for the layout. */
				if (GetTotalCategoriesHeight() != this->GetWidget<NWidgetBase>(WID_CF_EXPS_CATEGORY)->current_y) {
					this->SetupWidgets();
					this->ReInit();
					return;
				}
			}

			/* Check that the loan buttons are shown only when the user owns the company. */
			CompanyID company = this->window_number;
			int req_plane = (company != _local_company) ? SZSP_NONE : 0;
			if (req_plane != this->GetWidget<NWidgetStacked>(WID_CF_SEL_BUTTONS)->shown_plane) {
				this->SetupWidgets();
				this->ReInit();
				return;
			}

			const Company *c = Company::Get(company);
			this->SetWidgetDisabledState(WID_CF_INCREASE_LOAN, c->current_loan >= c->GetMaxLoan()); // Borrow button only shows when there is any more money to loan.
			this->SetWidgetDisabledState(WID_CF_REPAY_LOAN, company != _local_company || c->current_loan == 0); // Repay button only shows when there is any more money to repay.
		}

		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CF_TOGGLE_SIZE: // toggle size
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

			case WID_CF_INCREASE_LOAN: // increase loan
				Command<Commands::IncreaseLoan>::Post(STR_ERROR_CAN_T_BORROW_ANY_MORE_MONEY, _ctrl_pressed ? LoanCommand::Max : LoanCommand::Interval, 0);
				break;

			case WID_CF_REPAY_LOAN: // repay loan
				Command<Commands::DecreaseLoan>::Post(STR_ERROR_CAN_T_REPAY_LOAN, _ctrl_pressed ? LoanCommand::Max : LoanCommand::Interval, 0);
				break;

			case WID_CF_INFRASTRUCTURE: // show infrastructure details
				ShowCompanyInfrastructure(this->window_number);
				break;
		}
	}

	void RefreshVisibleColumns()
	{
		for (uint period = 0; period < this->first_visible; ++period) {
			const Company *c = Company::Get(this->window_number);
			const Expenses &expenses = c->yearly_expenses[NUM_PERIODS - period - 1];
			/* Show expenses column if it has any non-zero value in it. */
			if (std::ranges::any_of(expenses, [](const Money &value) { return value != 0; })) {
				this->first_visible = period;
				break;
			}
		}
	}

	void OnInvalidateData(int, bool) override
	{
		this->RefreshVisibleColumns();
	}

	/**
	 * Check on a regular interval if the maximum amount of money has changed.
	 * If it has, rescale the window to fit the new amount.
	 */
	const IntervalTimer<TimerWindow> rescale_interval = {std::chrono::seconds(3), [this](auto) {
		const Company *c = Company::Get(this->window_number);
		if (c->money > CompanyFinancesWindow::max_money) {
			CompanyFinancesWindow::max_money = std::max(c->money * 2, CompanyFinancesWindow::max_money * 4);
			this->SetupWidgets();
			this->ReInit();
		}
	}};
};

/** First conservative estimate of the maximum amount of money */
Money CompanyFinancesWindow::max_money = INT32_MAX;

static WindowDesc _company_finances_desc(
	WDP_AUTO, "company_finances", 0, 0,
	WC_FINANCES, WC_NONE,
	{},
	_nested_company_finances_widgets
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

	new CompanyFinancesWindow(_company_finances_desc, company);
}

/* Association of liveries to livery classes */
static const LiveryClass _livery_class[LS_END] = {
	LC_OTHER,
	LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL,
	LC_ROAD, LC_ROAD,
	LC_SHIP, LC_SHIP,
	LC_AIRCRAFT, LC_AIRCRAFT, LC_AIRCRAFT,
	LC_ROAD, LC_ROAD,
};

/**
 * Colour selection list item, with icon and string components.
 * @tparam TSprite Recolourable sprite to draw as icon.
 */
template <SpriteID TSprite = SPR_SQUARE>
class DropDownListColourItem : public DropDownIcon<DropDownString<DropDownListItem>> {
public:
	DropDownListColourItem(int colour, bool masked) :
			DropDownIcon<DropDownString<DropDownListItem>>(TSprite, GetColourPalette(static_cast<Colours>(colour % COLOUR_END)), GetString(colour < COLOUR_END ? (STR_COLOUR_DARK_BLUE + colour) : STR_COLOUR_DEFAULT), colour, masked)
	{
	}
};

/** Company livery colour scheme window. */
struct SelectCompanyLiveryWindow : public Window {
private:
	uint32_t sel = 0;
	LiveryClass livery_class{};
	Dimension square{};
	uint rows = 0;
	uint line_height = 0;
	GUIGroupList groups{};
	Scrollbar *vscroll = nullptr;

	void ShowColourDropDownMenu(uint32_t widget)
	{
		uint32_t used_colours = 0;
		const Livery *livery, *default_livery = nullptr;
		bool primary = widget == WID_SCL_PRI_COL_DROPDOWN;
		uint8_t default_col = 0;

		/* Disallow other company colours for the primary colour */
		if (this->livery_class < LC_GROUP_RAIL && HasBit(this->sel, LS_DEFAULT) && primary) {
			for (const Company *c : Company::Iterate()) {
				if (c->index != _local_company) SetBit(used_colours, c->colour);
			}
		}

		const Company *c = Company::Get(this->window_number);

		if (this->livery_class < LC_GROUP_RAIL) {
			/* Get the first selected livery to use as the default dropdown item */
			LiveryScheme scheme;
			for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
				if (HasBit(this->sel, scheme)) break;
			}
			if (scheme == LS_END) scheme = LS_DEFAULT;
			livery = &c->livery[scheme];
			if (scheme != LS_DEFAULT) default_livery = &c->livery[LS_DEFAULT];
		} else {
			const Group *g = Group::Get(this->sel);
			livery = &g->livery;
			if (g->parent == GroupID::Invalid()) {
				default_livery = &c->livery[LS_DEFAULT];
			} else {
				const Group *pg = Group::Get(g->parent);
				default_livery = &pg->livery;
			}
		}

		DropDownList list;
		if (default_livery != nullptr) {
			/* Add COLOUR_END to put the colour out of range, but also allow us to show what the default is */
			default_col = (primary ? default_livery->colour1 : default_livery->colour2) + COLOUR_END;
			list.push_back(std::make_unique<DropDownListColourItem<>>(default_col, false));
		}
		for (Colours colour = COLOUR_BEGIN; colour != COLOUR_END; colour++) {
			list.push_back(std::make_unique<DropDownListColourItem<>>(colour, HasBit(used_colours, colour)));
		}

		uint8_t sel;
		if (default_livery == nullptr || livery->in_use.Test(primary ? Livery::Flag::Primary : Livery::Flag::Secondary)) {
			sel = primary ? livery->colour1 : livery->colour2;
		} else {
			sel = default_col;
		}
		ShowDropDownList(this, std::move(list), sel, widget);
	}

	void BuildGroupList(CompanyID owner)
	{
		if (!this->groups.NeedRebuild()) return;

		this->groups.clear();

		if (this->livery_class >= LC_GROUP_RAIL) {
			VehicleType vtype = (VehicleType)(this->livery_class - LC_GROUP_RAIL);
			BuildGuiGroupList(this->groups, false, owner, vtype);
		}

		this->groups.RebuildDone();
	}

	void SetRows()
	{
		if (this->livery_class < LC_GROUP_RAIL) {
			this->rows = 0;
			for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (_livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
					this->rows++;
				}
			}
		} else {
			this->rows = (uint)this->groups.size();
		}

		this->vscroll->SetCount(this->rows);
	}

public:
	SelectCompanyLiveryWindow(WindowDesc &desc, CompanyID company, GroupID group) : Window(desc)
	{
		this->CreateNestedTree();
		this->GetWidget<NWidgetStacked>(WID_SCL_SEC_COL_DROP_SEL)->SetDisplayedPlane(_loaded_newgrf_features.has_2CC ? 0 : SZSP_NONE);
		this->vscroll = this->GetScrollbar(WID_SCL_MATRIX_SCROLLBAR);

		if (group == GroupID::Invalid()) {
			this->livery_class = LC_OTHER;
			this->sel = 1;
			this->LowerWidget(WID_SCL_CLASS_GENERAL);
			this->BuildGroupList(company);
			this->SetRows();
		} else {
			this->SetSelectedGroup(company, group);
		}

		this->FinishInitNested(company);
		this->owner = company;
		this->InvalidateData(1);
	}

	void SetSelectedGroup(CompanyID company, GroupID group)
	{
		this->RaiseWidget(WID_SCL_CLASS_GENERAL + this->livery_class);
		const Group *g = Group::Get(group);
		switch (g->vehicle_type) {
			case VEH_TRAIN: this->livery_class = LC_GROUP_RAIL; break;
			case VEH_ROAD: this->livery_class = LC_GROUP_ROAD; break;
			case VEH_SHIP: this->livery_class = LC_GROUP_SHIP; break;
			case VEH_AIRCRAFT: this->livery_class = LC_GROUP_AIRCRAFT; break;
			default: NOT_REACHED();
		}
		this->sel = group.base();
		this->LowerWidget(WID_SCL_CLASS_GENERAL + this->livery_class);

		this->groups.ForceRebuild();
		this->BuildGroupList(company);
		this->SetRows();

		/* Position scrollbar to selected group */
		for (uint i = 0; i < this->rows; i++) {
			if (this->groups[i].group->index == sel) {
				this->vscroll->SetPosition(i - this->vscroll->GetCapacity() / 2);
				break;
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_SCL_SPACER_DROPDOWN: {
				/* The matrix widget below needs enough room to print all the schemes. */
				Dimension d = {0, 0};
				for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					d = maxdim(d, GetStringBoundingBox(STR_LIVERY_DEFAULT + scheme));
				}

				size.width = std::max(size.width, 5 + d.width + padding.width);
				break;
			}

			case WID_SCL_MATRIX: {
				/* 11 items in the default rail class */
				this->square = GetSpriteSize(SPR_SQUARE);
				this->line_height = std::max(this->square.height, (uint)GetCharacterHeight(FS_NORMAL)) + padding.height;

				size.height = 5 * this->line_height;
				resize.width = 1;
				fill.height = resize.height = this->line_height;
				break;
			}

			case WID_SCL_SEC_COL_DROPDOWN:
				if (!_loaded_newgrf_features.has_2CC) break;
				[[fallthrough]];

			case WID_SCL_PRI_COL_DROPDOWN: {
				this->square = GetSpriteSize(SPR_SQUARE);
				int string_padding = this->square.width + WidgetDimensions::scaled.hsep_normal + padding.width;
				for (Colours colour = COLOUR_BEGIN; colour != COLOUR_END; colour++) {
					size.width = std::max(size.width, GetStringBoundingBox(STR_COLOUR_DARK_BLUE + colour).width + string_padding);
				}
				size.width = std::max(size.width, GetStringBoundingBox(STR_COLOUR_DEFAULT).width + string_padding);
				break;
			}
		}
	}

	void OnPaint() override
	{
		bool local = this->window_number == _local_company;

		/* Disable dropdown controls if no scheme is selected */
		bool disabled = this->livery_class < LC_GROUP_RAIL ? (this->sel == 0) : (this->sel == GroupID::Invalid());
		this->SetWidgetDisabledState(WID_SCL_PRI_COL_DROPDOWN, !local || disabled);
		this->SetWidgetDisabledState(WID_SCL_SEC_COL_DROPDOWN, !local || disabled);

		this->BuildGroupList(this->window_number);

		this->DrawWidgets();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_SCL_CAPTION:
				return GetString(STR_LIVERY_CAPTION, this->window_number);

			case WID_SCL_PRI_COL_DROPDOWN:
			case WID_SCL_SEC_COL_DROPDOWN: {
				const Company *c = Company::Get(this->window_number);
				bool primary = widget == WID_SCL_PRI_COL_DROPDOWN;
				StringID colour = STR_COLOUR_DEFAULT;

				if (this->livery_class < LC_GROUP_RAIL) {
					if (this->sel != 0) {
						LiveryScheme scheme = LS_DEFAULT;
						for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
							if (HasBit(this->sel, scheme)) break;
						}
						if (scheme == LS_END) scheme = LS_DEFAULT;
						const Livery *livery = &c->livery[scheme];
						if (scheme == LS_DEFAULT || livery->in_use.Test(primary ? Livery::Flag::Primary : Livery::Flag::Secondary)) {
							colour = STR_COLOUR_DARK_BLUE + (primary ? livery->colour1 : livery->colour2);
						}
					}
				} else {
					if (this->sel != GroupID::Invalid()) {
						const Group *g = Group::Get(this->sel);
						const Livery *livery = &g->livery;
						if (livery->in_use.Test(primary ? Livery::Flag::Primary : Livery::Flag::Secondary)) {
							colour = STR_COLOUR_DARK_BLUE + (primary ? livery->colour1 : livery->colour2);
						}
					}
				}
				return GetString(colour);
			}

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_SCL_MATRIX) return;

		bool rtl = _current_text_dir == TD_RTL;

		/* Coordinates of scheme name column. */
		const NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_SCL_SPACER_DROPDOWN);
		Rect sch = nwi->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		/* Coordinates of first dropdown. */
		nwi = this->GetWidget<NWidgetBase>(WID_SCL_PRI_COL_DROPDOWN);
		Rect pri = nwi->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		/* Coordinates of second dropdown. */
		nwi = this->GetWidget<NWidgetBase>(WID_SCL_SEC_COL_DROPDOWN);
		Rect sec = nwi->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

		Rect pri_squ = pri.WithWidth(this->square.width, rtl);
		Rect sec_squ = sec.WithWidth(this->square.width, rtl);

		pri = pri.Indent(this->square.width + WidgetDimensions::scaled.hsep_normal, rtl);
		sec = sec.Indent(this->square.width + WidgetDimensions::scaled.hsep_normal, rtl);

		Rect ir = r.WithHeight(this->resize.step_height).Shrink(WidgetDimensions::scaled.matrix);
		int square_offs = (ir.Height() - this->square.height) / 2;
		int text_offs   = (ir.Height() - GetCharacterHeight(FS_NORMAL)) / 2;

		int y = ir.top;

		/* Helper function to draw livery info. */
		auto draw_livery = [&](std::string_view str, const Livery &livery, bool is_selected, bool is_default_scheme, int indent) {
			/* Livery Label. */
			DrawString(sch.left + (rtl ? 0 : indent), sch.right - (rtl ? indent : 0), y + text_offs, str, is_selected ? TC_WHITE : TC_BLACK);

			/* Text below the first dropdown. */
			DrawSprite(SPR_SQUARE, GetColourPalette(livery.colour1), pri_squ.left, y + square_offs);
			DrawString(pri.left, pri.right, y + text_offs, (is_default_scheme || livery.in_use.Test(Livery::Flag::Primary)) ? STR_COLOUR_DARK_BLUE + livery.colour1 : STR_COLOUR_DEFAULT, is_selected ? TC_WHITE : TC_GOLD);

			/* Text below the second dropdown. */
			if (sec.right > sec.left) { // Second dropdown has non-zero size.
				DrawSprite(SPR_SQUARE, GetColourPalette(livery.colour2), sec_squ.left, y + square_offs);
				DrawString(sec.left, sec.right, y + text_offs, (is_default_scheme || livery.in_use.Test(Livery::Flag::Secondary)) ? STR_COLOUR_DARK_BLUE + livery.colour2 : STR_COLOUR_DEFAULT, is_selected ? TC_WHITE : TC_GOLD);
			}

			y += this->line_height;
		};

		const Company *c = Company::Get(this->window_number);

		if (livery_class < LC_GROUP_RAIL) {
			int pos = this->vscroll->GetPosition();
			for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (_livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
					if (pos-- > 0) continue;
					draw_livery(GetString(STR_LIVERY_DEFAULT + scheme), c->livery[scheme], HasBit(this->sel, scheme), scheme == LS_DEFAULT, 0);
				}
			}
		} else {
			auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->groups);
			for (auto it = first; it != last; ++it) {
				const Group *g = it->group;
				draw_livery(GetString(STR_GROUP_NAME, g->index), g->livery, this->sel == g->index, false, it->indent * WidgetDimensions::scaled.hsep_indent);
			}

			if (this->vscroll->GetCount() == 0) {
				const StringID empty_labels[] = { STR_LIVERY_TRAIN_GROUP_EMPTY, STR_LIVERY_ROAD_VEHICLE_GROUP_EMPTY, STR_LIVERY_SHIP_GROUP_EMPTY, STR_LIVERY_AIRCRAFT_GROUP_EMPTY };
				VehicleType vtype = (VehicleType)(this->livery_class - LC_GROUP_RAIL);
				DrawString(ir.left, ir.right, y + text_offs, empty_labels[vtype], TC_BLACK);
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			/* Livery Class buttons */
			case WID_SCL_CLASS_GENERAL:
			case WID_SCL_CLASS_RAIL:
			case WID_SCL_CLASS_ROAD:
			case WID_SCL_CLASS_SHIP:
			case WID_SCL_CLASS_AIRCRAFT:
			case WID_SCL_GROUPS_RAIL:
			case WID_SCL_GROUPS_ROAD:
			case WID_SCL_GROUPS_SHIP:
			case WID_SCL_GROUPS_AIRCRAFT:
				this->RaiseWidget(WID_SCL_CLASS_GENERAL + this->livery_class);
				this->livery_class = (LiveryClass)(widget - WID_SCL_CLASS_GENERAL);
				this->LowerWidget(WID_SCL_CLASS_GENERAL + this->livery_class);

				/* Select the first item in the list */
				if (this->livery_class < LC_GROUP_RAIL) {
					this->sel = 0;
					for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
						if (_livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
							this->sel = 1 << scheme;
							break;
						}
					}
				} else {
					this->sel = GroupID::Invalid().base();
					this->groups.ForceRebuild();
					this->BuildGroupList(this->window_number);

					if (!this->groups.empty()) {
						this->sel = this->groups[0].group->index.base();
					}
				}

				this->SetRows();
				this->SetDirty();
				break;

			case WID_SCL_PRI_COL_DROPDOWN: // First colour dropdown
				ShowColourDropDownMenu(WID_SCL_PRI_COL_DROPDOWN);
				break;

			case WID_SCL_SEC_COL_DROPDOWN: // Second colour dropdown
				ShowColourDropDownMenu(WID_SCL_SEC_COL_DROPDOWN);
				break;

			case WID_SCL_MATRIX: {
				if (this->livery_class < LC_GROUP_RAIL) {
					uint row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, widget);
					if (row >= this->rows) return;

					LiveryScheme j = (LiveryScheme)row;

					for (LiveryScheme scheme = LS_BEGIN; scheme <= j && scheme < LS_END; scheme++) {
						if (_livery_class[scheme] != this->livery_class || !HasBit(_loaded_newgrf_features.used_liveries, scheme)) j++;
					}
					assert(j < LS_END);

					if (_ctrl_pressed) {
						ToggleBit(this->sel, j);
					} else {
						this->sel = 1 << j;
					}
				} else {
					auto it = this->vscroll->GetScrolledItemFromWidget(this->groups, pt.y, this, widget);
					if (it == std::end(this->groups)) return;

					this->sel = it->group->index.base();
				}
				this->SetDirty();
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SCL_MATRIX);
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		bool local = this->window_number == _local_company;
		if (!local) return;

		Colours colour = static_cast<Colours>(index);
		if (colour >= COLOUR_END) colour = INVALID_COLOUR;

		if (this->livery_class < LC_GROUP_RAIL) {
			/* Set company colour livery */
			for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				/* Changed colour for the selected scheme, or all visible schemes if CTRL is pressed. */
				if (HasBit(this->sel, scheme) || (_ctrl_pressed && _livery_class[scheme] == this->livery_class && HasBit(_loaded_newgrf_features.used_liveries, scheme))) {
					Command<Commands::SetCompanyColour>::Post(scheme, widget == WID_SCL_PRI_COL_DROPDOWN, colour);
				}
			}
		} else {
			/* Setting group livery */
			Command<Commands::SetGroupLivery>::Post(static_cast<GroupID>(this->sel), widget == WID_SCL_PRI_COL_DROPDOWN, colour);
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		if (data != -1) {
			/* data contains a VehicleType, rebuild list if it displayed */
			if (this->livery_class == data + LC_GROUP_RAIL) {
				this->groups.ForceRebuild();
				this->BuildGroupList(this->window_number);
				this->SetRows();

				if (!Group::IsValidID(this->sel)) {
					this->sel = GroupID::Invalid().base();
					if (!this->groups.empty()) this->sel = this->groups[0].group->index.base();
				}

				this->SetDirty();
			}
			return;
		}

		this->SetWidgetsDisabledState(true, WID_SCL_CLASS_RAIL, WID_SCL_CLASS_ROAD, WID_SCL_CLASS_SHIP, WID_SCL_CLASS_AIRCRAFT);

		bool current_class_valid = this->livery_class == LC_OTHER || this->livery_class >= LC_GROUP_RAIL;
		if (_settings_client.gui.liveries == LIT_ALL || (_settings_client.gui.liveries == LIT_COMPANY && this->window_number == _local_company)) {
			for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
				if (HasBit(_loaded_newgrf_features.used_liveries, scheme)) {
					if (_livery_class[scheme] == this->livery_class) current_class_valid = true;
					this->EnableWidget(WID_SCL_CLASS_GENERAL + _livery_class[scheme]);
				} else if (this->livery_class < LC_GROUP_RAIL) {
					ClrBit(this->sel, scheme);
				}
			}
		}

		if (!current_class_valid) {
			Point pt = {0, 0};
			this->OnClick(pt, WID_SCL_CLASS_GENERAL, 1);
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_select_company_livery_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SCL_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_CLASS_GENERAL), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_COMPANY_GENERAL, STR_LIVERY_GENERAL_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_CLASS_RAIL), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_TRAINLIST, STR_LIVERY_TRAIN_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_CLASS_ROAD), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_TRUCKLIST, STR_LIVERY_ROAD_VEHICLE_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_CLASS_SHIP), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_SHIPLIST, STR_LIVERY_SHIP_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_CLASS_AIRCRAFT), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_IMG_AIRPLANESLIST, STR_LIVERY_AIRCRAFT_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_GROUPS_RAIL), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_GROUP_LIVERY_TRAIN, STR_LIVERY_TRAIN_GROUP_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_GROUPS_ROAD), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_GROUP_LIVERY_ROADVEH, STR_LIVERY_ROAD_VEHICLE_GROUP_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_GROUPS_SHIP), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_GROUP_LIVERY_SHIP, STR_LIVERY_SHIP_GROUP_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCL_GROUPS_AIRCRAFT), SetToolbarMinimalSize(1), SetFill(0, 1), SetSpriteTip(SPR_GROUP_LIVERY_AIRCRAFT, STR_LIVERY_AIRCRAFT_GROUP_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_SCL_MATRIX), SetMinimalSize(275, 0), SetResize(1, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_LIVERY_PANEL_TOOLTIP), SetScrollbar(WID_SCL_MATRIX_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SCL_MATRIX_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SCL_SPACER_DROPDOWN), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_SCL_PRI_COL_DROPDOWN), SetFill(0, 1), SetToolTip(STR_LIVERY_PRIMARY_TOOLTIP),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SCL_SEC_COL_DROP_SEL),
			NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_SCL_SEC_COL_DROPDOWN), SetFill(0, 1), SetToolTip(STR_LIVERY_SECONDARY_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _select_company_livery_desc(
	WDP_AUTO, "company_colour_scheme", 0, 0,
	WC_COMPANY_COLOUR, WC_NONE,
	{},
	_nested_select_company_livery_widgets
);

void ShowCompanyLiveryWindow(CompanyID company, GroupID group)
{
	SelectCompanyLiveryWindow *w = (SelectCompanyLiveryWindow *)BringWindowToFrontById(WC_COMPANY_COLOUR, company);
	if (w == nullptr) {
		new SelectCompanyLiveryWindow(_select_company_livery_desc, company, group);
	} else if (group != GroupID::Invalid()) {
		w->SetSelectedGroup(company, group);
	}
}

/**
 * Draws the face of a company manager's face.
 * @param cmf   the company manager's face
 * @param colour the (background) colour of the gradient
 * @param r      position to draw the face
 */
void DrawCompanyManagerFace(const CompanyManagerFace &cmf, Colours colour, const Rect &r)
{
	/* Determine offset from centre of drawing rect. */
	Dimension d = GetSpriteSize(SPR_GRADIENT);
	int x = CentreBounds(r.left, r.right, d.width);
	int y = CentreBounds(r.top, r.bottom, d.height);

	FaceVars vars = GetCompanyManagerFaceVars(cmf.style);

	/* First determine which parts are enabled. */
	uint64_t active_vars = GetActiveFaceVars(cmf, vars);

	std::unordered_map<uint8_t, PaletteID> palettes;

	/* Second, get palettes. */
	for (auto var : SetBitIterator(active_vars)) {
		if (vars[var].type != FaceVarType::Palette) continue;

		PaletteID pal = PAL_NONE;
		switch (vars[var].GetBits(cmf)) {
			default: NOT_REACHED();
			case 0: pal = PALETTE_TO_BROWN; break;
			case 1: pal = PALETTE_TO_BLUE;  break;
			case 2: pal = PALETTE_TO_GREEN; break;
		}
		for (uint8_t affected_var : SetBitIterator(std::get<uint64_t>(vars[var].data))) {
			palettes[affected_var] = pal;
		}
	}

	/* Draw the gradient (background) */
	DrawSprite(SPR_GRADIENT, GetColourPalette(colour), x, y);

	/* Thirdly, draw sprites. */
	for (auto var : SetBitIterator(active_vars)) {
		if (vars[var].type != FaceVarType::Sprite) continue;

		auto it = palettes.find(var);
		PaletteID pal = (it == std::end(palettes)) ? PAL_NONE : it->second;
		DrawSprite(vars[var].GetSprite(cmf), pal, x, y);
	}
}

/** Nested widget description for the company manager face selection dialog */
static constexpr std::initializer_list<NWidgetPart> _nested_select_company_manager_face_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_SCMF_CAPTION), SetStringTip(STR_FACE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_SCMF_TOGGLE_LARGE_SMALL), SetSpriteTip(SPR_LARGE_SMALL_WINDOW, STR_FACE_ADVANCED_TOOLTIP), SetAspect(WidgetDimensions::ASPECT_TOGGLE_SIZE),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_SCMF_SELECT_FACE),
			/* Left side */
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(4),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SCMF_FACE), SetMinimalSize(92, 119), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_RANDOM_NEW_FACE), SetFill(1, 0), SetStringTip(STR_FACE_NEW_FACE_BUTTON, STR_FACE_NEW_FACE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_TOGGLE_LARGE_SMALL_BUTTON), SetFill(1, 0), SetStringTip(STR_FACE_ADVANCED, STR_FACE_ADVANCED_TOOLTIP),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SCMF_SEL_LOADSAVE), // Load/number/save buttons under the portrait in the advanced view.
					NWidget(NWID_VERTICAL),
						NWidget(NWID_SPACER), SetFill(1, 1), SetResize(0, 1),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_LOAD), SetFill(1, 0), SetStringTip(STR_FACE_LOAD, STR_FACE_LOAD_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_FACECODE), SetFill(1, 0), SetStringTip(STR_FACE_FACECODE, STR_FACE_FACECODE_TOOLTIP),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_SAVE), SetFill(1, 0), SetStringTip(STR_FACE_SAVE, STR_FACE_SAVE_TOOLTIP),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		/* Right side */
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SCMF_SEL_PARTS), // Advanced face parts setting.
			NWidget(NWID_VERTICAL),
				NWidget(WWT_MATRIX, COLOUR_GREY, WID_SCMF_STYLE), SetResize(1, 0), SetFill(1, 0), SetMatrixDataTip(1, 1),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_GREY, WID_SCMF_PARTS), SetResize(1, 1), SetFill(1, 1), SetMatrixDataTip(1, 0), SetScrollbar(WID_SCMF_PARTS_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_SCMF_PARTS_SCROLLBAR),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_CANCEL), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_BUTTON_CANCEL, STR_FACE_CANCEL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SCMF_ACCEPT), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_BUTTON_OK, STR_FACE_OK_TOOLTIP),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SCMF_SEL_RESIZE),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

/** Management class for customizing the face of the company manager. */
class SelectCompanyManagerFaceWindow : public Window
{
	CompanyManagerFace face{}; ///< company manager face bits
	bool advanced = false; ///< advanced company manager face selection window
	uint selected_var = UINT_MAX; ///< Currently selected face variable. `UINT_MAX` for none, `UINT_MAX - 1` means style is clicked instead.
	uint click_state = 0; ///< Click state on selected face variable.
	int line_height = 0; ///< Height of each face variable row.

	std::vector<const FaceVar *> face_vars; ///< Visible face variables.

	/**
	 * Make face bits valid and update visible face variables.
	 */
	void UpdateData()
	{
		FaceVars vars = GetCompanyManagerFaceVars(this->face.style);
		ScaleAllCompanyManagerFaceBits(this->face, vars);

		uint64_t active_vars = GetActiveFaceVars(this->face, vars);
		/* Exclude active parts which have no string. */
		for (auto var : SetBitIterator(active_vars)) {
			if (vars[var].name == STR_NULL) ClrBit(active_vars, var);
		}

		/* Rebuild the sorted list of face variable pointers. */
		this->face_vars.clear();
		for (auto var : SetBitIterator(active_vars)) {
			this->face_vars.emplace_back(&vars[var]);
		}
		std::ranges::sort(this->face_vars, std::less{}, &FaceVar::position);

		this->GetScrollbar(WID_SCMF_PARTS_SCROLLBAR)->SetCount(std::size(this->face_vars));
	}

public:
	SelectCompanyManagerFaceWindow(WindowDesc &desc, Window *parent) : Window(desc)
	{
		this->CreateNestedTree();
		this->SelectDisplayPlanes(this->advanced);
		this->FinishInitNested(parent->window_number);
		this->parent = parent;
		this->owner = this->window_number;
		this->face = Company::Get(this->window_number)->face;

		this->UpdateData();
	}

	void OnInit() override
	{
		this->line_height = std::max(SETTING_BUTTON_HEIGHT, GetCharacterHeight(FS_NORMAL)) + WidgetDimensions::scaled.matrix.Vertical();
	}

	/**
	 * Select planes to display to the user with the #NWID_SELECTION widgets #WID_SCMF_SEL_LOADSAVE, #WID_SCMF_SEL_MALEFEMALE, and #WID_SCMF_SEL_PARTS.
	 * @param advanced Display advanced face management window.
	 */
	void SelectDisplayPlanes(bool advanced)
	{
		this->GetWidget<NWidgetStacked>(WID_SCMF_SEL_LOADSAVE)->SetDisplayedPlane(advanced ? 0 : SZSP_HORIZONTAL);
		this->GetWidget<NWidgetStacked>(WID_SCMF_SEL_PARTS)->SetDisplayedPlane(advanced ? 0 : SZSP_NONE);
		this->GetWidget<NWidgetStacked>(WID_SCMF_SEL_RESIZE)->SetDisplayedPlane(advanced ? 0 : SZSP_NONE);

		NWidgetCore *wi = this->GetWidget<NWidgetCore>(WID_SCMF_TOGGLE_LARGE_SMALL_BUTTON);
		if (advanced) {
			wi->SetStringTip(STR_FACE_SIMPLE, STR_FACE_SIMPLE_TOOLTIP);
		} else {
			wi->SetStringTip(STR_FACE_ADVANCED, STR_FACE_ADVANCED_TOOLTIP);
		}
	}

	static StringID GetLongestString(StringID a, StringID b)
	{
		return GetStringBoundingBox(a).width > GetStringBoundingBox(b).width ? a : b;
	}

	static uint GetMaximumFacePartsWidth()
	{
		StringID yes_no = GetLongestString(STR_FACE_YES, STR_FACE_NO);

		uint width = 0;
		for (uint style_index = 0; style_index != GetNumCompanyManagerFaceStyles(); ++style_index) {
			FaceVars vars = GetCompanyManagerFaceVars(style_index);
			for (const auto &info : vars) {
				if (info.name == STR_NULL) continue;
				if (info.type == FaceVarType::Toggle) {
					width = std::max(width, GetStringBoundingBox(GetString(STR_FACE_SETTING_TOGGLE, info.name, yes_no)).width);
				} else {
					uint64_t max_digits = GetParamMaxValue(info.valid_values);
					width = std::max(width, GetStringBoundingBox(GetString(STR_FACE_SETTING_NUMERIC, info.name, max_digits, max_digits)).width);
				}
			}
		}

		/* Include width of button and spacing. */
		width += SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide + WidgetDimensions::scaled.frametext.Horizontal();
		return width;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_SCMF_FACE:
				size = maxdim(size, GetScaledSpriteSize(SPR_GRADIENT));
				break;

			case WID_SCMF_STYLE:
				size.height = this->line_height;
				break;

			case WID_SCMF_PARTS:
				fill.height = resize.height = this->line_height;
				size.width = GetMaximumFacePartsWidth();
				size.height = resize.height * 5;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SCMF_FACE:
				DrawCompanyManagerFace(this->face, Company::Get(this->window_number)->colour, r);
				break;

			case WID_SCMF_STYLE: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.frametext, RectPadding::zero).WithHeight(this->line_height);
				bool rtl = _current_text_dir == TD_RTL;

				Rect br = ir.CentreToHeight(SETTING_BUTTON_HEIGHT).WithWidth(SETTING_BUTTON_WIDTH, rtl);
				Rect tr = ir.Shrink(RectPadding::zero, WidgetDimensions::scaled.matrix).CentreToHeight(GetCharacterHeight(FS_NORMAL)).Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide, rtl);

				DrawArrowButtons(br.left, br.top, COLOUR_YELLOW, this->selected_var == UINT_MAX - 1 ? this->click_state : 0, true, true);
				DrawString(tr, GetString(STR_FACE_SETTING_NUMERIC, STR_FACE_STYLE, this->face.style + 1, GetNumCompanyManagerFaceStyles()), TC_WHITE);
				break;
			}

			case WID_SCMF_PARTS: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.frametext, RectPadding::zero).WithHeight(this->line_height);
				bool rtl = _current_text_dir == TD_RTL;

				FaceVars vars = GetCompanyManagerFaceVars(this->face.style);

				auto [first, last] = this->GetScrollbar(WID_SCMF_PARTS_SCROLLBAR)->GetVisibleRangeIterators(this->face_vars);
				for (auto it = first; it != last; ++it) {
					const uint8_t var = static_cast<uint8_t>(*it - vars.data());
					const FaceVar &facevar = **it;

					Rect br = ir.CentreToHeight(SETTING_BUTTON_HEIGHT).WithWidth(SETTING_BUTTON_WIDTH, rtl);
					Rect tr = ir.Shrink(RectPadding::zero, WidgetDimensions::scaled.matrix).CentreToHeight(GetCharacterHeight(FS_NORMAL)).Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide, rtl);

					uint val = vars[var].GetBits(this->face);
					if (facevar.type == FaceVarType::Toggle) {
						DrawBoolButton(br.left, br.top, COLOUR_YELLOW, COLOUR_GREY, val == 1, true);
						DrawString(tr, GetString(STR_FACE_SETTING_TOGGLE, facevar.name, val == 1 ? STR_FACE_YES : STR_FACE_NO), TC_WHITE);
					} else {
						DrawArrowButtons(br.left, br.top, COLOUR_YELLOW, this->selected_var == var ? this->click_state : 0, true, true);
						DrawString(tr, GetString(STR_FACE_SETTING_NUMERIC, facevar.name, val + 1, facevar.valid_values), TC_WHITE);
					}

					ir = ir.Translate(0, this->line_height);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			/* Toggle size, advanced/simple face selection */
			case WID_SCMF_TOGGLE_LARGE_SMALL:
			case WID_SCMF_TOGGLE_LARGE_SMALL_BUTTON:
				this->advanced = !this->advanced;
				this->SelectDisplayPlanes(this->advanced);
				this->ReInit();
				break;

			/* OK button */
			case WID_SCMF_ACCEPT:
				Command<Commands::SetCompanyManagerFace>::Post(this->face.style, this->face.bits);
				[[fallthrough]];

			/* Cancel button */
			case WID_SCMF_CANCEL:
				this->Close();
				break;

			/* Load button */
			case WID_SCMF_LOAD: {
				auto cmf = ParseCompanyManagerFaceCode(_company_manager_face);
				if (cmf.has_value()) this->face = *cmf;
				ShowErrorMessage(GetEncodedString(STR_FACE_LOAD_DONE), {}, WL_INFO);
				this->UpdateData();
				this->SetDirty();
				break;
			}

			/* 'Company manager face number' button, view and/or set company manager face number */
			case WID_SCMF_FACECODE:
				ShowQueryString(FormatCompanyManagerFaceCode(this->face), STR_FACE_FACECODE_CAPTION, 128, this, CS_ALPHANUMERAL, {});
				break;

			/* Save button */
			case WID_SCMF_SAVE:
				_company_manager_face = FormatCompanyManagerFaceCode(this->face);
				ShowErrorMessage(GetEncodedString(STR_FACE_SAVE_DONE), {}, WL_INFO);
				break;

			/* Randomize face button */
			case WID_SCMF_RANDOM_NEW_FACE:
				RandomiseCompanyManagerFace(this->face, _interactive_random);
				this->UpdateData();
				this->SetDirty();
				break;

			case WID_SCMF_STYLE: {
				bool rtl = _current_text_dir == TD_RTL;
				Rect ir = this->GetWidget<NWidgetCore>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.frametext, RectPadding::zero);
				Rect br = ir.WithWidth(SETTING_BUTTON_WIDTH, rtl);

				uint num_styles = GetNumCompanyManagerFaceStyles();
				this->selected_var = UINT_MAX - 1;
				if (IsInsideBS(pt.x, br.left, SETTING_BUTTON_WIDTH / 2)) {
					SetCompanyManagerFaceStyle(this->face, (this->face.style + num_styles - 1) % num_styles);
					this->click_state = 1;
				} else if (IsInsideBS(pt.x, br.left + SETTING_BUTTON_WIDTH / 2, SETTING_BUTTON_WIDTH / 2)) {
					SetCompanyManagerFaceStyle(this->face, (this->face.style + 1) % num_styles);
					this->click_state = 2;
				}

				this->UpdateData();
				this->SetTimeout();
				this->SetDirty();
				break;
			}

			case WID_SCMF_PARTS: {
				bool rtl = _current_text_dir == TD_RTL;
				Rect ir = this->GetWidget<NWidgetCore>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.frametext, RectPadding::zero);
				Rect br = ir.WithWidth(SETTING_BUTTON_WIDTH, rtl);

				this->selected_var = UINT_MAX;

				FaceVars vars = GetCompanyManagerFaceVars(this->face.style);
				auto it = this->GetScrollbar(WID_SCMF_PARTS_SCROLLBAR)->GetScrolledItemFromWidget(this->face_vars, pt.y, this, widget, 0, this->line_height);
				if (it == std::end(this->face_vars)) break;

				this->selected_var = static_cast<uint8_t>(*it - vars.data());
				const auto &facevar = **it;

				if (facevar.type == FaceVarType::Toggle) {
					if (!IsInsideBS(pt.x, br.left, SETTING_BUTTON_WIDTH)) break;
					facevar.ChangeBits(this->face, 1);
					this->UpdateData();
				} else {
					if (IsInsideBS(pt.x, br.left, SETTING_BUTTON_WIDTH / 2)) {
						facevar.ChangeBits(this->face, -1);
						this->click_state = 1;
					} else if (IsInsideBS(pt.x, br.left + SETTING_BUTTON_WIDTH / 2, SETTING_BUTTON_WIDTH / 2)) {
						facevar.ChangeBits(this->face, 1);
						this->click_state = 2;
					} else {
						break;
					}
				}

				this->SetTimeout();
				this->SetDirty();
				break;
			}
		}
	}

	void OnResize() override
	{
		if (auto *wid = this->GetWidget<NWidgetResizeBase>(WID_SCMF_PARTS); wid != nullptr) {
			/* Workaround for automatic widget sizing ignoring resize steps. Manually ensure parts matrix is a
			 * multiple of its resize step. This trick only works here as the window itself is not resizable. */
			if (wid->UpdateVerticalSize((wid->current_y + wid->resize_y - 1) / wid->resize_y * wid->resize_y)) {
				this->ReInit();
				return;
			}
		}

		this->GetScrollbar(WID_SCMF_PARTS_SCROLLBAR)->SetCapacityFromWidget(this, WID_SCMF_PARTS);
	}

	void OnTimeout() override
	{
		this->click_state = 0;
		this->selected_var = UINT_MAX;
		this->SetDirty();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;
		/* Parse new company manager face number */
		auto cmf = ParseCompanyManagerFaceCode(*str);
		if (cmf.has_value()) {
			this->face = *cmf;
			ShowErrorMessage(GetEncodedString(STR_FACE_FACECODE_SET), {}, WL_INFO);
			this->UpdateData();
			this->SetDirty();
		} else {
			ShowErrorMessage(GetEncodedString(STR_FACE_FACECODE_ERR), {}, WL_INFO);
		}
	}
};

/** Company manager face selection window description */
static WindowDesc _select_company_manager_face_desc(
	WDP_AUTO, {}, 0, 0,
	WC_COMPANY_MANAGER_FACE, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_select_company_manager_face_widgets
);

/**
 * Open the simple/advanced company manager face selection window
 *
 * @param parent the parent company window
 */
static void DoSelectCompanyManagerFace(Window *parent)
{
	if (!Company::IsValidID(parent->window_number)) return;

	if (BringWindowToFrontById(WC_COMPANY_MANAGER_FACE, parent->window_number)) return;
	new SelectCompanyManagerFaceWindow(_select_company_manager_face_desc, parent);
}

static constexpr std::initializer_list<NWidgetPart> _nested_company_infrastructure_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_CI_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_CI_LIST), SetFill(1, 1), SetResize(0, 1),
				SetMinimalTextLines(5, WidgetDimensions::unscaled.framerect.Vertical()), SetScrollbar(WID_CI_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_CI_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

/**
 * Window with detailed information about the company's infrastructure.
 */
struct CompanyInfrastructureWindow : Window
{
	enum class InfrastructureItemType : uint8_t {
		Header, ///< Section header.
		Spacer, ///< Spacer
		Value, ///< Label with values.
		Total, ///< Total cost.
	};

	struct InfrastructureItem {
		InfrastructureItemType type;
		StringID label;
		uint count;
		Money cost;
	};

	uint count_width = 0;
	uint cost_width = 0;

	mutable std::vector<InfrastructureItem> list;

	CompanyInfrastructureWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->owner = this->window_number;
	}

	void OnInit() override
	{
		this->UpdateInfrastructureList();
	}

	void UpdateInfrastructureList()
	{
		this->list.clear();

		const Company *c = Company::GetIfValid(this->window_number);
		if (c == nullptr) return;

		Money total_monthly_cost = 0;

		if (uint32_t rail_total = c->infrastructure.GetRailTotal(); rail_total > 0) {
			/* Rail types and signals. */
			this->list.emplace_back(InfrastructureItemType::Header, STR_COMPANY_INFRASTRUCTURE_VIEW_RAIL_SECT);

			for (const RailType &rt : _sorted_railtypes) {
				if (c->infrastructure.rail[rt] == 0) continue;
				Money monthly_cost = RailMaintenanceCost(rt, c->infrastructure.rail[rt], rail_total);
				total_monthly_cost += monthly_cost;
				this->list.emplace_back(InfrastructureItemType::Value, GetRailTypeInfo(rt)->strings.name, c->infrastructure.rail[rt], monthly_cost);
			}

			if (c->infrastructure.signal > 0) {
				Money monthly_cost = SignalMaintenanceCost(c->infrastructure.signal);
				total_monthly_cost += monthly_cost;
				this->list.emplace_back(InfrastructureItemType::Value, STR_COMPANY_INFRASTRUCTURE_VIEW_SIGNALS, c->infrastructure.signal, monthly_cost);
			}
		}

		if (uint32_t road_total = c->infrastructure.GetRoadTotal(); road_total > 0) {
			/* Road types. */
			if (!this->list.empty()) this->list.emplace_back(InfrastructureItemType::Spacer);
			this->list.emplace_back(InfrastructureItemType::Header, STR_COMPANY_INFRASTRUCTURE_VIEW_ROAD_SECT);

			for (const RoadType &rt : _sorted_roadtypes) {
				if (!RoadTypeIsRoad(rt)) continue;
				if (c->infrastructure.road[rt] == 0) continue;
				Money monthly_cost = RoadMaintenanceCost(rt, c->infrastructure.road[rt], road_total);
				total_monthly_cost += monthly_cost;
				this->list.emplace_back(InfrastructureItemType::Value, GetRoadTypeInfo(rt)->strings.name, c->infrastructure.road[rt], monthly_cost);
			}
		}

		if (uint32_t tram_total = c->infrastructure.GetTramTotal(); tram_total > 0) {
			/* Tram types. */
			if (!this->list.empty()) this->list.emplace_back(InfrastructureItemType::Spacer);
			this->list.emplace_back(InfrastructureItemType::Header, STR_COMPANY_INFRASTRUCTURE_VIEW_TRAM_SECT);

			for (const RoadType &rt : _sorted_roadtypes) {
				if (!RoadTypeIsTram(rt)) continue;
				if (c->infrastructure.road[rt] == 0) continue;
				Money monthly_cost = RoadMaintenanceCost(rt, c->infrastructure.road[rt], tram_total);
				total_monthly_cost += monthly_cost;
				this->list.emplace_back(InfrastructureItemType::Value, GetRoadTypeInfo(rt)->strings.name, c->infrastructure.road[rt], monthly_cost);
			}
		}

		if (c->infrastructure.water > 0) {
			/* Canals, locks, and ship depots (docks are counted as stations). */
			if (!this->list.empty()) this->list.emplace_back(InfrastructureItemType::Spacer);
			this->list.emplace_back(InfrastructureItemType::Header, STR_COMPANY_INFRASTRUCTURE_VIEW_WATER_SECT);

			Money monthly_cost = CanalMaintenanceCost(c->infrastructure.water);
			total_monthly_cost += monthly_cost;
			this->list.emplace_back(InfrastructureItemType::Value, STR_COMPANY_INFRASTRUCTURE_VIEW_CANALS, c->infrastructure.water, monthly_cost);
		}

		if (Money airport_cost = AirportMaintenanceCost(c->index); airport_cost > 0 || c->infrastructure.station > 0) {
			/* Stations and airports. */
			if (!this->list.empty()) this->list.emplace_back(InfrastructureItemType::Spacer);
			this->list.emplace_back(InfrastructureItemType::Header, STR_COMPANY_INFRASTRUCTURE_VIEW_STATION_SECT);

			if (c->infrastructure.station > 0) {
				Money monthly_cost = StationMaintenanceCost(c->infrastructure.station);
				total_monthly_cost += monthly_cost;
				this->list.emplace_back(InfrastructureItemType::Value, STR_COMPANY_INFRASTRUCTURE_VIEW_STATIONS, c->infrastructure.station, monthly_cost);
			}

			if (airport_cost > 0) {
				Money monthly_cost = airport_cost;
				total_monthly_cost += monthly_cost;
				this->list.emplace_back(InfrastructureItemType::Value, STR_COMPANY_INFRASTRUCTURE_VIEW_AIRPORTS, c->infrastructure.airport, monthly_cost);
			}
		}

		if (_settings_game.economy.infrastructure_maintenance) {
			/* Total monthly maintenance cost. */
			this->list.emplace_back(InfrastructureItemType::Spacer);
			this->list.emplace_back(InfrastructureItemType::Total, STR_NULL, 0, total_monthly_cost);
		}

		/* Update scrollbar. */
		this->GetScrollbar(WID_CI_SCROLLBAR)->SetCount(std::size(list));
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_CI_CAPTION:
				return GetString(STR_COMPANY_INFRASTRUCTURE_VIEW_CAPTION, this->window_number);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void FindWindowPlacementAndResize(int def_width, int def_height, bool allow_resize) override
	{
		if (def_height == 0) {
			/* Try to open the window with the exact required rows, but clamp to a reasonable limit. */
			int rows = (this->GetWidget<NWidgetBase>(WID_CI_LIST)->current_y - WidgetDimensions::scaled.framerect.Vertical()) / GetCharacterHeight(FS_NORMAL);
			int delta = std::min(20, static_cast<int>(std::size(this->list))) - rows;
			def_height = this->height + delta * GetCharacterHeight(FS_NORMAL);
		}

		this->Window::FindWindowPlacementAndResize(def_width, def_height, allow_resize);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_CI_LIST) return;

		uint max_count = 1000; // Some random number to reserve minimum space.
		Money max_cost = 1000000; // Some random number to reserve minimum space.

		/* List of headers that might be used. */
		static constexpr StringID header_strings[] = {
			STR_COMPANY_INFRASTRUCTURE_VIEW_RAIL_SECT,
			STR_COMPANY_INFRASTRUCTURE_VIEW_ROAD_SECT,
			STR_COMPANY_INFRASTRUCTURE_VIEW_TRAM_SECT,
			STR_COMPANY_INFRASTRUCTURE_VIEW_WATER_SECT,
			STR_COMPANY_INFRASTRUCTURE_VIEW_STATION_SECT,
		};
		/* List of labels that might be used. */
		static constexpr StringID label_strings[] = {
			STR_COMPANY_INFRASTRUCTURE_VIEW_SIGNALS,
			STR_COMPANY_INFRASTRUCTURE_VIEW_CANALS,
			STR_COMPANY_INFRASTRUCTURE_VIEW_STATIONS,
			STR_COMPANY_INFRASTRUCTURE_VIEW_AIRPORTS,
		};

		uint max_header_width = GetStringListWidth(header_strings);
		uint max_label_width = GetStringListWidth(label_strings);

		/* Include width of all possible rail and road types. */
		for (const RailType &rt : _sorted_railtypes) max_label_width = std::max(max_label_width, GetStringBoundingBox(GetRailTypeInfo(rt)->strings.name).width);
		for (const RoadType &rt : _sorted_roadtypes) max_label_width = std::max(max_label_width, GetStringBoundingBox(GetRoadTypeInfo(rt)->strings.name).width);

		for (const InfrastructureItem &entry : this->list) {
			max_count = std::max(max_count, entry.count);
			max_cost = std::max(max_cost, entry.cost * 12);
		}

		max_label_width += WidgetDimensions::scaled.hsep_indent;
		this->count_width = GetStringBoundingBox(GetString(STR_JUST_COMMA, max_count)).width;

		if (_settings_game.economy.infrastructure_maintenance) {
			this->cost_width = GetStringBoundingBox(GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_COMPANY_INFRASTRUCTURE_VIEW_TOTAL_PERIOD : STR_COMPANY_INFRASTRUCTURE_VIEW_TOTAL_YEAR, max_cost)).width;
		} else {
			this->cost_width = 0;
		}

		size.width = max_label_width + WidgetDimensions::scaled.hsep_wide + this->count_width + WidgetDimensions::scaled.hsep_wide + this->cost_width;
		size.width = std::max(size.width, max_header_width) + WidgetDimensions::scaled.framerect.Horizontal();

		fill.height = resize.height = GetCharacterHeight(FS_NORMAL);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_CI_LIST) return;

		bool rtl = _current_text_dir == TD_RTL; // We allocate space from end-to-start so the label fills.
		int line_height = GetCharacterHeight(FS_NORMAL);

		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		Rect countr = ir.WithWidth(this->count_width, !rtl);
		Rect costr = ir.Indent(this->count_width + WidgetDimensions::scaled.hsep_wide, !rtl).WithWidth(this->cost_width, !rtl);
		Rect labelr = ir.Indent(this->count_width + WidgetDimensions::scaled.hsep_wide + this->cost_width + WidgetDimensions::scaled.hsep_wide, !rtl);

		auto [first, last] = this->GetScrollbar(WID_CI_SCROLLBAR)->GetVisibleRangeIterators(this->list);
		for (auto it = first; it != last; ++it) {
			switch (it->type) {
				case InfrastructureItemType::Header:
					/* Header is allowed to fill the window's width. */
					DrawString(ir.left, ir.right, labelr.top, GetString(it->label), TC_ORANGE);
					break;

				case InfrastructureItemType::Spacer:
					break;

				case InfrastructureItemType::Total:
					/* Draw line in the spacer above the total. */
					GfxFillRect(costr.Translate(0, -WidgetDimensions::scaled.vsep_normal).WithHeight(WidgetDimensions::scaled.fullbevel.top), PC_WHITE);
					DrawString(costr, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_COMPANY_INFRASTRUCTURE_VIEW_TOTAL_PERIOD : STR_COMPANY_INFRASTRUCTURE_VIEW_TOTAL_YEAR, it->cost * 12), TC_BLACK, SA_RIGHT | SA_FORCE);
					break;

				case InfrastructureItemType::Value:
					DrawString(labelr.Indent(WidgetDimensions::scaled.hsep_indent, rtl), GetString(it->label), TC_WHITE);
					DrawString(countr, GetString(STR_JUST_COMMA, it->count), TC_WHITE, SA_RIGHT | SA_FORCE);
					if (_settings_game.economy.infrastructure_maintenance) {
						DrawString(costr, GetString(TimerGameEconomy::UsingWallclockUnits() ? STR_COMPANY_INFRASTRUCTURE_VIEW_TOTAL_PERIOD : STR_COMPANY_INFRASTRUCTURE_VIEW_TOTAL_YEAR, it->cost * 12), TC_BLACK, SA_RIGHT | SA_FORCE);
					}
					break;
			}

			labelr.top += line_height;
			countr.top += line_height;
			costr.top += line_height;
		}
	}

	const IntervalTimer<TimerWindow> redraw_interval = {std::chrono::seconds(1), [this](auto) {
		this->UpdateInfrastructureList();
		this->SetWidgetDirty(WID_CI_LIST);
	}};

	void OnResize() override
	{
		this->GetScrollbar(WID_CI_SCROLLBAR)->SetCapacityFromWidget(this, WID_CI_LIST, WidgetDimensions::scaled.framerect.top);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		this->ReInit();
	}
};

static WindowDesc _company_infrastructure_desc(
	WDP_AUTO, "company_infrastructure", 0, 0,
	WC_COMPANY_INFRASTRUCTURE, WC_NONE,
	{},
	_nested_company_infrastructure_widgets
);

/**
 * Open the infrastructure window of a company.
 * @param company Company to show infrastructure of.
 */
static void ShowCompanyInfrastructure(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	AllocateWindowDescFront<CompanyInfrastructureWindow>(_company_infrastructure_desc, company);
}

static constexpr std::initializer_list<NWidgetPart> _nested_company_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_C_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0), SetPadding(4),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_FACE), SetMinimalSize(92, 119), SetFill(1, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_FACE_TITLE), SetFill(1, 1), SetMinimalTextLines(2, 0),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_C_DESC_INAUGURATION), SetFill(1, 0),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_LABEL, INVALID_COLOUR, WID_C_DESC_COLOUR_SCHEME), SetStringTip(STR_COMPANY_VIEW_COLOUR_SCHEME_TITLE),
							NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_DESC_COLOUR_SCHEME_EXAMPLE), SetMinimalSize(30, 0), SetFill(1, 1),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR, WID_C_DESC_VEHICLE), SetStringTip(STR_COMPANY_VIEW_VEHICLES_TITLE), SetAlignment(SA_LEFT | SA_TOP),
							NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_DESC_VEHICLE_COUNTS), SetMinimalTextLines(4, 0), SetFill(1, 1),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_C_SELECT_VIEW_BUILD_HQ),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_VIEW_HQ), SetStringTip(STR_COMPANY_VIEW_VIEW_HQ_BUTTON, STR_COMPANY_VIEW_VIEW_HQ_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_C_BUILD_HQ), SetStringTip(STR_COMPANY_VIEW_BUILD_HQ_BUTTON, STR_COMPANY_VIEW_BUILD_HQ_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_C_SELECT_RELOCATE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_C_RELOCATE_HQ), SetStringTip(STR_COMPANY_VIEW_RELOCATE_HQ, STR_COMPANY_VIEW_RELOCATE_HQ_TOOLTIP),
							NWidget(NWID_SPACER),
						EndContainer(),
					EndContainer(),
				EndContainer(),

				NWidget(WWT_TEXT, INVALID_COLOUR, WID_C_DESC_COMPANY_VALUE), SetFill(1, 0),

				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_C_DESC_INFRASTRUCTURE), SetStringTip(STR_COMPANY_VIEW_INFRASTRUCTURE),  SetAlignment(SA_LEFT | SA_TOP),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_C_DESC_INFRASTRUCTURE_COUNTS), SetMinimalTextLines(5, 0), SetFill(1, 0),
					NWidget(NWID_VERTICAL), SetPIPRatio(0, 0, 1),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_VIEW_INFRASTRUCTURE), SetStringTip(STR_COMPANY_VIEW_INFRASTRUCTURE_BUTTON, STR_COMPANY_VIEW_INFRASTRUCTURE_TOOLTIP),
					EndContainer(),
				EndContainer(),

				/* Multi player buttons. */
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPIPRatio(1, 0, 0),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_C_SELECT_HOSTILE_TAKEOVER),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_HOSTILE_TAKEOVER), SetStringTip(STR_COMPANY_VIEW_HOSTILE_TAKEOVER_BUTTON, STR_COMPANY_VIEW_HOSTILE_TAKEOVER_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_C_SELECT_GIVE_MONEY),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_GIVE_MONEY), SetStringTip(STR_COMPANY_VIEW_GIVE_MONEY_BUTTON, STR_COMPANY_VIEW_GIVE_MONEY_TOOLTIP),
						EndContainer(),
						NWidget(NWID_SELECTION, INVALID_COLOUR, WID_C_SELECT_MULTIPLAYER),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_COMPANY_JOIN), SetStringTip(STR_COMPANY_VIEW_JOIN, STR_COMPANY_VIEW_JOIN_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	/* Button bars at the bottom. */
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_C_SELECT_BUTTONS),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_NEW_FACE), SetFill(1, 0), SetStringTip(STR_COMPANY_VIEW_NEW_FACE_BUTTON, STR_COMPANY_VIEW_NEW_FACE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_COLOUR_SCHEME), SetFill(1, 0), SetStringTip(STR_COMPANY_VIEW_COLOUR_SCHEME_BUTTON, STR_COMPANY_VIEW_COLOUR_SCHEME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_PRESIDENT_NAME), SetFill(1, 0), SetStringTip(STR_COMPANY_VIEW_PRESIDENT_NAME_BUTTON, STR_COMPANY_VIEW_PRESIDENT_NAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_C_COMPANY_NAME), SetFill(1, 0), SetStringTip(STR_COMPANY_VIEW_COMPANY_NAME_BUTTON, STR_COMPANY_VIEW_COMPANY_NAME_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

/** Strings for the company vehicle counts */
static const StringID _company_view_vehicle_count_strings[] = {
	STR_COMPANY_VIEW_TRAINS, STR_COMPANY_VIEW_ROAD_VEHICLES, STR_COMPANY_VIEW_SHIPS, STR_COMPANY_VIEW_AIRCRAFT
};

/**
 * Window with general information about a company
 */
struct CompanyWindow : Window
{
	/** WID_C_CAPTION does not have a query string, so it can be safely used as invalid value. */
	static constexpr CompanyWidgets INVALID_QUERY_WIDGET = WID_C_CAPTION;

	CompanyWidgets query_widget{};

	/** Display planes in the company window. */
	enum CompanyWindowPlanes : uint8_t {
		/* Display planes of the #WID_C_SELECT_VIEW_BUILD_HQ selection widget. */
		CWP_VB_VIEW = 0,  ///< Display the view button
		CWP_VB_BUILD,     ///< Display the build button

		/* Display planes of the #WID_C_SELECT_RELOCATE selection widget. */
		CWP_RELOCATE_SHOW = 0, ///< Show the relocate HQ button.
		CWP_RELOCATE_HIDE,     ///< Hide the relocate HQ button.
	};

	CompanyWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->owner = this->window_number;
		this->OnInvalidateData();
	}

	void OnPaint() override
	{
		const Company *c = Company::Get(this->window_number);
		bool local = this->window_number == _local_company;

		if (!this->IsShaded()) {
			bool reinit = false;

			/* Button bar selection. */
			reinit |= this->GetWidget<NWidgetStacked>(WID_C_SELECT_BUTTONS)->SetDisplayedPlane(local ? 0 : SZSP_NONE);

			/* Build HQ button handling. */
			reinit |= this->GetWidget<NWidgetStacked>(WID_C_SELECT_VIEW_BUILD_HQ)->SetDisplayedPlane((local && c->location_of_HQ == INVALID_TILE) ? CWP_VB_BUILD : CWP_VB_VIEW);

			this->SetWidgetDisabledState(WID_C_VIEW_HQ, c->location_of_HQ == INVALID_TILE);

			/* Enable/disable 'Relocate HQ' button. */
			reinit |= this->GetWidget<NWidgetStacked>(WID_C_SELECT_RELOCATE)->SetDisplayedPlane((!local || c->location_of_HQ == INVALID_TILE) ? CWP_RELOCATE_HIDE : CWP_RELOCATE_SHOW);
			/* Enable/disable 'Give money' button. */
			reinit |= this->GetWidget<NWidgetStacked>(WID_C_SELECT_GIVE_MONEY)->SetDisplayedPlane((local || _local_company == COMPANY_SPECTATOR || !_settings_game.economy.give_money) ? SZSP_NONE : 0);
			/* Enable/disable 'Hostile Takeover' button. */
			reinit |= this->GetWidget<NWidgetStacked>(WID_C_SELECT_HOSTILE_TAKEOVER)->SetDisplayedPlane((local || _local_company == COMPANY_SPECTATOR || !c->is_ai || _networking) ? SZSP_NONE : 0);

			/* Multiplayer buttons. */
			reinit |= this->GetWidget<NWidgetStacked>(WID_C_SELECT_MULTIPLAYER)->SetDisplayedPlane((!_networking || !NetworkCanJoinCompany(c->index) || _local_company == c->index) ? (int)SZSP_NONE : 0);

			this->SetWidgetDisabledState(WID_C_COMPANY_JOIN, c->is_ai);

			if (reinit) {
				this->ReInit();
				return;
			}
		}

		this->DrawWidgets();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_C_FACE:
				size = maxdim(size, GetScaledSpriteSize(SPR_GRADIENT));
				break;

			case WID_C_DESC_COLOUR_SCHEME_EXAMPLE: {
				Point offset;
				Dimension d = GetSpriteSize(SPR_VEH_BUS_SW_VIEW, &offset);
				d.width -= offset.x;
				d.height -= offset.y;
				size = maxdim(size, d);
				break;
			}

			case WID_C_DESC_COMPANY_VALUE:
				/* INT64_MAX is arguably the maximum company value */
				size.width = GetStringBoundingBox(GetString(STR_COMPANY_VIEW_COMPANY_VALUE, INT64_MAX)).width;
				break;

			case WID_C_DESC_VEHICLE_COUNTS: {
				uint64_t max_value = GetParamMaxValue(5000); // Maximum number of vehicles
				for (const auto &count_string : _company_view_vehicle_count_strings) {
					size.width = std::max(size.width, GetStringBoundingBox(GetString(count_string, max_value)).width + padding.width);
				}
				break;
			}

			case WID_C_DESC_INFRASTRUCTURE_COUNTS: {
				uint64_t max_value = GetParamMaxValue(UINT_MAX);
				size.width = GetStringBoundingBox(GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_RAIL, max_value)).width;
				size.width = std::max(size.width, GetStringBoundingBox(GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_ROAD, max_value)).width);
				size.width = std::max(size.width, GetStringBoundingBox(GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_WATER, max_value)).width);
				size.width = std::max(size.width, GetStringBoundingBox(GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_STATION, max_value)).width);
				size.width = std::max(size.width, GetStringBoundingBox(GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_AIRPORT, max_value)).width);
				size.width = std::max(size.width, GetStringBoundingBox(GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_NONE, max_value)).width);
				size.width += padding.width;
				break;
			}

			case WID_C_VIEW_HQ:
			case WID_C_BUILD_HQ:
			case WID_C_RELOCATE_HQ:
			case WID_C_VIEW_INFRASTRUCTURE:
			case WID_C_GIVE_MONEY:
			case WID_C_HOSTILE_TAKEOVER:
			case WID_C_COMPANY_JOIN:
				size.width = GetStringBoundingBox(STR_COMPANY_VIEW_VIEW_HQ_BUTTON).width;
				size.width = std::max(size.width, GetStringBoundingBox(STR_COMPANY_VIEW_BUILD_HQ_BUTTON).width);
				size.width = std::max(size.width, GetStringBoundingBox(STR_COMPANY_VIEW_RELOCATE_HQ).width);
				size.width = std::max(size.width, GetStringBoundingBox(STR_COMPANY_VIEW_INFRASTRUCTURE_BUTTON).width);
				size.width = std::max(size.width, GetStringBoundingBox(STR_COMPANY_VIEW_GIVE_MONEY_BUTTON).width);
				size.width = std::max(size.width, GetStringBoundingBox(STR_COMPANY_VIEW_HOSTILE_TAKEOVER_BUTTON).width);
				size.width = std::max(size.width, GetStringBoundingBox(STR_COMPANY_VIEW_JOIN).width);
				size.width += padding.width;
				break;
		}
	}

	void DrawVehicleCountsWidget(const Rect &r, const Company *c) const
	{
		static_assert(VEH_COMPANY_END == lengthof(_company_view_vehicle_count_strings));

		int y = r.top;
		for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) {
			uint amount = c->group_all[type].num_vehicle;
			if (amount != 0) {
				DrawString(r.left, r.right, y, GetString(_company_view_vehicle_count_strings[type], amount));
				y += GetCharacterHeight(FS_NORMAL);
			}
		}

		if (y == r.top) {
			/* No String was emitted before, so there must be no vehicles at all. */
			DrawString(r.left, r.right, y, STR_COMPANY_VIEW_VEHICLES_NONE);
		}
	}

	void DrawInfrastructureCountsWidget(const Rect &r, const Company *c) const
	{
		int y = r.top;

		uint rail_pieces = c->infrastructure.signal + c->infrastructure.GetRailTotal();
		if (rail_pieces != 0) {
			DrawString(r.left, r.right, y, GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_RAIL, rail_pieces));
			y += GetCharacterHeight(FS_NORMAL);
		}

		/* GetRoadTotal() skips tram pieces, but we actually want road and tram here. */
		uint road_pieces = std::accumulate(std::begin(c->infrastructure.road), std::end(c->infrastructure.road), 0U);
		if (road_pieces != 0) {
			DrawString(r.left, r.right, y, GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_ROAD, road_pieces));
			y += GetCharacterHeight(FS_NORMAL);
		}

		if (c->infrastructure.water != 0) {
			DrawString(r.left, r.right, y, GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_WATER, c->infrastructure.water));
			y += GetCharacterHeight(FS_NORMAL);
		}

		if (c->infrastructure.station != 0) {
			DrawString(r.left, r.right, y, GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_STATION, c->infrastructure.station));
			y += GetCharacterHeight(FS_NORMAL);
		}

		if (c->infrastructure.airport != 0) {
			DrawString(r.left, r.right, y, GetString(STR_COMPANY_VIEW_INFRASTRUCTURE_AIRPORT, c->infrastructure.airport));
			y += GetCharacterHeight(FS_NORMAL);
		}

		if (y == r.top) {
			/* No String was emitted before, so there must be no infrastructure at all. */
			DrawString(r.left, r.right, y, STR_COMPANY_VIEW_INFRASTRUCTURE_NONE);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const Company *c = Company::Get(this->window_number);
		switch (widget) {
			case WID_C_FACE:
				DrawCompanyManagerFace(c->face, c->colour, r);
				break;

			case WID_C_FACE_TITLE:
				DrawStringMultiLine(r, GetString(STR_COMPANY_VIEW_PRESIDENT_MANAGER_TITLE, c->index), TC_FROMSTRING, SA_HOR_CENTER);
				break;

			case WID_C_DESC_COLOUR_SCHEME_EXAMPLE: {
				Point offset;
				Dimension d = GetSpriteSize(SPR_VEH_BUS_SW_VIEW, &offset);
				d.height -= offset.y;
				DrawSprite(SPR_VEH_BUS_SW_VIEW, GetCompanyPalette(c->index), r.left - offset.x, CentreBounds(r.top, r.bottom, d.height) - offset.y);
				break;
			}

			case WID_C_DESC_VEHICLE_COUNTS:
				DrawVehicleCountsWidget(r, c);
				break;

			case WID_C_DESC_INFRASTRUCTURE_COUNTS:
				DrawInfrastructureCountsWidget(r, c);
				break;
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_C_CAPTION:
				return GetString(STR_COMPANY_VIEW_CAPTION, this->window_number, this->window_number);

			case WID_C_DESC_INAUGURATION: {
				const Company &c = *Company::Get(this->window_number);
				if (TimerGameEconomy::UsingWallclockUnits()) {
					return GetString(STR_COMPANY_VIEW_INAUGURATED_TITLE_WALLCLOCK, c.inaugurated_year_calendar, c.inaugurated_year);
				}
				return GetString(STR_COMPANY_VIEW_INAUGURATED_TITLE, c.inaugurated_year);
			}

			case WID_C_DESC_COMPANY_VALUE:
				return GetString(STR_COMPANY_VIEW_COMPANY_VALUE, CalculateCompanyValue(Company::Get(this->window_number)));

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void OnResize() override
	{
		NWidgetResizeBase *wid = this->GetWidget<NWidgetResizeBase>(WID_C_FACE_TITLE);
		int y = GetStringHeight(GetString(STR_COMPANY_VIEW_PRESIDENT_MANAGER_TITLE, this->owner), wid->current_x);
		if (wid->UpdateVerticalSize(y)) this->ReInit(0, 0);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_C_NEW_FACE: DoSelectCompanyManagerFace(this); break;

			case WID_C_COLOUR_SCHEME:
				ShowCompanyLiveryWindow(this->window_number, GroupID::Invalid());
				break;

			case WID_C_PRESIDENT_NAME:
				ShowQueryString(GetString(STR_PRESIDENT_NAME, this->window_number), STR_COMPANY_VIEW_PRESIDENT_S_NAME_QUERY_CAPTION, MAX_LENGTH_PRESIDENT_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				this->query_widget = WID_C_PRESIDENT_NAME;
				break;

			case WID_C_COMPANY_NAME:
				ShowQueryString(GetString(STR_COMPANY_NAME, this->window_number), STR_COMPANY_VIEW_COMPANY_NAME_QUERY_CAPTION, MAX_LENGTH_COMPANY_NAME_CHARS, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				this->query_widget = WID_C_COMPANY_NAME;
				break;

			case WID_C_VIEW_HQ: {
				TileIndex tile = Company::Get(this->window_number)->location_of_HQ;
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(tile);
				} else {
					ScrollMainWindowToTile(tile);
				}
				break;
			}

			case WID_C_BUILD_HQ:
				if (this->window_number != _local_company) return;
				if (this->IsWidgetLowered(WID_C_BUILD_HQ)) {
					ResetObjectToPlace();
					this->RaiseButtons();
					break;
				}
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, HT_RECT, this);
				SetTileSelectSize(2, 2);
				this->LowerWidget(WID_C_BUILD_HQ);
				this->SetWidgetDirty(WID_C_BUILD_HQ);
				break;

			case WID_C_RELOCATE_HQ:
				if (this->IsWidgetLowered(WID_C_RELOCATE_HQ)) {
					ResetObjectToPlace();
					this->RaiseButtons();
					break;
				}
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, HT_RECT, this);
				SetTileSelectSize(2, 2);
				this->LowerWidget(WID_C_RELOCATE_HQ);
				this->SetWidgetDirty(WID_C_RELOCATE_HQ);
				break;

			case WID_C_VIEW_INFRASTRUCTURE:
				ShowCompanyInfrastructure(this->window_number);
				break;

			case WID_C_GIVE_MONEY:
				ShowQueryString({}, STR_COMPANY_VIEW_GIVE_MONEY_QUERY_CAPTION, 30, this, CS_NUMERAL, {});
				this->query_widget = WID_C_GIVE_MONEY;
				break;

			case WID_C_HOSTILE_TAKEOVER:
				ShowBuyCompanyDialog(this->window_number, true);
				break;

			case WID_C_COMPANY_JOIN: {
				this->query_widget = WID_C_COMPANY_JOIN;
				CompanyID company = this->window_number;
				if (_network_server) {
					NetworkServerDoMove(CLIENT_ID_SERVER, company);
					MarkWholeScreenDirty();
				} else {
					/* just send the join command */
					NetworkClientRequestMove(company);
				}
				break;
			}
		}
	}

	/** Redraw the window on a regular interval. */
	const IntervalTimer<TimerWindow> redraw_interval = {std::chrono::seconds(3), [this](auto) {
		this->SetDirty();
	}};

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (Command<Commands::BuildObject>::Post(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS, tile, OBJECT_HQ, 0) && !_shift_pressed) {
			ResetObjectToPlace();
			this->RaiseButtons();
		}
	}

	void OnPlaceObjectAbort() override
	{
		this->RaiseButtons();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		CompanyWidgets widget = this->query_widget;
		this->query_widget = CompanyWindow::INVALID_QUERY_WIDGET;

		if (!str.has_value()) return;

		switch (widget) {
			default: NOT_REACHED();

			case WID_C_GIVE_MONEY: {
				auto value = ParseInteger<uint64_t>(*str, 10, true);
				if (!value.has_value()) return;
				Money money = *value / GetCurrency().rate;
				Command<Commands::GiveMoney>::Post(STR_ERROR_CAN_T_GIVE_MONEY, money, this->window_number);
				break;
			}

			case WID_C_PRESIDENT_NAME:
				Command<Commands::RenamePresident>::Post(STR_ERROR_CAN_T_CHANGE_PRESIDENT, *str);
				break;

			case WID_C_COMPANY_NAME:
				Command<Commands::RenameCompany>::Post(STR_ERROR_CAN_T_CHANGE_COMPANY_NAME, *str);
				break;
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		/* Manually call OnResize to adjust minimum height of president name widget. */
		if (data == WID_C_PRESIDENT_NAME) this->OnResize();

		/* If a query string is visible, update its default value. */
		if (this->query_widget != CompanyWindow::INVALID_QUERY_WIDGET && data == this->query_widget) {
			UpdateQueryStringDefault(GetString(data == WID_C_COMPANY_NAME ? STR_COMPANY_NAME : STR_PRESIDENT_NAME, this->window_number));
		}
	}
};

static WindowDesc _company_desc(
	WDP_AUTO, "company", 0, 0,
	WC_COMPANY, WC_NONE,
	{},
	_nested_company_widgets
);

/**
 * Show the window with the overview of the company.
 * @param company The company to show the window for.
 */
void ShowCompany(CompanyID company)
{
	if (!Company::IsValidID(company)) return;

	AllocateWindowDescFront<CompanyWindow>(_company_desc, company);
}

/**
 * Redraw all windows with company infrastructure counts.
 * @param company The company to redraw the windows of.
 */
void DirtyCompanyInfrastructureWindows(CompanyID company)
{
	SetWindowDirty(WC_COMPANY, company);
	SetWindowDirty(WC_COMPANY_INFRASTRUCTURE, company);
}

struct BuyCompanyWindow : Window {
	BuyCompanyWindow(WindowDesc &desc, WindowNumber window_number, bool hostile_takeover) : Window(desc), hostile_takeover(hostile_takeover)
	{
		this->InitNested(window_number);

		const Company *c = Company::Get(this->window_number);
		this->company_value = hostile_takeover ? CalculateHostileTakeoverValue(c) : c->bankrupt_value;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_BC_FACE:
				size = GetScaledSpriteSize(SPR_GRADIENT);
				break;

			case WID_BC_QUESTION:
				const Company *c = Company::Get(this->window_number);
				size.height = GetStringHeight(GetString(this->hostile_takeover ? STR_BUY_COMPANY_HOSTILE_TAKEOVER : STR_BUY_COMPANY_MESSAGE, c->index, this->company_value), size.width);
				break;
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_BC_CAPTION:
				return GetString(STR_ERROR_MESSAGE_CAPTION_OTHER_COMPANY, Company::Get(this->window_number)->index);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BC_FACE: {
				const Company *c = Company::Get(this->window_number);
				DrawCompanyManagerFace(c->face, c->colour, r);
				break;
			}

			case WID_BC_QUESTION: {
				const Company *c = Company::Get(this->window_number);
				DrawStringMultiLine(r, GetString(this->hostile_takeover ? STR_BUY_COMPANY_HOSTILE_TAKEOVER : STR_BUY_COMPANY_MESSAGE, c->index, this->company_value), TC_FROMSTRING, SA_CENTER);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BC_NO:
				this->Close();
				break;

			case WID_BC_YES:
				Command<Commands::BuyCompany>::Post(STR_ERROR_CAN_T_BUY_COMPANY, this->window_number, this->hostile_takeover);
				break;
		}
	}

	/**
	 * Check on a regular interval if the company value has changed.
	 */
	const IntervalTimer<TimerWindow> rescale_interval = {std::chrono::seconds(3), [this](auto) {
		/* Value can't change when in bankruptcy. */
		if (!this->hostile_takeover) return;

		const Company *c = Company::Get(this->window_number);
		auto new_value = CalculateHostileTakeoverValue(c);
		if (new_value != this->company_value) {
			this->company_value = new_value;
			this->ReInit();
		}
	}};

private:
	bool hostile_takeover = false; ///< Whether the window is showing a hostile takeover.
	Money company_value{}; ///< The value of the company for which the user can buy it.
};

static constexpr std::initializer_list<NWidgetPart> _nested_buy_company_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_LIGHT_BLUE),
		NWidget(WWT_CAPTION, COLOUR_LIGHT_BLUE, WID_BC_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_LIGHT_BLUE),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BC_FACE), SetFill(0, 1),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BC_QUESTION), SetMinimalSize(240, 0), SetFill(1, 1),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(100, WidgetDimensions::unscaled.hsep_wide, 100),
				NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, WID_BC_NO), SetMinimalSize(60, 12), SetStringTip(STR_QUIT_NO), SetFill(1, 0),
				NWidget(WWT_TEXTBTN, COLOUR_LIGHT_BLUE, WID_BC_YES), SetMinimalSize(60, 12), SetStringTip(STR_QUIT_YES), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _buy_company_desc(
	WDP_AUTO, {}, 0, 0,
	WC_BUY_COMPANY, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_buy_company_widgets
);

/**
 * Show the query to buy another company.
 * @param company The company to buy.
 * @param hostile_takeover Whether this is a hostile takeover.
 */
void ShowBuyCompanyDialog(CompanyID company, bool hostile_takeover)
{
	auto window = BringWindowToFrontById(WC_BUY_COMPANY, company);
	if (window == nullptr) {
		new BuyCompanyWindow(_buy_company_desc, company, hostile_takeover);
	}
}
