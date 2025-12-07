/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file viewport_cmd.h Command definitions related to viewports. */

#ifndef VIEWPORT_CMD_H
#define VIEWPORT_CMD_H

#include "command_type.h"
#include "viewport_type.h"

CommandCost CmdScrollViewport(DoCommandFlags flags, TileIndex tile, ViewportScrollTarget target, uint32_t ref);

DEF_CMD_TRAIT(CMD_SCROLL_VIEWPORT, CmdScrollViewport, CommandFlag::Deity, CommandType::OtherManagement)

#endif /* VIEWPORT_CMD_H */
