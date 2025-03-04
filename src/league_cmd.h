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

std::tuple<CommandCost, LeagueTableID> CmdCreateLeagueTable(DoCommandFlags flags, const EncodedString &title, const EncodedString &header, const EncodedString &footer);
std::tuple<CommandCost, LeagueTableElementID> CmdCreateLeagueTableElement(DoCommandFlags flags, LeagueTableID table, int64_t rating, CompanyID company, const EncodedString &text, const EncodedString &score, LinkType link_type, LinkTargetID link_target);
CommandCost CmdUpdateLeagueTableElementData(DoCommandFlags flags, LeagueTableElementID element, CompanyID company, const EncodedString &text, LinkType link_type, LinkTargetID link_target);
CommandCost CmdUpdateLeagueTableElementScore(DoCommandFlags flags, LeagueTableElementID element, int64_t rating, const EncodedString &score);
CommandCost CmdRemoveLeagueTableElement(DoCommandFlags flags, LeagueTableElementID element);

DEF_CMD_TRAIT(CMD_CREATE_LEAGUE_TABLE, CmdCreateLeagueTable, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_CREATE_LEAGUE_TABLE_ELEMENT, CmdCreateLeagueTableElement, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_UPDATE_LEAGUE_TABLE_ELEMENT_DATA, CmdUpdateLeagueTableElementData, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_UPDATE_LEAGUE_TABLE_ELEMENT_SCORE, CmdUpdateLeagueTableElementScore, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_LEAGUE_TABLE_ELEMENT, CmdRemoveLeagueTableElement, CommandFlag::Deity, CMDT_OTHER_MANAGEMENT)

#endif /* LEAGUE_CMD_H */
