/* $Id$ */

/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

/** @file framerate_gui.cpp GUI for displaying framerate/game speed information. */

#include "framerate_type.h"
#include <chrono>
#include "gfx_func.h"
#include "window_gui.h"
#include "table/sprites.h"
#include "strings_func.h"
#include "debug.h"
#include "console_func.h"
#include "console_type.h"


namespace {

	/** Number of data points to keep in buffer for each performance measurement */
	const int NUM_FRAMERATE_POINTS = 512;
	/** Units a second is divided into in performance measurements */
	const int TIMESTAMP_PRECISION = 1000000;

	struct PerformanceData {
		static const TimingMeasurement INVALID_DURATION = UINT64_MAX;

		TimingMeasurement durations[NUM_FRAMERATE_POINTS];
		TimingMeasurement timestamps[NUM_FRAMERATE_POINTS];
		double expected_rate;
		int next_index;
		int prev_index;
		int num_valid;

		explicit PerformanceData(double expected_rate) : expected_rate(expected_rate), next_index(0), prev_index(0), num_valid(0) { }

		void Add(TimingMeasurement start_time, TimingMeasurement end_time)
		{
			this->durations[this->next_index] = end_time - start_time;
			this->timestamps[this->next_index] = start_time;
			this->prev_index = this->next_index;
			this->next_index += 1;
			if (this->next_index >= NUM_FRAMERATE_POINTS) this->next_index = 0;
			this->num_valid = min(NUM_FRAMERATE_POINTS, this->num_valid + 1);
		}

		void BeginAccumulate(TimingMeasurement start_time)
		{
			this->timestamps[this->next_index] = start_time;
			this->durations[this->next_index] = 0;
			this->prev_index = this->next_index;
			this->next_index += 1;
			if (this->next_index >= NUM_FRAMERATE_POINTS) this->next_index = 0;
			this->num_valid = min(NUM_FRAMERATE_POINTS, this->num_valid + 1);
		}

		void AddAccumulate(TimingMeasurement duration)
		{
			this->durations[this->prev_index] += duration;
		}

		void AddPause(TimingMeasurement start_time)
		{
			if (this->durations[this->prev_index] != INVALID_DURATION) {
				this->timestamps[this->next_index] = start_time;
				this->durations[this->next_index] = INVALID_DURATION;
				this->prev_index = this->next_index;
				this->next_index += 1;
				if (this->next_index >= NUM_FRAMERATE_POINTS) this->next_index = 0;
				this->num_valid += 1;
			}
		}

		double GetAverageDurationMilliseconds(int count)
		{
			count = min(count, this->num_valid);

			int first_point = this->prev_index - count;
			if (first_point < 0) first_point += NUM_FRAMERATE_POINTS;

			double sumtime = 0;
			for (int i = first_point; i < first_point + count; i++) {
				auto d = this->durations[i % NUM_FRAMERATE_POINTS];
				if (d != INVALID_DURATION) {
					sumtime += d;
				} else {
					count--;
				}
			}

			if (count == 0) return 0;
			return sumtime * 1000 / count / TIMESTAMP_PRECISION;
		}

		/** Get current rate of a performance element, based on approximately the past one second of data */
		double GetRate()
		{
			int point = this->prev_index;
			int last_point = this->next_index - this->num_valid;
			if (last_point < 0) last_point += NUM_FRAMERATE_POINTS;

			int count = 0;
			TimingMeasurement last = this->timestamps[point];
			TimingMeasurement total = 0;

			while (point != last_point) {
				if (this->durations[point] != INVALID_DURATION) {
					total += last - this->timestamps[point];
				}
				last = this->timestamps[point];
				if (total >= TIMESTAMP_PRECISION) break;
				point--;
				count++;
				if (point < 0) point = NUM_FRAMERATE_POINTS - 1;
			}

			if (total == 0 || count == 0) return 0;
			return (double)count * TIMESTAMP_PRECISION / total;
		}
	};

	/** Game loop rate, cycles per second */
	static const double GL_RATE = 1000.0 / MILLISECONDS_PER_TICK;

