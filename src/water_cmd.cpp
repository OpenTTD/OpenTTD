/* $Id$ */

/** @file water_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "bridge.h"
#include "cmd_helper.h"
#include "station_map.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "viewport_func.h"
#include "command_func.h"
#include "town.h"
#include "news.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "train.h"
#include "roadveh.h"
#include "water.h"
#include "water_map.h"
#include "industry_map.h"
#include "newgrf.h"
#include "newgrf_canal.h"
#include "misc/autoptr.hpp"
#include "transparency.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "variables.h"
#include "player_func.h"
#include "settings_type.h"
#include "clear_map.h"

#include "table/sprites.h"
#include "table/strings.h"

/**
 * Describes the behaviour of a tile during flooding.
 */
enum FloodingBehaviour {
	FLOOD_NONE,    ///< The tile does not flood neighboured tiles.
	FLOOD_ACTIVE,  ///< The tile floods neighboured tiles.
	FLOOD_PASSIVE, ///< The tile does not actively flood neighboured tiles, but it prevents them from drying up.
	FLOOD_DRYUP,   ///< The tile drys up if it is not constantly flooded from neighboured tiles.
};

/**
 * Describes from which directions a specific slope can be flooded (if the tile is floodable at all).
 */
static const uint8 _flood_from_dirs[] = {
	(1 << DIR_NW) | (1 << DIR_SW) | (1 << DIR_SE) | (1 << DIR_NE), // SLOPE_FLAT
	(1 << DIR_NE) | (1 << DIR_SE),                                 // SLOPE_W
	(1 << DIR_NW) | (1 << DIR_NE),                                 // SLOPE_S
	(1 << DIR_NE),                                                 // SLOPE_SW
	(1 << DIR_NW) | (1 << DIR_SW),                                 // SLOPE_E
	0,                                                             // SLOPE_EW
	(1 << DIR_NW),                                                 // SLOPE_SE
	(1 << DIR_N ) | (1 << DIR_NW) | (1 << DIR_NE),                 // SLOPE_WSE, SLOPE_STEEP_S
	(1 << DIR_SW) | (1 << DIR_SE),                                 // SLOPE_N
	(1 << DIR_SE),                                                 // SLOPE_NW
	0,                                                             // SLOPE_NS
	(1 << DIR_E ) | (1 << DIR_NE) | (1 << DIR_SE),                 // SLOPE_NWS, SLOPE_STEEP_W
	(1 << DIR_SW),                                                 // SLOPE_NE
	(1 << DIR_S ) | (1 << DIR_SW) | (1 << DIR_SE),                 // SLOPE_ENW, SLOPE_STEEP_N
	(1 << DIR_W ) | (1 << DIR_SW) | (1 << DIR_NW),                 // SLOPE_SEN, SLOPE_STEEP_E
};

/**
 * Makes a tile canal or water depending on the surroundings.
 * This as for example docks and shipdepots do not store
 * whether the tile used to be canal or 'normal' water.
 * @param t the tile to change.
 * @param o the owner of the new tile.
 */
void MakeWaterOrCanalDependingOnSurroundings(TileIndex t, Owner o)
{
	assert(GetTileSlope(t, NULL) == SLOPE_FLAT);

	/* Mark tile dirty in all cases */
	MarkTileDirtyByTile(t);

	/* Non-sealevel -> canal */
	if (TileHeight(t) != 0) {
		MakeCanal(t, o, Random());
		return;
	}

	bool has_water = false;
	bool has_canal = false;

	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		TileIndex neighbour = TileAddByDiagDir(t, dir);
		switch (GetTileType(neighbour)) {
			case MP_WATER:
				has_water |= IsSea(neighbour) || IsCoast(neighbour) || (IsShipDepot(neighbour) && GetShipDepotWaterOwner(neighbour) == OWNER_WATER);
				has_canal |= IsCanal(neighbour) || (IsShipDepot(neighbour) && GetShipDepotWaterOwner(neighbour) != OWNER_WATER);
				break;

			case MP_RAILWAY:
				/* Shore or flooded halftile */
				has_water |= (GetRailGroundType(neighbour) == RAIL_GROUND_WATER);
				break;

			default: break;
		}
	}
	if (has_canal || !has_water) {
		MakeCanal(t, o, Random());
	} else {
		MakeWater(t);
	}
}


