/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "functions.h"
#include "window.h"
#include "gui.h"
#include "gfx.h"
#include "player.h"
#include "economy.h"
#include "variables.h"
#include "date.h"
#include "helpers.hpp"

const byte _cargo_colours[NUM_CARGO] = {152, 32, 15, 174, 208, 194, 191, 84, 184, 10, 202, 48};

static uint _legend_excludebits;
static uint _legend_cargobits;

/************************/
/* GENERIC GRAPH DRAWER */
/************************/

enum {GRAPH_NUM = 16};

typedef struct GraphDrawer {
	uint sel; // bitmask of the players *excluded* (e.g. 11111111 means that no players are shown)
	byte num_dataset;
	byte num_on_x_axis;
	byte month;
	Year year;
	bool include_neg;
	byte num_vert_lines;
	uint16 unk61A;
	uint16 unk61C;
	int left, top;
	uint height;
	StringID format_str_y_axis;
	byte color_3, color_2, bg_line_color;
	byte colors[GRAPH_NUM];
	uint64 cost[GRAPH_NUM][24]; // last 2 years
} GraphDrawer;

#define INVALID_VALUE 0x80000000

static void DrawGraph(const GraphDrawer *gw)
{

	int i,j,k;
	uint x,y,old_x,old_y;
	int color;
	int right, bottom;
	int num_x, num_dataset;
	const uint64 *row_ptr, *col_ptr;
	int64 mx;
	int adj_height;
	uint64 y_scaling, tmp;
	int64 value;
	int64 cur_val;
	uint sel;

	/* the colors and cost array of GraphDrawer must accomodate
	 * both values for cargo and players. So if any are higher, quit */
	assert(GRAPH_NUM >= (int)NUM_CARGO && GRAPH_NUM >= (int)MAX_PLAYERS);

	color = _colour_gradient[gw->bg_line_color][4];

	/* draw the vertical lines */
	i = gw->num_vert_lines; assert(i > 0);
	x = gw->left + 66;
	bottom = gw->top + gw->height - 1;
	do {
		GfxFillRect(x, gw->top, x, bottom, color);
		x += 22;
	} while (--i);

	/* draw the horizontal lines */
	i = 9;
	x = gw->left + 44;
	y = gw->height + gw->top;
	right = gw->left + 44 + gw->num_vert_lines*22-1;

	do {
		GfxFillRect(x, y, right, y, color);
		y -= gw->height >> 3;
	} while (--i);

	/* draw vertical edge line */
	GfxFillRect(x, gw->top, x, bottom, gw->color_2);

	adj_height = gw->height;
	if (gw->include_neg) adj_height >>= 1;

	/* draw horiz edge line */
	y = adj_height + gw->top;
	GfxFillRect(x, y, right, y, gw->color_2);

	/* find the max element */
	if (gw->num_on_x_axis == 0)
		return;

	num_dataset = gw->num_dataset;
	assert(num_dataset > 0);

	row_ptr = gw->cost[0];
	mx = 0;
		/* bit selection for the showing of various players, base max element
		 * on to-be shown player-information. This way the graph can scale */
	sel = gw->sel;
	do {
		if (!(sel&1)) {
			num_x = gw->num_on_x_axis;
			assert(num_x > 0);
			col_ptr = row_ptr;
			do {
				if (*col_ptr != INVALID_VALUE) {
					mx = max((uint64)mx, *col_ptr);
				}
			} while (col_ptr++, --num_x);
		}
	} while (sel>>=1, row_ptr+=24, --num_dataset);

	/* setup scaling */
	y_scaling = INVALID_VALUE;
	value = adj_height * 2;

	if (mx > value) {
		mx = (mx + 7) & ~7;
		y_scaling = (((uint64) (value>>1) << 32) / mx);
		value = mx;
	}

	/* draw text strings on the y axis */
	tmp = value;
	if (gw->include_neg) tmp >>= 1;
	x = gw->left + 45;
	y = gw->top - 3;
	i = 9;
	do {
		SetDParam(0, gw->format_str_y_axis);
		SetDParam64(1, (int64)tmp);
		tmp -= (value >> 3);
		DrawStringRightAligned(x, y, STR_0170, gw->color_3);
		y += gw->height >> 3;
	} while (--i);

	/* draw strings on the x axis */
	if (gw->month != 0xFF) {
		x = gw->left + 44;
		y = gw->top + gw->height + 1;
		j = gw->month;
		k = gw->year;
		i = gw->num_on_x_axis;assert(i>0);
		do {
			SetDParam(2, k);
			SetDParam(0, j + STR_0162_JAN);
			SetDParam(1, j + STR_0162_JAN + 2);
			DrawString(x, y, j == 0 ? STR_016F : STR_016E, gw->color_3);

			j += 3;
			if (j >= 12) {
				j = 0;
				k++;
			}
			x += 22;
		} while (--i);
	} else {
		x = gw->left + 52;
		y = gw->top + gw->height + 1;
		j = gw->unk61A;
		i = gw->num_on_x_axis;assert(i>0);
		do {
			SetDParam(0, j);
			DrawString(x, y, STR_01CB, gw->color_3);
			j += gw->unk61C;
			x += 22;
		} while (--i);
	}

	/* draw lines and dots */
	i = 0;
	row_ptr = gw->cost[0];
	sel = gw->sel; // show only selected lines. GraphDrawer qw->sel set in Graph-Legend (_legend_excludebits)
	do {
		if (!(sel & 1)) {
			x = gw->left + 55;
			j = gw->num_on_x_axis;assert(j>0);
			col_ptr = row_ptr;
			color = gw->colors[i];
			old_y = old_x = INVALID_VALUE;
			do {
				cur_val = *col_ptr++;
				if (cur_val != INVALID_VALUE) {
					y = adj_height - BIGMULSS64(cur_val, y_scaling >> 1, 31) + gw->top;

					GfxFillRect(x-1, y-1, x+1, y+1, color);
					if (old_x != INVALID_VALUE)
						GfxDrawLine(old_x, old_y, x, y, color);

					old_x = x;
					old_y = y;
				} else {
					old_x = INVALID_VALUE;
				}
			} while (x+=22,--j);
		}
	} while (sel>>=1,row_ptr+=24, ++i < gw->num_dataset);
}

