/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_cmd.h Command definitions related to timetables. */

#ifndef TIMETABLE_CMD_H
#define TIMETABLE_CMD_H

#include "command_type.h"
#include "timer/timer_game_tick.h"

CommandCost CmdChangeTimetable(DoCommandFlag flags, VehicleID veh, VehicleOrderID order_number, ModifyTimetableFlags mtf, uint16_t data);
CommandCost CmdBulkChangeTimetable(DoCommandFlag flags, VehicleID veh, ModifyTimetableFlags mtf, uint16_t data);
CommandCost CmdSetVehicleOnTime(DoCommandFlag flags, VehicleID veh, bool apply_to_group);
CommandCost CmdAutofillTimetable(DoCommandFlag flags, VehicleID veh, bool autofill, bool preserve_wait_time);
CommandCost CmdSetTimetableStart(DoCommandFlag flags, VehicleID veh_id, bool timetable_all, TimerGameTick::TickCounter start_tick);

DEF_CMD_TRAIT(CMD_CHANGE_TIMETABLE,      CmdChangeTimetable,     0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_BULK_CHANGE_TIMETABLE, CmdBulkChangeTimetable, 0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_VEHICLE_ON_TIME,   CmdSetVehicleOnTime,    0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_AUTOFILL_TIMETABLE,    CmdAutofillTimetable,   0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_TIMETABLE_START,   CmdSetTimetableStart,   0, CMDT_ROUTE_MANAGEMENT)

#endif /* TIMETABLE_CMD_H */
