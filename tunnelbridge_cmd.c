/* $Id$ */

/** @file tunnelbridge_cmd.c
 * This file deals with tunnels and bridges (non-gui stuff)
 * @todo seperate this file into two
 */

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
#include "tunnel_map.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "player.h"
#include "town.h"
#include "sound.h"
#include "variables.h"
#include "bridge.h"
#include "train.h"
#include "water_map.h"
#include "yapf/yapf.h"
#include "date.h"
#include "newgrf_sound.h"

#include "table/bridge_land.h"

extern const byte _track_sloped_sprites[14];
extern const SpriteID _water_shore_sprites[15];

extern void DrawCanalWater(TileIndex tile);

const Bridge orig_bridge[] = {
/*
	     year of availablity
	     |  minimum length
	     |  |   maximum length
	     |  |   |    price
	     |  |   |    |    maximum speed
	     |  |   |    |    |  sprite to use in GUI                string with description
	     |  |   |    |    |  |                                   |                            */
	{    0, 0, 16,  80,  32, 0xA24                             , STR_5012_WOODEN             , NULL, 0 },
	{    0, 0,  2, 112,  48, 0xA26 | PALETTE_TO_STRUCT_RED     , STR_5013_CONCRETE           , NULL, 0 },
	{ 1930, 0,  5, 144,  64, 0xA25                             , STR_500F_GIRDER_STEEL       , NULL, 0 },
	{    0, 2, 10, 168,  80, 0xA22 | PALETTE_TO_STRUCT_CONCRETE, STR_5011_SUSPENSION_CONCRETE, NULL, 0 },
	{ 1930, 3, 16, 185,  96, 0xA22                             , STR_500E_SUSPENSION_STEEL   , NULL, 0 },
	{ 1930, 3, 16, 192, 112, 0xA22 | PALETTE_TO_STRUCT_YELLOW  , STR_500E_SUSPENSION_STEEL   , NULL, 0 },
	{ 1930, 3,  7, 224, 160, 0xA23                             , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 1930, 3,  8, 232, 208, 0xA23 | PALETTE_TO_STRUCT_BROWN   , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 1930, 3,  9, 248, 240, 0xA23 | PALETTE_TO_STRUCT_RED     , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 1930, 0,  2, 240, 256, 0xA27                             , STR_500F_GIRDER_STEEL       , NULL, 0 },
	{ 1995, 2, 16, 255, 320, 0xA28                             , STR_5014_TUBULAR_STEEL      , NULL, 0 },
	{ 2005, 2, 32, 380, 512, 0xA28 | PALETTE_TO_STRUCT_YELLOW  , STR_5014_TUBULAR_STEEL      , NULL, 0 },
	{ 2010, 2, 32, 510, 608, 0xA28 | PALETTE_TO_STRUCT_GREY    , STR_BRIDGE_TUBULAR_SILICON  , NULL, 0 }
};

Bridge _bridge[MAX_BRIDGES];


// calculate the price factor for building a long bridge.
// basically the cost delta is 1,1, 1, 2,2, 3,3,3, 4,4,4,4, 5,5,5,5,5, 6,6,6,6,6,6,  7,7,7,7,7,7,7,  8,8,8,8,8,8,8,8,
int CalcBridgeLenCostFactor(int x)
{
	int n;
	int r;

	if (x < 2) return x;
	x -= 2;
	for (n = 0, r = 2;; n++) {
		if (x <= n) return r + x * n;
		r += n * n;
		x -= n;
	}
}

#define M(x) (1 << (x))
enum {
	// foundation, whole tile is leveled up --> 3 corners raised
	BRIDGE_FULL_LEVELED_FOUNDATION = M(SLOPE_WSE) | M(SLOPE_NWS) | M(SLOPE_ENW) | M(SLOPE_SEN),
	// foundation, tile is partly leveled up --> 1 corner raised
	BRIDGE_PARTLY_LEVELED_FOUNDATION = M(SLOPE_W) | M(SLOPE_S) | M(SLOPE_E) | M(SLOPE_N),
	// no foundations (X,Y direction)
	BRIDGE_NO_FOUNDATION = M(SLOPE_FLAT) | M(SLOPE_SW) | M(SLOPE_SE) | M(SLOPE_NW) | M(SLOPE_NE),
	BRIDGE_HORZ_RAMP = (BRIDGE_PARTLY_LEVELED_FOUNDATION | BRIDGE_NO_FOUNDATION) & ~M(SLOPE_FLAT)
};
#undef M

static inline const PalSpriteID *GetBridgeSpriteTable(int index, byte table)
{
	const Bridge *bridge = &_bridge[index];
	assert(table < 7);
	if (bridge->sprite_table == NULL || bridge->sprite_table[table] == NULL) {
		return _bridge_sprite_table[index][table];
	} else {
		return bridge->sprite_table[table];
	}
}

static inline byte GetBridgeFlags(int index) { return _bridge[index].flags;}


/** Check the slope at the bridge ramps in three easy steps:
 * - valid slopes without foundation
 * - valid slopes with foundation
 * - rest is invalid
 */
#define M(x) (1 << (x))
static int32 CheckBridgeSlopeNorth(Axis axis, Slope tileh)
{
	uint32 valid;

	valid = M(SLOPE_FLAT) | (axis == AXIS_X ? M(SLOPE_NE) : M(SLOPE_NW));
	if (HASBIT(valid, tileh)) return 0;

	valid =
		BRIDGE_FULL_LEVELED_FOUNDATION | M(SLOPE_N) | M(SLOPE_STEEP_N) |
		(axis == AXIS_X ? M(SLOPE_E) | M(SLOPE_STEEP_E) : M(SLOPE_W) | M(SLOPE_STEEP_W));
	if (HASBIT(valid, tileh)) return _price.terraform;

	return CMD_ERROR;
}

static int32 CheckBridgeSlopeSouth(Axis axis, Slope tileh)
{
	uint32 valid;

	valid = M(SLOPE_FLAT) | (axis == AXIS_X ? M(SLOPE_SW) : M(SLOPE_SE));
	if (HASBIT(valid, tileh)) return 0;

	valid =
		BRIDGE_FULL_LEVELED_FOUNDATION | M(SLOPE_S) | M(SLOPE_STEEP_S) |
		(axis == AXIS_X ? M(SLOPE_W) | M(SLOPE_STEEP_W) : M(SLOPE_E) | M(SLOPE_STEEP_E));
	if (HASBIT(valid, tileh)) return _price.terraform;

	return CMD_ERROR;
}
#undef M


uint32 GetBridgeLength(TileIndex begin, TileIndex end)
{
	int x1 = TileX(begin);
	int y1 = TileY(begin);
	int x2 = TileX(end);
	int y2 = TileY(end);

	return abs(x2 + y2 - x1 - y1) - 1;
}

bool CheckBridge_Stuff(byte bridge_type, uint bridge_len)
{
	const Bridge *b = &_bridge[bridge_type];
	uint max; // max possible length of a bridge (with patch 100)

	if (bridge_type >= MAX_BRIDGES) return false;
	if (b->avail_year > _cur_year) return false;

	max = b->max_length;
	if (max >= 16 && _patches.longbridges) max = 100;

	return b->min_length <= bridge_len && bridge_len <= max;
}

