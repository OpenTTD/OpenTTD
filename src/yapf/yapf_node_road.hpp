/* $Id$ */

/** @file yapf_node_road.hpp Node tailored for road pathfinding. */

#ifndef  YAPF_NODE_ROAD_HPP
#define  YAPF_NODE_ROAD_HPP



/** Yapf Node for road YAPF */
template <class Tkey_>
struct CYapfRoadNodeT
	: CYapfNodeT<Tkey_, CYapfRoadNodeT<Tkey_> >
{
	typedef CYapfNodeT<Tkey_, CYapfRoadNodeT<Tkey_> > base;

	TileIndex       m_segment_last_tile;
	Trackdir        m_segment_last_td;

	void Set(CYapfRoadNodeT* parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		base::Set(parent, tile, td, is_choice);
		m_segment_last_tile = tile;
		m_segment_last_td = td;
	}
};

// now define two major node types (that differ by key type)
typedef CYapfRoadNodeT<CYapfNodeKeyExitDir>  CYapfRoadNodeExitDir;
typedef CYapfRoadNodeT<CYapfNodeKeyTrackDir> CYapfRoadNodeTrackDir;

// Default NodeList types
typedef CNodeList_HashTableT<CYapfRoadNodeExitDir , 8, 12> CRoadNodeListExitDir;
typedef CNodeList_HashTableT<CYapfRoadNodeTrackDir, 10, 14> CRoadNodeListTrackDir;



#endif /* YAPF_NODE_ROAD_HPP */
