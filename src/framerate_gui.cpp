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

#include "widgets/framerate_widget.h"


/**
 * Private declarations for performance measurement implementation
 */
namespace {

	/** Number of data points to keep in buffer for each performance measurement */
	const int NUM_FRAMERATE_POINTS = 512;
	/** %Units a second is divided into in performance measurements */
	const TimingMeasurement TIMESTAMP_PRECISION = 1000000;

	struct PerformanceData {
		/** Duration value indicating the value is not valid should be considered a gap in measurements */
		static const TimingMeasurement INVALID_DURATION = UINT64_MAX;

		/** Time spent processing each cycle of the performance element, circular buffer */
		TimingMeasurement durations[NUM_FRAMERATE_POINTS];
		/** Start time of each cycle of the performance element, circular buffer */
		TimingMeasurement timestamps[NUM_FRAMERATE_POINTS];
		/** Expected number of cycles per second when the system is running without slowdowns */
		double expected_rate;
		/** Next index to write to in \c durations and \c timestamps */
		int next_index;
		/** Last index written to in \c durations and \c timestamps */
		int prev_index;
		/** Number of data points recorded, clamped to \c NUM_FRAMERATE_POINTS */
		int num_valid;

		/** Current accumulated duration */
		TimingMeasurement acc_duration;
		/** Start time for current accumulation cycle */
		TimingMeasurement acc_timestamp;

		/**
		 * Initialize a data element with an expected collection rate
		 * @param expected_rate
		 * Expected number of cycles per second of the performance element. Use 1 if unknown or not relevant.
		 * The rate is used for highlighting slow-running elements in the GUI.
		 */
		explicit PerformanceData(double expected_rate) : expected_rate(expected_rate), next_index(0), prev_index(0), num_valid(0) { }

		/** Collect a complete measurement, given start and ending times for a processing block */
		void Add(TimingMeasurement start_time, TimingMeasurement end_time)
		{
			this->durations[this->next_index] = end_time - start_time;
			this->timestamps[this->next_index] = start_time;
			this->prev_index = this->next_index;
			this->next_index += 1;
			if (this->next_index >= NUM_FRAMERATE_POINTS) this->next_index = 0;
			this->num_valid = min(NUM_FRAMERATE_POINTS, this->num_valid + 1);
		}

		/** Begin an accumulation of multiple measurements into a single value, from a given start time */
		void BeginAccumulate(TimingMeasurement start_time)
		{
			this->timestamps[this->next_index] = this->acc_timestamp;
			this->durations[this->next_index] = this->acc_duration;
			this->prev_index = this->next_index;
			this->next_index += 1;
			if (this->next_index >= NUM_FRAMERATE_POINTS) this->next_index = 0;
			this->num_valid = min(NUM_FRAMERATE_POINTS, this->num_valid + 1);

			this->acc_duration = 0;
			this->acc_timestamp = start_time;
		}

		/** Accumulate a period onto the current measurement */
		void AddAccumulate(TimingMeasurement duration)
		{
			this->acc_duration += duration;
		}

		/** Indicate a pause/expected discontinuity in processing the element */
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

		/** Get average cycle processing time over a number of data points */
		double GetAverageDurationMilliseconds(int count)
		{
			count = min(count, this->num_valid);

			int first_point = this->prev_index - count;
			if (first_point < 0) first_point += NUM_FRAMERATE_POINTS;

			/* Sum durations, skipping invalid points */
			double sumtime = 0;
			for (int i = first_point; i < first_point + count; i++) {
				auto d = this->durations[i % NUM_FRAMERATE_POINTS];
				if (d != INVALID_DURATION) {
					sumtime += d;
				} else {
					/* Don't count the invalid durations */
					count--;
				}
			}

			if (count == 0) return 0; // avoid div by zero
			return sumtime * 1000 / count / TIMESTAMP_PRECISION;
		}

