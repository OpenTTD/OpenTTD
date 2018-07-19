/* $Id$ */

/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FRAMERATE_GUI_H
#define FRAMERATE_GUI_H

#include "stdafx.h"
#include "core/enum_type.hpp"

enum PerformanceElement {
	PFE_FIRST = 0,
	PFE_GAMELOOP = 0,  ///< Speed of gameloop processing.
	PFE_GL_ECONOMY,    ///< Time spent processing cargo movement
	PFE_GL_TRAINS,     ///< Time spent processing trains
	PFE_GL_ROADVEHS,   ///< Time spend processing road vehicles
	PFE_GL_SHIPS,      ///< Time spent processing ships
	PFE_GL_AIRCRAFT,   ///< Time spent processing aircraft
	PFE_GL_LANDSCAPE,  ///< Time spent processing other world features
	PFE_GL_LINKGRAPH,  ///< Time spent waiting for link graph background jobs
	PFE_DRAWING,       ///< Speed of drawing world and GUI.
	PFE_DRAWWORLD,     ///< Time spent drawing world viewports in GUI
	PFE_VIDEO,         ///< Speed of painting drawn video buffer.
	PFE_SOUND,         ///< Speed of mixing audio samples
	PFE_MAX,           ///< End of enum, must be last.
};
DECLARE_POSTFIX_INCREMENT(PerformanceElement)

typedef uint64 TimingMeasurement;

/**
 * RAII class for measuring simple elements of performance.
 * Construct an object with the appropriate element parameter when processing begins,
 * time is automatically taken when the object goes out of scope again.
 */
class PerformanceMeasurer {
	PerformanceElement elem;
	TimingMeasurement start_time;
public:
	PerformanceMeasurer(PerformanceElement elem);
	~PerformanceMeasurer();
	void SetExpectedRate(double rate);
	static void Paused(PerformanceElement elem);
};

/**
 * RAII class for measuring multi-step elements of performance.
 * At the beginning of a frame, call Reset on the element, then construct an object in the scope where
 * each processing cycle happens. The measurements are summed between resets.
 */
class PerformanceAccumulator {
	PerformanceElement elem;
	TimingMeasurement start_time;
public:
	PerformanceAccumulator(PerformanceElement elem);
	~PerformanceAccumulator();
	static void Reset(PerformanceElement elem);
};

void ShowFramerateWindow();

#endif /* FRAMERATE_GUI_H */
