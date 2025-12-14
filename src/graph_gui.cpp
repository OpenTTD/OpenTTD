/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file graph_gui.cpp GUI that shows performance graphs. */

#include "stdafx.h"
#include <ranges>
#include "misc/history_func.hpp"
#include "graph_gui.h"
#include "window_gui.h"
#include "company_base.h"
#include "company_gui.h"
#include "economy_func.h"
#include "cargotype.h"
#include "strings_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "gfx_func.h"
#include "core/geometry_func.hpp"
#include "currency.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_economy.h"
#include "zoom_func.h"
#include "industry.h"
#include "town.h"

#include "widgets/graph_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

/* Bitmasks of company and cargo indices that shouldn't be drawn. */
static CompanyMask _legend_excluded_companies;

/* Apparently these don't play well with enums. */
static const OverflowSafeInt64 INVALID_DATAPOINT(INT64_MAX); // Value used for a datapoint that shouldn't be drawn.
static const uint INVALID_DATAPOINT_POS = UINT_MAX;  // Used to determine if the previous point was drawn.

constexpr double INT64_MAX_IN_DOUBLE = static_cast<double>(INT64_MAX - 512); ///< The biggest double that when cast to int64_t still fits in a int64_t.
static_assert(static_cast<int64_t>(INT64_MAX_IN_DOUBLE) < INT64_MAX);

/****************/
/* GRAPH LEGEND */
/****************/

struct GraphLegendWindow : Window {
	GraphLegendWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);

		for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
			if (!_legend_excluded_companies.Test(c)) this->LowerWidget(WID_GL_FIRST_COMPANY + c);

			this->OnInvalidateData(c.base());
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (!IsInsideMM(widget, WID_GL_FIRST_COMPANY, WID_GL_FIRST_COMPANY + MAX_COMPANIES)) return;

		CompanyID cid = (CompanyID)(widget - WID_GL_FIRST_COMPANY);

		if (!Company::IsValidID(cid)) return;

		bool rtl = _current_text_dir == TD_RTL;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		Dimension d = GetSpriteSize(SPR_COMPANY_ICON);
		DrawCompanyIcon(cid, rtl ? ir.right - d.width : ir.left, CentreBounds(ir.top, ir.bottom, d.height));

		const Rect tr = ir.Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);
		DrawString(tr.left, tr.right, CentreBounds(tr.top, tr.bottom, GetCharacterHeight(FS_NORMAL)), GetString(STR_COMPANY_NAME_COMPANY_NUM, cid, cid), _legend_excluded_companies.Test(cid) ? TC_BLACK : TC_WHITE);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (!IsInsideMM(widget, WID_GL_FIRST_COMPANY, WID_GL_FIRST_COMPANY + MAX_COMPANIES)) return;

		_legend_excluded_companies.Flip(static_cast<CompanyID>(widget - WID_GL_FIRST_COMPANY));
		this->ToggleWidgetLoweredState(widget);
		this->SetDirty();
		InvalidateWindowData(WC_INCOME_GRAPH, 0);
		InvalidateWindowData(WC_OPERATING_PROFIT, 0);
		InvalidateWindowData(WC_DELIVERED_CARGO, 0);
		InvalidateWindowData(WC_PERFORMANCE_HISTORY, 0);
		InvalidateWindowData(WC_COMPANY_VALUE, 0);

		SndClickBeep();
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

		_legend_excluded_companies.Set(static_cast<CompanyID>(data));
		this->RaiseWidget(data + WID_GL_FIRST_COMPANY);
	}
};

/**
 * Construct a vertical list of buttons, one for each company.
 * @return Panel with company buttons.
 */
static std::unique_ptr<NWidgetBase> MakeNWidgetCompanyLines()
{
	auto vert = std::make_unique<NWidgetVertical>(NWidContainerFlag::EqualSize);
	vert->SetPadding(2, 2, 2, 2);
	uint sprite_height = GetSpriteSize(SPR_COMPANY_ICON, nullptr, ZoomLevel::Normal).height;

	for (WidgetID widnum = WID_GL_FIRST_COMPANY; widnum <= WID_GL_LAST_COMPANY; widnum++) {
		auto panel = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_BROWN, widnum);
		panel->SetMinimalSize(246, sprite_height + WidgetDimensions::unscaled.framerect.Vertical());
		panel->SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical(), FS_NORMAL);
		panel->SetFill(1, 1);
		panel->SetToolTip(STR_GRAPH_KEY_COMPANY_SELECTION_TOOLTIP);
		vert->Add(std::move(panel));
	}
	return vert;
}

static constexpr std::initializer_list<NWidgetPart> _nested_graph_legend_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_KEY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GL_BACKGROUND),
		NWidgetFunction(MakeNWidgetCompanyLines),
	EndContainer(),
};

static WindowDesc _graph_legend_desc(
	WDP_AUTO, "graph_legend", 0, 0,
	WC_GRAPH_LEGEND, WC_NONE,
	{},
	_nested_graph_legend_widgets
);

