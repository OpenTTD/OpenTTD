/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_cmd.h Command definitions related to waypoints. */

#ifndef WAYPOINT_CMD_H
#define WAYPOINT_CMD_H

#include "command_type.h"
#include "station_type.h"

enum StationClassID : uint16_t;
enum RoadStopClassID : uint16_t;

CommandCost CmdBuildRailWaypoint(DoCommandFlags flags, TileIndex start_tile, Axis axis, uint8_t width, uint8_t height, StationClassID spec_class, uint16_t spec_index, StationID station_to_join, bool adjacent);
CommandCost CmdRemoveFromRailWaypoint(DoCommandFlags flags, TileIndex start, TileIndex end, bool keep_rail);
CommandCost CmdBuildRoadWaypoint(DoCommandFlags flags, TileIndex start_tile, Axis axis, uint8_t width, uint8_t height, RoadStopClassID spec_class, uint16_t spec_index, StationID station_to_join, bool adjacent);
CommandCost CmdRemoveFromRoadWaypoint(DoCommandFlags flags, TileIndex start, TileIndex end);
CommandCost CmdBuildBuoy(DoCommandFlags flags, TileIndex tile);
CommandCost CmdRenameWaypoint(DoCommandFlags flags, StationID waypoint_id, const std::string &text);

template <> struct CommandTraits<CMD_BUILD_RAIL_WAYPOINT>       : DefaultCommandTraits<CMD_BUILD_RAIL_WAYPOINT,       "CmdBuildRailWaypoint",      CmdBuildRailWaypoint,      CommandFlags{}, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_REMOVE_FROM_RAIL_WAYPOINT> : DefaultCommandTraits<CMD_REMOVE_FROM_RAIL_WAYPOINT, "CmdRemoveFromRailWaypoint", CmdRemoveFromRailWaypoint, CommandFlags{}, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_ROAD_WAYPOINT>       : DefaultCommandTraits<CMD_BUILD_ROAD_WAYPOINT,       "CmdBuildRoadWaypoint",      CmdBuildRoadWaypoint,      CommandFlags{}, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_REMOVE_FROM_ROAD_WAYPOINT> : DefaultCommandTraits<CMD_REMOVE_FROM_ROAD_WAYPOINT, "CmdRemoveFromRoadWaypoint", CmdRemoveFromRoadWaypoint, CommandFlags{}, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_BUOY>                : DefaultCommandTraits<CMD_BUILD_BUOY,                "CmdBuildBuoy",              CmdBuildBuoy,              CMD_AUTO, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_RENAME_WAYPOINT>           : DefaultCommandTraits<CMD_RENAME_WAYPOINT,           "CmdRenameWaypoint",         CmdRenameWaypoint,         CommandFlags{}, CMDT_OTHER_MANAGEMENT> {};

#endif /* WAYPOINT_CMD_H */