		/** Get current rate of a performance element, based on approximately the past one second of data */
		double GetRate()
		{
			/* Start at last recorded point, end at latest when reaching the earliest recorded point */
			int point = this->prev_index;
			int last_point = this->next_index - this->num_valid;
			if (last_point < 0) last_point += NUM_FRAMERATE_POINTS;

			/* Number of data points collected */
			int count = 0;
			/* Time of previous data point */
			TimingMeasurement last = this->timestamps[point];
			/* Total duration covered by collected points */
			TimingMeasurement total = 0;

			while (point != last_point) {
				/* Only record valid data points, but pretend the gaps in measurements aren't there */
				if (this->durations[point] != INVALID_DURATION) {
					total += last - this->timestamps[point];
					count++;
				}
				last = this->timestamps[point];
				if (total >= TIMESTAMP_PRECISION) break; // end after 1 second has been collected
				point--;
				if (point < 0) point = NUM_FRAMERATE_POINTS - 1;
			}

			if (total == 0 || count == 0) return 0;
			return (double)count * TIMESTAMP_PRECISION / total;
		}
	};

	/** %Game loop rate, cycles per second */
	static const double GL_RATE = 1000.0 / MILLISECONDS_PER_TICK;

	/**
	 * Storage for all performance element measurements.
	 * Elements are initialized with the expected rate in recorded values per second.
	 * @hideinitializer
	 */
	PerformanceData _pf_data[PFE_MAX] = {
		PerformanceData(GL_RATE),               // PFE_GAMELOOP
		PerformanceData(1),                     // PFE_ACC_GL_ECONOMY
		PerformanceData(1),                     // PFE_ACC_GL_TRAINS
		PerformanceData(1),                     // PFE_ACC_GL_ROADVEHS
		PerformanceData(1),                     // PFE_ACC_GL_SHIPS
		PerformanceData(1),                     // PFE_ACC_GL_AIRCRAFT
		PerformanceData(1),                     // PFE_GL_LANDSCAPE
		PerformanceData(1),                     // PFE_GL_LINKGRAPH
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
	using namespace std::chrono;
	return (TimingMeasurement)time_point_cast<microseconds>(high_resolution_clock::now()).time_since_epoch().count();
}


/**
 * Begin a cycle of a measured element.
 * @param elem The element to be measured
 */
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

/**
 * Indicate that a cycle of "pause" where no processing occurs.
 * @param elem The element not currently being processed
 */
void PerformanceMeasurer::Paused(PerformanceElement elem)
{
	_pf_data[elem].AddPause(GetPerformanceTimer());
}


/**
 * Begin measuring one block of the accumulating value.
 * @param elem The element to be measured
 */
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

/**
 * Store the previous accumulator value and reset for a new cycle of accumulating measurements.
 * @note This function must be called once per frame, otherwise measurements are not collected.
 * @param elem The element to begin a new measurement cycle of
 */
void PerformanceAccumulator::Reset(PerformanceElement elem)
{
	_pf_data[elem].BeginAccumulate(GetPerformanceTimer());
}


void ShowFrametimeGraphWindow(PerformanceElement elem);


/** @hideinitializer */
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
	uint32 next_update;

	struct CachedDecimal {
		StringID strid;
		uint32 value;

		inline void SetRate(double value, double target)
		{
			const double threshold_good = target * 0.95;
			const double threshold_bad = target * 2 / 3;
			value = min(9999.99, value);
			this->value = (uint32)(value * 100);
			this->strid = (value > threshold_good) ? STR_FRAMERATE_FPS_GOOD : (value < threshold_bad) ? STR_FRAMERATE_FPS_BAD : STR_FRAMERATE_FPS_WARN;
		}

		inline void SetTime(double value, double target)
		{
			const double threshold_good = target / 3;
			const double threshold_bad = target;
			value = min(9999.99, value);
			this->value = (uint32)(value * 100);
			this->strid = (value < threshold_good) ? STR_FRAMERATE_MS_GOOD : (value > threshold_bad) ? STR_FRAMERATE_MS_BAD : STR_FRAMERATE_MS_WARN;
		}

		inline void InsertDParams(uint n) const
		{
			SetDParam(n, this->value);
			SetDParam(n + 1, 2);
		}
	};

	CachedDecimal rate_gameloop;            ///< cached game loop tick rate
	CachedDecimal rate_drawing;             ///< cached drawing frame rate
	CachedDecimal speed_gameloop;           ///< cached game loop speed factor
	CachedDecimal times_shortterm[PFE_MAX]; ///< cached short term average times
	CachedDecimal times_longterm[PFE_MAX];  ///< cached long term average times

