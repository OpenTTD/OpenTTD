/* $Id$ */

/** @file water_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "bridge.h"
#include "cmd_helper.h"
#include "station_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "strings.h"
#include "functions.h"
#include "landscape.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "town.h"
#include "news.h"
#include "sound.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "train.h"
#include "roadveh.h"
#include "water_map.h"
#include "newgrf.h"
#include "newgrf_canal.h"

static const SpriteID _water_shore_sprites[] = {
	0,
	SPR_SHORE_TILEH_1,
	SPR_SHORE_TILEH_2,
	SPR_SHORE_TILEH_3,
	SPR_SHORE_TILEH_4,
	0,
	SPR_SHORE_TILEH_6,
	0,
	SPR_SHORE_TILEH_8,
	SPR_SHORE_TILEH_9,
	0,
	0,
	SPR_SHORE_TILEH_12,
	0,
	0
};


static Vehicle *FindFloodableVehicleOnTile(TileIndex tile);
static void FloodVehicle(Vehicle *v);

/** Build a ship depot.
 * @param tile tile where ship depot is built
 * @param flags type of operation
 * @param p1 bit 0 depot orientation (Axis)
 * @param p2 unused
 */
CommandCost CmdBuildShipDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile2;

	CommandCost cost, ret;
	Depot *depot;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	Axis axis = Extract<Axis, 0>(p1);

	tile2 = tile + (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
	if (!EnsureNoVehicle(tile2)) return CMD_ERROR;

	if (!IsClearWaterTile(tile) || !IsClearWaterTile(tile2))
		return_cmd_error(STR_3801_MUST_BE_BUILT_ON_WATER);

	if (IsBridgeAbove(tile) || IsBridgeAbove(tile2)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	ret = DoCommand(tile2, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;

	depot = AllocateDepot();
	if (depot == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		depot->xy = tile;
		depot->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		MakeShipDepot(tile,  _current_player, DEPOT_NORTH, axis);
		MakeShipDepot(tile2, _current_player, DEPOT_SOUTH, axis);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile2);
	}

	return cost.AddCost(_price.build_ship_depot);
}

static CommandCost RemoveShipDepot(TileIndex tile, uint32 flags)
{
	TileIndex tile2;

	if (!IsShipDepot(tile)) return CMD_ERROR;
	if (!CheckTileOwnership(tile)) return CMD_ERROR;
	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	tile2 = GetOtherShipDepotTile(tile);

	if (!EnsureNoVehicle(tile2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Kill the depot, which is registered at the northernmost tile. Use that one */
		DeleteDepot(GetDepotByTile(tile2 < tile ? tile2 : tile));

		MakeWater(tile);
		MakeWater(tile2);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile2);
	}

	return CommandCost(_price.remove_ship_depot);
}

/** build a shiplift */
static CommandCost DoBuildShiplift(TileIndex tile, DiagDirection dir, uint32 flags)
{
	CommandCost ret;
	int delta;

	/* middle tile */
	ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;

	delta = TileOffsByDiagDir(dir);
	/* lower tile */
	ret = DoCommand(tile - delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	if (GetTileSlope(tile - delta, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	}

	/* upper tile */
	ret = DoCommand(tile + delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	if (GetTileSlope(tile + delta, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	}

	if ((MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) ||
	    (MayHaveBridgeAbove(tile - delta) && IsBridgeAbove(tile - delta)) ||
	    (MayHaveBridgeAbove(tile + delta) && IsBridgeAbove(tile + delta))) {
		return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
	}

	if (flags & DC_EXEC) {
		MakeLock(tile, _current_player, dir);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile - delta);
		MarkTileDirtyByTile(tile + delta);
	}

	return CommandCost(_price.clear_water * 22 >> 3);
}

static CommandCost RemoveShiplift(TileIndex tile, uint32 flags)
{
	TileIndexDiff delta = TileOffsByDiagDir(GetLockDirection(tile));

	if (!CheckTileOwnership(tile) && GetTileOwner(tile) != OWNER_NONE) return CMD_ERROR;

	/* make sure no vehicle is on the tile. */
	if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(tile + delta) || !EnsureNoVehicle(tile - delta))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);
		DoClearSquare(tile + delta);
		DoClearSquare(tile - delta);
	}

	return CommandCost(_price.clear_water * 2);
}

static void MarkTilesAroundDirty(TileIndex tile)
{
	MarkTileDirtyByTile(TILE_ADDXY(tile, 0, 1));
	MarkTileDirtyByTile(TILE_ADDXY(tile, 0, -1));
	MarkTileDirtyByTile(TILE_ADDXY(tile, 1, 0));
	MarkTileDirtyByTile(TILE_ADDXY(tile, -1, 0));
}

/** Builds a lock (ship-lift)
 * @param tile tile where to place the lock
 * @param flags type of operation
 * @param p1 unused
 * @param p2 unused
 */
CommandCost CmdBuildLock(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	DiagDirection dir;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	switch (GetTileSlope(tile, NULL)) {
		case SLOPE_SW: dir = DIAGDIR_SW; break;
		case SLOPE_SE: dir = DIAGDIR_SE; break;
		case SLOPE_NW: dir = DIAGDIR_NW; break;
		case SLOPE_NE: dir = DIAGDIR_NE; break;
		default: return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	}
	return DoBuildShiplift(tile, dir, flags);
}

/** Build a piece of canal.
 * @param tile end tile of stretch-dragging
 * @param flags type of operation
 * @param p1 start tile of stretch-dragging
 * @param p2 ctrl pressed - toggles ocean / canals at sealevel (ocean only allowed in the scenario editor)
 */
CommandCost CmdBuildCanal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost;
	int size_x, size_y;
	int x;
	int y;
	int sx, sy;

	if (p1 >= MapSize()) return CMD_ERROR;
	/* Outside of the editor you can only build canals, not oceans */
	if (HASBIT(p2, 0) && _game_mode != GM_EDITOR) return CMD_ERROR;

	x = TileX(tile);
	y = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (x < sx) Swap(x, sx);
	if (y < sy) Swap(y, sy);
	size_x = (x - sx) + 1;
	size_y = (y - sy) + 1;

	/* Outside the editor you can only drag canals, and not areas */
	if (_game_mode != GM_EDITOR && (sx != x && sy != y)) return CMD_ERROR;

	BEGIN_TILE_LOOP(tile, size_x, size_y, TileXY(sx, sy)) {
		CommandCost ret;

		if (GetTileSlope(tile, NULL) != SLOPE_FLAT) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
		}

		/* can't make water of water! */
		if (IsTileType(tile, MP_WATER) && (!IsTileOwner(tile, OWNER_WATER) || HASBIT(p2, 0))) continue;

		ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost.AddCost(ret);

		if (flags & DC_EXEC) {
			if (TileHeight(tile) == 0 && HASBIT(p2, 0)) {
				MakeWater(tile);
			} else {
				MakeCanal(tile, _current_player);
			}
			MarkTileDirtyByTile(tile);
			MarkTilesAroundDirty(tile);
		}

		cost.AddCost(_price.clear_water);
	} END_TILE_LOOP(tile, size_x, size_y, 0);

	if (cost.GetCost() == 0) {
		return_cmd_error(STR_1007_ALREADY_BUILT);
	} else {
		return cost;
	}
}

