/* $Id$ */

/** @file road_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "bridge.h"
#include "cmd_helper.h"
#include "rail_map.h"
#include "road_map.h"
#include "sprite.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "landscape.h"
#include "tile.h"
#include "town_map.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "player.h"
#include "town.h"
#include "gfx.h"
#include "sound.h"
#include "yapf/yapf.h"
#include "depot.h"
#include "newgrf.h"
#include "station_map.h"
#include "tunnel_map.h"


static uint CountRoadBits(RoadBits r)
{
	uint count = 0;

	if (r & ROAD_NW) ++count;
	if (r & ROAD_SW) ++count;
	if (r & ROAD_SE) ++count;
	if (r & ROAD_NE) ++count;
	return count;
}


bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, bool *edge_road, RoadType rt)
{
	RoadBits present;
	RoadBits n;
	*edge_road = true;

	if (_game_mode == GM_EDITOR || remove == ROAD_NONE) return true;

	/* Only do the special processing for actual players. */
	if (rt == ROADTYPE_ROAD && !IsValidPlayer(_current_player)) return true;

	/* Only do the special processing if the road is owned
	 * by a town */
	if (owner != OWNER_TOWN) return (owner == OWNER_NONE) || CheckOwnership(owner);

	if (_cheats.magic_bulldozer.value) return true;

	/* Get a bitmask of which neighbouring roads has a tile */
	n = ROAD_NONE;
	present = GetAnyRoadBits(tile, rt);
	if (present & ROAD_NE && GetAnyRoadBits(TILE_ADDXY(tile, -1,  0), rt) & ROAD_SW) n |= ROAD_NE;
	if (present & ROAD_SE && GetAnyRoadBits(TILE_ADDXY(tile,  0,  1), rt) & ROAD_NW) n |= ROAD_SE;
	if (present & ROAD_SW && GetAnyRoadBits(TILE_ADDXY(tile,  1,  0), rt) & ROAD_NE) n |= ROAD_SW;
	if (present & ROAD_NW && GetAnyRoadBits(TILE_ADDXY(tile,  0, -1), rt) & ROAD_SE) n |= ROAD_NW;

	/* If 0 or 1 bits are set in n, or if no bits that match the bits to remove,
	 * then allow it */
	if ((n & (n - 1)) != 0 && (n & remove) != 0) {
		Town *t;
		*edge_road = false;
		/* you can remove all kind of roads with extra dynamite */
		if (_patches.extra_dynamite) return true;

		t = ClosestTownFromTile(tile, (uint)-1);

		SetDParam(0, t->index);
		_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
		return false;
	}

	return true;
}

static bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, bool *edge_road, RoadType rt)
{
	return CheckAllowRemoveRoad(tile, remove, GetRoadOwner(tile, rt), edge_road, rt);
}

/** Delete a piece of road.
 * @param tile tile where to remove road from
 * @param flags operation to perform
 * @param p1 bit 0..3 road pieces to remove (RoadBits)
 *           bit 4..5 road type
 * @param p2 unused
 */
