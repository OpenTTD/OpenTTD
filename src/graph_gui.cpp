/* $Id$ */

/** @file graph_gui.cpp GUI that shows performance graphs. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "company_base.h"
#include "company_gui.h"
#include "economy_func.h"
#include "cargotype.h"
#include "strings_func.h"
#include "window_func.h"
#include "date_func.h"
#include "gfx_func.h"
#include "sortlist_type.h"

#include "table/strings.h"

/* Bitmasks of company and cargo indices that shouldn't be drawn. */
static uint _legend_excluded_companies;
static uint _legend_excluded_cargo;

/* Apparently these don't play well with enums. */
static const OverflowSafeInt64 INVALID_DATAPOINT(INT64_MAX); // Value used for a datapoint that shouldn't be drawn.
static const uint INVALID_DATAPOINT_POS = UINT_MAX;  // Used to determine if the previous point was drawn.

/****************/
/* GRAPH LEGEND */
/****************/

/** Widget numbers of the graph legend window. */
enum GraphLegendWidgetNumbers {
	GLW_CLOSEBOX,
	GLW_CAPTION,
	GLW_BACKGROUND,

	GLW_FIRST_COMPANY,
	GLW_LAST_COMPANY = GLW_FIRST_COMPANY + MAX_COMPANIES - 1,
};

struct GraphLegendWindow : Window {
	GraphLegendWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		for (uint i = GLW_FIRST_COMPANY; i < this->widget_count; i++) {
			if (!HasBit(_legend_excluded_companies, i - GLW_FIRST_COMPANY)) this->LowerWidget(i);
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (Company::IsValidID(c)) continue;

			SetBit(_legend_excluded_companies, c);
			this->RaiseWidget(c + GLW_FIRST_COMPANY);
		}

		this->DrawWidgets();

		const Company *c;
		FOR_ALL_COMPANIES(c) {
			DrawCompanyIcon(c->index, 4, 18 + c->index * 12);

			SetDParam(0, c->index);
			SetDParam(1, c->index);
			DrawString(21, this->width - 4, 17 + c->index * 12, STR_COMPANY_NAME_COMPANY_NUM, HasBit(_legend_excluded_companies, c->index) ? TC_BLACK : TC_WHITE);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (!IsInsideMM(widget, GLW_FIRST_COMPANY, MAX_COMPANIES + GLW_FIRST_COMPANY)) return;

		ToggleBit(_legend_excluded_companies, widget - GLW_FIRST_COMPANY);
		this->ToggleWidgetLoweredState(widget);
		this->SetDirty();
		InvalidateWindow(WC_INCOME_GRAPH, 0);
		InvalidateWindow(WC_OPERATING_PROFIT, 0);
		InvalidateWindow(WC_DELIVERED_CARGO, 0);
		InvalidateWindow(WC_PERFORMANCE_HISTORY, 0);
		InvalidateWindow(WC_COMPANY_VALUE, 0);
	}
};

/**
 * Construct a vertical list of buttons, one for each company.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @return Panel with company buttons.
 * @postcond \c *biggest_index contains the largest used index in the tree.
 */
static NWidgetBase *MakeNWidgetCompanyLines(int *biggest_index)
{
	NWidgetVertical *vert = new NWidgetVertical();

	for (int widnum = GLW_FIRST_COMPANY; widnum <= GLW_LAST_COMPANY; widnum++) {
		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, widnum);
		panel->SetMinimalSize(246, 12);
		panel->SetFill(false, false);
		panel->SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION);
		vert->Add(panel);
	}
	*biggest_index = GLW_LAST_COMPANY;
	return vert;
}

static const Widget _graph_legend_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,       STR_TOOLTIP_CLOSE_WINDOW},           // GLW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   249,     0,    13, STR_GRAPH_KEY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS}, // GLW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   249,    14,   195, 0x0,                   STR_NULL},                           // GLW_BACKGROUND
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    16,    27, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},    // GLW_FIRST_COMPANY
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    28,    39, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    40,    51, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    52,    63, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    64,    75, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    76,    87, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    88,    99, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   100,   111, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   112,   123, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   124,   135, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   136,   147, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   148,   159, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   160,   171, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   172,   183, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   184,   195, 0x0,                   STR_GRAPH_KEY_COMPANY_SELECTION},    // GLW_LAST_COMPANY
{   WIDGETS_END},
};

static const NWidgetPart _nested_graph_legend_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, GLW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, GLW_CAPTION), SetDataTip(STR_GRAPH_KEY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, GLW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 2),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidgetFunction(MakeNWidgetCompanyLines),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _graph_legend_desc(
	WDP_AUTO, WDP_AUTO, 250, 198, 250, 198,
	WC_GRAPH_LEGEND, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_graph_legend_widgets, _nested_graph_legend_widgets, lengthof(_nested_graph_legend_widgets)
);

static void ShowGraphLegend()
{
	AllocateWindowDescFront<GraphLegendWindow>(&_graph_legend_desc, 0);
}

/******************/
/* BASE OF GRAPHS */
/*****************/

/** Widget numbers of a base graph window. */
enum CompanyValueWidgets {
	BGW_CLOSEBOX,
	BGW_CAPTION,
	BGW_KEY_BUTTON,
	BGW_BACKGROUND,
};

struct BaseGraphWindow : Window {
protected:
	enum {
		GRAPH_MAX_DATASETS = 32,
		GRAPH_AXIS_LINE_COLOUR  = 215,

