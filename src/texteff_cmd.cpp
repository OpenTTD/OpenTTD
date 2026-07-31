/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texteff_cmd.cpp Command handling for text effects. */

#include "stdafx.h"
#include "command_func.h"
#include "texteff.hpp"
#include "landscape.h"
#include "texteff_cmd.h"

#include "safeguards.h"

/**
 * Create a GameScript text effect at the specified world location.
 * @param flags Operation to perform.
 * @param x World X coordinate.
 * @param y World Y coordinate.
 * @param mode Animation mode to use.
 * @param colour Colour of the text.
 * @param text Text to display.
 * @return The cost of this operation and the stable text effect ID.
 */
std::tuple<CommandCost, ScriptTextEffectID> CmdCreateTextEffect(DoCommandFlags flags, int32_t x, int32_t y, TextEffectMode mode, Colours colour, const EncodedString &text)
{
	if (!ScriptTextEffectData::CanAllocateItem()) return { CMD_ERROR, INVALID_SCRIPT_TEXT_EFFECT_ID };
	if (text.empty()) return { CMD_ERROR, INVALID_SCRIPT_TEXT_EFFECT_ID };
	if (mode != TextEffectMode::Rising && mode != TextEffectMode::Static) return { CMD_ERROR, INVALID_SCRIPT_TEXT_EFFECT_ID };
	if (colour >= Colours::End) return { CMD_ERROR, INVALID_SCRIPT_TEXT_EFFECT_ID };
	if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= Map::SizeX() * TILE_SIZE || static_cast<uint32_t>(y) >= Map::SizeY() * TILE_SIZE) {
		return { CMD_ERROR, INVALID_SCRIPT_TEXT_EFFECT_ID };
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		uint8_t duration = mode == TextEffectMode::Rising ? Ticks::DAY_TICKS : 0;
		ScriptTextEffectData *effect = ScriptTextEffectData::Create(x, y, mode, duration, colour, text);
		effect->UpdateVirtCoord();
		return { CommandCost(), effect->index };
	}

	return { CommandCost(), INVALID_SCRIPT_TEXT_EFFECT_ID };
}

/**
 * Update a GameScript text effect.
 * @param flags Operation to perform.
 * @param effect_id Stable ID of the text effect.
 * @param colour New colour of the text.
 * @param text New text to display.
 * @return The cost of this operation or an error.
 */
CommandCost CmdUpdateTextEffect(DoCommandFlags flags, ScriptTextEffectID effect_id, Colours colour, const EncodedString &text)
{
	if (effect_id == INVALID_SCRIPT_TEXT_EFFECT_ID || text.empty() || colour >= Colours::End) return CMD_ERROR;
	if (!ScriptTextEffectData::IsValidID(effect_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		ScriptTextEffectData *effect = ScriptTextEffectData::Get(effect_id);
		effect->sign.MarkDirty();
		effect->msg = text;
		effect->colour = colour;
		effect->UpdateVirtCoord();
	}
	return CommandCost();
}

/**
 * Remove a GameScript text effect.
 * @param flags Operation to perform.
 * @param effect_id Stable ID of the text effect.
 * @return The cost of this operation or an error.
 */
CommandCost CmdRemoveTextEffect(DoCommandFlags flags, ScriptTextEffectID effect_id)
{
	if (effect_id == INVALID_SCRIPT_TEXT_EFFECT_ID) return CMD_ERROR;
	if (!ScriptTextEffectData::IsValidID(effect_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) delete ScriptTextEffectData::Get(effect_id);
	return CommandCost();
}
