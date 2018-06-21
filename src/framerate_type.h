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

enum PerformanceElement {
	PFE_GAMELOOP,       ///< Speed of gameloop processing.
	PFE_FIRST = PFE_GAMELOOP,
	PFE_DRAWING,        ///< Speed of drawing world and GUI.
	PFE_VIDEO,          ///< Speed of painting drawn video buffer.
	PFE_SOUND,          ///< Speed of mixing audio samples
	PFE_MAX,            ///< End of enum, must be last.
};

typedef uint64 TimingMeasurement;

/**
 * RAII class for measuring elements of performance.
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
};

void ShowFramerateWindow();

#endif /* FRAMERATE_GUI_H */
