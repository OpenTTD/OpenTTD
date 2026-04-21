/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file timer.h Definition of Interval and OneShot timers. */

#ifndef TIMER_H
#define TIMER_H

#include "timer_manager.h"


/**
 * The base where every other type of timer is derived from.
 *
 * Never use this class directly yourself.
 */
template <typename Ttimer_type>
class BaseTimer {
public:
	using Tperiod = typename Ttimer_type::Tperiod;
	using Telapsed = typename Ttimer_type::Telapsed;
	using TStorage = typename Ttimer_type::TStorage;

	/**
	 * Create a new timer.
	 *
	 * @param period The period of the timer.
	 */
	[[nodiscard]] BaseTimer(const Tperiod period) :
		period(period)
	{
		TimerManager<Ttimer_type>::RegisterTimer(*this);
	}

	/**
	 * Unregister this timer.
	 */
	virtual ~BaseTimer()
	{
		TimerManager<Ttimer_type>::UnregisterTimer(*this);
	}

	/* Although these variables are public, they are only public to make saveload easier; not for common use. */

	Tperiod period; ///< The period of the timer.
	TStorage storage = {}; ///< The storage of the timer.

protected:
	/**
	 * Called by the timer manager to notify the timer that the given amount of time has elapsed.
	 *
	 * @param delta Depending on the time type, this is either in milliseconds or in ticks.
	 */
	virtual void Elapsed(Telapsed delta) = 0;

	/* To ensure only TimerManager can access Elapsed. */
	friend class TimerManager<Ttimer_type>;
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
template <typename Ttimer_type>
class IntervalTimer : public BaseTimer<Ttimer_type> {
public:
	using Tperiod = typename Ttimer_type::Tperiod;
	using Telapsed = typename Ttimer_type::Telapsed;

	/**
	 * Create a new interval timer.
	 *
	 * @param interval The interval between each callback.
	 * @param callback The callback to call when the interval has passed.
	 */
	[[nodiscard]] IntervalTimer(const Tperiod interval, std::function<void(uint)> callback) :
		BaseTimer<Ttimer_type>(interval),
		callback(std::move(callback))
	{
	}

	/**
	 * Set a new interval for the timer.
	 *
	 * @param interval The interval between each callback.
	 * @param reset Whether to reset the timer to zero.
	 */
	void SetInterval(const Tperiod interval, bool reset = true)
	{
		TimerManager<Ttimer_type>::ChangeRegisteredTimerPeriod(*this, interval);
		if (reset) this->storage = {};
	}

private:
	std::function<void(uint)> callback;

	void Elapsed(Telapsed count) override;
};

/**
 * A timeout timer will fire once after the interval. You can reset it to fire again.
 * The timer will never fire before the interval has passed, but in times of severe stress it might be late.
 */
template <typename Ttimer_type>
class TimeoutTimer : public BaseTimer<Ttimer_type> {
public:
	using Tperiod = typename Ttimer_type::Tperiod;
	using Telapsed = typename Ttimer_type::Telapsed;

	/**
	 * Create a new timeout timer.
	 *
	 * By default the timeout starts aborted; you will have to call Reset() before it starts.
	 *
	 * @param timeout The timeout after which the timer will fire.
	 * @param callback The callback to call when the timeout has passed.
	 * @param start Whether to start the timer immediately. If false, you can call Reset() to start it.
	 */
	[[nodiscard]] TimeoutTimer(const Tperiod timeout, std::function<void()> callback, bool start = false) :
		BaseTimer<Ttimer_type>(timeout),
		fired(!start),
		callback(std::move(callback))
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
	void Reset(const Tperiod timeout)
	{
		TimerManager<Ttimer_type>::ChangeRegisteredTimerPeriod(*this, timeout);
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

	void Elapsed(Telapsed count) override;
};

#endif /* TIMER_H */
