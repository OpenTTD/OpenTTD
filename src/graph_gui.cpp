/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "table/sprites.h"

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
	GraphLegendWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);

		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (!HasBit(_legend_excluded_companies, c)) this->LowerWidget(c + GLW_FIRST_COMPANY);

			this->OnInvalidateData(c);
		}
	}

	virtual void OnPaint()
	{
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (!IsInsideMM(widget, GLW_FIRST_COMPANY, MAX_COMPANIES + GLW_FIRST_COMPANY)) return;

		CompanyID cid = (CompanyID)(widget - GLW_FIRST_COMPANY);

		if (!Company::IsValidID(cid)) return;

		bool rtl = _dynlang.text_dir == TD_RTL;

		DrawCompanyIcon(cid, rtl ? r.right - 16 : r.left + 2, r.top + 2);

		SetDParam(0, cid);
		SetDParam(1, cid);
		DrawString(r.left + (rtl ? WD_FRAMERECT_LEFT : 19), r.right - (rtl ? 19 : WD_FRAMERECT_RIGHT), r.top + 1, STR_COMPANY_NAME_COMPANY_NUM, HasBit(_legend_excluded_companies, cid) ? TC_BLACK : TC_WHITE);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (!IsInsideMM(widget, GLW_FIRST_COMPANY, MAX_COMPANIES + GLW_FIRST_COMPANY)) return;

		ToggleBit(_legend_excluded_companies, widget - GLW_FIRST_COMPANY);
		this->ToggleWidgetLoweredState(widget);
		this->SetDirty();
		SetWindowDirty(WC_INCOME_GRAPH, 0);
		SetWindowDirty(WC_OPERATING_PROFIT, 0);
		SetWindowDirty(WC_DELIVERED_CARGO, 0);
		SetWindowDirty(WC_PERFORMANCE_HISTORY, 0);
		SetWindowDirty(WC_COMPANY_VALUE, 0);
	}

	virtual void OnInvalidateData(int data)
	{
		if (Company::IsValidID(data)) return;

		SetBit(_legend_excluded_companies, data);
		this->RaiseWidget(data + GLW_FIRST_COMPANY);
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
		panel->SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP);
		vert->Add(panel);
	}
	*biggest_index = GLW_LAST_COMPANY;
	return vert;
}

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
	WDP_AUTO, WDP_AUTO, 250, 196,
	WC_GRAPH_LEGEND, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_nested_graph_legend_widgets, lengthof(_nested_graph_legend_widgets)
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
		GRAPH_NUM_MONTHS = 24, ///< Number of months displayed in the graph.

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

	int graph_widget;
	StringID format_str_y_axis;
	byte colours[GRAPH_MAX_DATASETS];
	OverflowSafeInt64 cost[GRAPH_MAX_DATASETS][GRAPH_NUM_MONTHS]; ///< Stored costs for the last #GRAPH_NUM_MONTHS months

	int64 GetHighestValue(int initial_highest_value) const
	{
		OverflowSafeInt64 highest_value = initial_highest_value;

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

		return highest_value;
	}

	uint GetYLabelWidth(int64 highest_value) const
	{
		/* draw text strings on the y axis */
		int64 y_label = highest_value;
		int64 y_label_separation = highest_value / (GRAPH_NUM_LINES_Y - 1);

		/* If there are negative values, the graph goes from highest_value to
		 * -highest_value, not highest_value to 0. */
		if (this->has_negative_values) y_label_separation *= 2;

		uint max_width = 0;

		for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
			SetDParam(0, this->format_str_y_axis);
			SetDParam(1, y_label);
			Dimension d = GetStringBoundingBox(STR_GRAPH_Y_LABEL);
			if (d.width > max_width) max_width = d.width;

			y_label -= y_label_separation;
		}

		return max_width;
	}

	/**
	 * Actually draw the graph.
	 * @param r the rectangle of the data field of the graph
	 */
	void DrawGraph(Rect &r) const
	{
		uint x, y;                       ///< Reused whenever x and y coordinates are needed.
		OverflowSafeInt64 highest_value; ///< Highest value to be drawn.
		int x_axis_offset;               ///< Distance from the top of the graph to the x axis.

		/* the colours and cost array of GraphDrawer must accomodate
		 * both values for cargo and companies. So if any are higher, quit */
		assert_compile(GRAPH_MAX_DATASETS >= (int)NUM_CARGO && GRAPH_MAX_DATASETS >= (int)MAX_COMPANIES);
		assert(this->num_vert_lines > 0);

		byte grid_colour = _colour_gradient[COLOUR_GREY][4];

		/* Rect r will be adjusted to contain just the graph, with labels being
		 * placed outside the area. */
		r.top    += 5 + GetCharacterHeight(FS_SMALL) / 2;
		r.bottom -= (this->month == 0xFF ? 1 : 3) * GetCharacterHeight(FS_SMALL) + 4;
		r.left   += 9;
		r.right  -= 5;

		/* Start of with a highest_value of twice the height of the graph in pixels.
		 * It's a bit arbitrary, but it makes the cargo payment graph look a little
		 * nicer, and prevents division by zero when calculating where the datapoint
		 * should be drawn. */
		highest_value = r.bottom - r.top + 1;
		if (!this->has_negative_values) highest_value *= 2;
		highest_value = GetHighestValue(highest_value);

		/* Get width for Y labels */
		int label_width = GetYLabelWidth(highest_value);

		r.left += label_width;

		int x_sep = (r.right - r.left) / this->num_vert_lines;
		int y_sep = (r.bottom - r.top) / (GRAPH_NUM_LINES_Y - 1);

		/* Redetermine right and bottom edge of graph to fit with the integer
		 * separation values. */
		r.right = r.left + x_sep * this->num_vert_lines;
		r.bottom = r.top + y_sep * (GRAPH_NUM_LINES_Y - 1);

		/* Where to draw the X axis */
		x_axis_offset = r.bottom - r.top;
		if (this->has_negative_values) x_axis_offset /= 2;

		/* Draw the vertical grid lines. */

		/* Don't draw the first line, as that's where the axis will be. */
		x = r.left + x_sep;

		for (int i = 0; i < this->num_vert_lines; i++) {
			GfxFillRect(x, r.top, x, r.bottom, grid_colour);
			x += x_sep;
		}

		/* Draw the horizontal grid lines. */
		y = r.bottom;

		for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
			GfxFillRect(r.left - 3, y, r.left - 1, y, GRAPH_AXIS_LINE_COLOUR);
			GfxFillRect(r.left, y, r.right, y, grid_colour);
			y -= y_sep;
		}

		/* Draw the y axis. */
		GfxFillRect(r.left, r.top, r.left, r.bottom, GRAPH_AXIS_LINE_COLOUR);

		/* Draw the x axis. */
		y = x_axis_offset + r.top;
		GfxFillRect(r.left, y, r.right, y, GRAPH_AXIS_LINE_COLOUR);

		/* Find the largest value that will be drawn. */
		if (this->num_on_x_axis == 0)
			return;

		assert(this->num_on_x_axis > 0);
		assert(this->num_dataset > 0);

		/* draw text strings on the y axis */
		int64 y_label = highest_value;
		int64 y_label_separation = highest_value / (GRAPH_NUM_LINES_Y - 1);

		/* If there are negative values, the graph goes from highest_value to
		 * -highest_value, not highest_value to 0. */
		if (this->has_negative_values) y_label_separation *= 2;

		y = r.top - GetCharacterHeight(FS_SMALL) / 2;

		for (int i = 0; i < GRAPH_NUM_LINES_Y; i++) {
			SetDParam(0, this->format_str_y_axis);
			SetDParam(1, y_label);
			DrawString(r.left - label_width - 4, r.left - 4, y, STR_GRAPH_Y_LABEL, graph_axis_label_colour, SA_RIGHT);

			y_label -= y_label_separation;
			y += y_sep;
		}

		/* draw strings on the x axis */
		if (this->month != 0xFF) {
			x = r.left;
			y = r.bottom + 2;
			byte month = this->month;
			Year year  = this->year;
			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, month + STR_MONTH_ABBREV_JAN);
				SetDParam(1, month + STR_MONTH_ABBREV_JAN + 2);
				SetDParam(2, year);
				DrawStringMultiLine(x, x + x_sep, y, this->height, month == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH, graph_axis_label_colour);

				month += 3;
				if (month >= 12) {
					month = 0;
					year++;
				}
				x += x_sep;
			}
		} else {
			/* Draw the label under the data point rather than on the grid line. */
			x = r.left;
			y = r.bottom + 2;
			uint16 label = this->x_values_start;

			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, label);
				DrawString(x + 1, x + x_sep - 1, y, STR_GRAPH_Y_LABEL_NUMBER, graph_axis_label_colour, SA_CENTER);

				label += this->x_values_increment;
				x += x_sep;
			}
		}

		/* draw lines and dots */
		for (int i = 0; i < this->num_dataset; i++) {
			if (!HasBit(this->excluded_data, i)) {
				/* Centre the dot between the grid lines. */
				x = r.left + (x_sep / 2);

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

						y = r.top + x_axis_offset - (x_axis_offset * datapoint) / (highest_value >> reduce_range);

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

					x += x_sep;
				}
			}
		}
	}


	BaseGraphWindow(int widget, bool has_negative_values, StringID format_str_y_axis) :
			Window(), has_negative_values(has_negative_values),
			format_str_y_axis(format_str_y_axis)
	{
		SetWindowDirty(WC_GRAPH_LEGEND, 0);
		this->num_vert_lines = 24;
		this->graph_widget = widget;
	}

	void InitializeWindow(const WindowDesc *desc, WindowNumber number)
	{
		this->InitNested(desc, number);

		/* Initialise the dataset */
		this->UpdateStatistics(true);
	}

