/* $Id$ */

#include "../stdafx.h"

#include "yapf.hpp"
#include "yapf_node_rail.hpp"
#include "yapf_costrail.hpp"
#include "yapf_destrail.hpp"

int _total_pf_time_us = 0;





template <class Types>
class CYapfFollowAnyDepotRailT
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
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.GetLastTile(), old_node.GetLastTrackdir()))
			Yapf().AddMultipleNodes(&old_node, F.m_new_tile, F.m_new_td_bits);
	}

	FORCEINLINE char TransportTypeChar() const {return 't';}

	static bool stFindNearestDepotTwoWay(Vehicle *v, TileIndex t1, Trackdir td1, TileIndex t2, Trackdir td2, int max_distance, int reverse_penalty, TileIndex* depot_tile, bool* reversed)
	{
		Tpf pf;
		return pf.FindNearestDepotTwoWay(v, t1, td1, t2, td2, max_distance, reverse_penalty, depot_tile, reversed);
	}

	FORCEINLINE bool FindNearestDepotTwoWay(Vehicle *v, TileIndex t1, Trackdir td1, TileIndex t2, Trackdir td2, int max_distance, int reverse_penalty, TileIndex* depot_tile, bool* reversed)
	{
		// set origin and destination nodes
		Yapf().SetOrigin(t1, td1, t2, td2, reverse_penalty, true);
		Yapf().SetDestination(v);
		Yapf().SetMaxCost(10 * max_distance);

		// find the best path
		bool bFound = Yapf().FindPath(v);
		if (!bFound) return false;

		// some path found
		// get found depot tile
		Node& n = Yapf().GetBestNode();
		*depot_tile = n.GetLastTile();

		// walk through the path back to the origin
		Node* pNode = &n;
		while (pNode->m_parent != NULL) {
			pNode = pNode->m_parent;
		}

		// if the origin node is our front vehicle tile/Trackdir then we didn't reverse
		// but we can also look at the cost (== 0 -> not reversed, == reverse_penalty -> reversed)
		*reversed = (pNode->m_cost != 0);

		return true;
	}
};

template <class Types>
class CYapfFollowRailT
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
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.GetLastTile(), old_node.GetLastTrackdir()))
			Yapf().AddMultipleNodes(&old_node, F.m_new_tile, F.m_new_td_bits);
	}

	FORCEINLINE char TransportTypeChar() const {return 't';}

	static Trackdir stChooseRailTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs)
	{
		// create pathfinder instance
		Tpf pf;
		return pf.ChooseRailTrack(v, tile, enterdir, trackdirs);
	}

	FORCEINLINE Trackdir ChooseRailTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs)
	{
		// set origin and destination nodes
		Yapf().SetOrigin(v->tile, GetVehicleTrackdir(v), INVALID_TILE, INVALID_TRACKDIR, 1, true);
		Yapf().SetDestination(v);

		// find the best path
		Yapf().FindPath(v);

		// if path not found - return INVALID_TRACKDIR
		Trackdir next_trackdir = INVALID_TRACKDIR;
		Node* pNode = &Yapf().GetBestNode();
		if (pNode != NULL) {
			// path was found or at least suggested
			// walk through the path back to the origin
			Node* pPrev = NULL;
			while (pNode->m_parent != NULL) {
				pPrev = pNode;
				pNode = pNode->m_parent;
			}
			// return trackdir from the best origin node (one of start nodes)
			Node& best_next_node = *pPrev;
			assert(best_next_node.GetTile() == tile);
			next_trackdir = best_next_node.GetTrackdir();
		}
		return next_trackdir;
	}

	static bool stCheckReverseTrain(Vehicle* v, TileIndex t1, Trackdir td1, TileIndex t2, Trackdir td2)
	{
		Tpf pf;
		return pf.CheckReverseTrain(v, t1, td1, t2, td2);
	}

	FORCEINLINE bool CheckReverseTrain(Vehicle* v, TileIndex t1, Trackdir td1, TileIndex t2, Trackdir td2)
	{
		// create pathfinder instance
		// set origin and destination nodes
		Yapf().SetOrigin(t1, td1, t2, td2, 1, false);
		Yapf().SetDestination(v);

		// find the best path
		bool bFound = Yapf().FindPath(v);

		if (!bFound) return false;

		// path was found
		// walk through the path back to the origin
		Node* pNode = &Yapf().GetBestNode();
		while (pNode->m_parent != NULL) {
			pNode = pNode->m_parent;
		}

		// check if it was reversed origin
		Node& best_org_node = *pNode;
		bool reversed = (best_org_node.m_cost != 0);
		return reversed;
	}
};

template <class Tpf_, class Ttrack_follower, class Tnode_list, template <class Types> class TdestinationT, template <class Types> class TfollowT>
struct CYapfRail_TypesT
{
	typedef CYapfRail_TypesT<Tpf_, Ttrack_follower, Tnode_list, TdestinationT, TfollowT>  Types;

	typedef Tpf_                                Tpf;
	typedef Ttrack_follower                     TrackFollower;
	typedef Tnode_list                          NodeList;
	typedef CYapfBaseT<Types>                   PfBase;
	typedef TfollowT<Types>                     PfFollow;
	typedef CYapfOriginTileTwoWayT<Types>       PfOrigin;
	typedef TdestinationT<Types>                PfDestination;
	typedef CYapfSegmentCostCacheGlobalT<Types> PfCache;
	typedef CYapfCostRailT<Types>               PfCost;
};

