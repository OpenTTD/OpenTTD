/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_ship.cpp Implementation of YAPF for ships. */

#include "../../stdafx.h"
#include "../../ship.h"

#include "yapf.hpp"
#include "yapf_node_ship.hpp"

#include "../../safeguards.h"

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
	/** to access inherited path finder */
	inline Tpf& Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

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
			Yapf().AddMultipleNodes(&old_node, F);
		}
	}

	/** return debug report character to identify the transportation type */
	inline char TransportTypeChar() const
	{
		return 'w';
	}

	static Trackdir ChooseShipTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, ShipPathCache &path_cache)
	{
		/* handle special case - when next tile is destination tile */
		if (tile == v->dest_tile) {
			/* convert tracks to trackdirs */
			TrackdirBits trackdirs = (TrackdirBits)(tracks | ((int)tracks << 8));
			/* limit to trackdirs reachable from enterdir */
			trackdirs &= DiagdirReachesTrackdirs(enterdir);

			/* use vehicle's current direction if that's possible, otherwise use first usable one. */
			Trackdir veh_dir = v->GetVehicleTrackdir();
			return (HasTrackdir(trackdirs, veh_dir)) ? veh_dir : (Trackdir)FindFirstBit2x64(trackdirs);
		}

		/* move back to the old tile/trackdir (where ship is coming from) */
		TileIndex src_tile = TileAddByDiagDir(tile, ReverseDiagDir(enterdir));
		Trackdir trackdir = v->GetVehicleTrackdir();
		assert(IsValidTrackdir(trackdir));

		/* convert origin trackdir to TrackdirBits */
		TrackdirBits trackdirs = TrackdirToTrackdirBits(trackdir);
		/* get available trackdirs on the destination tile */
		TrackdirBits dest_trackdirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->dest_tile, TRANSPORT_WATER, 0));

		/* create pathfinder instance */
		Tpf pf;
		/* set origin and destination nodes */
		pf.SetOrigin(src_tile, trackdirs);
		pf.SetDestination(v->dest_tile, dest_trackdirs);
		/* find best path */
		path_found = pf.FindPath(v);

		Trackdir next_trackdir = INVALID_TRACKDIR; // this would mean "path not found"

		Node *pNode = pf.GetBestNode();
		if (pNode != NULL) {
			uint steps = 0;
			for (Node *n = pNode; n->m_parent != NULL; n = n->m_parent) steps++;

			/* walk through the path back to the origin */
			Node *pPrevNode = NULL;
			while (pNode->m_parent != NULL) {
				if (steps > 1 && --steps < YAPF_SHIP_PATH_CACHE_LENGTH) {
					TrackdirByte td;
					td = pNode->GetTrackdir();
					path_cache.push_front(td);
				}
				pPrevNode = pNode;
				pNode = pNode->m_parent;
			}
			/* return trackdir from the best next node (direct child of origin) */
			Node &best_next_node = *pPrevNode;
			assert(best_next_node.GetTile() == tile);
			next_trackdir = best_next_node.GetTrackdir();
			/* remove last element for the special case when tile == dest_tile */
			if (path_found && !path_cache.empty()) path_cache.pop_back();
		}
		return next_trackdir;
	}

	/**
	 * Check whether a ship should reverse to reach its destination.
	 * Called when leaving depot.
	 * @param v Ship
	 * @param tile Current position
	 * @param td1 Forward direction
	 * @param td2 Reverse direction
	 * @return true if the reverse direction is better
	 */
	static bool CheckShipReverse(const Ship *v, TileIndex tile, Trackdir td1, Trackdir td2)
	{
		/* get available trackdirs on the destination tile */
		TrackdirBits dest_trackdirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->dest_tile, TRANSPORT_WATER, 0));

		/* create pathfinder instance */
		Tpf pf;
		/* set origin and destination nodes */
		pf.SetOrigin(tile, TrackdirToTrackdirBits(td1) | TrackdirToTrackdirBits(td2));
		pf.SetDestination(v->dest_tile, dest_trackdirs);
		/* find best path */
		if (!pf.FindPath(v)) return false;

		Node *pNode = pf.GetBestNode();
		if (pNode == NULL) return false;

		/* path was found
		 * walk through the path back to the origin */
		while (pNode->m_parent != NULL) {
			pNode = pNode->m_parent;
		}

		Trackdir best_trackdir = pNode->GetTrackdir();
		assert(best_trackdir == td1 || best_trackdir == td2);
		return best_trackdir == td2;
	}

	static bool stFindNearestDepot(const Ship *v, TileIndex tile, Trackdir td1, Trackdir td2, int max_penalty, TileIndex *depot_tile, bool *reversed)
	{
		Tpf pf;
		bool result = pf.FindNearestDepot(v, tile, td1, td2, max_penalty, depot_tile, reversed);
		return result;
	}

	/**
	 * Find the best depot for a ship.
	 * @param v Vehicle
	 * @param tile Tile of the vehicle.
	 * @param td1 Trackdir of the vehicle.
	 * @param td2 reversed Trackdir of the vehicle.
	 * @param max_cost maximum pathfinder cost.
	 * @param depot_tile the tile of the depot.
	 * @param reversed whether the path to depot was found on reversed Trackdir.
	 */
	inline bool FindNearestDepot(const Ship *v, TileIndex tile, Trackdir td1, Trackdir td2, int max_cost, TileIndex *depot_tile, bool *reversed)
	{
		Tpf pf;
		/* Set origin. */
		pf.SetOrigin(tile, TrackdirToTrackdirBits(td1) | TrackdirToTrackdirBits(td2));
		pf.SetMaxCost(max_cost);

		/* find the best path */
		bool bFound = pf.FindPath(v);
		if (!bFound) return false;

		/* some path found
		 * get found depot tile */
		Node *n = pf.GetBestNode();
		*depot_tile = n->m_key.m_tile;

		/* walk through the path back to the origin */
		Node *pNode = n;
		while (pNode->m_parent != NULL) {
			pNode = pNode->m_parent;
		}

		/* if the origin node is the ship's Trackdir then we didn't reverse */
		*reversed = (pNode->m_key.m_td != td1);
		return true;
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
	int m_max_cost;

	CYapfCostShipT() : m_max_cost(0) {}

	/** to access inherited path finder */
	Tpf& Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	inline void SetMaxCost(int max_cost)
	{
		m_max_cost = max_cost;
	}

	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member
	 */
	inline bool PfCalcCost(Node &n, const TrackFollower *tf)
	{
		/* base tile cost depending on distance */
		int c = IsDiagonalTrackdir(n.GetTrackdir()) ? YAPF_TILE_LENGTH : YAPF_TILE_CORNER_LENGTH;
		/* additional penalty for curves */
		if (n.GetTrackdir() != NextTrackdir(n.m_parent->GetTrackdir())) {
			/* new trackdir does not match the next one when going straight */
			c += YAPF_TILE_LENGTH;
		}

		/* Skipped tile cost for aqueducts. */
		c += YAPF_TILE_LENGTH * tf->m_tiles_skipped;

		/* Ocean/canal speed penalty. */
		const ShipVehicleInfo *svi = ShipVehInfo(Yapf().GetVehicle()->engine_type);
		byte speed_frac = (GetEffectiveWaterClass(n.GetTile()) == WATER_CLASS_SEA) ? svi->ocean_speed_frac : svi->canal_speed_frac;
		if (speed_frac > 0) c += YAPF_TILE_LENGTH * (1 + tf->m_tiles_skipped) * speed_frac / (256 - speed_frac);

		/* Finish if we already exceeded the maximum path cost (i.e. when
		 * searching for the nearest depot). */
		if (m_max_cost > 0 && (n.m_parent->m_cost + c) > m_max_cost) return false;

		/* apply it */
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}
};

