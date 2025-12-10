/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

 /** @file yapf_ship_regions.cpp Implementation of YAPF for water regions, which are used for finding intermediate ship destinations. */

#include "../../stdafx.h"
#include "../../ship.h"

#include "yapf.hpp"
#include "yapf_ship_regions.h"
#include "../water_regions.h"

#include "../../safeguards.h"

static constexpr int DIRECT_NEIGHBOUR_COST = 100;
static constexpr int NODES_PER_REGION = 4;
static constexpr int MAX_NUMBER_OF_NODES = 65536;

static constexpr int NODE_LIST_HASH_BITS_OPEN = 12;
static constexpr int NODE_LIST_HASH_BITS_CLOSED = 12;

/** Yapf Node Key that represents a single patch of interconnected water within a water region. */
struct WaterRegionPatchKey {
	WaterRegionPatchDesc water_region_patch;

	inline void Set(const WaterRegionPatchDesc &water_region_patch)
	{
		this->water_region_patch = water_region_patch;
	}

	inline int CalcHash() const { return CalculateWaterRegionPatchHash(this->water_region_patch); }
	inline bool operator==(const WaterRegionPatchKey &other) const { return this->CalcHash() == other.CalcHash(); }
};

inline uint ManhattanDistance(const WaterRegionPatchKey &a, const WaterRegionPatchKey &b)
{
	return (std::abs(a.water_region_patch.x - b.water_region_patch.x) + std::abs(a.water_region_patch.y - b.water_region_patch.y)) * DIRECT_NEIGHBOUR_COST;
}

/** Yapf Node for water regions. */
struct WaterRegionNode : CYapfNodeT<WaterRegionPatchKey, WaterRegionNode> {
	using Key = WaterRegionPatchKey;
	using Node = WaterRegionNode;

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

using WaterRegionNodeList = NodeList<WaterRegionNode, NODE_LIST_HASH_BITS_OPEN, NODE_LIST_HASH_BITS_CLOSED>;

/* We don't need a follower but YAPF requires one. */
struct WaterRegionFollower : public CFollowTrackWater {};

class YapfShipRegions;

/* Types struct required for YAPF internals. */
struct WaterRegionTypes {
	using Tpf = YapfShipRegions;
	using TrackFollower = WaterRegionFollower;
	using NodeList = WaterRegionNodeList;
	using VehicleType = Ship;
};

/** Water region based YAPF implementation for ships. */
class YapfShipRegions
	: public CYapfBaseT<WaterRegionTypes>
	, public CYapfSegmentCostCacheNoneT<WaterRegionTypes>
{
private:
	using Node = typename WaterRegionTypes::NodeList::Item;

	std::vector<WaterRegionPatchKey> origin_keys;
	WaterRegionPatchKey dest;

	inline YapfShipRegions &Yapf()
	{
		return *static_cast<YapfShipRegions *>(this);
	}

public:
	explicit YapfShipRegions(int max_nodes)
	{
		this->max_search_nodes = max_nodes;
	}

	void AddOrigin(const WaterRegionPatchDesc &water_region_patch)
	{
		if (water_region_patch.label == INVALID_WATER_REGION_PATCH) return;
		if (!HasOrigin(water_region_patch)) {
			this->origin_keys.emplace_back(water_region_patch);
			Node &node = Yapf().CreateNewNode();
			node.Set(nullptr, water_region_patch);
			Yapf().AddStartupNode(node);
		}
	}

	bool HasOrigin(const WaterRegionPatchDesc &water_region_patch)
	{
		return std::ranges::find(this->origin_keys, WaterRegionPatchKey{ water_region_patch }) != this->origin_keys.end();
	}

	void SetDestination(const WaterRegionPatchDesc &water_region_patch)
	{
		this->dest.Set(water_region_patch);
	}

	inline void PfFollowNode(Node &old_node)
	{
		VisitWaterRegionPatchCallback visit_func = [&](const WaterRegionPatchDesc &water_region_patch) {
			Node &node = Yapf().CreateNewNode();
			node.Set(&old_node, water_region_patch);
			Yapf().AddNewNode(node, TrackFollower{});
		};
		VisitWaterRegionPatchNeighbours(old_node.key.water_region_patch, visit_func);
	}

	inline bool PfDetectDestination(Node &n) const
	{
		return n.key == this->dest;
	}

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

	inline bool PfCalcEstimate(Node &n)
	{
		if (this->PfDetectDestination(n)) {
			n.estimate = n.cost;
			return true;
		}

		n.estimate = n.cost + ManhattanDistance(n.key, this->dest);

		return true;
	}

	inline char TransportTypeChar() const { return '^'; }

	static std::vector<WaterRegionPatchDesc> FindWaterRegionPath(const Ship *v, TileIndex start_tile, int max_returned_path_length)
	{
		const WaterRegionPatchDesc start_water_region_patch = GetWaterRegionPatchInfo(start_tile);

		/* We reserve 4 nodes (patches) per water region. The vast majority of water regions have 1 or 2 regions so this should be a pretty
		 * safe limit. We cap the limit at 65536 which is at a region size of 16x16 is equivalent to one node per region for a 4096x4096 map. */
		const int node_limit = std::min(static_cast<int>(Map::Size() * NODES_PER_REGION) / WATER_REGION_NUMBER_OF_TILES, MAX_NUMBER_OF_NODES);
		YapfShipRegions pf(node_limit);
		pf.SetDestination(start_water_region_patch);

		if (v->current_order.IsType(OT_GOTO_STATION)) {
			StationID station_id = v->current_order.GetDestination().ToStationID();
			const BaseStation *station = BaseStation::Get(station_id);
			for (const auto &tile : station->GetTileArea(StationType::Dock)) {
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

/**
 * Finds a path at the water region level. Note that the starting region is always included if the path was found.
 * @param v The ship to find a path for.
 * @param start_tile The tile to start searching from.
 * @param max_returned_path_length The maximum length of the path that will be returned.
 * @returns A path of water region patches, or an empty vector if no path was found.
 */
std::vector<WaterRegionPatchDesc> YapfShipFindWaterRegionPath(const Ship *v, TileIndex start_tile, int max_returned_path_length)
{
	return YapfShipRegions::FindWaterRegionPath(v, start_tile, max_returned_path_length);
}