static CommandCost ClearTile_Water(TileIndex tile, byte flags)
{
	switch (GetWaterTileType(tile)) {
		case WATER_TILE_CLEAR:
			if (flags & DC_NO_WATER) return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

			/* Make sure no vehicle is on the tile */
			if (!EnsureNoVehicle(tile)) return CMD_ERROR;

			/* Make sure it's not an edge tile. */
			if (!IS_INT_INSIDE(TileX(tile), 1, MapMaxX() - 1) ||
					!IS_INT_INSIDE(TileY(tile), 1, MapMaxY() - 1)) {
				return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);
			}

			if (GetTileOwner(tile) != OWNER_WATER && GetTileOwner(tile) != OWNER_NONE && !CheckTileOwnership(tile)) return CMD_ERROR;

			if (flags & DC_EXEC) DoClearSquare(tile);
			return CommandCost(_price.clear_water);

		case WATER_TILE_COAST: {
			Slope slope = GetTileSlope(tile, NULL);

			/* Make sure no vehicle is on the tile */
			if (!EnsureNoVehicle(tile)) return CMD_ERROR;

			/* Make sure it's not an edge tile. */
			if (!IS_INT_INSIDE(TileX(tile), 1, MapMaxX() - 1) ||
					!IS_INT_INSIDE(TileY(tile), 1, MapMaxY() - 1)) {
				return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);
			}

			if (flags & DC_EXEC) DoClearSquare(tile);
			if (slope == SLOPE_N || slope == SLOPE_E || slope == SLOPE_S || slope == SLOPE_W) {
				return CommandCost(_price.clear_water);
			} else {
				return CommandCost(_price.purchase_land);
			}
		}

		case WATER_TILE_LOCK: {
			static const TileIndexDiffC _shiplift_tomiddle_offs[] = {
				{ 0,  0}, {0,  0}, { 0, 0}, {0,  0}, // middle
				{-1,  0}, {0,  1}, { 1, 0}, {0, -1}, // lower
				{ 1,  0}, {0, -1}, {-1, 0}, {0,  1}, // upper
			};

			if (flags & DC_AUTO) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
			if (_current_player == OWNER_WATER) return CMD_ERROR;
			/* move to the middle tile.. */
			return RemoveShiplift(tile + ToTileIndexDiff(_shiplift_tomiddle_offs[GetSection(tile)]), flags);
		}

		case WATER_TILE_DEPOT:
			if (flags & DC_AUTO) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
			return RemoveShipDepot(tile, flags);

		default:
			NOT_REACHED();
	}
}