template <class Types>
class CYapfDestinationAnyDepotShipT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

	/** to access inherited path finder */
	Tpf& Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(Node &n)
	{
		bool bDest = (IsShipDepotTile(n.m_key.m_tile) && GetShipDepotPart(n.m_key.m_tile) == DEPOT_PART_NORTH) && IsTileOwner(n.m_key.m_tile, Yapf().GetVehicle()->owner);
		return bDest;
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		n.m_estimate = n.m_cost;
		return true;
	}
};

/**
 * Config struct of YAPF for ships.
 *  Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Ttrack_follower, class Tnode_list, template <class Types> class CYapfDestinationTileT>
struct CYapfShip_TypesT
{
	/** Types - shortcut for this struct type */
	typedef CYapfShip_TypesT<Tpf_, Ttrack_follower, Tnode_list, CYapfDestinationTileT>  Types;

	/** Tpf - pathfinder type */
	typedef Tpf_                              Tpf;
	/** track follower helper class */
	typedef Ttrack_follower                   TrackFollower;
	/** node list type */
	typedef Tnode_list                        NodeList;
	typedef Ship                              VehicleType;
	/** pathfinder components (modules) */
	typedef CYapfBaseT<Types>                 PfBase;        // base pathfinder class
	typedef CYapfFollowShipT<Types>           PfFollow;      // node follower
	typedef CYapfOriginTileT<Types>           PfOrigin;      // origin provider
	typedef CYapfDestinationTileT<Types>      PfDestination; // destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       // segment cost cache provider
	typedef CYapfCostShipT<Types>             PfCost;        // cost provider
};

