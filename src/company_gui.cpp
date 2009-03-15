/* $Id$ */

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
#include "roadveh.h"
#include "train.h"
#include "aircraft.h"
#include "newgrf.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "date_func.h"
#include "string_func.h"
#include "widgets/dropdown_type.h"
#include "tilehighlight_func.h"
#include "settings_type.h"

#include "table/strings.h"

enum {
	FIRST_GUI_CALL = INT_MAX,  ///< default value to specify thuis is the first call of the resizable gui
};

static void DoShowCompanyFinances(CompanyID company, bool show_small, bool show_stickied, int top = FIRST_GUI_CALL, int left = FIRST_GUI_CALL);
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
	const ExpensesType *et; ///< Expenses items.
	const int length;       ///< Number of items in list.
	const int height;       ///< Height of list, 10 pixels per item, plus an additional 12 pixels per subtotal. */
};

static const ExpensesList _expenses_list_types[] = {
	{ _expenses_list_1, lengthof(_expenses_list_1), lengthof(_expenses_list_1) * 10 },
	{ _expenses_list_2, lengthof(_expenses_list_2), lengthof(_expenses_list_2) * 10 + 3 * 12 },
};

static void DrawCompanyEconomyStats(const Company *c, bool small)
{
	int type = _settings_client.gui.expenses_layout;
	int x, y, i, j, year;
	const Money (*tbl)[EXPENSES_END];
	StringID str;

	if (!small) { // normal sized economics window
		/* draw categories */
		DrawStringCenterUnderline(61, 15, STR_700F_EXPENDITURE_INCOME, TC_FROMSTRING);

		y = 27;
		for (i = 0; i < _expenses_list_types[type].length; i++) {
			ExpensesType et = _expenses_list_types[type].et[i];
			if (et == INVALID_EXPENSES) {
				y += 2;
				DrawStringRightAligned(111, y, STR_7020_TOTAL, TC_FROMSTRING);
				y += 20;
			} else {
				DrawString(2, y, STR_7011_CONSTRUCTION + et, TC_FROMSTRING);
				y += 10;
			}
		}

		DrawStringRightAligned(111, y + 2, STR_7020_TOTAL, TC_FROMSTRING);

		/* draw the price columns */
		year = _cur_year - 2;
		j = 3;
		x = 215;
		tbl = c->yearly_expenses + 2;

		do {
			if (year >= c->inaugurated_year) {
				SetDParam(0, year);
				DrawStringRightAlignedUnderline(x, 15, STR_7010, TC_FROMSTRING);

				Money sum = 0;
				Money subtotal = 0;

				int y = 27;

				for (int i = 0; i < _expenses_list_types[type].length; i++) {
					ExpensesType et = _expenses_list_types[type].et[i];
					Money cost;

					if (et == INVALID_EXPENSES) {
						GfxFillRect(x - 75, y, x, y, 215);
						cost = subtotal;
						subtotal = 0;
						y += 2;
					} else {
						cost = (*tbl)[et];
						subtotal += cost;
						sum += cost;
					}

					if (cost != 0 || et == INVALID_EXPENSES) {
						str = STR_701E;
						if (cost < 0) { cost = -cost; str++; }
						SetDParam(0, cost);
						DrawStringRightAligned(x, y, str, TC_FROMSTRING);
					}
					y += (et == INVALID_EXPENSES) ? 20 : 10;
				}

				str = STR_701E;
				if (sum < 0) { sum = -sum; str++; }
				SetDParam(0, sum);
				DrawStringRightAligned(x, y + 2, str, TC_FROMSTRING);

				GfxFillRect(x - 75, y, x, y, 215);
				x += 95;
			}
			year++;
			tbl--;
		} while (--j != 0);

		y += 14;

		/* draw max loan aligned to loan below (y += 10) */
		SetDParam(0, _economy.max_loan);
		DrawString(202, y + 10, STR_MAX_LOAN, TC_FROMSTRING);
	} else {
		y = 15;
	}

	DrawString(2, y, STR_7026_BANK_BALANCE, TC_FROMSTRING);
	SetDParam(0, c->money);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);

	y += 10;

	DrawString(2, y, STR_7027_LOAN, TC_FROMSTRING);
	SetDParam(0, c->current_loan);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);

	y += 12;

	GfxFillRect(182 - 75, y - 2, 182, y - 2, 215);

	SetDParam(0, c->money - c->current_loan);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);
}

enum CompanyFinancesWindowWidgets {
	CFW_WIDGET_TOGGLE_SIZE   = 2,
	CFW_WIDGET_EXPS_PANEL    = 4,
	CFW_WIDGET_TOTAL_PANEL   = 5,
	CFW_WIDGET_INCREASE_LOAN = 6,
	CFW_WIDGET_REPAY_LOAN    = 7,
};

