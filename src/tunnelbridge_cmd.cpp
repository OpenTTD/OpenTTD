/* $Id$ */

/** @file tunnelbridge_cmd.cpp
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
#include "landscape.h"
#include "tile.h"
#include "tunnel_map.h"
#include "unmovable_map.h"
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

const Bridge orig_bridge[] = {
/*
	     year of availablity
	     |  minimum length
	     |  |   maximum length
	     |  |   |    price
	     |  |   |    |    maximum speed
	     |  |   |    |    |  sprite to use in GUI                string with description
	     |  |   |    |    |  |                                   |                            */
	{    0, 0, 16,  80,  32, 0xA24, PAL_NONE                  , STR_5012_WOODEN             , NULL, 0 },
	{    0, 0,  2, 112,  48, 0xA26, PALETTE_TO_STRUCT_RED     , STR_5013_CONCRETE           , NULL, 0 },
	{ 1930, 0,  5, 144,  64, 0xA25, PAL_NONE                  , STR_500F_GIRDER_STEEL       , NULL, 0 },
	{    0, 2, 10, 168,  80, 0xA22, PALETTE_TO_STRUCT_CONCRETE, STR_5011_SUSPENSION_CONCRETE, NULL, 0 },
	{ 1930, 3, 16, 185,  96, 0xA22, PAL_NONE                  , STR_500E_SUSPENSION_STEEL   , NULL, 0 },
	{ 1930, 3, 16, 192, 112, 0xA22, PALETTE_TO_STRUCT_YELLOW  , STR_500E_SUSPENSION_STEEL   , NULL, 0 },
	{ 1930, 3,  7, 224, 160, 0xA23, PAL_NONE                  , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 1930, 3,  8, 232, 208, 0xA23, PALETTE_TO_STRUCT_BROWN   , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 1930, 3,  9, 248, 240, 0xA23, PALETTE_TO_STRUCT_RED     , STR_5010_CANTILEVER_STEEL   , NULL, 0 },
	{ 1930, 0,  2, 240, 256, 0xA27, PAL_NONE                  , STR_500F_GIRDER_STEEL       , NULL, 0 },
	{ 1995, 2, 16, 255, 320, 0xA28, PAL_NONE                  , STR_5014_TUBULAR_STEEL      , NULL, 0 },
	{ 2005, 2, 32, 380, 512, 0xA28, PALETTE_TO_STRUCT_YELLOW  , STR_5014_TUBULAR_STEEL      , NULL, 0 },
	{ 2010, 2, 32, 510, 608, 0xA28, PALETTE_TO_STRUCT_GREY    , STR_BRIDGE_TUBULAR_SILICON  , NULL, 0 }
};

Bridge _bridge[MAX_BRIDGES];


