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
	{  0, 0, 16,  80,  32, 0xA24                             , STR_5012_WOODEN             , NULL, 0 },
	{  0, 0,  2, 112,  48, 0xA26 | PALETTE_TO_STRUCT_RED     , STR_5013_CONCRETE           , NULL, 0 },
	{ 10, 0,  5, 144,  64, 0xA25                             , STR_500F_GIRDER_STEEL       , NULL, 0 },
	{  0, 2, 10, 168,  80, 0xA22 | PALETTE_TO_STRUCT_CONCRETE, STR_5011_SUSPENSION_CONCRETE, NULL, 0 },
	{ 10, 3, 16, 185,  96, 0xA22                             , STR_500E_SUSPENSION_STEEL   , NULL, 0 },
	{ 10, 3, 16, 192, 112, 0xA22 | PALETTE_TO_STRUCT_YELLOW  , STR_500E_SUSPENSION_STEEL   , NULL, 0 },
	{ 10, 3,  7, 224, 160, 0xA23                             , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 10, 3,  8, 232, 208, 0xA23 | PALETTE_TO_STRUCT_BROWN   , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 10, 3,  9, 248, 240, 0xA23 | PALETTE_TO_STRUCT_RED     , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 10, 0,  2, 240, 256, 0xA27                             , STR_500F_GIRDER_STEEL       , NULL, 0 },
	{ 75, 2, 16, 255, 320, 0xA28                             , STR_5014_TUBULAR_STEEL      , NULL, 0 },
	{ 85, 2, 32, 380, 512, 0xA28 | PALETTE_TO_STRUCT_YELLOW  , STR_5014_TUBULAR_STEEL      , NULL, 0 },
	{ 90, 2, 32, 510, 608, 0xA28 | PALETTE_TO_STRUCT_GREY    , STR_BRIDGE_TUBULAR_SILICON  , NULL, 0 }
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

enum {
	// foundation, whole tile is leveled up (tileh's 7, 11, 13, 14) --> 3 corners raised
	BRIDGE_FULL_LEVELED_FOUNDATION = 1 << 7 | 1 << 11 | 1 << 13 | 1 << 14,
	// foundation, tile is partly leveled up (tileh's 1, 2, 4, 8) --> 1 corner raised
	BRIDGE_PARTLY_LEVELED_FOUNDATION = 1 << 1 | 1 << 2 | 1 << 4 | 1 << 8,
	// no foundations (X,Y direction) (tileh's 0, 3, 6, 9, 12)
	BRIDGE_NO_FOUNDATION = 1 << 0 | 1 << 3 | 1 << 6 | 1 << 9 | 1 << 12,
};

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


/**	check if bridge can be built on slope
 *	direction 0 = X-axis, direction 1 = Y-axis
 *	is_start_tile = false		<-- end tile
 *	is_start_tile = true		<-- start tile
 */
static uint32 CheckBridgeSlope(Axis direction, uint tileh, bool is_start_tile)
{
	if (IsSteepTileh(tileh)) return CMD_ERROR;

	if (is_start_tile) {
		/* check slope at start tile
				- no extra cost
				- direction X: tiles 0, 12
				- direction Y: tiles 0,  9
		*/
		if ((direction == AXIS_X ? 0x1001 : 0x201) & (1 << tileh)) return 0;

		// disallow certain start tiles to avoid certain crooked bridges
		if (tileh == 2) return CMD_ERROR;
	} else {
		/*	check slope at end tile
				- no extra cost
				- direction X: tiles 0, 3
				- direction Y: tiles 0, 6
		*/
		if ((direction == AXIS_X ? 0x9 : 0x41) & (1 << tileh)) return 0;

		// disallow certain end tiles to avoid certain crooked bridges
		if (tileh == 8) return CMD_ERROR;
	}

	/*	disallow common start/end tiles to avoid certain crooked bridges e.g.
	 *	start-tile:	X 2,1 Y 2,4 (2 was disabled before)
	 *	end-tile:		X 8,4 Y 8,1 (8 was disabled before)
	 */
	if ((tileh == 1 && is_start_tile != (direction != AXIS_X)) ||
			(tileh == 4 && is_start_tile == (direction != AXIS_X))) {
		return CMD_ERROR;
	}

	// slope foundations
	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << tileh) || BRIDGE_PARTLY_LEVELED_FOUNDATION & (1 << tileh))
		return _price.terraform;

	return CMD_ERROR;
}

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
 * @param x,y end tile coord
 * @param p1 packed start tile coords (~ dx)
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0- 7) - bridge type (hi bh)
 * - p2 = (bit 8-..) - rail type. bit15 ((x>>8)&0x80) means road bridge.
 */
