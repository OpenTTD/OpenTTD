/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_cmd.h Command definitions related to objects. */

#ifndef OBJECT_CMD_H
#define OBJECT_CMD_H

#include "command_type.h"
#include "object_type.h"

CommandCost CmdBuildObject(DoCommandFlags flags, TileIndex tile, ObjectType type, uint8_t view);
CommandCost CmdBuildObjectArea(DoCommandFlags flags, TileIndex tile, TileIndex start_tile, ObjectType type, uint8_t view, bool diagonal);

template <> struct CommandTraits<CMD_BUILD_OBJECT> : DefaultCommandTraits<CMD_BUILD_OBJECT, "CmdBuildObject", CmdBuildObject, CMD_DEITY | CMD_NO_WATER | CMD_AUTO, CMDT_LANDSCAPE_CONSTRUCTION> {};
template <> struct CommandTraits<CMD_BUILD_OBJECT_AREA> : DefaultCommandTraits<CMD_BUILD_OBJECT_AREA, "CmdBuildObjectArea", CmdBuildObjectArea, CMD_DEITY | CMD_NO_WATER | CMD_NO_TEST | CMD_AUTO, CMDT_LANDSCAPE_CONSTRUCTION> {};

#endif /* OBJECT_CMD_H */
