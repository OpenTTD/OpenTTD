/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "rail_map.h"
#include "road_map.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "tile.h"
#include "town_map.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "player.h"
#include "town.h"
#include "gfx.h"
#include "sound.h"
#include "depot.h"

void RoadVehEnterDepot(Vehicle *v);


static bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, bool* edge_road)
{
	RoadBits present;
	RoadBits n;
	byte owner;
	*edge_road = true;

	if (_game_mode == GM_EDITOR) return true;

	// Only do the special processing for actual players.
	if (_current_player >= MAX_PLAYERS) return true;

	if (IsTileType(tile, MP_STREET) && IsLevelCrossing(tile)) {
		owner = GetCrossingRoadOwner(tile);
	} else {
		owner = GetTileOwner(tile);
	}
	// Only do the special processing if the road is owned
	// by a town
	if (owner != OWNER_TOWN) {
		return owner == OWNER_NONE || CheckOwnership(owner);
	}

	if (_cheats.magic_bulldozer.value) return true;

	// Get a bitmask of which neighbouring roads has a tile
	n = 0;
	present = GetAnyRoadBits(tile);
	if (present & ROAD_NE && GetAnyRoadBits(TILE_ADDXY(tile,-1, 0)) & ROAD_SW) n |= ROAD_NE;
	if (present & ROAD_SE && GetAnyRoadBits(TILE_ADDXY(tile, 0, 1)) & ROAD_NW) n |= ROAD_SE;
	if (present & ROAD_SW && GetAnyRoadBits(TILE_ADDXY(tile, 1, 0)) & ROAD_NE) n |= ROAD_SW;
	if (present & ROAD_NW && GetAnyRoadBits(TILE_ADDXY(tile, 0,-1)) & ROAD_SE) n |= ROAD_NW;

	// If 0 or 1 bits are set in n, or if no bits that match the bits to remove,
	// then allow it
	if ((n & (n - 1)) != 0 && (n & remove) != 0) {
		Town *t;
		*edge_road = false;
		// you can remove all kind of roads with extra dynamite
		if (_patches.extra_dynamite) return true;

		t = ClosestTownFromTile(tile, _patches.dist_local_authority);

		SetDParam(0, t->index);
		_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
		return false;
	}

	return true;
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

	int32 cost;
	TileIndex tile;
	PlayerID owner;
	Town *t;
	/* true if the roadpiece was always removeable,
	 * false if it was a center piece. Affects town ratings drop */
	bool edge_road;
	RoadBits pieces;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) */
	if (p1 >> 4) return CMD_ERROR;
	pieces = p1;

	tile = TileVirtXY(x, y);

	if (!IsTileType(tile, MP_STREET) && !IsTileType(tile, MP_TUNNELBRIDGE)) return CMD_ERROR;

	owner = IsLevelCrossing(tile) ? GetCrossingRoadOwner(tile) : GetTileOwner(tile);

	if (owner == OWNER_TOWN && _game_mode != GM_EDITOR) {
		if (IsTileType(tile, MP_TUNNELBRIDGE)) { // index of town is not saved for bridge (no space)
			t = ClosestTownFromTile(tile, _patches.dist_local_authority);
		} else {
			t = GetTownByTile(tile);
		}
	} else {
		t = NULL;
	}

	if (!CheckAllowRemoveRoad(tile, pieces, &edge_road)) return CMD_ERROR;

	switch (GetTileType(tile)) {
		case MP_TUNNELBRIDGE:
			if (!EnsureNoVehicleZ(tile, TilePixelHeight(tile))) return CMD_ERROR;

			if (!IsBridge(tile) ||
					!IsBridgeMiddle(tile) ||
					!IsTransportUnderBridge(tile) ||
					GetTransportTypeUnderBridge(tile) != TRANSPORT_ROAD ||
					(pieces & ComplementRoadBits(GetRoadBitsUnderBridge(tile))) != 0) {
				return CMD_ERROR;
			}

			cost = _price.remove_road * 2;

			if (flags & DC_EXEC) {
				ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);
				SetClearUnderBridge(tile);
				MarkTileDirtyByTile(tile);
			}
			return cost;

		case MP_STREET:
			if (!EnsureNoVehicle(tile)) return CMD_ERROR;

			// check if you're allowed to remove the street owned by a town
			// removal allowance depends on difficulty setting
			if (!CheckforTownRating(flags, t, ROAD_REMOVE)) return CMD_ERROR;

			switch (GetRoadType(tile)) {
				case ROAD_NORMAL: {
					RoadBits present = GetRoadBits(tile);
					RoadBits c = pieces;

					if (GetTileSlope(tile, NULL) != 0  &&
							(present == ROAD_Y || present == ROAD_X)) {
						c |= (c & 0xC) >> 2;
						c |= (c & 0x3) << 2;
					}

					// limit the bits to delete to the existing bits.
					c &= present;
					if (c == 0) return CMD_ERROR;

					// calculate the cost
					cost = 0;
					if (c & ROAD_NW) cost += _price.remove_road;
					if (c & ROAD_SW) cost += _price.remove_road;
					if (c & ROAD_SE) cost += _price.remove_road;
					if (c & ROAD_NE) cost += _price.remove_road;

					if (flags & DC_EXEC) {
						ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);

						present ^= c;
						if (present == 0) {
							DoClearSquare(tile);
						} else {
							SetRoadBits(tile, present);
							MarkTileDirtyByTile(tile);
						}
					}
					return cost;
				}

				case ROAD_CROSSING: {
					if (pieces & ComplementRoadBits(GetCrossingRoadBits(tile))) {
						return CMD_ERROR;
					}

					cost = _price.remove_road * 2;
					if (flags & DC_EXEC) {
						ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);

						MakeRailNormal(tile, GetTileOwner(tile), GetCrossingRailBits(tile), GetRailTypeCrossing(tile));
						MarkTileDirtyByTile(tile);
					}
					return cost;
				}

				default:
				case ROAD_DEPOT:
					return CMD_ERROR;
			}

		default: return CMD_ERROR;
	}
}