public:
	virtual void OnPaint()
	{
		this->DrawWidgets();

		NWidgetBase *nwid = this->GetWidget<NWidgetBase>(this->graph_widget);
		Rect r;
		r.left = nwid->pos_x;
		r.right = nwid->pos_x + nwid->current_x - 1;
		r.top = nwid->pos_y;
		r.bottom = nwid->pos_y + nwid->current_y - 1;

		this->DrawGraph(r);
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

	virtual void OnTick()
	{
		this->UpdateStatistics(false);
	}

	/**
	 * Update the statistics.
	 * @param initialize Initialize the data structure.
	 */
	void UpdateStatistics(bool initialize)
	{
		uint excluded_companies = _legend_excluded_companies;

		/* Exclude the companies which aren't valid */
		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (!Company::IsValidID(c)) SetBit(excluded_companies, c);
		}

		byte nums = 0;
		const Company *c;
		FOR_ALL_COMPANIES(c) {
			nums = min(this->num_vert_lines, max(nums, c->num_valid_stat_ent));
		}

		int mo = (_cur_month / 3 - nums) * 3;
		int yr = _cur_year;
		while (mo < 0) {
			yr--;
			mo += 12;
		}

		if (!initialize && this->excluded_data == excluded_companies && this->num_on_x_axis == nums &&
				this->year == yr && this->month == mo) {
			/* There's no reason to get new stats */
			return;
		}

		this->excluded_data = excluded_companies;
		this->num_on_x_axis = nums;
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
	}
};


