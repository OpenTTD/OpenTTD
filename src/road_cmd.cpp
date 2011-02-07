/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_cmd.cpp Commands related to road tiles. */

#include "stdafx.h"
#include "cmd_helper.h"
#include "road_internal.h"
#include "viewport_func.h"
#include "command_func.h"
#include "pathfinder/yapf/yapf_cache.h"
#include "depot_base.h"
#include "newgrf.h"
#include "autoslope.h"
#include "tunnelbridge_map.h"
#include "window_func.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "tunnelbridge.h"
#include "cheat_type.h"
#include "effectvehicle_func.h"
#include "effectvehicle_base.h"
#include "elrail_func.h"
#include "roadveh.h"
#include "town.h"
#include "company_base.h"
#include "core/random_func.hpp"
#include "newgrf_railtype.h"
#include "date_func.h"
#include "genworld.h"

#include "table/strings.h"

/**
 * Verify whether a road vehicle is available.
 * @return \c true if at least one road vehicle is available, \c false if not
 */
bool RoadVehiclesAreBuilt()
{
	const RoadVehicle *rv;
	FOR_ALL_ROADVEHICLES(rv) return true;

	return false;
}

/* Invalid RoadBits on slopes  */
static const RoadBits _invalid_tileh_slopes_road[2][15] = {
	/* The inverse of the mixable RoadBits on a leveled slope */
	{
		ROAD_NONE,         // SLOPE_FLAT
		ROAD_NE | ROAD_SE, // SLOPE_W
		ROAD_NE | ROAD_NW, // SLOPE_S

		ROAD_NE,           // SLOPE_SW
		ROAD_NW | ROAD_SW, // SLOPE_E
		ROAD_NONE,         // SLOPE_EW

		ROAD_NW,           // SLOPE_SE
		ROAD_NONE,         // SLOPE_WSE
		ROAD_SE | ROAD_SW, // SLOPE_N

		ROAD_SE,           // SLOPE_NW
		ROAD_NONE,         // SLOPE_NS
		ROAD_NONE,         // SLOPE_ENW

		ROAD_SW,           // SLOPE_NE
		ROAD_NONE,         // SLOPE_SEN
		ROAD_NONE          // SLOPE_NWS
	},
	/* The inverse of the allowed straight roads on a slope
	 * (with and without a foundation). */
	{
		ROAD_NONE, // SLOPE_FLAT
		ROAD_NONE, // SLOPE_W    Foundation
		ROAD_NONE, // SLOPE_S    Foundation

		ROAD_Y,    // SLOPE_SW
		ROAD_NONE, // SLOPE_E    Foundation
		ROAD_ALL,  // SLOPE_EW

		ROAD_X,    // SLOPE_SE
		ROAD_ALL,  // SLOPE_WSE
		ROAD_NONE, // SLOPE_N    Foundation

		ROAD_X,    // SLOPE_NW
		ROAD_ALL,  // SLOPE_NS
		ROAD_ALL,  // SLOPE_ENW

		ROAD_Y,    // SLOPE_NE
		ROAD_ALL,  // SLOPE_SEN
		ROAD_ALL   // SLOPE_NW
	}
};

static Foundation GetRoadFoundation(Slope tileh, RoadBits bits);

/**
 * Is it allowed to remove the given road bits from the given tile?
 * @param tile      the tile to remove the road from
 * @param remove    the roadbits that are going to be removed
 * @param owner     the actual owner of the roadbits of the tile
 * @param rt        the road type to remove the bits from
 * @param flags     command flags
 * @param town_check Shall the town rating checked/affected
 * @return A succeeded command when it is allowed to remove the road bits, a failed command otherwise.
 */