	PerformanceData _pf_data[PFE_MAX] = {
		PerformanceData(GL_RATE),               // PFE_GAMELOOP
		PerformanceData(1),                     // PFE_ACC_GL_ECONOMY
		PerformanceData(1),                     // PFE_ACC_GL_TRAINS
		PerformanceData(1),                     // PFE_ACC_GL_ROADVEHS
		PerformanceData(1),                     // PFE_ACC_GL_SHIPS
		PerformanceData(1),                     // PFE_ACC_GL_AIRCRAFT
		PerformanceData(GL_RATE),               // PFE_DRAWING
		PerformanceData(1),                     // PFE_ACC_DRAWWORLD
		PerformanceData(60.0),                  // PFE_VIDEO
		PerformanceData(1000.0 * 8192 / 44100), // PFE_SOUND
	};

}


/**
 * Return a timestamp with \c TIMESTAMP_PRECISION ticks per second precision.
 * The basis of the timestamp is implementation defined, but the value should be steady,
 * so differences can be taken to reliably measure intervals.
 */
static TimingMeasurement GetPerformanceTimer()
{
	using clock = std::chrono::high_resolution_clock;
	auto usec = std::chrono::time_point_cast<std::chrono::microseconds>(clock::now());
	return usec.time_since_epoch().count();
}


/** Begin a cycle of a measured element. */
PerformanceMeasurer::PerformanceMeasurer(PerformanceElement elem)
{
	assert(elem < PFE_MAX);

	this->elem = elem;
	this->start_time = GetPerformanceTimer();
}

/** Finish a cycle of a measured element and store the measurement taken. */
PerformanceMeasurer::~PerformanceMeasurer()
{
	_pf_data[this->elem].Add(this->start_time, GetPerformanceTimer());
}

/** Set the rate of expected cycles per second of a performance element. */
void PerformanceMeasurer::SetExpectedRate(double rate)
{
	_pf_data[this->elem].expected_rate = rate;
}

/** Indicate that a cycle of "pause" where no processing occurs. */
void PerformanceMeasurer::Paused(PerformanceElement elem)
{
	_pf_data[elem].AddPause(GetPerformanceTimer());
}


/** Begin measuring one block of the accumulating value. */
PerformanceAccumulator::PerformanceAccumulator(PerformanceElement elem)
{
	assert(elem < PFE_MAX);

	this->elem = elem;
	this->start_time = GetPerformanceTimer();
}

/** Finish and add one block of the accumulating value. */
PerformanceAccumulator::~PerformanceAccumulator()
{
	_pf_data[this->elem].AddAccumulate(GetPerformanceTimer() - this->start_time);
}

/** Store the previous accumulator value and reset for a new cycle of accumulating measurements. */
void PerformanceAccumulator::Reset(PerformanceElement elem)
{
	_pf_data[elem].BeginAccumulate(GetPerformanceTimer());
}


void ShowFrametimeGraphWindow(PerformanceElement elem);


enum FramerateWindowWidgets {
	WID_FRW_CAPTION,
	WID_FRW_RATE_GAMELOOP,
	WID_FRW_RATE_DRAWING,
	WID_FRW_RATE_FACTOR,
	WID_FRW_INFO_DATA_POINTS,
	WID_FRW_TIMES_NAMES,
	WID_FRW_TIMES_CURRENT,
	WID_FRW_TIMES_AVERAGE,
};

