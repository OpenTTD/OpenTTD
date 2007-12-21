/* $Id$ */

/** @file graph_gui.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "functions.h"
#include "gui.h"
#include "window_gui.h"
#include "gfx.h"
#include "player.h"
#include "economy_func.h"
#include "variables.h"
#include "date.h"
#include "helpers.hpp"
#include "cargotype.h"
#include "strings_func.h"

/* Bitmasks of player and cargo indices that shouldn't be drawn. */
static uint _legend_excluded_players;
static uint _legend_excluded_cargo;

/************************/
/* GENERIC GRAPH DRAWER */
/************************/

enum {
	GRAPH_MAX_DATASETS = 32,
	GRAPH_AXIS_LABEL_COLOUR = TC_BLACK,
	GRAPH_AXIS_LINE_COLOUR  = 215,

	GRAPH_X_POSITION_BEGINNING  = 44,  ///< Start the graph 44 pixels from gw->left
	GRAPH_X_POSITION_SEPARATION = 22,  ///< There are 22 pixels between each X value

	GRAPH_NUM_LINES_Y = 9, ///< How many horizontal lines to draw.
	/* 9 is convenient as that means the distance between them is the height of the graph / 8,
	 * which is the same
	 * as height >> 3. */
};

/* Apparently these don't play well with enums. */
static const OverflowSafeInt64 INVALID_DATAPOINT = INT64_MAX; // Value used for a datapoint that shouldn't be drawn.
static const uint INVALID_DATAPOINT_POS = UINT_MAX;  // Used to determine if the previous point was drawn.

struct GraphDrawer {
	uint excluded_data; ///< bitmask of the datasets that shouldn't be displayed.
	byte num_dataset;
	byte num_on_x_axis;
	bool has_negative_values;
	byte num_vert_lines;

	/* The starting month and year that values are plotted against. If month is
	 * 0xFF, use x_values_start and x_values_increment below instead. */
	byte month;
	Year year;

	/* These values are used if the graph is being plotted against values
	 * rather than the dates specified by month and year. */
	uint16 x_values_start;
	uint16 x_values_increment;

	int left, top;  ///< Where to start drawing the graph, in pixels.
	uint height;    ///< The height of the graph in pixels.
	StringID format_str_y_axis;
	byte colors[GRAPH_MAX_DATASETS];
	OverflowSafeInt64 cost[GRAPH_MAX_DATASETS][24]; ///< last 2 years
};