/********************/
/* OPERATING PROFIT */
/********************/

struct OperatingProfitGraphWindow : BaseGraphWindow {
	OperatingProfitGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(BGW_BACKGROUND, true, STR_JUST_CURRCOMPACT)
	{
		this->InitializeWindow(desc, window_number);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].income + c->old_economy[j].expenses;
	}
};

static const NWidgetPart _nested_operating_profit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_OPERATING_PROFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 160), EndContainer(),
};

static const WindowDesc _operating_profit_desc(
	WDP_AUTO, WDP_AUTO, 576, 174,
	WC_OPERATING_PROFIT, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_operating_profit_widgets, lengthof(_nested_operating_profit_widgets)
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
			BaseGraphWindow(BGW_BACKGROUND, false, STR_JUST_CURRCOMPACT)
	{
		this->InitializeWindow(desc, window_number);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].income;
	}
};

static const NWidgetPart _nested_income_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_INCOME_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 128), EndContainer(),
};


static const WindowDesc _income_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 142,
	WC_INCOME_GRAPH, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_income_graph_widgets, lengthof(_nested_income_graph_widgets)
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
			BaseGraphWindow(BGW_BACKGROUND, false, STR_JUST_COMMA)
	{
		this->InitializeWindow(desc, window_number);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].delivered_cargo;
	}
};

