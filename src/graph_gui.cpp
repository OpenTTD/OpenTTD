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

struct GraphLegendWindow : Window {
	GraphLegendWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		for (uint i = 3; i < this->widget_count; i++) {
			if (!HasBit(_legend_excluded_companies, i - 3)) this->LowerWidget(i);
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (IsValidCompanyID(c)) continue;

			SetBit(_legend_excluded_companies, c);
			this->RaiseWidget(c + 3);
		}

		this->DrawWidgets();

		const Company *c;
		FOR_ALL_COMPANIES(c) {
			DrawCompanyIcon(c->index, 4, 18 + c->index * 12);

			SetDParam(0, c->index);
			SetDParam(1, c->index);
			DrawString(21, 17 + c->index * 12, STR_7021, HasBit(_legend_excluded_companies, c->index) ? TC_BLACK : TC_WHITE);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (!IsInsideMM(widget, 3, MAX_COMPANIES + 3)) return;

		ToggleBit(_legend_excluded_companies, widget - 3);
		this->ToggleWidgetLoweredState(widget);
		this->SetDirty();
		InvalidateWindow(WC_INCOME_GRAPH, 0);
		InvalidateWindow(WC_OPERATING_PROFIT, 0);
		InvalidateWindow(WC_DELIVERED_CARGO, 0);
		InvalidateWindow(WC_PERFORMANCE_HISTORY, 0);
		InvalidateWindow(WC_COMPANY_VALUE, 0);
	}
};

static const Widget _graph_legend_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   249,     0,    13, STR_704E_KEY_TO_COMPANY_GRAPHS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   249,    14,   195, 0x0,                            STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    16,    27, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    28,    39, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    40,    51, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    52,    63, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    64,    75, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    76,    87, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,    88,    99, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   100,   111, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   112,   123, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   124,   135, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   136,   147, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   148,   159, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   160,   171, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   172,   183, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,   247,   184,   195, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{   WIDGETS_END},
};

static const WindowDesc _graph_legend_desc(
	WDP_AUTO, WDP_AUTO, 250, 198, 250, 198,
	WC_GRAPH_LEGEND, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_graph_legend_widgets
);

static void ShowGraphLegend()
{
	AllocateWindowDescFront<GraphLegendWindow>(&_graph_legend_desc, 0);
}

/******************/
/* BASE OF GRAPHS */
/*****************/

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
			DrawStringRightAligned(x, y, STR_0170, graph_axis_label_colour);

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
				SetDParam(0, month + STR_0162_JAN);
				SetDParam(1, month + STR_0162_JAN + 2);
				SetDParam(2, year);
				DrawString(x, y, month == 0 ? STR_016F : STR_016E, graph_axis_label_colour);

				month += 3;
				if (month >= 12) {
					month = 0;
					year++;
				}
				x += GRAPH_X_POSITION_SEPARATION;
			}
		} else {
			/* Draw the label under the data point rather than on the grid line. */
			x = this->gd_left + GRAPH_X_POSITION_BEGINNING + (GRAPH_X_POSITION_SEPARATION / 2) + 1;
			y = this->gd_top + this->gd_height + 1;
			uint16 label = this->x_values_start;

			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, label);
				DrawStringCentered(x, y, STR_01CB, graph_axis_label_colour);

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
			if (!IsValidCompanyID(c)) SetBit(excluded_companies, c);
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
			if (IsValidCompanyID(k)) {
				c = GetCompany(k);
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
		if (widget == 2) ShowGraphLegend();
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_7025_OPERATING_PROFIT_GRAPH, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_704C_KEY,                    STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   173, 0x0,                             STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _operating_profit_desc(
	WDP_AUTO, WDP_AUTO, 576, 174, 576, 174,
	WC_OPERATING_PROFIT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_operating_profit_widgets
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_7022_INCOME_GRAPH, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_704C_KEY,          STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   141, 0x0,                   STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _income_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 142, 576, 142,
	WC_INCOME_GRAPH, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_income_graph_widgets
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
			BaseGraphWindow(desc, window_number, 2, 18, 104, false, STR_7024)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].delivered_cargo;
	}
};

static const Widget _delivered_cargo_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_7050_UNITS_OF_CARGO_DELIVERED, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_704C_KEY,                      STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   141, 0x0,                               STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _delivered_cargo_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 142, 576, 142,
	WC_DELIVERED_CARGO, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_delivered_cargo_graph_widgets
);

void ShowDeliveredCargoGraph()
{
	AllocateWindowDescFront<DeliveredCargoGraphWindow>(&_delivered_cargo_graph_desc, 0);
}

/***********************/
/* PERFORMANCE HISTORY */
/***********************/