int32 CmdRemoveRoad(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* cost for removing inner/edge -roads */
	static const uint16 road_remove_cost[2] = {50, 18};

	Town *t;
	/* true if the roadpiece was always removeable,
	 * false if it was a center piece. Affects town ratings drop */
	bool edge_road;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	RoadType rt = (RoadType)GB(p1, 4, 2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

	Owner owner;
	switch (GetTileType(tile)) {
		case MP_STREET:
			owner = GetRoadOwner(tile, rt);
			break;

		case MP_STATION:
			if (!IsDriveThroughStopTile(tile)) return CMD_ERROR;
			owner = GetTileOwner(tile);
			break;

		case MP_TUNNELBRIDGE:
			if ((IsTunnel(tile) && GetTunnelTransportType(tile) != TRANSPORT_ROAD) ||
					(IsBridge(tile) && GetBridgeTransportType(tile) != TRANSPORT_ROAD)) return CMD_ERROR;
			owner = GetTileOwner(tile);
			break;

		default:
			return CMD_ERROR;
	}

	if (owner == OWNER_TOWN && _game_mode != GM_EDITOR) {
		t = GetTownByTile(tile);
	} else {
		t = NULL;
	}

	RoadBits pieces = Extract<RoadBits, 0>(p1);
	RoadTypes rts = GetRoadTypes(tile);
	/* The tile doesn't have the given road type */
	if (!HASBIT(rts, rt)) return CMD_ERROR;

	if (!CheckAllowRemoveRoad(tile, pieces, &edge_road, rt)) return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	/* check if you're allowed to remove the street owned by a town
	 * removal allowance depends on difficulty setting */
	if (!CheckforTownRating(flags, t, ROAD_REMOVE)) return CMD_ERROR;

	if (!IsTileType(tile, MP_STREET)) {
		/* If it's the last roadtype, just clear the whole tile */
		if (rts == RoadTypeToRoadTypes(rt)) return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);

		int32 cost;
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			TileIndex other_end = IsTunnel(tile) ? GetOtherTunnelEnd(tile) : GetOtherBridgeEnd(tile);
			/* Pay for *every* tile of the bridge or tunnel */
			cost = (DistanceManhattan(IsTunnel(tile) ? GetOtherTunnelEnd(tile) : GetOtherBridgeEnd(tile), tile) + 1) * _price.remove_road;
			if (flags & DC_EXEC) {
				SetRoadTypes(other_end, GetRoadTypes(other_end) & ~RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) & ~RoadTypeToRoadTypes(rt));

				/* Mark tiles diry that have been repaved */
				MarkTileDirtyByTile(other_end);
				if (IsBridge(tile)) {
					TileIndexDiff delta = TileOffsByDiagDir(GetBridgeRampDirection(tile));

					for (TileIndex t = tile; tile != other_end; tile += delta) MarkTileDirtyByTile(t);
				}
			}
		} else {
			cost = _price.remove_road;
			if (flags & DC_EXEC) {
				SetRoadTypes(tile, GetRoadTypes(tile) & ~RoadTypeToRoadTypes(rt));
				MarkTileDirtyByTile(tile);
			}
		}
		return cost;
	}

	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_NORMAL: {
			RoadBits present = GetRoadBits(tile, rt);
			RoadBits c = pieces;

			if (HasRoadWorks(tile)) return_cmd_error(STR_ROAD_WORKS_IN_PROGRESS);

			if (GetTileSlope(tile, NULL) != SLOPE_FLAT  &&
					(present == ROAD_Y || present == ROAD_X)) {
				c |= (RoadBits)((c & 0xC) >> 2);
				c |= (RoadBits)((c & 0x3) << 2);
			}

			/* limit the bits to delete to the existing bits. */
			c &= present;
			if (c == ROAD_NONE) return CMD_ERROR;

			if (flags & DC_EXEC) {
				ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);

				present ^= c;
				if (present == ROAD_NONE) {
					RoadTypes rts = GetRoadTypes(tile) & ComplementRoadTypes(RoadTypeToRoadTypes(rt));
					if (rts == ROADTYPES_NONE) {
						DoClearSquare(tile);
					} else {
						SetRoadBits(tile, ROAD_NONE, rt);
						SetRoadTypes(tile, rts);
					}
				} else {
					SetRoadBits(tile, present, rt);
					MarkTileDirtyByTile(tile);
				}
			}
			return CountRoadBits(c) * _price.remove_road;
		}

		case ROAD_TILE_CROSSING: {
			if (pieces & ComplementRoadBits(GetCrossingRoadBits(tile))) {
				return CMD_ERROR;
			}

			/* Don't allow road to be removed from the crossing when there is tram;
			 * we can't draw the crossing without trambits ;) */
			if (rt == ROADTYPE_ROAD && HASBIT(GetRoadTypes(tile), ROADTYPE_TRAM)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				if (rt == ROADTYPE_ROAD) {
					ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);
				}

				RoadTypes rts = GetRoadTypes(tile) & ComplementRoadTypes(RoadTypeToRoadTypes(rt));
				if (rts == ROADTYPES_NONE) {
					MakeRailNormal(tile, GetTileOwner(tile), GetCrossingRailBits(tile), GetRailType(tile));
				} else {
					SetRoadTypes(tile, rts);
				}
				MarkTileDirtyByTile(tile);
				YapfNotifyTrackLayoutChange(tile, FindFirstTrack(GetTrackBits(tile)));
			}
			return _price.remove_road * 2;
		}

		default:
		case ROAD_TILE_DEPOT:
			return CMD_ERROR;
	}
}


