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
#include "tile_cmd.h"
#include "landscape.h"
#include "tunnel_map.h"
#include "unmovable_map.h"
#include "viewport_func.h"
#include "command_func.h"
#include "town.h"
#include "variables.h"
#include "bridge.h"
#include "train.h"
#include "water_map.h"
#include "yapf/yapf.h"
#include "newgrf_sound.h"
#include "autoslope.h"
#include "transparency.h"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "date_func.h"
#include "functions.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "signal_func.h"
#include "tunnelbridge.h"
#include "player_base.h"
#include "engine_func.h"
#include "engine_base.h"
#include "economy_func.h"
#include "rail.h"
#include "cheat_func.h"
#include "elrail_func.h"
#include "landscape_type.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/bridge_land.h"

BridgeSpec _bridge[MAX_BRIDGES];
TileIndex _build_tunnel_endtile;

/** Reset the data been eventually changed by the grf loaded. */
void ResetBridges()
{
	/* First, free sprite table data */
	for (BridgeType i = 0; i < MAX_BRIDGES; i++) {
		if (_bridge[i].sprite_table != NULL) {
			for (uint j = 0; j < 7; j++) free(_bridge[i].sprite_table[j]);
			free(_bridge[i].sprite_table);
		}
	}

	/* Then, wipe out current bidges */
	memset(&_bridge, 0, sizeof(_bridge));
	/* And finally, reinstall default data */
	memcpy(&_bridge, &_orig_bridge, sizeof(_orig_bridge));
}

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

Foundation GetBridgeFoundation(Slope tileh, Axis axis)
{
	if ((tileh == SLOPE_FLAT) ||
	    (((tileh == SLOPE_NE) || (tileh == SLOPE_SW)) && (axis == AXIS_X)) ||
	    (((tileh == SLOPE_NW) || (tileh == SLOPE_SE)) && (axis == AXIS_Y))) return FOUNDATION_NONE;

	return (HasSlopeHighestCorner(tileh) ? InclinedFoundation(axis) : FlatteningFoundation(tileh));
}

/**
 * Determines if the track on a bridge ramp is flat or goes up/down.
 *
 * @param tileh Slope of the tile under the bridge head
 * @param axis Orientation of bridge
 * @return true iff the track is flat.
 */
bool HasBridgeFlatRamp(Slope tileh, Axis axis)
{
	ApplyFoundationToSlope(GetBridgeFoundation(tileh, axis), &tileh);
	/* If the foundation slope is flat the bridge has a non-flat ramp and vice versa. */
	return (tileh != SLOPE_FLAT);
}

static inline const PalSpriteID *GetBridgeSpriteTable(int index, byte table)
{
	const BridgeSpec *bridge = GetBridgeSpec(index);
	assert(table < 7);
	if (bridge->sprite_table == NULL || bridge->sprite_table[table] == NULL) {
		return _bridge_sprite_table[index][table];
	} else {
		return bridge->sprite_table[table];
	}
}


/**
 * Determines the foundation for the north bridge head, and tests if the resulting slope is valid.
 *
 * @param axis Axis of the bridge
 * @param tileh Slope of the tile under the north bridge head; returns slope on top of foundation
 * @param z TileZ corresponding to tileh, gets modified as well
 * @return Error or cost for bridge foundation
 */
static CommandCost CheckBridgeSlopeNorth(Axis axis, Slope *tileh, uint *z)
{
	Foundation f = GetBridgeFoundation(*tileh, axis);
	*z += ApplyFoundationToSlope(f, tileh);

	Slope valid_inclined = (axis == AXIS_X ? SLOPE_NE : SLOPE_NW);
	if ((*tileh != SLOPE_FLAT) && (*tileh != valid_inclined)) return CMD_ERROR;

	if (f == FOUNDATION_NONE) return CommandCost();

	return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
}

/**
 * Determines the foundation for the south bridge head, and tests if the resulting slope is valid.
 *
 * @param axis Axis of the bridge
 * @param tileh Slope of the tile under the south bridge head; returns slope on top of foundation
 * @param z TileZ corresponding to tileh, gets modified as well
 * @return Error or cost for bridge foundation
 */
static CommandCost CheckBridgeSlopeSouth(Axis axis, Slope *tileh, uint *z)
{
	Foundation f = GetBridgeFoundation(*tileh, axis);
	*z += ApplyFoundationToSlope(f, tileh);

	Slope valid_inclined = (axis == AXIS_X ? SLOPE_SW : SLOPE_SE);
	if ((*tileh != SLOPE_FLAT) && (*tileh != valid_inclined)) return CMD_ERROR;

	if (f == FOUNDATION_NONE) return CommandCost();

	return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
}