CommandCost CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, RoadType rt, DoCommandFlag flags, bool town_check)
{
	if (_game_mode == GM_EDITOR || remove == ROAD_NONE) return CommandCost();

	/* Water can always flood and towns can always remove "normal" road pieces.
	 * Towns are not be allowed to remove non "normal" road pieces, like tram
	 * tracks as that would result in trams that cannot turn. */
	if (_current_company == OWNER_WATER ||
			(rt == ROADTYPE_ROAD && !Company::IsValidID(_current_company))) return CommandCost();

	/* Only do the special processing if the road is owned
	 * by a town */
	if (owner != OWNER_TOWN) {
		if (owner == OWNER_NONE) return CommandCost();
		CommandCost ret = CheckOwnership(owner);
		return ret;
	}

	if (!town_check) return CommandCost();

	if (_cheats.magic_bulldozer.value) return CommandCost();

	Town *t = ClosestTownFromTile(tile, UINT_MAX);
	if (t == NULL) return CommandCost();

	/* check if you're allowed to remove the street owned by a town
	 * removal allowance depends on difficulty setting */
	CommandCost ret = CheckforTownRating(flags, t, ROAD_REMOVE);
	if (ret.Failed()) return ret;

	/* Get a bitmask of which neighbouring roads has a tile */
	RoadBits n = ROAD_NONE;
	RoadBits present = GetAnyRoadBits(tile, rt);
	if ((present & ROAD_NE) && (GetAnyRoadBits(TILE_ADDXY(tile, -1,  0), rt) & ROAD_SW)) n |= ROAD_NE;
	if ((present & ROAD_SE) && (GetAnyRoadBits(TILE_ADDXY(tile,  0,  1), rt) & ROAD_NW)) n |= ROAD_SE;
	if ((present & ROAD_SW) && (GetAnyRoadBits(TILE_ADDXY(tile,  1,  0), rt) & ROAD_NE)) n |= ROAD_SW;
	if ((present & ROAD_NW) && (GetAnyRoadBits(TILE_ADDXY(tile,  0, -1), rt) & ROAD_SE)) n |= ROAD_NW;

	int rating_decrease = RATING_ROAD_DOWN_STEP_EDGE;
	/* If 0 or 1 bits are set in n, or if no bits that match the bits to remove,
	 * then allow it */
	if (KillFirstBit(n) != ROAD_NONE && (n & remove) != ROAD_NONE) {
		/* you can remove all kind of roads with extra dynamite */
		if (!_settings_game.construction.extra_dynamite) {
			SetDParam(0, t->index);
			return_cmd_error(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
		}
		rating_decrease = RATING_ROAD_DOWN_STEP_INNER;
	}
	ChangeTownRating(t, rating_decrease, RATING_ROAD_MINIMUM, flags);

	return CommandCost();
}


/**
 * Delete a piece of road.
 * @param tile tile where to remove road from
 * @param flags operation to perform
 * @param pieces roadbits to remove
 * @param rt roadtype to remove
 * @param crossing_check should we check if there is a tram track when we are removing road from crossing?
 * @param town_check should we check if the town allows removal?
 */
static CommandCost RemoveRoad(TileIndex tile, DoCommandFlag flags, RoadBits pieces, RoadType rt, bool crossing_check, bool town_check = true)
{
	RoadTypes rts = GetRoadTypes(tile);
	/* The tile doesn't have the given road type */
	if (!HasBit(rts, rt)) return_cmd_error(rt == ROADTYPE_TRAM ? STR_ERROR_THERE_IS_NO_TRAMWAY : STR_ERROR_THERE_IS_NO_ROAD);

	switch (GetTileType(tile)) {
		case MP_ROAD: {
			CommandCost ret = EnsureNoVehicleOnGround(tile);
			if (ret.Failed()) return ret;
			break;
		}

		case MP_STATION: {
			if (!IsDriveThroughStopTile(tile)) return CMD_ERROR;

			CommandCost ret = EnsureNoVehicleOnGround(tile);
			if (ret.Failed()) return ret;
			break;
		}

		case MP_TUNNELBRIDGE: {
			if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return CMD_ERROR;
			CommandCost ret = TunnelBridgeIsFree(tile, GetOtherTunnelBridgeEnd(tile));
			if (ret.Failed()) return ret;
			break;
		}

		default:
			return CMD_ERROR;
	}

	CommandCost ret = CheckAllowRemoveRoad(tile, pieces, GetRoadOwner(tile, rt), rt, flags, town_check);
	if (ret.Failed()) return ret;

	if (!IsTileType(tile, MP_ROAD)) {
		/* If it's the last roadtype, just clear the whole tile */
		if (rts == RoadTypeToRoadTypes(rt)) return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);

		CommandCost cost(EXPENSES_CONSTRUCTION);
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			TileIndex other_end = GetOtherTunnelBridgeEnd(tile);
			/* Pay for *every* tile of the bridge or tunnel */
			cost.AddCost((GetTunnelBridgeLength(other_end, tile) + 2) * _price[PR_CLEAR_ROAD]);
			if (flags & DC_EXEC) {
				SetRoadTypes(other_end, GetRoadTypes(other_end) & ~RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) & ~RoadTypeToRoadTypes(rt));

				/* If the owner of the bridge sells all its road, also move the ownership
				 * to the owner of the other roadtype. */
				RoadType other_rt = (rt == ROADTYPE_ROAD) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
				Owner other_owner = GetRoadOwner(tile, other_rt);
				if (other_owner != GetTileOwner(tile)) {
					SetTileOwner(tile, other_owner);
					SetTileOwner(other_end, other_owner);
				}

				/* Mark tiles diry that have been repaved */
				MarkTileDirtyByTile(tile);
				MarkTileDirtyByTile(other_end);
				if (IsBridge(tile)) {
					TileIndexDiff delta = TileOffsByDiagDir(GetTunnelBridgeDirection(tile));

					for (TileIndex t = tile + delta; t != other_end; t += delta) MarkTileDirtyByTile(t);
				}
			}
		} else {
			assert(IsDriveThroughStopTile(tile));
			cost.AddCost(_price[PR_CLEAR_ROAD] * 2);
			if (flags & DC_EXEC) {
				SetRoadTypes(tile, GetRoadTypes(tile) & ~RoadTypeToRoadTypes(rt));
				MarkTileDirtyByTile(tile);
			}
		}
		return cost;
	}

	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_NORMAL: {
			const Slope tileh = GetTileSlope(tile, NULL);
			RoadBits present = GetRoadBits(tile, rt);
			const RoadBits other = GetOtherRoadBits(tile, rt);
			const Foundation f = GetRoadFoundation(tileh, present);

			if (HasRoadWorks(tile) && _current_company != OWNER_WATER) return_cmd_error(STR_ERROR_ROAD_WORKS_IN_PROGRESS);

			/* Autocomplete to a straight road
			 * @li on steep slopes
			 * @li if the bits of the other roadtypes result in another foundation
			 * @li if build on slopes is disabled */
			if (IsSteepSlope(tileh) || (IsStraightRoad(other) &&
					(other & _invalid_tileh_slopes_road[0][tileh & SLOPE_ELEVATED]) != ROAD_NONE) ||
					(tileh != SLOPE_FLAT && !_settings_game.construction.build_on_slopes)) {
				pieces |= MirrorRoadBits(pieces);
			}

			/* limit the bits to delete to the existing bits. */
			pieces &= present;
			if (pieces == ROAD_NONE) return_cmd_error(rt == ROADTYPE_TRAM ? STR_ERROR_THERE_IS_NO_TRAMWAY : STR_ERROR_THERE_IS_NO_ROAD);

			/* Now set present what it will be after the remove */
			present ^= pieces;

			/* Check for invalid RoadBit combinations on slopes */
			if (tileh != SLOPE_FLAT && present != ROAD_NONE &&
					(present & _invalid_tileh_slopes_road[0][tileh & SLOPE_ELEVATED]) == present) {
				return CMD_ERROR;
			}

			if (flags & DC_EXEC) {
				if (HasRoadWorks(tile)) {
					/* flooding tile with road works, don't forget to remove the effect vehicle too */
					assert(_current_company == OWNER_WATER);
					EffectVehicle *v;
					FOR_ALL_EFFECTVEHICLES(v) {
						if (TileVirtXY(v->x_pos, v->y_pos) == tile) {
							delete v;
						}
					}
				}
				if (present == ROAD_NONE) {
					RoadTypes rts = GetRoadTypes(tile) & ComplementRoadTypes(RoadTypeToRoadTypes(rt));
					if (rts == ROADTYPES_NONE) {
						/* Includes MarkTileDirtyByTile() */
						DoClearSquare(tile);
					} else {
						if (rt == ROADTYPE_ROAD && IsRoadOwner(tile, ROADTYPE_ROAD, OWNER_TOWN)) {
							/* Update nearest-town index */
							const Town *town = CalcClosestTownFromTile(tile);
							SetTownIndex(tile, town == NULL ? (TownID)INVALID_TOWN : town->index);
						}
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

			CommandCost cost(EXPENSES_CONSTRUCTION, CountBits(pieces) * _price[PR_CLEAR_ROAD]);
			/* If we build a foundation we have to pay for it. */
			if (f == FOUNDATION_NONE && GetRoadFoundation(tileh, present) != FOUNDATION_NONE) cost.AddCost(_price[PR_BUILD_FOUNDATION]);

			return cost;
		}

		case ROAD_TILE_CROSSING: {
			if (pieces & ComplementRoadBits(GetCrossingRoadBits(tile))) {
				return CMD_ERROR;
			}

			/* Don't allow road to be removed from the crossing when there is tram;
			 * we can't draw the crossing without roadbits ;) */
			if (rt == ROADTYPE_ROAD && HasTileRoadType(tile, ROADTYPE_TRAM) && (flags & DC_EXEC || crossing_check)) return CMD_ERROR;

			if (flags & DC_EXEC) {
				Track railtrack = GetCrossingRailTrack(tile);
				RoadTypes rts = GetRoadTypes(tile) & ComplementRoadTypes(RoadTypeToRoadTypes(rt));
				if (rts == ROADTYPES_NONE) {
					TrackBits tracks = GetCrossingRailBits(tile);
					bool reserved = HasCrossingReservation(tile);
					MakeRailNormal(tile, GetTileOwner(tile), tracks, GetRailType(tile));
					if (reserved) SetTrackReservation(tile, tracks);
				} else {
					SetRoadTypes(tile, rts);
					/* If we ever get HWAY and it is possible without road then we will need to promote ownership and invalidate town index here, too */
				}
				MarkTileDirtyByTile(tile);
				YapfNotifyTrackLayoutChange(tile, railtrack);
			}
			return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_ROAD] * 2);
		}

		default:
		case ROAD_TILE_DEPOT:
			return CMD_ERROR;
	}
}


