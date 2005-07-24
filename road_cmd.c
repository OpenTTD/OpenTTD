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
#include "player.h"
#include "town.h"
#include "gfx.h"
#include "sound.h"
#include "depot.h"
#include "pbs.h"
#include "debug.h"

/* When true, GetTrackStatus for roads will treat roads under reconstruction
 * as normal roads instead of impassable. This is used when detecting whether
 * a road can be removed. This is of course ugly, but I don't know a better
 * solution just like that... */
static bool _road_special_gettrackstatus;

void RoadVehEnterDepot(Vehicle *v);


static bool HasTileRoadAt(TileIndex tile, int i)
{
	int mask;
	byte b;

	switch (GetTileType(tile)) {
	case MP_STREET:
		b = _m[tile].m5;

		if ((b & 0xF0) == 0) {
		} else if (IsLevelCrossing(tile)) {
			b = (b&8)?5:10;
		} else if ((b & 0xF0) == 0x20) {
			return (~b & 3) == i;
		} else
			return false;
		break;

	case MP_STATION:
		b = _m[tile].m5;
		if (!IS_BYTE_INSIDE(b, 0x43, 0x43+8))
			return false;
		return ((~(b - 0x43) & 3) == i);

	case MP_TUNNELBRIDGE:
		mask = GetRoadBitsByTile(tile);
		b = 10; if (mask & 1) break;
		b = 5;  if (mask & 2) break;
		return false;

	default:
		return false;
	}

	return HASBIT(b, i);
}

static bool CheckAllowRemoveRoad(TileIndex tile, uint br, bool *edge_road)
{
	int blocks;
	byte owner;
	uint n;
	*edge_road = true;

	if (_game_mode == GM_EDITOR)
		return true;

	blocks = GetRoadBitsByTile(tile);
	if (blocks == 0)
		return true;

	// Only do the special processing for actual players.
	if (_current_player >= MAX_PLAYERS)
		return true;

	// A railway crossing has the road owner in the map3_lo byte.
	if (IsTileType(tile, MP_STREET) && IsLevelCrossing(tile)) {
		owner = _m[tile].m3;
	} else {
		owner = GetTileOwner(tile);
	}
	// Only do the special processing if the road is owned
	// by a town
	if (owner != OWNER_TOWN) {
		return owner == OWNER_NONE || CheckOwnership(owner);
	}

	if (_cheats.magic_bulldozer.value)
		return true;

	// Get a bitmask of which neighbouring roads has a tile
	n = 0;
	if (blocks&0x25 && HasTileRoadAt(TILE_ADDXY(tile,-1, 0), 1)) n |= 8;
	if (blocks&0x2A && HasTileRoadAt(TILE_ADDXY(tile, 0, 1), 0)) n |= 4;
	if (blocks&0x19 && HasTileRoadAt(TILE_ADDXY(tile, 1, 0), 3)) n |= 2;
	if (blocks&0x16 && HasTileRoadAt(TILE_ADDXY(tile, 0,-1), 2)) n |= 1;

	// If 0 or 1 bits are set in n, or if no bits that match the bits to remove,
	// then allow it
	if ((n & (n-1)) != 0 && (n & br) != 0) {
		Town *t;
		*edge_road = false;
		// you can remove all kind of roads with extra dynamite
		if (_patches.extra_dynamite)
			return true;

		t = ClosestTownFromTile(tile, _patches.dist_local_authority);

		SetDParam(0, t->index);
		_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
		return false;
	}

	return true;
}

uint GetRoadBitsByTile(TileIndex tile)
{
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_ROAD);
	return (byte)(r | (r >> 8));
}

/** Delete a piece of road.
 * @param x,y tile coordinates for road construction
 * @param p1 road piece flags
 * @param p2 unused
 */