static const Widget _company_finances_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,       0,      13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   379,       0,      13, STR_700E_FINANCES,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   380,   394,       0,      13, SPR_LARGE_SMALL_WINDOW, STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_GREY,   395,   406,       0,      13, 0x0,                    STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   406,      14, 13 + 10, 0x0,    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   406, 14 + 10, 47 + 10, 0x0, STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   202, 48 + 10, 59 + 10, STR_7029_BORROW,        STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   203,   406, 48 + 10, 59 + 10, STR_702A_REPAY,         STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};

static const Widget _company_finances_small_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   253,     0,    13, STR_700E_FINANCES,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   254,   267,     0,    13, SPR_LARGE_SMALL_WINDOW, STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_GREY,   268,   279,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,     0, 0x0,                    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   279,    14,    47, STR_NULL,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   139,    48,    59, STR_7029_BORROW,        STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   140,   279,    48,    59, STR_702A_REPAY,         STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};

struct CompanyFinancesWindow : Window {
	bool small;

	CompanyFinancesWindow(const WindowDesc *desc, CompanyID company, bool show_small,
					bool show_stickied, int top, int left) :
			Window(desc, company),
			small(show_small)
	{
		this->owner = (Owner)this->window_number;

		if (show_stickied) this->flags4 |= WF_STICKY;

		/* Check if repositioning from default is required */
		if (top != FIRST_GUI_CALL && left != FIRST_GUI_CALL) {
			this->top = top;
			this->left = left;
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		CompanyID company = (CompanyID)this->window_number;
		const Company *c = GetCompany(company);

		if (!small) {
			int type = _settings_client.gui.expenses_layout;
			int height = this->widget[CFW_WIDGET_EXPS_PANEL].bottom - this->widget[CFW_WIDGET_EXPS_PANEL].top + 1;
			if (_expenses_list_types[type].height + 26 != height) {
				this->SetDirty();
				ResizeWindowForWidget(this, CFW_WIDGET_EXPS_PANEL, 0, _expenses_list_types[type].height - height + 26);
				this->SetDirty();
				return;
			}
		}

		/* Recheck the size of the window as it might need to be resized due to the local company changing */
		int new_height = this->widget[(company == _local_company) ? CFW_WIDGET_INCREASE_LOAN : CFW_WIDGET_TOTAL_PANEL].bottom + 1;
		if (this->height != new_height) {
			/* Make window dirty before and after resizing */
			this->SetDirty();
			this->height = new_height;
			this->SetDirty();

			this->SetWidgetHiddenState(CFW_WIDGET_INCREASE_LOAN, company != _local_company);
			this->SetWidgetHiddenState(CFW_WIDGET_REPAY_LOAN,    company != _local_company);
		}

		/* Borrow button only shows when there is any more money to loan */
		this->SetWidgetDisabledState(CFW_WIDGET_INCREASE_LOAN, c->current_loan == _economy.max_loan);

		/* Repay button only shows when there is any more money to repay */
		this->SetWidgetDisabledState(CFW_WIDGET_REPAY_LOAN, company != _local_company || c->current_loan == 0);

		SetDParam(0, c->index);
		SetDParam(1, c->index);
		SetDParam(2, LOAN_INTERVAL);
		this->DrawWidgets();

		DrawCompanyEconomyStats(c, this->small);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case CFW_WIDGET_TOGGLE_SIZE: {// toggle size
				bool new_mode = !this->small;
				bool stickied = !!(this->flags4 & WF_STICKY);
				int oldtop = this->top;   ///< current top position of the window before closing it
				int oldleft = this->left; ///< current left position of the window before closing it
				CompanyID company = (CompanyID)this->window_number;

				delete this;
				/* Open up the (toggled size) Finance window at the same position as the previous */
				DoShowCompanyFinances(company, new_mode, stickied, oldtop, oldleft);
			}
			break;

			case CFW_WIDGET_INCREASE_LOAN: // increase loan
				DoCommandP(0, 0, _ctrl_pressed, CMD_INCREASE_LOAN | CMD_MSG(STR_702C_CAN_T_BORROW_ANY_MORE_MONEY));
				break;

			case CFW_WIDGET_REPAY_LOAN: // repay loan
				DoCommandP(0, 0, _ctrl_pressed, CMD_DECREASE_LOAN | CMD_MSG(STR_702F_CAN_T_REPAY_LOAN));
				break;
		}
	}
};

static const WindowDesc _company_finances_desc(
	WDP_AUTO, WDP_AUTO, 407, 60 + 10, 407, 60 + 10,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_company_finances_widgets
);

static const WindowDesc _company_finances_small_desc(
	WDP_AUTO, WDP_AUTO, 280, 60, 280, 60,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_company_finances_small_widgets
);

/**
 * Open the small/large finance window of the company
 *
 * @param company        the company who's finances are requested to be seen
 * @param show_small     show large or small version opf the window
 * @param show_stickied  previous "stickyness" of the window
 * @param top            previous top position of the window
 * @param left           previous left position of the window
 *
 * @pre is company a valid company
 */
