/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_ship.cpp Implementation of YAPF for ships. */

#include "../../stdafx.h"
#include "../../ship.h"
#include "../../industry.h"
#include "../../vehicle_func.h"

#include "yapf_includes.h"
#include "yapf_node_ship.hpp"
#include "yapf_ship_regions.h"
#include "../water_regions.h"

#include "../../safeguards.h"

constexpr int NUMBER_OR_WATER_REGIONS_LOOKAHEAD = 4;
constexpr int MAX_SHIP_PF_NODES = (NUMBER_OR_WATER_REGIONS_LOOKAHEAD + 1) * WATER_REGION_NUMBER_OF_TILES * 4; // 4 possible exit dirs per tile.

constexpr int SHIP_LOST_PATH_LENGTH = 8; // The length of the (aimless) path assigned when a ship is lost.

template <class Types>
class CYapfDestinationTileWaterT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type.
	typedef typename Node::Key Key;                      ///< key to hash tables.

protected:
	TileIndex    m_destTile;
	TrackdirBits m_destTrackdirs;
	StationID    m_destStation;

	bool                 m_has_intermediate_dest = false;
	TileIndex            m_intermediate_dest_tile;
	WaterRegionPatchDesc m_intermediate_dest_region_patch;

public:
	void SetDestination(const Ship *v)
	{
		if (v->current_order.IsType(OT_GOTO_STATION)) {
			m_destStation = v->current_order.GetDestination();
			m_destTile = CalcClosestStationTile(m_destStation, v->tile, STATION_DOCK);
			m_destTrackdirs = INVALID_TRACKDIR_BIT;
		} else {
			m_destStation = INVALID_STATION;
			m_destTile = v->dest_tile;
			m_destTrackdirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->dest_tile, TRANSPORT_WATER, 0));
		}
	}

	void SetIntermediateDestination(const WaterRegionPatchDesc &water_region_patch)
	{
		m_has_intermediate_dest = true;
		m_intermediate_dest_tile = GetWaterRegionCenterTile(water_region_patch);
		m_intermediate_dest_region_patch = water_region_patch;
	}

protected:
	/** To access inherited path finder. */
	inline Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	/** Called by YAPF to detect if node ends in the desired destination. */
	inline bool PfDetectDestination(Node &n)
	{
		return PfDetectDestinationTile(n.m_segment_last_tile, n.m_segment_last_td);
	}

	inline bool PfDetectDestinationTile(TileIndex tile, Trackdir trackdir)
	{
		if (m_has_intermediate_dest) {
			/* GetWaterRegionInfo is much faster than GetWaterRegionPatchInfo so we try that first. */
			if (GetWaterRegionInfo(tile) != m_intermediate_dest_region_patch) return false;
			return GetWaterRegionPatchInfo(tile) == m_intermediate_dest_region_patch;
		}

		if (m_destStation != INVALID_STATION) return IsDockingTile(tile) && IsShipDestinationTile(tile, m_destStation);

		return tile == m_destTile && ((m_destTrackdirs & TrackdirToTrackdirBits(trackdir)) != TRACKDIR_BIT_NONE);
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 * adds it to the actual cost from origin and stores the sum to the Node::m_estimate.
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		const TileIndex destination_tile = m_has_intermediate_dest ? m_intermediate_dest_tile : m_destTile;

		static const int dg_dir_to_x_offs[] = { -1, 0, 1, 0 };
		static const int dg_dir_to_y_offs[] = { 0, 1, 0, -1 };
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		TileIndex tile = n.m_segment_last_tile;
		DiagDirection exitdir = TrackdirToExitdir(n.m_segment_last_td);
		int x1 = 2 * TileX(tile) + dg_dir_to_x_offs[(int)exitdir];
		int y1 = 2 * TileY(tile) + dg_dir_to_y_offs[(int)exitdir];
		int x2 = 2 * TileX(destination_tile);
		int y2 = 2 * TileY(destination_tile);
		int dx = abs(x1 - x2);
		int dy = abs(y1 - y2);
		int dmin = std::min(dx, dy);
		int dxy = abs(dx - dy);
		int d = dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};

/** Node Follower module of YAPF for ships */
template <class Types>
class CYapfFollowShipT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type.
	typedef typename Node::Key Key;                      ///< key to hash tables.

protected:
	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

	std::vector<WaterRegionDesc> m_water_region_corridor;

