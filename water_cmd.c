/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
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

static void FloodVehicle(Vehicle *v);

static bool IsClearWaterTile(TileIndex tile)
{
	TileInfo ti;
	FindLandscapeHeightByTile(&ti, tile);
	return (ti.type == MP_WATER && ti.tileh == 0 && ti.map5 == 0);
}

/** Build a ship depot.
 * @param x,y tile coordinates where ship depot is built
 * @param p1 depot direction (0 through 3), where 0 is NW, 1 is NE, etc.
 * @param p2 unused
 */
int32 CmdBuildShipDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile, tile2;

	int32 cost, ret;
	Depot *depot;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 > 3) return CMD_ERROR;

	tile = TileVirtXY(x, y);
	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	tile2 = tile + (p1 ? TileDiffXY(0, 1) : TileDiffXY(1, 0));
	if (!EnsureNoVehicle(tile2)) return CMD_ERROR;

	if (!IsClearWaterTile(tile) || !IsClearWaterTile(tile2))
		return_cmd_error(STR_3801_MUST_BE_BUILT_ON_WATER);

	ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;
	ret = DoCommandByTile(tile2, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return CMD_ERROR;

	// pretend that we're not making land from the water even though we actually are.
	cost = 0;

	depot = AllocateDepot();
	if (depot == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		depot->xy = tile;
		_last_built_ship_depot_tile = tile;
		depot->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		ModifyTile(tile,
			MP_SETTYPE(MP_WATER) | MP_MAPOWNER_CURRENT | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
			(0x80 + p1*2)
		);

		ModifyTile(tile2,
			MP_SETTYPE(MP_WATER) | MP_MAPOWNER_CURRENT | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
			(0x81 + p1*2)
		);
	}

	return cost + _price.build_ship_depot;
}

static int32 RemoveShipDepot(TileIndex tile, uint32 flags)
{
	TileIndex tile2;

	if (!CheckTileOwnership(tile))
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	tile2 = tile + ((_m[tile].m5 & 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0));

	if (!EnsureNoVehicle(tile2))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* Kill the depot */
		DoDeleteDepot(tile);

		/* Make the tiles water */
		ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);
		ModifyTile(tile2, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);
	}

	return _price.remove_ship_depot;
}