/****************/
/* GRAPH LEGEND */
/****************/

static void GraphLegendWndProc(Window *w, WindowEvent *e)
{
	const Player* p;

	switch (e->event) {
	case WE_CREATE: {
		uint i;
		for (i = 3; i < w->widget_count; i++) {
			if (!HASBIT(_legend_excludebits, i - 3)) LowerWindowWidget(w, i);
		}
		break;
	}

	case WE_PAINT:
		FOR_ALL_PLAYERS(p) {
			if (!p->is_active) {
				SETBIT(_legend_excludebits, p->index);
				RaiseWindowWidget(w, p->index + 3);
			}
		}
		DrawWindowWidgets(w);

		FOR_ALL_PLAYERS(p) {
			if (!p->is_active) continue;

			DrawPlayerIcon(p->index, 4, 18+p->index*12);

			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			SetDParam(2, GetPlayerNameString(p->index, 3));
			DrawString(21,17+p->index*12,STR_7021,HASBIT(_legend_excludebits, p->index) ? 0x10 : 0xC);
		}
		break;

	case WE_CLICK:
		if (IS_INT_INSIDE(e->we.click.widget, 3, 11)) {
			_legend_excludebits ^= (1 << (e->we.click.widget - 3));
			ToggleWidgetLoweredState(w, e->we.click.widget);
			SetWindowDirty(w);
			InvalidateWindow(WC_INCOME_GRAPH, 0);
			InvalidateWindow(WC_OPERATING_PROFIT, 0);
			InvalidateWindow(WC_DELIVERED_CARGO, 0);
			InvalidateWindow(WC_PERFORMANCE_HISTORY, 0);
			InvalidateWindow(WC_COMPANY_VALUE, 0);
		}
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
	WDP_AUTO, WDP_AUTO, 250, 114,
	WC_GRAPH_LEGEND,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_graph_legend_widgets,
	GraphLegendWndProc
};

static void ShowGraphLegend(void)
{
	AllocateWindowDescFront(&_graph_legend_desc, 0);
}

/********************/
/* OPERATING PROFIT */
/********************/

static void SetupGraphDrawerForPlayers(GraphDrawer *gd)
{
	const Player* p;
	uint excludebits = _legend_excludebits;
	byte nums;
	int mo,yr;

	// Exclude the players which aren't valid
	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) SETBIT(excludebits,p->index);
	}
	gd->sel = excludebits;
	gd->num_vert_lines = 24;

	nums = 0;
	FOR_ALL_PLAYERS(p) {
		if (p->is_active) nums = max(nums,p->num_valid_stat_ent);
	}
	gd->num_on_x_axis = min(nums,24);

	mo = (_cur_month/3-nums)*3;
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
		int i,j;
		int numd;

		DrawWindowWidgets(w);

		gd.left = 2;
		gd.top = 18;
		gd.height = 136;
		gd.include_neg = true;
		gd.format_str_y_axis = STR_CURRCOMPACT;
		gd.color_3 = 0x10;
		gd.color_2 = 0xD7;
		gd.bg_line_color = 0xE;

		SetupGraphDrawerForPlayers(&gd);

		numd = 0;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				gd.colors[numd] = _colour_gradient[p->player_color][6];
				for (j = gd.num_on_x_axis, i = 0; --j >= 0;) {
					gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_VALUE : (uint64)(p->old_economy[j].income + p->old_economy[j].expenses);
					i++;
				}
			}
			numd++;
		}

		gd.num_dataset = numd;

		DrawGraph(&gd);
	}	break;
	case WE_CLICK:
		if (e->we.click.widget == 2) /* Clicked on Legend */
			ShowGraphLegend();
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
	WDP_AUTO, WDP_AUTO, 576, 174,
	WC_OPERATING_PROFIT,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_operating_profit_widgets,
	OperatingProfitWndProc
};


