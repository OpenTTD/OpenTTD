/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file history.cpp Implementation of functions for storing historical data. */

#include "../stdafx.h"

#include "../core/bitmath_func.hpp"
#include "history_type.hpp"
#include "history_func.hpp"

#include "../safeguards.h"

/**
 * Update mask of valid records for a historical data.
 * @note Call only for the largest history range sub-division.
 * @param[in,out] valid_history Valid history records.
 * @param hr History range to update mask for.
 * @param cur_month Current economy month.
 */
void UpdateValidHistory(ValidHistoryMask &valid_history, const HistoryRange &hr, uint cur_month)
{
	/* Update for subdivisions first. */
	if (hr.hr != nullptr) UpdateValidHistory(valid_history, *hr.hr, cur_month);

	/* No need to update if our last entry is marked valid. */
	if (HasBit(valid_history, hr.last - 1)) return;
	/* Is it the right time for this history range? */
	if (cur_month % hr.total_division != 0) return;
	/* Is the previous history range valid yet? */
	if (hr.division != 1 && !HasBit(valid_history, hr.first - hr.division)) return;

	SB(valid_history, hr.first, hr.records, GB(valid_history, hr.first, hr.records) << 1ULL | 1ULL);
}

/**
 * Test if history data is valid, without extracting data.
 * @param valid_history Mask of valid history records.
 * @param hr History range to test.
 * @param age Age of data to test.
 * @return True iff the data for history range and age is valid.
 */
bool IsValidHistory(ValidHistoryMask valid_history, const HistoryRange &hr, uint age)
{
	if (hr.hr == nullptr) {
		if (age < hr.periods) {
			uint slot = hr.first + age;
			return HasBit(valid_history, slot);
		}
	} else {
		if (age * hr.division < static_cast<uint>(hr.hr->periods - hr.division)) {
			uint start = age * hr.division + ((TimerGameEconomy::month / hr.hr->division) % hr.division);
			return IsValidHistory(valid_history, *hr.hr, start);
		}
		if (age < hr.periods) {
			uint slot = hr.first + age - ((hr.hr->periods / hr.division) - 1);
			return HasBit(valid_history, slot);
		}
	}
	return false;
}
