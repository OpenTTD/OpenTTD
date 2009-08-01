/* $Id$ */

/** @file waypoint_cmd.cpp Command Handling for waypoints. */

#include "stdafx.h"

#include "command_func.h"
#include "landscape.h"
#include "economy_func.h"
#include "bridge_map.h"
#include "town.h"
#include "waypoint_base.h"
#include "yapf/yapf.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "company_func.h"
#include "newgrf_station.h"
#include "viewport_func.h"
#include "train.h"
#include "water.h"

#include "table/strings.h"

/**
 * Update the virtual coords needed to draw the waypoint sign.
 */
void Waypoint::UpdateVirtCoord()
{
	Point pt = RemapCoords2(TileX(this->xy) * TILE_SIZE, TileY(this->xy) * TILE_SIZE);
	SetDParam(0, this->index);
	this->sign.UpdatePosition(pt.x, pt.y - 0x20, STR_WAYPOINT_VIEWPORT);
	/* Recenter viewport */
	InvalidateWindowData(WC_WAYPOINT_VIEW, this->index);
}

/**
 * Set the default name for a waypoint
 * @param wp Waypoint to work on
 */
void MakeDefaultWaypointName(Waypoint *wp)
{
	uint32 used = 0; // bitmap of used waypoint numbers, sliding window with 'next' as base
	uint32 next = 0; // first waypoint number in the bitmap
	StationID idx = 0; // index where we will stop

	wp->town = ClosestTownFromTile(wp->xy, UINT_MAX);

	/* Find first unused waypoint number belonging to this town. This can never fail,
	 * as long as there can be at most 65535 waypoints in total.
	 *
	 * This does 'n * m' search, but with 32bit 'used' bitmap, it needs at most 'n * (1 + ceil(m / 32))'
	 * steps (n - number of waypoints in pool, m - number of waypoints near this town).
	 * Usually, it needs only 'n' steps.
	 *
	 * If it wasn't using 'used' and 'idx', it would just search for increasing 'next',
	 * but this way it is faster */

	StationID cid = 0; // current index, goes to Waypoint::GetPoolSize()-1, then wraps to 0
	do {
		Waypoint *lwp = Waypoint::GetIfValid(cid);

		/* check only valid waypoints... */
		if (lwp != NULL && wp != lwp) {
			/* only waypoints with 'generic' name within the same city */
			if (lwp->name == NULL && lwp->town == wp->town && lwp->string_id == wp->string_id) {
				/* if lwp->town_cn < next, uint will overflow to '+inf' */
				uint i = (uint)lwp->town_cn - next;

				if (i < 32) {
					SetBit(used, i); // update bitmap
					if (i == 0) {
						/* shift bitmap while the lowest bit is '1';
						 * increase the base of the bitmap too */
						do {
							used >>= 1;
							next++;
						} while (HasBit(used, 0));
						/* when we are at 'idx' again at end of the loop and
						 * 'next' hasn't changed, then no waypoint had town_cn == next,
						 * so we can safely use it */
						idx = cid;
					}
				}
			}
		}

		cid++;
		if (cid == Waypoint::GetPoolSize()) cid = 0; // wrap to zero...
	} while (cid != idx);

	wp->town_cn = (uint16)next; // set index...
	wp->name = NULL; // ... and use generic name
}

/**
 * Find a deleted waypoint close to a tile.
 * @param tile to search from
 * @param str  the string to get the 'type' of
 * @return the deleted nearby waypoint
 */
static Waypoint *FindDeletedWaypointCloseTo(TileIndex tile, StringID str)
{
	Waypoint *wp, *best = NULL;
	uint thres = 8;

	FOR_ALL_WAYPOINTS(wp) {
		if (!wp->IsInUse() && wp->string_id == str && (wp->owner == _current_company || wp->owner == OWNER_NONE)) {
			uint cur_dist = DistanceManhattan(tile, wp->xy);

			if (cur_dist < thres) {
				thres = cur_dist;
				best = wp;
			}
		}
	}

	return best;
}

/**
 * Get the axis for a new waypoint. This means that if it is a valid
 * tile to build a waypoint on it returns a valid Axis, otherwise an
 * invalid one.
 * @param tile the tile to look at.
 * @return the axis for the to-be-build waypoint.
 */
