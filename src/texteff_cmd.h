/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file texteff_cmd.h Command declarations for text effects */

#ifndef TEXTEFF_CMD_H
#define TEXTEFF_CMD_H

#include "command_type.h"
#include "texteff.hpp"

std::tuple<CommandCost, TextEffectID> CmdCreateTextEffect(DoCommandFlags flags, int32_t x, int32_t y, TextEffectMode mode, const EncodedString &text);
CommandCost CmdUpdateTextEffect(DoCommandFlags flags, TextEffectID te_id, const EncodedString &text);
CommandCost CmdRemoveTextEffect(DoCommandFlags flags, TextEffectID te_id);

DEF_CMD_TRAIT(CMD_CREATE_TEXT_EFFECT, CmdCreateTextEffect, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_UPDATE_TEXT_EFFECT, CmdUpdateTextEffect, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CMDT_OTHER_MANAGEMENT)
DEF_CMD_TRAIT(CMD_REMOVE_TEXT_EFFECT, CmdRemoveTextEffect, CommandFlag::Deity, CMDT_OTHER_MANAGEMENT)

#endif /* TEXTEFF_CMD_H */