int32 CmdRemoveRoad(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	// cost for removing inner/edge -roads
	static const uint16 road_remove_cost[2] = {50, 18};

	TileInfo ti;
	int32 cost;
	TileIndex tile;
	PlayerID owner;
	Town *t;
	/* true if the roadpiece was always removeable,
	 * false if it was a center piece. Affects town ratings drop */
	bool edge_road;
	byte pieces = (byte)p1;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) */
	if (pieces >> 4) return CMD_ERROR;

	FindLandscapeHeight(&ti, x, y);
	tile = ti.tile;

	if (!IsTileType(tile, MP_STREET) && !IsTileType(tile, MP_TUNNELBRIDGE)) return CMD_ERROR;

	// owner for railroad crossing is stored somewhere else
	// XXX - Fix this so for a given tiletype the owner of the type is in the same variable
	owner = IsLevelCrossing(tile) ? _m[tile].m3 : GetTileOwner(tile);

	if (owner == OWNER_TOWN && _game_mode != GM_EDITOR) {
		if (IsTileType(tile, MP_TUNNELBRIDGE)) { // index of town is not saved for bridge (no space)
			t = ClosestTownFromTile(tile, _patches.dist_local_authority);
		} else
			t = GetTown(_m[tile].m2);
	} else
		t = NULL;

	// allow deleting road under bridge
	if (ti.type != MP_TUNNELBRIDGE && !EnsureNoVehicle(tile))
		return CMD_ERROR;

	{
		bool b;
		_road_special_gettrackstatus = true;
		b = CheckAllowRemoveRoad(tile, pieces, &edge_road);
		_road_special_gettrackstatus = false;
		if (!b) return CMD_ERROR;
	}

	if (ti.type == MP_TUNNELBRIDGE) {
		if (!EnsureNoVehicleZ(tile, TilePixelHeight(tile)))
			return CMD_ERROR;

		if ((ti.map5 & 0xE9) == 0xE8) {
			if (pieces & 10) goto return_error;
		} else if ((ti.map5 & 0xE9) == 0xE9) {
			if (pieces & 5) goto return_error;
		} else
			goto return_error;

		cost = _price.remove_road * 2;

		if (flags & DC_EXEC) {
			ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);
			_m[tile].m5 = ti.map5 & 0xC7;
			SetTileOwner(tile, OWNER_NONE);
			MarkTileDirtyByTile(tile);
		}
		return cost;
	} else if (ti.type == MP_STREET) {
		// check if you're allowed to remove the street owned by a town
		// removal allowance depends on difficulty setting
		if (!CheckforTownRating(tile, flags, t, ROAD_REMOVE)) return CMD_ERROR;

		// XXX - change cascading ifs to switch when doing rewrite
		if ((ti.map5 & 0xF0) == 0) { // normal road
			byte c = pieces, t2;

			if (ti.tileh != 0  && (ti.map5 == 5 || ti.map5 == 10)) {
				c |= (c & 0xC) >> 2;
				c |= (c & 0x3) << 2;
			}

			// limit the bits to delete to the existing bits.
			if ((c &= ti.map5) == 0) goto return_error;

			// calculate the cost
			t2 = c;
			cost = 0;
			do {
				if (t2&1) cost += _price.remove_road;
			} while(t2>>=1);

			if (flags & DC_EXEC) {
				ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);

				_m[tile].m5 ^= c;
				if ((_m[tile].m5&0xF) == 0)
					DoClearSquare(tile);
				else
					MarkTileDirtyByTile(tile);

			}
			return cost;
		} else if ((ti.map5 & 0xE0) == 0) { // railroad crossing
			byte c;

			if (!(ti.map5 & 8)) {
				c = 2;
				if (pieces & 5) goto return_error;
			} else {
				c = 1;
				if (pieces & 10) goto return_error;
			}

			cost = _price.remove_road * 2;
			if (flags & DC_EXEC) {
				byte pbs_track = PBSTileReserved(tile);
				ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);

				ModifyTile(tile,
					MP_SETTYPE(MP_RAILWAY) |
					MP_MAP2_CLEAR | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5,
					_m[tile].m4 & 0xF, /* map3_lo */
					c											/* map5 */
				);
				if (pbs_track != 0)
					PBSReserveTrack(tile, FIND_FIRST_BIT(pbs_track));
			}
			return cost;
		} else
			goto return_error;

	} else {
return_error:;
		return_cmd_error(INVALID_STRING_ID);
	}
}


enum {
	ROAD_NW = 1, // NW road track
	ROAD_SW = 2, // SW road track
	ROAD_SE = 4, // SE road track
	ROAD_NE = 8, // NE road track
	ROAD_ALL = (ROAD_NW | ROAD_SW | ROAD_SE | ROAD_NE)
};

static const byte _valid_tileh_slopes_road[3][15] = {
	// set of normal ones
	{
		ROAD_ALL, 0, 0,
		ROAD_SW | ROAD_NE, 0, 0,  // 3, 4, 5
		ROAD_NW | ROAD_SE, 0, 0,
		ROAD_NW | ROAD_SE, 0, 0,  // 9, 10, 11
		ROAD_SW | ROAD_NE, 0, 0
	},
	// allowed road for an evenly raised platform
	{
		0,
		ROAD_SW | ROAD_NW,
		ROAD_SW | ROAD_SE,
		ROAD_NW | ROAD_SE | ROAD_SW,

		ROAD_SE | ROAD_NE, // 4
		ROAD_ALL,
		ROAD_SW | ROAD_NE | ROAD_SE,
		ROAD_ALL,

		ROAD_NW | ROAD_NE, // 8
		ROAD_SW | ROAD_NE | ROAD_NW,
		ROAD_ALL,
		ROAD_ALL,

		ROAD_NW | ROAD_SE | ROAD_NE, // 12
		ROAD_ALL,
		ROAD_ALL
	},
	// valid railway crossings on slopes
	{
		1, 0, 0, // 0, 1, 2
		0, 0, 1, // 3, 4, 5
		0, 1, 0, // 6, 7, 8
		0, 1, 1, // 9, 10, 11
		0, 1, 1, // 12, 13, 14
	}
};