		GRAPH_X_POSITION_BEGINNING  = 44,  ///< Start the graph 44 pixels from gd_left
		GRAPH_X_POSITION_SEPARATION = 22,  ///< There are 22 pixels between each X value

		GRAPH_NUM_LINES_Y = 9, ///< How many horizontal lines to draw.
		/* 9 is convenient as that means the distance between them is the gd_height of the graph / 8,
		 * which is the same
		 * as height >> 3. */
	};

	uint excluded_data; ///< bitmask of the datasets that shouldn't be displayed.
	byte num_dataset;
	byte num_on_x_axis;
	bool has_negative_values;
	byte num_vert_lines;
	static const TextColour graph_axis_label_colour = TC_BLACK; ///< colour of the graph axis label.

	/* The starting month and year that values are plotted against. If month is
	 * 0xFF, use x_values_start and x_values_increment below instead. */
	byte month;
	Year year;

	/* These values are used if the graph is being plotted against values
	 * rather than the dates specified by month and year. */
	uint16 x_values_start;
	uint16 x_values_increment;

	int gd_left, gd_top;  ///< Where to start drawing the graph, in pixels.
	uint gd_height;    ///< The height of the graph in pixels.
	StringID format_str_y_axis;
	byte colours[GRAPH_MAX_DATASETS];
	OverflowSafeInt64 cost[GRAPH_MAX_DATASETS][24]; ///< last 2 years

	void DrawGraph() const
	{
		uint x, y;                       ///< Reused whenever x and y coordinates are needed.
		OverflowSafeInt64 highest_value; ///< Highest value to be drawn.
		int x_axis_offset;               ///< Distance from the top of the graph to the x axis.

		/* the colours and cost array of GraphDrawer must accomodate
		 * both values for cargo and companies. So if any are higher, quit */
		assert(GRAPH_MAX_DATASETS >= (int)NUM_CARGO && GRAPH_MAX_DATASETS >= (int)MAX_COMPANIES);
		assert(this->num_vert_lines > 0);

		byte grid_colour = _colour_gradient[COLOUR_GREY][4];

		/* The coordinates of the opposite edges of the graph. */
		int bottom = this->gd_top + this->gd_height - 1;
		int right  = this->gd_left + GRAPH_X_POSITION_BEGINNING + this->num_vert_lines * GRAPH_X_POSITION_SEPARATION - 1;

		/* Draw the vertical grid lines. */

		/* Don't draw the first line, as that's where the axis will be. */
		x = this->gd_left + GRAPH_X_POSITION_BEGINNING + GRAPH_X_POSITION_SEPARATION;

		for (int i = 0; i < this->num_vert_lines; i++) {
			GfxFillRect(x, this->gd_top, x, bottom, grid_colour);
			x += GRAPH_X_POSITION_SEPARATION;
		}

		/* Draw the horizontal grid lines. */
		x = this->gd_left + GRAPH_X_POSITION_BEGINNING;
		y = this->gd_height + this->gd_top;

		for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
			GfxFillRect(x, y, right, y, grid_colour);
			y -= (this->gd_height / (GRAPH_NUM_LINES_Y - 1));
		}

		/* Draw the y axis. */
		GfxFillRect(x, this->gd_top, x, bottom, GRAPH_AXIS_LINE_COLOUR);

		/* Find the distance from the gd_top of the graph to the x axis. */
		x_axis_offset = this->gd_height;

		/* The graph is currently symmetrical about the x axis. */
		if (this->has_negative_values) x_axis_offset /= 2;

		/* Draw the x axis. */
		y = x_axis_offset + this->gd_top;
		GfxFillRect(x, y, right, y, GRAPH_AXIS_LINE_COLOUR);

		/* Find the largest value that will be drawn. */
		if (this->num_on_x_axis == 0)
			return;

		assert(this->num_on_x_axis > 0);
		assert(this->num_dataset > 0);

		/* Start of with a value of twice the gd_height of the graph in pixels. It's a
		 * bit arbitrary, but it makes the cargo payment graph look a little nicer,
		 * and prevents division by zero when calculating where the datapoint
		 * should be drawn. */
		highest_value = x_axis_offset * 2;

		for (int i = 0; i < this->num_dataset; i++) {
			if (!HasBit(this->excluded_data, i)) {
				for (int j = 0; j < this->num_on_x_axis; j++) {
					OverflowSafeInt64 datapoint = this->cost[i][j];

					if (datapoint != INVALID_DATAPOINT) {
						/* For now, if the graph has negative values the scaling is
						 * symmetrical about the x axis, so take the absolute value
						 * of each data point. */
						highest_value = max(highest_value, abs(datapoint));
					}
				}
			}
		}

		/* Round up highest_value so that it will divide cleanly into the number of
		 * axis labels used. */
		int round_val = highest_value % (GRAPH_NUM_LINES_Y - 1);
		if (round_val != 0) highest_value += (GRAPH_NUM_LINES_Y - 1 - round_val);

		/* draw text strings on the y axis */
		int64 y_label = highest_value;
		int64 y_label_separation = highest_value / (GRAPH_NUM_LINES_Y - 1);

		/* If there are negative values, the graph goes from highest_value to
		 * -highest_value, not highest_value to 0. */
		if (this->has_negative_values) y_label_separation *= 2;

		x = this->gd_left + GRAPH_X_POSITION_BEGINNING + 1;
		y = this->gd_top - 3;

