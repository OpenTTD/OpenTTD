/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texteff_cmd.h Command declarations for text effects. */

#ifndef TEXTEFF_CMD_H
#define TEXTEFF_CMD_H

#include "command_type.h"
#include "gfx_type.h"
#include "strings_type.h"

CommandCost CmdCreateTextEffect(DoCommandFlags flags, int32_t x, int32_t y, Colours colour, const EncodedString &text);

DEF_CMD_TRAIT(Commands::CreateTextEffect, CmdCreateTextEffect, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)

#endif /* TEXTEFF_CMD_H */
