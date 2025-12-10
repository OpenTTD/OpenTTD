/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_cmd.h Road related functions. */

#ifndef ROAD_CMD_H
#define ROAD_CMD_H

#include "direction_type.h"
#include "road_type.h"
#include "command_type.h"
#include "station_type.h"
#include "town_type.h"

enum RoadStopClassID : uint16_t;

void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadType rt);
void UpdateNearestTownForRoadTiles(bool invalidate);

CommandCost CmdBuildLongRoad(DoCommandFlags flags, TileIndex end_tile, TileIndex start_tile, RoadType rt, Axis axis, DisallowedRoadDirections drd, bool start_half, bool end_half, bool is_ai);
std::tuple<CommandCost, Money> CmdRemoveLongRoad(DoCommandFlags flags, TileIndex end_tile, TileIndex start_tile, RoadType rt, Axis axis, bool start_half, bool end_half);
CommandCost CmdBuildRoad(DoCommandFlags flags, TileIndex tile, RoadBits pieces, RoadType rt, DisallowedRoadDirections toggle_drd, TownID town_id);
CommandCost CmdBuildRoadDepot(DoCommandFlags flags, TileIndex tile, RoadType rt, DiagDirection dir);
CommandCost CmdConvertRoad(DoCommandFlags flags, TileIndex tile, TileIndex area_start, RoadType to_type, bool diagonal);

DEF_CMD_TRAIT(CMD_BUILD_LONG_ROAD,  CmdBuildLongRoad,  CommandFlags({CommandFlag::Auto, CommandFlag::NoWater, CommandFlag::Deity}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(CMD_REMOVE_LONG_ROAD, CmdRemoveLongRoad, CommandFlags({CommandFlag::Auto, CommandFlag::NoTest}),              CommandType::LandscapeConstruction) // towns may disallow removing road bits (as they are connected) in test, but in exec they're removed and thus removing is allowed.
DEF_CMD_TRAIT(CMD_BUILD_ROAD,       CmdBuildRoad,      CommandFlags({CommandFlag::Auto, CommandFlag::NoWater, CommandFlag::Deity}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(CMD_BUILD_ROAD_DEPOT, CmdBuildRoadDepot, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}),             CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(CMD_CONVERT_ROAD,     CmdConvertRoad,    {},                                                                  CommandType::LandscapeConstruction)

CommandCallback CcPlaySound_CONSTRUCTION_OTHER;
CommandCallback CcBuildRoadTunnel;
void CcRoadDepot(Commands cmd, const CommandCost &result, TileIndex tile, RoadType rt, DiagDirection dir);
void CcRoadStop(Commands cmd, const CommandCost &result, TileIndex tile, uint8_t width, uint8_t length, RoadStopType, bool is_drive_through, DiagDirection dir, RoadType, RoadStopClassID spec_class, uint16_t spec_index, StationID, bool);

#endif /* ROAD_CMD_H */