struct PerformanceHistoryGraphWindow : BaseGraphWindow {
	PerformanceHistoryGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, window_number, 2, 18, 200, false, STR_7024)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].performance_history;
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget == 3) ShowPerformanceRatingDetail();
		this->BaseGraphWindow::OnClick(pt, widget);
	}
};

static const Widget _performance_history_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                             STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   475,     0,    13, STR_7051_COMPANY_PERFORMANCE_RATINGS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_704C_KEY,                         STR_704D_SHOW_KEY_TO_GRAPHS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   476,   525,     0,    13, STR_PERFORMANCE_DETAIL_KEY,           STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   237, 0x0,                                  STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _performance_history_desc(
	WDP_AUTO, WDP_AUTO, 576, 238, 576, 238,
	WC_PERFORMANCE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_performance_history_widgets
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
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   525,     0,    13, STR_7052_COMPANY_VALUES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   526,   575,     0,    13, STR_704C_KEY,            STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   575,    14,   237, 0x0,                     STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _company_value_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 238, 576, 238,
	WC_COMPANY_VALUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_company_value_graph_widgets
);

void ShowCompanyValueGraph()
{
	AllocateWindowDescFront<CompanyValueGraphWindow>(&_company_value_graph_desc, 0);
}

/*****************/
/* PAYMENT RATES */
/*****************/

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
			Widget *wi = &this->widget[3 + i];
			wi->type     = WWT_PANEL;
			wi->display_flags = RESIZE_NONE;
			wi->colour   = COLOUR_ORANGE;
			wi->left     = 493;
			wi->right    = 562;
			wi->top      = 24 + i * 8;
			wi->bottom   = wi->top + 7;
			wi->data     = 0;
			wi->tooltips = STR_7064_TOGGLE_GRAPH_FOR_CARGO;

			if (!HasBit(_legend_excluded_cargo, i)) this->LowerWidget(i + 3);
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
			if (i + 3 < this->widget_count) {
				/* Since the buttons have no text, no images,
				 * both the text and the coloured box have to be manually painted.
				 * clk_dif will move one pixel down and one pixel to the right
				 * when the button is clicked */
				byte clk_dif = this->IsWidgetLowered(i + 3) ? 1 : 0;

				GfxFillRect(x + clk_dif, y + clk_dif, x + 8 + clk_dif, y + 5 + clk_dif, 0);
				GfxFillRect(x + 1 + clk_dif, y + 1 + clk_dif, x + 7 + clk_dif, y + 4 + clk_dif, cs->legend_colour);
				SetDParam(0, cs->name);
				DrawString(x + 14 + clk_dif, y + clk_dif, STR_7065, TC_FROMSTRING);
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

		DrawString(2 + 46, 24 + this->gd_height + 7, STR_7062_DAYS_IN_TRANSIT, TC_FROMSTRING);
		DrawString(2 + 84, 24 - 9, STR_7063_PAYMENT_FOR_DELIVERING, TC_FROMSTRING);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= 3) {
			ToggleBit(_legend_excluded_cargo, widget - 3);
			this->ToggleWidgetLoweredState(widget);
			this->SetDirty();
		}
	}
};

static const Widget _cargo_payment_rates_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                     STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   567,     0,    13, STR_7061_CARGO_PAYMENT_RATES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL, RESIZE_BOTTOM,  COLOUR_GREY,     0,   567,    14,    45, 0x0,                          STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _cargo_payment_rates_desc(
	WDP_AUTO, WDP_AUTO, 568, 46, 568, 46,
	WC_PAYMENT_RATES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_cargo_payment_rates_widgets
);


void ShowCargoPaymentRates()
{
	AllocateWindowDescFront<PaymentRatesGraphWindow>(&_cargo_payment_rates_desc, 0);
}

/************************/
/* COMPANY LEAGUE TABLE */
/************************/

