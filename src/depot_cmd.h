/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file depot_cmd.h Command definitions related to depots. */

#ifndef DEPOT_CMD_H
#define DEPOT_CMD_H

#include "command_type.h"
#include "depot_type.h"
#include "vehicle_type.h"

CommandCost CmdRenameDepot(DoCommandFlags flags, DepotID depot_id, const std::string &text);

DEF_CMD_TRAIT(CMD_RENAME_DEPOT, CmdRenameDepot, {}, CommandType::OtherManagement)

void CcCloneVehicle(Commands cmd, const CommandCost &result, VehicleID veh_id);

#endif /* DEPOT_CMD_H */
