/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_text_effect.hpp Everything to display animated text in the game world. */

#ifndef SCRIPT_TEXT_EFFECT_HPP
#define SCRIPT_TEXT_EFFECT_HPP

#include "script_object.hpp"
#include "script_company.hpp"
#include "script_text.hpp"
#include "../../texteff.hpp"

/**
 * Class that handles text effect display in the game world.
 *
 * Text effects are saved and loaded. Static effects remain until explicitly
 * removed; rising effects disappear after their animation finishes.
 * @api game
 */
class ScriptTextEffect : public ScriptObject {
public:
	static constexpr ScriptTextEffectID TEXT_EFFECT_INVALID = ScriptTextEffectID::Invalid(); ///< An invalid text effect ID.

	/**
	 * Text effect animation modes.
	 */
	enum TextEffectMode : uint8_t {
		TE_RISING = to_underlying(::TextEffectMode::Rising), ///< Text slowly rises upwards and then disappears.
		TE_STATIC = to_underlying(::TextEffectMode::Static), ///< Text stays in place until removed.
	};

	/**
	 * Check whether a text effect exists.
	 * @param effect_id The text effect ID to check.
	 * @return True if and only if this text effect currently exists.
	 */
	static bool IsValidTextEffect(ScriptTextEffectID effect_id);

	/**
	 * Create text at the centre of a tile.
	 * @param tile The tile where the text is shown.
	 * @param text The text to display (a raw string or ScriptText object).
	 * @param mode The animation mode to use.
	 * @param colour The text colour.
	 * @return The new text effect ID, or #TEXT_EFFECT_INVALID if creation failed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre text != null && len(text) != 0.
	 * @pre mode is TE_RISING or TE_STATIC.
	 * @pre colour is a valid ScriptCompany::Colours value.
	 */
	static ScriptTextEffectID Create(TileIndex tile, Text *text, TextEffectMode mode, ScriptCompany::Colours colour);

	/**
	 * Create text at a world position.
	 * @param x X coordinate in the game world.
	 * @param y Y coordinate in the game world.
	 * @param text The text to display (a raw string or ScriptText object).
	 * @param mode The animation mode to use.
	 * @param colour The text colour.
	 * @return The new text effect ID, or #TEXT_EFFECT_INVALID if creation failed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre x and y identify a position within the map.
	 * @pre text != null && len(text) != 0.
	 * @pre mode is TE_RISING or TE_STATIC.
	 * @pre colour is a valid ScriptCompany::Colours value.
	 */
	static ScriptTextEffectID CreateAtPosition(SQInteger x, SQInteger y, Text *text, TextEffectMode mode, ScriptCompany::Colours colour);

	/**
	 * Update a text effect.
	 * @param effect_id Text effect ID.
	 * @param text The new text (a raw string or ScriptText object).
	 * @param colour The new text colour.
	 * @return True if the text effect was updated.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidTextEffect(effect_id).
	 * @pre text != null && len(text) != 0.
	 * @pre colour is a valid ScriptCompany::Colours value.
	 */
	static bool Update(ScriptTextEffectID effect_id, Text *text, ScriptCompany::Colours colour);

	/**
	 * Remove a text effect.
	 * @param effect_id Text effect ID.
	 * @return True if the text effect was removed.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre IsValidTextEffect(effect_id).
	 */
	static bool Remove(ScriptTextEffectID effect_id);
};

#endif /* SCRIPT_TEXT_EFFECT_HPP */