Axis GetAxisForNewWaypoint(TileIndex tile)
{
	/* The axis for rail waypoints is easy. */
	if (IsRailWaypointTile(tile)) return GetRailStationAxis(tile);

	/* Non-plain rail type, no valid axis for waypoints. */
	if (!IsTileType(tile, MP_RAILWAY) || GetRailTileType(tile) != RAIL_TILE_NORMAL) return INVALID_AXIS;

	switch (GetTrackBits(tile)) {
		case TRACK_BIT_X: return AXIS_X;
		case TRACK_BIT_Y: return AXIS_Y;
		default:          return INVALID_AXIS;
	}
}

CommandCost ClearTile_Station(TileIndex tile, DoCommandFlag flags);

/**
 * Check whether the given tile is suitable for a waypoint.
 * @param tile the tile to check for suitability
 * @param axis the axis of the waypoint
 */
static CommandCost IsValidTileForWaypoint(TileIndex tile, Axis axis, StationID *waypoint)
{
	/* if waypoint is set, then we have special handling to allow building on top of already existing waypoints.
	 * so waypoint points to INVALID_STATION if we can build on any waypoint.
	 * Or it points to a waypoint if we're only allowed to build on exactly that waypoint. */
	if (waypoint != NULL && IsTileType(tile, MP_STATION)) {
		if (!IsRailWaypoint(tile)) {
			return ClearTile_Station(tile, DC_AUTO); // get error message
		} else {
			StationID wp = GetStationIndex(tile);
			if (*waypoint == INVALID_STATION) {
				*waypoint = wp;
			} else if (*waypoint != wp) {
				return_cmd_error(STR_ERROR_WAYPOINT_ADJOINS_MORE_THAN_ONE_EXISTING);
			}
		}
	}

	if (GetAxisForNewWaypoint(tile) != axis) return_cmd_error(STR_ERROR_NO_SUITABLE_RAILROAD_TRACK);

	Owner owner = GetTileOwner(tile);
	if (!CheckOwnership(owner)) return CMD_ERROR;
	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	Slope tileh = GetTileSlope(tile, NULL);
	if (tileh != SLOPE_FLAT &&
			(!_settings_game.construction.build_on_slopes || IsSteepSlope(tileh) || !(tileh & (0x3 << axis)) || !(tileh & ~(0x3 << axis)))) {
		return_cmd_error(STR_ERROR_FLAT_LAND_REQUIRED);
	}

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	return CommandCost();
}

extern void GetStationLayout(byte *layout, int numtracks, int plat_len, const StationSpec *statspec);
extern CommandCost FindJoiningWaypoint(StationID existing_station, StationID station_to_join, bool adjacent, TileArea ta, Waypoint **wp);
extern bool CanExpandRailStation(const BaseStation *st, TileArea &new_ta, Axis axis);

/** Convert existing rail to waypoint. Eg build a waypoint station over
 * piece of rail
 * @param start_tile northern most tile where waypoint will be built
 * @param flags type of operation
 * @param p1 various bitstuffed elements
 * - p1 = (bit  4)    - orientation (Axis)
 * - p1 = (bit  8-15) - width of waypoint
 * - p1 = (bit 16-23) - height of waypoint
 * - p1 = (bit 24)    - allow waypoints directly adjacent to other waypoints.
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 7) - custom station class
 * - p2 = (bit  8-15) - custom station id
 * @param text unused
 * @return cost of operation or error
 */