static const NWidgetPart _nested_delivered_cargo_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_CARGO_DELIVERED_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 128), EndContainer(),
};

static const WindowDesc _delivered_cargo_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 142,
	WC_DELIVERED_CARGO, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_delivered_cargo_graph_widgets, lengthof(_nested_delivered_cargo_graph_widgets)
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
			BaseGraphWindow(PHW_BACKGROUND, false, STR_JUST_COMMA)
	{
		this->InitializeWindow(desc, window_number);
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

static const NWidgetPart _nested_performance_history_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, PHW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, PHW_CAPTION), SetDataTip(STR_GRAPH_COMPANY_PERFORMANCE_RATINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, PHW_DETAILED_PERFORMANCE), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_PERFORMANCE_DETAIL_KEY, STR_GRAPH_PERFORMANCE_DETAIL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, PHW_KEY), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, PHW_BACKGROUND), SetMinimalSize(576, 224), EndContainer(),
};

static const WindowDesc _performance_history_desc(
	WDP_AUTO, WDP_AUTO, 576, 238,
	WC_PERFORMANCE_HISTORY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_performance_history_widgets, lengthof(_nested_performance_history_widgets)
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
			BaseGraphWindow(BGW_BACKGROUND, false, STR_JUST_CURRCOMPACT)
	{
		this->InitializeWindow(desc, window_number);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *c, int j)
	{
		return c->old_economy[j].company_value;
	}
};

static const NWidgetPart _nested_company_value_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, BGW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, BGW_CAPTION), SetDataTip(STR_GRAPH_COMPANY_VALUES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, BGW_KEY_BUTTON), SetMinimalSize(50, 0), SetMinimalTextLines(1, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM + 2), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, BGW_BACKGROUND), SetMinimalSize(576, 224), EndContainer(),
};

static const WindowDesc _company_value_graph_desc(
	WDP_AUTO, WDP_AUTO, 576, 238,
	WC_COMPANY_VALUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_nested_company_value_graph_widgets, lengthof(_nested_company_value_graph_widgets)
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
	CPW_HEADER,
	CPW_GRAPH,
	CPW_FOOTER,
	CPW_CARGO_FIRST,
};

struct PaymentRatesGraphWindow : BaseGraphWindow {
	PaymentRatesGraphWindow(const WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(CPW_GRAPH, false, STR_JUST_CURRCOMPACT)
	{
		this->num_on_x_axis = 20;
		this->num_vert_lines = 20;
		this->month = 0xFF;
		this->x_values_start     = 10;
		this->x_values_increment = 10;

		/* Initialise the dataset */
		this->OnHundredthTick();

		this->InitNested(desc, window_number);
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		if (widget < CPW_CARGO_FIRST) return;

		const CargoSpec *cs = CargoSpec::Get(widget - CPW_CARGO_FIRST);
		SetDParam(0, cs->name);
		Dimension d = GetStringBoundingBox(STR_GRAPH_CARGO_PAYMENT_CARGO);
		d.width += 14; /* colour field */
		d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = maxdim(d, *size);
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget < CPW_CARGO_FIRST) return;

		const CargoSpec *cs = CargoSpec::Get(widget - CPW_CARGO_FIRST);
		bool rtl = _dynlang.text_dir == TD_RTL;

		/* Since the buttons have no text, no images,
		 * both the text and the coloured box have to be manually painted.
		 * clk_dif will move one pixel down and one pixel to the right
		 * when the button is clicked */
		byte clk_dif = this->IsWidgetLowered(widget) ? 1 : 0;
		int x = r.left + WD_FRAMERECT_LEFT;
		int y = r.top;

		int rect_x = clk_dif + (rtl ? r.right - 12 : r.left + WD_FRAMERECT_LEFT);

		GfxFillRect(rect_x, y + clk_dif, rect_x + 8, y + 5 + clk_dif, 0);
		GfxFillRect(rect_x + 1, y + 1 + clk_dif, rect_x + 7, y + 4 + clk_dif, cs->legend_colour);
		SetDParam(0, cs->name);
		DrawString(rtl ? r.left : x + 14 + clk_dif, (rtl ? r.right - 14 + clk_dif : r.right), y + clk_dif, STR_GRAPH_CARGO_PAYMENT_CARGO);
	}