int32 CmdBuildBridge(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int bridge_type;
	TransportType transport;
	RailType railtype;
	int sx,sy;
	TileInfo ti_start, ti_end;
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

	sx = TileX(p1) * 16;
	sy = TileY(p1) * 16;

	/* check if valid, and make sure that (x,y) are smaller than (sx,sy) */
	if (x == sx) {
		if (y == sy) return_cmd_error(STR_5008_CANNOT_START_AND_END_ON);
		direction = AXIS_Y;
		if (y > sy) {
			intswap(y,sy);
			intswap(x,sx);
		}
	} else if (y == sy) {
		direction = AXIS_X;
		if (x > sx) {
			intswap(y,sy);
			intswap(x,sx);
		}
	} else {
		return_cmd_error(STR_500A_START_AND_END_MUST_BE_IN);
	}

	/* set and test bridge length, availability */
	bridge_len = ((sx + sy - x - y) >> 4) - 1;
	if (!CheckBridge_Stuff(bridge_type, bridge_len)) return_cmd_error(STR_5015_CAN_T_BUILD_BRIDGE_HERE);

	/* retrieve landscape height and ensure it's on land */
	if ((
				FindLandscapeHeight(&ti_end, sx, sy),
				ti_end.type == MP_WATER && ti_end.map5 == 0
			) || (
				FindLandscapeHeight(&ti_start, x, y),
				ti_start.type == MP_WATER && ti_start.map5 == 0
			)) {
		return_cmd_error(STR_02A0_ENDS_OF_BRIDGE_MUST_BOTH);
	}

	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << ti_start.tileh)) {
		ti_start.z += 8;
		ti_start.tileh = 0;
	}

	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << ti_end.tileh)) {
		ti_end.z += 8;
		ti_end.tileh = 0;
	}

	if (ti_start.z != ti_end.z)
		return_cmd_error(STR_5009_LEVEL_LAND_OR_WATER_REQUIRED);


	// Towns are not allowed to use bridges on slopes.
	allow_on_slopes = (!_is_old_ai_player
	                   && _current_player != OWNER_TOWN && _patches.build_on_slopes);

	/* Try and clear the start landscape */

	ret = DoCommandByTile(ti_start.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;
	cost = ret;

	// true - bridge-start-tile, false - bridge-end-tile
	terraformcost = CheckBridgeSlope(direction, ti_start.tileh, true);
	if (CmdFailed(terraformcost) || (terraformcost && !allow_on_slopes))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	cost += terraformcost;

	/* Try and clear the end landscape */

	ret = DoCommandByTile(ti_end.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;
	cost += ret;

	// false - end tile slope check
	terraformcost = CheckBridgeSlope(direction, ti_end.tileh, false);
	if (CmdFailed(terraformcost) || (terraformcost && !allow_on_slopes))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
	cost += terraformcost;


	/* do the drill? */
	if (flags & DC_EXEC) {
		DiagDirection dir = AxisToDiagDir(direction);

		if (transport == TRANSPORT_RAIL) {
			MakeRailBridgeRamp(ti_start.tile, _current_player, bridge_type, dir, railtype);
			MakeRailBridgeRamp(ti_end.tile,   _current_player, bridge_type, ReverseDiagDir(dir), railtype);
		} else {
			MakeRoadBridgeRamp(ti_start.tile, _current_player, bridge_type, dir);
			MakeRoadBridgeRamp(ti_end.tile,   _current_player, bridge_type, ReverseDiagDir(dir));
		}
		MarkTileDirtyByTile(ti_start.tile);
		MarkTileDirtyByTile(ti_end.tile);
	}

	// position of middle part of the odd bridge (larger than MAX(i) otherwise)
	odd_middle_part = (bridge_len % 2) ? (bridge_len / 2) : bridge_len;

	tile = ti_start.tile;
	delta = (direction == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
	for (i = 0; i != bridge_len; i++) {
		TransportType transport_under;
		Owner owner_under = OWNER_NONE;
		RailType rail_under = INVALID_RAILTYPE;
		uint z;

		tile += delta;

		if (GetTileSlope(tile, &z) != 0 && z >= ti_start.z) {
			return_cmd_error(STR_5009_LEVEL_LAND_OR_WATER_REQUIRED);
		}

		switch (GetTileType(tile)) {
			case MP_WATER:
				if (!EnsureNoVehicle(tile)) return_cmd_error(STR_980E_SHIP_IN_THE_WAY);
				if (_m[tile].m5 > 1) goto not_valid_below;
				transport_under = TRANSPORT_WATER;
				break;

			case MP_RAILWAY:
				if (_m[tile].m5 != (direction == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X)) {
					goto not_valid_below;
				}
				transport_under = TRANSPORT_RAIL;
				owner_under = GetTileOwner(tile);
				rail_under = GB(_m[tile].m3, 0, 4);
				break;

			case MP_STREET:
				if (GetRoadType(tile) != ROAD_NORMAL ||
						GetRoadBits(tile) != (direction == AXIS_X ? ROAD_Y : ROAD_X)) {
					goto not_valid_below;
				}
				transport_under = TRANSPORT_ROAD;
				owner_under = GetTileOwner(tile);
				break;

			default:
not_valid_below:;
				/* try and clear the middle landscape */
				ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
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
				case TRANSPORT_RAIL:  SetRailUnderBridge(tile, owner_under, rail_under); break;
				case TRANSPORT_ROAD:  SetRoadUnderBridge(tile, owner_under); break;
				case TRANSPORT_WATER: SetWaterUnderBridge(tile); break;
				default:              SetClearUnderBridge(tile); break;
			}

			MarkTileDirtyByTile(tile);
		}
	}

	SetSignalsOnBothDir(ti_start.tile, direction == AXIS_X ? TRACK_X : TRACK_Y);

	/*	for human player that builds the bridge he gets a selection to choose from bridges (DC_QUERY_COST)
			It's unnecessary to execute this command every time for every bridge. So it is done only
			and cost is computed in "bridge_gui.c". For AI, Towns this has to be of course calculated
	*/
	if (!(flags & DC_QUERY_COST)) {
		const Bridge *b = &_bridge[bridge_type];

		bridge_len += 2;	// begin and end tiles/ramps

		if (_current_player < MAX_PLAYERS && !_is_old_ai_player)
			bridge_len = CalcBridgeLenCostFactor(bridge_len);

		cost += (int64)bridge_len * _price.build_bridge * b->price >> 8;
	}

	return cost;
}


/** Build Tunnel.
 * @param x,y start tile coord of tunnel
 * @param p1 railtype, 0x200 for road tunnel
 * @param p2 unused
 */
int32 CmdBuildTunnel(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndexDiff delta;
	TileIndex start_tile;
	TileIndex end_tile;
	DiagDirection direction;
	uint start_tileh;
	uint end_tileh;
	uint start_z;
	uint end_z;
	int32 cost;
	int32 ret;

	_build_tunnel_endtile = 0;

	if (p1 != 0x200 && !ValParamRailtype(p1)) return CMD_ERROR;

	start_tile = TileVirtXY(x, y);
	start_tileh = GetTileSlope(start_tile, &start_z);

	switch (start_tileh) {
		case  3: direction = DIAGDIR_SW; break;
		case  6: direction = DIAGDIR_SE; break;
		case  9: direction = DIAGDIR_NW; break;
		case 12: direction = DIAGDIR_NE; break;
		default: return_cmd_error(STR_500B_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	ret = DoCommandByTile(start_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;
	cost = _price.build_tunnel + ret;

	delta = TileOffsByDir(direction);
	end_tile = start_tile;
	for (;;) {
		end_tile += delta;
		end_tileh = GetTileSlope(end_tile, &end_z);

		if (start_z == end_z) break;

		if (!_cheats.crossing_tunnels.value && IsTunnelInWay(end_tile, start_z)) {
			return_cmd_error(STR_5003_ANOTHER_TUNNEL_IN_THE_WAY);
		}

		cost += _price.build_tunnel;
		cost += cost >> 3;
		if (cost >= 400000000) cost = 400000000;
	}

	// if the command fails from here on we want the end tile to be highlighted
	_build_tunnel_endtile = end_tile;

	// slope of end tile must be complementary to the slope of the start tile
	if (end_tileh != (15 ^ start_tileh)) {
		ret = DoCommandByTile(end_tile, end_tileh & start_tileh, 0, flags, CMD_TERRAFORM_LAND);
		if (CmdFailed(ret)) return_cmd_error(STR_5005_UNABLE_TO_EXCAVATE_LAND);
	} else {
		ret = DoCommandByTile(end_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
	}
	cost += _price.build_tunnel + ret;

	if (flags & DC_EXEC) {
		if (GB(p1, 9, 1) == TRANSPORT_RAIL) {
			MakeRailTunnel(start_tile, _current_player, direction,                 GB(p1, 0, 4));
			MakeRailTunnel(end_tile,   _current_player, ReverseDiagDir(direction), GB(p1, 0, 4));
		} else {
			MakeRoadTunnel(start_tile, _current_player, direction);
			MakeRoadTunnel(end_tile,   _current_player, ReverseDiagDir(direction));
		}

		if (GB(p1, 9, 1) == 0) UpdateSignalsOnSegment(start_tile, direction);
	}

	return cost;
}

TileIndex CheckTunnelBusy(TileIndex tile, uint *length)
{
	uint z = GetTileZ(tile);
	DiagDirection dir = GetTunnelDirection(tile);
	TileIndexDiff delta = TileOffsByDir(dir);
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

static int32 DoClearTunnel(TileIndex tile, uint32 flags)
{
	Town *t;
	TileIndex endtile;
	uint length;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// in scenario editor you can always destroy tunnels
	if (_game_mode != GM_EDITOR && !CheckTileOwnership(tile)) {
		if (!(_patches.extra_dynamite || _cheats.magic_bulldozer.value) || !IsTileOwner(tile, OWNER_TOWN))
			return CMD_ERROR;
	}

	endtile = CheckTunnelBusy(tile, &length);
	if (endtile == INVALID_TILE) return CMD_ERROR;

	_build_tunnel_endtile = endtile;

	t = ClosestTownFromTile(tile, (uint)-1); //needed for town rating penalty
	// check if you're allowed to remove the tunnel owned by a town
	// removal allowal depends on difficulty settings
	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		if (!CheckforTownRating(flags, t, TUNNELBRIDGE_REMOVE)) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	if (flags & DC_EXEC) {
		// We first need to request the direction before calling DoClearSquare
		//  else the direction is always 0.. dah!! ;)
		DiagDirection dir = GetTunnelDirection(tile);

		// Adjust the town's player rating. Do this before removing the tile owner info.
		if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);

		DoClearSquare(tile);
		DoClearSquare(endtile);
		UpdateSignalsOnSegment(tile, ReverseDiagDir(dir));
		UpdateSignalsOnSegment(endtile, dir);
	}
	return _price.clear_tunnel * (length + 1);
}


static int32 DoClearBridge(TileIndex tile, uint32 flags)
{
	DiagDirection direction;
	TileIndexDiff delta;
	TileIndex endtile;
	Vehicle *v;
	Town *t;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (IsBridgeMiddle(tile)) {
		if (IsTransportUnderBridge(tile)) {
			/* delete transport route under the bridge */
			int32 cost;

			// check if we own the tile below the bridge..
			if (_current_player != OWNER_WATER && (!CheckTileOwnership(tile) || !EnsureNoVehicleZ(tile, TilePixelHeight(tile))))
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
		} else if (IsWaterUnderBridge(tile) && TilePixelHeight(tile) != 0) {
			/* delete canal under bridge */

			// check for vehicles under bridge
			if (!EnsureNoVehicleZ(tile, TilePixelHeight(tile))) return CMD_ERROR;

			if (flags & DC_EXEC) {
				SetClearUnderBridge(tile);
				MarkTileDirtyByTile(tile);
			}
			return _price.clear_water;
		}

		tile = GetSouthernBridgeEnd(tile);
	}

	// floods, scenario editor can always destroy bridges
	if (_current_player != OWNER_WATER && _game_mode != GM_EDITOR && !CheckTileOwnership(tile)) {
		if (!(_patches.extra_dynamite || _cheats.magic_bulldozer.value) || !IsTileOwner(tile, OWNER_TOWN))
			return CMD_ERROR;
	}

	endtile = GetOtherBridgeEnd(tile);

	if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(endtile)) return CMD_ERROR;

	direction = GetBridgeRampDirection(tile);
	delta = TileOffsByDir(direction);

	/*	Make sure there's no vehicle on the bridge
			Omit tile and endtile, since these are already checked, thus solving the problem
			of bridges over water, or higher bridges, where z is not increased, eg level bridge
	*/
	/* Bridges on slopes might have their Z-value offset..correct this */
	v = FindVehicleBetween(
		tile    + delta,
		endtile - delta,
		TilePixelHeight(tile) + 8 + GetCorrectTileHeight(tile)
	);
	if (v != NULL) {
		VehicleInTheWayErrMsg(v);
		return CMD_ERROR;
	}

	t = ClosestTownFromTile(tile, (uint)-1); //needed for town rating penalty
	// check if you're allowed to remove the bridge owned by a town.
	// removal allowal depends on difficulty settings
	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		if (!CheckforTownRating(flags, t, TUNNELBRIDGE_REMOVE)) return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		TileIndex c;

		//checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
		// you have a "Poor" (0) town rating
		if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);

		DoClearSquare(tile);
		DoClearSquare(endtile);
		for (c = tile + delta; c != endtile; c += delta) {
			if (_m[c].m5 & 0x20) {
				// transport under bridge
				if (GB(_m[c].m5, 3, 2) == TRANSPORT_RAIL) {
					MakeRailNormal(c, GetTileOwner(c), _m[c].m5 & 1 ? TRACK_BIT_X : TRACK_BIT_Y, GB(_m[c].m3, 0, 3));
				} else {
					uint town = IsTileOwner(c, OWNER_TOWN) ? ClosestTownFromTile(c, (uint)-1)->index : 0;
					MakeRoadNormal(c, GetTileOwner(c), _m[c].m5 & 1 ? ROAD_X : ROAD_Y, town);
				}
				MarkTileDirtyByTile(c);
			} else {
				// clear under bridge
				if (GB(_m[c].m5, 3, 2) == 0) {
					// grass under bridge
					DoClearSquare(c);
				} else {
					// water under bridge
					if (GetTileSlope(c, NULL) == 0) {
						MakeWater(c);
					} else {
						MakeShore(c);
					}
					MarkTileDirtyByTile(c);
				}
			}
		}

		UpdateSignalsOnSegment(tile, ReverseDiagDir(direction));
		UpdateSignalsOnSegment(endtile, direction);
	}

	return (DistanceManhattan(tile, endtile) + 1) * _price.clear_bridge;
}

static int32 ClearTile_TunnelBridge(TileIndex tile, byte flags)
{
	byte m5 = _m[tile].m5;

	if (IsTunnel(tile)) {
		if (flags & DC_AUTO) return_cmd_error(STR_5006_MUST_DEMOLISH_TUNNEL_FIRST);
		return DoClearTunnel(tile, flags);
	} else if (m5 & 0x80) {
		if (flags & DC_AUTO) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		return DoClearBridge(tile, flags);
	}

	return CMD_ERROR;
}

int32 DoConvertTunnelBridgeRail(TileIndex tile, uint totype, bool exec)
{
	TileIndex endtile;
	uint length;
	Vehicle *v;

	if (IsTunnel(tile) && GetTunnelTransportType(tile) == TRANSPORT_RAIL) {
		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		if (GB(_m[tile].m3, 0, 4) == totype) return CMD_ERROR;

		endtile = CheckTunnelBusy(tile, &length);
		if (endtile == INVALID_TILE) return CMD_ERROR;

		if (exec) {
			SB(_m[tile].m3, 0, 4, totype);
			SB(_m[endtile].m3, 0, 4, totype);
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(endtile);
		}
		return (length + 1) * (_price.build_rail >> 1);
	} else if ((_m[tile].m5 & 0xF8) == 0xE0) {
		// bridge middle part with rail below
		// only check for train under bridge
		if (!CheckTileOwnership(tile) || !EnsureNoVehicleZ(tile, TilePixelHeight(tile)))
			return CMD_ERROR;

		// tile is already of requested type?
		if (GB(_m[tile].m3, 0, 4) == totype) return CMD_ERROR;
		// change type.
		if (exec) {
			SB(_m[tile].m3, 0, 4, totype);
			MarkTileDirtyByTile(tile);
		}
		return _price.build_rail >> 1;
	} else if ((_m[tile].m5 & 0xC6) == 0x80) {
		TileIndexDiff delta;
		int32 cost;
		uint z = TilePixelHeight(tile);

		z += 8;

		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		// railway bridge
		endtile = GetOtherBridgeEnd(tile);
		// Make sure there's no vehicle on the bridge
		v = FindVehicleBetween(tile, endtile, z);
		if (v != NULL) {
			VehicleInTheWayErrMsg(v);
			return CMD_ERROR;
		}

		if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(endtile)) {
			_error_message = STR_8803_TRAIN_IN_THE_WAY;
			return CMD_ERROR;
		}

		if (GB(_m[tile].m3, 0, 4) == totype) return CMD_ERROR;

		if (exec) {
			SB(_m[tile].m3, 0, 4, totype);
			SB(_m[endtile].m3, 0, 4, totype);
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(endtile);
		}
		cost = 2 * (_price.build_rail >> 1);
		delta = TileOffsByDir(GetBridgeRampDirection(tile));
		for (tile += delta; tile != endtile; tile += delta) {
			if (exec) {
				SB(_m[tile].m3, 4, 4, totype);
				MarkTileDirtyByTile(tile);
			}
			cost += _price.build_rail >> 1;
		}

		return cost;
	} else
		return CMD_ERROR;
}


// fast routine for getting the height of a middle bridge tile. 'tile' MUST be a middle bridge tile.
static uint GetBridgeHeight(const TileInfo *ti)
{
	TileIndex tile = GetSouthernBridgeEnd(ti->tile);

	/* Return the height there (the height of the NORTH CORNER)
	 * If the end of the bridge is on a tileh 7 (all raised, except north corner),
	 * the z coordinate is 1 height level too low. Compensate for that */
	return TilePixelHeight(tile) + (GetTileSlope(tile, NULL) == 7 ? 8 : 0);
}

static const byte _bridge_foundations[2][16] = {
// 0 1  2  3  4 5 6 7  8 9 10 11 12 13 14 15
	{1,16,18,3,20,5,0,7,22,0,10,11,12,13,14},
	{1,15,17,0,19,5,6,7,21,9,10,11, 0,13,14},
};

extern const byte _road_sloped_sprites[14];

static void DrawBridgePillars(const TileInfo *ti, int x, int y, int z)
{
	const PalSpriteID *b;
	PalSpriteID image;
	int piece;

	b = _bridge_poles_table[GetBridgeType(ti->tile)];

	// Draw first piece
	// (necessary for cantilever bridges)

	image = b[12 + GB(ti->map5, 0, 1)];
	piece = GetBridgePiece(ti->tile);

	if (image != 0 && piece != 0) {
		if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
		DrawGroundSpriteAt(image, x, y, z);
	}

	image = b[GB(ti->map5, 0, 1) * 6 + piece];

	if (image != 0) {
		int back_height, front_height, i=z;
		const byte *p;

		static const byte _tileh_bits[4][8] = {
			{2,1,8,4,  16,11,0,9},
			{1,8,4,2,  11,16,9,0},
			{4,8,1,2,  16,11,0,9},
			{2,4,8,1,  11,16,9,0},
		};

		if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);

		p = _tileh_bits[(image & 1) * 2 + (ti->map5&0x01)];
		front_height = ti->z + ((ti->tileh & p[0])?8:0);
		back_height = ti->z + ((ti->tileh & p[1])?8:0);

		if (IsSteepTileh(ti->tileh)) {
			if (!(ti->tileh & p[2])) front_height += 8;
			if (!(ti->tileh & p[3])) back_height += 8;
		}

		for (; z >= front_height || z >= back_height; z = z - 8) {
			if (z >= front_height) {
				// front facing pillar
				AddSortableSpriteToDraw(image, x,y, p[4], p[5], 0x28, z);
			}
			if (z >= back_height && z < i - 8) {
				// back facing pillar
				AddSortableSpriteToDraw(image, x - p[6], y - p[7], p[4], p[5], 0x28, z);
			}
		}
	}
}

