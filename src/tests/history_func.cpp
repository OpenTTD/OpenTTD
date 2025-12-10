/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file history_func.cpp Test functionality for misc/history_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../misc/history_type.hpp"
#include "../misc/history_func.hpp"

#include "../safeguards.h"

template <>
uint16_t SumHistory(std::span<const uint16_t> history)
{
	uint32_t total = std::accumulate(std::begin(history), std::end(history), 0, [](uint32_t r, const uint16_t &value) { return r + value; });
	return ClampTo<uint16_t>(total);
}

/**
 * Helper to get history records and return the value, instead returning its validity.
 * @param history History data to extract from.
 * @param hr History range to get.
 * @param age Age of data to get.
 * @return Historical value for the period and age.
 */
template <typename T>
T GetHistory(const HistoryData<T> &history, const HistoryRange &hr, uint age)
{
	T result;
	GetHistory(history, 0, hr, age, result);
	return result;
}

TEST_CASE("History Rotation and Reporting tests")
{
	HistoryData<uint16_t> history{};
	ValidHistoryMask valid_history = 0;

	/* Fill the history with decreasing data points for 24 years of history. This ensures that no data period should
	 * contain the same value as another period. */
	uint16_t i = 12 * HISTORY_PERIODS;
	for (uint date = 1; date <= 12 * HISTORY_PERIODS; ++date, --i) {
		history[THIS_MONTH] = i;
		UpdateValidHistory(valid_history, HISTORY_YEAR, date % 12);
		RotateHistory(history, valid_history, HISTORY_YEAR, date % 12);
	}

	/* With the decreasing sequence, the expected value is triangle number (x*x+n)/2 and the square of the total divisions.
	 *   for quarters:  1 +  2 +  3 =  6,  4 +  5 +  6 = 15,  7 +  8 +  9 = 24, 10 + 11 + 12 = 33
	 *                 13 + 14 + 15 = 42, 16 + 17 + 18 = 51, 19 + 20 + 21 = 60, 22 + 23 + 24 = 69...
	 *      for years:  6 + 15 + 24 + 33 = 78, 42 + 51 + 60 + 69 = 222...
	 */
	for (uint j = 0; j < HISTORY_PERIODS; ++j) {
		CHECK(GetHistory(history, HISTORY_MONTH,   j) == (( 1 *  1 +  1) / 2) +  1 *  1 * j);
		CHECK(GetHistory(history, HISTORY_QUARTER, j) == (( 3 *  3 +  3) / 2) +  3 *  3 * j);
		CHECK(GetHistory(history, HISTORY_YEAR,    j) == ((12 * 12 + 12) / 2) + 12 * 12 * j);
	}

	/* Double-check quarter history matches summed month history. */
	CHECK(GetHistory(history, HISTORY_MONTH,  0) + GetHistory(history, HISTORY_MONTH,  1) + GetHistory(history, HISTORY_MONTH,  2) == GetHistory(history, HISTORY_QUARTER, 0));
	CHECK(GetHistory(history, HISTORY_MONTH,  3) + GetHistory(history, HISTORY_MONTH,  4) + GetHistory(history, HISTORY_MONTH,  5) == GetHistory(history, HISTORY_QUARTER, 1));
	CHECK(GetHistory(history, HISTORY_MONTH,  6) + GetHistory(history, HISTORY_MONTH,  7) + GetHistory(history, HISTORY_MONTH,  8) == GetHistory(history, HISTORY_QUARTER, 2));
	CHECK(GetHistory(history, HISTORY_MONTH,  9) + GetHistory(history, HISTORY_MONTH, 10) + GetHistory(history, HISTORY_MONTH, 11) == GetHistory(history, HISTORY_QUARTER, 3));
	CHECK(GetHistory(history, HISTORY_MONTH, 12) + GetHistory(history, HISTORY_MONTH, 13) + GetHistory(history, HISTORY_MONTH, 14) == GetHistory(history, HISTORY_QUARTER, 4));
	CHECK(GetHistory(history, HISTORY_MONTH, 15) + GetHistory(history, HISTORY_MONTH, 16) + GetHistory(history, HISTORY_MONTH, 17) == GetHistory(history, HISTORY_QUARTER, 5));
	CHECK(GetHistory(history, HISTORY_MONTH, 18) + GetHistory(history, HISTORY_MONTH, 19) + GetHistory(history, HISTORY_MONTH, 20) == GetHistory(history, HISTORY_QUARTER, 6));
	CHECK(GetHistory(history, HISTORY_MONTH, 21) + GetHistory(history, HISTORY_MONTH, 22) + GetHistory(history, HISTORY_MONTH, 23) == GetHistory(history, HISTORY_QUARTER, 7));

	/* Double-check year history matches summed quarter history. */
	CHECK(GetHistory(history, HISTORY_QUARTER,  0) + GetHistory(history, HISTORY_QUARTER,  1) + GetHistory(history, HISTORY_QUARTER,  2) + GetHistory(history, HISTORY_QUARTER,  3) == GetHistory(history, HISTORY_YEAR, 0));
	CHECK(GetHistory(history, HISTORY_QUARTER,  4) + GetHistory(history, HISTORY_QUARTER,  5) + GetHistory(history, HISTORY_QUARTER,  6) + GetHistory(history, HISTORY_QUARTER,  7) == GetHistory(history, HISTORY_YEAR, 1));
	CHECK(GetHistory(history, HISTORY_QUARTER,  8) + GetHistory(history, HISTORY_QUARTER,  9) + GetHistory(history, HISTORY_QUARTER, 10) + GetHistory(history, HISTORY_QUARTER, 11) == GetHistory(history, HISTORY_YEAR, 2));
	CHECK(GetHistory(history, HISTORY_QUARTER, 12) + GetHistory(history, HISTORY_QUARTER, 13) + GetHistory(history, HISTORY_QUARTER, 14) + GetHistory(history, HISTORY_QUARTER, 15) == GetHistory(history, HISTORY_YEAR, 3));
	CHECK(GetHistory(history, HISTORY_QUARTER, 16) + GetHistory(history, HISTORY_QUARTER, 17) + GetHistory(history, HISTORY_QUARTER, 18) + GetHistory(history, HISTORY_QUARTER, 19) == GetHistory(history, HISTORY_YEAR, 4));
	CHECK(GetHistory(history, HISTORY_QUARTER, 20) + GetHistory(history, HISTORY_QUARTER, 21) + GetHistory(history, HISTORY_QUARTER, 22) + GetHistory(history, HISTORY_QUARTER, 23) == GetHistory(history, HISTORY_YEAR, 5));
}
