/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file follow_track.hpp Template function for track followers */

#ifndef  FOLLOW_TRACK_HPP
#define  FOLLOW_TRACK_HPP

#include "../pbs.h"
#include "../roadveh.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnelbridge.h"
#include "../tunnelbridge_map.h"
#include "../depot_map.h"
#include "pf_performance_timer.hpp"

/**
 * Track follower helper template class (can serve pathfinders and vehicle
 *  controllers). See 6 different typedefs below for 3 different transport
 *  types w/ or w/o 90-deg turns allowed
 */
template <TransportType Ttr_type_, typename VehicleType, bool T90deg_turns_allowed_ = true, bool Tmask_reserved_tracks = false>
struct CFollowTrackT
{
	enum ErrorCode {
		EC_NONE,
		EC_OWNER,
		EC_RAIL_TYPE,
		EC_90DEG,
		EC_NO_WAY,
		EC_RESERVED,
	};

	const VehicleType  *m_veh;           ///< moving vehicle
	Owner               m_veh_owner;     ///< owner of the vehicle
	TileIndex           m_old_tile;      ///< the origin (vehicle moved from) before move
	Trackdir            m_old_td;        ///< the trackdir (the vehicle was on) before move
	TileIndex           m_new_tile;      ///< the new tile (the vehicle has entered)
	TrackdirBits        m_new_td_bits;   ///< the new set of available trackdirs
	DiagDirection       m_exitdir;       ///< exit direction (leaving the old tile)
	bool                m_is_tunnel;     ///< last turn passed tunnel
	bool                m_is_bridge;     ///< last turn passed bridge ramp
	bool                m_is_station;    ///< last turn passed station
	int                 m_tiles_skipped; ///< number of skipped tunnel or station tiles
	ErrorCode           m_err;
	CPerformanceTimer  *m_pPerf;
	RailTypes           m_railtypes;

	inline CFollowTrackT(const VehicleType *v = NULL, RailTypes railtype_override = INVALID_RAILTYPES, CPerformanceTimer *pPerf = NULL)
	{
		Init(v, railtype_override, pPerf);
	}

	inline CFollowTrackT(Owner o, RailTypes railtype_override = INVALID_RAILTYPES, CPerformanceTimer *pPerf = NULL)
	{
		m_veh = NULL;
		Init(o, railtype_override, pPerf);
	}

	inline void Init(const VehicleType *v, RailTypes railtype_override, CPerformanceTimer *pPerf)
	{
		assert(!IsRailTT() || (v != NULL && v->type == VEH_TRAIN));
		m_veh = v;
		Init(v != NULL ? v->owner : INVALID_OWNER, IsRailTT() && railtype_override == INVALID_RAILTYPES ? Train::From(v)->compatible_railtypes : railtype_override, pPerf);
	}

	inline void Init(Owner o, RailTypes railtype_override, CPerformanceTimer *pPerf)
	{
		assert((!IsRoadTT() || m_veh != NULL) && (!IsRailTT() || railtype_override != INVALID_RAILTYPES));
		m_veh_owner = o;
		m_pPerf = pPerf;
		/* don't worry, all is inlined so compiler should remove unnecessary initializations */
		m_old_tile = INVALID_TILE;
		m_old_td = INVALID_TRACKDIR;
		m_new_tile = INVALID_TILE;
		m_new_td_bits = TRACKDIR_BIT_NONE;
		m_exitdir = INVALID_DIAGDIR;
		m_is_station = m_is_bridge = m_is_tunnel = false;
		m_tiles_skipped = 0;
		m_err = EC_NONE;
		m_railtypes = railtype_override;
	}

	inline static TransportType TT() { return Ttr_type_; }
	inline static bool IsWaterTT() { return TT() == TRANSPORT_WATER; }
	inline static bool IsRailTT() { return TT() == TRANSPORT_RAIL; }
	inline bool IsTram() { return IsRoadTT() && HasBit(RoadVehicle::From(m_veh)->compatible_roadtypes, ROADTYPE_TRAM); }
	inline static bool IsRoadTT() { return TT() == TRANSPORT_ROAD; }
	inline static bool Allow90degTurns() { return T90deg_turns_allowed_; }
	inline static bool DoTrackMasking() { return Tmask_reserved_tracks; }

	/** Tests if a tile is a road tile with a single tramtrack (tram can reverse) */
	inline DiagDirection GetSingleTramBit(TileIndex tile)
	{
		assert(IsTram()); // this function shouldn't be called in other cases

		if (IsNormalRoadTile(tile)) {
			RoadBits rb = GetRoadBits(tile, ROADTYPE_TRAM);
			switch (rb) {
				case ROAD_NW: return DIAGDIR_NW;
				case ROAD_SW: return DIAGDIR_SW;
				case ROAD_SE: return DIAGDIR_SE;
				case ROAD_NE: return DIAGDIR_NE;
				default: break;
			}
		}
		return INVALID_DIAGDIR;
	}

