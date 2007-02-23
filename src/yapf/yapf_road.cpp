/* $Id$ */

/** @file yapf_road.cpp */

#include "../stdafx.h"

#include "yapf.hpp"
#include "yapf_node_road.hpp"


template <class Types>
class CYapfCostRoadT
{
public:
	typedef typename Types::Tpf Tpf; ///< pathfinder (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower; ///< track follower helper
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	int SlopeCost(TileIndex tile, TileIndex next_tile, Trackdir trackdir)
	{
		// height of the center of the current tile
		int x1 = TileX(tile) * TILE_SIZE;
		int y1 = TileY(tile) * TILE_SIZE;
		int z1 = GetSlopeZ(x1 + TILE_SIZE / 2, y1 + TILE_SIZE / 2);

		// height of the center of the next tile
		int x2 = TileX(next_tile) * TILE_SIZE;
		int y2 = TileY(next_tile) * TILE_SIZE;
		int z2 = GetSlopeZ(x2 + TILE_SIZE / 2, y2 + TILE_SIZE / 2);

		if (z2 - z1 > 1) {
			/* Slope up */
			return Yapf().PfGetSettings().road_slope_penalty;
		}
		return 0;
	}

	/** return one tile cost */
	FORCEINLINE int OneTileCost(TileIndex tile, Trackdir trackdir)
	{
		int cost = 0;
		// set base cost
		if (IsDiagonalTrackdir(trackdir)) {
			cost += YAPF_TILE_LENGTH;
			switch (GetTileType(tile)) {
				case MP_STREET:
					/* Increase the cost for level crossings */
					if (IsLevelCrossing(tile))
						cost += Yapf().PfGetSettings().road_crossing_penalty;
					break;
				case MP_STATION:
					if (IsDriveThroughStopTile(tile))
						cost += Yapf().PfGetSettings().road_stop_penalty;
					break;

				default:
					break;
			}
		} else {
			// non-diagonal trackdir
			cost = YAPF_TILE_CORNER_LENGTH + Yapf().PfGetSettings().road_curve_penalty;
		}
		return cost;
	}

public:
	/** Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member */
	FORCEINLINE bool PfCalcCost(Node& n)
	{
		int segment_cost = 0;
		// start at n.m_key.m_tile / n.m_key.m_td and walk to the end of segment
		TileIndex tile = n.m_key.m_tile;
		Trackdir trackdir = n.m_key.m_td;
		while (true) {
			// base tile cost depending on distance between edges
			segment_cost += Yapf().OneTileCost(tile, trackdir);

			const Vehicle* v = Yapf().GetVehicle();
			// we have reached the vehicle's destination - segment should end here to avoid target skipping
			if (v->current_order.type == OT_GOTO_STATION && tile == v->dest_tile) break;

			// stop if we have just entered the depot
			if (IsTileDepotType(tile, TRANSPORT_ROAD) && trackdir == DiagdirToDiagTrackdir(ReverseDiagDir(GetRoadDepotDirection(tile)))) {
				// next time we will reverse and leave the depot
				break;
			}

			// if there are no reachable trackdirs on new tile, we have end of road
			TrackFollower F(Yapf().GetVehicle());
			if (!F.Follow(tile, trackdir)) break;

			// if there are more trackdirs available & reachable, we are at the end of segment
			if (KillFirstBit2x64(F.m_new_td_bits) != 0) break;

			Trackdir new_td = (Trackdir)FindFirstBit2x64(F.m_new_td_bits);

			// stop if RV is on simple loop with no junctions
			if (F.m_new_tile == n.m_key.m_tile && new_td == n.m_key.m_td) return false;

			// if we skipped some tunnel tiles, add their cost
			segment_cost += F.m_tiles_skipped * YAPF_TILE_LENGTH;

			// add hilly terrain penalty
			segment_cost += Yapf().SlopeCost(tile, F.m_new_tile, trackdir);

			// add min/max speed penalties
			int min_speed = 0;
			int max_speed = F.GetSpeedLimit(&min_speed);
			if (max_speed < v->max_speed) segment_cost += 1 * (v->max_speed - max_speed);
			if (min_speed > v->max_speed) segment_cost += 10 * (min_speed - v->max_speed);

			// move to the next tile
			tile = F.m_new_tile;
			trackdir = new_td;
		};

		// save end of segment back to the node
		n.m_segment_last_tile = tile;
		n.m_segment_last_td = trackdir;

		// save also tile cost
		int parent_cost = (n.m_parent != NULL) ? n.m_parent->m_cost : 0;
		n.m_cost = parent_cost + segment_cost;
		return true;
	}
};


