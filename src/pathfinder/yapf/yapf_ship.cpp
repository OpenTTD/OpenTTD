/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_ship.cpp Implementation of YAPF for ships. */

#include "../../stdafx.h"
#include "../../ship.h"
#include "../../vehicle_func.h"

#include "yapf.hpp"
#include "yapf_node_ship.hpp"
#include "yapf_ship_regions.h"
#include "../water_regions.h"

#include "../../safeguards.h"

constexpr int NUMBER_OR_WATER_REGIONS_LOOKAHEAD = 4;
constexpr int MAX_SHIP_PF_NODES = (NUMBER_OR_WATER_REGIONS_LOOKAHEAD + 1) * WATER_REGION_NUMBER_OF_TILES * 4; // 4 possible exit dirs per tile.
constexpr int SHIP_REVERSE_PENALTY = 10 * YAPF_TILE_LENGTH;
constexpr int SHIP_LOST_PATH_LENGTH = 8; // The length of the (aimless) path assigned when a ship is lost.

template <class Types>
class CYapfDestinationTileWaterT
{
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Item Node; ///< this will be our node type.
	typedef typename Node::Key Key; ///< key to hash tables.

protected:
	TileIndex dest_tile;
	TrackdirBits dest_trackdirs;
	StationID dest_station;

	bool has_intermediate_dest = false;
	TileIndex intermediate_dest_tile;
	WaterRegionPatchDesc intermediate_dest_region_patch;

public:
	void SetDestination(const Ship *v)
	{
		if (v->current_order.IsType(OT_GOTO_STATION)) {
			this->dest_station = v->current_order.GetDestination().ToStationID();
			this->dest_tile = CalcClosestStationTile(this->dest_station, v->tile, StationType::Dock);
			this->dest_trackdirs = INVALID_TRACKDIR_BIT;
		} else {
			this->dest_station = StationID::Invalid();
			this->dest_tile = v->dest_tile;
			this->dest_trackdirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->dest_tile, TRANSPORT_WATER, 0));
		}
	}

	void SetIntermediateDestination(const WaterRegionPatchDesc &water_region_patch)
	{
		this->has_intermediate_dest = true;
		this->intermediate_dest_tile = GetWaterRegionCenterTile(water_region_patch);
		this->intermediate_dest_region_patch = water_region_patch;
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
		return this->PfDetectDestinationTile(n.segment_last_tile, n.segment_last_td);
	}

	inline bool PfDetectDestinationTile(TileIndex tile, Trackdir trackdir)
	{
		if (this->has_intermediate_dest) {
			/* GetWaterRegionInfo is much faster than GetWaterRegionPatchInfo so we try that first. */
			if (GetWaterRegionInfo(tile) != this->intermediate_dest_region_patch) return false;
			return GetWaterRegionPatchInfo(tile) == this->intermediate_dest_region_patch;
		}

		if (this->dest_station != StationID::Invalid()) return IsDockingTile(tile) && IsShipDestinationTile(tile, this->dest_station);

		return tile == this->dest_tile && ((this->dest_trackdirs & TrackdirToTrackdirBits(trackdir)) != TRACKDIR_BIT_NONE);
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 * adds it to the actual cost from origin and stores the sum to the Node::estimate.
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		const TileIndex destination_tile = this->has_intermediate_dest ? this->intermediate_dest_tile : this->dest_tile;

		static const int dg_dir_to_x_offs[] = { -1, 0, 1, 0 };
		static const int dg_dir_to_y_offs[] = { 0, 1, 0, -1 };
		if (this->PfDetectDestination(n)) {
			n.estimate = n.cost;
			return true;
		}

		TileIndex tile = n.segment_last_tile;
		DiagDirection exitdir = TrackdirToExitdir(n.segment_last_td);
		int x1 = 2 * TileX(tile) + dg_dir_to_x_offs[(int)exitdir];
		int y1 = 2 * TileY(tile) + dg_dir_to_y_offs[(int)exitdir];
		int x2 = 2 * TileX(destination_tile);
		int y2 = 2 * TileY(destination_tile);
		int dx = abs(x1 - x2);
		int dy = abs(y1 - y2);
		int dmin = std::min(dx, dy);
		int dxy = abs(dx - dy);
		int d = dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
		n.estimate = n.cost + d;
		assert(n.estimate >= n.parent->estimate);
		return true;
	}
};

/** Node Follower module of YAPF for ships */
template <class Types>
class CYapfFollowShipT
{
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Item Node; ///< this will be our node type.
	typedef typename Node::Key Key; ///< key to hash tables.

protected:
	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

