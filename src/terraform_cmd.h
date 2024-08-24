/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file terraform_cmd.h Command definitions related to terraforming. */

#ifndef TERRAFORM_CMD_H
#define TERRAFORM_CMD_H

#include "command_type.h"
#include "map_type.h"
#include "slope_type.h"

std::tuple<CommandCost, Money, TileIndex> CmdTerraformLand(DoCommandFlags flags, TileIndex tile, Slope slope, bool dir_up);
std::tuple<CommandCost, Money, TileIndex> CmdLevelLand(DoCommandFlags flags, TileIndex tile, TileIndex start_tile, bool diagonal, LevelMode lm);

template <> struct CommandTraits<CMD_TERRAFORM_LAND> : DefaultCommandTraits<CMD_TERRAFORM_LAND, "CmdTerraformLand", CmdTerraformLand, CMD_ALL_TILES | CMD_AUTO,               CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_LEVEL_LAND>     : DefaultCommandTraits<CMD_LEVEL_LAND,     "CmdLevelLand",     CmdLevelLand,     CMD_ALL_TILES | CMD_AUTO | CMD_NO_TEST, CMDT_LANDSCAPE_CONSTRUCTION> {}; // test run might clear tiles multiple times, in execution that only happens once

CommandCallback CcPlaySound_EXPLOSION;
void CcTerraform(Commands cmd, const CommandCost &result, Money, TileIndex tile);

#endif /* TERRAFORM_CMD_H */