// build a shiplift
static int32 DoBuildShiplift(TileIndex tile, int dir, uint32 flags)
{
	int32 ret;
	int delta;

	// middle tile
	ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;

	delta = TileOffsByDir(dir);
	// lower tile
	ret = DoCommandByTile(tile - delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;
	if (GetTileSlope(tile - delta, NULL)) return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	// upper tile
	ret = DoCommandByTile(tile + delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;
	if (GetTileSlope(tile + delta, NULL)) return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	if (flags & DC_EXEC) {
		ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0x10 + dir);
		ModifyTile(tile - delta, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0x14 + dir);
		ModifyTile(tile + delta, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0x18 + dir);
	}

	return _price.clear_water * 22 >> 3;
}

static int32 RemoveShiplift(TileIndex tile, uint32 flags)
{
	TileIndexDiff delta = TileOffsByDir(_m[tile].m5 & 3);

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
 * @param x,y tile coordinates where to place the lock
 * @param p1 unused
 * @param p2 unused
 */
int32 CmdBuildLock(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex tile = TileVirtXY(x, y);
	uint tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);
	tileh = GetTileSlope(tile, NULL);

	if (tileh == 3 || tileh == 6 || tileh == 9 || tileh == 12) {
		static const byte _shiplift_dirs[16] = {0, 0, 0, 2, 0, 0, 1, 0, 0, 3, 0, 0, 0};
		return DoBuildShiplift(tile, _shiplift_dirs[tileh], flags);
	}

	return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
}

/** Build a piece of canal.
 * @param x,y end tile of stretch-dragging
 * @param p1 start tile of stretch-dragging
 * @param p2 unused
 */
int32 CmdBuildCanal(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int32 ret, cost;
	int size_x, size_y;
	int sx, sy;

	if (p1 > MapSize()) return CMD_ERROR;

	sx = TileX(p1);
	sy = TileY(p1);
	/* x,y are in pixel-coordinates, transform to tile-coordinates
	 * to be able to use the BEGIN_TILE_LOOP() macro */
	x >>= 4; y >>= 4;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (x < sx) intswap(x, sx);
	if (y < sy) intswap(y, sy);
	size_x = (x - sx) + 1;
	size_y = (y - sy) + 1;

	/* Outside the editor you can only drag canals, and not areas */
	if (_game_mode != GM_EDITOR && (sx != x && sy != y)) return CMD_ERROR;

	cost = 0;
	BEGIN_TILE_LOOP(tile, size_x, size_y, TileXY(sx, sy)) {
		ret = 0;
		if (GetTileSlope(tile, NULL) != 0) return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);

			// can't make water of water!
			if (IsTileType(tile, MP_WATER)) {
				_error_message = STR_1007_ALREADY_BUILT;
			} else {
				/* is middle piece of a bridge? */
				if (IsTileType(tile, MP_TUNNELBRIDGE) && _m[tile].m5 & 0x40) { /* build under bridge */
					if (_m[tile].m5 & 0x20) // transport route under bridge
						return_cmd_error(STR_5800_OBJECT_IN_THE_WAY);

					if (_m[tile].m5 & 0x18) // already water under bridge
						return_cmd_error(STR_1007_ALREADY_BUILT);
				/* no bridge? then try to clear it. */
				} else
					ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);

				if (CmdFailed(ret)) return CMD_ERROR;
				cost += ret;

				/* execute modifications */
				if (flags & DC_EXEC) {
					if (IsTileType(tile, MP_TUNNELBRIDGE)) {
						// change owner to OWNER_WATER and set land under bridge bit to water
						ModifyTile(tile, MP_MAP5 | MP_MAPOWNER, OWNER_WATER, _m[tile].m5 | 0x08);
					} else {
						ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);
					}
					// mark the tiles around dirty too
					MarkTilesAroundDirty(tile);
				}

				cost += _price.clear_water;
			}
	} END_TILE_LOOP(tile, size_x, size_y, 0);

	return (cost == 0) ? CMD_ERROR : cost;
}

static int32 ClearTile_Water(TileIndex tile, byte flags)
{
	byte m5 = _m[tile].m5;
	uint slope;

	if (m5 <= 1) { // water and shore
		// Allow building on water? It's ok to build on shores.
		if (flags & DC_NO_WATER && m5 != 1)
			return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

		// Make sure no vehicle is on the tile
		if (!EnsureNoVehicle(tile))
			return CMD_ERROR;

		// Make sure it's not an edge tile.
		if (!(IS_INT_INSIDE(TileX(tile), 1, MapMaxX() - 1) &&
				IS_INT_INSIDE(TileY(tile), 1, MapMaxY() - 1)))
			return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);

		if (m5 == 0) {
			if (flags & DC_EXEC)
				DoClearSquare(tile);
			return _price.clear_water;
		} else if (m5 == 1) {
			slope = GetTileSlope(tile,NULL);
			if (slope == 8 || slope == 4 || slope == 2 || slope == 1) {
				if (flags & DC_EXEC)
					DoClearSquare(tile);
				return _price.clear_water;
			}
			if (flags & DC_EXEC)
				DoClearSquare(tile);
			return _price.purchase_land;
		} else
			return CMD_ERROR;
	} else if ((m5 & 0x10) == 0x10) {
		// shiplift

		static const TileIndexDiffC _shiplift_tomiddle_offs[] = {
			{ 0,  0}, {0,  0}, { 0, 0}, {0,  0}, // middle
			{-1,  0}, {0,  1}, { 1, 0}, {0, -1}, // lower
			{ 1,  0}, {0, -1}, {-1, 0}, {0,  1}, // upper
		};

		if (flags & DC_AUTO) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
		// don't allow water to delete it.
		if (_current_player == OWNER_WATER) return CMD_ERROR;
		// move to the middle tile..
		return RemoveShiplift(tile + ToTileIndexDiff(_shiplift_tomiddle_offs[m5 & 0xF]), flags);
	} else {
		// ship depot
		if (flags & DC_AUTO)
			return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);

		if (m5 == 0x80 || m5 == 0x82) {}
		else if (m5 == 0x81) { tile -= TileDiffXY(1, 0); }
		else if (m5 == 0x83) { tile -= TileDiffXY(0, 1); }
		else
			return CMD_ERROR;

		return RemoveShipDepot(tile,flags);
	}
}