static void ShowGraphLegend()
{
	AllocateWindowDescFront<GraphLegendWindow>(_graph_legend_desc, 0);
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
	static constexpr PixelColour GRAPH_BASE_COLOUR      =  GREY_SCALE(2);
	static constexpr PixelColour GRAPH_GRID_COLOUR      =  GREY_SCALE(3);
	static constexpr PixelColour GRAPH_AXIS_LINE_COLOUR =  GREY_SCALE(1);
	static constexpr PixelColour GRAPH_ZERO_LINE_COLOUR =  GREY_SCALE(8);
	static constexpr PixelColour GRAPH_YEAR_LINE_COLOUR =  GREY_SCALE(5);
	static const int GRAPH_NUM_MONTHS       =  24; ///< Number of months displayed in the graph.
	static const int GRAPH_PAYMENT_RATE_STEPS = 20; ///< Number of steps on Payment rate graph.
	static const int PAYMENT_GRAPH_X_STEP_DAYS    = 10; ///< X-axis step label for cargo payment rates "Days in transit".
	static const int PAYMENT_GRAPH_X_STEP_SECONDS = 20; ///< X-axis step label for cargo payment rates "Seconds in transit".
	static const int ECONOMY_YEAR_MINUTES = 12;  ///< Minutes per economic year.
	static const int ECONOMY_QUARTER_MINUTES = 3;  ///< Minutes per economic quarter.
	static const int ECONOMY_MONTH_MINUTES = 1;  ///< Minutes per economic month.

	static const TextColour GRAPH_AXIS_LABEL_COLOUR = TC_BLACK; ///< colour of the graph axis label.

	static const int MIN_GRAPH_NUM_LINES_Y  =   9; ///< Minimal number of horizontal lines to draw.
	static const int MIN_GRID_PIXEL_SIZE    =  20; ///< Minimum distance between graph lines.

	struct GraphScale {
		StringID label = STR_NULL;
		uint8_t month_increment = 0;
		int16_t x_values_increment = 0;
		const HistoryRange *history_range = nullptr;
	};

	static inline constexpr GraphScale MONTHLY_SCALE_WALLCLOCK[] = {
		{STR_GRAPH_LAST_24_MINUTES_TIME_LABEL, HISTORY_MONTH.total_division, ECONOMY_MONTH_MINUTES, &HISTORY_MONTH},
		{STR_GRAPH_LAST_72_MINUTES_TIME_LABEL, HISTORY_QUARTER.total_division, ECONOMY_QUARTER_MINUTES, &HISTORY_QUARTER},
		{STR_GRAPH_LAST_288_MINUTES_TIME_LABEL, HISTORY_YEAR.total_division, ECONOMY_YEAR_MINUTES, &HISTORY_YEAR},
	};

	static inline constexpr GraphScale MONTHLY_SCALE_CALENDAR[] = {
		{STR_GRAPH_LAST_24_MONTHS, HISTORY_MONTH.total_division, ECONOMY_MONTH_MINUTES, &HISTORY_MONTH},
		{STR_GRAPH_LAST_24_QUARTERS, HISTORY_QUARTER.total_division, ECONOMY_QUARTER_MINUTES, &HISTORY_QUARTER},
		{STR_GRAPH_LAST_24_YEARS, HISTORY_YEAR.total_division, ECONOMY_YEAR_MINUTES, &HISTORY_YEAR},
	};

	uint64_t excluded_data = 0; ///< bitmask of datasets hidden by the player.
	uint64_t excluded_range = 0; ///< bitmask of ranges hidden by the player.
	uint64_t masked_range = 0; ///< bitmask of ranges that are not available for the current data.
	uint8_t num_on_x_axis = 0;
	uint8_t num_vert_lines = GRAPH_NUM_MONTHS;

	/* The starting month and year that values are plotted against. */
	TimerGameEconomy::Month month{};
	TimerGameEconomy::Year year{};
	uint8_t month_increment = 3; ///< month increment between vertical lines. must be divisor of 12.

	bool draw_dates = true; ///< Should we draw months and years on the time axis?

	/* These values are used if the graph is being plotted against values
	 * rather than the dates specified by month and year. */
	bool x_values_reversed = true;
	int16_t x_values_increment = ECONOMY_QUARTER_MINUTES;

	StringID format_str_y_axis{};

	struct DataSet {
		std::array<OverflowSafeInt64, GRAPH_NUM_MONTHS> values;
		PixelColour colour;
		uint8_t exclude_bit;
		uint8_t range_bit;
		uint8_t dash;
	};
	std::vector<DataSet> data{};

	std::span<const StringID> ranges{};
	std::span<const GraphScale> scales{};
	uint8_t selected_scale = 0;

	uint8_t highlight_data = UINT8_MAX; ///< Data set that should be highlighted, or UINT8_MAX for none.
	uint8_t highlight_range = UINT8_MAX; ///< Data range that should be highlighted, or UINT8_MAX for none.
	bool highlight_state = false; ///< Current state of highlight, toggled every TIMER_BLINK_INTERVAL period.

	struct BaseFiller {
		DataSet &dataset; ///< Dataset to fill.

		inline void MakeZero(uint i) const { this->dataset.values[i] = 0; }
		inline void MakeInvalid(uint i) const { this->dataset.values[i] = INVALID_DATAPOINT; }
	};

	template <typename Tprojection>
	struct Filler : BaseFiller {
		Tprojection proj; ///< Projection to apply.

		inline void Fill(uint i, const auto &data) const { this->dataset.values[i] = std::invoke(this->proj, data); }
	};

	/**
	 * Get appropriate part of dataset values for the current number of horizontal points.
	 * @param dataset Dataset to get values of
	 * @returns span covering dataset's current valid range.
	 */
	std::span<const OverflowSafeInt64> GetDataSetRange(const DataSet &dataset) const
	{
		return {std::begin(dataset.values), std::begin(dataset.values) + this->num_on_x_axis};
	}

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

		for (const DataSet &dataset : this->data) {
			if (HasBit(this->excluded_data, dataset.exclude_bit)) continue;
			if (HasBit(this->excluded_range, dataset.range_bit)) continue;

			for (const OverflowSafeInt64 &datapoint : this->GetDataSetRange(dataset)) {
				if (datapoint != INVALID_DATAPOINT) {
					current_interval.highest = std::max(current_interval.highest, datapoint);
					current_interval.lowest  = std::min(current_interval.lowest, datapoint);
				}
			}
		}

		/* Always include zero in the shown range. */
		double abs_lower  = (current_interval.lowest > 0) ? 0 : (double)abs(current_interval.lowest);
		double abs_higher = (current_interval.highest < 0) ? 0 : (double)current_interval.highest;

		/* Prevent showing values too close to the graph limits. */
		abs_higher = (11.0 * abs_higher) / 10.0;
		abs_lower = (11.0 * abs_lower) / 10.0;

		int num_pos_grids;
		OverflowSafeInt64 grid_size;

		if (abs_lower != 0 || abs_higher != 0) {
			/* The number of grids to reserve for the positive part is: */
			num_pos_grids = (int)floor(0.5 + num_hori_lines * abs_higher / (abs_higher + abs_lower));

			/* If there are any positive or negative values, force that they have at least one grid. */
			if (num_pos_grids == 0 && abs_higher != 0) num_pos_grids++;
			if (num_pos_grids == num_hori_lines && abs_lower != 0) num_pos_grids--;

			/* Get the required grid size for each side and use the maximum one. */

			OverflowSafeInt64 grid_size_higher = 0;
			if (abs_higher > 0) {
				grid_size_higher = abs_higher > INT64_MAX_IN_DOUBLE ? INT64_MAX : static_cast<int64_t>(abs_higher);
				grid_size_higher = (grid_size_higher + num_pos_grids - 1) / num_pos_grids;
			}

			OverflowSafeInt64 grid_size_lower = 0;
			if (abs_lower > 0) {
				grid_size_lower = abs_lower > INT64_MAX_IN_DOUBLE ? INT64_MAX : static_cast<int64_t>(abs_lower);
				grid_size_lower = (grid_size_lower + num_hori_lines - num_pos_grids - 1) / (num_hori_lines - num_pos_grids);
			}

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
			Dimension d = GetStringBoundingBox(GetString(STR_GRAPH_Y_LABEL, this->format_str_y_axis, y_label));
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

		bool rtl = _current_text_dir == TD_RTL;

		/* Rect r will be adjusted to contain just the graph, with labels being
		 * placed outside the area. */
		r.top    += ScaleGUITrad(5) + GetCharacterHeight(FS_SMALL) / 2;
		r.bottom -= (this->draw_dates ? 2 : 1) * GetCharacterHeight(FS_SMALL) + ScaleGUITrad(4);
		r.left   += ScaleGUITrad(rtl ? 5 : 9);
		r.right  -= ScaleGUITrad(rtl ? 9 : 5);

		/* Initial number of horizontal lines. */
		int num_hori_lines = 160 / ScaleGUITrad(MIN_GRID_PIXEL_SIZE);
		/* For the rest of the height, the number of horizontal lines will increase more slowly. */
		int resize = (r.bottom - r.top - 160) / (2 * ScaleGUITrad(MIN_GRID_PIXEL_SIZE));
		if (resize > 0) num_hori_lines += resize;

		interval = GetValuesInterval(num_hori_lines);

		int label_width = GetYLabelWidth(interval, num_hori_lines);

		if (rtl) {
			r.right -= label_width;
		} else {
			r.left += label_width;
		}

		int x_sep = (r.right - r.left) / this->num_vert_lines;
		int y_sep = (r.bottom - r.top) / num_hori_lines;

		/* Redetermine right and bottom edge of graph to fit with the integer
		 * separation values. */
		if (rtl) {
			r.left = r.right - x_sep * this->num_vert_lines;
		} else {
			r.right = r.left + x_sep * this->num_vert_lines;
		}
		r.bottom = r.top + y_sep * num_hori_lines;

		OverflowSafeInt64 interval_size = interval.highest + abs(interval.lowest);
		/* Where to draw the X axis. Use floating point to avoid overflowing and results of zero. */
		x_axis_offset = (int)((r.bottom - r.top) * (double)interval.highest / (double)interval_size);

		/* Draw the background of the graph itself. */
		GfxFillRect(r.left, r.top, r.right, r.bottom, GRAPH_BASE_COLOUR);

		/* Draw the grid lines. */
		int gridline_width = WidgetDimensions::scaled.bevel.top;
		PixelColour grid_colour = GRAPH_GRID_COLOUR;

		/* Don't draw the first line, as that's where the axis will be. */
		if (rtl) {
			x_sep = -x_sep;
			x = r.right + x_sep;
		} else {
			x = r.left + x_sep;
		}

		for (int i = 1; i < this->num_vert_lines + 1; i++) {
			/* If using wallclock units, we separate periods with a lighter line. */
			if (TimerGameEconomy::UsingWallclockUnits()) {
				grid_colour = (i % 4 == 0) ? GRAPH_YEAR_LINE_COLOUR : GRAPH_GRID_COLOUR;
			}
			GfxFillRect(x, r.top, x + gridline_width - 1, r.bottom, grid_colour);
			x += x_sep;
		}

		/* Draw the horizontal grid lines. */
		y = r.bottom;

		for (int i = 0; i < (num_hori_lines + 1); i++) {
			if (rtl) {
				GfxFillRect(r.right + 1, y, r.right + ScaleGUITrad(3), y + gridline_width - 1, GRAPH_AXIS_LINE_COLOUR);
			} else {
				GfxFillRect(r.left - ScaleGUITrad(3), y, r.left - 1, y + gridline_width - 1, GRAPH_AXIS_LINE_COLOUR);
			}
			GfxFillRect(r.left, y, r.right + gridline_width - 1, y + gridline_width - 1, GRAPH_GRID_COLOUR);
			y -= y_sep;
		}

		/* Draw the y axis. */
		GfxFillRect(r.left, r.top, r.left + gridline_width - 1, r.bottom + gridline_width - 1, GRAPH_AXIS_LINE_COLOUR);

		/* Draw the x axis. */
		y = x_axis_offset + r.top;
		GfxFillRect(r.left, y, r.right + gridline_width - 1, y + gridline_width - 1, GRAPH_ZERO_LINE_COLOUR);

		/* Find the largest value that will be drawn. */
		if (this->num_on_x_axis == 0) return;

		assert(this->num_on_x_axis > 0);

		/* draw text strings on the y axis */
		int64_t y_label = interval.highest;
		int64_t y_label_separation = abs(interval.highest - interval.lowest) / num_hori_lines;

		y = r.top - GetCharacterHeight(FS_SMALL) / 2;

		for (int i = 0; i < (num_hori_lines + 1); i++) {
			if (rtl) {
				DrawString(r.right + ScaleGUITrad(4), r.right + label_width + ScaleGUITrad(4), y,
					GetString(STR_GRAPH_Y_LABEL, this->format_str_y_axis, y_label),
					GRAPH_AXIS_LABEL_COLOUR, SA_RIGHT | SA_FORCE);
			} else {
				DrawString(r.left - label_width - ScaleGUITrad(4), r.left - ScaleGUITrad(4), y,
					GetString(STR_GRAPH_Y_LABEL, this->format_str_y_axis, y_label),
					GRAPH_AXIS_LABEL_COLOUR, SA_RIGHT | SA_FORCE);
			}

			y_label -= y_label_separation;
			y += y_sep;
		}

		x = rtl ? r.right : r.left;
		y = r.bottom + ScaleGUITrad(2);

		/* if there are not enough datapoints to fill the graph, align to the right */
		x += (this->num_vert_lines - this->num_on_x_axis) * x_sep;

		/* Draw x-axis labels and markings for graphs based on financial quarters and years.  */
		if (this->draw_dates) {
			TimerGameEconomy::Month mo = this->month;
			TimerGameEconomy::Year yr = this->year;
			for (int i = 0; i < this->num_on_x_axis; i++) {
				if (rtl) {
					DrawStringMultiLineWithClipping(x + x_sep, x, y, this->height,
						GetString(mo == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH, STR_MONTH_ABBREV_JAN + mo, yr),
						GRAPH_AXIS_LABEL_COLOUR, SA_LEFT);
				} else {
					DrawStringMultiLineWithClipping(x, x + x_sep, y, this->height,
						GetString(mo == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH, STR_MONTH_ABBREV_JAN + mo, yr),
						GRAPH_AXIS_LABEL_COLOUR, SA_LEFT);
				}

				mo += this->month_increment;
				if (mo >= 12) {
					mo = 0;
					yr++;

					/* Draw a lighter grid line between years. Top and bottom adjustments ensure we don't draw over top and bottom horizontal grid lines. */
					GfxFillRect(x + x_sep, r.top + gridline_width, x + x_sep + gridline_width - 1, r.bottom - 1, GRAPH_YEAR_LINE_COLOUR);
				}
				x += x_sep;
			}
		} else {
			/* Draw x-axis labels for graphs not based on quarterly performance (cargo payment rates, and all graphs when using wallclock units). */
			int16_t iterator;
			uint16_t label;
			if (this->x_values_reversed) {
				label = this->x_values_increment * this->num_on_x_axis;
				iterator = -this->x_values_increment;
			} else {
				label = this->x_values_increment;
				iterator = this->x_values_increment;
			}

			for (int i = 0; i < this->num_on_x_axis; i++) {
				if (rtl) {
					DrawString(x + x_sep + 1, x - 1, y, GetString(STR_GRAPH_Y_LABEL_NUMBER, label), GRAPH_AXIS_LABEL_COLOUR, SA_HOR_CENTER);
				} else {
					DrawString(x + 1, x + x_sep - 1, y, GetString(STR_GRAPH_Y_LABEL_NUMBER, label), GRAPH_AXIS_LABEL_COLOUR, SA_HOR_CENTER);
				}

				label += iterator;
				x += x_sep;
			}
		}

		/* Draw lines and dots. */
		uint linewidth = ScaleGUITrad(_settings_client.gui.graph_line_thickness);
		uint pointwidth = ScaleGUITrad(_settings_client.gui.graph_line_thickness + 1);
		uint pointoffs1 = pointwidth / 2;
		uint pointoffs2 = pointwidth - pointoffs1;

		auto draw_dataset = [&](const DataSet &dataset, PixelColour colour) {
			if (HasBit(this->excluded_data, dataset.exclude_bit)) return;
			if (HasBit(this->excluded_range, dataset.range_bit)) return;

			/* Centre the dot between the grid lines. */
			if (rtl) {
				x = r.right + (x_sep / 2);
			} else {
				x = r.left + (x_sep / 2);
			}

			/* if there are not enough datapoints to fill the graph, align to the right */
			x += (this->num_vert_lines - this->num_on_x_axis) * x_sep;

			uint prev_x = INVALID_DATAPOINT_POS;
			uint prev_y = INVALID_DATAPOINT_POS;

			const uint dash = ScaleGUITrad(dataset.dash);
			for (OverflowSafeInt64 datapoint : this->GetDataSetRange(dataset)) {
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
					int mult_range = FindLastBit<uint32_t>(x_axis_offset) + FindLastBit<uint64_t>(abs(datapoint));
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
					if (prev_x != INVALID_DATAPOINT_POS) GfxDrawLine(prev_x, prev_y, x, y, colour, linewidth, dash);

					prev_x = x;
					prev_y = y;
				} else {
					prev_x = INVALID_DATAPOINT_POS;
					prev_y = INVALID_DATAPOINT_POS;
				}

				x += x_sep;
			}
		};

		/* Draw unhighlighted datasets. */
		for (const DataSet &dataset : this->data) {
			if (dataset.exclude_bit != this->highlight_data && dataset.range_bit != this->highlight_range) {
				draw_dataset(dataset, dataset.colour);
			}
		}

		/* If any dataset or range is highlighted, draw separately after the rest so they appear on top of all other
		 * data. Highlighted data is only drawn when highlight_state is set, otherwise it is invisible. */
		if (this->highlight_state && (this->highlight_data != UINT8_MAX || this->highlight_range != UINT8_MAX)) {
			for (const DataSet &dataset : this->data) {
				if (dataset.exclude_bit == this->highlight_data || dataset.range_bit == this->highlight_range) {
					draw_dataset(dataset, PC_WHITE);
				}
			}
		}
	}

	BaseGraphWindow(WindowDesc &desc, StringID format_str_y_axis) :
			Window(desc),
			format_str_y_axis(format_str_y_axis)
	{
		SetWindowDirty(WC_GRAPH_LEGEND, 0);
	}

	const IntervalTimer<TimerWindow> blink_interval = {TIMER_BLINK_INTERVAL, [this](auto) {
		/* If nothing is highlighted then no redraw is needed. */
		if (this->highlight_data == UINT8_MAX && this->highlight_range == UINT8_MAX) return;

		/* Toggle the highlight state and redraw. */
		this->highlight_state = !this->highlight_state;
		this->SetDirty();
	}};

	void UpdateMatrixSize(WidgetID widget, Dimension &size, Dimension &resize, auto labels)
	{
		size = {};
		for (const StringID &str : labels) {
			size = maxdim(size, GetStringBoundingBox(str, FS_SMALL));
		}

		size.width += WidgetDimensions::scaled.framerect.Horizontal();
		size.height += WidgetDimensions::scaled.framerect.Vertical();

		/* Set fixed height for number of ranges. */
		size.height *= static_cast<uint>(std::size(labels));

		resize.width = 0;
		resize.height = 0;
		this->GetWidget<NWidgetCore>(widget)->SetMatrixDimension(1, ClampTo<uint32_t>(std::size(labels)));
	}