public:
	/**
	 * Called by YAPF to move from the given node to the next tile. For each
	 *  reachable trackdir on the new tile creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n)
	 */
	inline void PfFollowNode(Node &old_node)
	{
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.m_key.m_tile, old_node.m_key.m_td)) {
			if (m_water_region_corridor.empty()
					|| std::find(m_water_region_corridor.begin(), m_water_region_corridor.end(),
						GetWaterRegionInfo(F.m_new_tile)) != m_water_region_corridor.end()) {
				Yapf().AddMultipleNodes(&old_node, F);
			}
		}
	}

	/** Restricts the search by creating corridor or water regions through which the ship is allowed to travel. */
	inline void RestrictSearch(const std::vector<WaterRegionPatchDesc> &path)
	{
		m_water_region_corridor.clear();
		for (const WaterRegionPatchDesc &path_entry : path) m_water_region_corridor.push_back(path_entry);
	}

	/** Return debug report character to identify the transportation type. */
	inline char TransportTypeChar() const
	{
		return 'w';
	}

	/** Creates a random path, avoids 90 degree turns. */
	static Trackdir CreateRandomPath(const Ship *v, TileIndex tile, Trackdir dir, ShipPathCache &path_cache, int path_length)
	{
		for (int i = 0; i < path_length; ++i) {
			TrackFollower F(v);
			if (F.Follow(tile, dir)) {
				tile = F.m_new_tile;
				TrackdirBits dirs = F.m_new_td_bits & ~TrackdirCrossesTrackdirs(dir);
				const int strip_amount = _random.Next(CountBits(dirs));
				for (int s = 0; s < strip_amount; ++s) RemoveFirstTrackdir(&dirs);
				dir = FindFirstTrackdir(dirs);
				if (dir == INVALID_TRACKDIR) break;
				path_cache.push_back(dir);
			}
		}

		if (path_cache.empty()) return INVALID_TRACKDIR;

		const Trackdir result = path_cache.front();
		path_cache.pop_front();
		return result;
	}

	static Trackdir ChooseShipTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, ShipPathCache &path_cache)
	{
		/* Handle special case - when next tile is destination tile. */
		if (tile == v->dest_tile) {
			/* Convert tracks to trackdirs */
			TrackdirBits trackdirs = TrackBitsToTrackdirBits(tracks);
			/* Limit to trackdirs reachable from enterdir. */
			trackdirs &= DiagdirReachesTrackdirs(enterdir);

			/* use vehicle's current direction if that's possible, otherwise use first usable one. */
			Trackdir veh_dir = v->GetVehicleTrackdir();
			return (HasTrackdir(trackdirs, veh_dir)) ? veh_dir : (Trackdir)FindFirstBit(trackdirs);
		}

		/* Move back to the old tile/trackdir (where ship is coming from). */
		TileIndex src_tile = TileAddByDiagDir(tile, ReverseDiagDir(enterdir));
		Trackdir trackdir = v->GetVehicleTrackdir();
		assert(IsValidTrackdir(trackdir));

		/* Convert origin trackdir to TrackdirBits. */
		TrackdirBits trackdirs = TrackdirToTrackdirBits(trackdir);

		const std::vector<WaterRegionPatchDesc> high_level_path = YapfShipFindWaterRegionPath(v, tile, NUMBER_OR_WATER_REGIONS_LOOKAHEAD + 1);
		if (high_level_path.empty()) {
			path_found = false;
			/* Make the ship move around aimlessly. This prevents repeated pathfinder calls and clearly indicates that the ship is lost. */
			return CreateRandomPath(v, src_tile, trackdir, path_cache, SHIP_LOST_PATH_LENGTH);
		}

		/* Try one time without restricting the search area, which generally results in better and more natural looking paths.
		 * However the pathfinder can hit the node limit in certain situations such as long aqueducts or maze-like terrain.
		 * If that happens we run the pathfinder again, but restricted only to the regions provided by the region pathfinder. */
		for (int attempt = 0; attempt < 2; ++attempt) {
			Tpf pf(MAX_SHIP_PF_NODES);

			/* Set origin and destination nodes */
			pf.SetOrigin(src_tile, trackdirs);
			pf.SetDestination(v);
			const bool is_intermediate_destination = static_cast<int>(high_level_path.size()) >= NUMBER_OR_WATER_REGIONS_LOOKAHEAD + 1;
			if (is_intermediate_destination) pf.SetIntermediateDestination(high_level_path.back());

			/* Restrict the search area to prevent the low level pathfinder from expanding too many nodes. This can happen
			 * when the terrain is very "maze-like" or when the high level path "teleports" via a very long aqueduct. */
			if (attempt > 0) pf.RestrictSearch(high_level_path);

			/* Find best path. */
			path_found = pf.FindPath(v);
			Node *node = pf.GetBestNode();
			if (attempt == 0 && !path_found) continue; // Try again with restricted search area.
			if (!path_found || !node) return INVALID_TRACKDIR;

			/* Return only the path within the current water region if an intermediate destination was returned. If not, cache the entire path
			 * to the final destination tile. The low-level pathfinder might actually prefer a different docking tile in a nearby region. Without
			 * caching the full path the ship can get stuck in a loop. */
			const WaterRegionPatchDesc end_water_patch = GetWaterRegionPatchInfo(node->GetTile());
			const WaterRegionPatchDesc start_water_patch = GetWaterRegionPatchInfo(tile);
			while (node->m_parent) {
				const WaterRegionPatchDesc node_water_patch = GetWaterRegionPatchInfo(node->GetTile());
				if (node_water_patch == start_water_patch || (!is_intermediate_destination && node_water_patch != end_water_patch)) {
					path_cache.push_front(node->GetTrackdir());
				}
				node = node->m_parent;
			}
			assert(!path_cache.empty());

			/* Take out the last trackdir as the result. */
			const Trackdir result = path_cache.front();
			path_cache.pop_front();

			/* Clear path cache when in final water region patch. This is to allow ships to spread over different docking tiles dynamically. */
			if (start_water_patch == end_water_patch) path_cache.clear();

			return result;
		}

		return INVALID_TRACKDIR;
	}

	/**
	 * Check whether a ship should reverse to reach its destination.
	 * Called when leaving depot.
	 * @param v Ship.
	 * @param tile Current position.
	 * @param td1 Forward direction.
	 * @param td2 Reverse direction.
	 * @param trackdir [out] the best of all possible reversed trackdirs.
	 * @return true if the reverse direction is better.
	 */
	static bool CheckShipReverse(const Ship *v, TileIndex tile, Trackdir td1, Trackdir td2, Trackdir *trackdir)
	{
		const std::vector<WaterRegionPatchDesc> high_level_path = YapfShipFindWaterRegionPath(v, tile, NUMBER_OR_WATER_REGIONS_LOOKAHEAD + 1);
		if (high_level_path.empty()) {
			if (trackdir) *trackdir = INVALID_TRACKDIR;
			return false;
		}

		/* Create pathfinder instance. */
		Tpf pf(MAX_SHIP_PF_NODES);
		/* Set origin and destination nodes. */
		if (trackdir == nullptr) {
			pf.SetOrigin(tile, TrackdirToTrackdirBits(td1) | TrackdirToTrackdirBits(td2));
		} else {
			DiagDirection entry = ReverseDiagDir(VehicleExitDir(v->direction, v->state));
			TrackdirBits rtds = DiagdirReachesTrackdirs(entry) & TrackStatusToTrackdirBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0, entry));
			pf.SetOrigin(tile, rtds);
		}
		pf.SetDestination(v);
		if (high_level_path.size() > 1) pf.SetIntermediateDestination(high_level_path.back());
		pf.RestrictSearch(high_level_path);

		/* Find best path. */
		if (!pf.FindPath(v)) return false;

		Node *pNode = pf.GetBestNode();
		if (pNode == nullptr) return false;

		/* Path was found, walk through the path back to the origin. */
		while (pNode->m_parent != nullptr) {
			pNode = pNode->m_parent;
		}

		Trackdir best_trackdir = pNode->GetTrackdir();
		if (trackdir != nullptr) {
			*trackdir = best_trackdir;
		} else {
			assert(best_trackdir == td1 || best_trackdir == td2);
		}
		return best_trackdir != td1;
	}
};

