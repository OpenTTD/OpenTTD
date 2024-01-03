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
#include "group_type.h"
#include "vehicle_type.h"
#include "vehiclelist.h"
#include "vehiclelist_cmd.h"

enum Colours : byte;
enum GroupFlags : uint8_t;

/** Action for \c CmdAlterGroup. */
enum class AlterGroupMode : byte {
	Rename,    ///< Change group name.
	SetParent, ///< Change group parent.
};

std::tuple<CommandCost, GroupID> CmdCreateGroup(DoCommandFlag flags, VehicleType vt, GroupID parent_group);
CommandCost CmdAlterGroup(DoCommandFlag flags, AlterGroupMode mode, GroupID group_id, GroupID parent_id, const std::string &text);
CommandCost CmdDeleteGroup(DoCommandFlag flags, GroupID group_id);
std::tuple<CommandCost, GroupID> CmdAddVehicleGroup(DoCommandFlag flags, GroupID group_id, VehicleID veh_id, bool add_shared, const VehicleListIdentifier &vli);
CommandCost CmdAddSharedVehicleGroup(DoCommandFlag flags, GroupID id_g, VehicleType type);
CommandCost CmdRemoveAllVehiclesGroup(DoCommandFlag flags, GroupID group_id);
CommandCost CmdSetGroupFlag(DoCommandFlag flags, GroupID group_id, GroupFlags flag, bool value, bool recursive);
CommandCost CmdSetGroupLivery(DoCommandFlag flags, GroupID group_id, bool primary, Colours colour);

DEF_CMD_TRAIT(CMD_CREATE_GROUP,              CmdCreateGroup,            0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DELETE_GROUP,              CmdDeleteGroup,            0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ALTER_GROUP,               CmdAlterGroup,             0, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ADD_VEHICLE_GROUP,         CmdAddVehicleGroup,        0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_ADD_SHARED_VEHICLE_GROUP,  CmdAddSharedVehicleGroup,  0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_ALL_VEHICLES_GROUP, CmdRemoveAllVehiclesGroup, 0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GROUP_FLAG,            CmdSetGroupFlag,           0, CMDT_ROUTE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_GROUP_LIVERY,          CmdSetGroupLivery,         0, CMDT_ROUTE_MANAGEMENT)

void CcCreateGroup(Commands cmd, const CommandCost &result, GroupID new_group, VehicleType vt, GroupID parent_group);
void CcAddVehicleNewGroup(Commands cmd, const CommandCost &result, GroupID new_group, GroupID, VehicleID veh_id, bool, const VehicleListIdentifier &);

#endif /* GROUP_CMD_H */
