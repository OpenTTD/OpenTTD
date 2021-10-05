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

CommandProc CmdBuildAirport;
CommandProc CmdBuildDock;
CommandProc CmdBuildRailStation;
CommandProc CmdRemoveFromRailStation;
CommandProc CmdBuildRoadStop;
CommandProc CmdRemoveRoadStop;
CommandProc CmdRenameStation;
CommandProc CmdOpenCloseAirport;

DEF_CMD_TRAIT(CMD_BUILD_AIRPORT,            CmdBuildAirport,          CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_DOCK,               CmdBuildDock,             CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_RAIL_STATION,       CmdBuildRailStation,      CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_FROM_RAIL_STATION, CmdRemoveFromRailStation, 0,                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_ROAD_STOP,          CmdBuildRoadStop,         CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_ROAD_STOP,         CmdRemoveRoadStop,        0,                       CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_RENAME_STATION,           CmdRenameStation,         0,                       CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_OPEN_CLOSE_AIRPORT,       CmdOpenCloseAirport,      0,                       CMDT_ROUTE_MANAGEMENT)

#endif /* STATION_CMD_H */