uint GetBridgeFoundation(uint tileh, Axis axis)
{
	int i;
	// normal level sloped building (7, 11, 13, 14)
	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << tileh)) return tileh;

	// inclined sloped building
	if ((
				(i  = 0, tileh == 1) ||
				(i += 2, tileh == 2) ||
				(i += 2, tileh == 4) ||
				(i += 2, tileh == 8)
			) && (
				      axis == AXIS_X ||
				(i++, axis == AXIS_Y)
			)) {
		return i + 15;
	}

	return 0;
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

	// draw tunnel?
	if (IsTunnel(ti->tile)) {
		if (GetTunnelTransportType(ti->tile) == TRANSPORT_RAIL) {
			image = GetRailTypeInfo(GB(_m[ti->tile].m3, 0, 4))->base_sprites.tunnel;
		} else {
			image = SPR_TUNNEL_ENTRY_REAR_ROAD;
		}

		if (ice) image += 32;

		image += GetTunnelDirection(ti->tile) * 2;
		DrawGroundSprite(image);

		AddSortableSpriteToDraw(image+1, ti->x + 15, ti->y + 15, 1, 1, 8, (byte)ti->z);
	// draw bridge?
	} else if (ti->map5 & 0x80) {
		RailType rt;
		int base_offset;

		if (HASBIT(ti->map5, 1)) { /* This is a road bridge */
			base_offset = 8;
		} else { /* Rail bridge */
			if (HASBIT(ti->map5, 6)) { /* The bits we need depend on the fact whether it is a bridge head or not */
				rt = GB(_m[ti->tile].m3, 4, 3);
			} else {
				rt = GB(_m[ti->tile].m3, 0, 3);
			}

			base_offset = GetRailTypeInfo(rt)->bridge_offset;
			assert(base_offset != 8); /* This one is used for roads */
		}

		/* as the lower 3 bits are used for other stuff, make sure they are clear */
		assert( (base_offset & 0x07) == 0x00);

		if (!(ti->map5 & 0x40)) {	// bridge ramps
			if (!(BRIDGE_NO_FOUNDATION & (1 << ti->tileh))) {	// no foundations for 0, 3, 6, 9, 12
				int f = GetBridgeFoundation(ti->tileh, ti->map5 & 0x1);	// pass direction
				if (f) DrawFoundation(ti, f);
			}

			// HACK Wizardry to convert the bridge ramp direction into a sprite offset
			base_offset += (6 - GetBridgeRampDirection(ti->tile)) % 4;

			if (ti->tileh == 0) base_offset += 4; // sloped bridge head

			/* Table number 6 always refers to the bridge heads for any bridge type */
			image = GetBridgeSpriteTable(GetBridgeType(ti->tile), 6)[base_offset];

			if (!ice) {
				DrawClearLandTile(ti, 3);
			} else {
				DrawGroundSprite(SPR_FLAT_SNOWY_TILE + _tileh_to_sprite[ti->tileh]);
			}

			// draw ramp
			if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
			AddSortableSpriteToDraw(image, ti->x, ti->y, 16, 16, 7, ti->z);
		} else {
			// bridge middle part.
			Axis axis = GetBridgeAxis(ti->tile);
			uint z;
			int x,y;

			if (IsTransportUnderBridge(ti->tile)) {
				// draw foundation?
				if (ti->tileh) {
					int f = _bridge_foundations[axis][ti->tileh];
					if (f) DrawFoundation(ti, f);
				}

				if (GetTransportTypeUnderBridge(ti->tile) == TRANSPORT_RAIL) {
					const RailtypeInfo *rti = GetRailTypeInfo(GB(_m[ti->tile].m3, 0, 4));

					if (ti->tileh == 0) {
						image = (axis == AXIS_X ? SPR_RAIL_TRACK_Y : SPR_RAIL_TRACK_X);
					} else {
						image = SPR_RAIL_TRACK_Y + _track_sloped_sprites[ti->tileh - 1];
					}
					image += rti->total_offset;
					if (ice) image += rti->snow_offset;
				} else {
					// road
					if (ti->tileh == 0) {
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
					if (ti->tileh == 0) {
						DrawGroundSprite(SPR_FLAT_WATER_TILE);
						if (ti->z != 0) DrawCanalWater(ti->tile);
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

			z = GetBridgeHeight(ti) + 5;

			// draw rail or road component
			image = b[0];
			if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
			AddSortableSpriteToDraw(
				image, ti->x, ti->y,
				axis == AXIS_X ? 16 : 11,
				axis == AXIS_X ? 11 : 16,
				1, z
			);

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

			if (ti->z + 5 == z) {
				// draw poles below for small bridges
				image = b[2];
				if (image != 0) {
					if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
					DrawGroundSpriteAt(image, x, y, z);
				}
			} else if (_patches.bridge_pillars) {
				// draw pillars below for high bridges
				DrawBridgePillars(ti, x, y, z);
			}
		}
	}
}

static uint GetSlopeZ_TunnelBridge(const TileInfo* ti)
{
	uint z = ti->z;
	uint x = ti->x & 0xF;
	uint y = ti->y & 0xF;
	uint tileh = ti->tileh;

	// swap directions if Y tunnel/bridge to let the code handle the X case only.
	if (ti->map5 & 1) uintswap(x,y);

	// to the side of the tunnel/bridge?
	if (IS_INT_INSIDE(y, 5, 10+1)) {
		// tunnel?
		if ((ti->map5 & 0xF0) == 0) return z;

		// bridge?
		if (ti->map5 & 0x80) {
			// bridge ending?
			if (!(ti->map5 & 0x40)) {
				if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << tileh)) // 7, 11, 13, 14
					z += 8;

				// no ramp for bridge ending
				if ((BRIDGE_PARTLY_LEVELED_FOUNDATION & (1 << tileh) || BRIDGE_NO_FOUNDATION & (1 << tileh)) && tileh != 0) {
					return z + 8;
				} else if (!(ti->map5 & 0x20)) { // northern / southern ending
					// ramp
					return z + (x >> 1) + 1;
				} else {
					// ramp in opposite dir
					return z + ((x ^ 0xF) >> 1);
				}

			// bridge middle part
			} else {
				// build on slopes?
				if (tileh != 0) z += 8;

				// keep the same elevation because we're on the bridge?
				if (_get_z_hint >= z + 8) return _get_z_hint;

				// actually on the bridge, but not yet in the shared area.
				if (!IS_INT_INSIDE(x, 5, 10 + 1)) return GetBridgeHeight(ti) + 8;

				// in the shared area, assume that we're below the bridge, cause otherwise the hint would've caught it.
				// if rail or road below then it means it's possibly build on slope below the bridge.
				if (ti->map5 & 0x20) {
					uint f = _bridge_foundations[ti->map5 & 1][tileh];
					// make sure that the slope is not inclined foundation
					if (IS_BYTE_INSIDE(f, 1, 15)) return z;

					// change foundation type? XXX - should be const; accessor function!
					if (f != 0) tileh = _inclined_tileh[f - 15];
				}

				// no transport route, fallback to default
			}
		}
	} else {
		// if it's a bridge middle with transport route below, then we need to compensate for build on slopes
		if ((ti->map5 & (0x80 | 0x40 | 0x20)) == (0x80 | 0x40 | 0x20)) {
			uint f;
			if (tileh != 0) z += 8;
			f = _bridge_foundations[ti->map5 & 1][tileh];
			if (IS_BYTE_INSIDE(f, 1, 15)) return z;
			if (f != 0) tileh = _inclined_tileh[f - 15];
		}
	}

	// default case
	return GetPartialZ(ti->x & 0xF, ti->y & 0xF, tileh) + ti->z;
}

static uint GetSlopeTileh_TunnelBridge(const TileInfo* ti)
{
	// not accurate, but good enough for slope graphics drawing
	return 0;
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
	0,0,0,

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
	0,0,0,
};

static void GetTileDesc_TunnelBridge(TileIndex tile, TileDesc *td)
{
	if (IsTunnel(tile)) {
		td->str = (GetTunnelTransportType(tile) == TRANSPORT_RAIL) ?
			STR_5017_RAILROAD_TUNNEL : STR_5018_ROAD_TUNNEL;
	} else {
		td->str = _bridge_tile_str[GB(_m[tile].m5, 1, 2) << 4 | GetBridgeType(tile)];

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
			if (GetTileZ(tile) > _opt.snow_line) {
				if (!(_m[tile].m4 & 0x80)) {
					_m[tile].m4 |= 0x80;
					MarkTileDirtyByTile(tile);
				}
			} else {
				if (_m[tile].m4 & 0x80) {
					_m[tile].m4 &= ~0x80;
					MarkTileDirtyByTile(tile);
				}
			}
			break;

		case LT_DESERT:
			if (GetMapExtraBits(tile) == 1 && !(_m[tile].m4 & 0x80)) {
				_m[tile].m4 |= 0x80;
				MarkTileDirtyByTile(tile);
			}
			break;
	}

	// if it's a bridge with water below, call tileloop_water on it.
	if ((_m[tile].m5 & 0xF8) == 0xC8) TileLoop_Water(tile);
}

static void ClickTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}