/** Build a Bridge
 * @param end_tile end tile
 * @param p1 packed start tile coords (~ dx)
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0- 7) - bridge type (hi bh)
 * - p2 = (bit 8-..) - rail type. bit15 ((x>>8)&0x80) means road bridge.
 */
int32 CmdBuildBridge(TileIndex end_tile, uint32 flags, uint32 p1, uint32 p2)
{
	int bridge_type;
	TransportType transport;
	RailType railtype;
	uint x;
	uint y;
	uint sx;
	uint sy;
	TileIndex tile_start;
	TileIndex tile_end;
	Slope tileh_start;
	Slope tileh_end;
	uint z_start;
	uint z_end;
	TileIndex tile;
	TileIndexDiff delta;
	uint bridge_len;
	uint odd_middle_part;
	Axis direction;
	uint i;
	int32 cost, terraformcost, ret;
	bool allow_on_slopes;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* unpack parameters */
	bridge_type = GB(p2, 0, 8);

	if (p1 >= MapSize()) return CMD_ERROR;

	// type of bridge
	if (HASBIT(p2, 15)) {
		railtype = 0;
		transport = TRANSPORT_ROAD;
	} else {
		if (!ValParamRailtype(GB(p2, 8, 8))) return CMD_ERROR;
		railtype = GB(p2, 8, 8);
		transport = TRANSPORT_RAIL;
	}

	x = TileX(end_tile);
	y = TileY(end_tile);
	sx = TileX(p1);
	sy = TileY(p1);

	/* check if valid, and make sure that (x,y) are smaller than (sx,sy) */
	if (x == sx) {
		if (y == sy) return_cmd_error(STR_5008_CANNOT_START_AND_END_ON);
		direction = AXIS_Y;
		if (y > sy) uintswap(y,sy);
	} else if (y == sy) {
		direction = AXIS_X;
		if (x > sx) uintswap(x,sx);
	} else {
		return_cmd_error(STR_500A_START_AND_END_MUST_BE_IN);
	}

	/* set and test bridge length, availability */
	bridge_len = sx + sy - x - y - 1;
	if (!CheckBridge_Stuff(bridge_type, bridge_len)) return_cmd_error(STR_5015_CAN_T_BUILD_BRIDGE_HERE);

	/* retrieve landscape height and ensure it's on land */
	tile_start = TileXY(x, y);
	tile_end = TileXY(sx, sy);
	if (IsClearWaterTile(tile_start) || IsClearWaterTile(tile_end)) {
		return_cmd_error(STR_02A0_ENDS_OF_BRIDGE_MUST_BOTH);
	}

	tileh_start = GetTileSlope(tile_start, &z_start);
	tileh_end = GetTileSlope(tile_end, &z_end);

	if (IsSteepSlope(tileh_start)) z_start += TILE_HEIGHT;
	if (HASBIT(BRIDGE_FULL_LEVELED_FOUNDATION, tileh_start)) {
		z_start += TILE_HEIGHT;
		tileh_start = SLOPE_FLAT;
	}

	if (IsSteepSlope(tileh_end)) z_end += TILE_HEIGHT;
	if (HASBIT(BRIDGE_FULL_LEVELED_FOUNDATION, tileh_end)) {
		z_end += TILE_HEIGHT;
		tileh_end = SLOPE_FLAT;
	}

	if (z_start != z_end) return_cmd_error(STR_5009_LEVEL_LAND_OR_WATER_REQUIRED);

	// Towns are not allowed to use bridges on slopes.
	allow_on_slopes = (!_is_old_ai_player
	                   && _current_player != OWNER_TOWN && _patches.build_on_slopes);

	/* Try and clear the start landscape */

	ret = DoCommand(tile_start, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;
	cost = ret;

	terraformcost = CheckBridgeSlopeNorth(direction, tileh_start);
	if (CmdFailed(terraformcost) || (terraformcost != 0 && !allow_on_slopes))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	cost += terraformcost;

	/* Try and clear the end landscape */

	ret = DoCommand(tile_end, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;
	cost += ret;

	// false - end tile slope check
	terraformcost = CheckBridgeSlopeSouth(direction, tileh_end);
	if (CmdFailed(terraformcost) || (terraformcost != 0 && !allow_on_slopes))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	cost += terraformcost;


	/* do the drill? */
	if (flags & DC_EXEC) {
		DiagDirection dir = AxisToDiagDir(direction);

		if (transport == TRANSPORT_RAIL) {
			MakeRailBridgeRamp(tile_start, _current_player, bridge_type, dir, railtype);
			MakeRailBridgeRamp(tile_end,   _current_player, bridge_type, ReverseDiagDir(dir), railtype);
		} else {
			MakeRoadBridgeRamp(tile_start, _current_player, bridge_type, dir);
			MakeRoadBridgeRamp(tile_end,   _current_player, bridge_type, ReverseDiagDir(dir));
		}
		MarkTileDirtyByTile(tile_start);
		MarkTileDirtyByTile(tile_end);
	}

	// position of middle part of the odd bridge (larger than MAX(i) otherwise)
	odd_middle_part = (bridge_len % 2) ? (bridge_len / 2) : bridge_len;

	tile = tile_start;
	delta = (direction == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
	for (i = 0; i != bridge_len; i++) {
		TransportType transport_under;
		Owner owner_under = OWNER_NONE;
		RailType rail_under = INVALID_RAILTYPE;
		uint z;

		tile += delta;

		if (GetTileSlope(tile, &z) != SLOPE_FLAT && z >= z_start) {
			return_cmd_error(STR_5009_LEVEL_LAND_OR_WATER_REQUIRED);
		}

		switch (GetTileType(tile)) {
			case MP_WATER:
				if (!EnsureNoVehicle(tile)) return_cmd_error(STR_980E_SHIP_IN_THE_WAY);
				if (!IsWater(tile) && !IsCoast(tile)) goto not_valid_below;
				transport_under = TRANSPORT_WATER;
				owner_under = GetTileOwner(tile);
				break;

			case MP_RAILWAY:
				if (GetRailTileType(tile) != RAIL_TILE_NORMAL ||
						GetTrackBits(tile) != AxisToTrackBits(OtherAxis(direction))) {
					goto not_valid_below;
				}
				transport_under = TRANSPORT_RAIL;
				owner_under = GetTileOwner(tile);
				rail_under = GetRailType(tile);
				break;

			case MP_STREET:
				if (GetRoadTileType(tile) != ROAD_TILE_NORMAL) goto not_valid_below;
				if (HasRoadWorks(tile)) return_cmd_error(STR_ROAD_WORKS_IN_PROGRESS);
				if (GetRoadBits(tile) != (direction == AXIS_X ? ROAD_Y : ROAD_X)) {
					goto not_valid_below;
				}
				transport_under = TRANSPORT_ROAD;
				owner_under = GetTileOwner(tile);
				break;

			default:
not_valid_below:;
				/* try and clear the middle landscape */
				ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
				if (CmdFailed(ret)) return ret;
				cost += ret;
				transport_under = INVALID_TRANSPORT;
				break;
		}

		if (flags & DC_EXEC) {
			uint piece;

			//bridges pieces sequence (middle parts)
			// bridge len 1: 0
			// bridge len 2: 0 1
			// bridge len 3: 0 4 1
			// bridge len 4: 0 2 3 1
			// bridge len 5: 0 2 5 3 1
			// bridge len 6: 0 2 3 2 3 1
			// bridge len 7: 0 2 3 4 2 3 1
			// #0 - always as first, #1 - always as last (if len>1)
			// #2,#3 are to pair in order
			// for odd bridges: #5 is going in the bridge middle if on even position, #4 on odd (counting from 0)

			if (i == 0) { // first tile
				piece = 0;
			} else if (i == bridge_len - 1) { // last tile
				piece = 1;
			} else if (i == odd_middle_part) { // we are on the middle of odd bridge: #5 on even pos, #4 on odd
				piece = 5 - (i % 2);
			} else {
					// generate #2 and #3 in turns [i%2==0], after the middle of odd bridge
					// this sequence swaps [... XOR (i>odd_middle_part)],
					// for even bridges XOR does not apply as odd_middle_part==bridge_len
					piece = 2 + ((i % 2 == 0) ^ (i > odd_middle_part));
			}

			if (transport == TRANSPORT_RAIL) {
				MakeRailBridgeMiddle(tile, bridge_type, piece, direction, railtype);
			} else {
				MakeRoadBridgeMiddle(tile, bridge_type, piece, direction);
			}
			switch (transport_under) {
				case TRANSPORT_RAIL: SetRailUnderBridge(tile, owner_under, rail_under); break;
				case TRANSPORT_ROAD: SetRoadUnderBridge(tile, owner_under); break;

				case TRANSPORT_WATER:
					if (owner_under == OWNER_WATER) {
						SetWaterUnderBridge(tile);
					} else {
						SetCanalUnderBridge(tile, owner_under);
					}
					break;

				default: SetClearUnderBridge(tile); break;
			}

			MarkTileDirtyByTile(tile);
		}
	}

	SetSignalsOnBothDir(tile_start, AxisToTrack(direction));
	YapfNotifyTrackLayoutChange(tile_start, AxisToTrack(direction));

	/* for human player that builds the bridge he gets a selection to choose from bridges (DC_QUERY_COST)
	 * It's unnecessary to execute this command every time for every bridge. So it is done only
	 * and cost is computed in "bridge_gui.c". For AI, Towns this has to be of course calculated
	 */
	if (!(flags & DC_QUERY_COST)) {
		const Bridge *b = &_bridge[bridge_type];

		bridge_len += 2; // begin and end tiles/ramps

		if (IsValidPlayer(_current_player) && !_is_old_ai_player)
			bridge_len = CalcBridgeLenCostFactor(bridge_len);

		cost += (int64)bridge_len * _price.build_bridge * b->price >> 8;
	}

	return cost;
}


/** Build Tunnel.
 * @param tile start tile of tunnel
 * @param p1 railtype, 0x200 for road tunnel
 * @param p2 unused
 */
int32 CmdBuildTunnel(TileIndex start_tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndexDiff delta;
	TileIndex end_tile;
	DiagDirection direction;
	Slope start_tileh;
	Slope end_tileh;
	uint start_z;
	uint end_z;
	int32 cost;
	int32 ret;

	_build_tunnel_endtile = 0;

	if (p1 != 0x200 && !ValParamRailtype(p1)) return CMD_ERROR;

	start_tileh = GetTileSlope(start_tile, &start_z);

	switch (start_tileh) {
		case SLOPE_SW: direction = DIAGDIR_SW; break;
		case SLOPE_SE: direction = DIAGDIR_SE; break;
		case SLOPE_NW: direction = DIAGDIR_NW; break;
		case SLOPE_NE: direction = DIAGDIR_NE; break;
		default: return_cmd_error(STR_500B_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	ret = DoCommand(start_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;

	/* XXX - do NOT change 'ret' in the loop, as it is used as the price
	 * for the clearing of the entrance of the tunnel. Assigning it to
	 * cost before the loop will yield different costs depending on start-
	 * position, because of increased-cost-by-length: 'cost += cost >> 3' */
	cost = 0;
	delta = TileOffsByDiagDir(direction);
	end_tile = start_tile;
	for (;;) {
		end_tile += delta;
		end_tileh = GetTileSlope(end_tile, &end_z);

		if (start_z == end_z) break;

		if (!_cheats.crossing_tunnels.value && IsTunnelInWay(end_tile, start_z)) {
			return_cmd_error(STR_5003_ANOTHER_TUNNEL_IN_THE_WAY);
		}

		cost += _price.build_tunnel;
		cost += cost >> 3; // add a multiplier for longer tunnels
		if (cost >= 400000000) cost = 400000000;
	}

	/* Add the cost of the entrance */
	cost += _price.build_tunnel + ret;

	// if the command fails from here on we want the end tile to be highlighted
	_build_tunnel_endtile = end_tile;

	// slope of end tile must be complementary to the slope of the start tile
	if (end_tileh != ComplementSlope(start_tileh)) {
		ret = DoCommand(end_tile, end_tileh & start_tileh, 0, flags, CMD_TERRAFORM_LAND);
		if (CmdFailed(ret)) return_cmd_error(STR_5005_UNABLE_TO_EXCAVATE_LAND);
	} else {
		ret = DoCommand(end_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
	}
	cost += _price.build_tunnel + ret;

	if (flags & DC_EXEC) {
		if (GB(p1, 9, 1) == TRANSPORT_RAIL) {
			MakeRailTunnel(start_tile, _current_player, direction,                 GB(p1, 0, 4));
			MakeRailTunnel(end_tile,   _current_player, ReverseDiagDir(direction), GB(p1, 0, 4));
			UpdateSignalsOnSegment(start_tile, direction);
			YapfNotifyTrackLayoutChange(start_tile, AxisToTrack(DiagDirToAxis(direction)));
		} else {
			MakeRoadTunnel(start_tile, _current_player, direction);
			MakeRoadTunnel(end_tile,   _current_player, ReverseDiagDir(direction));
		}
	}

	return cost;
}

TileIndex CheckTunnelBusy(TileIndex tile, uint *length)
{
	uint z = GetTileZ(tile);
	DiagDirection dir = GetTunnelDirection(tile);
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	uint len = 0;
	TileIndex starttile = tile;
	Vehicle *v;

	do {
		tile += delta;
		len++;
	} while (
		!IsTunnelTile(tile) ||
		ReverseDiagDir(GetTunnelDirection(tile)) != dir ||
		GetTileZ(tile) != z
	);

	v = FindVehicleBetween(starttile, tile, z);
	if (v != NULL) {
		_error_message = v->type == VEH_Train ?
			STR_5000_TRAIN_IN_TUNNEL : STR_5001_ROAD_VEHICLE_IN_TUNNEL;
		return INVALID_TILE;
	}

	if (length != NULL) *length = len;
	return tile;
}

static inline bool CheckAllowRemoveTunnelBridge(TileIndex tile)
{
	/* Floods can remove anything as well as the scenario editor */
	if (_current_player == OWNER_WATER || _game_mode == GM_EDITOR) return true;
	/* Obviously if the bridge/tunnel belongs to us, or no-one, we can remove it */
	if (CheckTileOwnership(tile) || IsTileOwner(tile, OWNER_NONE)) return true;
	/* Otherwise we can only remove town-owned stuff with extra patch-settings, or cheat */
	if (IsTileOwner(tile, OWNER_TOWN) && (_patches.extra_dynamite || _cheats.magic_bulldozer.value)) return true;
	return false;
}

static int32 DoClearTunnel(TileIndex tile, uint32 flags)
{
	Town *t = NULL;
	TileIndex endtile;
	uint length;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!CheckAllowRemoveTunnelBridge(tile)) return CMD_ERROR;

	endtile = CheckTunnelBusy(tile, &length);
	if (endtile == INVALID_TILE) return CMD_ERROR;

	_build_tunnel_endtile = endtile;

	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		t = ClosestTownFromTile(tile, (uint)-1); // town penalty rating

		/* Check if you are allowed to remove the tunnel owned by a town
		 * Removal depends on difficulty settings */
		if (!CheckforTownRating(flags, t, TUNNELBRIDGE_REMOVE)) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	if (flags & DC_EXEC) {
		// We first need to request the direction before calling DoClearSquare
		//  else the direction is always 0.. dah!! ;)
		DiagDirection dir = GetTunnelDirection(tile);
		Track track;

		// Adjust the town's player rating. Do this before removing the tile owner info.
		if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);

		DoClearSquare(tile);
		DoClearSquare(endtile);
		UpdateSignalsOnSegment(tile, ReverseDiagDir(dir));
		UpdateSignalsOnSegment(endtile, dir);
		track = AxisToTrack(DiagDirToAxis(dir));
		YapfNotifyTrackLayoutChange(tile, track);
		YapfNotifyTrackLayoutChange(endtile, track);
	}
	return _price.clear_tunnel * (length + 1);
}


static uint GetBridgeHeightRamp(TileIndex t)
{
	uint h;
	uint tileh = GetTileSlope(t, &h);
	uint f = GetBridgeFoundation(tileh, DiagDirToAxis(GetBridgeRampDirection(t)));

	// one height level extra if the ramp is on a flat foundation
	return
		h + TILE_HEIGHT +
		(IS_INT_INSIDE(f, 1, 15) ? TILE_HEIGHT : 0) +
		(IsSteepSlope(tileh) ? TILE_HEIGHT : 0);
}


static int32 DoClearBridge(TileIndex tile, uint32 flags)
{
	DiagDirection direction;
	TileIndexDiff delta;
	TileIndex endtile;
	Vehicle *v;
	Town *t = NULL;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (IsBridgeMiddle(tile)) {
		if (IsTransportUnderBridge(tile)) {
			/* delete transport route under the bridge */
			int32 cost;

			// check if we own the tile below the bridge..
			if (_current_player != OWNER_WATER && (!CheckTileOwnership(tile) || !EnsureNoVehicleOnGround(tile)))
				return CMD_ERROR;

			if (GetTransportTypeUnderBridge(tile) == TRANSPORT_RAIL) {
				cost = _price.remove_rail;
			} else {
				cost = _price.remove_road * 2;
			}

			if (flags & DC_EXEC) {
				SetClearUnderBridge(tile);
				MarkTileDirtyByTile(tile);
			}
			return cost;
		} else if (IsWaterUnderBridge(tile) && !IsTileOwner(tile, OWNER_WATER)) {
			/* delete canal under bridge */

			// check for vehicles under bridge
			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				SetClearUnderBridge(tile);
				MarkTileDirtyByTile(tile);
			}
			return _price.clear_water;
		}

		tile = GetSouthernBridgeEnd(tile);
	}

	if (!CheckAllowRemoveTunnelBridge(tile)) return CMD_ERROR;

	endtile = GetOtherBridgeEnd(tile);

	if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(endtile)) return CMD_ERROR;

	direction = GetBridgeRampDirection(tile);
	delta = TileOffsByDiagDir(direction);

	/* Make sure there's no vehicle on the bridge
	 * Omit tile and endtile, since these are already checked, thus solving the
	 * problem of bridges over water, or higher bridges, where z is not increased,
	 * eg level bridge
	 */
	v = FindVehicleBetween(
		tile    + delta,
		endtile - delta,
		GetBridgeHeightRamp(tile)
	);
	if (v != NULL) return_cmd_error(VehicleInTheWayErrMsg(v));

	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		t = ClosestTownFromTile(tile, (uint)-1); // town penalty rating

		/* Check if you are allowed to remove the bridge owned by a town
		 * Removal depends on difficulty settings */
		if (!CheckforTownRating(flags, t, TUNNELBRIDGE_REMOVE)) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	if (flags & DC_EXEC) {
		TileIndex c;
		Track track;

		//checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
		// you have a "Poor" (0) town rating
		if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);

		DoClearSquare(tile);
		DoClearSquare(endtile);
		for (c = tile + delta; c != endtile; c += delta) {
			if (IsTransportUnderBridge(c)) {
				if (GetTransportTypeUnderBridge(c) == TRANSPORT_RAIL) {
					MakeRailNormal(c, GetTileOwner(c), GetRailBitsUnderBridge(c), GetRailType(c));
				} else {
					TownID town = IsTileOwner(c, OWNER_TOWN) ? ClosestTownFromTile(c, (uint)-1)->index : 0;
					MakeRoadNormal(c, GetTileOwner(c), GetRoadBitsUnderBridge(c), town);
				}
				MarkTileDirtyByTile(c);
			} else {
				if (IsClearUnderBridge(c)) {
					DoClearSquare(c);
				} else {
					if (GetTileSlope(c, NULL) == SLOPE_FLAT) {
						if (IsTileOwner(c, OWNER_WATER)) {
							MakeWater(c);
						} else {
							MakeCanal(c, GetTileOwner(c));
						}
					} else {
						MakeShore(c);
					}
					MarkTileDirtyByTile(c);
				}
			}
		}

		UpdateSignalsOnSegment(tile, ReverseDiagDir(direction));
		UpdateSignalsOnSegment(endtile, direction);
		track = AxisToTrack(DiagDirToAxis(direction));
		YapfNotifyTrackLayoutChange(tile, track);
		YapfNotifyTrackLayoutChange(endtile, track);
	}

	return (DistanceManhattan(tile, endtile) + 1) * _price.clear_bridge;
}

static int32 ClearTile_TunnelBridge(TileIndex tile, byte flags)
{
	if (IsTunnel(tile)) {
		if (flags & DC_AUTO) return_cmd_error(STR_5006_MUST_DEMOLISH_TUNNEL_FIRST);
		return DoClearTunnel(tile, flags);
	} else if (IsBridge(tile)) { // XXX Is this necessary?
		if (flags & DC_AUTO) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		return DoClearBridge(tile, flags);
	}

	return CMD_ERROR;
}

int32 DoConvertTunnelBridgeRail(TileIndex tile, RailType totype, bool exec)
{
	TileIndex endtile;

	if (IsTunnel(tile) && GetTunnelTransportType(tile) == TRANSPORT_RAIL) {
		uint length;

		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		if (GetRailType(tile) == totype) return CMD_ERROR;

		// 'hidden' elrails can't be downgraded to normal rail when elrails are disabled
		if (_patches.disable_elrails && totype == RAILTYPE_RAIL && GetRailType(tile) == RAILTYPE_ELECTRIC) return CMD_ERROR;

		endtile = CheckTunnelBusy(tile, &length);
		if (endtile == INVALID_TILE) return CMD_ERROR;

		if (exec) {
			Track track;
			SetRailType(tile, totype);
			SetRailType(endtile, totype);
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(endtile);

			track = AxisToTrack(DiagDirToAxis(GetTunnelDirection(tile)));
			YapfNotifyTrackLayoutChange(tile, track);
			YapfNotifyTrackLayoutChange(endtile, track);
		}
		return (length + 1) * (_price.build_rail >> 1);
	} else if (IsBridge(tile) &&
			IsBridgeMiddle(tile) &&
			IsTransportUnderBridge(tile) &&
			GetTransportTypeUnderBridge(tile) == TRANSPORT_RAIL) {
		// only check for train under bridge
		if (!CheckTileOwnership(tile) || !EnsureNoVehicleOnGround(tile))
			return CMD_ERROR;

		if (GetRailType(tile) == totype) return CMD_ERROR;

		if (exec) {
			SetRailType(tile, totype);
			MarkTileDirtyByTile(tile);

			YapfNotifyTrackLayoutChange(tile, GetRailUnderBridge(tile));
		}
		return _price.build_rail >> 1;
	} else if (IsBridge(tile) && IsBridgeRamp(tile) && GetBridgeTransportType(tile) == TRANSPORT_RAIL) {
		TileIndexDiff delta;
		int32 cost;

		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		endtile = GetOtherBridgeEnd(tile);

		if (!EnsureNoVehicle(tile) ||
				!EnsureNoVehicle(endtile) ||
				FindVehicleBetween(tile, endtile, GetBridgeHeightRamp(tile)) != NULL) {
			return_cmd_error(STR_8803_TRAIN_IN_THE_WAY);
		}

		if (GetRailType(tile) == totype) return CMD_ERROR;

		if (exec) {
			Track track;
			SetRailType(tile, totype);
			SetRailType(endtile, totype);
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(endtile);

			track = AxisToTrack(DiagDirToAxis(GetBridgeRampDirection(tile)));
			YapfNotifyTrackLayoutChange(tile, track);
			YapfNotifyTrackLayoutChange(endtile, track);
		}
		cost = 2 * (_price.build_rail >> 1);
		delta = TileOffsByDiagDir(GetBridgeRampDirection(tile));
		for (tile += delta; tile != endtile; tile += delta) {
			if (exec) {
				SetRailTypeOnBridge(tile, totype);
				MarkTileDirtyByTile(tile);
			}
			cost += _price.build_rail >> 1;
		}

		return cost;
	} else {
		return CMD_ERROR;
	}
}


// fast routine for getting the height of a middle bridge tile. 'tile' MUST be a middle bridge tile.
uint GetBridgeHeight(TileIndex t)
{
	return GetBridgeHeightRamp(GetSouthernBridgeEnd(t));
}

static const byte _bridge_foundations[][31] = {
	// 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15                              _S              _W      _N  _E
	{  0, 16, 18,  3, 20,  5,  0,  7, 22,  0, 10, 11, 12, 13, 14,  0,  0,  0,  0,  0,  0,  0,  0, 18,  0,  0,  0, 16,  0, 22, 20 },
	{  0, 15, 17,  0, 19,  5,  6,  7, 21,  9, 10, 11,  0, 13, 14,  0,  0,  0,  0,  0,  0,  0,  0, 17,  0,  0,  0, 15,  0, 21, 19 }
};

extern const byte _road_sloped_sprites[14];

static void DrawBridgePillars(PalSpriteID image, const TileInfo *ti, int x, int y, int z)
{
	if (image != 0) {
		Axis axis = GetBridgeAxis(ti->tile);
		bool drawfarpillar = !HASBIT(GetBridgeFlags(GetBridgeType(ti->tile)), 0);
		int back_height, front_height;
		int i = z;
		const byte *p;

		static const byte _tileh_bits[4][8] = {
			{ 2, 1, 8, 4,  16,  2, 0, 9 },
			{ 1, 8, 4, 2,   2, 16, 9, 0 },
			{ 4, 8, 1, 2,  16,  2, 0, 9 },
			{ 2, 4, 8, 1,   2, 16, 9, 0 }
		};

		if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);

		p = _tileh_bits[(image & 1) * 2 + (axis == AXIS_X ? 0 : 1)];
		front_height = ti->z + (ti->tileh & p[0] ? TILE_HEIGHT : 0);
		back_height  = ti->z + (ti->tileh & p[1] ? TILE_HEIGHT : 0);

		if (IsSteepSlope(ti->tileh)) {
			if (!(ti->tileh & p[2])) front_height += TILE_HEIGHT;
			if (!(ti->tileh & p[3])) back_height  += TILE_HEIGHT;
		}

		for (; z >= front_height || z >= back_height; z -= TILE_HEIGHT) {
			/* HACK set height of the BB of pillars to 1, because the origin of the
			 * sprites is at the top
			 */
			if (z >= front_height) { // front facing pillar
				AddSortableSpriteToDraw(image, x, y, p[4], p[5], 1, z);
			}

			if (drawfarpillar && z >= back_height && z < i - TILE_HEIGHT) { // back facing pillar
				AddSortableSpriteToDraw(image, x - p[6], y - p[7], p[4], p[5], 1, z);
			}
		}
	}
}

uint GetBridgeFoundation(Slope tileh, Axis axis)
{
	uint i;

	if (HASBIT(BRIDGE_FULL_LEVELED_FOUNDATION, tileh)) return tileh;

	// inclined sloped building
	switch (tileh) {
		case SLOPE_W:
		case SLOPE_STEEP_W: i = 0; break;
		case SLOPE_S:
		case SLOPE_STEEP_S: i = 2; break;
		case SLOPE_E:
		case SLOPE_STEEP_E: i = 4; break;
		case SLOPE_N:
		case SLOPE_STEEP_N: i = 6; break;
		default: return 0;
	}
	if (axis != AXIS_X) ++i;
	return i + 15;
}

/**
 * Draws a tunnel of bridge tile.
 * For tunnels, this is rather simple, as you only needa draw the entrance.
 * Bridges are a bit more complex. base_offset is where the sprite selection comes into play
 * and it works a bit like a bitmask.<p> For bridge heads:
 * <ul><li>Bit 0: direction</li>
 * <li>Bit 1: northern or southern heads</li>
 * <li>Bit 2: Set if the bridge head is sloped</li>
 * <li>Bit 3 and more: Railtype Specific subset</li>
 * </ul>
 * For middle parts:
 * <ul><li>Bits 0-1: need to be 0</li>
 * <li>Bit 2: direction</li>
 * <li>Bit 3 and above: Railtype Specific subset</li>
 * </ul>
 * Please note that in this code, "roads" are treated as railtype 1, whilst the real railtypes are 0, 2 and 3
 */
static void DrawTile_TunnelBridge(TileInfo *ti)
{
	uint32 image;
	const PalSpriteID *b;
	bool ice = _m[ti->tile].m4 & 0x80;

	if (IsTunnel(ti->tile)) {
		if (GetTunnelTransportType(ti->tile) == TRANSPORT_RAIL) {
			image = GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.tunnel;
		} else {
			image = SPR_TUNNEL_ENTRY_REAR_ROAD;
		}

		if (ice) image += 32;

		image += GetTunnelDirection(ti->tile) * 2;
		DrawGroundSprite(image);
		if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

		AddSortableSpriteToDraw(image+1, ti->x + TILE_SIZE - 1, ti->y + TILE_SIZE - 1, 1, 1, 8, (byte)ti->z);
	} else if (IsBridge(ti->tile)) { // XXX is this necessary?
		int base_offset;

		if (GetBridgeTransportType(ti->tile) == TRANSPORT_RAIL) {
			RailType rt;

			if (IsBridgeRamp(ti->tile)) {
				rt = GetRailType(ti->tile);
			} else {
				rt = GetRailTypeOnBridge(ti->tile);
			}

			base_offset = GetRailTypeInfo(rt)->bridge_offset;
			assert(base_offset != 8); /* This one is used for roads */
		} else {
			base_offset = 8;
		}

		/* as the lower 3 bits are used for other stuff, make sure they are clear */
		assert( (base_offset & 0x07) == 0x00);

		if (IsBridgeRamp(ti->tile)) {
			if (!HASBIT(BRIDGE_NO_FOUNDATION, ti->tileh)) {
				int f = GetBridgeFoundation(ti->tileh, DiagDirToAxis(GetBridgeRampDirection(ti->tile)));
				if (f) DrawFoundation(ti, f);
			}

			// HACK Wizardry to convert the bridge ramp direction into a sprite offset
			base_offset += (6 - GetBridgeRampDirection(ti->tile)) % 4;

			if (ti->tileh == SLOPE_FLAT) base_offset += 4; // sloped bridge head

			/* Table number 6 always refers to the bridge heads for any bridge type */
			image = GetBridgeSpriteTable(GetBridgeType(ti->tile), 6)[base_offset];

			if (!ice) {
				DrawClearLandTile(ti, 3);
			} else {
				DrawGroundSprite(SPR_FLAT_SNOWY_TILE + _tileh_to_sprite[ti->tileh]);
			}

			if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

			// draw ramp
			if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
			/* HACK set the height of the BB of a sloped ramp to 1 so a vehicle on
			 * it doesn't disappear behind it
			 */
			AddSortableSpriteToDraw(
				image, ti->x, ti->y, 16, 16, ti->tileh == SLOPE_FLAT ? 1 : 8, ti->z
			);
		} else {
			// bridge middle part.
			Axis axis = GetBridgeAxis(ti->tile);
			uint z;
			int x,y;

			if (IsTransportUnderBridge(ti->tile)) {
				uint f = _bridge_foundations[axis][ti->tileh];

				if (f != 0) DrawFoundation(ti, f);

				if (GetTransportTypeUnderBridge(ti->tile) == TRANSPORT_RAIL) {
					const RailtypeInfo* rti = GetRailTypeInfo(GetRailType(ti->tile));

					if (ti->tileh == SLOPE_FLAT) {
						image = (axis == AXIS_X ? SPR_RAIL_TRACK_Y : SPR_RAIL_TRACK_X);
					} else {
						image = SPR_RAIL_TRACK_Y + _track_sloped_sprites[ti->tileh - 1];
					}
					image += rti->total_offset;
					if (ice) image += rti->snow_offset;
				} else {
					if (ti->tileh == SLOPE_FLAT) {
						image = (axis == AXIS_X ? SPR_ROAD_Y : SPR_ROAD_X);
					} else {
						image = _road_sloped_sprites[ti->tileh - 1] + 0x53F;
					}
					if (ice) image += 19;
				}
				DrawGroundSprite(image);
			} else {
				if (IsClearUnderBridge(ti->tile)) {
					image = (ice ? SPR_FLAT_SNOWY_TILE : SPR_FLAT_GRASS_TILE);
					DrawGroundSprite(image + _tileh_to_sprite[ti->tileh]);
				} else {
					if (ti->tileh == SLOPE_FLAT) {
						DrawGroundSprite(SPR_FLAT_WATER_TILE);
						if (ti->z != 0 || !IsTileOwner(ti->tile, OWNER_WATER)) DrawCanalWater(ti->tile);
					} else {
						DrawGroundSprite(_water_shore_sprites[ti->tileh]);
					}
				}
			}

			if (axis != AXIS_X) base_offset += 4;

			/*  base_offset needs to be 0 due to the structure of the sprite table see table/bridge_land.h */
			assert( (base_offset & 0x03) == 0x00);
			// get bridge sprites
			b = GetBridgeSpriteTable(GetBridgeType(ti->tile), GetBridgePiece(ti->tile)) + base_offset;

			z = GetBridgeHeight(ti->tile) - 3;

			// draw rail or road component
			image = b[0];
			if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
			if (axis == AXIS_X) {
				AddSortableSpriteToDraw(image, ti->x, ti->y, 16, 11, 1, z);
			} else {
				AddSortableSpriteToDraw(image, ti->x, ti->y, 11, 16, 1, z);
			}

			x = ti->x;
			y = ti->y;
			image = b[1];
			if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);

			// draw roof, the component of the bridge which is logically between the vehicle and the camera
			if (axis == AXIS_X) {
				y += 12;
				if (image & SPRITE_MASK) AddSortableSpriteToDraw(image, x, y, 16, 1, 0x28, z);
			} else {
				x += 12;
				if (image & SPRITE_MASK) AddSortableSpriteToDraw(image, x, y, 1, 16, 0x28, z);
			}

			if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC || GetRailTypeOnBridge(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);

			if (ti->z + 5 == z) {
				// draw poles below for small bridges
				image = b[2];
				if (image != 0) {
					if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
					DrawGroundSpriteAt(image, x, y, z);
				}
			} else if (_patches.bridge_pillars) {
				// draw pillars below for high bridges
				DrawBridgePillars(b[2], ti, x, y, z);
			}
		}
	}
}

