/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_townlist.hpp List all the towns. */

#ifndef SCRIPT_TOWNLIST_HPP
#define SCRIPT_TOWNLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of towns that are currently on the map.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTownList : public ScriptList {
public:
	ScriptTownList();
};

/**
 * Creates a list of all TownEffects known in the game.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTownEffectList : public ScriptList {
public:
	ScriptTownEffectList();
};

#endif /* SCRIPT_TOWNLIST_HPP */