/** return true if a tile is a water tile. */
static bool IsWateredTile(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_WATER:
			if (!IsCoast(tile)) return true;

			switch (GetTileSlope(tile, NULL)) {
				case SLOPE_W:
				case SLOPE_S:
				case SLOPE_E:
				case SLOPE_N:
					return true;

				default:
					return false;
			}

		case MP_STATION: return IsOilRig(tile) || IsDock(tile) || IsBuoy(tile);
		default:         return false;
	}
}

/** draw a canal styled water tile with dikes around */
void DrawCanalWater(TileIndex tile)
{
	uint wa;

	/* Test for custom graphics, else use the default */
	SpriteID dikes_base = GetCanalSprite(CF_DIKES, tile);
	if (dikes_base == 0) dikes_base = SPR_CANALS_BASE + 57;

	/* determine the edges around with water. */
	wa = IsWateredTile(TILE_ADDXY(tile, -1, 0)) << 0;
	wa += IsWateredTile(TILE_ADDXY(tile, 0, 1)) << 1;
	wa += IsWateredTile(TILE_ADDXY(tile, 1, 0)) << 2;
	wa += IsWateredTile(TILE_ADDXY(tile, 0, -1)) << 3;

	if (!(wa & 1)) DrawGroundSprite(dikes_base,     PAL_NONE);
	if (!(wa & 2)) DrawGroundSprite(dikes_base + 1, PAL_NONE);
	if (!(wa & 4)) DrawGroundSprite(dikes_base + 2, PAL_NONE);
	if (!(wa & 8)) DrawGroundSprite(dikes_base + 3, PAL_NONE);

	/* right corner */
	switch (wa & 0x03) {
		case 0: DrawGroundSprite(dikes_base + 4, PAL_NONE); break;
		case 3: if (!IsWateredTile(TILE_ADDXY(tile, -1, 1))) DrawGroundSprite(dikes_base + 8, PAL_NONE); break;
	}

	/* bottom corner */
	switch (wa & 0x06) {
		case 0: DrawGroundSprite(dikes_base + 5, PAL_NONE); break;
		case 6: if (!IsWateredTile(TILE_ADDXY(tile, 1, 1))) DrawGroundSprite(dikes_base + 9, PAL_NONE); break;
	}

	/* left corner */
	switch (wa & 0x0C) {
		case  0: DrawGroundSprite(dikes_base + 6, PAL_NONE); break;
		case 12: if (!IsWateredTile(TILE_ADDXY(tile, 1, -1))) DrawGroundSprite(dikes_base + 10, PAL_NONE); break;
	}

	/* upper corner */
	switch (wa & 0x09) {
		case 0: DrawGroundSprite(dikes_base + 7, PAL_NONE); break;
		case 9: if (!IsWateredTile(TILE_ADDXY(tile, -1, -1))) DrawGroundSprite(dikes_base + 11, PAL_NONE); break;
	}
}