static uint32 GetTileTrackStatus_TunnelBridge(TileIndex tile, TransportType mode)
{
	uint32 result;
	byte m5 = _m[tile].m5;

	if (IsTunnel(tile)) {
		if (GetTunnelTransportType(tile) == mode) {
			return DiagDirToAxis(GetTunnelDirection(tile)) == AXIS_X ? 0x101 : 0x202;
		}
	} else if (m5 & 0x80) {
		/* This is a bridge */
		result = 0;
		if (GB(m5, 1, 2) == mode) {
			/* Transport over the bridge is compatible */
			result = m5 & 1 ? 0x202 : 0x101;
		}
		if (m5 & 0x40) {
			/* Bridge middle part */
			if (!(m5 & 0x20)) {
				/* Clear ground or water underneath */
				if ((m5 & 0x18) != 8) {
					/* Clear ground */
					return result;
				} else {
					if (mode != TRANSPORT_WATER) return result;
				}
			} else {
				/* Transport underneath */
				if (GB(m5, 3, 2) != mode) {
					/* Incompatible transport underneath */
					return result;
				}
			}
			/* If we've not returned yet, there is a compatible
			 * transport or water beneath, so we can add it to
			 * result */
			/* Why is this xor'd ? Can't it just be or'd? */
			result ^= m5 & 1 ? 0x101 : 0x202;
		}
		return result;
	} else {
		assert(0); /* This should never occur */
	}
	return 0;
}