static void DrawGraph(const GraphDrawer *gw)
{
	uint x, y;                       ///< Reused whenever x and y coordinates are needed.
	OverflowSafeInt64 highest_value; ///< Highest value to be drawn.
	int x_axis_offset;               ///< Distance from the top of the graph to the x axis.

	/* the colors and cost array of GraphDrawer must accomodate
	 * both values for cargo and players. So if any are higher, quit */
	assert(GRAPH_MAX_DATASETS >= (int)NUM_CARGO && GRAPH_MAX_DATASETS >= (int)MAX_PLAYERS);
	assert(gw->num_vert_lines > 0);

	byte grid_colour = _colour_gradient[14][4];

	/* The coordinates of the opposite edges of the graph. */
	int bottom = gw->top + gw->height - 1;
	int right  = gw->left + GRAPH_X_POSITION_BEGINNING + gw->num_vert_lines * GRAPH_X_POSITION_SEPARATION - 1;

	/* Draw the vertical grid lines. */

	/* Don't draw the first line, as that's where the axis will be. */
	x = gw->left + GRAPH_X_POSITION_BEGINNING + GRAPH_X_POSITION_SEPARATION;

	for (int i = 0; i < gw->num_vert_lines; i++) {
		GfxFillRect(x, gw->top, x, bottom, grid_colour);
		x += GRAPH_X_POSITION_SEPARATION;
	}

	/* Draw the horizontal grid lines. */
	x = gw->left + GRAPH_X_POSITION_BEGINNING;
	y = gw->height + gw->top;

	for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
		GfxFillRect(x, y, right, y, grid_colour);
		y -= (gw->height / (GRAPH_NUM_LINES_Y - 1));
	}

	/* Draw the y axis. */
	GfxFillRect(x, gw->top, x, bottom, GRAPH_AXIS_LINE_COLOUR);

	/* Find the distance from the top of the graph to the x axis. */
	x_axis_offset = gw->height;

	/* The graph is currently symmetrical about the x axis. */
	if (gw->has_negative_values) x_axis_offset /= 2;

	/* Draw the x axis. */
	y = x_axis_offset + gw->top;
	GfxFillRect(x, y, right, y, GRAPH_AXIS_LINE_COLOUR);

	/* Find the largest value that will be drawn. */
	if (gw->num_on_x_axis == 0)
		return;

	assert(gw->num_on_x_axis > 0);
	assert(gw->num_dataset > 0);

	/* Start of with a value of twice the height of the graph in pixels. It's a
	 * bit arbitrary, but it makes the cargo payment graph look a little nicer,
	 * and prevents division by zero when calculating where the datapoint
	 * should be drawn. */
	highest_value = x_axis_offset * 2;

	for (int i = 0; i < gw->num_dataset; i++) {
		if (!HasBit(gw->excluded_data, i)) {
			for (int j = 0; j < gw->num_on_x_axis; j++) {
				OverflowSafeInt64 datapoint = gw->cost[i][j];

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
	if (gw->has_negative_values) y_label_separation *= 2;

	x = gw->left + GRAPH_X_POSITION_BEGINNING + 1;
	y = gw->top - 3;

	for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
		SetDParam(0, gw->format_str_y_axis);
		SetDParam(1, y_label);
		DrawStringRightAligned(x, y, STR_0170, GRAPH_AXIS_LABEL_COLOUR);

		y_label -= y_label_separation;
		y += (gw->height / (GRAPH_NUM_LINES_Y - 1));
	}

	/* draw strings on the x axis */
	if (gw->month != 0xFF) {
		x = gw->left + GRAPH_X_POSITION_BEGINNING;
		y = gw->top + gw->height + 1;
		byte month = gw->month;
		Year year  = gw->year;
		for (int i = 0; i < gw->num_on_x_axis; i++) {
			SetDParam(0, month + STR_0162_JAN);
			SetDParam(1, month + STR_0162_JAN + 2);
			SetDParam(2, year);
			DrawString(x, y, month == 0 ? STR_016F : STR_016E, GRAPH_AXIS_LABEL_COLOUR);

			month += 3;
			if (month >= 12) {
				month = 0;
				year++;
			}
			x += GRAPH_X_POSITION_SEPARATION;
		}
	} else {
		/* Draw the label under the data point rather than on the grid line. */
		x = gw->left + GRAPH_X_POSITION_BEGINNING + (GRAPH_X_POSITION_SEPARATION / 2) + 1;
		y = gw->top + gw->height + 1;
		uint16 label = gw->x_values_start;

		for (int i = 0; i < gw->num_on_x_axis; i++) {
			SetDParam(0, label);
			DrawStringCentered(x, y, STR_01CB, GRAPH_AXIS_LABEL_COLOUR);

			label += gw->x_values_increment;
			x += GRAPH_X_POSITION_SEPARATION;
		}
	}

	/* draw lines and dots */
	for (int i = 0; i < gw->num_dataset; i++) {
		if (!HasBit(gw->excluded_data, i)) {
			/* Centre the dot between the grid lines. */
			x = gw->left + GRAPH_X_POSITION_BEGINNING + (GRAPH_X_POSITION_SEPARATION / 2);

			byte color  = gw->colors[i];
			uint prev_x = INVALID_DATAPOINT_POS;
			uint prev_y = INVALID_DATAPOINT_POS;

			for (int j = 0; j < gw->num_on_x_axis; j++) {
				OverflowSafeInt64 datapoint = gw->cost[i][j];

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

					y = gw->top + x_axis_offset - (x_axis_offset * datapoint) / (highest_value >> reduce_range);

					/* Draw the point. */
					GfxFillRect(x - 1, y - 1, x + 1, y + 1, color);

					/* Draw the line connected to the previous point. */
					if (prev_x != INVALID_DATAPOINT_POS) GfxDrawLine(prev_x, prev_y, x, y, color);

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

/****************/
/* GRAPH LEGEND */
/****************/

static void GraphLegendWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_CREATE:
			for (uint i = 3; i < w->widget_count; i++) {
				if (!HasBit(_legend_excluded_players, i - 3)) w->LowerWidget(i);
			}
			break;

		case WE_PAINT: {
			const Player *p;

			FOR_ALL_PLAYERS(p) {
				if (p->is_active) continue;

				SetBit(_legend_excluded_players, p->index);
				w->RaiseWidget(p->index + 3);
			}

			DrawWindowWidgets(w);

			FOR_ALL_PLAYERS(p) {
				if (!p->is_active) continue;

				DrawPlayerIcon(p->index, 4, 18 + p->index * 12);

				SetDParam(0, p->index);
				SetDParam(1, p->index);
				DrawString(21, 17 + p->index * 12, STR_7021, HasBit(_legend_excluded_players, p->index) ? TC_BLACK : TC_WHITE);
			}
			break;
		}

		case WE_CLICK:
			if (!IsInsideMM(e->we.click.widget, 3, 11)) return;

			ToggleBit(_legend_excluded_players, e->we.click.widget - 3);
			w->ToggleWidgetLoweredState(e->we.click.widget);
			SetWindowDirty(w);
			InvalidateWindow(WC_INCOME_GRAPH, 0);
			InvalidateWindow(WC_OPERATING_PROFIT, 0);
			InvalidateWindow(WC_DELIVERED_CARGO, 0);
			InvalidateWindow(WC_PERFORMANCE_HISTORY, 0);
			InvalidateWindow(WC_COMPANY_VALUE, 0);
			break;
	}
}

static const Widget _graph_legend_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                       STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   249,     0,    13, STR_704E_KEY_TO_COMPANY_GRAPHS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   249,    14,   113, 0x0,                            STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    16,    27, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    28,    39, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    40,    51, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    52,    63, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    64,    75, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    76,    87, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,    88,    99, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,     2,   247,   100,   111, 0x0,                            STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{   WIDGETS_END},
};

static const WindowDesc _graph_legend_desc = {
	WDP_AUTO, WDP_AUTO, 250, 114, 250, 114,
	WC_GRAPH_LEGEND, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_graph_legend_widgets,
	GraphLegendWndProc
};

static void ShowGraphLegend()
{
	AllocateWindowDescFront(&_graph_legend_desc, 0);
}

/********************/
/* OPERATING PROFIT */
/********************/

static void SetupGraphDrawerForPlayers(GraphDrawer *gd)
{
	const Player* p;
	uint excluded_players = _legend_excluded_players;
	byte nums;
	int mo, yr;

	/* Exclude the players which aren't valid */
	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) SetBit(excluded_players, p->index);
	}
	gd->excluded_data = excluded_players;
	gd->num_vert_lines = 24;

	nums = 0;
	FOR_ALL_PLAYERS(p) {
		if (p->is_active) nums = max(nums, p->num_valid_stat_ent);
	}
	gd->num_on_x_axis = min(nums, 24);

	mo = (_cur_month / 3 - nums) * 3;
	yr = _cur_year;
	while (mo < 0) {
		yr--;
		mo += 12;
	}

	gd->year = yr;
	gd->month = mo;
}

