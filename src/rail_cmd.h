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
#include "track_type.h"
#include "rail_type.h"
#include "signal_type.h"

CommandCost CmdBuildRailroadTrack(DoCommandFlags flags, TileIndex end_tile, TileIndex start_tile, RailType railtype, Track track, bool auto_remove_signals, bool fail_on_obstacle);
CommandCost CmdRemoveRailroadTrack(DoCommandFlags flags, TileIndex end_tile, TileIndex start_tile, Track track);
CommandCost CmdBuildSingleRail(DoCommandFlags flags, TileIndex tile, RailType railtype, Track track, bool auto_remove_signals);
CommandCost CmdRemoveSingleRail(DoCommandFlags flags, TileIndex tile, Track track);
CommandCost CmdBuildTrainDepot(DoCommandFlags flags, TileIndex tile, RailType railtype, DiagDirection dir);
CommandCost CmdBuildSingleSignal(DoCommandFlags flags, TileIndex tile, Track track, SignalType sigtype, SignalVariant sigvar, bool convert_signal, bool skip_existing_signals, bool ctrl_pressed, SignalType cycle_start, SignalType cycle_stop, uint8_t num_dir_cycle, uint8_t signals_copy);
CommandCost CmdRemoveSingleSignal(DoCommandFlags flags, TileIndex tile, Track track);
CommandCost CmdConvertRail(DoCommandFlags flags, TileIndex tile, TileIndex area_start, RailType totype, bool diagonal);
CommandCost CmdBuildSignalTrack(DoCommandFlags flags, TileIndex tile, TileIndex end_tile, Track track, SignalType sigtype, SignalVariant sigvar, bool mode, bool autofill, bool minimise_gaps, uint8_t signal_density);
CommandCost CmdRemoveSignalTrack(DoCommandFlags flags, TileIndex tile, TileIndex end_tile, Track track, bool autofill);

template <> struct CommandTraits<CMD_BUILD_RAILROAD_TRACK>  : DefaultCommandTraits<CMD_BUILD_RAILROAD_TRACK,  "CmdBuildRailroadTrack",  CmdBuildRailroadTrack,  CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_REMOVE_RAILROAD_TRACK> : DefaultCommandTraits<CMD_REMOVE_RAILROAD_TRACK, "CmdRemoveRailroadTrack", CmdRemoveRailroadTrack, CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_SINGLE_RAIL>     : DefaultCommandTraits<CMD_BUILD_SINGLE_RAIL,     "CmdBuildSingleRail",     CmdBuildSingleRail,     CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_REMOVE_SINGLE_RAIL>    : DefaultCommandTraits<CMD_REMOVE_SINGLE_RAIL,    "CmdRemoveSingleRail",    CmdRemoveSingleRail,    CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_TRAIN_DEPOT>     : DefaultCommandTraits<CMD_BUILD_TRAIN_DEPOT,     "CmdBuildTrainDepot",     CmdBuildTrainDepot,     CMD_AUTO | CMD_NO_WATER, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_SINGLE_SIGNAL>   : DefaultCommandTraits<CMD_BUILD_SINGLE_SIGNAL,   "CmdBuildSingleSignal",   CmdBuildSingleSignal,   CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_REMOVE_SINGLE_SIGNAL>  : DefaultCommandTraits<CMD_REMOVE_SINGLE_SIGNAL,  "CmdRemoveSingleSignal",  CmdRemoveSingleSignal,  CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_CONVERT_RAIL>          : DefaultCommandTraits<CMD_CONVERT_RAIL,          "CmdConvertRail",         CmdConvertRail,         CommandFlags{},          CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_SIGNAL_TRACK>    : DefaultCommandTraits<CMD_BUILD_SIGNAL_TRACK,    "CmdBuildSignalTrack",    CmdBuildSignalTrack,    CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_REMOVE_SIGNAL_TRACK>   : DefaultCommandTraits<CMD_REMOVE_SIGNAL_TRACK,   "CmdRemoveSignalTrack",   CmdRemoveSignalTrack,   CMD_AUTO,                CMDT_LANDSCAPE_CONSTRUCTION> {};

CommandCallback CcPlaySound_CONSTRUCTION_RAIL;
CommandCallback CcStation;
CommandCallback CcBuildRailTunnel;
void CcRailDepot(Commands cmd, const CommandCost &result, TileIndex tile, RailType rt, DiagDirection dir);

#endif /* RAIL_CMD_H */
