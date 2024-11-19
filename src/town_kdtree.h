/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_kdtree.h Declarations for accessing the k-d tree of towns */

#ifndef TOWN_KDTREE_H
#define TOWN_KDTREE_H

#include "core/kdtree.hpp"
#include "town.h"

struct Kdtree_TownXYFunc {
	inline uint16_t operator()(TownID tid, int dim)
	{
		return (dim == 0) ? TileX(Town::Get(tid)->xy) : TileY(Town::Get(tid)->xy);
	}
};

using TownKdtree = Kdtree<TownID, Kdtree_TownXYFunc, uint16_t, int>;
extern TownKdtree _town_kdtree;
extern TownKdtree _town_local_authority_kdtree;

#endif