template <class Types>
class CYapfDestinationAnyDepotRoadT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = IsTileDepotType(n.m_segment_last_tile, TRANSPORT_ROAD);
		return bDest;
	}

	/** Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate */
	FORCEINLINE bool PfCalcEstimate(Node& n)
	{
		n.m_estimate = n.m_cost;
		return true;
	}
};


template <class Types>
class CYapfDestinationTileRoadT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

protected:
	TileIndex    m_destTile;
	TrackdirBits m_destTrackdirs;

public:
	void SetDestination(TileIndex tile, TrackdirBits trackdirs)
	{
		m_destTile = tile;
		m_destTrackdirs = trackdirs;
	}

protected:
	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = (n.m_segment_last_tile == m_destTile) && ((m_destTrackdirs & TrackdirToTrackdirBits(n.m_segment_last_td)) != TRACKDIR_BIT_NONE);
		return bDest;
	}

	/** Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate */
	inline bool PfCalcEstimate(Node& n)
	{
		static int dg_dir_to_x_offs[] = {-1, 0, 1, 0};
		static int dg_dir_to_y_offs[] = {0, 1, 0, -1};
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		TileIndex tile = n.m_segment_last_tile;
		DiagDirection exitdir = TrackdirToExitdir(n.m_segment_last_td);
		int x1 = 2 * TileX(tile) + dg_dir_to_x_offs[(int)exitdir];
		int y1 = 2 * TileY(tile) + dg_dir_to_y_offs[(int)exitdir];
		int x2 = 2 * TileX(m_destTile);
		int y2 = 2 * TileY(m_destTile);
		int dx = abs(x1 - x2);
		int dy = abs(y1 - y2);
		int dmin = min(dx, dy);
		int dxy = abs(dx - dy);
		int d = dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};



template <class Types>
class CYapfFollowRoadT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