static uint GetSlopeZ_TunnelBridge(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	x &= 0xF;
	y &= 0xF;

	if (IsTunnel(tile)) {
		uint pos = (DiagDirToAxis(GetTunnelDirection(tile)) == AXIS_X ? y : x);

		// In the tunnel entrance?
		if (5 <= pos && pos <= 10) return z;
	} else {
		if (IsBridgeRamp(tile)) {
			DiagDirection dir = GetBridgeRampDirection(tile);
			uint pos = (DiagDirToAxis(dir) == AXIS_X ? y : x);

			// On the bridge ramp?
			if (5 <= pos && pos <= 10) {
				uint delta;

				if (IsSteepSlope(tileh)) return z + TILE_HEIGHT * 2;
				if (HASBIT(BRIDGE_HORZ_RAMP, tileh)) return z + TILE_HEIGHT;

				if (HASBIT(BRIDGE_FULL_LEVELED_FOUNDATION, tileh)) z += TILE_HEIGHT;
				switch (dir) {
					default:
					case DIAGDIR_NE: delta = (TILE_SIZE - 1 - x) / 2; break;
					case DIAGDIR_SE: delta = y / 2; break;
					case DIAGDIR_SW: delta = x / 2; break;
					case DIAGDIR_NW: delta = (TILE_SIZE - 1 - y) / 2; break;
				}
				return z + 1 + delta;
			} else {
				uint f = GetBridgeFoundation(tileh, DiagDirToAxis(dir));

				if (f != 0) {
					if (IsSteepSlope(tileh)) {
						z += TILE_HEIGHT;
					} else if (f < 15) {
						return z + TILE_HEIGHT;
					}
					tileh = _inclined_tileh[f - 15];
				}
			}
		} else {
			uint ground_z;

			// HACK on the bridge?
			ground_z = z + TILE_HEIGHT;
			if (tileh != SLOPE_FLAT) ground_z += TILE_HEIGHT;
			if (IsSteepSlope(tileh)) ground_z += TILE_HEIGHT;
			if (_get_z_hint >= ground_z) return _get_z_hint;

			if (IsTransportUnderBridge(tile)) {
				uint f = _bridge_foundations[GetBridgeAxis(tile)][tileh];

				if (f != 0) {
					if (IsSteepSlope(tileh)) {
						z += TILE_HEIGHT;
					} else if (f < 15) {
						return z + TILE_HEIGHT;
					}
					tileh = _inclined_tileh[f - 15];
				}
			}
		}
	}

	return z + GetPartialZ(x, y, tileh);
}