void ShowOperatingProfitGraph(void)
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
		int i,j;
		int numd;

		DrawWindowWidgets(w);

		gd.left = 2;
		gd.top = 18;
		gd.height = 104;
		gd.include_neg = false;
		gd.format_str_y_axis = STR_CURRCOMPACT;
		gd.color_3 = 0x10;
		gd.color_2 = 0xD7;
		gd.bg_line_color = 0xE;
		SetupGraphDrawerForPlayers(&gd);

		numd = 0;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				gd.colors[numd] = _colour_gradient[p->player_color][6];
				for (j = gd.num_on_x_axis, i = 0; --j >= 0;) {
					gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_VALUE : (uint64)p->old_economy[j].income;
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
		if (e->we.click.widget == 2)
			ShowGraphLegend();
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
	WDP_AUTO, WDP_AUTO, 576, 142,
	WC_INCOME_GRAPH,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_income_graph_widgets,
	IncomeGraphWndProc
};

void ShowIncomeGraph(void)
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
		int i,j;
		int numd;

		DrawWindowWidgets(w);

		gd.left = 2;
		gd.top = 18;
		gd.height = 104;
		gd.include_neg = false;
		gd.format_str_y_axis = STR_7024;
		gd.color_3 = 0x10;
		gd.color_2 = 0xD7;
		gd.bg_line_color = 0xE;
		SetupGraphDrawerForPlayers(&gd);

		numd = 0;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				gd.colors[numd] = _colour_gradient[p->player_color][6];
				for (j = gd.num_on_x_axis, i = 0; --j >= 0;) {
					gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_VALUE : (uint64)p->old_economy[j].delivered_cargo;
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
		if (e->we.click.widget == 2)
			ShowGraphLegend();
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
	WDP_AUTO, WDP_AUTO, 576, 142,
	WC_DELIVERED_CARGO,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_delivered_cargo_graph_widgets,
	DeliveredCargoGraphWndProc
};

void ShowDeliveredCargoGraph(void)
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
		int i,j;
		int numd;

		DrawWindowWidgets(w);

		gd.left = 2;
		gd.top = 18;
		gd.height = 200;
		gd.include_neg = false;
		gd.format_str_y_axis = STR_7024;
		gd.color_3 = 0x10;
		gd.color_2 = 0xD7;
		gd.bg_line_color = 0xE;
		SetupGraphDrawerForPlayers(&gd);

		numd = 0;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				gd.colors[numd] = _colour_gradient[p->player_color][6];
				for (j = gd.num_on_x_axis, i = 0; --j >= 0;) {
					gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_VALUE : (uint64)p->old_economy[j].performance_history;
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
		if (e->we.click.widget == 2)
			ShowGraphLegend();
		if (e->we.click.widget == 3)
			ShowPerformanceRatingDetail();
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
	WDP_AUTO, WDP_AUTO, 576, 238,
	WC_PERFORMANCE_HISTORY,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_performance_history_widgets,
	PerformanceHistoryWndProc
};

