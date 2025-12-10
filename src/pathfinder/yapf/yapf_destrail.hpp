/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf_destrail.hpp Determining the destination for rail vehicles. */

#ifndef YAPF_DESTRAIL_HPP
#define YAPF_DESTRAIL_HPP

#include "../../train.h"
#include "../pathfinder_func.h"
#include "../pathfinder_type.h"

class CYapfDestinationRailBase {
protected:
	RailTypes compatible_railtypes;

public:
	void SetDestination(const Train *v, bool override_rail_type = false)
	{
		this->compatible_railtypes = v->compatible_railtypes;
		if (override_rail_type) this->compatible_railtypes.Set(GetAllCompatibleRailTypes(v->railtypes));
	}

	bool IsCompatibleRailType(RailType rt)
	{
		return this->compatible_railtypes.Test(rt);
	}

	RailTypes GetCompatibleRailTypes() const
	{
		return this->compatible_railtypes;
	}
};

template <class Types>
class CYapfDestinationAnyDepotRailT : public CYapfDestinationRailBase {
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables

	/** to access inherited path finder */
	Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(Node &n)
	{
		return this->PfDetectDestination(n.GetLastTile(), n.GetLastTrackdir());
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(TileIndex tile, Trackdir)
	{
		return IsRailDepotTile(tile);
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::estimate
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		n.estimate = n.cost;
		return true;
	}
};

template <class Types>
class CYapfDestinationAnySafeTileRailT : public CYapfDestinationRailBase {
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables
	typedef typename Types::TrackFollower TrackFollower; ///< TrackFollower. Need to typedef for gcc 2.95

	/** to access inherited path finder */
	Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(Node &n)
	{
		return this->PfDetectDestination(n.GetLastTile(), n.GetLastTrackdir());
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(TileIndex tile, Trackdir td)
	{
		return IsSafeWaitingPosition(Yapf().GetVehicle(), tile, td, true, !TrackFollower::Allow90degTurns()) &&
				IsWaitingPositionFree(Yapf().GetVehicle(), tile, td, !TrackFollower::Allow90degTurns());
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::estimate.
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		n.estimate = n.cost;
		return true;
	}
};

template <class Types>
class CYapfDestinationTileOrStationRailT : public CYapfDestinationRailBase {
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables

protected:
	TileIndex dest_tile;
	TrackdirBits dest_trackdirs;
	StationID dest_station_id;
	bool any_depot;

	/** to access inherited path finder */
	Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	void SetDestination(const Train *v)
	{
		this->any_depot = false;
		switch (v->current_order.GetType()) {
			case OT_GOTO_WAYPOINT:
				if (!Waypoint::Get(v->current_order.GetDestination().ToStationID())->IsSingleTile()) {
					/* In case of 'complex' waypoints we need to do a look
					 * ahead. This look ahead messes a bit about, which
					 * means that it 'corrupts' the cache. To prevent this
					 * we disable caching when we're looking for a complex
					 * waypoint. */
					Yapf().DisableCache(true);
				}
				[[fallthrough]];

			case OT_GOTO_STATION:
				this->dest_tile = CalcClosestStationTile(v->current_order.GetDestination().ToStationID(), v->tile, v->current_order.IsType(OT_GOTO_STATION) ? StationType::Rail : StationType::RailWaypoint);
				this->dest_station_id = v->current_order.GetDestination().ToStationID();
				this->dest_trackdirs = INVALID_TRACKDIR_BIT;
				break;

			case OT_GOTO_DEPOT:
				if (v->current_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) {
					this->any_depot = true;
				}
				[[fallthrough]];

			default:
				this->dest_tile = v->dest_tile;
				this->dest_station_id = StationID::Invalid();
				this->dest_trackdirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->dest_tile, TRANSPORT_RAIL, 0));
				break;
		}
		this->CYapfDestinationRailBase::SetDestination(v);
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(Node &n)
	{
		return this->PfDetectDestination(n.GetLastTile(), n.GetLastTrackdir());
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(TileIndex tile, Trackdir td)
	{
		if (this->dest_station_id != StationID::Invalid()) {
			return HasStationTileRail(tile)
				&& (GetStationIndex(tile) == this->dest_station_id)
				&& (GetRailStationTrack(tile) == TrackdirToTrack(td));
		}

		if (this->any_depot) {
			return IsRailDepotTile(tile);
		}

		return (tile == this->dest_tile) && HasTrackdir(this->dest_trackdirs, td);
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::estimate
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		if (this->PfDetectDestination(n)) {
			n.estimate = n.cost;
			return true;
		}

		n.estimate = n.cost + OctileDistanceCost(n.GetLastTile(), n.GetLastTrackdir(), this->dest_tile);
		assert(n.estimate >= n.parent->estimate);
		return true;
	}
};

#endif /* YAPF_DESTRAIL_HPP */
