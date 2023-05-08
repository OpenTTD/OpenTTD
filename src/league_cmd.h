/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file league_cmd.h Command definitions related to league tables. */

#ifndef LEAGUE_CMD_H
#define LEAGUE_CMD_H

#include "league_type.h"
#include "command_type.h"
#include "company_type.h"

std::tuple<CommandCost, LeagueTableID> CmdCreateLeagueTable(DoCommandFlag flags, const std::string &title, const std::string &header, const std::string &footer);
std::tuple<CommandCost, LeagueTableElementID> CmdCreateLeagueTableElement(DoCommandFlag flags, LeagueTableID table, int64_t rating, CompanyID company, const std::string &text, const std::string &score, LinkType link_type, LinkTargetID link_target);
CommandCost CmdUpdateLeagueTableElementData(DoCommandFlag flags, LeagueTableElementID element, CompanyID company, const std::string &text, LinkType link_type, LinkTargetID link_target);
CommandCost CmdUpdateLeagueTableElementScore(DoCommandFlag flags, LeagueTableElementID element, int64_t rating, const std::string &score);
CommandCost CmdRemoveLeagueTableElement(DoCommandFlag flags, LeagueTableElementID element);

DEF_CMD_TRAIT(CMD_CREATE_LEAGUE_TABLE, CmdCreateLeagueTable, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CREATE_LEAGUE_TABLE_ELEMENT, CmdCreateLeagueTableElement, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_UPDATE_LEAGUE_TABLE_ELEMENT_DATA, CmdUpdateLeagueTableElementData, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_UPDATE_LEAGUE_TABLE_ELEMENT_SCORE, CmdUpdateLeagueTableElementScore, CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_LEAGUE_TABLE_ELEMENT, CmdRemoveLeagueTableElement, CMD_DEITY, CMDT_OTHER_MANAGEMENT)

#endif /* LEAGUE_CMD_H */
