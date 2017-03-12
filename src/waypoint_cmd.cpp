/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_cmd.cpp %Command Handling for waypoints. */

#include "stdafx.h"

#include "cmd_helper.h"
#include "command_func.h"
#include "landscape.h"
#include "bridge_map.h"
#include "town.h"
#include "waypoint_base.h"
#include "pathfinder/yapf/yapf_cache.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "company_func.h"
#include "newgrf_station.h"
#include "company_base.h"
#include "water.h"
#include "company_gui.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Update the virtual coords needed to draw the waypoint sign.
 */
void Waypoint::UpdateVirtCoord()
{
	Point pt = RemapCoords2(TileX(this->xy) * TILE_SIZE, TileY(this->xy) * TILE_SIZE);
	SetDParam(0, this->index);
	this->sign.UpdatePosition(pt.x, pt.y - 32 * ZOOM_LVL_BASE, STR_VIEWPORT_WAYPOINT);
	/* Recenter viewport */
	InvalidateWindowData(WC_WAYPOINT_VIEW, this->index);
}

/**
 * Find a deleted waypoint close to a tile.
 * @param tile to search from
 * @param str  the string to get the 'type' of
 * @param cid previous owner of the waypoint
 * @return the deleted nearby waypoint
 */
static Waypoint *FindDeletedWaypointCloseTo(TileIndex tile, StringID str, CompanyID cid)
{
	Waypoint *wp, *best = NULL;
	uint thres = 8;

	FOR_ALL_WAYPOINTS(wp) {
		if (!wp->IsInUse() && wp->string_id == str && wp->owner == cid) {
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

extern CommandCost ClearTile_Station(TileIndex tile, DoCommandFlag flags);

/**
 * Check whether the given tile is suitable for a waypoint.
 * @param tile the tile to check for suitability
 * @param axis the axis of the waypoint
 * @param waypoint Waypoint the waypoint to check for is already joined to. If we find another waypoint it can join to it will throw an error.
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
	CommandCost ret = CheckOwnership(owner);
	if (ret.Succeeded()) ret = EnsureNoVehicleOnGround(tile);
	if (ret.Failed()) return ret;

	Slope tileh = GetTileSlope(tile);
	if (tileh != SLOPE_FLAT &&
			(!_settings_game.construction.build_on_slopes || IsSteepSlope(tileh) || !(tileh & (0x3 << axis)) || !(tileh & ~(0x3 << axis)))) {
		return_cmd_error(STR_ERROR_FLAT_LAND_REQUIRED);
	}

	if (IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	return CommandCost();
}

extern void GetStationLayout(byte *layout, int numtracks, int plat_len, const StationSpec *statspec);
extern CommandCost FindJoiningWaypoint(StationID existing_station, StationID station_to_join, bool adjacent, TileArea ta, Waypoint **wp);
extern CommandCost CanExpandRailStation(const BaseStation *st, TileArea &new_ta, Axis axis);

/**
 * Convert existing rail to waypoint. Eg build a waypoint station over
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
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildRailWaypoint(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Unpack parameters */
	Axis axis      = Extract<Axis, 4, 1>(p1);
	byte width     = GB(p1,  8, 8);
	byte height    = GB(p1, 16, 8);
	bool adjacent  = HasBit(p1, 24);

	StationClassID spec_class = Extract<StationClassID, 0, 8>(p2);
	byte spec_index           = GB(p2, 8, 8);
	StationID station_to_join = GB(p2, 16, 16);

	/* Check if the given station class is valid */
	if (spec_class != STAT_CLASS_WAYP) return CMD_ERROR;
	if (spec_index >= StationClass::Get(spec_class)->GetSpecCount()) return CMD_ERROR;

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
		CommandCost ret = IsValidTileForWaypoint(tile, axis, &est);
		if (ret.Failed()) return ret;
	}

	Waypoint *wp = NULL;
	TileArea new_location(TileArea(start_tile, width, height));
	CommandCost ret = FindJoiningWaypoint(est, station_to_join, adjacent, new_location, &wp);
	if (ret.Failed()) return ret;

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	TileIndex center_tile = start_tile + (count / 2) * offset;
	if (wp == NULL && reuse) wp = FindDeletedWaypointCloseTo(center_tile, STR_SV_STNAME_WAYPOINT, _current_company);

	if (wp != NULL) {
		/* Reuse an existing waypoint. */
		if (wp->owner != _current_company) return_cmd_error(STR_ERROR_TOO_CLOSE_TO_ANOTHER_WAYPOINT);

		/* check if we want to expand an already existing waypoint? */
		if (wp->train_station.tile != INVALID_TILE) {
			CommandCost ret = CanExpandRailStation(wp, new_location, axis);
			if (ret.Failed()) return ret;
		}

		CommandCost ret = wp->rect.BeforeAddRect(start_tile, width, height, StationRect::ADD_TEST);
		if (ret.Failed()) return ret;
	} else {
		/* allocate and initialize new waypoint */
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

		if (wp->town == NULL) MakeDefaultName(wp);

		wp->UpdateVirtCoord();

		const StationSpec *spec = StationClass::Get(spec_class)->GetSpec(spec_index);
		byte *layout_ptr = AllocaM(byte, count);
		if (spec == NULL) {
			/* The layout must be 0 for the 'normal' waypoints by design. */
			memset(layout_ptr, 0, count);
		} else {
			/* But for NewGRF waypoints we like to have their style. */
			GetStationLayout(layout_ptr, count, 1, spec);
		}
		byte map_spec_index = AllocateSpecToStation(spec, wp, true);

		Company *c = Company::Get(wp->owner);
		for (int i = 0; i < count; i++) {
			TileIndex tile = start_tile + i * offset;
			byte old_specindex = HasStationTileRail(tile) ? GetCustomStationSpecIndex(tile) : 0;
			if (!HasStationTileRail(tile)) c->infrastructure.station++;
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
		DirtyCompanyInfrastructureWindows(wp->owner);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, count * _price[PR_BUILD_WAYPOINT_RAIL]);
}

/**
 * Build a buoy.
 * @param tile tile where to place the buoy
 * @param flags operation to perform
 * @param p1 unused
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildBuoy(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (tile == 0 || !HasTileWaterGround(tile)) return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	if (IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	if (!IsTileFlat(tile)) return_cmd_error(STR_ERROR_SITE_UNSUITABLE);

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	Waypoint *wp = FindDeletedWaypointCloseTo(tile, STR_SV_STNAME_BUOY, OWNER_NONE);
	if (wp == NULL && !Waypoint::CanAllocateItem()) return_cmd_error(STR_ERROR_TOO_MANY_STATIONS_LOADING);

	CommandCost cost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_WAYPOINT_BUOY]);
	if (!IsWaterTile(tile)) {
		CommandCost ret = DoCommand(tile, 0, 0, flags | DC_AUTO, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;
		cost.AddCost(ret);
	}

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

		if (wp->town == NULL) MakeDefaultName(wp);

		MakeBuoy(tile, wp->index, GetWaterClass(tile));
		MarkTileDirtyByTile(tile);

		wp->UpdateVirtCoord();
		InvalidateWindowData(WC_WAYPOINT_VIEW, wp->index);
	}

	return cost;
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

	if (HasStationInUse(wp->index, false, _current_company)) return_cmd_error(STR_ERROR_BUOY_IS_IN_USE);
	/* remove the buoy if there is a ship on tile when company goes bankrupt... */
	if (!(flags & DC_BANKRUPT)) {
		CommandCost ret = EnsureNoVehicleOnGround(tile);
		if (ret.Failed()) return ret;
	}

	if (flags & DC_EXEC) {
		wp->facilities &= ~FACIL_DOCK;

		InvalidateWindowData(WC_WAYPOINT_VIEW, wp->index);

		/* We have to set the water tile's state to the same state as before the
		 * buoy was placed. Otherwise one could plant a buoy on a canal edge,
		 * remove it and flood the land (if the canal edge is at level 0) */
		MakeWaterKeepingClass(tile, GetTileOwner(tile));

		wp->rect.AfterRemoveTile(wp, tile);

		wp->UpdateVirtCoord();
		wp->delete_ctr = 0;
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_WAYPOINT_BUOY]);
}

/**
 * Check whether the name is unique amongst the waypoints.
 * @param name The name to check.
 * @return True iff the name is unique.
 */
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
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameWaypoint(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Waypoint *wp = Waypoint::GetIfValid(p1);
	if (wp == NULL) return CMD_ERROR;

	if (wp->owner != OWNER_NONE) {
		CommandCost ret = CheckOwnership(wp->owner);
		if (ret.Failed()) return ret;
	}

	bool reset = StrEmpty(text);

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_STATION_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueWaypointName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(wp->name);
		wp->name = reset ? NULL : stredup(text);

		wp->UpdateVirtCoord();
	}
	return CommandCost();
}
