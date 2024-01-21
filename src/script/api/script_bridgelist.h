/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_bridgelist.h List all the bridges. */

#ifndef SCRIPT_BRIDGELIST_HPP
#define SCRIPT_BRIDGELIST_HPP

#include "script_list.h"

/**
 * Create a list of bridges types.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptBridgeList : public ScriptList {
public:
	ScriptBridgeList();
};

/**
 * Create a list of bridges types that can be built on a specific length.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptBridgeList_Length : public ScriptList {
public:
	/**
	 * @param length The length of the bridge you want to build.
	 */
	ScriptBridgeList_Length(SQInteger length);
};

#endif /* SCRIPT_BRIDGELIST_HPP */
