/* $Id$ */

/** @file road_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "bridge.h"
#include "cmd_helper.h"
#include "rail_map.h"
#include "road_map.h"
#include "road_internal.h"
#include "sprite.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "town_map.h"
#include "viewport_func.h"
#include "command_func.h"
#include "town.h"
#include "yapf/yapf.h"
#include "depot.h"
#include "newgrf.h"
#include "station_map.h"
#include "tunnel_map.h"
#include "misc/autoptr.hpp"
#include "variables.h"
#include "autoslope.h"
#include "transparency.h"
#include "tunnelbridge_map.h"
#include "window_func.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "vehicle_base.h"
#include "sound_func.h"
#include "road_func.h"

#include "table/sprites.h"
#include "table/strings.h"

#define M(x) (1 << (x))
/* Level crossings may only be built on these slopes */
static const uint32 VALID_LEVEL_CROSSING_SLOPES = (M(SLOPE_SEN) | M(SLOPE_ENW) | M(SLOPE_NWS) | M(SLOPE_NS) | M(SLOPE_WSE) | M(SLOPE_EW) | M(SLOPE_FLAT));
#undef M

Foundation GetRoadFoundation(Slope tileh, RoadBits bits);

bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, bool *edge_road, RoadType rt)
{
	RoadBits present;
	RoadBits n;
	*edge_road = true;

	if (_game_mode == GM_EDITOR || remove == ROAD_NONE) return true;

	/* Water can always flood and towns can always remove "normal" road pieces.
	 * Towns are not be allowed to remove non "normal" road pieces, like tram
	 * tracks as that would result in trams that cannot turn. */
	if (_current_player == OWNER_WATER ||
			(rt == ROADTYPE_ROAD && !IsValidPlayer(_current_player))) return true;

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
 *           bit    6 ignore the fact that the tram track has not been removed
 *                    yet when removing the road bits when not actually doing
 *                    it. Makes it possible to test whether the road bits can
 *                    be removed from a level crossing without physically
 *                    removing the tram bits before the test.
 * @param p2 unused
 */
CommandCost CmdRemoveRoad(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* cost for removing inner/edge -roads */
	static const uint16 road_remove_cost[2] = {50, 18};

	/* true if the roadpiece was always removeable,
	 * false if it was a center piece. Affects town ratings drop */
	bool edge_road;

	RoadType rt = (RoadType)GB(p1, 4, 2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

	Town *t = NULL;
	switch (GetTileType(tile)) {
		case MP_ROAD:
			if (_game_mode != GM_EDITOR && GetRoadOwner(tile, rt) == OWNER_TOWN) t = GetTownByTile(tile);
			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;
			break;

		case MP_STATION:
			if (!IsDriveThroughStopTile(tile)) return CMD_ERROR;
			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;
			break;

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return CMD_ERROR;
			if (GetVehicleTunnelBridge(tile, GetOtherTunnelBridgeEnd(tile)) != NULL) return CMD_ERROR;
			break;

		default:
			return CMD_ERROR;
	}

	RoadBits pieces = Extract<RoadBits, 0>(p1);
	RoadTypes rts = GetRoadTypes(tile);
	/* The tile doesn't have the given road type */
	if (!HasBit(rts, rt)) return CMD_ERROR;

	if (!CheckAllowRemoveRoad(tile, pieces, &edge_road, rt)) return CMD_ERROR;

	/* check if you're allowed to remove the street owned by a town
	 * removal allowance depends on difficulty setting */
	if (!CheckforTownRating(flags, t, ROAD_REMOVE)) return CMD_ERROR;

	if (!IsTileType(tile, MP_ROAD)) {
		/* If it's the last roadtype, just clear the whole tile */
		if (rts == RoadTypeToRoadTypes(rt)) return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);

		CommandCost cost(EXPENSES_CONSTRUCTION);
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			TileIndex other_end = GetOtherTunnelBridgeEnd(tile);
			/* Pay for *every* tile of the bridge or tunnel */
			cost.AddCost((DistanceManhattan(other_end, tile) + 1) * _price.remove_road);
			if (flags & DC_EXEC) {
				SetRoadTypes(other_end, GetRoadTypes(other_end) & ~RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) & ~RoadTypeToRoadTypes(rt));

				/* Mark tiles diry that have been repaved */
				MarkTileDirtyByTile(tile);
				MarkTileDirtyByTile(other_end);
				if (IsBridge(tile)) {
					TileIndexDiff delta = TileOffsByDiagDir(GetTunnelBridgeDirection(tile));

					for (TileIndex t = tile + delta; t != other_end; t += delta) MarkTileDirtyByTile(t);
				}
			}
		} else {
			cost.AddCost(_price.remove_road);
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

			ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);
			if (flags & DC_EXEC) {
				present ^= c;
				if (present == ROAD_NONE) {
					RoadTypes rts = GetRoadTypes(tile) & ComplementRoadTypes(RoadTypeToRoadTypes(rt));
					if (rts == ROADTYPES_NONE) {
						/* Includes MarkTileDirtyByTile() */
						DoClearSquare(tile);
					} else {
						SetRoadBits(tile, ROAD_NONE, rt);
						SetRoadTypes(tile, rts);
						MarkTileDirtyByTile(tile);
					}
				} else {
					/* When bits are removed, you *always* end up with something that
					 * is not a complete straight road tile. However, trams do not have
					 * onewayness, so they cannot remove it either. */
					if (rt != ROADTYPE_TRAM) SetDisallowedRoadDirections(tile, DRD_NONE);
					SetRoadBits(tile, present, rt);
					MarkTileDirtyByTile(tile);
				}
			}
			return CommandCost(EXPENSES_CONSTRUCTION, CountBits(c) * _price.remove_road);
		}

		case ROAD_TILE_CROSSING: {
			if (pieces & ComplementRoadBits(GetCrossingRoadBits(tile))) {
				return CMD_ERROR;
			}

			/* Don't allow road to be removed from the crossing when there is tram;
			 * we can't draw the crossing without trambits ;) */
			if (rt == ROADTYPE_ROAD && HasBit(GetRoadTypes(tile), ROADTYPE_TRAM) && ((flags & DC_EXEC) || !HasBit(p1, 6))) return CMD_ERROR;

			if (rt == ROADTYPE_ROAD) {
				ChangeTownRating(t, -road_remove_cost[(byte)edge_road], RATING_ROAD_MINIMUM);
			}

			if (flags & DC_EXEC) {
				RoadTypes rts = GetRoadTypes(tile) & ComplementRoadTypes(RoadTypeToRoadTypes(rt));
				if (rts == ROADTYPES_NONE) {
					MakeRailNormal(tile, GetTileOwner(tile), GetCrossingRailBits(tile), GetRailType(tile));
				} else {
					SetRoadTypes(tile, rts);
				}
				MarkTileDirtyByTile(tile);
				YapfNotifyTrackLayoutChange(tile, FindFirstTrack(GetTrackBits(tile)));
			}
			return CommandCost(EXPENSES_CONSTRUCTION, _price.remove_road * 2);
		}

		default:
		case ROAD_TILE_DEPOT:
			return CMD_ERROR;
	}
}


