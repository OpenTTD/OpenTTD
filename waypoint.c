/* $Id$ */

#include "stdafx.h"
#include "openttd.h"

#include "command.h"
#include "functions.h"
#include "gfx.h"
#include "map.h"
#include "order.h"
#include "rail_map.h"
#include "saveload.h"
#include "station.h"
#include "tile.h"
#include "town.h"
#include "waypoint.h"
#include "variables.h"
#include "table/strings.h"
#include "vehicle.h"
#include "yapf/yapf.h"
#include "date.h"

enum {
	MAX_WAYPOINTS_PER_TOWN = 64,
};

/**
 * Called if a new block is added to the waypoint-pool
 */
static void WaypointPoolNewBlock(uint start_item)
{
	Waypoint *wp;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (wp = GetWaypoint(start_item); wp != NULL; wp = (wp->index + 1U < GetWaypointPoolSize()) ? GetWaypoint(wp->index + 1U) : NULL) wp->index = start_item++;
}

DEFINE_OLD_POOL(Waypoint, Waypoint, WaypointPoolNewBlock, NULL)

/* Create a new waypoint */
static Waypoint* AllocateWaypoint(void)
{
	Waypoint *wp;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (wp = GetWaypoint(0); wp != NULL; wp = (wp->index + 1U < GetWaypointPoolSize()) ? GetWaypoint(wp->index + 1U) : NULL) {
		if (!IsValidWaypoint(wp)) {
			uint index = wp->index;

			memset(wp, 0, sizeof(*wp));
			wp->index = index;

			return wp;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_Waypoint_pool)) return AllocateWaypoint();

	return NULL;
}

/* Update the sign for the waypoint */
static void UpdateWaypointSign(Waypoint* wp)
{
	Point pt = RemapCoords2(TileX(wp->xy) * TILE_SIZE, TileY(wp->xy) * TILE_SIZE);
	SetDParam(0, wp->index);
	UpdateViewportSignPos(&wp->sign, pt.x, pt.y - 0x20, STR_WAYPOINT_VIEWPORT);
}

/* Redraw the sign of a waypoint */
static void RedrawWaypointSign(const Waypoint* wp)
{
	MarkAllViewportsDirty(
		wp->sign.left - 6,
		wp->sign.top,
		wp->sign.left + (wp->sign.width_1 << 2) + 12,
		wp->sign.top + 48);
}

/* Update all signs */
void UpdateAllWaypointSigns(void)
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		UpdateWaypointSign(wp);
	}
}

/* Internal handler to delete a waypoint */
void DestroyWaypoint(Waypoint *wp)
{
	RemoveOrderFromAllVehicles(OT_GOTO_WAYPOINT, wp->index);

	if (wp->string != STR_NULL) DeleteName(wp->string);

	RedrawWaypointSign(wp);
}

/* Set the default name for a waypoint */
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

		if (local_wp->xy && local_wp->string == STR_NULL && local_wp->town_index == wp->town_index)
			used_waypoint[local_wp->town_cn] = true;
	}

	/* Find an empty spot */
	for (i = 0; used_waypoint[i] && i < MAX_WAYPOINTS_PER_TOWN; i++) {}

	wp->string = STR_NULL;
	wp->town_cn = i;
}

/* Find a deleted waypoint close to a tile. */
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
void AfterLoadWaypoints(void)
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		uint i;

		if (wp->grfid == 0) continue;

		for (i = 0; i < GetNumCustomStations(STAT_CLASS_WAYP); i++) {
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, i);
			if (statspec != NULL && statspec->grfid == wp->grfid && statspec->localidx == wp->localidx) {
				wp->stat_id = i;
				break;
			}
		}
	}
}

/** Convert existing rail to waypoint. Eg build a waypoint station over
 * piece of rail
 * @param tile tile where waypoint will be built
 * @param p1 graphics for waypoint type, 0 indicates standard graphics
 * @param p2 unused
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
int32 CmdBuildTrainWaypoint(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Waypoint *wp;
	Slope tileh;
	Axis axis;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

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
	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);
	if (tileh != SLOPE_FLAT &&
			(!_patches.build_on_slopes || IsSteepSlope(tileh) || !(tileh & (0x3 << axis)) || !(tileh & ~(0x3 << axis)))) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	/* Check if there is an already existing, deleted, waypoint close to us that we can reuse. */
	wp = FindDeletedWaypointCloseTo(tile);
	if (wp == NULL) {
		wp = AllocateWaypoint();
		if (wp == NULL) return CMD_ERROR;

		wp->town_index = 0;
		wp->string = STR_NULL;
		wp->town_cn = 0;
	}

	if (flags & DC_EXEC) {
		const StationSpec* statspec;

		MakeRailWaypoint(tile, GetTileOwner(tile), axis, GetRailType(tile), wp->index);
		MarkTileDirtyByTile(tile);

		statspec = GetCustomStationSpec(STAT_CLASS_WAYP, p1);

		if (statspec != NULL) {
			wp->stat_id = p1;
			wp->grfid = statspec->grfid;
			wp->localidx = statspec->localidx;
		} else {
			// Specified custom graphics do not exist, so use default.
			wp->stat_id = 0;
			wp->grfid = 0;
			wp->localidx = 0;
		}

		wp->deleted = 0;
		wp->xy = tile;
		wp->build_date = _date;

		if (wp->town_index == 0) MakeDefaultWaypointName(wp);

		UpdateWaypointSign(wp);
		RedrawWaypointSign(wp);
		YapfNotifyTrackLayoutChange(tile, AxisToTrack(axis));
	}

	return _price.build_train_depot;
}