/**
 * Calculate the costs for roads on slopes
 *  Aside modify the RoadBits to fit on the slopes
 *
 * @note The RoadBits are modified too!
 * @param tileh The current slope
 * @param pieces The RoadBits we want to add
 * @param existing The existent RoadBits of the current type
 * @param other The other existent RoadBits
 * @return The costs for these RoadBits on this slope
 */
static CommandCost CheckRoadSlope(Slope tileh, RoadBits *pieces, RoadBits existing, RoadBits other)
{
	/* Remove already build pieces */
	CLRBITS(*pieces, existing);

	/* If we can't build anything stop here */
	if (*pieces == ROAD_NONE) return CMD_ERROR;

	/* All RoadBit combos are valid on flat land */
	if (tileh == SLOPE_FLAT) return CommandCost();

	/* Proceed steep Slopes first to reduce lookup table size */
	if (IsSteepSlope(tileh)) {
		/* Force straight roads. */
		*pieces |= MirrorRoadBits(*pieces);

		/* Use existing as all existing since only straight
		 * roads are allowed here. */
		existing |= other;

		if ((existing == ROAD_NONE || existing == *pieces) && IsStraightRoad(*pieces)) {
			return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
		}
		return CMD_ERROR;
	}

	/* Save the merge of all bits of the current type */
	RoadBits type_bits = existing | *pieces;

	/* Roads on slopes */
	if (_settings_game.construction.build_on_slopes && (_invalid_tileh_slopes_road[0][tileh] & (other | type_bits)) == ROAD_NONE) {

		/* If we add leveling we've got to pay for it */
		if ((other | existing) == ROAD_NONE) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);

		return CommandCost();
	}

	/* Autocomplete uphill roads */
	*pieces |= MirrorRoadBits(*pieces);
	type_bits = existing | *pieces;

	/* Uphill roads */
	if (IsStraightRoad(type_bits) && (other == type_bits || other == ROAD_NONE) &&
			(_invalid_tileh_slopes_road[1][tileh] & (other | type_bits)) == ROAD_NONE) {

		/* Slopes with foundation ? */
		if (IsSlopeWithOneCornerRaised(tileh)) {

			/* Prevent build on slopes if it isn't allowed */
			if (_settings_game.construction.build_on_slopes) {

				/* If we add foundation we've got to pay for it */
				if ((other | existing) == ROAD_NONE) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);

				return CommandCost();
			}
		} else {
			if (HasExactlyOneBit(existing) && GetRoadFoundation(tileh, existing) == FOUNDATION_NONE) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
			return CommandCost();
		}
	}
	return CMD_ERROR;
}