/* YAPF type 1 - uses TileIndex/Trackdir as Node key, allows 90-deg turns */
struct CYapfShip1         : CYapfT<CYapfShip_TypesT<CYapfShip1        , CFollowTrackWater    , CShipNodeListTrackDir, CYapfDestinationTileT> > {};
/* YAPF type 2 - uses TileIndex/DiagDirection as Node key, allows 90-deg turns */
struct CYapfShip2         : CYapfT<CYapfShip_TypesT<CYapfShip2        , CFollowTrackWater    , CShipNodeListExitDir , CYapfDestinationTileT> > {};
/* YAPF type 3 - uses TileIndex/Trackdir as Node key, forbids 90-deg turns */
struct CYapfShip3         : CYapfT<CYapfShip_TypesT<CYapfShip3        , CFollowTrackWaterNo90, CShipNodeListTrackDir, CYapfDestinationTileT> > {};

struct CYapfShipAnyDepot1 : CYapfT<CYapfShip_TypesT<CYapfShipAnyDepot1, CFollowTrackWater    , CShipNodeListTrackDir, CYapfDestinationAnyDepotShipT> > {};
struct CYapfShipAnyDepot2 : CYapfT<CYapfShip_TypesT<CYapfShipAnyDepot2, CFollowTrackWater    , CShipNodeListExitDir , CYapfDestinationAnyDepotShipT> > {};
struct CYapfShipAnyDepot3 : CYapfT<CYapfShip_TypesT<CYapfShipAnyDepot3, CFollowTrackWaterNo90, CShipNodeListTrackDir, CYapfDestinationAnyDepotShipT> > {};

/** Ship controller helper - path finder invoker */
Track YapfShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, ShipPathCache &path_cache)
{
	/* default is YAPF type 2 */
	typedef Trackdir (*PfnChooseShipTrack)(const Ship*, TileIndex, DiagDirection, TrackBits, bool &path_found, ShipPathCache &path_cache);
	PfnChooseShipTrack pfnChooseShipTrack = CYapfShip2::ChooseShipTrack; // default: ExitDir, allow 90-deg

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.forbid_90_deg) {
		pfnChooseShipTrack = &CYapfShip3::ChooseShipTrack; // Trackdir, forbid 90-deg
	} else if (_settings_game.pf.yapf.disable_node_optimization) {
		pfnChooseShipTrack = &CYapfShip1::ChooseShipTrack; // Trackdir, allow 90-deg
	}

	Trackdir td_ret = pfnChooseShipTrack(v, tile, enterdir, tracks, path_found, path_cache);
	return (td_ret != INVALID_TRACKDIR) ? TrackdirToTrack(td_ret) : INVALID_TRACK;
}

FindDepotData YapfShipFindNearestDepot(const Ship *v, int max_penalty)
{
	FindDepotData fdd;

	Trackdir td = v->GetVehicleTrackdir();
	Trackdir td_rev = ReverseTrackdir(td);
	TileIndex tile = v->tile;

	/* default is YAPF type 2 */
	typedef bool(*PfnFindNearestDepot)(const Ship*, TileIndex, Trackdir, Trackdir, int, TileIndex*, bool*);
	PfnFindNearestDepot pfnFindNearestDepot = &CYapfShipAnyDepot2::stFindNearestDepot;

	/* check if non-default YAPF type should be used */
	if (_settings_game.pf.forbid_90_deg) {
		pfnFindNearestDepot = &CYapfShipAnyDepot3::stFindNearestDepot; // Trackdir, forbid 90-deg
	} else if (_settings_game.pf.yapf.disable_node_optimization) {
		pfnFindNearestDepot = &CYapfShipAnyDepot1::stFindNearestDepot; // Trackdir, allow 90-deg
	}

	bool ret = pfnFindNearestDepot(v, tile, td, td_rev, max_penalty, &fdd.tile, &fdd.reverse);

	fdd.best_length = ret ? DistanceManhattan(tile, fdd.tile) : UINT_MAX; // distance manhattan or NOT_FOUND
	return fdd;
}

bool YapfShipCheckReverse(const Ship *v)
{
	Trackdir td = v->GetVehicleTrackdir();
	Trackdir td_rev = ReverseTrackdir(td);
	TileIndex tile = v->tile;

	typedef bool (*PfnCheckReverseShip)(const Ship*, TileIndex, Trackdir, Trackdir);
	PfnCheckReverseShip pfnCheckReverseShip = CYapfShip2::CheckShipReverse; // default: ExitDir, allow 90-deg

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.forbid_90_deg) {
		pfnCheckReverseShip = &CYapfShip3::CheckShipReverse; // Trackdir, forbid 90-deg
	} else if (_settings_game.pf.yapf.disable_node_optimization) {
		pfnCheckReverseShip = &CYapfShip1::CheckShipReverse; // Trackdir, allow 90-deg
	}

	bool reverse = pfnCheckReverseShip(v, tile, td, td_rev);

	return reverse;
}