static void DoShowCompanyFinances(CompanyID company, bool show_small, bool show_stickied, int top, int left)
{
	if (!IsValidCompanyID(company)) return;

	if (BringWindowToFrontById(WC_FINANCES, company)) return;
	new CompanyFinancesWindow(show_small ? &_company_finances_small_desc : &_company_finances_desc, company, show_small, show_stickied, top, left);
}

void ShowCompanyFinances(CompanyID company)
{
	DoShowCompanyFinances(company, false, false);
}

/* List of colours for the livery window */
static const StringID _colour_dropdown[] = {
	STR_00D1_DARK_BLUE,
	STR_00D2_PALE_GREEN,
	STR_00D3_PINK,
	STR_00D4_YELLOW,
	STR_00D5_RED,
	STR_00D6_LIGHT_BLUE,
	STR_00D7_GREEN,
	STR_00D8_DARK_GREEN,
	STR_00D9_BLUE,
	STR_00DA_CREAM,
	STR_00DB_MAUVE,
	STR_00DC_PURPLE,
	STR_00DD_ORANGE,
	STR_00DE_BROWN,
	STR_00DF_GREY,
	STR_00E0_WHITE,
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
		return 14;
	}

	bool Selectable() const
	{
		return true;
	}

	void Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
	{
		DrawSprite(SPR_VEH_BUS_SIDE_VIEW, PALETTE_RECOLOUR_START + this->result, x + 16, y + 7);
		DrawStringTruncated(x + 32, y + 3, this->String(), sel ? TC_WHITE : TC_BLACK, width - 30);
	}
};

struct SelectCompanyLiveryWindow : public Window {
private:
	uint32 sel;
	LiveryClass livery_class;

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
		livery = &GetCompany((CompanyID)this->window_number)->livery[scheme];

		DropDownList *list = new DropDownList();
		for (uint i = 0; i < lengthof(_colour_dropdown); i++) {
			list->push_back(new DropDownListColourItem(i, HasBit(used_colours, i)));
		}

		ShowDropDownList(this, list, widget == SCLW_WIDGET_PRI_COL_DROPDOWN ? livery->colour1 : livery->colour2, widget);
	}

public:
	SelectCompanyLiveryWindow(const WindowDesc *desc, CompanyID company) : Window(desc, company)
	{
		this->owner = company;
		this->livery_class = LC_OTHER;
		this->sel = 1;
		this->LowerWidget(SCLW_WIDGET_CLASS_GENERAL);
		this->OnInvalidateData(_loaded_newgrf_features.has_2CC);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const Company *c = GetCompany((CompanyID)this->window_number);
		LiveryScheme scheme = LS_DEFAULT;
		int y = 51;

		/* Disable dropdown controls if no scheme is selected */
		this->SetWidgetDisabledState(SCLW_WIDGET_PRI_COL_DROPDOWN, this->sel == 0);
		this->SetWidgetDisabledState(SCLW_WIDGET_SEC_COL_DROPDOWN, this->sel == 0);

		if (this->sel != 0) {
			for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
				if (HasBit(this->sel, scheme)) break;
			}
			if (scheme == LS_END) scheme = LS_DEFAULT;
		}

		SetDParam(0, STR_00D1_DARK_BLUE + c->livery[scheme].colour1);
		SetDParam(1, STR_00D1_DARK_BLUE + c->livery[scheme].colour2);

		this->DrawWidgets();

		for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
			if (_livery_class[scheme] == this->livery_class) {
				bool sel = HasBit(this->sel, scheme) != 0;

				if (scheme != LS_DEFAULT) {
					DrawSprite(c->livery[scheme].in_use ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, 2, y);
				}

				DrawString(15, y, STR_LIVERY_DEFAULT + scheme, sel ? TC_WHITE : TC_BLACK);

				DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOUR(c->livery[scheme].colour1), 152, y);
				DrawString(165, y, STR_00D1_DARK_BLUE + c->livery[scheme].colour1, sel ? TC_WHITE : TC_GOLD);

				if (!this->IsWidgetHidden(SCLW_WIDGET_SEC_COL_DROPDOWN)) {
					DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOUR(c->livery[scheme].colour2), 277, y);
					DrawString(290, y, STR_00D1_DARK_BLUE + c->livery[scheme].colour2, sel ? TC_WHITE : TC_GOLD);
				}

				y += 14;
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Number of liveries in each class, used to determine the height of the livery window */
		static const byte livery_height[] = {
			1,
			13,
			4,
			2,
			3,
		};

		switch (widget) {
			/* Livery Class buttons */
			case SCLW_WIDGET_CLASS_GENERAL:
			case SCLW_WIDGET_CLASS_RAIL:
			case SCLW_WIDGET_CLASS_ROAD:
			case SCLW_WIDGET_CLASS_SHIP:
			case SCLW_WIDGET_CLASS_AIRCRAFT: {
				LiveryScheme scheme;

				this->RaiseWidget(this->livery_class + SCLW_WIDGET_CLASS_GENERAL);
				this->livery_class = (LiveryClass)(widget - SCLW_WIDGET_CLASS_GENERAL);
				this->sel = 0;
				this->LowerWidget(this->livery_class + SCLW_WIDGET_CLASS_GENERAL);

				/* Select the first item in the list */
				for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					if (_livery_class[scheme] == this->livery_class) {
						this->sel = 1 << scheme;
						break;
					}
				}
				this->height = 49 + livery_height[this->livery_class] * 14;
				this->widget[SCLW_WIDGET_MATRIX].bottom = this->height - 1;
				this->widget[SCLW_WIDGET_MATRIX].data = livery_height[this->livery_class] << 8 | 1;
				MarkWholeScreenDirty();
				break;
			}

			case SCLW_WIDGET_PRI_COL_DROPDOWN: // First colour dropdown
				ShowColourDropDownMenu(SCLW_WIDGET_PRI_COL_DROPDOWN);
				break;

			case SCLW_WIDGET_SEC_COL_DROPDOWN: // Second colour dropdown
				ShowColourDropDownMenu(SCLW_WIDGET_SEC_COL_DROPDOWN);
				break;

			case SCLW_WIDGET_MATRIX: {
				LiveryScheme scheme;
				LiveryScheme j = (LiveryScheme)((pt.y - 48) / 14);

				for (scheme = LS_BEGIN; scheme <= j; scheme++) {
					if (_livery_class[scheme] != this->livery_class) j++;
					if (scheme >= LS_END) return;
				}
				if (j >= LS_END) return;

				/* If clicking on the left edge, toggle using the livery */
				if (pt.x < 10) {
					DoCommandP(0, j | (2 << 8), !GetCompany((CompanyID)this->window_number)->livery[j].in_use, CMD_SET_COMPANY_COLOUR);
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
		static bool has2cc = true;

		if (has2cc == !!data) return;

		has2cc = !!data;

		int r = this->widget[has2cc ? SCLW_WIDGET_SEC_COL_DROPDOWN : SCLW_WIDGET_PRI_COL_DROPDOWN].right;
		this->SetWidgetHiddenState(SCLW_WIDGET_SEC_COL_DROPDOWN, !has2cc);
		this->widget[SCLW_WIDGET_CAPTION].right = r;
		this->widget[SCLW_WIDGET_SPACER_CLASS].right = r;
		this->widget[SCLW_WIDGET_MATRIX].right = r;
		this->width = r + 1;
	}
};