static const NWidgetPart _framerate_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_FRW_CAPTION), SetDataTip(STR_FRAMERATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPadding(6), SetPIP(0, 3, 0),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_RATE_GAMELOOP), SetDataTip(STR_FRAMERATE_RATE_GAMELOOP, STR_FRAMERATE_RATE_GAMELOOP_TOOLTIP),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_RATE_DRAWING),  SetDataTip(STR_FRAMERATE_RATE_BLITTER,  STR_FRAMERATE_RATE_BLITTER_TOOLTIP),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_RATE_FACTOR),   SetDataTip(STR_FRAMERATE_SPEED_FACTOR,  STR_FRAMERATE_SPEED_FACTOR_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPadding(6), SetPIP(0, 3, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 6, 0),
				NWidget(WWT_EMPTY, COLOUR_GREY, WID_FRW_TIMES_NAMES),
				NWidget(WWT_EMPTY, COLOUR_GREY, WID_FRW_TIMES_CURRENT),
				NWidget(WWT_EMPTY, COLOUR_GREY, WID_FRW_TIMES_AVERAGE),
			EndContainer(),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_INFO_DATA_POINTS), SetDataTip(STR_FRAMERATE_DATA_POINTS, 0x0),
		EndContainer(),
	EndContainer(),
};

struct FramerateWindow : Window {
	bool small;

	static const int VSPACING = 3;

	FramerateWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->small = this->IsShaded();
	}

	virtual void OnTick()
	{
		if (this->small != this->IsShaded()) {
			this->small = this->IsShaded();
			this->GetWidget<NWidgetLeaf>(WID_FRW_CAPTION)->SetDataTip(this->small ? STR_FRAMERATE_CAPTION_SMALL : STR_FRAMERATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
		}

		this->SetDirty();
	}

	static void SetDParamGoodWarnBadDuration(double value)
	{
		const double threshold_good = MILLISECONDS_PER_TICK / 3;
		const double threshold_bad = MILLISECONDS_PER_TICK;

		uint tpl;
		if (value < threshold_good) tpl = STR_FRAMERATE_MS_GOOD;
		else if (value > threshold_bad) tpl = STR_FRAMERATE_MS_BAD;
		else tpl = STR_FRAMERATE_MS_WARN;

		value = min(9999.99, value);
		SetDParam(0, tpl);
		SetDParam(1, (int)(value * 100));
		SetDParam(2, 2);
	}

	static void SetDParamGoodWarnBadRate(double value, PerformanceElement elem)
	{
		const double threshold_good = _pf_data[elem].expected_rate * 0.95;
		const double threshold_bad = _pf_data[elem].expected_rate * 2 / 3;

		uint tpl;
		if (value > threshold_good) tpl = STR_FRAMERATE_FPS_GOOD;
		else if (value < threshold_bad) tpl = STR_FRAMERATE_FPS_BAD;
		else tpl = STR_FRAMERATE_FPS_WARN;

		value = min(9999.99, value);
		SetDParam(0, tpl);
		SetDParam(1, (int)(value * 100));
		SetDParam(2, 2);
	}

	virtual void SetStringParameters(int widget) const
	{
		double value;

		switch (widget) {
			case WID_FRW_CAPTION:
				if (!this->small) break;
				value = _pf_data[PFE_GAMELOOP].GetRate();
				SetDParamGoodWarnBadRate(value, PFE_GAMELOOP);
				value /= _pf_data[PFE_GAMELOOP].expected_rate;
				SetDParam(3, (int)(value * 100));
				SetDParam(4, 2);
				break;

			case WID_FRW_RATE_GAMELOOP:
				value = _pf_data[PFE_GAMELOOP].GetRate();
				SetDParamGoodWarnBadRate(value, PFE_GAMELOOP);
				break;
			case WID_FRW_RATE_DRAWING:
				value = _pf_data[PFE_DRAWING].GetRate();
				SetDParamGoodWarnBadRate(value, PFE_DRAWING);
				break;
			case WID_FRW_RATE_FACTOR:
				value = _pf_data[PFE_GAMELOOP].GetRate();
				value /= _pf_data[PFE_GAMELOOP].expected_rate;
				SetDParam(0, (int)(value * 100));
				SetDParam(1, 2);
				break;
			case WID_FRW_INFO_DATA_POINTS:
				SetDParam(0, NUM_FRAMERATE_POINTS);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_FRW_RATE_GAMELOOP:
				SetDParamGoodWarnBadRate(9999.99, PFE_GAMELOOP);
				*size = GetStringBoundingBox(STR_FRAMERATE_RATE_GAMELOOP);
				break;
			case WID_FRW_RATE_DRAWING:
				SetDParamGoodWarnBadRate(9999.99, PFE_DRAWING);
				*size = GetStringBoundingBox(STR_FRAMERATE_RATE_BLITTER);
				break;
			case WID_FRW_RATE_FACTOR:
				SetDParam(0, 999999);
				SetDParam(1, 2);
				*size = GetStringBoundingBox(STR_FRAMERATE_SPEED_FACTOR);
				break;

			case WID_FRW_TIMES_NAMES: {
				int linecount = PFE_MAX - PFE_FIRST;
				size->width = 0;
				size->height = FONT_HEIGHT_NORMAL * (linecount + 1) + VSPACING;
				for (int line = 0; line < linecount; line++) {
					Dimension line_size = GetStringBoundingBox(STR_FRAMERATE_GAMELOOP + line);
					size->width = max(size->width, line_size.width);
				}
				break;
			}

			case WID_FRW_TIMES_CURRENT:
			case WID_FRW_TIMES_AVERAGE: {
				int linecount = PFE_MAX - PFE_FIRST;
				*size = GetStringBoundingBox(STR_FRAMERATE_CURRENT + (widget - WID_FRW_TIMES_CURRENT));
				SetDParamGoodWarnBadDuration(9999.99);
				Dimension item_size = GetStringBoundingBox(STR_FRAMERATE_VALUE);
				size->width = max(size->width, item_size.width);
				size->height += FONT_HEIGHT_NORMAL * linecount + VSPACING;
				break;
			}

			default:
				Window::UpdateWidgetSize(widget, size, padding, fill, resize);
				break;
		}
	}

	typedef double (ElementValueExtractor)(PerformanceElement elem);
	void DrawElementTimesColumn(const Rect &r, int heading_str, ElementValueExtractor get_value_func) const
	{
		int y = r.top;
		DrawString(r.left, r.right, y, heading_str, TC_FROMSTRING, SA_CENTER);
		y += FONT_HEIGHT_NORMAL + VSPACING;

		for (auto e = PFE_FIRST; e < PFE_MAX; e++) {
			double value = get_value_func(e);
			SetDParamGoodWarnBadDuration(value);
			DrawString(r.left, r.right, y, STR_FRAMERATE_VALUE, TC_FROMSTRING, SA_RIGHT);
			y += FONT_HEIGHT_NORMAL;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_FRW_TIMES_NAMES: {
				int linecount = PFE_MAX - PFE_FIRST;
				int y = r.top + FONT_HEIGHT_NORMAL + VSPACING;
				for (int i = 0; i < linecount; i++) {
					DrawString(r.left, r.right, y, STR_FRAMERATE_GAMELOOP + i, TC_FROMSTRING, SA_LEFT);
					y += FONT_HEIGHT_NORMAL;
				}
				break;
			}
			case WID_FRW_TIMES_CURRENT:
				DrawElementTimesColumn(r, STR_FRAMERATE_CURRENT, [](PerformanceElement elem) {
					return _pf_data[elem].GetAverageDurationMilliseconds(8);
				});
				break;
			case WID_FRW_TIMES_AVERAGE:
				DrawElementTimesColumn(r, STR_FRAMERATE_AVERAGE, [](PerformanceElement elem) {
					return _pf_data[elem].GetAverageDurationMilliseconds(NUM_FRAMERATE_POINTS);
				});
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_FRW_TIMES_NAMES:
			case WID_FRW_TIMES_CURRENT:
			case WID_FRW_TIMES_AVERAGE: {
				int line = this->GetRowFromWidget(pt.y, widget, VSPACING, FONT_HEIGHT_NORMAL);
				if (line > 0) {
					line -= 1;
					ShowFrametimeGraphWindow((PerformanceElement)line);
				}
				break;
			}
		}
	}
};

static WindowDesc _framerate_display_desc(
	WDP_AUTO, "framerate_display", 60, 40,
	WC_FRAMERATE_DISPLAY, WC_NONE,
	0,
	_framerate_window_widgets, lengthof(_framerate_window_widgets)
);