/**
 * Build a piece of road.
 * @param tile tile where to build road
 * @param flags operation to perform
 * @param p1 bit 0..3 road pieces to build (RoadBits)
 *           bit 4..5 road type
 *           bit 6..7 disallowed directions to toggle
 * @param p2 the town that is building the road (0 if not applicable)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildRoad(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);

	RoadBits existing = ROAD_NONE;
	RoadBits other_bits = ROAD_NONE;

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-company is building the road */
	if ((Company::IsValidID(_current_company) && p2 != 0) || (_current_company == OWNER_TOWN && !Town::IsValidID(p2))) return CMD_ERROR;
	if (_current_company != OWNER_TOWN) {
		const Town *town = CalcClosestTownFromTile(tile);
		p2 = (town != NULL) ? town->index : (TownID)INVALID_TOWN;
	}

	RoadBits pieces = Extract<RoadBits, 0, 4>(p1);

	/* do not allow building 'zero' road bits, code wouldn't handle it */
	if (pieces == ROAD_NONE) return CMD_ERROR;

	RoadType rt = Extract<RoadType, 4, 2>(p1);
	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	DisallowedRoadDirections toggle_drd = Extract<DisallowedRoadDirections, 6, 2>(p1);

	Slope tileh = GetTileSlope(tile, NULL);

	bool need_to_clear = false;
	switch (GetTileType(tile)) {
		case MP_ROAD:
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL: {
					if (HasRoadWorks(tile)) return_cmd_error(STR_ERROR_ROAD_WORKS_IN_PROGRESS);

					other_bits = GetOtherRoadBits(tile, rt);
					if (!HasTileRoadType(tile, rt)) break;

					existing = GetRoadBits(tile, rt);
					bool crossing = !IsStraightRoad(existing | pieces);
					if (rt != ROADTYPE_TRAM && (GetDisallowedRoadDirections(tile) != DRD_NONE || toggle_drd != DRD_NONE) && crossing) {
						/* Junctions cannot be one-way */
						return_cmd_error(STR_ERROR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);
					}
					if ((existing & pieces) == pieces) {
						/* We only want to set the (dis)allowed road directions */
						if (toggle_drd != DRD_NONE && rt != ROADTYPE_TRAM) {
							if (crossing) return_cmd_error(STR_ERROR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);

							Owner owner = GetRoadOwner(tile, ROADTYPE_ROAD);
							if (owner != OWNER_NONE) {
								CommandCost ret = CheckOwnership(owner, tile);
								if (ret.Failed()) return ret;
							}

							DisallowedRoadDirections dis_existing = GetDisallowedRoadDirections(tile);
							DisallowedRoadDirections dis_new      = dis_existing ^ toggle_drd;

							/* We allow removing disallowed directions to break up
							 * deadlocks, but adding them can break articulated
							 * vehicles. As such, only when less is disallowed,
							 * i.e. bits are removed, we skip the vehicle check. */
							if (CountBits(dis_existing) <= CountBits(dis_new)) {
								CommandCost ret = EnsureNoVehicleOnGround(tile);
								if (ret.Failed()) return ret;
							}

							/* Ignore half built tiles */
							if ((flags & DC_EXEC) && rt != ROADTYPE_TRAM && IsStraightRoad(existing)) {
								SetDisallowedRoadDirections(tile, dis_new);
								MarkTileDirtyByTile(tile);
							}
							return CommandCost();
						}
						return_cmd_error(STR_ERROR_ALREADY_BUILT);
					}
					break;
				}

				case ROAD_TILE_CROSSING:
					other_bits = GetCrossingRoadBits(tile);
					if (pieces & ComplementRoadBits(other_bits)) goto do_clear;
					pieces = other_bits; // we need to pay for both roadbits

					if (HasTileRoadType(tile, rt)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
					break;

				case ROAD_TILE_DEPOT:
					if ((GetAnyRoadBits(tile, rt) & pieces) == pieces) return_cmd_error(STR_ERROR_ALREADY_BUILT);
					goto do_clear;

				default: NOT_REACHED();
			}
			break;

		case MP_RAILWAY: {
			if (IsSteepSlope(tileh)) {
				return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
			}

			/* Level crossings may only be built on these slopes */
			if (!HasBit(VALID_LEVEL_CROSSING_SLOPES, tileh)) {
				return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
			}

			if (GetRailTileType(tile) != RAIL_TILE_NORMAL) goto do_clear;

			if (RailNoLevelCrossings(GetRailType(tile))) {
				return_cmd_error(STR_ERROR_CROSSING_DISALLOWED);
			}

			Axis roaddir;
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

			CommandCost ret = EnsureNoVehicleOnGround(tile);
			if (ret.Failed()) return ret;

			if (flags & DC_EXEC) {
				Track railtrack = AxisToTrack(OtherAxis(roaddir));
				YapfNotifyTrackLayoutChange(tile, railtrack);
				/* Always add road to the roadtypes (can't draw without it) */
				bool reserved = HasBit(GetRailReservationTrackBits(tile), railtrack);
				MakeRoadCrossing(tile, _current_company, _current_company, GetTileOwner(tile), roaddir, GetRailType(tile), RoadTypeToRoadTypes(rt) | ROADTYPES_ROAD, p2);
				SetCrossingReservation(tile, reserved);
				UpdateLevelCrossing(tile, false);
				MarkTileDirtyByTile(tile);
			}
			return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_ROAD] * (rt == ROADTYPE_ROAD ? 2 : 4));
		}

		case MP_STATION: {
			if ((GetAnyRoadBits(tile, rt) & pieces) == pieces) return_cmd_error(STR_ERROR_ALREADY_BUILT);
			if (!IsDriveThroughStopTile(tile)) goto do_clear;

			RoadBits curbits = AxisToRoadBits(DiagDirToAxis(GetRoadStopDir(tile)));
			if (pieces & ~curbits) goto do_clear;
			pieces = curbits; // we need to pay for both roadbits

			if (HasTileRoadType(tile, rt)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
			break;
		}

		case MP_TUNNELBRIDGE: {
			if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) goto do_clear;
			if (MirrorRoadBits(DiagDirToRoadBits(GetTunnelBridgeDirection(tile))) != pieces) goto do_clear;
			if (HasTileRoadType(tile, rt)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
			/* Don't allow adding roadtype to the bridge/tunnel when vehicles are already driving on it */
			CommandCost ret = TunnelBridgeIsFree(tile, GetOtherTunnelBridgeEnd(tile));
			if (ret.Failed()) return ret;
			break;
		}

		default: {
do_clear:;
			need_to_clear = true;
			break;
		}
	}

	if (need_to_clear) {
		CommandCost ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;
		cost.AddCost(ret);
	}

	if (other_bits != pieces) {
		/* Check the foundation/slopes when adding road/tram bits */
		CommandCost ret = CheckRoadSlope(tileh, &pieces, existing, other_bits);
		/* Return an error if we need to build a foundation (ret != 0) but the
		 * current setting is turned off */
		if (ret.Failed() || (ret.GetCost() != 0 && !_settings_game.construction.build_on_slopes)) {
			return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
		}
		cost.AddCost(ret);
	}

	if (!need_to_clear) {
		if (IsTileType(tile, MP_ROAD)) {
			/* Don't put the pieces that already exist */
			pieces &= ComplementRoadBits(existing);

			/* Check if new road bits will have the same foundation as other existing road types */
			if (IsNormalRoad(tile)) {
				Slope slope = GetTileSlope(tile, NULL);
				Foundation found_new = GetRoadFoundation(slope, pieces | existing);

				/* Test if all other roadtypes can be built at that foundation */
				for (RoadType rtest = ROADTYPE_ROAD; rtest < ROADTYPE_END; rtest++) {
					if (rtest != rt) { // check only other road types
						RoadBits bits = GetRoadBits(tile, rtest);
						/* do not check if there are not road bits of given type */
						if (bits != ROAD_NONE && GetRoadFoundation(slope, bits) != found_new) {
							return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
						}
					}
				}
			}
		}

		CommandCost ret = EnsureNoVehicleOnGround(tile);
		if (ret.Failed()) return ret;

	}
	cost.AddCost(CountBits(pieces) * _price[PR_BUILD_ROAD]);

	if (!need_to_clear && IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* Pay for *every* tile of the bridge or tunnel */
		cost.MultiplyCost(GetTunnelBridgeLength(GetOtherTunnelBridgeEnd(tile), tile) + 2);
	}

	if (flags & DC_EXEC) {
		switch (GetTileType(tile)) {
			case MP_ROAD: {
				RoadTileType rtt = GetRoadTileType(tile);
				if (existing == ROAD_NONE || rtt == ROAD_TILE_CROSSING) {
					SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
					SetRoadOwner(tile, rt, _current_company);
					if (rt == ROADTYPE_ROAD) SetTownIndex(tile, p2);
				}
				if (rtt != ROAD_TILE_CROSSING) SetRoadBits(tile, existing | pieces, rt);
				break;
			}

			case MP_TUNNELBRIDGE: {
				TileIndex other_end = GetOtherTunnelBridgeEnd(tile);

				SetRoadTypes(other_end, GetRoadTypes(other_end) | RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
				SetRoadOwner(other_end, rt, _current_company);
				SetRoadOwner(tile, rt, _current_company);

				/* Mark tiles diry that have been repaved */
				MarkTileDirtyByTile(other_end);
				MarkTileDirtyByTile(tile);
				if (IsBridge(tile)) {
					TileIndexDiff delta = TileOffsByDiagDir(GetTunnelBridgeDirection(tile));

					for (TileIndex t = tile + delta; t != other_end; t += delta) MarkTileDirtyByTile(t);
				}
				break;
			}

			case MP_STATION:
				assert(IsDriveThroughStopTile(tile));
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
				SetRoadOwner(tile, rt, _current_company);
				break;

			default:
				MakeRoadNormal(tile, pieces, RoadTypeToRoadTypes(rt), p2, _current_company, _current_company);
				break;
		}

		if (rt != ROADTYPE_TRAM && IsNormalRoadTile(tile)) {
			existing |= pieces;
			SetDisallowedRoadDirections(tile, IsStraightRoad(existing) ?
					GetDisallowedRoadDirections(tile) ^ toggle_drd : DRD_NONE);
		}

		MarkTileDirtyByTile(tile);
	}
	return cost;
}