CommandCost CmdBuildRailWaypoint(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Unpack parameters */
	Axis axis      = (Axis)GB(p1,  4, 1);
	byte width     = GB(p1,  8, 8);
	byte height    = GB(p1, 16, 8);
	bool adjacent  = HasBit(p1, 24);

	StationClassID spec_class = (StationClassID)GB(p2, 0, 8);
	byte spec_index           = GB(p2, 8, 8);
	StationID station_to_join = GB(p2, 16, 16);

	/* Check if the given station class is valid */
	if (spec_class != STAT_CLASS_WAYP) return CMD_ERROR;
	if (spec_index >= GetNumCustomStations(spec_class)) return CMD_ERROR;

	/* The number of parts to build */
	byte count = axis == AXIS_X ? height : width;

	if ((axis == AXIS_X ? width : height) != 1) return CMD_ERROR;
	if (count == 0 || count > _settings_game.station.station_spread) return CMD_ERROR;

	bool reuse = (station_to_join != NEW_STATION);
	if (!reuse) station_to_join = INVALID_STATION;
	bool distant_join = (station_to_join != INVALID_STATION);

	if (distant_join && (!_settings_game.station.distant_join_stations || !Waypoint::IsValidID(station_to_join))) return CMD_ERROR;

	/* Make sure the area below consists of clear tiles. (OR tiles belonging to a certain rail station) */
	StationID est = INVALID_STATION;

	/* Check whether the tiles we're building on are valid rail or not. */
	TileIndexDiff offset = TileOffsByDiagDir(AxisToDiagDir(OtherAxis(axis)));
	for (int i = 0; i < count; i++) {
		TileIndex tile = start_tile + i * offset;
		CommandCost ret = IsValidTileForWaypoint(tile, axis, _settings_game.station.nonuniform_stations ? &est : NULL);
		if (ret.Failed()) return ret;
	}

	Waypoint *wp = NULL;
	TileArea new_location(TileArea(start_tile, width, height));
	CommandCost ret = FindJoiningWaypoint(est, station_to_join, adjacent, new_location, &wp);
	if (ret.Failed()) return ret;

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	TileIndex center_tile = start_tile + (count / 2) * offset;
	if (wp == NULL && reuse) wp = FindDeletedWaypointCloseTo(center_tile, STR_SV_STNAME_WAYPOINT);

	if (wp != NULL) {
		/* Reuse an existing station. */
		if (wp->owner != _current_company) return_cmd_error(STR_ERROR_TOO_CLOSE_TO_ANOTHER_WAYPOINT);

		/* check if we want to expanding an already existing station? */
		if (wp->train_station.tile != INVALID_TILE && !CanExpandRailStation(wp, new_location, axis)) return CMD_ERROR;

		if (!wp->rect.BeforeAddRect(start_tile, width, height, StationRect::ADD_TEST)) return CMD_ERROR;
	} else {
		/* allocate and initialize new station */
		if (!Waypoint::CanAllocateItem()) return_cmd_error(STR_ERROR_TOO_MANY_STATIONS_LOADING);
	}

	if (flags & DC_EXEC) {
		if (wp == NULL) {
			wp = new Waypoint(start_tile);
		} else if (!wp->IsInUse()) {
			/* Move existing (recently deleted) waypoint to the new location */
			wp->xy = start_tile;
		}
		wp->owner = GetTileOwner(start_tile);

		wp->rect.BeforeAddRect(start_tile, width, height, StationRect::ADD_TRY);

		wp->delete_ctr = 0;
		wp->facilities |= FACIL_TRAIN;
		wp->build_date = _date;
		wp->string_id = STR_SV_STNAME_WAYPOINT;
		wp->train_station = new_location;

		if (wp->town == NULL) MakeDefaultWaypointName(wp);

		wp->UpdateVirtCoord();

		const StationSpec *spec = GetCustomStationSpec(spec_class, spec_index);
		byte *layout_ptr = AllocaM(byte, count);
		if (spec == NULL) {
			/* The layout must be 0 for the 'normal' waypoints by design. */
			memset(layout_ptr, 0, count);
		} else {
			/* But for NewGRF waypoints we like to have their style. */
			GetStationLayout(layout_ptr, count, 1, spec);
		}
		byte map_spec_index = AllocateSpecToStation(spec, wp, true);

		for (int i = 0; i < count; i++) {
			TileIndex tile = start_tile + i * offset;
			byte old_specindex = IsTileType(tile, MP_STATION) ? GetCustomStationSpecIndex(tile) : 0;
			bool reserved = IsTileType(tile, MP_RAILWAY) ?
					HasBit(GetRailReservationTrackBits(tile), AxisToTrack(axis)) :
					HasStationReservation(tile);
			MakeRailWaypoint(tile, wp->owner, wp->index, axis, layout_ptr[i], GetRailType(tile));
			SetCustomStationSpecIndex(tile, map_spec_index);
			SetRailStationReservation(tile, reserved);
			MarkTileDirtyByTile(tile);

			DeallocateSpecFromStation(wp, old_specindex);
			YapfNotifyTrackLayoutChange(tile, AxisToTrack(axis));
		}
	}

	return CommandCost(EXPENSES_CONSTRUCTION, count * _price.build_train_depot);
}

