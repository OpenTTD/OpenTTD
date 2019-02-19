/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file guitimer_func.h GUI Timers. */

#ifndef GUITIMER_FUNC_H
#define GUITIMER_FUNC_H

class GUITimer
{
protected:
	uint timer;
	uint interval;

public:
	GUITimer() : timer(0), interval(0) { }
	explicit GUITimer(uint interval) : timer(0), interval(interval) { }

	inline bool HasElapsed() const
	{
		return this->interval == 0;
	}

	inline void SetInterval(uint interval)
	{
		this->timer = 0;
		this->interval = interval;
	}

	/**
	 * Count how many times the interval has elapsed.
	 * Use to ensure a specific amount of events happen within a timeframe, e.g. for animation.
	 * @param delta Time since last test.
	 * @return Number of times the interval has elapsed.
	 */
	inline uint CountElapsed(uint delta)
	{
		if (this->interval == 0) return 0;
		uint count = delta / this->interval;
		if (this->timer + (delta % this->interval) >= this->interval) count++;
		this->timer = (this->timer + delta) % this->interval;
		return count;
	}

	/**
	 * Test if a timer has elapsed.
	 * Use to ensure an event happens only once within a timeframe, e.g. for window updates.
	 * @param delta Time since last test.
	 * @return True iff the timer has elapsed.
	 */
	inline bool Elapsed(uint delta)
	{
		if (this->CountElapsed(delta) == 0) return false;
		this->SetInterval(0);
		return true;
	}
};

#endif /* GUITIMER_FUNC_H */