struct LocksDrawTileStruct {
	int8 delta_x, delta_y, delta_z;
	byte width, height, depth;
	SpriteID image;
};

#include "table/water_land.h"

static void DrawWaterStuff(const TileInfo *ti, const WaterDrawTileStruct *wdts,
	SpriteID palette, uint base
)
{
	SpriteID image;
	SpriteID water_base = GetCanalSprite(CF_WATERSLOPE, ti->tile);
	SpriteID locks_base = GetCanalSprite(CF_LOCKS, ti->tile);

	/* If no custom graphics, use defaults */
	if (water_base == 0) water_base = SPR_CANALS_BASE + 5;
	if (locks_base == 0) {
		locks_base = SPR_CANALS_BASE + 9;
	} else {
		/* If using custom graphics, ignore the variation on height */
		base = 0;
	}

	image = wdts++->image;
	if (image < 4) image += water_base;
	DrawGroundSprite(image, PAL_NONE);

	for (; wdts->delta_x != 0x80; wdts++) {
		AddSortableSpriteToDraw(wdts->image + base + ((wdts->image < 24) ? locks_base : 0), palette,
			ti->x + wdts->delta_x, ti->y + wdts->delta_y,
			wdts->width, wdts->height,
			wdts->unk, ti->z + wdts->delta_z,
			HASBIT(_transparent_opt, TO_BUILDINGS));
	}
}

static void DrawTile_Water(TileInfo *ti)
{
	switch (GetWaterTileType(ti->tile)) {
		case WATER_TILE_CLEAR:
			DrawGroundSprite(SPR_FLAT_WATER_TILE, PAL_NONE);
			if (ti->z != 0 || !IsTileOwner(ti->tile, OWNER_WATER)) DrawCanalWater(ti->tile);
			DrawBridgeMiddle(ti);
			break;

		case WATER_TILE_COAST:
			assert(!IsSteepSlope(ti->tileh));
			if (_coast_base != 0) {
				DrawGroundSprite(_coast_base + ti->tileh, PAL_NONE);
			} else {
				DrawGroundSprite(_water_shore_sprites[ti->tileh], PAL_NONE);
			}
			DrawBridgeMiddle(ti);
			break;

		case WATER_TILE_LOCK: {
			const WaterDrawTileStruct *t = _shiplift_display_seq[GetSection(ti->tile)];
			DrawWaterStuff(ti, t, 0, ti->z > t[3].delta_y ? 24 : 0);
		} break;

		case WATER_TILE_DEPOT:
			DrawWaterStuff(ti, _shipdepot_display_seq[GetSection(ti->tile)], PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)), 0);
			break;
	}
}

