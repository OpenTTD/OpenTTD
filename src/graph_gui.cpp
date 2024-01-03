/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file graph_gui.cpp GUI that shows performance graphs. */

#include "stdafx.h"
#include "graph_gui.h"
#include "window_gui.h"
#include "company_base.h"
#include "company_gui.h"
#include "economy_func.h"
#include "cargotype.h"
#include "strings_func.h"
#include "window_func.h"
#include "gfx_func.h"
#include "core/geometry_func.hpp"
#include "currency.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_calendar.h"
#include "zoom_func.h"

#include "widgets/graph_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

/* Bitmasks of company and cargo indices that shouldn't be drawn. */
static CompanyMask _legend_excluded_companies;
static CargoTypes _legend_excluded_cargo;

/* Apparently these don't play well with enums. */
static const OverflowSafeInt64 INVALID_DATAPOINT(INT64_MAX); // Value used for a datapoint that shouldn't be drawn.
static const uint INVALID_DATAPOINT_POS = UINT_MAX;  // Used to determine if the previous point was drawn.

/****************/
/* GRAPH LEGEND */
/****************/

struct GraphLegendWindow : Window {
	GraphLegendWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);

		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (!HasBit(_legend_excluded_companies, c)) this->LowerWidget(c + WID_GL_FIRST_COMPANY);

			this->OnInvalidateData(c);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (!IsInsideMM(widget, WID_GL_FIRST_COMPANY, MAX_COMPANIES + WID_GL_FIRST_COMPANY)) return;

		CompanyID cid = (CompanyID)(widget - WID_GL_FIRST_COMPANY);

		if (!Company::IsValidID(cid)) return;

		bool rtl = _current_text_dir == TD_RTL;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		Dimension d = GetSpriteSize(SPR_COMPANY_ICON);
		DrawCompanyIcon(cid, rtl ? ir.right - d.width : ir.left, CenterBounds(ir.top, ir.bottom, d.height));

		const Rect tr = ir.Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);
		SetDParam(0, cid);
		SetDParam(1, cid);
		DrawString(tr.left, tr.right, CenterBounds(tr.top, tr.bottom, GetCharacterHeight(FS_NORMAL)), STR_COMPANY_NAME_COMPANY_NUM, HasBit(_legend_excluded_companies, cid) ? TC_BLACK : TC_WHITE);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (!IsInsideMM(widget, WID_GL_FIRST_COMPANY, MAX_COMPANIES + WID_GL_FIRST_COMPANY)) return;

		ToggleBit(_legend_excluded_companies, widget - WID_GL_FIRST_COMPANY);
		this->ToggleWidgetLoweredState(widget);
		this->SetDirty();
		InvalidateWindowData(WC_INCOME_GRAPH, 0);
		InvalidateWindowData(WC_OPERATING_PROFIT, 0);
		InvalidateWindowData(WC_DELIVERED_CARGO, 0);
		InvalidateWindowData(WC_PERFORMANCE_HISTORY, 0);
		InvalidateWindowData(WC_COMPANY_VALUE, 0);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (Company::IsValidID(data)) return;

		SetBit(_legend_excluded_companies, data);
		this->RaiseWidget(data + WID_GL_FIRST_COMPANY);
	}
};

/**
 * Construct a vertical list of buttons, one for each company.
 * @return Panel with company buttons.
 */
static std::unique_ptr<NWidgetBase> MakeNWidgetCompanyLines()
{
	auto vert = std::make_unique<NWidgetVertical>(NC_EQUALSIZE);
	vert->SetPadding(2, 2, 2, 2);
	uint sprite_height = GetSpriteSize(SPR_COMPANY_ICON, nullptr, ZOOM_LVL_OUT_4X).height;

	for (WidgetID widnum = WID_GL_FIRST_COMPANY; widnum <= WID_GL_LAST_COMPANY; widnum++) {
		auto panel = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_BROWN, widnum);
		panel->SetMinimalSize(246, sprite_height + WidgetDimensions::unscaled.framerect.Vertical());
		panel->SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical(), FS_NORMAL);
		panel->SetFill(1, 1);
		panel->SetDataTip(0x0, STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP);
		vert->Add(std::move(panel));
	}
	return vert;
}

static const NWidgetPart _nested_graph_legend_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_KEY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GL_BACKGROUND),
		NWidgetFunction(MakeNWidgetCompanyLines),
	EndContainer(),
};

static WindowDesc _graph_legend_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_legend", 0, 0,
	WC_GRAPH_LEGEND, WC_NONE,
	0,
	std::begin(_nested_graph_legend_widgets), std::end(_nested_graph_legend_widgets)
);

static void ShowGraphLegend()
{
	AllocateWindowDescFront<GraphLegendWindow>(&_graph_legend_desc, 0);
}

/** Contains the interval of a graph's data. */
struct ValuesInterval {
	OverflowSafeInt64 highest; ///< Highest value of this interval. Must be zero or greater.
	OverflowSafeInt64 lowest;  ///< Lowest value of this interval. Must be zero or less.
};

/******************/
/* BASE OF GRAPHS */
/*****************/