static const StringID _performance_titles[] = {
	STR_7066_ENGINEER,
	STR_7066_ENGINEER,
	STR_7067_TRAFFIC_MANAGER,
	STR_7067_TRAFFIC_MANAGER,
	STR_7068_TRANSPORT_COORDINATOR,
	STR_7068_TRANSPORT_COORDINATOR,
	STR_7069_ROUTE_SUPERVISOR,
	STR_7069_ROUTE_SUPERVISOR,
	STR_706A_DIRECTOR,
	STR_706A_DIRECTOR,
	STR_706B_CHIEF_EXECUTIVE,
	STR_706B_CHIEF_EXECUTIVE,
	STR_706C_CHAIRMAN,
	STR_706C_CHAIRMAN,
	STR_706D_PRESIDENT,
	STR_706E_TYCOON,
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
			SetDParam(0, i + STR_01AC_1ST);
			SetDParam(1, c->index);
			SetDParam(2, c->index);
			SetDParam(3, GetPerformanceTitleFromValue(c->old_economy[1].performance_history));

			DrawString(2, 15 + i * 10, i == 0 ? STR_7054 : STR_7055, TC_FROMSTRING);
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


static const Widget _company_league_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE,  COLOUR_GREY,   0,  10,  0,  13, STR_00C5,                      STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE,  COLOUR_GREY,  11, 387,  0,  13, STR_7053_COMPANY_LEAGUE_TABLE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX, RESIZE_NONE,  COLOUR_GREY, 388, 399,  0,  13, STR_NULL,                      STR_STICKY_BUTTON},
{      WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0, 399, 14, 166, 0x0,                           STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _company_league_desc(
	WDP_AUTO, WDP_AUTO, 400, 167, 400, 167,
	WC_COMPANY_LEAGUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_company_league_widgets
);

void ShowCompanyLeagueTable()
{
	AllocateWindowDescFront<CompanyLeagueWindow>(&_company_league_desc, 0);
}

/*****************************/
/* PERFORMANCE RATING DETAIL */
/*****************************/

struct PerformanceRatingDetailWindow : Window {
private:
	enum PerformanteRatingWidgets {
		PRW_COMPANY_FIRST = 13,
		PRW_COMPANY_LAST  = PRW_COMPANY_FIRST + MAX_COMPANIES - 1,
	};

public:
	static CompanyID company;
	int timeout;

	PerformanceRatingDetailWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		/* Disable the companies who are not active */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + PRW_COMPANY_FIRST, !IsValidCompanyID(i));
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
		if (company == INVALID_COMPANY || !IsValidCompanyID(company)) {
			if (company != INVALID_COMPANY) {
				/* Raise and disable the widget for the previous selection. */
				this->RaiseWidget(company + PRW_COMPANY_FIRST);
				this->DisableWidget(company + PRW_COMPANY_FIRST);
				this->SetDirty();

				company = INVALID_COMPANY;
			}

			for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
				if (IsValidCompanyID(i)) {
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
			if (!IsValidCompanyID(i)) {
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

			DrawString(7, y, STR_PERFORMANCE_DETAIL_VEHICLES + i, TC_FROMSTRING);

			/* Draw the score */
			SetDParam(0, score);
			DrawStringRightAligned(107, y, STR_PERFORMANCE_DETAIL_INT, TC_FROMSTRING);

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
			DrawStringCentered(137, y, STR_PERFORMANCE_DETAIL_PERCENT, TC_FROMSTRING);

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
					DrawString(167, y, STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY, TC_FROMSTRING);
					break;
				default:
					DrawString(167, y, STR_PERFORMANCE_DETAIL_AMOUNT_INT, TC_FROMSTRING);
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
		if (_pause_game != 0) return;

		/* Update the company score every 5 days */
		if (--this->timeout == 0) {
			this->UpdateCompanyStats();
			this->SetDirty();
		}
	}
};

CompanyID PerformanceRatingDetailWindow::company = INVALID_COMPANY;


static const Widget _performance_rating_detail_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   298,     0,    13, STR_PERFORMANCE_DETAIL, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    14,    40, 0x0,                    STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    41,    60, 0x0,                    STR_PERFORMANCE_DETAIL_VEHICLES_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    61,    80, 0x0,                    STR_PERFORMANCE_DETAIL_STATIONS_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,    81,   100, 0x0,                    STR_PERFORMANCE_DETAIL_MIN_PROFIT_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   101,   120, 0x0,                    STR_PERFORMANCE_DETAIL_MIN_INCOME_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   121,   140, 0x0,                    STR_PERFORMANCE_DETAIL_MAX_INCOME_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   141,   160, 0x0,                    STR_PERFORMANCE_DETAIL_DELIVERED_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   161,   180, 0x0,                    STR_PERFORMANCE_DETAIL_CARGO_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   181,   200, 0x0,                    STR_PERFORMANCE_DETAIL_MONEY_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   201,   220, 0x0,                    STR_PERFORMANCE_DETAIL_LOAN_TIP},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   298,   221,   240, 0x0,                    STR_PERFORMANCE_DETAIL_TOTAL_TIP},

{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   261,   297,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     2,    38,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    39,    75,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,    76,   112,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   113,   149,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   150,   186,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   187,   223,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,   224,   260,    27,    39, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{   WIDGETS_END},
};

static const WindowDesc _performance_rating_detail_desc(
	WDP_AUTO, WDP_AUTO, 299, 241, 299, 241,
	WC_PERFORMANCE_DETAIL, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_performance_rating_detail_widgets
);

void ShowPerformanceRatingDetail()
{
	AllocateWindowDescFront<PerformanceRatingDetailWindow>(&_performance_rating_detail_desc, 0);
}