static const RoadBits _valid_tileh_slopes_road[][15] = {
	/* set of normal ones */
	{
		ROAD_ALL, ROAD_NONE, ROAD_NONE,
		ROAD_X,   ROAD_NONE, ROAD_NONE,  // 3, 4, 5
		ROAD_Y,   ROAD_NONE, ROAD_NONE,
		ROAD_Y,   ROAD_NONE, ROAD_NONE,  // 9, 10, 11
		ROAD_X,   ROAD_NONE, ROAD_NONE
	},
	/* allowed road for an evenly raised platform */
	{
		ROAD_NONE,
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


static uint32 CheckRoadSlope(Slope tileh, RoadBits* pieces, RoadBits existing)
{
	RoadBits road_bits;

	if (IsSteepSlope(tileh)) {
		if (existing == 0) {
			/* force full pieces. */
			*pieces |= (RoadBits)((*pieces & 0xC) >> 2);
			*pieces |= (RoadBits)((*pieces & 0x3) << 2);
			if (*pieces == ROAD_X || *pieces == ROAD_Y) return _price.terraform;
		}
		return CMD_ERROR;
	}
	road_bits = *pieces | existing;

	/* no special foundation */
	if ((~_valid_tileh_slopes_road[0][tileh] & road_bits) == 0) {
		/* force that all bits are set when we have slopes */
		if (tileh != SLOPE_FLAT) *pieces |= _valid_tileh_slopes_road[0][tileh];
		return 0; // no extra cost
	}

	/* foundation is used. Whole tile is leveled up */
	if ((~_valid_tileh_slopes_road[1][tileh] & road_bits) == 0) {
		return existing != 0 ? 0 : _price.terraform;
	}

	/* partly leveled up tile, only if there's no road on that tile */
	if (existing == 0 && (tileh == SLOPE_W || tileh == SLOPE_S || tileh == SLOPE_E || tileh == SLOPE_N)) {
		/* force full pieces. */
		*pieces |= (RoadBits)((*pieces & 0xC) >> 2);
		*pieces |= (RoadBits)((*pieces & 0x3) << 2);
		if (*pieces == ROAD_X || *pieces == ROAD_Y) return _price.terraform;
	}
	return CMD_ERROR;
}

/** Build a piece of road.
 * @param tile tile where to build road
 * @param flags operation to perform
 * @param p1 bit 0..3 road pieces to build (RoadBits)
 *           bit 4..5 road type
 * @param p2 the town that is building the road (0 if not applicable)
 */
int32 CmdBuildRoad(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost = 0;
	int32 ret;
	RoadBits existing = ROAD_NONE;
	Slope tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-player is building the road */
	if ((IsValidPlayer(_current_player) && p2 != 0) || (_current_player == OWNER_TOWN && !IsValidTownID(p2))) return CMD_ERROR;

	RoadBits pieces = Extract<RoadBits, 0>(p1);

	RoadType rt = (RoadType)GB(p1, 4, 2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);

	switch (GetTileType(tile)) {
		case MP_STREET:
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL:
					if (HasRoadWorks(tile)) return_cmd_error(STR_ROAD_WORKS_IN_PROGRESS);
					if (!HASBIT(GetRoadTypes(tile), rt)) break;

					existing = GetRoadBits(tile, rt);
					if ((existing & pieces) == pieces) {
						return_cmd_error(STR_1007_ALREADY_BUILT);
					}
					if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;
					break;

				case ROAD_TILE_CROSSING:
					if (HASBIT(GetRoadTypes(tile), rt)) return_cmd_error(STR_1007_ALREADY_BUILT);
					if (pieces & ComplementRoadBits(GetCrossingRoadBits(tile))) goto do_clear;
					break;

				default:
				case ROAD_TILE_DEPOT:
					goto do_clear;
			}
			break;

		case MP_RAILWAY: {
			Axis roaddir;

			if (IsSteepSlope(tileh)) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}

#define M(x) (1 << (x))
			/* Level crossings may only be built on these slopes */
			if (!HASBIT(M(SLOPE_SEN) | M(SLOPE_ENW) | M(SLOPE_NWS) | M(SLOPE_NS) | M(SLOPE_WSE) | M(SLOPE_EW) | M(SLOPE_FLAT), tileh)) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}
#undef M

			if (GetRailTileType(tile) != RAIL_TILE_NORMAL) goto do_clear;
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

			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				YapfNotifyTrackLayoutChange(tile, FindFirstTrack(GetTrackBits(tile)));
				/* Always add road to the roadtypes (can't draw without it) */
				MakeRoadCrossing(tile, _current_player, _current_player, _current_player, GetTileOwner(tile), roaddir, GetRailType(tile), RoadTypeToRoadTypes(rt) | ROADTYPES_ROAD, p2);
				MarkTileDirtyByTile(tile);
			}
			return _price.build_road * (rt == ROADTYPE_ROAD ? 2 : 4);
		}

		case MP_STATION:
			if (!IsDriveThroughStopTile(tile)) return CMD_ERROR;
			if (HASBIT(GetRoadTypes(tile), rt)) return_cmd_error(STR_1007_ALREADY_BUILT);
			break;

		case MP_TUNNELBRIDGE:
			if ((IsTunnel(tile) && GetTunnelTransportType(tile) != TRANSPORT_ROAD) ||
					(IsBridge(tile) && GetBridgeTransportType(tile) != TRANSPORT_ROAD)) return CMD_ERROR;
			if (HASBIT(GetRoadTypes(tile), rt)) return_cmd_error(STR_1007_ALREADY_BUILT);
			break;

		default:
do_clear:;
			ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost += ret;
	}

	ret = CheckRoadSlope(tileh, &pieces, existing);
	/* Return an error if we need to build a foundation (ret != 0) but the
	 * current patch-setting is turned off (or stupid AI@work) */
	if (CmdFailed(ret) || (ret != 0 && (!_patches.build_on_slopes || _is_old_ai_player)))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	cost += ret;

	if (IsTileType(tile, MP_STREET)) {
		/* Don't put the pieces that already exist */
		pieces &= ComplementRoadBits(existing);
	}

	cost += CountRoadBits(pieces) * _price.build_road;
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* Pay for *every* tile of the bridge or tunnel */
		cost *= DistanceManhattan(IsTunnel(tile) ? GetOtherTunnelEnd(tile) : GetOtherBridgeEnd(tile), tile);
	}

	if (flags & DC_EXEC) {
		switch (GetTileType(tile)) {
			case MP_STREET: {
				RoadTileType rtt = GetRoadTileType(tile);
				if (existing == ROAD_NONE || rtt == ROAD_TILE_CROSSING) {
					SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
					SetRoadOwner(tile, rt, _current_player);
				}
				if (rtt != ROAD_TILE_CROSSING) SetRoadBits(tile, existing | pieces, rt);
			} break;

			case MP_TUNNELBRIDGE: {
				TileIndex other_end = IsTunnel(tile) ? GetOtherTunnelEnd(tile) : GetOtherBridgeEnd(tile);

				SetRoadTypes(other_end, GetRoadTypes(other_end) | RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));

				/* Mark tiles diry that have been repaved */
				MarkTileDirtyByTile(other_end);
				if (IsBridge(tile)) {
					TileIndexDiff delta = TileOffsByDiagDir(GetBridgeRampDirection(tile));

					for (TileIndex t = tile + delta; tile != other_end; tile += delta) MarkTileDirtyByTile(t);
				}
			} break;

			case MP_STATION:
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
				break;

			default:
				MakeRoadNormal(tile, pieces, RoadTypeToRoadTypes(rt), p2, _current_player, _current_player, _current_player);
				break;
		}

		MarkTileDirtyByTile(tile);
	}
	return cost;
}

