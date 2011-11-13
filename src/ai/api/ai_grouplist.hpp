/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_grouplist.hpp List all the groups (you own). */

#ifndef AI_GROUPLIST_HPP
#define AI_GROUPLIST_HPP

#include "ai_list.hpp"

/**
 * Creates a list of groups of which you are the owner.
 * @note Neither AIGroup::GROUP_ALL nor AIGroup::GROUP_DEFAULT is in this list.
 * @ingroup AIList
 */
class AIGroupList : public AIList {
public:
	AIGroupList();
};

#endif /* AI_GROUPLIST_HPP */