void ShowPerformanceHistoryGraph(void)
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
		int i,j;
		int numd;

		DrawWindowWidgets(w);

		gd.left = 2;
		gd.top = 18;
		gd.height = 200;
		gd.include_neg = false;
		gd.format_str_y_axis = STR_CURRCOMPACT;
		gd.color_3 = 0x10;
		gd.color_2 = 0xD7;
		gd.bg_line_color = 0xE;
		SetupGraphDrawerForPlayers(&gd);

		numd = 0;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) {
				gd.colors[numd] = _colour_gradient[p->player_color][6];
				for (j = gd.num_on_x_axis, i = 0; --j >= 0;) {
					gd.cost[numd][i] = (j >= p->num_valid_stat_ent) ? INVALID_VALUE : (uint64)p->old_economy[j].company_value;
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
		if (e->we.click.widget == 2)
			ShowGraphLegend();
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
	WDP_AUTO, WDP_AUTO, 576, 238,
	WC_COMPANY_VALUE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_company_value_graph_widgets,
	CompanyValueGraphWndProc
};

void ShowCompanyValueGraph(void)
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
	case WE_CREATE: {
		uint i;
		for (i = 3; i < w->widget_count; i++) {
			if (!HASBIT(_legend_cargobits, i - 3)) LowerWindowWidget(w, i);
		}
		break;
	}

	case WE_PAINT: {
		int j, x, y;
		CargoID i;
		GraphDrawer gd;

		DrawWindowWidgets(w);

		x = 495;
		y = 24;

		gd.sel = _legend_cargobits;
		gd.left = 2;
		gd.top = 24;
		gd.height = 104;
		gd.include_neg = false;
		gd.format_str_y_axis = STR_CURRCOMPACT;
		gd.color_3 = 16;
		gd.color_2 = 215;
		gd.bg_line_color = 14;
		gd.num_dataset = NUM_CARGO;
		gd.num_on_x_axis = 20;
		gd.num_vert_lines = 20;
		gd.month = 0xFF;
		gd.unk61A = 10;
		gd.unk61C = 10;

		for (i = 0; i != NUM_CARGO; i++) {
			/* Since the buttons have no text, no images,
			 * both the text and the colored box have to be manually painted.
			 * clk_dif will move one pixel down and one pixel to the right
			 * when the button is clicked */
			byte clk_dif = IsWindowWidgetLowered(w, i + 3) ? 1 : 0;

			GfxFillRect(x + clk_dif, y + clk_dif, x + 8 + clk_dif, y + 5 + clk_dif, 0);
			GfxFillRect(x + 1 + clk_dif, y + 1 + clk_dif, x + 7 + clk_dif, y + 4 + clk_dif, _cargo_colours[i]);
			SetDParam(0, _cargoc.names_s[i]);
			DrawString(x + 14 + clk_dif, y + clk_dif, STR_7065, 0);
			y += 8;
			gd.colors[i] = _cargo_colours[i];
			for (j = 0; j != 20; j++) {
				gd.cost[i][j] = (uint64)GetTransportedGoodsIncome(10, 20, j * 6 + 6, i);
			}
		}

		DrawGraph(&gd);

		DrawString(2 + 46, 24 + gd.height + 7, STR_7062_DAYS_IN_TRANSIT, 0);
		DrawString(2 + 84, 24 - 9, STR_7063_PAYMENT_FOR_DELIVERING, 0);
	} break;

	case WE_CLICK: {
		switch (e->we.click.widget) {
		case 3: case 4: case 5: case 6:
		case 7: case 8: case 9: case 10:
		case 11: case 12: case 13: case 14:
			TOGGLEBIT(_legend_cargobits, e->we.click.widget - 3);
			ToggleWidgetLoweredState(w, e->we.click.widget);
			SetWindowDirty(w);
			break;
		}
	} break;
	}
}

