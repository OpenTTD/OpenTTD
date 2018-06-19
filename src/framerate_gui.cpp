/* $Id$ */

/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

/** @file framerate_gui.cpp GUI for displaying framerate/game speed information. */

#include "framerate_type.h"
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

	TimingMeasurement _framerate_durations[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	TimingMeasurement _framerate_timestamps[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	double _framerate_expected_rate[FRAMERATE_MAX] = {
		1000.0 / MILLISECONDS_PER_TICK, // FRAMERATE_GAMELOOP
		1000.0 / MILLISECONDS_PER_TICK, // FRAMERATE_DRAWING
		60.0,                           // FRAMERATE_VIDEO
		1000.0 * 8192 / 44100,          // FRAMERATE_SOUND
	};
	int _framerate_next_measurement_point[FRAMERATE_MAX] = {};
	int _framerate_num_measurements[FRAMERATE_MAX] = {};

	void StoreMeasurement(FramerateElement elem, TimingMeasurement start_time, TimingMeasurement end_time)
	{
		_framerate_durations[elem][_framerate_next_measurement_point[elem]] = end_time - start_time;
		_framerate_timestamps[elem][_framerate_next_measurement_point[elem]] = start_time;
		_framerate_next_measurement_point[elem] += 1;
		_framerate_next_measurement_point[elem] %= NUM_FRAMERATE_POINTS;
		_framerate_num_measurements[elem] = min(NUM_FRAMERATE_POINTS, _framerate_num_measurements[elem] + 1);
	}

	double GetAverageDuration(FramerateElement elem, int points)
	{
		assert(elem < FRAMERATE_MAX);
		points = min(points, _framerate_num_measurements[elem]);

		int first_point = _framerate_next_measurement_point[elem] - points - 1;
		while (first_point < 0) first_point += NUM_FRAMERATE_POINTS;

		double sumtime = 0;
		for (int i = first_point; i < first_point + points; i++) {
			sumtime += _framerate_durations[elem][i % NUM_FRAMERATE_POINTS];
		}

		return sumtime * 1000 / points / TIMESTAMP_PRECISION;
	}

	TimingMeasurement GetAverageTimestep(FramerateElement elem, int points)
	{
		assert(elem < FRAMERATE_MAX);
		points = min(points, _framerate_num_measurements[elem]);

		int first = _framerate_next_measurement_point[elem] - points;
		int last = _framerate_next_measurement_point[elem] - 1;
		if (first < 0) first += NUM_FRAMERATE_POINTS;
		if (last < 0) last += NUM_FRAMERATE_POINTS;

		points -= 1;
		if (points < 1) return TIMESTAMP_PRECISION;

		return (_framerate_timestamps[elem][last] - _framerate_timestamps[elem][first]) / points;
	}

	double MillisecondsToFps(TimingMeasurement ms)
	{
		return (double)TIMESTAMP_PRECISION / ms;
	}

	double GetFramerate(FramerateElement elem, int points = NUM_FRAMERATE_POINTS)
	{
		return MillisecondsToFps(GetAverageTimestep(elem, points));
	}

}


#ifdef WIN32
#include <windows.h>
static TimingMeasurement GetPerformanceTimer()
{
	LARGE_INTEGER pfc, pfq;
	if (QueryPerformanceFrequency(&pfq) && QueryPerformanceCounter(&pfc)) {
		return pfc.QuadPart * TIMESTAMP_PRECISION / pfq.QuadPart;
	} else {
		return GetTickCount() * 1000;
	}
}
#else
static TimingMeasurement GetPerformanceTimer()
{
	return GetTime() * 1000;
}
#endif


FramerateMeasurer::FramerateMeasurer(FramerateElement elem)
{
	assert(elem < FRAMERATE_MAX);

	this->elem = elem;
	this->start_time = GetPerformanceTimer();
}

FramerateMeasurer::~FramerateMeasurer()
{
	StoreMeasurement(this->elem, this->start_time, GetPerformanceTimer());
}

void FramerateMeasurer::SetExpectedRate(double rate)
{
	_framerate_expected_rate[this->elem] = rate;
}


void ShowFrametimeGraphWindow(FramerateElement elem);


enum FramerateWindowWidgets {
	WID_FRW_CAPTION,
	WID_FRW_TOGGLE_SIZE,
	WID_FRW_DETAILSPANEL,
	WID_FRW_RATE_GAMELOOP,
	WID_FRW_RATE_BLITTER,
	WID_FRW_INFO_DATA_POINTS,
	WID_FRW_TIMES_NAMES,
	WID_FRW_TIMES_CURRENT,
	WID_FRW_TIMES_AVERAGE,
};

static const NWidgetPart _framerate_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_FRAMERATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_IMGBTN, COLOUR_GREY, WID_FRW_TOGGLE_SIZE), SetDataTip(SPR_LARGE_SMALL_WINDOW, STR_TOOLTIP_TOGGLE_LARGE_SMALL_WINDOW),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPadding(6), SetPIP(0, 3, 0),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_RATE_GAMELOOP), SetDataTip(STR_FRAMERATE_RATE_GAMELOOP, STR_FRAMERATE_RATE_GAMELOOP_TOOLTIP),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_RATE_BLITTER),  SetDataTip(STR_FRAMERATE_RATE_BLITTER,  STR_FRAMERATE_RATE_BLITTER_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_SELECTION, INVALID_COLOUR, WID_FRW_DETAILSPANEL),
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
	EndContainer(),
};

