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

	const int NUM_FRAMERATE_POINTS = 128;

	uint32 _framerate_durations[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	uint32 _framerate_timestamps[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	double _framerate_expected_rate[FRAMERATE_MAX] = {
		1000.0 / MILLISECONDS_PER_TICK, // FRAMERATE_GAMELOOP
		1000.0 / MILLISECONDS_PER_TICK, // FRAMERATE_DRAWING
		60.0,                           // FRAMERATE_VIDEO
		1000.0 * 8192 / 44100,          // FRAMERATE_SOUND
	};
	int _framerate_next_measurement_point[FRAMERATE_MAX] = {};
	int _framerate_num_measurements[FRAMERATE_MAX] = {};

	void StoreMeasurement(FramerateElement elem, uint32 start_time, uint32 end_time)
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

		return sumtime / points;
	}

	uint32 GetAverageTimestep(FramerateElement elem, int points)
	{
		assert(elem < FRAMERATE_MAX);
		points = min(points, _framerate_num_measurements[elem]);

		int first = _framerate_next_measurement_point[elem] - points;
		int last = _framerate_next_measurement_point[elem] - 1;
		if (first < 0) first += NUM_FRAMERATE_POINTS;
		if (last < 0) last += NUM_FRAMERATE_POINTS;

		points -= 1;
		if (points < 1) return 1000;

		return (_framerate_timestamps[elem][last] - _framerate_timestamps[elem][first]) / points;
	}

	double MillisecondsToFps(uint32 ms)
	{
		return 1000.0 / ms;
	}

	double GetFramerate(FramerateElement elem)
	{
		return MillisecondsToFps(GetAverageTimestep(elem, NUM_FRAMERATE_POINTS));
	}

}


#ifdef WIN32
#include <windows.h>
static uint32 GetTime()
{
	LARGE_INTEGER pfc, pfq;
	if (QueryPerformanceFrequency(&pfq) && QueryPerformanceCounter(&pfc)) {
		pfq.QuadPart /= 1000;
		return pfc.QuadPart / pfq.QuadPart;
	} else {
		return GetTickCount();
	}
}
#endif


FramerateMeasurer::FramerateMeasurer(FramerateElement elem)
{
	assert(elem < FRAMERATE_MAX);

	this->elem = elem;
	this->start_time = GetTime();
}

FramerateMeasurer::~FramerateMeasurer()
{
	StoreMeasurement(this->elem, this->start_time, GetTime());
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
	WID_FRW_TIMES_WORST,
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
					NWidget(WWT_EMPTY, COLOUR_GREY, WID_FRW_TIMES_WORST),
				EndContainer(),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_INFO_DATA_POINTS), SetDataTip(STR_FRAMERATE_DATA_POINTS, 0x0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct FramerateWindow : Window {
	bool small;
	uint32 last_update;

	static const int VSPACING = 3;
	static const uint32 UPDATE_INTERVAL = 200;

	FramerateWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->last_update = 0;
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

	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->SetDirty();
	}

	virtual void OnTick()
	{
		uint32 now = GetTime();
		if (now - this->last_update >= UPDATE_INTERVAL) {
			this->last_update = now;
			this->InvalidateData();
		}
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
				value = GetFramerate(FRAMERATE_GAMELOOP);
				SetDParamGoodWarnBadRate(value, FRAMERATE_GAMELOOP);
				break;
			case WID_FRW_RATE_BLITTER:
				value = GetFramerate(FRAMERATE_DRAWING);
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
			case WID_FRW_TIMES_AVERAGE:
			case WID_FRW_TIMES_WORST: {
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
			case WID_FRW_TIMES_WORST:
				DrawElementTimesColumn(r, STR_FRAMERATE_WORST, [](FramerateElement elem) {
					uint32 worst = 0;
					for (int i = 0; i < NUM_FRAMERATE_POINTS; i++) {
						worst = max(worst, _framerate_durations[elem][i]);
					}
					return (double)worst;
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

			case WID_FRW_TIMES_NAMES: {
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
	FramerateElement element;

	FrametimeGraphWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->element = (FramerateElement)number;
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
			size->width = NUM_FRAMERATE_POINTS;
			size->height = 100;
		}
	}

	virtual void OnTick()
	{
		this->SetDirty();
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget == WID_FGW_GRAPH) {
			for (int i = 0; i < NUM_FRAMERATE_POINTS; i++) {
				int point = _framerate_next_measurement_point[this->element] + i;
				uint32 value = _framerate_durations[this->element][point % NUM_FRAMERATE_POINTS];
				GfxDrawLine(r.left + i, r.bottom - value, r.left + i, r.bottom, 0);
			}
		}
	}
};

static WindowDesc _frametime_graph_window_desc(
	WDP_AUTO, "frametime_graph", 140, 90,
	WC_FRAMETIME_GRAPH, WC_FRAMERATE_DISPLAY,
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
	const int count1 = NUM_FRAMERATE_POINTS / 1;
	const int count2 = NUM_FRAMERATE_POINTS / 4;
	const int count3 = NUM_FRAMERATE_POINTS / 8;

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
			MillisecondsToFps(GetAverageTimestep(e, count1)),
			MillisecondsToFps(GetAverageTimestep(e, count2)),
			MillisecondsToFps(GetAverageTimestep(e, count3)),
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
