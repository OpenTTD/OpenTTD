/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_cmd.h Command definitions related to tree tiles. */

#ifndef TREE_CMD_H
#define TREE_CMD_H

#include "command_type.h"

void PlaceTree(TileIndex tile, uint32_t r, bool keep_density = false);

CommandCost CmdPlantTree(DoCommandFlags flags, TileIndex tile, TileIndex start_tile, uint8_t tree_to_plant, bool diagonal);

DEF_CMD_TRAIT(CMD_PLANT_TREE, CmdPlantTree, CommandFlag::Auto, CommandType::LandscapeConstruction)

#endif /* TREE_CMD_H */
