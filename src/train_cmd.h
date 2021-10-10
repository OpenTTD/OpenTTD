/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file train_cmd.h Command definitions related to trains. */

#ifndef TRAIN_CMD_H
#define TRAIN_CMD_H

#include "command_type.h"
#include "engine_type.h"
#include "vehicle_type.h"

CommandCost CmdBuildRailVehicle(DoCommandFlag flags, TileIndex tile, const Engine *e, uint16 data, Vehicle **v);
CommandCost CmdSellRailWagon(DoCommandFlag flags, Vehicle *v, uint16 data, uint32 user);

CommandProc CmdMoveRailVehicle;
CommandProc CmdForceTrainProceed;
CommandProc CmdReverseTrainDirection;

DEF_CMD_TRAIT(CMD_MOVE_RAIL_VEHICLE,       CmdMoveRailVehicle,       0, CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_FORCE_TRAIN_PROCEED,     CmdForceTrainProceed,     0, CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REVERSE_TRAIN_DIRECTION, CmdReverseTrainDirection, 0, CMDT_VEHICLE_MANAGEMENT)

#endif /* TRAIN_CMD_H */
