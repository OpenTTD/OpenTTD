/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file history_func.cpp Test functionality for misc/history_func. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../misc/history_type.hpp"
#include "../misc/history_func.hpp"

#include "../safeguards.h"

template <>
uint16_t SumHistory(HistoryData<uint16_t>::iterator first, HistoryData<uint16_t>::iterator last)
{
	uint32_t total = std::accumulate(first, last, 0, [](uint32_t r, const uint16_t &value) { return r + value; });
	return ClampTo<uint16_t>(total);
}

TEST_CASE("History Rotation and Reporting tests")
{
	HistoryData<uint16_t> history{};

	/* Fill the history with decreasing data points for 24 years of history. This ensures that no data period should
	 * contain the same value as another period. */
	uint16_t i = 12 * HISTORY_PERIODS;
	for (uint date = 1; date <= 12 * HISTORY_PERIODS; ++date, --i) {
		history[THIS_MONTH] = i;
		RotateHistory(history, date % 12);
	}

	/* With the decreasing sequence, the expected value is triangle number (x*x+n)/2 and the square of the total divisions.
	 *   for quarters:  1 +  2 +  3 =  6,  4 +  5 +  6 = 15,  7 +  8 +  9 = 24, 10 + 11 + 12 = 33
	 *                 13 + 14 + 15 = 42, 16 + 17 + 18 = 51, 19 + 20 + 21 = 60, 22 + 23 + 24 = 69...
	 *      for years:  6 + 15 + 24 + 33 = 78, 42 + 51 + 60 + 69 = 222...
	 */
	for (uint j = 0; j < HISTORY_PERIODS; ++j) {
		CHECK(GetHistory(history, 0, j) == (( 1 *  1 +  1) / 2) +  1 *  1 * j);
		CHECK(GetHistory(history, 1, j) == (( 3 *  3 +  3) / 2) +  3 *  3 * j);
		CHECK(GetHistory(history, 2, j) == ((12 * 12 + 12) / 2) + 12 * 12 * j);
	}

	/* Double-check quarter history matches summed month history. */
	CHECK(GetHistory(history, 0,  0) + GetHistory(history, 0,  1) + GetHistory(history, 0,  2) == GetHistory(history, 1, 0));
	CHECK(GetHistory(history, 0,  3) + GetHistory(history, 0,  4) + GetHistory(history, 0,  5) == GetHistory(history, 1, 1));
	CHECK(GetHistory(history, 0,  6) + GetHistory(history, 0,  7) + GetHistory(history, 0,  8) == GetHistory(history, 1, 2));
	CHECK(GetHistory(history, 0,  9) + GetHistory(history, 0, 10) + GetHistory(history, 0, 11) == GetHistory(history, 1, 3));
	CHECK(GetHistory(history, 0, 12) + GetHistory(history, 0, 13) + GetHistory(history, 0, 14) == GetHistory(history, 1, 4));
	CHECK(GetHistory(history, 0, 15) + GetHistory(history, 0, 16) + GetHistory(history, 0, 17) == GetHistory(history, 1, 5));
	CHECK(GetHistory(history, 0, 18) + GetHistory(history, 0, 19) + GetHistory(history, 0, 20) == GetHistory(history, 1, 6));
	CHECK(GetHistory(history, 0, 21) + GetHistory(history, 0, 22) + GetHistory(history, 0, 23) == GetHistory(history, 1, 7));

	/* Double-check year history matches summed quarter history. */
	CHECK(GetHistory(history, 1,  0) + GetHistory(history, 1,  1) + GetHistory(history, 1,  2) + GetHistory(history, 1,  3) == GetHistory(history, 2, 0));
	CHECK(GetHistory(history, 1,  4) + GetHistory(history, 1,  5) + GetHistory(history, 1,  6) + GetHistory(history, 1,  7) == GetHistory(history, 2, 1));
	CHECK(GetHistory(history, 1,  8) + GetHistory(history, 1,  9) + GetHistory(history, 1, 10) + GetHistory(history, 1, 11) == GetHistory(history, 2, 2));
	CHECK(GetHistory(history, 1, 12) + GetHistory(history, 1, 13) + GetHistory(history, 1, 14) + GetHistory(history, 1, 15) == GetHistory(history, 2, 3));
	CHECK(GetHistory(history, 1, 16) + GetHistory(history, 1, 17) + GetHistory(history, 1, 18) + GetHistory(history, 1, 19) == GetHistory(history, 2, 4));
	CHECK(GetHistory(history, 1, 20) + GetHistory(history, 1, 21) + GetHistory(history, 1, 22) + GetHistory(history, 1, 23) == GetHistory(history, 2, 5));
}