/** calculate the price factor for building a long bridge.
 * basically the cost delta is 1,1, 1, 2,2, 3,3,3, 4,4,4,4, 5,5,5,5,5, 6,6,6,6,6,6,  7,7,7,7,7,7,7,  8,8,8,8,8,8,8,8,
 */
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
enum BridgeFoundation {
	/* foundation, whole tile is leveled up --> 3 corners raised */
	BRIDGE_FULL_LEVELED_FOUNDATION = M(SLOPE_WSE) | M(SLOPE_NWS) | M(SLOPE_ENW) | M(SLOPE_SEN),
	/* foundation, tile is partly leveled up --> 1 corner raised */
	BRIDGE_PARTLY_LEVELED_FOUNDATION = M(SLOPE_W) | M(SLOPE_S) | M(SLOPE_E) | M(SLOPE_N),
	/* no foundations (X,Y direction) */
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
static CommandCost CheckBridgeSlopeNorth(Axis axis, Slope tileh)
{
	uint32 valid;

	valid = M(SLOPE_FLAT) | (axis == AXIS_X ? M(SLOPE_NE) : M(SLOPE_NW));
	if (HASBIT(valid, tileh)) return CommandCost();

	valid =
		BRIDGE_FULL_LEVELED_FOUNDATION | M(SLOPE_N) | M(SLOPE_STEEP_N) |
		(axis == AXIS_X ? M(SLOPE_E) | M(SLOPE_STEEP_E) : M(SLOPE_W) | M(SLOPE_STEEP_W));
	if (HASBIT(valid, tileh)) return CommandCost(_price.terraform);

	return CMD_ERROR;
}

static CommandCost CheckBridgeSlopeSouth(Axis axis, Slope tileh)
{
	uint32 valid;

	valid = M(SLOPE_FLAT) | (axis == AXIS_X ? M(SLOPE_SW) : M(SLOPE_SE));
	if (HASBIT(valid, tileh)) return CommandCost();

	valid =
		BRIDGE_FULL_LEVELED_FOUNDATION | M(SLOPE_S) | M(SLOPE_STEEP_S) |
		(axis == AXIS_X ? M(SLOPE_W) | M(SLOPE_STEEP_W) : M(SLOPE_E) | M(SLOPE_STEEP_E));
	if (HASBIT(valid, tileh)) return CommandCost(_price.terraform);

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
 * @param flags type of operation
 * @param p1 packed start tile coords (~ dx)
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0- 7) - bridge type (hi bh)
 * - p2 = (bit 8-..) - rail type or road types.
 * - p2 = (bit 15  ) - set means road bridge.
 */
CommandCost CmdBuildBridge(TileIndex end_tile, uint32 flags, uint32 p1, uint32 p2)
{
	uint bridge_type;
	RailType railtype;
	RoadTypes roadtypes;
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
	Axis direction;
	CommandCost cost, terraformcost, ret;
	bool allow_on_slopes;
	bool replace_bridge = false;
	uint replaced_bridge_type;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* unpack parameters */
	bridge_type = GB(p2, 0, 8);

	if (p1 >= MapSize()) return CMD_ERROR;

	/* type of bridge */
	if (HASBIT(p2, 15)) {
		railtype = INVALID_RAILTYPE; // road bridge
		roadtypes = (RoadTypes)GB(p2, 8, 3);
		if (!AreValidRoadTypes(roadtypes)) return CMD_ERROR;
	} else {
		if (!ValParamRailtype(GB(p2, 8, 8))) return CMD_ERROR;
		railtype = (RailType)GB(p2, 8, 8);
		roadtypes = ROADTYPES_NONE;
	}

	x = TileX(end_tile);
	y = TileY(end_tile);
	sx = TileX(p1);
	sy = TileY(p1);

	/* check if valid, and make sure that (x,y) are smaller than (sx,sy) */
	if (x == sx) {
		if (y == sy) return_cmd_error(STR_5008_CANNOT_START_AND_END_ON);
		direction = AXIS_Y;
		if (y > sy) Swap(y, sy);
	} else if (y == sy) {
		direction = AXIS_X;
		if (x > sx) Swap(x, sx);
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

	/* Towns are not allowed to use bridges on slopes. */
	allow_on_slopes = (!_is_old_ai_player
	                   && _current_player != OWNER_TOWN && _patches.build_on_slopes);

	TransportType transport_type = railtype == INVALID_RAILTYPE ? TRANSPORT_ROAD : TRANSPORT_RAIL;

	if (IsBridgeTile(tile_start) && IsBridgeTile(tile_end) &&
			GetOtherBridgeEnd(tile_start) == tile_end &&
			GetBridgeTransportType(tile_start) == transport_type) {
		/* Replace a current bridge. */

		/* If this is a railway bridge, make sure the railtypes match. */
		if (transport_type == TRANSPORT_RAIL && GetRailType(tile_start) != railtype) {
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		}

		/* Do not replace town bridges with lower speed bridges. */
		if (!(flags & DC_QUERY_COST) && IsTileOwner(tile_start, OWNER_TOWN) &&
				_bridge[bridge_type].speed < _bridge[GetBridgeType(tile_start)].speed) {
			Town *t = ClosestTownFromTile(tile_start, UINT_MAX);

			if (t == NULL) {
				return CMD_ERROR;
			} else {
				SetDParam(0, t->index);
				return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
			}
		}

		/* Do not replace the bridge with the same bridge type. */
		if (!(flags & DC_QUERY_COST) && bridge_type == GetBridgeType(tile_start)) {
			return_cmd_error(STR_1007_ALREADY_BUILT);
		}

		/* Do not allow replacing another player's bridges. */
		if (!IsTileOwner(tile_start, _current_player) && !IsTileOwner(tile_start, OWNER_TOWN)) {
			return_cmd_error(STR_1024_AREA_IS_OWNED_BY_ANOTHER);
		}

		cost.AddCost((bridge_len + 1) * _price.clear_bridge); // The cost of clearing the current bridge.
		replace_bridge = true;
		replaced_bridge_type = GetBridgeType(tile_start);

		/* Do not remove road types when upgrading a bridge */
		roadtypes |= GetRoadTypes(tile_start);
	} else {
		/* Build a new bridge. */

		/* Try and clear the start landscape */
		ret = DoCommand(tile_start, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost = ret;

		terraformcost = CheckBridgeSlopeNorth(direction, tileh_start);
		if (CmdFailed(terraformcost) || (terraformcost.GetCost() != 0 && !allow_on_slopes))
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
		cost.AddCost(terraformcost);

		/* Try and clear the end landscape */
		ret = DoCommand(tile_end, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost.AddCost(ret);

		/* false - end tile slope check */
		terraformcost = CheckBridgeSlopeSouth(direction, tileh_end);
		if (CmdFailed(terraformcost) || (terraformcost.GetCost() != 0 && !allow_on_slopes))
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
		cost.AddCost(terraformcost);
	}

	if (!replace_bridge) {
		TileIndex Heads[] = {tile_start, tile_end};
		int i;

		for (i = 0; i < 2; i++) {
			if (MayHaveBridgeAbove(Heads[i])) {
				if (IsBridgeAbove(Heads[i])) {
					TileIndex north_head = GetNorthernBridgeEnd(Heads[i]);

					if (direction == GetBridgeAxis(Heads[i])) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

					if (z_start + TILE_HEIGHT == GetBridgeHeight(north_head)) {
						return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
					}
				}
			}
		}
	}

	/* do the drill? */
	if (flags & DC_EXEC) {
		DiagDirection dir = AxisToDiagDir(direction);
		Owner owner = (replace_bridge && IsTileOwner(tile_start, OWNER_TOWN)) ? OWNER_TOWN : _current_player;

		if (railtype != INVALID_RAILTYPE) {
			MakeRailBridgeRamp(tile_start, owner, bridge_type, dir, railtype);
			MakeRailBridgeRamp(tile_end,   owner, bridge_type, ReverseDiagDir(dir), railtype);
		} else {
			MakeRoadBridgeRamp(tile_start, owner, bridge_type, dir, roadtypes);
			MakeRoadBridgeRamp(tile_end,   owner, bridge_type, ReverseDiagDir(dir), roadtypes);
		}
		MarkTileDirtyByTile(tile_start);
		MarkTileDirtyByTile(tile_end);
	}

	delta = (direction == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
	for (tile = tile_start + delta; tile != tile_end; tile += delta) {
		uint z;

		if (GetTileSlope(tile, &z) != SLOPE_FLAT && z >= z_start) return_cmd_error(STR_5009_LEVEL_LAND_OR_WATER_REQUIRED);

		if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile) && !replace_bridge) {
			/* Disallow crossing bridges for the time being */
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		}

		switch (GetTileType(tile)) {
			case MP_WATER:
				if (!EnsureNoVehicle(tile)) return_cmd_error(STR_980E_SHIP_IN_THE_WAY);
				if (!IsWater(tile) && !IsCoast(tile)) goto not_valid_below;
				break;

			case MP_RAILWAY:
				if (!IsPlainRailTile(tile)) goto not_valid_below;
				break;

			case MP_STREET:
				if (GetRoadTileType(tile) == ROAD_TILE_DEPOT) goto not_valid_below;
				break;

			case MP_TUNNELBRIDGE:
				if (IsTunnel(tile)) break;
				if (replace_bridge) break;
				if (direction == DiagDirToAxis(GetBridgeRampDirection(tile))) goto not_valid_below;
				if (z_start < GetBridgeHeight(tile)) goto not_valid_below;
				break;

			case MP_UNMOVABLE:
				if (!IsOwnedLand(tile)) goto not_valid_below;
				break;

			case MP_CLEAR:
				if (!replace_bridge && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
				break;

			default:
not_valid_below:;
				/* try and clear the middle landscape */
				ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
				if (CmdFailed(ret)) return ret;
				cost.AddCost(ret);
				break;
		}

		if (flags & DC_EXEC) {
			SetBridgeMiddle(tile, direction);
			MarkTileDirtyByTile(tile);
		}
	}

	if (flags & DC_EXEC) {
		Track track = AxisToTrack(direction);
		SetSignalsOnBothDir(tile_start, track);
		YapfNotifyTrackLayoutChange(tile_start, track);
	}

	/* for human player that builds the bridge he gets a selection to choose from bridges (DC_QUERY_COST)
	 * It's unnecessary to execute this command every time for every bridge. So it is done only
	 * and cost is computed in "bridge_gui.c". For AI, Towns this has to be of course calculated
	 */
	if (!(flags & DC_QUERY_COST)) {
		const Bridge *b = &_bridge[bridge_type];

		bridge_len += 2; // begin and end tiles/ramps

		if (IsValidPlayer(_current_player) && !_is_old_ai_player)
			bridge_len = CalcBridgeLenCostFactor(bridge_len);

		cost.AddCost((int64)bridge_len * _price.build_bridge * b->price >> 8);
	}

	return cost;
}


/** Build Tunnel.
 * @param start_tile start tile of tunnel
 * @param flags type of operation
 * @param p1 railtype or roadtypes. bit 9 set means road tunnel
 * @param p2 unused
 */
CommandCost CmdBuildTunnel(TileIndex start_tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndexDiff delta;
	TileIndex end_tile;
	DiagDirection direction;
	Slope start_tileh;
	Slope end_tileh;
	uint start_z;
	uint end_z;
	CommandCost cost;
	CommandCost ret;

	_build_tunnel_endtile = 0;
	if (!HASBIT(p1, 9)) {
		if (!ValParamRailtype(p1)) return CMD_ERROR;
	} else if (!AreValidRoadTypes((RoadTypes)GB(p1, 0, 3))) {
		return CMD_ERROR;
	}

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

	delta = TileOffsByDiagDir(direction);
	DiagDirection tunnel_in_way_dir;
	if (OtherAxis(DiagDirToAxis(direction)) == AXIS_X) {
		tunnel_in_way_dir = (TileX(start_tile) < (MapMaxX() / 2)) ? DIAGDIR_SW : DIAGDIR_NE;
	} else {
		tunnel_in_way_dir = (TileY(start_tile) < (MapMaxX() / 2)) ? DIAGDIR_SE : DIAGDIR_NW;
	}

	end_tile = start_tile;
	for (;;) {
		end_tile += delta;
		end_tileh = GetTileSlope(end_tile, &end_z);

		if (start_z == end_z) break;

		if (!_cheats.crossing_tunnels.value && IsTunnelInWayDir(end_tile, start_z, tunnel_in_way_dir)) {
			return_cmd_error(STR_5003_ANOTHER_TUNNEL_IN_THE_WAY);
		}

		cost.AddCost(_price.build_tunnel);
		cost.AddCost(cost.GetCost() >> 3); // add a multiplier for longer tunnels
	}

	/* Add the cost of the entrance */
	cost.AddCost(_price.build_tunnel);
	cost.AddCost(ret);

	/* if the command fails from here on we want the end tile to be highlighted */
	_build_tunnel_endtile = end_tile;

	/* slope of end tile must be complementary to the slope of the start tile */
	if (end_tileh != ComplementSlope(start_tileh)) {
		/* Some (rail) track bits might be terraformed into the correct direction,
		 * but that would still leave tracks on foundation. Therefor excavation will
		 * always fail for rail tiles. On the other hand, for road tiles it might
		 * succeed when there is only one road bit on the tile, but then that road
		 * bit is removed leaving a clear tile.
		 * This therefor preserves the behaviour that half road tiles are always removable.
		 */
		if (IsTileType(end_tile, MP_RAILWAY)) return_cmd_error(STR_1008_MUST_REMOVE_RAILROAD_TRACK);

		ret = DoCommand(end_tile, end_tileh & start_tileh, 0, flags, CMD_TERRAFORM_LAND);
		if (CmdFailed(ret)) return_cmd_error(STR_5005_UNABLE_TO_EXCAVATE_LAND);
	} else {
		ret = DoCommand(end_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
	}
	cost.AddCost(_price.build_tunnel);
	cost.AddCost(ret);

	if (flags & DC_EXEC) {
		if (GB(p1, 9, 1) == TRANSPORT_RAIL) {
			MakeRailTunnel(start_tile, _current_player, direction,                 (RailType)GB(p1, 0, 4));
			MakeRailTunnel(end_tile,   _current_player, ReverseDiagDir(direction), (RailType)GB(p1, 0, 4));
			UpdateSignalsOnSegment(start_tile, direction);
			YapfNotifyTrackLayoutChange(start_tile, AxisToTrack(DiagDirToAxis(direction)));
		} else {
			MakeRoadTunnel(start_tile, _current_player, direction,                 (RoadTypes)GB(p1, 0, 3));
			MakeRoadTunnel(end_tile,   _current_player, ReverseDiagDir(direction), (RoadTypes)GB(p1, 0, 3));
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
		_error_message = v->type == VEH_TRAIN ?
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

static CommandCost DoClearTunnel(TileIndex tile, uint32 flags)
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
		/* We first need to request the direction before calling DoClearSquare
		 *  else the direction is always 0.. dah!! ;) */
		DiagDirection dir = GetTunnelDirection(tile);
		Track track;

		/* Adjust the town's player rating. Do this before removing the tile owner info. */
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
	return CommandCost(_price.clear_tunnel * (length + 1));
}


static bool IsVehicleOnBridge(TileIndex starttile, TileIndex endtile, uint z)
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if ((v->tile == starttile || v->tile == endtile) && v->z_pos == z) {
			_error_message = VehicleInTheWayErrMsg(v);
			return true;
		}
	}
	return false;
}

static CommandCost DoClearBridge(TileIndex tile, uint32 flags)
{
	DiagDirection direction;
	TileIndexDiff delta;
	TileIndex endtile;
	Town *t = NULL;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (!CheckAllowRemoveTunnelBridge(tile)) return CMD_ERROR;

	endtile = GetOtherBridgeEnd(tile);
	byte bridge_height = GetBridgeHeight(tile);

	if (FindVehicleOnTileZ(tile, bridge_height) != NULL ||
			FindVehicleOnTileZ(endtile, bridge_height) != NULL ||
			IsVehicleOnBridge(tile, endtile, bridge_height)) {
		return CMD_ERROR;
	}

	direction = GetBridgeRampDirection(tile);
	delta = TileOffsByDiagDir(direction);

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

		/* checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
		 * you have a "Poor" (0) town rating */
		if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);

		DoClearSquare(tile);
		DoClearSquare(endtile);
		for (c = tile + delta; c != endtile; c += delta) {
				ClearBridgeMiddle(c);
			MarkTileDirtyByTile(c);
		}

		UpdateSignalsOnSegment(tile, ReverseDiagDir(direction));
		UpdateSignalsOnSegment(endtile, direction);
		track = AxisToTrack(DiagDirToAxis(direction));
		YapfNotifyTrackLayoutChange(tile, track);
		YapfNotifyTrackLayoutChange(endtile, track);
	}

	return CommandCost((DistanceManhattan(tile, endtile) + 1) * _price.clear_bridge);
}

static CommandCost ClearTile_TunnelBridge(TileIndex tile, byte flags)
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

/**
 * Switches the rail type for a tunnel or a bridgehead. As the railtype
 * on the bridge are determined by the one of the bridgehead, this
 * functions converts the railtype on the entire bridge.
 * @param tile        The tile on which the railtype is to be convert.
 * @param totype      The railtype we want to convert to
 * @param exec        Switches between test and execute mode
 * @return            The cost and state of the operation
 * @retval CMD_ERROR  An error occured during the operation.
 */
CommandCost DoConvertTunnelBridgeRail(TileIndex tile, RailType totype, bool exec)
{
	TileIndex endtile;

	if (IsTunnel(tile) && GetTunnelTransportType(tile) == TRANSPORT_RAIL) {
		uint length;

		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		if (GetRailType(tile) == totype) return CMD_ERROR;

		/* 'hidden' elrails can't be downgraded to normal rail when elrails are disabled */
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
		return CommandCost((length + 1) * (_price.build_rail / 2));
	} else if (IsBridge(tile) && GetBridgeTransportType(tile) == TRANSPORT_RAIL) {

		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		endtile = GetOtherBridgeEnd(tile);
		byte bridge_height = GetBridgeHeight(tile);

		if (FindVehicleOnTileZ(tile, bridge_height) != NULL ||
				FindVehicleOnTileZ(endtile, bridge_height) != NULL ||
				IsVehicleOnBridge(tile, endtile, bridge_height)) {
			return CMD_ERROR;
		}

		if (GetRailType(tile) == totype) return CMD_ERROR;

		if (exec) {
			TileIndexDiff delta;
			Track track;

			SetRailType(tile, totype);
			SetRailType(endtile, totype);
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(endtile);

			track = AxisToTrack(DiagDirToAxis(GetBridgeRampDirection(tile)));
			YapfNotifyTrackLayoutChange(tile, track);
			YapfNotifyTrackLayoutChange(endtile, track);

			delta = TileOffsByDiagDir(GetBridgeRampDirection(tile));
			for (tile += delta; tile != endtile; tile += delta) {
				MarkTileDirtyByTile(tile); // TODO encapsulate this into a function
			}
		}

		return CommandCost((DistanceManhattan(tile, endtile) + 1) * (_price.build_rail / 2));
	} else {
		return CMD_ERROR;
	}
}


static void DrawBridgePillars(const PalSpriteID *psid, const TileInfo* ti, Axis axis, uint type, int x, int y, int z)
{
	SpriteID image = psid->sprite;
	if (image != 0) {
		bool drawfarpillar = !HASBIT(GetBridgeFlags(type), 0);
		int back_height, front_height;
		int i = z;
		const byte *p;
		SpriteID pal;

		static const byte _tileh_bits[4][8] = {
			{ 2, 1, 8, 4,  16,  2, 0, 9 },
			{ 1, 8, 4, 2,   2, 16, 9, 0 },
			{ 4, 8, 1, 2,  16,  2, 0, 9 },
			{ 2, 4, 8, 1,   2, 16, 9, 0 }
		};

		if (HASBIT(_transparent_opt, TO_BRIDGES)) {
			SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
			pal = PALETTE_TO_TRANSPARENT;
		} else {
			pal = psid->pal;
		}

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
				AddSortableSpriteToDraw(image, pal, x, y, p[4], p[5], 1, z);
			}

			if (drawfarpillar && z >= back_height && z < i - TILE_HEIGHT) { // back facing pillar
				AddSortableSpriteToDraw(image, pal, x - p[6], y - p[7], p[4], p[5], 1, z);
			}
		}
	}
}

uint GetBridgeFoundation(Slope tileh, Axis axis)
{
	uint i;

	if (HASBIT(BRIDGE_FULL_LEVELED_FOUNDATION, tileh)) return tileh;

	/* inclined sloped building */
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
 * Draws the trambits over an already drawn (lower end) of a bridge.
 * @param x       the x of the bridge
 * @param y       the y of the bridge
 * @param z       the z of the bridge
 * @param offset  number representing whether to level or sloped and the direction
 * @param overlay do we want to still see the road?
 */
static void DrawBridgeTramBits(int x, int y, byte z, int offset, bool overlay)
{
	static const SpriteID tram_offsets[2][6] = { { 107, 108, 109, 110, 111, 112 }, { 4, 5, 15, 16, 17, 18 } };
	static const SpriteID back_offsets[6]    =   {  95,  95,  99, 102, 100, 101 };
	static const SpriteID front_offsets[6]   =   {  97,  98, 103, 106, 104, 105 };

	static const uint size_x[6] = { 11, 16, 16, 16, 16, 16 };
	static const uint size_y[6] = { 16, 11, 16, 16, 16, 16 };

	AddSortableSpriteToDraw(SPR_TRAMWAY_BASE + tram_offsets[overlay][offset], PAL_NONE, x, y, size_x[offset], size_y[offset], offset >= 2 ? 1 : 0, z);

	SpriteID front = SPR_TRAMWAY_BASE + front_offsets[offset];
	SpriteID back  = SPR_TRAMWAY_BASE + back_offsets[offset];
	SpriteID pal   = PAL_NONE;
	if (HASBIT(_transparent_opt, TO_BUILDINGS)) {
		SETBIT(front, PALETTE_MODIFIER_TRANSPARENT);
		SETBIT(back,  PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	}

	AddSortableSpriteToDraw(back,  pal, x, y, size_x[offset], size_y[offset], 0, z);
	/* For sloped sprites the bounding box needs to be higher, as the pylons stop on a higher point */
	AddSortableSpriteToDraw(front, pal, x, y, size_x[offset], size_y[offset], offset >= 2 ? 0x30 : 0x10, z);
}

/**
 * Draws a tunnel of bridge tile.
 * For tunnels, this is rather simple, as you only needa draw the entrance.
 * Bridges are a bit more complex. base_offset is where the sprite selection comes into play
 * and it works a bit like a bitmask.<p> For bridge heads:
 * @param ti TileInfo of the structure to draw
 * <ul><li>Bit 0: direction</li>
 * <li>Bit 1: northern or southern heads</li>
 * <li>Bit 2: Set if the bridge head is sloped</li>
 * <li>Bit 3 and more: Railtype Specific subset</li>
 * </ul>
 * Please note that in this code, "roads" are treated as railtype 1, whilst the real railtypes are 0, 2 and 3
 */
static void DrawTile_TunnelBridge(TileInfo *ti)
{
	SpriteID image;
	SpriteID pal;

	if (IsTunnel(ti->tile)) {
		if (GetTunnelTransportType(ti->tile) == TRANSPORT_RAIL) {
			image = GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.tunnel;
		} else {
			image = SPR_TUNNEL_ENTRY_REAR_ROAD;
		}

		if (HasTunnelSnowOrDesert(ti->tile)) image += 32;

		image += GetTunnelDirection(ti->tile) * 2;
		DrawGroundSprite(image, PAL_NONE);
		if (GetTunnelTransportType(ti->tile) == TRANSPORT_ROAD) {
			DiagDirection dir = GetTunnelDirection(ti->tile);
			RoadTypes rts = GetRoadTypes(ti->tile);

			if (HASBIT(rts, ROADTYPE_TRAM)) {
				static const SpriteID tunnel_sprites[2][4] = { { 28, 78, 79, 27 }, {  5, 76, 77,  4 } };

				DrawGroundSprite(SPR_TRAMWAY_BASE + tunnel_sprites[rts - ROADTYPES_TRAM][dir], PAL_NONE);
				AddSortableSpriteToDraw(SPR_TRAMWAY_TUNNEL_WIRES + dir, PAL_NONE, ti->x, ti->y, 16, 16, 16, (byte)ti->z);
			}
		} else if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) {
			DrawCatenary(ti);
		}

		AddSortableSpriteToDraw(image + 1, PAL_NONE, ti->x + TILE_SIZE - 1, ti->y + TILE_SIZE - 1, 1, 1, 8, (byte)ti->z);
		DrawBridgeMiddle(ti);
	} else if (IsBridge(ti->tile)) { // XXX is this necessary?
		const PalSpriteID *psid;
		int base_offset;
		bool ice = HasBridgeSnowOrDesert(ti->tile);

		if (GetBridgeTransportType(ti->tile) == TRANSPORT_RAIL) {
			base_offset = GetRailTypeInfo(GetRailType(ti->tile))->bridge_offset;
			assert(base_offset != 8); // This one is used for roads
		} else {
			base_offset = 8;
		}

		/* as the lower 3 bits are used for other stuff, make sure they are clear */
		assert( (base_offset & 0x07) == 0x00);

		if (!HASBIT(BRIDGE_NO_FOUNDATION, ti->tileh)) {
			int f = GetBridgeFoundation(ti->tileh, DiagDirToAxis(GetBridgeRampDirection(ti->tile)));
			if (f != 0) DrawFoundation(ti, f);
		}

		/* HACK Wizardry to convert the bridge ramp direction into a sprite offset */
		base_offset += (6 - GetBridgeRampDirection(ti->tile)) % 4;

		if (ti->tileh == SLOPE_FLAT) base_offset += 4; // sloped bridge head

		/* Table number 6 always refers to the bridge heads for any bridge type */
		psid = &GetBridgeSpriteTable(GetBridgeType(ti->tile), 6)[base_offset];

		if (!ice) {
			DrawClearLandTile(ti, 3);
		} else {
			DrawGroundSprite(SPR_FLAT_SNOWY_TILE + _tileh_to_sprite[ti->tileh], PAL_NONE);
		}

		image = psid->sprite;

		/* draw ramp */
		if (HASBIT(_transparent_opt, TO_BRIDGES)) {
			SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
			pal = PALETTE_TO_TRANSPARENT;
		} else {
			pal = psid->pal;
		}

		/* HACK set the height of the BB of a sloped ramp to 1 so a vehicle on
		 * it doesn't disappear behind it
		 */
		AddSortableSpriteToDraw(
			image, pal, ti->x, ti->y, 16, 16, ti->tileh == SLOPE_FLAT ? 0 : 8, ti->z
		);

		if (GetBridgeTransportType(ti->tile) == TRANSPORT_ROAD) {
			RoadTypes rts = GetRoadTypes(ti->tile);

			if (HASBIT(rts, ROADTYPE_TRAM)) {
				uint offset = GetBridgeRampDirection(ti->tile);
				uint z = ti->z;
				if (ti->tileh != SLOPE_FLAT) {
					offset = (offset + 1) & 1;
					z += TILE_HEIGHT;
				} else {
					offset += 2;
				}
				DrawBridgeTramBits(ti->x, ti->y, z, offset, HASBIT(rts, ROADTYPE_ROAD));
			}
		} else if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) {
			DrawCatenary(ti);
		}

		DrawBridgeMiddle(ti);
	}
}


/** Compute bridge piece. Computes the bridge piece to display depending on the position inside the bridge.
 * bridges pieces sequence (middle parts)
 * bridge len 1: 0
 * bridge len 2: 0 1
 * bridge len 3: 0 4 1
 * bridge len 4: 0 2 3 1
 * bridge len 5: 0 2 5 3 1
 * bridge len 6: 0 2 3 2 3 1
 * bridge len 7: 0 2 3 4 2 3 1
 * #0 - always as first, #1 - always as last (if len>1)
 * #2,#3 are to pair in order
 * for odd bridges: #5 is going in the bridge middle if on even position, #4 on odd (counting from 0)
 * @param north Northernmost tile of bridge
 * @param south Southernmost tile of bridge
 * @return Index of bridge piece
 */
static uint CalcBridgePiece(uint north, uint south)
{
	if (north == 1) {
		return 0;
	} else if (south == 1) {
		return 1;
	} else if (north < south) {
		return north & 1 ? 3 : 2;
	} else if (north > south) {
		return south & 1 ? 2 : 3;
	} else {
		return north & 1 ? 5 : 4;
	}
}


void DrawBridgeMiddle(const TileInfo* ti)
{
	const PalSpriteID* psid;
	SpriteID image;
	SpriteID pal;
	uint base_offset;
	TileIndex rampnorth;
	TileIndex rampsouth;
	Axis axis;
	uint piece;
	uint type;
	int x;
	int y;
	uint z;

	if (!IsBridgeAbove(ti->tile)) return;

	rampnorth = GetNorthernBridgeEnd(ti->tile);
	rampsouth = GetSouthernBridgeEnd(ti->tile);

	axis = GetBridgeAxis(ti->tile);
	piece = CalcBridgePiece(
		DistanceManhattan(ti->tile, rampnorth),
		DistanceManhattan(ti->tile, rampsouth)
	);
	type = GetBridgeType(rampsouth);

	if (GetBridgeTransportType(rampsouth) == TRANSPORT_RAIL) {
		base_offset = GetRailTypeInfo(GetRailType(rampsouth))->bridge_offset;
	} else {
		base_offset = 8;
	}

	psid = base_offset + GetBridgeSpriteTable(type, piece);
	if (axis != AXIS_X) psid += 4;

	x = ti->x;
	y = ti->y;
	uint bridge_z = GetBridgeHeight(rampsouth);
	z = bridge_z - 3;

	image = psid->sprite;
	if (HASBIT(_transparent_opt, TO_BRIDGES)) {
		SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	} else {
		pal = psid->pal;
	}

	if (axis == AXIS_X) {
		AddSortableSpriteToDraw(image, pal, x, y, 16, 11, 1, z);
	} else {
		AddSortableSpriteToDraw(image, pal, x, y, 11, 16, 1, z);
	}

	psid++;
	image = psid->sprite;
	if (HASBIT(_transparent_opt, TO_BRIDGES)) {
		SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	} else {
		pal = psid->pal;
	}

	if (GetBridgeTransportType(rampsouth) == TRANSPORT_ROAD) {
		RoadTypes rts = GetRoadTypes(rampsouth);

		if (HASBIT(rts, ROADTYPE_TRAM)) {
			DrawBridgeTramBits(x, y, bridge_z, axis ^ 1, HASBIT(rts, ROADTYPE_ROAD));
		}
	} else if (GetRailType(rampsouth) == RAILTYPE_ELECTRIC) {
		DrawCatenary(ti);
	}

	/* draw roof, the component of the bridge which is logically between the vehicle and the camera */
	if (axis == AXIS_X) {
		y += 12;
		if (image & SPRITE_MASK) AddSortableSpriteToDraw(image, pal, x, y, 16, 1, 0x28, z);
	} else {
		x += 12;
		if (image & SPRITE_MASK) AddSortableSpriteToDraw(image, pal, x, y, 1, 16, 0x28, z);
	}

	psid++;
	if (ti->z + 5 == z) {
		/* draw poles below for small bridges */
		if (psid->sprite != 0) {
			image = psid->sprite;
			if (HASBIT(_transparent_opt, TO_BRIDGES)) {
				SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
				pal = PALETTE_TO_TRANSPARENT;
			} else {
				pal = psid->pal;
			}

			DrawGroundSpriteAt(image, pal, x, y, z);
		}
	} else if (_patches.bridge_pillars) {
		/* draw pillars below for high bridges */
		DrawBridgePillars(psid, ti, axis, type, x, y, z);
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

		/* In the tunnel entrance? */
		if (5 <= pos && pos <= 10) return z;
	} else {
		DiagDirection dir = GetBridgeRampDirection(tile);
		uint pos = (DiagDirToAxis(dir) == AXIS_X ? y : x);

		/* On the bridge ramp? */
		if (5 <= pos && pos <= 10) {
			uint delta;

			if (IsSteepSlope(tileh)) return z + TILE_HEIGHT * 2;
			if (HASBIT(BRIDGE_HORZ_RAMP, tileh)) return z + TILE_HEIGHT;

			if (HASBIT(BRIDGE_FULL_LEVELED_FOUNDATION, tileh)) z += TILE_HEIGHT;
			switch (dir) {
				default: NOT_REACHED();
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
				tileh = (Slope)_inclined_tileh[f - 15];
			}
		}
	}

	return z + GetPartialZ(x, y, tileh);
}

static Slope GetSlopeTileh_TunnelBridge(TileIndex tile, Slope tileh)
{
	if (IsTunnel(tile)) {
		return tileh;
	} else {
		if (HASBIT(BRIDGE_NO_FOUNDATION, tileh)) {
			return tileh;
		} else {
			uint f = GetBridgeFoundation(tileh, DiagDirToAxis(GetBridgeRampDirection(tile)));

			if (f == 0) return tileh;
			if (f < 15) return SLOPE_FLAT;
			return (Slope)_inclined_tileh[f - 15];
		}
	}
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
	}
	td->owner = GetTileOwner(tile);
}


