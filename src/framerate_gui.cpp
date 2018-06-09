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
#include "strings_func.h"
#include "debug.h"
#include "console_func.h"
#include "console_type.h"


namespace {

	const int NUM_FRAMERATE_POINTS = 128;

	static uint32 _framerate_measurements[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	static int _framerate_next_measurement_point[FRAMERATE_MAX] = {};

	void StoreMeasurement(FramerateElement elem, uint32 value)
	{
		_framerate_measurements[elem][_framerate_next_measurement_point[elem]] = value;
		_framerate_next_measurement_point[elem] += 1;
		_framerate_next_measurement_point[elem] %= NUM_FRAMERATE_POINTS;
	}

	template <int Points>
	double GetAverageMeasurement(FramerateElement elem)
	{
		assert(elem > FRAMERATE_OVERALL); // overall is absolute times, not difference times
		assert(elem < FRAMERATE_MAX);

		int first_point = _framerate_next_measurement_point[elem] - Points - 1;
		while (first_point < 0) first_point += NUM_FRAMERATE_POINTS;

		double sumtime = 0;
		for (int i = first_point; i < first_point + Points; i++) {
			sumtime += _framerate_measurements[elem][i % NUM_FRAMERATE_POINTS];
		}

		return sumtime / Points;
	}

	template <int Points>
	uint32 GetOverallAverage()
	{
		int first, last;

		first = _framerate_next_measurement_point[FRAMERATE_OVERALL] - Points;
		last = _framerate_next_measurement_point[FRAMERATE_OVERALL] - 1;
		if (first < 0) first += NUM_FRAMERATE_POINTS;
		if (last < 0) last += NUM_FRAMERATE_POINTS;

		return (_framerate_measurements[FRAMERATE_OVERALL][last] - _framerate_measurements[FRAMERATE_OVERALL][first]) / Points;
	}

	double MillisecondsToFps(uint32 ms)
	{
		return 1000.0 / ms;
	}

}


FramerateMeasurer::FramerateMeasurer(FramerateElement elem)
{
	assert(elem > FRAMERATE_OVERALL); // not allowed to measure "overall", happens automatically with gameloop
	assert(elem < FRAMERATE_MAX);
	this->elem = elem;

	this->start_time = GetTime();

	if (elem == FRAMERATE_GAMELOOP) {
		StoreMeasurement(FRAMERATE_OVERALL, this->start_time);
	}
}

FramerateMeasurer::~FramerateMeasurer()
{
	StoreMeasurement(this->elem, GetTime() - this->start_time);
}


enum FramerateWindowWidgets {
	WID_FRW_CAPTION,
	WID_FRW_CURRENT_FPS,
	WID_FRW_TIMES_GAMELOOP,
	WID_FRW_TIMES_DRAWING,
	WID_FRW_TIMES_VIDEO,
};

struct FramerateWindow : Window {

	FramerateWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
	}

	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		if (!gui_scope) return;
		this->SetDirty();
	}

	virtual void OnTick()
	{
		this->InvalidateData();
	}

	static void SetDParmGoodWarnBadSmall(double value, double threshold_good, double threshold_bad)
	{
		uint tpl;
		if (value < threshold_good) tpl = STR_FRAMERATE_VALUE_GOOD;
		else if (value > threshold_bad) tpl = STR_FRAMERATE_VALUE_BAD;
		else tpl = STR_FRAMERATE_VALUE_WARN;
		value = min(9999.99, value);
		SetDParam(0, tpl);
		SetDParam(1, (int)(value * 100));
		SetDParam(2, 2);
	}

	static void SetDParmGoodWarnBadLarge(double value, double threshold_good, double threshold_bad)
	{
		uint tpl;
		if (value > threshold_good) tpl = STR_FRAMERATE_VALUE_GOOD;
		else if (value < threshold_bad) tpl = STR_FRAMERATE_VALUE_BAD;
		else tpl = STR_FRAMERATE_VALUE_WARN;
		value = min(9999.99, value);
		SetDParam(0, tpl);
		SetDParam(1, (int)(value * 100));
		SetDParam(2, 2);
	}

	virtual void SetStringParameters(int widget) const
	{
		static char text_value[FRAMERATE_MAX][32];
		double value;

		switch (widget) {
			case WID_FRW_CURRENT_FPS:
				value = MillisecondsToFps(GetOverallAverage<NUM_FRAMERATE_POINTS>());
				SetDParmGoodWarnBadLarge(value, 20, 32); // "target" framerate is 33.33, anything less indicates a performance problem
				break;
			case WID_FRW_TIMES_GAMELOOP:
				value = GetAverageMeasurement<NUM_FRAMERATE_POINTS>(FRAMERATE_GAMELOOP);
				SetDParmGoodWarnBadSmall(value, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				break;
			case WID_FRW_TIMES_DRAWING:
				value = GetAverageMeasurement<NUM_FRAMERATE_POINTS>(FRAMERATE_DRAWING);
				SetDParmGoodWarnBadSmall(value, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				break;
			case WID_FRW_TIMES_VIDEO:
				value = GetAverageMeasurement<NUM_FRAMERATE_POINTS>(FRAMERATE_VIDEO);
				SetDParmGoodWarnBadSmall(value, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_FRW_CURRENT_FPS:
				SetDParmGoodWarnBadLarge(9999.99, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				*size = GetStringBoundingBox(STR_FRAMERATE_DISPLAY_CURRENT_FPS);
				break;

			case WID_FRW_TIMES_GAMELOOP:
				SetDParmGoodWarnBadSmall(9999.99, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				*size = GetStringBoundingBox(STR_FRAMERATE_DISPLAY_TIMES_GAMELOOP);
				break;

			case WID_FRW_TIMES_DRAWING:
				SetDParmGoodWarnBadSmall(9999.99, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				*size = GetStringBoundingBox(STR_FRAMERATE_DISPLAY_TIMES_DRAWING);
				break;

			case WID_FRW_TIMES_VIDEO:
				SetDParmGoodWarnBadSmall(9999.99, MILLISECONDS_PER_TICK / 3, MILLISECONDS_PER_TICK);
				*size = GetStringBoundingBox(STR_FRAMERATE_DISPLAY_TIMES_VIDEO);
				break;

			default:
				Window::UpdateWidgetSize(widget, size, padding, fill, resize);
				break;
		}
	}
};

static const NWidgetPart _framerate_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_FRAMERATE_DISPLAY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, 3, 0), SetPadding(3, 3, 3, 3),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_CURRENT_FPS),    SetDataTip(STR_FRAMERATE_DISPLAY_CURRENT_FPS,    STR_FRAMERATE_DISPLAY_CURRENT_FPS_TOOLTIP),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_GAMELOOP), SetDataTip(STR_FRAMERATE_DISPLAY_TIMES_GAMELOOP, STR_FRAMERATE_DISPLAY_TIMES_GAMELOOP_TOOLTIP),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_DRAWING),  SetDataTip(STR_FRAMERATE_DISPLAY_TIMES_DRAWING,  STR_FRAMERATE_DISPLAY_TIMES_DRAWING_TOOLTIP),
			NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_VIDEO),    SetDataTip(STR_FRAMERATE_DISPLAY_TIMES_VIDEO,    STR_FRAMERATE_DISPLAY_TIMES_VIDEO_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _framerate_display_desc(
	WDP_AUTO, "framerate_display", 60, 40,
	WC_FRAMERATE_DISPLAY, WC_NONE,
	0,
	_framerate_window_widgets, lengthof(_framerate_window_widgets)
);


void DoShowFramerate()
{
	const int count1 = NUM_FRAMERATE_POINTS / 1;
	const int count2 = NUM_FRAMERATE_POINTS / 4;
	const int count3 = NUM_FRAMERATE_POINTS / 8;

	IConsolePrintF(TC_SILVER, "Based on num. data points: %d %d %d", count1, count2, count3);

	IConsolePrintF(TC_LIGHT_BLUE, "Overall framerate: %.2ffps  %.2ffps  %.2ffps",
		MillisecondsToFps(GetOverallAverage<count1>()),
		MillisecondsToFps(GetOverallAverage<count2>()),
		MillisecondsToFps(GetOverallAverage<count3>()));

	static char *MEASUREMENT_NAMES[FRAMERATE_MAX] = {
		"Overall",
		"Gameloop",
		"Drawing",
		"Video output",
	};

	for (FramerateElement e = FRAMERATE_OVERALL; e < FRAMERATE_MAX; e = (FramerateElement)(e + 1)) {
		if (e == FRAMERATE_OVERALL) continue;
		IConsolePrintF(TC_LIGHT_BLUE, "%s times: %.2fms  %.2fms  %.2fms",
			MEASUREMENT_NAMES[e],
			GetAverageMeasurement<count1>(e),
			GetAverageMeasurement<count2>(e),
			GetAverageMeasurement<count3>(e));
	}

	AllocateWindowDescFront<FramerateWindow>(&_framerate_display_desc, 0);
}