/** Build a buoy.
 * @param tile tile where to place the bouy
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 * @param text unused
 * @return cost of operation or error
 */
CommandCost CmdBuildBuoy(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsWaterTile(tile) || tile == 0) return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	if (GetTileSlope(tile, NULL) != SLOPE_FLAT) return_cmd_error(STR_ERROR_SITE_UNSUITABLE);

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	Waypoint *wp = FindDeletedWaypointCloseTo(tile, STR_SV_STNAME_BUOY);
	if (wp == NULL && !Waypoint::CanAllocateItem()) return_cmd_error(STR_ERROR_TOO_MANY_STATIONS_LOADING);

	if (flags & DC_EXEC) {
		if (wp == NULL) {
			wp = new Waypoint(tile);
		} else {
			/* Move existing (recently deleted) buoy to the new location */
			wp->xy = tile;
			InvalidateWindowData(WC_WAYPOINT_VIEW, wp->index);
		}
		wp->rect.BeforeAddTile(tile, StationRect::ADD_TRY);

		wp->string_id = STR_SV_STNAME_BUOY;

		wp->facilities |= FACIL_DOCK;
		wp->owner = OWNER_NONE;

		wp->build_date = _date;

		if (wp->town == NULL) MakeDefaultWaypointName(wp);

		MakeBuoy(tile, wp->index, GetWaterClass(tile));

		wp->UpdateVirtCoord();
		InvalidateWindowData(WC_WAYPOINT_VIEW, wp->index);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.build_dock);
}

/**
 * Remove a buoy
 * @param tile TileIndex been queried
 * @param flags operation to perform
 * @pre IsBuoyTile(tile)
 * @return cost or failure of operation
 */
CommandCost RemoveBuoy(TileIndex tile, DoCommandFlag flags)
{
	/* XXX: strange stuff, allow clearing as invalid company when clearing landscape */
	if (!Company::IsValidID(_current_company) && !(flags & DC_BANKRUPT)) return_cmd_error(INVALID_STRING_ID);

	Waypoint *wp = Waypoint::GetByTile(tile);

	if (HasStationInUse(wp->index, INVALID_COMPANY)) return_cmd_error(STR_BUOY_IS_IN_USE);
	/* remove the buoy if there is a ship on tile when company goes bankrupt... */
	if (!(flags & DC_BANKRUPT) && !EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		wp->facilities &= ~FACIL_DOCK;

		InvalidateWindowData(WC_WAYPOINT_VIEW, wp->index);

		/* We have to set the water tile's state to the same state as before the
		 * buoy was placed. Otherwise one could plant a buoy on a canal edge,
		 * remove it and flood the land (if the canal edge is at level 0) */
		MakeWaterKeepingClass(tile, GetTileOwner(tile));
		MarkTileDirtyByTile(tile);

		wp->rect.AfterRemoveTile(wp, tile);

		wp->UpdateVirtCoord();
		wp->delete_ctr = 0;
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.remove_truck_station);
}


static bool IsUniqueWaypointName(const char *name)
{
	const Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		if (wp->name != NULL && strcmp(wp->name, name) == 0) return false;
	}

	return true;
}

/**
 * Rename a waypoint.
 * @param tile unused
 * @param flags type of operation
 * @param p1 id of waypoint
 * @param p2 unused
 * @param text the new name of the waypoint or an empty string when resetting to the default
 * @return cost of operation or error
 */
CommandCost CmdRenameWaypoint(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Waypoint *wp = Waypoint::GetIfValid(p1);
	if (wp == NULL || !(CheckOwnership(wp->owner) || wp->owner == OWNER_NONE)) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_STATION_NAME_BYTES) return CMD_ERROR;
		if (!IsUniqueWaypointName(text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(wp->name);

		if (reset) {
			MakeDefaultWaypointName(wp); // sets wp->name = NULL
		} else {
			wp->name = strdup(text);
		}

		wp->UpdateVirtCoord();
	}
	return CommandCost();
}