		for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
			SetDParam(0, this->format_str_y_axis);
			SetDParam(1, y_label);
			DrawString(x - GRAPH_X_POSITION_BEGINNING, x, y, STR_GRAPH_Y_LABEL, graph_axis_label_colour, SA_RIGHT);

			y_label -= y_label_separation;
			y += (this->gd_height / (GRAPH_NUM_LINES_Y - 1));
		}

		/* draw strings on the x axis */
		if (this->month != 0xFF) {
			x = this->gd_left + GRAPH_X_POSITION_BEGINNING;
			y = this->gd_top + this->gd_height + 1;
			byte month = this->month;
			Year year  = this->year;
			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, month + STR_MONTH_ABBREV_JAN);
				SetDParam(1, month + STR_MONTH_ABBREV_JAN + 2);
				SetDParam(2, year);
				DrawStringMultiLine(x, x + GRAPH_X_POSITION_SEPARATION, y, this->height, month == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH, graph_axis_label_colour);

				month += 3;
				if (month >= 12) {
					month = 0;
					year++;
				}
				x += GRAPH_X_POSITION_SEPARATION;
			}
		} else {
			/* Draw the label under the data point rather than on the grid line. */
			x = this->gd_left + GRAPH_X_POSITION_BEGINNING;
			y = this->gd_top + this->gd_height + 1;
			uint16 label = this->x_values_start;

			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, label);
				DrawString(x + 1, x + GRAPH_X_POSITION_SEPARATION - 1, y, STR_GRAPH_Y_LABEL_NUMBER, graph_axis_label_colour, SA_CENTER);

				label += this->x_values_increment;
				x += GRAPH_X_POSITION_SEPARATION;
			}
		}

		/* draw lines and dots */
		for (int i = 0; i < this->num_dataset; i++) {
			if (!HasBit(this->excluded_data, i)) {
				/* Centre the dot between the grid lines. */
				x = this->gd_left + GRAPH_X_POSITION_BEGINNING + (GRAPH_X_POSITION_SEPARATION / 2);

				byte colour  = this->colours[i];
				uint prev_x = INVALID_DATAPOINT_POS;
				uint prev_y = INVALID_DATAPOINT_POS;

				for (int j = 0; j < this->num_on_x_axis; j++) {
					OverflowSafeInt64 datapoint = this->cost[i][j];

					if (datapoint != INVALID_DATAPOINT) {
						/*
						 * Check whether we need to reduce the 'accuracy' of the
						 * datapoint value and the highest value to splut overflows.
						 * And when 'drawing' 'one million' or 'one million and one'
						 * there is no significant difference, so the least
						 * significant bits can just be removed.
						 *
						 * If there are more bits needed than would fit in a 32 bits
						 * integer, so at about 31 bits because of the sign bit, the
						 * least significant bits are removed.
						 */
						int mult_range = FindLastBit(x_axis_offset) + FindLastBit(abs(datapoint));
						int reduce_range = max(mult_range - 31, 0);

						/* Handle negative values differently (don't shift sign) */
						if (datapoint < 0) {
							datapoint = -(abs(datapoint) >> reduce_range);
						} else {
							datapoint >>= reduce_range;
						}

						y = this->gd_top + x_axis_offset - (x_axis_offset * datapoint) / (highest_value >> reduce_range);

						/* Draw the point. */
						GfxFillRect(x - 1, y - 1, x + 1, y + 1, colour);

						/* Draw the line connected to the previous point. */
						if (prev_x != INVALID_DATAPOINT_POS) GfxDrawLine(prev_x, prev_y, x, y, colour);

						prev_x = x;
						prev_y = y;
					} else {
						prev_x = INVALID_DATAPOINT_POS;
						prev_y = INVALID_DATAPOINT_POS;
					}

					x += GRAPH_X_POSITION_SEPARATION;
				}
			}
		}
	}


	BaseGraphWindow(const WindowDesc *desc, WindowNumber window_number, int left,
									int top, int height, bool has_negative_values, StringID format_str_y_axis) :
			Window(desc, window_number), has_negative_values(has_negative_values),
			gd_left(left), gd_top(top), gd_height(height), format_str_y_axis(format_str_y_axis)
	{
		InvalidateWindow(WC_GRAPH_LEGEND, 0);
	}

public:
	virtual void OnPaint()
	{
		this->DrawWidgets();

		uint excluded_companies = _legend_excluded_companies;

		/* Exclude the companies which aren't valid */
		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (!Company::IsValidID(c)) SetBit(excluded_companies, c);
		}
		this->excluded_data = excluded_companies;
		this->num_vert_lines = 24;

		byte nums = 0;
		const Company *c;
		FOR_ALL_COMPANIES(c) {
			nums = max(nums, c->num_valid_stat_ent);
		}
		this->num_on_x_axis = min(nums, 24);

		int mo = (_cur_month / 3 - nums) * 3;
		int yr = _cur_year;
		while (mo < 0) {
			yr--;
			mo += 12;
		}

		this->year = yr;
		this->month = mo;

		int numd = 0;
		for (CompanyID k = COMPANY_FIRST; k < MAX_COMPANIES; k++) {
			c = Company::GetIfValid(k);
			if (c != NULL) {
				this->colours[numd] = _colour_gradient[c->colour][6];
				for (int j = this->num_on_x_axis, i = 0; --j >= 0;) {
					this->cost[numd][i] = (j >= c->num_valid_stat_ent) ? INVALID_DATAPOINT : GetGraphData(c, j);
					i++;
				}
			}
			numd++;
		}

		this->num_dataset = numd;

		this->DrawGraph();
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return INVALID_DATAPOINT;
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Clicked on legend? */
		if (widget == BGW_KEY_BUTTON) ShowGraphLegend();
	}
};


