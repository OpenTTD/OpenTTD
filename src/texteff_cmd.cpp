/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file texteff_cmd.cpp Command handling for text effects. */

#include "stdafx.h"
#include "texteff_cmd.h"
#include "command_func.h"
#include "company_func.h"
#include "landscape.h"
#include "map_func.h"
#include "texteff.hpp"
#include "timer/timer_game_tick.h"

#include "safeguards.h"

/**
 * Show a rising text effect at a position in the game world.
 * The text effect is purely visual; it is not saved.
 * @param flags Operation to perform.
 * @param x World X coordinate.
 * @param y World Y coordinate.
 * @param colour Colour of the text.
 * @param text Text to display.
 * @return The cost of this operation or an error.
 */
CommandCost CmdCreateTextEffect(DoCommandFlags flags, int32_t x, int32_t y, Colours colour, const EncodedString &text)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;
	if (text.empty()) return CMD_ERROR;
	if (colour >= Colours::End) return CMD_ERROR;
	if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= Map::SizeX() * TILE_SIZE || static_cast<uint32_t>(y) >= Map::SizeY() * TILE_SIZE) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		Point pt = RemapCoords(x, y, GetSlopePixelZ(x, y));
		AddTextEffect(EncodedString{text}, pt.x, pt.y, Ticks::DAY_TICKS, TextEffectMode::Rising, colour);
	}

	return CommandCost();
}
