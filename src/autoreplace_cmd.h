/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_cmd.h Command definitions related to autoreplace. */

#ifndef AUTOREPLACE_CMD_H
#define AUTOREPLACE_CMD_H

#include "command_type.h"

CommandProc CmdAutoreplaceVehicle;
CommandProc CmdSetAutoReplace;

DEF_CMD_TRAIT(CMD_AUTOREPLACE_VEHICLE, CmdAutoreplaceVehicle, 0, CMDT_VEHICLE_MANAGEMENT)
DEF_CMD_TRAIT(CMD_SET_AUTOREPLACE,     CmdSetAutoReplace,     0, CMDT_VEHICLE_MANAGEMENT)

#endif /* AUTOREPLACE_CMD_H */
