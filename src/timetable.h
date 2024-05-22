/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable.h Functions related to time tabling. */

#ifndef TIMETABLE_H
#define TIMETABLE_H

#include "timer/timer_game_tick.h"
#include "timer/timer_game_economy.h"
#include "vehicle_type.h"

static const TimerGameEconomy::Year MAX_TIMETABLE_START_YEARS = 15; ///< The maximum start date offset, in economy years.

enum class TimetableMode : uint8_t {
	Days,
	Seconds,
	Ticks,
};

TimerGameTick::TickCounter GetStartTickFromDate(TimerGameEconomy::Date start_date);
TimerGameEconomy::Date GetDateFromStartTick(TimerGameTick::TickCounter start_tick);

void ShowTimetableWindow(const Vehicle *v);
void UpdateVehicleTimetable(Vehicle *v, bool travelling);
void SetTimetableParams(int param1, int param2, TimerGameTick::Ticks ticks);

#endif /* TIMETABLE_H */