protected:
	/// to access inherited path finder
	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:

	/** Called by YAPF to move from the given node to the next tile. For each
	 *  reachable trackdir on the new tile creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n) */
	inline void PfFollowNode(Node& old_node)
	{
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.m_segment_last_tile, old_node.m_segment_last_td))
			Yapf().AddMultipleNodes(&old_node, F.m_new_tile, F.m_new_td_bits);
	}

	/// return debug report character to identify the transportation type
	FORCEINLINE char TransportTypeChar() const {return 'r';}

	static Trackdir stChooseRoadTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir)
	{
		Tpf pf;
		return pf.ChooseRoadTrack(v, tile, enterdir);
	}

	FORCEINLINE Trackdir ChooseRoadTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir)
	{
		// handle special case - when next tile is destination tile
		if (tile == v->dest_tile) {
			// choose diagonal trackdir reachable from enterdir
			return (Trackdir)DiagdirToDiagTrackdir(enterdir);
		}
		// our source tile will be the next vehicle tile (should be the given one)
		TileIndex src_tile = tile;
		// get available trackdirs on the start tile
		uint ts = GetTileTrackStatus(tile, TRANSPORT_ROAD);
		TrackdirBits src_trackdirs = (TrackdirBits)(ts & TRACKDIR_BIT_MASK);
		// select reachable trackdirs only
		src_trackdirs &= DiagdirReachesTrackdirs(enterdir);

		// get available trackdirs on the destination tile
		TileIndex dest_tile = v->dest_tile;
		uint dest_ts = GetTileTrackStatus(dest_tile, TRANSPORT_ROAD);
		TrackdirBits dest_trackdirs = (TrackdirBits)(dest_ts & TRACKDIR_BIT_MASK);

		// set origin and destination nodes
		Yapf().SetOrigin(src_tile, src_trackdirs);
		Yapf().SetDestination(dest_tile, dest_trackdirs);

		// find the best path
		Yapf().FindPath(v);

		// if path not found - return INVALID_TRACKDIR
		Trackdir next_trackdir = INVALID_TRACKDIR;
		Node* pNode = &Yapf().GetBestNode();
		if (pNode != NULL) {
			// path was found or at least suggested
			// walk through the path back to its origin
			while (pNode->m_parent != NULL) {
				pNode = pNode->m_parent;
			}
			// return trackdir from the best origin node (one of start nodes)
			Node& best_next_node = *pNode;
			assert(best_next_node.GetTile() == tile);
			next_trackdir = best_next_node.GetTrackdir();
		}
		return next_trackdir;
	}

	static uint stDistanceToTile(const Vehicle *v, TileIndex tile)
	{
		Tpf pf;
		return pf.DistanceToTile(v, tile);
	}

	FORCEINLINE uint DistanceToTile(const Vehicle *v, TileIndex dst_tile)
	{
		// handle special case - when current tile is the destination tile
		if (dst_tile == v->tile) {
			// distance is zero in this case
			return 0;
		}

		if (!SetOriginFromVehiclePos(v)) return UINT_MAX;

		// set destination tile, trackdir
		//   get available trackdirs on the destination tile
		uint dest_ts = GetTileTrackStatus(dst_tile, TRANSPORT_ROAD);
		TrackdirBits dst_td_bits = (TrackdirBits)(dest_ts & TRACKDIR_BIT_MASK);
		Yapf().SetDestination(dst_tile, dst_td_bits);

		// find the best path
		Yapf().FindPath(v);

		// if path not found - return distance = UINT_MAX
		uint dist = UINT_MAX;
		Node* pNode = &Yapf().GetBestNode();
		if (pNode != NULL) {
			// path was found or at least suggested
			// get the path cost estimate
			dist = pNode->GetCostEstimate();
		}

		return dist;
	}

	/** Return true if the valid origin (tile/trackdir) was set from the current vehicle position. */
	FORCEINLINE bool SetOriginFromVehiclePos(const Vehicle *v)
	{
		// set origin (tile, trackdir)
		TileIndex src_tile = v->tile;
		Trackdir src_td = GetVehicleTrackdir(v);
		if ((GetTileTrackStatus(src_tile, TRANSPORT_ROAD) & TrackdirToTrackdirBits(src_td)) == 0) {
			// sometimes the roadveh is not on the road (it resides on non-existing track)
			// how should we handle that situation?
			return false;
		}
		Yapf().SetOrigin(src_tile, TrackdirToTrackdirBits(src_td));
		return true;
	}

	static Depot* stFindNearestDepot(const Vehicle* v, TileIndex tile, Trackdir td)
	{
		Tpf pf;
		return pf.FindNearestDepot(v, tile, td);
	}

	FORCEINLINE Depot* FindNearestDepot(const Vehicle* v, TileIndex tile, Trackdir td)
	{
		// set origin and destination nodes
		Yapf().SetOrigin(tile, TrackdirToTrackdirBits(td));

		// find the best path
		bool bFound = Yapf().FindPath(v);
		if (!bFound) return false;

		// some path found
		// get found depot tile
		Node& n = Yapf().GetBestNode();
		TileIndex depot_tile = n.m_segment_last_tile;
		assert(IsTileDepotType(depot_tile, TRANSPORT_ROAD));
		Depot* ret = GetDepotByTile(depot_tile);
		return ret;
	}
};

template <class Tpf_, class Tnode_list, template <class Types> class Tdestination>
struct CYapfRoad_TypesT
{
	typedef CYapfRoad_TypesT<Tpf_, Tnode_list, Tdestination>  Types;

