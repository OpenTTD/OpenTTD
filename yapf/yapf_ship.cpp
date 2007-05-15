/* $Id$ */

#include "../stdafx.h"

#include "yapf.hpp"

/** Node Follower module of YAPF for ships */
template <class Types>
class CYapfFollowShipT
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
		TrackFollower F;
		if (F.Follow(old_node.m_key.m_tile, old_node.m_key.m_td))
			Yapf().AddMultipleNodes(&old_node, F);
	}

	/// return debug report character to identify the transportation type
	FORCEINLINE char TransportTypeChar() const {return 'w';}

	static Trackdir ChooseShipTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks)
	{
		// handle special case - when next tile is destination tile
		if (tile == v->dest_tile) {
			// convert tracks to trackdirs
			TrackdirBits trackdirs = (TrackdirBits)(tracks | ((int)tracks << 8));
			// choose any trackdir reachable from enterdir
			trackdirs &= DiagdirReachesTrackdirs(enterdir);
			return (Trackdir)FindFirstBit2x64(trackdirs);
		}

		// move back to the old tile/trackdir (where ship is coming from)
		TileIndex src_tile = TILE_ADD(tile, TileOffsByDiagDir(ReverseDiagDir(enterdir)));
		Trackdir trackdir = GetVehicleTrackdir(v);
		assert(IsValidTrackdir(trackdir));

		// convert origin trackdir to TrackdirBits
		TrackdirBits trackdirs = TrackdirToTrackdirBits(trackdir);
		// get available trackdirs on the destination tile
		TrackdirBits dest_trackdirs = (TrackdirBits)(GetTileTrackStatus(v->dest_tile, TRANSPORT_WATER) & TRACKDIR_BIT_MASK);

		// create pathfinder instance
		Tpf pf;
		// set origin and destination nodes
		pf.SetOrigin(src_tile, trackdirs);
		pf.SetDestination(v->dest_tile, dest_trackdirs);
		// find best path
		pf.FindPath(v);

		Trackdir next_trackdir = INVALID_TRACKDIR; // this would mean "path not found"

		Node* pNode = pf.GetBestNode();
		if (pNode != NULL) {
			// walk through the path back to the origin
			Node* pPrevNode = NULL;
			while (pNode->m_parent != NULL) {
				pPrevNode = pNode;
				pNode = pNode->m_parent;
			}
			// return trackdir from the best next node (direct child of origin)
			Node& best_next_node = *pPrevNode;
			assert(best_next_node.GetTile() == tile);
			next_trackdir = best_next_node.GetTrackdir();
		}
		return next_trackdir;
	}
};

/** Cost Provider module of YAPF for ships */
template <class Types>
class CYapfCostShipT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	/** Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member */
	FORCEINLINE bool PfCalcCost(Node& n, const TrackFollower &tf)
	{
		// base tile cost depending on distance
		int c = IsDiagonalTrackdir(n.GetTrackdir()) ? 10 : 7;
		// additional penalty for curves
		if (n.m_parent != NULL && n.GetTrackdir() != n.m_parent->GetTrackdir()) c += 3;
		// apply it
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}
};

/** Config struct of YAPF for ships.
 *  Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Ttrack_follower, class Tnode_list>
struct CYapfShip_TypesT
{
	/** Types - shortcut for this struct type */
	typedef CYapfShip_TypesT<Tpf_, Ttrack_follower, Tnode_list>  Types;

	/** Tpf - pathfinder type */
	typedef Tpf_                              Tpf;
	/** track follower helper class */
	typedef Ttrack_follower                   TrackFollower;
	/** node list type */
	typedef Tnode_list                        NodeList;
	/** pathfinder components (modules) */
	typedef CYapfBaseT<Types>                 PfBase;        // base pathfinder class
	typedef CYapfFollowShipT<Types>           PfFollow;      // node follower
	typedef CYapfOriginTileT<Types>           PfOrigin;      // origin provider
	typedef CYapfDestinationTileT<Types>      PfDestination; // destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       // segment cost cache provider
	typedef CYapfCostShipT<Types>             PfCost;        // cost provider
};

// YAPF type 1 - uses TileIndex/Trackdir as Node key, allows 90-deg turns
struct CYapfShip1 : CYapfT<CYapfShip_TypesT<CYapfShip1, CFollowTrackWater    , CShipNodeListTrackDir> > {};
// YAPF type 2 - uses TileIndex/DiagDirection as Node key, allows 90-deg turns
struct CYapfShip2 : CYapfT<CYapfShip_TypesT<CYapfShip2, CFollowTrackWater    , CShipNodeListExitDir > > {};
// YAPF type 3 - uses TileIndex/Trackdir as Node key, forbids 90-deg turns
struct CYapfShip3 : CYapfT<CYapfShip_TypesT<CYapfShip3, CFollowTrackWaterNo90, CShipNodeListTrackDir> > {};

/** Ship controller helper - path finder invoker */
Trackdir YapfChooseShipTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks)
{
	// default is YAPF type 2
	typedef Trackdir (*PfnChooseShipTrack)(Vehicle*, TileIndex, DiagDirection, TrackBits);
	PfnChooseShipTrack pfnChooseShipTrack = CYapfShip2::ChooseShipTrack; // default: ExitDir, allow 90-deg

	// check if non-default YAPF type needed
	if (_patches.forbid_90_deg)
		pfnChooseShipTrack = &CYapfShip3::ChooseShipTrack; // Trackdir, forbid 90-deg
	else if (_patches.yapf.disable_node_optimization)
		pfnChooseShipTrack = &CYapfShip1::ChooseShipTrack; // Trackdir, allow 90-deg

	Trackdir td_ret = pfnChooseShipTrack(v, tile, enterdir, tracks);
	return td_ret;
}

/** performance measurement helper */
void* NpfBeginInterval()
{
	CPerformanceTimer& perf = *new CPerformanceTimer;
	perf.Start();
	return &perf;
}

/** performance measurement helper */
int NpfEndInterval(void* vperf)
{
	CPerformanceTimer& perf = *(CPerformanceTimer*)vperf;
	perf.Stop();
	int t = perf.Get(1000000);
	delete &perf;
	return t;
}
