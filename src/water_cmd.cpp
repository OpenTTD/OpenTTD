/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "station_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
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
#include "water_map.h"
#include "newgrf.h"

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


static void FloodVehicle(Vehicle *v);

/** Build a ship depot.
 * @param tile tile where ship depot is built
 * @param p1 depot direction (0 == X or 1 == Y)
 * @param p2 unused
 */
int32 CmdBuildShipDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile2;

	int32 cost, ret;
	Depot *depot;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 > 1) return CMD_ERROR;

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	tile2 = tile + (p1 ? TileDiffXY(0, 1) : TileDiffXY(1, 0));
	if (!EnsureNoVehicle(tile2)) return CMD_ERROR;

	if (!IsClearWaterTile(tile) || !IsClearWaterTile(tile2))
		return_cmd_error(STR_3801_MUST_BE_BUILT_ON_WATER);

	if (IsBridgeAbove(tile) || IsBridgeAbove(tile2)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	ret = DoCommand(tile2, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;

	// pretend that we're not making land from the water even though we actually are.
	cost = 0;

	depot = AllocateDepot();
	if (depot == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		depot->xy = tile;
		depot->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		MakeShipDepot(tile,_current_player, DEPOT_NORTH, p1);
		MakeShipDepot(tile2,_current_player, DEPOT_SOUTH, p1);
		MarkTileDirtyByTile(tile);
		MarkTileDirtyByTile(tile2);
	}

	return cost + _price.build_ship_depot;
}

static int32 RemoveShipDepot(TileIndex tile, uint32 flags)
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

	return _price.remove_ship_depot;
}

// build a shiplift
static int32 DoBuildShiplift(TileIndex tile, DiagDirection dir, uint32 flags)
{
	int32 ret;
	int delta;

	// middle tile
	ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;

	delta = TileOffsByDiagDir(dir);
	// lower tile
	ret = DoCommand(tile - delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	if (GetTileSlope(tile - delta, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	}

	// upper tile
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

	return _price.clear_water * 22 >> 3;
}

static int32 RemoveShiplift(TileIndex tile, uint32 flags)
{
	TileIndexDiff delta = TileOffsByDiagDir(GetLockDirection(tile));

	if (!CheckTileOwnership(tile)) return CMD_ERROR;

	// make sure no vehicle is on the tile.
	if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(tile + delta) || !EnsureNoVehicle(tile - delta))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);
		DoClearSquare(tile + delta);
		DoClearSquare(tile - delta);
	}

	return _price.clear_water * 2;
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
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdBuildLock(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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
 * @param p1 start tile of stretch-dragging
 * @param p2 ctrl pressed - toggles ocean / canals at sealevel
 */
int32 CmdBuildCanal(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	int size_x, size_y;
	int x;
	int y;
	int sx, sy;

	if (p1 >= MapSize()) return CMD_ERROR;

	x = TileX(tile);
	y = TileY(tile);
	sx = TileX(p1);
	sy = TileY(p1);

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (x < sx) intswap(x, sx);
	if (y < sy) intswap(y, sy);
	size_x = (x - sx) + 1;
	size_y = (y - sy) + 1;

	/* Outside the editor you can only drag canals, and not areas */
	if (_game_mode != GM_EDITOR && (sx != x && sy != y)) return CMD_ERROR;

	cost = 0;
	BEGIN_TILE_LOOP(tile, size_x, size_y, TileXY(sx, sy)) {
		int32 ret;

		if (GetTileSlope(tile, NULL) != SLOPE_FLAT) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
		}

		// can't make water of water!
		if (IsTileType(tile, MP_WATER) && (!IsTileOwner(tile, OWNER_WATER) || HASBIT(p2, 0))) continue;

		ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost += ret;

		if (flags & DC_EXEC) {
			if (TileHeight(tile) == 0 && HASBIT(p2, 0)) {
				MakeWater(tile);
			} else {
				MakeCanal(tile, _current_player);
			}
			MarkTileDirtyByTile(tile);
			MarkTilesAroundDirty(tile);
		}

		cost += _price.clear_water;
	} END_TILE_LOOP(tile, size_x, size_y, 0);

	if (cost == 0) {
		return_cmd_error(STR_1007_ALREADY_BUILT);
	} else {
		return cost;
	}
}

