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

/* aircraft_gui.cpp */
CommandCallback CcBuildAircraft;

/* airport_gui.cpp */
CommandCallback CcBuildAirport;

/* bridge_gui.cpp */
CommandCallback CcBuildBridge;

/* dock_gui.cpp */
CommandCallback CcBuildDocks;
CommandCallback CcBuildCanal;

/* depot_gui.cpp */
CommandCallback CcCloneVehicle;

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

/* roadveh_gui.cpp */
CommandCallback CcBuildRoadVeh;

/* ship_gui.cpp */
CommandCallback CcBuildShip;

/* train_gui.cpp */
CommandCallback CcBuildWagon;
CommandCallback CcBuildLoco;

/* town_gui.cpp */
CommandCallback CcFoundTown;
CommandCallback CcFoundRandomTown;

/* group_gui.cpp */
CommandCallback CcCreateGroup;

/* ai/ai_core.cpp */
CommandCallback CcAI;

CommandCallback * const _callback_table[] = {
	/* 0x00 */ NULL,
	/* 0x01 */ CcBuildAircraft,
	/* 0x02 */ CcBuildAirport,
	/* 0x03 */ CcBuildBridge,
	/* 0x04 */ CcBuildCanal,
	/* 0x05 */ CcBuildDocks,
	/* 0x06 */ CcBuildLoco,
	/* 0x07 */ CcBuildRoadVeh,
	/* 0x08 */ CcBuildShip,
	/* 0x09 */ CcFoundTown,
	/* 0x0A */ CcBuildRoadTunnel,
	/* 0x0B */ CcBuildRailTunnel,
	/* 0x0C */ CcBuildWagon,
	/* 0x0D */ CcRoadDepot,
	/* 0x0E */ CcRailDepot,
	/* 0x0F */ CcPlaceSign,
	/* 0x10 */ CcPlaySound10,
	/* 0x11 */ CcPlaySound1D,
	/* 0x12 */ CcPlaySound1E,
	/* 0x13 */ CcStation,
	/* 0x14 */ CcTerraform,
	/* 0x15 */ CcAI,
	/* 0x16 */ CcCloneVehicle,
	/* 0x17 */ CcGiveMoney,
	/* 0x18 */ CcCreateGroup,
	/* 0x19 */ CcFoundRandomTown,
};

const int _callback_table_count = lengthof(_callback_table);