	virtual void OnClick(Point pt, int widget)
	{
		if (widget >= CPW_CARGO_FIRST) {
			int i = 0;
			const CargoSpec *cs;
			FOR_ALL_CARGOSPECS(cs) {
				if (cs->Index() + CPW_CARGO_FIRST == widget) break;
				i++;
			}

			ToggleBit(_legend_excluded_cargo, i);
			this->ToggleWidgetLoweredState(widget);
			this->excluded_data = _legend_excluded_cargo;
			this->SetDirty();
		}
	}

	virtual void OnTick()
	{
		/* Override default OnTick */
	}

	virtual void OnHundredthTick()
	{
		this->excluded_data = _legend_excluded_cargo;

		int i = 0;
		const CargoSpec *cs;
		FOR_ALL_CARGOSPECS(cs) {
			this->colours[i] = cs->legend_colour;
			for (uint j = 0; j != 20; j++) {
				this->cost[i][j] = GetTransportedGoodsIncome(10, 20, j * 4 + 4, cs->Index());
			}

			i++;
		}
		this->num_dataset = i;
	}
};

/** Construct the row containing the digit keys. */
static NWidgetBase *MakeCargoButtons(int *biggest_index)
{
	NWidgetVertical *ver = new NWidgetVertical;

	const CargoSpec *cs;
	FOR_ALL_CARGOSPECS(cs) {
		*biggest_index = CPW_CARGO_FIRST + cs->Index();
		NWidgetBackground *leaf = new NWidgetBackground(WWT_PANEL, COLOUR_ORANGE, *biggest_index, NULL);
		leaf->tool_tip = STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO;
		leaf->SetFill(true, false);
		leaf->SetLowered(true);
		ver->Add(leaf);
	}
	return ver;
}


static const NWidgetPart _nested_cargo_payment_rates_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CPW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, CPW_CAPTION), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CPW_BACKGROUND), SetMinimalSize(568, 128), SetResize(0, 1),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(true, false),
				NWidget(WWT_TEXT, COLOUR_GREY, CPW_HEADER), SetMinimalSize(0, 6), SetPadding(2, 0, 2, 0), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_TITLE, STR_NULL),
				NWidget(NWID_SPACER), SetFill(true, false),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_EMPTY, COLOUR_GREY, CPW_GRAPH), SetMinimalSize(495, 0), SetFill(true, true),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(false, false),
						NWidgetFunction(MakeCargoButtons),
					NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(false, true),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(false, true),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetFill(true, false),
				NWidget(WWT_TEXT, COLOUR_GREY, CPW_FOOTER), SetMinimalSize(0, 6), SetPadding(2, 0, 2, 0), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_X_LABEL, STR_NULL),
				NWidget(NWID_SPACER), SetFill(true, false),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static const WindowDesc _cargo_payment_rates_desc(
	WDP_AUTO, WDP_AUTO, 568, 46,
	WC_PAYMENT_RATES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_nested_cargo_payment_rates_widgets, lengthof(_nested_cargo_payment_rates_widgets)
);