/** Build a ship depot.
 * @param tile tile where ship depot is built
 * @param flags type of operation
 * @param p1 bit 0 depot orientation (Axis)
 * @param p2 unused
 */
CommandCost CmdBuildShipDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile2;

	CommandCost ret;

	Axis axis = Extract<Axis, 0>(p1);

	tile2 = tile + (axis == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));

	if (!IsWaterTile(tile) || !IsWaterTile(tile2))
		return_cmd_error(STR_3801_MUST_BE_BUILT_ON_WATER);

	if (IsBridgeAbove(tile) || IsBridgeAbove(tile2)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	Owner o1 = GetTileOwner(tile);
	Owner o2 = GetTileOwner(tile2);
	ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	ret = DoCommand(tile2, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;

	Depot *depot = new Depot(tile);
	if (depot == NULL) return CMD_ERROR;
	AutoPtrT<Depot> d_auto_delete = depot;

	if (flags & DC_EXEC) {
		depot->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		MakeShipDepot(tile,  _current_player, DEPOT_NORTH, axis, o1);
		MakeShipDepot(tile2, _current_player, DEPOT_SOUTH, axis, o2);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile2);
		d_auto_delete.Detach();
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.build_ship_depot);
}

void MakeWaterOrCanalDependingOnOwner(TileIndex tile, Owner o)
{
	if (o == OWNER_WATER) {
		MakeWater(tile);
	} else {
		MakeCanal(tile, o, Random());
	}
}

static CommandCost RemoveShipDepot(TileIndex tile, uint32 flags)
{
	TileIndex tile2;

	if (!IsShipDepot(tile)) return CMD_ERROR;
	if (!CheckTileOwnership(tile)) return CMD_ERROR;
	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	tile2 = GetOtherShipDepotTile(tile);

	if (!EnsureNoVehicleOnGround(tile2)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Kill the depot, which is registered at the northernmost tile. Use that one */
		delete GetDepotByTile(tile2 < tile ? tile2 : tile);

		MakeWaterOrCanalDependingOnOwner(tile,  GetShipDepotWaterOwner(tile));
		MakeWaterOrCanalDependingOnOwner(tile2, GetShipDepotWaterOwner(tile2));
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile2);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.remove_ship_depot);
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

	return CommandCost(EXPENSES_CONSTRUCTION, _price.clear_water * 22 >> 3);
}

static CommandCost RemoveShiplift(TileIndex tile, uint32 flags)
{
	TileIndexDiff delta = TileOffsByDiagDir(GetLockDirection(tile));

	if (!CheckTileOwnership(tile) && GetTileOwner(tile) != OWNER_NONE) return CMD_ERROR;

	/* make sure no vehicle is on the tile. */
	if (!EnsureNoVehicleOnGround(tile) || !EnsureNoVehicleOnGround(tile + delta) || !EnsureNoVehicleOnGround(tile - delta))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);
		MakeWaterOrCanalDependingOnSurroundings(tile + delta, _current_player);
		MakeWaterOrCanalDependingOnSurroundings(tile - delta, _current_player);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.clear_water * 2);
}

/**
 * Marks the tiles around a tile as dirty.
 *
 * This functions marks the tiles around a given tile as dirty for repaint.
 *
 * @param tile The center of the tile where all other tiles are marked as dirty
 * @ingroup dirty
 * @see TerraformAddDirtyTileAround
 */
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
	DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile, NULL));
	if (dir == INVALID_DIAGDIR) return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	/* Disallow building of locks on river rapids */
	if (IsRiverTile(tile)) return_cmd_error(STR_0239_SITE_UNSUITABLE);

	return DoBuildShiplift(tile, dir, flags);
}

