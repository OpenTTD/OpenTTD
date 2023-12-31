/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_func.h Functions related to engines. */

#ifndef ENGINE_FUNC_H
#define ENGINE_FUNC_H

#include "engine_type.h"
#include "vehicle_type.h"
#include "company_type.h"
#include "timer/timer_game_calendar.h"

void SetupEngines();
void StartupEngines();
void CheckEngines();

/* Original engine data counts and offsets */
extern const uint8_t _engine_counts[4];
extern const uint8_t _engine_offsets[4];

bool IsEngineBuildable(EngineID engine, VehicleType type, CompanyID company);
bool IsEngineRefittable(EngineID engine);
void SetYearEngineAgingStops();
void CalcEngineReliability(Engine *e, bool new_month);
void StartupOneEngine(Engine *e, const TimerGameCalendar::YearMonthDay &aging_ymd, uint32_t seed);

uint GetTotalCapacityOfArticulatedParts(EngineID engine);

#endif /* ENGINE_FUNC_H */