static void OperatingProfitWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			GraphDrawer gd;
			const Player* p;

			DrawWindowWidgets(w);

			gd.left = 2;
			gd.top = 18;
			gd.height = 136;
			gd.has_negative_values = true;
			gd.format_str_y_axis = STR_CURRCOMPACT;

			SetupGraphDrawerForPlayers(&gd);

			int numd = 0;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active) {
					gd.colors[numd] = _colour_gradient[p->player_color][6];
					for (int j = gd.num_on_x_axis, i = 0; --j >= 0;) {
						gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_DATAPOINT : (p->old_economy[j].income + p->old_economy[j].expenses);
						i++;
					}
				}
				numd++;
			}

			gd.num_dataset = numd;

			DrawGraph(&gd);
			break;
		}

		case WE_CLICK:
			/* Clicked on legend? */
			if (e->we.click.widget == 2) ShowGraphLegend();
			break;
	}
}

static const Widget _operating_profit_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                        STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   525,     0,    13, STR_7025_OPERATING_PROFIT_GRAPH, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   526,   575,     0,    13, STR_704C_KEY,                    STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   575,    14,   173, 0x0,                             STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _operating_profit_desc = {
	WDP_AUTO, WDP_AUTO, 576, 174, 576, 174,
	WC_OPERATING_PROFIT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_operating_profit_widgets,
	OperatingProfitWndProc
};


