/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file water_cmd.h Command definitions related to water tiles. */

#ifndef WATER_CMD_H
#define WATER_CMD_H

#include "command_type.h"

CommandCost CmdBuildShipDepot(DoCommandFlag flags, TileIndex tile, Axis axis);
CommandProc CmdBuildCanal;
CommandProc CmdBuildLock;

DEF_CMD_TRAIT(CMD_BUILD_SHIP_DEPOT, CmdBuildShipDepot, CMD_AUTO, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_CANAL,      CmdBuildCanal,     CMD_AUTO, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_LOCK,       CmdBuildLock,      CMD_AUTO, CMDT_LANDSCAPE_CONSTRUCTION)

#endif /* WATER_CMD_H */
