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

constexpr int DIRECT_NEIGHBOUR_COST = 100;
constexpr int NODES_PER_REGION = 4;
constexpr int MAX_NUMBER_OF_NODES = 65536;

/** Yapf Node Key that represents a single patch of interconnected water within a water region. */
struct CYapfRegionPatchNodeKey {
	WaterRegionPatchDesc water_region_patch;

	inline void Set(const WaterRegionPatchDesc &water_region_patch)
	{
		this->water_region_patch = water_region_patch;
	}

	inline int CalcHash() const { return CalculateWaterRegionPatchHash(this->water_region_patch); }
	inline bool operator==(const CYapfRegionPatchNodeKey &other) const { return this->CalcHash() == other.CalcHash(); }
};

inline uint ManhattanDistance(const CYapfRegionPatchNodeKey &a, const CYapfRegionPatchNodeKey &b)
{
	return (std::abs(a.water_region_patch.x - b.water_region_patch.x) + std::abs(a.water_region_patch.y - b.water_region_patch.y)) * DIRECT_NEIGHBOUR_COST;
}

/** Yapf Node for water regions. */
template <class Tkey_>
struct CYapfRegionNodeT : CYapfNodeT<Tkey_, CYapfRegionNodeT<Tkey_> > {
	typedef Tkey_ Key;
	typedef CYapfRegionNodeT<Tkey_> Node;

	inline void Set(Node *parent, const WaterRegionPatchDesc &water_region_patch)
	{
		this->key.Set(water_region_patch);
		this->hash_next = nullptr;
		this->parent = parent;
		this->cost = 0;
		this->estimate = 0;
	}

	inline void Set(Node *parent, const Key &key)
	{
		this->Set(parent, key.water_region_patch);
	}

	DiagDirection GetDiagDirFromParent() const
	{
		if (this->parent == nullptr) return INVALID_DIAGDIR;
		const int dx = this->key.water_region_patch.x - this->parent->key.water_region_patch.x;
		const int dy = this->key.water_region_patch.y - this->parent->key.water_region_patch.y;
		if (dx > 0 && dy == 0) return DIAGDIR_SW;
		if (dx < 0 && dy == 0) return DIAGDIR_NE;
		if (dx == 0 && dy > 0) return DIAGDIR_SE;
		if (dx == 0 && dy < 0) return DIAGDIR_NW;
		return INVALID_DIAGDIR;
	}
};

/** YAPF origin for water regions. */
template <class Types>
class CYapfOriginRegionT
{
public:
	typedef typename Types::Tpf Tpf; ///< The pathfinder class (derived from THIS class).
	typedef typename Types::NodeList::Item Node; ///< This will be our node type.
	typedef typename Node::Key Key; ///< Key to hash tables.

protected:
	inline Tpf &Yapf() { return *static_cast<Tpf*>(this); }

private:
	std::vector<CYapfRegionPatchNodeKey> origin_keys;

public:
	void AddOrigin(const WaterRegionPatchDesc &water_region_patch)
	{
		if (water_region_patch.label == INVALID_WATER_REGION_PATCH) return;
		if (!HasOrigin(water_region_patch)) this->origin_keys.emplace_back(water_region_patch);
	}

	bool HasOrigin(const WaterRegionPatchDesc &water_region_patch)
	{
		return std::ranges::find(this->origin_keys, CYapfRegionPatchNodeKey{ water_region_patch }) != this->origin_keys.end();
	}

	void PfSetStartupNodes()
	{
		for (const CYapfRegionPatchNodeKey &origin_key : this->origin_keys) {
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
	typedef typename Types::Tpf Tpf; ///< The pathfinder class (derived from THIS class).
	typedef typename Types::NodeList::Item Node; ///< This will be our node type.
	typedef typename Node::Key Key; ///< Key to hash tables.

protected:
	Key dest;

public:
	void SetDestination(const WaterRegionPatchDesc &water_region_patch)
	{
		this->dest.Set(water_region_patch);
	}

protected:
	Tpf &Yapf() { return *static_cast<Tpf*>(this); }

public:
	inline bool PfDetectDestination(Node &n) const
	{
		return n.key == this->dest;
	}

	inline bool PfCalcEstimate(Node &n)
	{
		if (this->PfDetectDestination(n)) {
			n.estimate = n.cost;
			return true;
		}

		n.estimate = n.cost + ManhattanDistance(n.key, this->dest);

		return true;
	}
};

/** YAPF node following for water region pathfinding. */
template <class Types>
class CYapfFollowRegionT
{
public:
	typedef typename Types::Tpf Tpf; ///< The pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Item Node; ///< This will be our node type.
	typedef typename Node::Key Key; ///< Key to hash tables.

protected:
	inline Tpf &Yapf() { return *static_cast<Tpf*>(this); }

public:
	inline void PfFollowNode(Node &old_node)
	{
		TVisitWaterRegionPatchCallBack visit_func = [&](const WaterRegionPatchDesc &water_region_patch)
		{
			Node &node = Yapf().CreateNewNode();
			node.Set(&old_node, water_region_patch);
			Yapf().AddNewNode(node, TrackFollower{});
		};
		VisitWaterRegionPatchNeighbours(old_node.key.water_region_patch, visit_func);
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
			StationID station_id = v->current_order.GetDestination().ToStationID();
			const BaseStation *station = BaseStation::Get(station_id);
			TileArea tile_area;
			station->GetTileArea(&tile_area, StationType::Dock);
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
				node = node->parent;
				if (node != nullptr) path.push_back(node->key.water_region_patch);
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
	typedef typename Types::Tpf Tpf; ///< The pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Item Node; ///< This will be our node type.
	typedef typename Node::Key Key; ///< Key to hash tables.

protected:
	/** To access inherited path finder. */
	Tpf &Yapf() { return *static_cast<Tpf*>(this); }

public:
	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 * Calculates only the cost of given node, adds it to the parent node cost
	 * and stores the result into Node::cost member.
	 */
	inline bool PfCalcCost(Node &n, const TrackFollower *)
	{
		n.cost = n.parent->cost + ManhattanDistance(n.key, n.parent->key);

		/* Incentivise zigzagging by adding a slight penalty when the search continues in the same direction. */
		Node *grandparent = n.parent->parent;
		if (grandparent != nullptr) {
			const DiagDirDiff dir_diff = DiagDirDifference(n.parent->GetDiagDirFromParent(), n.GetDiagDirFromParent());
			if (dir_diff != DIAGDIRDIFF_90LEFT && dir_diff != DIAGDIRDIFF_90RIGHT) n.cost += 1;
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

typedef NodeList<CYapfRegionNodeT<CYapfRegionPatchNodeKey>, 12, 12> CRegionNodeListWater;

struct CYapfRegionWater : CYapfT<CYapfRegion_TypesT<CYapfRegionWater, CRegionNodeListWater>>
{
	explicit CYapfRegionWater(int max_nodes) { this->max_search_nodes = max_nodes; }
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
