/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file autoreplace_cmd.h Command definitions related to autoreplace. */

#ifndef AUTOREPLACE_CMD_H
#define AUTOREPLACE_CMD_H

#include "command_type.h"
#include "vehicle_type.h"
#include "engine_type.h"
#include "group_type.h"

CommandCost CmdAutoreplaceVehicle(DoCommandFlags flags, VehicleID veh_id);
CommandCost CmdSetAutoReplace(DoCommandFlags flags, GroupID id_g, EngineID old_engine_type, EngineID new_engine_type, bool when_old);
CommandCost CmdMassRailtypeUpgrade(DoCommandFlags flags, GroupID group, EngineID new_engine);

DEF_CMD_TRAIT(Commands::AutoreplaceVehicle, CmdAutoreplaceVehicle, {}, CommandType::VehicleManagement)
DEF_CMD_TRAIT(Commands::SetAutoreplace, CmdSetAutoReplace, {}, CommandType::VehicleManagement)
DEF_CMD_TRAIT(Commands::MassRailtypeUpgrade, CmdMassRailtypeUpgrade, CommandFlag::NoTest, CommandType::VehicleManagement) // the depot conversion and the replacements only happen in the exec run, so the test run cannot predict the cost

#endif /* AUTOREPLACE_CMD_H */
