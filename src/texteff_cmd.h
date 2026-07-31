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
#include "texteff.hpp"

std::tuple<CommandCost, ScriptTextEffectID> CmdCreateTextEffect(DoCommandFlags flags, int32_t x, int32_t y, TextEffectMode mode, Colours colour, const EncodedString &text);
CommandCost CmdUpdateTextEffect(DoCommandFlags flags, ScriptTextEffectID effect_id, Colours colour, const EncodedString &text);
CommandCost CmdRemoveTextEffect(DoCommandFlags flags, ScriptTextEffectID effect_id);

DEF_CMD_TRAIT(Commands::CreateTextEffect, CmdCreateTextEffect, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::UpdateTextEffect, CmdUpdateTextEffect, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::RemoveTextEffect, CmdRemoveTextEffect, CommandFlag::Deity, CommandType::OtherManagement)

#endif /* TEXTEFF_CMD_H */