void ShowOperatingProfitGraph()
{
	if (AllocateWindowDescFront(&_operating_profit_desc, 0)) {
		InvalidateWindow(WC_GRAPH_LEGEND, 0);
	}
}


/****************/
/* INCOME GRAPH */
/****************/

static void IncomeGraphWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			GraphDrawer gd;
			const Player* p;

			DrawWindowWidgets(w);

			gd.left = 2;
			gd.top = 18;
			gd.height = 104;
			gd.has_negative_values = false;
			gd.format_str_y_axis = STR_CURRCOMPACT;
			SetupGraphDrawerForPlayers(&gd);

			int numd = 0;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active) {
					gd.colors[numd] = _colour_gradient[p->player_color][6];
					for (int j = gd.num_on_x_axis, i = 0; --j >= 0;) {
						gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_DATAPOINT : p->old_economy[j].income;
						i++;
					}
				}
				numd++;
			}

			gd.num_dataset = numd;

			DrawGraph(&gd);
			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == 2) ShowGraphLegend();
			break;
	}
}

static const Widget _income_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   525,     0,    13, STR_7022_INCOME_GRAPH, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   526,   575,     0,    13, STR_704C_KEY,          STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   575,    14,   141, 0x0,                   STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _income_graph_desc = {
	WDP_AUTO, WDP_AUTO, 576, 142, 576, 142,
	WC_INCOME_GRAPH, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_income_graph_widgets,
	IncomeGraphWndProc
};

void ShowIncomeGraph()
{
	if (AllocateWindowDescFront(&_income_graph_desc, 0)) {
		InvalidateWindow(WC_GRAPH_LEGEND, 0);
	}
}

/*******************/
/* DELIVERED CARGO */
/*******************/

static void DeliveredCargoGraphWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			GraphDrawer gd;
			const Player* p;

			DrawWindowWidgets(w);

			gd.left = 2;
			gd.top = 18;
			gd.height = 104;
			gd.has_negative_values = false;
			gd.format_str_y_axis = STR_7024;
			SetupGraphDrawerForPlayers(&gd);

			int numd = 0;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active) {
					gd.colors[numd] = _colour_gradient[p->player_color][6];
					for (int j = gd.num_on_x_axis, i = 0; --j >= 0;) {
						gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_DATAPOINT : (OverflowSafeInt64)p->old_economy[j].delivered_cargo;
						i++;
					}
				}
				numd++;
			}

			gd.num_dataset = numd;

			DrawGraph(&gd);
			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == 2) ShowGraphLegend();
			break;
	}
}

static const Widget _delivered_cargo_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   525,     0,    13, STR_7050_UNITS_OF_CARGO_DELIVERED, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   526,   575,     0,    13, STR_704C_KEY,                      STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   575,    14,   141, 0x0,                               STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _delivered_cargo_graph_desc = {
	WDP_AUTO, WDP_AUTO, 576, 142, 576, 142,
	WC_DELIVERED_CARGO, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_delivered_cargo_graph_widgets,
	DeliveredCargoGraphWndProc
};

void ShowDeliveredCargoGraph()
{
	if (AllocateWindowDescFront(&_delivered_cargo_graph_desc, 0)) {
		InvalidateWindow(WC_GRAPH_LEGEND, 0);
	}
}

/***********************/
/* PERFORMANCE HISTORY */
/***********************/