static void AnimateTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}

static void TileLoop_TunnelBridge(TileIndex tile)
{
	bool snow_or_desert = IsTunnelTile(tile) ? HasTunnelSnowOrDesert(tile) : HasBridgeSnowOrDesert(tile);
	switch (_opt.landscape) {
		case LT_ARCTIC:
			if (snow_or_desert != (GetTileZ(tile) > GetSnowLine())) {
				if (IsTunnelTile(tile)) {
					SetTunnelSnowOrDesert(tile, !snow_or_desert);
				} else {
					SetBridgeSnowOrDesert(tile, !snow_or_desert);
				}
				MarkTileDirtyByTile(tile);
			}
			break;

		case LT_TROPIC:
			if (GetTropicZone(tile) == TROPICZONE_DESERT && !snow_or_desert) {
				if (IsTunnelTile(tile)) {
					SetTunnelSnowOrDesert(tile, true);
				} else {
					SetBridgeSnowOrDesert(tile, true);
				}
				MarkTileDirtyByTile(tile);
			}
			break;
	}
}

static void ClickTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}


static uint32 GetTileTrackStatus_TunnelBridge(TileIndex tile, TransportType mode, uint sub_mode)
{
	if (IsTunnel(tile)) {
		if (GetTunnelTransportType(tile) != mode) return 0;
		if (GetTunnelTransportType(tile) == TRANSPORT_ROAD && (GetRoadTypes(tile) & sub_mode) == 0) return 0;
		return AxisToTrackBits(DiagDirToAxis(GetTunnelDirection(tile))) * 0x101;
	} else {
		if (GetBridgeTransportType(tile) != mode) return 0;
		if (GetBridgeTransportType(tile) == TRANSPORT_ROAD && (GetRoadTypes(tile) & sub_mode) == 0) return 0;
		return AxisToTrackBits(DiagDirToAxis(GetBridgeRampDirection(tile))) * 0x101;
	}
}