static uint32 CheckRoadSlope(int tileh, byte *pieces, byte existing)
{
	if (!IsSteepTileh(tileh)) {
		byte road_bits = *pieces | existing;

		// no special foundation
		if ((~_valid_tileh_slopes_road[0][tileh] & road_bits) == 0) {
			// force that all bits are set when we have slopes
			if (tileh != 0) *pieces |= _valid_tileh_slopes_road[0][tileh];
			return 0; // no extra cost
		}

		// foundation is used. Whole tile is leveled up
		if ((~_valid_tileh_slopes_road[1][tileh] & road_bits) == 0) {
			return existing ? 0 : _price.terraform;
		}

		// partly leveled up tile, only if there's no road on that tile
		if ( !existing && (tileh == 1 || tileh == 2 || tileh == 4 || tileh == 8) ) {
			// force full pieces.
			*pieces |= (*pieces & 0xC) >> 2;
			*pieces |= (*pieces & 0x3) << 2;
			return (*pieces == (ROAD_NE|ROAD_SW) || *pieces == (ROAD_SE|ROAD_NW)) ? _price.terraform : CMD_ERROR;
		}
	}
	return CMD_ERROR;
}

/** Build a piece of road.
 * @param x,y tile coordinates for road construction
 * @param p1 road piece flags
 * @param p2 the town that is building the road (0 if not applicable)
 */
int32 CmdBuildRoad(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti;
	int32 cost;
	byte pieces = (byte)p1, existing = 0;
	TileIndex tile;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-player is building the road */
	if ((pieces >> 4) || (_current_player < MAX_PLAYERS && p2 != 0) || !IsTownIndex(p2)) return CMD_ERROR;

	FindLandscapeHeight(&ti, x, y);
	tile = ti.tile;

	// allow building road under bridge
	if (ti.type != MP_TUNNELBRIDGE && !EnsureNoVehicle(tile)) return CMD_ERROR;

	if (ti.type == MP_STREET) {
		if (!(ti.map5 & 0xF0)) {
			if ( (pieces & (byte)(ti.map5)) == (pieces))
				return_cmd_error(STR_1007_ALREADY_BUILT);
			existing = ti.map5;
		} else {
			if (!(ti.map5 & 0xE0) && pieces != ((ti.map5 & 8) ? 5 : 10))
				return_cmd_error(STR_1007_ALREADY_BUILT);
			goto do_clear;
		}
	} else if (ti.type == MP_RAILWAY) {
		byte m5;

		if (IsSteepTileh(ti.tileh)) // very steep tile
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

		if(!_valid_tileh_slopes_road[2][ti.tileh]) // prevent certain slopes
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

		if (ti.map5 == 2) {
			if (pieces & 5) goto do_clear;
			m5 = 0x10;
		} else if (ti.map5 == 1) {
			if (pieces & 10) goto do_clear;
			m5 = 0x18;
		} else
			goto do_clear;

		if (flags & DC_EXEC) {
			byte pbs_track = PBSTileReserved(tile);
			ModifyTile(tile,
				MP_SETTYPE(MP_STREET) |
				MP_MAP2 | MP_MAP3LO | MP_MAP3HI | MP_MAP5,
				p2,
				_current_player, /* map3_lo */
				_m[tile].m3 & 0xF, /* map3_hi */
				m5 /* map5 */
			);
			if (pbs_track != 0)
				PBSReserveTrack(tile, FIND_FIRST_BIT(pbs_track));
		}
		return _price.build_road * 2;
	} else if (ti.type == MP_TUNNELBRIDGE) {

		/* check for flat land */
		if (IsSteepTileh(ti.tileh)) // very steep tile
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

		/* is this middle part of a bridge? */
		if ((ti.map5 & 0xC0) != 0xC0)
				goto do_clear;

		/* only allow roads pertendicular to bridge */
		if ((pieces & 5U) == (ti.map5 & 0x01U))
				goto do_clear;

		/* check if clear land under bridge */
		if ((ti.map5 & 0xF8) == 0xE8) 			/* road under bridge */
				return_cmd_error(STR_1007_ALREADY_BUILT);
		else if ((ti.map5 & 0xE0) == 0xE0) 	/* other transport route under bridge */
				return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);
		else if ((ti.map5 & 0xF8) == 0xC8) 	/* water under bridge */
				return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

		/* all checked, can build road now! */
		cost = _price.build_road * 2;
		if (flags & DC_EXEC) {
			ModifyTile(tile,
				MP_MAPOWNER_CURRENT | MP_MAP5,
				(ti.map5 & 0xC7) | 0x28 // map5
			);
		}
		return cost;
	} else {
do_clear:;
		if (CmdFailed(DoCommandByTile(tile, 0, 0, flags & ~DC_EXEC, CMD_LANDSCAPE_CLEAR)))
			return CMD_ERROR;
	}

	cost = CheckRoadSlope(ti.tileh, &pieces, existing);
	if (CmdFailed(cost)) return_cmd_error(STR_1800_LAND_SLOPED_IN_WRONG_DIRECTION);

	if (cost && (!_patches.build_on_slopes || (!_patches.ainew_active && _is_ai_player)))
		return CMD_ERROR;

	if (!(ti.type == MP_STREET && (ti.map5 & 0xF0) == 0)) {
		cost += DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	} else {
		// Don't put the pieces that already exist
		pieces &= ~ti.map5;
	}

	{
		byte t = pieces;
		while (t) {
			if (t & 1) cost += _price.build_road;
			t >>= 1;
		}
	}

	if (flags & DC_EXEC) {
		if (ti.type != MP_STREET) {
			SetTileType(tile, MP_STREET);
			_m[tile].m5 = 0;
			_m[tile].m2 = p2;
			SetTileOwner(tile, _current_player);
		}

		_m[tile].m5 |= (byte)pieces;

		MarkTileDirtyByTile(tile);
	}
	return cost;
}