	/**
	 * main follower routine. Fills all members and return true on success.
	 *  Otherwise returns false if track can't be followed.
	 */
	inline bool Follow(TileIndex old_tile, Trackdir old_td)
	{
		m_old_tile = old_tile;
		m_old_td = old_td;
		m_err = EC_NONE;
		assert(((TrackStatusToTrackdirBits(GetTileTrackStatus(m_old_tile, TT(), IsRoadTT() ? RoadVehicle::From(m_veh)->compatible_roadtypes : 0)) & TrackdirToTrackdirBits(m_old_td)) != 0) ||
		       (IsTram() && GetSingleTramBit(m_old_tile) != INVALID_DIAGDIR)); // Disable the assertion for single tram bits
		m_exitdir = TrackdirToExitdir(m_old_td);
		if (ForcedReverse()) return true;
		if (!CanExitOldTile()) return false;
		FollowTileExit();
		if (!QueryNewTileTrackStatus()) return TryReverse();
		m_new_td_bits &= DiagdirReachesTrackdirs(m_exitdir);
		if (m_new_td_bits == TRACKDIR_BIT_NONE || !CanEnterNewTile()) {
			/* In case we can't enter the next tile, but are
			 * a normal road vehicle, then we can actually
			 * try to reverse as this is the end of the road.
			 * Trams can only turn on the appropriate bits in
			 * which case reaching this would mean a dead end
			 * near a building and in that case there would
			 * a "false" QueryNewTileTrackStatus result and
			 * as such reversing is already tried. The fact
			 * that function failed can have to do with a
			 * missing road bit, or inability to connect the
			 * different bits due to slopes. */
			if (IsRoadTT() && !IsTram() && TryReverse()) return true;

			/* CanEnterNewTile already set a reason.
			 * Do NOT overwrite it (important for example for EC_RAIL_TYPE).
			 * Only set a reason if CanEnterNewTile was not called */
			if (m_new_td_bits == TRACKDIR_BIT_NONE) m_err = EC_NO_WAY;

			return false;
		}
		if (!Allow90degTurns()) {
			m_new_td_bits &= (TrackdirBits)~(int)TrackdirCrossesTrackdirs(m_old_td);
			if (m_new_td_bits == TRACKDIR_BIT_NONE) {
				m_err = EC_90DEG;
				return false;
			}
		}
		return true;
	}

	inline bool MaskReservedTracks()
	{
		if (!DoTrackMasking()) return true;

		if (m_is_station) {
			/* Check skipped station tiles as well. */
			TileIndexDiff diff = TileOffsByDiagDir(m_exitdir);
			for (TileIndex tile = m_new_tile - diff * m_tiles_skipped; tile != m_new_tile; tile += diff) {
				if (HasStationReservation(tile)) {
					m_new_td_bits = TRACKDIR_BIT_NONE;
					m_err = EC_RESERVED;
					return false;
				}
			}
		}

		TrackBits reserved = GetReservedTrackbits(m_new_tile);
		/* Mask already reserved trackdirs. */
		m_new_td_bits &= ~TrackBitsToTrackdirBits(reserved);
		/* Mask out all trackdirs that conflict with the reservation. */
		Track t;
		FOR_EACH_SET_TRACK(t, TrackdirBitsToTrackBits(m_new_td_bits)) {
			if (TracksOverlap(reserved | TrackToTrackBits(t))) m_new_td_bits &= ~TrackToTrackdirBits(t);
		}
		if (m_new_td_bits == TRACKDIR_BIT_NONE) {
			m_err = EC_RESERVED;
			return false;
		}
		return true;
	}

protected:
	/** Follow the m_exitdir from m_old_tile and fill m_new_tile and m_tiles_skipped */
	inline void FollowTileExit()
	{
		m_is_station = m_is_bridge = m_is_tunnel = false;
		m_tiles_skipped = 0;

		/* extra handling for tunnels and bridges in our direction */
		if (IsTileType(m_old_tile, MP_TUNNELBRIDGE)) {
			DiagDirection enterdir = GetTunnelBridgeDirection(m_old_tile);
			if (enterdir == m_exitdir) {
				/* we are entering the tunnel / bridge */
				if (IsTunnel(m_old_tile)) {
					m_is_tunnel = true;
					m_new_tile = GetOtherTunnelEnd(m_old_tile);
				} else { // IsBridge(m_old_tile)
					m_is_bridge = true;
					m_new_tile = GetOtherBridgeEnd(m_old_tile);
				}
				m_tiles_skipped = GetTunnelBridgeLength(m_new_tile, m_old_tile);
				return;
			}
			assert(ReverseDiagDir(enterdir) == m_exitdir);
		}

		/* normal or station tile, do one step */
		m_new_tile = TileAddByDiagDir(m_old_tile, m_exitdir);

		/* special handling for stations */
		if (IsRailTT() && HasStationTileRail(m_new_tile)) {
			m_is_station = true;
		} else if (IsRoadTT() && IsRoadStopTile(m_new_tile)) {
			m_is_station = true;
		} else {
			m_is_station = false;
		}
	}

