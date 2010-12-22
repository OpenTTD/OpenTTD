/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_node_road.hpp Node tailored for road pathfinding. */

#ifndef YAPF_NODE_ROAD_HPP
#define YAPF_NODE_ROAD_HPP

/** Yapf Node for road YAPF */
template <class Tkey_>
struct CYapfRoadNodeT
	: CYapfNodeT<Tkey_, CYapfRoadNodeT<Tkey_> >
{
	typedef CYapfNodeT<Tkey_, CYapfRoadNodeT<Tkey_> > base;

	TileIndex       m_segment_last_tile;
	Trackdir        m_segment_last_td;

	void Set(CYapfRoadNodeT *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		base::Set(parent, tile, td, is_choice);
		m_segment_last_tile = tile;
		m_segment_last_td = td;
	}
};

/* now define two major node types (that differ by key type) */
typedef CYapfRoadNodeT<CYapfNodeKeyExitDir>  CYapfRoadNodeExitDir;
typedef CYapfRoadNodeT<CYapfNodeKeyTrackDir> CYapfRoadNodeTrackDir;

/* Default NodeList types */
typedef CNodeList_HashTableT<CYapfRoadNodeExitDir , 8, 12> CRoadNodeListExitDir;
typedef CNodeList_HashTableT<CYapfRoadNodeTrackDir, 10, 14> CRoadNodeListTrackDir;

#endif /* YAPF_NODE_ROAD_HPP */