static int32 ClearTile_Water(TileIndex tile, byte flags)
{
	switch (GetWaterTileType(tile)) {
		case WATER_CLEAR:
			if (flags & DC_NO_WATER) return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

			// Make sure no vehicle is on the tile
			if (!EnsureNoVehicle(tile)) return CMD_ERROR;

			// Make sure it's not an edge tile.
			if (!IS_INT_INSIDE(TileX(tile), 1, MapMaxX() - 1) ||
					!IS_INT_INSIDE(TileY(tile), 1, MapMaxY() - 1)) {
				return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);
			}

			if (GetTileOwner(tile) != OWNER_WATER && !CheckTileOwnership(tile)) return CMD_ERROR;

			if (flags & DC_EXEC) DoClearSquare(tile);
			return _price.clear_water;

		case WATER_COAST: {
			Slope slope = GetTileSlope(tile, NULL);

			// Make sure no vehicle is on the tile
			if (!EnsureNoVehicle(tile)) return CMD_ERROR;

			// Make sure it's not an edge tile.
			if (!IS_INT_INSIDE(TileX(tile), 1, MapMaxX() - 1) ||
					!IS_INT_INSIDE(TileY(tile), 1, MapMaxY() - 1)) {
				return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);
			}

			if (flags & DC_EXEC) DoClearSquare(tile);
			if (slope == SLOPE_N || slope == SLOPE_E || slope == SLOPE_S || slope == SLOPE_W) {
				return _price.clear_water;
			} else {
				return _price.purchase_land;
			}
		}

		case WATER_LOCK: {
			static const TileIndexDiffC _shiplift_tomiddle_offs[] = {
				{ 0,  0}, {0,  0}, { 0, 0}, {0,  0}, // middle
				{-1,  0}, {0,  1}, { 1, 0}, {0, -1}, // lower
				{ 1,  0}, {0, -1}, {-1, 0}, {0,  1}, // upper
			};

			if (flags & DC_AUTO) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
			if (_current_player == OWNER_WATER) return CMD_ERROR;
			// move to the middle tile..
			return RemoveShiplift(tile + ToTileIndexDiff(_shiplift_tomiddle_offs[GetSection(tile)]), flags);
		}

		case WATER_DEPOT:
			if (flags & DC_AUTO) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
			return RemoveShipDepot(tile, flags);

		default:
			NOT_REACHED();
			return 0;
	}

	return 0; // useless but silences warning
}

// return true if a tile is a water tile.
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

		case MP_STATION: return IsOilRig(tile) || IsDock(tile) || IsBuoy_(tile);
		default:         return false;
	}
}