// return true if a tile is a water tile.
static bool IsWateredTile(TileIndex tile)
{
	byte m5 = _m[tile].m5;

	switch (GetTileType(tile)) {
		case MP_WATER:
			// true, if not coast/riverbank
			return m5 != 1;

		case MP_STATION:
			// returns true if it is a dock-station
			// m5 inside values is m5 < 75 all stations, 83 <= m5 <= 114 new airports
			return !(m5 < 75 || (m5 >= 83 && m5 <= 114));

		case MP_TUNNELBRIDGE:
			// true, if tile is middle part of bridge with water underneath
			return (m5 & 0xF8) == 0xC8;

		default:
			return false;
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
	if ((wa & 3) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 4);
	else if ((wa & 3) == 3 && !IsWateredTile(TILE_ADDXY(tile, -1, 1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 8);

	// bottom corner
	if ((wa & 6) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 5);
	else if ((wa & 6) == 6 && !IsWateredTile(TILE_ADDXY(tile, 1, 1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 9);

	// left corner
	if ((wa & 12) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 6);
	else if ((wa & 12) == 12 && !IsWateredTile(TILE_ADDXY(tile, 1, -1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 10);

	// upper corner
	if ((wa & 9) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 7);
	else if ((wa & 9) == 9 && !IsWateredTile(TILE_ADDXY(tile, -1, -1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 11);
}

typedef struct LocksDrawTileStruct {
	int8 delta_x, delta_y, delta_z;
	byte width, height, depth;
	SpriteID image;
} LocksDrawTileStruct;

#include "table/water_land.h"

static void DrawWaterStuff(TileInfo *ti, const WaterDrawTileStruct *wdts,
	uint32 palette, uint base
)
{
	uint32 image;

	DrawGroundSprite(wdts++->image);

	for (; wdts->delta_x != 0x80; wdts++) {
		image =	wdts->image + base;
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
	// draw water tile
	if (ti->map5 == 0) {
		DrawGroundSprite(0xFDD);
		if (ti->z != 0) DrawCanalWater(ti->tile);
		return;
	}

	// draw shore
	if (ti->map5 == 1) {
		assert(ti->tileh < 16);
		DrawGroundSprite(_water_shore_sprites[ti->tileh]);
		return;
	}

	// draw shiplift
	if ((ti->map5 & 0xF0) == 0x10) {
		const WaterDrawTileStruct *t = _shiplift_display_seq[ti->map5 & 0xF];
		DrawWaterStuff(ti, t, 0, ti->z > t[3].delta_y ? 24 : 0);
		return;
	}

	DrawWaterStuff(ti, _shipdepot_display_seq[ti->map5 & 0x7F], PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile)), 0);
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


static uint GetSlopeZ_Water(TileInfo *ti)
{
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Water(TileInfo *ti)
{
	return ti->tileh;
}

static void GetAcceptedCargo_Water(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Water(TileIndex tile, TileDesc *td)
{
	if (_m[tile].m5 == 0 && TilePixelHeight(tile) == 0)
		td->str = STR_3804_WATER;
	else if (_m[tile].m5 == 0)
		td->str = STR_LANDINFO_CANAL;
	else if (_m[tile].m5 == 1)
		td->str = STR_3805_COAST_OR_RIVERBANK;
	else if ((_m[tile].m5&0xF0) == 0x10)
		td->str = STR_LANDINFO_LOCK;
	else
		td->str = STR_3806_SHIP_DEPOT;

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
	if (IsTileType(target, MP_WATER))
		return;

	if (TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[1]))) != 0 ||
			TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[2]))) != 0)
		return;

	if (TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[3]))) != 0 ||
			TileHeight(TILE_ADD(tile, ToTileIndexDiff(offs[4]))) != 0) {
		// make coast..
		switch (GetTileType(target)) {
			case MP_RAILWAY: {
				uint slope = GetTileSlope(target, NULL);
				byte tracks = _m[target].m5 & 0x3F;
				if (!(
						(slope == 1 && tracks == 0x20) ||
						(slope == 2 && tracks == 0x04) ||
						(slope == 4 && tracks == 0x10) ||
						(slope == 8 && tracks == 0x08)))
					break;
			}
			/* FALLTHROUGH */

			case MP_CLEAR:
			case MP_TREES:
				_current_player = OWNER_WATER;
				if (DoCommandByTile(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR) != CMD_ERROR) {
					ModifyTile(
						target,
						MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR |
							MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
						OWNER_WATER, 1
					);
				}
				break;

			case MP_TUNNELBRIDGE:
				// Middle part of bridge with clear land below?
				if ((_m[target].m5 & 0xF8) == 0xC0) {
					_m[target].m5 |= 0x08;
					MarkTileDirtyByTile(tile);
				}
				break;

			default:
				break;
		}
	} else {
		if (IsTileType(target, MP_TUNNELBRIDGE)) {
			byte m5 = _m[target].m5;
			if ((m5 & 0xF8) == 0xC8 || (m5 & 0xF8) == 0xF0)
				return;

			if ((m5 & 0xC0) == 0xC0) {
				ModifyTile(target, MP_MAPOWNER | MP_MAP5, OWNER_WATER, (m5 & ~0x38) | 0x8);
				return;
			}
		}

		_current_player = OWNER_WATER;
		{
			Vehicle *v = FindVehicleOnTileZ(target, 0);
			if (v != NULL) FloodVehicle(v);
		}

		if (DoCommandByTile(target, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR) != CMD_ERROR) {
			ModifyTile(
				target,
				MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR |
					MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
				OWNER_WATER,
				0
			);
		}
	}
}

static void FloodVehicle(Vehicle *v)
{
	Vehicle *u;
	if (!(v->vehstatus & VS_CRASHED)) {
		uint16 pass = 0;

		if (v->type == VEH_Road) {	// flood bus/truck
			pass = 1;	// driver
			if (v->cargo_type == CT_PASSENGERS)
				pass += v->cargo_count;

			v->vehstatus |= VS_CRASHED;
			v->u.road.crashed_ctr = 2000;	// max 2220, disappear pretty fast
			RebuildVehicleLists();
		}

		else if (v->type == VEH_Train) {
			v = GetFirstVehicleInChain(v);
			u = v;
			if (v->subtype == TS_Front_Engine) pass = 4; // driver

			// crash all wagons, and count passangers
			BEGIN_ENUM_WAGONS(v)
				if (v->cargo_type == CT_PASSENGERS) pass += v->cargo_count;
				v->vehstatus |= VS_CRASHED;
			END_ENUM_WAGONS(v)

			v = u;
			v->u.rail.crash_anim_pos = 4000; // max 4440, disappear pretty fast
			RebuildVehicleLists();
		} else
			return;

		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

		SetDParam(0, pass);
		AddNewsItem(STR_B006_FLOOD_VEHICLE_DESTROYED,
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
			v->index,
			0);
	CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE); // show cool destruction effects
	SndPlayVehicleFx(SND_12_EXPLOSION, v); // create sound
	}
}

// called from tunnelbridge_cmd
void TileLoop_Water(TileIndex tile)
{
	int i;
	static const TileIndexDiffC _tile_loop_offs_array[][5] = {
		// tile to mod																shore?				shore?
		{{-1,  0}, {0, 0}, {0, 1}, {-1,  0}, {-1,  1}},
		{{ 0,  1}, {0, 1}, {1, 1}, { 0,  2}, { 1,  2}},
		{{ 1,  0}, {1, 0}, {1, 1}, { 2,  0}, { 2,  1}},
		{{ 0, -1}, {0, 0}, {1, 0}, { 0, -1}, { 1, -1}}
	};

	if (IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1) &&
			IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) {
		for(i=0; i!=4; i++)
			TileLoopWaterHelper(tile, _tile_loop_offs_array[i]);
	}
	// _current_player can be changed by TileLoopWaterHelper.. reset it back
	//   here
	_current_player = OWNER_NONE;

	// edges
	if (TileX(tile) == 0 && IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) //NE
		TileLoopWaterHelper(tile, _tile_loop_offs_array[2]);

	if (TileX(tile) == (MapSizeX() - 2) && IS_INT_INSIDE(TileY(tile), 1, MapSizeY() - 3 + 1)) //SW
		TileLoopWaterHelper(tile, _tile_loop_offs_array[0]);

	if (TileY(tile) == 0 && IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1)) //NW
		TileLoopWaterHelper(tile, _tile_loop_offs_array[1]);

	if (TileY(tile) == (MapSizeY() - 2) && IS_INT_INSIDE(TileX(tile), 1, MapSizeX() - 3 + 1)) //SE
		TileLoopWaterHelper(tile, _tile_loop_offs_array[3]);

}


