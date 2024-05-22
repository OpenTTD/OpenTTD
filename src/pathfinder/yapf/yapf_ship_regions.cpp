/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file yapf_ship_regions.cpp Implementation of YAPF for water regions, which are used for finding intermediate ship destinations. */

#include "../../stdafx.h"
#include "../../ship.h"

#include "yapf.hpp"
#include "yapf_ship_regions.h"
#include "../water_regions.h"

#include "../../safeguards.h"

constexpr int DIRECT_NEIGHBOR_COST = 100;
constexpr int NODES_PER_REGION = 4;
constexpr int MAX_NUMBER_OF_NODES = 65536;

/** Yapf Node Key that represents a single patch of interconnected water within a water region. */
struct CYapfRegionPatchNodeKey {
	WaterRegionPatchDesc m_water_region_patch;

	inline void Set(const WaterRegionPatchDesc &water_region_patch)
	{
		m_water_region_patch = water_region_patch;
	}

	inline int CalcHash() const { return CalculateWaterRegionPatchHash(m_water_region_patch); }
	inline bool operator==(const CYapfRegionPatchNodeKey &other) const { return CalcHash() == other.CalcHash(); }
};

inline uint ManhattanDistance(const CYapfRegionPatchNodeKey &a, const CYapfRegionPatchNodeKey &b)
{
	return (std::abs(a.m_water_region_patch.x - b.m_water_region_patch.x) + std::abs(a.m_water_region_patch.y - b.m_water_region_patch.y)) * DIRECT_NEIGHBOR_COST;
}

/** Yapf Node for water regions. */
template <class Tkey_>
struct CYapfRegionNodeT {
	typedef Tkey_ Key;
	typedef CYapfRegionNodeT<Tkey_> Node;

	Tkey_       m_key;
	Node       *m_hash_next;
	Node       *m_parent;
	int         m_cost;
	int         m_estimate;

	inline void Set(Node *parent, const WaterRegionPatchDesc &water_region_patch)
	{
		m_key.Set(water_region_patch);
		m_hash_next = nullptr;
		m_parent = parent;
		m_cost = 0;
		m_estimate = 0;
	}

	inline void Set(Node *parent, const Key &key)
	{
		Set(parent, key.m_water_region_patch);
	}

	DiagDirection GetDiagDirFromParent() const
	{
		if (!m_parent) return INVALID_DIAGDIR;
		const int dx = m_key.m_water_region_patch.x - m_parent->m_key.m_water_region_patch.x;
		const int dy = m_key.m_water_region_patch.y - m_parent->m_key.m_water_region_patch.y;
		if (dx > 0 && dy == 0) return DIAGDIR_SW;
		if (dx < 0 && dy == 0) return DIAGDIR_NE;
		if (dx == 0 && dy > 0) return DIAGDIR_SE;
		if (dx == 0 && dy < 0) return DIAGDIR_NW;
		return INVALID_DIAGDIR;
	}

	inline Node *GetHashNext() { return m_hash_next; }
	inline void SetHashNext(Node *pNext) { m_hash_next = pNext; }
	inline const Tkey_ &GetKey() const { return m_key; }
	inline int GetCost() { return m_cost; }
	inline int GetCostEstimate() { return m_estimate; }
	inline bool operator<(const Node &other) const { return m_estimate < other.m_estimate; }
};

/** YAPF origin for water regions. */
template <class Types>
class CYapfOriginRegionT
{
public:
	typedef typename Types::Tpf Tpf;              ///< The pathfinder class (derived from THIS class).
	typedef typename Types::NodeList::Titem Node; ///< This will be our node type.
	typedef typename Node::Key Key;               ///< Key to hash tables.

protected:
	inline Tpf &Yapf() { return *static_cast<Tpf*>(this); }

private:
	std::vector<CYapfRegionPatchNodeKey> m_origin_keys;

public:
	void AddOrigin(const WaterRegionPatchDesc &water_region_patch)
	{
		if (water_region_patch.label == INVALID_WATER_REGION_PATCH) return;
		if (!HasOrigin(water_region_patch)) m_origin_keys.push_back(CYapfRegionPatchNodeKey{ water_region_patch });
	}

