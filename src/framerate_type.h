/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file framerate_type.h
 * Types for recording game performance data.
 *
 * @par Adding new measurements
 * Adding a new measurement requires multiple steps, which are outlined here.
 * The first thing to do is add a new member of the #PerformanceElement enum.
 * It must be added before \c PFE_MAX and should be added in a logical place.
 * For example, an element of the game loop would be added next to the other game loop elements, and a rendering element next to the other rendering elements.
 *
 * @par
 * Second is adding a member to the \link anonymous_namespace{framerate_gui.cpp}::_pf_data _pf_data \endlink array, in the same position as the new #PerformanceElement member.
 *
 * @par
 * Third is adding strings for the new element. There is an array in #ConPrintFramerate with strings used for the console command.
 * Additionally, there are two sets of strings in \c english.txt for two GUI uses, also in the #PerformanceElement order.
 * Search for \c STR_FRAMERATE_GAMELOOP and \c STR_FRAMETIME_CAPTION_GAMELOOP in \c english.txt to find those.
 *
 * @par
 * Last is actually adding the measurements. There are two ways to measure, either one-shot (a single function/block handling all processing),
 * or as an accumulated element (multiple functions/blocks that need to be summed across each frame/tick).
 * Use either the PerformanceMeasurer or the PerformanceAccumulator class respectively for the two cases.
 * Either class is used by instantiating an object of it at the beginning of the block to be measured, so it auto-destructs at the end of the block.
 * For PerformanceAccumulator, make sure to also call PerformanceAccumulator::Reset once at the beginning of a new frame. Usually the StateGameLoop function is appropriate for this.
 *
 * @see framerate_gui.cpp for implementation
 */

#ifndef FRAMERATE_TYPE_H
#define FRAMERATE_TYPE_H

#include "stdafx.h"
#include "core/enum_type.hpp"

/**
 * Elements of game performance that can be measured.
 *
 * @note When adding new elements here, make sure to also update all other locations depending on the length and order of this enum.
 * See <em>Adding new measurements</em> above.
 */
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
	PFE_ALLSCRIPTS,    ///< Sum of all GS/AI scripts
	PFE_GAMESCRIPT,    ///< Game script execution
	PFE_AI0,           ///< AI execution for player slot 1
	PFE_AI1,           ///< AI execution for player slot 2
	PFE_AI2,           ///< AI execution for player slot 3
	PFE_AI3,           ///< AI execution for player slot 4
	PFE_AI4,           ///< AI execution for player slot 5
	PFE_AI5,           ///< AI execution for player slot 6
	PFE_AI6,           ///< AI execution for player slot 7
	PFE_AI7,           ///< AI execution for player slot 8
	PFE_AI8,           ///< AI execution for player slot 9
	PFE_AI9,           ///< AI execution for player slot 10
	PFE_AI10,          ///< AI execution for player slot 11
	PFE_AI11,          ///< AI execution for player slot 12
	PFE_AI12,          ///< AI execution for player slot 13
	PFE_AI13,          ///< AI execution for player slot 14
	PFE_AI14,          ///< AI execution for player slot 15
	PFE_MAX,           ///< End of enum, must be last.
};
DECLARE_POSTFIX_INCREMENT(PerformanceElement)

/** Type used to hold a performance timing measurement */
typedef uint64_t TimingMeasurement;

/**
 * RAII class for measuring simple elements of performance.
 * Construct an object with the appropriate element parameter when processing begins,
 * time is automatically taken when the object goes out of scope again.
 *
 * Call Paused at the start of a frame if the processing of this element is paused.
 */
class PerformanceMeasurer {
	PerformanceElement elem;
	TimingMeasurement start_time;
public:
	PerformanceMeasurer(PerformanceElement elem);
	~PerformanceMeasurer();
	void SetExpectedRate(double rate);
	static void SetInactive(PerformanceElement elem);
	static void Paused(PerformanceElement elem);
};

/**
 * RAII class for measuring multi-step elements of performance.
 * At the beginning of a frame, call Reset on the element, then construct an object in the scope where
 * each processing cycle happens. The measurements are summed between resets.
 *
 * Usually StateGameLoop is an appropriate function to place Reset calls in, but for elements with
 * more isolated scopes it can also be appropriate to Reset somewhere else.
 * An example is the CallVehicleTicks function where all the vehicle type elements are reset.
 *
 * The PerformanceMeasurer::Paused function can also be used with elements otherwise measured with this class.
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
void ProcessPendingPerformanceMeasurements();

#endif /* FRAMERATE_TYPE_H */