static const Widget _select_company_livery_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE,  COLOUR_GREY,   0,  10,   0,  13, STR_00C5,                   STR_018B_CLOSE_WINDOW },
{  WWT_CAPTION, RESIZE_NONE,  COLOUR_GREY,  11, 399,   0,  13, STR_7007_NEW_COLOUR_SCHEME, STR_018C_WINDOW_TITLE_DRAG_THIS },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,  21,  14,  35, SPR_IMG_COMPANY_GENERAL,    STR_LIVERY_GENERAL_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  22,  43,  14,  35, SPR_IMG_TRAINLIST,          STR_LIVERY_TRAIN_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  44,  65,  14,  35, SPR_IMG_TRUCKLIST,          STR_LIVERY_ROADVEH_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  66,  87,  14,  35, SPR_IMG_SHIPLIST,           STR_LIVERY_SHIP_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  88, 109,  14,  35, SPR_IMG_AIRPLANESLIST,      STR_LIVERY_AIRCRAFT_TIP },
{    WWT_PANEL, RESIZE_NONE,  COLOUR_GREY, 110, 399,  14,  35, 0x0,                        STR_NULL },
{    WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0, 149,  36,  47, 0x0,                        STR_NULL },
{ WWT_DROPDOWN, RESIZE_NONE,  COLOUR_GREY, 150, 274,  36,  47, STR_02BD,                   STR_LIVERY_PRIMARY_TIP },
{ WWT_DROPDOWN, RESIZE_NONE,  COLOUR_GREY, 275, 399,  36,  47, STR_02E1,                   STR_LIVERY_SECONDARY_TIP },
{   WWT_MATRIX, RESIZE_NONE,  COLOUR_GREY,   0, 399,  48,  48 + 1 * 14, (1 << 8) | 1,      STR_LIVERY_PANEL_TIP },
{ WIDGETS_END },
};

