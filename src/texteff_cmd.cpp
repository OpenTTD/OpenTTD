/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file texteff_cmd.cpp Command handling for text effects */

#include "stdafx.h"
#include "command_func.h"
#include "texteff.hpp"
#include "strings_func.h"
#include "tile_map.h"
#include "texteff_cmd.h"

#include "table/strings.h"

#include "safeguards.h"
#include "landscape.h"

/**
 * Show a text effect at the specified location.
 * @param flags operation to perform
 * @param x X coordinate in the game
 * @param y Y coordinate in the game
 * @param mode The animation mode to use
 * @param text The text to display
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, TextEffectID> CmdCreateTextEffect(DoCommandFlags flags, int32_t x, int32_t y, TextEffectMode mode, const EncodedString &text)
{
    if (text.empty()) return { CMD_ERROR, INVALID_TE_ID };
    if (mode != TE_RISING && mode != TE_STATIC) return { CMD_ERROR, INVALID_TE_ID };

    if (flags.Test(DoCommandFlag::Execute)) {        
        Point pt = RemapCoords2(x, y);
        EncodedString encoded_text = text;
        TextEffectID te_id;

        if (mode == TE_RISING) {
            te_id = AddTextEffect(std::move(encoded_text), pt.x, pt.y, Ticks::DAY_TICKS, TE_RISING);
        } else {
            te_id = AddTextEffect(std::move(encoded_text), pt.x, pt.y, 0, TE_STATIC);
        }
        
        return { CommandCost(), te_id };
    }

    return { CommandCost(), INVALID_TE_ID };
}

CommandCost CmdUpdateTextEffect(DoCommandFlags flags, TextEffectID te_id, const EncodedString &text)
{
    if (te_id == INVALID_TE_ID) return CMD_ERROR;
    if (text.empty()) return CMD_ERROR;

    if (flags.Test(DoCommandFlag::Execute)) {        
        EncodedString encoded_text = text;
        UpdateTextEffect(te_id, std::move(encoded_text));
    }

    return CommandCost();
}

CommandCost CmdRemoveTextEffect(DoCommandFlags flags, TextEffectID te_id)
{
    if (te_id == INVALID_TE_ID) return CMD_ERROR;

    if (flags.Test(DoCommandFlag::Execute)) {        
        RemoveTextEffect(te_id);
    }

    return CommandCost();
}