struct BaseGraphWindow : Window {
protected:
	static const int GRAPH_MAX_DATASETS     =  64;
	static const int GRAPH_BASE_COLOUR      =  GREY_SCALE(2);
	static const int GRAPH_GRID_COLOUR      =  GREY_SCALE(3);
	static const int GRAPH_AXIS_LINE_COLOUR =  GREY_SCALE(1);
	static const int GRAPH_ZERO_LINE_COLOUR =  GREY_SCALE(8);
	static const int GRAPH_YEAR_LINE_COLOUR =  GREY_SCALE(5);
	static const int GRAPH_NUM_MONTHS       =  24; ///< Number of months displayed in the graph.

	static const TextColour GRAPH_AXIS_LABEL_COLOUR = TC_BLACK; ///< colour of the graph axis label.

	static const int MIN_GRAPH_NUM_LINES_Y  =   9; ///< Minimal number of horizontal lines to draw.
	static const int MIN_GRID_PIXEL_SIZE    =  20; ///< Minimum distance between graph lines.

	uint64_t excluded_data; ///< bitmask of the datasets that shouldn't be displayed.
	byte num_dataset;
	byte num_on_x_axis;
	byte num_vert_lines;

	/* The starting month and year that values are plotted against. */
	TimerGameCalendar::Month month;
	TimerGameCalendar::Year year;

	bool draw_dates = true; ///< Should we draw months and years on the time axis?

	/* These values are used if the graph is being plotted against values
	 * rather than the dates specified by month and year. */
	uint16_t x_values_start;
	uint16_t x_values_increment;

	WidgetID graph_widget;
	StringID format_str_y_axis;
	byte colours[GRAPH_MAX_DATASETS];
	OverflowSafeInt64 cost[GRAPH_MAX_DATASETS][GRAPH_NUM_MONTHS]; ///< Stored costs for the last #GRAPH_NUM_MONTHS months

	/**
	 * Get the interval that contains the graph's data. Excluded data is ignored to show smaller values in
	 * better detail when disabling higher ones.
	 * @param num_hori_lines Number of horizontal lines to be drawn.
	 * @return Highest and lowest values of the graph (ignoring disabled data).
	 */
	ValuesInterval GetValuesInterval(int num_hori_lines) const
	{
		assert(num_hori_lines > 0);

		ValuesInterval current_interval;
		current_interval.highest = INT64_MIN;
		current_interval.lowest  = INT64_MAX;

		for (int i = 0; i < this->num_dataset; i++) {
			if (HasBit(this->excluded_data, i)) continue;
			for (int j = 0; j < this->num_on_x_axis; j++) {
				OverflowSafeInt64 datapoint = this->cost[i][j];

				if (datapoint != INVALID_DATAPOINT) {
					current_interval.highest = std::max(current_interval.highest, datapoint);
					current_interval.lowest  = std::min(current_interval.lowest, datapoint);
				}
			}
		}

		/* Prevent showing values too close to the graph limits. */
		current_interval.highest = (11 * current_interval.highest) / 10;
		current_interval.lowest =  (11 * current_interval.lowest) / 10;

		/* Always include zero in the shown range. */
		double abs_lower  = (current_interval.lowest > 0) ? 0 : (double)abs(current_interval.lowest);
		double abs_higher = (current_interval.highest < 0) ? 0 : (double)current_interval.highest;

		int num_pos_grids;
		int64_t grid_size;

		if (abs_lower != 0 || abs_higher != 0) {
			/* The number of grids to reserve for the positive part is: */
			num_pos_grids = (int)floor(0.5 + num_hori_lines * abs_higher / (abs_higher + abs_lower));

			/* If there are any positive or negative values, force that they have at least one grid. */
			if (num_pos_grids == 0 && abs_higher != 0) num_pos_grids++;
			if (num_pos_grids == num_hori_lines && abs_lower != 0) num_pos_grids--;

			/* Get the required grid size for each side and use the maximum one. */
			int64_t grid_size_higher = (abs_higher > 0) ? ((int64_t)abs_higher + num_pos_grids - 1) / num_pos_grids : 0;
			int64_t grid_size_lower = (abs_lower > 0) ? ((int64_t)abs_lower + num_hori_lines - num_pos_grids - 1) / (num_hori_lines - num_pos_grids) : 0;
			grid_size = std::max(grid_size_higher, grid_size_lower);
		} else {
			/* If both values are zero, show an empty graph. */
			num_pos_grids = num_hori_lines / 2;
			grid_size = 1;
		}

		current_interval.highest = num_pos_grids * grid_size;
		current_interval.lowest = -(num_hori_lines - num_pos_grids) * grid_size;
		return current_interval;
	}