static void PerformanceHistoryWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			GraphDrawer gd;
			const Player* p;

			DrawWindowWidgets(w);

			gd.left = 2;
			gd.top = 18;
			gd.height = 200;
			gd.has_negative_values = false;
			gd.format_str_y_axis = STR_7024;
			SetupGraphDrawerForPlayers(&gd);

			int numd = 0;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active) {
					gd.colors[numd] = _colour_gradient[p->player_color][6];
					for (int j = gd.num_on_x_axis, i = 0; --j >= 0;) {
						gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_DATAPOINT : (OverflowSafeInt64)p->old_economy[j].performance_history;
						i++;
					}
				}
				numd++;
			}

			gd.num_dataset = numd;

			DrawGraph(&gd);
			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == 2) ShowGraphLegend();
			if (e->we.click.widget == 3) ShowPerformanceRatingDetail();
			break;
	}
}

static const Widget _performance_history_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                             STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   475,     0,    13, STR_7051_COMPANY_PERFORMANCE_RATINGS, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   526,   575,     0,    13, STR_704C_KEY,                         STR_704D_SHOW_KEY_TO_GRAPHS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   476,   525,     0,    13, STR_PERFORMANCE_DETAIL_KEY,           STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   575,    14,   237, 0x0,                                  STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _performance_history_desc = {
	WDP_AUTO, WDP_AUTO, 576, 238, 576, 238,
	WC_PERFORMANCE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_performance_history_widgets,
	PerformanceHistoryWndProc
};

void ShowPerformanceHistoryGraph()
{
	if (AllocateWindowDescFront(&_performance_history_desc, 0)) {
		InvalidateWindow(WC_GRAPH_LEGEND, 0);
	}
}

/*****************/
/* COMPANY VALUE */
/*****************/

static void CompanyValueGraphWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			GraphDrawer gd;
			const Player* p;

			DrawWindowWidgets(w);

			gd.left = 2;
			gd.top = 18;
			gd.height = 200;
			gd.has_negative_values = false;
			gd.format_str_y_axis = STR_CURRCOMPACT;
			SetupGraphDrawerForPlayers(&gd);

			int numd = 0;
			FOR_ALL_PLAYERS(p) {
				if (p->is_active) {
					gd.colors[numd] = _colour_gradient[p->player_color][6];
					for (int j = gd.num_on_x_axis, i = 0; --j >= 0;) {
						gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_DATAPOINT : p->old_economy[j].company_value;
						i++;
					}
				}
				numd++;
			}

			gd.num_dataset = numd;

			DrawGraph(&gd);
			break;
		}

		case WE_CLICK:
			if (e->we.click.widget == 2) ShowGraphLegend();
			break;
	}
}

static const Widget _company_value_graph_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   525,     0,    13, STR_7052_COMPANY_VALUES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,    14,   526,   575,     0,    13, STR_704C_KEY,            STR_704D_SHOW_KEY_TO_GRAPHS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   575,    14,   237, 0x0,                     STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _company_value_graph_desc = {
	WDP_AUTO, WDP_AUTO, 576, 238, 576, 238,
	WC_COMPANY_VALUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_company_value_graph_widgets,
	CompanyValueGraphWndProc
};

void ShowCompanyValueGraph()
{
	if (AllocateWindowDescFront(&_company_value_graph_desc, 0)) {
		InvalidateWindow(WC_GRAPH_LEGEND, 0);
	}
}

/*****************/
/* PAYMENT RATES */
/*****************/

