/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signs_cmd.h Command definitions related to signs. */

#ifndef SIGNS_CMD_H
#define SIGNS_CMD_H

#include "command_type.h"
#include "signs_type.h"

std::tuple<CommandCost, SignID> CmdPlaceSign(DoCommandFlags flags, TileIndex tile, const std::string &text);
CommandCost CmdRenameSign(DoCommandFlags flags, SignID sign_id, const std::string &text);
CommandCost CmdMoveSign(DoCommandFlags flags, SignID sign_id, TileIndex tile);

DEF_CMD_TRAIT(CMD_PLACE_SIGN,  CmdPlaceSign,  CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_RENAME_SIGN, CmdRenameSign, CommandFlag::Deity, CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_MOVE_SIGN, CmdMoveSign, CommandFlag::Deity, CommandType::OtherManagement)

void CcPlaceSign(Commands cmd, const CommandCost &result, SignID new_sign);

#endif /* SIGNS_CMD_H */