/********************/
/* OPERATING PROFIT */
/********************/

struct OperatingProfitGraphWindow : BaseGraphWindow {
	OperatingProfitGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 18, 136, true, STR_CURRCOMPACT)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].income + c->old_economy[j].expenses;
	}
};

static const Widget _operating_profit_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,                    STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_GRAPH_OPERATING_PROFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_GRAPH_KEY_BUTTON,               STR_GRAPH_KEY_TOOLTIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   173, 0x0,                                STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_operating_profit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_OPERATING_PROFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 14), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 160), EndContainer(),
};

static const WindowDesc _operating_profit_desc(
	WDP_AUTO, WDP_AUTO, 576, 174, 576, 174,
	WC_OPERATING_PROFIT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_operating_profit_widgets, _nested_operating_profit_widgets, lengthof(_nested_operating_profit_widgets)
);


void ShowOperatingProfitGraph()
{
	AllocateWindowDescFront<OperatingProfitGraphWindow>(&_operating_profit_desc, 0);
}


/****************/
/* INCOME GRAPH */
/****************/

struct IncomeGraphWindow : BaseGraphWindow {
	IncomeGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 18, 104, false, STR_CURRCOMPACT)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].income;
	}
};

static const Widget _income_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,          STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_GRAPH_INCOME_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_GRAPH_KEY_BUTTON,     STR_GRAPH_KEY_TOOLTIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   141, 0x0,                      STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_income_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_INCOME_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 14), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 128), EndContainer(),
};


static const WindowDesc _income_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 142, 576, 142,
	WC_INCOME_GRAPH, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_income_graph_widgets, _nested_income_graph_widgets, lengthof(_nested_income_graph_widgets)
);

void ShowIncomeGraph()
{
	AllocateWindowDescFront<IncomeGraphWindow>(&_income_graph_desc, 0);
}

/*******************/
/* DELIVERED CARGO */
/*******************/

struct DeliveredCargoGraphWindow : BaseGraphWindow {
	DeliveredCargoGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 18, 104, false, STR_JUST_COMMA)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].delivered_cargo;
	}
};

static const Widget _delivered_cargo_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,                   STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_GRAPH_CARGO_DELIVERED_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_GRAPH_KEY_BUTTON,              STR_GRAPH_KEY_TOOLTIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   141, 0x0,                               STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_delivered_cargo_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_CARGO_DELIVERED_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 14), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 128), EndContainer(),
};

static const WindowDesc _delivered_cargo_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 142, 576, 142,
	WC_DELIVERED_CARGO, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_delivered_cargo_graph_widgets, _nested_delivered_cargo_graph_widgets, lengthof(_nested_delivered_cargo_graph_widgets)
);

void ShowDeliveredCargoGraph()
{
	AllocateWindowDescFront<DeliveredCargoGraphWindow>(&_delivered_cargo_graph_desc, 0);
}

/***********************/
/* PERFORMANCE HISTORY */
/***********************/

/** Widget numbers of the performance history window. */
enum PerformanceHistoryGraphWidgets {
	PHW_CLOSEBOX,
	PHW_CAPTION,
	PHW_KEY,
	PHW_DETAILED_PERFORMANCE,
	PHW_BACKGROUND,
};

struct PerformanceHistoryGraphWindow : BaseGraphWindow {
	PerformanceHistoryGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 18, 200, false, STR_JUST_COMMA)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].performance_history;
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == PHW_DETAILED_PERFORMANCE) ShowPerformanceRatingDetail();
		this->BaseGraphWindow::OnClick(pt, widget);
	}
};

static const Widget _performance_history_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,                               STR_TOOLTIP_CLOSE_WINDOW},              // PHW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   475,     0,    13, STR_GRAPH_COMPANY_PERFORMANCE_RATINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},    // PHW_CAPTION
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_GRAPH_KEY_BUTTON,                          STR_GRAPH_KEY_TOOLTIP},                 // PHW_KEY
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   476,   525,     0,    13, STR_PERFORMANCE_DETAIL_KEY,                    STR_SHOW_DETAILED_PERFORMANCE_RATINGS}, // PHW_DETAILED_PERFORMANCE
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   237, 0x0,                                           STR_NULL},                              // PHW_BACKGROUND
{   WIDGETS_END},
};

static const NWidgetPart _nested_performance_history_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, PHW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, PHW_CAPTION), SetDataTip(STR_GRAPH_COMPANY_PERFORMANCE_RATINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, PHW_DETAILED_PERFORMANCE), SetMinimalSize(50, 14), SetDataTip(STR_PERFORMANCE_DETAIL_KEY, STR_SHOW_DETAILED_PERFORMANCE_RATINGS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, PHW_KEY), SetMinimalSize(50, 14), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, PHW_BACKGROUND), SetMinimalSize(576, 224), EndContainer(),
};

static const WindowDesc _performance_history_desc(
	WDP_AUTO, WDP_AUTO, 576, 238, 576, 238,
	WC_PERFORMANCE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_performance_history_widgets, _nested_performance_history_widgets, lengthof(_nested_performance_history_widgets)
);

void ShowPerformanceHistoryGraph()
{
	AllocateWindowDescFront<PerformanceHistoryGraphWindow>(&_performance_history_desc, 0);
}

/*****************/
/* COMPANY VALUE */
/*****************/

