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
#include "newgrf_debug.h"
#include "newgrf_railtype.h"
#include "newgrf_roadtype.h"
#include "date_func.h"
#include "genworld.h"
#include "company_gui.h"
#include "road.h"
#include "road_func.h"

#include "table/strings.h"
#include "table/roadtypes.h"

#include "safeguards.h"

/** Helper type for lists/vectors of road vehicles */
typedef SmallVector<RoadVehicle *, 16> RoadVehicleList;

RoadtypeInfo _roadtypes[ROADTYPE_END][ROADSUBTYPE_END];
RoadTypeIdentifier _sorted_roadtypes[ROADTYPE_END][ROADSUBTYPE_END];
uint8 _sorted_roadtypes_size[ROADTYPE_END];

/**
 * Reset all road type information to its default values.
 */
void ResetRoadTypes()
{
	static const RoadtypeInfo empty_roadtype = {
		{0,0,0,0,0,0},
		{0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,{},{},0,{},{}},
		ROADSUBTYPES_NONE, ROTFB_NONE, 0, 0, 0, 0,
		RoadTypeLabelList(), 0, 0, ROADSUBTYPES_NONE, ROADSUBTYPES_NONE, 0,
		{}, {} };

	/* Road type */
	assert_compile(lengthof(_original_roadtypes) <= lengthof(_roadtypes[ROADTYPE_ROAD]));
	uint i = 0;
	for (; i < lengthof(_original_roadtypes);       i++) _roadtypes[ROADTYPE_ROAD][i] = _original_roadtypes[i];
	for (; i < lengthof(_roadtypes[ROADTYPE_ROAD]); i++) _roadtypes[ROADTYPE_ROAD][i] = empty_roadtype;

	/* Tram type */
	assert_compile(lengthof(_original_tramtypes) <= lengthof(_roadtypes[ROADTYPE_TRAM]));
	i = 0;
	for (; i < lengthof(_original_tramtypes);       i++) _roadtypes[ROADTYPE_TRAM][i] = _original_tramtypes[i];
	for (; i < lengthof(_roadtypes[ROADTYPE_TRAM]); i++) _roadtypes[ROADTYPE_TRAM][i] = empty_roadtype;
}

void ResolveRoadTypeGUISprites(RoadtypeInfo *rti)
{
	SpriteID cursors_base = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_CURSORS);
	if (cursors_base != 0) {
		rti->gui_sprites.build_x_road = cursors_base + 0;
		rti->gui_sprites.build_y_road = cursors_base + 1;
		rti->gui_sprites.auto_road = cursors_base + 2;
		rti->gui_sprites.build_depot = cursors_base + 3;
		rti->gui_sprites.build_tunnel = cursors_base + 4;
		rti->gui_sprites.convert_road = cursors_base + 5;
		rti->cursor.road_swne = cursors_base + 6;
		rti->cursor.road_nwse = cursors_base + 7;
		rti->cursor.autoroad = cursors_base + 8;
		rti->cursor.depot = cursors_base + 9;
		rti->cursor.tunnel = cursors_base + 10;
		rti->cursor.convert_road = cursors_base + 11;
	}
}

/**
 * Compare roadtypes based on their sorting order.
 * @param first  The roadtype to compare to.
 * @param second The roadtype to compare.
 * @return True iff the first should be sorted before the second.
 */
static int CDECL CompareRoadTypes(const RoadTypeIdentifier *first, const RoadTypeIdentifier *second)
{
	return GetRoadTypeInfo(*first)->sorting_order - GetRoadTypeInfo(*second)->sorting_order;
}

/**
 * Resolve sprites of custom road types
 */
void InitRoadTypes()
{
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		for (RoadSubType rst = ROADSUBTYPE_BEGIN; rst != ROADSUBTYPE_END; rst++) {
			RoadtypeInfo *rti = &_roadtypes[rt][rst];
			ResolveRoadTypeGUISprites(rti);
		}

		_sorted_roadtypes_size[rt] = 0;
		for (RoadSubType rst = ROADSUBTYPE_BEGIN; rst != ROADSUBTYPE_END; rst++) {
			if (_roadtypes[rt][rst].label != 0) {
				_sorted_roadtypes[rt][_sorted_roadtypes_size[rt]++] = RoadTypeIdentifier(rt, rst);
			}
		}
		QSortT(_sorted_roadtypes[rt], _sorted_roadtypes_size[rt], CompareRoadTypes);
	}
}

/**
 * Allocate a new road type label
 */
RoadTypeIdentifier AllocateRoadType(RoadTypeLabel label, RoadType basetype)
{
	RoadTypeIdentifier rtid;

	rtid.basetype = INVALID_ROADTYPE;
	rtid.subtype = INVALID_ROADSUBTYPE;

	for (RoadSubType rt = ROADSUBTYPE_BEGIN; rt != ROADSUBTYPE_END; rt++) {
		RoadtypeInfo *rti = &_roadtypes[basetype][rt];

		if (rti->label == 0) {
			/* Set up new road type */
			*rti = (basetype == ROADTYPE_ROAD ? _original_roadtypes[ROADSUBTYPE_NORMAL] : _original_tramtypes[ROADSUBTYPE_NORMAL]);
			rti->label = label;
			rti->alternate_labels.Clear();

			/* Make us compatible with ourself. */
			rti->powered_roadtypes = (RoadSubTypes)(1 << rt);

			/* We also introduce ourself. */
			rti->introduces_roadtypes = (RoadSubTypes)(1 << rt);

			/* Default sort order; order of allocation, but with some
			 * offsets so it's easier for NewGRF to pick a spot without
			 * changing the order of other (original) road types.
			 * The << is so you can place other roadtypes in between the
			 * other roadtypes, the 7 is to be able to place something
			 * before the first (default) road type. */
			rti->sorting_order = rt << 4 | 7;

			rtid.basetype = basetype;
			rtid.subtype = rt;

			return rtid;
		}
	}

	return rtid;
}

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