void DrawShipDepotSprite(int x, int y, int image)
{
	const WaterDrawTileStruct *wdts = _shipdepot_display_seq[image];

	DrawSprite(wdts++->image, PAL_NONE, x, y);

	for (; wdts->delta_x != 0x80; wdts++) {
		Point pt = RemapCoords(wdts->delta_x, wdts->delta_y, wdts->delta_z);
		DrawSprite(wdts->image, PLAYER_SPRITE_COLOR(_local_player), x + pt.x, y + pt.y);
	}
}


static uint GetSlopeZ_Water(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
}

static Foundation GetFoundation_Water(TileIndex tile, Slope tileh)
{
	return FOUNDATION_NONE;
}

static void GetAcceptedCargo_Water(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Water(TileIndex tile, TileDesc *td)
{
	switch (GetWaterTileType(tile)) {
		case WATER_TILE_CLEAR:
			if (TilePixelHeight(tile) == 0 || IsTileOwner(tile, OWNER_WATER)) {
				td->str = STR_3804_WATER;
			} else {
				td->str = STR_LANDINFO_CANAL;
			}
			break;
		case WATER_TILE_COAST: td->str = STR_3805_COAST_OR_RIVERBANK; break;
		case WATER_TILE_LOCK : td->str = STR_LANDINFO_LOCK; break;
		case WATER_TILE_DEPOT: td->str = STR_3806_SHIP_DEPOT; break;
		default: assert(0); break;
	}

	td->owner = GetTileOwner(tile);
}

static void AnimateTile_Water(TileIndex tile)
{
	/* not used */
}

static void TileLoopWaterHelper(TileIndex tile, const TileIndexDiffC *offs)
{
	TileIndex target = TILE_ADD(tile, ToTileIndexDiff(offs[0]));

	/* type of this tile mustn't be water already. */
	if (IsTileType(target, MP_WATER)) return;

	if (TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[1]))) != 0 ||
			TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[2]))) != 0) {
		return;
	}

	if (TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[3]))) != 0 ||
			TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[4]))) != 0) {
		/* make coast.. */
		switch (GetTileType(target)) {
			case MP_RAILWAY: {
				TrackBits tracks;
				Slope slope;

				if (!IsPlainRailTile(target)) break;

				tracks = GetTrackBits(target);
				slope = GetTileSlope(target, NULL);
				if (!(
							(slope == SLOPE_W && tracks == TRACK_BIT_RIGHT) ||
							(slope == SLOPE_S && tracks == TRACK_BIT_UPPER) ||
							(slope == SLOPE_E && tracks == TRACK_BIT_LEFT)  ||
							(slope == SLOPE_N && tracks == TRACK_BIT_LOWER)
						)) {
					break;
				}
			}
			/* FALLTHROUGH */

			case MP_CLEAR:
			case MP_TREES:
				_current_player = OWNER_WATER;
				if (CmdSucceeded(DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
					MakeShore(target);
					MarkTileDirtyByTile(target);
				}
				break;

			default:
				break;
		}
	} else {
		_current_player = OWNER_WATER;

		Vehicle *v = FindFloodableVehicleOnTile(target);
		if (v != NULL) FloodVehicle(v);

		if (CmdSucceeded(DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
			MakeWater(target);
			MarkTileDirtyByTile(target);
		}
	}
}

/**
 * Finds a vehicle to flood.
 * It does not find vehicles that are already crashed on bridges, i.e. flooded.
 * @param tile the tile where to find a vehicle to flood
 * @return a vehicle too flood or NULL when there is no vehicle too flood.
 */
static Vehicle *FindFloodableVehicleOnTile(TileIndex tile)
{
	if (IsTileType(tile, MP_STATION) && IsAirport(tile)) {
		const Station *st = GetStationByTile(tile);
		const AirportFTAClass *airport = st->Airport();
		for (uint x = 0; x < airport->size_x; x++) {
			for (uint y = 0; y < airport->size_y; y++) {
				tile = TILE_ADDXY(st->airport_tile, x, y);
				Vehicle *v = FindVehicleOnTileZ(tile, 1 + airport->delta_z);
				if (v != NULL && (v->vehstatus & VS_CRASHED) == 0) return v;
			}
		}

		/* No vehicle could be flooded on this airport anymore */
		return NULL;
	}

	if (!IsBridgeTile(tile)) return FindVehicleOnTileZ(tile, 0);

	TileIndex end = GetOtherBridgeEnd(tile);
	byte z = GetBridgeHeight(tile);
	Vehicle *v;

	/* check the start tile first since as this is closest to the water */
	v = FindVehicleOnTileZ(tile, z);
	if (v != NULL && (v->vehstatus & VS_CRASHED) == 0) return v;

	/* check a vehicle in between both bridge heads */
	v = FindVehicleBetween(tile, end, z, true);
	if (v != NULL) return v;

	/* check the end tile last to give fleeing vehicles a chance to escape */
	v = FindVehicleOnTileZ(end, z);
	return (v != NULL && (v->vehstatus & VS_CRASHED) == 0) ? v : NULL;
}

static void FloodVehicle(Vehicle *v)
{
	if (!(v->vehstatus & VS_CRASHED)) {
		uint16 pass = 0;

		if (v->type == VEH_TRAIN || v->type == VEH_ROAD || v->type == VEH_AIRCRAFT) {
			if (v->type == VEH_AIRCRAFT) {
				/* Crashing aircraft are always at z_pos == 1, never on z_pos == 0,
				 * because that's always the shadow. Except for the heliport, because
				 * that station has a big z_offset for the aircraft. */
				if (!IsTileType(v->tile, MP_STATION) || !IsAirport(v->tile) || GetTileMaxZ(v->tile) != 0) return;
				const Station *st = GetStationByTile(v->tile);
				const AirportFTAClass *airport = st->Airport();

				if (v->z_pos != airport->delta_z + 1) return;
			}
			Vehicle *u;

			if (v->type != VEH_AIRCRAFT) v = GetFirstVehicleInChain(v);
			u = v;

			/* crash all wagons, and count passengers */
			BEGIN_ENUM_WAGONS(v)
				if (IsCargoInClass(v->cargo_type, CC_PASSENGERS)) pass += v->cargo.Count();
				v->vehstatus |= VS_CRASHED;
				MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
			END_ENUM_WAGONS(v)

			v = u;

			switch (v->type) {
				default: NOT_REACHED();
				case VEH_TRAIN:
					if (IsFrontEngine(v)) pass += 4; // driver
					v->u.rail.crash_anim_pos = 4000; // max 4440, disappear pretty fast
					break;

				case VEH_ROAD:
					if (IsRoadVehFront(v)) pass += 1; // driver
					v->u.road.crashed_ctr = 2000; // max 2220, disappear pretty fast
					break;

				case VEH_AIRCRAFT:
					pass += 2; // driver
					v->u.air.crashed_counter = 9000; // max 10000, disappear pretty fast
					break;
			}

			RebuildVehicleLists();
		} else {
			return;
		}

		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

		SetDParam(0, pass);
		AddNewsItem(STR_B006_FLOOD_VEHICLE_DESTROYED,
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_VEHICLE, NT_ACCIDENT, 0),
			v->index,
			0);
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
		SndPlayVehicleFx(SND_12_EXPLOSION, v);
	}
}

