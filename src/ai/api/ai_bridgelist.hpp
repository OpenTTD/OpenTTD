/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_bridgelist.hpp List all the bridges. */

#ifndef AI_BRIDGELIST_HPP
#define AI_BRIDGELIST_HPP

#include "ai_list.hpp"

/**
 * Create a list of bridges.
 * @ingroup AIList
 */
class AIBridgeList : public AIList {
public:
	AIBridgeList();
};

/**
 * Create a list of bridges that can be built on a specific length.
 * @ingroup AIList
 */
class AIBridgeList_Length : public AIList {
public:
	/**
	 * @param length The length of the bridge you want to build.
	 */
	AIBridgeList_Length(uint length);
};

#endif /* AI_BRIDGELIST_HPP */