	static const int VSPACING = 3; ///< space between column heading and values

	FramerateWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->small = this->IsShaded();
		this->UpdateData();
	}

	virtual void OnTick()
	{
		/* Check if the shaded state has changed, switch caption text if it has */
		if (this->small != this->IsShaded()) {
			this->small = this->IsShaded();
			this->GetWidget<NWidgetLeaf>(WID_FRW_CAPTION)->SetDataTip(this->small ? STR_FRAMERATE_CAPTION_SMALL : STR_FRAMERATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS);
			this->next_update = 0;
		}

		if (_realtime_tick >= this->next_update) {
			this->UpdateData();
			this->SetDirty();
			this->next_update = _realtime_tick + 100;
		}
	}

	void UpdateData()
	{
		double gl_rate = _pf_data[PFE_GAMELOOP].GetRate();
		this->rate_gameloop.SetRate(gl_rate, _pf_data[PFE_GAMELOOP].expected_rate);
		this->speed_gameloop.SetRate(gl_rate / _pf_data[PFE_GAMELOOP].expected_rate, 1.0);
		if (this->small) return; // in small mode, this is everything needed

		this->rate_drawing.SetRate(_pf_data[PFE_DRAWING].GetRate(), _pf_data[PFE_DRAWING].expected_rate);

		for (PerformanceElement e = PFE_FIRST; e < PFE_MAX; e++) {
			this->times_shortterm[e].SetTime(_pf_data[e].GetAverageDurationMilliseconds(8), MILLISECONDS_PER_TICK);
			this->times_longterm[e].SetTime(_pf_data[e].GetAverageDurationMilliseconds(NUM_FRAMERATE_POINTS), MILLISECONDS_PER_TICK);
		}
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_FRW_CAPTION:
				/* When the window is shaded, the caption shows game loop rate and speed factor */
				if (!this->small) break;
				SetDParam(0, this->rate_gameloop.strid);
				this->rate_gameloop.InsertDParams(1);
				this->speed_gameloop.InsertDParams(3);
				break;

			case WID_FRW_RATE_GAMELOOP:
				SetDParam(0, this->rate_gameloop.strid);
				this->rate_gameloop.InsertDParams(1);
				break;
			case WID_FRW_RATE_DRAWING:
				SetDParam(0, this->rate_drawing.strid);
				this->rate_drawing.InsertDParams(1);
				break;
			case WID_FRW_RATE_FACTOR:
				this->speed_gameloop.InsertDParams(0);
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
				SetDParam(0, STR_FRAMERATE_FPS_GOOD);
				SetDParam(1, 999999);
				SetDParam(2, 2);
				*size = GetStringBoundingBox(STR_FRAMERATE_RATE_GAMELOOP);
				break;
			case WID_FRW_RATE_DRAWING:
				SetDParam(0, STR_FRAMERATE_FPS_GOOD);
				SetDParam(1, 999999);
				SetDParam(2, 2);
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
				SetDParam(0, 999999);
				SetDParam(1, 2);
				Dimension item_size = GetStringBoundingBox(STR_FRAMERATE_MS_GOOD);
				size->width = max(size->width, item_size.width);
				size->height += FONT_HEIGHT_NORMAL * linecount + VSPACING;
				break;
			}
		}
	}

	/** Render a column of formatted average durations */
	void DrawElementTimesColumn(const Rect &r, StringID heading_str, const CachedDecimal *values) const
	{
		int y = r.top;
		DrawString(r.left, r.right, y, heading_str, TC_FROMSTRING, SA_CENTER, true);
		y += FONT_HEIGHT_NORMAL + VSPACING;

		for (PerformanceElement e = PFE_FIRST; e < PFE_MAX; e++) {
			values[e].InsertDParams(0);
			DrawString(r.left, r.right, y, values[e].strid, TC_FROMSTRING, SA_RIGHT);
			y += FONT_HEIGHT_NORMAL;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_FRW_TIMES_NAMES: {
				/* Render a column of titles for performance element names */
				int linecount = PFE_MAX - PFE_FIRST;
				int y = r.top + FONT_HEIGHT_NORMAL + VSPACING; // first line contains headings in the value columns
				for (int i = 0; i < linecount; i++) {
					DrawString(r.left, r.right, y, STR_FRAMERATE_GAMELOOP + i, TC_FROMSTRING, SA_LEFT);
					y += FONT_HEIGHT_NORMAL;
				}
				break;
			}
			case WID_FRW_TIMES_CURRENT:
				/* Render short-term average values */
				DrawElementTimesColumn(r, STR_FRAMERATE_CURRENT, this->times_shortterm);
				break;
			case WID_FRW_TIMES_AVERAGE:
				/* Render averages of all recorded values */
				DrawElementTimesColumn(r, STR_FRAMERATE_AVERAGE, this->times_longterm);
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_FRW_TIMES_NAMES:
			case WID_FRW_TIMES_CURRENT:
			case WID_FRW_TIMES_AVERAGE: {
				/* Open time graph windows when clicking detail measurement lines */
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


/** @hideinitializer */
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
	int vertical_scale;       ///< number of TIMESTAMP_PRECISION units vertically
	int horizontal_scale;     ///< number of half-second units horizontally
	uint32 next_scale_update; ///< realtime tick for next scale update

	PerformanceElement element; ///< what element this window renders graph for
	Dimension graph_size;       ///< size of the main graph area (excluding axis labels)

	FrametimeGraphWindow(WindowDesc *desc, WindowNumber number) : Window(desc)
	{
		this->element = (PerformanceElement)number;
		this->horizontal_scale = 4;
		this->vertical_scale = TIMESTAMP_PRECISION / 10;
		this->next_scale_update = 0;

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

			/* Size graph in height to fit at least 10 vertical labels with space between, or at least 100 pixels */
			graph_size.height = max<uint>(100, 10 * (size_ms_label.height + 1));
			/* Always 2:1 graph area */
			graph_size.width = 2 * graph_size.height;
			*size = graph_size;

			size->width += size_ms_label.width + 2;
			size->height += size_s_label.height + 2;
		}
	}

	void SelectHorizontalScale(TimingMeasurement range)
	{
		/* Determine horizontal scale based on period covered by 60 points
		* (slightly less than 2 seconds at full game speed) */
		struct ScaleDef { TimingMeasurement range; int scale; };
		static const ScaleDef hscales[] = {
			{ 120, 60 },
			{  10, 20 },
			{   5, 10 },
			{   3,  4 },
			{   1,  2 },
		};
		for (const ScaleDef *sc = hscales; sc < hscales + lengthof(hscales); sc++) {
			if (range < sc->range) this->horizontal_scale = sc->scale;
		}
	}

	void SelectVerticalScale(TimingMeasurement range)
	{
		/* Determine vertical scale based on peak value (within the horizontal scale + a bit) */
		static const TimingMeasurement vscales[] = {
			TIMESTAMP_PRECISION * 100,
			TIMESTAMP_PRECISION * 10,
			TIMESTAMP_PRECISION * 5,
			TIMESTAMP_PRECISION,
			TIMESTAMP_PRECISION / 2,
			TIMESTAMP_PRECISION / 5,
			TIMESTAMP_PRECISION / 10,
			TIMESTAMP_PRECISION / 50,
			TIMESTAMP_PRECISION / 200,
		};
		for (const TimingMeasurement *sc = vscales; sc < vscales + lengthof(vscales); sc++) {
			if (range < *sc) this->vertical_scale = (int)*sc;
		}
	}

	/** Recalculate the graph scaling factors based on current recorded data */
	void UpdateScale()
	{
		const TimingMeasurement *durations = _pf_data[this->element].durations;
		const TimingMeasurement *timestamps = _pf_data[this->element].timestamps;
		int num_valid = _pf_data[this->element].num_valid;
		int point = _pf_data[this->element].prev_index;

		TimingMeasurement lastts = timestamps[point];
		TimingMeasurement time_sum = 0;
		TimingMeasurement peak_value = 0;
		int count = 0;

		/* Sensible default for when too few measurements are available */
		this->horizontal_scale = 4;

		for (int i = 1; i < num_valid; i++) {
			point--;
			if (point < 0) point = NUM_FRAMERATE_POINTS - 1;

			TimingMeasurement value = durations[point];
			if (value == PerformanceData::INVALID_DURATION) {
				/* Skip gaps in data by pretending time is continuous across them */
				lastts = timestamps[point];
				continue;
			}
			if (value > peak_value) peak_value = value;
			count++;

			/* Accumulate period of time covered by data */
			time_sum += lastts - timestamps[point];
			lastts = timestamps[point];

			/* Enough data to select a range and get decent data density */
			if (count == 60) this->SelectHorizontalScale(time_sum / TIMESTAMP_PRECISION);

			/* End when enough points have been collected and the horizontal scale has been exceeded */
			if (count >= 60 && time_sum >= (this->horizontal_scale + 2) * TIMESTAMP_PRECISION / 2) break;
		}

		this->SelectVerticalScale(peak_value);
	}

	virtual void OnTick()
	{
		this->SetDirty();

		if (this->next_scale_update < _realtime_tick) {
			this->next_scale_update = _realtime_tick + 500;
			this->UpdateScale();
		}
	}

	/** Scale and interpolate a value from a source range into a destination range */
	template<typename T>
	static inline T Scinterlate(T dst_min, T dst_max, T src_min, T src_max, T value)
	{
		T dst_diff = dst_max - dst_min;
		T src_diff = src_max - src_min;
		return (value - src_min) * dst_diff / src_diff + dst_min;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget == WID_FGW_GRAPH) {
			const TimingMeasurement *durations  = _pf_data[this->element].durations;
			const TimingMeasurement *timestamps = _pf_data[this->element].timestamps;
			int point = _pf_data[this->element].prev_index;

			const int x_zero = r.right - (int)this->graph_size.width;
			const int x_max = r.right;
			const int y_zero = r.top + (int)this->graph_size.height;
			const int y_max = r.top;
			const int c_grid = PC_DARK_GREY;
			const int c_lines = PC_BLACK;
			const int c_peak = PC_DARK_RED;

			const TimingMeasurement draw_horz_scale = (TimingMeasurement)this->horizontal_scale * TIMESTAMP_PRECISION / 2;
			const TimingMeasurement draw_vert_scale = (TimingMeasurement)this->vertical_scale;

			/* Number of \c horizontal_scale units in each horizontal division */
			const uint horz_div_scl = (this->horizontal_scale <= 20) ? 1 : 10;
			/* Number of divisions of the horizontal axis */
			const uint horz_divisions = this->horizontal_scale / horz_div_scl;
			/* Number of divisions of the vertical axis */
			const uint vert_divisions = 10;

			/* Draw division lines and labels for the vertical axis */
			for (uint division = 0; division < vert_divisions; division++) {
				int y = Scinterlate(y_zero, y_max, 0, (int)vert_divisions, (int)division);
				GfxDrawLine(x_zero, y, x_max, y, c_grid);
				if (division % 2 == 0) {
					if ((TimingMeasurement)this->vertical_scale > TIMESTAMP_PRECISION) {
						SetDParam(0, this->vertical_scale * division / 10 / TIMESTAMP_PRECISION);
						DrawString(r.left, x_zero - 2, y - FONT_HEIGHT_SMALL, STR_FRAMERATE_GRAPH_SECONDS, TC_GREY, SA_RIGHT | SA_FORCE, false, FS_SMALL);
					} else {
						SetDParam(0, this->vertical_scale * division / 10 * 1000 / TIMESTAMP_PRECISION);
						DrawString(r.left, x_zero - 2, y - FONT_HEIGHT_SMALL, STR_FRAMERATE_GRAPH_MILLISECONDS, TC_GREY, SA_RIGHT | SA_FORCE, false, FS_SMALL);
					}
				}
			}
			/* Draw divison lines and labels for the horizontal axis */
			for (uint division = horz_divisions; division > 0; division--) {
				int x = Scinterlate(x_zero, x_max, 0, (int)horz_divisions, (int)horz_divisions - (int)division);
				GfxDrawLine(x, y_max, x, y_zero, c_grid);
				if (division % 2 == 0) {
					SetDParam(0, division * horz_div_scl / 2);
					DrawString(x, x_max, y_zero + 2, STR_FRAMERATE_GRAPH_SECONDS, TC_GREY, SA_LEFT | SA_FORCE, false, FS_SMALL);
				}
			}

			/* Position of last rendered data point */
			Point lastpoint = {
				x_max,
				(int)Scinterlate<int64>(y_zero, y_max, 0, this->vertical_scale, durations[point])
			};
			/* Timestamp of last rendered data point */
			TimingMeasurement lastts = timestamps[point];

			TimingMeasurement peak_value = 0;
			Point peak_point = { 0, 0 };
			TimingMeasurement value_sum = 0;
			TimingMeasurement time_sum = 0;
			int points_drawn = 0;

			for (int i = 1; i < NUM_FRAMERATE_POINTS; i++) {
				point--;
				if (point < 0) point = NUM_FRAMERATE_POINTS - 1;

				TimingMeasurement value = durations[point];
				if (value == PerformanceData::INVALID_DURATION) {
					/* Skip gaps in measurements, pretend the data points on each side are continuous */
					lastts = timestamps[point];
					continue;
				}

				/* Use total time period covered for value along horizontal axis */
				time_sum += lastts - timestamps[point];
				lastts = timestamps[point];
				/* Stop if past the width of the graph */
				if (time_sum > draw_horz_scale) break;

				/* Draw line from previous point to new point */
				Point newpoint = {
					(int)Scinterlate<int64>(x_zero, x_max, 0, (int64)draw_horz_scale, (int64)draw_horz_scale - (int64)time_sum),
					(int)Scinterlate<int64>(y_zero, y_max, 0, (int64)draw_vert_scale, (int64)value)
				};
				assert(newpoint.x <= lastpoint.x);
				GfxDrawLine(lastpoint.x, lastpoint.y, newpoint.x, newpoint.y, c_lines);
				lastpoint = newpoint;

				/* Record peak and average value across graphed data */
				value_sum += value;
				points_drawn++;
				if (value > peak_value) {
					peak_value = value;
					peak_point = newpoint;
				}
			}

			/* If the peak value is significantly larger than the average, mark and label it */
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



/** Open the general framerate window */
void ShowFramerateWindow()
{
	AllocateWindowDescFront<FramerateWindow>(&_framerate_display_desc, 0);
}

/** Open a graph window for a performance element */
void ShowFrametimeGraphWindow(PerformanceElement elem)
{
	if (elem < PFE_FIRST || elem >= PFE_MAX) return; // maybe warn?
	AllocateWindowDescFront<FrametimeGraphWindow>(&_frametime_graph_window_desc, elem, true);
}

/** Print performance statistics to game console */
void ConPrintFramerate()
{
	const int count1 = NUM_FRAMERATE_POINTS / 8;
	const int count2 = NUM_FRAMERATE_POINTS / 4;
	const int count3 = NUM_FRAMERATE_POINTS / 1;

	IConsolePrintF(TC_SILVER, "Based on num. data points: %d %d %d", count1, count2, count3);

	static const char *MEASUREMENT_NAMES[PFE_MAX] = {
		"Game loop",
		"  GL station ticks",
		"  GL train ticks",
		"  GL road vehicle ticks",
		"  GL ship ticks",
		"  GL aircraft ticks",
		"  GL landscape ticks",
		"  GL link graph delays",
		"Drawing",
		"  Viewport drawing",
		"Video output",
		"Sound mixing",
	};

	static const PerformanceElement rate_elements[] = { PFE_GAMELOOP, PFE_DRAWING, PFE_VIDEO };

	bool printed_anything = false;

	for (const PerformanceElement *e = rate_elements; e < rate_elements + lengthof(rate_elements); e++) {
		auto &pf = _pf_data[*e];
		if (pf.num_valid == 0) continue;
		IConsolePrintF(TC_GREEN, "%s rate: %.2ffps  (expected: %.2ffps)",
			MEASUREMENT_NAMES[*e],
			pf.GetRate(),
			pf.expected_rate);
		printed_anything = true;
	}

	for (PerformanceElement e = PFE_FIRST; e < PFE_MAX; e++) {
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
