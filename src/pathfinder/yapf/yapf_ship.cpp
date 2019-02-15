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
			TrackdirBits trackdirs = TrackBitsToTrackdirBits(tracks);
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
	/** to access inherited path finder */
	Tpf& Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	inline int CurveCost(Trackdir td1, Trackdir td2)
	{
		assert(IsValidTrackdir(td1));
		assert(IsValidTrackdir(td2));

		if (HasTrackdir(TrackdirCrossesTrackdirs(td1), td2)) {
			/* 90-deg curve penalty */
			return Yapf().PfGetSettings().ship_curve90_penalty;
		} else if (td2 != NextTrackdir(td1)) {
			/* 45-deg curve penalty */
			return Yapf().PfGetSettings().ship_curve45_penalty;
		}
		return 0;
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
		c += CurveCost(n.m_parent->GetTrackdir(), n.GetTrackdir());

		/* Skipped tile cost for aqueducts. */
		c += YAPF_TILE_LENGTH * tf->m_tiles_skipped;

		/* Ocean/canal speed penalty. */
		const ShipVehicleInfo *svi = ShipVehInfo(Yapf().GetVehicle()->engine_type);
		byte speed_frac = (GetEffectiveWaterClass(n.GetTile()) == WATER_CLASS_SEA) ? svi->ocean_speed_frac : svi->canal_speed_frac;
		if (speed_frac > 0) c += YAPF_TILE_LENGTH * (1 + tf->m_tiles_skipped) * speed_frac / (256 - speed_frac);

		/* apply it */
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}
};

/**
 * Config struct of YAPF for ships.
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
	typedef Ship                              VehicleType;
	/** pathfinder components (modules) */
	typedef CYapfBaseT<Types>                 PfBase;        // base pathfinder class
	typedef CYapfFollowShipT<Types>           PfFollow;      // node follower
	typedef CYapfOriginTileT<Types>           PfOrigin;      // origin provider
	typedef CYapfDestinationTileT<Types>      PfDestination; // destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       // segment cost cache provider
	typedef CYapfCostShipT<Types>             PfCost;        // cost provider
};

/* YAPF type 1 - uses TileIndex/Trackdir as Node key */
struct CYapfShip1 : CYapfT<CYapfShip_TypesT<CYapfShip1, CFollowTrackWater    , CShipNodeListTrackDir> > {};
/* YAPF type 2 - uses TileIndex/DiagDirection as Node key */
struct CYapfShip2 : CYapfT<CYapfShip_TypesT<CYapfShip2, CFollowTrackWater    , CShipNodeListExitDir > > {};

static inline bool RequireTrackdirKey()
{
	/* If the two curve penalties are not equal, then it is not possible to use the
	 * ExitDir keyed node list, as it there will be key overlap. Using Trackdir keyed
	 * nodes means potentially more paths are tested, which would be wasteful if it's
	 * not necessary.
	 */
	return _settings_game.pf.yapf.ship_curve45_penalty != _settings_game.pf.yapf.ship_curve90_penalty;
}

/** Ship controller helper - path finder invoker */
Track YapfShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, ShipPathCache &path_cache)
{
	/* default is YAPF type 2 */
	typedef Trackdir (*PfnChooseShipTrack)(const Ship*, TileIndex, DiagDirection, TrackBits, bool &path_found, ShipPathCache &path_cache);
	PfnChooseShipTrack pfnChooseShipTrack = CYapfShip2::ChooseShipTrack; // default: ExitDir

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.yapf.disable_node_optimization || RequireTrackdirKey()) {
		pfnChooseShipTrack = &CYapfShip1::ChooseShipTrack; // Trackdir
	}

	Trackdir td_ret = pfnChooseShipTrack(v, tile, enterdir, tracks, path_found, path_cache);
	return (td_ret != INVALID_TRACKDIR) ? TrackdirToTrack(td_ret) : INVALID_TRACK;
}

bool YapfShipCheckReverse(const Ship *v)
{
	Trackdir td = v->GetVehicleTrackdir();
	Trackdir td_rev = ReverseTrackdir(td);
	TileIndex tile = v->tile;

	typedef bool (*PfnCheckReverseShip)(const Ship*, TileIndex, Trackdir, Trackdir);
	PfnCheckReverseShip pfnCheckReverseShip = CYapfShip2::CheckShipReverse; // default: ExitDir

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.yapf.disable_node_optimization || RequireTrackdirKey()) {
		pfnCheckReverseShip = &CYapfShip1::CheckShipReverse; // Trackdir
	}

	bool reverse = pfnCheckReverseShip(v, tile, td, td_rev);

	return reverse;
}