public:
	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_GRAPH_RANGE_MATRIX:
				this->UpdateMatrixSize(widget, size, resize, this->ranges);
				break;

			case WID_GRAPH_SCALE_MATRIX:
				this->UpdateMatrixSize(widget, size, resize, this->scales | std::views::transform(&GraphScale::label));
				break;

			case WID_GRAPH_GRAPH: {
				uint x_label_width = 0;

				/* Draw x-axis labels and markings for graphs based on financial quarters and years.  */
				if (this->draw_dates) {
					uint yr = GetParamMaxValue(this->year.base(), 4, FS_SMALL);
					for (uint mo = 0; mo < 12; ++mo) {
						x_label_width = std::max(x_label_width, GetStringBoundingBox(GetString(mo == 0 ? STR_GRAPH_X_LABEL_MONTH_YEAR : STR_GRAPH_X_LABEL_MONTH, STR_MONTH_ABBREV_JAN + mo, yr)).width);
					}
				} else {
					/* Draw x-axis labels for graphs not based on quarterly performance (cargo payment rates). */
					uint64_t max_value = GetParamMaxValue((this->num_on_x_axis + 1) * this->x_values_increment, 0, FS_SMALL);
					x_label_width = GetStringBoundingBox(GetString(STR_GRAPH_Y_LABEL_NUMBER, max_value)).width;
				}

				uint y_label_width = GetStringBoundingBox(GetString(STR_GRAPH_Y_LABEL, this->format_str_y_axis, INT64_MAX)).width;

				size.width  = std::max<uint>(size.width,  ScaleGUITrad(5) + y_label_width + this->num_vert_lines * (x_label_width + ScaleGUITrad(5)) + ScaleGUITrad(9));
				size.height = std::max<uint>(size.height, ScaleGUITrad(5) + (1 + MIN_GRAPH_NUM_LINES_Y * 2 + (this->draw_dates ? 3 : 1)) * GetCharacterHeight(FS_SMALL) + ScaleGUITrad(4));
				size.height = std::max<uint>(size.height, size.width / 3);
				break;
			}

			default: break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_GRAPH_GRAPH:
				this->DrawGraph(r);
				break;

			case WID_GRAPH_RANGE_MATRIX: {
				uint line_height = GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical();
				uint index = 0;
				Rect line = r.WithHeight(line_height);
				for (const auto &str : this->ranges) {
					bool lowered = !HasBit(this->excluded_range, index) && !HasBit(this->masked_range, index);

					/* Redraw frame if lowered */
					if (lowered) DrawFrameRect(line, COLOUR_BROWN, FrameFlag::Lowered);

					const Rect text = line.Shrink(WidgetDimensions::scaled.framerect);
					DrawString(text, str, (this->highlight_state && this->highlight_range == index) ? TC_WHITE : TC_BLACK, SA_CENTER, false, FS_SMALL);

					if (HasBit(this->masked_range, index)) {
						GfxFillRect(line.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(COLOUR_BROWN, SHADE_DARKER), FILLRECT_CHECKER);
					}

					line = line.Translate(0, line_height);
					++index;
				}
				break;
			}

			case WID_GRAPH_SCALE_MATRIX: {
				uint line_height = GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical();
				uint8_t selected_month_increment = this->scales[this->selected_scale].month_increment;
				Rect line = r.WithHeight(line_height);
				for (const auto &scale : this->scales) {
					/* Redraw frame if selected */
					if (selected_month_increment == scale.month_increment) DrawFrameRect(line, COLOUR_BROWN, FrameFlag::Lowered);

					DrawString(line.Shrink(WidgetDimensions::scaled.framerect), scale.label, TC_BLACK, SA_CENTER, false, FS_SMALL);

					line = line.Translate(0, line_height);
				}
				break;
			}

			default: break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* Clicked on legend? */
		switch (widget) {
			case WID_GRAPH_KEY_BUTTON:
				ShowGraphLegend();
				break;

			case WID_GRAPH_RANGE_MATRIX: {
				int row = GetRowFromWidget(pt.y, widget, 0, GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical());

				if (HasBit(this->masked_range, row)) break;
				ToggleBit(this->excluded_range, row);
				SndClickBeep();
				this->SetDirty();
				break;
			}

			case WID_GRAPH_SCALE_MATRIX: {
				int row = GetRowFromWidget(pt.y, widget, 0, GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical());
				const auto &scale = this->scales[row];
				if (this->selected_scale != row) {
					this->selected_scale = row;
					this->month_increment = scale.month_increment;
					this->x_values_increment = scale.x_values_increment;
					this->InvalidateData();
				}
				break;
			}

			default: break;
		}
	}

	void OnMouseOver(Point pt, WidgetID widget) override
	{
		/* Test if a range should be highlighted. */
		uint8_t new_highlight_range = UINT8_MAX;
		if (widget == WID_GRAPH_RANGE_MATRIX) {
			int row = GetRowFromWidget(pt.y, widget, 0, GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical());
			if (!HasBit(this->excluded_range, row)) new_highlight_range = static_cast<uint8_t>(row);
		}

		/* Test if a dataset should be highlighted. */
		uint8_t new_highlight_data = UINT8_MAX;
		if (widget == WID_GRAPH_MATRIX) {
			auto dataset_index = this->GetDatasetIndex(pt.y);
			if (dataset_index.has_value() && !HasBit(this->excluded_data, *dataset_index)) new_highlight_data = *dataset_index;
		}

		if (this->highlight_data == new_highlight_data && this->highlight_range == new_highlight_range) return;

		/* Range or data set highlight has changed, set and redraw. */
		this->highlight_data = new_highlight_data;
		this->highlight_range = new_highlight_range;
		this->highlight_state = true;
		this->SetDirty();
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

	virtual void UpdateStatistics(bool initialize) = 0;

	virtual std::optional<uint8_t> GetDatasetIndex(int) { return std::nullopt; }
};

