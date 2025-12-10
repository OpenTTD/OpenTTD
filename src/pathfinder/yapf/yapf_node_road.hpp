/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf_node_road.hpp Node tailored for road pathfinding. */

#ifndef YAPF_NODE_ROAD_HPP
#define YAPF_NODE_ROAD_HPP

#include "../../tile_type.h"
#include "../../track_type.h"
#include "nodelist.hpp"
#include "yapf_node.hpp"

/** Yapf Node for road YAPF */
struct CYapfRoadNode : CYapfNodeT<CYapfNodeKeyExitDir, CYapfRoadNode> {
	typedef CYapfNodeT<CYapfNodeKeyExitDir, CYapfRoadNode> base;

	TileIndex segment_last_tile;
	Trackdir segment_last_td;

	void Set(CYapfRoadNode *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		this->base::Set(parent, tile, td, is_choice);
		this->segment_last_tile = tile;
		this->segment_last_td = td;
	}
};

typedef NodeList<CYapfRoadNode, 8, 10> CRoadNodeList;

#endif /* YAPF_NODE_ROAD_HPP */