	/**
	 * Get width for Y labels.
	 * @param current_interval Interval that contains all of the graph data.
	 * @param num_hori_lines Number of horizontal lines to be drawn.
	 */
	uint GetYLabelWidth(ValuesInterval current_interval, int num_hori_lines) const
	{
		/* draw text strings on the y axis */
		int64_t y_label = current_interval.highest;
		int64_t y_label_separation = (current_interval.highest - current_interval.lowest) / num_hori_lines;

		uint max_width = 0;

		for (int i = 0; i < (num_hori_lines + 1); i++) {
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
	void DrawGraph(Rect r) const
	{
		uint x, y;               ///< Reused whenever x and y coordinates are needed.
		ValuesInterval interval; ///< Interval that contains all of the graph data.
		int x_axis_offset;       ///< Distance from the top of the graph to the x axis.

		/* the colours and cost array of GraphDrawer must accommodate
		 * both values for cargo and companies. So if any are higher, quit */
		static_assert(GRAPH_MAX_DATASETS >= (int)NUM_CARGO && GRAPH_MAX_DATASETS >= (int)MAX_COMPANIES);
		assert(this->num_vert_lines > 0);

		/* Rect r will be adjusted to contain just the graph, with labels being
		 * placed outside the area. */
		r.top    += ScaleGUITrad(5) + GetCharacterHeight(FS_SMALL) / 2;
		r.bottom -= (this->draw_dates ? 2 : 1) * GetCharacterHeight(FS_SMALL) + ScaleGUITrad(4);
		r.left   += ScaleGUITrad(9);
		r.right  -= ScaleGUITrad(5);

		/* Initial number of horizontal lines. */
		int num_hori_lines = 160 / ScaleGUITrad(MIN_GRID_PIXEL_SIZE);
		/* For the rest of the height, the number of horizontal lines will increase more slowly. */
		int resize = (r.bottom - r.top - 160) / (2 * ScaleGUITrad(MIN_GRID_PIXEL_SIZE));
		if (resize > 0) num_hori_lines += resize;

		interval = GetValuesInterval(num_hori_lines);

		int label_width = GetYLabelWidth(interval, num_hori_lines);

		r.left += label_width;

		int x_sep = (r.right - r.left) / this->num_vert_lines;
		int y_sep = (r.bottom - r.top) / num_hori_lines;

		/* Redetermine right and bottom edge of graph to fit with the integer
		 * separation values. */
		r.right = r.left + x_sep * this->num_vert_lines;
		r.bottom = r.top + y_sep * num_hori_lines;

		OverflowSafeInt64 interval_size = interval.highest + abs(interval.lowest);
		/* Where to draw the X axis. Use floating point to avoid overflowing and results of zero. */
		x_axis_offset = (int)((r.bottom - r.top) * (double)interval.highest / (double)interval_size);

		/* Draw the background of the graph itself. */
		GfxFillRect(r.left, r.top, r.right, r.bottom, GRAPH_BASE_COLOUR);

		/* Draw the vertical grid lines. */

		/* Don't draw the first line, as that's where the axis will be. */
		x = r.left + x_sep;

		for (int i = 0; i < this->num_vert_lines; i++) {
			GfxFillRect(x, r.top, x, r.bottom, GRAPH_GRID_COLOUR);
			x += x_sep;
		}

		/* Draw the horizontal grid lines. */
		y = r.bottom;

		for (int i = 0; i < (num_hori_lines + 1); i++) {
			GfxFillRect(r.left - ScaleGUITrad(3), y, r.left - 1, y, GRAPH_AXIS_LINE_COLOUR);
			GfxFillRect(r.left, y, r.right, y, GRAPH_GRID_COLOUR);
			y -= y_sep;
		}

		/* Draw the y axis. */
		GfxFillRect(r.left, r.top, r.left, r.bottom, GRAPH_AXIS_LINE_COLOUR);

		/* Draw the x axis. */
		y = x_axis_offset + r.top;
		GfxFillRect(r.left, y, r.right, y, GRAPH_ZERO_LINE_COLOUR);

		/* Find the largest value that will be drawn. */
		if (this->num_on_x_axis == 0) return;

		assert(this->num_on_x_axis > 0);

		/* draw text strings on the y axis */
		int64_t y_label = interval.highest;
		int64_t y_label_separation = abs(interval.highest - interval.lowest) / num_hori_lines;

		y = r.top - GetCharacterHeight(FS_SMALL) / 2;

		for (int i = 0; i < (num_hori_lines + 1); i++) {
			SetDParam(0, this->format_str_y_axis);
			SetDParam(1, y_label);
			DrawString(r.left - label_width - ScaleGUITrad(4), r.left - ScaleGUITrad(4), y, STR_GRAPH_Y_LABEL, GRAPH_AXIS_LABEL_COLOUR, SA_RIGHT);

			y_label -= y_label_separation;
			y += y_sep;
		}

		/* Draw x-axis labels and markings for graphs based on financial quarters and years.  */
		if (this->draw_dates) {
			x = r.left;
			y = r.bottom + ScaleGUITrad(2);
			TimerGameCalendar::Month month = this->month;
			TimerGameCalendar::Year year = this->year;
			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, month + STR_MONTH_ABBREV_JAN);
				SetDParam(1, year);
				DrawStringMultiLine(x, x + x_sep, y, this->height, month == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH, GRAPH_AXIS_LABEL_COLOUR, SA_LEFT);

				month += 3;
				if (month >= 12) {
					month = 0;
					year++;

					/* Draw a lighter grid line between years. Top and bottom adjustments ensure we don't draw over top and bottom horizontal grid lines. */
					GfxFillRect(x + x_sep, r.top + 1, x + x_sep, r.bottom - 1, GRAPH_YEAR_LINE_COLOUR);
				}
				x += x_sep;
			}
		} else {
			/* Draw x-axis labels for graphs not based on quarterly performance (cargo payment rates). */
			x = r.left;
			y = r.bottom + ScaleGUITrad(2);
			uint16_t label = this->x_values_start;

			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, label);
				DrawString(x + 1, x + x_sep - 1, y, STR_GRAPH_Y_LABEL_NUMBER, GRAPH_AXIS_LABEL_COLOUR, SA_HOR_CENTER);

				label += this->x_values_increment;
				x += x_sep;
			}
		}

		/* draw lines and dots */
		uint linewidth = _settings_client.gui.graph_line_thickness;
		uint pointoffs1 = (linewidth + 1) / 2;
		uint pointoffs2 = linewidth + 1 - pointoffs1;
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
						 * datapoint value and the highest value to split overflows.
						 * And when 'drawing' 'one million' or 'one million and one'
						 * there is no significant difference, so the least
						 * significant bits can just be removed.
						 *
						 * If there are more bits needed than would fit in a 32 bits
						 * integer, so at about 31 bits because of the sign bit, the
						 * least significant bits are removed.
						 */
						int mult_range = FindLastBit(x_axis_offset) + FindLastBit(abs(datapoint));
						int reduce_range = std::max(mult_range - 31, 0);

						/* Handle negative values differently (don't shift sign) */
						if (datapoint < 0) {
							datapoint = -(abs(datapoint) >> reduce_range);
						} else {
							datapoint >>= reduce_range;
						}
						y = r.top + x_axis_offset - ((r.bottom - r.top) * datapoint) / (interval_size >> reduce_range);

						/* Draw the point. */
						GfxFillRect(x - pointoffs1, y - pointoffs1, x + pointoffs2, y + pointoffs2, colour);

						/* Draw the line connected to the previous point. */
						if (prev_x != INVALID_DATAPOINT_POS) GfxDrawLine(prev_x, prev_y, x, y, colour, linewidth);

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


	BaseGraphWindow(WindowDesc *desc, WidgetID widget, StringID format_str_y_axis) :
			Window(desc),
			format_str_y_axis(format_str_y_axis)
	{
		SetWindowDirty(WC_GRAPH_LEGEND, 0);
		this->num_vert_lines = 24;
		this->graph_widget = widget;
	}

	void InitializeWindow(WindowNumber number)
	{
		/* Initialise the dataset */
		this->UpdateStatistics(true);

		this->InitNested(number);
	}