/** Cost Provider module of YAPF for ships. */
template <class Types>
class CYapfCostShipT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type.
	typedef typename Node::Key Key;               ///< key to hash tables.

	/** to access inherited path finder */
	Tpf &Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	inline int CurveCost(Trackdir td1, Trackdir td2)
	{
		assert(IsValidTrackdir(td1));
		assert(IsValidTrackdir(td2));

		if (HasTrackdir(TrackdirCrossesTrackdirs(td1), td2)) {
			/* 90-deg curve penalty. */
			return Yapf().PfGetSettings().ship_curve90_penalty;
		} else if (td2 != NextTrackdir(td1)) {
			/* 45-deg curve penalty. */
			return Yapf().PfGetSettings().ship_curve45_penalty;
		}
		return 0;
	}

	static Vehicle *CountShipProc(Vehicle *v, void *data)
	{
		uint *count = (uint*)data;
		/* Ignore other vehicles (aircraft) and ships inside depot. */
		if (v->type == VEH_SHIP && (v->vehstatus & VS_HIDDEN) == 0) (*count)++;

		return nullptr;
	}

	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 * Calculates only the cost of given node, adds it to the parent node cost
	 * and stores the result into Node::m_cost member.
	 */
	inline bool PfCalcCost(Node &n, const TrackFollower *tf)
	{
		/* Base tile cost depending on distance. */
		int c = IsDiagonalTrackdir(n.GetTrackdir()) ? YAPF_TILE_LENGTH : YAPF_TILE_CORNER_LENGTH;
		/* Additional penalty for curves. */
		c += CurveCost(n.m_parent->GetTrackdir(), n.GetTrackdir());

		if (IsDockingTile(n.GetTile())) {
			/* Check docking tile for occupancy. */
			uint count = 0;
			HasVehicleOnPos(n.GetTile(), &count, &CountShipProc);
			c += count * 3 * YAPF_TILE_LENGTH;
		}

		/* Skipped tile cost for aqueducts. */
		c += YAPF_TILE_LENGTH * tf->m_tiles_skipped;

		/* Ocean/canal speed penalty. */
		const ShipVehicleInfo *svi = ShipVehInfo(Yapf().GetVehicle()->engine_type);
		byte speed_frac = (GetEffectiveWaterClass(n.GetTile()) == WATER_CLASS_SEA) ? svi->ocean_speed_frac : svi->canal_speed_frac;
		if (speed_frac > 0) c += YAPF_TILE_LENGTH * (1 + tf->m_tiles_skipped) * speed_frac / (256 - speed_frac);

		/* Apply it. */
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}
};