static const WindowDesc _select_company_livery_desc(
	WDP_AUTO, WDP_AUTO, 400, 49 + 1 * 14, 400, 49 + 1 * 14,
	WC_COMPANY_COLOUR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_select_company_livery_widgets
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

/** Widget description for the normal/simple company manager face selection dialog */
static const Widget _select_company_manager_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},              // SCMFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   174,     0,    13, STR_7043_FACE_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // SCMFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   189,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_ADVANCED_TIP},              // SCMFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   189,    14,   150, 0x0,                     STR_NULL},                           // SCMFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    94,   151,   162, STR_012E_CANCEL,         STR_7047_CANCEL_NEW_FACE_SELECTION}, // SCMFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   189,   151,   162, STR_012F_OK,             STR_7048_ACCEPT_NEW_FACE_SELECTION}, // SCMFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    75,    86, STR_7044_MALE,           STR_7049_SELECT_MALE_FACES},         // SCMFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    87,    98, STR_7045_FEMALE,         STR_704A_SELECT_FEMALE_FACES},       // SCMFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   137,   148, STR_7046_NEW_FACE,       STR_704B_GENERATE_RANDOM_NEW_FACE},  // SCMFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    16,    27, STR_FACE_ADVANCED,       STR_FACE_ADVANCED_TIP},              // SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{   WIDGETS_END},
};

/** Widget description for the advanced company manager face selection dialog */
static const Widget _select_company_manager_face_adv_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},              // SCMFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   204,     0,    13, STR_7043_FACE_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // SCMFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   205,   219,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_SIMPLE_TIP},                // SCMFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   219,    14,   207, 0x0,                     STR_NULL},                           // SCMFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    94,   208,   219, STR_012E_CANCEL,         STR_7047_CANCEL_NEW_FACE_SELECTION}, // SCMFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   219,   208,   219, STR_012F_OK,             STR_7048_ACCEPT_NEW_FACE_SELECTION}, // SCMFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    96,   156,    32,    43, STR_7044_MALE,           STR_7049_SELECT_MALE_FACES},         // SCMFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   157,   217,    32,    43, STR_7045_FEMALE,         STR_704A_SELECT_FEMALE_FACES},       // SCMFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   137,   148, STR_RANDOM,              STR_704B_GENERATE_RANDOM_NEW_FACE},  // SCMFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   217,    16,    27, STR_FACE_SIMPLE,         STR_FACE_SIMPLE_TIP},                // SCMFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   158,   169, STR_FACE_LOAD,           STR_FACE_LOAD_TIP},                  // SCMFW_WIDGET_LOAD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   170,   181, STR_FACE_FACECODE,       STR_FACE_FACECODE_TIP},              // SCMFW_WIDGET_FACECODE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   182,   193, STR_FACE_SAVE,           STR_FACE_SAVE_TIP},                  // SCMFW_WIDGET_SAVE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    96,   156,    46,    57, STR_FACE_EUROPEAN,       STR_FACE_SELECT_EUROPEAN},           // SCMFW_WIDGET_ETHNICITY_EUR
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   157,   217,    46,    57, STR_FACE_AFRICAN,        STR_FACE_SELECT_AFRICAN},            // SCMFW_WIDGET_ETHNICITY_AFR
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   217,    60,    71, STR_EMPTY,               STR_FACE_MOUSTACHE_EARRING_TIP},     // SCMFW_WIDGET_HAS_MOUSTACHE_EARRING
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   217,    72,    83, STR_EMPTY,               STR_FACE_GLASSES_TIP},               // SCMFW_WIDGET_HAS_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   110,   121, SPR_ARROW_LEFT,          STR_FACE_EYECOLOUR_TIP},             // SCMFW_WIDGET_EYECOLOUR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   110,   121, STR_EMPTY,               STR_FACE_EYECOLOUR_TIP},             // SCMFW_WIDGET_EYECOLOUR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   110,   121, SPR_ARROW_RIGHT,         STR_FACE_EYECOLOUR_TIP},             // SCMFW_WIDGET_EYECOLOUR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   158,   169, SPR_ARROW_LEFT,          STR_FACE_CHIN_TIP},                  // SCMFW_WIDGET_CHIN_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   158,   169, STR_EMPTY,               STR_FACE_CHIN_TIP},                  // SCMFW_WIDGET_CHIN
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   158,   169, SPR_ARROW_RIGHT,         STR_FACE_CHIN_TIP},                  // SCMFW_WIDGET_CHIN_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,    98,   109, SPR_ARROW_LEFT,          STR_FACE_EYEBROWS_TIP},              // SCMFW_WIDGET_EYEBROWS_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,    98,   109, STR_EMPTY,               STR_FACE_EYEBROWS_TIP},              // SCMFW_WIDGET_EYEBROWS
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,    98,   109, SPR_ARROW_RIGHT,         STR_FACE_EYEBROWS_TIP},              // SCMFW_WIDGET_EYEBROWS_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   146,   157, SPR_ARROW_LEFT,          STR_FACE_LIPS_MOUSTACHE_TIP},        // SCMFW_WIDGET_LIPS_MOUSTACHE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   146,   157, STR_EMPTY,               STR_FACE_LIPS_MOUSTACHE_TIP},        // SCMFW_WIDGET_LIPS_MOUSTACHE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   146,   157, SPR_ARROW_RIGHT,         STR_FACE_LIPS_MOUSTACHE_TIP},        // SCMFW_WIDGET_LIPS_MOUSTACHE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   134,   145, SPR_ARROW_LEFT,          STR_FACE_NOSE_TIP},                  // SCMFW_WIDGET_NOSE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   134,   145, STR_EMPTY,               STR_FACE_NOSE_TIP},                  // SCMFW_WIDGET_NOSE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   134,   145, SPR_ARROW_RIGHT,         STR_FACE_NOSE_TIP},                  // SCMFW_WIDGET_NOSE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,    86,    97, SPR_ARROW_LEFT,          STR_FACE_HAIR_TIP},                  // SCMFW_WIDGET_HAIR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,    86,    97, STR_EMPTY,               STR_FACE_HAIR_TIP},                  // SCMFW_WIDGET_HAIR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,    86,    97, SPR_ARROW_RIGHT,         STR_FACE_HAIR_TIP},                  // SCMFW_WIDGET_HAIR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   170,   181, SPR_ARROW_LEFT,          STR_FACE_JACKET_TIP},                // SCMFW_WIDGET_JACKET_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   170,   181, STR_EMPTY,               STR_FACE_JACKET_TIP},                // SCMFW_WIDGET_JACKET
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   170,   181, SPR_ARROW_RIGHT,         STR_FACE_JACKET_TIP},                // SCMFW_WIDGET_JACKET_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   182,   193, SPR_ARROW_LEFT,          STR_FACE_COLLAR_TIP},                // SCMFW_WIDGET_COLLAR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   182,   193, STR_EMPTY,               STR_FACE_COLLAR_TIP},                // SCMFW_WIDGET_COLLAR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   182,   193, SPR_ARROW_RIGHT,         STR_FACE_COLLAR_TIP},                // SCMFW_WIDGET_COLLAR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   194,   205, SPR_ARROW_LEFT,          STR_FACE_TIE_EARRING_TIP},           // SCMFW_WIDGET_TIE_EARRING_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   194,   205, STR_EMPTY,               STR_FACE_TIE_EARRING_TIP},           // SCMFW_WIDGET_TIE_EARRING
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   194,   205, SPR_ARROW_RIGHT,         STR_FACE_TIE_EARRING_TIP},           // SCMFW_WIDGET_TIE_EARRING_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   122,   133, SPR_ARROW_LEFT,          STR_FACE_GLASSES_TIP_2},             // SCMFW_WIDGET_GLASSES_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   122,   133, STR_EMPTY,               STR_FACE_GLASSES_TIP_2},             // SCMFW_WIDGET_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   122,   133, SPR_ARROW_RIGHT,         STR_FACE_GLASSES_TIP_2},             // SCMFW_WIDGET_GLASSES_R
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
	};
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
		DrawStringRightAligned(this->widget[widget_index].left - (is_bool_widget ? 5 : 14), this->widget[widget_index].top + 1, str, TC_GOLD);

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
			DrawStringCentered(this->widget[widget_index].left + (this->widget[widget_index].right - this->widget[widget_index].left) / 2 +
				this->IsWidgetLowered(widget_index), this->widget[widget_index].top + 1 + this->IsWidgetLowered(widget_index), str, TC_WHITE);
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
		this->face = GetCompany((CompanyID)this->window_number)->face;
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
		DrawCompanyManagerFace(this->face, GetCompany((CompanyID)this->window_number)->colour, 2, 16);
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
				ShowErrorMessage(INVALID_STRING_ID, STR_FACE_LOAD_DONE, 0, 0);
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
				ShowErrorMessage(INVALID_STRING_ID, STR_FACE_SAVE_DONE, 0, 0);
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
			ShowErrorMessage(INVALID_STRING_ID, STR_FACE_FACECODE_SET, 0, 0);
			this->UpdateData();
			this->SetDirty();
		} else {
			ShowErrorMessage(INVALID_STRING_ID, STR_FACE_FACECODE_ERR, 0, 0);
		}
	}
};