/** Invalid RoadBits on slopes.  */
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
static CommandCost RemoveRoad(TileIndex tile, DoCommandFlag flags, RoadBits pieces, RoadTypeIdentifier rtid, bool crossing_check, bool town_check = true)
{
	RoadType rt = rtid.basetype;
	RoadTypes rts = GetRoadTypes(tile);
	/* The tile doesn't have the given road type */
	if (!HasBit(rts, rtid.basetype)) return_cmd_error(rt == ROADTYPE_TRAM ? STR_ERROR_THERE_IS_NO_TRAMWAY : STR_ERROR_THERE_IS_NO_ROAD);

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
			/* Removing any roadbit in the bridge axis removes the roadtype (that's the behaviour remove-long-roads needs) */
			if ((AxisToRoadBits(DiagDirToAxis(GetTunnelBridgeDirection(tile))) & pieces) == ROAD_NONE) return_cmd_error(rt == ROADTYPE_TRAM ? STR_ERROR_THERE_IS_NO_TRAMWAY : STR_ERROR_THERE_IS_NO_ROAD);

			TileIndex other_end = GetOtherTunnelBridgeEnd(tile);
			/* Pay for *every* tile of the bridge or tunnel */
			uint len = GetTunnelBridgeLength(other_end, tile) + 2;
			cost.AddCost(len * 2 * _price[PR_CLEAR_ROAD]);
			if (flags & DC_EXEC) {
				Company *c = Company::GetIfValid(GetRoadOwner(tile, rt));
				if (c != NULL) {
					/* A full diagonal road tile has two road bits. */
					c->infrastructure.road[rtid.basetype][rtid.subtype] -= len * 2 * TUNNELBRIDGE_TRACKBIT_FACTOR;
					DirtyCompanyInfrastructureWindows(c->index);
				}

				RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
				rtids.ClearRoadType(rt);
				SetRoadTypes(other_end, rtids);
				SetRoadTypes(tile, rtids);

				/* If the owner of the bridge sells all its road, also move the ownership
				 * to the owner of the other roadtype, unless the bridge owner is a town. */
				RoadType other_rt = (rt == ROADTYPE_ROAD) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
				Owner other_owner = GetRoadOwner(tile, other_rt);
				if (!IsTileOwner(tile, other_owner) && !IsTileOwner(tile, OWNER_TOWN)) {
					SetTileOwner(tile, other_owner);
					SetTileOwner(other_end, other_owner);
				}

				/* Mark tiles dirty that have been repaved */
				if (IsBridge(tile)) {
					MarkBridgeDirty(tile);
				} else {
					MarkTileDirtyByTile(tile);
					MarkTileDirtyByTile(other_end);
				}
			}
		} else {
			assert(IsDriveThroughStopTile(tile));
			cost.AddCost(_price[PR_CLEAR_ROAD] * 2);
			if (flags & DC_EXEC) {
				Company *c = Company::GetIfValid(GetRoadOwner(tile, rt));
				if (c != NULL) {
					/* A full diagonal road tile has two road bits. */
					c->infrastructure.road[rtid.basetype][rtid.subtype] -= 2;
					DirtyCompanyInfrastructureWindows(c->index);
				}
				RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
				rtids.ClearRoadType(rt);
				SetRoadTypes(tile, rtids);
				MarkTileDirtyByTile(tile);
			}
		}
		return cost;
	}

	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_NORMAL: {
			Slope tileh = GetTileSlope(tile);

			/* Steep slopes behave the same as slopes with one corner raised. */
			if (IsSteepSlope(tileh)) {
				tileh = SlopeWithOneCornerRaised(GetHighestSlopeCorner(tileh));
			}

			RoadBits present = GetRoadBits(tile, rt);
			const RoadBits other = GetOtherRoadBits(tile, rt);
			const Foundation f = GetRoadFoundation(tileh, present);

			if (HasRoadWorks(tile) && _current_company != OWNER_WATER) return_cmd_error(STR_ERROR_ROAD_WORKS_IN_PROGRESS);

			/* Autocomplete to a straight road
			 * @li if the bits of the other roadtypes result in another foundation
			 * @li if build on slopes is disabled */
			if ((IsStraightRoad(other) && (other & _invalid_tileh_slopes_road[0][tileh & SLOPE_ELEVATED]) != ROAD_NONE) ||
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

				Company *c = Company::GetIfValid(GetRoadOwner(tile, rt));
				if (c != NULL) {
					c->infrastructure.road[rtid.basetype][rtid.subtype] -= CountBits(pieces);
					DirtyCompanyInfrastructureWindows(c->index);
				}

				if (present == ROAD_NONE) {
					RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
					rtids.ClearRoadType(rt);
					if (rtids.PresentRoadTypes() == ROADTYPES_NONE) {
						/* Includes MarkTileDirtyByTile() */
						DoClearSquare(tile);
					} else {
						if (rt == ROADTYPE_ROAD && IsRoadOwner(tile, ROADTYPE_ROAD, OWNER_TOWN)) {
							/* Update nearest-town index */
							const Town *town = CalcClosestTownFromTile(tile);
							SetTownIndex(tile, town == NULL ? (TownID)INVALID_TOWN : town->index);
						}
						SetRoadBits(tile, ROAD_NONE, rt);
						SetRoadTypes(tile, rtids);
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

			if (flags & DC_EXEC) {
				Company *c = Company::GetIfValid(GetRoadOwner(tile, rt));
				if (c != NULL) {
					/* A full diagonal road tile has two road bits. */
					c->infrastructure.road[rtid.basetype][rtid.subtype] -= 2;
					DirtyCompanyInfrastructureWindows(c->index);
				}

				Track railtrack = GetCrossingRailTrack(tile);
				RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
				rtids.ClearRoadType(rt);
				if (rtids.PresentRoadTypes() == ROADTYPES_NONE) {
					TrackBits tracks = GetCrossingRailBits(tile);
					bool reserved = HasCrossingReservation(tile);
					MakeRailNormal(tile, GetTileOwner(tile), tracks, GetRailType(tile));
					if (reserved) SetTrackReservation(tile, tracks);

					/* Update rail count for level crossings. The plain track should still be accounted
					 * for, so only subtract the difference to the level crossing cost. */
					c = Company::GetIfValid(GetTileOwner(tile));
					if (c != NULL) {
						c->infrastructure.rail[GetRailType(tile)] -= LEVELCROSSING_TRACKBIT_FACTOR - 1;
						DirtyCompanyInfrastructureWindows(c->index);
					}
				} else {
					SetRoadTypes(tile, rtids);
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

	/* Steep slopes behave the same as slopes with one corner raised. */
	if (IsSteepSlope(tileh)) {
		tileh = SlopeWithOneCornerRaised(GetHighestSlopeCorner(tileh));
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
 *           bit 4..8 road type
 *           bit 9..10 disallowed directions to toggle
 * @param p2 the town that is building the road (0 if not applicable)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildRoad(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CompanyID company = _current_company;
	CommandCost cost(EXPENSES_CONSTRUCTION);

	RoadBits existing = ROAD_NONE;
	RoadBits other_bits = ROAD_NONE;

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-company is building the road */
	if ((Company::IsValidID(company) && p2 != 0) || (company == OWNER_TOWN && !Town::IsValidID(p2)) || (company == OWNER_DEITY && p2 != 0)) return CMD_ERROR;
	if (company != OWNER_TOWN) {
		const Town *town = CalcClosestTownFromTile(tile);
		p2 = (town != NULL) ? town->index : (TownID)INVALID_TOWN;

		if (company == OWNER_DEITY) {
			company = OWNER_TOWN;

			/* If we are not within a town, we are not owned by the town */
			if (town == NULL || DistanceSquare(tile, town->xy) > town->cache.squared_town_zone_radius[HZB_TOWN_EDGE]) {
				company = OWNER_NONE;
			}
		}
	}

	RoadBits pieces = Extract<RoadBits, 0, 4>(p1);

	/* do not allow building 'zero' road bits, code wouldn't handle it */
	if (pieces == ROAD_NONE) return CMD_ERROR;

	RoadTypeIdentifier rtid;
	if (!rtid.UnpackIfValid(GB(p1, 4, 5))) return CMD_ERROR;
	if (!ValParamRoadType(rtid)) return CMD_ERROR;

	DisallowedRoadDirections toggle_drd = Extract<DisallowedRoadDirections, 9, 2>(p1);

	Slope tileh = GetTileSlope(tile);

	bool need_to_clear = false;
	switch (GetTileType(tile)) {
		case MP_ROAD:
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL: {
					if (HasRoadWorks(tile)) return_cmd_error(STR_ERROR_ROAD_WORKS_IN_PROGRESS);

					other_bits = GetOtherRoadBits(tile, rtid.basetype);
					if (!HasTileRoadType(tile, rtid.basetype)) break;

					existing = GetRoadBits(tile, rtid.basetype);
					bool crossing = !IsStraightRoad(existing | pieces);
					if (rtid.IsRoad() && (GetDisallowedRoadDirections(tile) != DRD_NONE || toggle_drd != DRD_NONE) && crossing) {
						/* Junctions cannot be one-way */
						return_cmd_error(STR_ERROR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);
					}
					if ((existing & pieces) == pieces) {
						/* We only want to set the (dis)allowed road directions */
						if (toggle_drd != DRD_NONE && rtid.IsRoad()) {
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
							if ((flags & DC_EXEC) && rtid.IsRoad() && IsStraightRoad(existing)) {
								SetDisallowedRoadDirections(tile, dis_new);
								MarkTileDirtyByTile(tile);
							}
							return CommandCost();
						}
						return_cmd_error(STR_ERROR_ALREADY_BUILT);
					}
					/* Disallow breaking end-of-line of someone else
					 * so trams can still reverse on this tile. */
					if (rtid.IsTram() && HasExactlyOneBit(existing)) {
						Owner owner = GetRoadOwner(tile, rtid.basetype);
						if (Company::IsValidID(owner)) {
							CommandCost ret = CheckOwnership(owner);
							if (ret.Failed()) return ret;
						}
					}
					break;
				}

				case ROAD_TILE_CROSSING:
					if (RoadNoLevelCrossing(rtid)) {
						return_cmd_error(STR_ERROR_CROSSING_DISALLOWED_ROAD);
					}

					other_bits = GetCrossingRoadBits(tile);
					if (pieces & ComplementRoadBits(other_bits)) goto do_clear;
					pieces = other_bits; // we need to pay for both roadbits

					if (HasTileRoadType(tile, rtid.basetype)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
					break;

				case ROAD_TILE_DEPOT:
					if ((GetAnyRoadBits(tile, rtid.basetype) & pieces) == pieces) return_cmd_error(STR_ERROR_ALREADY_BUILT);
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

			if (RoadNoLevelCrossing(rtid)) {
				return_cmd_error(STR_ERROR_CROSSING_DISALLOWED_ROAD);
			}

			if (RailNoLevelCrossings(GetRailType(tile))) {
				return_cmd_error(STR_ERROR_CROSSING_DISALLOWED_RAIL);
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
				/* Update company infrastructure counts. A level crossing has two road bits. */
				Company *c = Company::GetIfValid(company);
				if (c != NULL) {
					c->infrastructure.road[rtid.basetype][rtid.subtype] += 2;
					DirtyCompanyInfrastructureWindows(company);
				}
				/* Update rail count for level crossings. The plain track is already
				 * counted, so only add the difference to the level crossing cost. */
				c = Company::GetIfValid(GetTileOwner(tile));
				if (c != NULL) {
					c->infrastructure.rail[GetRailType(tile)] += LEVELCROSSING_TRACKBIT_FACTOR - 1;
					DirtyCompanyInfrastructureWindows(c->index);
				}

				/* Always add road to the roadtypes (can't draw without it) */
				bool reserved = HasBit(GetRailReservationTrackBits(tile), railtrack);
				MakeRoadCrossing(tile, company, company, GetTileOwner(tile), roaddir, GetRailType(tile), RoadTypeIdentifiers::FromRoadTypeIdentifier(rtid), p2);
				SetCrossingReservation(tile, reserved);
				UpdateLevelCrossing(tile, false);
				MarkTileDirtyByTile(tile);
			}
			return CommandCost(EXPENSES_CONSTRUCTION, 2 * RoadBuildCost(rtid));
		}

		case MP_STATION: {
			if ((GetAnyRoadBits(tile, rtid.basetype) & pieces) == pieces) return_cmd_error(STR_ERROR_ALREADY_BUILT);
			if (!IsDriveThroughStopTile(tile)) goto do_clear;

			RoadBits curbits = AxisToRoadBits(DiagDirToAxis(GetRoadStopDir(tile)));
			if (pieces & ~curbits) goto do_clear;
			pieces = curbits; // we need to pay for both roadbits

			if (HasTileRoadType(tile, rtid.basetype)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
			break;
		}

		case MP_TUNNELBRIDGE: {
			if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) goto do_clear;
			/* Only allow building the outern roadbit, so building long roads stops at existing bridges */
			if (MirrorRoadBits(DiagDirToRoadBits(GetTunnelBridgeDirection(tile))) != pieces) goto do_clear;
			if (HasTileRoadType(tile, rtid.basetype)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
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
				Slope slope = GetTileSlope(tile);
				Foundation found_new = GetRoadFoundation(slope, pieces | existing);

				/* Test if all other roadtypes can be built at that foundation */
				for (RoadType rtest = ROADTYPE_ROAD; rtest < ROADTYPE_END; rtest++) {
					if (rtest != rtid.basetype) { // check only other road types
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

		if (IsNormalRoadTile(tile)) {
			/* If the road types don't match, try to convert only if vehicles of
			 * the new road type are not powered on the present road type and vehicles of
			 * the present road type are powered on the new road type. */
			RoadTypeIdentifier existing_rtid = GetRoadType(tile, rtid.basetype);
			if (existing_rtid.IsValid() && existing_rtid != rtid) {
				if (HasPowerOnRoad(rtid, existing_rtid)) {
					rtid = existing_rtid;
				} else if (HasPowerOnRoad(existing_rtid, rtid)) {
					CommandCost ret = DoCommand(tile, tile, rtid.Pack(), flags, CMD_CONVERT_ROAD);
					if (ret.Failed()) return ret;
					cost.AddCost(ret);
				} else {
					return CMD_ERROR;
				}
			}
		}
	}

	uint num_pieces = (!need_to_clear && IsTileType(tile, MP_TUNNELBRIDGE)) ?
			/* There are 2 pieces on *every* tile of the bridge or tunnel */
			2 * (GetTunnelBridgeLength(GetOtherTunnelBridgeEnd(tile), tile) + 2) :
			/* Count pieces */
			CountBits(pieces);

	cost.AddCost(num_pieces * RoadBuildCost(rtid));

	if (flags & DC_EXEC) {
		switch (GetTileType(tile)) {
			case MP_ROAD: {
				RoadTileType rtt = GetRoadTileType(tile);
				if (existing == ROAD_NONE || rtt == ROAD_TILE_CROSSING) {
					RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
					rtids.MergeRoadType(rtid);
					SetRoadTypes(tile, rtids);

					SetRoadOwner(tile, rtid.basetype, company);
					if (rtid.IsRoad()) SetTownIndex(tile, p2);
				}
				if (rtt != ROAD_TILE_CROSSING) SetRoadBits(tile, existing | pieces, rtid.basetype);
				break;
			}

			case MP_TUNNELBRIDGE: {
				TileIndex other_end = GetOtherTunnelBridgeEnd(tile);

				RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
				rtids.MergeRoadType(rtid);
				SetRoadTypes(other_end, rtids);
				SetRoadTypes(tile, rtids);
				SetRoadOwner(other_end, rtid.basetype, company);
				SetRoadOwner(tile, rtid.basetype, company);

				/* Mark tiles dirty that have been repaved */
				if (IsBridge(tile)) {
					MarkBridgeDirty(tile);
				} else {
					MarkTileDirtyByTile(other_end);
					MarkTileDirtyByTile(tile);
				}
				break;
			}

			case MP_STATION: {
				assert(IsDriveThroughStopTile(tile));
				RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
				rtids.MergeRoadType(rtid);
				SetRoadTypes(tile, rtids);
				SetRoadOwner(tile, rtid.basetype, company);
				break;
			}

			default:
				MakeRoadNormal(tile, pieces, RoadTypeIdentifiers::FromRoadTypeIdentifier(rtid), p2, company, company);
				break;
		}

		/* Update company infrastructure count. */
		Company *c = Company::GetIfValid(GetRoadOwner(tile, rtid.basetype));
		if (c != NULL) {
			if (IsTileType(tile, MP_TUNNELBRIDGE)) num_pieces *= TUNNELBRIDGE_TRACKBIT_FACTOR;
			c->infrastructure.road[rtid.basetype][rtid.subtype] += num_pieces;
			DirtyCompanyInfrastructureWindows(c->index);
		}

		if (rtid.IsRoad() && IsNormalRoadTile(tile)) {
			existing |= pieces;
			SetDisallowedRoadDirections(tile, IsStraightRoad(existing) ?
					GetDisallowedRoadDirections(tile) ^ toggle_drd : DRD_NONE);
		}

		MarkTileDirtyByTile(tile);
	}
	return cost;
}

/**
 * Checks whether a road or tram connection can be found when building a new road or tram.
 * @param tile Tile at which the road being built will end.
 * @param rt Roadtype of the road being built.
 * @param dir Direction that the road is following.
 * @return True if the next tile at dir direction is suitable for being connected directly by a second roadbit at the end of the road being built.
 */
static bool CanConnectToRoad(TileIndex tile, RoadType rt, DiagDirection dir)
{
	RoadBits bits = GetAnyRoadBits(tile + TileOffsByDiagDir(dir), rt, false);
	return (bits & DiagDirToRoadBits(ReverseDiagDir(dir))) != 0;
}

/**
 * Build a long piece of road.
 * @param start_tile start tile of drag (the building cost will appear over this tile)
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1). Only used if bit 6 is set or if we are building a single tile
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2). Only used if bit 6 is set or if we are building a single tile
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3..7) - road type identifier
 * - p2 = (bit 8) - set road direction
 * - p2 = (bit 9) - defines two different behaviors for this command:
 *      - 0 = Build up to an obstacle. Do not build the first and last roadbits unless they can be connected to something, or if we are building a single tile
 *      - 1 = Fail if an obstacle is found. Always take into account bit 0 and 1. This behavior is used for scripts
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildLongRoad(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DisallowedRoadDirections drd = DRD_NORTHBOUND;

	if (p1 >= MapSize()) return CMD_ERROR;

	TileIndex end_tile = p1;
	RoadTypeIdentifier rtid;
	if (!rtid.UnpackIfValid(GB(p2, 3, 5))) return CMD_ERROR;
	if (!ValParamRoadType(rtid)) return CMD_ERROR;

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
	 * will be interpreted correctly in the GTTS. Furthermore
	 * when you just 'click' on one tile to build them. */
	if ((axis == AXIS_Y) == (start_tile == end_tile && HasBit(p2, 0) == HasBit(p2, 1))) drd ^= DRD_BOTH;
	/* No disallowed direction bits have to be toggled */
	if (!HasBit(p2, 8)) drd = DRD_NONE;

	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error = CMD_ERROR;
	TileIndex tile = start_tile;
	bool had_bridge = false;
	bool had_tunnel = false;
	bool had_success = false;
	bool is_ai = HasBit(p2, 9);

	/* Start tile is the first tile clicked by the user. */
	for (;;) {
		RoadBits bits = AxisToRoadBits(axis);

		/* Determine which road parts should be built. */
		if (!is_ai && start_tile != end_tile) {
			/* Only build the first and last roadbit if they can connect to something. */
			if (tile == end_tile && !CanConnectToRoad(tile, rtid.basetype, dir)) { // TODO
				bits = DiagDirToRoadBits(ReverseDiagDir(dir));
			} else if (tile == start_tile && !CanConnectToRoad(tile, rtid.basetype, ReverseDiagDir(dir))) { // TODO
				bits = DiagDirToRoadBits(dir);
			}
		} else {
			/* Road parts only have to be built at the start tile or at the end tile. */
			if (tile == end_tile && !HasBit(p2, 1)) bits &= DiagDirToRoadBits(ReverseDiagDir(dir));
			if (tile == start_tile && HasBit(p2, 0)) bits &= DiagDirToRoadBits(dir);
		}

		CommandCost ret = DoCommand(tile, drd << 9 | rtid.Pack() << 4 | bits, 0, flags, CMD_BUILD_ROAD);
		if (ret.Failed()) {
			last_error = ret;
			if (last_error.GetErrorMessage() != STR_ERROR_ALREADY_BUILT) {
				if (is_ai) return last_error;
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
 * RoadSubType is ignored, each roadtype can remove all its subtypes
 * @param start_tile start tile of drag
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 - 7) - road type
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveLongRoad(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);

	if (p1 >= MapSize()) return CMD_ERROR;

	TileIndex end_tile = p1;
	RoadTypeIdentifier rtid;
	if (!rtid.UnpackIfValid(GB(p2, 3, 5))) return CMD_ERROR;

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
			CommandCost ret = RemoveRoad(tile, flags & ~DC_EXEC, bits, rtid, true);
			if (ret.Succeeded()) {
				if (flags & DC_EXEC) {
					money -= ret.GetCost();
					if (money < 0) {
						_additional_cash_required = DoCommand(start_tile, end_tile, p2, flags & ~DC_EXEC, CMD_REMOVE_LONG_ROAD).GetCost();
						return cost;
					}
					RemoveRoad(tile, flags, bits, rtid, true, false);
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
 *           bit 2..7 road type identifier
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 *
 * @todo When checking for the tile slope,
 * distinguish between "Flat land required" and "land sloped in wrong direction"
 */
CommandCost CmdBuildRoadDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DiagDirection dir = Extract<DiagDirection, 0, 2>(p1);

	RoadTypeIdentifier rtid;
	if (!rtid.UnpackIfValid(GB(p1, 2, 5))) return CMD_ERROR;
	if (!ValParamRoadType(rtid)) return CMD_ERROR;

	Slope tileh = GetTileSlope(tile);
	if (tileh != SLOPE_FLAT && (
				!_settings_game.construction.build_on_slopes ||
				!CanBuildDepotByTileh(dir, tileh)
			)) {
		return_cmd_error(STR_ERROR_FLAT_LAND_REQUIRED);
	}

	CommandCost cost = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (cost.Failed()) return cost;

	if (IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	if (!Depot::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Depot *dep = new Depot(tile);
		dep->build_date = _date;

		/* A road depot has two road bits. */
		Company::Get(_current_company)->infrastructure.road[rtid.basetype][rtid.subtype] += 2;
		DirtyCompanyInfrastructureWindows(_current_company);

		MakeRoadDepot(tile, _current_company, dep->index, dir, rtid);
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
		Company *c = Company::GetIfValid(GetTileOwner(tile));
		if (c != NULL) {
			/* A road depot has two road bits. */
			RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
			RoadTypeIdentifier rtid = rtids.HasRoad() ? rtids.road_identifier : rtids.tram_identifier;
			c->infrastructure.road[rtid.basetype][rtid.subtype] -= 2;
			DirtyCompanyInfrastructureWindows(c->index);
		}

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
				RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
				RoadTypeIdentifier rtid;
				FOR_EACH_SET_ROADTYPEIDENTIFIER(rtid, rtids) {
					CommandCost tmp_ret = RemoveRoad(tile, flags, GetRoadBits(tile, rtid.basetype), rtid, true);
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
			RoadTypeIdentifier rtid;
			do {
				if (HasBit(rts, rt)) {
					rtid.basetype = rt;
					CommandCost tmp_ret = RemoveRoad(tile, flags, GetCrossingRoadBits(tile), rtid, false);
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

	/* Steep slopes behave the same as slopes with one corner raised. */
	if (IsSteepSlope(tileh)) {
		tileh = SlopeWithOneCornerRaised(GetHighestSlopeCorner(tileh));
	}

	/* Leveled RoadBits on a slope */
	if ((_invalid_tileh_slopes_road[0][tileh] & bits) == ROAD_NONE) return FOUNDATION_LEVELED;

	/* Straight roads without foundation on a slope */
	if (!IsSlopeWithOneCornerRaised(tileh) &&
			(_invalid_tileh_slopes_road[1][tileh] & bits) == ROAD_NONE)
		return FOUNDATION_NONE;

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
 * Get the sprite offset within a spritegroup.
 * @param slope Slope
 * @param bits Roadbits
 * @return Offset for the sprite within the spritegroup.
 */
static uint GetRoadSpriteOffset(Slope slope, RoadBits bits)
{
	if (slope != SLOPE_FLAT) {
		switch (slope) {
			case SLOPE_NE: return 11;
			case SLOPE_SE: return 12;
			case SLOPE_SW: return 13;
			case SLOPE_NW: return 14;
			default: NOT_REACHED();
		}
	} else {
		static const uint offsets[] = {
			0, 18, 17, 7,
			16, 0, 10, 5,
			15, 8, 1, 4,
			9, 3, 6, 2
		};
		return offsets[bits];
	}
}

/**
 * Should the road be drawn as a unpaved snow/desert road?
 * By default, roads are always drawn as unpaved if they are on desert or
 * above the snow line, but NewGRFs can override this for desert.
 *
 * @param tile The tile the road is on
 * @param roadside What sort of road this is
 * @return True if snow/desert road sprites should be used.
 */
static bool DrawRoadAsSnowDesert(TileIndex tile, Roadside roadside)
{
	return (IsOnSnow(tile) &&
			!(_settings_game.game_creation.landscape == LT_TROPIC && HasGrfMiscBit(GMB_DESERT_PAVED_ROADS) &&
				roadside != ROADSIDE_BARREN && roadside != ROADSIDE_GRASS && roadside != ROADSIDE_GRASS_ROAD_WORKS));
}

/**
 * Draws the catenary for the RoadType of the given tile
 * @param ti   information about the tile (slopes, height etc)
 * @param rtid road type to draw catenary for
 * @param rb   the roadbits for the tram
 */
void DrawRoadTypeCatenary(const TileInfo *ti, RoadTypeIdentifier rtid, RoadBits rb)
{
	/* Don't draw the catenary under a low bridge */
	if (IsBridgeAbove(ti->tile) && !IsTransparencySet(TO_CATENARY)) {
		int height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));

		if (height <= GetTileMaxZ(ti->tile) + 1) return;
	}

	if (CountBits(rb) > 2) {
		/* On junctions we check whether neighbouring tiles also have catenary, and possibly
		 * do not draw catenary towards those neighbours, which do not have catenary. */
		RoadBits rb_new = ROAD_NONE;
		for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
			if (rb & DiagDirToRoadBits(dir)) {
				TileIndex neighbour = TileAddByDiagDir(ti->tile, dir);
				if (MayHaveRoad(neighbour)) {
					RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(neighbour);
					if ((rtids.HasRoad() && HasRoadCatenary(rtids.road_identifier)) ||
							(rtids.HasTram() && HasRoadCatenary(rtids.tram_identifier))) {
						rb_new |= DiagDirToRoadBits(dir);
					}
				}
			}
		}
		if (CountBits(rb_new) >= 2) rb = rb_new;
	}

	const RoadtypeInfo* rti = GetRoadTypeInfo(rtid);
	SpriteID front = GetCustomRoadSprite(rti, ti->tile, ROTSG_CATENARY_FRONT);
	SpriteID back = GetCustomRoadSprite(rti, ti->tile, ROTSG_CATENARY_BACK);

	if (front != 0 || back != 0) {
		if (front != 0) front += GetRoadSpriteOffset(ti->tileh, rb);
		if (back != 0) back += GetRoadSpriteOffset(ti->tileh, rb);
	} else if (ti->tileh != SLOPE_FLAT) {
		back  = SPR_TRAMWAY_BACK_WIRES_SLOPED  + _road_sloped_sprites[ti->tileh - 1];
		front = SPR_TRAMWAY_FRONT_WIRES_SLOPED + _road_sloped_sprites[ti->tileh - 1];
	} else {
		back  = SPR_TRAMWAY_BASE + _road_backpole_sprites_1[rb];
		front = SPR_TRAMWAY_BASE + _road_frontwire_sprites_1[rb];
	}

    /* Catenary uses 1st company colour to help identify owner.
     * For tiles with OWNER_TOWN or OWNER_NONE, recolour CC to grey as a neutral colour. */
	Owner owner = GetRoadOwner(ti->tile, rtid.basetype);
	PaletteID pal = (owner == OWNER_NONE || owner == OWNER_TOWN ? GENERAL_SPRITE_COLOUR(COLOUR_GREY) : COMPANY_SPRITE_COLOUR(owner));
	if (back != 0) AddSortableSpriteToDraw(back,  pal, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_CATENARY));
	if (front != 0) AddSortableSpriteToDraw(front, pal, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_CATENARY));
}

/**
 * Draws the catenary for the given tile
 * @param ti information about the tile (slopes, height etc)
 */
void DrawRoadCatenary(const TileInfo *ti)
{
	RoadBits road;
	RoadBits tram;

	if (IsTileType(ti->tile, MP_ROAD)) {
		if (IsNormalRoad(ti->tile)) {
			road = GetRoadBits(ti->tile, ROADTYPE_ROAD);
			tram = GetRoadBits(ti->tile, ROADTYPE_TRAM);
		}
		else if (IsLevelCrossing(ti->tile)) {
			tram = road = (GetCrossingRailAxis(ti->tile) == AXIS_Y ? ROAD_X : ROAD_Y);
		}
	}
	else if (IsTileType(ti->tile, MP_STATION)) {
		if (IsRoadStop(ti->tile)) {
			Axis axis = GetRoadStopDir(ti->tile) == DIAGDIR_NE ? AXIS_X : AXIS_Y;
			tram = road = (axis == AXIS_X ? ROAD_X : ROAD_Y);
		}
	}
	else {
		// No road here, no catenary to draw
		return;
	}

	RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(ti->tile);
	const RoadtypeInfo* road_rti = rtids.HasRoad() ? GetRoadTypeInfo(rtids.road_identifier) : NULL;
	const RoadtypeInfo* tram_rti = rtids.HasTram() ? GetRoadTypeInfo(rtids.tram_identifier) : NULL;

	if (road_rti != NULL && HasRoadCatenaryDrawn(rtids.road_identifier)) {
		DrawRoadTypeCatenary(ti, rtids.road_identifier, road);
	}

	if (tram_rti != NULL && HasRoadCatenaryDrawn(rtids.tram_identifier)) {
		DrawRoadTypeCatenary(ti, rtids.tram_identifier, tram);
	}
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
	int z = ti->z;
	if (ti->tileh != SLOPE_FLAT) z = GetSlopePixelZ(x, y);
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

	RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(ti->tile);
	const RoadtypeInfo* road_rti = rtids.HasRoad() ? GetRoadTypeInfo(rtids.road_identifier) : NULL;
	const RoadtypeInfo* tram_rti = rtids.HasTram() ? GetRoadTypeInfo(rtids.tram_identifier) : NULL;

	if (ti->tileh != SLOPE_FLAT) {
		DrawFoundation(ti, GetRoadFoundation(ti->tileh, road | tram));
		/* DrawFoundation() modifies ti. */
	}

	/* Determine sprite offsets */
	uint road_offset = GetRoadSpriteOffset(ti->tileh, road);
	uint tram_offset = GetRoadSpriteOffset(ti->tileh, tram);

	/* Draw baseset underlay */
	SpriteID image = SPR_ROAD_Y + (road_rti != NULL ? road_offset : tram_offset);
	PaletteID pal = PAL_NONE;

	Roadside roadside = GetRoadside(ti->tile);
	if (DrawRoadAsSnowDesert(ti->tile, roadside)) {
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

	/* Road underlay takes precendence over tram */
	if (road_rti != NULL) {
		if (road_rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(road_rti, ti->tile, ROTSG_GROUND);
			DrawGroundSprite(ground + road_offset, PAL_NONE);
		}
	} else {
		if (tram_rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(tram_rti, ti->tile, ROTSG_GROUND);
			DrawGroundSprite(ground + tram_offset, PAL_NONE);
		} else {
			DrawGroundSprite(SPR_TRAMWAY_TRAM + tram_offset, PAL_NONE);
		}
	}

	/* Draw road overlay */
	if (road_rti != NULL) {
		if (road_rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(road_rti, ti->tile, ROTSG_OVERLAY);
			if (ground != 0) DrawGroundSprite(ground + road_offset, PAL_NONE);
		}
	}

	/* Draw tram overlay */
	if (tram_rti != NULL) {
		if (tram_rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(tram_rti, ti->tile, ROTSG_OVERLAY);
			if (ground != 0) DrawGroundSprite(ground + tram_offset, PAL_NONE);
		} else if (road_rti != NULL) {
			DrawGroundSprite(SPR_TRAMWAY_OVERLAY + tram_offset, PAL_NONE);
		}
	}

	/* Draw one way */
	if (road_rti != NULL) {
		DisallowedRoadDirections drd = GetDisallowedRoadDirections(ti->tile);
		if (drd != DRD_NONE) {
			DrawGroundSpriteAt(SPR_ONEWAY_BASE + drd - 1 + ((road == ROAD_X) ? 0 : 3), PAL_NONE, 8, 8, GetPartialPixelZ(8, 8, ti->tileh));
		}
	}

	if (HasRoadWorks(ti->tile)) {
		/* Road works */
		DrawGroundSprite((road | tram) & ROAD_X ? SPR_EXCAVATION_X : SPR_EXCAVATION_Y, PAL_NONE);
		return;
	}

	/* Draw road, tram catenary */
	DrawRoadCatenary(ti);

	/* Return if full detail is disabled, or we are zoomed fully out. */
	if (!HasBit(_display_opt, DO_FULL_DETAIL) || _cur_dpi->zoom > ZOOM_LVL_DETAIL) return;

	/* Do not draw details (street lights, trees) under low bridge */
	if (IsBridgeAbove(ti->tile) && (roadside == ROADSIDE_TREES || roadside == ROADSIDE_STREET_LIGHTS)) {
		int height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));
		int minz = GetTileMaxZ(ti->tile) + 2;

		if (roadside == ROADSIDE_TREES) minz++;

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

			Axis axis = GetCrossingRailAxis(ti->tile);

			const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(ti->tile));

			RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(ti->tile);
			const RoadtypeInfo* road_rti = rtids.HasRoad() ? GetRoadTypeInfo(rtids.road_identifier) : NULL;
			const RoadtypeInfo* tram_rti = rtids.HasTram() ? GetRoadTypeInfo(rtids.tram_identifier) : NULL;

			/* Draw base ground */
			if (rti->UsesOverlay()) {
				PaletteID pal = PAL_NONE;
				SpriteID image = SPR_ROAD_Y + axis;

				Roadside roadside = GetRoadside(ti->tile);
				if (DrawRoadAsSnowDesert(ti->tile, roadside)) {
					image += 19;
				} else {
					switch (roadside) {
						case ROADSIDE_BARREN: pal = PALETTE_TO_BARE_LAND; break;
						case ROADSIDE_GRASS:  break;
						default:              image -= 19; break; // Paved
					}
				}

				DrawGroundSprite(image, pal);
			} else {
				PaletteID pal = PAL_NONE;
				SpriteID image = rti->base_sprites.crossing + axis;
				if (IsCrossingBarred(ti->tile)) image += 2;

				Roadside roadside = GetRoadside(ti->tile);
				if (DrawRoadAsSnowDesert(ti->tile, roadside)) {
					image += 8;
				} else {
					switch (roadside) {
						case ROADSIDE_BARREN: pal = PALETTE_TO_BARE_LAND; break;
						case ROADSIDE_GRASS:  break;
						default:              image += 4; break; // Paved
					}
				}

				DrawGroundSprite(image, pal);
			}

			/* Draw road/tram underlay; road underlay takes precendence over tram */
			if (road_rti != NULL) {
				if (road_rti->UsesOverlay()) {
					SpriteID ground = GetCustomRoadSprite(road_rti, ti->tile, ROTSG_GROUND);
					DrawGroundSprite(ground + axis, PAL_NONE);
				}
			} else {
				if (tram_rti->UsesOverlay()) {
					SpriteID ground = GetCustomRoadSprite(tram_rti, ti->tile, ROTSG_GROUND);
					DrawGroundSprite(ground + axis, PAL_NONE);
				} else {
					DrawGroundSprite(SPR_TRAMWAY_TRAM + axis, PAL_NONE);
				}
			}

			/* Draw road overlay */
			if (road_rti != NULL) {
				if (road_rti->UsesOverlay()) {
					SpriteID ground = GetCustomRoadSprite(road_rti, ti->tile, ROTSG_OVERLAY);
					if (ground != 0) DrawGroundSprite(ground + axis, PAL_NONE);
				}
			}

			/* Draw tram overlay */
			if (tram_rti != NULL) {
				if (tram_rti->UsesOverlay()) {
					SpriteID ground = GetCustomRoadSprite(tram_rti, ti->tile, ROTSG_OVERLAY);
					if (ground != 0) DrawGroundSprite(ground + axis, PAL_NONE);
				} else if (road_rti != NULL) {
					DrawGroundSprite(SPR_TRAMWAY_OVERLAY + axis, PAL_NONE);
				}
			}

			/* Draw rail/PBS overlay */
			bool draw_pbs = _game_mode != GM_MENU && _settings_client.gui.show_track_reservation && HasCrossingReservation(ti->tile);
			if (rti->UsesOverlay()) {
				PaletteID pal = draw_pbs ? PALETTE_CRASH : PAL_NONE;
				SpriteID rail = GetCustomRailSprite(rti, ti->tile, RTSG_CROSSING) + axis;
				DrawGroundSprite(rail, pal);

				DrawRailTileSeq(ti, &_crossing_layout, TO_CATENARY, rail, 0, PAL_NONE);
			} else if (draw_pbs || tram_rti != NULL || road_rti->UsesOverlay()) {
				/* Add another rail overlay, unless there is only the base road sprite. */
				PaletteID pal = draw_pbs ? PALETTE_CRASH : PAL_NONE;
				SpriteID rail = GetCrossingRoadAxis(ti->tile) == AXIS_Y ? GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.single_x : GetRailTypeInfo(GetRailType(ti->tile))->base_sprites.single_y;
				DrawGroundSprite(rail, pal);
			}

			/* Draw road, tram catenary */
			DrawRoadCatenary(ti);

			/* Draw rail catenary */
			if (HasRailCatenaryDrawn(GetRailType(ti->tile))) DrawRailCatenary(ti);

			break;
		}

		default:
		case ROAD_TILE_DEPOT: {
			if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

			PaletteID palette = COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tile));
			RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(ti->tile);
			const RoadtypeInfo* rti = GetRoadTypeInfo(rtids.HasRoad() ? rtids.road_identifier : rtids.tram_identifier);

			int relocation = GetCustomRoadSprite(rti, ti->tile, ROTSG_DEPOT);
			bool default_gfx = relocation == 0;
			if (default_gfx) {
				if (HasBit(rti->flags, ROTF_CATENARY)) {
					if (_loaded_newgrf_features.tram == TRAMWAY_REPLACE_DEPOT_WITH_TRACK && rtids.HasTram() && !rti->UsesOverlay()) {
						/* Sprites with track only work for default tram */
						relocation = SPR_TRAMWAY_DEPOT_WITH_TRACK - SPR_ROAD_DEPOT;
						default_gfx = false;
					} else {
						/* Sprites without track are always better, if provided */
						relocation = SPR_TRAMWAY_DEPOT_NO_TRACK - SPR_ROAD_DEPOT;
					}
				}
			} else {
				relocation -= SPR_ROAD_DEPOT;
			}

			DiagDirection dir = GetRoadDepotDirection(ti->tile);
			const DrawTileSprites *dts = &_road_depot[dir];
			DrawGroundSprite(dts->ground.sprite, PAL_NONE);

			if (default_gfx) {
				uint offset = GetRoadSpriteOffset(SLOPE_FLAT, DiagDirToRoadBits(dir));
				if (rti->UsesOverlay()) {
					SpriteID ground = GetCustomRoadSprite(rti, ti->tile, ROTSG_OVERLAY);
					if (ground != 0) DrawGroundSprite(ground + offset, PAL_NONE);
				} else if (rtids.HasTram()) {
					DrawGroundSprite(SPR_TRAMWAY_OVERLAY + offset, PAL_NONE);
				}
			}

			DrawRailTileSeq(ti, dts, TO_BUILDINGS, relocation, 0, palette);
			break;
		}
	}
	DrawBridgeMiddle(ti);
}

/**
 * Draw the road depot sprite.
 * @param x   The x offset to draw at.
 * @param y   The y offset to draw at.
 * @param dir The direction the depot must be facing.
 * @param rtid The road type of the depot to draw.
 */
void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadTypeIdentifier rtid)
{
	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);

	const RoadtypeInfo* rti = GetRoadTypeInfo(rtid);
	int relocation = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_DEPOT);
	bool default_gfx = relocation == 0;
	if (default_gfx) {
		if (HasBit(rti->flags, ROTF_CATENARY)) {
			if (_loaded_newgrf_features.tram == TRAMWAY_REPLACE_DEPOT_WITH_TRACK && rtid.IsTram() && !rti->UsesOverlay()) {
				/* Sprites with track only work for default tram */
				relocation = SPR_TRAMWAY_DEPOT_WITH_TRACK - SPR_ROAD_DEPOT;
				default_gfx = false;
			} else {
				/* Sprites without track are always better, if provided */
				relocation = SPR_TRAMWAY_DEPOT_NO_TRACK - SPR_ROAD_DEPOT;
			}
		}
	} else {
		relocation -= SPR_ROAD_DEPOT;
	}

	const DrawTileSprites *dts = &_road_depot[dir];
	DrawSprite(dts->ground.sprite, PAL_NONE, x, y);

	if (default_gfx) {
		uint offset = GetRoadSpriteOffset(SLOPE_FLAT, DiagDirToRoadBits(dir));
		if (rti->UsesOverlay()) {
			SpriteID ground = GetCustomRoadSprite(rti, INVALID_TILE, ROTSG_OVERLAY);
			if (ground != 0) DrawSprite(ground + offset, PAL_NONE, x, y);
		} else if (rtid.IsTram()) {
			DrawSprite(SPR_TRAMWAY_OVERLAY + offset, PAL_NONE, x, y);
		}
	}

	DrawRailTileSeqInGUI(x, y, dts, relocation, 0, palette);
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

static int GetSlopePixelZ_Road(TileIndex tile, uint x, uint y)
{

	if (IsNormalRoad(tile)) {
		int z;
		Slope tileh = GetTilePixelSlope(tile, &z);
		if (tileh == SLOPE_FLAT) return z;

		Foundation f = GetRoadFoundation(tileh, GetAllRoadBits(tile));
		z += ApplyPixelFoundationToSlope(f, &tileh);
		return z + GetPartialPixelZ(x & 0xF, y & 0xF, tileh);
	} else {
		return GetTileMaxPixelZ(tile);
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
				if (GetFoundationSlope(tile) == SLOPE_FLAT && EnsureNoVehicleOnGround(tile).Succeeded() && Chance16(1, 40)) {
					StartRoadWorks(tile);

					if (_settings_client.sound.ambient) SndPlayTileFx(SND_21_JACKHAMMER, tile);
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
				RemoveRoad(tile, DC_EXEC | DC_AUTO | DC_NO_WATER, (old_rb ^ new_rb), RoadTypeIdentifier(ROADTYPE_ROAD, ROADSUBTYPE_NORMAL), true);
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
static const TrackBits _road_trackbits[16] = {
	TRACK_BIT_NONE,                                  // ROAD_NONE
	TRACK_BIT_NONE,                                  // ROAD_NW
	TRACK_BIT_NONE,                                  // ROAD_SW
	TRACK_BIT_LEFT,                                  // ROAD_W
	TRACK_BIT_NONE,                                  // ROAD_SE
	TRACK_BIT_Y,                                     // ROAD_Y
	TRACK_BIT_LOWER,                                 // ROAD_S
	TRACK_BIT_LEFT | TRACK_BIT_LOWER | TRACK_BIT_Y,  // ROAD_Y | ROAD_SW
	TRACK_BIT_NONE,                                  // ROAD_NE
	TRACK_BIT_UPPER,                                 // ROAD_N
	TRACK_BIT_X,                                     // ROAD_X
	TRACK_BIT_LEFT | TRACK_BIT_UPPER | TRACK_BIT_X,  // ROAD_X | ROAD_NW
	TRACK_BIT_RIGHT,                                 // ROAD_E
	TRACK_BIT_RIGHT | TRACK_BIT_UPPER | TRACK_BIT_Y, // ROAD_Y | ROAD_NE
	TRACK_BIT_RIGHT | TRACK_BIT_LOWER | TRACK_BIT_X, // ROAD_X | ROAD_SE
	TRACK_BIT_ALL,                                   // ROAD_ALL
};

static TrackStatus GetTileTrackStatus_Road(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	TrackdirBits trackdirbits = TRACKDIR_BIT_NONE;
	TrackdirBits red_signals = TRACKDIR_BIT_NONE; // crossing barred
	switch (mode) {
		case TRANSPORT_RAIL:
			if (IsLevelCrossing(tile)) trackdirbits = TrackBitsToTrackdirBits(GetCrossingRailBits(tile));
			break;

		case TRANSPORT_ROAD: {
			RoadType rt = (RoadType)sub_mode;
			if (!HasTileRoadType(tile, rt)) break;
			switch (GetRoadTileType(tile)) {
				case ROAD_TILE_NORMAL: {
					const uint drd_to_multiplier[DRD_END] = { 0x101, 0x100, 0x1, 0x0 };
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
		}

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

	RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
	if (rtids.HasRoad()) {
		const RoadtypeInfo *rti = GetRoadTypeInfo(rtids.road_identifier);
		td->roadtype = rti->strings.name;
		td->road_speed = rti->max_speed / 2;
		road_owner = GetRoadOwner(tile, ROADTYPE_ROAD);
	}
	if (rtids.HasTram()) {
		const RoadtypeInfo *rti = GetRoadTypeInfo(rtids.tram_identifier);
		td->tramtype = rti->strings.name;
		td->tram_speed = rti->max_speed / 2;
		tram_owner = GetRoadOwner(tile, ROADTYPE_TRAM);
	}

	switch (GetRoadTileType(tile)) {
		case ROAD_TILE_CROSSING: {
			td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_RAIL_LEVEL_CROSSING;
			rail_owner = GetTileOwner(tile);

			const RailtypeInfo *rti = GetRailTypeInfo(GetRailType(tile));
			td->railtype = rti->strings.name;
			td->rail_speed = rti->max_speed;

			break;
		}

		case ROAD_TILE_DEPOT:
			td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT;
			td->build_date = Depot::GetByTile(tile)->build_date;
			break;

		default: {
			td->str = (rtids.HasRoad() ? _road_tile_strings[GetRoadside(tile)] : STR_LAI_ROAD_DESCRIPTION_TRAMWAY);
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
	RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
	if (IsRoadDepot(tile)) {
		if (GetTileOwner(tile) == old_owner) {
			if (new_owner == INVALID_OWNER) {
				DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);
			} else {
				/* A road depot has two road bits. No need to dirty windows here, we'll redraw the whole screen anyway. */
				RoadTypeIdentifier rtid = rtids.HasRoad() ? rtids.road_identifier : rtids.tram_identifier;
				Company::Get(old_owner)->infrastructure.road[rtid.basetype][rtid.subtype] -= 2;
				Company::Get(new_owner)->infrastructure.road[rtid.basetype][rtid.subtype] += 2;

				SetTileOwner(tile, new_owner);
				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					if (GetRoadOwner(tile, rt) == old_owner) {
						SetRoadOwner(tile, rt, new_owner);
					}
				}
			}
		}
		return;
	}

	for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
		/* Update all roadtypes, no matter if they are present */
		if (GetRoadOwner(tile, rt) == old_owner) {
			RoadTypeIdentifier rtid = rt == ROADTYPE_ROAD ? rtids.road_identifier : rtids.tram_identifier;
			if (rtid.IsValid()) {
				/* A level crossing has two road bits. No need to dirty windows here, we'll redraw the whole screen anyway. */
				uint num_bits = IsLevelCrossing(tile) ? 2 : CountBits(GetRoadBits(tile, rt));
				Company::Get(old_owner)->infrastructure.road[rtid.basetype][rtid.subtype] -= num_bits;
				if (new_owner != INVALID_OWNER) Company::Get(new_owner)->infrastructure.road[rtid.basetype][rtid.subtype] += num_bits;
			}

			SetRoadOwner(tile, rt, new_owner == INVALID_OWNER ? OWNER_NONE : new_owner);
		}
	}

	if (IsLevelCrossing(tile)) {
		if (GetTileOwner(tile) == old_owner) {
			if (new_owner == INVALID_OWNER) {
				DoCommand(tile, 0, GetCrossingRailTrack(tile), DC_EXEC | DC_BANKRUPT, CMD_REMOVE_SINGLE_RAIL);
			} else {
				/* Update infrastructure counts. No need to dirty windows here, we'll redraw the whole screen anyway. */
				Company::Get(old_owner)->infrastructure.rail[GetRailType(tile)] -= LEVELCROSSING_TRACKBIT_FACTOR;
				Company::Get(new_owner)->infrastructure.rail[GetRailType(tile)] += LEVELCROSSING_TRACKBIT_FACTOR;

				SetTileOwner(tile, new_owner);
			}
		}
	}
}

static CommandCost TerraformTile_Road(TileIndex tile, DoCommandFlag flags, int z_new, Slope tileh_new)
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
						int z_old;
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

/** Update power of train under which is the railtype being converted */
static Vehicle *UpdateRoadVehPowerProc(Vehicle *v, void *data)
{
	if (v->type != VEH_ROAD) return NULL;

	RoadVehicleList *affected_trains = static_cast<RoadVehicleList*>(data);
	affected_trains->Include(RoadVehicle::From(v)->First());

	return NULL;
}

/**
 * Checks the tile and returns whether the current player is allowed to convert the roadtype to another roadtype
 * @param tile the tile to convert
 * @param to_type the RoadTypeIdentifier to be converted
 * @return whether the road is convertible
 */
static bool CanConvertRoadType(Owner owner, RoadType basetype)
{
	return (owner == OWNER_NONE || (owner == OWNER_TOWN && basetype == ROADTYPE_ROAD));
}

/**
 * Convert the ownership of the RoadType of the tile if applyable
 * @param tile the tile of which convert ownership
 * @param num_pieces the count of the roadbits to assign to the new owner
 * @param owner the current owner of the RoadType
 * @param from_type the old RoadTypeIdentifier
 * @param to_type the new RoadTypeIdentifier
 */
static void ConvertRoadTypeOwner(TileIndex tile, uint num_pieces, Owner owner, RoadTypeIdentifier from_type, RoadTypeIdentifier to_type)
{
	// Scenario editor, maybe? Don't touch the owners when converting roadtypes...
	if (_current_company >= MAX_COMPANIES) return;

	// We can't get a company from invalid owners but we can get ownership of roads without an owner
	if (owner >= MAX_COMPANIES && owner != OWNER_NONE) return;

	Company *c;

	switch (owner) {
	case OWNER_NONE:
		SetRoadOwner(tile, to_type.basetype, (Owner)_current_company);
		c = Company::Get(_current_company);
		c->infrastructure.road[to_type.basetype][to_type.subtype] += num_pieces;
		DirtyCompanyInfrastructureWindows(c->index);
		break;

	default:
		c = Company::Get(owner);
		c->infrastructure.road[from_type.basetype][from_type.subtype] -= num_pieces;
		c->infrastructure.road[to_type.basetype][to_type.subtype] += num_pieces;
		DirtyCompanyInfrastructureWindows(c->index);
		break;
	}
}

/**
 * Convert one road subtype to another.
 * Not meant to convert from road to tram.
 *
 * @param tile end tile of road conversion drag
 * @param flags operation to perform
 * @param p1 start tile of drag
 * @param p2 various bitstuffed elements:
 * - p2 = (bit  0..4) new roadtype to convert to.
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdConvertRoad(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2,  const char *text)
{
	RoadTypeIdentifier to_type;
	if (!to_type.UnpackIfValid(GB(p2, 0, 5))) return CMD_ERROR;

	TileIndex area_start = p1;
	TileIndex area_end = tile;

	if (!ValParamRoadType(to_type)) return CMD_ERROR;
	if (area_start >= MapSize()) return CMD_ERROR;

	RoadVehicleList affected_rvs;

	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost error = CommandCost(to_type.IsRoad() ? STR_ERROR_NO_SUITABLE_ROAD : STR_ERROR_NO_SUITABLE_TRAMWAY); // by default, there is no road to convert.

	TileIterator *iter = new OrthogonalTileIterator(area_start, area_end);
	for (; (tile = *iter) != INVALID_TILE; ++(*iter)) {
		/* Is road present on tile? */
		if (!MayHaveRoad(tile)) continue;
		RoadTypeIdentifiers rtids = RoadTypeIdentifiers::FromTile(tile);
		if (!rtids.HasType(to_type.basetype)) continue;

		/* Converting to the same subtype? */
		RoadTypeIdentifier from_type = rtids.GetType(to_type.basetype);
		if (from_type.subtype == to_type.subtype) continue;

		/* Check if there is any infrastructure on tile */
		TileType tt = GetTileType(tile);
		switch (tt) {
			case MP_STATION:
				if (!IsRoadStop(tile)) continue;
				break;
			case MP_ROAD:
				if (IsLevelCrossing(tile) && RoadNoLevelCrossing(to_type)) {
					error.MakeError(STR_ERROR_CROSSING_DISALLOWED_ROAD);
					continue;
				}
				break;
			case MP_TUNNELBRIDGE:
				if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) continue;
				break;
			default: continue;
		}

		/* Trying to convert other's road */ // TODO allow upgrade?
		Owner owner = GetRoadOwner(tile, to_type.basetype);
		if (!CanConvertRoadType(owner, to_type.basetype)) {
			CommandCost ret = CheckOwnership(owner, tile);
			if (ret.Failed()) {
				error = ret;
				continue;
			}
		}

		/* Vehicle on the tile when not converting normal <-> powered
		 * Tunnels and bridges have special check later */
		if (tt != MP_TUNNELBRIDGE) {
			if (!HasPowerOnRoad(from_type, to_type)) {
				CommandCost ret = EnsureNoVehicleOnGround(tile);
				if (ret.Failed()) {
					error = ret;
					continue;
				}

				if (to_type.basetype == ROADTYPE_ROAD && owner == OWNER_TOWN) {
					error.MakeError(STR_ERROR_INCOMPATIBLE_ROAD);
					continue;
				}
			}

			uint num_pieces = CountBits(GetAnyRoadBits(tile, from_type.basetype));;
			cost.AddCost(num_pieces * RoadConvertCost(from_type, to_type));

			if (flags & DC_EXEC) { // we can safely convert, too
				/* Update the company infrastructure counters. */
				if (!IsRoadStopTile(tile) && CanConvertRoadType(owner, to_type.basetype) && owner != OWNER_TOWN) {
					ConvertRoadTypeOwner(tile, num_pieces, owner, from_type, to_type);
				}

				/* Perform the conversion */
				rtids.MergeRoadType(to_type);
				SetRoadTypes(tile, rtids);
				MarkTileDirtyByTile(tile);

				/* update power of train on this tile */
				FindVehicleOnPos(tile, &affected_rvs, &UpdateRoadVehPowerProc);

				if (IsRoadDepotTile(tile)) {
					/* Update build vehicle window related to this depot */
					InvalidateWindowData(WC_VEHICLE_DEPOT, tile);
					InvalidateWindowData(WC_BUILD_VEHICLE, tile);
				}
			}
		} else {
			TileIndex endtile = GetOtherTunnelBridgeEnd(tile);

			/* If both ends of tunnel/bridge are in the range, do not try to convert twice -
			 * it would cause assert because of different test and exec runs */
			if (endtile < tile) {
				if (OrthogonalTileArea(area_start, area_end).Contains(endtile)) continue;
			}

			/* When not converting rail <-> el. rail, any vehicle cannot be in tunnel/bridge */
			if (!HasPowerOnRoad(from_type, to_type)) {
				CommandCost ret = TunnelBridgeIsFree(tile, endtile);
				if (ret.Failed()) {
					error = ret;
					continue;
				}

				if (to_type.basetype == ROADTYPE_ROAD && owner == OWNER_TOWN) {
					error.MakeError(STR_ERROR_INCOMPATIBLE_ROAD);
					continue;
				}
			}

			/* There are 2 pieces on *every* tile of the bridge or tunnel */
			uint num_pieces = (GetTunnelBridgeLength(tile, endtile) + 2) * 2;
			cost.AddCost(num_pieces * RoadConvertCost(from_type, to_type));

			if (flags & DC_EXEC) {
				/* Update the company infrastructure counters. */
				if (CanConvertRoadType(owner, to_type.basetype) && owner != OWNER_TOWN) {
					ConvertRoadTypeOwner(tile, num_pieces, owner, from_type, to_type);
					ConvertRoadTypeOwner(endtile, num_pieces, owner, from_type, to_type);
					SetTunnelBridgeOwner(tile, endtile, _current_company);
				}

				/* Perform the conversion */
				rtids.MergeRoadType(to_type);
				SetRoadTypes(tile, rtids);
				SetRoadTypes(endtile, rtids);

				FindVehicleOnPos(tile, &affected_rvs, &UpdateRoadVehPowerProc);
				FindVehicleOnPos(endtile, &affected_rvs, &UpdateRoadVehPowerProc);

				if (IsBridge(tile)) {
					MarkBridgeDirty(tile);
				} else {
					MarkTileDirtyByTile(tile);
					MarkTileDirtyByTile(endtile);
				}
			}
		}
	}

	if (flags & DC_EXEC) {
		/* Roadtype changed, update roadvehicles as when entering different track */
		for (RoadVehicle **v = affected_rvs.Begin(); v != affected_rvs.End(); v++) {
			(*v)->CargoChanged();
		}
	}

	delete iter;
	return (cost.GetCost() == 0) ? error : cost;
}


/** Tile callback functions for road tiles */
extern const TileTypeProcs _tile_type_road_procs = {
	DrawTile_Road,           // draw_tile_proc
	GetSlopePixelZ_Road,     // get_slope_z_proc
	ClearTile_Road,          // clear_tile_proc
	NULL,                    // add_accepted_cargo_proc
	GetTileDesc_Road,        // get_tile_desc_proc
	GetTileTrackStatus_Road, // get_tile_track_status_proc
	ClickTile_Road,          // click_tile_proc
	NULL,                    // animate_tile_proc
	TileLoop_Road,           // tile_loop_proc
	ChangeTileOwner_Road,    // change_tile_owner_proc
	NULL,                    // add_produced_cargo_proc
	VehicleEnter_Road,       // vehicle_enter_tile_proc
	GetFoundation_Road,      // get_foundation_proc
	TerraformTile_Road,      // terraform_tile_proc
};
