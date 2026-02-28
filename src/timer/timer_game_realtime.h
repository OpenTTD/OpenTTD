/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file timer_game_realtime.h Definition of the real time game-timer. */

#ifndef TIMER_GAME_REALTIME_H
#define TIMER_GAME_REALTIME_H

#include <chrono>

/**
 * Timer that represents real time for game-related purposes.
 *
 * For pausing, there are several modes:
 * - Continue to tick during pause (Trigger::Always).
 * - Stop ticking when paused (Trigger::Unpaused).
 * - Only tick when unpaused or when there was a Command executed recently (recently: since last autosave) (Trigger::Autosave).
 *
 * @note The lowest possible interval is 1ms, although realistic the lowest
 * interval is 27ms. This timer is only updated when the game-thread makes
 * a tick, which happens every 27ms.
 * @note Callbacks are executed in the game-thread.
 */
class TimerGameRealtime {
public:
	/** When is the timer supposed to be run. */
	enum class Trigger : uint8_t {
		Always, ///< Always run, even when paused.
		Unpaused, ///< Only run when not paused.
		Autosave, ///< Only run when not paused or there was a Command executed recently.
	};

	struct TPeriod {
		std::chrono::milliseconds period;
		Trigger trigger;

		TPeriod(std::chrono::milliseconds period, Trigger trigger) : period(period), trigger(trigger) {}

		bool operator < (const TPeriod &other) const
		{
			if (this->trigger != other.trigger) return this->trigger < other.trigger;
			return this->period < other.period;
		}

		bool operator == (const TPeriod &other) const
		{
			return this->trigger == other.trigger && this->period == other.period;
		}
	};
	using TElapsed = std::chrono::milliseconds;
	struct TStorage {
		std::chrono::milliseconds elapsed;
	};
};

#endif /* TIMER_GAME_REALTIME_H */