public:
	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != this->graph_widget) return;

		uint x_label_width = 0;

		/* Draw x-axis labels and markings for graphs based on financial quarters and years.  */
		if (this->draw_dates) {
			TimerGameCalendar::Month month = this->month;
			TimerGameCalendar::Year year = this->year;
			for (int i = 0; i < this->num_on_x_axis; i++) {
				SetDParam(0, month + STR_MONTH_ABBREV_JAN);
				SetDParam(1, year);
				x_label_width = std::max(x_label_width, GetStringBoundingBox(month == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH).width);

				month += 3;
				if (month >= 12) {
					month = 0;
					year++;
				}
			}
		} else {
			/* Draw x-axis labels for graphs not based on quarterly performance (cargo payment rates). */
			SetDParamMaxValue(0, this->x_values_start + this->num_on_x_axis * this->x_values_increment, 0, FS_SMALL);
			x_label_width = GetStringBoundingBox(STR_GRAPH_Y_LABEL_NUMBER).width;
		}

		SetDParam(0, this->format_str_y_axis);
		SetDParam(1, INT64_MAX);
		uint y_label_width = GetStringBoundingBox(STR_GRAPH_Y_LABEL).width;

		size->width  = std::max<uint>(size->width,  ScaleGUITrad(5) + y_label_width + this->num_on_x_axis * (x_label_width + ScaleGUITrad(5)) + ScaleGUITrad(9));
		size->height = std::max<uint>(size->height, ScaleGUITrad(5) + (1 + MIN_GRAPH_NUM_LINES_Y * 2 + (this->draw_dates ? 3 : 1)) * GetCharacterHeight(FS_SMALL) + ScaleGUITrad(4));
		size->height = std::max<uint>(size->height, size->width / 3);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != this->graph_widget) return;

		DrawGraph(r);
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *, int)
	{
		return INVALID_DATAPOINT;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* Clicked on legend? */
		if (widget == WID_CV_KEY_BUTTON) ShowGraphLegend();
	}

	void OnGameTick() override
	{
		this->UpdateStatistics(false);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->UpdateStatistics(true);
	}

	/**
	 * Update the statistics.
	 * @param initialize Initialize the data structure.
	 */
	void UpdateStatistics(bool initialize)
	{
		CompanyMask excluded_companies = _legend_excluded_companies;

		/* Exclude the companies which aren't valid */
		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (!Company::IsValidID(c)) SetBit(excluded_companies, c);
		}

		byte nums = 0;
		for (const Company *c : Company::Iterate()) {
			nums = std::min(this->num_vert_lines, std::max(nums, c->num_valid_stat_ent));
		}

		int mo = (TimerGameCalendar::month / 3 - nums) * 3;
		auto yr = TimerGameCalendar::year;
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
			const Company *c = Company::GetIfValid(k);
			if (c != nullptr) {
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
	OperatingProfitGraphWindow(WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, WID_CV_GRAPH, STR_JUST_CURRENCY_SHORT)
	{
		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].income + c->old_economy[j].expenses;
	}
};

static const NWidgetPart _nested_operating_profit_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_OPERATING_PROFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CV_KEY_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_CV_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CV_GRAPH), SetMinimalSize(576, 160), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_CV_RESIZE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _operating_profit_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_operating_profit", 0, 0,
	WC_OPERATING_PROFIT, WC_NONE,
	0,
	std::begin(_nested_operating_profit_widgets), std::end(_nested_operating_profit_widgets)
);


