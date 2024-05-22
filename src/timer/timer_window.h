/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_window.h Definition of the Window system */

#ifndef TIMER_WINDOW_H
#define TIMER_WINDOW_H

#include <chrono>

/**
 * Timer that represents the real time, usable for the Window system.
 *
 * This can be used to create intervals based on milliseconds, seconds, etc.
 * Mostly used for animation, scrolling, etc.
 *
 * Please be mindful that the order in which timers are called is not guaranteed.
 *
 * @note The lowest possible interval is 1ms.
 * @note These timers can only be used in the Window system.
 */
class TimerWindow {
public:
	using TPeriod = std::chrono::milliseconds;
	using TElapsed = std::chrono::milliseconds;
	struct TStorage {
		std::chrono::milliseconds elapsed;
	};
};

#endif /* TIMER_WINDOW_H */
