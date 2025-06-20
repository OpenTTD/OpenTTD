/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file history_func.hpp Functions for storing historical data. */

#ifndef HISTORY_FUNC_HPP
#define HISTORY_FUNC_HPP

#include "../core/math_func.hpp"

#include "../timer/timer_game_economy.h"
#include "history_type.hpp"

/**
 * Sum history data between first and last elements.
 * @note The summation should prevent overflowing, and perform transformations relevant to the type of data.
 * @tparam T type of history data element.
 * @param first First element to sum.
 * @param last Last element to sum.
 * @return Sum of history elements.
 */
template <typename T>
T SumHistory(typename std::span<T> first);

/**
 * Rotate history.
 * @tparam T type of history data element.
 * @param history Historical data to rotate.
 */
template <typename T>
void RotateHistory(HistoryData<T> &history, const HistoryRange &hr, uint age)
{
	if (age % hr.total_division != 0) return;

	std::move_backward(std::next(std::begin(history), hr.first), std::next(std::begin(history), hr.last - 1), std::next(std::begin(history), hr.last));

	if (hr.division == 1) {
		history[hr.first] = history[hr.first - 1];
	} else {
		auto first = std::next(std::begin(history), hr.first - hr.division);
		auto last = std::next(first, hr.division);
		history[hr.first] = SumHistory<T>(std::span{first, last});
	}
}

template <typename T>
void RotateHistory(HistoryData<T> &history, uint age)
{
	RotateHistory(history, HISTORY_MONTH, age);
	RotateHistory(history, HISTORY_QUARTER, age);
	RotateHistory(history, HISTORY_YEAR, age);
	history.front() = {};
}

/**
 * Get an average value for the previous month, as reset for the next month.
 * @param total Accrued total to average. Will be reset to zero.
 * @return Average value for the month.
 */
template <typename T, typename Taccrued>
T GetAndResetAccumulatedAverage(Taccrued &total)
{
	T result = ClampTo<T>(total / std::max(1U, TimerGameEconomy::days_since_last_month));
	total = 0;
	return result;
}

template <typename T>
T GetHistory(const HistoryData<T> &history, const HistoryRange &hr, uint age)
{
	if (hr.hr == nullptr) {
		if (age < hr.periods) return history[hr.first + age];
	} else {
		if (age * hr.division < static_cast<uint>(hr.hr->periods - hr.division)) {
			std::array<T, HISTORY_MAX_DIVISION> result; // No need to clear as we fill every element we use.
			uint start = age * hr.division + ((TimerGameEconomy::month / hr.hr->division) % hr.division);
			for (auto i = start; i != start + hr.division; ++i) {
				result[i - start] = GetHistory(history, *hr.hr, i);
			}
			return SumHistory<T>(std::span{std::begin(result), hr.division});
		}
		if (age < hr.periods) return history[hr.first + age - ((hr.hr->periods / hr.division) - 1)];
	}
	NOT_REACHED();
}

/**
 * Get history data for the specified period and age within that period.
 * @param history History data to extract from.
 * @param period Period to get.
 * @param age Age of data to get.
 * @return Historical value for period and age.
 */
template <typename T>
T GetHistory(const HistoryData<T> &history, uint period, uint age)
{
	switch (period) {
		case 0: return GetHistory(history, HISTORY_MONTH, age);
		case 1: return GetHistory(history, HISTORY_QUARTER, age);
		case 2: return GetHistory(history, HISTORY_YEAR, age);
		default: NOT_REACHED();
	}
}

/**
 * Fill some data with historical data.
 * @param history Historical data to fill from.
 * @param fillers Fillers to fill with history data.
 */
template <uint N, typename T, typename... Tfillers>
void FillFromHistory(const HistoryData<T> &history, uint period, Tfillers... fillers)
{
	for (uint i = 0; i != N; ++i) {
		auto data = GetHistory(history, period, N - i - 1);
		(fillers.Fill(i, data), ...);
	}
}

#endif /* HISTORY_FUNC_HPP */