static void CargoPaymentRatesWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			GraphDrawer gd;

			DrawWindowWidgets(w);

			int x = 495;
			int y = 24;

			gd.excluded_data = _legend_excluded_cargo;
			gd.left = 2;
			gd.top = 24;
			gd.height = w->height - 38;
			gd.has_negative_values = false;
			gd.format_str_y_axis = STR_CURRCOMPACT;
			gd.num_on_x_axis = 20;
			gd.num_vert_lines = 20;
			gd.month = 0xFF;
			gd.x_values_start     = 10;
			gd.x_values_increment = 10;

			uint i = 0;
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				const CargoSpec *cs = GetCargo(c);
				if (!cs->IsValid()) continue;

				/* Only draw labels for widgets that exist. If the widget doesn't
				 * exist then the local player has used the climate cheat or
				 * changed the NewGRF configuration with this window open. */
				if (i + 3 < w->widget_count) {
					/* Since the buttons have no text, no images,
					 * both the text and the colored box have to be manually painted.
					 * clk_dif will move one pixel down and one pixel to the right
					 * when the button is clicked */
					byte clk_dif = w->IsWidgetLowered(i + 3) ? 1 : 0;

					GfxFillRect(x + clk_dif, y + clk_dif, x + 8 + clk_dif, y + 5 + clk_dif, 0);
					GfxFillRect(x + 1 + clk_dif, y + 1 + clk_dif, x + 7 + clk_dif, y + 4 + clk_dif, cs->legend_colour);
					SetDParam(0, cs->name);
					DrawString(x + 14 + clk_dif, y + clk_dif, STR_7065, TC_FROMSTRING);
					y += 8;
				}

				gd.colors[i] = cs->legend_colour;
				for (uint j = 0; j != 20; j++) {
					gd.cost[i][j] = GetTransportedGoodsIncome(10, 20, j * 6 + 6, c);
				}

				i++;
			}
			gd.num_dataset = i;

			DrawGraph(&gd);

			DrawString(2 + 46, 24 + gd.height + 7, STR_7062_DAYS_IN_TRANSIT, TC_FROMSTRING);
			DrawString(2 + 84, 24 - 9, STR_7063_PAYMENT_FOR_DELIVERING, TC_FROMSTRING);
			break;
		}

		case WE_CLICK:
			if (e->we.click.widget >= 3) {
				ToggleBit(_legend_excluded_cargo, e->we.click.widget - 3);
				w->ToggleWidgetLoweredState(e->we.click.widget);
				SetWindowDirty(w);
			}
			break;
	}
}

static const Widget _cargo_payment_rates_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                     STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   567,     0,    13, STR_7061_CARGO_PAYMENT_RATES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL, RESIZE_BOTTOM,    14,     0,   567,    14,    45, 0x0,                          STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _cargo_payment_rates_desc = {
	WDP_AUTO, WDP_AUTO, 568, 46, 568, 46,
	WC_PAYMENT_RATES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_cargo_payment_rates_widgets,
	CargoPaymentRatesWndProc
};


void ShowCargoPaymentRates()
{
	Window *w = AllocateWindowDescFront(&_cargo_payment_rates_desc, 0);
	if (w == NULL) return;

	/* Count the number of active cargo types */
	uint num_active = 0;
	for (CargoID c = 0; c < NUM_CARGO; c++) {
		if (GetCargo(c)->IsValid()) num_active++;
	}

	/* Resize the window to fit the cargo types */
	ResizeWindow(w, 0, max(num_active, 12U) * 8);

	/* Add widgets for each cargo type */
	w->widget_count += num_active;
	w->widget = ReallocT(w->widget, w->widget_count + 1);
	w->widget[w->widget_count].type = WWT_LAST;

	/* Set the properties of each widget */
	for (uint i = 0; i != num_active; i++) {
		Widget *wi = &w->widget[3 + i];
		wi->type     = WWT_PANEL;
		wi->display_flags = RESIZE_NONE;
		wi->color    = 12;
		wi->left     = 493;
		wi->right    = 562;
		wi->top      = 24 + i * 8;
		wi->bottom   = wi->top + 7;
		wi->data     = 0;
		wi->tooltips = STR_7064_TOGGLE_GRAPH_FOR_CARGO;

		if (!HasBit(_legend_excluded_cargo, i)) w->LowerWidget(i + 3);
	}

	SetWindowDirty(w);
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

static int CDECL PerfHistComp(const void* elem1, const void* elem2)
{
	const Player* p1 = *(const Player* const*)elem1;
	const Player* p2 = *(const Player* const*)elem2;

	return p2->old_economy[1].performance_history - p1->old_economy[1].performance_history;
}

static void CompanyLeagueWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT: {
			const Player* plist[MAX_PLAYERS];
			const Player* p;

			DrawWindowWidgets(w);

			uint pl_num = 0;
			FOR_ALL_PLAYERS(p) if (p->is_active) plist[pl_num++] = p;

			qsort((void*)plist, pl_num, sizeof(*plist), PerfHistComp);

			for (uint i = 0; i != pl_num; i++) {
				p = plist[i];
				SetDParam(0, i + STR_01AC_1ST);
				SetDParam(1, p->index);
				SetDParam(2, p->index);
				SetDParam(3, GetPerformanceTitleFromValue(p->old_economy[1].performance_history));

				DrawString(2, 15 + i * 10, i == 0 ? STR_7054 : STR_7055, TC_FROMSTRING);
				DrawPlayerIcon(p->index, 27, 16 + i * 10);
			}

			break;
		}
	}
}


