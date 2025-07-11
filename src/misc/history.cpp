/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file history.cpp Implementation of functions for storing historical data. */

#ifndef HISTORY_CPP
#define HISTORY_CPP

#include "../stdafx.h"

#include "../core/bitmath_func.hpp"
#include "history_type.hpp"
#include "history_func.hpp"

#include "../safeguards.h"

static void UpdateValidHistory(ValidHistoryMask &valid_history, const HistoryRange &hr, uint cur_month)
{
	if (cur_month % hr.total_division != 0) return;
	if (hr.division != 1 && !HasBit(valid_history, hr.first - hr.division)) return;

	SB(valid_history, hr.first, hr.records, GB(valid_history, hr.first, hr.records) << 1ULL | 1ULL);
}

/**
 * Update mask of valid records.
 * @param[in,out] valid_history Valid history records.
 * @param age Current economy month.
 */
void UpdateValidHistory(ValidHistoryMask &valid_history, uint cur_month)
{
	UpdateValidHistory(valid_history, HISTORY_MONTH, cur_month);
	UpdateValidHistory(valid_history, HISTORY_QUARTER, cur_month);
	UpdateValidHistory(valid_history, HISTORY_YEAR, cur_month);
}

static bool IsValidHistory(ValidHistoryMask valid_history, const HistoryRange &hr, uint age)
{
	if (hr.hr == nullptr) {
		if (age < hr.periods) {
			uint slot = hr.first + age;
			return HasBit(valid_history, slot);
		}
	} else {
		if (age * hr.division < static_cast<uint>(hr.hr->periods - hr.division)) {
			uint start = age * hr.division + ((TimerGameEconomy::month / hr.hr->division) % hr.division);
			return IsValidHistory(valid_history, *hr.hr, start/* + hr.division - 1*/);
		}
		if (age < hr.periods) {
			uint slot = hr.first + age - ((hr.hr->periods / hr.division) - 1);
			return HasBit(valid_history, slot);
		}
	}
	return false;
}

bool IsValidHistory(ValidHistoryMask valid_history, uint period, uint age)
{
	switch (period) {
		case 0: return IsValidHistory(valid_history, HISTORY_MONTH, age);
		case 1: return IsValidHistory(valid_history, HISTORY_QUARTER, age);
		case 2: return IsValidHistory(valid_history, HISTORY_YEAR, age);
		default: NOT_REACHED();
	}
}

#endif /* HISTORY_CPP */