/**
 * Build a long piece of road.
 * @param start_tile start tile of drag (the building cost will appear over this tile)
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 + 4) - road type
 * - p2 = (bit 5) - set road direction
 * - p2 = (bit 6) - 0 = build up to an obstacle, 1 = fail if an obstacle is found (used for AIs).
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildLongRoad(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DisallowedRoadDirections drd = DRD_NORTHBOUND;

	if (p1 >= MapSize()) return CMD_ERROR;

	TileIndex end_tile = p1;
	RoadType rt = Extract<RoadType, 3, 2>(p2);
	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	Axis axis = Extract<Axis, 2, 1>(p2);
	/* Only drag in X or Y direction dictated by the direction variable */
	if (axis == AXIS_X && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (axis == AXIS_Y && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	DiagDirection dir = AxisToDiagDir(axis);

	/* Swap direction, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HasBit(p2, 0))) {
		dir = ReverseDiagDir(dir);
		p2 ^= 3;
		drd = DRD_SOUTHBOUND;
	}

	/* On the X-axis, we have to swap the initial bits, so they
	 * will be interpreted correctly in the GTTS. Futhermore
	 * when you just 'click' on one tile to build them. */
	if ((axis == AXIS_Y) == (start_tile == end_tile && HasBit(p2, 0) == HasBit(p2, 1))) drd ^= DRD_BOTH;
	/* No disallowed direction bits have to be toggled */
	if (!HasBit(p2, 5)) drd = DRD_NONE;

	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error = CMD_ERROR;
	TileIndex tile = start_tile;
	bool had_bridge = false;
	bool had_tunnel = false;
	bool had_success = false;
	/* Start tile is the first tile clicked by the user. */
	for (;;) {
		RoadBits bits = AxisToRoadBits(axis);

		/* Road parts only have to be built at the start tile or at the end tile. */
		if (tile == end_tile && !HasBit(p2, 1)) bits &= DiagDirToRoadBits(ReverseDiagDir(dir));
		if (tile == start_tile && HasBit(p2, 0)) bits &= DiagDirToRoadBits(dir);

		CommandCost ret = DoCommand(tile, drd << 6 | rt << 4 | bits, 0, flags, CMD_BUILD_ROAD);
		if (ret.Failed()) {
			last_error = ret;
			if (last_error.GetErrorMessage() != STR_ERROR_ALREADY_BUILT) {
				if (HasBit(p2, 6)) return last_error;
				break;
			}
		} else {
			had_success = true;
			/* Only pay for the upgrade on one side of the bridges and tunnels */
			if (IsTileType(tile, MP_TUNNELBRIDGE)) {
				if (IsBridge(tile)) {
					if (!had_bridge || GetTunnelBridgeDirection(tile) == dir) {
						cost.AddCost(ret);
					}
					had_bridge = true;
				} else { // IsTunnel(tile)
					if (!had_tunnel || GetTunnelBridgeDirection(tile) == dir) {
						cost.AddCost(ret);
					}
					had_tunnel = true;
				}
			} else {
				cost.AddCost(ret);
			}
		}

		if (tile == end_tile) break;

		tile += TileOffsByDiagDir(dir);
	}

	return had_success ? cost : last_error;
}

/**
 * Remove a long piece of road.
 * @param start_tile start tile of drag
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 + 4) - road type
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveLongRoad(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);

	if (p1 >= MapSize()) return CMD_ERROR;

	TileIndex end_tile = p1;
	RoadType rt = Extract<RoadType, 3, 2>(p2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

	Axis axis = Extract<Axis, 2, 1>(p2);
	/* Only drag in X or Y direction dictated by the direction variable */
	if (axis == AXIS_X && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (axis == AXIS_Y && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	/* Swap start and ending tile, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HasBit(p2, 0))) {
		TileIndex t = start_tile;
		start_tile = end_tile;
		end_tile = t;
		p2 ^= IsInsideMM(p2 & 3, 1, 3) ? 3 : 0;
	}

	Money money = GetAvailableMoneyForCommand();
	TileIndex tile = start_tile;
	CommandCost last_error = CMD_ERROR;
	bool had_success = false;
	/* Start tile is the small number. */
	for (;;) {
		RoadBits bits = AxisToRoadBits(axis);

		if (tile == end_tile && !HasBit(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HasBit(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		/* try to remove the halves. */
		if (bits != 0) {
			CommandCost ret = RemoveRoad(tile, flags & ~DC_EXEC, bits, rt, true);
			if (ret.Succeeded()) {
				if (flags & DC_EXEC) {
					money -= ret.GetCost();
					if (money < 0) {
						_additional_cash_required = DoCommand(start_tile, end_tile, p2, flags & ~DC_EXEC, CMD_REMOVE_LONG_ROAD).GetCost();
						return cost;
					}
					RemoveRoad(tile, flags, bits, rt, true, false);
				}
				cost.AddCost(ret);
				had_success = true;
			} else {
				/* Ownership errors are more important. */
				if (last_error.GetErrorMessage() != STR_ERROR_OWNED_BY) last_error = ret;
			}
		}

		if (tile == end_tile) break;

		tile += (axis == AXIS_Y) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return had_success ? cost : last_error;
}

/**
 * Build a road depot.
 * @param tile tile where to build the depot
 * @param flags operation to perform
 * @param p1 bit 0..1 entrance direction (DiagDirection)
 *           bit 2..3 road type
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 *
 * @todo When checking for the tile slope,
 * distingush between "Flat land required" and "land sloped in wrong direction"
 */
CommandCost CmdBuildRoadDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DiagDirection dir = Extract<DiagDirection, 0, 2>(p1);
	RoadType rt = Extract<RoadType, 2, 2>(p1);

	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	Slope tileh = GetTileSlope(tile, NULL);
	if (tileh != SLOPE_FLAT && (
				!_settings_game.construction.build_on_slopes ||
				IsSteepSlope(tileh) ||
				!CanBuildDepotByTileh(dir, tileh)
			)) {
		return_cmd_error(STR_ERROR_FLAT_LAND_REQUIRED);
	}

	CommandCost cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (cost.Failed()) return cost;

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	if (!Depot::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Depot *dep = new Depot(tile);
		dep->build_date = _date;

		MakeRoadDepot(tile, _current_company, dep->index, dir, rt);
		MarkTileDirtyByTile(tile);
		MakeDefaultName(dep);
	}
	cost.AddCost(_price[PR_BUILD_DEPOT_ROAD]);
	return cost;
}

static CommandCost RemoveRoadDepot(TileIndex tile, DoCommandFlag flags)
{
	if (_current_company != OWNER_WATER) {
		CommandCost ret = CheckTileOwnership(tile);
		if (ret.Failed()) return ret;
	}

	CommandCost ret = EnsureNoVehicleOnGround(tile);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		delete Depot::GetByTile(tile);
		DoClearSquare(tile);
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_DEPOT_ROAD]);
}

static CommandCost ClearTile_Road(TileIndex tile, DoCommandFlag flags)
{
	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_NORMAL: {
			RoadBits b = GetAllRoadBits(tile);

			/* Clear the road if only one piece is on the tile OR we are not using the DC_AUTO flag */
			if ((HasExactlyOneBit(b) && GetRoadBits(tile, ROADTYPE_TRAM) == ROAD_NONE) || !(flags & DC_AUTO)) {
				CommandCost ret(EXPENSES_CONSTRUCTION);
				RoadType rt;
				FOR_EACH_SET_ROADTYPE(rt, GetRoadTypes(tile)) {
					CommandCost tmp_ret = RemoveRoad(tile, flags, GetRoadBits(tile, rt), rt, true);
					if (tmp_ret.Failed()) return tmp_ret;
					ret.AddCost(tmp_ret);
				}
				return ret;
			}
			return_cmd_error(STR_ERROR_MUST_REMOVE_ROAD_FIRST);
		}

		case ROAD_TILE_CROSSING: {
			RoadTypes rts = GetRoadTypes(tile);
			CommandCost ret(EXPENSES_CONSTRUCTION);

			if (flags & DC_AUTO) return_cmd_error(STR_ERROR_MUST_REMOVE_ROAD_FIRST);

			/* Must iterate over the roadtypes in a reverse manner because
			 * tram tracks must be removed before the road bits. */
			RoadType rt = ROADTYPE_TRAM;
			do {
				if (HasBit(rts, rt)) {
					CommandCost tmp_ret = RemoveRoad(tile, flags, GetCrossingRoadBits(tile), rt, false);
					if (tmp_ret.Failed()) return tmp_ret;
					ret.AddCost(tmp_ret);
				}
			} while (rt-- != ROADTYPE_ROAD);

			if (flags & DC_EXEC) {
				DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			}
			return ret;
		}

		default:
		case ROAD_TILE_DEPOT:
			if (flags & DC_AUTO) {
				return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
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

/**
 * Get the foundationtype of a RoadBits Slope combination
 *
 * @param tileh The Slope part
 * @param bits The RoadBits part
 * @return The resulting Foundation
 */
static Foundation GetRoadFoundation(Slope tileh, RoadBits bits)
{
	/* Flat land and land without a road doesn't require a foundation */
	if (tileh == SLOPE_FLAT || bits == ROAD_NONE) return FOUNDATION_NONE;

	if (!IsSteepSlope(tileh)) {
		/* Leveled RoadBits on a slope */
		if ((_invalid_tileh_slopes_road[0][tileh] & bits) == ROAD_NONE) return FOUNDATION_LEVELED;

		/* Straight roads without foundation on a slope */
		if (!IsSlopeWithOneCornerRaised(tileh) &&
				(_invalid_tileh_slopes_road[1][tileh] & bits) == ROAD_NONE)
			return FOUNDATION_NONE;
	}

	/* Roads on steep Slopes or on Slopes with one corner raised */
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
			!(_settings_game.game_creation.landscape == LT_TROPIC && HasGrfMiscBit(GMB_DESERT_PAVED_ROADS) &&
				roadside != ROADSIDE_BARREN && roadside != ROADSIDE_GRASS && roadside != ROADSIDE_GRASS_ROAD_WORKS));
}

/**
 * Draws the catenary for the given tile
 * @param ti   information about the tile (slopes, height etc)
 * @param tram the roadbits for the tram
 */
void DrawTramCatenary(const TileInfo *ti, RoadBits tram)
{
	/* Do not draw catenary if it is invisible */
	if (IsInvisibilitySet(TO_CATENARY)) return;

	/* Don't draw the catenary under a low bridge */
	if (MayHaveBridgeAbove(ti->tile) && IsBridgeAbove(ti->tile) && !IsTransparencySet(TO_CATENARY)) {
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

	AddSortableSpriteToDraw(back,  PAL_NONE, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_CATENARY));
	AddSortableSpriteToDraw(front, PAL_NONE, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_CATENARY));
}

/**
 * Draws details on/around the road
 * @param img the sprite to draw
 * @param ti  the tile to draw on
 * @param dx  the offset from the top of the BB of the tile
 * @param dy  the offset from the top of the BB of the tile
 * @param h   the height of the sprite to draw
 */
static void DrawRoadDetail(SpriteID img, const TileInfo *ti, int dx, int dy, int h)
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
static void DrawRoadBits(TileInfo *ti)
{
	RoadBits road = GetRoadBits(ti->tile, ROADTYPE_ROAD);
	RoadBits tram = GetRoadBits(ti->tile, ROADTYPE_TRAM);

	SpriteID image = 0;
	PaletteID pal = PAL_NONE;

	if (ti->tileh != SLOPE_FLAT) {
		DrawFoundation(ti, GetRoadFoundation(ti->tileh, road | tram));

		/* DrawFoundation() modifies ti.
		 * Default sloped sprites.. */
		if (ti->tileh != SLOPE_FLAT) image = _road_sloped_sprites[ti->tileh - 1] + 0x53F;
	}

	if (image == 0) image = _road_tile_sprites_1[road != ROAD_NONE ? road : tram];

	Roadside roadside = GetRoadside(ti->tile);

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
			DrawGroundSpriteAt(SPR_ONEWAY_BASE + drd - 1 + ((road == ROAD_X) ? 0 : 3), PAL_NONE, 8, 8, GetPartialZ(8, 8, ti->tileh));
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

	/* Do not draw details (street lights, trees) under low bridge */
	if (MayHaveBridgeAbove(ti->tile) && IsBridgeAbove(ti->tile) && (roadside == ROADSIDE_TREES || roadside == ROADSIDE_STREET_LIGHTS)) {
		uint height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));
		uint minz = GetTileMaxZ(ti->tile) + 2 * TILE_HEIGHT;

		if (roadside == ROADSIDE_TREES) minz += TILE_HEIGHT;

		if (height < minz) return;
	}

	/* If there are no road bits, return, as there is nothing left to do */
	if (HasAtMostOneBit(road)) return;

	/* Draw extra details. */
	for (const DrawRoadTileStruct *drts = _road_display_table[roadside][road | tram]; drts->image != 0; drts++) {
		DrawRoadDetail(drts->image, ti, drts->subcoord_x, drts->subcoord_y, 0x10);
	}
}

/** Tile callback function for rendering a road tile to the screen */
static void DrawTile_Road(TileInfo *ti)
{
	switch (GetRoadTileType(ti->tile)) {
		case ROAD_TILE_NORMAL:
			DrawRoadBits(ti);
			break;

		case ROAD_TILE_CROSSING: {
			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

			PaletteID pal = PAL_NONE;
			const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));

			if (rti->UsesOverlay()) {
				Axis axis = GetCrossingRailAxis(ti->tile);
				SpriteID road = SPR_ROAD_Y + axis;

				Roadside roadside = GetRoadside(ti->tile);

				if (AlwaysDrawUnpavedRoads(ti->tile, roadside)) {
					road += 19;
				} else {
					switch (roadside) {
						case ROADSIDE_BARREN: pal = PALETTE_TO_BARE_LAND; break;
						case ROADSIDE_GRASS:  break;
						default:              road -= 19; break; // Paved
					}
				}

				DrawGroundSprite(road, pal);

				SpriteID rail = GetCustomRailSprite(rti, ti->tile, RTSG_CROSSING) + axis;
				/* Draw tracks, but draw PBS reserved tracks darker. */
				pal = (_game_mode != GM_MENU && _settings_client.gui.show_track_reservation && HasCrossingReservation(ti->tile)) ? PALETTE_CRASH : PAL_NONE;
				DrawGroundSprite(rail, pal);

				DrawRailTileSeq(ti, &_crossing_layout, TO_CATENARY, rail, 0, PAL_NONE);
			} else {
				SpriteID image = rti->base_sprites.crossing;

				if (GetCrossingRoadAxis(ti->tile) == AXIS_X) image++;
				if (IsCrossingBarred(ti->tile)) image += 2;

				Roadside roadside = GetRoadside(ti->tile);

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

				/* PBS debugging, draw reserved tracks darker */
				if (_game_mode != GM_MENU && _settings_client.gui.show_track_reservation && HasCrossingReservation(ti->tile)) {
					DrawGroundSprite(GetCrossingRoadAxis(ti->tile) == AXIS_Y ? GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.single_x : GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.single_y, PALETTE_CRASH);
				}
			}

			if (HasTileRoadType(ti->tile, ROADTYPE_TRAM)) {
				DrawGroundSprite(SPR_TRAMWAY_OVERLAY + (GetCrossingRoadAxis(ti->tile) ^ 1), pal);
				DrawTramCatenary(ti, GetCrossingRoadBits(ti->tile));
			}
			if (HasCatenaryDrawn(GetRailType(ti->tile))) DrawCatenary(ti);
			break;
		}

		default:
		case ROAD_TILE_DEPOT: {
			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

			PaletteID palette = COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile));

			const DrawTileSprites *dts;
			if (HasTileRoadType(ti->tile, ROADTYPE_TRAM)) {
				dts =  &_tram_depot[GetRoadDepotDirection(ti->tile)];
			} else {
				dts =  &_road_depot[GetRoadDepotDirection(ti->tile)];
			}

			DrawGroundSprite(dts->ground.sprite, PAL_NONE);
			DrawOrigTileSeq(ti, dts, TO_BUILDINGS, palette);
			break;
		}
	}
	DrawBridgeMiddle(ti);
}