static const Widget _company_league_widgets[] = {
{   WWT_CLOSEBOX, RESIZE_NONE, 14,   0,  10,  0, 13, STR_00C5,                      STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION, RESIZE_NONE, 14,  11, 387,  0, 13, STR_7053_COMPANY_LEAGUE_TABLE, STR_018C_WINDOW_TITLE_DRAG_THIS},
{  WWT_STICKYBOX, RESIZE_NONE, 14, 388, 399,  0, 13, STR_NULL,                      STR_STICKY_BUTTON},
{      WWT_PANEL, RESIZE_NONE, 14,   0, 399, 14, 96, 0x0,                           STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _company_league_desc = {
	WDP_AUTO, WDP_AUTO, 400, 97, 400, 97,
	WC_COMPANY_LEAGUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_company_league_widgets,
	CompanyLeagueWndProc
};

void ShowCompanyLeagueTable()
{
	AllocateWindowDescFront(&_company_league_desc, 0);
}

/*****************************/
/* PERFORMANCE RATING DETAIL */
/*****************************/

static void PerformanceRatingDetailWndProc(Window *w, WindowEvent *e)
{
	static PlayerID _performance_rating_detail_player = INVALID_PLAYER;

	switch (e->event) {
		case WE_PAINT: {
			byte x;
			uint16 y = 14;
			int total_score = 0;
			int color_done, color_notdone;

			/* Draw standard stuff */
			DrawWindowWidgets(w);

			/* Check if the currently selected player is still active. */
			if (_performance_rating_detail_player == INVALID_PLAYER || !GetPlayer(_performance_rating_detail_player)->is_active) {
				if (_performance_rating_detail_player != INVALID_PLAYER) {
					/* Raise and disable the widget for the previous selection. */
					w->RaiseWidget(_performance_rating_detail_player + 13);
					w->DisableWidget(_performance_rating_detail_player + 13);
					SetWindowDirty(w);

					_performance_rating_detail_player = INVALID_PLAYER;
				}

				for (PlayerID i = PLAYER_FIRST; i < MAX_PLAYERS; i++) {
					if (GetPlayer(i)->is_active) {
						/* Lower the widget corresponding to this player. */
						w->LowerWidget(i + 13);
						SetWindowDirty(w);

						_performance_rating_detail_player = i;
						break;
					}
				}
			}

			/* If there are no active players, don't display anything else. */
			if (_performance_rating_detail_player == INVALID_PLAYER) break;

			/* Paint the player icons */
			for (PlayerID i = PLAYER_FIRST; i < MAX_PLAYERS; i++) {
				if (!GetPlayer(i)->is_active) {
					/* Check if we have the player as an active player */
					if (!w->IsWidgetDisabled(i + 13)) {
						/* Bah, player gone :( */
						w->DisableWidget(i + 13);

						/* We need a repaint */
						SetWindowDirty(w);
					}
					continue;
				}

				/* Check if we have the player marked as inactive */
				if (w->IsWidgetDisabled(i + 13)) {
					/* New player! Yippie :p */
					w->EnableWidget(i + 13);
					/* We need a repaint */
					SetWindowDirty(w);
				}

				x = (i == _performance_rating_detail_player) ? 1 : 0;
				DrawPlayerIcon(i, i * 37 + 13 + x, 16 + x);
			}

			/* The colors used to show how the progress is going */
			color_done = _colour_gradient[COLOUR_GREEN][4];
			color_notdone = _colour_gradient[COLOUR_RED][4];

			/* Draw all the score parts */
			for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) {
				int val    = _score_part[_performance_rating_detail_player][i];
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
				DrawStringRightAligned(107, y, SET_PERFORMANCE_DETAIL_INT, TC_FROMSTRING);

				/* Calculate the %-bar */
				x = Clamp(val, 0, needed) * 50 / needed;

				/* SCORE_LOAN is inversed */
				if (val < 0 && i == SCORE_LOAN) x = 0;

				/* Draw the bar */
				if (x !=  0) GfxFillRect(112,     y - 2, 112 + x,  y + 10, color_done);
				if (x != 50) GfxFillRect(112 + x, y - 2, 112 + 50, y + 10, color_notdone);

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

			break;
		}

		case WE_CLICK:
			/* Check which button is clicked */
			if (IsInsideMM(e->we.click.widget, 13, 21)) {
				/* Is it no on disable? */
				if (!w->IsWidgetDisabled(e->we.click.widget)) {
					w->RaiseWidget(_performance_rating_detail_player + 13);
					_performance_rating_detail_player = (PlayerID)(e->we.click.widget - 13);
					w->LowerWidget(_performance_rating_detail_player + 13);
					SetWindowDirty(w);
				}
			}
			break;

		case WE_CREATE: {
			Player *p2;

			/* Disable the players who are not active */
			for (PlayerID i = PLAYER_FIRST; i < MAX_PLAYERS; i++) {
				w->SetWidgetDisabledState(i + 13, !GetPlayer(i)->is_active);
			}
			/* Update all player stats with the current data
			 * (this is because _score_info is not saved to a savegame) */
			FOR_ALL_PLAYERS(p2) {
				if (p2->is_active) UpdateCompanyRatingAndValue(p2, false);
			}

			w->custom[0] = DAY_TICKS;
			w->custom[1] = 5;

			if (_performance_rating_detail_player != INVALID_PLAYER) w->LowerWidget(_performance_rating_detail_player + 13);
			SetWindowDirty(w);

			break;
		}

		case WE_TICK:
			/* Update the player score every 5 days */
			if (--w->custom[0] == 0) {
				w->custom[0] = DAY_TICKS;
				if (--w->custom[1] == 0) {
					Player *p2;

					w->custom[1] = 5;
					FOR_ALL_PLAYERS(p2) {
						/* Skip if player is not active */
						if (p2->is_active) UpdateCompanyRatingAndValue(p2, false);
					}
					SetWindowDirty(w);
				}
			}

			break;
	}
}

static const Widget _performance_rating_detail_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   298,     0,    13, STR_PERFORMANCE_DETAIL, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,    14,    27, 0x0,                    STR_NULL},

{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,    28,    47, 0x0,                    STR_PERFORMANCE_DETAIL_VEHICLES_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,    48,    67, 0x0,                    STR_PERFORMANCE_DETAIL_STATIONS_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,    68,    87, 0x0,                    STR_PERFORMANCE_DETAIL_MIN_PROFIT_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,    88,   107, 0x0,                    STR_PERFORMANCE_DETAIL_MIN_INCOME_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,   108,   127, 0x0,                    STR_PERFORMANCE_DETAIL_MAX_INCOME_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,   128,   147, 0x0,                    STR_PERFORMANCE_DETAIL_DELIVERED_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,   148,   167, 0x0,                    STR_PERFORMANCE_DETAIL_CARGO_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,   168,   187, 0x0,                    STR_PERFORMANCE_DETAIL_MONEY_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,   188,   207, 0x0,                    STR_PERFORMANCE_DETAIL_LOAN_TIP},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   298,   208,   227, 0x0,                    STR_PERFORMANCE_DETAIL_TOTAL_TIP},

{      WWT_PANEL,   RESIZE_NONE,    14,     2,    38,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,    39,    75,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,    76,   112,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,   113,   149,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,   150,   186,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,   187,   223,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,   224,   260,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{      WWT_PANEL,   RESIZE_NONE,    14,   261,   297,    14,    26, 0x0,                    STR_704F_CLICK_HERE_TO_TOGGLE_COMPANY},
{   WIDGETS_END},
};

static const WindowDesc _performance_rating_detail_desc = {
	WDP_AUTO, WDP_AUTO, 299, 228, 299, 228,
	WC_PERFORMANCE_DETAIL, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_performance_rating_detail_widgets,
	PerformanceRatingDetailWndProc
};

void ShowPerformanceRatingDetail()
{
	AllocateWindowDescFront(&_performance_rating_detail_desc, 0);
}
