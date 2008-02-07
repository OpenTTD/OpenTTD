/* $Id$ */

/** @file waypoint.cpp */

#include "stdafx.h"
#include "openttd.h"

#include "command_func.h"
#include "landscape.h"
#include "order.h"
#include "rail_map.h"
#include "rail.h"
#include "bridge_map.h"
#include "saveload.h"
#include "station.h"
#include "town.h"
#include "waypoint.h"
#include "variables.h"
#include "yapf/yapf.h"
#include "newgrf.h"
#include "misc/autoptr.hpp"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "window_func.h"
#include "economy_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "vehicle_base.h"
#include "string_func.h"
#include "signal_func.h"
#include "player_func.h"
#include "settings_type.h"

#include "table/strings.h"

enum {
	MAX_WAYPOINTS_PER_TOWN = 64,
};

DEFINE_OLD_POOL_GENERIC(Waypoint, Waypoint)


/**
 * Update the sign for the waypoint
 * @param wp Waypoint to update sign */
static void UpdateWaypointSign(Waypoint* wp)
{
	Point pt = RemapCoords2(TileX(wp->xy) * TILE_SIZE, TileY(wp->xy) * TILE_SIZE);
	SetDParam(0, wp->index);
	UpdateViewportSignPos(&wp->sign, pt.x, pt.y - 0x20, STR_WAYPOINT_VIEWPORT);
}

/**
 * Redraw the sign of a waypoint
 * @param wp Waypoint to redraw sign */
static void RedrawWaypointSign(const Waypoint* wp)
{
	MarkAllViewportsDirty(
		wp->sign.left - 6,
		wp->sign.top,
		wp->sign.left + (wp->sign.width_1 << 2) + 12,
		wp->sign.top + 48);
}

/**
 * Update all signs
 */
void UpdateAllWaypointSigns()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		UpdateWaypointSign(wp);
	}
}

/**
 * Set the default name for a waypoint
 * @param wp Waypoint to work on
 */
static void MakeDefaultWaypointName(Waypoint* wp)
{
	Waypoint *local_wp;
	bool used_waypoint[MAX_WAYPOINTS_PER_TOWN];
	int i;

	wp->town_index = ClosestTownFromTile(wp->xy, (uint)-1)->index;

	memset(used_waypoint, 0, sizeof(used_waypoint));

	/* Find an unused waypoint number belonging to this town */
	FOR_ALL_WAYPOINTS(local_wp) {
		if (wp == local_wp) continue;

		if (local_wp->xy && local_wp->name == NULL && local_wp->town_index == wp->town_index)
			used_waypoint[local_wp->town_cn] = true;
	}

	/* Find an empty spot */
	for (i = 0; used_waypoint[i] && i < MAX_WAYPOINTS_PER_TOWN; i++) {}

	wp->name = NULL;
	wp->town_cn = i;
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
		if (wp->deleted) {
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
 * Update waypoint graphics id against saved GRFID/localidx.
 * This is to ensure the chosen graphics are correct if GRF files are changed.
 */
void AfterLoadWaypoints()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		uint i;

		if (wp->grfid == 0) continue;

		for (i = 0; i < GetNumCustomStations(STAT_CLASS_WAYP); i++) {
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, i);
			if (statspec != NULL && statspec->grffile->grfid == wp->grfid && statspec->localidx == wp->localidx) {
				wp->stat_id = i;
				break;
			}
		}
	}
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
CommandCost CmdBuildTrainWaypoint(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Waypoint *wp;
	AutoPtrT<Waypoint> wp_auto_delete;
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

	if (!CheckTileOwnership(tile)) return CMD_ERROR;
	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);
	if (tileh != SLOPE_FLAT &&
			(!_patches.build_on_slopes || IsSteepSlope(tileh) || !(tileh & (0x3 << axis)) || !(tileh & ~(0x3 << axis)))) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	wp = FindDeletedWaypointCloseTo(tile);
	if (wp == NULL) {
		wp = new Waypoint(tile);
		if (wp == NULL) return CMD_ERROR;

		wp_auto_delete = wp;

		wp->town_index = INVALID_TOWN;
		wp->name = NULL;
		wp->town_cn = 0;
	} else if (flags & DC_EXEC) {
		/* Move existing (recently deleted) waypoint to the new location */

		/* First we update the destination for all vehicles that
		 * have the old waypoint in their orders. */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN &&
					v->First() == v &&
					v->current_order.type == OT_GOTO_WAYPOINT &&
					v->dest_tile == wp->xy) {
				v->dest_tile = tile;
			}
		}

		RedrawWaypointSign(wp);
		wp->xy = tile;
	}

	if (flags & DC_EXEC) {
		const StationSpec* statspec;

		MakeRailWaypoint(tile, GetTileOwner(tile), axis, GetRailType(tile), wp->index);
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
		wp_auto_delete.Detach();
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.build_train_depot);
}

/**
 * Daily loop for waypoints
 */
void WaypointsDailyLoop()
{
	Waypoint *wp;

	/* Check if we need to delete a waypoint */
	FOR_ALL_WAYPOINTS(wp) {
		if (wp->deleted != 0 && --wp->deleted == 0) DeleteWaypoint(wp);
	}
}

/**
 * Remove a waypoint
 * @param tile from which to remove waypoint
 * @param flags type of operation
 * @param justremove will indicate if it is removed from rail or if rails are removed too
 * @return cost of operation or error
 */
