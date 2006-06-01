/* $Id$ */

#ifndef  FOLLOW_TRACK_HPP
#define  FOLLOW_TRACK_HPP

#include "yapf.hpp"

/** Track follower helper template class (can serve pathfinders and vehicle
    controllers). See 6 different typedefs below for 3 different transport
    types w/ of w/o 90-deg turns allowed */
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
		assert(!IsRailTT() || (v != NULL && v->type == VEH_Train));
		m_veh = v;
		m_pPerf = pPerf;
		// don't worry, all is inlined so compiler should remove unnecessary initializations
		m_new_tile = INVALID_TILE;
		m_new_td_bits = TRACKDIR_BIT_NONE;
		m_exitdir = INVALID_DIAGDIR;
		m_is_station = m_is_tunnel = false;
		m_tiles_skipped = 0;
	}

	FORCEINLINE static TransportType TT() {return Ttr_type_;}
	FORCEINLINE static bool IsWaterTT() {return TT() == TRANSPORT_WATER;}
	FORCEINLINE static bool IsRailTT() {return TT() == TRANSPORT_RAIL;}
	FORCEINLINE static bool IsRoadTT() {return TT() == TRANSPORT_ROAD;}
	FORCEINLINE static bool Allow90degTurns() {return T90deg_turns_allowed_;}

	/** main follower routine. Fills all members and return true on success.
	    Otherwise returns false if track can't be followed. */
	FORCEINLINE bool Follow(TileIndex old_tile, Trackdir old_td)
	{
		m_old_tile = old_tile;
		m_old_td = old_td;
		assert((GetTileTrackStatus(m_old_tile, TT()) & TrackdirToTrackdirBits(m_old_td)) != 0);
		m_exitdir = TrackdirToExitdir(m_old_td);
		if (EnteredRailDepot()) return true;
		if (!CanExitOldTile()) return false;
		FollowTileExit();
		if (!QueryNewTileTrackStatus()) return false;
		if (!CanEnterNewTile()) return false;
		m_new_td_bits &= DiagdirReachesTrackdirs(m_exitdir);
		if (!Allow90degTurns())
			m_new_td_bits &= (TrackdirBits)~(int)TrackdirCrossesTrackdirs(m_old_td);
		return (m_new_td_bits != TRACKDIR_BIT_NONE);
	}

