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
#include "company_type.h"
#include "industry_type.h"

enum class IndustryAction : byte;
enum IndustryControlFlags : byte;

CommandCost CmdBuildIndustry(DoCommandFlag flags, TileIndex tile, IndustryType it, uint32 first_layout, bool fund, uint32 seed);
CommandCost CmdIndustryCtrl(DoCommandFlag flags, IndustryID ind_id, IndustryAction action, IndustryControlFlags ctlflags, Owner company_id, const std::string &text);

DEF_CMD_TRAIT(CMD_BUILD_INDUSTRY, CmdBuildIndustry, CMD_DEITY,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_INDUSTRY_CTRL,  CmdIndustryCtrl,  CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)

void CcBuildIndustry(Commands cmd, const CommandCost &result, TileIndex tile, IndustryType indtype, uint32, bool, uint32);

#endif /* INDUSTRY_CMD_H */
