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

enum IndustryControlFlags : byte;

CommandCost CmdBuildIndustry(DoCommandFlag flags, TileIndex tile, IndustryType it, uint32_t first_layout, bool fund, uint32_t seed);
CommandCost CmdIndustrySetFlags(DoCommandFlag flags, IndustryID ind_id, IndustryControlFlags ctlflags);
CommandCost CmdIndustrySetExclusivity(DoCommandFlag flags, IndustryID ind_id, Owner company_id, bool consumer);
CommandCost CmdIndustrySetText(DoCommandFlag flags, IndustryID ind_id, const std::string &text);
CommandCost CmdIndustrySetProduction(DoCommandFlag flags, IndustryID ind_id, byte prod_level, bool show_news, const std::string &text);

DEF_CMD_TRAIT(CMD_BUILD_INDUSTRY, CmdBuildIndustry, CMD_DEITY, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_INDUSTRY_SET_FLAGS, CmdIndustrySetFlags, CMD_DEITY, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INDUSTRY_SET_EXCLUSIVITY, CmdIndustrySetExclusivity, CMD_DEITY, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INDUSTRY_SET_TEXT, CmdIndustrySetText, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_INDUSTRY_SET_PRODUCTION, CmdIndustrySetProduction, CMD_DEITY, CMDT_OTHER_MANAGEMENT)

void CcBuildIndustry(Commands cmd, const CommandCost &result, TileIndex tile, IndustryType indtype, uint32_t, bool, uint32_t);

#endif /* INDUSTRY_CMD_H */
