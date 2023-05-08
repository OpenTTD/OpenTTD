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

CommandCost CmdBuildRailVehicle(DoCommandFlag flags, TileIndex tile, const Engine *e, Vehicle **ret);
CommandCost CmdSellRailWagon(DoCommandFlag flags, Vehicle *t, bool sell_chain, bool backup_order, ClientID user);

CommandCost CmdMoveRailVehicle(DoCommandFlag flags, VehicleID src_veh, VehicleID dest_veh, bool move_chain);
CommandCost CmdForceTrainProceed(DoCommandFlag flags, VehicleID veh_id);
CommandCost CmdReverseTrainDirection(DoCommandFlag flags, VehicleID veh_id, bool reverse_single_veh);

DEF_CMD_TRAIT(CMD_MOVE_RAIL_VEHICLE,       CmdMoveRailVehicle,       CMD_LOCATION, CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_FORCE_TRAIN_PROCEED,     CmdForceTrainProceed,     CMD_LOCATION, CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REVERSE_TRAIN_DIRECTION, CmdReverseTrainDirection, CMD_LOCATION, CMDT_VEHICLE_MANAGEMENT)

void CcBuildWagon(Commands cmd, const CommandCost &result, VehicleID new_veh_id, uint, uint16_t, CargoArray, TileIndex tile, EngineID, bool, CargoID, ClientID);

#endif /* TRAIN_CMD_H */