static const RoadBits _valid_tileh_slopes_road[][15] = {
	// set of normal ones
	{
		ROAD_ALL, 0, 0,
		ROAD_X,   0, 0,  // 3, 4, 5
		ROAD_Y,   0, 0,
		ROAD_Y,   0, 0,  // 9, 10, 11
		ROAD_X,   0, 0
	},
	// allowed road for an evenly raised platform
	{
		0,
		ROAD_SW | ROAD_NW,
		ROAD_SW | ROAD_SE,
		ROAD_Y  | ROAD_SW,

		ROAD_SE | ROAD_NE, // 4
		ROAD_ALL,
		ROAD_X  | ROAD_SE,
		ROAD_ALL,

		ROAD_NW | ROAD_NE, // 8
		ROAD_X  | ROAD_NW,
		ROAD_ALL,
		ROAD_ALL,

		ROAD_Y  | ROAD_NE, // 12
		ROAD_ALL,
		ROAD_ALL
	},
};


static uint32 CheckRoadSlope(int tileh, RoadBits* pieces, RoadBits existing)
{
	if (!IsSteepTileh(tileh)) {
		RoadBits road_bits = *pieces | existing;

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
		if (existing == 0 && (tileh == 1 || tileh == 2 || tileh == 4 || tileh == 8)) {
			// force full pieces.
			*pieces |= (*pieces & 0xC) >> 2;
			*pieces |= (*pieces & 0x3) << 2;
			return (*pieces == ROAD_X || *pieces == ROAD_Y) ? _price.terraform : CMD_ERROR;
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
	RoadBits existing = 0;
	RoadBits pieces;
	TileIndex tile;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-player is building the road */
	if ((p1 >> 4) || (_current_player < MAX_PLAYERS && p2 != 0) || !IsTownIndex(p2)) return CMD_ERROR;
	pieces = p1;

	FindLandscapeHeight(&ti, x, y);
	tile = ti.tile;

	// allow building road under bridge
	if (ti.type != MP_TUNNELBRIDGE && !EnsureNoVehicle(tile)) return CMD_ERROR;

	switch (ti.type) {
		case MP_STREET:
			switch (GetRoadType(tile)) {
				case ROAD_NORMAL:
					existing = GetRoadBits(tile);
					if ((existing & pieces) == pieces) {
						return_cmd_error(STR_1007_ALREADY_BUILT);
					}
					break;

				case ROAD_CROSSING:
					if (pieces != GetCrossingRoadBits(tile)) { // XXX is this correct?
						return_cmd_error(STR_1007_ALREADY_BUILT);
					}
					goto do_clear;

				default:
				case ROAD_DEPOT:
					goto do_clear;
			}
			break;

		case MP_RAILWAY: {
			Axis roaddir;

			if (IsSteepTileh(ti.tileh)) { // very steep tile
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}

#define M(x) (1 << (x))
			/* Level crossings may only be built on these slopes */
			if (!HASBIT(M(14) | M(13) | M(11) | M(10) | M(7) | M(5) | M(0), ti.tileh)) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}
#undef M

			if (GetRailTileType(tile) != RAIL_TYPE_NORMAL) goto do_clear;
			switch (GetTrackBits(tile)) {
				case TRACK_BIT_X:
					if (pieces & ROAD_X) goto do_clear;
					roaddir = AXIS_Y;
					break;

				case TRACK_BIT_Y:
					if (pieces & ROAD_Y) goto do_clear;
					roaddir = AXIS_X;
					break;

				default: goto do_clear;
			}

			if (flags & DC_EXEC) {
				MakeRoadCrossing(tile, _current_player, GetTileOwner(tile), roaddir, GetRailType(tile), p2);
				MarkTileDirtyByTile(tile);
			}
			return _price.build_road * 2;
		}

		case MP_TUNNELBRIDGE:
			/* check for flat land */
			if (IsSteepTileh(ti.tileh)) { // very steep tile
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}

			if (!IsBridge(tile) || !IsBridgeMiddle(tile)) goto do_clear;

			/* only allow roads pertendicular to bridge */
			if ((pieces & (GetBridgeAxis(tile) == AXIS_X ? ROAD_X : ROAD_Y)) != 0) {
				goto do_clear;
			}

			/* check if clear land under bridge */
			if (IsTransportUnderBridge(tile)) {
				switch (GetTransportTypeUnderBridge(tile)) {
					case TRANSPORT_ROAD: return_cmd_error(STR_1007_ALREADY_BUILT);
					default: return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);
				}
			} else {
				if (IsWaterUnderBridge(tile)) {
					return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);
				}
			}

			/* all checked, can build road now! */
			cost = _price.build_road * 2;
			if (flags & DC_EXEC) {
				SetRoadUnderBridge(tile, _current_player);
				MarkTileDirtyByTile(tile);
			}
			return cost;

		default:
do_clear:;
			if (CmdFailed(DoCommandByTile(tile, 0, 0, flags & ~DC_EXEC, CMD_LANDSCAPE_CLEAR)))
				return CMD_ERROR;
	}

	cost = CheckRoadSlope(ti.tileh, &pieces, existing);
	if (CmdFailed(cost)) return_cmd_error(STR_1800_LAND_SLOPED_IN_WRONG_DIRECTION);

	if (cost && (!_patches.build_on_slopes || _is_old_ai_player))
		return CMD_ERROR;

	if (ti.type == MP_STREET && GetRoadType(tile) == ROAD_NORMAL) {
		// Don't put the pieces that already exist
		pieces &= ComplementRoadBits(existing);
	} else {
		cost += DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	}

	if (pieces & ROAD_NW) cost += _price.build_road;
	if (pieces & ROAD_SW) cost += _price.build_road;
	if (pieces & ROAD_SE) cost += _price.build_road;
	if (pieces & ROAD_NE) cost += _price.build_road;

	if (flags & DC_EXEC) {
		if (ti.type == MP_STREET) {
			SetRoadBits(tile, existing | pieces);
		} else {
			MakeRoadNormal(tile, _current_player, pieces, p2);
		}

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

	if (GetRailTypeCrossing(tile) == totype) return CMD_ERROR;

	if (exec) {
		SetRailTypeCrossing(tile, totype);
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

	if (p1 >= MapSize()) return CMD_ERROR;

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
		RoadBits bits = HASBIT(p2, 2) ? ROAD_Y : ROAD_X;

		if (tile == end_tile && !HASBIT(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HASBIT(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		ret = DoCommandByTile(tile, bits, 0, flags, CMD_BUILD_ROAD);
		if (CmdFailed(ret)) {
			if (_error_message != STR_1007_ALREADY_BUILT) return CMD_ERROR;
		} else {
			cost += ret;
		}

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

	if (p1 >= MapSize()) return CMD_ERROR;

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
		p2 ^= IS_INT_INSIDE(p2 & 3, 1, 3) ? 3 : 0;
	}

	cost = 0;
	tile = start_tile;
	// Start tile is the small number.
	for (;;) {
		RoadBits bits = HASBIT(p2, 2) ? ROAD_Y : ROAD_X;

		if (tile == end_tile && !HASBIT(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HASBIT(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		// try to remove the halves.
		if (bits != 0) {
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
	int32 cost;
	Depot *dep;
	TileIndex tile;
	uint tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 > 3) return CMD_ERROR; // check direction

	tile = TileVirtXY(x, y);

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);
	if (tileh != 0 && (
				!_patches.build_on_slopes ||
				IsSteepTileh(tileh) ||
				!CanBuildDepotByTileh(p1, tileh)
			)) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	cost = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	dep = AllocateDepot();
	if (dep == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (IsLocalPlayer()) _last_built_road_depot_tile = tile;

		dep->xy = tile;
		dep->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		MakeRoadDepot(tile, _current_player, p1);
		MarkTileDirtyByTile(tile);
	}
	return cost + _price.build_road_depot;
}

static int32 RemoveRoadDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) DoDeleteDepot(tile);

	return _price.remove_road_depot;
}

#define M(x) (1<<(x))

static int32 ClearTile_Road(TileIndex tile, byte flags)
{
	switch (GetRoadType(tile)) {
		case ROAD_NORMAL: {
			RoadBits b = GetRoadBits(tile);

			if (!((1 << b) & (M(1)|M(2)|M(4)|M(8))) &&
					(!(flags & DC_AI_BUILDING) || !IsTileOwner(tile, OWNER_TOWN)) &&
					flags & DC_AUTO) {
				return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);
			}
			return DoCommandByTile(tile, b, 0, flags, CMD_REMOVE_ROAD);
		}

		case ROAD_CROSSING: {
			int32 ret;

			if (flags & DC_AUTO) return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);

			ret = DoCommandByTile(tile, GetCrossingRoadBits(tile), 0, flags, CMD_REMOVE_ROAD);
			if (CmdFailed(ret)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
			return ret;
		}

		default:
		case ROAD_DEPOT:
			if (flags & DC_AUTO) {
				return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
			}
			return RemoveRoadDepot(tile, flags);
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


uint GetRoadFoundation(uint tileh, RoadBits bits)
{
	int i;
	// normal level sloped building
	if ((~_valid_tileh_slopes_road[1][tileh] & bits) == 0) return tileh;

	// inclined sloped building
	if ((
				(i  = 0, tileh == 1) ||
				(i += 2, tileh == 2) ||
				(i += 2, tileh == 4) ||
				(i += 2, tileh == 8)
			) && (
				(     bits == ROAD_X) ||
				(i++, bits == ROAD_Y)
			)) {
		return i + 15;
	}

	return 0;
}

const byte _road_sloped_sprites[14] = {
	0,  0,  2,  0,
	0,  1,  0,  0,
	3,  0,  0,  0,
	0,  0
};

/**
 * Draw ground sprite and road pieces
 * @param ti TileInfo
 * @param road RoadBits to draw
 * @param ground_type Ground type
 * @param snow Draw snow
 * @param flat Draw foundation
 */
static void DrawRoadBits(TileInfo* ti, RoadBits road, byte ground_type, bool snow, bool flat)
{
	const DrawRoadTileStruct *drts;
	PalSpriteID image = 0;

	if (ti->tileh != 0) {
		int foundation;
		if (flat) {
			foundation = ti->tileh;
		} else {
			foundation = GetRoadFoundation(ti->tileh, road);
		}

		if (foundation != 0) DrawFoundation(ti, foundation);

		// DrawFoundation() modifies ti.
		// Default sloped sprites..
		if (ti->tileh != 0) image = _road_sloped_sprites[ti->tileh - 1] + 0x53F;
	}

	if (image == 0) image = _road_tile_sprites_1[road];

	if (ground_type == 0) image |= PALETTE_TO_BARE_LAND;

	if (snow) {
		image += 19;
	} else if (ground_type > 1 && ground_type != 6) {
		// Pavement tiles.
		image -= 19;
	}

	DrawGroundSprite(image);

	// Return if full detail is disabled, or we are zoomed fully out.
	if (!(_display_opt & DO_FULL_DETAIL) || _cur_dpi->zoom == 2) return;

	if (ground_type >= 6) {
		// Road works
		DrawGroundSprite(road & ROAD_X ? SPR_EXCAVATION_X : SPR_EXCAVATION_Y);
		return;
	}

	// Draw extra details.
	for (drts = _road_display_table[ground_type][road]; drts->image != 0; drts++) {
		int x = ti->x | drts->subcoord_x;
		int y = ti->y | drts->subcoord_y;
		byte z = ti->z;
		if (ti->tileh != 0) z = GetSlopeZ(x, y);
		AddSortableSpriteToDraw(drts->image, x, y, 2, 2, 0x10, z);
	}
}

static void DrawTile_Road(TileInfo *ti)
{
	PalSpriteID image;
	uint16 m2;

	switch (GetRoadType(ti->tile)) {
		case ROAD_NORMAL:
			DrawRoadBits(ti, GetRoadBits(ti->tile), GB(_m[ti->tile].m4, 4, 3), HASBIT(_m[ti->tile].m4, 7), false);
			break;

		case ROAD_CROSSING: {
			if (ti->tileh != 0) DrawFoundation(ti, ti->tileh);

			image = GetRailTypeInfo(GetRailTypeCrossing(ti->tile))->base_sprites.crossing;

			if (GetCrossingRoadAxis(ti->tile) == AXIS_X) image++;

			if ((ti->map5 & 4) != 0) image += 2;

			if ( _m[ti->tile].m4 & 0x80) {
				image += 8;
			} else {
				m2 = GB(_m[ti->tile].m4, 4, 3);
				if (m2 == 0) image |= PALETTE_TO_BARE_LAND;
				if (m2 > 1) image += 4;
			}

			DrawGroundSprite(image);
			break;
		}

		default:
		case ROAD_DEPOT: {
			uint32 ormod;
			PlayerID player;
			const DrawRoadSeqStruct* drss;

			if (ti->tileh != 0) DrawFoundation(ti, ti->tileh);

			ormod = PALETTE_TO_GREY;	//was this a bug/problem?
			player = GetTileOwner(ti->tile);
			if (player < MAX_PLAYERS) ormod = PLAYER_SPRITE_COLOR(player);

			drss = _road_display_datas[GetRoadDepotDirection(ti->tile)];

			DrawGroundSprite(drss++->image);

			for (; drss->image != 0; drss++) {
				uint32 image = drss->image;

				if (image & PALETTE_MODIFIER_COLOR) image |= ormod;
				if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);

				AddSortableSpriteToDraw(image, ti->x | drss->subcoord_x,
					ti->y | drss->subcoord_y, drss->width, drss->height, 0x14, ti->z
				);
			}
			break;
		}
	}
}

void DrawRoadDepotSprite(int x, int y, int image)
{
	uint32 ormod;
	const DrawRoadSeqStruct *dtss;

	ormod = PLAYER_SPRITE_COLOR(_local_player);

	dtss = _road_display_datas[image];

	x += 33;
	y += 17;

	DrawSprite(dtss++->image, x, y);

	for (; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->subcoord_x, dtss->subcoord_y, 0);

		image = dtss->image;
		if (image & PALETTE_MODIFIER_COLOR) image |= ormod;

		DrawSprite(image, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Road(const TileInfo* ti)
{
	uint tileh = ti->tileh;
	uint z = ti->z;

	if (tileh == 0) return z;
	if (GetRoadType(ti->tile) == ROAD_NORMAL) {
		uint f = GetRoadFoundation(tileh, GetRoadBits(ti->tile));

		if (f != 0) {
			if (f < 15) return z + 8; // leveled foundation
			tileh = _inclined_tileh[f - 15]; // inclined foundation
		}
		return z + GetPartialZ(ti->x & 0xF, ti->y & 0xF, tileh);
	} else {
		return z + 8;
	}
}

static uint GetSlopeTileh_Road(const TileInfo *ti)
{
	if (ti->tileh == 0) return ti->tileh;
	if (GetRoadType(ti->tile) == ROAD_NORMAL) {
		uint f = GetRoadFoundation(ti->tileh, GetRoadBits(ti->tile));

		if (f == 0) return ti->tileh;
		if (f < 15) return 0; // leveled foundation
		return _inclined_tileh[f - 15]; // inclined foundation
	} else {
		return 0;
	}
}

static void GetAcceptedCargo_Road(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void AnimateTile_Road(TileIndex tile)
{
	if (IsLevelCrossing(tile)) MarkTileDirtyByTile(tile);
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

	switch (_opt.landscape) {
		case LT_HILLY:
			if ((_m[tile].m4 & 0x80) != (GetTileZ(tile) > _opt.snow_line ? 0x80 : 0x00)) {
				_m[tile].m4 ^= 0x80;
				MarkTileDirtyByTile(tile);
			}
			break;

		case LT_DESERT:
			if (GetMapExtraBits(tile) == 1 && !(_m[tile].m4 & 0x80)) {
				_m[tile].m4 |= 0x80;
				MarkTileDirtyByTile(tile);
			}
			break;
	}

	if (GetRoadType(tile) == ROAD_DEPOT) return;

	if (GB(_m[tile].m4, 4, 3) < 6) {
		t = ClosestTownFromTile(tile, (uint)-1);

		grp = 0;
		if (t != NULL) {
			grp = GetTownRadiusGroup(t, tile);

			// Show an animation to indicate road work
			if (t->road_build_months != 0 &&
					!(DistanceManhattan(t->xy, tile) >= 8 && grp == 0) &&
					(_m[tile].m5 == ROAD_Y || _m[tile].m5 == ROAD_X)) {
				if (GetTileSlope(tile, NULL) == 0 && EnsureNoVehicle(tile) && CHANCE16(1, 20)) {
					SB(_m[tile].m4, 4, 3, (GB(_m[tile].m4, 4, 3) <= 1 ? 6 : 7));

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

			if (b == p[0]) return;

			if (b == p[1]) {
				b = p[0];
			} else if (b == 0) {
				b = p[1];
			} else {
				b = 0;
			}
			SB(_m[tile].m4, 4, 3, b);
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
	if (GetRoadType(tile) == ROAD_DEPOT) ShowRoadDepotWindow(tile);
}

static const byte _road_trackbits[16] = {
	0x0, 0x0, 0x0, 0x10, 0x0, 0x2, 0x8, 0x1A, 0x0, 0x4, 0x1, 0x15, 0x20, 0x26, 0x29, 0x3F,
};

static uint32 GetTileTrackStatus_Road(TileIndex tile, TransportType mode)
{
	switch (mode) {
		case TRANSPORT_RAIL:
			if (!IsLevelCrossing(tile)) return 0;
			return GetCrossingRailBits(tile) * 0x101;

		case TRANSPORT_ROAD:
			switch (GetRoadType(tile)) {
				case ROAD_NORMAL:
					return GB(_m[tile].m4, 4, 3) >= 6 ?
						0 : _road_trackbits[GetRoadBits(tile)] * 0x101;

				case ROAD_CROSSING: {
					uint32 r = (GetCrossingRoadAxis(tile) == AXIS_X ? TRACK_BIT_X : TRACK_BIT_Y) * 0x101;

					if (_m[tile].m5 & 4) r *= 0x10001;
					return r;
				}

				default:
				case ROAD_DEPOT:
					break;
			}
			break;

		default: break;
	}
	return 0;
}

static const StringID _road_tile_strings[] = {
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
	td->owner = GetTileOwner(tile);
	switch (GetRoadType(tile)) {
		case ROAD_CROSSING: td->str = STR_1818_ROAD_RAIL_LEVEL_CROSSING; break;
		case ROAD_DEPOT: td->str = STR_1817_ROAD_VEHICLE_DEPOT; break;
		default: td->str = _road_tile_strings[GB(_m[tile].m4, 4, 3)]; break;
	}
}

static const byte _roadveh_enter_depot_unk0[4] = {
	8, 9, 0, 1
};

static uint32 VehicleEnter_Road(Vehicle *v, TileIndex tile, int x, int y)
{
	switch (GetRoadType(tile)) {
		case ROAD_CROSSING:
			if (v->type == VEH_Train && GB(_m[tile].m5, 2, 1) == 0) {
				/* train crossing a road */
				SndPlayVehicleFx(SND_0E_LEVEL_CROSSING, v);
				SB(_m[tile].m5, 2, 1, 1);
				MarkTileDirtyByTile(tile);
			}
			break;

		case ROAD_DEPOT:
			if (v->type == VEH_Road && v->u.road.frame == 11) {
				if (_roadveh_enter_depot_unk0[GetRoadDepotDirection(tile)] == v->u.road.state) {
					RoadVehEnterDepot(v);
					return 4;
				}
			}
			break;

		default: break;
	}
	return 0;
}

static void VehicleLeave_Road(Vehicle *v, TileIndex tile, int x, int y)
{
	if (IsLevelCrossing(tile) && v->type == VEH_Train && v->next == NULL) {
		// Turn off level crossing lights
		SB(_m[tile].m5, 2, 1, 0);
		MarkTileDirtyByTile(tile);
	}
}

static void ChangeTileOwner_Road(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (IsLevelCrossing(tile) && GetCrossingRoadOwner(tile) == old_player) {
		SetCrossingRoadOwner(tile, new_player == OWNER_SPECTATOR ? OWNER_NONE : new_player);
	}

	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != OWNER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	}	else {
		switch (GetRoadType(tile)) {
			case ROAD_NORMAL:
				SetTileOwner(tile, OWNER_NONE);
				break;

			case ROAD_CROSSING:
				MakeRoadNormal(tile, GetCrossingRoadOwner(tile), GetCrossingRoadBits(tile), GetTownIndex(tile));
				break;

			default:
			case ROAD_DEPOT:
				DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				break;
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