static void ChangeTileOwner_TunnelBridge(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != OWNER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	}	else {
		if ((_m[tile].m5 & 0xC0) == 0xC0) {
			// the stuff BELOW the middle part is owned by the deleted player.
			if (!(_m[tile].m5 & (1 << 4 | 1 << 3))) {
				// convert railway into grass.
				SetClearUnderBridge(tile);
			} else {
				// for road, change the owner of the road to local authority
				SetTileOwner(tile, OWNER_NONE);
			}
		} else {
			DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}
}


static const byte _tunnel_fractcoord_1[4] = {0x8E,0x18,0x81,0xE8};
static const byte _tunnel_fractcoord_2[4] = {0x81,0x98,0x87,0x38};
static const byte _tunnel_fractcoord_3[4] = {0x82,0x88,0x86,0x48};
static const byte _exit_tunnel_track[4] = {1,2,1,2};

static const byte _road_exit_tunnel_state[4] = {8, 9, 0, 1};
static const byte _road_exit_tunnel_frame[4] = {2, 7, 9, 4};

static const byte _tunnel_fractcoord_4[4] = {0x52, 0x85, 0x98, 0x29};
static const byte _tunnel_fractcoord_5[4] = {0x92, 0x89, 0x58, 0x25};
static const byte _tunnel_fractcoord_6[4] = {0x92, 0x89, 0x56, 0x45};
static const byte _tunnel_fractcoord_7[4] = {0x52, 0x85, 0x96, 0x49};

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
					if (v->spritenum < 4)
						SndPlayVehicleFx(SND_05_TRAIN_THROUGH_TUNNEL, v);
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
	} else if (_m[tile].m5 & 0x80) {
		if (v->type == VEH_Road || (v->type == VEH_Train && IsFrontEngine(v))) {
			uint h;

			// Compensate for possible foundation
			if (GetTileSlope(tile, &h) != 0) h += 8;
			if (!(_m[tile].m5 & 0x40) || // start/end tile of bridge
					myabs(h - v->z_pos) > 2) { // high above the ground -> on the bridge
				/* modify speed of vehicle */
				uint16 spd = _bridge[GetBridgeType(tile)].speed;
				if (v->type == VEH_Road) spd *= 2;
				if (v->cur_speed > spd) v->cur_speed = spd;
			}
		}
	}
	return 0;
}