static void ChangeTileOwner_TunnelBridge(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else {
		if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR))) {
			/* When clearing the bridge/tunnel failed there are still vehicles on/in
			 * the bridge/tunnel. As all *our* vehicles are already removed, they
			 * must be of another owner. Therefor this must be a road bridge/tunnel.
			 * In that case we can safely reassign the ownership to OWNER_NONE. */
			assert((IsTunnel(tile) ? GetTunnelTransportType(tile) : GetBridgeTransportType(tile)) == TRANSPORT_ROAD);
			SetTileOwner(tile, OWNER_NONE);
		}
	}
}


static const byte _tunnel_fractcoord_1[4]    = {0x8E, 0x18, 0x81, 0xE8};
static const byte _tunnel_fractcoord_2[4]    = {0x81, 0x98, 0x87, 0x38};
static const byte _tunnel_fractcoord_3[4]    = {0x82, 0x88, 0x86, 0x48};
static const byte _exit_tunnel_track[4]      = {1, 2, 1, 2};

/** Get the trackdir of the exit of a tunnel */
static const Trackdir _road_exit_tunnel_state[DIAGDIR_END] = {
	TRACKDIR_X_SW, TRACKDIR_Y_NW, TRACKDIR_X_NE, TRACKDIR_Y_SE
};
static const byte _road_exit_tunnel_frame[4] = {2, 7, 9, 4};

