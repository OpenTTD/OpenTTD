/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_cmd.h Command definitions for rail. */

#ifndef RAIL_CMD_H
#define RAIL_CMD_H

#include "command_type.h"

CommandProc CmdBuildRailroadTrack;
CommandProc CmdRemoveRailroadTrack;
CommandProc CmdBuildSingleRail;
CommandProc CmdRemoveSingleRail;
CommandCost CmdBuildTrainDepot(DoCommandFlag flags, TileIndex tile, RailType railtype, DiagDirection dir);
CommandProc CmdBuildSingleSignal;
CommandProc CmdRemoveSingleSignal;
CommandProc CmdConvertRail;
CommandProc CmdBuildSignalTrack;
CommandProc CmdRemoveSignalTrack;

DEF_CMD_TRAIT(CMD_BUILD_RAILROAD_TRACK,  CmdBuildRailroadTrack,  CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_RAILROAD_TRACK, CmdRemoveRailroadTrack, CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_SINGLE_RAIL,     CmdBuildSingleRail,     CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_SINGLE_RAIL,    CmdRemoveSingleRail,    CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_TRAIN_DEPOT,     CmdBuildTrainDepot,     CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_SIGNALS,         CmdBuildSingleSignal,   CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_SIGNALS,        CmdRemoveSingleSignal,  CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_CONVERT_RAIL,          CmdConvertRail,                0,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_BUILD_SIGNAL_TRACK,    CmdBuildSignalTrack,    CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)
DEF_CMD_TRAIT(CMD_REMOVE_SIGNAL_TRACK,   CmdRemoveSignalTrack,   CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION)

CommandCallback CcPlaySound_CONSTRUCTION_RAIL;
CommandCallback CcRailDepot;
CommandCallback CcStation;
CommandCallback CcBuildRailTunnel;

#endif /* RAIL_CMD_H */