TileIndex GetVehicleOutOfTunnelTile(const Vehicle *v)
{
	TileIndex tile;
	TileIndexDiff delta = (v->direction & 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	byte z = v->z_pos;

	for (tile = v->tile;; tile += delta) {
		if (IsTunnelTile(tile) && GetTileZ(tile) == z) break;
	}
	return tile;
}

const TileTypeProcs _tile_type_tunnelbridge_procs = {
	DrawTile_TunnelBridge,					/* draw_tile_proc */
	GetSlopeZ_TunnelBridge,					/* get_slope_z_proc */
	ClearTile_TunnelBridge,					/* clear_tile_proc */
	GetAcceptedCargo_TunnelBridge,	/* get_accepted_cargo_proc */
	GetTileDesc_TunnelBridge,				/* get_tile_desc_proc */
	GetTileTrackStatus_TunnelBridge,/* get_tile_track_status_proc */
	ClickTile_TunnelBridge,					/* click_tile_proc */
	AnimateTile_TunnelBridge,				/* animate_tile_proc */
	TileLoop_TunnelBridge,					/* tile_loop_clear */
	ChangeTileOwner_TunnelBridge,		/* change_tile_owner_clear */
	NULL,														/* get_produced_cargo_proc */
	VehicleEnter_TunnelBridge,			/* vehicle_enter_tile_proc */
	NULL,														/* vehicle_leave_tile_proc */
	GetSlopeTileh_TunnelBridge,			/* get_slope_tileh_proc */
};