	bool HasOrigin(const WaterRegionPatchDesc &water_region_patch)
	{
		return std::find(m_origin_keys.begin(), m_origin_keys.end(), CYapfRegionPatchNodeKey{ water_region_patch }) != m_origin_keys.end();
	}

	void PfSetStartupNodes()
	{
		for (const CYapfRegionPatchNodeKey &origin_key : m_origin_keys) {
			Node &node = Yapf().CreateNewNode();
			node.Set(nullptr, origin_key);
			Yapf().AddStartupNode(node);
		}
	}
};

/** YAPF destination provider for water regions. */
template <class Types>
class CYapfDestinationRegionT
{
public:
	typedef typename Types::Tpf Tpf;              ///< The pathfinder class (derived from THIS class).
	typedef typename Types::NodeList::Titem Node; ///< This will be our node type.
	typedef typename Node::Key Key;               ///< Key to hash tables.

protected:
	Key m_dest;

public:
	void SetDestination(const WaterRegionPatchDesc &water_region_patch)
	{
		m_dest.Set(water_region_patch);
	}

protected:
	Tpf &Yapf() { return *static_cast<Tpf*>(this); }

public:
	inline bool PfDetectDestination(Node &n) const
	{
		return n.m_key == m_dest;
	}

	inline bool PfCalcEstimate(Node &n)
	{
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		n.m_estimate = n.m_cost + ManhattanDistance(n.m_key, m_dest);

		return true;
	}
};

/** YAPF node following for water region pathfinding. */
template <class Types>
class CYapfFollowRegionT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< The pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< This will be our node type.
	typedef typename Node::Key Key;                      ///< Key to hash tables.

protected:
	inline Tpf &Yapf() { return *static_cast<Tpf*>(this); }

public:
	inline void PfFollowNode(Node &old_node)
	{
		TVisitWaterRegionPatchCallBack visitFunc = [&](const WaterRegionPatchDesc &water_region_patch)
		{
			Node &node = Yapf().CreateNewNode();
			node.Set(&old_node, water_region_patch);
			Yapf().AddNewNode(node, TrackFollower{});
		};
		VisitWaterRegionPatchNeighbors(old_node.m_key.m_water_region_patch, visitFunc);
	}

	inline char TransportTypeChar() const { return '^'; }

	static std::vector<WaterRegionPatchDesc> FindWaterRegionPath(const Ship *v, TileIndex start_tile, int max_returned_path_length)
	{
		const WaterRegionPatchDesc start_water_region_patch = GetWaterRegionPatchInfo(start_tile);

		/* We reserve 4 nodes (patches) per water region. The vast majority of water regions have 1 or 2 regions so this should be a pretty
		 * safe limit. We cap the limit at 65536 which is at a region size of 16x16 is equivalent to one node per region for a 4096x4096 map. */
		Tpf pf(std::min(static_cast<int>(Map::Size() * NODES_PER_REGION) / WATER_REGION_NUMBER_OF_TILES, MAX_NUMBER_OF_NODES));
		pf.SetDestination(start_water_region_patch);

		if (v->current_order.IsType(OT_GOTO_STATION)) {
			DestinationID station_id = v->current_order.GetDestination();
			const BaseStation *station = BaseStation::Get(station_id);
			TileArea tile_area;
			station->GetTileArea(&tile_area, STATION_DOCK);
			for (const auto &tile : tile_area) {
				if (IsDockingTile(tile) && IsShipDestinationTile(tile, station_id)) {
					pf.AddOrigin(GetWaterRegionPatchInfo(tile));
				}
			}
		} else {
			TileIndex tile = v->dest_tile;
			pf.AddOrigin(GetWaterRegionPatchInfo(tile));
		}

		/* If origin and destination are the same we simply return that water patch. */
		std::vector<WaterRegionPatchDesc> path = { start_water_region_patch };
		path.reserve(max_returned_path_length);
		if (pf.HasOrigin(start_water_region_patch)) return path;

		/* Find best path. */
		if (!pf.FindPath(v)) return {}; // Path not found.

		Node *node = pf.GetBestNode();
		for (int i = 0; i < max_returned_path_length - 1; ++i) {
			if (node != nullptr) {
				node = node->m_parent;
				if (node != nullptr) path.push_back(node->m_key.m_water_region_patch);
			}
		}

		assert(!path.empty());
		return path;
	}
};

