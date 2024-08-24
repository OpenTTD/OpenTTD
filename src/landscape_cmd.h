/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file landscape_cmd.h Command definitions related to landscape (slopes etc.). */

#ifndef LANDSCAPE_CMD_H
#define LANDSCAPE_CMD_H

#include "command_type.h"

CommandCost CmdLandscapeClear(DoCommandFlags flags, TileIndex tile);
std::tuple<CommandCost, Money> CmdClearArea(DoCommandFlags flags, TileIndex tile, TileIndex start_tile, bool diagonal);

template <> struct CommandTraits<CMD_LANDSCAPE_CLEAR> : DefaultCommandTraits<CMD_LANDSCAPE_CLEAR, "CmdLandscapeClear", CmdLandscapeClear, CMD_DEITY,   CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_CLEAR_AREA>      : DefaultCommandTraits<CMD_CLEAR_AREA,      "CmdClearArea",      CmdClearArea,      CMD_NO_TEST, CMDT_LANDSCAPE_CONSTRUCTION> {}; // destroying multi-tile houses makes town rating differ between test and execution

#endif /* LANDSCAPE_CMD_H */