int32 DoConvertStreetRail(TileIndex tile, uint totype, bool exec)
{
	// not a railroad crossing?
	if (!IsLevelCrossing(tile)) return CMD_ERROR;

	// not owned by me?
	if (!CheckTileOwnership(tile) || !EnsureNoVehicle(tile)) return CMD_ERROR;

	// tile is already of requested type?
	if ((_m[tile].m4 & 0xFU) == totype) return CMD_ERROR;

	if (exec) {
		// change type.
		_m[tile].m4 = (_m[tile].m4 & 0xF0) + totype;
		MarkTileDirtyByTile(tile);
	}

	return _price.build_rail >> 1;
}


/** Build a long piece of road.
 * @param x,y end tile of drag
 * @param p1 start tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 */
int32 CmdBuildLongRoad(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start_tile, end_tile, tile;
	int32 cost, ret;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 > MapSize()) return CMD_ERROR;

	start_tile = p1;
	end_tile = TileVirtXY(x, y);

	/* Only drag in X or Y direction dictated by the direction variable */
	if (!HASBIT(p2, 2) && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (HASBIT(p2, 2)  && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	/* Swap start and ending tile, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HASBIT(p2, 0))) {
		TileIndex t = start_tile;
		start_tile = end_tile;
		end_tile = t;
		p2 ^= IS_INT_INSIDE(p2&3, 1, 3) ? 3 : 0;
	}

	cost = 0;
	tile = start_tile;
	// Start tile is the small number.
	for (;;) {
		uint bits = HASBIT(p2, 2) ? ROAD_SE | ROAD_NW : ROAD_SW | ROAD_NE;
		if (tile == end_tile && !HASBIT(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HASBIT(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		ret = DoCommandByTile(tile, bits, 0, flags, CMD_BUILD_ROAD);
		if (CmdFailed(ret)) {
			if (_error_message != STR_1007_ALREADY_BUILT) return CMD_ERROR;
		} else
			cost += ret;

		if (tile == end_tile) break;

		tile += HASBIT(p2, 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return (cost == 0) ? CMD_ERROR : cost;
}

/** Remove a long piece of road.
 * @param x,y end tile of drag
 * @param p1 start tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 */
int32 CmdRemoveLongRoad(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start_tile, end_tile, tile;
	int32 cost, ret;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 > MapSize()) return CMD_ERROR;

	start_tile = p1;
	end_tile = TileVirtXY(x, y);

	/* Only drag in X or Y direction dictated by the direction variable */
	if (!HASBIT(p2, 2) && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (HASBIT(p2, 2)  && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	/* Swap start and ending tile, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HASBIT(p2, 0))) {
		TileIndex t = start_tile;
		start_tile = end_tile;
		end_tile = t;
		p2 ^= IS_INT_INSIDE(p2&3, 1, 3) ? 3 : 0;
	}

	cost = 0;
	tile = start_tile;
	// Start tile is the small number.
	for (;;) {
		uint bits = HASBIT(p2, 2) ? ROAD_SE | ROAD_NW : ROAD_SW | ROAD_NE;
		if (tile == end_tile && !HASBIT(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HASBIT(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		// try to remove the halves.
		if (bits) {
			ret = DoCommandByTile(tile, bits, 0, flags, CMD_REMOVE_ROAD);
			if (!CmdFailed(ret)) cost += ret;
		}

		if (tile == end_tile) break;

		tile += HASBIT(p2, 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return (cost == 0) ? CMD_ERROR : cost;
}

/** Build a road depot.
 * @param x,y tile coordinates where the depot will be built
 * @param p1 depot direction (0 through 3), where 0 is NW, 1 is NE, etc.
 * @param p2 unused
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
int32 CmdBuildRoadDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti;
	int32 cost;
	Depot *dep;
	TileIndex tile;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 > 3) return CMD_ERROR; // check direction

	FindLandscapeHeight(&ti, x, y);

	tile = ti.tile;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if ((ti.tileh != 0) && (
			!_patches.build_on_slopes ||
			IsSteepTileh(ti.tileh) ||
			!CanBuildDepotByTileh(p1, ti.tileh)
		)
	) {
			return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	cost = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	dep = AllocateDepot();
	if (dep == NULL)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (_current_player == _local_player) _last_built_road_depot_tile = tile;

		dep->xy = tile;
		dep->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		ModifyTile(tile,
			MP_SETTYPE(MP_STREET) |
			MP_MAPOWNER_CURRENT | MP_MAP5,
			(p1 | 0x20) /* map5 */
		);

	}
	return cost + _price.build_road_depot;
}

static int32 RemoveRoadDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoDeleteDepot(tile);
	}

	return _price.remove_road_depot;
}

