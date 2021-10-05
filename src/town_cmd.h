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

CommandProc CmdFoundTown;
CommandProc CmdRenameTown;
CommandProc CmdDoTownAction;
CommandProc CmdTownGrowthRate;
CommandProc CmdTownRating;
CommandProc CmdTownCargoGoal;
CommandProc CmdTownSetText;
CommandProc CmdExpandTown;
CommandProc CmdDeleteTown;

DEF_CMD_TRAIT(CMD_FOUND_TOWN,       CmdFoundTown,      CMD_DEITY | CMD_NO_TEST,  CMDT_LANDSCAPE_CONSTRUCTION) // founding random town can fail only in exec run
DEF_CMD_TRAIT(CMD_RENAME_TOWN,      CmdRenameTown,     CMD_DEITY | CMD_SERVER,   CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_DO_TOWN_ACTION,   CmdDoTownAction,   0,                        CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_TOWN_CARGO_GOAL,  CmdTownCargoGoal,  CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_GROWTH_RATE, CmdTownGrowthRate, CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_RATING,      CmdTownRating,     CMD_DEITY,                CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_TOWN_SET_TEXT,    CmdTownSetText,    CMD_DEITY | CMD_STR_CTRL, CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_EXPAND_TOWN,      CmdExpandTown,     CMD_DEITY,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_DELETE_TOWN,      CmdDeleteTown,     CMD_OFFLINE,              CMDT_LANDSCAPE_CONSTRUCTION)

#endif /* TOWN_CMD_H */
