/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tunnelbridge_cmd.h Command definitions related to tunnels and bridges. */

#ifndef TUNNELBRIDGE_CMD_H
#define TUNNELBRIDGE_CMD_H

#include "command_type.h"
#include "transport_type.h"
#include "bridge.h"

CommandCost CmdBuildBridge(DoCommandFlag flags, TileIndex tile_end, TileIndex tile_start, TransportType transport_type, BridgeType bridge_type, byte road_rail_type);
CommandCost CmdBuildTunnel(DoCommandFlag flags, TileIndex start_tile, TransportType transport_type, byte road_rail_type);

DEF_CMD_TRAIT(CMD_BUILD_BRIDGE, CmdBuildBridge, CMD_DEITY | CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_TUNNEL, CmdBuildTunnel, CMD_DEITY | CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)

void CcBuildBridge(Commands cmd, const CommandCost &result, TileIndex end_tile, TileIndex tile_start, TransportType transport_type, BridgeType, byte);

#endif /* TUNNELBRIDGE_CMD_H */