/** Build a piece of canal.
 * @param tile end tile of stretch-dragging
 * @param flags type of operation
 * @param p1 start tile of stretch-dragging
 * @param p2 specifies canal (0), water (1) or river (2); last two can only be built in scenario editor
 */
CommandCost CmdBuildCanal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);
	int size_x, size_y;
	int x;
	int y;
	int sx, sy;

	if (p1 >= MapSize()) return CMD_ERROR;

	/* Outside of the editor you can only build canals, not oceans */
	if (p2 != 0 && _game_mode != GM_EDITOR) return CMD_ERROR;

	x = TileX(tile);
	y = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);

	if (x < sx) Swap(x, sx);
	if (y < sy) Swap(y, sy);
	size_x = (x - sx) + 1;
	size_y = (y - sy) + 1;

	/* Outside the editor you can only drag canals, and not areas */
	if (_game_mode != GM_EDITOR && (sx != x && sy != y)) return CMD_ERROR;

	BEGIN_TILE_LOOP(tile, size_x, size_y, TileXY(sx, sy)) {
		CommandCost ret;

		Slope slope = GetTileSlope(tile, NULL);
		if (slope != SLOPE_FLAT && (p2 != 2 || !IsInclinedSlope(slope))) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
		}

		/* can't make water of water! */
		if (IsTileType(tile, MP_WATER) && (!IsTileOwner(tile, OWNER_WATER) || p2 == 1)) continue;

		ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost.AddCost(ret);

		if (flags & DC_EXEC) {
			if (TileHeight(tile) == 0 && p2 == 1) {
				MakeWater(tile);
			} else if (p2 == 2) {
				MakeRiver(tile, Random());
			} else {
				MakeCanal(tile, _current_player, Random());
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
		case WATER_TILE_RIVER:
			if (flags & DC_NO_WATER) return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

			/* Make sure it's not an edge tile. */
			if (!IsInsideMM(TileX(tile), 1, MapMaxX() - 1) ||
					!IsInsideMM(TileY(tile), 1, MapMaxY() - 1)) {
				return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);
			}

			/* Make sure no vehicle is on the tile */
			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

			if (GetTileOwner(tile) != OWNER_WATER && GetTileOwner(tile) != OWNER_NONE && !CheckTileOwnership(tile)) return CMD_ERROR;

			if (flags & DC_EXEC) DoClearSquare(tile);
			return CommandCost(EXPENSES_CONSTRUCTION, _price.clear_water);

		case WATER_TILE_COAST: {
			Slope slope = GetTileSlope(tile, NULL);

			/* Make sure no vehicle is on the tile */
			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

			if (flags & DC_EXEC) DoClearSquare(tile);
			if (IsSlopeWithOneCornerRaised(slope)) {
				return CommandCost(EXPENSES_CONSTRUCTION, _price.clear_water);
			} else {
				return CommandCost(EXPENSES_CONSTRUCTION, _price.clear_roughland);
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
			return IsSlopeWithOneCornerRaised(GetTileSlope(tile, NULL));

		case MP_RAILWAY:
			if (GetRailGroundType(tile) == RAIL_GROUND_WATER) {
				assert(IsPlainRailTile(tile));
				return IsSlopeWithOneCornerRaised(GetTileSlope(tile, NULL));
			}
			return false;

		case MP_STATION:  return IsOilRig(tile) || IsDock(tile) || IsBuoy(tile);
		case MP_INDUSTRY: return (GetIndustrySpec(GetIndustryType(tile))->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0;
		default:          return false;
	}
}

static void DrawWaterEdges(SpriteID base, TileIndex tile)
{
	uint wa;

	/* determine the edges around with water. */
	wa  = IsWateredTile(TILE_ADDXY(tile, -1,  0)) << 0;
	wa += IsWateredTile(TILE_ADDXY(tile,  0,  1)) << 1;
	wa += IsWateredTile(TILE_ADDXY(tile,  1,  0)) << 2;
	wa += IsWateredTile(TILE_ADDXY(tile,  0, -1)) << 3;

	if (!(wa & 1)) DrawGroundSprite(base,     PAL_NONE);
	if (!(wa & 2)) DrawGroundSprite(base + 1, PAL_NONE);
	if (!(wa & 4)) DrawGroundSprite(base + 2, PAL_NONE);
	if (!(wa & 8)) DrawGroundSprite(base + 3, PAL_NONE);

	/* right corner */
	switch (wa & 0x03) {
		case 0: DrawGroundSprite(base + 4, PAL_NONE); break;
		case 3: if (!IsWateredTile(TILE_ADDXY(tile, -1, 1))) DrawGroundSprite(base + 8, PAL_NONE); break;
	}

	/* bottom corner */
	switch (wa & 0x06) {
		case 0: DrawGroundSprite(base + 5, PAL_NONE); break;
		case 6: if (!IsWateredTile(TILE_ADDXY(tile, 1, 1))) DrawGroundSprite(base + 9, PAL_NONE); break;
	}

	/* left corner */
	switch (wa & 0x0C) {
		case  0: DrawGroundSprite(base + 6, PAL_NONE); break;
		case 12: if (!IsWateredTile(TILE_ADDXY(tile, 1, -1))) DrawGroundSprite(base + 10, PAL_NONE); break;
	}

	/* upper corner */
	switch (wa & 0x09) {
		case 0: DrawGroundSprite(base + 7, PAL_NONE); break;
		case 9: if (!IsWateredTile(TILE_ADDXY(tile, -1, -1))) DrawGroundSprite(base + 11, PAL_NONE); break;
	}
}

/** draw a canal styled water tile with dikes around */
void DrawCanalWater(TileIndex tile)
{
	/* Test for custom graphics, else use the default */
	SpriteID dikes_base = GetCanalSprite(CF_DIKES, tile);
	if (dikes_base == 0) dikes_base = SPR_CANAL_DIKES_BASE;

	DrawWaterEdges(dikes_base, tile);
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
	if (water_base == 0) water_base = SPR_CANALS_BASE;
	if (locks_base == 0) {
		locks_base = SPR_SHIPLIFT_BASE;
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
			IsTransparencySet(TO_BUILDINGS));
	}
}

static void DrawRiverWater(const TileInfo *ti)
{
	SpriteID image = SPR_FLAT_WATER_TILE;
	SpriteID edges_base = GetCanalSprite(CF_RIVER_EDGE, ti->tile);

	if (ti->tileh != SLOPE_FLAT) {
		image = GetCanalSprite(CF_RIVER_SLOPE, ti->tile);
		if (image == 0) {
			switch (ti->tileh) {
				case SLOPE_NW: image = SPR_WATER_SLOPE_Y_DOWN; break;
				case SLOPE_SW: image = SPR_WATER_SLOPE_X_UP;   break;
				case SLOPE_SE: image = SPR_WATER_SLOPE_Y_UP;   break;
				case SLOPE_NE: image = SPR_WATER_SLOPE_X_DOWN; break;
				default:       image = SPR_FLAT_WATER_TILE;    break;
			}
		} else {
			switch (ti->tileh) {
				default: NOT_REACHED();
				case SLOPE_SE:             edges_base += 12; break;
				case SLOPE_NE: image += 1; edges_base += 24; break;
				case SLOPE_SW: image += 2; edges_base += 36; break;
				case SLOPE_NW: image += 3; edges_base += 48; break;
			}
		}
	}

	DrawGroundSprite(image, PAL_NONE);

	/* Draw river edges if available. */
	if (edges_base > 48) DrawWaterEdges(edges_base, ti->tile);
}

void DrawShoreTile(Slope tileh)
{
	/* Converts the enum Slope into an offset based on SPR_SHORE_BASE.
	 * This allows to calculate the proper sprite to display for this Slope */
	static const byte tileh_to_shoresprite[32] = {
		0, 1, 2, 3, 4, 16, 6, 7, 8, 9, 17, 11, 12, 13, 14, 0,
		0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0,  5,  0, 10, 15, 0,
	};

	assert(!IsHalftileSlope(tileh)); // Halftile slopes need to get handled earlier.
	assert(tileh != SLOPE_FLAT);     // Shore is never flat

	assert((tileh != SLOPE_EW) && (tileh != SLOPE_NS)); // No suitable sprites for current flooding behaviour

	DrawGroundSprite(SPR_SHORE_BASE + tileh_to_shoresprite[tileh], PAL_NONE);
}

static void DrawTile_Water(TileInfo *ti)
{
	switch (GetWaterTileType(ti->tile)) {
		case WATER_TILE_CLEAR:
			DrawGroundSprite(SPR_FLAT_WATER_TILE, PAL_NONE);
			if (IsCanal(ti->tile)) DrawCanalWater(ti->tile);
			DrawBridgeMiddle(ti);
			break;

		case WATER_TILE_COAST: {
			DrawShoreTile(ti->tileh);
			DrawBridgeMiddle(ti);
		} break;

		case WATER_TILE_LOCK: {
			const WaterDrawTileStruct *t = _shiplift_display_seq[GetSection(ti->tile)];
			DrawWaterStuff(ti, t, 0, ti->z > t[3].delta_y ? 24 : 0);
		} break;

		case WATER_TILE_DEPOT:
			DrawWaterStuff(ti, _shipdepot_display_seq[GetSection(ti->tile)], PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)), 0);
			break;

		case WATER_TILE_RIVER:
			DrawRiverWater(ti);
			DrawBridgeMiddle(ti);
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
		case WATER_TILE_RIVER:
			if (!IsCanal(tile)) {
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

/**
 * Marks tile dirty if it is a canal tile.
 * Called to avoid glitches when flooding tiles next to canal tile.
 *
 * @param tile tile to check
 */
static inline void MarkTileDirtyIfCanal(TileIndex tile)
{
	if (IsTileType(tile, MP_WATER) && IsCanal(tile)) MarkTileDirtyByTile(tile);
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

	/* if non-uniform stations are disabled, flood some train in this train station (if there is any) */
	if (!_patches.nonuniform_stations && IsTileType(tile, MP_STATION) && GetStationType(tile) == STATION_RAIL) {
		const Station *st = GetStationByTile(tile);

		BEGIN_TILE_LOOP(t, st->trainst_w, st->trainst_h, st->train_tile)
			if (st->TileBelongsToRailStation(t)) {
				Vehicle *v = FindVehicleOnTileZ(t, 0);
				if (v != NULL && (v->vehstatus & VS_CRASHED) == 0) return v;
			}
		END_TILE_LOOP(t, st->trainst_w, st->trainst_h, st->train_tile)

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

			if (v->type != VEH_AIRCRAFT) v = v->First();
			u = v;

			/* crash all wagons, and count passengers */
			BEGIN_ENUM_WAGONS(v)
				if (IsCargoInClass(v->cargo_type, CC_PASSENGERS)) pass += v->cargo.Count();
				v->vehstatus |= VS_CRASHED;
				MarkSingleVehicleDirty(v);
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

		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
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

/**
 * Returns the behaviour of a tile during flooding.
 *
 * @return Behaviour of the tile
 */
static FloodingBehaviour GetFloodingBehaviour(TileIndex tile)
{
	/* FLOOD_ACTIVE:  'single-corner-raised'-coast, sea, sea-shipdepots, sea-buoys, rail with flooded halftile
	 * FLOOD_DRYUP:   coast with more than one corner raised, coast with rail-track
	 * FLOOD_PASSIVE: oilrig, dock, water-industries
	 * FLOOD_NONE:    canals, rivers, everything else
	 */
	switch (GetTileType(tile)) {
		case MP_WATER:
			if (IsCoast(tile)) {
				Slope tileh = GetTileSlope(tile, NULL);
				return (IsSlopeWithOneCornerRaised(tileh) ? FLOOD_ACTIVE : FLOOD_DRYUP);
			} else {
				return ((IsSea(tile) || (IsShipDepot(tile) && (GetShipDepotWaterOwner(tile) == OWNER_WATER))) ? FLOOD_ACTIVE : FLOOD_NONE);
			}

		case MP_RAILWAY:
			if (GetRailGroundType(tile) == RAIL_GROUND_WATER) {
				return (IsSlopeWithOneCornerRaised(GetTileSlope(tile, NULL)) ? FLOOD_ACTIVE : FLOOD_DRYUP);
			}
			return FLOOD_NONE;

		case MP_STATION:
			if (IsSeaBuoyTile(tile)) return FLOOD_ACTIVE;
			if (IsOilRig(tile) || IsDock(tile)) return FLOOD_PASSIVE;
			return FLOOD_NONE;

		case MP_INDUSTRY:
			return ((GetIndustrySpec(GetIndustryType(tile))->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0 ? FLOOD_PASSIVE : FLOOD_NONE);

		default:
			return FLOOD_NONE;
	}
}

/**
 * Floods a tile.
 */
static void DoFloodTile(TileIndex target)
{
	if (IsTileType(target, MP_WATER)) return;

	bool flooded = false; // Will be set to true if something is changed.

	_current_player = OWNER_WATER;

	if (GetTileSlope(target, NULL) != SLOPE_FLAT) {
		/* make coast.. */
		switch (GetTileType(target)) {
			case MP_RAILWAY: {
				if (!IsPlainRailTile(target)) break;

				flooded = FloodHalftile(target);

				Vehicle *v = FindFloodableVehicleOnTile(target);
				if (v != NULL) FloodVehicle(v);

				break;
			}

			case MP_CLEAR:
			case MP_TREES:
				if (CmdSucceeded(DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
					MakeShore(target);
					MarkTileDirtyByTile(target);
					flooded = true;
				}
				break;

			default:
				break;
		}
	} else {
		/* Flood vehicles */
		Vehicle *v = FindFloodableVehicleOnTile(target);
		if (v != NULL) FloodVehicle(v);

		/* flood flat tile */
		if (CmdSucceeded(DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
			MakeWater(target);
			MarkTileDirtyByTile(target);
			flooded = true;
		}
	}

	if (flooded) {
		/* Mark surrounding canal tiles dirty too to avoid glitches */
		for (Direction dir = DIR_BEGIN; dir < DIR_END; dir++) {
			MarkTileDirtyIfCanal(target + TileOffsByDir(dir));
		}

		/* update signals if needed */
		UpdateSignalsInBuffer();
	}

	_current_player = OWNER_NONE;
}

/**
 * Drys a tile up.
 */
static void DoDryUp(TileIndex tile)
{
	_current_player = OWNER_WATER;

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			assert(IsPlainRailTile(tile));
			assert(GetRailGroundType(tile) == RAIL_GROUND_WATER);

			RailGroundType new_ground;
			switch (GetTrackBits(tile)) {
				case TRACK_BIT_UPPER: new_ground = RAIL_GROUND_FENCE_HORIZ1; break;
				case TRACK_BIT_LOWER: new_ground = RAIL_GROUND_FENCE_HORIZ2; break;
				case TRACK_BIT_LEFT:  new_ground = RAIL_GROUND_FENCE_VERT1;  break;
				case TRACK_BIT_RIGHT: new_ground = RAIL_GROUND_FENCE_VERT2;  break;
				default: NOT_REACHED();
			}
			SetRailGroundType(tile, new_ground);
			MarkTileDirtyByTile(tile);
			break;

		case MP_WATER:
			assert(IsCoast(tile));

			if (CmdSucceeded(DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
				MakeClear(tile, CLEAR_GRASS, 3);
				MarkTileDirtyByTile(tile);
			}
			break;

		default: NOT_REACHED();
	}

	_current_player = OWNER_NONE;
}

/**
 * Let a water tile floods its diagonal adjoining tiles
 * called from tunnelbridge_cmd, and by TileLoop_Industry() and TileLoop_Track()
 *
 * @param tile the water/shore tile that floods
 */
void TileLoop_Water(TileIndex tile)
{
	switch (GetFloodingBehaviour(tile)) {
		case FLOOD_ACTIVE:
			for (Direction dir = DIR_BEGIN; dir < DIR_END; dir++) {
				TileIndex dest = AddTileIndexDiffCWrap(tile, TileIndexDiffCByDir(dir));
				if (dest == INVALID_TILE) continue;

				uint z_dest;
				Slope slope_dest = (Slope)(GetFoundationSlope(dest, &z_dest) & ~SLOPE_HALFTILE_MASK & ~SLOPE_STEEP);
				if (z_dest > 0) continue;

				if (!HasBit(_flood_from_dirs[slope_dest], ReverseDir(dir))) continue;

				DoFloodTile(dest);
			}
			break;

		case FLOOD_DRYUP: {
			Slope slope_here = (Slope)(GetFoundationSlope(tile, NULL) & ~SLOPE_HALFTILE_MASK & ~SLOPE_STEEP);
			uint check_dirs = _flood_from_dirs[slope_here];
			uint dir;
			FOR_EACH_SET_BIT(dir, check_dirs) {
				TileIndex dest = AddTileIndexDiffCWrap(tile, TileIndexDiffCByDir((Direction)dir));
				if (dest == INVALID_TILE) continue;

				FloodingBehaviour dest_behaviour = GetFloodingBehaviour(dest);
				if ((dest_behaviour == FLOOD_ACTIVE) || (dest_behaviour == FLOOD_PASSIVE)) return;
			}
			DoDryUp(tile);
			break;
		}

		default: return;
	}
}

void ConvertGroundTilesIntoWaterTiles()
{
	TileIndex tile;
	uint z;
	Slope slope;

	for (tile = 0; tile < MapSize(); ++tile) {
		slope = GetTileSlope(tile, &z);
		if (IsTileType(tile, MP_CLEAR) && z == 0) {
			/* Make both water for tiles at level 0
			 * and make shore, as that looks much better
			 * during the generation. */
			switch (slope) {
				case SLOPE_FLAT:
					MakeWater(tile);
					break;

				case SLOPE_N:
				case SLOPE_E:
				case SLOPE_S:
				case SLOPE_W:
					MakeShore(tile);
					break;

				default:
					uint check_dirs = _flood_from_dirs[slope & ~SLOPE_STEEP];
					uint dir;
					FOR_EACH_SET_BIT(dir, check_dirs) {
						TileIndex dest = TILE_ADD(tile, TileOffsByDir((Direction)dir));
						Slope slope_dest = (Slope)(GetTileSlope(dest, NULL) & ~SLOPE_STEEP);
						if (slope_dest == SLOPE_FLAT || IsSlopeWithOneCornerRaised(slope_dest)) {
							MakeShore(tile);
							break;
						}
					}
					break;
			}
		}
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
		case WATER_TILE_RIVER: ts = (GetTileSlope(tile, NULL) == SLOPE_FLAT) ? TRACK_BIT_ALL : TRACK_BIT_NONE; break;
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

static VehicleEnterTileStatus VehicleEnter_Water(Vehicle *v, TileIndex tile, int x, int y)
{
	return VETSB_CONTINUE;
}

static CommandCost TerraformTile_Water(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	/* Canals can't be terraformed */
	if (IsWaterTile(tile) && IsCanal(tile)) return_cmd_error(STR_MUST_DEMOLISH_CANAL_FIRST);

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
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
	TerraformTile_Water,      /* terraform_tile_proc */
};