/* Daily loop for waypoints */
void WaypointsDailyLoop(void)
{
	Waypoint *wp;

	/* Check if we need to delete a waypoint */
	FOR_ALL_WAYPOINTS(wp) {
		if (wp->deleted != 0 && --wp->deleted == 0) DeleteWaypoint(wp);
	}
}

/* Remove a waypoint */
int32 RemoveTrainWaypoint(TileIndex tile, uint32 flags, bool justremove)
{
	Waypoint *wp;

	/* Make sure it's a waypoint */
	if (!IsTileType(tile, MP_RAILWAY) ||
			!IsRailWaypoint(tile) ||
			(!CheckTileOwnership(tile) && _current_player != OWNER_WATER) ||
			!EnsureNoVehicle(tile)) {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		Track track = GetRailWaypointTrack(tile);
		wp = GetWaypointByTile(tile);

		wp->deleted = 30; // let it live for this many days before we do the actual deletion.
		RedrawWaypointSign(wp);

		if (justremove) {
			MakeRailNormal(tile, GetTileOwner(tile), GetRailWaypointBits(tile), GetRailType(tile));
			MarkTileDirtyByTile(tile);
		} else {
			DoClearSquare(tile);
			SetSignalsOnBothDir(tile, track);
		}
		YapfNotifyTrackLayoutChange(tile, track);
	}

	return _price.remove_train_depot;
}

/** Delete a waypoint
 * @param tile tile where waypoint is to be deleted
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdRemoveTrainWaypoint(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);
	return RemoveTrainWaypoint(tile, flags, true);
}

/** Rename a waypoint.
 * @param tile unused
 * @param p1 id of waypoint
 * @param p2 unused
 */
int32 CmdRenameWaypoint(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Waypoint *wp;

	if (!IsValidWaypointID(p1)) return CMD_ERROR;

	if (_cmd_text[0] != '\0') {
		StringID str = AllocateNameUnique(_cmd_text, 0);

		if (str == 0) return CMD_ERROR;

		if (flags & DC_EXEC) {
			wp = GetWaypoint(p1);
			if (wp->string != STR_NULL) DeleteName(wp->string);

			wp->string = str;
			wp->town_cn = 0;

			UpdateWaypointSign(wp);
			MarkWholeScreenDirty();
		} else {
			DeleteName(str);
		}
	} else {
		if (flags & DC_EXEC) {
			wp = GetWaypoint(p1);
			if (wp->string != STR_NULL) DeleteName(wp->string);

			MakeDefaultWaypointName(wp);
			UpdateWaypointSign(wp);
			MarkWholeScreenDirty();
		}
	}
	return 0;
}

/* This hacks together some dummy one-shot Station structure for a waypoint. */
Station *ComposeWaypointStation(TileIndex tile)
{
	Waypoint *wp = GetWaypointByTile(tile);
	static Station stat;

	stat.train_tile = stat.xy = wp->xy;
	stat.town = GetTown(wp->town_index);
	stat.string_id = wp->string == STR_NULL ? /* FIXME? */ 0 : wp->string;
	stat.build_date = wp->build_date;

	return &stat;
}

/* Draw a waypoint */
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype)
{
	x += 33;
	y += 17;

	if (!DrawStationTile(x, y, railtype, AXIS_X, STAT_CLASS_WAYP, stat_id)) {
		DrawDefaultWaypointSprite(x, y, railtype);
	}
}

/* Fix savegames which stored waypoints in their old format */
void FixOldWaypoints(void)
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

void InitializeWaypoints(void)
{
	CleanPool(&_Waypoint_pool);
	AddBlockToPool(&_Waypoint_pool);
}

static const SaveLoad _waypoint_desc[] = {
	SLE_CONDVAR(Waypoint, xy,         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Waypoint, xy,         SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, town_index, SLE_UINT16,                 12, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, town_cn,    SLE_UINT8,                  12, SL_MAX_VERSION),
	    SLE_VAR(Waypoint, string,     SLE_UINT16),
	    SLE_VAR(Waypoint, deleted,    SLE_UINT8),

	SLE_CONDVAR(Waypoint, build_date, SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(Waypoint, build_date, SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, localidx,   SLE_UINT8,                   3, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, grfid,      SLE_UINT32,                 17, SL_MAX_VERSION),

	SLE_END()
};

static void Save_WAYP(void)
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		SlSetArrayIndex(wp->index);
		SlObject(wp, _waypoint_desc);
	}
}

static void Load_WAYP(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Waypoint *wp;

		if (!AddBlockIfNeeded(&_Waypoint_pool, index))
			error("Waypoints: failed loading savegame: too many waypoints");

		wp = GetWaypoint(index);
		SlObject(wp, _waypoint_desc);
	}
}

const ChunkHandler _waypoint_chunk_handlers[] = {
	{ 'CHKP', Save_WAYP, Load_WAYP, CH_ARRAY | CH_LAST},
};