void ShowOperatingProfitGraph()
{
	AllocateWindowDescFront<OperatingProfitGraphWindow>(&_operating_profit_desc, 0);
}


/****************/
/* INCOME GRAPH */
/****************/

struct IncomeGraphWindow : BaseGraphWindow {
	IncomeGraphWindow(WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, WID_CV_GRAPH, STR_JUST_CURRENCY_SHORT)
	{
		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].income;
	}
};

static const NWidgetPart _nested_income_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_INCOME_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CV_KEY_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_CV_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CV_GRAPH), SetMinimalSize(576, 128), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_CV_RESIZE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _income_graph_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_income", 0, 0,
	WC_INCOME_GRAPH, WC_NONE,
	0,
	std::begin(_nested_income_graph_widgets), std::end(_nested_income_graph_widgets)
);

void ShowIncomeGraph()
{
	AllocateWindowDescFront<IncomeGraphWindow>(&_income_graph_desc, 0);
}

/*******************/
/* DELIVERED CARGO */
/*******************/

struct DeliveredCargoGraphWindow : BaseGraphWindow {
	DeliveredCargoGraphWindow(WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, WID_CV_GRAPH, STR_JUST_COMMA)
	{
		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].delivered_cargo.GetSum<OverflowSafeInt64>();
	}
};

static const NWidgetPart _nested_delivered_cargo_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_CARGO_DELIVERED_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CV_KEY_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_CV_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CV_GRAPH), SetMinimalSize(576, 128), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_CV_RESIZE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _delivered_cargo_graph_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_delivered_cargo", 0, 0,
	WC_DELIVERED_CARGO, WC_NONE,
	0,
	std::begin(_nested_delivered_cargo_graph_widgets), std::end(_nested_delivered_cargo_graph_widgets)
);

void ShowDeliveredCargoGraph()
{
	AllocateWindowDescFront<DeliveredCargoGraphWindow>(&_delivered_cargo_graph_desc, 0);
}

/***********************/
/* PERFORMANCE HISTORY */
/***********************/

struct PerformanceHistoryGraphWindow : BaseGraphWindow {
	PerformanceHistoryGraphWindow(WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, WID_PHG_GRAPH, STR_JUST_COMMA)
	{
		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].performance_history;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_PHG_DETAILED_PERFORMANCE) ShowPerformanceRatingDetail();
		this->BaseGraphWindow::OnClick(pt, widget, click_count);
	}
};

static const NWidgetPart _nested_performance_history_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_COMPANY_PERFORMANCE_RATINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_PHG_DETAILED_PERFORMANCE), SetMinimalSize(50, 0), SetDataTip(STR_PERFORMANCE_DETAIL_KEY, STR_GRAPH_PERFORMANCE_DETAIL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_PHG_KEY), SetMinimalSize(50, 0), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_PHG_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_BROWN, WID_PHG_GRAPH), SetMinimalSize(576, 224), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_PHG_RESIZE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _performance_history_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_performance", 0, 0,
	WC_PERFORMANCE_HISTORY, WC_NONE,
	0,
	std::begin(_nested_performance_history_widgets), std::end(_nested_performance_history_widgets)
);

void ShowPerformanceHistoryGraph()
{
	AllocateWindowDescFront<PerformanceHistoryGraphWindow>(&_performance_history_desc, 0);
}

/*****************/
/* COMPANY VALUE */
/*****************/

struct CompanyValueGraphWindow : BaseGraphWindow {
	CompanyValueGraphWindow(WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, WID_CV_GRAPH, STR_JUST_CURRENCY_SHORT)
	{
		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].company_value;
	}
};

static const NWidgetPart _nested_company_value_graph_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_COMPANY_VALUES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CV_KEY_BUTTON), SetMinimalSize(50, 0), SetDataTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_CV_BACKGROUND),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CV_GRAPH), SetMinimalSize(576, 224), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_CV_RESIZE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _company_value_graph_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_company_value", 0, 0,
	WC_COMPANY_VALUE, WC_NONE,
	0,
	std::begin(_nested_company_value_graph_widgets), std::end(_nested_company_value_graph_widgets)
);

void ShowCompanyValueGraph()
{
	AllocateWindowDescFront<CompanyValueGraphWindow>(&_company_value_graph_desc, 0);
}

/*****************/
/* PAYMENT RATES */
/*****************/

struct PaymentRatesGraphWindow : BaseGraphWindow {
	uint line_height;   ///< Pixel height of each cargo type row.
	Scrollbar *vscroll; ///< Cargo list scrollbar.
	uint legend_width;  ///< Width of legend 'blob'.

	PaymentRatesGraphWindow(WindowDesc *desc, WindowNumber window_number) :
			BaseGraphWindow(desc, WID_CPR_GRAPH, STR_JUST_CURRENCY_SHORT)
	{
		this->num_on_x_axis = 20;
		this->num_vert_lines = 20;
		this->draw_dates = false;
		this->x_values_start     = 10;
		this->x_values_increment = 10;

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_CPR_MATRIX_SCROLLBAR);
		this->vscroll->SetCount(_sorted_standard_cargo_specs.size());

		/* Initialise the dataset */
		this->UpdatePaymentRates();