class BaseCompanyGraphWindow : public BaseGraphWindow {
public:
	BaseCompanyGraphWindow(WindowDesc &desc, StringID format_str_y_axis) : BaseGraphWindow(desc, format_str_y_axis) {}

	void InitializeWindow(WindowNumber number)
	{
		/* Initialise the dataset */
		this->UpdateStatistics(true);

		this->CreateNestedTree();

		auto *wid = this->GetWidget<NWidgetCore>(WID_GRAPH_FOOTER);
		wid->SetString(TimerGameEconomy::UsingWallclockUnits() ? STR_GRAPH_LAST_72_MINUTES_TIME_LABEL : STR_EMPTY);

		this->FinishInitNested(number);
	}

	/**
	 * Update the statistics.
	 * @param initialize Initialize the data structure.
	 */
	void UpdateStatistics(bool initialize) override
	{
		CompanyMask excluded_companies = _legend_excluded_companies;

		/* Exclude the companies which aren't valid */
		for (CompanyID c = CompanyID::Begin(); c < MAX_COMPANIES; ++c) {
			if (!Company::IsValidID(c)) excluded_companies.Set(c);
		}

		uint8_t nums = 0;
		for (const Company *c : Company::Iterate()) {
			nums = std::min(this->num_vert_lines, std::max(nums, c->num_valid_stat_ent));
		}

		int mo = (TimerGameEconomy::month / this->month_increment - nums) * this->month_increment;
		auto yr = TimerGameEconomy::year;
		while (mo < 0) {
			yr--;
			mo += 12;
		}

		if (!initialize && this->excluded_data == excluded_companies.base() && this->num_on_x_axis == nums &&
				this->year == yr && this->month == mo) {
			/* There's no reason to get new stats */
			return;
		}

		this->excluded_data = excluded_companies.base();
		this->num_on_x_axis = nums;
		this->year = yr;
		this->month = mo;

		this->data.clear();
		for (CompanyID k = CompanyID::Begin(); k < MAX_COMPANIES; ++k) {
			const Company *c = Company::GetIfValid(k);
			if (c == nullptr) continue;

			DataSet &dataset = this->data.emplace_back();
			dataset.colour = GetColourGradient(c->colour, SHADE_LIGHTER);
			dataset.exclude_bit = k.base();

			for (int j = this->num_on_x_axis, i = 0; --j >= 0;) {
				if (j >= c->num_valid_stat_ent) {
					dataset.values[i] = INVALID_DATAPOINT;
				} else {
					/* Ensure we never assign INVALID_DATAPOINT, as that has another meaning.
					 * Instead, use the value just under it. Hopefully nobody will notice. */
					dataset.values[i] = std::min(GetGraphData(c, j), INVALID_DATAPOINT - 1);
				}
				i++;
			}
		}
	}

	virtual OverflowSafeInt64 GetGraphData(const Company *, int) = 0;
};


/********************/
/* OPERATING PROFIT */
/********************/

struct OperatingProfitGraphWindow : BaseCompanyGraphWindow {
	OperatingProfitGraphWindow(WindowDesc &desc, WindowNumber window_number) :
			BaseCompanyGraphWindow(desc, STR_JUST_CURRENCY_SHORT)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();

		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].income + c->old_economy[j].expenses;
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_operating_profit_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_OPERATING_PROFIT_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_KEY_BUTTON), SetMinimalSize(50, 0), SetStringTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(576, 160), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _operating_profit_desc(
	WDP_AUTO, "graph_operating_profit", 0, 0,
	WC_OPERATING_PROFIT, WC_NONE,
	{},
	_nested_operating_profit_widgets
);


void ShowOperatingProfitGraph()
{
	AllocateWindowDescFront<OperatingProfitGraphWindow>(_operating_profit_desc, 0);
}


