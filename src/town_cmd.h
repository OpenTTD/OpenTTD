/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_cmd.h Command definitions related to towns. */

#ifndef TOWN_CMD_H
#define TOWN_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "town_type.h"

enum TownEffect : byte;

std::tuple<CommandCost, Money, TownID> CmdFoundTown(DoCommandFlag flags, TileIndex tile, TownSize size, bool city, TownLayout layout, bool random_location, uint32 townnameparts, const std::string &text);
CommandCost CmdRenameTown(DoCommandFlag flags, TownID town_id, const std::string &text);
CommandCost CmdDoTownAction(DoCommandFlag flags, TownID town_id, uint8 action);
CommandCost CmdTownGrowthRate(DoCommandFlag flags, TownID town_id, uint16 growth_rate);
CommandCost CmdTownRating(DoCommandFlag flags, TownID town_id, CompanyID company_id, int16 rating);
CommandCost CmdTownCargoGoal(DoCommandFlag flags, TownID town_id, TownEffect te, uint32 goal);
CommandCost CmdTownSetText(DoCommandFlag flags, TownID town_id, const std::string &text);
CommandCost CmdExpandTown(DoCommandFlag flags, TownID town_id, uint32 grow_amount);
CommandCost CmdDeleteTown(DoCommandFlag flags, TownID town_id);

DEF_CMD_TRAIT(CMD_FOUND_TOWN,       CmdFoundTown,      CMD_DEITY | CMD_NO_TEST,  CMDT_LANDSCAPE_CONSTRUCTION) // founding random town can fail only in exec run
DEF_CMD_TRAIT(CMD_RENAME_TOWN,      CmdRenameTown,     CMD_DEITY | CMD_SERVER,   CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DO_TOWN_ACTION,   CmdDoTownAction,   0,                        CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_TOWN_CARGO_GOAL,  CmdTownCargoGoal,  CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_GROWTH_RATE, CmdTownGrowthRate, CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_RATING,      CmdTownRating,     CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_SET_TEXT,    CmdTownSetText,    CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_EXPAND_TOWN,      CmdExpandTown,     CMD_DEITY,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_DELETE_TOWN,      CmdDeleteTown,     CMD_OFFLINE,              CMDT_LANDSCAPE_CONSTRUCTION)

CommandCallback CcFoundTown;
void CcFoundRandomTown(Commands cmd, const CommandCost &result, Money, TownID town_id);

#endif /* TOWN_CMD_H */