#define M(x) (1<<(x))

static int32 ClearTile_Road(TileIndex tile, byte flags)
{
	int32 ret;
	byte m5 = _m[tile].m5;

	if ( (m5 & 0xF0) == 0) {
		byte b = m5 & 0xF;

		if (! ((1 << b) & (M(1)|M(2)|M(4)|M(8))) ) {
			if ((!(flags & DC_AI_BUILDING) || !IsTileOwner(tile, OWNER_TOWN)) && flags & DC_AUTO)
				return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);
		}
		return DoCommandByTile(tile, b, 0, flags, CMD_REMOVE_ROAD);
	} else if ( (m5 & 0xE0) == 0) {
		if (flags & DC_AUTO)
			return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);

		ret = DoCommandByTile(tile, (m5&8)?5:10, 0, flags, CMD_REMOVE_ROAD);
		if (ret == CMD_ERROR)
			return CMD_ERROR;

		if (flags & DC_EXEC) {
			DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		}

		return ret;
	} else {
		if (flags & DC_AUTO)
			return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
		return RemoveRoadDepot(tile,flags);
	}
}


typedef struct DrawRoadTileStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
} DrawRoadTileStruct;

typedef struct DrawRoadSeqStruct {
	uint32 image;
	byte subcoord_x;
	byte subcoord_y;
	byte width;
	byte height;
} DrawRoadSeqStruct;

#include "table/road_land.h"


uint GetRoadFoundation(uint tileh, uint bits) {
	int i;
	// normal level sloped building
	if ((~_valid_tileh_slopes_road[1][tileh] & bits) == 0)
		return tileh;

	// inclined sloped building
	if ( ((i=0, tileh == 1) || (i+=2, tileh == 2) || (i+=2, tileh == 4) || (i+=2, tileh == 8)) &&
		((bits == (ROAD_SW | ROAD_NE)) || (i++, bits == (ROAD_NW | ROAD_SE))))
		return i + 15;

	// rail crossing
	if ((bits & 0x10) && _valid_tileh_slopes_road[2][tileh])
		return tileh;

	return 0;
}

const byte _road_sloped_sprites[14] = {
	0,  0,  2,  0,
	0,  1,  0,  0,
	3,  0,  0,  0,
	0,  0
};