void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadType rt)
{
	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);
	const DrawTileSprites *dts = (rt == ROADTYPE_TRAM) ? &_tram_depot[dir] : &_road_depot[dir];

	x += 33;
	y += 17;

	DrawSprite(dts->ground.sprite, PAL_NONE, x, y);
	DrawOrigTileSeqInGUI(x, y, dts, palette);
}

/**
 * Updates cached nearest town for all road tiles
 * @param invalidate are we just invalidating cached data?
 * @pre invalidate == true implies _generating_world == true
 */
void UpdateNearestTownForRoadTiles(bool invalidate)
{
	assert(!invalidate || _generating_world);

	for (TileIndex t = 0; t < MapSize(); t++) {
		if (IsTileType(t, MP_ROAD) && !IsRoadDepot(t) && !HasTownOwnedRoad(t)) {
			TownID tid = (TownID)INVALID_TOWN;
			if (!invalidate) {
				const Town *town = CalcClosestTownFromTile(t);
				if (town != NULL) tid = town->index;
			}
			SetTownIndex(t, tid);
		}
	}
}

static uint GetSlopeZ_Road(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	if (tileh == SLOPE_FLAT) return z;
	if (IsNormalRoad(tile)) {
		Foundation f = GetRoadFoundation(tileh, GetAllRoadBits(tile));
		z += ApplyFoundationToSlope(f, &tileh);
		return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
	} else {
		return z + TILE_HEIGHT;
	}
}