static const Widget _cargo_payment_rates_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,    14,     0,    10,     0,    13, STR_00C5,                     STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,    14,    11,   567,     0,    13, STR_7061_CARGO_PAYMENT_RATES, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,    14,     0,   567,    14,   141, 0x0,                          STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    24,    31, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    32,    39, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    40,    47, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    48,    55, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    56,    63, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    64,    71, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    72,    79, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    80,    87, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    88,    95, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,    96,   103, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,   104,   111, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{      WWT_PANEL,   RESIZE_NONE,    12,   493,   562,   112,   119, 0x0,                          STR_7064_TOGGLE_GRAPH_FOR_CARGO},
{   WIDGETS_END},
};

static const WindowDesc _cargo_payment_rates_desc = {
	WDP_AUTO, WDP_AUTO, 568, 142,
	WC_PAYMENT_RATES,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_cargo_payment_rates_widgets,
	CargoPaymentRatesWndProc
};


void ShowCargoPaymentRates(void)
{
	AllocateWindowDescFront(&_cargo_payment_rates_desc, 0);
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
			uint pl_num;
			uint i;

			DrawWindowWidgets(w);

			pl_num = 0;
			FOR_ALL_PLAYERS(p) if (p->is_active) plist[pl_num++] = p;

			qsort((void*)plist, pl_num, sizeof(*plist), PerfHistComp);

			for (i = 0; i != pl_num; i++) {
				p = plist[i];
				SetDParam(0, i + STR_01AC_1ST);
				SetDParam(1, p->name_1);
				SetDParam(2, p->name_2);
				SetDParam(3, GetPlayerNameString(p->index, 4));
				SetDParam(5, GetPerformanceTitleFromValue(p->old_economy[1].performance_history));

				DrawString(2, 15 + i * 10, i == 0 ? STR_7054 : STR_7055, 0);
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
	WDP_AUTO, WDP_AUTO, 400, 97,
	WC_COMPANY_LEAGUE,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_company_league_widgets,
	CompanyLeagueWndProc
};

void ShowCompanyLeagueTable(void)
{
	AllocateWindowDescFront(&_company_league_desc,0);
}

/*****************************/
/* PERFORMANCE RATING DETAIL */
/*****************************/

static void PerformanceRatingDetailWndProc(Window *w, WindowEvent *e)
{
	static PlayerID _performance_rating_detail_player = PLAYER_FIRST;

	switch (e->event) {
		case WE_PAINT: {
			byte x;
			uint16 y = 14;
			int total_score = 0;
			int color_done, color_notdone;

			// Draw standard stuff
			DrawWindowWidgets(w);

			// Paint the player icons
			for (PlayerID i = PLAYER_FIRST; i < MAX_PLAYERS; i++) {
				if (!GetPlayer(i)->is_active) {
					// Check if we have the player as an active player
					if (!IsWindowWidgetDisabled(w, i + 13)) {
						// Bah, player gone :(
						DisableWindowWidget(w, i + 13);
						// Is this player selected? If so, select first player (always save? :s)
						if (IsWindowWidgetLowered(w, i + 13)) {
							RaiseWindowWidget(w, i + 13);
							LowerWindowWidget(w, 13);
							_performance_rating_detail_player = PLAYER_FIRST;
						}
						// We need a repaint
						SetWindowDirty(w);
					}
				continue;
				}

				// Check if we have the player marked as inactive
				if (IsWindowWidgetDisabled(w, i + 13)) {
					// New player! Yippie :p
					EnableWindowWidget(w, i + 13);
					// We need a repaint
					SetWindowDirty(w);
				}

				x = (i == _performance_rating_detail_player) ? 1 : 0;
				DrawPlayerIcon(i, i * 37 + 13 + x, 16 + x);
			}

			// The colors used to show how the progress is going
			color_done = _colour_gradient[COLOUR_GREEN][4];
			color_notdone = _colour_gradient[COLOUR_RED][4];

			// Draw all the score parts
			for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) {
				int val    = _score_part[_performance_rating_detail_player][i];
				int needed = _score_info[i].needed;
				int score  = _score_info[i].score;

				y += 20;
				// SCORE_TOTAL has his own rulez ;)
				if (i == SCORE_TOTAL) {
					needed = total_score;
					score = SCORE_MAX;
				} else {
					total_score += score;
				}

				DrawString(7, y, STR_PERFORMANCE_DETAIL_VEHICLES + i, 0);

				// Draw the score
				SetDParam(0, score);
				DrawStringRightAligned(107, y, SET_PERFORMANCE_DETAIL_INT, 0);

				// Calculate the %-bar
				if (val > needed) {
					x = 50;
				} else if (val == 0) {
					x = 0;
				} else {
					x = val * 50 / needed;
				}

				// SCORE_LOAN is inversed
				if (val < 0 && i == SCORE_LOAN) x = 0;

				// Draw the bar
				if (x !=  0) GfxFillRect(112,     y - 2, 112 + x,  y + 10, color_done);
				if (x != 50) GfxFillRect(112 + x, y - 2, 112 + 50, y + 10, color_notdone);

				// Calculate the %
				x = (val <= needed) ? val * 100 / needed : 100;

				// SCORE_LOAN is inversed
				if (val < 0 && i == SCORE_LOAN) x = 0;

				// Draw it
				SetDParam(0, x);
				DrawStringCentered(137, y, STR_PERFORMANCE_DETAIL_PERCENT, 0);

				// SCORE_LOAN is inversed
				if (i == SCORE_LOAN) val = needed - val;

				// Draw the amount we have against what is needed
				//  For some of them it is in currency format
				SetDParam(0, val);
				SetDParam(1, needed);
				switch (i) {
					case SCORE_MIN_PROFIT:
					case SCORE_MIN_INCOME:
					case SCORE_MAX_INCOME:
					case SCORE_MONEY:
					case SCORE_LOAN:
						DrawString(167, y, STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY, 0);
						break;
					default:
						DrawString(167, y, STR_PERFORMANCE_DETAIL_AMOUNT_INT, 0);
				}
			}

			break;
		}

		case WE_CLICK:
			// Check which button is clicked
			if (IS_INT_INSIDE(e->we.click.widget, 13, 21)) {
				// Is it no on disable?
				if (!IsWindowWidgetDisabled(w, e->we.click.widget)) {
					RaiseWindowWidget(w, _performance_rating_detail_player + 13);
					_performance_rating_detail_player = (PlayerID)(e->we.click.widget - 13);
					LowerWindowWidget(w, _performance_rating_detail_player + 13);
					SetWindowDirty(w);
				}
			}
			break;

		case WE_CREATE: {
			PlayerID i;
			Player *p2;

			/* Disable the players who are not active */
			for (i = PLAYER_FIRST; i < MAX_PLAYERS; i++) {
				SetWindowWidgetDisabledState(w, i + 13, !GetPlayer(i)->is_active);
			}
			/* Update all player stats with the current data
			 * (this is because _score_info is not saved to a savegame) */
			FOR_ALL_PLAYERS(p2) {
				if (p2->is_active) UpdateCompanyRatingAndValue(p2, false);
			}

			w->custom[0] = DAY_TICKS;
			w->custom[1] = 5;

			_performance_rating_detail_player = PLAYER_FIRST;
			LowerWindowWidget(w, _performance_rating_detail_player + 13);
			SetWindowDirty(w);

			break;
		}

		case WE_TICK: {
			// Update the player score every 5 days
			if (--w->custom[0] == 0) {
				w->custom[0] = DAY_TICKS;
				if (--w->custom[1] == 0) {
					Player *p2;

					w->custom[1] = 5;
					FOR_ALL_PLAYERS(p2) {
						// Skip if player is not active
						if (p2->is_active) UpdateCompanyRatingAndValue(p2, false);
					}
					SetWindowDirty(w);
				}
			}

			break;
		}
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
	WDP_AUTO, WDP_AUTO, 299, 228,
	WC_PERFORMANCE_DETAIL,0,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_performance_rating_detail_widgets,
	PerformanceRatingDetailWndProc
};

void ShowPerformanceRatingDetail(void)
{
	AllocateWindowDescFront(&_performance_rating_detail_desc, 0);
}