protected:
	/** Follow the m_exitdir from m_old_tile and fill m_new_tile and m_tiles_skipped */
	FORCEINLINE void FollowTileExit()
	{
		// extra handling for tunnels in our direction
		if (IsTunnelTile(m_old_tile)) {
			DiagDirection tunnel_enterdir = GetTunnelDirection(m_old_tile);
			if (tunnel_enterdir == m_exitdir) {
				// we are entering the tunnel
				FindLengthOfTunnelResult flotr = FindLengthOfTunnel(m_old_tile, m_exitdir);
				m_new_tile = flotr.tile;
				m_is_tunnel = true;
				m_tiles_skipped = flotr.length - 1;
				return;
			}
			assert(ReverseDiagDir(tunnel_enterdir) == m_exitdir);
		}
		// not a tunnel or station
		m_is_tunnel = false;
		m_tiles_skipped = 0;

		// normal or station tile
		TileIndexDiff diff = TileOffsByDir(m_exitdir);
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
			uint32 ts = GetTileTrackStatus(m_new_tile, TT());
			m_new_td_bits = (TrackdirBits)(ts & TRACKDIR_BIT_MASK);
		}
		return (m_new_td_bits != TRACKDIR_BIT_NONE);
	}

	/** return true if we can leave m_old_tile in m_exitdir */
	FORCEINLINE bool CanExitOldTile()
	{
		// road stop can be left at one direction only
		if (IsRoadTT() && IsRoadStopTile(m_old_tile)) {
			DiagDirection exitdir = GetRoadStopDir(m_old_tile);
			if (exitdir != m_exitdir)
				return false;
		}

		// road depots can be also left in one direction only
		if (IsRoadTT() && IsTileDepotType(m_old_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(m_old_tile);
			if (exitdir != m_exitdir)
				return false;
		}
		return true;
	}

	/** return true if we can enter m_new_tile from m_exitdir */
	FORCEINLINE bool CanEnterNewTile()
	{
		if (IsRoadTT() && IsRoadStopTile(m_new_tile)) {
			// road stop can be entered from one direction only
			DiagDirection exitdir = GetRoadStopDir(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir)
				return false;
		}

		// road and rail depots can also be entered from one direction only
		if (IsRoadTT() && IsTileDepotType(m_new_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir)
				return false;
		}
		if (IsRailTT() && IsTileDepotType(m_new_tile, TT())) {
			DiagDirection exitdir = GetRailDepotDirection(m_new_tile);
			if (ReverseDiagDir(exitdir) != m_exitdir)
				return false;
		}

		// rail transport is possible only on tiles with the same owner as vehicle
		if (IsRailTT() && GetTileOwner(m_new_tile) != m_veh->owner) {
			// different owner
			if (IsBridgeTile(m_new_tile)) {
				if (IsBridgeMiddle(m_new_tile)) {
						// bridge middle has no owner - tile is owned by the owner of the under-bridge track
					if (GetBridgeAxis(m_new_tile) != DiagDirToAxis(m_exitdir)) {
						// so it must be under bridge track (and wrong owner)
						return false;
					}
					// in the middle of the bridge - when we came here, it should be ok
				} else {
					// different owner, on the bridge ramp
					return false;
				}
			} else {
				// different owner, not a bridge
				return false;
			}
		}

		// rail transport is possible only on compatible rail types
		if (IsRailTT()) {
			RailType rail_type = GetTileRailType(m_new_tile, DiagdirToDiagTrackdir(m_exitdir));
			if (((1 << rail_type) & m_veh->u.rail.compatible_railtypes) == 0) {
				// incompatible rail type
				return false;
			}
		}

		// tunnel tiles can be entered only from proper direction
		if (!IsWaterTT() && !m_is_tunnel && IsTunnelTile(m_new_tile)) {
			DiagDirection tunnel_enterdir = GetTunnelDirection(m_new_tile);
			if (tunnel_enterdir != m_exitdir)
				return false;
		}

		// special handling for rail stations - get to the end of platform
		if (IsRailTT() && m_is_station) {
			// entered railway station
			// get platform length
			uint length = GetPlatformLength(m_new_tile, TrackdirToExitdir(m_old_td));
			// how big step we must do to get to the last platform tile;
			m_tiles_skipped = length - 1;
			// move to the platform end
			TileIndexDiff diff = TileOffsByDir(m_exitdir);
			diff *= m_tiles_skipped;
			m_new_tile = TILE_ADD(m_new_tile, diff);
			return true;
		}

		return true;
	}

	FORCEINLINE bool EnteredRailDepot()
	{
		// rail depots cause reversing
		if (IsRailTT() && IsTileDepotType(m_old_tile, TT())) {
			DiagDirection exitdir = GetRailDepotDirection(m_old_tile);
			if (exitdir != m_exitdir) {
				// reverse
				m_new_tile = m_old_tile;
				m_new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(m_old_td));
				m_exitdir = exitdir;
				m_tiles_skipped = 0;
				m_is_tunnel = false;
				return true;
			}
		}
		return false;
	}
public:
	/** Helper for pathfinders - get min/max speed on the m_old_tile/m_old_td */
	int GetSpeedLimit(int *pmin_speed = NULL)
	{
		int min_speed = 0;
		int max_speed = INT_MAX; // no limit

		// for now we handle only on-bridge speed limit
		if (IsBridgeTile(m_old_tile) && !IsWaterTT() && IsDiagonalTrackdir(m_old_td)) {
			bool is_on_bridge = true;
			if (IsBridgeMiddle(m_old_tile)) {
				// get track axis
				Axis track_axis = DiagDirToAxis(TrackdirToExitdir(m_old_td));
				// get under-bridge axis
				Axis bridge_axis =  GetBridgeAxis(m_old_tile);
				if (track_axis != bridge_axis) is_on_bridge = false;
			}
			if (is_on_bridge) {
				int spd = _bridge[GetBridgeType(m_old_tile)].speed;
				if (IsRoadTT()) spd *= 2;
				if (max_speed > spd) max_speed = spd;
			}
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
