/* $Id$ */

/** @file waypoint_cmp.cpp Command Handling for waypoints. */

#include "stdafx.h"

#include "command_func.h"
#include "landscape.h"
#include "economy_func.h"
#include "bridge_map.h"
#include "town.h"
#include "waypoint.h"
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

#include "table/strings.h"

/**
 * Update the sign for the waypoint
 * @param wp Waypoint to update sign */
void UpdateWaypointSign(Waypoint *wp)
{
	Point pt = RemapCoords2(TileX(wp->xy) * TILE_SIZE, TileY(wp->xy) * TILE_SIZE);
	SetDParam(0, wp->index);
	UpdateViewportSignPos(&wp->sign, pt.x, pt.y - 0x20, STR_WAYPOINT_VIEWPORT);
}

/**
 * Redraw the sign of a waypoint
 * @param wp Waypoint to redraw sign */
void RedrawWaypointSign(const Waypoint *wp)
{
	MarkAllViewportsDirty(
		wp->sign.left - 6,
		wp->sign.top,
		wp->sign.left + (wp->sign.width_1 << 2) + 12,
		wp->sign.top + 48);
}

/**
 * Set the default name for a waypoint
 * @param wp Waypoint to work on
 */
static void MakeDefaultWaypointName(Waypoint *wp)
{
	uint32 used = 0; // bitmap of used waypoint numbers, sliding window with 'next' as base
	uint32 next = 0; // first waypoint number in the bitmap
	WaypointID idx = 0; // index where we will stop

	wp->town_index = ClosestTownFromTile(wp->xy, UINT_MAX)->index;

	/* Find first unused waypoint number belonging to this town. This can never fail,
	 * as long as there can be at most 65535 waypoints in total.
	 *
	 * This does 'n * m' search, but with 32bit 'used' bitmap, it needs at most 'n * (1 + ceil(m / 32))'
	 * steps (n - number of waypoints in pool, m - number of waypoints near this town).
	 * Usually, it needs only 'n' steps.
	 *
	 * If it wasn't using 'used' and 'idx', it would just search for increasing 'next',
	 * but this way it is faster */

	WaypointID cid = 0; // current index, goes to GetWaypointPoolSize()-1, then wraps to 0
	do {
		Waypoint *lwp = GetWaypoint(cid);

		/* check only valid waypoints... */
		if (lwp->IsValid() && wp != lwp) {
			/* only waypoints with 'generic' name within the same city */
			if (lwp->name == NULL && lwp->town_index == wp->town_index) {
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
		if (cid == GetWaypointPoolSize()) cid = 0; // wrap to zero...
	} while (cid != idx);

	wp->town_cn = (uint16)next; // set index...
	wp->name = NULL; // ... and use generic name
}

/**
 * Find a deleted waypoint close to a tile.
 * @param tile to search from
 */
static Waypoint *FindDeletedWaypointCloseTo(TileIndex tile)
{
	Waypoint *wp, *best = NULL;
	uint thres = 8;

	FOR_ALL_WAYPOINTS(wp) {
		if (wp->deleted && wp->owner == _current_company) {
			uint cur_dist = DistanceManhattan(tile, wp->xy);

			if (cur_dist < thres) {
				thres = cur_dist;
				best = wp;
			}
		}
	}

	return best;
}

/** Convert existing rail to waypoint. Eg build a waypoint station over
 * piece of rail
 * @param tile tile where waypoint will be built
 * @param flags type of operation
 * @param p1 graphics for waypoint type, 0 indicates standard graphics
 * @param p2 unused
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
CommandCost CmdBuildTrainWaypoint(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Waypoint *wp;
	Slope tileh;
	Axis axis;

	/* if custom gfx are used, make sure it is within bounds */
	if (p1 >= GetNumCustomStations(STAT_CLASS_WAYP)) return CMD_ERROR;

	if (!IsTileType(tile, MP_RAILWAY) ||
			GetRailTileType(tile) != RAIL_TILE_NORMAL || (
				(axis = AXIS_X, GetTrackBits(tile) != TRACK_BIT_X) &&
				(axis = AXIS_Y, GetTrackBits(tile) != TRACK_BIT_Y)
			)) {
		return_cmd_error(STR_1005_NO_SUITABLE_RAILROAD_TRACK);
	}

	Owner owner = GetTileOwner(tile);
	if (!CheckOwnership(owner)) return CMD_ERROR;
	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);
	if (tileh != SLOPE_FLAT &&
			(!_settings_game.construction.build_on_slopes || IsSteepSlope(tileh) || !(tileh & (0x3 << axis)) || !(tileh & ~(0x3 << axis)))) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	wp = FindDeletedWaypointCloseTo(tile);
	if (wp == NULL && !Waypoint::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (wp == NULL) {
			wp = new Waypoint(tile);

			wp->town_index = INVALID_TOWN;
			wp->name = NULL;
			wp->town_cn = 0;
		} else {
			/* Move existing (recently deleted) waypoint to the new location */

			/* First we update the destination for all vehicles that
			 * have the old waypoint in their orders. */
			Vehicle *v;
			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_TRAIN &&
						v->First() == v &&
						v->current_order.IsType(OT_GOTO_WAYPOINT) &&
						v->dest_tile == wp->xy) {
					v->dest_tile = tile;
				}
			}

			RedrawWaypointSign(wp);
			wp->xy = tile;
			InvalidateWindowData(WC_WAYPOINT_VIEW, wp->index);
		}
		wp->owner = owner;

		const StationSpec *statspec;

		bool reserved = HasBit(GetTrackReservation(tile), AxisToTrack(axis));
		MakeRailWaypoint(tile, owner, axis, GetRailType(tile), wp->index);
		SetDepotWaypointReservation(tile, reserved);
		MarkTileDirtyByTile(tile);

		statspec = GetCustomStationSpec(STAT_CLASS_WAYP, p1);

		if (statspec != NULL) {
			wp->stat_id = p1;
			wp->grfid = statspec->grffile->grfid;
			wp->localidx = statspec->localidx;
		} else {
			/* Specified custom graphics do not exist, so use default. */
			wp->stat_id = 0;
			wp->grfid = 0;
			wp->localidx = 0;
		}

		wp->deleted = 0;
		wp->build_date = _date;

		if (wp->town_index == INVALID_TOWN) MakeDefaultWaypointName(wp);

		UpdateWaypointSign(wp);
		RedrawWaypointSign(wp);
		YapfNotifyTrackLayoutChange(tile, AxisToTrack(axis));
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.build_train_depot);
}

