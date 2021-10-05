/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_cmd.h Command definitions related to industries. */

#ifndef INDUSTRY_CMD_H
#define INDUSTRY_CMD_H

#include "command_type.h"

CommandProc CmdBuildIndustry;
CommandProc CmdIndustryCtrl;

DEF_CMD_TRAIT(CMD_BUILD_INDUSTRY, CmdBuildIndustry, CMD_DEITY,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_INDUSTRY_CTRL,  CmdIndustryCtrl,  CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)

#endif /* INDUSTRY_CMD_H */
