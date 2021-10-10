/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_cmd.h Command definitions related to engine groups. */

#ifndef GROUP_CMD_H
#define GROUP_CMD_H

#include "command_type.h"

CommandProc CmdCreateGroup;
CommandProc CmdAlterGroup;
CommandProc CmdDeleteGroup;
CommandProc CmdAddVehicleGroup;
CommandProc CmdAddSharedVehicleGroup;
CommandProc CmdRemoveAllVehiclesGroup;
CommandProc CmdSetGroupFlag;
CommandProc CmdSetGroupLivery;

DEF_CMD_TRAIT(CMD_CREATE_GROUP,              CmdCreateGroup,            0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DELETE_GROUP,              CmdDeleteGroup,            0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ALTER_GROUP,               CmdAlterGroup,             0, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ADD_VEHICLE_GROUP,         CmdAddVehicleGroup,        0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ADD_SHARED_VEHICLE_GROUP,  CmdAddSharedVehicleGroup,  0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_ALL_VEHICLES_GROUP, CmdRemoveAllVehiclesGroup, 0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GROUP_FLAG,            CmdSetGroupFlag,           0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GROUP_LIVERY,          CmdSetGroupLivery,         0, CMDT_ROUTE_MANAGEMENT)

CommandCallback CcCreateGroup;
CommandCallback CcAddVehicleNewGroup;

#endif /* GROUP_CMD_H */