static void DrawTile_Road(TileInfo *ti)
{
	uint32 image;
	uint16 m2;

	if ( (ti->map5 & 0xF0) == 0) { // if it is a road the upper 4 bits are 0
		const DrawRoadTileStruct *drts;

		if (ti->tileh != 0) {
			int f = GetRoadFoundation(ti->tileh, ti->map5 & 0xF);
			if (f) DrawFoundation(ti, f);

			// default sloped sprites..
			if (ti->tileh != 0) {
				image = _road_sloped_sprites[ti->tileh - 1] + 0x53F;
			} else  {
				image = _road_tile_sprites_1[ti->map5 & 0xF];
			}
		} else {
			image = _road_tile_sprites_1[ti->map5 & 0xF];
		}

		m2 = GB(_m[ti->tile].m4, 4, 3);

		if (m2 == 0) image |= PALETTE_TO_BARE_LAND;

		if (_m[ti->tile].m4 & 0x80) {
			image += 19;
		} else if (m2 > 1 && m2 != 6) {
			image -= 19; /* pavement along the road? */
		}

		DrawGroundSprite(image);

		if (!(_display_opt & DO_FULL_DETAIL) || _cur_dpi->zoom == 2)
			return;

		if (m2 >= 6) {
			// roadwork
			DrawGroundSprite(0x586 + ((ti->map5&8)!=0 ? 0 : 1));
			return;
		}

		drts = _road_display_table[m2][ti->map5 & 0xF];

		while ((image = drts->image) != 0) {
			int x = ti->x | drts->subcoord_x;
			int y = ti->y | drts->subcoord_y;
			byte z = ti->z;
			if (ti->tileh != 0)	z = GetSlopeZ(x, y);
			AddSortableSpriteToDraw(image, x, y, 2, 2, 0x10, z);
			drts++;
		}
	} else if ( (ti->map5 & 0xE0) == 0) { // railroad crossing
		int f = GetRoadFoundation(ti->tileh, ti->map5 & 0xF);
		if (f) DrawFoundation(ti, f);

		image = 0x55B;

		if ( (ti->map5 & 8) != 0)
			image--;

		if ( (ti->map5 & 4) != 0)
			image += 2;

		if ( _m[ti->tile].m4 & 0x80) {
			image += 8;
		} else {
			m2 = GB(_m[ti->tile].m4, 4, 3);
			if (m2 == 0) image |= PALETTE_TO_BARE_LAND;
			if (m2 > 1) image += 4;
		}

		DrawGroundSprite(image + (_m[ti->tile].m4 & 0xF) * 12);

		if (_debug_pbs_level >= 1) {
			byte pbs = PBSTileReserved(ti->tile);
			if (pbs & TRACK_BIT_DIAG1) DrawGroundSprite(0x3ED | PALETTE_CRASH);
			if (pbs & TRACK_BIT_DIAG2) DrawGroundSprite(0x3EE | PALETTE_CRASH);
			if (pbs & TRACK_BIT_UPPER) DrawGroundSprite(0x3EF | PALETTE_CRASH);
			if (pbs & TRACK_BIT_LOWER) DrawGroundSprite(0x3F0 | PALETTE_CRASH);
			if (pbs & TRACK_BIT_LEFT)  DrawGroundSprite(0x3F2 | PALETTE_CRASH);
			if (pbs & TRACK_BIT_RIGHT) DrawGroundSprite(0x3F1 | PALETTE_CRASH);
		}

	} else {
		uint32 ormod;
		int player;
		const DrawRoadSeqStruct *drss;

		if (ti->tileh != 0) { DrawFoundation(ti, ti->tileh); }

		ormod = PALETTE_TO_GREY;	//was this a bug/problem?
		player = GetTileOwner(ti->tile);
		if (player < MAX_PLAYERS)
			ormod = PLAYER_SPRITE_COLOR(player);

		drss = _road_display_datas[ti->map5 & 0xF];

		DrawGroundSprite(drss++->image);

		for (; drss->image != 0; drss++) {
			uint32 image = drss->image;

			if (image & PALETTE_MODIFIER_COLOR)
				image |= ormod;
			if (_display_opt & DO_TRANS_BUILDINGS) // show transparent depots
				MAKE_TRANSPARENT(image);

			AddSortableSpriteToDraw(image, ti->x | drss->subcoord_x,
				ti->y | drss->subcoord_y, drss->width, drss->height, 0x14, ti->z);
		}
	}
}