static Slope GetSlopeTileh_TunnelBridge(TileIndex tile, Slope tileh)
{
	uint f;

	if (IsTunnel(tile)) {
		return tileh;
	} else {
		if (IsBridgeRamp(tile)) {
			if (HASBIT(BRIDGE_NO_FOUNDATION, tileh)) {
				return tileh;
			} else {
				f = GetBridgeFoundation(tileh, DiagDirToAxis(GetBridgeRampDirection(tile)));
			}
		} else {
			if (IsTransportUnderBridge(tile)) {
				f = _bridge_foundations[GetBridgeAxis(tile)][tileh];
			} else {
				return tileh;
			}
		}
	}

	if (f == 0) return tileh;
	if (f < 15) return SLOPE_FLAT;
	return _inclined_tileh[f - 15];
}


static void GetAcceptedCargo_TunnelBridge(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static const StringID _bridge_tile_str[(MAX_BRIDGES + 3) + (MAX_BRIDGES + 3)] = {
	STR_501F_WOODEN_RAIL_BRIDGE,
	STR_5020_CONCRETE_RAIL_BRIDGE,
	STR_501C_STEEL_GIRDER_RAIL_BRIDGE,
	STR_501E_REINFORCED_CONCRETE_SUSPENSION,
	STR_501B_STEEL_SUSPENSION_RAIL_BRIDGE,
	STR_501B_STEEL_SUSPENSION_RAIL_BRIDGE,
	STR_501D_STEEL_CANTILEVER_RAIL_BRIDGE,
	STR_501D_STEEL_CANTILEVER_RAIL_BRIDGE,
	STR_501D_STEEL_CANTILEVER_RAIL_BRIDGE,
	STR_501C_STEEL_GIRDER_RAIL_BRIDGE,
	STR_5027_TUBULAR_RAIL_BRIDGE,
	STR_5027_TUBULAR_RAIL_BRIDGE,
	STR_5027_TUBULAR_RAIL_BRIDGE,
	0, 0, 0,

	STR_5025_WOODEN_ROAD_BRIDGE,
	STR_5026_CONCRETE_ROAD_BRIDGE,
	STR_5022_STEEL_GIRDER_ROAD_BRIDGE,
	STR_5024_REINFORCED_CONCRETE_SUSPENSION,
	STR_5021_STEEL_SUSPENSION_ROAD_BRIDGE,
	STR_5021_STEEL_SUSPENSION_ROAD_BRIDGE,
	STR_5023_STEEL_CANTILEVER_ROAD_BRIDGE,
	STR_5023_STEEL_CANTILEVER_ROAD_BRIDGE,
	STR_5023_STEEL_CANTILEVER_ROAD_BRIDGE,
	STR_5022_STEEL_GIRDER_ROAD_BRIDGE,
	STR_5028_TUBULAR_ROAD_BRIDGE,
	STR_5028_TUBULAR_ROAD_BRIDGE,
	STR_5028_TUBULAR_ROAD_BRIDGE,
	0, 0, 0,
};

static void GetTileDesc_TunnelBridge(TileIndex tile, TileDesc *td)
{
	if (IsTunnel(tile)) {
		td->str = (GetTunnelTransportType(tile) == TRANSPORT_RAIL) ?
			STR_5017_RAILROAD_TUNNEL : STR_5018_ROAD_TUNNEL;
	} else {
		td->str = _bridge_tile_str[GetBridgeTransportType(tile) << 4 | GetBridgeType(tile)];

		// the owner is stored at the end of the bridge
		if (IsBridgeMiddle(tile)) tile = GetSouthernBridgeEnd(tile);
	}
	td->owner = GetTileOwner(tile);
}


static void AnimateTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}