	/** stores track status (available trackdirs) for the new tile into m_new_td_bits */
	inline bool QueryNewTileTrackStatus()
	{
		CPerfStart perf(*m_pPerf);
		if (IsRailTT() && IsPlainRailTile(m_new_tile)) {
			m_new_td_bits = (TrackdirBits)(GetTrackBits(m_new_tile) * 0x101);
		} else {
			m_new_td_bits = TrackStatusToTrackdirBits(GetTileTrackStatus(m_new_tile, TT(), IsRoadTT() ? RoadVehicle::From(m_veh)->compatible_roadtypes : 0));

			if (IsTram() && m_new_td_bits == 0) {
				/* GetTileTrackStatus() returns 0 for single tram bits.
				 * As we cannot change it there (easily) without breaking something, change it here */
				switch (GetSingleTramBit(m_new_tile)) {
					case DIAGDIR_NE:
					case DIAGDIR_SW:
						m_new_td_bits = TRACKDIR_BIT_X_NE | TRACKDIR_BIT_X_SW;
						break;

					case DIAGDIR_NW:
					case DIAGDIR_SE:
						m_new_td_bits = TRACKDIR_BIT_Y_NW | TRACKDIR_BIT_Y_SE;
						break;

					default: break;
				}
			}
		}
		return (m_new_td_bits != TRACKDIR_BIT_NONE);
	}