		this->FinishInitNested(window_number);
	}

	void OnInit() override
	{
		/* Width of the legend blob. */
		this->legend_width = GetCharacterHeight(FS_SMALL) * 9 / 6;
	}

	void UpdateExcludedData()
	{
		this->excluded_data = 0;

		int i = 0;
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			if (HasBit(_legend_excluded_cargo, cs->Index())) SetBit(this->excluded_data, i);
			i++;
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		if (widget != WID_CPR_MATRIX) {
			BaseGraphWindow::UpdateWidgetSize(widget, size, padding, fill, resize);
			return;
		}

		size->height = GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical();

		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			SetDParam(0, cs->name);
			Dimension d = GetStringBoundingBox(STR_GRAPH_CARGO_PAYMENT_CARGO);
			d.width += this->legend_width + WidgetDimensions::scaled.hsep_normal; // colour field
			d.width += WidgetDimensions::scaled.framerect.Horizontal();
			d.height += WidgetDimensions::scaled.framerect.Vertical();
			*size = maxdim(d, *size);
		}

		this->line_height = size->height;
		size->height = this->line_height * 11; /* Default number of cargo types in most climates. */
		resize->width = 0;
		resize->height = this->line_height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_CPR_MATRIX) {
			BaseGraphWindow::DrawWidget(r, widget);
			return;
		}

		bool rtl = _current_text_dir == TD_RTL;

		int pos = this->vscroll->GetPosition();
		int max = pos + this->vscroll->GetCapacity();

		Rect line = r.WithHeight(this->line_height);
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			if (pos-- > 0) continue;
			if (--max < 0) break;

			bool lowered = !HasBit(_legend_excluded_cargo, cs->Index());

			/* Redraw frame if lowered */
			if (lowered) DrawFrameRect(line, COLOUR_BROWN, FR_LOWERED);

			const Rect text = line.Shrink(WidgetDimensions::scaled.framerect);

			/* Cargo-colour box with outline */
			const Rect cargo = text.WithWidth(this->legend_width, rtl);
			GfxFillRect(cargo, PC_BLACK);
			GfxFillRect(cargo.Shrink(WidgetDimensions::scaled.bevel), cs->legend_colour);

			/* Cargo name */
			SetDParam(0, cs->name);
			DrawString(text.Indent(this->legend_width + WidgetDimensions::scaled.hsep_normal, rtl), STR_GRAPH_CARGO_PAYMENT_CARGO);

			line = line.Translate(0, this->line_height);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CPR_ENABLE_CARGOES:
				/* Remove all cargoes from the excluded lists. */
				_legend_excluded_cargo = 0;
				this->excluded_data = 0;
				this->SetDirty();
				break;

			case WID_CPR_DISABLE_CARGOES: {
				/* Add all cargoes to the excluded lists. */
				int i = 0;
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					SetBit(_legend_excluded_cargo, cs->Index());
					SetBit(this->excluded_data, i);
					i++;
				}
				this->SetDirty();
				break;
			}

			case WID_CPR_MATRIX: {
				auto it = this->vscroll->GetScrolledItemFromWidget(_sorted_standard_cargo_specs, pt.y, this, WID_CPR_MATRIX);
				if (it != _sorted_standard_cargo_specs.end()) {
					ToggleBit(_legend_excluded_cargo, (*it)->Index());
					this->UpdateExcludedData();
					this->SetDirty();
					break;
				}
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_CPR_MATRIX);
	}

	void OnGameTick() override
	{
		/* Override default OnGameTick */
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->UpdatePaymentRates();
	}

	/** Update the payment rates on a regular interval. */
	IntervalTimer<TimerWindow> update_payment_interval = {std::chrono::seconds(3), [this](auto) {
		this->UpdatePaymentRates();
	}};

	/**
	 * Update the payment rates according to the latest information.
	 */
	void UpdatePaymentRates()
	{
		this->UpdateExcludedData();

		int i = 0;
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			this->colours[i] = cs->legend_colour;
			for (uint j = 0; j != 20; j++) {
				this->cost[i][j] = GetTransportedGoodsIncome(10, 20, j * 4 + 4, cs->Index());
			}
			i++;
		}
		this->num_dataset = i;
	}
};

static const NWidgetPart _nested_cargo_payment_rates_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_CPR_BACKGROUND), SetMinimalSize(568, 128),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_TEXT, COLOUR_BROWN, WID_CPR_HEADER), SetMinimalSize(0, 6), SetPadding(2, 0, 2, 0), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_TITLE, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_BROWN, WID_CPR_GRAPH), SetMinimalSize(495, 0), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CPR_ENABLE_CARGOES), SetDataTip(STR_GRAPH_CARGO_ENABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_ENABLE_ALL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CPR_DISABLE_CARGOES), SetDataTip(STR_GRAPH_CARGO_DISABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_DISABLE_ALL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_BROWN, WID_CPR_MATRIX), SetFill(1, 0), SetResize(0, 2), SetMatrixDataTip(1, 0, STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO), SetScrollbar(WID_CPR_MATRIX_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_CPR_MATRIX_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(0, 1), SetResize(0, 1),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(12, 0), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_TEXT, COLOUR_BROWN, WID_CPR_FOOTER), SetMinimalSize(0, 6), SetPadding(2, 0, 2, 0), SetDataTip(STR_GRAPH_CARGO_PAYMENT_RATES_X_LABEL, STR_NULL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_CPR_RESIZE), SetDataTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _cargo_payment_rates_desc(__FILE__, __LINE__,
	WDP_AUTO, "graph_cargo_payment_rates", 0, 0,
	WC_PAYMENT_RATES, WC_NONE,
	0,
	std::begin(_nested_cargo_payment_rates_widgets), std::end(_nested_cargo_payment_rates_widgets)
);