struct CYapfRail1         : CYapfT<CYapfRail_TypesT<CYapfRail1        , CFollowTrackRail    , CRailNodeListTrackDir, CYapfDestinationTileOrStationRailT, CYapfFollowRailT> > {};
struct CYapfRail2         : CYapfT<CYapfRail_TypesT<CYapfRail2        , CFollowTrackRail    , CRailNodeListExitDir , CYapfDestinationTileOrStationRailT, CYapfFollowRailT> > {};
struct CYapfRail3         : CYapfT<CYapfRail_TypesT<CYapfRail3        , CFollowTrackRailNo90, CRailNodeListTrackDir, CYapfDestinationTileOrStationRailT, CYapfFollowRailT> > {};

struct CYapfAnyDepotRail1 : CYapfT<CYapfRail_TypesT<CYapfAnyDepotRail1, CFollowTrackRail    , CRailNodeListTrackDir, CYapfDestinationAnyDepotRailT     , CYapfFollowAnyDepotRailT> > {};
struct CYapfAnyDepotRail2 : CYapfT<CYapfRail_TypesT<CYapfAnyDepotRail2, CFollowTrackRail    , CRailNodeListExitDir , CYapfDestinationAnyDepotRailT     , CYapfFollowAnyDepotRailT> > {};
struct CYapfAnyDepotRail3 : CYapfT<CYapfRail_TypesT<CYapfAnyDepotRail3, CFollowTrackRailNo90, CRailNodeListTrackDir, CYapfDestinationAnyDepotRailT     , CYapfFollowAnyDepotRailT> > {};


Trackdir YapfChooseRailTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs)
{
	// default is YAPF type 2
	typedef Trackdir (*PfnChooseRailTrack)(Vehicle*, TileIndex, DiagDirection, TrackdirBits);
	PfnChooseRailTrack pfnChooseRailTrack = &CYapfRail2::stChooseRailTrack;

	// check if non-default YAPF type needed
	if (_patches.forbid_90_deg)
		pfnChooseRailTrack = &CYapfRail3::stChooseRailTrack; // Trackdir, forbid 90-deg
	else if (_patches.yapf.disable_node_optimization)
		pfnChooseRailTrack = &CYapfRail1::stChooseRailTrack; // Trackdir, allow 90-deg

	Trackdir td_ret = pfnChooseRailTrack(v, tile, enterdir, trackdirs);

	return td_ret;
}

bool YapfCheckReverseTrain(Vehicle* v)
{
	// tile where the engine is
	TileIndex tile = v->tile;
	// tile where we have last wagon
	Vehicle* last_veh = GetLastVehicleInChain(v);
	// if we are in tunnel then give up
	if (v->u.rail.track == 0x40 || last_veh->u.rail.track == 0x40) return false;
	// get trackdirs of both ends
	Trackdir td = GetVehicleTrackdir(v);
	Trackdir td_rev = ReverseTrackdir(GetVehicleTrackdir(last_veh));


	typedef bool (*PfnCheckReverseTrain)(Vehicle*, TileIndex, Trackdir, TileIndex, Trackdir);
	PfnCheckReverseTrain pfnCheckReverseTrain = CYapfRail2::stCheckReverseTrain;

	// check if non-default YAPF type needed
	if (_patches.forbid_90_deg)
		pfnCheckReverseTrain = &CYapfRail3::stCheckReverseTrain; // Trackdir, forbid 90-deg
	else if (_patches.yapf.disable_node_optimization)
		pfnCheckReverseTrain = &CYapfRail1::stCheckReverseTrain; // Trackdir, allow 90-deg

	bool reverse = pfnCheckReverseTrain(v, tile, td, last_veh->tile, td_rev);

	return reverse;
}

bool YapfFindNearestRailDepotTwoWay(Vehicle *v, int max_distance, int reverse_penalty, TileIndex* depot_tile, bool* reversed)
{
	*depot_tile = INVALID_TILE;
	*reversed = false;

	Vehicle* last_veh = GetLastVehicleInChain(v);

	bool first_in_tunnel = v->u.rail.track == 0x40;
	bool last_in_tunnel = last_veh->u.rail.track == 0x40;

	// tile where the engine and last wagon are
	TileIndex tile = first_in_tunnel ? INVALID_TILE : v->tile;
	TileIndex last_tile = last_in_tunnel ? INVALID_TILE : last_veh->tile;

	// their trackdirs
	Trackdir td = first_in_tunnel ? INVALID_TRACKDIR : GetVehicleTrackdir(v);
	Trackdir td_rev = last_in_tunnel ? INVALID_TRACKDIR : ReverseTrackdir(GetVehicleTrackdir(last_veh));

	typedef bool (*PfnFindNearestDepotTwoWay)(Vehicle*, TileIndex, Trackdir, TileIndex, Trackdir, int, int, TileIndex*, bool*);
	PfnFindNearestDepotTwoWay pfnFindNearestDepotTwoWay = &CYapfAnyDepotRail2::stFindNearestDepotTwoWay;

	// check if non-default YAPF type needed
	if (_patches.forbid_90_deg)
		pfnFindNearestDepotTwoWay = &CYapfAnyDepotRail3::stFindNearestDepotTwoWay; // Trackdir, forbid 90-deg
	else if (_patches.yapf.disable_node_optimization)
		pfnFindNearestDepotTwoWay = &CYapfAnyDepotRail1::stFindNearestDepotTwoWay; // Trackdir, allow 90-deg

	bool ret = pfnFindNearestDepotTwoWay(v, tile, td, last_tile, td_rev, max_distance, reverse_penalty, depot_tile, reversed);
	return ret;
}

int CSegmentCostCacheBase::s_rail_change_counter = 0;

void YapfNotifyTrackLayoutChange(TileIndex tile, Track track) {CSegmentCostCacheBase::NotifyTrackLayoutChange(tile, track);}