static const byte _coast_tracks[16] = {0, 32, 4, 0, 16, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};
static const byte _shipdepot_tracks[4] = {1,1,2,2};
static const byte _shiplift_tracks[12] = {1,2,1,2,1,2,1,2,1,2,1,2};
static uint32 GetTileTrackStatus_Water(TileIndex tile, TransportType mode)
{
	uint m5;
	uint b;

	if (mode != TRANSPORT_WATER)
		return 0;

	m5 = _m[tile].m5;
	if (m5 == 0)
		return 0x3F3F;

	if (m5 == 1) {
		b = _coast_tracks[GetTileSlope(tile, NULL)&0xF];
		return b + (b<<8);
	}

	if ( (m5 & 0x10) == 0x10) {
		//
		b = _shiplift_tracks[m5 & 0xF];
		return b + (b<<8);
	}

	if (!(m5 & 0x80))
		return 0;

	b = _shipdepot_tracks[m5 & 0x7F];
	return b + (b<<8);
}

extern void ShowShipDepotWindow(TileIndex tile);

static void ClickTile_Water(TileIndex tile)
{
	byte m5 = _m[tile].m5 - 0x80;

	if (IS_BYTE_INSIDE(m5, 0, 3+1)) {
		if (m5 & 1)
			tile += (m5 == 1) ? TileDiffXY(-1, 0) : TileDiffXY(0, -1);
		ShowShipDepotWindow(tile);
	}
}

static void ChangeTileOwner_Water(TileIndex tile, byte old_player, byte new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != 255) {
		SetTileOwner(tile, new_player);
	} else {
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static uint32 VehicleEnter_Water(Vehicle *v, TileIndex tile, int x, int y)
{
	return 0;
}

void InitializeDock(void)
{
	_last_built_ship_depot_tile = 0;
}

const TileTypeProcs _tile_type_water_procs = {
	DrawTile_Water,						/* draw_tile_proc */
	GetSlopeZ_Water,					/* get_slope_z_proc */
	ClearTile_Water,					/* clear_tile_proc */
	GetAcceptedCargo_Water,		/* get_accepted_cargo_proc */
	GetTileDesc_Water,				/* get_tile_desc_proc */
	GetTileTrackStatus_Water,	/* get_tile_track_status_proc */
	ClickTile_Water,					/* click_tile_proc */
	AnimateTile_Water,				/* animate_tile_proc */
	TileLoop_Water,						/* tile_loop_clear */
	ChangeTileOwner_Water,		/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	VehicleEnter_Water,				/* vehicle_enter_tile_proc */
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Water,			/* get_slope_tileh_proc */
};