void ShowCargoPaymentRates()
{
	AllocateWindowDescFront<PaymentRatesGraphWindow>(&_cargo_payment_rates_desc, 0);
}

/*****************************/
/* PERFORMANCE RATING DETAIL */
/*****************************/

struct PerformanceRatingDetailWindow : Window {
	static CompanyID company;
	int timeout;

	PerformanceRatingDetailWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->UpdateCompanyStats();

		this->InitNested(window_number);
		this->OnInvalidateData(INVALID_COMPANY);
	}

	void UpdateCompanyStats()
	{
		/* Update all company stats with the current data
		 * (this is because _score_info is not saved to a savegame) */
		for (Company *c : Company::Iterate()) {
			UpdateCompanyRatingAndValue(c, false);
		}

		this->timeout = Ticks::DAY_TICKS * 5;
	}

	uint score_info_left;
	uint score_info_right;
	uint bar_left;
	uint bar_right;
	uint bar_width;
	uint bar_height;
	uint score_detail_left;
	uint score_detail_right;

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_PRD_SCORE_FIRST:
				this->bar_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.fullbevel.Vertical();
				size->height = this->bar_height + WidgetDimensions::scaled.matrix.Vertical();

				uint score_info_width = 0;
				for (uint i = SCORE_BEGIN; i < SCORE_END; i++) {
					score_info_width = std::max(score_info_width, GetStringBoundingBox(STR_PERFORMANCE_DETAIL_VEHICLES + i).width);
				}
				SetDParamMaxValue(0, 1000);
				score_info_width += GetStringBoundingBox(STR_JUST_COMMA).width + WidgetDimensions::scaled.hsep_wide;

				SetDParamMaxValue(0, 100);
				this->bar_width = GetStringBoundingBox(STR_PERFORMANCE_DETAIL_PERCENT).width + WidgetDimensions::scaled.hsep_indent * 2; // Wide bars!

				/* At this number we are roughly at the max; it can become wider,
				 * but then you need at 1000 times more money. At that time you're
				 * not that interested anymore in the last few digits anyway.
				 * The 500 is because 999 999 500 to 999 999 999 are rounded to
				 * 1 000 M, and not 999 999 k. Use negative numbers to account for
				 * the negative income/amount of money etc. as well. */
				int max = -(999999999 - 500);

				/* Scale max for the display currency. Prior to rendering the value
				 * is converted into the display currency, which may cause it to
				 * raise significantly. We need to compensate for that since {{CURRCOMPACT}}
				 * is used, which can produce quite short renderings of very large
				 * values. Otherwise the calculated width could be too narrow.
				 * Note that it doesn't work if there was a currency with an exchange
				 * rate greater than max.
				 * When the currency rate is more than 1000, the 999 999 k becomes at
				 * least 999 999 M which roughly is equally long. Furthermore if the
				 * exchange rate is that high, 999 999 k is usually not enough anymore
				 * to show the different currency numbers. */
				if (_currency->rate < 1000) max /= _currency->rate;
				SetDParam(0, max);
				SetDParam(1, max);
				uint score_detail_width = GetStringBoundingBox(STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY).width;

				size->width = WidgetDimensions::scaled.frametext.Horizontal() + score_info_width + WidgetDimensions::scaled.hsep_wide + this->bar_width + WidgetDimensions::scaled.hsep_wide + score_detail_width;
				uint left  = WidgetDimensions::scaled.frametext.left;
				uint right = size->width - WidgetDimensions::scaled.frametext.right;

				bool rtl = _current_text_dir == TD_RTL;
				this->score_info_left  = rtl ? right - score_info_width : left;
				this->score_info_right = rtl ? right : left + score_info_width;

				this->score_detail_left  = rtl ? left : right - score_detail_width;
				this->score_detail_right = rtl ? left + score_detail_width : right;

				this->bar_left  = left + (rtl ? score_detail_width : score_info_width) + WidgetDimensions::scaled.hsep_wide;
				this->bar_right = this->bar_left + this->bar_width - 1;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		/* No need to draw when there's nothing to draw */
		if (this->company == INVALID_COMPANY) return;

		if (IsInsideMM(widget, WID_PRD_COMPANY_FIRST, WID_PRD_COMPANY_LAST + 1)) {
			if (this->IsWidgetDisabled(widget)) return;
			CompanyID cid = (CompanyID)(widget - WID_PRD_COMPANY_FIRST);
			Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
			DrawCompanyIcon(cid, CenterBounds(r.left, r.right, sprite_size.width), CenterBounds(r.top, r.bottom, sprite_size.height));
			return;
		}

		if (!IsInsideMM(widget, WID_PRD_SCORE_FIRST, WID_PRD_SCORE_LAST + 1)) return;

		ScoreID score_type = (ScoreID)(widget - WID_PRD_SCORE_FIRST);

		/* The colours used to show how the progress is going */
		int colour_done = _colour_gradient[COLOUR_GREEN][4];
		int colour_notdone = _colour_gradient[COLOUR_RED][4];

		/* Draw all the score parts */
		int64_t val    = _score_part[company][score_type];
		int64_t needed = _score_info[score_type].needed;
		int   score  = _score_info[score_type].score;

		/* SCORE_TOTAL has its own rules ;) */
		if (score_type == SCORE_TOTAL) {
			for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) score += _score_info[i].score;
			needed = SCORE_MAX;
		}

		uint bar_top  = CenterBounds(r.top, r.bottom, this->bar_height);
		uint text_top = CenterBounds(r.top, r.bottom, GetCharacterHeight(FS_NORMAL));

		DrawString(this->score_info_left, this->score_info_right, text_top, STR_PERFORMANCE_DETAIL_VEHICLES + score_type);

		/* Draw the score */
		SetDParam(0, score);
		DrawString(this->score_info_left, this->score_info_right, text_top, STR_JUST_COMMA, TC_BLACK, SA_RIGHT);

		/* Calculate the %-bar */
		uint x = Clamp<int64_t>(val, 0, needed) * this->bar_width / needed;
		bool rtl = _current_text_dir == TD_RTL;
		if (rtl) {
			x = this->bar_right - x;
		} else {
			x = this->bar_left + x;
		}

		/* Draw the bar */
		if (x != this->bar_left)  GfxFillRect(this->bar_left, bar_top, x,               bar_top + this->bar_height - 1, rtl ? colour_notdone : colour_done);
		if (x != this->bar_right) GfxFillRect(x,              bar_top, this->bar_right, bar_top + this->bar_height - 1, rtl ? colour_done : colour_notdone);

		/* Draw it */
		SetDParam(0, Clamp<int64_t>(val, 0, needed) * 100 / needed);
		DrawString(this->bar_left, this->bar_right, text_top, STR_PERFORMANCE_DETAIL_PERCENT, TC_FROMSTRING, SA_HOR_CENTER);

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

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, WID_PRD_COMPANY_FIRST, WID_PRD_COMPANY_LAST + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				this->RaiseWidget(this->company + WID_PRD_COMPANY_FIRST);
				this->company = (CompanyID)(widget - WID_PRD_COMPANY_FIRST);
				this->LowerWidget(this->company + WID_PRD_COMPANY_FIRST);
				this->SetDirty();
			}
		}
	}

	void OnGameTick() override
	{
		/* Update the company score every 5 days */
		if (--this->timeout == 0) {
			this->UpdateCompanyStats();
			this->SetDirty();
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data the company ID of the company that is going to be removed
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		/* Disable the companies who are not active */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(i + WID_PRD_COMPANY_FIRST, !Company::IsValidID(i));
		}

		/* Check if the currently selected company is still active. */
		if (this->company != INVALID_COMPANY && !Company::IsValidID(this->company)) {
			/* Raise the widget for the previous selection. */
			this->RaiseWidget(this->company + WID_PRD_COMPANY_FIRST);
			this->company = INVALID_COMPANY;
		}

		if (this->company == INVALID_COMPANY) {
			for (const Company *c : Company::Iterate()) {
				this->company = c->index;
				break;
			}
		}

		/* Make sure the widget is lowered */
		this->LowerWidget(this->company + WID_PRD_COMPANY_FIRST);
	}
};