// draw a canal styled water tile with dikes around
void DrawCanalWater(TileIndex tile)
{
	uint wa;

	// determine the edges around with water.
	wa = IsWateredTile(TILE_ADDXY(tile, -1, 0)) << 0;
	wa += IsWateredTile(TILE_ADDXY(tile, 0, 1)) << 1;
	wa += IsWateredTile(TILE_ADDXY(tile, 1, 0)) << 2;
	wa += IsWateredTile(TILE_ADDXY(tile, 0, -1)) << 3;

	if (!(wa & 1)) DrawGroundSprite(SPR_CANALS_BASE + 57);
	if (!(wa & 2)) DrawGroundSprite(SPR_CANALS_BASE + 58);
	if (!(wa & 4)) DrawGroundSprite(SPR_CANALS_BASE + 59);
	if (!(wa & 8)) DrawGroundSprite(SPR_CANALS_BASE + 60);

	// right corner
	switch (wa & 0x03) {
		case 0: DrawGroundSprite(SPR_CANALS_BASE + 57 + 4); break;
		case 3: if (!IsWateredTile(TILE_ADDXY(tile, -1, 1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 8); break;
	}

	// bottom corner
	switch (wa & 0x06) {
		case 0: DrawGroundSprite(SPR_CANALS_BASE + 57 + 5); break;
		case 6: if (!IsWateredTile(TILE_ADDXY(tile, 1, 1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 9); break;
	}

	// left corner
	switch (wa & 0x0C) {
		case  0: DrawGroundSprite(SPR_CANALS_BASE + 57 + 6); break;
		case 12: if (!IsWateredTile(TILE_ADDXY(tile, 1, -1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 10); break;
	}

	// upper corner
	switch (wa & 0x09) {
		case 0: DrawGroundSprite(SPR_CANALS_BASE + 57 + 7); break;
		case 9: if (!IsWateredTile(TILE_ADDXY(tile, -1, -1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 11); break;
	}
}

typedef struct LocksDrawTileStruct {
	int8 delta_x, delta_y, delta_z;
	byte width, height, depth;
	SpriteID image;
} LocksDrawTileStruct;

#include "table/water_land.h"

static void DrawWaterStuff(const TileInfo *ti, const WaterDrawTileStruct *wdts,
	uint32 palette, uint base
)
{
	DrawGroundSprite(wdts++->image);

	for (; wdts->delta_x != 0x80; wdts++) {
		uint32 image = wdts->image + base;
		if (_display_opt & DO_TRANS_BUILDINGS) {
			MAKE_TRANSPARENT(image);
		} else {
			image |= palette;
		}
		AddSortableSpriteToDraw(image, ti->x + wdts->delta_x, ti->y + wdts->delta_y, wdts->width, wdts->height, wdts->unk, ti->z + wdts->delta_z);
	}
}

static void DrawTile_Water(TileInfo *ti)
{
	switch (GetWaterTileType(ti->tile)) {
		case WATER_CLEAR:
			DrawGroundSprite(SPR_FLAT_WATER_TILE);
			if (ti->z != 0 || !IsTileOwner(ti->tile, OWNER_WATER)) DrawCanalWater(ti->tile);
			DrawBridgeMiddle(ti);
			break;

		case WATER_COAST:
			assert(!IsSteepSlope(ti->tileh));
			if (_coast_base != 0) {
				DrawGroundSprite(_coast_base + ti->tileh);
			} else {
				DrawGroundSprite(_water_shore_sprites[ti->tileh]);
			}
			DrawBridgeMiddle(ti);
			break;

		case WATER_LOCK: {
			const WaterDrawTileStruct *t = _shiplift_display_seq[GetSection(ti->tile)];
			DrawWaterStuff(ti, t, 0, ti->z > t[3].delta_y ? 24 : 0);
		} break;

		case WATER_DEPOT:
			DrawWaterStuff(ti, _shipdepot_display_seq[GetSection(ti->tile)], PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)), 0);
			break;
	}
}

void DrawShipDepotSprite(int x, int y, int image)
{
	const WaterDrawTileStruct *wdts = _shipdepot_display_seq[image];

	DrawSprite(wdts++->image, x, y);

	for (; wdts->delta_x != 0x80; wdts++) {
		Point pt = RemapCoords(wdts->delta_x, wdts->delta_y, wdts->delta_z);
		DrawSprite(wdts->image + PLAYER_SPRITE_COLOR(_local_player), x + pt.x, y + pt.y);
	}
}


static uint GetSlopeZ_Water(TileIndex tile, uint x, uint y)
{
	uint z;
	uint tileh = GetTileSlope(tile, &z);

	return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
}

static Slope GetSlopeTileh_Water(TileIndex tile, Slope tileh)
{
	return tileh;
}

static void GetAcceptedCargo_Water(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Water(TileIndex tile, TileDesc *td)
{
	switch (GetWaterTileType(tile)) {
		case WATER_CLEAR:
			if (TilePixelHeight(tile) == 0 || IsTileOwner(tile, OWNER_WATER)) {
				td->str = STR_3804_WATER;
			} else {
				td->str = STR_LANDINFO_CANAL;
			}
			break;
		case WATER_COAST: td->str = STR_3805_COAST_OR_RIVERBANK; break;
		case WATER_LOCK : td->str = STR_LANDINFO_LOCK; break;
		case WATER_DEPOT: td->str = STR_3806_SHIP_DEPOT; break;
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

	// type of this tile mustn't be water already.
	if (IsTileType(target, MP_WATER)) return;

	if (TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[1]))) != 0 ||
			TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[2]))) != 0) {
		return;
	}

	if (TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[3]))) != 0 ||
			TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[4]))) != 0) {
		// make coast..
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
				if (!CmdFailed(DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
					MakeShore(target);
					MarkTileDirtyByTile(target);
				}
				break;

			default:
				break;
		}
	} else {
		_current_player = OWNER_WATER;
		{
			Vehicle *v = FindVehicleOnTileZ(target, 0);
			if (v != NULL) FloodVehicle(v);
		}

		if (!CmdFailed(DoCommand(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
			MakeWater(target);
			MarkTileDirtyByTile(target);
		}
	}
}

static void FloodVehicle(Vehicle *v)
{
	if (!(v->vehstatus & VS_CRASHED)) {
		uint16 pass = 0;

		if (v->type == VEH_Road) { // flood bus/truck
			pass = 1; // driver
			if (v->cargo_type == CT_PASSENGERS)
				pass += v->cargo_count;

			v->vehstatus |= VS_CRASHED;
			v->u.road.crashed_ctr = 2000; // max 2220, disappear pretty fast
			RebuildVehicleLists();
		} else if (v->type == VEH_Train) {
			Vehicle *u;

			v = GetFirstVehicleInChain(v);
			u = v;
			if (IsFrontEngine(v)) pass = 4; // driver

			// crash all wagons, and count passangers
			BEGIN_ENUM_WAGONS(v)
				if (v->cargo_type == CT_PASSENGERS) pass += v->cargo_count;
				v->vehstatus |= VS_CRASHED;
			END_ENUM_WAGONS(v)

			v = u;
			v->u.rail.crash_anim_pos = 4000; // max 4440, disappear pretty fast
			RebuildVehicleLists();
		} else {
			return;
		}

		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

		SetDParam(0, pass);
		AddNewsItem(STR_B006_FLOOD_VEHICLE_DESTROYED,
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
			v->index,
			0);
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
		SndPlayVehicleFx(SND_12_EXPLOSION, v);
	}
}

// called from tunnelbridge_cmd, and by TileLoop_Industry()
void TileLoop_Water(TileIndex tile)
{
	static const TileIndexDiffC _tile_loop_offs_array[][5] = {
		// tile to mod              shore?    shore?
		{{-1,  0}, {0, 0}, {0, 1}, {-1,  0}, {-1,  1}},
		{{ 0,  1}, {0, 1}, {1, 1}, { 0,  2}, { 1,  2}},
		{{ 1,  0}, {1, 0}, {1, 1}, { 2,  0}, { 2,  1}},
		{{ 0, -1}, {0, 0}, {1, 0}, { 0, -1}, { 1, -1}}
	};

	/* Ensure sea-level canals do not flood */
	if ((IsTileType(tile, MP_WATER) || IsTileType(tile, MP_TUNNELBRIDGE)) &&
			!IsTileOwner(tile, OWNER_WATER)) return;

	if (IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1) &&
			IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) {
		uint i;

		for (i = 0; i != lengthof(_tile_loop_offs_array); i++) {
			TileLoopWaterHelper(tile, _tile_loop_offs_array[i]);
		}
	}
	// _current_player can be changed by TileLoopWaterHelper.. reset it back
	//   here
	_current_player = OWNER_NONE;

	// edges
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

static uint32 GetTileTrackStatus_Water(TileIndex tile, TransportType mode)
{
	static const byte coast_tracks[] = {0, 32, 4, 0, 16, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};

	TrackBits ts;

	if (mode != TRANSPORT_WATER) return 0;

	switch (GetWaterTileType(tile)) {
		case WATER_CLEAR: ts = TRACK_BIT_ALL; break;
		case WATER_COAST: ts = coast_tracks[GetTileSlope(tile, NULL) & 0xF]; break;
		case WATER_LOCK:  ts = AxisToTrackBits(DiagDirToAxis(GetLockDirection(tile))); break;
		case WATER_DEPOT: ts = AxisToTrackBits(GetShipDepotAxis(tile)); break;
		default: return 0;
	}
	if (TileX(tile) == 0) {
		// NE border: remove tracks that connects NE tile edge
		ts &= ~(TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT);
	}
	if (TileY(tile) == 0) {
		// NW border: remove tracks that connects NW tile edge
		ts &= ~(TRACK_BIT_Y | TRACK_BIT_LEFT | TRACK_BIT_UPPER);
	}
	return ts * 0x101;
}

static void ClickTile_Water(TileIndex tile)
{
	if (GetWaterTileType(tile) == WATER_DEPOT) {
		TileIndex tile2 = GetOtherShipDepotTile(tile);

		ShowDepotWindow(tile < tile2 ? tile : tile2, VEH_Ship);
	}
}

static void ChangeTileOwner_Water(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else {
		DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static uint32 VehicleEnter_Water(Vehicle *v, TileIndex tile, int x, int y)
{
	return 0;
}


const TileTypeProcs _tile_type_water_procs = {
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
	GetSlopeTileh_Water,      /* get_slope_tileh_proc */
};