static const RoadBits _valid_tileh_slopes_road[][15] = {
	/* set of normal ones */
	{
		ROAD_ALL,  // SLOPE_FLAT
		ROAD_NONE, // SLOPE_W
		ROAD_NONE, // SLOPE_S

		ROAD_X,    // SLOPE_SW
		ROAD_NONE, // SLOPE_E
		ROAD_NONE, // SLOPE_EW

		ROAD_Y,    // SLOPE_SE
		ROAD_NONE, // SLOPE_WSE
		ROAD_NONE, // SLOPE_N

		ROAD_Y,    // SLOPE_NW
		ROAD_NONE, // SLOPE_NS
		ROAD_NONE, // SLOPE_NE

		ROAD_X,    // SLOPE_ENW
		ROAD_NONE, // SLOPE_SEN
		ROAD_NONE  // SLOPE_ELEVATED
	},
	/* allowed road for an evenly raised platform */
	{
		ROAD_NONE,         // SLOPE_FLAT
		ROAD_SW | ROAD_NW, // SLOPE_W
		ROAD_SW | ROAD_SE, // SLOPE_S

		ROAD_Y  | ROAD_SW, // SLOPE_SW
		ROAD_SE | ROAD_NE, // SLOPE_E
		ROAD_ALL,          // SLOPE_EW

		ROAD_X  | ROAD_SE, // SLOPE_SE
		ROAD_ALL,          // SLOPE_WSE
		ROAD_NW | ROAD_NE, // SLOPE_N

		ROAD_X  | ROAD_NW, // SLOPE_NW
		ROAD_ALL,          // SLOPE_NS
		ROAD_ALL,          // SLOPE_NE

		ROAD_Y  | ROAD_NE, // SLOPE_ENW
		ROAD_ALL,          // SLOPE_SEN
		ROAD_ALL           // SLOPE_ELEVATED
	},
	/* Singe bits on slopes */
	{
		ROAD_ALL,          // SLOPE_FLAT
		ROAD_NE | ROAD_SE, // SLOPE_W
		ROAD_NE | ROAD_NW, // SLOPE_S

		ROAD_NE,           // SLOPE_SW
		ROAD_NW | ROAD_SW, // SLOPE_E
		ROAD_ALL,          // SLOPE_EW

		ROAD_NW,           // SLOPE_SE
		ROAD_ALL,          // SLOPE_WSE
		ROAD_SE | ROAD_SW, // SLOPE_N

		ROAD_SE,           // SLOPE_NW
		ROAD_ALL,          // SLOPE_NS
		ROAD_ALL,          // SLOPE_NE

		ROAD_SW,           // SLOPE_ENW
		ROAD_ALL,          // SLOPE_SEN
		ROAD_ALL,          // SLOPE_ELEVATED
	},
};