/****************/
/* INCOME GRAPH */
/****************/

struct IncomeGraphWindow : BaseCompanyGraphWindow {
	IncomeGraphWindow(WindowDesc &desc, WindowNumber window_number) :
			BaseCompanyGraphWindow(desc, STR_JUST_CURRENCY_SHORT)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();

		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].income;
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_income_graph_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_INCOME_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_KEY_BUTTON), SetMinimalSize(50, 0), SetStringTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(576, 128), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _income_graph_desc(
	WDP_AUTO, "graph_income", 0, 0,
	WC_INCOME_GRAPH, WC_NONE,
	{},
	_nested_income_graph_widgets
);

void ShowIncomeGraph()
{
	AllocateWindowDescFront<IncomeGraphWindow>(_income_graph_desc, 0);
}

/*******************/
/* DELIVERED CARGO */
/*******************/

struct DeliveredCargoGraphWindow : BaseCompanyGraphWindow {
	DeliveredCargoGraphWindow(WindowDesc &desc, WindowNumber window_number) :
			BaseCompanyGraphWindow(desc, STR_JUST_COMMA)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();

		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].delivered_cargo.GetSum<OverflowSafeInt64>();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_delivered_cargo_graph_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_CARGO_DELIVERED_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_KEY_BUTTON), SetMinimalSize(50, 0), SetStringTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(576, 128), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _delivered_cargo_graph_desc(
	WDP_AUTO, "graph_delivered_cargo", 0, 0,
	WC_DELIVERED_CARGO, WC_NONE,
	{},
	_nested_delivered_cargo_graph_widgets
);

void ShowDeliveredCargoGraph()
{
	AllocateWindowDescFront<DeliveredCargoGraphWindow>(_delivered_cargo_graph_desc, 0);
}

/***********************/
/* PERFORMANCE HISTORY */
/***********************/

struct PerformanceHistoryGraphWindow : BaseCompanyGraphWindow {
	PerformanceHistoryGraphWindow(WindowDesc &desc, WindowNumber window_number) :
			BaseCompanyGraphWindow(desc, STR_JUST_COMMA)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();

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

static constexpr std::initializer_list<NWidgetPart> _nested_performance_history_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_COMPANY_PERFORMANCE_RATINGS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_PHG_DETAILED_PERFORMANCE), SetMinimalSize(50, 0), SetStringTip(STR_PERFORMANCE_DETAIL_KEY, STR_GRAPH_PERFORMANCE_DETAIL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_KEY_BUTTON), SetMinimalSize(50, 0), SetStringTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(576, 224), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _performance_history_desc(
	WDP_AUTO, "graph_performance", 0, 0,
	WC_PERFORMANCE_HISTORY, WC_NONE,
	{},
	_nested_performance_history_widgets
);

void ShowPerformanceHistoryGraph()
{
	AllocateWindowDescFront<PerformanceHistoryGraphWindow>(_performance_history_desc, 0);
}

/*****************/
/* COMPANY VALUE */
/*****************/

struct CompanyValueGraphWindow : BaseCompanyGraphWindow {
	CompanyValueGraphWindow(WindowDesc &desc, WindowNumber window_number) :
			BaseCompanyGraphWindow(desc, STR_JUST_CURRENCY_SHORT)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();

		this->InitializeWindow(window_number);
	}

	OverflowSafeInt64 GetGraphData(const Company *c, int j) override
	{
		return c->old_economy[j].company_value;
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_company_value_graph_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_COMPANY_VALUES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_KEY_BUTTON), SetMinimalSize(50, 0), SetStringTip(STR_GRAPH_KEY_BUTTON, STR_GRAPH_KEY_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(576, 224), SetFill(1, 1), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _company_value_graph_desc(
	WDP_AUTO, "graph_company_value", 0, 0,
	WC_COMPANY_VALUE, WC_NONE,
	{},
	_nested_company_value_graph_widgets
);

void ShowCompanyValueGraph()
{
	AllocateWindowDescFront<CompanyValueGraphWindow>(_company_value_graph_desc, 0);
}

struct BaseCargoGraphWindow : BaseGraphWindow {
	Scrollbar *vscroll = nullptr; ///< Cargo list scrollbar.
	uint line_height = 0; ///< Pixel height of each cargo type row.
	uint legend_width = 0; ///< Width of legend 'blob'.

	CargoTypes cargo_types{}; ///< Cargo types that can be selected.

	BaseCargoGraphWindow(WindowDesc &desc, StringID format_str_y_axis) : BaseGraphWindow(desc, format_str_y_axis) {}

	void InitializeWindow(WindowNumber number, StringID footer_wallclock = STR_NULL, StringID footer_calendar = STR_NULL)
	{
		this->CreateNestedTree();

		this->excluded_range = this->masked_range;
		this->cargo_types = this->GetCargoTypes(number);

		this->vscroll = this->GetScrollbar(WID_GRAPH_MATRIX_SCROLLBAR);
		this->vscroll->SetCount(CountBits(this->cargo_types));

		auto *wid = this->GetWidget<NWidgetCore>(WID_GRAPH_FOOTER);
		wid->SetString(TimerGameEconomy::UsingWallclockUnits() ? footer_wallclock : footer_calendar);

		this->FinishInitNested(number);

		/* Initialise the dataset */
		this->InvalidateData();
	}

	virtual CargoTypes GetCargoTypes(WindowNumber number) const = 0;
	virtual CargoTypes &GetExcludedCargoTypes() const = 0;

	std::optional<uint8_t> GetDatasetIndex(int y) override
	{
		int row = this->vscroll->GetScrolledRowFromWidget(y, this, WID_GRAPH_MATRIX);
		if (row >= this->vscroll->GetCount()) return std::nullopt;

		for (const CargoSpec *cs : _sorted_cargo_specs) {
			if (!HasBit(this->cargo_types, cs->Index())) continue;
			if (row-- > 0) continue;

			return cs->Index();
		}

		return std::nullopt;
	}

	void OnInit() override
	{
		/* Width of the legend blob. */
		this->legend_width = GetCharacterHeight(FS_SMALL) * 9 / 6;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_GRAPH_MATRIX) {
			BaseGraphWindow::UpdateWidgetSize(widget, size, padding, fill, resize);
			return;
		}

		size.height = GetCharacterHeight(FS_SMALL) + WidgetDimensions::scaled.framerect.Vertical();

		for (CargoType cargo_type : SetCargoBitIterator(this->cargo_types)) {
			const CargoSpec *cs = CargoSpec::Get(cargo_type);

			Dimension d = GetStringBoundingBox(GetString(STR_GRAPH_CARGO_PAYMENT_CARGO, cs->name));
			d.width += this->legend_width + WidgetDimensions::scaled.hsep_normal; // colour field
			d.width += WidgetDimensions::scaled.framerect.Horizontal();
			d.height += WidgetDimensions::scaled.framerect.Vertical();
			size = maxdim(d, size);
		}

		this->line_height = size.height;
		size.height = this->line_height * 11; /* Default number of cargo types in most climates. */
		resize.width = 0;
		fill.height = resize.height = this->line_height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_GRAPH_MATRIX) {
			BaseGraphWindow::DrawWidget(r, widget);
			return;
		}

		bool rtl = _current_text_dir == TD_RTL;

		int pos = this->vscroll->GetPosition();
		int max = pos + this->vscroll->GetCapacity();

		Rect line = r.WithHeight(this->line_height);

		for (const CargoSpec *cs : _sorted_cargo_specs) {
			if (!HasBit(this->cargo_types, cs->Index())) continue;

			if (pos-- > 0) continue;
			if (--max < 0) break;

			bool lowered = !HasBit(this->excluded_data, cs->Index());

			/* Redraw frame if lowered */
			if (lowered) DrawFrameRect(line, COLOUR_BROWN, FrameFlag::Lowered);

			const Rect text = line.Shrink(WidgetDimensions::scaled.framerect);

			/* Cargo-colour box with outline */
			const Rect cargo = text.WithWidth(this->legend_width, rtl);
			GfxFillRect(cargo, PC_BLACK);
			PixelColour pc = cs->legend_colour;
			if (this->highlight_data == cs->Index()) pc = this->highlight_state ? PC_WHITE : PC_BLACK;
			GfxFillRect(cargo.Shrink(WidgetDimensions::scaled.bevel), pc);

			/* Cargo name */
			DrawString(text.Indent(this->legend_width + WidgetDimensions::scaled.hsep_normal, rtl), GetString(STR_GRAPH_CARGO_PAYMENT_CARGO, cs->name));

			line = line.Translate(0, this->line_height);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GRAPH_ENABLE_CARGOES:
				/* Remove all cargoes from the excluded lists. */
				this->GetExcludedCargoTypes() = {};
				this->excluded_data = this->GetExcludedCargoTypes();
				this->SetDirty();
				break;

			case WID_GRAPH_DISABLE_CARGOES: {
				/* Add all cargoes to the excluded lists. */
				this->GetExcludedCargoTypes() = this->cargo_types;
				this->excluded_data = this->GetExcludedCargoTypes();
				this->SetDirty();
				break;
			}

			case WID_GRAPH_MATRIX: {
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GRAPH_MATRIX);
				if (row >= this->vscroll->GetCount()) return;

				SndClickBeep();

				for (const CargoSpec *cs : _sorted_cargo_specs) {
					if (!HasBit(this->cargo_types, cs->Index())) continue;
					if (row-- > 0) continue;

					ToggleBit(this->GetExcludedCargoTypes(), cs->Index());
					this->excluded_data = this->GetExcludedCargoTypes();
					this->SetDirty();
					break;
				}
				break;
			}

			default:
				this->BaseGraphWindow::OnClick(pt, widget, click_count);
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_GRAPH_MATRIX);
	}
};

