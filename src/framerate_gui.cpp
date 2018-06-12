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

	uint32 _framerate_durations[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	uint32 _framerate_timestamps[FRAMERATE_MAX][NUM_FRAMERATE_POINTS] = {};
	double _framerate_expected_rate[FRAMERATE_MAX] = {
		1000.0 / MILLISECONDS_PER_TICK, // FRAMERATE_GAMELOOP
		1000.0 / MILLISECONDS_PER_TICK, // FRAMERATE_DRAWING
		60.0,                           // FRAMERATE_VIDEO
		1000.0 * 8192 / 44100,          // FRAMERATE_SOUND
	};
	int _framerate_next_measurement_point[FRAMERATE_MAX] = {};

	void StoreMeasurement(FramerateElement elem, uint32 start_time, uint32 end_time)
	{
		_framerate_durations[elem][_framerate_next_measurement_point[elem]] = end_time - start_time;
		_framerate_timestamps[elem][_framerate_next_measurement_point[elem]] = start_time;
		_framerate_next_measurement_point[elem] += 1;
		_framerate_next_measurement_point[elem] %= NUM_FRAMERATE_POINTS;
	}

	double GetAverageDuration(FramerateElement elem, int points)
	{
		assert(elem < FRAMERATE_MAX);

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

		int first = _framerate_next_measurement_point[elem] - points;
		int last = _framerate_next_measurement_point[elem] - 1;
		if (first < 0) first += NUM_FRAMERATE_POINTS;
		if (last < 0) last += NUM_FRAMERATE_POINTS;

		return (_framerate_timestamps[elem][last] - _framerate_timestamps[elem][first]) / points;
	}

	double MillisecondsToFps(uint32 ms)
	{
		return 1000.0 / ms;
	}

}


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


enum FramerateWindowWidgets {
	WID_FRW_CAPTION,
	WID_FRW_TIMES_GAMELOOP,
	WID_FRW_TIMES_DRAWING,
	WID_FRW_TIMES_VIDEO,
	WID_FRW_TIMES_SOUND,
	WID_FRW_FPS_GAMELOOP,
	WID_FRW_FPS_DRAWING,
	WID_FRW_FPS_VIDEO,
	WID_FRW_FPS_SOUND,
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

	static void SetDParmGoodWarnBadDuration(double value)
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

	static void SetDParmGoodWarnBadRate(double value, FramerateElement elem)
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
		static char text_value[FRAMERATE_MAX][32];
		double value;

		switch (widget) {
			case WID_FRW_TIMES_GAMELOOP:
				value = GetAverageDuration(FRAMERATE_GAMELOOP, NUM_FRAMERATE_POINTS);
				SetDParmGoodWarnBadDuration(value);
				break;
			case WID_FRW_TIMES_DRAWING:
				value = GetAverageDuration(FRAMERATE_DRAWING, NUM_FRAMERATE_POINTS);
				SetDParmGoodWarnBadDuration(value);
				break;
			case WID_FRW_TIMES_VIDEO:
				value = GetAverageDuration(FRAMERATE_VIDEO, NUM_FRAMERATE_POINTS);
				SetDParmGoodWarnBadDuration(value);
				break;
			case WID_FRW_TIMES_SOUND:
				value = GetAverageDuration(FRAMERATE_SOUND, NUM_FRAMERATE_POINTS);
				SetDParmGoodWarnBadDuration(value);
				break;

			case WID_FRW_FPS_GAMELOOP:
				value = MillisecondsToFps(GetAverageTimestep(FRAMERATE_GAMELOOP, NUM_FRAMERATE_POINTS));
				SetDParmGoodWarnBadRate(value, FRAMERATE_GAMELOOP);
				break;
			case WID_FRW_FPS_DRAWING:
				value = MillisecondsToFps(GetAverageTimestep(FRAMERATE_DRAWING, NUM_FRAMERATE_POINTS));
				SetDParmGoodWarnBadRate(value, FRAMERATE_DRAWING);
				break;
			case WID_FRW_FPS_VIDEO:
				value = MillisecondsToFps(GetAverageTimestep(FRAMERATE_VIDEO, NUM_FRAMERATE_POINTS));
				SetDParmGoodWarnBadRate(value, FRAMERATE_VIDEO);
				break;
			case WID_FRW_FPS_SOUND:
				value = MillisecondsToFps(GetAverageTimestep(FRAMERATE_SOUND, NUM_FRAMERATE_POINTS));
				SetDParmGoodWarnBadRate(value, FRAMERATE_SOUND);
				break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_FRW_TIMES_GAMELOOP:
			case WID_FRW_TIMES_DRAWING:
			case WID_FRW_TIMES_VIDEO:
			case WID_FRW_TIMES_SOUND:
				SetDParmGoodWarnBadDuration(9999.99);
				*size = GetStringBoundingBox(STR_FRAMERATE_DISPLAY_VALUE);
				break;

			case WID_FRW_FPS_GAMELOOP:
			case WID_FRW_FPS_DRAWING:
			case WID_FRW_FPS_VIDEO:
			case WID_FRW_FPS_SOUND:
				SetDParmGoodWarnBadRate(9999.99, FRAMERATE_GAMELOOP);
				*size = GetStringBoundingBox(STR_FRAMERATE_DISPLAY_VALUE);
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
		NWidget(NWID_HORIZONTAL),  SetPIP(6, 3, 6),
			NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_FRAMERATE_DISPLAY_GAMELOOP, STR_FRAMERATE_DISPLAY_GAMELOOP_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_FRAMERATE_DISPLAY_DRAWING,  STR_FRAMERATE_DISPLAY_DRAWING_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_FRAMERATE_DISPLAY_VIDEO,    STR_FRAMERATE_DISPLAY_VIDEO_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY), SetDataTip(STR_FRAMERATE_DISPLAY_SOUND,    STR_FRAMERATE_DISPLAY_SOUND_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_GAMELOOP), SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_GAMELOOP_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_DRAWING),  SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_DRAWING_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_VIDEO),    SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_VIDEO_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_TIMES_SOUND),    SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_SOUND_TOOLTIP),
			EndContainer(),
			NWidget(NWID_VERTICAL), SetPIP(6, 6, 6),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_FPS_GAMELOOP),   SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_GAMELOOP_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_FPS_DRAWING),    SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_DRAWING_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_FPS_VIDEO),      SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_VIDEO_TOOLTIP),
				NWidget(WWT_TEXT, COLOUR_GREY, WID_FRW_FPS_SOUND),      SetDataTip(STR_FRAMERATE_DISPLAY_VALUE, STR_FRAMERATE_DISPLAY_SOUND_TOOLTIP),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _framerate_display_desc(
	WDP_AUTO, "framerate_display", 60, 40,
	WC_FRAMERATE_DISPLAY, WC_NONE,
	0,
	_framerate_window_widgets, lengthof(_framerate_window_widgets)
);


void ShowFramerateWindow()
{
	AllocateWindowDescFront<FramerateWindow>(&_framerate_display_desc, 0);
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