void DrawRoadDepotSprite(int x, int y, int image)
{
	uint32 ormod;
	const DrawRoadSeqStruct *dtss;

	ormod = PLAYER_SPRITE_COLOR(_local_player);

	dtss = _road_display_datas[image];

	x+=33;
	y+=17;

	DrawSprite(dtss++->image, x, y);

	for(; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->subcoord_x, dtss->subcoord_y, 0);

		image = dtss->image;
		if (image & PALETTE_MODIFIER_COLOR)
			image |= ormod;

		DrawSprite(image, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Road(TileInfo *ti)
{
	uint z = ti->z;
	int th = ti->tileh;

	// check if it's a foundation
	if (ti->tileh != 0) {
		if ((ti->map5 & 0xE0) == 0) { /* road or crossing */
			uint f = GetRoadFoundation(ti->tileh, ti->map5 & 0x3F);
			if (f != 0) {
				if (f < 15) {
					// leveled foundation
					return z + 8;
				}
				// inclined foundation
				th = _inclined_tileh[f - 15];
			}
		} else if ((ti->map5 & 0xF0) == 0x20) {
			// depot
			return z + 8;
		}
		return GetPartialZ(ti->x&0xF, ti->y&0xF, th) + z;
	}
	return z; // normal Z if no slope
}

static uint GetSlopeTileh_Road(TileInfo *ti)
{
	// check if it's a foundation
	if (ti->tileh != 0) {
		if ((ti->map5 & 0xE0) == 0) { /* road or crossing */
			uint f = GetRoadFoundation(ti->tileh, ti->map5 & 0x3F);
			if (f != 0) {
				if (f < 15) {
					// leveled foundation
					return 0;
				}
				// inclined foundation
				return _inclined_tileh[f - 15];
			}
		} else if ((ti->map5 & 0xF0) == 0x20) {
			// depot
			return 0;
		}
	}
	return ti->tileh;
}

static void GetAcceptedCargo_Road(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void AnimateTile_Road(TileIndex tile)
{
	if (IsLevelCrossing(tile)) {
		MarkTileDirtyByTile(tile);
	}
}

static const byte _town_road_types[5][2] = {
	{1,1},
	{2,2},
	{2,2},
	{5,5},
	{3,2},
};

static const byte _town_road_types_2[5][2] = {
	{1,1},
	{2,2},
	{3,2},
	{3,2},
	{3,2},
};


static void TileLoop_Road(TileIndex tile)
{
	Town *t;
	int grp;

	if (_opt.landscape == LT_HILLY) {
		// Fix snow style if the road is above the snowline
		if ((_m[tile].m4 & 0x80) != ((GetTileZ(tile) > _opt.snow_line) ? 0x80 : 0x00)) {
			_m[tile].m4 ^= 0x80;
			MarkTileDirtyByTile(tile);
		}
	} else if (_opt.landscape == LT_DESERT) {
		// Fix desert style
		if (GetMapExtraBits(tile) == 1 && !(_m[tile].m4 & 0x80)) {
			_m[tile].m4 |= 0x80;
			MarkTileDirtyByTile(tile);
		}
	}

	if (_m[tile].m5 & 0xE0)
		return;

	if (GB(_m[tile].m4, 4, 3) < 6) {
		t = ClosestTownFromTile(tile, (uint)-1);

		grp = 0;
		if (t != NULL) {
			grp = GetTownRadiusGroup(t, tile);

			// Show an animation to indicate road work
			if (t->road_build_months != 0 &&
					!(DistanceManhattan(t->xy, tile) >= 8 && grp == 0) &&
					(_m[tile].m5==5 || _m[tile].m5==10)) {
				if (GetTileSlope(tile, NULL) == 0 && EnsureNoVehicle(tile) && CHANCE16(1,20)) {
					_m[tile].m4 |= (GB(_m[tile].m4, 4, 3) <=  2 ? 7 : 6) << 4;

					SndPlayTileFx(SND_21_JACKHAMMER, tile);
					CreateEffectVehicleAbove(
						TileX(tile) * 16 + 7,
						TileY(tile) * 16 + 7,
						0,
						EV_BULLDOZER);
					MarkTileDirtyByTile(tile);
					return;
				}
			}
		}

		{
			const byte *p = (_opt.landscape == LT_CANDY) ? _town_road_types_2[grp] : _town_road_types[grp];
			byte b = GB(_m[tile].m4, 4, 3);

			if (b == p[0])
				return;

			if (b == p[1]) {
				b = p[0];
			} else if (b == 0) {
				b = p[1];
			} else {
				b = 0;
			}
			_m[tile].m4 = (_m[tile].m4 & ~0x70) | (b << 4);
			MarkTileDirtyByTile(tile);
		}
	} else {
		// Handle road work
		//XXX undocumented

		byte b = _m[tile].m4;
		//roadworks take place only
		//keep roadworks running for 16 loops
		//lower 4 bits of map3_hi store the counter now
		if ((b & 0xF) != 0xF) {
			_m[tile].m4 = b + 1;
			return;
		}
		//roadworks finished
		_m[tile].m4 = (GB(b, 4, 3) == 6 ? 1 : 2) << 4;
		MarkTileDirtyByTile(tile);
	}
}

void ShowRoadDepotWindow(TileIndex tile);

static void ClickTile_Road(TileIndex tile)
{
	if ((_m[tile].m5 & 0xF0) == 0x20) {
		ShowRoadDepotWindow(tile);
	}
}

static const byte _road_trackbits[16] = {
	0x0, 0x0, 0x0, 0x10, 0x0, 0x2, 0x8, 0x1A, 0x0, 0x4, 0x1, 0x15, 0x20, 0x26, 0x29, 0x3F,
};

static uint32 GetTileTrackStatus_Road(TileIndex tile, TransportType mode)
{
	if (mode == TRANSPORT_RAIL) {
		if (!IsLevelCrossing(tile))
			return 0;
		return _m[tile].m5 & 8 ? 0x101 : 0x202;
	} else if  (mode == TRANSPORT_ROAD) {
		byte b = _m[tile].m5;
		if ((b & 0xF0) == 0) {
			/* Ordinary road */
			if (!_road_special_gettrackstatus && GB(_m[tile].m4, 4, 3) >= 6)
				return 0;
			return _road_trackbits[b&0xF] * 0x101;
		} else if (IsLevelCrossing(tile)) {
			/* Crossing */
			uint32 r = 0x101;
			if (b&8) r <<= 1;

			if (b&4) {
				r *= 0x10001;
			}
			return r;
		}
	}
	return 0;
}

static const StringID _road_tile_strings[] = {
	STR_1818_ROAD_RAIL_LEVEL_CROSSING,
	STR_1817_ROAD_VEHICLE_DEPOT,

	STR_1814_ROAD,
	STR_1814_ROAD,
	STR_1814_ROAD,
	STR_1815_ROAD_WITH_STREETLIGHTS,
	STR_1814_ROAD,
	STR_1816_TREE_LINED_ROAD,
	STR_1814_ROAD,
	STR_1814_ROAD,
};

static void GetTileDesc_Road(TileIndex tile, TileDesc *td)
{
	int i = (_m[tile].m5 >> 4);
	if (i == 0)
		i = GB(_m[tile].m4, 4, 3) + 3;
	td->str = _road_tile_strings[i - 1];
	td->owner = GetTileOwner(tile);
}

static const byte _roadveh_enter_depot_unk0[4] = {
	8, 9, 0, 1
};

static uint32 VehicleEnter_Road(Vehicle *v, TileIndex tile, int x, int y)
{
	if (IsLevelCrossing(tile)) {
		if (v->type == VEH_Train && (_m[tile].m5 & 4) == 0) {
			/* train crossing a road */
			SndPlayVehicleFx(SND_0E_LEVEL_CROSSING, v);
			_m[tile].m5 |= 4;
			MarkTileDirtyByTile(tile);
		}
	} else if ((_m[tile].m5&0xF0) ==  0x20){
		if (v->type == VEH_Road && v->u.road.frame == 11) {
			if (_roadveh_enter_depot_unk0[_m[tile].m5&3] == v->u.road.state) {
				RoadVehEnterDepot(v);
				return 4;
			}
		}
	}
	return 0;
}

static void VehicleLeave_Road(Vehicle *v, TileIndex tile, int x, int y)
{
	if (IsLevelCrossing(tile) && v->type == VEH_Train && v->next == NULL) {
		// Turn off level crossing lights
		_m[tile].m5 &= ~4;
		MarkTileDirtyByTile(tile);
	}
}

static void ChangeTileOwner_Road(TileIndex tile, byte old_player, byte new_player)
{
	byte b;

	// road/rail crossing where the road is owned by the current player?
	if (old_player == _m[tile].m3 && IsLevelCrossing(tile)) {
		_m[tile].m3 = (new_player == 0xFF) ? OWNER_NONE : new_player;
	}

	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != 255) {
		SetTileOwner(tile, new_player);
	}	else {
		b = _m[tile].m5&0xF0;
		if (b == 0) {
			SetTileOwner(tile, OWNER_NONE);
		} else if (IsLevelCrossing(tile)) {
			_m[tile].m5 = (_m[tile].m5&8) ? 0x5 : 0xA;
			SetTileOwner(tile, _m[tile].m3);
			_m[tile].m3 = 0;
			_m[tile].m4 &= 0x80;
		} else {
			DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}
}

void InitializeRoad(void)
{
	_last_built_road_depot_tile = 0;
}

const TileTypeProcs _tile_type_road_procs = {
	DrawTile_Road,						/* draw_tile_proc */
	GetSlopeZ_Road,						/* get_slope_z_proc */
	ClearTile_Road,						/* clear_tile_proc */
	GetAcceptedCargo_Road,		/* get_accepted_cargo_proc */
	GetTileDesc_Road,					/* get_tile_desc_proc */
	GetTileTrackStatus_Road,	/* get_tile_track_status_proc */
	ClickTile_Road,						/* click_tile_proc */
	AnimateTile_Road,					/* animate_tile_proc */
	TileLoop_Road,						/* tile_loop_clear */
	ChangeTileOwner_Road,			/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	VehicleEnter_Road,				/* vehicle_enter_tile_proc */
	VehicleLeave_Road,				/* vehicle_leave_tile_proc */
	GetSlopeTileh_Road,				/* get_slope_tileh_proc */
};
