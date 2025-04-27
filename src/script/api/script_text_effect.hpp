/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_text_effect.hpp Everything to display rising text in the game world. */

#ifndef SCRIPT_TEXT_EFFECT_HPP
#define SCRIPT_TEXT_EFFECT_HPP

#include "script_object.hpp"
#include "script_company.hpp"
#include "script_text.hpp"

/**
 * Class that handles rising text in the game world.
 *
 * The text slowly rises upwards and disappears on its own, like the
 * income and cost animations.
 * @api game
 */
class ScriptTextEffect : public ScriptObject {
public:
	/**
	 * Show rising text at the centre of a tile.
	 * @param tile The tile where the text is shown.
	 * @param text The text to display (a raw string or ScriptText object).
	 * @param colour The text colour.
	 * @return True if the text effect was shown.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre ScriptMap::IsValidTile(tile).
	 * @pre text != null && len(text) != 0.
	 * @pre colour is a valid ScriptCompany::Colours value.
	 */
	static bool Create(TileIndex tile, Text *text, ScriptCompany::Colours colour);

	/**
	 * Show rising text at a position in the game world.
	 * @param x X coordinate in the game world.
	 * @param y Y coordinate in the game world.
	 * @param text The text to display (a raw string or ScriptText object).
	 * @param colour The text colour.
	 * @return True if the text effect was shown.
	 * @pre ScriptCompanyMode::IsDeity().
	 * @pre x and y identify a position within the map.
	 * @pre text != null && len(text) != 0.
	 * @pre colour is a valid ScriptCompany::Colours value.
	 */
	static bool CreateAtPosition(SQInteger x, SQInteger y, Text *text, ScriptCompany::Colours colour);
};

#endif /* SCRIPT_TEXT_EFFECT_HPP */
