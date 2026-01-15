/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

DEF_CMD_TRAIT(Commands::CreateGroup, CmdCreateGroup, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(Commands::DeleteGroup, CmdDeleteGroup, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(Commands::AlterGroup, CmdAlterGroup, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::AddVehicleToGroup, CmdAddVehicleGroup, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(Commands::AddSharedVehiclesToGroup, CmdAddSharedVehicleGroup, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(Commands::RemoveAllVehiclesGroup, CmdRemoveAllVehiclesGroup, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(Commands::SetGroupFlag, CmdSetGroupFlag, {}, CommandType::RouteManagement)
DEF_CMD_TRAIT(Commands::SetGroupLivery, CmdSetGroupLivery, {}, CommandType::RouteManagement)

void CcCreateGroup(Commands cmd, const CommandCost &result, GroupID new_group, VehicleType vt, GroupID parent_group);
void CcAddVehicleNewGroup(Commands cmd, const CommandCost &result, GroupID new_group, GroupID, VehicleID veh_id, bool, const VehicleListIdentifier &);

#endif /* GROUP_CMD_H */