struct CompanyValueGraphWindow : BaseGraphWindow {
	CompanyValueGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 18, 200, false, STR_CURRCOMPACT)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].company_value;
	}
};

static const Widget _company_value_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,                  STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_GRAPH_COMPANY_VALUES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_GRAPH_KEY_BUTTON,             STR_GRAPH_KEY_TOOLTIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   237, 0x0,                              STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_company_value_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_COMPANY_VALUES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 14), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 224), EndContainer(),
};

static const WindowDesc _company_value_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 238, 576, 238,
	WC_COMPANY_VALUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_company_value_graph_widgets, _nested_company_value_graph_widgets, lengthof(_nested_company_value_graph_widgets)
);

void ShowCompanyValueGraph()
{
	AllocateWindowDescFront<CompanyValueGraphWindow>(&_company_value_graph_desc, 0);
}

/*****************/
/* PAYMENT RATES */
/*****************/

/** Widget numbers of the cargo payment rates. */
enum CargoPaymentRatesWidgets {
	CPW_CLOSEBOX,
	CPW_CAPTION,
	CPW_BACKGROUND,
	CPW_CARGO_FIRST,
};

struct PaymentRatesGraphWindow : BaseGraphWindow {
	PaymentRatesGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 24, 200, false, STR_CURRCOMPACT)
	{
		uint num_active = 0;
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			if (GetCargo(c)->IsValid()) num_active++;
		}

		/* Resize the window to fit the cargo types */
		ResizeWindow(this, 0, max(num_active, 12U) * 8);

		/* Add widgets for each cargo type */
		this->widget_count += num_active;
		this->widget = ReallocT(this->widget, this->widget_count + 1);
		this->widget[this->widget_count].type = WWT_LAST;

		/* Set the properties of each widget */
		for (uint i = 0; i != num_active; i++) {
			Widget *wi = &this->widget[CPW_CARGO_FIRST + i];
			wi->type     = WWT_PANEL;
			wi->display_flags = RESIZE_NONE;
			wi->colour   = COLOUR_ORANGE;
			wi->left     = 493;
			wi->right    = 562;
			wi->top      = 24 + i * 8;
			wi->bottom   = wi->top + 7;
			wi->data     = 0;
			wi->tooltips = STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO;

			if (!HasBit(_legend_excluded_cargo, i)) this->LowerWidget(i + CPW_CARGO_FIRST);
		}

		this->SetDirty();

		this->gd_height = this->height - 38;
		this->num_on_x_axis = 20;
		this->num_vert_lines = 20;
		this->month = 0xFF;
		this->x_values_start     = 10;
		this->x_values_increment = 10;

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();

		this->excluded_data = _legend_excluded_cargo;

		int x = 495;
		int y = 24;

		uint i = 0;
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			const CargoSpec *cs = GetCargo(c);
			if (!cs->IsValid()) continue;

			/* Only draw labels for widgets that exist. If the widget doesn't
			 * exist then the local company has used the climate cheat or
			 * changed the NewGRF configuration with this window open. */
			if (i + CPW_CARGO_FIRST < this->widget_count) {
				/* Since the buttons have no text, no images,
				 * both the text and the coloured box have to be manually painted.
				 * clk_dif will move one pixel down and one pixel to the right
				 * when the button is clicked */
				byte clk_dif = this->IsWidgetLowered(i + CPW_CARGO_FIRST) ? 1 : 0;

				GfxFillRect(x + clk_dif, y + clk_dif, x + 8 + clk_dif, y + 5 + clk_dif, 0);
				GfxFillRect(x + 1 + clk_dif, y + 1 + clk_dif, x + 7 + clk_dif, y + 4 + clk_dif, cs->legend_colour);
				SetDParam(0, cs->name);
				DrawString(x + 14 + clk_dif, this->width, y + clk_dif, STR_GRAPH_CARGO_PAYMENT_CARGO);
				y += 8;
			}

			this->colours[i] = cs->legend_colour;
			for (uint j = 0; j != 20; j++) {
				this->cost[i][j] = GetTransportedGoodsIncome(10, 20, j * 4 + 4, c);
			}

			i++;
		}
		this->num_dataset = i;

		this->DrawGraph();

		DrawString(2 + 46, this->width, 24 + this->gd_height + 7, STR_GRAPH_CARGO_PAYMENT_RATES_X_LABEL);
		DrawString(2 + 84, this->width, 24 - 9, STR_GRAPH_CARGO_PAYMENT_RATES_TITLE);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= CPW_CARGO_FIRST) {
			ToggleBit(_legend_excluded_cargo, widget - CPW_CARGO_FIRST);
			this->ToggleWidgetLoweredState(widget);
			this->SetDirty();
		}
	}
};

static const Widget _cargo_payment_rates_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,                       STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   567,     0,    13, STR_GRAPH_CARGO_PAYMENT_RATES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL, RESIZE_BOTTOM,  COLOUR_GREY,     0,   567,    14,    45, 0x0,                                   STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_cargo_payment_rates_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CPW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, CPW_CAPTION), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CPW_BACKGROUND), SetMinimalSize(568, 32), SetResize(0, 1), EndContainer(),
};

static const WindowDesc _cargo_payment_rates_desc(
	WDP_AUTO, WDP_AUTO, 568, 46, 568, 46,
	WC_PAYMENT_RATES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_cargo_payment_rates_widgets, _nested_cargo_payment_rates_widgets, lengthof(_nested_cargo_payment_rates_widgets)
);