/**
 * Calculate the costs for roads on slopes
 *  Aside modify the RoadBits to fit on the slopes
 *
 * @note The RoadBits are modified too!
 * @param tileh The current slope
 * @param pieces The RoadBits we want to add
 * @param existing The existent RoadBits
 * @return The costs for these RoadBits on this slope
 */
static CommandCost CheckRoadSlope(Slope tileh, RoadBits* pieces, RoadBits existing)
{
	if (IsSteepSlope(tileh)) {
		/* Force straight roads. */
		*pieces |= MirrorRoadBits(*pieces);

		if (existing == ROAD_NONE || existing == *pieces) {
			if (*pieces == ROAD_X || *pieces == ROAD_Y) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
		}
		return CMD_ERROR;
	}

	RoadBits road_bits = *pieces | existing;

	/* Single bits on slopes.
	 * We check for the roads that need at least 2 bits */
	if (_patches.build_on_slopes && !_is_old_ai_player &&
			existing == ROAD_NONE && CountBits(*pieces) == 1 &&
			(_valid_tileh_slopes_road[2][tileh] & *pieces) == ROAD_NONE) {
		return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
	}

	/* no special foundation */
	if ((~_valid_tileh_slopes_road[0][tileh] & road_bits) == ROAD_NONE) {
		/* force that all bits are set when we have slopes */
		if (tileh != SLOPE_FLAT) *pieces |= _valid_tileh_slopes_road[0][tileh];
		return CommandCost(); // no extra cost
	}

	/* foundation is used. Whole tile is leveled up */
	if ((~_valid_tileh_slopes_road[1][tileh] & road_bits) == ROAD_NONE) {
		return CommandCost(EXPENSES_CONSTRUCTION, existing != ROAD_NONE ? (Money)0 : _price.terraform);
	}

	/* Force straight roads. */
	*pieces |= MirrorRoadBits(*pieces);

	/* partly leveled up tile, only if there's no road on that tile */
	if ((existing == ROAD_NONE || existing == *pieces) && (tileh == SLOPE_W || tileh == SLOPE_S || tileh == SLOPE_E || tileh == SLOPE_N)) {
		if (*pieces == ROAD_X || *pieces == ROAD_Y) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
	}
	return CMD_ERROR;
}

/** Build a piece of road.
 * @param tile tile where to build road
 * @param flags operation to perform
 * @param p1 bit 0..3 road pieces to build (RoadBits)
 *           bit 4..5 road type
 *           bit 6..7 disallowed directions to toggle
 * @param p2 the town that is building the road (0 if not applicable)
 */
