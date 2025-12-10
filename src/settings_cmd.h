/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file settings_cmd.h Command definitions related to settings. */

#ifndef SETTINGS_CMD_H
#define SETTINGS_CMD_H

#include "command_type.h"

CommandCost CmdChangeSetting(DoCommandFlags flags, const std::string &name, int32_t value);
CommandCost CmdChangeCompanySetting(DoCommandFlags flags, const std::string &name, int32_t value);

DEF_CMD_TRAIT(CMD_CHANGE_SETTING,         CmdChangeSetting,        CommandFlags({CommandFlag::Server, CommandFlag::NoEst}), CommandType::ServerSetting)
DEF_CMD_TRAIT(CMD_CHANGE_COMPANY_SETTING, CmdChangeCompanySetting, CommandFlag::NoEst, CommandType::CompanySetting)

#endif /* SETTINGS_CMD_H */
