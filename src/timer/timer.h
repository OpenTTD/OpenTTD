/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer.h Definition of Interval and OneShot timers */

#ifndef TIMER_H
#define TIMER_H

#include "timer_manager.h"


/**
 * The base where every other type of timer is derived from.
 *
 * Never use this class directly yourself.
 */
template <typename TTimerType>
class BaseTimer {
public:
	using TPeriod = typename TTimerType::TPeriod;
	using TElapsed = typename TTimerType::TElapsed;
	using TStorage = typename TTimerType::TStorage;

	/**
	 * Create a new timer.
	 *
	 * @param period The period of the timer.
	 */
	NODISCARD BaseTimer(const TPeriod period) :
		period(period)
	{
		TimerManager<TTimerType>::RegisterTimer(*this);
	}

	/**
	 * Delete the timer.
	 */
	virtual ~BaseTimer()
	{
		TimerManager<TTimerType>::UnregisterTimer(*this);
	}

	/* Although these variables are public, they are only public to make saveload easier; not for common use. */

	TPeriod period; ///< The period of the timer.
	TStorage storage = {}; ///< The storage of the timer.

protected:
	/**
	 * Called by the timer manager to notify the timer that the given amount of time has elapsed.
	 *
	 * @param delta Depending on the time type, this is either in milliseconds or in ticks.
	 */
	virtual void Elapsed(TElapsed delta) = 0;

	/* To ensure only TimerManager can access Elapsed. */
	friend class TimerManager<TTimerType>;
};

/**
 * An interval timer will fire every interval, and will continue to fire until it is deleted.
 *
 * The callback receives how many times the timer has fired since the last time it fired.
 * It will always try to fire every interval, but in times of severe stress it might be late.
 *
 * Each Timer-type needs to implement the Elapsed() method, and call the callback if needed.
 *
 * Setting the period to zero disables the interval. It can be reenabled at any time by
 * calling SetInterval() with a non-zero period.
 */
template <typename TTimerType>
class IntervalTimer : public BaseTimer<TTimerType> {
public:
	using TPeriod = typename TTimerType::TPeriod;
	using TElapsed = typename TTimerType::TElapsed;

	/**
	 * Create a new interval timer.
	 *
	 * @param interval The interval between each callback.
	 * @param callback The callback to call when the interval has passed.
	 */
	NODISCARD IntervalTimer(const TPeriod interval, std::function<void(uint)> callback) :
		BaseTimer<TTimerType>(interval),
		callback(callback)
	{
	}

	/**
	 * Set a new interval for the timer.
	 *
	 * @param interval The interval between each callback.
	 * @param reset Whether to reset the timer to zero.
	 */
	void SetInterval(const TPeriod interval, bool reset = true)
	{
		this->period = interval;
		if (reset) this->storage = {};
	}

private:
	std::function<void(uint)> callback;

	void Elapsed(TElapsed count) override;
};

/**
 * A timeout timer will fire once after the interval. You can reset it to fire again.
 * The timer will never fire before the interval has passed, but in times of severe stress it might be late.
 */
template <typename TTimerType>
class TimeoutTimer : public BaseTimer<TTimerType> {
public:
	using TPeriod = typename TTimerType::TPeriod;
	using TElapsed = typename TTimerType::TElapsed;

	/**
	 * Create a new timeout timer.
	 *
	 * By default the timeout starts aborted; you will have to call Reset() before it starts.
	 *
	 * @param timeout The timeout after which the timer will fire.
	 * @param callback The callback to call when the timeout has passed.
	 * @param start Whether to start the timer immediately. If false, you can call Reset() to start it.
	 */
	NODISCARD TimeoutTimer(const TPeriod timeout, std::function<void()> callback, bool start = false) :
		BaseTimer<TTimerType>(timeout),
		fired(!start),
		callback(callback)
	{
	}

	/**
	 * Reset the timer, so it will fire again after the timeout.
	 */
	void Reset()
	{
		this->fired = false;
		this->storage = {};
	}

	/**
	 * Reset the timer, so it will fire again after the timeout.
	 *
	 * @param timeout Set a new timeout for the next trigger.
	 */
	void Reset(const TPeriod timeout)
	{
		this->period = timeout;
		this->fired = false;
		this->storage = {};
	}

	/**
	 * Abort the timer so it doesn't fire if it hasn't yet.
	 */
	void Abort()
	{
		this->fired = true;
	}

	/**
	 * Check whether the timeout occurred.
	 *
	 * @return True iff the timeout occurred.
	 */
	bool HasFired() const
	{
		return this->fired;
	}

	/* Although these variables are public, they are only public to make saveload easier; not for common use. */

	bool fired; ///< Whether the timeout has occurred.

private:
	std::function<void()> callback;

	void Elapsed(TElapsed count) override;
};

#endif /* TIMER_H */
