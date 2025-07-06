/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file history_type.hpp Types for storing historical data. */

#ifndef HISTORY_TYPE_HPP
#define HISTORY_TYPE_HPP

static constexpr uint8_t HISTORY_RECORDS = 25;

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
