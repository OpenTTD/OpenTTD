/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file history_func.hpp Functions for storing historical data. */

#ifndef HISTORY_FUNC_HPP
#define HISTORY_FUNC_HPP

#include "history_type.hpp"

/**
 * Rotate history.
 * @tparam T type of history data element.
 * @param history Historical data to rotate.
 */
template <typename T>
void RotateHistory(HistoryData<T> &history)
{
	std::rotate(std::rbegin(history), std::rbegin(history) + 1, std::rend(history));
	history[THIS_MONTH] = {};
}

/**
 * Fill some data with historical data.
 * @param history Historical data to fill from.
 * @param fillers Fillers to fill with history data.
 */
template <uint N, typename T, typename... Tfillers>
void FillFromHistory(const HistoryData<T> &history, Tfillers... fillers)
{
	for (uint i = 0; i != N; ++i) {
		auto &data = history[N - i];
		(fillers.Fill(i, data), ...);
	}
}

#endif /* HISTORY_FUNC_HPP */