/*****************/
/* PAYMENT RATES */
/*****************/

struct PaymentRatesGraphWindow : BaseCargoGraphWindow {
	static inline CargoTypes excluded_cargo_types{};

	PaymentRatesGraphWindow(WindowDesc &desc, WindowNumber window_number) : BaseCargoGraphWindow(desc, STR_JUST_CURRENCY_SHORT)
	{
		this->num_on_x_axis = GRAPH_PAYMENT_RATE_STEPS;
		this->num_vert_lines = GRAPH_PAYMENT_RATE_STEPS;
		this->draw_dates = false;

		this->x_values_reversed = false;
		/* The x-axis is labeled in either seconds or days. A day is two seconds, so we adjust the label if needed. */
		this->x_values_increment = (TimerGameEconomy::UsingWallclockUnits() ? PAYMENT_GRAPH_X_STEP_SECONDS : PAYMENT_GRAPH_X_STEP_DAYS);

		this->InitializeWindow(window_number, STR_GRAPH_CARGO_PAYMENT_RATES_SECONDS, STR_GRAPH_CARGO_PAYMENT_RATES_DAYS);
	}

	CargoTypes GetCargoTypes(WindowNumber) const override
	{
		return _standard_cargo_mask;
	}

	CargoTypes &GetExcludedCargoTypes() const override
	{
		return PaymentRatesGraphWindow::excluded_cargo_types;
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
	const IntervalTimer<TimerWindow> update_payment_interval = {std::chrono::seconds(3), [this](auto) {
		this->UpdatePaymentRates();
	}};

	void UpdateStatistics(bool) override {}

	/**
	 * Update the payment rates according to the latest information.
	 */
	void UpdatePaymentRates()
	{
		this->excluded_data = this->GetExcludedCargoTypes();

		this->data.clear();
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			DataSet &dataset = this->data.emplace_back();
			dataset.colour = cs->legend_colour;
			dataset.exclude_bit = cs->Index();

			for (uint j = 0; j != this->num_on_x_axis; j++) {
				dataset.values[j] = GetTransportedGoodsIncome(10, 20, j * 4 + 4, cs->Index());
			}
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_cargo_payment_rates_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_GRAPH_CARGO_PAYMENT_RATES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND), SetMinimalSize(568, 128),
		NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_HEADER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetStringTip(STR_GRAPH_CARGO_PAYMENT_RATES_TITLE), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(495, 0), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_ENABLE_CARGOES), SetStringTip(STR_GRAPH_CARGO_ENABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_ENABLE_ALL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_DISABLE_CARGOES), SetStringTip(STR_GRAPH_CARGO_DISABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_DISABLE_ALL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_MATRIX), SetFill(1, 0), SetResize(0, 2), SetMatrixDataTip(1, 0, STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO), SetScrollbar(WID_GRAPH_MATRIX_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_GRAPH_MATRIX_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(0, 1), SetResize(0, 1),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _cargo_payment_rates_desc(
	WDP_AUTO, "graph_cargo_payment_rates", 0, 0,
	WC_PAYMENT_RATES, WC_NONE,
	{},
	_nested_cargo_payment_rates_widgets
);


void ShowCargoPaymentRates()
{
	AllocateWindowDescFront<PaymentRatesGraphWindow>(_cargo_payment_rates_desc, 0);
}

/*****************************/
/* PERFORMANCE RATING DETAIL */
/*****************************/

struct PerformanceRatingDetailWindow : Window {
	static CompanyID company;
	int timeout = 0;
	uint score_info_left = 0;
	uint score_info_right = 0;
	uint bar_left = 0;
	uint bar_right = 0;
	uint bar_width = 0;
	uint bar_height = 0;
	uint score_detail_left = 0;
	uint score_detail_right = 0;

	PerformanceRatingDetailWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->UpdateCompanyStats();

		this->InitNested(window_number);
		this->OnInvalidateData(CompanyID::Invalid().base());
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

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_PRD_SCORE_FIRST:
				this->bar_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.fullbevel.Vertical();
				size.height = this->bar_height + WidgetDimensions::scaled.matrix.Vertical();

				uint score_info_width = 0;
				for (uint i = SCORE_BEGIN; i < SCORE_END; i++) {
					score_info_width = std::max(score_info_width, GetStringBoundingBox(STR_PERFORMANCE_DETAIL_VEHICLES + i).width);
				}
				score_info_width += GetStringBoundingBox(GetString(STR_JUST_COMMA, GetParamMaxValue(1000))).width + WidgetDimensions::scaled.hsep_wide;

				this->bar_width = GetStringBoundingBox(GetString(STR_PERFORMANCE_DETAIL_PERCENT, GetParamMaxValue(100))).width + WidgetDimensions::scaled.hsep_indent * 2; // Wide bars!

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
				if (GetCurrency().rate < 1000) max /= GetCurrency().rate;
				uint score_detail_width = GetStringBoundingBox(GetString(STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY, max, max)).width;

				size.width = WidgetDimensions::scaled.frametext.Horizontal() + score_info_width + WidgetDimensions::scaled.hsep_wide + this->bar_width + WidgetDimensions::scaled.hsep_wide + score_detail_width;
				uint left  = WidgetDimensions::scaled.frametext.left;
				uint right = size.width - WidgetDimensions::scaled.frametext.right;

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
		if (this->company == CompanyID::Invalid()) return;

		if (IsInsideMM(widget, WID_PRD_COMPANY_FIRST, WID_PRD_COMPANY_LAST + 1)) {
			if (this->IsWidgetDisabled(widget)) return;
			CompanyID cid = (CompanyID)(widget - WID_PRD_COMPANY_FIRST);
			Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
			DrawCompanyIcon(cid, CentreBounds(r.left, r.right, sprite_size.width), CentreBounds(r.top, r.bottom, sprite_size.height));
			return;
		}

		if (!IsInsideMM(widget, WID_PRD_SCORE_FIRST, WID_PRD_SCORE_LAST + 1)) return;

		ScoreID score_type = (ScoreID)(widget - WID_PRD_SCORE_FIRST);

		/* The colours used to show how the progress is going */
		PixelColour colour_done = GetColourGradient(COLOUR_GREEN, SHADE_NORMAL);
		PixelColour colour_notdone = GetColourGradient(COLOUR_RED, SHADE_NORMAL);

		/* Draw all the score parts */
		int64_t val    = _score_part[company][score_type];
		int64_t needed = _score_info[score_type].needed;
		int   score  = _score_info[score_type].score;

		/* SCORE_TOTAL has its own rules ;) */
		if (score_type == SCORE_TOTAL) {
			for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) score += _score_info[i].score;
			needed = SCORE_MAX;
		}

		uint bar_top  = CentreBounds(r.top, r.bottom, this->bar_height);
		uint text_top = CentreBounds(r.top, r.bottom, GetCharacterHeight(FS_NORMAL));

		DrawString(this->score_info_left, this->score_info_right, text_top, STR_PERFORMANCE_DETAIL_VEHICLES + score_type);

		/* Draw the score */
		DrawString(this->score_info_left, this->score_info_right, text_top, GetString(STR_JUST_COMMA, score), TC_BLACK, SA_RIGHT);

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
		DrawString(this->bar_left, this->bar_right, text_top, GetString(STR_PERFORMANCE_DETAIL_PERCENT, Clamp<int64_t>(val, 0, needed) * 100 / needed), TC_FROMSTRING, SA_HOR_CENTER);

		/* SCORE_LOAN is inverted */
		if (score_type == SCORE_LOAN) val = needed - val;

		/* Draw the amount we have against what is needed
		 * For some of them it is in currency format */
		switch (score_type) {
			case SCORE_MIN_PROFIT:
			case SCORE_MIN_INCOME:
			case SCORE_MAX_INCOME:
			case SCORE_MONEY:
			case SCORE_LOAN:
				DrawString(this->score_detail_left, this->score_detail_right, text_top, GetString(STR_PERFORMANCE_DETAIL_AMOUNT_CURRENCY, val, needed));
				break;
			default:
				DrawString(this->score_detail_left, this->score_detail_right, text_top, GetString(STR_PERFORMANCE_DETAIL_AMOUNT_INT, val, needed));
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* Check which button is clicked */
		if (IsInsideMM(widget, WID_PRD_COMPANY_FIRST, WID_PRD_COMPANY_LAST + 1)) {
			/* Is it no on disable? */
			if (!this->IsWidgetDisabled(widget)) {
				this->RaiseWidget(WID_PRD_COMPANY_FIRST + this->company);
				this->company = (CompanyID)(widget - WID_PRD_COMPANY_FIRST);
				this->LowerWidget(WID_PRD_COMPANY_FIRST + this->company);
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
		for (CompanyID i = CompanyID::Begin(); i < MAX_COMPANIES; ++i) {
			this->SetWidgetDisabledState(WID_PRD_COMPANY_FIRST + i, !Company::IsValidID(i));
		}

		/* Check if the currently selected company is still active. */
		if (this->company != CompanyID::Invalid() && !Company::IsValidID(this->company)) {
			/* Raise the widget for the previous selection. */
			this->RaiseWidget(WID_PRD_COMPANY_FIRST + this->company);
			this->company = CompanyID::Invalid();
		}

		if (this->company == CompanyID::Invalid()) {
			for (const Company *c : Company::Iterate()) {
				this->company = c->index;
				break;
			}
		}

		/* Make sure the widget is lowered */
		if (this->company != CompanyID::Invalid()) {
			this->LowerWidget(WID_PRD_COMPANY_FIRST + this->company);
		}
	}
};

CompanyID PerformanceRatingDetailWindow::company = CompanyID::Invalid();

/*******************************/
/* INDUSTRY PRODUCTION HISTORY */
/*******************************/

struct IndustryProductionGraphWindow : BaseCargoGraphWindow {
	static inline constexpr StringID RANGE_LABELS[] = {
		STR_GRAPH_INDUSTRY_RANGE_PRODUCED,
		STR_GRAPH_INDUSTRY_RANGE_TRANSPORTED,
		STR_GRAPH_INDUSTRY_RANGE_DELIVERED,
		STR_GRAPH_INDUSTRY_RANGE_WAITING,
	};

	static inline CargoTypes excluded_cargo_types{};

	IndustryProductionGraphWindow(WindowDesc &desc, WindowNumber window_number) :
			BaseCargoGraphWindow(desc, STR_JUST_COMMA)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->month_increment = 1;
		this->x_values_increment = ECONOMY_MONTH_MINUTES;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();
		this->ranges = RANGE_LABELS;

		const Industry *i = Industry::Get(window_number);
		if (!i->IsCargoProduced()) this->masked_range = (1U << 0) | (1U << 1);
		if (!i->IsCargoAccepted()) this->masked_range = (1U << 2) | (1U << 3);

		this->InitializeWindow(window_number);
	}

	void OnInit() override
	{
		this->BaseCargoGraphWindow::OnInit();

		this->scales = TimerGameEconomy::UsingWallclockUnits() ? MONTHLY_SCALE_WALLCLOCK : MONTHLY_SCALE_CALENDAR;
	}

	CargoTypes GetCargoTypes(WindowNumber window_number) const override
	{
		CargoTypes cargo_types{};
		const Industry *i = Industry::Get(window_number);
		for (const auto &a : i->accepted) {
			if (IsValidCargoType(a.cargo)) SetBit(cargo_types, a.cargo);
		}
		for (const auto &p : i->produced) {
			if (IsValidCargoType(p.cargo)) SetBit(cargo_types, p.cargo);
		}
		return cargo_types;
	}

	CargoTypes &GetExcludedCargoTypes() const override
	{
		return IndustryProductionGraphWindow::excluded_cargo_types;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_GRAPH_CAPTION) return GetString(STR_GRAPH_INDUSTRY_CAPTION, this->window_number);

		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateStatistics(bool initialize) override
	{
		int mo = (TimerGameEconomy::month / this->month_increment - this->num_vert_lines) * this->month_increment;
		auto yr = TimerGameEconomy::year;
		while (mo < 0) {
			yr--;
			mo += 12;
		}

		if (!initialize && this->excluded_data == this->GetExcludedCargoTypes() && this->num_on_x_axis == this->num_vert_lines && this->year == yr && this->month == mo) {
			/* There's no reason to get new stats */
			return;
		}

		this->excluded_data = this->GetExcludedCargoTypes();
		this->year = yr;
		this->month = mo;

		const Industry *i = Industry::Get(this->window_number);

		this->data.clear();
		this->data.reserve(
			2 * std::ranges::count_if(i->produced, &IsValidCargoType, &Industry::ProducedCargo::cargo) +
			2 * std::ranges::count_if(i->accepted, &IsValidCargoType, &Industry::AcceptedCargo::cargo));

		for (const auto &p : i->produced) {
			if (!IsValidCargoType(p.cargo)) continue;
			const CargoSpec *cs = CargoSpec::Get(p.cargo);

			DataSet &produced = this->data.emplace_back();
			produced.colour = cs->legend_colour;
			produced.exclude_bit = cs->Index();
			produced.range_bit = 0;

			DataSet &transported = this->data.emplace_back();
			transported.colour = cs->legend_colour;
			transported.exclude_bit = cs->Index();
			transported.range_bit = 1;
			transported.dash = 2;

			FillFromHistory<GRAPH_NUM_MONTHS>(p.history, i->valid_history, *this->scales[this->selected_scale].history_range,
				Filler{{produced}, &Industry::ProducedHistory::production},
				Filler{{transported}, &Industry::ProducedHistory::transported});
		}

		for (const auto &a : i->accepted) {
			if (!IsValidCargoType(a.cargo)) continue;
			const CargoSpec *cs = CargoSpec::Get(a.cargo);

			DataSet &accepted = this->data.emplace_back();
			accepted.colour = cs->legend_colour;
			accepted.exclude_bit = cs->Index();
			accepted.range_bit = 2;
			accepted.dash = 1;

			DataSet &waiting = this->data.emplace_back();
			waiting.colour = cs->legend_colour;
			waiting.exclude_bit = cs->Index();
			waiting.range_bit = 3;
			waiting.dash = 4;

			FillFromHistory<GRAPH_NUM_MONTHS>(a.history.get(), i->valid_history, *this->scales[this->selected_scale].history_range,
				Filler{{accepted}, &Industry::AcceptedHistory::accepted},
				Filler{{waiting}, &Industry::AcceptedHistory::waiting});
		}

		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_industry_production_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_GRAPH_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND), SetMinimalSize(568, 128),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(495, 0), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
				NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_RANGE_MATRIX), SetFill(1, 0), SetResize(0, 0), SetMatrixDataTip(1, 0, STR_GRAPH_TOGGLE_RANGE),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_ENABLE_CARGOES), SetStringTip(STR_GRAPH_CARGO_ENABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_ENABLE_ALL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_DISABLE_CARGOES), SetStringTip(STR_GRAPH_CARGO_DISABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_DISABLE_ALL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_MATRIX), SetFill(1, 0), SetResize(0, 2), SetMatrixDataTip(1, 0, STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO), SetScrollbar(WID_GRAPH_MATRIX_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_GRAPH_MATRIX_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_SCALE_MATRIX), SetFill(1, 0), SetResize(0, 0), SetMatrixDataTip(1, 0, STR_GRAPH_SELECT_SCALE),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(0, 1), SetResize(0, 1),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _industry_production_desc(
	WDP_AUTO, "graph_industry_production", 0, 0,
	WC_INDUSTRY_PRODUCTION, WC_INDUSTRY_VIEW,
	{},
	_nested_industry_production_widgets
);

void ShowIndustryProductionGraph(WindowNumber window_number)
{
	AllocateWindowDescFront<IndustryProductionGraphWindow>(_industry_production_desc, window_number);
}

struct TownCargoGraphWindow : BaseCargoGraphWindow {
	static inline constexpr StringID RANGE_LABELS[] = {
		STR_GRAPH_TOWN_RANGE_PRODUCED,
		STR_GRAPH_TOWN_RANGE_TRANSPORTED,
	};