	/** return true if we can leave m_old_tile in m_exitdir */
	inline bool CanExitOldTile()
	{
		/* road stop can be left at one direction only unless it's a drive-through stop */
		if (IsRoadTT() && IsStandardRoadStopTile(m_old_tile)) {
			DiagDirection exitdir = GetRoadStopDir(m_old_tile);
			if (exitdir != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* single tram bits can only be left in one direction */
		if (IsTram()) {
			DiagDirection single_tram = GetSingleTramBit(m_old_tile);
			if (single_tram != INVALID_DIAGDIR && single_tram != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* road depots can be also left in one direction only */
		if (IsRoadTT() && IsDepotTypeTile(m_old_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(m_old_tile);
			if (exitdir != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}
		return true;
	}

	/** return true if we can enter m_new_tile from m_exitdir */
	inline bool CanEnterNewTile()
	{
		if (IsRoadTT() && IsStandardRoadStopTile(m_new_tile)) {
			/* road stop can be entered from one direction only unless it's a drive-through stop */
			DiagDirection exitdir = GetRoadStopDir(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* single tram bits can only be entered from one direction */
		if (IsTram()) {
			DiagDirection single_tram = GetSingleTramBit(m_new_tile);
			if (single_tram != INVALID_DIAGDIR && single_tram != ReverseDiagDir(m_exitdir)) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* road and rail depots can also be entered from one direction only */
		if (IsRoadTT() && IsDepotTypeTile(m_new_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
			/* don't try to enter other company's depots */
			if (GetTileOwner(m_new_tile) != m_veh_owner) {
				m_err = EC_OWNER;
				return false;
			}
		}
		if (IsRailTT() && IsDepotTypeTile(m_new_tile, TT())) {
			DiagDirection exitdir = GetRailDepotDirection(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		/* rail transport is possible only on tiles with the same owner as vehicle */
		if (IsRailTT() && GetTileOwner(m_new_tile) != m_veh_owner) {
			/* different owner */
			m_err = EC_NO_WAY;
			return false;
		}

		/* rail transport is possible only on compatible rail types */
		if (IsRailTT()) {
			RailType rail_type = GetTileRailType(m_new_tile);
			if (!HasBit(m_railtypes, rail_type)) {
				/* incompatible rail type */
				m_err = EC_RAIL_TYPE;
				return false;
			}
		}

		/* tunnel holes and bridge ramps can be entered only from proper direction */
		if (IsTileType(m_new_tile, MP_TUNNELBRIDGE)) {
			if (IsTunnel(m_new_tile)) {
				if (!m_is_tunnel) {
					DiagDirection tunnel_enterdir = GetTunnelBridgeDirection(m_new_tile);
					if (tunnel_enterdir != m_exitdir) {
						m_err = EC_NO_WAY;
						return false;
					}
				}
			} else { // IsBridge(m_new_tile)
				if (!m_is_bridge) {
					DiagDirection ramp_enderdir = GetTunnelBridgeDirection(m_new_tile);
					if (ramp_enderdir != m_exitdir) {
						m_err = EC_NO_WAY;
						return false;
					}
				}
			}
		}

		/* special handling for rail stations - get to the end of platform */
		if (IsRailTT() && m_is_station) {
			/* entered railway station
			 * get platform length */
			uint length = BaseStation::GetByTile(m_new_tile)->GetPlatformLength(m_new_tile, TrackdirToExitdir(m_old_td));
			/* how big step we must do to get to the last platform tile; */
			m_tiles_skipped = length - 1;
			/* move to the platform end */
			TileIndexDiff diff = TileOffsByDiagDir(m_exitdir);
			diff *= m_tiles_skipped;
			m_new_tile = TILE_ADD(m_new_tile, diff);
			return true;
		}

		return true;
	}

	/** return true if we must reverse (in depots and single tram bits) */
	inline bool ForcedReverse()
	{
		/* rail and road depots cause reversing */
		if (!IsWaterTT() && IsDepotTypeTile(m_old_tile, TT())) {
			DiagDirection exitdir = IsRailTT() ? GetRailDepotDirection(m_old_tile) : GetRoadDepotDirection(m_old_tile);
			if (exitdir != m_exitdir) {
				/* reverse */
				m_new_tile = m_old_tile;
				m_new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(m_old_td));
				m_exitdir = exitdir;
				m_tiles_skipped = 0;
				m_is_tunnel = m_is_bridge = m_is_station = false;
				return true;
			}
		}

		/* single tram bits cause reversing */
		if (IsTram() && GetSingleTramBit(m_old_tile) == ReverseDiagDir(m_exitdir)) {
			/* reverse */
			m_new_tile = m_old_tile;
			m_new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(m_old_td));
			m_exitdir = ReverseDiagDir(m_exitdir);
			m_tiles_skipped = 0;
			m_is_tunnel = m_is_bridge = m_is_station = false;
			return true;
		}

		return false;
	}

	/** return true if we successfully reversed at end of road/track */
	inline bool TryReverse()
	{
		if (IsRoadTT() && !IsTram()) {
			/* if we reached the end of road, we can reverse the RV and continue moving */
			m_exitdir = ReverseDiagDir(m_exitdir);
			/* new tile will be the same as old one */
			m_new_tile = m_old_tile;
			/* set new trackdir bits to all reachable trackdirs */
			QueryNewTileTrackStatus();
			m_new_td_bits &= DiagdirReachesTrackdirs(m_exitdir);
			if (m_new_td_bits != TRACKDIR_BIT_NONE) {
				/* we have some trackdirs reachable after reversal */
				return true;
			}
		}
		m_err = EC_NO_WAY;
		return false;
	}

public:
	/** Helper for pathfinders - get min/max speed on the m_old_tile/m_old_td */
	int GetSpeedLimit(int *pmin_speed = NULL) const
	{
		int min_speed = 0;
		int max_speed = INT_MAX; // no limit

		/* Check for on-bridge speed limit */
		if (!IsWaterTT() && IsBridgeTile(m_old_tile)) {
			int spd = GetBridgeSpec(GetBridgeType(m_old_tile))->speed;
			if (IsRoadTT()) spd *= 2;
			if (max_speed > spd) max_speed = spd;
		}
		/* Check for speed limit imposed by railtype */
		if (IsRailTT()) {
			uint16 rail_speed = GetRailTypeInfo(GetRailType(m_old_tile))->max_speed;
			if (rail_speed > 0) max_speed = min(max_speed, rail_speed);
		}

		/* if min speed was requested, return it */
		if (pmin_speed != NULL) *pmin_speed = min_speed;
		return max_speed;
	}
};

typedef CFollowTrackT<TRANSPORT_WATER, Ship,        true > CFollowTrackWater;
typedef CFollowTrackT<TRANSPORT_ROAD,  RoadVehicle, true > CFollowTrackRoad;
typedef CFollowTrackT<TRANSPORT_RAIL,  Train,       true > CFollowTrackRail;

typedef CFollowTrackT<TRANSPORT_WATER, Ship,        false> CFollowTrackWaterNo90;
typedef CFollowTrackT<TRANSPORT_ROAD,  RoadVehicle, false> CFollowTrackRoadNo90;
typedef CFollowTrackT<TRANSPORT_RAIL,  Train,       false> CFollowTrackRailNo90;

typedef CFollowTrackT<TRANSPORT_RAIL, Train, true,  true > CFollowTrackFreeRail;
typedef CFollowTrackT<TRANSPORT_RAIL, Train, false, true > CFollowTrackFreeRailNo90;

#endif /* FOLLOW_TRACK_HPP */
