/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_viewport.hpp Everything to manipulate the users viewport. */

#ifndef SCRIPT_VIEWPORT_HPP
#define SCRIPT_VIEWPORT_HPP

#include "script_object.hpp"

/**
 * Class that manipultes the users viewport.
 * @api game
 */
class ScriptViewport : public ScriptObject {
public:
	/**
	 * Scroll the viewport to the given tile, where the tile will be in the
	 *  center of the screen.
	 * @param tile The tile to put in the center of the screen.
	 * @pre !ScriptGame::IsMultiplayer().
	 * @pre ScriptMap::IsValidTile(tile).
	 */
	static void ScrollTo(TileIndex tile);
};

#endif /* SCRIPT_ADMIN_HPP */
