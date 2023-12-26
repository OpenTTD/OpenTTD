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

enum StationClassID : byte;
enum RoadStopClassID : byte;

extern Town *AirportGetNearestTown(const struct AirportSpec *as, Direction rotation, TileIndex tile, TileIterator &&it, uint &mindist);
extern uint8_t GetAirportNoiseLevelForDistance(const struct AirportSpec *as, uint distance);

CommandCost CmdBuildAirport(DoCommandFlag flags, TileIndex tile, byte airport_type, byte layout, StationID station_to_join, bool allow_adjacent);
CommandCost CmdBuildDock(DoCommandFlag flags, TileIndex tile, StationID station_to_join, bool adjacent);
CommandCost CmdBuildRailStation(DoCommandFlag flags, TileIndex tile_org, RailType rt, Axis axis, byte numtracks, byte plat_len, StationClassID spec_class, uint16_t spec_index, StationID station_to_join, bool adjacent);
CommandCost CmdRemoveFromRailStation(DoCommandFlag flags, TileIndex start, TileIndex end, bool keep_rail);
CommandCost CmdBuildRoadStop(DoCommandFlag flags, TileIndex tile, uint8_t width, uint8_t length, RoadStopType stop_type, bool is_drive_through, DiagDirection ddir, RoadType rt, RoadStopClassID spec_class, uint16_t spec_index, StationID station_to_join, bool adjacent);
CommandCost CmdRemoveRoadStop(DoCommandFlag flags, TileIndex tile, uint8_t width, uint8_t height, RoadStopType stop_type, bool remove_road);
CommandCost CmdRenameStation(DoCommandFlag flags, StationID station_id, const std::string &text);
CommandCost CmdOpenCloseAirport(DoCommandFlag flags, StationID station_id);

DEF_CMD_TRAIT(CMD_BUILD_AIRPORT,            CmdBuildAirport,          CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_DOCK,               CmdBuildDock,             CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_RAIL_STATION,       CmdBuildRailStation,      CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_FROM_RAIL_STATION, CmdRemoveFromRailStation, 0,                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_ROAD_STOP,          CmdBuildRoadStop,         CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_ROAD_STOP,         CmdRemoveRoadStop,        0,                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_RENAME_STATION,           CmdRenameStation,         0,                       CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_OPEN_CLOSE_AIRPORT,       CmdOpenCloseAirport,      0,                       CMDT_ROUTE_MANAGEMENT)

#endif /* STATION_CMD_H */