	typedef Tpf_                              Tpf;
	typedef CFollowTrackRoad                  TrackFollower;
	typedef Tnode_list                        NodeList;
	typedef CYapfBaseT<Types>                 PfBase;
	typedef CYapfFollowRoadT<Types>           PfFollow;
	typedef CYapfOriginTileT<Types>           PfOrigin;
	typedef Tdestination<Types>               PfDestination;
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;
	typedef CYapfCostRoadT<Types>             PfCost;
};

struct CYapfRoad1         : CYapfT<CYapfRoad_TypesT<CYapfRoad1        , CRoadNodeListTrackDir, CYapfDestinationTileRoadT    > > {};
struct CYapfRoad2         : CYapfT<CYapfRoad_TypesT<CYapfRoad2        , CRoadNodeListExitDir , CYapfDestinationTileRoadT    > > {};

struct CYapfRoadAnyDepot1 : CYapfT<CYapfRoad_TypesT<CYapfRoadAnyDepot1, CRoadNodeListTrackDir, CYapfDestinationAnyDepotRoadT> > {};
struct CYapfRoadAnyDepot2 : CYapfT<CYapfRoad_TypesT<CYapfRoadAnyDepot2, CRoadNodeListExitDir , CYapfDestinationAnyDepotRoadT> > {};


Trackdir YapfChooseRoadTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir)
{
	// default is YAPF type 2
	typedef Trackdir (*PfnChooseRoadTrack)(Vehicle*, TileIndex, DiagDirection);
	PfnChooseRoadTrack pfnChooseRoadTrack = &CYapfRoad2::stChooseRoadTrack; // default: ExitDir, allow 90-deg

	// check if non-default YAPF type should be used
	if (_patches.yapf.disable_node_optimization)
		pfnChooseRoadTrack = &CYapfRoad1::stChooseRoadTrack; // Trackdir, allow 90-deg

	Trackdir td_ret = pfnChooseRoadTrack(v, tile, enterdir);
	return td_ret;
}

uint YapfRoadVehDistanceToTile(const Vehicle* v, TileIndex tile)
{
	// default is YAPF type 2
	typedef uint (*PfnDistanceToTile)(const Vehicle*, TileIndex);
	PfnDistanceToTile pfnDistanceToTile = &CYapfRoad2::stDistanceToTile; // default: ExitDir, allow 90-deg

	// check if non-default YAPF type should be used
	if (_patches.yapf.disable_node_optimization)
		pfnDistanceToTile = &CYapfRoad1::stDistanceToTile; // Trackdir, allow 90-deg

	// measure distance in YAPF units
	uint dist = pfnDistanceToTile(v, tile);
	// convert distance to tiles
	if (dist != UINT_MAX)
		dist = (dist + YAPF_TILE_LENGTH - 1) / YAPF_TILE_LENGTH;
	return dist;
}

Depot* YapfFindNearestRoadDepot(const Vehicle *v)
{
	TileIndex tile = v->tile;
	Trackdir trackdir = GetVehicleTrackdir(v);
	if ((GetTileTrackStatus(tile, TRANSPORT_ROAD) & TrackdirToTrackdirBits(trackdir)) == 0)
		return NULL;

	// handle the case when our vehicle is already in the depot tile
	if (IsTileType(tile, MP_STREET) && IsTileDepotType(tile, TRANSPORT_ROAD)) {
		// only what we need to return is the Depot*
		return GetDepotByTile(tile);
	}

	// default is YAPF type 2
	typedef Depot* (*PfnFindNearestDepot)(const Vehicle*, TileIndex, Trackdir);
	PfnFindNearestDepot pfnFindNearestDepot = &CYapfRoadAnyDepot2::stFindNearestDepot;

	// check if non-default YAPF type should be used
	if (_patches.yapf.disable_node_optimization)
		pfnFindNearestDepot = &CYapfRoadAnyDepot1::stFindNearestDepot; // Trackdir, allow 90-deg

	Depot* ret = pfnFindNearestDepot(v, tile, trackdir);
	return ret;
}
