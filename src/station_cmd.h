/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_cmd.h Command definitions related to stations. */

#ifndef STATION_CMD_H
#define STATION_CMD_H

#include "command_type.h"
#include "station_type.h"

enum StationClassID : uint16_t;
enum RoadStopClassID : uint16_t;

extern Town *AirportGetNearestTown(const struct AirportSpec *as, Direction rotation, TileIndex tile, TileIterator &&it, uint &mindist);
extern uint8_t GetAirportNoiseLevelForDistance(const struct AirportSpec *as, uint distance);

CommandCost CmdBuildAirport(DoCommandFlags flags, TileIndex tile, uint8_t airport_type, uint8_t layout, StationID station_to_join, bool allow_adjacent);
CommandCost CmdBuildDock(DoCommandFlags flags, TileIndex tile, StationID station_to_join, bool adjacent);
CommandCost CmdBuildRailStation(DoCommandFlags flags, TileIndex tile_org, RailType rt, Axis axis, uint8_t numtracks, uint8_t plat_len, StationClassID spec_class, uint16_t spec_index, StationID station_to_join, bool adjacent);
CommandCost CmdRemoveFromRailStation(DoCommandFlags flags, TileIndex start, TileIndex end, bool keep_rail);
CommandCost CmdBuildRoadStop(DoCommandFlags flags, TileIndex tile, uint8_t width, uint8_t length, RoadStopType stop_type, bool is_drive_through, DiagDirection ddir, RoadType rt, RoadStopClassID spec_class, uint16_t spec_index, StationID station_to_join, bool adjacent);
CommandCost CmdRemoveRoadStop(DoCommandFlags flags, TileIndex tile, uint8_t width, uint8_t height, RoadStopType stop_type, bool remove_road);
CommandCost CmdRenameStation(DoCommandFlags flags, StationID station_id, const std::string &text);
CommandCost CmdOpenCloseAirport(DoCommandFlags flags, StationID station_id);

DEF_CMD_TRAIT(CMD_BUILD_AIRPORT,            CmdBuildAirport,          CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_DOCK,               CmdBuildDock,             CommandFlag::Auto,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_RAIL_STATION,       CmdBuildRailStation,      CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_FROM_RAIL_STATION, CmdRemoveFromRailStation, {},                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_ROAD_STOP,          CmdBuildRoadStop,         CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_ROAD_STOP,         CmdRemoveRoadStop,        {},                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_RENAME_STATION,           CmdRenameStation,         {},                       CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_OPEN_CLOSE_AIRPORT,       CmdOpenCloseAirport,      {},                       CMDT_ROUTE_MANAGEMENT)

#endif /* STATION_CMD_H */