CommandCost CmdBuildRoad(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost ret;
	RoadBits existing = ROAD_NONE;
	RoadBits all_bits = ROAD_NONE;
	Slope tileh;

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-player is building the road */
	if ((IsValidPlayer(_current_player) && p2 != 0) || (_current_player == OWNER_TOWN && !IsValidTownID(p2))) return CMD_ERROR;

	RoadBits pieces = Extract<RoadBits, 0>(p1);

	RoadType rt = (RoadType)GB(p1, 4, 2);
	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	DisallowedRoadDirections toggle_drd = (DisallowedRoadDirections)GB(p1, 6, 2);

	tileh = GetTileSlope(tile, NULL);

	switch (GetTileType(tile)) {
		case MP_ROAD:
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL: {
					if (HasRoadWorks(tile)) return_cmd_error(STR_ROAD_WORKS_IN_PROGRESS);
					if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

					all_bits = GetAllRoadBits(tile);
					if (!HasBit(GetRoadTypes(tile), rt)) break;

					existing = GetRoadBits(tile, rt);
					RoadBits merged = existing | pieces;
					bool crossing = (merged != ROAD_X && merged != ROAD_Y);
					if (rt != ROADTYPE_TRAM && (GetDisallowedRoadDirections(tile) != DRD_NONE || toggle_drd != DRD_NONE) && crossing) {
						/* Junctions cannot be one-way */
						return_cmd_error(STR_ERR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);
					}
					if ((existing & pieces) == pieces) {
						/* We only want to set the (dis)allowed road directions */
						if (toggle_drd != DRD_NONE && rt != ROADTYPE_TRAM && GetRoadOwner(tile, ROADTYPE_ROAD) == _current_player) {
							if (crossing) return_cmd_error(STR_ERR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);

							/* Ignore half built tiles */
							if (flags & DC_EXEC && rt != ROADTYPE_TRAM && (existing == ROAD_X || existing == ROAD_Y)) {
								SetDisallowedRoadDirections(tile, GetDisallowedRoadDirections(tile) ^ toggle_drd);
								MarkTileDirtyByTile(tile);
							}
							return CommandCost();
						}
						return_cmd_error(STR_1007_ALREADY_BUILT);
					}
				} break;

				case ROAD_TILE_CROSSING:
					if (HasBit(GetRoadTypes(tile), rt)) return_cmd_error(STR_1007_ALREADY_BUILT);
					all_bits = GetCrossingRoadBits(tile);
					if (pieces & ComplementRoadBits(all_bits)) goto do_clear;
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

			/* Level crossings may only be built on these slopes */
			if (!HasBit(VALID_LEVEL_CROSSING_SLOPES, tileh)) {
				return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
			}

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
				UpdateLevelCrossing(tile, false);
				MarkTileDirtyByTile(tile);
			}
			return CommandCost(EXPENSES_CONSTRUCTION, _price.build_road * (rt == ROADTYPE_ROAD ? 2 : 4));
		}

		case MP_STATION:
			if (!IsDriveThroughStopTile(tile)) return CMD_ERROR;
			if (HasBit(GetRoadTypes(tile), rt)) return_cmd_error(STR_1007_ALREADY_BUILT);
			/* Don't allow adding roadtype to the roadstop when vehicles are already driving on it */
			if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;
			break;

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return CMD_ERROR;
			if (HasBit(GetRoadTypes(tile), rt)) return_cmd_error(STR_1007_ALREADY_BUILT);
			/* Don't allow adding roadtype to the bridge/tunnel when vehicles are already driving on it */
			if (GetVehicleTunnelBridge(tile, GetOtherTunnelBridgeEnd(tile)) != NULL) return CMD_ERROR;
			break;

		default:
do_clear:;
			ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return ret;
			cost.AddCost(ret);
	}

	if (all_bits != pieces) {
		/* Check the foundation/slopes when adding road/tram bits */
		ret = CheckRoadSlope(tileh, &pieces, all_bits | existing);
		/* Return an error if we need to build a foundation (ret != 0) but the
		 * current patch-setting is turned off (or stupid AI@work) */
		if (CmdFailed(ret) || (ret.GetCost() != 0 && (!_patches.build_on_slopes || _is_old_ai_player))) {
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
		}
		cost.AddCost(ret);
	}

	if (IsTileType(tile, MP_ROAD)) {
		/* Don't put the pieces that already exist */
		pieces &= ComplementRoadBits(existing);

		/* Check if new road bits will have the same foundation as other existing road types */
		if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
			Slope slope = GetTileSlope(tile, NULL);
			Foundation found_new = GetRoadFoundation(slope, pieces | existing);

			/* Test if all other roadtypes can be built at that foundation */
			for (RoadType rtest = ROADTYPE_ROAD; rtest < ROADTYPE_END; rtest++) {
				if (rtest != rt) { // check only other road types
					RoadBits bits = GetRoadBits(tile, rtest);
					/* do not check if there are not road bits of given type */
					if (bits != ROAD_NONE && GetRoadFoundation(slope, bits) != found_new) {
						return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
					}
				}
			}
		}
	}

	cost.AddCost(CountBits(pieces) * _price.build_road);
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* Pay for *every* tile of the bridge or tunnel */
		cost.MultiplyCost(DistanceManhattan(GetOtherTunnelBridgeEnd(tile), tile) + 1);
	}

	if (flags & DC_EXEC) {
		switch (GetTileType(tile)) {
			case MP_ROAD: {
				RoadTileType rtt = GetRoadTileType(tile);
				if (existing == ROAD_NONE || rtt == ROAD_TILE_CROSSING) {
					SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
					SetRoadOwner(tile, rt, _current_player);
					if (_current_player == OWNER_TOWN && rt == ROADTYPE_ROAD) SetTownIndex(tile, p2);
				}
				if (rtt != ROAD_TILE_CROSSING) SetRoadBits(tile, existing | pieces, rt);
			} break;

			case MP_TUNNELBRIDGE: {
				TileIndex other_end = GetOtherTunnelBridgeEnd(tile);

				SetRoadTypes(other_end, GetRoadTypes(other_end) | RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));

				/* Mark tiles diry that have been repaved */
				MarkTileDirtyByTile(other_end);
				MarkTileDirtyByTile(tile);
				if (IsBridge(tile)) {
					TileIndexDiff delta = TileOffsByDiagDir(GetTunnelBridgeDirection(tile));

					for (TileIndex t = tile + delta; t != other_end; t += delta) MarkTileDirtyByTile(t);
				}
			} break;

			case MP_STATION:
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
				break;

			default:
				MakeRoadNormal(tile, pieces, RoadTypeToRoadTypes(rt), p2, _current_player, _current_player, _current_player);
				break;
		}

		if (rt != ROADTYPE_TRAM && IsTileType(tile, MP_ROAD) && GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
			existing |= pieces;
			SetDisallowedRoadDirections(tile, (existing == ROAD_X || existing == ROAD_Y) ?
					GetDisallowedRoadDirections(tile) ^ toggle_drd : DRD_NONE);
		}

		MarkTileDirtyByTile(tile);
	}
	return cost;
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
 * - p2 = (bit 5) - set road direction
 */