CompanyID PerformanceRatingDetailWindow::company = INVALID_COMPANY;

/**
 * Make a vertical list of panels for outputting score details.
 * @return Panel with performance details.
 */
static std::unique_ptr<NWidgetBase> MakePerformanceDetailPanels()
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

	static_assert(lengthof(performance_tips) == SCORE_END - SCORE_BEGIN);

	auto vert = std::make_unique<NWidgetVertical>(NC_EQUALSIZE);
	for (WidgetID widnum = WID_PRD_SCORE_FIRST; widnum <= WID_PRD_SCORE_LAST; widnum++) {
		auto panel = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_BROWN, widnum);
		panel->SetFill(1, 1);
		panel->SetDataTip(0x0, performance_tips[widnum - WID_PRD_SCORE_FIRST]);
		vert->Add(std::move(panel));
	}
	return vert;
}

/** Make a number of rows with buttons for each company for the performance rating detail window. */
std::unique_ptr<NWidgetBase> MakeCompanyButtonRowsGraphGUI()
{
	return MakeCompanyButtonRows(WID_PRD_COMPANY_FIRST, WID_PRD_COMPANY_LAST, COLOUR_BROWN, 8, STR_PERFORMANCE_DETAIL_SELECT_COMPANY_TOOLTIP);
}

static const NWidgetPart _nested_performance_rating_detail_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetDataTip(STR_PERFORMANCE_DETAIL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidgetFunction(MakeCompanyButtonRowsGraphGUI), SetPadding(2),
	EndContainer(),
	NWidgetFunction(MakePerformanceDetailPanels),
};

static WindowDesc _performance_rating_detail_desc(__FILE__, __LINE__,
	WDP_AUTO, "league_details", 0, 0,
	WC_PERFORMANCE_DETAIL, WC_NONE,
	0,
	std::begin(_nested_performance_rating_detail_widgets), std::end(_nested_performance_rating_detail_widgets)
);

void ShowPerformanceRatingDetail()
{
	AllocateWindowDescFront<PerformanceRatingDetailWindow>(&_performance_rating_detail_desc, 0);
}

void InitializeGraphGui()
{
	_legend_excluded_companies = 0;
	_legend_excluded_cargo = 0;
}
