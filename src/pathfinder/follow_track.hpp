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
	enum ErrorCode : uint8_t {
		EC_NONE,
		EC_OWNER,
		EC_RAIL_ROAD_TYPE,
		EC_90DEG,
		EC_NO_WAY,
		EC_RESERVED,
	};

	const VehicleType *veh; ///< moving vehicle
	Owner veh_owner; ///< owner of the vehicle
	TileIndex old_tile; ///< the origin (vehicle moved from) before move
	Trackdir old_td; ///< the trackdir (the vehicle was on) before move
	TileIndex new_tile; ///< the new tile (the vehicle has entered)
	TrackdirBits new_td_bits; ///< the new set of available trackdirs
	DiagDirection exitdir; ///< exit direction (leaving the old tile)
	bool is_tunnel; ///< last turn passed tunnel
	bool is_bridge; ///< last turn passed bridge ramp
	bool is_station; ///< last turn passed station
	int tiles_skipped; ///< number of skipped tunnel or station tiles
	ErrorCode err;
	RailTypes railtypes;

	inline CFollowTrackT(const VehicleType *v = nullptr, RailTypes railtype_override = INVALID_RAILTYPES)
	{
		Init(v, railtype_override);
	}

	inline CFollowTrackT(Owner o, RailTypes railtype_override = INVALID_RAILTYPES)
	{
		assert(IsRailTT());
		this->veh = nullptr;
		Init(o, railtype_override);
	}

	inline void Init(const VehicleType *v, RailTypes railtype_override)
	{
		assert(!IsRailTT() || (v != nullptr && v->type == VEH_TRAIN));
		this->veh = v;
		Init(v != nullptr ? v->owner : INVALID_OWNER, IsRailTT() && railtype_override == INVALID_RAILTYPES ? Train::From(v)->compatible_railtypes : railtype_override);
	}

	inline void Init(Owner o, RailTypes railtype_override)
	{
		assert(!IsRoadTT() || this->veh != nullptr);
		assert(!IsRailTT() || railtype_override != INVALID_RAILTYPES);
		this->veh_owner = o;
		/* don't worry, all is inlined so compiler should remove unnecessary initializations */
		this->old_tile = INVALID_TILE;
		this->old_td = INVALID_TRACKDIR;
		this->new_tile = INVALID_TILE;
		this->new_td_bits = TRACKDIR_BIT_NONE;
		this->exitdir = INVALID_DIAGDIR;
		this->is_station = false;
		this->is_bridge = false;
		this->is_tunnel = false;
		this->tiles_skipped = 0;
		this->err = EC_NONE;
		this->railtypes = railtype_override;
	}

	debug_inline static TransportType TT() { return Ttr_type_; }
	debug_inline static bool IsWaterTT() { return TT() == TRANSPORT_WATER; }
	debug_inline static bool IsRailTT() { return TT() == TRANSPORT_RAIL; }
	inline bool IsTram() { return IsRoadTT() && RoadTypeIsTram(RoadVehicle::From(this->veh)->roadtype); }
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
		this->old_tile = old_tile;
		this->old_td = old_td;
		this->err = EC_NONE;

		assert([&]() {
			if (this->IsTram() && this->GetSingleTramBit(this->old_tile) != INVALID_DIAGDIR) return true; // Skip the check for single tram bits
			const uint sub_mode = (IsRoadTT() && this->veh != nullptr) ? (this->IsTram() ? RTT_TRAM : RTT_ROAD) : 0;
			const TrackdirBits old_tile_valid_dirs = TrackStatusToTrackdirBits(GetTileTrackStatus(this->old_tile, TT(), sub_mode));
			return (old_tile_valid_dirs & TrackdirToTrackdirBits(this->old_td)) != TRACKDIR_BIT_NONE;
		}());

		this->exitdir = TrackdirToExitdir(this->old_td);
		if (this->ForcedReverse()) return true;
		if (!this->CanExitOldTile()) return false;
		this->FollowTileExit();
		if (!this->QueryNewTileTrackStatus()) return TryReverse();
		this->new_td_bits &= DiagdirReachesTrackdirs(this->exitdir);
		if (this->new_td_bits == TRACKDIR_BIT_NONE || !this->CanEnterNewTile()) {
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
			if (this->new_td_bits == TRACKDIR_BIT_NONE) this->err = EC_NO_WAY;

			return false;
		}
		if ((!IsRailTT() && !Allow90degTurns()) || (IsRailTT() && Rail90DegTurnDisallowed(GetTileRailType(this->old_tile), GetTileRailType(this->new_tile), !Allow90degTurns()))) {
			this->new_td_bits &= (TrackdirBits)~(int)TrackdirCrossesTrackdirs(this->old_td);
			if (this->new_td_bits == TRACKDIR_BIT_NONE) {
				this->err = EC_90DEG;
				return false;
			}
		}
		return true;
	}

	inline bool MaskReservedTracks()
	{
		if (!DoTrackMasking()) return true;

		if (this->is_station) {
			/* Check skipped station tiles as well. */
			TileIndexDiff diff = TileOffsByDiagDir(this->exitdir);
			for (TileIndex tile = this->new_tile - diff * this->tiles_skipped; tile != this->new_tile; tile += diff) {
				if (HasStationReservation(tile)) {
					this->new_td_bits = TRACKDIR_BIT_NONE;
					this->err = EC_RESERVED;
					return false;
				}
			}
		}

		TrackBits reserved = GetReservedTrackbits(this->new_tile);
		/* Mask already reserved trackdirs. */
		this->new_td_bits &= ~TrackBitsToTrackdirBits(reserved);
		/* Mask out all trackdirs that conflict with the reservation. */
		for (Track t : SetTrackBitIterator(TrackdirBitsToTrackBits(this->new_td_bits))) {
			if (TracksOverlap(reserved | TrackToTrackBits(t))) this->new_td_bits &= ~TrackToTrackdirBits(t);
		}
		if (this->new_td_bits == TRACKDIR_BIT_NONE) {
			this->err = EC_RESERVED;
			return false;
		}
		return true;
	}