bool CheckBridge_Stuff(BridgeType bridge_type, uint bridge_len)
{
	const BridgeSpec *b = GetBridgeSpec(bridge_type);
	uint max; // max possible length of a bridge (with patch 100)

	if (bridge_type >= MAX_BRIDGES) return false;
	if (b->avail_year > _cur_year) return false;

	max = b->max_length;
	if (max >= 16 && _settings.construction.longbridges) max = 100;

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
	BridgeType bridge_type;
	RailType railtype = INVALID_RAILTYPE;
	RoadTypes roadtypes = ROADTYPES_NONE;
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
	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost ret;
	bool replace_bridge = false;
	BridgeType replaced_bridge_type;
	TransportType transport_type;

	/* unpack parameters */
	bridge_type = GB(p2, 0, 8);

	if (p1 >= MapSize()) return CMD_ERROR;

	transport_type = (TransportType)GB(p2, 15, 2);

	/* type of bridge */
	switch (transport_type) {
		case TRANSPORT_ROAD:
			roadtypes = (RoadTypes)GB(p2, 8, 3);
			if (!AreValidRoadTypes(roadtypes) || !HasRoadTypesAvail(_current_player, roadtypes)) return CMD_ERROR;
			break;

		case TRANSPORT_RAIL:
			railtype = (RailType)GB(p2, 8, 8);
			if (!ValParamRailtype(railtype)) return CMD_ERROR;
			break;

		default:
			/* For now, only TRANSPORT_RAIL and TRANSPORT_ROAD are allowed.
			 * But let not this stops us for preparing the future */
			return CMD_ERROR;
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
	if (IsWaterTile(tile_start) || IsWaterTile(tile_end)) {
		return_cmd_error(STR_02A0_ENDS_OF_BRIDGE_MUST_BOTH);
	}

	tileh_start = GetTileSlope(tile_start, &z_start);
	tileh_end = GetTileSlope(tile_end, &z_end);

	CommandCost terraform_cost_north = CheckBridgeSlopeNorth(direction, &tileh_start, &z_start);
	CommandCost terraform_cost_south = CheckBridgeSlopeSouth(direction, &tileh_end,   &z_end);

	if (z_start != z_end) return_cmd_error(STR_BRIDGEHEADS_NOT_SAME_HEIGHT);

	if (IsBridgeTile(tile_start) && IsBridgeTile(tile_end) &&
			GetOtherBridgeEnd(tile_start) == tile_end &&
			GetTunnelBridgeTransportType(tile_start) == transport_type) {
		/* Replace a current bridge. */

		/* If this is a railway bridge, make sure the railtypes match. */
		if (transport_type == TRANSPORT_RAIL && GetRailType(tile_start) != railtype) {
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		}

		/* Do not replace town bridges with lower speed bridges. */
		if (!(flags & DC_QUERY_COST) && IsTileOwner(tile_start, OWNER_TOWN) &&
				GetBridgeSpec(bridge_type)->speed < GetBridgeSpec(GetBridgeType(tile_start))->speed) {
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

		bool allow_on_slopes = (!_is_old_ai_player && _settings.construction.build_on_slopes);

		/* Try and clear the start landscape */
		ret = DoCommand(tile_start, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost = ret;

		if (CmdFailed(terraform_cost_north) || (terraform_cost_north.GetCost() != 0 && !allow_on_slopes))
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
		cost.AddCost(terraform_cost_north);

		/* Try and clear the end landscape */
		ret = DoCommand(tile_end, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
		cost.AddCost(ret);

		/* false - end tile slope check */
		if (CmdFailed(terraform_cost_south) || (terraform_cost_south.GetCost() != 0 && !allow_on_slopes))
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
		cost.AddCost(terraform_cost_south);
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

		switch (transport_type) {
			case TRANSPORT_RAIL:
				MakeRailBridgeRamp(tile_start, owner, bridge_type, dir,                 railtype);
				MakeRailBridgeRamp(tile_end,   owner, bridge_type, ReverseDiagDir(dir), railtype);
				break;

			case TRANSPORT_ROAD:
				MakeRoadBridgeRamp(tile_start, owner, bridge_type, dir,                 roadtypes);
				MakeRoadBridgeRamp(tile_end,   owner, bridge_type, ReverseDiagDir(dir), roadtypes);
				break;

			default:
				NOT_REACHED();
				break;
		}
		MarkTileDirtyByTile(tile_start);
		MarkTileDirtyByTile(tile_end);
	}

	delta = (direction == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
	for (tile = tile_start + delta; tile != tile_end; tile += delta) {
		if (GetTileMaxZ(tile) > z_start) return_cmd_error(STR_BRIDGE_TOO_LOW_FOR_TERRAIN);

		if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile) && !replace_bridge) {
			/* Disallow crossing bridges for the time being */
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		}

		switch (GetTileType(tile)) {
			case MP_WATER:
				if (!EnsureNoVehicleOnGround(tile)) return_cmd_error(STR_980E_SHIP_IN_THE_WAY);
				if (!IsWater(tile) && !IsCoast(tile)) goto not_valid_below;
				break;

			case MP_RAILWAY:
				if (!IsPlainRailTile(tile)) goto not_valid_below;
				break;

			case MP_ROAD:
				if (IsRoadDepot(tile)) goto not_valid_below;
				break;

			case MP_TUNNELBRIDGE:
				if (IsTunnel(tile)) break;
				if (replace_bridge) break;
				if (direction == DiagDirToAxis(GetTunnelBridgeDirection(tile))) goto not_valid_below;
				if (z_start < GetBridgeHeight(tile)) goto not_valid_below;
				break;

			case MP_UNMOVABLE:
				if (!IsOwnedLand(tile)) goto not_valid_below;
				break;

			case MP_CLEAR:
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

	if (flags & DC_EXEC && transport_type == TRANSPORT_RAIL) {
		Track track = AxisToTrack(direction);
		AddSideToSignalBuffer(tile_start, INVALID_DIAGDIR, _current_player);
		YapfNotifyTrackLayoutChange(tile_start, track);
	}

	/* for human player that builds the bridge he gets a selection to choose from bridges (DC_QUERY_COST)
	 * It's unnecessary to execute this command every time for every bridge. So it is done only
	 * and cost is computed in "bridge_gui.c". For AI, Towns this has to be of course calculated
	 */
	if (!(flags & DC_QUERY_COST) || (IsValidPlayer(_current_player) && GetPlayer(_current_player)->is_ai)) {
		bridge_len += 2; // begin and end tiles/ramps

		if (IsValidPlayer(_current_player) && !_is_old_ai_player)
			bridge_len = CalcBridgeLenCostFactor(bridge_len);

		cost.AddCost((int64)bridge_len * _price.build_bridge * GetBridgeSpec(bridge_type)->price >> 8);
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
	TransportType transport_type = (TransportType)GB(p1, 9, 1);
	uint start_z;
	uint end_z;
	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost ret;

	_build_tunnel_endtile = 0;
	if (transport_type == TRANSPORT_RAIL) {
		if (!ValParamRailtype((RailType)p1)) return CMD_ERROR;
	} else {
		const RoadTypes rts = (RoadTypes)GB(p1, 0, 3);
		if (!AreValidRoadTypes(rts) || !HasRoadTypesAvail(_current_player, rts)) return CMD_ERROR;
	}

	start_tileh = GetTileSlope(start_tile, &start_z);
	direction = GetInclinedSlopeDirection(start_tileh);
	if (direction == INVALID_DIAGDIR) return_cmd_error(STR_500B_SITE_UNSUITABLE_FOR_TUNNEL);

	if (IsWaterTile(start_tile)) return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

	ret = DoCommand(start_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(ret)) return ret;

	/* XXX - do NOT change 'ret' in the loop, as it is used as the price
	 * for the clearing of the entrance of the tunnel. Assigning it to
	 * cost before the loop will yield different costs depending on start-
	 * position, because of increased-cost-by-length: 'cost += cost >> 3' */

	delta = TileOffsByDiagDir(direction);
	DiagDirection tunnel_in_way_dir;
	if (DiagDirToAxis(direction) == AXIS_Y) {
		tunnel_in_way_dir = (TileX(start_tile) < (MapMaxX() / 2)) ? DIAGDIR_SW : DIAGDIR_NE;
	} else {
		tunnel_in_way_dir = (TileY(start_tile) < (MapMaxX() / 2)) ? DIAGDIR_SE : DIAGDIR_NW;
	}

	end_tile = start_tile;

	/** Tile shift coeficient. Will decrease for very long tunnels to avoid exponential growth of price*/
	int tiles_coef = 3;
	/** Number of tiles from start of tunnel */
	int tiles = 0;
	/** Number of tiles at which the cost increase coefficient per tile is halved */
	int tiles_bump = 25;

	for (;;) {
		end_tile += delta;
		end_tileh = GetTileSlope(end_tile, &end_z);

		if (start_z == end_z) break;

		if (!_cheats.crossing_tunnels.value && IsTunnelInWayDir(end_tile, start_z, tunnel_in_way_dir)) {
			return_cmd_error(STR_5003_ANOTHER_TUNNEL_IN_THE_WAY);
		}

		tiles++;
		if (tiles == tiles_bump) {
			tiles_coef++;
			tiles_bump *= 2;
		}

		cost.AddCost(_price.build_tunnel);
		cost.AddCost(cost.GetCost() >> tiles_coef); // add a multiplier for longer tunnels
	}

	/* Add the cost of the entrance */
	cost.AddCost(_price.build_tunnel);
	cost.AddCost(ret);

	/* if the command fails from here on we want the end tile to be highlighted */
	_build_tunnel_endtile = end_tile;

	if (IsWaterTile(end_tile)) return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

	/* slope of end tile must be complementary to the slope of the start tile */
	if (end_tileh != ComplementSlope(start_tileh)) {
		/* Check if there is a structure on the terraformed tile. Do not add the cost, that will be done by the terraforming */
		ret = DoCommand(end_tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return_cmd_error(STR_5005_UNABLE_TO_EXCAVATE_LAND);

		ret = DoCommand(end_tile, end_tileh & start_tileh, 0, flags, CMD_TERRAFORM_LAND);
		if (CmdFailed(ret)) return_cmd_error(STR_5005_UNABLE_TO_EXCAVATE_LAND);
	} else {
		ret = DoCommand(end_tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (CmdFailed(ret)) return ret;
	}
	cost.AddCost(_price.build_tunnel);
	cost.AddCost(ret);

	if (flags & DC_EXEC) {
		if (transport_type == TRANSPORT_RAIL) {
			MakeRailTunnel(start_tile, _current_player, direction,                 (RailType)GB(p1, 0, 4));
			MakeRailTunnel(end_tile,   _current_player, ReverseDiagDir(direction), (RailType)GB(p1, 0, 4));
			AddSideToSignalBuffer(start_tile, INVALID_DIAGDIR, _current_player);
			YapfNotifyTrackLayoutChange(start_tile, DiagDirToDiagTrack(direction));
		} else {
			MakeRoadTunnel(start_tile, _current_player, direction,                 (RoadTypes)GB(p1, 0, 3));
			MakeRoadTunnel(end_tile,   _current_player, ReverseDiagDir(direction), (RoadTypes)GB(p1, 0, 3));
		}
	}

	return cost;
}


static inline bool CheckAllowRemoveTunnelBridge(TileIndex tile)
{
	/* Floods can remove anything as well as the scenario editor */
	if (_current_player == OWNER_WATER || _game_mode == GM_EDITOR) return true;
	/* Obviously if the bridge/tunnel belongs to us, or no-one, we can remove it */
	if (CheckTileOwnership(tile) || IsTileOwner(tile, OWNER_NONE)) return true;
	/* Otherwise we can only remove town-owned stuff with extra patch-settings, or cheat */
	if (IsTileOwner(tile, OWNER_TOWN) && (_settings.construction.extra_dynamite || _cheats.magic_bulldozer.value)) return true;
	return false;
}

static CommandCost DoClearTunnel(TileIndex tile, uint32 flags)
{
	Town *t = NULL;
	TileIndex endtile;

	if (!CheckAllowRemoveTunnelBridge(tile)) return CMD_ERROR;

	endtile = GetOtherTunnelEnd(tile);

	if (GetVehicleTunnelBridge(tile, endtile) != NULL) return CMD_ERROR;

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

	/* checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
	 * you have a "Poor" (0) town rating */
	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);
	}

	if (flags & DC_EXEC) {
		if (GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL) {
			/* We first need to request values before calling DoClearSquare */
			DiagDirection dir = GetTunnelBridgeDirection(tile);
			Owner owner = GetTileOwner(tile);

			DoClearSquare(tile);
			DoClearSquare(endtile);

			/* cannot use INVALID_DIAGDIR for signal update because the tunnel doesn't exist anymore */
			AddSideToSignalBuffer(tile,    ReverseDiagDir(dir), owner);
			AddSideToSignalBuffer(endtile, dir,                 owner);

			Track track = DiagDirToDiagTrack(dir);
			YapfNotifyTrackLayoutChange(tile,    track);
			YapfNotifyTrackLayoutChange(endtile, track);
		} else {
			DoClearSquare(tile);
			DoClearSquare(endtile);
		}
	}
	return CommandCost(EXPENSES_CONSTRUCTION, _price.clear_tunnel * (GetTunnelBridgeLength(tile, endtile) + 2));
}


static CommandCost DoClearBridge(TileIndex tile, uint32 flags)
{
	DiagDirection direction;
	TileIndexDiff delta;
	TileIndex endtile;
	Town *t = NULL;

	if (!CheckAllowRemoveTunnelBridge(tile)) return CMD_ERROR;

	endtile = GetOtherBridgeEnd(tile);

	if (GetVehicleTunnelBridge(tile, endtile) != NULL) return CMD_ERROR;

	direction = GetTunnelBridgeDirection(tile);
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

	/* checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
	 * you have a "Poor" (0) town rating */
	if (IsTileOwner(tile, OWNER_TOWN) && _game_mode != GM_EDITOR) {
		ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);
	}

	if (flags & DC_EXEC) {
		/* read this value before actual removal of bridge */
		bool rail = GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL;
		Owner owner = GetTileOwner(tile);
		uint height = GetBridgeHeight(tile);

		DoClearSquare(tile);
		DoClearSquare(endtile);
		for (TileIndex c = tile + delta; c != endtile; c += delta) {
			/* do not let trees appear from 'nowhere' after removing bridge */
			if (IsNormalRoadTile(c) && GetRoadside(c) == ROADSIDE_TREES) {
				uint minz = GetTileMaxZ(c) + 3 * TILE_HEIGHT;
				if (height < minz) SetRoadside(c, ROADSIDE_PAVED);
			}
			ClearBridgeMiddle(c);
			MarkTileDirtyByTile(c);
		}

		if (rail) {
			/* cannot use INVALID_DIAGDIR for signal update because the bridge doesn't exist anymore */
			AddSideToSignalBuffer(tile,    ReverseDiagDir(direction), owner);
			AddSideToSignalBuffer(endtile, direction,                 owner);

			Track track = DiagDirToDiagTrack(direction);
			YapfNotifyTrackLayoutChange(tile,    track);
			YapfNotifyTrackLayoutChange(endtile, track);
		}
	}

	return CommandCost(EXPENSES_CONSTRUCTION, (GetTunnelBridgeLength(tile, endtile) + 2) * _price.clear_bridge);
}

static CommandCost ClearTile_TunnelBridge(TileIndex tile, byte flags)
{
	if (IsTunnel(tile)) {
		if (flags & DC_AUTO) return_cmd_error(STR_5006_MUST_DEMOLISH_TUNNEL_FIRST);
		return DoClearTunnel(tile, flags);
	} else { // IsBridge(tile)
		if (flags & DC_AUTO) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);
		return DoClearBridge(tile, flags);
	}

	return CMD_ERROR;
}

/**
 * Draws the pillars under high bridges.
 *
 * @param psid Image and palette of a bridge pillar.
 * @param ti #TileInfo of current bridge-middle-tile.
 * @param axis Orientation of bridge.
 * @param type Bridge type.
 * @param x Sprite X position of front pillar.
 * @param y Sprite Y position of front pillar.
 * @param z_bridge Absolute height of bridge bottom.
 */
static void DrawBridgePillars(const PalSpriteID *psid, const TileInfo* ti, Axis axis, BridgeType type, int x, int y, int z_bridge)
{
	/* Do not draw bridge pillars if they are invisible */
	if (IsInvisibilitySet(TO_BRIDGES)) return;

	SpriteID image = psid->sprite;

	if (image != 0) {
		bool drawfarpillar = !HasBit(GetBridgeSpec(type)->flags, 0);

		/* "side" specifies the side the pillars stand on.
		 * The length of the pillars is then set to the height of the bridge over the corners of this edge.
		 *
		 *                axis==AXIS_X  axis==AXIS_Y
		 *   side==false      SW            NW
		 *   side==true       NE            SE
		 *
		 * I have no clue, why this was done this way.
		 */
		bool side = HasBit(image, 0);

		/* "dir" means the edge the pillars stand on */
		DiagDirection dir = AxisToDiagDir(axis);
		if (side != (axis == AXIS_Y)) dir = ReverseDiagDir(dir);

		/* Determine ground height under pillars */
		int front_height = ti->z;
		int back_height = ti->z;
		GetSlopeZOnEdge(ti->tileh, dir, &front_height, &back_height);

		/* x and y size of bounding-box of pillars */
		int w = (axis == AXIS_X ? 16 : 2);
		int h = (axis == AXIS_X ? 2 : 16);
		/* sprite position of back facing pillar */
		int x_back = x - (axis == AXIS_X ? 0 : 9);
		int y_back = y - (axis == AXIS_X ? 9 : 0);

		for (int cur_z = z_bridge; cur_z >= front_height || cur_z >= back_height; cur_z -= TILE_HEIGHT) {
			/* Draw front facing pillar */
			if (cur_z >= front_height) {
				AddSortableSpriteToDraw(image, psid->pal, x, y, w, h, BB_HEIGHT_UNDER_BRIDGE - 5, cur_z, IsTransparencySet(TO_BRIDGES), 0, 0, -5);
			}

			/* Draw back facing pillar, but not the highest part directly under the bridge-floor */
			if (drawfarpillar && cur_z >= back_height && cur_z < z_bridge - TILE_HEIGHT) {
				AddSortableSpriteToDraw(image, psid->pal, x_back, y_back, w, h, BB_HEIGHT_UNDER_BRIDGE - 5, cur_z, IsTransparencySet(TO_BRIDGES), 0, 0, -5);
			}
		}
	}
}

/**
 * Draws the trambits over an already drawn (lower end) of a bridge.
 * @param x       the x of the bridge
 * @param y       the y of the bridge
 * @param z       the z of the bridge
 * @param offset  number representing whether to level or sloped and the direction
 * @param overlay do we want to still see the road?
 * @param head    are we drawing bridge head?
 */
static void DrawBridgeTramBits(int x, int y, byte z, int offset, bool overlay, bool head)
{
	static const SpriteID tram_offsets[2][6] = { { 107, 108, 109, 110, 111, 112 }, { 4, 5, 15, 16, 17, 18 } };
	static const SpriteID back_offsets[6]    =   {  95,  96,  99, 102, 100, 101 };
	static const SpriteID front_offsets[6]   =   {  97,  98, 103, 106, 104, 105 };

	static const uint size_x[6] = {  1, 16, 16,  1, 16,  1 };
	static const uint size_y[6] = { 16,  1,  1, 16,  1, 16 };
	static const uint front_bb_offset_x[6] = { 15,  0,  0, 15,  0, 15 };
	static const uint front_bb_offset_y[6] = {  0, 15, 15,  0, 15,  0 };

	/* The sprites under the vehicles are drawn as SpriteCombine. StartSpriteCombine() has already been called
	 * The bounding boxes here are the same as for bridge front/roof */
	if (head || !IsInvisibilitySet(TO_BRIDGES)) {
		AddSortableSpriteToDraw(SPR_TRAMWAY_BASE + tram_offsets[overlay][offset], PAL_NONE,
			x, y, size_x[offset], size_y[offset], 0x28, z,
			!head && IsTransparencySet(TO_BRIDGES));
	}

	/* Do not draw catenary if it is set invisible */
	if (!IsInvisibilitySet(TO_CATENARY)) {
		AddSortableSpriteToDraw(SPR_TRAMWAY_BASE + back_offsets[offset], PAL_NONE,
			x, y, size_x[offset], size_y[offset], 0x28, z,
			IsTransparencySet(TO_CATENARY));
	}

	/* Start a new SpriteCombine for the front part */
	EndSpriteCombine();
	StartSpriteCombine();

	/* For sloped sprites the bounding box needs to be higher, as the pylons stop on a higher point */
	if (!IsInvisibilitySet(TO_CATENARY)) {
		AddSortableSpriteToDraw(SPR_TRAMWAY_BASE + front_offsets[offset], PAL_NONE,
			x, y, size_x[offset] + front_bb_offset_x[offset], size_y[offset] + front_bb_offset_y[offset], 0x28, z,
			IsTransparencySet(TO_CATENARY), front_bb_offset_x[offset], front_bb_offset_y[offset]);
	}
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
	TransportType transport_type = GetTunnelBridgeTransportType(ti->tile);
	DiagDirection tunnelbridge_direction = GetTunnelBridgeDirection(ti->tile);

	if (IsTunnel(ti->tile)) {
		/* Front view of tunnel bounding boxes:
		 *
		 *   122223  <- BB_Z_SEPARATOR
		 *   1    3
		 *   1    3                1,3 = empty helper BB
		 *   1    3                  2 = SpriteCombine of tunnel-roof and catenary (tram & elrail)
		 *
		 */

		static const int _tunnel_BB[4][12] = {
			/*  tunnnel-roof  |  Z-separator  | tram-catenary
			 * w  h  bb_x bb_y| x   y   w   h |bb_x bb_y w h */
			{  1,  0, -15, -14,  0, 15, 16,  1, 0, 1, 16, 15 }, // NE
			{  0,  1, -14, -15, 15,  0,  1, 16, 1, 0, 15, 16 }, // SE
			{  1,  0, -15, -14,  0, 15, 16,  1, 0, 1, 16, 15 }, // SW
			{  0,  1, -14, -15, 15,  0,  1, 16, 1, 0, 15, 16 }, // NW
		};
		const int *BB_data = _tunnel_BB[tunnelbridge_direction];

		bool catenary = false;

		if (transport_type == TRANSPORT_RAIL) {
			image = GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.tunnel;
		} else {
			image = SPR_TUNNEL_ENTRY_REAR_ROAD;
		}

		if (HasTunnelBridgeSnowOrDesert(ti->tile)) image += 32;

		image += tunnelbridge_direction * 2;
		DrawGroundSprite(image, PAL_NONE);
		if (transport_type == TRANSPORT_ROAD) {
			RoadTypes rts = GetRoadTypes(ti->tile);

			if (HasBit(rts, ROADTYPE_TRAM)) {
				static const SpriteID tunnel_sprites[2][4] = { { 28, 78, 79, 27 }, {  5, 76, 77,  4 } };

				DrawGroundSprite(SPR_TRAMWAY_BASE + tunnel_sprites[rts - ROADTYPES_TRAM][tunnelbridge_direction], PAL_NONE);

				/* Do not draw wires if they are invisible */
				if (!IsInvisibilitySet(TO_CATENARY)) {
					catenary = true;
					StartSpriteCombine();
					AddSortableSpriteToDraw(SPR_TRAMWAY_TUNNEL_WIRES + tunnelbridge_direction, PAL_NONE, ti->x, ti->y, BB_data[10], BB_data[11], TILE_HEIGHT, ti->z, IsTransparencySet(TO_CATENARY), BB_data[8], BB_data[9], BB_Z_SEPARATOR);
				}
			}
		} else if (HasCatenaryDrawn(GetRailType(ti->tile))) {
			catenary = true;
			StartSpriteCombine();
			DrawCatenaryOnTunnel(ti);
		}

		AddSortableSpriteToDraw(image + 1, PAL_NONE, ti->x + TILE_SIZE - 1, ti->y + TILE_SIZE - 1, BB_data[0], BB_data[1], TILE_HEIGHT, ti->z, false, BB_data[2], BB_data[3], BB_Z_SEPARATOR);

		if (catenary) EndSpriteCombine();

		/* Add helper BB for sprite sorting, that separate the tunnel from things beside of it */
		AddSortableSpriteToDraw(SPR_EMPTY_BOUNDING_BOX, PAL_NONE, ti->x             , ti->y             , BB_data[6], BB_data[7], TILE_HEIGHT, ti->z);
		AddSortableSpriteToDraw(SPR_EMPTY_BOUNDING_BOX, PAL_NONE, ti->x + BB_data[4], ti->y + BB_data[5], BB_data[6], BB_data[7], TILE_HEIGHT, ti->z);

		DrawBridgeMiddle(ti);
	} else { // IsBridge(ti->tile)
		const PalSpriteID *psid;
		int base_offset;
		bool ice = HasTunnelBridgeSnowOrDesert(ti->tile);

		if (transport_type == TRANSPORT_RAIL) {
			base_offset = GetRailTypeInfo(GetRailType(ti->tile))->bridge_offset;
			assert(base_offset != 8); // This one is used for roads
		} else {
			base_offset = 8;
		}

		/* as the lower 3 bits are used for other stuff, make sure they are clear */
		assert( (base_offset & 0x07) == 0x00);

		DrawFoundation(ti, GetBridgeFoundation(ti->tileh, DiagDirToAxis(tunnelbridge_direction)));

		/* HACK Wizardry to convert the bridge ramp direction into a sprite offset */
		base_offset += (6 - tunnelbridge_direction) % 4;

		if (ti->tileh == SLOPE_FLAT) base_offset += 4; // sloped bridge head

		/* Table number 6 always refers to the bridge heads for any bridge type */
		psid = &GetBridgeSpriteTable(GetBridgeType(ti->tile), 6)[base_offset];

		if (!ice) {
			DrawClearLandTile(ti, 3);
		} else {
			DrawGroundSprite(SPR_FLAT_SNOWY_TILE + _tileh_to_sprite[ti->tileh], PAL_NONE);
		}

		/* draw ramp */

		/* Draw Trambits as SpriteCombine */
		if (transport_type == TRANSPORT_ROAD) StartSpriteCombine();

		/* HACK set the height of the BB of a sloped ramp to 1 so a vehicle on
		 * it doesn't disappear behind it
		 */
		/* Bridge heads are drawn solid no matter how invisibility/transparency is set */
		AddSortableSpriteToDraw(psid->sprite, psid->pal, ti->x, ti->y, 16, 16, ti->tileh == SLOPE_FLAT ? 0 : 8, ti->z);

		if (transport_type == TRANSPORT_ROAD) {
			RoadTypes rts = GetRoadTypes(ti->tile);

			if (HasBit(rts, ROADTYPE_TRAM)) {
				uint offset = tunnelbridge_direction;
				uint z = ti->z;
				if (ti->tileh != SLOPE_FLAT) {
					offset = (offset + 1) & 1;
					z += TILE_HEIGHT;
				} else {
					offset += 2;
				}
				/* DrawBridgeTramBits() calls EndSpriteCombine() and StartSpriteCombine() */
				DrawBridgeTramBits(ti->x, ti->y, z, offset, HasBit(rts, ROADTYPE_ROAD), true);
			}
			EndSpriteCombine();
		} else if (HasCatenaryDrawn(GetRailType(ti->tile))) {
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
	/* Sectional view of bridge bounding boxes:
	 *
	 *  1           2                                1,2 = SpriteCombine of Bridge front/(back&floor) and TramCatenary
	 *  1           2                                  3 = empty helper BB
	 *  1     7     2                                4,5 = pillars under higher bridges
	 *  1 6 88888 6 2                                  6 = elrail-pylons
	 *  1 6 88888 6 2                                  7 = elrail-wire
	 *  1 6 88888 6 2  <- TILE_HEIGHT                  8 = rail-vehicle on bridge
	 *  3333333333333  <- BB_Z_SEPARATOR
	 *                 <- unused
	 *    4       5    <- BB_HEIGHT_UNDER_BRIDGE
	 *    4       5
	 *    4       5
	 *
	 */

	/* Z position of the bridge sprites relative to bridge height (downwards) */
	static const int BRIDGE_Z_START = 3;

	const PalSpriteID* psid;
	uint base_offset;
	TileIndex rampnorth;
	TileIndex rampsouth;
	TransportType transport_type;
	Axis axis;
	uint piece;
	BridgeType type;
	int x;
	int y;
	uint z;

	if (!IsBridgeAbove(ti->tile)) return;

	rampnorth = GetNorthernBridgeEnd(ti->tile);
	rampsouth = GetSouthernBridgeEnd(ti->tile);
	transport_type = GetTunnelBridgeTransportType(rampsouth);

	axis = GetBridgeAxis(ti->tile);
	piece = CalcBridgePiece(
		GetTunnelBridgeLength(ti->tile, rampnorth) + 1,
		GetTunnelBridgeLength(ti->tile, rampsouth) + 1
	);
	type = GetBridgeType(rampsouth);

	if (transport_type == TRANSPORT_RAIL) {
		base_offset = GetRailTypeInfo(GetRailType(rampsouth))->bridge_offset;
	} else {
		base_offset = 8;
	}

	psid = base_offset + GetBridgeSpriteTable(type, piece);
	if (axis != AXIS_X) psid += 4;

	x = ti->x;
	y = ti->y;
	uint bridge_z = GetBridgeHeight(rampsouth);
	z = bridge_z - BRIDGE_Z_START;

	/* Add a bounding box, that separates the bridge from things below it. */
	AddSortableSpriteToDraw(SPR_EMPTY_BOUNDING_BOX, PAL_NONE, x, y, 16, 16, 1, bridge_z - TILE_HEIGHT + BB_Z_SEPARATOR);

	/* Draw Trambits as SpriteCombine */
	if (transport_type == TRANSPORT_ROAD) StartSpriteCombine();

	/* Draw floor and far part of bridge*/
	if (!IsInvisibilitySet(TO_BRIDGES)) {
		if (axis == AXIS_X) {
			AddSortableSpriteToDraw(psid->sprite, psid->pal, x, y, 16, 1, 0x28, z, IsTransparencySet(TO_BRIDGES), 0, 0, BRIDGE_Z_START);
		} else {
			AddSortableSpriteToDraw(psid->sprite, psid->pal, x, y, 1, 16, 0x28, z, IsTransparencySet(TO_BRIDGES), 0, 0, BRIDGE_Z_START);
		}
	}

	psid++;

	if (transport_type == TRANSPORT_ROAD) {
		RoadTypes rts = GetRoadTypes(rampsouth);

		if (HasBit(rts, ROADTYPE_TRAM)) {
			/* DrawBridgeTramBits() calls EndSpriteCombine() and StartSpriteCombine() */
			DrawBridgeTramBits(x, y, bridge_z, axis ^ 1, HasBit(rts, ROADTYPE_ROAD), false);
		} else {
			EndSpriteCombine();
			StartSpriteCombine();
		}
	} else if (HasCatenaryDrawn(GetRailType(rampsouth))) {
		DrawCatenaryOnBridge(ti);
	}

	/* draw roof, the component of the bridge which is logically between the vehicle and the camera */
	if (!IsInvisibilitySet(TO_BRIDGES)) {
		if (axis == AXIS_X) {
			y += 12;
			if (psid->sprite & SPRITE_MASK) AddSortableSpriteToDraw(psid->sprite, psid->pal, x, y, 16, 4, 0x28, z, IsTransparencySet(TO_BRIDGES), 0, 3, BRIDGE_Z_START);
		} else {
			x += 12;
			if (psid->sprite & SPRITE_MASK) AddSortableSpriteToDraw(psid->sprite, psid->pal, x, y, 4, 16, 0x28, z, IsTransparencySet(TO_BRIDGES), 3, 0, BRIDGE_Z_START);
		}
	}

	/* Draw TramFront as SpriteCombine */
	if (transport_type == TRANSPORT_ROAD) EndSpriteCombine();

	/* Do not draw anything more if bridges are invisible */
	if (IsInvisibilitySet(TO_BRIDGES)) return;

	psid++;
	if (ti->z + 5 == z) {
		/* draw poles below for small bridges */
		if (psid->sprite != 0) {
			SpriteID image = psid->sprite;
			SpriteID pal   = psid->pal;
			if (IsTransparencySet(TO_BRIDGES)) {
				SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
				pal = PALETTE_TO_TRANSPARENT;
			}

			DrawGroundSpriteAt(image, pal, x, y, z);
		}
	} else if (_settings.gui.bridge_pillars) {
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
		uint pos = (DiagDirToAxis(GetTunnelBridgeDirection(tile)) == AXIS_X ? y : x);

		/* In the tunnel entrance? */
		if (5 <= pos && pos <= 10) return z;
	} else { // IsBridge(tile)
		DiagDirection dir = GetTunnelBridgeDirection(tile);
		uint pos = (DiagDirToAxis(dir) == AXIS_X ? y : x);

		z += ApplyFoundationToSlope(GetBridgeFoundation(tileh, DiagDirToAxis(dir)), &tileh);

		/* On the bridge ramp? */
		if (5 <= pos && pos <= 10) {
			uint delta;

			if (tileh != SLOPE_FLAT) return z + TILE_HEIGHT;

			switch (dir) {
				default: NOT_REACHED();
				case DIAGDIR_NE: delta = (TILE_SIZE - 1 - x) / 2; break;
				case DIAGDIR_SE: delta = y / 2; break;
				case DIAGDIR_SW: delta = x / 2; break;
				case DIAGDIR_NW: delta = (TILE_SIZE - 1 - y) / 2; break;
			}
			return z + 1 + delta;
		}
	}

	return z + GetPartialZ(x, y, tileh);
}

static Foundation GetFoundation_TunnelBridge(TileIndex tile, Slope tileh)
{
	return IsTunnel(tile) ? FOUNDATION_NONE : GetBridgeFoundation(tileh, DiagDirToAxis(GetTunnelBridgeDirection(tile)));
}


static void GetAcceptedCargo_TunnelBridge(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_TunnelBridge(TileIndex tile, TileDesc *td)
{
	if (IsTunnel(tile)) {
		td->str = (GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL) ?
			STR_5017_RAILROAD_TUNNEL : STR_5018_ROAD_TUNNEL;
	} else { //so it must be a bridge
		td->str = GetBridgeSpec(GetBridgeType(tile))->transport_name[GetTunnelBridgeTransportType(tile)];
	}
	td->owner[0] = GetTileOwner(tile);
}


static void AnimateTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}

static void TileLoop_TunnelBridge(TileIndex tile)
{
	bool snow_or_desert = HasTunnelBridgeSnowOrDesert(tile);
	switch (_opt.landscape) {
		case LT_ARCTIC:
			if (snow_or_desert != (GetTileZ(tile) > GetSnowLine())) {
				SetTunnelBridgeSnowOrDesert(tile, !snow_or_desert);
				MarkTileDirtyByTile(tile);
			}
			break;

		case LT_TROPIC:
			if (GetTropicZone(tile) == TROPICZONE_DESERT && !snow_or_desert) {
				SetTunnelBridgeSnowOrDesert(tile, true);
				MarkTileDirtyByTile(tile);
			}
			break;

		default:
			break;
	}
}

static void ClickTile_TunnelBridge(TileIndex tile)
{
	/* not used */
}


static TrackStatus GetTileTrackStatus_TunnelBridge(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	TransportType transport_type = GetTunnelBridgeTransportType(tile);
	if (transport_type != mode || (transport_type == TRANSPORT_ROAD && (GetRoadTypes(tile) & sub_mode) == 0)) return 0;

	DiagDirection dir = GetTunnelBridgeDirection(tile);
	if (side != INVALID_DIAGDIR && side != ReverseDiagDir(dir)) return 0;
	return CombineTrackStatus(TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir)), TRACKDIR_BIT_NONE);
}

static void ChangeTileOwner_TunnelBridge(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (!IsTileOwner(tile, old_player)) return;

	if (new_player != PLAYER_SPECTATOR) {
		SetTileOwner(tile, new_player);
	} else {
		if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR))) {
			/* When clearing the bridge/tunnel failed there are still vehicles on/in
			 * the bridge/tunnel. As all *our* vehicles are already removed, they
			 * must be of another owner. Therefor this must be a road bridge/tunnel.
			 * In that case we can safely reassign the ownership to OWNER_NONE. */
			assert(GetTunnelBridgeTransportType(tile) == TRANSPORT_ROAD);
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

static VehicleEnterTileStatus VehicleEnter_TunnelBridge(Vehicle *v, TileIndex tile, int x, int y)
{
	int z = GetSlopeZ(x, y) - v->z_pos;

	if (abs(z) > 2) return VETSB_CANNOT_ENTER;
	const DiagDirection dir = GetTunnelBridgeDirection(tile);

	if (IsTunnel(tile)) {
		byte fc;
		DiagDirection vdir;

		if (v->type == VEH_TRAIN) {
			fc = (x & 0xF) + (y << 4);

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
	} else { // IsBridge(tile)

		if (v->IsPrimaryVehicle()) {
			/* modify speed of vehicle */
			uint16 spd = GetBridgeSpec(GetBridgeType(tile))->speed;

			if (v->type == VEH_ROAD) spd *= 2;
			if (v->cur_speed > spd) v->cur_speed = spd;
		}

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
				ClrBit(v->u.rail.flags, VRF_GOINGUP);
				ClrBit(v->u.rail.flags, VRF_GOINGDOWN);
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

static CommandCost TerraformTile_TunnelBridge(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	if (_settings.construction.build_on_slopes && AutoslopeEnabled() && IsBridge(tile)) {
		DiagDirection direction = GetTunnelBridgeDirection(tile);
		Axis axis = DiagDirToAxis(direction);
		CommandCost res;
		uint z_old;
		Slope tileh_old = GetTileSlope(tile, &z_old);

		/* Check if new slope is valid for bridges in general (so we can savely call GetBridgeFoundation()) */
		if ((direction == DIAGDIR_NW) || (direction == DIAGDIR_NE)) {
			CheckBridgeSlopeSouth(axis, &tileh_old, &z_old);
			res = CheckBridgeSlopeSouth(axis, &tileh_new, &z_new);
		} else {
			CheckBridgeSlopeNorth(axis, &tileh_old, &z_old);
			res = CheckBridgeSlopeNorth(axis, &tileh_new, &z_new);
		}

		/* Surface slope is valid and remains unchanged? */
		if (!CmdFailed(res) && (z_old == z_new) && (tileh_old == tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
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
	GetFoundation_TunnelBridge,      /* get_foundation_proc */
	TerraformTile_TunnelBridge,      /* terraform_tile_proc */
};
