/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file roadveh_cmd.h Command definitions related to road vehicles. */

#ifndef ROADVEH_CMD_H
#define ROADVEH_CMD_H

#include "command_type.h"
#include "engine_type.h"
#include "vehicle_type.h"

CommandCost CmdBuildRoadVehicle(TileIndex tile, DoCommandFlag flags, const Engine *e, uint16 data, Vehicle **v);

CommandProc CmdTurnRoadVeh;

DEF_CMD_TRAIT(CMD_TURN_ROADVEH, CmdTurnRoadVeh, 0, CMDT_VEHICLE_MANAGEMENT)

#endif /* ROADVEH_CMD_H */