CommandCost RemoveTrainWaypoint(TileIndex tile, uint32 flags, bool justremove)
{
	Waypoint *wp;

	/* Make sure it's a waypoint */
	if (!IsTileType(tile, MP_RAILWAY) ||
			!IsRailWaypoint(tile) ||
			(!CheckTileOwnership(tile) && _current_player != OWNER_WATER) ||
			!EnsureNoVehicleOnGround(tile)) {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		Track track = GetRailWaypointTrack(tile);
		Owner owner = GetTileOwner(tile); // cannot use _current_player because of possible floods
		wp = GetWaypointByTile(tile);

		wp->deleted = 30; // let it live for this many days before we do the actual deletion.
		RedrawWaypointSign(wp);

		if (justremove) {
			MakeRailNormal(tile, GetTileOwner(tile), GetRailWaypointBits(tile), GetRailType(tile));
			MarkTileDirtyByTile(tile);
		} else {
			DoClearSquare(tile);
			AddTrackToSignalBuffer(tile, track, owner);
		}
		YapfNotifyTrackLayoutChange(tile, track);
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
CommandCost CmdRemoveTrainWaypoint(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	return RemoveTrainWaypoint(tile, flags, true);
}

static bool IsUniqueWaypointName(const char *name)
{
	const Waypoint *wp;
	char buf[512];

	FOR_ALL_WAYPOINTS(wp) {
		SetDParam(0, wp->index);
		GetString(buf, STR_WAYPOINT_RAW, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
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
CommandCost CmdRenameWaypoint(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Waypoint *wp;

	if (!IsValidWaypointID(p1)) return CMD_ERROR;

	wp = GetWaypoint(p1);
	if (!CheckTileOwnership(wp->xy)) return CMD_ERROR;

	if (!StrEmpty(_cmd_text)) {
		if (!IsUniqueWaypointName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

		if (flags & DC_EXEC) {
			free(wp->name);
			wp->name = strdup(_cmd_text);
			wp->town_cn = 0;

			UpdateWaypointSign(wp);
			MarkWholeScreenDirty();
		}
	} else {
		if (flags & DC_EXEC) {
			free(wp->name);

			MakeDefaultWaypointName(wp);
			UpdateWaypointSign(wp);
			MarkWholeScreenDirty();
		}
	}
	return CommandCost();
}

/**
 * This hacks together some dummy one-shot Station structure for a waypoint.
 * @param tile on which to work
 * @return pointer to a Station
 */
Station *ComposeWaypointStation(TileIndex tile)
{
	Waypoint *wp = GetWaypointByTile(tile);

	/* instead of 'static Station stat' use byte array to avoid Station's destructor call upon exit. As
	 * a side effect, the station is not constructed now. */
	static byte stat_raw[sizeof(Station)];
	static Station &stat = *(Station*)stat_raw;

	stat.train_tile = stat.xy = wp->xy;
	stat.town = GetTown(wp->town_index);
	stat.build_date = wp->build_date;

	return &stat;
}

/**
 * Draw a waypoint
 * @param x coordinate
 * @param y coordinate
 * @param stat_id station id
 * @param railtype RailType to use for
 */
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype)
{
	x += 33;
	y += 17;

	if (!DrawStationTile(x, y, railtype, AXIS_X, STAT_CLASS_WAYP, stat_id)) {
		DrawDefaultWaypointSprite(x, y, railtype);
	}
}

Waypoint::Waypoint(TileIndex tile)
{
	this->xy = tile;
}

Waypoint::~Waypoint()
{
	free(this->name);

	if (CleaningPool()) return;

	RemoveOrderFromAllVehicles(OT_GOTO_WAYPOINT, this->index);

	RedrawWaypointSign(this);
	this->xy = 0;
}

/**
 * Fix savegames which stored waypoints in their old format
 */
void FixOldWaypoints()
{
	Waypoint *wp;

	/* Convert the old 'town_or_string', to 'string' / 'town' / 'town_cn' */
	FOR_ALL_WAYPOINTS(wp) {
		wp->town_index = ClosestTownFromTile(wp->xy, (uint)-1)->index;
		wp->town_cn = 0;
		if (wp->string & 0xC000) {
			wp->town_cn = wp->string & 0x3F;
			wp->string = STR_NULL;
		}
	}
}

void InitializeWaypoints()
{
	_Waypoint_pool.CleanPool();
	_Waypoint_pool.AddBlockToPool();
}

static const SaveLoad _waypoint_desc[] = {
	SLE_CONDVAR(Waypoint, xy,         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Waypoint, xy,         SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, town_index, SLE_UINT16,                 12, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, town_cn,    SLE_UINT8,                  12, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, string,     SLE_STRINGID,                0, 83),
	SLE_CONDSTR(Waypoint, name,       SLE_STR, 0,                 84, SL_MAX_VERSION),
	    SLE_VAR(Waypoint, deleted,    SLE_UINT8),

	SLE_CONDVAR(Waypoint, build_date, SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(Waypoint, build_date, SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, localidx,   SLE_UINT8,                   3, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, grfid,      SLE_UINT32,                 17, SL_MAX_VERSION),

	SLE_END()
};

static void Save_WAYP()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		SlSetArrayIndex(wp->index);
		SlObject(wp, _waypoint_desc);
	}
}

static void Load_WAYP()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Waypoint *wp = new (index) Waypoint();
		SlObject(wp, _waypoint_desc);
	}
}

extern const ChunkHandler _waypoint_chunk_handlers[] = {
	{ 'CHKP', Save_WAYP, Load_WAYP, CH_ARRAY | CH_LAST},
};