static void TileLoop_TunnelBridge(TileIndex tile)
{
	switch (_opt.landscape) {
		case LT_HILLY:
			if (HASBIT(_m[tile].m4, 7) != (GetTileZ(tile) > _opt.snow_line)) {
				TOGGLEBIT(_m[tile].m4, 7);
				MarkTileDirtyByTile(tile);
			}
			break;

		case LT_DESERT:
			if (GetTropicZone(tile) == TROPICZONE_DESERT && !(_m[tile].m4 & 0x80)) {
				_m[tile].m4 |= 0x80;
				MarkTileDirtyByTile(tile);
			}
			break;
	}

	if (IsBridge(tile) && IsBridgeMiddle(tile) && IsWaterUnderBridge(tile)) {
		TileLoop_Water(tile);
	}
}

static void ClickTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}


static uint32 GetTileTrackStatus_TunnelBridge(TileIndex tile, TransportType mode)
{
	if (IsTunnel(tile)) {
		if (GetTunnelTransportType(tile) != mode) return 0;
		return AxisToTrackBits(DiagDirToAxis(GetTunnelDirection(tile))) * 0x101;
	} else {
		if (IsBridgeRamp(tile)) {
			if (GetBridgeTransportType(tile) != mode) return 0;
			return AxisToTrackBits(DiagDirToAxis(GetBridgeRampDirection(tile))) * 0x101;
		} else {
			uint32 result = 0;

			if (GetBridgeTransportType(tile) == mode) {
				result = AxisToTrackBits(GetBridgeAxis(tile)) * 0x101;
			}
			if ((IsTransportUnderBridge(tile) && mode == GetTransportTypeUnderBridge(tile)) ||
					(IsWaterUnderBridge(tile)     && mode == TRANSPORT_WATER && GetTileSlope(tile, NULL) == SLOPE_FLAT)) {
				result |= AxisToTrackBits(OtherAxis(GetBridgeAxis(tile))) * 0x101;
			}
			return result;
		}
	}
}

