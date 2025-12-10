/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file timetable_cmd.h Command definitions related to timetables. */

#ifndef TIMETABLE_CMD_H
#define TIMETABLE_CMD_H

#include "command_type.h"
#include "order_type.h"
#include "timer/timer_game_tick.h"
#include "vehicle_type.h"

CommandCost CmdChangeTimetable(DoCommandFlags flags, VehicleID veh, VehicleOrderID order_number, ModifyTimetableFlags mtf, uint16_t data);
CommandCost CmdBulkChangeTimetable(DoCommandFlags flags, VehicleID veh, ModifyTimetableFlags mtf, uint16_t data);
CommandCost CmdSetVehicleOnTime(DoCommandFlags flags, VehicleID veh, bool apply_to_group);
CommandCost CmdAutofillTimetable(DoCommandFlags flags, VehicleID veh, bool autofill, bool preserve_wait_time);
CommandCost CmdSetTimetableStart(DoCommandFlags flags, VehicleID veh_id, bool timetable_all, TimerGameTick::TickCounter start_tick);

DEF_CMD_TRAIT(CMD_CHANGE_TIMETABLE,      CmdChangeTimetable,     {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(CMD_BULK_CHANGE_TIMETABLE, CmdBulkChangeTimetable, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(CMD_SET_VEHICLE_ON_TIME,   CmdSetVehicleOnTime,    {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(CMD_AUTOFILL_TIMETABLE,    CmdAutofillTimetable,   {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(CMD_SET_TIMETABLE_START,   CmdSetTimetableStart,   {}, CommandType::RouteManagement)

#endif /* TIMETABLE_CMD_H */