static const byte _tunnel_fractcoord_4[4]    = {0x52, 0x85, 0x98, 0x29};
static const byte _tunnel_fractcoord_5[4]    = {0x92, 0x89, 0x58, 0x25};
static const byte _tunnel_fractcoord_6[4]    = {0x92, 0x89, 0x56, 0x45};
static const byte _tunnel_fractcoord_7[4]    = {0x52, 0x85, 0x96, 0x49};

static uint32 VehicleEnter_TunnelBridge(Vehicle *v, TileIndex tile, int x, int y)
{
	int z = GetSlopeZ(x, y) - v->z_pos;

	if (myabs(z) > 2) return VETSB_CANNOT_ENTER;

	if (IsTunnel(tile)) {
		byte fc;
		DiagDirection dir;
		DiagDirection vdir;

		if (v->type == VEH_TRAIN) {
			fc = (x & 0xF) + (y << 4);

			dir = GetTunnelDirection(tile);
			vdir = DirToDiagDir(v->direction);

			if (v->u.rail.track != TRACK_BIT_WORMHOLE && dir == vdir) {
				if (IsFrontEngine(v) && fc == _tunnel_fractcoord_1[dir]) {
					if (!PlayVehicleSound(v, VSE_TUNNEL) && RailVehInfo(v->engine_type)->engclass == 0) {
						SndPlayVehicleFx(SND_05_TRAIN_THROUGH_TUNNEL, v);
					}
					return VETSB_CONTINUE;
				}
				if (fc == _tunnel_fractcoord_2[dir]) {
					v->tile = tile;
					v->u.rail.track = TRACK_BIT_WORMHOLE;
					v->vehstatus |= VS_HIDDEN;
					return VETSB_ENTERED_WORMHOLE;
				}
			}

			if (dir == ReverseDiagDir(vdir) && fc == _tunnel_fractcoord_3[dir] && z == 0) {
				/* We're at the tunnel exit ?? */
				v->tile = tile;
				v->u.rail.track = (TrackBits)_exit_tunnel_track[dir];
				assert(v->u.rail.track);
				v->vehstatus &= ~VS_HIDDEN;
				return VETSB_ENTERED_WORMHOLE;
			}
		} else if (v->type == VEH_ROAD) {
			fc = (x & 0xF) + (y << 4);
			dir = GetTunnelDirection(tile);
			vdir = DirToDiagDir(v->direction);

			/* Enter tunnel? */
			if (v->u.road.state != RVSB_WORMHOLE && dir == vdir) {
				if (fc == _tunnel_fractcoord_4[dir] ||
						fc == _tunnel_fractcoord_5[dir]) {
					v->tile = tile;
					v->u.road.state = RVSB_WORMHOLE;
					v->vehstatus |= VS_HIDDEN;
					return VETSB_ENTERED_WORMHOLE;
				} else {
					return VETSB_CONTINUE;
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
				return VETSB_ENTERED_WORMHOLE;
			}
		}
	} else if (IsBridge(tile)) { // XXX is this necessary?
		DiagDirection dir;

		if (v->HasFront() && v->IsPrimaryVehicle()) {
			/* modify speed of vehicle */
			uint16 spd = _bridge[GetBridgeType(tile)].speed;

			if (v->type == VEH_ROAD) spd *= 2;
			if (v->cur_speed > spd) v->cur_speed = spd;
		}

		dir = GetBridgeRampDirection(tile);
		if (DirToDiagDir(v->direction) == dir) {
			switch (dir) {
				default: NOT_REACHED();
				case DIAGDIR_NE: if ((x & 0xF) != 0)             return VETSB_CONTINUE; break;
				case DIAGDIR_SE: if ((y & 0xF) != TILE_SIZE - 1) return VETSB_CONTINUE; break;
				case DIAGDIR_SW: if ((x & 0xF) != TILE_SIZE - 1) return VETSB_CONTINUE; break;
				case DIAGDIR_NW: if ((y & 0xF) != 0)             return VETSB_CONTINUE; break;
			}
			if (v->type == VEH_TRAIN) {
				v->u.rail.track = TRACK_BIT_WORMHOLE;
				CLRBIT(v->u.rail.flags, VRF_GOINGUP);
				CLRBIT(v->u.rail.flags, VRF_GOINGDOWN);
			} else {
				v->u.road.state = RVSB_WORMHOLE;
			}
			return VETSB_ENTERED_WORMHOLE;
		} else if (DirToDiagDir(v->direction) == ReverseDiagDir(dir)) {
			v->tile = tile;
			if (v->type == VEH_TRAIN) {
				if (v->u.rail.track == TRACK_BIT_WORMHOLE) {
					v->u.rail.track = (DiagDirToAxis(dir) == AXIS_X ? TRACK_BIT_X : TRACK_BIT_Y);
					return VETSB_ENTERED_WORMHOLE;
				}
			} else {
				if (v->u.road.state == RVSB_WORMHOLE) {
					v->u.road.state = _road_exit_tunnel_state[dir];
					v->u.road.frame = 0;
					return VETSB_ENTERED_WORMHOLE;
				}
			}
			return VETSB_CONTINUE;
		}
	}
	return VETSB_CONTINUE;
}

extern const TileTypeProcs _tile_type_tunnelbridge_procs = {
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
