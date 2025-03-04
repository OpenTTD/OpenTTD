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
#include "town.h"
#include "town_type.h"

enum TownAcceptanceEffect : uint8_t;
using HouseID = uint16_t;

std::tuple<CommandCost, Money, TownID> CmdFoundTown(DoCommandFlags flags, TileIndex tile, TownSize size, bool city, TownLayout layout, bool random_location, uint32_t townnameparts, const std::string &text);
CommandCost CmdRenameTown(DoCommandFlags flags, TownID town_id, const std::string &text);
CommandCost CmdDoTownAction(DoCommandFlags flags, TownID town_id, TownAction action);
CommandCost CmdTownGrowthRate(DoCommandFlags flags, TownID town_id, uint16_t growth_rate);
CommandCost CmdTownRating(DoCommandFlags flags, TownID town_id, CompanyID company_id, int16_t rating);
CommandCost CmdTownCargoGoal(DoCommandFlags flags, TownID town_id, TownAcceptanceEffect tae, uint32_t goal);
CommandCost CmdTownSetText(DoCommandFlags flags, TownID town_id, const EncodedString &text);
CommandCost CmdExpandTown(DoCommandFlags flags, TownID town_id, uint32_t grow_amount);
CommandCost CmdDeleteTown(DoCommandFlags flags, TownID town_id);
CommandCost CmdPlaceHouse(DoCommandFlags flags, TileIndex tile, HouseID house, bool house_protected);

DEF_CMD_TRAIT(CMD_FOUND_TOWN,       CmdFoundTown,      CommandFlags({CommandFlag::Deity, CommandFlag::NoTest}),  CMDT_LANDSCAPE_CONSTRUCTION) // founding random town can fail only in exec run
DEF_CMD_TRAIT(CMD_RENAME_TOWN,      CmdRenameTown,     CommandFlags({CommandFlag::Deity, CommandFlag::Server}),  CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DO_TOWN_ACTION,   CmdDoTownAction,   CommandFlags({CommandFlag::Location}),                    CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_TOWN_CARGO_GOAL,  CmdTownCargoGoal,  CommandFlags({CommandFlag::Deity}),                       CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_GROWTH_RATE, CmdTownGrowthRate, CommandFlags({CommandFlag::Deity}),                       CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_RATING,      CmdTownRating,     CommandFlags({CommandFlag::Deity}),                       CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_SET_TEXT,    CmdTownSetText,    CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_EXPAND_TOWN,      CmdExpandTown,     CommandFlags({CommandFlag::Deity}),                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_DELETE_TOWN,      CmdDeleteTown,     CommandFlags({CommandFlag::Offline}),                     CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_PLACE_HOUSE,      CmdPlaceHouse,     CommandFlags({CommandFlag::Deity}),                       CMDT_OTHER_MANAGEMENT)

CommandCallback CcFoundTown;
void CcFoundRandomTown(Commands cmd, const CommandCost &result, Money, TownID town_id);

#endif /* TOWN_CMD_H */