/** normal/simple company manager face selection window description */
static const WindowDesc _select_company_manager_face_desc(
	WDP_AUTO, WDP_AUTO, 190, 163, 190, 163,
	WC_COMPANY_MANAGER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_select_company_manager_face_widgets
);

/** advanced company manager face selection window description */
static const WindowDesc _select_company_manager_face_adv_desc(
	WDP_AUTO, WDP_AUTO, 220, 220, 220, 220,
	WC_COMPANY_MANAGER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_CONSTRUCTION,
	_select_company_manager_face_adv_widgets
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
	if (!IsValidCompanyID((CompanyID)parent->window_number)) return;

	if (BringWindowToFrontById(WC_COMPANY_MANAGER_FACE, parent->window_number)) return;
	new SelectCompanyManagerFaceWindow(adv ? &_select_company_manager_face_adv_desc : &_select_company_manager_face_desc, parent, adv, top, left); // simple or advanced window
}


/* Names of the widgets. Keep them in the same order as in the widget array */
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   359,     0,    13, STR_7001,                          STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   359,    14,   157, 0x0,                               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    89,   158,   169, STR_7004_NEW_FACE,                 STR_7030_SELECT_NEW_FACE_FOR_PRESIDENT},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    90,   179,   158,   169, STR_7005_COLOUR_SCHEME,            STR_7031_CHANGE_THE_COMPANY_VEHICLE},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   180,   269,   158,   169, STR_7009_PRESIDENT_NAME,           STR_7032_CHANGE_THE_PRESIDENT_S},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   270,   359,   158,   169, STR_7008_COMPANY_NAME,             STR_7033_CHANGE_THE_COMPANY_NAME},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,    18,    29, STR_7072_VIEW_HQ,                  STR_7070_BUILD_COMPANY_HEADQUARTERS},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,    32,    43, STR_RELOCATE_HQ,                   STR_RELOCATE_COMPANY_HEADQUARTERS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   179,   158,   169, STR_7077_BUY_25_SHARE_IN_COMPANY,  STR_7079_BUY_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   180,   359,   158,   169, STR_7078_SELL_25_SHARE_IN_COMPANY, STR_707A_SELL_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,   138,   149, STR_COMPANY_PASSWORD,              STR_COMPANY_PASSWORD_TOOLTIP},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,   138,   149, STR_COMPANY_JOIN,                  STR_COMPANY_JOIN_TIP},
{   WIDGETS_END},
};


