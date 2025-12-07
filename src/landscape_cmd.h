/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file landscape_cmd.h Command definitions related to landscape (slopes etc.). */

#ifndef LANDSCAPE_CMD_H
#define LANDSCAPE_CMD_H

#include "command_type.h"

CommandCost CmdLandscapeClear(DoCommandFlags flags, TileIndex tile);
std::tuple<CommandCost, Money> CmdClearArea(DoCommandFlags flags, TileIndex tile, TileIndex start_tile, bool diagonal);

DEF_CMD_TRAIT(CMD_LANDSCAPE_CLEAR, CmdLandscapeClear, CommandFlag::Deity,  CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(CMD_CLEAR_AREA,      CmdClearArea,      CommandFlag::NoTest, CommandType::LandscapeConstruction) // destroying multi-tile houses makes town rating differ between test and execution

#endif /* LANDSCAPE_CMD_H */
