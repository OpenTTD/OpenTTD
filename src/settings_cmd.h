/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_cmd.h Command definitions related to settings. */

#ifndef SETTINGS_CMD_H
#define SETTINGS_CMD_H

#include "command_type.h"

CommandCost CmdChangeSetting(DoCommandFlags flags, const std::string &name, int32_t value);
CommandCost CmdChangeCompanySetting(DoCommandFlags flags, const std::string &name, int32_t value);

template <> struct CommandTraits<CMD_CHANGE_SETTING>         : DefaultCommandTraits<CMD_CHANGE_SETTING,         "CmdChangeSetting",        CmdChangeSetting,        CMD_SERVER | CMD_NO_EST, CMDT_SERVER_SETTING> {};
template <> struct CommandTraits<CMD_CHANGE_COMPANY_SETTING> : DefaultCommandTraits<CMD_CHANGE_COMPANY_SETTING, "CmdChangeCompanySetting", CmdChangeCompanySetting, CMD_NO_EST,              CMDT_COMPANY_SETTING> {};

#endif /* SETTINGS_CMD_H */
