/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_game_realtime.h Definition of the real time game-timer */

#ifndef TIMER_GAME_REALTIME_H
#define TIMER_GAME_REALTIME_H

#include <chrono>

/**
 * Timer that represents real time for game-related purposes.
 *
 * For pausing, there are several modes:
 * - Continue to tick during pause (PeriodFlags::ALWAYS).
 * - Stop ticking when paused (PeriodFlags::UNPAUSED).
 * - Only tick when unpaused or when there was a Command executed recently (recently: since last autosave) (PeriodFlags::AUTOSAVE).
 *
 * @note The lowest possible interval is 1ms, although realistic the lowest
 * interval is 27ms. This timer is only updated when the game-thread makes
 * a tick, which happens every 27ms.
 * @note Callbacks are executed in the game-thread.
 */
class TimerGameRealtime {
public:
	enum PeriodFlags {
		ALWAYS, ///< Always run, even when paused.
		UNPAUSED, ///< Only run when not paused.
		AUTOSAVE, ///< Only run when not paused or there was a Command executed recently.
	};

	struct TPeriod {
		std::chrono::milliseconds period;
		PeriodFlags flag;

		TPeriod(std::chrono::milliseconds period, PeriodFlags flag) : period(period), flag(flag) {}

		bool operator < (const TPeriod &other) const
		{
			if (this->flag != other.flag) return this->flag < other.flag;
			return this->period < other.period;
		}

		bool operator == (const TPeriod &other) const
		{
			return this->flag == other.flag && this->period == other.period;
		}
	};
	using TElapsed = std::chrono::milliseconds;
	struct TStorage {
		std::chrono::milliseconds elapsed;
	};
};

#endif /* TIMER_GAME_REALTIME_H */