static void ChangeTileOwner_TunnelBridge(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else {
		if (IsBridge(tile) && IsBridgeMiddle(tile) && IsTransportUnderBridge(tile)) {
			// the stuff BELOW the middle part is owned by the deleted player.
			if (GetTransportTypeUnderBridge(tile) == TRANSPORT_RAIL) {
				SetClearUnderBridge(tile);
			} else {
				SetTileOwner(tile, OWNER_NONE);
			}
		} else {
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}
}


static const byte _tunnel_fractcoord_1[4]    = {0x8E, 0x18, 0x81, 0xE8};
static const byte _tunnel_fractcoord_2[4]    = {0x81, 0x98, 0x87, 0x38};
static const byte _tunnel_fractcoord_3[4]    = {0x82, 0x88, 0x86, 0x48};
static const byte _exit_tunnel_track[4]      = {1, 2, 1, 2};

static const byte _road_exit_tunnel_state[4] = {8, 9, 0, 1};
static const byte _road_exit_tunnel_frame[4] = {2, 7, 9, 4};

static const byte _tunnel_fractcoord_4[4]    = {0x52, 0x85, 0x98, 0x29};
static const byte _tunnel_fractcoord_5[4]    = {0x92, 0x89, 0x58, 0x25};
static const byte _tunnel_fractcoord_6[4]    = {0x92, 0x89, 0x56, 0x45};
static const byte _tunnel_fractcoord_7[4]    = {0x52, 0x85, 0x96, 0x49};

static uint32 VehicleEnter_TunnelBridge(Vehicle *v, TileIndex tile, int x, int y)
{
	if (IsTunnel(tile)) {
		int z = GetSlopeZ(x, y) - v->z_pos;
		byte fc;
		DiagDirection dir;
		DiagDirection vdir;

		if (myabs(z) > 2) return 8;

		if (v->type == VEH_Train) {
			fc = (x & 0xF) + (y << 4);

			dir = GetTunnelDirection(tile);
			vdir = DirToDiagDir(v->direction);

			if (v->u.rail.track != 0x40 && dir == vdir) {
				if (IsFrontEngine(v) && fc == _tunnel_fractcoord_1[dir]) {
					if (!PlayVehicleSound(v, VSE_TUNNEL) && v->spritenum < 4) {
						SndPlayVehicleFx(SND_05_TRAIN_THROUGH_TUNNEL, v);
					}
					return 0;
				}
				if (fc == _tunnel_fractcoord_2[dir]) {
					v->tile = tile;
					v->u.rail.track = 0x40;
					v->vehstatus |= VS_HIDDEN;
					return 4;
				}
			}

			if (dir == ReverseDiagDir(vdir) && fc == _tunnel_fractcoord_3[dir] && z == 0) {
				/* We're at the tunnel exit ?? */
				v->tile = tile;
				v->u.rail.track = _exit_tunnel_track[dir];
				assert(v->u.rail.track);
				v->vehstatus &= ~VS_HIDDEN;
				return 4;
			}
		} else if (v->type == VEH_Road) {
			fc = (x & 0xF) + (y << 4);
			dir = GetTunnelDirection(tile);
			vdir = DirToDiagDir(v->direction);

			// Enter tunnel?
			if (v->u.road.state != 0xFF && dir == vdir) {
				if (fc == _tunnel_fractcoord_4[dir] ||
						fc == _tunnel_fractcoord_5[dir]) {
					v->tile = tile;
					v->u.road.state = 0xFF;
					v->vehstatus |= VS_HIDDEN;
					return 4;
				} else {
					return 0;
				}
			}

			if (dir == ReverseDiagDir(vdir) && (
						/* We're at the tunnel exit ?? */
						fc == _tunnel_fractcoord_6[dir] ||
						fc == _tunnel_fractcoord_7[dir]
					) &&
					z == 0) {
				v->tile = tile;
				v->u.road.state = _road_exit_tunnel_state[dir];
				v->u.road.frame = _road_exit_tunnel_frame[dir];
				v->vehstatus &= ~VS_HIDDEN;
				return 4;
			}
		}
	} else if (IsBridge(tile)) { // XXX is this necessary?
		if (v->type == VEH_Road || (v->type == VEH_Train && IsFrontEngine(v))) {
			if (IsBridgeRamp(tile) || v->z_pos > GetTileMaxZ(tile)) {
				/* modify speed of vehicle */
				uint16 spd = _bridge[GetBridgeType(tile)].speed;
				if (v->type == VEH_Road) spd *= 2;
				if (v->cur_speed > spd) v->cur_speed = spd;
			}
		}
	}
	return 0;
}

/** Retrieve the exit-tile of the vehicle from inside a tunnel
 * Very similar to GetOtherTunnelEnd(), but we use the vehicle's
 * direction for determining which end of the tunnel to find
 * @param v the vehicle which is inside the tunnel and needs an exit
 * @return the exit-tile of the tunnel based on the vehicle's direction */
TileIndex GetVehicleOutOfTunnelTile(const Vehicle *v)
{
#if 1
	return v->tile;
#else
	TileIndex tile = v->tile;
	DiagDirection dir = DirToDiagDir(v->direction);
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	byte z = v->z_pos;

	dir = ReverseDiagDir(dir);
	while (
		!IsTunnelTile(tile) ||
		GetTunnelDirection(tile) != dir ||
		GetTileZ(tile) != z
	) {
		tile += delta;
	}

	return tile;
#endif
}

const TileTypeProcs _tile_type_tunnelbridge_procs = {
	DrawTile_TunnelBridge,           /* draw_tile_proc */
	GetSlopeZ_TunnelBridge,          /* get_slope_z_proc */
	ClearTile_TunnelBridge,          /* clear_tile_proc */
	GetAcceptedCargo_TunnelBridge,   /* get_accepted_cargo_proc */
	GetTileDesc_TunnelBridge,        /* get_tile_desc_proc */
	GetTileTrackStatus_TunnelBridge, /* get_tile_track_status_proc */
	ClickTile_TunnelBridge,          /* click_tile_proc */
	AnimateTile_TunnelBridge,        /* animate_tile_proc */
	TileLoop_TunnelBridge,           /* tile_loop_clear */
	ChangeTileOwner_TunnelBridge,    /* change_tile_owner_clear */
	NULL,                            /* get_produced_cargo_proc */
	VehicleEnter_TunnelBridge,       /* vehicle_enter_tile_proc */
	GetSlopeTileh_TunnelBridge,      /* get_slope_tileh_proc */
};
