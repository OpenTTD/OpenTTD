/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_cmd.h Command definitions related to engines. */

#ifndef ENGINE_CMD_H
#define ENGINE_CMD_H

#include "command_type.h"

CommandCost CmdWantEnginePreview(DoCommandFlags flags, EngineID engine_id);
CommandCost CmdEngineCtrl(DoCommandFlags flags, EngineID engine_id, CompanyID company_id, bool allow);
CommandCost CmdRenameEngine(DoCommandFlags flags, EngineID engine_id, const std::string &text);
CommandCost CmdSetVehicleVisibility(DoCommandFlags flags, EngineID engine_id, bool hide);

template <> struct CommandTraits<CMD_WANT_ENGINE_PREVIEW>    : DefaultCommandTraits<CMD_WANT_ENGINE_PREVIEW,    "CmdWantEnginePreview",    CmdWantEnginePreview,    CommandFlags{}, CMDT_VEHICLE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_ENGINE_CTRL>            : DefaultCommandTraits<CMD_ENGINE_CTRL,            "CmdEngineCtrl",           CmdEngineCtrl,           CMD_DEITY,  CMDT_VEHICLE_MANAGEMENT> {};
template <> struct CommandTraits<CMD_RENAME_ENGINE>          : DefaultCommandTraits<CMD_RENAME_ENGINE,          "CmdRenameEngine",         CmdRenameEngine,         CMD_SERVER, CMDT_OTHER_MANAGEMENT> {};
template <> struct CommandTraits<CMD_SET_VEHICLE_VISIBILITY> : DefaultCommandTraits<CMD_SET_VEHICLE_VISIBILITY, "CmdSetVehicleVisibility", CmdSetVehicleVisibility, CommandFlags{}, CMDT_COMPANY_SETTING> {};

#endif /* ENGINE_CMD_H */