void ShowCargoPaymentRates()
{
	AllocateWindowDescFront<PaymentRatesGraphWindow>(&_cargo_payment_rates_desc, 0);
}

/************************/
/* COMPANY LEAGUE TABLE */
/************************/

/** Widget numbers for the company league window. */
enum CompanyLeagueWidgets {
	CLW_CLOSEBOX,
	CLW_CAPTION,
	CLW_STICKYBOX,
	CLW_BACKGROUND,
};

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
	CompanyLeagueWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->InitNested(desc, window_number);
		this->companies.ForceRebuild();
		this->companies.NeedResort();
	}

	virtual void OnPaint()
	{
		this->BuildCompanyList();
		this->companies.Sort(&PerformanceSorter);

		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != CLW_BACKGROUND) return;

		uint y = r.top + WD_FRAMERECT_TOP;
		for (uint i = 0; i != this->companies.Length(); i++) {
			const Company *c = this->companies[i];
			SetDParam(0, i + STR_ORDINAL_NUMBER_1ST);
			SetDParam(1, c->index);
			SetDParam(2, c->index);
			SetDParam(3, GetPerformanceTitleFromValue(c->old_economy[1].performance_history));

			DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, i == 0 ? STR_COMPANY_LEAGUE_FIRST : STR_COMPANY_LEAGUE_OTHER);
			DrawCompanyIcon(c->index, _dynlang.text_dir == TD_RTL ? r.right - 43 : r.left + 27, y + 1);
			y += FONT_HEIGHT_NORMAL;
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

static const NWidgetPart _nested_company_league_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY, CLW_CLOSEBOX),
		NWidget(WWT_CAPTION, COLOUR_GREY, CLW_CAPTION), SetDataTip(STR_COMPANY_LEAGUE_TABLE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY, CLW_STICKYBOX),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY, CLW_BACKGROUND), SetMinimalSize(400, 0), SetMinimalTextLines(15, WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM),
};