protected:
	/** Follow the exitdir from old_tile and fill new_tile and tiles_skipped */
	inline void FollowTileExit()
	{
		this->is_station = false;
		this->is_bridge = false;
		this->is_tunnel = false;
		this->tiles_skipped = 0;

		/* extra handling for tunnels and bridges in our direction */
		if (IsTileType(this->old_tile, MP_TUNNELBRIDGE)) {
			DiagDirection enterdir = GetTunnelBridgeDirection(this->old_tile);
			if (enterdir == this->exitdir) {
				/* we are entering the tunnel / bridge */
				if (IsTunnel(this->old_tile)) {
					this->is_tunnel = true;
					this->new_tile = GetOtherTunnelEnd(this->old_tile);
				} else { // IsBridge(old_tile)
					this->is_bridge = true;
					this->new_tile = GetOtherBridgeEnd(this->old_tile);
				}
				this->tiles_skipped = GetTunnelBridgeLength(this->new_tile, this->old_tile);
				return;
			}
			assert(ReverseDiagDir(enterdir) == this->exitdir);
		}

		/* normal or station tile, do one step */
		this->new_tile = TileAddByDiagDir(this->old_tile, this->exitdir);

		/* special handling for stations */
		if (IsRailTT() && HasStationTileRail(this->new_tile)) {
			this->is_station = true;
		} else if (IsRoadTT() && IsStationRoadStopTile(this->new_tile)) {
			this->is_station = true;
		}
	}

	/** stores track status (available trackdirs) for the new tile into new_td_bits */
	inline bool QueryNewTileTrackStatus()
	{
		if (IsRailTT() && IsPlainRailTile(this->new_tile)) {
			this->new_td_bits = (TrackdirBits)(GetTrackBits(this->new_tile) * 0x101);
		} else if (IsRoadTT()) {
			this->new_td_bits = GetTrackdirBitsForRoad(this->new_tile, this->IsTram() ? RTT_TRAM : RTT_ROAD);
		} else {
			this->new_td_bits = TrackStatusToTrackdirBits(GetTileTrackStatus(this->new_tile, TT(), 0));
		}
		return (this->new_td_bits != TRACKDIR_BIT_NONE);
	}

	/** return true if we can leave old_tile in exitdir */
	inline bool CanExitOldTile()
	{
		/* road stop can be left at one direction only unless it's a drive-through stop */
		if (IsRoadTT() && IsBayRoadStopTile(this->old_tile)) {
			DiagDirection exitdir = GetBayRoadStopDir(this->old_tile);
			if (exitdir != this->exitdir) {
				this->err = EC_NO_WAY;
				return false;
			}
		}

		/* single tram bits can only be left in one direction */
		if (this->IsTram()) {
			DiagDirection single_tram = GetSingleTramBit(this->old_tile);
			if (single_tram != INVALID_DIAGDIR && single_tram != this->exitdir) {
				this->err = EC_NO_WAY;
				return false;
			}
		}

		/* road depots can be also left in one direction only */
		if (IsRoadTT() && IsDepotTypeTile(this->old_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(this->old_tile);
			if (exitdir != this->exitdir) {
				this->err = EC_NO_WAY;
				return false;
			}
		}
		return true;
	}

	/** return true if we can enter new_tile from exitdir */
	inline bool CanEnterNewTile()
	{
		if (IsRoadTT() && IsBayRoadStopTile(this->new_tile)) {
			/* road stop can be entered from one direction only unless it's a drive-through stop */
			DiagDirection exitdir = GetBayRoadStopDir(this->new_tile);
			if (ReverseDiagDir(exitdir) != this->exitdir) {
				this->err = EC_NO_WAY;
				return false;
			}
		}

		/* single tram bits can only be entered from one direction */
		if (this->IsTram()) {
			DiagDirection single_tram = this->GetSingleTramBit(this->new_tile);
			if (single_tram != INVALID_DIAGDIR && single_tram != ReverseDiagDir(this->exitdir)) {
				this->err = EC_NO_WAY;
				return false;
			}
		}

		/* road and rail depots can also be entered from one direction only */
		if (IsRoadTT() && IsDepotTypeTile(this->new_tile, TT())) {
			DiagDirection exitdir = GetRoadDepotDirection(this->new_tile);
			if (ReverseDiagDir(exitdir) != this->exitdir) {
				this->err = EC_NO_WAY;
				return false;
			}
			/* don't try to enter other company's depots */
			if (GetTileOwner(this->new_tile) != this->veh_owner) {
				this->err = EC_OWNER;
				return false;
			}
		}
		if (IsRailTT() && IsDepotTypeTile(this->new_tile, TT())) {
			DiagDirection exitdir = GetRailDepotDirection(this->new_tile);
			if (ReverseDiagDir(exitdir) != this->exitdir) {
				this->err = EC_NO_WAY;
				return false;
			}
		}

		/* rail transport is possible only on tiles with the same owner as vehicle */
		if (IsRailTT() && GetTileOwner(this->new_tile) != this->veh_owner) {
			/* different owner */
			this->err = EC_NO_WAY;
			return false;
		}

		/* rail transport is possible only on compatible rail types */
		if (IsRailTT()) {
			RailType rail_type = GetTileRailType(this->new_tile);
			if (!this->railtypes.Test(rail_type)) {
				/* incompatible rail type */
				this->err = EC_RAIL_ROAD_TYPE;
				return false;
			}
		}

		/* road transport is possible only on compatible road types */
		if (IsRoadTT()) {
			const RoadVehicle *v = RoadVehicle::From(this->veh);
			RoadType roadtype = GetRoadType(this->new_tile, GetRoadTramType(v->roadtype));
			if (!v->compatible_roadtypes.Test(roadtype)) {
				/* incompatible road type */
				this->err = EC_RAIL_ROAD_TYPE;
				return false;
			}
		}

		/* tunnel holes and bridge ramps can be entered only from proper direction */
		if (IsTileType(this->new_tile, MP_TUNNELBRIDGE)) {
			if (IsTunnel(this->new_tile)) {
				if (!this->is_tunnel) {
					DiagDirection tunnel_enterdir = GetTunnelBridgeDirection(this->new_tile);
					if (tunnel_enterdir != this->exitdir) {
						this->err = EC_NO_WAY;
						return false;
					}
				}
			} else { // IsBridge(new_tile)
				if (!this->is_bridge) {
					DiagDirection ramp_enderdir = GetTunnelBridgeDirection(this->new_tile);
					if (ramp_enderdir != this->exitdir) {
						this->err = EC_NO_WAY;
						return false;
					}
				}
			}
		}

		/* special handling for rail stations - get to the end of platform */
		if (IsRailTT() && this->is_station) {
			/* entered railway station
			 * get platform length */
			uint length = BaseStation::GetByTile(this->new_tile)->GetPlatformLength(this->new_tile, TrackdirToExitdir(this->old_td));
			/* how big step we must do to get to the last platform tile? */
			this->tiles_skipped = length - 1;
			/* move to the platform end */
			TileIndexDiff diff = TileOffsByDiagDir(this->exitdir);
			diff *= this->tiles_skipped;
			this->new_tile = TileAdd(this->new_tile, diff);
			return true;
		}

		return true;
	}

	/** return true if we must reverse (in depots and single tram bits) */
	inline bool ForcedReverse()
	{
		/* rail and road depots cause reversing */
		if (!IsWaterTT() && IsDepotTypeTile(this->old_tile, TT())) {
			DiagDirection exitdir = IsRailTT() ? GetRailDepotDirection(this->old_tile) : GetRoadDepotDirection(this->old_tile);
			if (exitdir != this->exitdir) {
				/* reverse */
				this->new_tile = this->old_tile;
				this->new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(this->old_td));
				this->exitdir = exitdir;
				this->tiles_skipped = 0;
				this->is_tunnel = false;
				this->is_bridge = false;
				this->is_station = false;
				return true;
			}
		}

		/* Single tram bits and standard road stops cause reversing. */
		if (IsRoadTT() && ((this->IsTram() && GetSingleTramBit(this->old_tile) == ReverseDiagDir(this->exitdir)) ||
				(IsBayRoadStopTile(this->old_tile) && GetBayRoadStopDir(this->old_tile) == ReverseDiagDir(this->exitdir)))) {
			/* reverse */
			this->new_tile = this->old_tile;
			this->new_td_bits = TrackdirToTrackdirBits(ReverseTrackdir(this->old_td));
			this->exitdir = ReverseDiagDir(this->exitdir);
			this->tiles_skipped = 0;
			this->is_tunnel = false;
			this->is_bridge = false;
			this->is_station = false;
			return true;
		}

		return false;
	}

	/** return true if we successfully reversed at end of road/track */
	inline bool TryReverse()
	{
		if (IsRoadTT() && !this->IsTram()) {
			/* if we reached the end of road, we can reverse the RV and continue moving */
			this->exitdir = ReverseDiagDir(this->exitdir);
			/* new tile will be the same as old one */
			this->new_tile = this->old_tile;
			/* set new trackdir bits to all reachable trackdirs */
			QueryNewTileTrackStatus();
			this->new_td_bits &= DiagdirReachesTrackdirs(this->exitdir);
			if (this->new_td_bits != TRACKDIR_BIT_NONE) {
				/* we have some trackdirs reachable after reversal */
				return true;
			}
		}
		this->err = EC_NO_WAY;
		return false;
	}

public:
	/** Helper for pathfinders - get min/max speed on the old_tile/old_td */
	int GetSpeedLimit(int *pmin_speed = nullptr) const
	{
		int min_speed = 0;
		int max_speed = INT_MAX; // no limit

		/* Check for on-bridge speed limit */
		if (!IsWaterTT() && IsBridgeTile(this->old_tile)) {
			int spd = GetBridgeSpec(GetBridgeType(this->old_tile))->speed;
			if (IsRoadTT()) spd *= 2;
			max_speed = std::min(max_speed, spd);
		}
		/* Check for speed limit imposed by railtype */
		if (IsRailTT()) {
			uint16_t rail_speed = GetRailTypeInfo(GetRailType(this->old_tile))->max_speed;
			if (rail_speed > 0) max_speed = std::min<int>(max_speed, rail_speed);
		}
		if (IsRoadTT()) {
			/* max_speed is already in roadvehicle units, no need to further modify (divide by 2) */
			uint16_t road_speed = GetRoadTypeInfo(GetRoadType(this->old_tile, GetRoadTramType(RoadVehicle::From(this->veh)->roadtype)))->max_speed;
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