struct FramerateWindow : Window {
	bool small;

	static const int VSPACING = 3;

	FramerateWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->SetSmall(true);
	}

	void SetSmall(bool small)
	{
		this->small = small;
		int plane = this->small ? SZSP_NONE : 0;
		this->GetWidget<NWidgetStacked>(WID_FRW_DETAILSPANEL)->SetDisplayedPlane(plane);
		this->ReInit();
	}

	virtual void OnTick()
	{
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

	static void SetDParamGoodWarnBadRate(double value, FramerateElement elem)
	{
		const double threshold_good = _framerate_expected_rate[elem] * 0.95;
		const double threshold_bad = _framerate_expected_rate[elem] * 2 / 3;

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
			case WID_FRW_RATE_GAMELOOP:
				value = GetFramerate(FRAMERATE_GAMELOOP, 8);
				SetDParamGoodWarnBadRate(value, FRAMERATE_GAMELOOP);
				break;
			case WID_FRW_RATE_BLITTER:
				value = GetFramerate(FRAMERATE_DRAWING, 8);
				SetDParamGoodWarnBadRate(value, FRAMERATE_DRAWING);
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
				SetDParamGoodWarnBadRate(9999.99, FRAMERATE_GAMELOOP);
				*size = GetStringBoundingBox(STR_FRAMERATE_RATE_GAMELOOP);
				break;
			case WID_FRW_RATE_BLITTER:
				SetDParamGoodWarnBadRate(9999.99, FRAMERATE_DRAWING);
				*size = GetStringBoundingBox(STR_FRAMERATE_RATE_BLITTER);
				break;

			case WID_FRW_TIMES_NAMES: {
				int linecount = FRAMERATE_MAX - FRAMERATE_FIRST;
				size->width = 0;
				size->height = FONT_HEIGHT_NORMAL * (linecount + 1) + VSPACING;
				for (int line = 0; line < linecount; line++) {
					auto line_size = GetStringBoundingBox(STR_FRAMERATE_GAMELOOP + line);
					size->width = max(size->width, line_size.width);
				}
				break;
			}

			case WID_FRW_TIMES_CURRENT:
			case WID_FRW_TIMES_AVERAGE: {
				int linecount = FRAMERATE_MAX - FRAMERATE_FIRST;
				*size = GetStringBoundingBox(STR_FRAMERATE_CURRENT + (widget - WID_FRW_TIMES_CURRENT));
				SetDParamGoodWarnBadDuration(9999.99);
				auto item_size = GetStringBoundingBox(STR_FRAMERATE_VALUE);
				size->width = max(size->width, item_size.width);
				size->height += FONT_HEIGHT_NORMAL * linecount + VSPACING;
				break;
			}

			default:
				Window::UpdateWidgetSize(widget, size, padding, fill, resize);
				break;
		}
	}

	typedef double (ElementValueExtractor)(FramerateElement elem);
	void DrawElementTimesColumn(const Rect &r, int heading_str, ElementValueExtractor get_value_func) const
	{
		int y = r.top;
		DrawString(r.left, r.right, y, heading_str, TC_FROMSTRING, SA_CENTER);
		y += FONT_HEIGHT_NORMAL + VSPACING;

		for (FramerateElement e = FRAMERATE_FIRST; e < FRAMERATE_MAX; e = (FramerateElement)(e + 1)) {
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
				int linecount = FRAMERATE_MAX - FRAMERATE_FIRST;
				int y = r.top + FONT_HEIGHT_NORMAL + VSPACING;
				for (int i = 0; i < linecount; i++) {
					DrawString(r.left, r.right, y, STR_FRAMERATE_GAMELOOP + i, TC_FROMSTRING, SA_LEFT);
					y += FONT_HEIGHT_NORMAL;
				}
				break;
			}
			case WID_FRW_TIMES_CURRENT:
				DrawElementTimesColumn(r, STR_FRAMERATE_CURRENT, [](FramerateElement elem) {
					return GetAverageDuration(elem, 8);
				});
				break;
			case WID_FRW_TIMES_AVERAGE:
				DrawElementTimesColumn(r, STR_FRAMERATE_AVERAGE, [](FramerateElement elem) {
					return GetAverageDuration(elem, NUM_FRAMERATE_POINTS);
				});
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_FRW_TOGGLE_SIZE:
				this->SetSmall(!this->small);
				break;

			case WID_FRW_TIMES_NAMES:
			case WID_FRW_TIMES_CURRENT:
			case WID_FRW_TIMES_AVERAGE: {
				int line = this->GetRowFromWidget(pt.y, widget, VSPACING, FONT_HEIGHT_NORMAL);
				if (line > 0) {
					line -= 1;
					ShowFrametimeGraphWindow((FramerateElement)line);
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
	mutable int vertical_scale;   ///< number of TIMESTAMP_PRECISION units vertically
	int horizontal_scale;         ///< number of half-second units horizontally

	FramerateElement element;
	Dimension graph_size;

	FrametimeGraphWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->element = (FramerateElement)number;
		this->horizontal_scale = 4;
		this->vertical_scale = TIMESTAMP_PRECISION / 10;

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
			SetDParam(0, this->vertical_scale);
			auto size_ms_label = GetStringBoundingBox(STR_FRAMERATE_GRAPH_MILLISECONDS);
			SetDParam(0, this->horizontal_scale / 2);
			auto size_s_label = GetStringBoundingBox(STR_FRAMERATE_GRAPH_SECONDS);

			graph_size.width = this->horizontal_scale * max<uint>(100, size_s_label.width + 10) / 2;
			graph_size.height = max<uint>(100, 10 * (size_ms_label.height + 1));
			*size = graph_size;

			size->width += size_ms_label.width + 2;
			size->height += size_s_label.height + 2;
		}
	}

	virtual void OnTick()
	{
		this->SetDirty();
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
			auto &durations = _framerate_durations[this->element];
			auto &timestamps = _framerate_timestamps[this->element];
			int point = _framerate_next_measurement_point[this->element] - 1;
			if (point < 0) point = NUM_FRAMERATE_POINTS - 1;

			int x_zero = r.right - this->graph_size.width;
			int x_max = r.right;
			int y_zero = r.top + this->graph_size.height;
			int y_max = r.top;
			int c_grid = PC_DARK_GREY;
			int c_lines = PC_BLACK;
			int c_peak = PC_DARK_RED;

			int draw_horz_scale = this->horizontal_scale;
			if (this->vertical_scale >= TIMESTAMP_PRECISION) draw_horz_scale *= 5;

			for (int division = 0; division < 10; division++) {
				int y = Scinterlate(y_zero, y_max, 0, 10, division);
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
			for (int division = draw_horz_scale; division > 0; division--) {
				int x = Scinterlate(x_zero, x_max, 0, draw_horz_scale, draw_horz_scale - division);
				GfxDrawLine(x, y_max, x, y_zero, c_grid);
				if (division % 2 == 0) {
					SetDParam(0, division / 2);
					DrawString(x, x_max, y_zero + 2, STR_FRAMERATE_GRAPH_SECONDS, TC_GREY, SA_LEFT | SA_FORCE, false, FS_SMALL);
				}
			}

			Point lastpoint{
				x_max,
				Scinterlate(y_zero, y_max, 0, this->vertical_scale, (int)durations[point])
			};
			TimingMeasurement firsttime = timestamps[point];

			TimingMeasurement peak_value = 0;
			Point peak_point;
			TimingMeasurement value_sum = 0;
			int points_drawn = 0;

			for (int i = 1; i < NUM_FRAMERATE_POINTS; i++) {
				point--;
				if (point < 0) point = NUM_FRAMERATE_POINTS - 1;

				TimingMeasurement value = durations[point];
				TimingMeasurement timediff = (firsttime - timestamps[point]) * 1000 / TIMESTAMP_PRECISION;
				if ((int)timediff > draw_horz_scale * 500) break;

				Point newpoint{
					Scinterlate(x_zero, x_max, 0, draw_horz_scale * 500, draw_horz_scale * 500 - timediff),
					Scinterlate(y_zero, y_max, 0, this->vertical_scale, (int)value)
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

			if (peak_value < TIMESTAMP_PRECISION / 50) this->vertical_scale = TIMESTAMP_PRECISION / 50;
			else if (peak_value < TIMESTAMP_PRECISION / 10) this->vertical_scale = TIMESTAMP_PRECISION / 10;
			else if (peak_value < TIMESTAMP_PRECISION / 5) this->vertical_scale = TIMESTAMP_PRECISION / 5;
			else if (peak_value < TIMESTAMP_PRECISION / 2) this->vertical_scale = TIMESTAMP_PRECISION / 2;
			else if (peak_value < TIMESTAMP_PRECISION) this->vertical_scale = TIMESTAMP_PRECISION;
			else if (peak_value < TIMESTAMP_PRECISION * 5) this->vertical_scale = TIMESTAMP_PRECISION * 5;
			else this->vertical_scale = TIMESTAMP_PRECISION * 10;
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

void ShowFrametimeGraphWindow(FramerateElement elem)
{
	if (elem < FRAMERATE_FIRST || elem >= FRAMERATE_MAX) return; // maybe warn?
	AllocateWindowDescFront<FrametimeGraphWindow>(&_frametime_graph_window_desc, elem, true);
}

void ConPrintFramerate()
{
	const int count1 = NUM_FRAMERATE_POINTS / 8;
	const int count2 = NUM_FRAMERATE_POINTS / 4;
	const int count3 = NUM_FRAMERATE_POINTS / 1;

	IConsolePrintF(TC_SILVER, "Based on num. data points: %d %d %d", count1, count2, count3);

	static char *MEASUREMENT_NAMES[FRAMERATE_MAX] = {
		"Game loop",
		"Drawing",
		"Video output",
		"Sound mixing",
	};

	for (FramerateElement e = FRAMERATE_FIRST; e < FRAMERATE_MAX; e = (FramerateElement)(e + 1)) {
		IConsolePrintF(TC_GREEN, "%s rate: %.2ffps  %.2ffps  %.2ffps  (expected: %.2ffps)",
			MEASUREMENT_NAMES[e],
			GetFramerate(e, count1),
			GetFramerate(e, count2),
			GetFramerate(e, count3),
			_framerate_expected_rate[e]);
	}

	for (FramerateElement e = FRAMERATE_FIRST; e < FRAMERATE_MAX; e = (FramerateElement)(e + 1)) {
		IConsolePrintF(TC_LIGHT_BLUE, "%s times: %.2fms  %.2fms  %.2fms",
			MEASUREMENT_NAMES[e],
			GetAverageDuration(e, count1),
			GetAverageDuration(e, count2),
			GetAverageDuration(e, count3));
	}
}