/** Cost Provider of YAPF for water regions. */
template <class Types>
class CYapfCostRegionT
{
public:
	typedef typename Types::Tpf Tpf;              ///< The pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< This will be our node type.
	typedef typename Node::Key Key;               ///< Key to hash tables.

protected:
	/** To access inherited path finder. */
	Tpf &Yapf() { return *static_cast<Tpf*>(this); }

public:
	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 * Calculates only the cost of given node, adds it to the parent node cost
	 * and stores the result into Node::m_cost member.
	 */
	inline bool PfCalcCost(Node &n, const TrackFollower *)
	{
		n.m_cost = n.m_parent->m_cost + ManhattanDistance(n.m_key, n.m_parent->m_key);

		/* Incentivise zigzagging by adding a slight penalty when the search continues in the same direction. */
		Node *grandparent = n.m_parent->m_parent;
		if (grandparent != nullptr) {
			const DiagDirDiff dir_diff = DiagDirDifference(n.m_parent->GetDiagDirFromParent(), n.GetDiagDirFromParent());
			if (dir_diff != DIAGDIRDIFF_90LEFT && dir_diff != DIAGDIRDIFF_90RIGHT) n.m_cost += 1;
		}

		return true;
	}
};

/* We don't need a follower but YAPF requires one. */
struct DummyFollower : public CFollowTrackWater {};

/**
 * Config struct of YAPF for route planning.
 * Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Tnode_list>
struct CYapfRegion_TypesT
{
	typedef CYapfRegion_TypesT<Tpf_, Tnode_list> Types;         ///< Shortcut for this struct type.
	typedef Tpf_                                 Tpf;           ///< Pathfinder type.
	typedef DummyFollower                        TrackFollower; ///< Track follower helper class
	typedef Tnode_list                           NodeList;
	typedef Ship                                 VehicleType;

	/** Pathfinder components (modules). */
	typedef CYapfBaseT<Types>                 PfBase;        ///< Base pathfinder class.
	typedef CYapfFollowRegionT<Types>         PfFollow;      ///< Node follower.
	typedef CYapfOriginRegionT<Types>         PfOrigin;      ///< Origin provider.
	typedef CYapfDestinationRegionT<Types>    PfDestination; ///< Destination/distance provider.
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       ///< Segment cost cache provider.
	typedef CYapfCostRegionT<Types>           PfCost;        ///< Cost provider.
};

typedef CNodeList_HashTableT<CYapfRegionNodeT<CYapfRegionPatchNodeKey>, 12, 12> CRegionNodeListWater;

struct CYapfRegionWater : CYapfT<CYapfRegion_TypesT<CYapfRegionWater, CRegionNodeListWater>>
{
	explicit CYapfRegionWater(int max_nodes) { m_max_search_nodes = max_nodes; }
};

/**
 * Finds a path at the water region level. Note that the starting region is always included if the path was found.
 * @param v The ship to find a path for.
 * @param start_tile The tile to start searching from.
 * @param max_returned_path_length The maximum length of the path that will be returned.
 * @returns A path of water region patches, or an empty vector if no path was found.
 */
std::vector<WaterRegionPatchDesc> YapfShipFindWaterRegionPath(const Ship *v, TileIndex start_tile, int max_returned_path_length)
{
	return CYapfRegionWater::FindWaterRegionPath(v, start_tile, max_returned_path_length);
}