/**
 * Config struct of YAPF for ships.
 * Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Ttrack_follower, class Tnode_list>
struct CYapfShip_TypesT
{
	typedef CYapfShip_TypesT<Tpf_, Ttrack_follower, Tnode_list>  Types;         ///< Shortcut for this struct type.
	typedef Tpf_                                                 Tpf;           ///< Pathfinder type.
	typedef Ttrack_follower                                      TrackFollower; ///< Track follower helper class.
	typedef Tnode_list                                           NodeList;
	typedef Ship                                                 VehicleType;

	/** Pathfinder components (modules). */
	typedef CYapfBaseT<Types>                 PfBase;        ///< Base pathfinder class.
	typedef CYapfFollowShipT<Types>           PfFollow;      ///< Node follower.
	typedef CYapfOriginTileT<Types>           PfOrigin;      ///< Origin provider.
	typedef CYapfDestinationTileWaterT<Types> PfDestination; ///< Destination/distance provider.
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       ///< Segment cost cache provider.
	typedef CYapfCostShipT<Types>             PfCost;        ///< Cost provider.
};

struct CYapfShip : CYapfT<CYapfShip_TypesT<CYapfShip, CFollowTrackWater, CShipNodeListExitDir > >
{
	explicit CYapfShip(int max_nodes) { m_max_search_nodes = max_nodes; }
};

/** Ship controller helper - path finder invoker. */
Track YapfShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, ShipPathCache &path_cache)
{
	Trackdir td_ret = CYapfShip::ChooseShipTrack(v, tile, enterdir, tracks, path_found, path_cache);
	return (td_ret != INVALID_TRACKDIR) ? TrackdirToTrack(td_ret) : INVALID_TRACK;
}

bool YapfShipCheckReverse(const Ship *v, Trackdir *trackdir)
{
	Trackdir td = v->GetVehicleTrackdir();
	Trackdir td_rev = ReverseTrackdir(td);
	TileIndex tile = v->tile;
	return CYapfShip::CheckShipReverse(v, tile, td, td_rev, trackdir);
}