enum FrametimeGraphWindowWidgets {
	WID_FGW_CAPTION,
	WID_FGW_GRAPH,
};

static const NWidgetPart _frametime_graph_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_FGW_CAPTION), SetDataTip(STR_WHITE_STRING, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPadding(6),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_FGW_GRAPH),
		EndContainer(),
	EndContainer(),
};

struct FrametimeGraphWindow : Window {
	int vertical_scale;     ///< number of TIMESTAMP_PRECISION units vertically
	int horizontal_scale;   ///< number of half-second units horizontally
	int scale_update_timer; ///< ticks left before next scale update

	PerformanceElement element;
	Dimension graph_size;

	FrametimeGraphWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->element = (PerformanceElement)number;
		this->horizontal_scale = 4;
		this->vertical_scale = TIMESTAMP_PRECISION / 10;
		this->scale_update_timer = 0;

		this->InitNested(number);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_FGW_CAPTION:
				SetDParam(0, STR_FRAMETIME_CAPTION_GAMELOOP + this->element);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_FGW_GRAPH) {
			SetDParam(0, 100);
			Dimension size_ms_label = GetStringBoundingBox(STR_FRAMERATE_GRAPH_MILLISECONDS);
			SetDParam(0, 100);
			Dimension size_s_label = GetStringBoundingBox(STR_FRAMERATE_GRAPH_SECONDS);

			graph_size.height = max<uint>(100, 10 * (size_ms_label.height + 1));
			graph_size.width = 2 * graph_size.height;
			*size = graph_size;

			size->width += size_ms_label.width + 2;
			size->height += size_s_label.height + 2;
		}
	}

	void UpdateScale()
	{
		auto &durations = _pf_data[this->element].durations;
		auto &timestamps = _pf_data[this->element].timestamps;
		auto num_valid = _pf_data[this->element].num_valid;
		int point = _pf_data[this->element].prev_index;

		TimingMeasurement lastts = timestamps[point];
		TimingMeasurement time_sum = 0;
		TimingMeasurement peak_value = 0;
		int count = 0;

		for (int i = 1; i < num_valid; i++) {
			point--;
			if (point < 0) point = NUM_FRAMERATE_POINTS - 1;

			TimingMeasurement value = durations[point];
			if (value == PerformanceData::INVALID_DURATION) {
				lastts = timestamps[point];
				continue;
			}
			if (value > peak_value) peak_value = value;
			count++;

			time_sum += lastts - timestamps[point];
			lastts = timestamps[point];

			/* Determine horizontal scale based on duration covered by 60 points
			 * (slightly less than 2 seconds at full game speed) */
			if (count == 60) {
				TimingMeasurement seconds = time_sum / TIMESTAMP_PRECISION;
				if (seconds < 3) this->horizontal_scale = 4;
				else if (seconds < 5) this->horizontal_scale = 10;
				else if (seconds < 10) this->horizontal_scale = 20;
				else this->horizontal_scale = 60;
			}

			if (count >= 60 && time_sum >= (this->horizontal_scale + 2) * TIMESTAMP_PRECISION / 2) break;
		}

		/* Determine vertical scale based on peak value */
		static const TimingMeasurement vscales[] = {
			TIMESTAMP_PRECISION * 100,
			TIMESTAMP_PRECISION * 10,
			TIMESTAMP_PRECISION * 5,
			TIMESTAMP_PRECISION,
			TIMESTAMP_PRECISION / 2,
			TIMESTAMP_PRECISION / 5,
			TIMESTAMP_PRECISION / 10,
			TIMESTAMP_PRECISION / 50,
		};
		for (auto sc : vscales) {
			if (peak_value < sc) this->vertical_scale = sc;
		}
	}

	virtual void OnTick()
	{
		this->SetDirty();

		if (this->scale_update_timer > 0) {
			this->scale_update_timer--;
		} else {
			this->scale_update_timer = 10;
			this->UpdateScale();
		}
	}

	/** Scale and interpolate a value from a source range into a destination range */
	static inline int Scinterlate(int dst_min, int dst_max, int src_min, int src_max, int value)
	{
		int dst_diff = dst_max - dst_min;
		int src_diff = src_max - src_min;
		return (value - src_min) * dst_diff / src_diff + dst_min;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget == WID_FGW_GRAPH) {
			const auto &durations = _pf_data[this->element].durations;
			const auto &timestamps = _pf_data[this->element].timestamps;
			int point = _pf_data[this->element].prev_index;

			const int x_zero = r.right - this->graph_size.width;
			const int x_max = r.right;
			const int y_zero = r.top + this->graph_size.height;
			const int y_max = r.top;
			const int c_grid = PC_DARK_GREY;
			const int c_lines = PC_BLACK;
			const int c_peak = PC_DARK_RED;

			const TimingMeasurement draw_horz_scale = this->horizontal_scale * TIMESTAMP_PRECISION / 2;
			const TimingMeasurement draw_vert_scale = this->vertical_scale;

			const int horz_div_scl = (this->horizontal_scale <= 20) ? 1 : 10;
			const int horz_divisions = this->horizontal_scale / horz_div_scl;
			const int vert_divisions = 10;

			for (int division = 0; division < vert_divisions; division++) {
				int y = Scinterlate(y_zero, y_max, 0, vert_divisions, division);
				GfxDrawLine(x_zero, y, x_max, y, c_grid);
				if (division % 2 == 0) {
					if (this->vertical_scale > TIMESTAMP_PRECISION) {
						SetDParam(0, this->vertical_scale * division / 10 / TIMESTAMP_PRECISION);
						DrawString(r.left, x_zero - 2, y - FONT_HEIGHT_SMALL, STR_FRAMERATE_GRAPH_SECONDS, TC_GREY, SA_RIGHT | SA_FORCE, false, FS_SMALL);
					} else {
						SetDParam(0, this->vertical_scale * division / 10 * 1000 / TIMESTAMP_PRECISION);
						DrawString(r.left, x_zero - 2, y - FONT_HEIGHT_SMALL, STR_FRAMERATE_GRAPH_MILLISECONDS, TC_GREY, SA_RIGHT | SA_FORCE, false, FS_SMALL);
					}
				}
			}
			for (int division = horz_divisions; division > 0; division--) {
				int x = Scinterlate(x_zero, x_max, 0, horz_divisions, horz_divisions - division);
				GfxDrawLine(x, y_max, x, y_zero, c_grid);
				if (division % 2 == 0) {
					SetDParam(0, division * horz_div_scl / 2);
					DrawString(x, x_max, y_zero + 2, STR_FRAMERATE_GRAPH_SECONDS, TC_GREY, SA_LEFT | SA_FORCE, false, FS_SMALL);
				}
			}

			Point lastpoint{
				x_max,
				Scinterlate(y_zero, y_max, 0, this->vertical_scale, (int)durations[point])
			};
			TimingMeasurement lastts = timestamps[point];

			TimingMeasurement peak_value = 0;
			Point peak_point;
			TimingMeasurement value_sum = 0;
			TimingMeasurement time_sum = 0;
			int points_drawn = 0;

			for (int i = 1; i < NUM_FRAMERATE_POINTS; i++) {
				point--;
				if (point < 0) point = NUM_FRAMERATE_POINTS - 1;

				TimingMeasurement value = durations[point];
				if (value == PerformanceData::INVALID_DURATION) {
					lastts = timestamps[point];
					continue;
				}

				time_sum += lastts - timestamps[point];
				lastts = timestamps[point];
				if (time_sum > draw_horz_scale) break;

				Point newpoint{
					Scinterlate(x_zero, x_max, 0, draw_horz_scale, draw_horz_scale - time_sum),
					Scinterlate(y_zero, y_max, 0, draw_vert_scale, (int)value)
				};
				assert(newpoint.x <= lastpoint.x);
				GfxDrawLine(lastpoint.x, lastpoint.y, newpoint.x, newpoint.y, c_lines);
				lastpoint = newpoint;

				value_sum += value;
				points_drawn++;
				if (value > peak_value) {
					peak_value = value;
					peak_point = newpoint;
				}
			}

			if (points_drawn > 0 && peak_value > TIMESTAMP_PRECISION / 100 && 2 * peak_value > 3 * value_sum / points_drawn) {
				TextColour tc_peak = (TextColour)(TC_IS_PALETTE_COLOUR | c_peak);
				GfxFillRect(peak_point.x - 1, peak_point.y - 1, peak_point.x + 1, peak_point.y + 1, c_peak);
				SetDParam(0, peak_value * 1000 / TIMESTAMP_PRECISION);
				int label_y = max(y_max, peak_point.y - FONT_HEIGHT_SMALL);
				if (peak_point.x - x_zero > (int)this->graph_size.width / 2) {
					DrawString(x_zero, peak_point.x - 2, label_y, STR_FRAMERATE_GRAPH_MILLISECONDS, tc_peak, SA_RIGHT | SA_FORCE, false, FS_SMALL);
				} else {
					DrawString(peak_point.x + 2, x_max, label_y, STR_FRAMERATE_GRAPH_MILLISECONDS, tc_peak, SA_LEFT | SA_FORCE, false, FS_SMALL);
				}
			}
		}
	}
};