/**
 * Switches the rail type on a level crossing.
 * @param tile        The tile on which the railtype is to be convert.
 * @param totype      The railtype we want to convert to
 * @param exec        Switches between test and execute mode
 * @return            The cost and state of the operation
 * @retval CMD_ERROR  An error occured during the operation.
 */
int32 DoConvertStreetRail(TileIndex tile, RailType totype, bool exec)
{
	/* not a railroad crossing? */
	if (!IsLevelCrossing(tile)) return CMD_ERROR;

	/* not owned by me? */
	if (!CheckTileOwnership(tile) || !EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (GetRailType(tile) == totype) return CMD_ERROR;

	/* 'hidden' elrails can't be downgraded to normal rail when elrails are disabled */
	if (_patches.disable_elrails && totype == RAILTYPE_RAIL && GetRailType(tile) == RAILTYPE_ELECTRIC) return CMD_ERROR;

	if (exec) {
		SetRailType(tile, totype);
		MarkTileDirtyByTile(tile);
		YapfNotifyTrackLayoutChange(tile, FindFirstTrack(GetCrossingRailBits(tile)));
	}

	return _price.build_rail / 2;
}


/** Build a long piece of road.
 * @param end_tile end tile of drag
 * @param flags operation to perform
 * @param p1 start tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 + 4) - road type
 */
int32 CmdBuildLongRoad(TileIndex end_tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start_tile, tile;
	int32 cost, ret;
	bool had_bridge = false;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 >= MapSize()) return CMD_ERROR;

	start_tile = p1;
	RoadType rt = (RoadType)GB(p2, 3, 2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

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
	/* Start tile is the small number. */
	for (;;) {
		RoadBits bits = HASBIT(p2, 2) ? ROAD_Y : ROAD_X;

		if (tile == end_tile && !HASBIT(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HASBIT(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		ret = DoCommand(tile, rt << 4 | bits, 0, flags, CMD_BUILD_ROAD);
		if (CmdFailed(ret)) {
			if (_error_message != STR_1007_ALREADY_BUILT) return CMD_ERROR;
			_error_message = INVALID_STRING_ID;
		} else {
			/* Only pay for the upgrade on one side of the bridge */
			if (IsBridgeTile(tile)) {
				if ((!had_bridge || GetBridgeRampDirection(tile) == DIAGDIR_SE || GetBridgeRampDirection(tile) == DIAGDIR_SW)) {
					cost += ret;
				}
				had_bridge = true;
			} else {
				cost += ret;
			}
		}

		if (tile == end_tile) break;

		tile += HASBIT(p2, 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return (cost == 0) ? CMD_ERROR : cost;
}

/** Remove a long piece of road.
 * @param end_tile end tile of drag
 * @param flags operation to perform
 * @param p1 start tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 + 4) - road type
 */
int32 CmdRemoveLongRoad(TileIndex end_tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start_tile, tile;
	int32 cost, ret;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	if (p1 >= MapSize()) return CMD_ERROR;

	start_tile = p1;
	RoadType rt = (RoadType)GB(p2, 3, 2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

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
	/* Start tile is the small number. */
	for (;;) {
		RoadBits bits = HASBIT(p2, 2) ? ROAD_Y : ROAD_X;

		if (tile == end_tile && !HASBIT(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HASBIT(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		/* try to remove the halves. */
		if (bits != 0) {
			ret = DoCommand(tile, rt << 4 | bits, 0, flags, CMD_REMOVE_ROAD);
			if (!CmdFailed(ret)) cost += ret;
		}

		if (tile == end_tile) break;

		tile += HASBIT(p2, 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return (cost == 0) ? CMD_ERROR : cost;
}

/** Build a road depot.
 * @param tile tile where to build the depot
 * @param flags operation to perform
 * @param p1 bit 0..1 entrance direction (DiagDirection)
 *           bit 2..3 road type
 * @param p2 unused
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
int32 CmdBuildRoadDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	Depot *dep;
	Slope tileh;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	DiagDirection dir = Extract<DiagDirection, 0>(p1);
	RoadType rt = (RoadType)GB(p1, 2, 2);

	if (!IsValidRoadType(rt)) return CMD_ERROR;

	tileh = GetTileSlope(tile, NULL);
	if (tileh != SLOPE_FLAT && (
				!_patches.build_on_slopes ||
				IsSteepSlope(tileh) ||
				!CanBuildDepotByTileh(dir, tileh)
			)) {
		return_cmd_error(STR_0007_FLAT_LAND_REQUIRED);
	}

	cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(cost)) return CMD_ERROR;

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

	dep = AllocateDepot();
	if (dep == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		dep->xy = tile;
		dep->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		MakeRoadDepot(tile, _current_player, dir, rt);
		MarkTileDirtyByTile(tile);
	}
	return cost + _price.build_road_depot;
}

static int32 RemoveRoadDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) DeleteDepot(GetDepotByTile(tile));

	return _price.remove_road_depot;
}

static int32 ClearTile_Road(TileIndex tile, byte flags)
{
	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_NORMAL: {
			RoadBits b = GetAllRoadBits(tile);

#define M(x) (1 << (x))
			/* Clear the road if only one piece is on the tile OR the AI tries
			 * to clear town road OR we are not using the DC_AUTO flag */
			if ((M(b) & (M(ROAD_NW) | M(ROAD_SW) | M(ROAD_SE) | M(ROAD_NE))) ||
			    ((flags & DC_AI_BUILDING) && IsTileOwner(tile, OWNER_TOWN)) ||
			    !(flags & DC_AUTO)
				) {
				RoadTypes rts = GetRoadTypes(tile);
				int32 ret = 0;
				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					if (HASBIT(rts, rt)) {
						int32 tmp_ret = DoCommand(tile, rt << 4 | GetRoadBits(tile, rt), 0, flags, CMD_REMOVE_ROAD);
						if (CmdFailed(tmp_ret)) return tmp_ret;
						ret += rt;
					}
				}
				return ret;
			}
			return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);
		}
#undef M

		case ROAD_TILE_CROSSING: {
			int32 ret;

			if (flags & DC_AUTO) return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);

			ret = DoCommand(tile, GetCrossingRoadBits(tile), 0, flags, CMD_REMOVE_ROAD);
			if (CmdFailed(ret)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
			return ret;
		}

		default:
		case ROAD_TILE_DEPOT:
			if (flags & DC_AUTO) {
				return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
			}
			return RemoveRoadDepot(tile, flags);
	}
}


struct DrawRoadTileStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
};

#include "table/road_land.h"


uint GetRoadFoundation(Slope tileh, RoadBits bits)
{
	uint i;

	/* normal level sloped building */
	if (!IsSteepSlope(tileh) &&
			(~_valid_tileh_slopes_road[1][tileh] & bits) == 0) {
		return tileh;
	}

	/* inclined sloped building */
	switch (bits) {
		case ROAD_X: i = 0; break;
		case ROAD_Y: i = 1; break;
		default:     return 0;
	}
	switch (tileh) {
		case SLOPE_W:
		case SLOPE_STEEP_W: i += 0; break;
		case SLOPE_S:
		case SLOPE_STEEP_S: i += 2; break;
		case SLOPE_E:
		case SLOPE_STEEP_E: i += 4; break;
		case SLOPE_N:
		case SLOPE_STEEP_N: i += 6; break;
		default: return 0;
	}
	return i + 15;
}

const byte _road_sloped_sprites[14] = {
	0,  0,  2,  0,
	0,  1,  0,  0,
	3,  0,  0,  0,
	0,  0
};

/**
 * Whether to draw unpaved roads regardless of the town zone.
 * By default, OpenTTD always draws roads as unpaved if they are on a desert
 * tile or above the snowline. Newgrf files, however, can set a bit that allows
 * paved roads to be built on desert tiles as they would be on grassy tiles.
 *
 * @param tile The tile the road is on
 * @param roadside What sort of road this is
 * @return True if the road should be drawn unpaved regardless of the roadside.
 */
static bool AlwaysDrawUnpavedRoads(TileIndex tile, Roadside roadside)
{
	return (IsOnSnow(tile) &&
			!(_opt.landscape == LT_TROPIC && HasGrfMiscBit(GMB_DESERT_PAVED_ROADS) &&
				roadside != ROADSIDE_BARREN && roadside != ROADSIDE_GRASS && roadside != ROADSIDE_GRASS_ROAD_WORKS));
}

/**
 * Draws the catenary for the given tile
 * @param ti   information about the tile (slopes, height etc)
 * @param tram the roadbits for the tram
 */
void DrawTramCatenary(TileInfo *ti, RoadBits tram)
{
	/* Don't draw the catenary under a low bridge */
	if (MayHaveBridgeAbove(ti->tile) && IsBridgeAbove(ti->tile) && !HASBIT(_transparent_opt, TO_BUILDINGS)) {
		uint height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));

		if (height <= GetTileMaxZ(ti->tile) + TILE_HEIGHT) return;
	}

	SpriteID front;
	SpriteID back;

	if (ti->tileh != SLOPE_FLAT) {
		back  = SPR_TRAMWAY_BACK_WIRES_SLOPED  + _road_sloped_sprites[ti->tileh - 1];
		front = SPR_TRAMWAY_FRONT_WIRES_SLOPED + _road_sloped_sprites[ti->tileh - 1];
	} else {
		back  = SPR_TRAMWAY_BASE + _road_backpole_sprites_1[tram];
		front = SPR_TRAMWAY_BASE + _road_frontwire_sprites_1[tram];
	}

	SpriteID pal = PAL_NONE;
	if (HASBIT(_transparent_opt, TO_BUILDINGS)) {
		SETBIT(front, PALETTE_MODIFIER_TRANSPARENT);
		SETBIT(back,  PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	}

	AddSortableSpriteToDraw(back,  pal, ti->x, ti->y, 16, 16, 0x1F, ti->z);
	AddSortableSpriteToDraw(front, pal, ti->x, ti->y, 16, 16, 0x1F, ti->z);
}

/**
 * Draw ground sprite and road pieces
 * @param ti TileInfo
 */
static void DrawRoadBits(TileInfo* ti)
{
	RoadBits road = GetRoadBits(ti->tile, ROADTYPE_ROAD);
	RoadBits tram = GetRoadBits(ti->tile, ROADTYPE_TRAM);

	const DrawRoadTileStruct *drts;
	SpriteID image = 0;
	SpriteID pal = PAL_NONE;
	Roadside roadside;

	if (ti->tileh != SLOPE_FLAT) {
		int foundation = GetRoadFoundation(ti->tileh, road | tram);

		if (foundation != 0) DrawFoundation(ti, foundation);

		/* DrawFoundation() modifies ti.
		 * Default sloped sprites.. */
		if (ti->tileh != SLOPE_FLAT) image = _road_sloped_sprites[ti->tileh - 1] + 0x53F;
	}

	if (image == 0) image = _road_tile_sprites_1[road != ROAD_NONE ? road : tram];

	roadside = GetRoadside(ti->tile);

	if (AlwaysDrawUnpavedRoads(ti->tile, roadside)) {
		image += 19;
	} else {
		switch (roadside) {
			case ROADSIDE_BARREN:           pal = PALETTE_TO_BARE_LAND; break;
			case ROADSIDE_GRASS:            break;
			case ROADSIDE_GRASS_ROAD_WORKS: break;
			default:                        image -= 19; break; // Paved
		}
	}

	DrawGroundSprite(image, pal);

	/* For tram we overlay the road graphics with either tram tracks only
	 * (when there is actual road beneath the trams) or with tram tracks
	 * and some dirts which hides the road graphics */
	if (tram != ROAD_NONE) {
		if (ti->tileh != SLOPE_FLAT) {
			image = _road_sloped_sprites[ti->tileh - 1] + SPR_TRAMWAY_SLOPED_OFFSET;
		} else {
			image = _road_tile_sprites_1[tram] - SPR_ROAD_Y;
		}
		image += (road == ROAD_NONE) ? SPR_TRAMWAY_TRAM : SPR_TRAMWAY_OVERLAY;
		DrawGroundSprite(image, pal);
	}

	if (HasRoadWorks(ti->tile)) {
		/* Road works */
		DrawGroundSprite((road | tram) & ROAD_X ? SPR_EXCAVATION_X : SPR_EXCAVATION_Y, PAL_NONE);
		return;
	}

	if (tram != ROAD_NONE) DrawTramCatenary(ti, tram);

	/* Return if full detail is disabled, or we are zoomed fully out. */
	if (!HASBIT(_display_opt, DO_FULL_DETAIL) || _cur_dpi->zoom > ZOOM_LVL_DETAIL) return;

	/* Draw extra details. */
	for (drts = _road_display_table[roadside][road]; drts->image != 0; drts++) {
		int x = ti->x | drts->subcoord_x;
		int y = ti->y | drts->subcoord_y;
		byte z = ti->z;
		if (ti->tileh != SLOPE_FLAT) z = GetSlopeZ(x, y);
		AddSortableSpriteToDraw(drts->image, PAL_NONE, x, y, 2, 2, 0x10, z);
	}
}

static void DrawTile_Road(TileInfo *ti)
{
	switch (GetRoadTileType(ti->tile)) {
		case ROAD_TILE_NORMAL:
			DrawRoadBits(ti);
			break;

		case ROAD_TILE_CROSSING: {
			SpriteID image;
			SpriteID pal = PAL_NONE;
			Roadside roadside = GetRoadside(ti->tile);

			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, ti->tileh);

			image = GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.crossing;

			if (GetCrossingRoadAxis(ti->tile) == AXIS_X) image++;
			if (IsCrossingBarred(ti->tile)) image += 2;

			if (AlwaysDrawUnpavedRoads(ti->tile, roadside)) {
				image += 8;
			} else {
				switch (roadside) {
					case ROADSIDE_BARREN: pal = PALETTE_TO_BARE_LAND; break;
					case ROADSIDE_GRASS:  break;
					default:              image += 4; break; // Paved
				}
			}

			DrawGroundSprite(image, pal);
			if (HASBIT(GetRoadTypes(ti->tile), ROADTYPE_TRAM)) {
				DrawGroundSprite(SPR_TRAMWAY_OVERLAY + (GetCrossingRoadAxis(ti->tile) ^ 1), pal);
				DrawTramCatenary(ti, GetCrossingRoadBits(ti->tile));
			}
			if (GetRailType(ti->tile) == RAILTYPE_ELECTRIC) DrawCatenary(ti);
			break;
		}

		default:
		case ROAD_TILE_DEPOT: {
			const DrawTileSprites* dts;
			const DrawTileSeqStruct* dtss;
			SpriteID palette;

			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, ti->tileh);

			palette = PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile));

			if (HASBIT(GetRoadTypes(ti->tile), ROADTYPE_TRAM)) {
				dts =  &_tram_depot[GetRoadDepotDirection(ti->tile)];
			} else {
				dts =  &_road_depot[GetRoadDepotDirection(ti->tile)];
			}

			DrawGroundSprite(dts->ground_sprite, PAL_NONE);

			for (dtss = dts->seq; dtss->image != 0; dtss++) {
				SpriteID image = dtss->image;
				SpriteID pal;

				if (HASBIT(_transparent_opt, TO_BUILDINGS)) {
					SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
					pal = PALETTE_TO_TRANSPARENT;
				} else if (HASBIT(image, PALETTE_MODIFIER_COLOR)) {
					pal = palette;
				} else {
					pal = PAL_NONE;
				}

				AddSortableSpriteToDraw(
					image, pal,
					ti->x + dtss->delta_x, ti->y + dtss->delta_y,
					dtss->size_x, dtss->size_y,
					dtss->size_z, ti->z
				);
			}
			break;
		}
	}
	DrawBridgeMiddle(ti);
}

void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadType rt)
{
	SpriteID palette = PLAYER_SPRITE_COLOR(_local_player);
	const DrawTileSprites* dts = (rt == ROADTYPE_TRAM) ? &_tram_depot[dir] : &_road_depot[dir];
	const DrawTileSeqStruct* dtss;

	x += 33;
	y += 17;

	DrawSprite(dts->ground_sprite, PAL_NONE, x, y);

	for (dtss = dts->seq; dtss->image != 0; dtss++) {
		Point pt = RemapCoords(dtss->delta_x, dtss->delta_y, dtss->delta_z);
		SpriteID image = dtss->image;

		DrawSprite(image, HASBIT(image, PALETTE_MODIFIER_COLOR) ? palette : PAL_NONE, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Road(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	if (tileh == SLOPE_FLAT) return z;
	if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
		uint f = GetRoadFoundation(tileh, GetAllRoadBits(tile));

		if (f != 0) {
			if (IsSteepSlope(tileh)) {
				z += TILE_HEIGHT;
			} else if (f < 15) {
				return z + TILE_HEIGHT; // leveled foundation
			}
			tileh = _inclined_tileh[f - 15]; // inclined foundation
		}
		return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
	} else {
		return z + TILE_HEIGHT;
	}
}

static Slope GetSlopeTileh_Road(TileIndex tile, Slope tileh)
{
	if (tileh == SLOPE_FLAT) return SLOPE_FLAT;
	if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
		uint f = GetRoadFoundation(tileh, GetAllRoadBits(tile));

		if (f == 0) return tileh;
		if (f < 15) return SLOPE_FLAT; // leveled foundation
		return _inclined_tileh[f - 15]; // inclined foundation
	} else {
		return SLOPE_FLAT;
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


static const Roadside _town_road_types[][2] = {
	{ ROADSIDE_GRASS,         ROADSIDE_GRASS },
	{ ROADSIDE_PAVED,         ROADSIDE_PAVED },
	{ ROADSIDE_PAVED,         ROADSIDE_PAVED },
	{ ROADSIDE_TREES,         ROADSIDE_TREES },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED }
};

static const Roadside _town_road_types_2[][2] = {
	{ ROADSIDE_GRASS,         ROADSIDE_GRASS },
	{ ROADSIDE_PAVED,         ROADSIDE_PAVED },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED }
};


static void TileLoop_Road(TileIndex tile)
{
	switch (_opt.landscape) {
		case LT_ARCTIC:
			if (IsOnSnow(tile) != (GetTileZ(tile) > GetSnowLine())) {
				ToggleSnow(tile);
				MarkTileDirtyByTile(tile);
			}
			break;

		case LT_TROPIC:
			if (GetTropicZone(tile) == TROPICZONE_DESERT && !IsOnDesert(tile)) {
				ToggleDesert(tile);
				MarkTileDirtyByTile(tile);
			}
			break;
	}

	if (GetRoadTileType(tile) == ROAD_TILE_DEPOT) return;

	if (!HasRoadWorks(tile)) {
		const Town* t = ClosestTownFromTile(tile, (uint)-1);
		int grp = 0;

		if (t != NULL) {
			grp = GetTownRadiusGroup(t, tile);

			/* Show an animation to indicate road work */
			if (t->road_build_months != 0 &&
					(DistanceManhattan(t->xy, tile) < 8 || grp != 0) &&
					GetRoadTileType(tile) == ROAD_TILE_NORMAL && (GetAllRoadBits(tile) == ROAD_X || GetAllRoadBits(tile) == ROAD_Y)) {
				if (GetTileSlope(tile, NULL) == SLOPE_FLAT && EnsureNoVehicleOnGround(tile) && CHANCE16(1, 20)) {
					StartRoadWorks(tile);

					SndPlayTileFx(SND_21_JACKHAMMER, tile);
					CreateEffectVehicleAbove(
						TileX(tile) * TILE_SIZE + 7,
						TileY(tile) * TILE_SIZE + 7,
						0,
						EV_BULLDOZER);
					MarkTileDirtyByTile(tile);
					return;
				}
			}
		}

		{
			/* Adjust road ground type depending on 'grp' (grp is the distance to the center) */
			const Roadside* new_rs = (_opt.landscape == LT_TOYLAND) ? _town_road_types_2[grp] : _town_road_types[grp];
			Roadside cur_rs = GetRoadside(tile);

			/* We have our desired type, do nothing */
			if (cur_rs == new_rs[0]) return;

			/* We have the pre-type of the desired type, switch to the desired type */
			if (cur_rs == new_rs[1]) {
				cur_rs = new_rs[0];
			/* We have barren land, install the pre-type */
			} else if (cur_rs == ROADSIDE_BARREN) {
				cur_rs = new_rs[1];
			/* We're totally off limits, remove any installation and make barren land */
			} else {
				cur_rs = ROADSIDE_BARREN;
			}
			SetRoadside(tile, cur_rs);
			MarkTileDirtyByTile(tile);
		}
	} else if (IncreaseRoadWorksCounter(tile)) {
		TerminateRoadWorks(tile);
		MarkTileDirtyByTile(tile);
	}
}

static void ClickTile_Road(TileIndex tile)
{
	if (GetRoadTileType(tile) == ROAD_TILE_DEPOT) ShowDepotWindow(tile, VEH_ROAD);
}

static const byte _road_trackbits[16] = {
	0x0, 0x0, 0x0, 0x10, 0x0, 0x2, 0x8, 0x1A, 0x0, 0x4, 0x1, 0x15, 0x20, 0x26, 0x29, 0x3F,
};

static uint32 GetTileTrackStatus_Road(TileIndex tile, TransportType mode, uint sub_mode)
{

	switch (mode) {
		case TRANSPORT_RAIL:
			if (!IsLevelCrossing(tile)) return 0;
			return GetCrossingRailBits(tile) * 0x101;

		case TRANSPORT_ROAD:
			if ((GetRoadTypes(tile) & sub_mode) == 0) return 0;
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL: {
					RoadType rt = (RoadType)FindFirstBit(sub_mode);
					return HasRoadWorks(tile) ? 0 : _road_trackbits[GetRoadBits(tile, rt)] * 0x101;
				}

				case ROAD_TILE_CROSSING: {
					uint32 r = AxisToTrackBits(GetCrossingRoadAxis(tile)) * 0x101;

					if (IsCrossingBarred(tile)) r *= 0x10001;
					return r;
				}

				default:
				case ROAD_TILE_DEPOT:
					return AxisToTrackBits(DiagDirToAxis(GetRoadDepotDirection(tile))) * 0x101;
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
	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_CROSSING: td->str = STR_1818_ROAD_RAIL_LEVEL_CROSSING; break;
		case ROAD_TILE_DEPOT: td->str = STR_1817_ROAD_VEHICLE_DEPOT; break;
		default: td->str = _road_tile_strings[GetRoadside(tile)]; break;
	}
}

static const byte _roadveh_enter_depot_unk0[4] = {
	8, 9, 0, 1
};

static uint32 VehicleEnter_Road(Vehicle *v, TileIndex tile, int x, int y)
{
	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_CROSSING:
			if (v->type == VEH_TRAIN && !IsCrossingBarred(tile)) {
				/* train crossing a road */
				SndPlayVehicleFx(SND_0E_LEVEL_CROSSING, v);
				BarCrossing(tile);
				MarkTileDirtyByTile(tile);
			}
			break;

		case ROAD_TILE_DEPOT:
			if (v->type == VEH_ROAD &&
					v->u.road.frame == 11 &&
					_roadveh_enter_depot_unk0[GetRoadDepotDirection(tile)] == v->u.road.state) {
				VehicleEnterDepot(v);
				return VETSB_ENTERED_WORMHOLE;
			}
			break;

		default: break;
	}
	return VETSB_CONTINUE;
}