void ShowCargoPaymentRates()
{
	AllocateWindowDescFront<PaymentRatesGraphWindow>(&_cargo_payment_rates_desc, 0);
}

/************************/
/* COMPANY LEAGUE TABLE */
/************************/

static const StringID _performance_titles[] = {
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ENGINEER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ENGINEER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRAFFIC_MANAGER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRAFFIC_MANAGER,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRANSPORT_COORDINATOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TRANSPORT_COORDINATOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ROUTE_SUPERVISOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_ROUTE_SUPERVISOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_DIRECTOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_DIRECTOR,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHIEF_EXECUTIVE,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHIEF_EXECUTIVE,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHAIRMAN,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_CHAIRMAN,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_PRESIDENT,
	STR_COMPANY_LEAGUE_PERFORMANCE_TITLE_TYCOON,
};

static inline StringID GetPerformanceTitleFromValue(uint value)
{
	return _performance_titles[minu(value, 1000) >> 6];
}

class CompanyLeagueWindow : public Window {
private:
	GUIList<const Company*> companies;

	/**
	 * (Re)Build the company league list
	 */
	void BuildCompanyList()
	{
		if (!this->companies.NeedRebuild()) return;

		this->companies.Clear();

		const Company *c;
		FOR_ALL_COMPANIES(c) {
			*this->companies.Append() = c;
		}

		this->companies.Compact();
		this->companies.RebuildDone();
	}

	/** Sort the company league by performance history */
	static int CDECL PerformanceSorter(const Company * const *c1, const Company * const *c2)
	{
		return (*c2)->old_economy[1].performance_history - (*c1)->old_economy[1].performance_history;
	}

public:
	CompanyLeagueWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->companies.ForceRebuild();
		this->companies.NeedResort();

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		this->BuildCompanyList();
		this->companies.Sort(&PerformanceSorter);

		this->DrawWidgets();

		for (uint i = 0; i != this->companies.Length(); i++) {
			const Company *c = this->companies[i];
			SetDParam(0, i + STR_ORDINAL_NUMBER_1ST);
			SetDParam(1, c->index);
			SetDParam(2, c->index);
			SetDParam(3, GetPerformanceTitleFromValue(c->old_economy[1].performance_history));

			DrawString(2, this->width, 15 + i * 10, i == 0 ? STR_COMPANY_LEAGUE_FIRST : STR_COMPANY_LEAGUE_OTHER);
			DrawCompanyIcon(c->index, 27, 16 + i * 10);
		}
	}

	virtual void OnTick()
	{
		if (this->companies.NeedResort()) {
			this->SetDirty();
		}
	}

	virtual void OnInvalidateData(int data)
	{
		if (data == 0) {
			this->companies.ForceRebuild();
		} else {
			this->companies.ForceResort();
		}
	}
};

/** Widget numbers for the company league window. */
enum CompanyLeagueWidgets {
	CLW_CLOSEBOX,
	CLW_CAPTION,
	CLW_STICKYBOX,
	CLW_BACKGROUND,
};

static const Widget _company_league_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,  COLOUR_GREY,   0,  10,  0,  13, STR_BLACK_CROSS,                  STR_TOOLTIP_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE,  COLOUR_GREY,  11, 387,  0,  13, STR_COMPANY_LEAGUE_TABLE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX, RESIZE_NONE,  COLOUR_GREY, 388, 399,  0,  13, STR_NULL,                         STR_STICKY_BUTTON},
{      WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0, 399, 14, 166, 0x0,                              STR_NULL},
{   WIDGETS_END},
};

static const NWidgetPart _nested_company_league_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CLW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, CLW_CAPTION), SetDataTip(STR_COMPANY_LEAGUE_TABLE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, CLW_STICKYBOX),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CLW_BACKGROUND), SetMinimalSize(400, 153),
};

static const WindowDesc _company_league_desc(
	WDP_AUTO, WDP_AUTO, 400, 167, 400, 167,
	WC_COMPANY_LEAGUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_company_league_widgets, _nested_company_league_widgets, lengthof(_nested_company_league_widgets)
);

void ShowCompanyLeagueTable()
{
	AllocateWindowDescFront<CompanyLeagueWindow>(&_company_league_desc, 0);
}

/*****************************/
/* PERFORMANCE RATING DETAIL */
/*****************************/

/** Widget numbers of the performance rating details window. */
enum PerformanceRatingDetailsWidgets {
	PRW_CLOSEBOX,
	PRW_CAPTION,
	PRW_BACKGROUND,

	PRW_SCORE_FIRST,
	PRW_SCORE_LAST = PRW_SCORE_FIRST + (SCORE_END - SCORE_BEGIN) - 1,

	PRW_COMPANY_FIRST,
	PRW_COMPANY_LAST  = PRW_COMPANY_FIRST + MAX_COMPANIES - 1,
};

struct PerformanceRatingDetailWindow : Window {
	static CompanyID company;
	int timeout;

	PerformanceRatingDetailWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		/* Disable the companies who are not active */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + PRW_COMPANY_FIRST, !Company::IsValidID(i));
		}

		this->UpdateCompanyStats();

		if (company != INVALID_COMPANY) this->LowerWidget(company + PRW_COMPANY_FIRST);

		this->FindWindowPlacementAndResize(desc);
	}

	void UpdateCompanyStats()
	{
		/* Update all company stats with the current data
		 * (this is because _score_info is not saved to a savegame) */
		Company *c;
		FOR_ALL_COMPANIES(c) {
			UpdateCompanyRatingAndValue(c, false);
		}

		this->timeout = DAY_TICKS * 5;

	}

	virtual void OnPaint()
	{
		byte x;
		uint16 y = 27;
		int total_score = 0;
		int colour_done, colour_notdone;

		/* Draw standard stuff */
		this->DrawWidgets();

		/* Check if the currently selected company is still active. */
		if (company == INVALID_COMPANY || !Company::IsValidID(company)) {
			if (company != INVALID_COMPANY) {
				/* Raise and disable the widget for the previous selection. */
				this->RaiseWidget(company + PRW_COMPANY_FIRST);
				this->DisableWidget(company + PRW_COMPANY_FIRST);
				this->SetDirty();

				company = INVALID_COMPANY;
			}

			for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
				if (Company::IsValidID(i)) {
					/* Lower the widget corresponding to this company. */
					this->LowerWidget(i + PRW_COMPANY_FIRST);
					this->SetDirty();

					company = i;
					break;
				}
			}
		}

		/* If there are no active companies, don't display anything else. */
		if (company == INVALID_COMPANY) return;

		/* Paint the company icons */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			if (!Company::IsValidID(i)) {
				/* Check if we have the company as an active company */
				if (!this->IsWidgetDisabled(i + PRW_COMPANY_FIRST)) {
					/* Bah, company gone :( */
					this->DisableWidget(i + PRW_COMPANY_FIRST);

					/* We need a repaint */
					this->SetDirty();
				}
				continue;
			}

			/* Check if we have the company marked as inactive */
			if (this->IsWidgetDisabled(i + PRW_COMPANY_FIRST)) {
				/* New company! Yippie :p */
				this->EnableWidget(i + PRW_COMPANY_FIRST);
				/* We need a repaint */
				this->SetDirty();
			}

			x = (i == company) ? 1 : 0;
			DrawCompanyIcon(i, (i % 8) * 37 + 13 + x, (i < 8 ? 0 : 13) + 16 + x);
		}

		/* The colours used to show how the progress is going */
		colour_done = _colour_gradient[COLOUR_GREEN][4];
		colour_notdone = _colour_gradient[COLOUR_RED][4];

		/* Draw all the score parts */
		for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) {
			int val    = _score_part[company][i];
			int needed = _score_info[i].needed;
			int score  = _score_info[i].score;

			y += 20;
			/* SCORE_TOTAL has his own rulez ;) */
			if (i == SCORE_TOTAL) {
				needed = total_score;
				score = SCORE_MAX;
			} else {
				total_score += score;
			}

			DrawString(7, 107, y, STR_PERFORMANCE_DETAIL_VEHICLES + i);

			/* Draw the score */
			SetDParam(0, score);
			DrawString(7, 107, y, STR_PERFORMANCE_DETAIL_INT, TC_FROMSTRING, SA_RIGHT);

			/* Calculate the %-bar */
			x = Clamp(val, 0, needed) * 50 / needed;

			/* SCORE_LOAN is inversed */
			if (val < 0 && i == SCORE_LOAN) x = 0;

			/* Draw the bar */
			if (x !=  0) GfxFillRect(112,     y - 2, 112 + x,  y + 10, colour_done);
			if (x != 50) GfxFillRect(112 + x, y - 2, 112 + 50, y + 10, colour_notdone);

			/* Calculate the % */
			x = Clamp(val, 0, needed) * 100 / needed;

			/* SCORE_LOAN is inversed */
			if (val < 0 && i == SCORE_LOAN) x = 0;

			/* Draw it */
			SetDParam(0, x);
			DrawString(112, 162, y, STR_PERFORMANCE_DETAIL_PERCENT, TC_FROMSTRING, SA_CENTER);

			/* SCORE_LOAN is inversed */
			if (i == SCORE_LOAN) val = needed - val;

			/* Draw the amount we have against what is needed
			 * For some of them it is in currency format */
			SetDParam(0, val);
			SetDParam(1, needed);
			switch (i) {
				case SCORE_MIN_PROFIT:
				case SCORE_MIN_INCOME:
				case SCORE_MAX_INCOME:
				case SCORE_MONEY:
				case SCORE_LOAN:
					DrawString(167, this->width, y, STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY);
					break;
				default:
					DrawString(167, this->width, y, STR_PERFORMANCE_DETAIL_AMOUNT_INT);
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, PRW_COMPANY_FIRST, PRW_COMPANY_LAST + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				this->RaiseWidget(company + PRW_COMPANY_FIRST);
				company = (CompanyID)(widget - PRW_COMPANY_FIRST);
				this->LowerWidget(company + PRW_COMPANY_FIRST);
				this->SetDirty();
			}
		}
	}

	virtual void OnTick()
	{
		if (_pause_mode != PM_UNPAUSED) return;

		/* Update the company score every 5 days */
		if (--this->timeout == 0) {
			this->UpdateCompanyStats();
			this->SetDirty();
		}
	}
};

CompanyID PerformanceRatingDetailWindow::company = INVALID_COMPANY;

/** Make a vertical list of panels for outputting score details.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @return Panel with performance details.
 * @postcond \c *biggest_index contains the largest used index in the tree.
 */