static Foundation GetFoundation_Road(TileIndex tile, Slope tileh)
{
	if (IsNormalRoad(tile)) {
		return GetRoadFoundation(tileh, GetAllRoadBits(tile));
	} else {
		return FlatteningFoundation(tileh);
	}
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
	switch (_settings_game.game_creation.landscape) {
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

	if (IsRoadDepot(tile)) return;

	const Town *t = ClosestTownFromTile(tile, UINT_MAX);
	if (!HasRoadWorks(tile)) {
		HouseZonesBits grp = HZB_TOWN_EDGE;

		if (t != NULL) {
			grp = GetTownRadiusGroup(t, tile);

			/* Show an animation to indicate road work */
			if (t->road_build_months != 0 &&
					(DistanceManhattan(t->xy, tile) < 8 || grp != HZB_TOWN_EDGE) &&
					IsNormalRoad(tile) && !HasAtMostOneBit(GetAllRoadBits(tile))) {
				if (GetFoundationSlope(tile, NULL) == SLOPE_FLAT && EnsureNoVehicleOnGround(tile).Succeeded() && Chance16(1, 40)) {
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
			const Roadside *new_rs = (_settings_game.game_creation.landscape == LT_TOYLAND) ? _town_road_types_2[grp] : _town_road_types[grp];
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

		if (_settings_game.economy.mod_road_rebuild) {
			/* Generate a nicer town surface */
			const RoadBits old_rb = GetAnyRoadBits(tile, ROADTYPE_ROAD);
			const RoadBits new_rb = CleanUpRoadBits(tile, old_rb);

			if (old_rb != new_rb) {
				RemoveRoad(tile, DC_EXEC | DC_AUTO | DC_NO_WATER, (old_rb ^ new_rb), ROADTYPE_ROAD, true);
			}
		}

		MarkTileDirtyByTile(tile);
	}
}

static bool ClickTile_Road(TileIndex tile)
{
	if (!IsRoadDepot(tile)) return false;

	ShowDepotWindow(tile, VEH_ROAD);
	return true;
}

/* Converts RoadBits to TrackBits */
static const byte _road_trackbits[16] = {
	0x0, 0x0, 0x0, 0x10, 0x0, 0x2, 0x8, 0x1A, 0x0, 0x4, 0x1, 0x15, 0x20, 0x26, 0x29, 0x3F,
};

static TrackStatus GetTileTrackStatus_Road(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	TrackdirBits trackdirbits = TRACKDIR_BIT_NONE;
	TrackdirBits red_signals = TRACKDIR_BIT_NONE; // crossing barred
	switch (mode) {
		case TRANSPORT_RAIL:
			if (IsLevelCrossing(tile)) trackdirbits = TrackBitsToTrackdirBits(GetCrossingRailBits(tile));
			break;

		case TRANSPORT_ROAD:
			if ((GetRoadTypes(tile) & sub_mode) == 0) break;
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL: {
					const uint drd_to_multiplier[DRD_END] = { 0x101, 0x100, 0x1, 0x0 };
					RoadType rt = (RoadType)FindFirstBit(sub_mode);
					RoadBits bits = GetRoadBits(tile, rt);

					/* no roadbit at this side of tile, return 0 */
					if (side != INVALID_DIAGDIR && (DiagDirToRoadBits(side) & bits) == 0) break;

					uint multiplier = drd_to_multiplier[rt == ROADTYPE_TRAM ? DRD_NONE : GetDisallowedRoadDirections(tile)];
					if (!HasRoadWorks(tile)) trackdirbits = (TrackdirBits)(_road_trackbits[bits] * multiplier);
					break;
				}

				case ROAD_TILE_CROSSING: {
					Axis axis = GetCrossingRoadAxis(tile);

					if (side != INVALID_DIAGDIR && axis != DiagDirToAxis(side)) break;

					trackdirbits = TrackBitsToTrackdirBits(AxisToTrackBits(axis));
					if (IsCrossingBarred(tile)) red_signals = trackdirbits;
					break;
				}

				default:
				case ROAD_TILE_DEPOT: {
					DiagDirection dir = GetRoadDepotDirection(tile);

					if (side != INVALID_DIAGDIR && side != dir) break;

					trackdirbits = TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir));
					break;
				}
			}
			break;

		default: break;
	}
	return CombineTrackStatus(trackdirbits, red_signals);
}

static const StringID _road_tile_strings[] = {
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD_WITH_STREETLIGHTS,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_TREE_LINED_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
};

static void GetTileDesc_Road(TileIndex tile, TileDesc *td)
{
	Owner rail_owner = INVALID_OWNER;
	Owner road_owner = INVALID_OWNER;
	Owner tram_owner = INVALID_OWNER;

	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_CROSSING: {
			td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_RAIL_LEVEL_CROSSING;
			RoadTypes rts = GetRoadTypes(tile);
			rail_owner = GetTileOwner(tile);
			if (HasBit(rts, ROADTYPE_ROAD)) road_owner = GetRoadOwner(tile, ROADTYPE_ROAD);
			if (HasBit(rts, ROADTYPE_TRAM)) tram_owner = GetRoadOwner(tile, ROADTYPE_TRAM);

			const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(tile));
			td->rail_speed = rti->max_speed;

			break;
		}

		case ROAD_TILE_DEPOT:
			td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT;
			road_owner = GetTileOwner(tile); // Tile has only one owner, roadtype does not matter
			td->build_date = Depot::GetByTile(tile)->build_date;
			break;

		default: {
			RoadTypes rts = GetRoadTypes(tile);
			td->str = (HasBit(rts, ROADTYPE_ROAD) ? _road_tile_strings[GetRoadside(tile)] : STR_LAI_ROAD_DESCRIPTION_TRAMWAY);
			if (HasBit(rts, ROADTYPE_ROAD)) road_owner = GetRoadOwner(tile, ROADTYPE_ROAD);
			if (HasBit(rts, ROADTYPE_TRAM)) tram_owner = GetRoadOwner(tile, ROADTYPE_TRAM);
			break;
		}
	}

	/* Now we have to discover, if the tile has only one owner or many:
	 *   - Find a first_owner of the tile. (Currently road or tram must be present, but this will break when the third type becomes available)
	 *   - Compare the found owner with the other owners, and test if they differ.
	 * Note: If road exists it will be the first_owner.
	 */
	Owner first_owner = (road_owner == INVALID_OWNER ? tram_owner : road_owner);
	bool mixed_owners = (tram_owner != INVALID_OWNER && tram_owner != first_owner) || (rail_owner != INVALID_OWNER && rail_owner != first_owner);

	if (mixed_owners) {
		/* Multiple owners */
		td->owner_type[0] = (rail_owner == INVALID_OWNER ? STR_NULL : STR_LAND_AREA_INFORMATION_RAIL_OWNER);
		td->owner[0] = rail_owner;
		td->owner_type[1] = (road_owner == INVALID_OWNER ? STR_NULL : STR_LAND_AREA_INFORMATION_ROAD_OWNER);
		td->owner[1] = road_owner;
		td->owner_type[2] = (tram_owner == INVALID_OWNER ? STR_NULL : STR_LAND_AREA_INFORMATION_TRAM_OWNER);
		td->owner[2] = tram_owner;
	} else {
		/* One to rule them all */
		td->owner[0] = first_owner;
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
		case ROAD_TILE_DEPOT: {
			if (v->type != VEH_ROAD) break;

			RoadVehicle *rv = RoadVehicle::From(v);
			if (rv->frame == RVC_DEPOT_STOP_FRAME &&
					_roadveh_enter_depot_dir[GetRoadDepotDirection(tile)] == rv->state) {
				rv->state = RVSB_IN_DEPOT;
				rv->vehstatus |= VS_HIDDEN;
				rv->direction = ReverseDir(rv->direction);
				if (rv->Next() == NULL) VehicleEnterDepot(rv->First());
				rv->tile = tile;

				InvalidateWindowData(WC_VEHICLE_DEPOT, rv->tile);
				return VETSB_ENTERED_WORMHOLE;
			}
			break;
		}

		default: break;
	}
	return VETSB_CONTINUE;
}


