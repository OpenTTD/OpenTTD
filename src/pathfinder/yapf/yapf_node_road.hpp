/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_node_road.hpp Node tailored for road pathfinding. */

#ifndef YAPF_NODE_ROAD_HPP
#define YAPF_NODE_ROAD_HPP

#include "../../tile_type.h"
#include "../../track_type.h"
#include "nodelist.hpp"
#include "yapf_node.hpp"

/** Yapf Node for road YAPF */
template <class Tkey_>
struct CYapfRoadNodeT : CYapfNodeT<Tkey_, CYapfRoadNodeT<Tkey_> > {
	typedef CYapfNodeT<Tkey_, CYapfRoadNodeT<Tkey_> > base;

	TileIndex segment_last_tile;
	Trackdir segment_last_td;

	void Set(CYapfRoadNodeT *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		this->base::Set(parent, tile, td, is_choice);
		this->segment_last_tile = tile;
		this->segment_last_td = td;
	}
};

/* now define two major node types (that differ by key type) */
typedef CYapfRoadNodeT<CYapfNodeKeyExitDir>  CYapfRoadNodeExitDir;
typedef CYapfRoadNodeT<CYapfNodeKeyTrackDir> CYapfRoadNodeTrackDir;

/* Default NodeList types */
typedef NodeList<CYapfRoadNodeExitDir , 8, 10> CRoadNodeListExitDir;
typedef NodeList<CYapfRoadNodeTrackDir, 8, 10> CRoadNodeListTrackDir;

#endif /* YAPF_NODE_ROAD_HPP */
