/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file callback_table.cpp All command callbacks. */

#include "stdafx.h"
#include "callback_table.h"
#include "command_type.h"

/* If you add a callback for DoCommandP, also add the callback in here
 *   see below for the full list!
 * If you don't do it, it won't work across the network!! */

/* ai/ai_core.cpp */
CommandCallback CcAI;

/* airport_gui.cpp */
CommandCallback CcBuildAirport;

/* bridge_gui.cpp */
CommandCallback CcBuildBridge;

/* dock_gui.cpp */
CommandCallback CcBuildDocks;
CommandCallback CcBuildCanal;

/* depot_gui.cpp */
CommandCallback CcCloneVehicle;

/* group_gui.cpp */
CommandCallback CcCreateGroup;

/* main_gui.cpp */
CommandCallback CcPlaySound10;
CommandCallback CcPlaceSign;
CommandCallback CcTerraform;
CommandCallback CcGiveMoney;

/* rail_gui.cpp */
CommandCallback CcPlaySound1E;
CommandCallback CcRailDepot;
CommandCallback CcStation;
CommandCallback CcBuildRailTunnel;

/* road_gui.cpp */
CommandCallback CcPlaySound1D;
CommandCallback CcBuildRoadTunnel;
CommandCallback CcRoadDepot;

/* train_gui.cpp */
CommandCallback CcBuildWagon;

/* town_gui.cpp */
CommandCallback CcFoundTown;
CommandCallback CcFoundRandomTown;

/* vehicle_gui.cpp */
CommandCallback CcBuildPrimaryVehicle;

CommandCallback * const _callback_table[] = {
	/* 0x00 */ NULL,
	/* 0x01 */ CcBuildPrimaryVehicle,
	/* 0x02 */ CcBuildAirport,
	/* 0x03 */ CcBuildBridge,
	/* 0x04 */ CcBuildCanal,
	/* 0x05 */ CcBuildDocks,
	/* 0x06 */ CcFoundTown,
	/* 0x07 */ CcBuildRoadTunnel,
	/* 0x08 */ CcBuildRailTunnel,
	/* 0x09 */ CcBuildWagon,
	/* 0x0A */ CcRoadDepot,
	/* 0x0B */ CcRailDepot,
	/* 0x0C */ CcPlaceSign,
	/* 0x0D */ CcPlaySound10,
	/* 0x0E */ CcPlaySound1D,
	/* 0x0F */ CcPlaySound1E,
	/* 0x10 */ CcStation,
	/* 0x11 */ CcTerraform,
	/* 0x12 */ CcAI,
	/* 0x13 */ CcCloneVehicle,
	/* 0x14 */ CcGiveMoney,
	/* 0x15 */ CcCreateGroup,
	/* 0x16 */ CcFoundRandomTown,
};

const int _callback_table_count = lengthof(_callback_table);