static void ChangeTileOwner_Road(TileIndex tile, Owner old_owner, Owner new_owner)
{
	if (IsRoadDepot(tile)) {
		if (GetTileOwner(tile) == old_owner) {
			if (new_owner == INVALID_OWNER) {
				DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);
			} else {
				SetTileOwner(tile, new_owner);
			}
		}
		return;
	}

	for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
		/* Update all roadtypes, no matter if they are present */
		if (GetRoadOwner(tile, rt) == old_owner) {
			SetRoadOwner(tile, rt, new_owner == INVALID_OWNER ? OWNER_NONE : new_owner);
		}
	}

	if (IsLevelCrossing(tile)) {
		if (GetTileOwner(tile) == old_owner) {
			if (new_owner == INVALID_OWNER) {
				DoCommand(tile, 0, GetCrossingRailTrack(tile), DC_EXEC | DC_BANKRUPT, CMD_REMOVE_SINGLE_RAIL);
			} else {
				SetTileOwner(tile, new_owner);
			}
		}
	}
}

static CommandCost TerraformTile_Road(TileIndex tile, DoCommandFlag flags, uint z_new, Slope tileh_new)
{
	if (_settings_game.construction.build_on_slopes && AutoslopeEnabled()) {
		switch (GetRoadTileType(tile)) {
			case ROAD_TILE_CROSSING:
				if (!IsSteepSlope(tileh_new) && (GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new)) && HasBit(VALID_LEVEL_CROSSING_SLOPES, tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
				break;

			case ROAD_TILE_DEPOT:
				if (AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, GetRoadDepotDirection(tile))) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
				break;

			case ROAD_TILE_NORMAL: {
				RoadBits bits = GetAllRoadBits(tile);
				RoadBits bits_copy = bits;
				/* Check if the slope-road_bits combination is valid at all, i.e. it is safe to call GetRoadFoundation(). */
				if (CheckRoadSlope(tileh_new, &bits_copy, ROAD_NONE, ROAD_NONE).Succeeded()) {
					/* CheckRoadSlope() sometimes changes the road_bits, if it does not agree with them. */
					if (bits == bits_copy) {
						uint z_old;
						Slope tileh_old = GetTileSlope(tile, &z_old);

						/* Get the slope on top of the foundation */
						z_old += ApplyFoundationToSlope(GetRoadFoundation(tileh_old, bits), &tileh_old);
						z_new += ApplyFoundationToSlope(GetRoadFoundation(tileh_new, bits), &tileh_new);

						/* The surface slope must not be changed */
						if ((z_old == z_new) && (tileh_old == tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
					}
				}
				break;
			}

			default: NOT_REACHED();
		}
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

/** Tile callback functions for road tiles */
extern const TileTypeProcs _tile_type_road_procs = {
	DrawTile_Road,           // draw_tile_proc
	GetSlopeZ_Road,          // get_slope_z_proc
	ClearTile_Road,          // clear_tile_proc
	NULL,                    // add_accepted_cargo_proc
	GetTileDesc_Road,        // get_tile_desc_proc
	GetTileTrackStatus_Road, // get_tile_track_status_proc
	ClickTile_Road,          // click_tile_proc
	NULL,                    // animate_tile_proc
	TileLoop_Road,           // tile_loop_clear
	ChangeTileOwner_Road,    // change_tile_owner_clear
	NULL,                    // add_produced_cargo_proc
	VehicleEnter_Road,       // vehicle_enter_tile_proc
	GetFoundation_Road,      // get_foundation_proc
	TerraformTile_Road,      // terraform_tile_proc
};
