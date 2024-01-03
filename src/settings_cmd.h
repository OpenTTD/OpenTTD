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

CommandCost CmdChangeSetting(DoCommandFlag flags, const std::string &name, int32_t value);
CommandCost CmdChangeCompanySetting(DoCommandFlag flags, const std::string &name, int32_t value);

DEF_CMD_TRAIT(CMD_CHANGE_SETTING,         CmdChangeSetting,        CMD_SERVER, CMDT_SERVER_SETTING)
DEF_CMD_TRAIT(CMD_CHANGE_COMPANY_SETTING, CmdChangeCompanySetting, 0,          CMDT_COMPANY_SETTING)

#endif /* SETTINGS_CMD_H */