static const WindowDesc _company_league_desc(
	WDP_AUTO, WDP_AUTO, 400, 167,
	WC_COMPANY_LEAGUE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_STICKY_BUTTON,
	_nested_company_league_widgets, lengthof(_nested_company_league_widgets)
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

	PerformanceRatingDetailWindow(const WindowDesc *desc, WindowNumber window_number) : Window()
	{
		this->UpdateCompanyStats();

		this->InitNested(desc, window_number);
		this->OnInvalidateData(INVALID_COMPANY);
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

	uint score_info_left;
	uint score_info_right;
	uint bar_left;
	uint bar_right;
	uint bar_width;
	uint bar_height;
	uint score_detail_left;
	uint score_detail_right;

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *resize)
	{
		switch (widget) {
			case PRW_SCORE_FIRST:
				this->bar_height = FONT_HEIGHT_NORMAL + 4;
				size->height = this->bar_height + 2 * WD_MATRIX_TOP;

				uint score_info_width = 0;
				for (uint i = SCORE_BEGIN; i < SCORE_END; i++) {
					score_info_width = max(score_info_width, GetStringBoundingBox(STR_PERFORMANCE_DETAIL_VEHICLES + i).width);
				}
				SetDParam(0, 1000);
				score_info_width += GetStringBoundingBox(STR_BLACK_COMMA).width + WD_FRAMERECT_LEFT;

				SetDParam(0, 100);
				this->bar_width = GetStringBoundingBox(STR_PERFORMANCE_DETAIL_PERCENT).width + 20; // Wide bars!

				/* At this number we are roughly at the max; it can become wider,
				 * but then you need at 1000 times more money. At that time you're
				 * not that interested anymore in the last few digits anyway. */
				uint max = 999999999; // nine 9s
				SetDParam(0, max);
				SetDParam(1, max);
				uint score_detail_width = GetStringBoundingBox(STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY).width;

				size->width = 7 + score_info_width + 5 + this->bar_width + 5 + score_detail_width + 7;
				uint left  = 7;
				uint right = size->width - 7;

				bool rtl = _dynlang.text_dir == TD_RTL;
				this->score_info_left  = rtl ? right - score_info_width : left;
				this->score_info_right = rtl ? right : left + score_info_width;

				this->score_detail_left  = rtl ? left : right - score_detail_width;
				this->score_detail_right = rtl ? left + score_detail_width : right;

				this->bar_left  = left + (rtl ? score_detail_width : score_info_width) + 5;
				this->bar_right = this->bar_left + this->bar_width;
				break;
		}
	}

	virtual void OnPaint()
	{
		/* Draw standard stuff */
		this->DrawWidgets();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		/* No need to draw when there's nothing to draw */
		if (this->company == INVALID_COMPANY) return;

		if (IsInsideMM(widget, PRW_COMPANY_FIRST, PRW_COMPANY_LAST + 1)) {
			if (this->IsWidgetDisabled(widget)) return;
			CompanyID cid = (CompanyID)(widget - PRW_COMPANY_FIRST);
			int offset = (cid == this->company) ? 1 : 0;
			Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
			DrawCompanyIcon(cid, (r.left + r.right - sprite_size.width) / 2 + offset, (r.top + r.bottom - sprite_size.height) / 2 + offset);
			return;
		}

		if (!IsInsideMM(widget, PRW_SCORE_FIRST, PRW_SCORE_LAST + 1)) return;

		ScoreID score_type = (ScoreID)(widget - PRW_SCORE_FIRST);

			/* The colours used to show how the progress is going */
		int colour_done = _colour_gradient[COLOUR_GREEN][4];
		int colour_notdone = _colour_gradient[COLOUR_RED][4];

		/* Draw all the score parts */
		int val    = _score_part[company][score_type];
		int needed = _score_info[score_type].needed;
		int score  = _score_info[score_type].score;

		/* SCORE_TOTAL has his own rules ;) */
		if (score_type == SCORE_TOTAL) {
			for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) score += _score_info[i].score;
			needed = SCORE_MAX;
		}

		uint bar_top  = r.top + WD_MATRIX_TOP;
		uint text_top = bar_top + 2;

		DrawString(this->score_info_left, this->score_info_right, text_top, STR_PERFORMANCE_DETAIL_VEHICLES + score_type);

		/* Draw the score */
		SetDParam(0, score);
		DrawString(this->score_info_left, this->score_info_right, text_top, STR_BLACK_COMMA, TC_FROMSTRING, SA_RIGHT);

		/* Calculate the %-bar */
		uint x = Clamp(val, 0, needed) * this->bar_width / needed;
		bool rtl = _dynlang.text_dir == TD_RTL;
		if (rtl) {
			x = this->bar_right - x;
		} else {
			x = this->bar_left + x;
		}

		/* Draw the bar */
		if (x != this->bar_left)  GfxFillRect(this->bar_left, bar_top, x, bar_top + this->bar_height, rtl ? colour_notdone : colour_done);
		if (x != this->bar_right) GfxFillRect(x, bar_top, this->bar_right, bar_top + this->bar_height, rtl ? colour_done : colour_notdone);

		/* Draw it */
		SetDParam(0, Clamp(val, 0, needed) * 100 / needed);
		DrawString(this->bar_left, this->bar_right, text_top, STR_PERFORMANCE_DETAIL_PERCENT, TC_FROMSTRING, SA_CENTER);

		/* SCORE_LOAN is inversed */
		if (score_type == SCORE_LOAN) val = needed - val;

		/* Draw the amount we have against what is needed
		 * For some of them it is in currency format */
		SetDParam(0, val);
		SetDParam(1, needed);
		switch (score_type) {
			case SCORE_MIN_PROFIT:
			case SCORE_MIN_INCOME:
			case SCORE_MAX_INCOME:
			case SCORE_MONEY:
			case SCORE_LOAN:
				DrawString(this->score_detail_left, this->score_detail_right, text_top, STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY);
				break;
			default:
				DrawString(this->score_detail_left, this->score_detail_right, text_top, STR_PERFORMANCE_DETAIL_AMOUNT_INT);
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, PRW_COMPANY_FIRST, PRW_COMPANY_LAST + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				this->RaiseWidget(this->company + PRW_COMPANY_FIRST);
				this->company = (CompanyID)(widget - PRW_COMPANY_FIRST);
				this->LowerWidget(this->company + PRW_COMPANY_FIRST);
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

	/**
	 * Invalidate the data of this window.
	 * @param data the company ID of the company that is going to be removed
	 */
	virtual void OnInvalidateData(int data)
	{
		/* Disable the companies who are not active */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + PRW_COMPANY_FIRST, !Company::IsValidID(i));
		}

		/* Check if the currently selected company is still active. */
		if (this->company != INVALID_COMPANY && !Company::IsValidID(this->company)) {
			/* Raise the widget for the previous selection. */
			this->RaiseWidget(this->company + PRW_COMPANY_FIRST);
			this->company = INVALID_COMPANY;
		}

		if (this->company == INVALID_COMPANY) {
			const Company *c;
			FOR_ALL_COMPANIES(c) {
				this->company = c->index;
				break;
			}
		}

		/* Make sure the widget is lowered */
		this->LowerWidget(this->company + PRW_COMPANY_FIRST);
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
		STR_PERFORMANCE_DETAIL_VEHICLES_TOOLTIP,
		STR_PERFORMANCE_DETAIL_STATIONS_TOOLTIP,
		STR_PERFORMANCE_DETAIL_MIN_PROFIT_TOOLTIP,
		STR_PERFORMANCE_DETAIL_MIN_INCOME_TOOLTIP,
		STR_PERFORMANCE_DETAIL_MAX_INCOME_TOOLTIP,
		STR_PERFORMANCE_DETAIL_DELIVERED_TOOLTIP,
		STR_PERFORMANCE_DETAIL_CARGO_TOOLTIP,
		STR_PERFORMANCE_DETAIL_MONEY_TOOLTIP,
		STR_PERFORMANCE_DETAIL_LOAN_TOOLTIP,
		STR_PERFORMANCE_DETAIL_TOTAL_TOOLTIP,
	};

	assert_compile(lengthof(performance_tips) == SCORE_END - SCORE_BEGIN);

	NWidgetVertical *vert = new NWidgetVertical(NC_EQUALSIZE);
	for (int widnum = PRW_SCORE_FIRST; widnum <= PRW_SCORE_LAST; widnum++) {
		NWidgetBackground *panel = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, widnum);
		panel->SetFill(true, true);
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

	Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
	sprite_size.width  += WD_MATRIX_LEFT + WD_MATRIX_RIGHT;
	sprite_size.height += WD_MATRIX_TOP + WD_MATRIX_BOTTOM + 1; // 1 for the 'offset' of being pressed

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
		panel->SetMinimalSize(sprite_size.width, sprite_size.height);
		panel->SetFill(true, false);
		panel->SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP);
		hor->Add(panel);
		hor_length++;
	}
	*biggest_index = PRW_COMPANY_LAST;
	if (vert == NULL) return hor; // All buttons fit in a single row.

	if (hor_length > 0 && hor_length < MAX_LENGTH) {
		/* Last row is partial, add a spacer at the end to force all buttons to the left. */
		NWidgetSpacer *spc = new NWidgetSpacer(0, 0);
		spc->SetMinimalSize(sprite_size.width, sprite_size.height);
		spc->SetFill(true, false);
		hor->Add(spc);
	}
	if (hor != NULL) vert->Add(hor);
	return vert;
}

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
	WDP_AUTO, WDP_AUTO, 299, 241,
	WC_PERFORMANCE_DETAIL, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_nested_performance_rating_detail_widgets, lengthof(_nested_performance_rating_detail_widgets)
);

void ShowPerformanceRatingDetail()
{
	AllocateWindowDescFront<PerformanceRatingDetailWindow>(&_performance_rating_detail_desc, 0);
}