/**
 * Remove a waypoint
 * @param tile from which to remove waypoint
 * @param flags type of operation
 * @param justremove will indicate if it is removed from rail or if rails are removed too
 * @return cost of operation or error
 */
CommandCost RemoveTrainWaypoint(TileIndex tile, DoCommandFlag flags, bool justremove)
{
	Waypoint *wp;

	/* Make sure it's a waypoint */
	if (!IsRailWaypointTile(tile) ||
			(!CheckTileOwnership(tile) && _current_company != OWNER_WATER) ||
			!EnsureNoVehicleOnGround(tile)) {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		Track track = GetRailWaypointTrack(tile);
		wp = GetWaypointByTile(tile);

		wp->deleted = 30; // let it live for this many days before we do the actual deletion.
		RedrawWaypointSign(wp);

		Vehicle *v = NULL;
		if (justremove) {
			TrackBits tracks = GetRailWaypointBits(tile);
			bool reserved = GetDepotWaypointReservation(tile);
			MakeRailNormal(tile, wp->owner, tracks, GetRailType(tile));
			if (reserved) SetTrackReservation(tile, tracks);
			MarkTileDirtyByTile(tile);
		} else {
			if (GetDepotWaypointReservation(tile)) {
				v = GetTrainForReservation(tile, track);
				if (v != NULL) FreeTrainTrackReservation(v);
			}
			DoClearSquare(tile);
			AddTrackToSignalBuffer(tile, track, wp->owner);
		}
		YapfNotifyTrackLayoutChange(tile, track);
		if (v != NULL) TryPathReserve(v, true);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.remove_train_depot);
}

/**
 * Delete a waypoint
 * @param tile tile where waypoint is to be deleted
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 * @return cost of operation or error
 */
CommandCost CmdRemoveTrainWaypoint(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	return RemoveTrainWaypoint(tile, flags, true);
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
 * @return cost of operation or error
 */
CommandCost CmdRenameWaypoint(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsValidWaypointID(p1)) return CMD_ERROR;

	Waypoint *wp = GetWaypoint(p1);
	if (!CheckOwnership(wp->owner)) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_WAYPOINT_NAME_BYTES) return CMD_ERROR;
		if (!IsUniqueWaypointName(text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(wp->name);

		if (reset) {
			MakeDefaultWaypointName(wp); // sets wp->name = NULL
		} else {
			wp->name = strdup(text);
		}

		UpdateWaypointSign(wp);
		MarkWholeScreenDirty();
	}
	return CommandCost();
}
