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

enum Colours : uint8_t;
enum class GroupFlag : uint8_t;

/** Action for \c CmdAlterGroup. */
enum class AlterGroupMode : uint8_t {
	Rename,    ///< Change group name.
	SetParent, ///< Change group parent.
};

std::tuple<CommandCost, GroupID> CmdCreateGroup(DoCommandFlags flags, VehicleType vt, GroupID parent_group);
CommandCost CmdAlterGroup(DoCommandFlags flags, AlterGroupMode mode, GroupID group_id, GroupID parent_id, const std::string &text);
CommandCost CmdDeleteGroup(DoCommandFlags flags, GroupID group_id);
std::tuple<CommandCost, GroupID> CmdAddVehicleGroup(DoCommandFlags flags, GroupID group_id, VehicleID veh_id, bool add_shared, const VehicleListIdentifier &vli);
CommandCost CmdAddSharedVehicleGroup(DoCommandFlags flags, GroupID id_g, VehicleType type);
CommandCost CmdRemoveAllVehiclesGroup(DoCommandFlags flags, GroupID group_id);
CommandCost CmdSetGroupFlag(DoCommandFlags flags, GroupID group_id, GroupFlag flag, bool value, bool recursive);
CommandCost CmdSetGroupLivery(DoCommandFlags flags, GroupID group_id, bool primary, Colours colour);

template <> struct CommandTraits<CMD_CREATE_GROUP>              : DefaultCommandTraits<CMD_CREATE_GROUP,              "CmdCreateGroup",            CmdCreateGroup,            CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_DELETE_GROUP>              : DefaultCommandTraits<CMD_DELETE_GROUP,              "CmdDeleteGroup",            CmdDeleteGroup,            CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_ALTER_GROUP>               : DefaultCommandTraits<CMD_ALTER_GROUP,               "CmdAlterGroup",             CmdAlterGroup,             CommandFlags{}, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_ADD_VEHICLE_GROUP>         : DefaultCommandTraits<CMD_ADD_VEHICLE_GROUP,         "CmdAddVehicleGroup",        CmdAddVehicleGroup,        CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_ADD_SHARED_VEHICLE_GROUP>  : DefaultCommandTraits<CMD_ADD_SHARED_VEHICLE_GROUP,  "CmdAddSharedVehicleGroup",  CmdAddSharedVehicleGroup,  CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_REMOVE_ALL_VEHICLES_GROUP> : DefaultCommandTraits<CMD_REMOVE_ALL_VEHICLES_GROUP, "CmdRemoveAllVehiclesGroup", CmdRemoveAllVehiclesGroup, CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_GROUP_FLAG>            : DefaultCommandTraits<CMD_SET_GROUP_FLAG,            "CmdSetGroupFlag",           CmdSetGroupFlag,           CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_GROUP_LIVERY>          : DefaultCommandTraits<CMD_SET_GROUP_LIVERY,          "CmdSetGroupLivery",         CmdSetGroupLivery,         CommandFlags{}, CMDT_ROUTE_MANAGEMENT> {};

void CcCreateGroup(Commands cmd, const CommandCost &result, GroupID new_group, VehicleType vt, GroupID parent_group);
void CcAddVehicleNewGroup(Commands cmd, const CommandCost &result, GroupID new_group, GroupID, VehicleID veh_id, bool, const VehicleListIdentifier &);

#endif /* GROUP_CMD_H */
