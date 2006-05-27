/* $Id$ */

#include "../stdafx.h"

#include "yapf.hpp"
#include "yapf_node_road.hpp"


template <class Types>
class CYapfCostRoadT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	int SlopeCost(TileIndex tile, TileIndex next_tile, Trackdir trackdir)
	{
		// height of the center of the current tile
		int x1 = TileX(tile) * TILE_SIZE;
		int y1 = TileY(tile) * TILE_SIZE;
		int z1 = GetSlopeZ(x1 + TILE_HEIGHT, y1 + TILE_HEIGHT);

		// height of the center of the next tile
		int x2 = TileX(next_tile) * TILE_SIZE;
		int y2 = TileY(next_tile) * TILE_SIZE;
		int z2 = GetSlopeZ(x2 + TILE_HEIGHT, y2 + TILE_HEIGHT);

		if (z2 - z1 > 1) {
			/* Slope up */
			return Yapf().PfGetSettings().rail_slope_penalty;
		}
		return 0;
	}

	/** return one tile cost */
	FORCEINLINE int OneTileCost(TileIndex tile, Trackdir trackdir)
	{
		int cost = 0;
		// set base cost
		if (IsDiagonalTrackdir(trackdir)) {
			cost += 10;
			switch (GetTileType(tile)) {
				case MP_STREET:
					/* Increase the cost for level crossings */
					if (IsLevelCrossing(tile))
						cost += Yapf().PfGetSettings().rail_crossing_penalty;
					break;

				default:
					break;
			}
		} else {
			// non-diagonal trackdir
			cost = 7;
		}
		return cost;
	}

public:
	FORCEINLINE bool PfCalcCost(Node& n)
	{
		int segment_cost = 0;
		// start at n.m_key.m_tile / n.m_key.m_td and walk to the end of segment
		TileIndex tile = n.m_key.m_tile;
		Trackdir trackdir = n.m_key.m_td;
		while (true) {
			// base tile cost depending on distance between edges
			segment_cost += Yapf().OneTileCost(tile, trackdir);

			// if there are no reachable trackdirs n new tile, we have end of road
			TrackFollower F;
			if (!F.Follow(tile, trackdir)) break;

			// if there are more trackdirs available & reachable, we are at the end of segment
			if (KillFirstBit2x64(F.m_new_td_bits) != 0) break;

			Trackdir new_td = (Trackdir)FindFirstBit2x64(F.m_new_td_bits);

			// stop if RV is on simple loop with no junctions
			if (F.m_new_tile == n.m_key.m_tile && new_td == n.m_key.m_td) return false;

			// if we skipped some tunnel tiles, add their cost
			segment_cost += 10 * F.m_tunnel_tiles_skipped;

			// add hilly terrain penalty
			segment_cost += Yapf().SlopeCost(tile, F.m_new_tile, trackdir);

			// add min/max speed penalties
			int min_speed = 0;
			int max_speed = F.GetSpeedLimit(&min_speed);
			Vehicle* v = Yapf().GetVehicle();
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
	typedef typename Types::Tpf Tpf;
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = IsTileDepotType(n.m_segment_last_tile, TRANSPORT_ROAD);
		return bDest;
	}

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
	typedef typename Types::Tpf Tpf;
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

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
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = (n.m_segment_last_tile == m_destTile) && ((m_destTrackdirs & TrackdirToTrackdirBits(n.m_segment_last_td)) != TRACKDIR_BIT_NONE);
		return bDest;
	}

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
		int d = dmin * 7 + (dxy - 1) * 5;
		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};



template <class Types>
class CYapfFollowRoadT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:

	inline void PfFollowNode(Node& old_node)
	{
		TrackFollower F;
		if (F.Follow(old_node.m_segment_last_tile, old_node.m_segment_last_td))
			Yapf().AddMultipleNodes(&old_node, F.m_new_tile, F.m_new_td_bits);
	}

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

	static Depot* stFindNearestDepot(Vehicle* v, TileIndex tile, Trackdir td)
	{
		Tpf pf;
		return pf.FindNearestDepot(v, tile, td);
	}

	FORCEINLINE Depot* FindNearestDepot(Vehicle* v, TileIndex tile, Trackdir td)
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

Depot* YapfFindNearestRoadDepot(const Vehicle *v)
{
	TileIndex tile = v->tile;
	if (v->u.road.state == 255) tile = GetVehicleOutOfTunnelTile(v);
	Trackdir trackdir = GetVehicleTrackdir(v);
	if ((GetTileTrackStatus(tile, TRANSPORT_ROAD) & TrackdirToTrackdirBits(trackdir)) == 0)
		return NULL;

	// default is YAPF type 2
	typedef Depot* (*PfnFindNearestDepot)(Vehicle*, TileIndex, Trackdir);
	PfnFindNearestDepot pfnFindNearestDepot = &CYapfRoadAnyDepot2::stFindNearestDepot;

	// check if non-default YAPF type should be used
	if (_patches.yapf.disable_node_optimization)
		pfnFindNearestDepot = &CYapfRoadAnyDepot1::stFindNearestDepot; // Trackdir, allow 90-deg

	Depot* ret = pfnFindNearestDepot(const_cast<Vehicle*>(v), tile, trackdir);
	return ret;
}