static void ChangeTileOwner_Road(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	if (GetRoadTileType(tile) == ROAD_TILE_DEPOT) {
		if (new_player == PLAYER_SPECTATOR) {
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		} else {
			SetTileOwner(tile, new_player);
		}
		return;
	}

	for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
		if (!HASBIT(GetRoadTypes(tile), rt)) continue;

		if (GetRoadOwner(tile, rt) == old_player) {
			SetRoadOwner(tile, rt, new_player == PLAYER_SPECTATOR ? OWNER_NONE : new_player);
		}
	}

	if (IsLevelCrossing(tile)) {
		if (GetTileOwner(tile) == old_player) {
			if (new_player == PLAYER_SPECTATOR) {
				MakeRoadNormal(tile, GetCrossingRoadBits(tile), GetRoadTypes(tile), GetTownIndex(tile), GetRoadOwner(tile, ROADTYPE_ROAD), GetRoadOwner(tile, ROADTYPE_TRAM), GetRoadOwner(tile, ROADTYPE_HWAY));
			} else {
				SetTileOwner(tile, new_player);
			}
		}
	}
}


extern const TileTypeProcs _tile_type_road_procs = {
	DrawTile_Road,           /* draw_tile_proc */
	GetSlopeZ_Road,          /* get_slope_z_proc */
	ClearTile_Road,          /* clear_tile_proc */
	GetAcceptedCargo_Road,   /* get_accepted_cargo_proc */
	GetTileDesc_Road,        /* get_tile_desc_proc */
	GetTileTrackStatus_Road, /* get_tile_track_status_proc */
	ClickTile_Road,          /* click_tile_proc */
	AnimateTile_Road,        /* animate_tile_proc */
	TileLoop_Road,           /* tile_loop_clear */
	ChangeTileOwner_Road,    /* change_tile_owner_clear */
	NULL,                    /* get_produced_cargo_proc */
	VehicleEnter_Road,       /* vehicle_enter_tile_proc */
	GetSlopeTileh_Road,      /* get_slope_tileh_proc */
};