/**
 * Draws text "Vehicles:" and number of all vehicle types, or "(none)"
 * @param company ID of company to print statistics of
 */
static void DrawCompanyVehiclesAmount(CompanyID company)
{
	const int x = 110;
	int y = 63;
	const Vehicle *v;
	uint train = 0;
	uint road  = 0;
	uint air   = 0;
	uint ship  = 0;

	DrawString(x, y, STR_7039_VEHICLES, TC_FROMSTRING);

	FOR_ALL_VEHICLES(v) {
		if (v->owner == company) {
			switch (v->type) {
				case VEH_TRAIN:    if (IsFrontEngine(v)) train++; break;
				case VEH_ROAD:     if (IsRoadVehFront(v)) road++; break;
				case VEH_AIRCRAFT: if (IsNormalAircraft(v)) air++; break;
				case VEH_SHIP:     ship++; break;
				default: break;
			}
		}
	}

	if (train + road + air + ship == 0) {
		DrawString(x + 70, y, STR_7042_NONE, TC_FROMSTRING);
	} else {
		if (train != 0) {
			SetDParam(0, train);
			DrawString(x + 70, y, STR_TRAINS, TC_FROMSTRING);
			y += 10;
		}

		if (road != 0) {
			SetDParam(0, road);
			DrawString(x + 70, y, STR_ROAD_VEHICLES, TC_FROMSTRING);
			y += 10;
		}

		if (air != 0) {
			SetDParam(0, air);
			DrawString(x + 70, y, STR_AIRCRAFT, TC_FROMSTRING);
			y += 10;
		}

		if (ship != 0) {
			SetDParam(0, ship);
			DrawString(x + 70, y, STR_SHIPS, TC_FROMSTRING);
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

			DrawString(120, (num++) * height + 116, STR_707D_OWNED_BY, TC_FROMSTRING);
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
		const Company *c = GetCompany((CompanyID)this->window_number);
		bool local = this->window_number == _local_company;

		this->SetWidgetHiddenState(CW_WIDGET_NEW_FACE,       !local);
		this->SetWidgetHiddenState(CW_WIDGET_COLOUR_SCHEME,   !local);
		this->SetWidgetHiddenState(CW_WIDGET_PRESIDENT_NAME, !local);
		this->SetWidgetHiddenState(CW_WIDGET_COMPANY_NAME,   !local);
		this->widget[CW_WIDGET_BUILD_VIEW_HQ].data = (local && c->location_of_HQ == INVALID_TILE) ? STR_706F_BUILD_HQ : STR_7072_VIEW_HQ;
		if (local && c->location_of_HQ != INVALID_TILE) this->widget[CW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; // HQ is already built.
		this->SetWidgetDisabledState(CW_WIDGET_BUILD_VIEW_HQ, !local && c->location_of_HQ == INVALID_TILE);
		this->SetWidgetHiddenState(CW_WIDGET_RELOCATE_HQ,      !local || c->location_of_HQ == INVALID_TILE);
		this->SetWidgetHiddenState(CW_WIDGET_BUY_SHARE,        local);
		this->SetWidgetHiddenState(CW_WIDGET_SELL_SHARE,       local);
		this->SetWidgetHiddenState(CW_WIDGET_COMPANY_PASSWORD, !local || !_networking);
		this->SetWidgetHiddenState(CW_WIDGET_COMPANY_JOIN,     local || !_networking);
		this->SetWidgetDisabledState(CW_WIDGET_COMPANY_JOIN,   !IsHumanCompany(c->index));

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
		DrawStringMultiCenter(48, 141, STR_7037_PRESIDENT, MAX_LENGTH_PRESIDENT_NAME_PIXELS);

		/* "Inaugurated:" */
		SetDParam(0, c->inaugurated_year);
		DrawString(110, 23, STR_7038_INAUGURATED, TC_FROMSTRING);

		/* "Colour scheme:" */
		DrawString(110, 43, STR_7006_COLOUR_SCHEME, TC_FROMSTRING);
		/* Draw company-colour bus */
		DrawSprite(SPR_VEH_BUS_SW_VIEW, COMPANY_SPRITE_COLOUR(c->index), 215, 44);

		/* "Vehicles:" */
		DrawCompanyVehiclesAmount((CompanyID)this->window_number);

		/* "Company value:" */
		SetDParam(0, CalculateCompanyValue(c));
		DrawString(110, 106, STR_7076_COMPANY_VALUE, TC_FROMSTRING);

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
				ShowQueryString(STR_PRESIDENT_NAME, STR_700B_PRESIDENT_S_NAME, MAX_LENGTH_PRESIDENT_NAME_BYTES, MAX_LENGTH_PRESIDENT_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case CW_WIDGET_COMPANY_NAME:
				this->query_widget = CW_WIDGET_COMPANY_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_COMPANY_NAME, STR_700A_COMPANY_NAME, MAX_LENGTH_COMPANY_NAME_BYTES, MAX_LENGTH_COMPANY_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case CW_WIDGET_BUILD_VIEW_HQ: {
				TileIndex tile = GetCompany((CompanyID)this->window_number)->location_of_HQ;
				if (tile == INVALID_TILE) {
					if ((byte)this->window_number != _local_company) return;
					SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, VHM_RECT, this);
					SetTileSelectSize(2, 2);
					this->LowerWidget(CW_WIDGET_BUILD_VIEW_HQ);
					this->InvalidateWidget(CW_WIDGET_BUILD_VIEW_HQ);
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
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, VHM_RECT, this);
				SetTileSelectSize(2, 2);
				this->LowerWidget(CW_WIDGET_RELOCATE_HQ);
				this->InvalidateWidget(CW_WIDGET_RELOCATE_HQ);
				break;

			case CW_WIDGET_BUY_SHARE:
				DoCommandP(0, this->window_number, 0, CMD_BUY_SHARE_IN_COMPANY | CMD_MSG(STR_707B_CAN_T_BUY_25_SHARE_IN_THIS));
				break;

			case CW_WIDGET_SELL_SHARE:
				DoCommandP(0, this->window_number, 0, CMD_SELL_SHARE_IN_COMPANY | CMD_MSG(STR_707C_CAN_T_SELL_25_SHARE_IN));
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
		if (DoCommandP(tile, 0, 0, CMD_BUILD_COMPANY_HQ | CMD_MSG(STR_7071_CAN_T_BUILD_COMPANY_HEADQUARTERS)))
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
				DoCommandP(0, 0, 0, CMD_RENAME_PRESIDENT | CMD_MSG(STR_700D_CAN_T_CHANGE_PRESIDENT), NULL, str);
				break;

			case CW_WIDGET_COMPANY_NAME:
				DoCommandP(0, 0, 0, CMD_RENAME_COMPANY | CMD_MSG(STR_700C_CAN_T_CHANGE_COMPANY_NAME), NULL, str);
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
	_company_widgets
);

void ShowCompany(CompanyID company)
{
	if (!IsValidCompanyID(company)) return;

	AllocateWindowDescFront<CompanyWindow>(&_company_desc, company);
}



struct BuyCompanyWindow : Window {
	BuyCompanyWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		Company *c = GetCompany((CompanyID)this->window_number);
		SetDParam(0, STR_COMPANY_NAME);
		SetDParam(1, c->index);
		this->DrawWidgets();

		DrawCompanyManagerFace(c->face, c->colour, 2, 16);

		SetDParam(0, c->index);
		SetDParam(1, c->bankrupt_value);
		DrawStringMultiCenter(214, 65, STR_705B_WE_ARE_LOOKING_FOR_A_TRANSPORT, 238);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 3:
				delete this;
				break;

			case 4:
				DoCommandP(0, this->window_number, 0, CMD_BUY_COMPANY | CMD_MSG(STR_7060_CAN_T_BUY_COMPANY));
				break;
		}
	}
};

static const Widget _buy_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,    11,   333,     0,    13, STR_00B3_MESSAGE_FROM, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     0,   333,    14,   136, 0x0,                   STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,   148,   207,   117,   128, STR_00C9_NO,           STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,   218,   277,   117,   128, STR_00C8_YES,          STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _buy_company_desc(
	153, 171, 334, 137, 334, 137,
	WC_BUY_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_CONSTRUCTION,
	_buy_company_widgets
);


void ShowBuyCompanyDialog(CompanyID company)
{
	AllocateWindowDescFront<BuyCompanyWindow>(&_buy_company_desc, company);
}
