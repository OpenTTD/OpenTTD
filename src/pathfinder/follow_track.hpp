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
#include "pathfinder_func.h"

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
		EC_RAIL_ROAD_TYPE,
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
	RailTypes           m_railtypes;

	inline CFollowTrackT(const VehicleType *v = nullptr, RailTypes railtype_override = INVALID_RAILTYPES)
	{
		Init(v, railtype_override);
	}

	inline CFollowTrackT(Owner o, RailTypes railtype_override = INVALID_RAILTYPES)
	{
		assert(IsRailTT());
		this->m_veh = nullptr;
		Init(o, railtype_override);
	}

	inline void Init(const VehicleType *v, RailTypes railtype_override)
	{
		assert(!IsRailTT() || (v != nullptr && v->type == VEH_TRAIN));
		this->m_veh = v;
		Init(v != nullptr ? v->owner : INVALID_OWNER, IsRailTT() && railtype_override == INVALID_RAILTYPES ? Train::From(v)->compatible_railtypes : railtype_override);
	}

	inline void Init(Owner o, RailTypes railtype_override)
	{
		assert(!IsRoadTT() || this->m_veh != nullptr);
		assert(!IsRailTT() || railtype_override != INVALID_RAILTYPES);
		this->m_veh_owner = o;
		/* don't worry, all is inlined so compiler should remove unnecessary initializations */
		this->m_old_tile = INVALID_TILE;
		this->m_old_td = INVALID_TRACKDIR;
		this->m_new_tile = INVALID_TILE;
		this->m_new_td_bits = TRACKDIR_BIT_NONE;
		this->m_exitdir = INVALID_DIAGDIR;
		this->m_is_station = false;
		this->m_is_bridge = false;
		this->m_is_tunnel = false;
		this->m_tiles_skipped = 0;
		this->m_err = EC_NONE;
		this->m_railtypes = railtype_override;
	}

	debug_inline static TransportType TT() { return Ttr_type_; }
	debug_inline static bool IsWaterTT() { return TT() == TRANSPORT_WATER; }
	debug_inline static bool IsRailTT() { return TT() == TRANSPORT_RAIL; }
	inline bool IsTram() { return IsRoadTT() && RoadTypeIsTram(RoadVehicle::From(this->m_veh)->roadtype); }
	debug_inline static bool IsRoadTT() { return TT() == TRANSPORT_ROAD; }
	inline static bool Allow90degTurns() { return T90deg_turns_allowed_; }
	inline static bool DoTrackMasking() { return Tmask_reserved_tracks; }

	/** Tests if a tile is a road tile with a single tramtrack (tram can reverse) */
	inline DiagDirection GetSingleTramBit(TileIndex tile)
	{
		assert(this->IsTram()); // this function shouldn't be called in other cases

		if (IsNormalRoadTile(tile)) {
			RoadBits rb = GetRoadBits(tile, RTT_TRAM);
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
		this->m_old_tile = old_tile;
		this->m_old_td = old_td;
		this->m_err = EC_NONE;

		assert([&]() {
			if (this->IsTram() && this->GetSingleTramBit(this->m_old_tile) != INVALID_DIAGDIR) return true; // Skip the check for single tram bits
			const uint sub_mode = (IsRoadTT() && this->m_veh != nullptr) ? (this->IsTram() ? RTT_TRAM : RTT_ROAD) : 0;
			const TrackdirBits old_tile_valid_dirs = TrackStatusToTrackdirBits(GetTileTrackStatus(this->m_old_tile, TT(), sub_mode));
			return (old_tile_valid_dirs & TrackdirToTrackdirBits(this->m_old_td)) != TRACKDIR_BIT_NONE;
		}());

		this->m_exitdir = TrackdirToExitdir(this->m_old_td);
		if (this->ForcedReverse()) return true;
		if (!this->CanExitOldTile()) return false;
		this->FollowTileExit();
		if (!this->QueryNewTileTrackStatus()) return TryReverse();
		this->m_new_td_bits &= DiagdirReachesTrackdirs(this->m_exitdir);
		if (this->m_new_td_bits == TRACKDIR_BIT_NONE || !this->CanEnterNewTile()) {
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
			if (IsRoadTT() && !this->IsTram() && this->TryReverse()) return true;

			/* CanEnterNewTile already set a reason.
			 * Do NOT overwrite it (important for example for EC_RAIL_ROAD_TYPE).
			 * Only set a reason if CanEnterNewTile was not called */
			if (this->m_new_td_bits == TRACKDIR_BIT_NONE) this->m_err = EC_NO_WAY;

			return false;
		}
		if ((!IsRailTT() && !Allow90degTurns()) || (IsRailTT() && Rail90DegTurnDisallowed(GetTileRailType(this->m_old_tile), GetTileRailType(this->m_new_tile), !Allow90degTurns()))) {
			this->m_new_td_bits &= (TrackdirBits)~(int)TrackdirCrossesTrackdirs(this->m_old_td);
			if (this->m_new_td_bits == TRACKDIR_BIT_NONE) {
				this->m_err = EC_90DEG;
				return false;
			}
		}
		return true;
	}

	inline bool MaskReservedTracks()
	{
		if (!DoTrackMasking()) return true;

		if (this->m_is_station) {
			/* Check skipped station tiles as well. */
			TileIndexDiff diff = TileOffsByDiagDir(this->m_exitdir);
			for (TileIndex tile = this->m_new_tile - diff * this->m_tiles_skipped; tile != this->m_new_tile; tile += diff) {
				if (HasStationReservation(tile)) {
					this->m_new_td_bits = TRACKDIR_BIT_NONE;
					this->m_err = EC_RESERVED;
					return false;
				}
			}
		}

		TrackBits reserved = GetReservedTrackbits(this->m_new_tile);
		/* Mask already reserved trackdirs. */
		this->m_new_td_bits &= ~TrackBitsToTrackdirBits(reserved);
		/* Mask out all trackdirs that conflict with the reservation. */
		for (Track t : SetTrackBitIterator(TrackdirBitsToTrackBits(this->m_new_td_bits))) {
			if (TracksOverlap(reserved | TrackToTrackBits(t))) this->m_new_td_bits &= ~TrackToTrackdirBits(t);
		}
		if (this->m_new_td_bits == TRACKDIR_BIT_NONE) {
			this->m_err = EC_RESERVED;
			return false;
		}
		return true;
	}

protected:
	/** Follow the m_exitdir from m_old_tile and fill m_new_tile and m_tiles_skipped */
	inline void FollowTileExit()
	{
		this->m_is_station = false;
		this->m_is_bridge = false;
		this->m_is_tunnel = false;
		this->m_tiles_skipped = 0;

		/* extra handling for tunnels and bridges in our direction */
		if (IsTileType(this->m_old_tile, MP_TUNNELBRIDGE)) {
			DiagDirection enterdir = GetTunnelBridgeDirection(this->m_old_tile);
			if (enterdir == this->m_exitdir) {
				/* we are entering the tunnel / bridge */
				if (IsTunnel(this->m_old_tile)) {
					this->m_is_tunnel = true;
					this->m_new_tile = GetOtherTunnelEnd(this->m_old_tile);
				} else { // IsBridge(m_old_tile)
					this->m_is_bridge = true;
					this->m_new_tile = GetOtherBridgeEnd(this->m_old_tile);
				}
				this->m_tiles_skipped = GetTunnelBridgeLength(this->m_new_tile, this->m_old_tile);
				return;
			}
			assert(ReverseDiagDir(enterdir) == this->m_exitdir);
		}

		/* normal or station tile, do one step */
		this->m_new_tile = TileAddByDiagDir(this->m_old_tile, this->m_exitdir);

		/* special handling for stations */
		if (IsRailTT() && HasStationTileRail(this->m_new_tile)) {
			this->m_is_station = true;
		} else if (IsRoadTT() && IsStationRoadStopTile(this->m_new_tile)) {
			this->m_is_station = true;
		}
	}

	/** stores track status (available trackdirs) for the new tile into m_new_td_bits */
	inline bool QueryNewTileTrackStatus()
	{
		if (IsRailTT() && IsPlainRailTile(this->m_new_tile)) {
			this->m_new_td_bits = (TrackdirBits)(GetTrackBits(this->m_new_tile) * 0x101);
		} else if (IsRoadTT()) {
			this->m_new_td_bits = GetTrackdirBitsForRoad(this->m_new_tile, this->IsTram() ? RTT_TRAM : RTT_ROAD);
		} else {
			this->m_new_td_bits = TrackStatusToTrackdirBits(GetTileTrackStatus(this->m_new_tile, TT(), 0));
		}
		return (this->m_new_td_bits != TRACKDIR_BIT_NONE);
	}

	/** return true if we can leave m_old_tile in m_exitdir */
	inline bool CanExitOldTile()
	{
		/* road stop can be left at one direction only unless it's a drive-through stop */
		if (IsRoadTT() && IsBayRoadStopTile(this->m_old_tile)) {
			DiagDirection exitdir = GetBayRoadStopDir(this->m_old_tile);
			if (exitdir != this->m_exitdir) {
				this->m_err = EC_NO_WAY;
				return false;
			}
		}

		/* single tram bits can only be left in one direction */
		if (this->IsTram()) {
			DiagDirection single_tram = GetSingleTramBit(this->m_old_tile);
			if (single_tram != INVALID_DIAGDIR && single_tram != this->m_exitdir) {
				this->m_err = EC_NO_WAY;
				return false;
			}
		}

		/* road depots can be also left in one direction only */
		if (IsRoadTT() && IsDepotTypeTile(this->m_old_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(this->m_old_tile);
			if (exitdir != this->m_exitdir) {
				this->m_err = EC_NO_WAY;
				return false;
			}
		}
		return true;
	}

	/** return true if we can enter m_new_tile from m_exitdir */
	inline bool CanEnterNewTile()
	{
		if (IsRoadTT() && IsBayRoadStopTile(this->m_new_tile)) {
			/* road stop can be entered from one direction only unless it's a drive-through stop */
			DiagDirection exitdir = GetBayRoadStopDir(this->m_new_tile);
			if (ReverseDiagDir(exitdir) != this->m_exitdir) {
				this->m_err = EC_NO_WAY;
				return false;
			}
		}

		/* single tram bits can only be entered from one direction */
		if (this->IsTram()) {
			DiagDirection single_tram = this->GetSingleTramBit(this->m_new_tile);
			if (single_tram != INVALID_DIAGDIR && single_tram != ReverseDiagDir(this->m_exitdir)) {
				this->m_err = EC_NO_WAY;
				return false;
			}
		}

		/* road and rail depots can also be entered from one direction only */
		if (IsRoadTT() && IsDepotTypeTile(this->m_new_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(this->m_new_tile);
			if (ReverseDiagDir(exitdir) != this->m_exitdir) {
				this->m_err = EC_NO_WAY;
				return false;
			}
			/* don't try to enter other company's depots */
			if (GetTileOwner(this->m_new_tile) != this->m_veh_owner) {
				this->m_err = EC_OWNER;
				return false;
			}
		}
		if (IsRailTT() && IsDepotTypeTile(this->m_new_tile, TT())) {
			DiagDirection exitdir = GetRailDepotDirection(this->m_new_tile);
			if (ReverseDiagDir(exitdir) != this->m_exitdir) {
				this->m_err = EC_NO_WAY;
				return false;
			}
		}

		/* rail transport is possible only on tiles with the same owner as vehicle */
		if (IsRailTT() && GetTileOwner(this->m_new_tile) != this->m_veh_owner) {
			/* different owner */
			this->m_err = EC_NO_WAY;
			return false;
		}

		/* rail transport is possible only on compatible rail types */
		if (IsRailTT()) {
			RailType rail_type = GetTileRailType(this->m_new_tile);
			if (!HasBit(this->m_railtypes, rail_type)) {
				/* incompatible rail type */
				this->m_err = EC_RAIL_ROAD_TYPE;
				return false;
			}
		}

		/* road transport is possible only on compatible road types */
		if (IsRoadTT()) {
			const RoadVehicle *v = RoadVehicle::From(this->m_veh);
			RoadType roadtype = GetRoadType(this->m_new_tile, GetRoadTramType(v->roadtype));
			if (!HasBit(v->compatible_roadtypes, roadtype)) {
				/* incompatible road type */
				this->m_err = EC_RAIL_ROAD_TYPE;
				return false;
			}
		}

		/* tunnel holes and bridge ramps can be entered only from proper direction */
		if (IsTileType(this->m_new_tile, MP_TUNNELBRIDGE)) {
			if (IsTunnel(this->m_new_tile)) {
				if (!this->m_is_tunnel) {
					DiagDirection tunnel_enterdir = GetTunnelBridgeDirection(this->m_new_tile);
					if (tunnel_enterdir != this->m_exitdir) {
						this->m_err = EC_NO_WAY;
						return false;
					}
				}
			} else { // IsBridge(m_new_tile)
				if (!this->m_is_bridge) {
					DiagDirection ramp_enderdir = GetTunnelBridgeDirection(this->m_new_tile);
					if (ramp_enderdir != this->m_exitdir) {
						this->m_err = EC_NO_WAY;
						return false;
					}
				}
			}
		}

		/* special handling for rail stations - get to the end of platform */
		if (IsRailTT() && this->m_is_station) {
			/* entered railway station
			 * get platform length */
			uint length = BaseStation::GetByTile(this->m_new_tile)->GetPlatformLength(this->m_new_tile, TrackdirToExitdir(this->m_old_td));
			/* how big step we must do to get to the last platform tile? */
			this->m_tiles_skipped = length - 1;
			/* move to the platform end */
			TileIndexDiff diff = TileOffsByDiagDir(this->m_exitdir);
			diff *= this->m_tiles_skipped;
			this->m_new_tile = TileAdd(this->m_new_tile, diff);
			return true;
		}

		return true;
	}

	/** return true if we must reverse (in depots and single tram bits) */
	inline bool ForcedReverse()
	{
		/* rail and road depots cause reversing */
		if (!IsWaterTT() && IsDepotTypeTile(this->m_old_tile, TT())) {
			DiagDirection exitdir = IsRailTT() ? GetRailDepotDirection(this->m_old_tile) : GetRoadDepotDirection(this->m_old_tile);
			if (exitdir != this->m_exitdir) {
				/* reverse */
				this->m_new_tile = this->m_old_tile;
				this->m_new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(this->m_old_td));
				this->m_exitdir = exitdir;
				this->m_tiles_skipped = 0;
				this->m_is_tunnel = false;
				this->m_is_bridge = false;
				this->m_is_station = false;
				return true;
			}
		}

		/* Single tram bits and standard road stops cause reversing. */
		if (IsRoadTT() && ((this->IsTram() && GetSingleTramBit(this->m_old_tile) == ReverseDiagDir(this->m_exitdir)) ||
				(IsBayRoadStopTile(this->m_old_tile) && GetBayRoadStopDir(this->m_old_tile) == ReverseDiagDir(this->m_exitdir)))) {
			/* reverse */
			this->m_new_tile = this->m_old_tile;
			this->m_new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(this->m_old_td));
			this->m_exitdir = ReverseDiagDir(this->m_exitdir);
			this->m_tiles_skipped = 0;
			this->m_is_tunnel = false;
			this->m_is_bridge = false;
			this->m_is_station = false;
			return true;
		}

		return false;
	}

	/** return true if we successfully reversed at end of road/track */
	inline bool TryReverse()
	{
		if (IsRoadTT() && !this->IsTram()) {
			/* if we reached the end of road, we can reverse the RV and continue moving */
			this->m_exitdir = ReverseDiagDir(this->m_exitdir);
			/* new tile will be the same as old one */
			this->m_new_tile = this->m_old_tile;
			/* set new trackdir bits to all reachable trackdirs */
			QueryNewTileTrackStatus();
			this->m_new_td_bits &= DiagdirReachesTrackdirs(this->m_exitdir);
			if (this->m_new_td_bits != TRACKDIR_BIT_NONE) {
				/* we have some trackdirs reachable after reversal */
				return true;
			}
		}
		this->m_err = EC_NO_WAY;
		return false;
	}

public:
	/** Helper for pathfinders - get min/max speed on the m_old_tile/m_old_td */
	int GetSpeedLimit(int *pmin_speed = nullptr) const
	{
		int min_speed = 0;
		int max_speed = INT_MAX; // no limit

		/* Check for on-bridge speed limit */
		if (!IsWaterTT() && IsBridgeTile(this->m_old_tile)) {
			int spd = GetBridgeSpec(GetBridgeType(this->m_old_tile))->speed;
			if (IsRoadTT()) spd *= 2;
			max_speed = std::min(max_speed, spd);
		}
		/* Check for speed limit imposed by railtype */
		if (IsRailTT()) {
			uint16_t rail_speed = GetRailTypeInfo(GetRailType(this->m_old_tile))->max_speed;
			if (rail_speed > 0) max_speed = std::min<int>(max_speed, rail_speed);
		}
		if (IsRoadTT()) {
			/* max_speed is already in roadvehicle units, no need to further modify (divide by 2) */
			uint16_t road_speed = GetRoadTypeInfo(GetRoadType(this->m_old_tile, GetRoadTramType(RoadVehicle::From(this->m_veh)->roadtype)))->max_speed;
			if (road_speed > 0) max_speed = std::min<int>(max_speed, road_speed);
		}

		/* if min speed was requested, return it */
		if (pmin_speed != nullptr) *pmin_speed = min_speed;
		return max_speed;
	}
};

typedef CFollowTrackT<TRANSPORT_WATER, Ship,        true > CFollowTrackWater;
typedef CFollowTrackT<TRANSPORT_ROAD,  RoadVehicle, true > CFollowTrackRoad;
typedef CFollowTrackT<TRANSPORT_RAIL,  Train,       true > CFollowTrackRail;

typedef CFollowTrackT<TRANSPORT_RAIL,  Train,       false> CFollowTrackRailNo90;

typedef CFollowTrackT<TRANSPORT_RAIL, Train, true,  true > CFollowTrackFreeRail;
typedef CFollowTrackT<TRANSPORT_RAIL, Train, false, true > CFollowTrackFreeRailNo90;

#endif /* FOLLOW_TRACK_HPP */