	static inline CargoTypes excluded_cargo_types{};

	TownCargoGraphWindow(WindowDesc &desc, WindowNumber window_number) : BaseCargoGraphWindow(desc, STR_JUST_COMMA)
	{
		this->num_on_x_axis = GRAPH_NUM_MONTHS;
		this->num_vert_lines = GRAPH_NUM_MONTHS;
		this->month_increment = 1;
		this->x_values_reversed = true;
		this->x_values_increment = ECONOMY_MONTH_MINUTES;
		this->draw_dates = !TimerGameEconomy::UsingWallclockUnits();
		this->ranges = RANGE_LABELS;

		this->InitializeWindow(window_number);
	}

	void OnInit() override
	{
		this->BaseCargoGraphWindow::OnInit();

		this->scales = TimerGameEconomy::UsingWallclockUnits() ? MONTHLY_SCALE_WALLCLOCK : MONTHLY_SCALE_CALENDAR;
	}

	CargoTypes GetCargoTypes(WindowNumber window_number) const override
	{
		CargoTypes cargo_types{};
		const Town *t = Town::Get(window_number);
		for (const auto &s : t->supplied) {
			if (IsValidCargoType(s.cargo)) SetBit(cargo_types, s.cargo);
		}
		return cargo_types;
	}

	CargoTypes &GetExcludedCargoTypes() const override
	{
		return TownCargoGraphWindow::excluded_cargo_types;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_GRAPH_CAPTION) return GetString(STR_GRAPH_TOWN_CARGO_CAPTION, this->window_number);

		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateStatistics(bool initialize) override
	{
		int mo = TimerGameEconomy::month - this->num_vert_lines;
		auto yr = TimerGameEconomy::year;
		while (mo < 0) {
			yr--;
			mo += 12;
		}

		if (!initialize && this->excluded_data == this->GetExcludedCargoTypes() && this->num_on_x_axis == this->num_vert_lines && this->year == yr && this->month == mo) {
			/* There's no reason to get new stats */
			return;
		}

		this->excluded_data = this->GetExcludedCargoTypes();
		this->year = yr;
		this->month = mo;

		const Town *t = Town::Get(this->window_number);

		this->data.clear();
		for (const auto &s : t->supplied) {
			if (!IsValidCargoType(s.cargo)) continue;
			const CargoSpec *cs = CargoSpec::Get(s.cargo);

			this->data.reserve(this->data.size() + 2);

			DataSet &produced = this->data.emplace_back();
			produced.colour = cs->legend_colour;
			produced.exclude_bit = cs->Index();
			produced.range_bit = 0;

			DataSet &transported = this->data.emplace_back();
			transported.colour = cs->legend_colour;
			transported.exclude_bit = cs->Index();
			transported.range_bit = 1;
			transported.dash = 2;

			FillFromHistory<GRAPH_NUM_MONTHS>(s.history, t->valid_history, *this->scales[this->selected_scale].history_range,
				Filler{{produced}, &Town::SuppliedHistory::production},
				Filler{{transported}, &Town::SuppliedHistory::transported});
		}

		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_town_cargo_graph_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_GRAPH_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN, WID_GRAPH_BACKGROUND), SetMinimalSize(568, 128),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GRAPH_GRAPH), SetMinimalSize(495, 0), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
				NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_RANGE_MATRIX), SetFill(1, 0), SetResize(0, 0), SetMatrixDataTip(1, 0, STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_ENABLE_CARGOES), SetStringTip(STR_GRAPH_CARGO_ENABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_ENABLE_ALL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_GRAPH_DISABLE_CARGOES), SetStringTip(STR_GRAPH_CARGO_DISABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_DISABLE_ALL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_MATRIX), SetFill(1, 0), SetResize(0, 2), SetMatrixDataTip(1, 0, STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO), SetScrollbar(WID_GRAPH_MATRIX_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WID_GRAPH_MATRIX_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidget(WWT_MATRIX, COLOUR_BROWN, WID_GRAPH_SCALE_MATRIX), SetFill(1, 0), SetResize(0, 0), SetMatrixDataTip(1, 0, STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO),
				NWidget(NWID_SPACER), SetMinimalSize(0, 24), SetFill(0, 1),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(0, 1), SetResize(0, 1),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_TEXT, INVALID_COLOUR, WID_GRAPH_FOOTER), SetFill(1, 0), SetResize(1, 0), SetPadding(2, 0, 2, 0), SetTextStyle(TC_BLACK, FS_SMALL), SetAlignment(SA_CENTER),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN, WID_GRAPH_RESIZE), SetResizeWidgetTypeTip(RWV_HIDE_BEVEL, STR_TOOLTIP_RESIZE),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _town_cargo_graph_desc(
	WDP_AUTO, "graph_town_cargo", 0, 0,
	WC_TOWN_CARGO_GRAPH, WC_TOWN_VIEW,
	{},
	_nested_town_cargo_graph_widgets
);

