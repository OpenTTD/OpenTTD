/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_cmd.h Command definitions for vehicles. */

#ifndef VEHICLE_CMD_H
#define VEHICLE_CMD_H

#include "command_type.h"

CommandProc CmdBuildVehicle;
CommandProc CmdSellVehicle;
CommandProc CmdRefitVehicle;
CommandProc CmdSendVehicleToDepot;
CommandProc CmdChangeServiceInt;
CommandProc CmdRenameVehicle;
CommandProc CmdCloneVehicle;
CommandProc CmdStartStopVehicle;
CommandProc CmdMassStartStopVehicle;
CommandProc CmdDepotSellAllVehicles;
CommandProc CmdDepotMassAutoReplace;

DEF_CMD_TRAIT(CMD_BUILD_VEHICLE,           CmdBuildVehicle,         CMD_CLIENT_ID, CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_SELL_VEHICLE,            CmdSellVehicle,          CMD_CLIENT_ID, CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REFIT_VEHICLE,           CmdRefitVehicle,         0,             CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_SEND_VEHICLE_TO_DEPOT,   CmdSendVehicleToDepot,   0,             CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CHANGE_SERVICE_INT,      CmdChangeServiceInt,     0,             CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_RENAME_VEHICLE,          CmdRenameVehicle,        0,             CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CLONE_VEHICLE,           CmdCloneVehicle,         CMD_NO_TEST,   CMDT_VEHICLE_CONSTRUCTION) // NewGRF callbacks influence building and refitting making it impossible to correctly estimate the cost
DEF_CMD_TRAIT(CMD_START_STOP_VEHICLE,      CmdStartStopVehicle,     0,             CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_MASS_START_STOP,         CmdMassStartStopVehicle, 0,             CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DEPOT_SELL_ALL_VEHICLES, CmdDepotSellAllVehicles, 0,             CMDT_VEHICLE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_DEPOT_MASS_AUTOREPLACE,  CmdDepotMassAutoReplace, 0,             CMDT_VEHICLE_CONSTRUCTION)

CommandCallback CcBuildPrimaryVehicle;
CommandCallback CcStartStopVehicle;

#endif /* VEHICLE_CMD_H */