static NWidgetBase *MakePerformanceDetailPanels(int *biggest_index)
{
	const StringID performance_tips[] = {
		STR_PERFORMANCE_DETAIL_VEHICLES_TIP,
		STR_PERFORMANCE_DETAIL_STATIONS_TIP,
		STR_PERFORMANCE_DETAIL_MIN_PROFIT_TIP,
		STR_PERFORMANCE_DETAIL_MIN_INCOME_TIP,
		STR_PERFORMANCE_DETAIL_MAX_INCOME_TIP,
		STR_PERFORMANCE_DETAIL_DELIVERED_TIP,
		STR_PERFORMANCE_DETAIL_CARGO_TIP,
		STR_PERFORMANCE_DETAIL_MONEY_TIP,
		STR_PERFORMANCE_DETAIL_LOAN_TIP,
		STR_PERFORMANCE_DETAIL_TOTAL_TIP,
	};

	assert(lengthof(performance_tips) == SCORE_END - SCORE_BEGIN);

	NWidgetVertical *vert = new NWidgetVertical();
	for (int widnum = PRW_SCORE_FIRST; widnum <= PRW_SCORE_LAST; widnum++) {
		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, widnum);
		panel->SetMinimalSize(299, 20);
		panel->SetFill(false, false);
		panel->SetDataTip(0x0, performance_tips[widnum - PRW_SCORE_FIRST]);
		vert->Add(panel);
	}
	*biggest_index = PRW_SCORE_LAST;
	return vert;
}

/**
 * Make a number of rows with button-like graphics, for enabling/disabling each company.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @return Panel with rows of company buttons.
 * @postcond \c *biggest_index contains the largest used index in the tree.
 */
static NWidgetBase *MakeCompanyButtonRows(int *biggest_index)
{
	static const int MAX_LENGTH = 8; // Maximal number of company buttons in one row.
	NWidgetVertical *vert = NULL; // Storage for all rows.
	NWidgetHorizontal *hor = NULL; // Storage for buttons in one row.
	int hor_length = 0;

	for (int widnum = PRW_COMPANY_FIRST; widnum <= PRW_COMPANY_LAST; widnum++) {
		/* Ensure there is room in 'hor' for another button. */
		if (hor_length == MAX_LENGTH) {
			if (vert == NULL) vert = new NWidgetVertical();
			vert->Add(hor);
			hor = NULL;
			hor_length = 0;
		}
		if (hor == NULL) {
			hor = new NWidgetHorizontal();
			hor_length = 0;
		}

		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, widnum);
		panel->SetMinimalSize(37, 13);
		panel->SetFill(false, false);
		panel->SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION);
		hor->Add(panel);
		hor_length++;
	}
	*biggest_index = PRW_COMPANY_LAST;
	if (vert == NULL) return hor; // All buttons fit in a single row.

	if (hor_length > 0 && hor_length < MAX_LENGTH) {
		/* Last row is partial, add a spacer at the end to force all buttons to the left. */
		NWidgetSpacer *spc = new NWidgetSpacer(0, 0);
		spc->SetFill(true, false);
		hor->Add(spc);
	}
	if (hor != NULL) vert->Add(hor);
	return vert;
}

static const Widget _performance_rating_detail_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_BLACK_CROSS,        STR_TOOLTIP_CLOSE_WINDOW},            // PRW_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   298,     0,    13, STR_PERFORMANCE_DETAIL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS},  // PRW_CAPTION
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    14,    40, 0x0,                    STR_NULL},                            // PRW_BACKGROUND

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    41,    60, 0x0,                    STR_PERFORMANCE_DETAIL_VEHICLES_TIP}, // PRW_SCORE_FIRST
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    61,    80, 0x0,                    STR_PERFORMANCE_DETAIL_STATIONS_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    81,   100, 0x0,                    STR_PERFORMANCE_DETAIL_MIN_PROFIT_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   101,   120, 0x0,                    STR_PERFORMANCE_DETAIL_MIN_INCOME_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   121,   140, 0x0,                    STR_PERFORMANCE_DETAIL_MAX_INCOME_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   141,   160, 0x0,                    STR_PERFORMANCE_DETAIL_DELIVERED_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   161,   180, 0x0,                    STR_PERFORMANCE_DETAIL_CARGO_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   181,   200, 0x0,                    STR_PERFORMANCE_DETAIL_MONEY_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   201,   220, 0x0,                    STR_PERFORMANCE_DETAIL_LOAN_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   221,   240, 0x0,                    STR_PERFORMANCE_DETAIL_TOTAL_TIP},    // PRW_SCORE_LAST

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},     // PRW_COMPANY_FIRST
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   261,   297,    14,    26, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    27,    39, 0x0,                    STR_GRAPH_KEY_COMPANY_SELECTION},     // PRW_COMPANY_LAST
{   WIDGETS_END},
};

static const NWidgetPart _nested_performance_rating_detail_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, PRW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, PRW_CAPTION), SetDataTip(STR_PERFORMANCE_DETAIL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, PRW_BACKGROUND),
		NWidgetFunction(MakeCompanyButtonRows), SetPadding(0, 1, 1, 2),
	EndContainer(),
	NWidgetFunction(MakePerformanceDetailPanels),
};

static const WindowDesc _performance_rating_detail_desc(
	WDP_AUTO, WDP_AUTO, 299, 241, 299, 241,
	WC_PERFORMANCE_DETAIL, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_performance_rating_detail_widgets, _nested_performance_rating_detail_widgets, lengthof(_nested_performance_rating_detail_widgets)
);

void ShowPerformanceRatingDetail()
{
	AllocateWindowDescFront<PerformanceRatingDetailWindow>(&_performance_rating_detail_desc, 0);
}