static WindowDesc _frametime_graph_window_desc(
	WDP_AUTO, "frametime_graph", 140, 90,
	WC_FRAMETIME_GRAPH, WC_NONE,
	0,
	_frametime_graph_window_widgets, lengthof(_frametime_graph_window_widgets)
);



void ShowFramerateWindow()
{
	AllocateWindowDescFront<FramerateWindow>(&_framerate_display_desc, 0);
}

void ShowFrametimeGraphWindow(PerformanceElement elem)
{
	if (elem < PFE_FIRST || elem >= PFE_MAX) return; // maybe warn?
	AllocateWindowDescFront<FrametimeGraphWindow>(&_frametime_graph_window_desc, elem, true);
}

void ConPrintFramerate()
{
	const int count1 = NUM_FRAMERATE_POINTS / 8;
	const int count2 = NUM_FRAMERATE_POINTS / 4;
	const int count3 = NUM_FRAMERATE_POINTS / 1;

	IConsolePrintF(TC_SILVER, "Based on num. data points: %d %d %d", count1, count2, count3);

	static char *MEASUREMENT_NAMES[PFE_MAX] = {
		"Game loop",
		"  GL station ticks",
		"  GL train ticks",
		"  GL road vehicle ticks",
		"  GL ship ticks",
		"  GL aircraft ticks",
		"Drawing",
		"  Viewport drawing",
		"Video output",
		"Sound mixing",
	};

	static const PerformanceElement rate_elements[] = { PFE_GAMELOOP, PFE_DRAWING, PFE_VIDEO };

	bool printed_anything = false;

	for (auto e : rate_elements) {
		auto &pf = _pf_data[e];
		if (pf.num_valid == 0) continue;
		IConsolePrintF(TC_GREEN, "%s rate: %.2ffps  (expected: %.2ffps)",
			MEASUREMENT_NAMES[e],
			pf.GetRate(),
			pf.expected_rate);
		printed_anything = true;
	}

	for (auto e = PFE_FIRST; e < PFE_MAX; e++) {
		auto &pf = _pf_data[e];
		if (pf.num_valid == 0) continue;
		IConsolePrintF(TC_LIGHT_BLUE, "%s times: %.2fms  %.2fms  %.2fms",
			MEASUREMENT_NAMES[e],
			pf.GetAverageDurationMilliseconds(count1),
			pf.GetAverageDurationMilliseconds(count2),
			pf.GetAverageDurationMilliseconds(count3));
		printed_anything = true;
	}

	if (!printed_anything) {
		IConsoleWarning("No performance measurements have been taken yet");
	}
}
