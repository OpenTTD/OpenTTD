/* $Id$ */

/** @file follow_track.hpp Template function for track followers */

#ifndef  FOLLOW_TRACK_HPP
#define  FOLLOW_TRACK_HPP

#include "yapf.hpp"

/** Track follower helper template class (can serve pathfinders and vehicle
 *  controllers). See 6 different typedefs below for 3 different transport
 *  types w/ of w/o 90-deg turns allowed */
template <TransportType Ttr_type_, bool T90deg_turns_allowed_ = true>
struct CFollowTrackT : public FollowTrack_t
{
	CPerformanceTimer* m_pPerf;

	FORCEINLINE CFollowTrackT(const Vehicle* v = NULL, CPerformanceTimer* pPerf = NULL)
	{
		Init(v, pPerf);
	}

	FORCEINLINE void Init(const Vehicle* v, CPerformanceTimer* pPerf)
	{
		assert(!IsRailTT() || (v != NULL && v->type == VEH_TRAIN));
		m_veh = v;
		m_pPerf = pPerf;
		// don't worry, all is inlined so compiler should remove unnecessary initializations
		m_new_tile = INVALID_TILE;
		m_new_td_bits = TRACKDIR_BIT_NONE;
		m_exitdir = INVALID_DIAGDIR;
		m_is_station = m_is_bridge = m_is_tunnel = false;
		m_tiles_skipped = 0;
		m_err = EC_NONE;
	}

	FORCEINLINE static TransportType TT() {return Ttr_type_;}
	FORCEINLINE static bool IsWaterTT() {return TT() == TRANSPORT_WATER;}
	FORCEINLINE static bool IsRailTT() {return TT() == TRANSPORT_RAIL;}
	FORCEINLINE static bool IsRoadTT() {return TT() == TRANSPORT_ROAD;}
	FORCEINLINE static bool Allow90degTurns() {return T90deg_turns_allowed_;}