	std::vector<WaterRegionDesc> water_region_corridor;

public:
	/**
	 * Called by YAPF to move from the given node to the next tile. For each
	 *  reachable trackdir on the new tile creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n)
	 */
	inline void PfFollowNode(Node &old_node)
	{
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.key.tile, old_node.key.td)) {
			if (this->water_region_corridor.empty()
					|| std::ranges::find(this->water_region_corridor, GetWaterRegionInfo(F.new_tile)) != this->water_region_corridor.end()) {
				Yapf().AddMultipleNodes(&old_node, F);
			}
		}
	}

	/** Restricts the search by creating corridor or water regions through which the ship is allowed to travel. */
	inline void RestrictSearch(const std::vector<WaterRegionPatchDesc> &path)
	{
		this->water_region_corridor.clear();
		for (const WaterRegionPatchDesc &path_entry : path) this->water_region_corridor.push_back(path_entry);
	}

	/** Return debug report character to identify the transportation type. */
	inline char TransportTypeChar() const
	{
		return 'w';
	}

	/** Returns a random trackdir out of a set of trackdirs. */
	static Trackdir GetRandomTrackdir(TrackdirBits trackdirs)
	{
		const int strip_amount = RandomRange(CountBits(trackdirs));
		for (int s = 0; s < strip_amount; ++s) RemoveFirstTrackdir(&trackdirs);
		return FindFirstTrackdir(trackdirs);
	}

	/** Returns a random tile/trackdir that can be reached from the current tile/trackdir, or tile/INVALID_TRACKDIR if none is available. */
	static std::pair<TileIndex, Trackdir> GetRandomFollowUpTileTrackdir(const Ship *v, TileIndex tile, Trackdir dir)
	{
		TrackFollower follower(v);
		if (follower.Follow(tile, dir)) {
			TrackdirBits dirs = follower.new_td_bits;
			const TrackdirBits dirs_without_90_degree = dirs & ~TrackdirCrossesTrackdirs(dir);
			if (dirs_without_90_degree != TRACKDIR_BIT_NONE) dirs = dirs_without_90_degree;
			return { follower.new_tile, GetRandomTrackdir(dirs) };
		}
		return { follower.new_tile, INVALID_TRACKDIR };
	}

	/** Creates a random path, avoids 90 degree turns. */
	static Trackdir CreateRandomPath(const Ship *v, ShipPathCache &path_cache, int path_length)
	{
		std::pair<TileIndex, Trackdir> tile_dir = { v->tile, v->GetVehicleTrackdir()};
		for (int i = 0; i < path_length; ++i) {
			tile_dir = GetRandomFollowUpTileTrackdir(v, tile_dir.first, tile_dir.second);
			if (tile_dir.second == INVALID_TRACKDIR) break;
			path_cache.emplace_back(tile_dir.second);
		}

		if (path_cache.empty()) return ReverseTrackdir(v->GetVehicleTrackdir());

		/* Reverse the path so we can take from the end. */
		std::reverse(std::begin(path_cache), std::end(path_cache));

		const Trackdir result = path_cache.back().trackdir;
		path_cache.pop_back();
		return result;
	}

	static Trackdir ChooseShipTrack(const Ship *v, TrackdirBits forward_dirs, TrackdirBits reverse_dirs,
		bool &path_found, ShipPathCache &path_cache)
	{
		assert(HasBit(forward_dirs, v->GetVehicleTrackdir()));
		assert(HasBit(reverse_dirs, ReverseTrackdir(v->GetVehicleTrackdir())));
		assert((forward_dirs & reverse_dirs) == TRACKDIR_BIT_NONE);
		assert((TrackdirReachesTrackdirs(v->GetVehicleTrackdir()) & reverse_dirs) == TRACKDIR_BIT_NONE);

		const std::vector<WaterRegionPatchDesc> high_level_path = YapfShipFindWaterRegionPath(v, v->tile, NUMBER_OR_WATER_REGIONS_LOOKAHEAD + 1);
		if (high_level_path.empty()) {
			path_found = false;
			/* Make the ship move around aimlessly. This prevents repeated pathfinder calls and clearly indicates that the ship is lost. */
			return CreateRandomPath(v, path_cache, SHIP_LOST_PATH_LENGTH);
		}

		/* Try one time without restricting the search area, which generally results in better and more natural looking paths.
		 * However the pathfinder can hit the node limit in certain situations such as long aqueducts or maze-like terrain.
		 * If that happens we run the pathfinder again, but restricted only to the regions provided by the region pathfinder. */
		for (int attempt = 0; attempt < 2; ++attempt) {
			Tpf pf(MAX_SHIP_PF_NODES);

			/* Set origin and destination nodes */
			pf.SetOrigin(v->tile, forward_dirs, reverse_dirs, SHIP_REVERSE_PENALTY);
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

			/* Make the ship move around aimlessly. This prevents repeated pathfinder calls and clearly indicates that the ship is lost. */
			if (!path_found) return CreateRandomPath(v, path_cache, SHIP_LOST_PATH_LENGTH);

			/* Return only the path within the current water region if an intermediate destination was returned. If not, cache the entire path
			 * to the final destination tile. The low-level pathfinder might actually prefer a different docking tile in a nearby region. Without
			 * caching the full path the ship can get stuck in a loop. */
			const WaterRegionPatchDesc end_water_patch = GetWaterRegionPatchInfo(node->GetTile());
			assert(GetWaterRegionPatchInfo(v->tile) == high_level_path.front());
			const WaterRegionPatchDesc start_patch = high_level_path.front();
			while (node->parent) {
				const WaterRegionPatchDesc current_patch = GetWaterRegionPatchInfo(node->parent->GetTile());
				const bool current_patch_on_high_level_path = std::ranges::find(high_level_path, current_patch) != high_level_path.end();
				const bool add_full_path = !is_intermediate_destination && current_patch != end_water_patch;

				/* The cached path must always lead to a region patch that's on the high level path.
				 * This is what can happen when that's not the case https://github.com/OpenTTD/OpenTTD/issues/12176. */
				if (add_full_path || !current_patch_on_high_level_path || current_patch == start_patch) {
					path_cache.push_back(node->GetTrackdir());
				} else {
					path_cache.clear();
				}
				node = node->parent;
			}
			assert(node->GetTile() == v->tile);

			/* Return a reverse direction if that is the best option. */
			if (HasBit(reverse_dirs, node->GetTrackdir())) {
				path_cache.clear();
				return node->GetTrackdir();
			}

			/* A empty path means we are already at the destination. The pathfinder shouldn't have been called at all.
			 * Return a random reachable trackdir to hopefully nudge the ship out of this strange situation. */
			if (path_cache.empty()) return CreateRandomPath(v, path_cache, 1);

			/* Take out the last trackdir as the result. */
			const Trackdir result = path_cache.back().trackdir;
			path_cache.pop_back();

			/* Clear path cache when in final water region patch. This is to allow ships to spread over different docking tiles dynamically. */
			if (start_patch == end_water_patch) path_cache.clear();

			return result;
		}

		NOT_REACHED();
	}
};