void ShowTownCargoGraph(WindowNumber window_number)
{
	AllocateWindowDescFront<TownCargoGraphWindow>(_town_cargo_graph_desc, window_number);
}

/**
 * Make a vertical list of panels for outputting score details.
 * @return Panel with performance details.
 */
static std::unique_ptr<NWidgetBase> MakePerformanceDetailPanels()
{
	auto realtime = TimerGameEconomy::UsingWallclockUnits();
	const StringID performance_tips[] = {
		realtime ? STR_PERFORMANCE_DETAIL_VEHICLES_TOOLTIP_PERIODS : STR_PERFORMANCE_DETAIL_VEHICLES_TOOLTIP_YEARS,
		STR_PERFORMANCE_DETAIL_STATIONS_TOOLTIP,
		realtime ? STR_PERFORMANCE_DETAIL_MIN_PROFIT_TOOLTIP_PERIODS : STR_PERFORMANCE_DETAIL_MIN_PROFIT_TOOLTIP_YEARS,
		STR_PERFORMANCE_DETAIL_MIN_INCOME_TOOLTIP,
		STR_PERFORMANCE_DETAIL_MAX_INCOME_TOOLTIP,
		STR_PERFORMANCE_DETAIL_DELIVERED_TOOLTIP,
		STR_PERFORMANCE_DETAIL_CARGO_TOOLTIP,
		STR_PERFORMANCE_DETAIL_MONEY_TOOLTIP,
		STR_PERFORMANCE_DETAIL_LOAN_TOOLTIP,
		STR_PERFORMANCE_DETAIL_TOTAL_TOOLTIP,
	};

	static_assert(lengthof(performance_tips) == SCORE_END - SCORE_BEGIN);

	auto vert = std::make_unique<NWidgetVertical>(NWidContainerFlag::EqualSize);
	for (WidgetID widnum = WID_PRD_SCORE_FIRST; widnum <= WID_PRD_SCORE_LAST; widnum++) {
		auto panel = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_BROWN, widnum);
		panel->SetFill(1, 1);
		panel->SetToolTip(performance_tips[widnum - WID_PRD_SCORE_FIRST]);
		vert->Add(std::move(panel));
	}
	return vert;
}

/** Make a number of rows with buttons for each company for the performance rating detail window. */
std::unique_ptr<NWidgetBase> MakeCompanyButtonRowsGraphGUI()
{
	return MakeCompanyButtonRows(WID_PRD_COMPANY_FIRST, WID_PRD_COMPANY_LAST, COLOUR_BROWN, 8, STR_PERFORMANCE_DETAIL_SELECT_COMPANY_TOOLTIP);
}

static constexpr std::initializer_list<NWidgetPart> _nested_performance_rating_detail_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_PERFORMANCE_DETAIL, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidgetFunction(MakeCompanyButtonRowsGraphGUI), SetPadding(2),
	EndContainer(),
	NWidgetFunction(MakePerformanceDetailPanels),
};

static WindowDesc _performance_rating_detail_desc(
	WDP_AUTO, "league_details", 0, 0,
	WC_PERFORMANCE_DETAIL, WC_NONE,
	{},
	_nested_performance_rating_detail_widgets
);

void ShowPerformanceRatingDetail()
{
	AllocateWindowDescFront<PerformanceRatingDetailWindow>(_performance_rating_detail_desc, 0);
}

void InitializeGraphGui()
{
	_legend_excluded_companies = CompanyMask{};
	PaymentRatesGraphWindow::excluded_cargo_types = {};
	IndustryProductionGraphWindow::excluded_cargo_types = {};
}