/** called from tunnelbridge_cmd, and by TileLoop_Industry() */
void TileLoop_Water(TileIndex tile)
{
	static const TileIndexDiffC _tile_loop_offs_array[][5] = {
		// tile to mod              shore?    shore?
		{{-1,  0}, {0, 0}, {0, 1}, {-1,  0}, {-1,  1}},
		{{ 0,  1}, {0, 1}, {1, 1}, { 0,  2}, { 1,  2}},
		{{ 1,  0}, {1, 0}, {1, 1}, { 2,  0}, { 2,  1}},
		{{ 0, -1}, {0, 0}, {1, 0}, { 0, -1}, { 1, -1}}
	};

	/* Ensure sea-level canals and buoys on canal borders do not flood */
	if ((IsTileType(tile, MP_WATER) || IsBuoyTile(tile)) && !IsTileOwner(tile, OWNER_WATER)) return;

	if (IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1) &&
			IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) {
		uint i;

		for (i = 0; i != lengthof(_tile_loop_offs_array); i++) {
			TileLoopWaterHelper(tile, _tile_loop_offs_array[i]);
		}
	}
	/* _current_player can be changed by TileLoopWaterHelper.. reset it back here */
	_current_player = OWNER_NONE;

	/* edges */
	if (TileX(tile) == 0 && IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) { //NE
		TileLoopWaterHelper(tile, _tile_loop_offs_array[2]);
	}

	if (TileX(tile) == MapSizeX() - 2 && IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) { //SW
		TileLoopWaterHelper(tile, _tile_loop_offs_array[0]);
	}

	if (TileY(tile) == 0 && IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1)) { //NW
		TileLoopWaterHelper(tile, _tile_loop_offs_array[1]);
	}

	if (TileY(tile) == MapSizeY() - 2 && IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1)) { //SE
		TileLoopWaterHelper(tile, _tile_loop_offs_array[3]);
	}
}