CommandCost CmdBuildLongRoad(TileIndex end_tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start_tile, tile;
	CommandCost ret, cost(EXPENSES_CONSTRUCTION);
	bool had_bridge = false;
	bool had_tunnel = false;
	bool had_success = false;
	DisallowedRoadDirections drd = DRD_NORTHBOUND;

	if (p1 >= MapSize()) return CMD_ERROR;

	start_tile = p1;
	RoadType rt = (RoadType)GB(p2, 3, 2);
	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	/* Only drag in X or Y direction dictated by the direction variable */
	if (!HasBit(p2, 2) && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (HasBit(p2, 2)  && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	/* Swap start and ending tile, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HasBit(p2, 0))) {
		TileIndex t = start_tile;
		start_tile = end_tile;
		end_tile = t;
		p2 ^= IsInsideMM(p2 & 3, 1, 3) ? 3 : 0;
		drd = DRD_SOUTHBOUND;
	}

	/* On the X-axis, we have to swap the initial bits, so they
	 * will be interpreted correctly in the GTTS. Futhermore
	 * when you just 'click' on one tile to build them. */
	if (HasBit(p2, 2) == (start_tile == end_tile && HasBit(p2, 0) == HasBit(p2, 1))) drd ^= DRD_BOTH;
	/* No disallowed direction bits have to be toggled */
	if (!HasBit(p2, 5)) drd = DRD_NONE;

	tile = start_tile;
	/* Start tile is the small number. */
	for (;;) {
		RoadBits bits = HasBit(p2, 2) ? ROAD_Y : ROAD_X;

		if (tile == end_tile && !HasBit(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HasBit(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		ret = DoCommand(tile, drd << 6 | rt << 4 | bits, 0, flags, CMD_BUILD_ROAD);
		if (CmdFailed(ret)) {
			if (_error_message != STR_1007_ALREADY_BUILT) return CMD_ERROR;
			_error_message = INVALID_STRING_ID;
		} else {
			had_success = true;
			/* Only pay for the upgrade on one side of the bridges and tunnels */
			if (IsTileType(tile, MP_TUNNELBRIDGE)) {
				if (IsBridge(tile)) {
					if ((!had_bridge || GetTunnelBridgeDirection(tile) == DIAGDIR_SE || GetTunnelBridgeDirection(tile) == DIAGDIR_SW)) {
						cost.AddCost(ret);
					}
					had_bridge = true;
				} else { // IsTunnel(tile)
					if ((!had_tunnel || GetTunnelBridgeDirection(tile) == DIAGDIR_SE || GetTunnelBridgeDirection(tile) == DIAGDIR_SW)) {
						cost.AddCost(ret);
					}
					had_tunnel = true;
				}
			} else {
				cost.AddCost(ret);
			}
		}

		if (tile == end_tile) break;

		tile += HasBit(p2, 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return !had_success ? CMD_ERROR : cost;
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
CommandCost CmdRemoveLongRoad(TileIndex end_tile, uint32 flags, uint32 p1, uint32 p2)
{
	TileIndex start_tile, tile;
	CommandCost ret, cost(EXPENSES_CONSTRUCTION);
	Money money;

	if (p1 >= MapSize()) return CMD_ERROR;

	start_tile = p1;
	RoadType rt = (RoadType)GB(p2, 3, 2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

	/* Only drag in X or Y direction dictated by the direction variable */
	if (!HasBit(p2, 2) && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (HasBit(p2, 2)  && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	/* Swap start and ending tile, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HasBit(p2, 0))) {
		TileIndex t = start_tile;
		start_tile = end_tile;
		end_tile = t;
		p2 ^= IsInsideMM(p2 & 3, 1, 3) ? 3 : 0;
	}

	money = GetAvailableMoneyForCommand();
	tile = start_tile;
	/* Start tile is the small number. */
	for (;;) {
		RoadBits bits = HasBit(p2, 2) ? ROAD_Y : ROAD_X;

		if (tile == end_tile && !HasBit(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HasBit(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		/* try to remove the halves. */
		if (bits != 0) {
			ret = DoCommand(tile, rt << 4 | bits, 0, flags & ~DC_EXEC, CMD_REMOVE_ROAD);
			if (CmdSucceeded(ret)) {
				if (flags & DC_EXEC) {
					money -= ret.GetCost();
					if (money < 0) {
						_additional_cash_required = DoCommand(end_tile, start_tile, p2, flags & ~DC_EXEC, CMD_REMOVE_LONG_ROAD).GetCost();
						return cost;
					}
					DoCommand(tile, rt << 4 | bits, 0, flags, CMD_REMOVE_ROAD);
				}
				cost.AddCost(ret);
			}
		}

		if (tile == end_tile) break;

		tile += HasBit(p2, 2) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return (cost.GetCost() == 0) ? CMD_ERROR : cost;
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
CommandCost CmdBuildRoadDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost;
	Slope tileh;

	DiagDirection dir = Extract<DiagDirection, 0>(p1);
	RoadType rt = (RoadType)GB(p1, 2, 2);

	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

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

	Depot *dep = new Depot(tile);
	if (dep == NULL) return CMD_ERROR;
	AutoPtrT<Depot> d_auto_delete = dep;

	if (flags & DC_EXEC) {
		dep->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		MakeRoadDepot(tile, _current_player, dir, rt);
		MarkTileDirtyByTile(tile);
		d_auto_delete.Detach();
	}
	return cost.AddCost(_price.build_road_depot);
}

static CommandCost RemoveRoadDepot(TileIndex tile, uint32 flags)
{
	if (!CheckTileOwnership(tile) && _current_player != OWNER_WATER)
		return CMD_ERROR;

	if (!EnsureNoVehicleOnGround(tile)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);
		delete GetDepotByTile(tile);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price.remove_road_depot);
}

static CommandCost ClearTile_Road(TileIndex tile, byte flags)
{
	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_NORMAL: {
			RoadBits b = GetAllRoadBits(tile);

			/* Clear the road if only one piece is on the tile OR the AI tries
			 * to clear town road OR we are not using the DC_AUTO flag */
			if ((CountBits(b) == 1 && GetRoadBits(tile, ROADTYPE_TRAM) == ROAD_NONE) ||
			    ((flags & DC_AI_BUILDING) && IsTileOwner(tile, OWNER_TOWN)) ||
			    !(flags & DC_AUTO)
				) {
				RoadTypes rts = GetRoadTypes(tile);
				CommandCost ret(EXPENSES_CONSTRUCTION);
				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					if (HasBit(rts, rt)) {
						CommandCost tmp_ret = DoCommand(tile, rt << 4 | GetRoadBits(tile, rt), 0, flags, CMD_REMOVE_ROAD);
						if (CmdFailed(tmp_ret)) return tmp_ret;
						ret.AddCost(tmp_ret);
					}
				}
				return ret;
			}
			return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);
		}

		case ROAD_TILE_CROSSING: {
			RoadTypes rts = GetRoadTypes(tile);
			CommandCost ret(EXPENSES_CONSTRUCTION);

			if (flags & DC_AUTO) return_cmd_error(STR_1801_MUST_REMOVE_ROAD_FIRST);

			/* Must iterate over the roadtypes in a reverse manner because
			 * tram tracks must be removed before the road bits. */
			for (RoadType rt = ROADTYPE_HWAY; rt >= ROADTYPE_ROAD; rt--) {
				if (HasBit(rts, rt)) {
					CommandCost tmp_ret = DoCommand(tile, 1 << 6 | rt << 4 | GetCrossingRoadBits(tile), 0, flags, CMD_REMOVE_ROAD);
					if (CmdFailed(tmp_ret)) return tmp_ret;
					ret.AddCost(tmp_ret);
				}
			}

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


Foundation GetRoadFoundation(Slope tileh, RoadBits bits)
{
	if (!IsSteepSlope(tileh)) {
		if ((~_valid_tileh_slopes_road[0][tileh] & bits) == 0) {
			/* As one can remove a single road piece when in a corner on a foundation as
			 * it is on a sloped piece of landscape, one creates a state that cannot be
			 * created directly, but the state itself is still perfectly drawable.
			 * However, as we do not want this to be build directly, we need to check
			 * for that situation in here. */
			return (tileh != 0 && CountBits(bits) == 1) ? FOUNDATION_LEVELED : FOUNDATION_NONE;
		}
		if ((~_valid_tileh_slopes_road[1][tileh] & bits) == 0) return FOUNDATION_LEVELED;
	}

	return (bits == ROAD_X ? FOUNDATION_INCLINED_X : FOUNDATION_INCLINED_Y);
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
	if (MayHaveBridgeAbove(ti->tile) && IsBridgeAbove(ti->tile) && !IsTransparencySet(TO_BUILDINGS)) {
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

	AddSortableSpriteToDraw(back,  PAL_NONE, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_BUILDINGS));
	AddSortableSpriteToDraw(front, PAL_NONE, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_BUILDINGS));
}

/**
 * Draws details on/around the road
 * @param img the sprite to draw
 * @param ti  the tile to draw on
 * @param dx  the offset from the top of the BB of the tile
 * @param dy  the offset from the top of the BB of the tile
 * @param h   the height of the sprite to draw
 */
static void DrawRoadDetail(SpriteID img, TileInfo *ti, int dx, int dy, int h)
{
	int x = ti->x | dx;
	int y = ti->y | dy;
	byte z = ti->z;
	if (ti->tileh != SLOPE_FLAT) z = GetSlopeZ(x, y);
	AddSortableSpriteToDraw(img, PAL_NONE, x, y, 2, 2, h, z);
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
		DrawFoundation(ti, GetRoadFoundation(ti->tileh, road | tram));

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

	if (road != ROAD_NONE) {
		DisallowedRoadDirections drd = GetDisallowedRoadDirections(ti->tile);
		if (drd != DRD_NONE) {
			DrawRoadDetail(SPR_ONEWAY_BASE + drd - 1 + ((road == ROAD_X) ? 0 : 3), ti, 8, 8, 0);
		}
	}

	if (HasRoadWorks(ti->tile)) {
		/* Road works */
		DrawGroundSprite((road | tram) & ROAD_X ? SPR_EXCAVATION_X : SPR_EXCAVATION_Y, PAL_NONE);
		return;
	}

	if (tram != ROAD_NONE) DrawTramCatenary(ti, tram);

	/* Return if full detail is disabled, or we are zoomed fully out. */
	if (!HasBit(_display_opt, DO_FULL_DETAIL) || _cur_dpi->zoom > ZOOM_LVL_DETAIL) return;

	/* Draw extra details. */
	for (drts = _road_display_table[roadside][road]; drts->image != 0; drts++) {
		DrawRoadDetail(drts->image, ti, drts->subcoord_x, drts->subcoord_y, 0x10);
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

			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

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
			if (HasBit(GetRoadTypes(ti->tile), ROADTYPE_TRAM)) {
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

			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

			palette = PLAYER_SPRITE_COLOR(GetTileOwner(ti->tile));

			if (HasBit(GetRoadTypes(ti->tile), ROADTYPE_TRAM)) {
				dts =  &_tram_depot[GetRoadDepotDirection(ti->tile)];
			} else {
				dts =  &_road_depot[GetRoadDepotDirection(ti->tile)];
			}

			DrawGroundSprite(dts->ground_sprite, PAL_NONE);

			for (dtss = dts->seq; dtss->image != 0; dtss++) {
				SpriteID image = dtss->image;
				SpriteID pal;

				if (!IsTransparencySet(TO_BUILDINGS) && HasBit(image, PALETTE_MODIFIER_COLOR)) {
					pal = palette;
				} else {
					pal = PAL_NONE;
				}

				AddSortableSpriteToDraw(
					image, pal,
					ti->x + dtss->delta_x, ti->y + dtss->delta_y,
					dtss->size_x, dtss->size_y,
					dtss->size_z, ti->z,
					IsTransparencySet(TO_BUILDINGS)
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

		DrawSprite(image, HasBit(image, PALETTE_MODIFIER_COLOR) ? palette : PAL_NONE, x + pt.x, y + pt.y);
	}
}

static uint GetSlopeZ_Road(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	if (tileh == SLOPE_FLAT) return z;
	if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
		Foundation f = GetRoadFoundation(tileh, GetAllRoadBits(tile));
		z += ApplyFoundationToSlope(f, &tileh);
		return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
	} else {
		return z + TILE_HEIGHT;
	}
}

static Foundation GetFoundation_Road(TileIndex tile, Slope tileh)
{
	if (GetRoadTileType(tile) == ROAD_TILE_NORMAL) {
		return GetRoadFoundation(tileh, GetAllRoadBits(tile));
	} else {
		return FlatteningFoundation(tileh);
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

	const Town* t = ClosestTownFromTile(tile, (uint)-1);
	if (!HasRoadWorks(tile)) {
		HouseZonesBits grp = HZB_TOWN_EDGE;

		if (t != NULL) {
			grp = GetTownRadiusGroup(t, tile);

			/* Show an animation to indicate road work */
			if (t->road_build_months != 0 &&
					(DistanceManhattan(t->xy, tile) < 8 || grp != HZB_TOWN_EDGE) &&
					GetRoadTileType(tile) == ROAD_TILE_NORMAL && CountBits(GetAllRoadBits(tile)) > 1 ) {
				if (GetTileSlope(tile, NULL) == SLOPE_FLAT && EnsureNoVehicleOnGround(tile) && Chance16(1, 40)) {
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

		if (_patches.mod_road_rebuild) {
			/* Generate a nicer town surface */
			const RoadBits old_rb = GetAnyRoadBits(tile, ROADTYPE_ROAD);
			const RoadBits new_rb = CleanUpRoadBits(tile, old_rb);

			if (old_rb != new_rb) {
				DoCommand(tile, (old_rb ^ new_rb), t->index, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_REMOVE_ROAD);
			}
		}

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
					const uint drd_to_multiplier[DRD_END] = { 0x101, 0x100, 0x1, 0x0 };
					uint multiplier = drd_to_multiplier[rt == ROADTYPE_TRAM ? DRD_NONE : GetDisallowedRoadDirections(tile)];
					return HasRoadWorks(tile) ? 0 : _road_trackbits[GetRoadBits(tile, rt)] * multiplier;
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

/**
 * Given the direction the road depot is pointing, this is the direction the
 * vehicle should be travelling in in order to enter the depot.
 */
static const byte _roadveh_enter_depot_dir[4] = {
	TRACKDIR_X_SW, TRACKDIR_Y_NW, TRACKDIR_X_NE, TRACKDIR_Y_SE
};

static VehicleEnterTileStatus VehicleEnter_Road(Vehicle *v, TileIndex tile, int x, int y)
{
	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_CROSSING:
			if (v->type == VEH_TRAIN) {
				/* it should be barred */
				assert(IsCrossingBarred(tile));
			}
			break;

		case ROAD_TILE_DEPOT:
			if (v->type == VEH_ROAD &&
					v->u.road.frame == 11 &&
					_roadveh_enter_depot_dir[GetRoadDepotDirection(tile)] == v->u.road.state) {
				v->u.road.state = RVSB_IN_DEPOT;
				v->vehstatus |= VS_HIDDEN;
				v->direction = ReverseDir(v->direction);
				if (v->Next() == NULL) VehicleEnterDepot(v);
				v->tile = tile;

				InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
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
		if (GetTileOwner(tile) == old_player) {
			if (new_player == PLAYER_SPECTATOR) {
				DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			} else {
				SetTileOwner(tile, new_player);
			}
		}
		return;
	}

	for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
		if (!HasBit(GetRoadTypes(tile), rt)) continue;

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

static CommandCost TerraformTile_Road(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	if (_patches.build_on_slopes && AutoslopeEnabled()) {
		switch (GetRoadTileType(tile)) {
			case ROAD_TILE_CROSSING:
				if (!IsSteepSlope(tileh_new) && (GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new)) && HasBit(VALID_LEVEL_CROSSING_SLOPES, tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
				break;

			case ROAD_TILE_DEPOT:
				if (AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, GetRoadDepotDirection(tile))) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
				break;

			case ROAD_TILE_NORMAL: {
				RoadBits bits = GetAllRoadBits(tile);
				RoadBits bits_copy = bits;
				/* Check if the slope-road_bits combination is valid at all, i.e. it is save to call GetRoadFoundation(). */
				if (!CmdFailed(CheckRoadSlope(tileh_new, &bits_copy, ROAD_NONE))) {
					/* CheckRoadSlope() sometimes changes the road_bits, if it does not agree with them. */
					if (bits == bits_copy) {
						uint z_old;
						Slope tileh_old = GetTileSlope(tile, &z_old);

						/* Get the slope on top of the foundation */
						z_old += ApplyFoundationToSlope(GetRoadFoundation(tileh_old, bits), &tileh_old);
						z_new += ApplyFoundationToSlope(GetRoadFoundation(tileh_new, bits), &tileh_new);

						/* The surface slope must not be changed */
						if ((z_old == z_new) && (tileh_old == tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
					}
				}
				break;
			}

			default: NOT_REACHED();
		}
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
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
	GetFoundation_Road,      /* get_foundation_proc */
	TerraformTile_Road,      /* terraform_tile_proc */
};