	/** main follower routine. Fills all members and return true on success.
	 *  Otherwise returns false if track can't be followed. */
	FORCEINLINE bool Follow(TileIndex old_tile, Trackdir old_td)
	{
		m_old_tile = old_tile;
		m_old_td = old_td;
		m_err = EC_NONE;
		assert((GetTileTrackStatus(m_old_tile, TT(), m_veh->u.road.compatible_roadtypes) & TrackdirToTrackdirBits(m_old_td)) != 0);
		m_exitdir = TrackdirToExitdir(m_old_td);
		if (EnteredDepot()) return true;
		if (!CanExitOldTile()) return false;
		FollowTileExit();
		if (!QueryNewTileTrackStatus()) return TryReverse();
		if (!CanEnterNewTile()) return false;
		m_new_td_bits &= DiagdirReachesTrackdirs(m_exitdir);
		if (m_new_td_bits == TRACKDIR_BIT_NONE) {
			m_err = EC_NO_WAY;
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

protected:
	/** Follow the m_exitdir from m_old_tile and fill m_new_tile and m_tiles_skipped */
	FORCEINLINE void FollowTileExit()
	{
		m_is_station = m_is_bridge = m_is_tunnel = false;
		m_tiles_skipped = 0;

		// extra handling for tunnels and bridges in our direction
		if (IsTileType(m_old_tile, MP_TUNNELBRIDGE)) {
			DiagDirection enterdir = GetTunnelBridgeDirection(m_old_tile);
			if (enterdir == m_exitdir) {
				// we are entering the tunnel / bridge
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

		// normal or station tile, do one step
		TileIndexDiff diff = TileOffsByDiagDir(m_exitdir);
		m_new_tile = TILE_ADD(m_old_tile, diff);

		// special handling for stations
		if (IsRailTT() && IsRailwayStationTile(m_new_tile)) {
			m_is_station = true;
		} else if (IsRoadTT() && IsRoadStopTile(m_new_tile)) {
			m_is_station = true;
		} else {
			m_is_station = false;
		}
	}

	/** stores track status (available trackdirs) for the new tile into m_new_td_bits */
	FORCEINLINE bool QueryNewTileTrackStatus()
	{
		CPerfStart perf(*m_pPerf);
		if (IsRailTT() && GetTileType(m_new_tile) == MP_RAILWAY && IsPlainRailTile(m_new_tile)) {
			m_new_td_bits = (TrackdirBits)(GetTrackBits(m_new_tile) * 0x101);
		} else {
			uint32 ts = GetTileTrackStatus(m_new_tile, TT(), m_veh->u.road.compatible_roadtypes);
			m_new_td_bits = (TrackdirBits)(ts & TRACKDIR_BIT_MASK);
		}
		return (m_new_td_bits != TRACKDIR_BIT_NONE);
	}

	/** return true if we can leave m_old_tile in m_exitdir */
	FORCEINLINE bool CanExitOldTile()
	{
		// road stop can be left at one direction only unless it's a drive-through stop
		if (IsRoadTT() && IsStandardRoadStopTile(m_old_tile)) {
			DiagDirection exitdir = GetRoadStopDir(m_old_tile);
			if (exitdir != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		// road depots can be also left in one direction only
		if (IsRoadTT() && IsTileDepotType(m_old_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(m_old_tile);
			if (exitdir != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}
		return true;
	}

	/** return true if we can enter m_new_tile from m_exitdir */
	FORCEINLINE bool CanEnterNewTile()
	{
		if (IsRoadTT() && IsStandardRoadStopTile(m_new_tile)) {
			// road stop can be entered from one direction only unless it's a drive-through stop
			DiagDirection exitdir = GetRoadStopDir(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		// road and rail depots can also be entered from one direction only
		if (IsRoadTT() && IsTileDepotType(m_new_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
			// don't try to enter other player's depots
			if (GetTileOwner(m_new_tile) != m_veh->owner) {
				m_err = EC_OWNER;
				return false;
			}
		}
		if (IsRailTT() && IsTileDepotType(m_new_tile, TT())) {
			DiagDirection exitdir = GetRailDepotDirection(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir) {
				m_err = EC_NO_WAY;
				return false;
			}
		}

		// rail transport is possible only on tiles with the same owner as vehicle
		if (IsRailTT() && GetTileOwner(m_new_tile) != m_veh->owner) {
			// different owner
			m_err = EC_NO_WAY;
			return false;
		}

		// rail transport is possible only on compatible rail types
		if (IsRailTT()) {
			RailType rail_type = GetTileRailType(m_new_tile);
			if (!HasBit(m_veh->u.rail.compatible_railtypes, rail_type)) {
				// incompatible rail type
				m_err = EC_RAIL_TYPE;
				return false;
			}
		}

		// tunnel holes and bridge ramps can be entered only from proper direction
		if (!IsWaterTT() && IsTileType(m_new_tile, MP_TUNNELBRIDGE)) {
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

		// special handling for rail stations - get to the end of platform
		if (IsRailTT() && m_is_station) {
			// entered railway station
			// get platform length
			uint length = GetStationByTile(m_new_tile)->GetPlatformLength(m_new_tile, TrackdirToExitdir(m_old_td));
			// how big step we must do to get to the last platform tile;
			m_tiles_skipped = length - 1;
			// move to the platform end
			TileIndexDiff diff = TileOffsByDiagDir(m_exitdir);
			diff *= m_tiles_skipped;
			m_new_tile = TILE_ADD(m_new_tile, diff);
			return true;
		}

		return true;
	}

	/** return true if we entered depot and reversed inside */
	FORCEINLINE bool EnteredDepot()
	{
		// rail and road depots cause reversing
		if (!IsWaterTT() && IsTileDepotType(m_old_tile, TT())) {
			DiagDirection exitdir = IsRailTT() ? GetRailDepotDirection(m_old_tile) : GetRoadDepotDirection(m_old_tile);
			if (exitdir != m_exitdir) {
				// reverse
				m_new_tile = m_old_tile;
				m_new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(m_old_td));
				m_exitdir = exitdir;
				m_tiles_skipped = 0;
				m_is_tunnel = m_is_bridge = m_is_station = false;
				return true;
			}
		}
		return false;
	}

	/** return true if we successfully reversed at end of road/track */
	FORCEINLINE bool TryReverse()
	{
		if (IsRoadTT()) {
			// if we reached the end of road, we can reverse the RV and continue moving
			m_exitdir = ReverseDiagDir(m_exitdir);
			// new tile will be the same as old one
			m_new_tile = m_old_tile;
			// set new trackdir bits to all reachable trackdirs
			QueryNewTileTrackStatus();
			m_new_td_bits &= DiagdirReachesTrackdirs(m_exitdir);
			if (m_new_td_bits != TRACKDIR_BIT_NONE) {
				// we have some trackdirs reachable after reversal
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

		// for now we handle only on-bridge speed limit
		if (!IsWaterTT() && IsBridgeTile(m_old_tile)) {
			int spd = _bridge[GetBridgeType(m_old_tile)].speed;
			if (IsRoadTT()) spd *= 2;
			if (max_speed > spd) max_speed = spd;
		}

		// if min speed was requested, return it
		if (pmin_speed) *pmin_speed = min_speed;
		return max_speed;
	}
};

typedef CFollowTrackT<TRANSPORT_WATER, true > CFollowTrackWater;
typedef CFollowTrackT<TRANSPORT_ROAD , true > CFollowTrackRoad;
typedef CFollowTrackT<TRANSPORT_RAIL , true > CFollowTrackRail;

typedef CFollowTrackT<TRANSPORT_WATER, false> CFollowTrackWaterNo90;
typedef CFollowTrackT<TRANSPORT_ROAD , false> CFollowTrackRoadNo90;
typedef CFollowTrackT<TRANSPORT_RAIL , false> CFollowTrackRailNo90;

#endif /* FOLLOW_TRACK_HPP */