static uint32 GetTileTrackStatus_Water(TileIndex tile, TransportType mode, uint sub_mode)
{
	static const byte coast_tracks[] = {0, 32, 4, 0, 16, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};

	TrackBits ts;

	if (mode != TRANSPORT_WATER) return 0;

	switch (GetWaterTileType(tile)) {
		case WATER_TILE_CLEAR: ts = TRACK_BIT_ALL; break;
		case WATER_TILE_COAST: ts = (TrackBits)coast_tracks[GetTileSlope(tile, NULL) & 0xF]; break;
		case WATER_TILE_LOCK:  ts = AxisToTrackBits(DiagDirToAxis(GetLockDirection(tile))); break;
		case WATER_TILE_DEPOT: ts = AxisToTrackBits(GetShipDepotAxis(tile)); break;
		default: return 0;
	}
	if (TileX(tile) == 0) {
		/* NE border: remove tracks that connects NE tile edge */
		ts &= ~(TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT);
	}
	if (TileY(tile) == 0) {
		/* NW border: remove tracks that connects NW tile edge */
		ts &= ~(TRACK_BIT_Y | TRACK_BIT_LEFT | TRACK_BIT_UPPER);
	}
	return ts * 0x101;
}

static void ClickTile_Water(TileIndex tile)
{
	if (GetWaterTileType(tile) == WATER_TILE_DEPOT) {
		TileIndex tile2 = GetOtherShipDepotTile(tile);

		ShowDepotWindow(tile < tile2 ? tile : tile2, VEH_SHIP);
	}
}

static void ChangeTileOwner_Water(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else if (IsShipDepot(tile)) {
		DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	} else {
		SetTileOwner(tile, OWNER_NONE);
	}
}

static uint32 VehicleEnter_Water(Vehicle *v, TileIndex tile, int x, int y)
{
	return VETSB_CONTINUE;
}


extern const TileTypeProcs _tile_type_water_procs = {
	DrawTile_Water,           /* draw_tile_proc */
	GetSlopeZ_Water,          /* get_slope_z_proc */
	ClearTile_Water,          /* clear_tile_proc */
	GetAcceptedCargo_Water,   /* get_accepted_cargo_proc */
	GetTileDesc_Water,        /* get_tile_desc_proc */
	GetTileTrackStatus_Water, /* get_tile_track_status_proc */
	ClickTile_Water,          /* click_tile_proc */
	AnimateTile_Water,        /* animate_tile_proc */
	TileLoop_Water,           /* tile_loop_clear */
	ChangeTileOwner_Water,    /* change_tile_owner_clear */
	NULL,                     /* get_produced_cargo_proc */
	VehicleEnter_Water,       /* vehicle_enter_tile_proc */
	GetFoundation_Water,      /* get_foundation_proc */
};
