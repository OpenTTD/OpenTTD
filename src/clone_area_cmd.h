/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file clone_area_cmd.h Command definitions related to cloning area. */

#ifndef CLONE_AREA_CMD_H
#define CLONE_AREA_CMD_H
std::tuple<CommandCost, Money, TileIndex> CmdCloneAreaCopy(DoCommandFlag flags, TileIndex tile, TileIndex start_tile, bool diagonal);
std::tuple<CommandCost, Money, TileIndex> CmdCloneAreaPaste(DoCommandFlag flags, TileIndex tile, TileIndex start_tile, bool diagonal);
std::tuple<CommandCost, Money, TileIndex> CmdCloneAreaPasteProperty(DoCommandFlag flags, TileIndex tile, TileIndex area_start, bool diagonal);

DEF_CMD_TRAIT(CMD_CLONE_AREA_COPY,  CmdCloneAreaCopy,  CMD_NO_TEST, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_CLONE_AREA_PASTE, CmdCloneAreaPaste, CMD_ALL_TILES | CMD_AUTO | CMD_NO_TEST, CMDT_LANDSCAPE_CONSTRUCTION)

void CcCloneArea(Commands cmd, const CommandCost &result, Money, TileIndex tile);

#endif /* CLONE_AREA_CMD_H */
