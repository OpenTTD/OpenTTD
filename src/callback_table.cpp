/* $Id$ */

/** @file callback_table.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "callback_table.h"
#include "functions.h"

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
CommandCallback CcBuildTown;
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

CommandCallback CcAI;

CommandCallback *_callback_table[] = {
	/* 0x00 */ NULL,
	/* 0x01 */ CcBuildAircraft,
	/* 0x02 */ CcBuildAirport,
	/* 0x03 */ CcBuildBridge,
	/* 0x04 */ CcBuildCanal,
	/* 0x05 */ CcBuildDocks,
	/* 0x06 */ CcBuildLoco,
	/* 0x07 */ CcBuildRoadVeh,
	/* 0x08 */ CcBuildShip,
	/* 0x09 */ CcBuildTown,
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
};

const int _callback_table_count = lengthof(_callback_table);