/** Cost Provider module of YAPF for ships. */
template <class Types>
class CYapfCostShipT
{
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Item Node; ///< this will be our node type.
	typedef typename Node::Key Key; ///< key to hash tables.

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
		if (v->type == VEH_SHIP && !v->vehstatus.Test(VehState::Hidden)) (*count)++;

		return nullptr;
	}

	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 * Calculates only the cost of given node, adds it to the parent node cost
	 * and stores the result into Node::cost member.
	 */
	inline bool PfCalcCost(Node &n, const TrackFollower *tf)
	{
		/* Base tile cost depending on distance. */
		int c = IsDiagonalTrackdir(n.GetTrackdir()) ? YAPF_TILE_LENGTH : YAPF_TILE_CORNER_LENGTH;
		/* Additional penalty for curves. */
		c += this->CurveCost(n.parent->GetTrackdir(), n.GetTrackdir());

		if (IsDockingTile(n.GetTile())) {
			/* Check docking tile for occupancy. */
			uint count = 0;
			HasVehicleOnPos(n.GetTile(), &count, &CountShipProc);
			c += count * 3 * YAPF_TILE_LENGTH;
		}

		/* Skipped tile cost for aqueducts. */
		c += YAPF_TILE_LENGTH * tf->tiles_skipped;

		/* Ocean/canal speed penalty. */
		const ShipVehicleInfo *svi = ShipVehInfo(Yapf().GetVehicle()->engine_type);
		uint8_t speed_frac = (GetEffectiveWaterClass(n.GetTile()) == WATER_CLASS_SEA) ? svi->ocean_speed_frac : svi->canal_speed_frac;
		if (speed_frac > 0) c += YAPF_TILE_LENGTH * (1 + tf->tiles_skipped) * speed_frac / (256 - speed_frac);

		/* Apply it. */
		n.cost = n.parent->cost + c;
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
	explicit CYapfShip(int max_nodes) { this->max_search_nodes = max_nodes; }
};

Trackdir YapfShipChooseTrack(const Ship *v, bool &path_found, ShipPathCache &path_cache)
{
	/* This is always called when a ship is about to exit a tile, hence we add up to three reverse directions. */
	const TrackdirBits forward_dirs = TrackdirToTrackdirBits(v->GetVehicleTrackdir());
	const TrackdirBits all_water_dirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->tile, TRANSPORT_WATER, 0));
	const TrackdirBits reverse_dirs = all_water_dirs & DiagdirReachesTrackdirs(ReverseDiagDir(TrackdirToExitdir(v->GetVehicleTrackdir())));

	const Trackdir trackdir = CYapfShip::ChooseShipTrack(v, forward_dirs, reverse_dirs, path_found, path_cache);
	assert(trackdir != INVALID_TRACKDIR);
	return trackdir;
}

bool YapfShipCheckReverse(const Ship *v, Trackdir &trackdir)
{
	/* It is not clear where the ship is within its current track, so we only add the forward (current) and reverse direction. */
	const TrackdirBits forward_dirs = TrackdirToTrackdirBits(v->GetVehicleTrackdir());
	const TrackdirBits reverse_dirs = TrackdirToTrackdirBits(ReverseTrackdir(v->GetVehicleTrackdir()));

	bool path_found = false;
	ShipPathCache dummy_cache;
	trackdir = CYapfShip::ChooseShipTrack(v, forward_dirs, reverse_dirs, path_found, dummy_cache);
	assert(trackdir != INVALID_TRACKDIR);
	return path_found && HasBit(reverse_dirs, trackdir);
}
