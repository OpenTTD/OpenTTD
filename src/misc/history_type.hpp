/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file history_type.hpp Types for storing historical data. */

#ifndef HISTORY_TYPE_HPP
#define HISTORY_TYPE_HPP

struct HistoryRange {
	const HistoryRange *hr;
	const uint8_t periods; ///< Number of periods for this range.
	const uint8_t records; ///< Number of records needed for this range.
	const uint8_t first; ///< Index of first element in history data.
	const uint8_t last; ///< Index of last element in history data.
	const uint8_t division; ///< Number of divisions of the previous history range.
	const uint8_t total_division; ///< Number of divisions of the initial history range.

	explicit constexpr HistoryRange(uint8_t periods) :
		hr(nullptr), periods(periods), records(this->periods), first(1), last(this->first + this->records), division(1), total_division(1)
	{
	}

	constexpr HistoryRange(const HistoryRange &hr, uint8_t division, uint8_t periods) :
		hr(&hr), periods(periods), records(this->periods - ((hr.periods / division) - 1)), first(hr.last), last(this->first + this->records),
		division(division), total_division(division * hr.total_division)
	{
	}
};

static constexpr uint8_t HISTORY_PERIODS = 24;
static constexpr HistoryRange HISTORY_MONTH{HISTORY_PERIODS};
static constexpr HistoryRange HISTORY_QUARTER{HISTORY_MONTH, 3, HISTORY_PERIODS};
static constexpr HistoryRange HISTORY_YEAR{HISTORY_QUARTER, 4, HISTORY_PERIODS};

/** Maximum number of divisions from previous history range. */
static constexpr uint8_t HISTORY_MAX_DIVISION = std::max({HISTORY_MONTH.division, HISTORY_QUARTER.division, HISTORY_YEAR.division});

/** Total number of records require for all history data. */
static constexpr uint8_t HISTORY_RECORDS = HISTORY_YEAR.last;

static constexpr uint8_t THIS_MONTH = 0;
static constexpr uint8_t LAST_MONTH = 1;

/**
 * Container type for storing history data.
 * @tparam T type of history data.
 */
template <typename T>
using HistoryData = std::array<T, HISTORY_RECORDS>;

/** Mask of valid history records. */
using ValidHistoryMask = uint64_t;

#endif /* HISTORY_TYPE_HPP */
