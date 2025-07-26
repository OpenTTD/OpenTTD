/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_text_effect.hpp Everything to display animated text in the game world. */

#ifndef SCRIPT_TEXT_EFFECT_HPP
#define SCRIPT_TEXT_EFFECT_HPP

#include "script_object.hpp"
#include "script_text.hpp"
#include "../../texteff.hpp"

/**
 * Class that handles text effect display in the game world.
 * @api game
 */
class ScriptTextEffect : public ScriptObject {
public:
    /**
     * Text effect animation modes.
     */
    enum ScriptTextEffectMode {
        TE_RISING = ::TE_RISING, ///< Text slowly rises upwards
        TE_STATIC = ::TE_STATIC, ///< Text stays in place
    };

    /**
     * Create animated text at a tile location.
     * @param tile The tile where to show the text.
     * @param text The text to display.
     * @param mode The animation mode to use.
     * @return True if the text effect was created successfully.
     */
    static TextEffectID Create(TileIndex tile, Text *text, ScriptTextEffectMode mode);

    /**
     * Create animated text at the specified location.
     * @param x X coordinate in the game world.
     * @param y Y coordinate in the game world.
     * @param text The text to display.
     * @param mode The animation mode to use.
     * @return True if the text effect was created successfully.
     */
    static TextEffectID CreateAtPosition(SQInteger x, SQInteger y, Text *text, ScriptTextEffectMode mode);
    
    /**
     * Update animated text
     * @param te_id Text effect ID.
     * @param text The text to update
     * @return True if the text effect was updated successfully
     */
    static bool Update(TextEffectID te_id, Text *text);

    /**
     * Update animated text
     * @param te_id Text effect ID.
     * @return True if the text effect was removed successfully
     */
    static bool Remove(TextEffectID te_id);
};

#endif /* SCRIPT_TEXT_EFFECT_HPP */
