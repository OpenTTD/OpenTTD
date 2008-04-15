/* $Id$ */

/** @file town_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "tile_cmd.h"
#include "debug.h"
#include "road_map.h"
#include "road_internal.h" /* Cleaning up road bits */
#include "landscape.h"
#include "town_map.h"
#include "tunnel_map.h"
#include "viewport_func.h"
#include "town.h"
#include "command_func.h"
#include "industry.h"
#include "station_base.h"
#include "player_base.h"
#include "news_func.h"
#include "saveload.h"
#include "gui.h"
#include "unmovable_map.h"
#include "water_map.h"
#include "variables.h"
#include "bridge.h"
#include "bridge_map.h"
#include "genworld.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_house.h"
#include "newgrf_commons.h"
#include "newgrf_townname.h"
#include "misc/autoptr.hpp"
#include "autoslope.h"
#include "waypoint.h"
#include "transparency.h"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "window_func.h"
#include "string_func.h"
#include "newgrf_cargo.h"
#include "oldpool_func.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/town_land.h"

uint _total_towns;
HouseSpec _house_specs[HOUSE_MAX];

bool _town_sort_dirty;
byte _town_sort_order;
const Town **_town_sort;

Town *_cleared_town;
int _cleared_town_rating;

/* Initialize the town-pool */
DEFINE_OLD_POOL_GENERIC(Town, Town)

Town::Town(TileIndex tile)
{
	if (tile != 0) _total_towns++;
	this->xy = tile;
}

Town::~Town()
{
	free(this->name);

	if (CleaningPool()) return;

	Industry *i;

	/* Delete town authority window
	 * and remove from list of sorted towns */
	DeleteWindowById(WC_TOWN_VIEW, this->index);
	_town_sort_dirty = true;
	_total_towns--;

	/* Delete all industries belonging to the town */
	FOR_ALL_INDUSTRIES(i) if (i->town == this) delete i;

	/* Go through all tiles and delete those belonging to the town */
	for (TileIndex tile = 0; tile < MapSize(); ++tile) {
		switch (GetTileType(tile)) {
			case MP_HOUSE:
				if (GetTownByTile(tile) == this) DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				break;

			case MP_ROAD:
			case MP_TUNNELBRIDGE:
				if (IsTileOwner(tile, OWNER_TOWN) &&
						ClosestTownFromTile(tile, (uint)-1) == this)
					DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				break;

			default:
				break;
		}
	}

	DeleteSubsidyWithTown(this->index);

	MarkWholeScreenDirty();

	this->xy = 0;
}

/**
 * Generate a random town road layout.
 *
 * The layout is based on the TileHash.
 */
void Town::InitializeLayout()
{
	this->layout = (TownLayout)(TileHash(TileX(this->xy), TileY(this->xy)) % NUM_TLS);

	/* Set invalid layouts to valid ones */
	switch (this->layout) {
		default: break;
		case TL_RANDOM: this->layout = TL_ORIGINAL; break;
		case TL_NO_ROADS: this->layout = TL_BETTER_ROADS; break;
	}
}

// Local
static int _grow_town_result;

/* Describe the possible states */
enum TownGrowthResult {
	GROWTH_SUCCEED         = -1,
	GROWTH_SEARCH_STOPPED  =  0
//	GROWTH_SEARCH_RUNNING >=  1
};

static bool BuildTownHouse(Town *t, TileIndex tile);

static void TownDrawHouseLift(const TileInfo *ti)
{
	AddChildSpriteScreen(SPR_LIFT, PAL_NONE, 14, 60 - GetLiftPosition(ti->tile));
}

typedef void TownDrawTileProc(const TileInfo *ti);
static TownDrawTileProc *const _town_draw_tile_procs[1] = {
	TownDrawHouseLift
};

/**
 * Return a random direction
 *
 * @return a random direction
 */
static inline DiagDirection RandomDiagDir()
{
	return (DiagDirection)(3 & Random());
}

/**
 * House Tile drawing handler.
 * Part of the tile loop process
 * @param ti TileInfo of the tile to draw
 */
static void DrawTile_Town(TileInfo *ti)
{
	HouseID house_id = GetHouseType(ti->tile);

	if (house_id >= NEW_HOUSE_OFFSET) {
		/* Houses don't necessarily need new graphics. If they don't have a
		 * spritegroup associated with them, then the sprite for the substitute
		 * house id is drawn instead. */
		if (GetHouseSpecs(house_id)->spritegroup != NULL) {
			DrawNewHouseTile(ti, house_id);
			return;
		} else {
			house_id = GetHouseSpecs(house_id)->substitute_id;
		}
	}

	/* Retrieve pointer to the draw town tile struct */
	const DrawBuildingsTileStruct *dcts = &_town_draw_tile_data[house_id << 4 | TileHash2Bit(ti->x, ti->y) << 2 | GetHouseBuildingStage(ti->tile)];

	if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, FOUNDATION_LEVELED);

	DrawGroundSprite(dcts->ground.sprite, dcts->ground.pal);

	/* If houses are invisible, do not draw the upper part */
	if (IsInvisibilitySet(TO_HOUSES)) return;

	/* Add a house on top of the ground? */
	SpriteID image = dcts->building.sprite;
	if (image != 0) {
		AddSortableSpriteToDraw(image, dcts->building.pal,
			ti->x + dcts->subtile_x,
			ti->y + dcts->subtile_y,
			dcts->width,
			dcts->height,
			dcts->dz,
			ti->z,
			IsTransparencySet(TO_HOUSES)
		);

		if (IsTransparencySet(TO_HOUSES)) return;
	}

	{
		int proc = dcts->draw_proc - 1;

		if (proc >= 0) _town_draw_tile_procs[proc](ti);
	}
}

static uint GetSlopeZ_Town(TileIndex tile, uint x, uint y)
{
	return GetTileMaxZ(tile);
}

static Foundation GetFoundation_Town(TileIndex tile, Slope tileh)
{
	return FlatteningFoundation(tileh);
}

/**
 * Animate a tile for a town
 * Only certain houses can be animated
 * The newhouses animation superseeds regular ones
 * @param tile TileIndex of the house to animate
 */
static void AnimateTile_Town(TileIndex tile)
{
	if (GetHouseType(tile) >= NEW_HOUSE_OFFSET) {
		AnimateNewHouseTile(tile);
		return;
	}

	if (_tick_counter & 3) return;

	/* If the house is not one with a lift anymore, then stop this animating.
	 * Not exactly sure when this happens, but probably when a house changes.
	 * Before this was just a return...so it'd leak animated tiles..
	 * That bug seems to have been here since day 1?? */
	if (!(GetHouseSpecs(GetHouseType(tile))->building_flags & BUILDING_IS_ANIMATED)) {
		DeleteAnimatedTile(tile);
		return;
	}

	if (!LiftHasDestination(tile)) {
		uint i;

		/* Building has 6 floors, number 0 .. 6, where 1 is illegal.
		 * This is due to the fact that the first floor is, in the graphics,
		 *  the height of 2 'normal' floors.
		 * Furthermore, there are 6 lift positions from floor N (incl) to floor N + 1 (excl) */
		do {
			i = RandomRange(7);
		} while (i == 1 || i * 6 == GetLiftPosition(tile));

		SetLiftDestination(tile, i);
	}

	int pos = GetLiftPosition(tile);
	int dest = GetLiftDestination(tile) * 6;
	pos += (pos < dest) ? 1 : -1;
	SetLiftPosition(tile, pos);

	if (pos == dest) HaltLift(tile);

	MarkTileDirtyByTile(tile);
}

/**
 * Determines if a town is close to a tile
 * @param tile TileIndex of the tile to query
 * @param dist maximum distance to be accepted
 * @returns true if the tile correspond to the distance criteria
 */
static bool IsCloseToTown(TileIndex tile, uint dist)
{
	const Town *t;

	FOR_ALL_TOWNS(t) {
		if (DistanceManhattan(tile, t->xy) < dist) return true;
	}
	return false;
}

/**
 * Marks the town sign as needing a repaint.
 *
 * This function marks the area of the sign of a town as dirty for repaint.
 *
 * @param t Town requesting town sign for repaint
 * @ingroup dirty
 */
static void MarkTownSignDirty(Town *t)
{
	MarkAllViewportsDirty(
		t->sign.left - 6,
		t->sign.top - 3,
		t->sign.left + t->sign.width_1 * 4 + 12,
		t->sign.top + 45
	);
}

/**
 * Resize the sign(label) of the town after changes in
 * population (creation or growth or else)
 * @param t Town to update
 */
void UpdateTownVirtCoord(Town *t)
{
	MarkTownSignDirty(t);
	Point pt = RemapCoords2(TileX(t->xy) * TILE_SIZE, TileY(t->xy) * TILE_SIZE);
	SetDParam(0, t->index);
	SetDParam(1, t->population);
	UpdateViewportSignPos(&t->sign, pt.x, pt.y - 24,
		_patches.population_in_label ? STR_TOWN_LABEL_POP : STR_TOWN_LABEL);
	MarkTownSignDirty(t);
}

/** Update the virtual coords needed to draw the town sign for all towns. */
void UpdateAllTownVirtCoords()
{
	Town *t;
	FOR_ALL_TOWNS(t) {
		UpdateTownVirtCoord(t);
	}
}

/**
 * Change the towns population
 * @param t Town which polulation has changed
 * @param mod polulation change (can be positive or negative)
 */
static void ChangePopulation(Town *t, int mod)
{
	t->population += mod;
	InvalidateWindow(WC_TOWN_VIEW, t->index);
	UpdateTownVirtCoord(t);

	if (_town_sort_order & 2) _town_sort_dirty = true;
}

/**
 * Determines the world population
 * Basically, count population of all towns, one by one
 * @return uint32 the calculated population of the world
 */
uint32 GetWorldPopulation()
{
	uint32 pop = 0;
	const Town *t;

	FOR_ALL_TOWNS(t) pop += t->population;
	return pop;
}

/**
 * Helper function for house completion stages progression
 * @param tile TileIndex of the house (or parts of it) to "grow"
 */
static void MakeSingleHouseBigger(TileIndex tile)
{
	assert(IsTileType(tile, MP_HOUSE));

	/* means it is completed, get out. */
	if (LiftHasDestination(tile)) return;

	/* progress in construction stages */
	IncHouseConstructionTick(tile);
	if (GetHouseConstructionTick(tile) != 0) return;

	/* Check and/or  */
	if (HasBit(GetHouseSpecs(GetHouseType(tile))->callback_mask, CBM_HOUSE_CONSTRUCTION_STATE_CHANGE)) {
		uint16 callback_res = GetHouseCallback(CBID_HOUSE_CONSTRUCTION_STATE_CHANGE, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback_res != CALLBACK_FAILED) ChangeHouseAnimationFrame(tile, callback_res);
	}

	if (IsHouseCompleted(tile)) {
		/* Now that construction is complete, we can add the population of the
		 * building to the town. */
		ChangePopulation(GetTownByTile(tile), GetHouseSpecs(GetHouseType(tile))->population);
	}
	MarkTileDirtyByTile(tile);
}

/** Make the house advances in its construction stages until completion
 * @param tile TileIndex of house
 */
static void MakeTownHouseBigger(TileIndex tile)
{
	uint flags = GetHouseSpecs(GetHouseType(tile))->building_flags;
	if (flags & BUILDING_HAS_1_TILE)  MakeSingleHouseBigger(TILE_ADDXY(tile, 0, 0));
	if (flags & BUILDING_2_TILES_Y)   MakeSingleHouseBigger(TILE_ADDXY(tile, 0, 1));
	if (flags & BUILDING_2_TILES_X)   MakeSingleHouseBigger(TILE_ADDXY(tile, 1, 0));
	if (flags & BUILDING_HAS_4_TILES) MakeSingleHouseBigger(TILE_ADDXY(tile, 1, 1));
}

/**
 * Periodic tic handler for houses and town
 * @param tile been asked to do its stuff
 */
static void TileLoop_Town(TileIndex tile)
{
	HouseID house_id = GetHouseType(tile);

	/* NewHouseTileLoop returns false if Callback 21 succeeded, i.e. the house
	 * doesn't exist any more, so don't continue here. */
	if (house_id >= NEW_HOUSE_OFFSET && !NewHouseTileLoop(tile)) return;

	if (!IsHouseCompleted(tile)) {
		/* Construction is not completed. See if we can go further in construction*/
		MakeTownHouseBigger(tile);
		return;
	}

	const HouseSpec *hs = GetHouseSpecs(house_id);

	/* If the lift has a destination, it is already an animated tile. */
	if ((hs->building_flags & BUILDING_IS_ANIMATED) &&
			house_id < NEW_HOUSE_OFFSET &&
			!LiftHasDestination(tile) &&
			Chance16(1, 2)) {
		AddAnimatedTile(tile);
	}

	Town *t = GetTownByTile(tile);
	uint32 r = Random();

	if (HasBit(hs->callback_mask, CBM_HOUSE_PRODUCE_CARGO)) {
		for (uint i = 0; i < 256; i++) {
			uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, r, house_id, t, tile);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoID cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grffile);
			if (cargo == CT_INVALID) continue;

			uint amt = GB(callback, 0, 8);
			uint moved = MoveGoodsToStation(tile, 1, 1, cargo, amt);

			const CargoSpec *cs = GetCargo(cargo);
			switch (cs->town_effect) {
				case TE_PASSENGERS:
					t->new_max_pass += amt;
					t->new_act_pass += moved;
					break;

				case TE_MAIL:
					t->new_max_mail += amt;
					t->new_act_mail += moved;
					break;

				default:
					break;
			}
		}
	} else {
		if (GB(r, 0, 8) < hs->population) {
			uint amt = GB(r, 0, 8) / 8 + 1;

			if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
			t->new_max_pass += amt;
			t->new_act_pass += MoveGoodsToStation(tile, 1, 1, CT_PASSENGERS, amt);
		}

		if (GB(r, 8, 8) < hs->mail_generation) {
			uint amt = GB(r, 8, 8) / 8 + 1;

			if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
			t->new_max_mail += amt;
			t->new_act_mail += MoveGoodsToStation(tile, 1, 1, CT_MAIL, amt);
		}
	}

	_current_player = OWNER_TOWN;

	if (hs->building_flags & BUILDING_HAS_1_TILE &&
			HasBit(t->flags12, TOWN_IS_FUNDED) &&
			CanDeleteHouse(tile) &&
			max(_cur_year - GetHouseConstructionYear(tile), 0) >= hs->minimum_life &&
			--t->time_until_rebuild == 0) {
		t->time_until_rebuild = GB(r, 16, 8) + 192;

		ClearTownHouse(t, tile);

		/* Rebuild with another house? */
		if (GB(r, 24, 8) >= 12) BuildTownHouse(t, tile);
	}

	_current_player = OWNER_NONE;
}

/**
 * Unused handler
 * @param tile unused
 */
static void ClickTile_Town(TileIndex tile)
{
	/* not used */
}

static CommandCost ClearTile_Town(TileIndex tile, byte flags)
{
	if ((flags & DC_AUTO) && !(flags & DC_AI_BUILDING)) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
	if (!CanDeleteHouse(tile)) return CMD_ERROR;

	const HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));

	CommandCost cost(EXPENSES_CONSTRUCTION);
	cost.AddCost(_price.remove_house * hs->removal_cost >> 8);

	int rating = hs->remove_rating_decrease;
	_cleared_town_rating += rating;
	Town *t = _cleared_town = GetTownByTile(tile);

	if (IsValidPlayer(_current_player)) {
		if (rating > t->ratings[_current_player] && !(flags & DC_NO_TOWN_RATING) && !_cheats.magic_bulldozer.value) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	ChangeTownRating(t, -rating, RATING_HOUSE_MINIMUM);
	if (flags & DC_EXEC) {
		ClearTownHouse(t, tile);
	}

	return cost;
}

static void GetProducedCargo_Town(TileIndex tile, CargoID *b)
{
	HouseID house_id = GetHouseType(tile);
	const HouseSpec *hs = GetHouseSpecs(house_id);
	Town *t = GetTownByTile(tile);

	if (HasBit(hs->callback_mask, CBM_HOUSE_PRODUCE_CARGO)) {
		for (uint i = 0; i < 256; i++) {
			uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, 0, house_id, t, tile);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoID cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grffile);

			if (cargo == CT_INVALID) continue;
			*(b++) = cargo;
		}
	} else {
		if (hs->population > 0) {
			*(b++) = CT_PASSENGERS;
		}
		if (hs->mail_generation > 0) {
			*(b++) = CT_MAIL;
		}
	}
}

static void GetAcceptedCargo_Town(TileIndex tile, AcceptedCargo ac)
{
	const HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));
	CargoID accepts[3];

	/* Set the initial accepted cargo types */
	for (uint8 i = 0; i < lengthof(accepts); i++) {
		accepts[i] = hs->accepts_cargo[i];
	}

	/* Check for custom accepted cargo types */
	if (HasBit(hs->callback_mask, CBM_HOUSE_ACCEPT_CARGO)) {
		uint16 callback = GetHouseCallback(CBID_HOUSE_ACCEPT_CARGO, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback != CALLBACK_FAILED) {
			/* Replace accepted cargo types with translated values from callback */
			accepts[0] = GetCargoTranslation(GB(callback,  0, 5), hs->grffile);
			accepts[1] = GetCargoTranslation(GB(callback,  5, 5), hs->grffile);
			accepts[2] = GetCargoTranslation(GB(callback, 10, 5), hs->grffile);
		}
	}

	/* Check for custom cargo acceptance */
	if (HasBit(hs->callback_mask, CBM_HOUSE_CARGO_ACCEPTANCE)) {
		uint16 callback = GetHouseCallback(CBID_HOUSE_CARGO_ACCEPTANCE, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback != CALLBACK_FAILED) {
			if (accepts[0] != CT_INVALID) ac[accepts[0]] = GB(callback, 0, 4);
			if (accepts[1] != CT_INVALID) ac[accepts[1]] = GB(callback, 4, 4);
			if (_opt.landscape != LT_TEMPERATE && HasBit(callback, 12)) {
				/* The 'S' bit indicates food instead of goods */
				ac[CT_FOOD] = GB(callback, 8, 4);
			} else {
				if (accepts[2] != CT_INVALID) ac[accepts[2]] = GB(callback, 8, 4);
			}
			return;
		}
	}

	/* No custom acceptance, so fill in with the default values */
	for (uint8 i = 0; i < lengthof(accepts); i++) {
		if (accepts[i] != CT_INVALID) ac[accepts[i]] = hs->cargo_acceptance[i];
	}
}

static void GetTileDesc_Town(TileIndex tile, TileDesc *td)
{
	td->str = GetHouseSpecs(GetHouseType(tile))->building_name;
	if (!IsHouseCompleted(tile)) {
		SetDParamX(td->dparam, 0, td->str);
		td->str = STR_2058_UNDER_CONSTRUCTION;
	}

	td->owner = OWNER_TOWN;
}

static TrackStatus GetTileTrackStatus_Town(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	/* not used */
	return 0;
}

static void ChangeTileOwner_Town(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	/* not used */
}

static bool GrowTown(Town *t);

static void TownTickHandler(Town *t)
{
	if (HasBit(t->flags12, TOWN_IS_FUNDED)) {
		int i = t->grow_counter - 1;
		if (i < 0) {
			if (GrowTown(t)) {
				i = t->growth_rate;
			} else {
				i = 0;
			}
		}
		t->grow_counter = i;
	}

	UpdateTownRadius(t);
}

void OnTick_Town()
{
	if (_game_mode == GM_EDITOR) return;

	/* Make sure each town's tickhandler invocation frequency is about the
	 * same - TOWN_GROWTH_FREQUENCY - independent on the number of towns. */
	for (_cur_town_iter += GetMaxTownIndex() + 1;
	     _cur_town_iter >= TOWN_GROWTH_FREQUENCY;
	     _cur_town_iter -= TOWN_GROWTH_FREQUENCY) {
		uint32 i = _cur_town_ctr;

		if (++_cur_town_ctr > GetMaxTownIndex())
			_cur_town_ctr = 0;

		if (IsValidTownID(i)) TownTickHandler(GetTown(i));
	}
}

/**
 * Return the RoadBits of a tile
 *
 * @note There are many other functions doing things like that.
 * @note Needs to be checked for needlessness.
 * @param tile The tile we want to analyse
 * @return The roadbits of the given tile
 */
static RoadBits GetTownRoadBits(TileIndex tile)
{
	TrackBits b = GetAnyRoadTrackBits(tile, ROADTYPE_ROAD);
	RoadBits r = ROAD_NONE;

	if (b == TRACK_BIT_NONE) return r;
	if (b & TRACK_BIT_X)     r |= ROAD_X;
	if (b & TRACK_BIT_Y)     r |= ROAD_Y;
	if (b & TRACK_BIT_UPPER) r |= ROAD_NE | ROAD_NW;
	if (b & TRACK_BIT_LOWER) r |= ROAD_SE | ROAD_SW;
	if (b & TRACK_BIT_LEFT)  r |= ROAD_NW | ROAD_SW;
	if (b & TRACK_BIT_RIGHT) r |= ROAD_NE | ROAD_SE;
	return r;
}

/**
 * Check if a neighboring tile has a road
 *
 * @param tile curent tile
 * @param dir target direction
 * @param dist_multi distance multiplyer
 * @return true if one of the neighboring tiles at the
 *  given distance is a road tile else false
 */
static bool IsNeighborRoadTile(TileIndex tile, const DiagDirection dir, uint dist_multi)
{
	/* Lookup table for the used diff values */
	const TileIndexDiff tid_lt[3] = {
		TileOffsByDiagDir(ChangeDiagDir(dir, DIAGDIRDIFF_90RIGHT)),
		TileOffsByDiagDir(ChangeDiagDir(dir, DIAGDIRDIFF_90LEFT)),
		TileOffsByDiagDir(ReverseDiagDir(dir)),
	};

	/* We add 1 to the distance because we want to get 1 for
	 * the min distance multiplyer and not 0.
	 * Therefore we start at 4. The 4 is used because
	 * there are 4 tiles per distance step to check. */
	dist_multi = (dist_multi + 1) * 4;
	for (uint pos = 4; pos < dist_multi; pos++) {
		TileIndexDiff cur = 0;
		/* For each even value of pos add the right TileIndexDiff
		 * for each uneven value the left TileIndexDiff
		 * for each with 2nd bit set (2,3,6,7,..) add the reversed TileIndexDiff */
		cur += tid_lt[(pos & 1) ? 0 : 1];
		if (pos & 2) cur += tid_lt[2];

		cur = (uint)(pos / 4) * cur; // Multiply for the fitting distance
		if (GetTownRoadBits(TILE_ADD(tile, cur)) & DiagDirToRoadBits((pos & 2) ? dir : ReverseDiagDir(dir))) return true;
	}
	return false;
}

/**
 * Check if a Road is allowed on a given tile
 *
 * @param t The current town
 * @param tile The target tile
 * @param dir The direction in which we want to extend the town
 * @return true if it is allowed else false
 */
static bool IsRoadAllowedHere(Town *t, TileIndex tile, DiagDirection dir)
{
	if (TileX(tile) < 2 || TileX(tile) >= MapMaxX() || TileY(tile) < 2 || TileY(tile) >= MapMaxY()) return false;

	Slope cur_slope, desired_slope;

	for (;;) {
		/* Check if there already is a road at this point? */
		if (GetTownRoadBits(tile) == ROAD_NONE) {
			/* No, try if we are able to build a road piece there.
			 * If that fails clear the land, and if that fails exit.
			 * This is to make sure that we can build a road here later. */
			if (CmdFailed(DoCommand(tile, ((dir == DIAGDIR_NW || dir == DIAGDIR_SE) ? ROAD_X : ROAD_Y), 0, DC_AUTO, CMD_BUILD_ROAD)) &&
					CmdFailed(DoCommand(tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR)))
				return false;
		}

		cur_slope = GetTileSlope(tile, NULL);
		if (cur_slope == SLOPE_FLAT) {
no_slope:
			/* Tile has no slope */
			switch (t->GetActiveLayout()) {
				default: NOT_REACHED();

				case TL_ORIGINAL: // Disallow the road if any neighboring tile has a road (distance: 1)
					return !IsNeighborRoadTile(tile, dir, 1);

				case TL_BETTER_ROADS: // Disallow the road if any neighboring tile has a road (distance: 1 and 2).
					return !IsNeighborRoadTile(tile, dir, 2);
			}
		}

		/* If the tile is not a slope in the right direction, then
		 * maybe terraform some. */
		desired_slope = (dir == DIAGDIR_NW || dir == DIAGDIR_SE) ? SLOPE_NW : SLOPE_NE;
		if (desired_slope != cur_slope && ComplementSlope(desired_slope) != cur_slope) {
			if (Chance16(1, 8)) {
				CommandCost res = CMD_ERROR;
				if (!_generating_world && Chance16(1, 10)) {
					/* Note: Do not replace "^ SLOPE_ELEVATED" with ComplementSlope(). The slope might be steep. */
					res = DoCommand(tile, Chance16(1, 16) ? cur_slope : cur_slope ^ SLOPE_ELEVATED, 0,
							DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
				}
				if (CmdFailed(res) && Chance16(1, 3)) {
					/* We can consider building on the slope, though. */
					goto no_slope;
				}
			}
			return false;
		}
		return true;
	}
}

static bool TerraformTownTile(TileIndex tile, int edges, int dir)
{
	TILE_ASSERT(tile);

	CommandCost r = DoCommand(tile, edges, dir, DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
	if (CmdFailed(r) || r.GetCost() >= (_price.terraform + 2) * 8) return false;
	DoCommand(tile, edges, dir, DC_AUTO | DC_NO_WATER | DC_EXEC, CMD_TERRAFORM_LAND);
	return true;
}

static void LevelTownLand(TileIndex tile)
{
	TILE_ASSERT(tile);

	/* Don't terraform if land is plain or if there's a house there. */
	if (IsTileType(tile, MP_HOUSE)) return;
	Slope tileh = GetTileSlope(tile, NULL);
	if (tileh == SLOPE_FLAT) return;

	/* First try up, then down */
	if (!TerraformTownTile(tile, ~tileh & SLOPE_ELEVATED, 1)) {
		TerraformTownTile(tile, tileh & SLOPE_ELEVATED, 0);
	}
}

/**
 * Generate the RoadBits of a grid tile
 *
 * @param t current town
 * @param tile tile in reference to the town
 * @param dir The direction to which we are growing ATM
 * @return the RoadBit of the current tile regarding
 *  the selected town layout
 */
static RoadBits GetTownRoadGridElement(Town *t, TileIndex tile, DiagDirection dir)
{
	/* align the grid to the downtown */
	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile); // Vector from downtown to the tile
	RoadBits rcmd = ROAD_NONE;

	switch (t->GetActiveLayout()) {
		default: NOT_REACHED();

		case TL_2X2_GRID:
			if ((grid_pos.x % 3) == 0) rcmd |= ROAD_Y;
			if ((grid_pos.y % 3) == 0) rcmd |= ROAD_X;
			break;

		case TL_3X3_GRID:
			if ((grid_pos.x % 4) == 0) rcmd |= ROAD_Y;
			if ((grid_pos.y % 4) == 0) rcmd |= ROAD_X;
			break;
	}

	/* Optimise only X-junctions */
	if (rcmd != ROAD_ALL) return rcmd;

	RoadBits rb_template;

	switch (GetTileSlope(tile, NULL)) {
		default:       rb_template = ROAD_ALL; break;
		case SLOPE_W:  rb_template = ROAD_NW | ROAD_SW; break;
		case SLOPE_SW: rb_template = ROAD_Y  | ROAD_SW; break;
		case SLOPE_S:  rb_template = ROAD_SW | ROAD_SE; break;
		case SLOPE_SE: rb_template = ROAD_X  | ROAD_SE; break;
		case SLOPE_E:  rb_template = ROAD_SE | ROAD_NE; break;
		case SLOPE_NE: rb_template = ROAD_Y  | ROAD_NE; break;
		case SLOPE_N:  rb_template = ROAD_NE | ROAD_NW; break;
		case SLOPE_NW: rb_template = ROAD_X  | ROAD_NW; break;
		case SLOPE_STEEP_W:
		case SLOPE_STEEP_S:
		case SLOPE_STEEP_E:
		case SLOPE_STEEP_N:
			rb_template = ROAD_NONE;
			break;
	}

	/* Stop if the template is compatible to the growth dir */
	if (DiagDirToRoadBits(ReverseDiagDir(dir)) & rb_template) return rb_template;
	/* If not generate a straight road in the direction of the growth */
	return DiagDirToRoadBits(dir) | DiagDirToRoadBits(ReverseDiagDir(dir));
}

/**
 * Grows the town with an extra house.
 *  Check if there are enough neighbor house tiles
 *  next to the current tile. If there are enough
 *  add another house.
 *
 * @param t The current town
 * @param tile The target tile for the extra house
 * @return true if an extra house has been added
 */
static bool GrowTownWithExtraHouse(Town *t, TileIndex tile)
{
	/* We can't look further than that. */
	if (TileX(tile) < 2 || TileY(tile) < 2 || MapMaxX() <= TileX(tile) || MapMaxY() <= TileY(tile)) return false;

	uint counter = 0; // counts the house neighbor tiles

	/* Check the tiles E,N,W and S of the current tile for houses */
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {

		if (IsTileType(TileAddByDiagDir(tile, dir), MP_HOUSE)) counter++;

		/* If there are enough neighbors stop here */
		if (counter >= 3) {
			if (BuildTownHouse(t, tile)) {
				_grow_town_result = GROWTH_SUCCEED;
				return true;
			}
			return false;
		}
	}
	return false;
}

/**
 * Grows the town with a road piece.
 *
 * @param t The current town
 * @param tile The current tile
 * @param rcmd The RoadBits we want to build on the tile
 * @return true if the RoadBits have been added else false
 */
static bool GrowTownWithRoad(const Town *t, TileIndex tile, RoadBits rcmd)
{
	if (CmdSucceeded(DoCommand(tile, rcmd, t->index, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD))) {
		_grow_town_result = GROWTH_SUCCEED;
		return true;
	}
	return false;
}

/**
 * Grows the town with a bridge.
 *  At first we check if a bridge is reasonable.
 *  If so we check if we are able to build it.
 *
 * @param t The current town
 * @param tile The current tile
 * @param bridge_dir The valid direction in which to grow a bridge
 * @return true if a bridge has been build else false
 */
static bool GrowTownWithBridge(const Town *t, const TileIndex tile, const DiagDirection bridge_dir)
{
	assert(bridge_dir < DIAGDIR_END);

	const Slope slope = GetTileSlope(tile, NULL);
	if (slope == SLOPE_FLAT) return false; // no slope, no bridge

	/* Make sure the direction is compatible with the slope.
	 * Well we check if the slope has an up bit set in the
	 * reverse direction. */
	if (HASBITS(slope, InclinedSlope(bridge_dir))) return false;

	/* Assure that the bridge is connectable to the start side */
	if (!(GetTownRoadBits(TileAddByDiagDir(tile, ReverseDiagDir(bridge_dir))) & DiagDirToRoadBits(bridge_dir))) return false;

	/* We are in the right direction */
	uint8 bridge_length = 0;      // This value stores the length of the possible bridge
	TileIndex bridge_tile = tile; // Used to store the other waterside

	const int delta = TileOffsByDiagDir(bridge_dir);
	do {
		if (bridge_length++ >= 11) {
			/* Max 11 tile long bridges */
			return false;
		}
		bridge_tile += delta;
	} while (TileX(bridge_tile) != 0 && TileY(bridge_tile) != 0 && IsWaterTile(bridge_tile));

	/* no water tiles in between? */
	if (bridge_length == 1) return false;

	for (uint8 times = 0; times <= 22; times++) {
		byte bridge_type = RandomRange(MAX_BRIDGES - 1);

		/* Can we actually build the bridge? */
		if (CmdSucceeded(DoCommand(tile, bridge_tile, bridge_type | ((0x80 | ROADTYPES_ROAD) << 8), DC_AUTO, CMD_BUILD_BRIDGE))) {
			DoCommand(tile, bridge_tile, bridge_type | ((0x80 | ROADTYPES_ROAD) << 8), DC_EXEC | DC_AUTO, CMD_BUILD_BRIDGE);
			_grow_town_result = GROWTH_SUCCEED;
			return true;
		}
	}
	/* Quit if it selecting an appropiate bridge type fails a large number of times. */
	return false;
}

/**
 * Grows the given town.
 * There are at the moment 3 possible way's for
 * the town expansion:
 *  @li Generate a random tile and check if there is a road allowed
 *  @li TL_ORIGINAL
 *  @li TL_BETTER_ROADS
 *  @li Check if the town geometry allows a road and which one
 *  @li TL_2X2_GRID
 *  @li TL_3X3_GRID
 *  @li Forbid roads, only build houses
 *  @li TL_NO_ROADS
 *
 * @param tile_ptr The current tile
 * @param cur_rb The current tiles RoadBits
 * @param target_dir The target road dir
 * @param t1 The current town
 */
static void GrowTownInTile(TileIndex *tile_ptr, RoadBits cur_rb, DiagDirection target_dir, Town *t1)
{
	RoadBits rcmd = ROAD_NONE;  // RoadBits for the road construction command
	TileIndex tile = *tile_ptr; // The main tile on which we base our growth

	TILE_ASSERT(tile);

	if (cur_rb == ROAD_NONE) {
		/* Tile has no road. First reset the status counter
		 * to say that this is the last iteration. */
		_grow_town_result = GROWTH_SEARCH_STOPPED;

		/* Remove hills etc */
		LevelTownLand(tile);

		/* Is a road allowed here? */
		switch (t1->GetActiveLayout()) {
			default: NOT_REACHED();

			case TL_NO_ROADS: /* Disallow Roads */
				return;

			case TL_3X3_GRID:
			case TL_2X2_GRID:
				rcmd = GetTownRoadGridElement(t1, tile, target_dir);
				if (rcmd == ROAD_NONE) return;
				break;

			case TL_BETTER_ROADS:
			case TL_ORIGINAL:
				if (!IsRoadAllowedHere(t1, tile, target_dir)) return;

				DiagDirection source_dir = ReverseDiagDir(target_dir);

				if (Chance16(1, 4)) {
					/* Randomize a new target dir */
					do target_dir = RandomDiagDir(); while (target_dir == source_dir);
				}

				if (!IsRoadAllowedHere(t1, TileAddByDiagDir(tile, target_dir), target_dir)) {
					/* A road is not allowed to continue the randomized road,
					 *  return if the road we're trying to build is curved. */
					if (target_dir != ReverseDiagDir(source_dir)) return;

					/* Return if neither side of the new road is a house */
					if (!IsTileType(TileAddByDiagDir(tile, ChangeDiagDir(target_dir, DIAGDIRDIFF_90RIGHT)), MP_HOUSE) &&
							!IsTileType(TileAddByDiagDir(tile, ChangeDiagDir(target_dir, DIAGDIRDIFF_90LEFT)), MP_HOUSE)) {
						return;
					}

					/* That means that the road is only allowed if there is a house
					 *  at any side of the new road. */
				}

				rcmd = DiagDirToRoadBits(target_dir) | DiagDirToRoadBits(source_dir);
				break;
		}

	} else if (target_dir < DIAGDIR_END && !(cur_rb & DiagDirToRoadBits(ReverseDiagDir(target_dir)))) {
		/* Continue building on a partial road.
		 * Should be allways OK, so we only generate
		 * the fitting RoadBits */
		_grow_town_result = GROWTH_SEARCH_STOPPED;

		switch (t1->GetActiveLayout()) {
			default: NOT_REACHED();

			case TL_NO_ROADS: /* Disallow Roads */
				return;

			case TL_3X3_GRID:
			case TL_2X2_GRID:
			 	rcmd = GetTownRoadGridElement(t1, tile, target_dir);
				break;

			case TL_BETTER_ROADS:
			case TL_ORIGINAL:
				rcmd = DiagDirToRoadBits(ReverseDiagDir(target_dir));
				break;
		}
	} else {
		bool allow_house = false; // Value which decides if we want to construct a house

		/* Reached a tunnel/bridge? Then continue at the other side of it. */
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			if (GetTunnelBridgeTransportType(tile) == TRANSPORT_ROAD) {
				*tile_ptr = GetOtherTunnelBridgeEnd(tile);
			}
			return;
		}

		/* Possibly extend the road in a direction.
		 * Randomize a direction and if it has a road, bail out. */
		target_dir = RandomDiagDir();
		if (cur_rb & DiagDirToRoadBits(target_dir)) return;

		/* This is the tile we will reach if we extend to this direction. */
		TileIndex house_tile = TileAddByDiagDir(tile, target_dir); // position of a possible house

		/* Don't walk into water. */
		if (IsWaterTile(house_tile)) return;

		switch (t1->GetActiveLayout()) {
			default: NOT_REACHED();

			case TL_NO_ROADS:
				allow_house = true;
				break;

			case TL_3X3_GRID: /* Use 2x2 grid afterwards! */
				GrowTownWithExtraHouse(t1, TileAddByDiagDir(house_tile, target_dir));
				/* FALL THROUGH */

			case TL_2X2_GRID:
				rcmd = GetTownRoadGridElement(t1, house_tile, target_dir);
				allow_house = (rcmd == ROAD_NONE);
				break;

			case TL_BETTER_ROADS: /* Use original afterwards! */
				GrowTownWithExtraHouse(t1, TileAddByDiagDir(house_tile, target_dir));
				/* FALL THROUGH */

			case TL_ORIGINAL:
				 /* Allow a house at the edge. 60% chance or
				  * always ok if no road allowed. */
				rcmd = DiagDirToRoadBits(target_dir);
				allow_house = (!IsRoadAllowedHere(t1, house_tile, target_dir) || Chance16(6, 10));
				break;
		}

		if (allow_house) {
			/* Build a house, but not if there already is a house there. */
			if (!IsTileType(house_tile, MP_HOUSE)) {
				/* Level the land if possible */
				if (Chance16(1, 6)) LevelTownLand(house_tile);

				/* And build a house.
				 * Set result to -1 if we managed to build it. */
				if (BuildTownHouse(t1, house_tile)) {
					_grow_town_result = GROWTH_SUCCEED;
				}
			}
			return;
		}

		_grow_town_result = GROWTH_SEARCH_STOPPED;
	}

	/* Return if a water tile */
	if (IsWaterTile(tile)) return;

	/* Make the roads look nicer */
	rcmd = CleanUpRoadBits(tile, rcmd);
	if (rcmd == ROAD_NONE) return;

	/* Only use the target direction for bridges to ensure they're connected.
	 * The target_dir is as computed previously according to town layout, so
	 * it will match it perfectly. */
	if (GrowTownWithBridge(t1, tile, target_dir)) return;

	GrowTownWithRoad(t1, tile, rcmd);
}

/** Returns "growth" if a house was built, or no if the build failed.
 * @param t town to inquiry
 * @param tile to inquiry
 * @return something other than zero(0)if town expansion was possible
 */
static int GrowTownAtRoad(Town *t, TileIndex tile)
{
	/* Special case.
	 * @see GrowTownInTile Check the else if
	 */
	DiagDirection target_dir = DIAGDIR_END; // The direction in which we want to extend the town

	TILE_ASSERT(tile);

	/* Number of times to search.
	 * Better roads, 2X2 and 3X3 grid grow quite fast so we give
	 * them a little handicap. */
	switch (t->GetActiveLayout()) {
		case TL_BETTER_ROADS:
			_grow_town_result = 10 + t->num_houses * 2 / 9;
			break;

		case TL_3X3_GRID:
		case TL_2X2_GRID:
			_grow_town_result = 10 + t->num_houses * 1 / 9;
			break;

		default:
			_grow_town_result = 10 + t->num_houses * 4 / 9;
			break;
	}

	do {
		RoadBits cur_rb = GetTownRoadBits(tile); // The RoadBits of the current tile

		/* Try to grow the town from this point */
		GrowTownInTile(&tile, cur_rb, target_dir, t);

		/* Exclude the source position from the bitmask
		 * and return if no more road blocks available */
		cur_rb &= ~DiagDirToRoadBits(ReverseDiagDir(target_dir));
		if (cur_rb == ROAD_NONE)
			return _grow_town_result;

		/* Select a random bit from the blockmask, walk a step
		 * and continue the search from there. */
		do target_dir = RandomDiagDir(); while (!(cur_rb & DiagDirToRoadBits(target_dir)));
		tile = TileAddByDiagDir(tile, target_dir);

		if (IsTileType(tile, MP_ROAD)) {
			/* Don't allow building over roads of other cities */
			if (IsTileOwner(tile, OWNER_TOWN) && GetTownByTile(tile) != t) {
				_grow_town_result = GROWTH_SUCCEED;
			} else if (IsTileOwner(tile, OWNER_NONE) && _game_mode == GM_EDITOR) {
				/* If we are in the SE, and this road-piece has no town owner yet, it just found an
				 * owner :) (happy happy happy road now) */
				SetTileOwner(tile, OWNER_TOWN);
				SetTownIndex(tile, t->index);
			}
		}

		/* Max number of times is checked. */
	} while (--_grow_town_result >= 0);

	return (_grow_town_result == -2);
}

/**
 * Generate a random road block.
 * The probability of a straight road
 * is somewhat higher than a curved.
 *
 * @return A RoadBits value with 2 bits set
 */
static RoadBits GenRandomRoadBits()
{
	uint32 r = Random();
	uint a = GB(r, 0, 2);
	uint b = GB(r, 8, 2);
	if (a == b) b ^= 2;
	return (RoadBits)((ROAD_NW << a) + (ROAD_NW << b));
}

/** Grow the town
 * @param t town to grow
 * @return true iff a house was built
 */
static bool GrowTown(Town *t)
{
	/* Let the town be a ghost town
	 * The player wanted it in such a way. Thus there he has it. ;)
	 * Never reached in editor mode. */
	if (_patches.town_layout == TL_NO_ROADS && _generating_world) {
		return false;
	}

	static const TileIndexDiffC _town_coord_mod[] = {
		{-1,  0},
		{ 1,  1},
		{ 1, -1},
		{-1, -1},
		{-1,  0},
		{ 0,  2},
		{ 2,  0},
		{ 0, -2},
		{-1, -1},
		{-2,  2},
		{ 2,  2},
		{ 2, -2},
		{ 0,  0}
	};

	/* Current player is a town */
	PlayerID old_player = _current_player;
	_current_player = OWNER_TOWN;

	TileIndex tile = t->xy; // The tile we are working with ATM

	/* Find a road that we can base the construction on. */
	const TileIndexDiffC *ptr;
	for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
		if (GetTownRoadBits(tile) != ROAD_NONE) {
			int r = GrowTownAtRoad(t, tile);
			_current_player = old_player;
			return r != 0;
		}
		tile = TILE_ADD(tile, ToTileIndexDiff(*ptr));
	}

	/* No road available, try to build a random road block by
	 * clearing some land and then building a road there. */
	tile = t->xy;
	for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
		/* Only work with plain land that not already has a house */
		if (!IsTileType(tile, MP_HOUSE) && GetTileSlope(tile, NULL) == SLOPE_FLAT) {
			if (CmdSucceeded(DoCommand(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR))) {
				DoCommand(tile, GenRandomRoadBits(), t->index, DC_EXEC | DC_AUTO, CMD_BUILD_ROAD);
				_current_player = old_player;
				return true;
			}
		}
		tile = TILE_ADD(tile, ToTileIndexDiff(*ptr));
	}

	_current_player = old_player;
	return false;
}

void UpdateTownRadius(Town *t)
{
	static const uint16 _town_radius_data[23][5] = {
		{  4,  0,  0,  0,  0}, // 0
		{ 16,  0,  0,  0,  0},
		{ 25,  0,  0,  0,  0},
		{ 36,  0,  0,  0,  0},
		{ 49,  0,  4,  0,  0},
		{ 64,  0,  4,  0,  0}, // 20
		{ 64,  0,  9,  0,  1},
		{ 64,  0,  9,  0,  4},
		{ 64,  0, 16,  0,  4},
		{ 81,  0, 16,  0,  4},
		{ 81,  0, 16,  0,  4}, // 40
		{ 81,  0, 25,  0,  9},
		{ 81, 36, 25,  0,  9},
		{ 81, 36, 25, 16,  9},
		{ 81, 49,  0, 25,  9},
		{ 81, 64,  0, 25,  9}, // 60
		{ 81, 64,  0, 36,  9},
		{ 81, 64,  0, 36, 16},
		{100, 81,  0, 49, 16},
		{100, 81,  0, 49, 25},
		{121, 81,  0, 49, 25}, // 80
		{121, 81,  0, 49, 25},
		{121, 81,  0, 49, 36}, // 88
	};

	if (t->num_houses < 92) {
		memcpy(t->radius, _town_radius_data[t->num_houses / 4], sizeof(t->radius));
	} else {
		int mass = t->num_houses / 8;
		/* At least very roughly extrapolate. Empirical numbers dancing between
		 * overwhelming by cottages and skyscrapers outskirts. */
		t->radius[0] = mass * mass;
		/* Actually we are proportional to sqrt() but that's right because
		 * we are covering an area. */
		t->radius[1] = mass * 7;
		t->radius[2] = 0;
		t->radius[3] = mass * 4;
		t->radius[4] = mass * 3;
		//debug("%d (->%d): %d %d %d %d\n", t->num_houses, mass, t->radius[0], t->radius[1], t->radius[3], t->radius[4]);
	}
}

static bool CreateTownName(uint32 *townnameparts)
{
	extern int _nb_orig_names;
	Town *t2;
	char buf1[64];
	char buf2[64];
	uint32 r;
	/* Do not set too low tries, since when we run out of names, we loop
	 * for #tries only one time anyway - then we stop generating more
	 * towns. Do not show it too high neither, since looping through all
	 * the other towns may take considerable amount of time (10000 is
	 * too much). */
	int tries = 1000;
	bool grf = (_opt.town_name >= _nb_orig_names);
	uint32 grfid = grf ? GetGRFTownNameId(_opt.town_name - _nb_orig_names) : 0;
	uint16 townnametype = grf ? GetGRFTownNameType(_opt.town_name - _nb_orig_names) : SPECSTR_TOWNNAME_START + _opt.town_name;

	assert(townnameparts != NULL);

	for (;;) {
restart:
		r = Random();

		SetDParam(0, r);
		if (grf && grfid != 0) {
			GRFTownNameGenerate(buf1, grfid, townnametype, r, lastof(buf1));
		} else {
			GetString(buf1, townnametype, lastof(buf1));
		}

		/* Check size and width */
		if (strlen(buf1) >= 31 || GetStringBoundingBox(buf1).width > 130) continue;

		FOR_ALL_TOWNS(t2) {
			/* We can't just compare the numbers since
			 * several numbers may map to a single name. */
			SetDParam(0, t2->index);
			GetString(buf2, STR_TOWN, lastof(buf2));
			if (strcmp(buf1, buf2) == 0) {
				if (tries-- < 0) return false;
				goto restart;
			}
		}
		*townnameparts = r;
		return true;
	}
}

void UpdateTownMaxPass(Town *t)
{
	t->max_pass = t->population >> 3;
	t->max_mail = t->population >> 4;
}

/**
 * Does the actual town creation.
 *
 * @param t The town
 * @param tile Where to put it
 * @param townnameparts The town name
 * @param size_mode How the size should be determined
 * @param size Parameter for size determination
 */
static void DoCreateTown(Town *t, TileIndex tile, uint32 townnameparts, TownSizeMode size_mode, uint size)
{
	extern int _nb_orig_names;

	t->xy = tile;
	t->num_houses = 0;
	t->time_until_rebuild = 10;
	UpdateTownRadius(t);
	t->flags12 = 0;
	t->population = 0;
	t->grow_counter = 0;
	t->growth_rate = 250;
	t->new_max_pass = 0;
	t->new_max_mail = 0;
	t->new_act_pass = 0;
	t->new_act_mail = 0;
	t->max_pass = 0;
	t->max_mail = 0;
	t->act_pass = 0;
	t->act_mail = 0;

	t->pct_pass_transported = 0;
	t->pct_mail_transported = 0;
	t->fund_buildings_months = 0;
	t->new_act_food = 0;
	t->new_act_water = 0;
	t->act_food = 0;
	t->act_water = 0;

	for (uint i = 0; i != MAX_PLAYERS; i++) t->ratings[i] = RATING_INITIAL;

	t->have_ratings = 0;
	t->exclusivity = INVALID_PLAYER;
	t->exclusive_counter = 0;
	t->statues = 0;

	if (_opt.town_name < _nb_orig_names) {
		/* Original town name */
		t->townnamegrfid = 0;
		t->townnametype = SPECSTR_TOWNNAME_START + _opt.town_name;
	} else {
		/* Newgrf town name */
		t->townnamegrfid = GetGRFTownNameId(_opt.town_name  - _nb_orig_names);
		t->townnametype  = GetGRFTownNameType(_opt.town_name - _nb_orig_names);
	}
	t->townnameparts = townnameparts;

	UpdateTownVirtCoord(t);
	_town_sort_dirty = true;

	t->InitializeLayout();

	/* Random town size. */
	int x = (Random() & 0xF) + 8;

	switch (size_mode) {
		default: NOT_REACHED();

		case TSM_RANDOM:
			t->larger_town = false;
			break;

		case TSM_FIXED:
			x = size * 16 + 3;
			t->larger_town = false;
			break;

		case TSM_CITY:
			x *= _patches.initial_city_size;
			t->larger_town = true;
			break;
	}

	t->num_houses += x;
	UpdateTownRadius(t);

	int i = x * 4;
	do {
		GrowTown(t);
	} while (--i);

	t->num_houses -= x;
	UpdateTownRadius(t);
	UpdateTownMaxPass(t);
}

/** Create a new town.
 * This obviously only works in the scenario editor. Function not removed
 * as it might be possible in the future to fund your own town :)
 * @param tile coordinates where town is built
 * @param flags type of operation
 * @param p1 size of the town (0 = small, 1 = medium, 2 = large)
 * @param p2 size mode (@see TownSizeMode)
 */
CommandCost CmdBuildTown(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* Only in the scenario editor */
	if (_game_mode != GM_EDITOR) return CMD_ERROR;
	if (p2 > TSM_CITY) return CMD_ERROR;

	/* Check if too close to the edge of map */
	if (DistanceFromEdge(tile) < 12)
		return_cmd_error(STR_0237_TOO_CLOSE_TO_EDGE_OF_MAP);

	/* Can only build on clear flat areas, possibly with trees. */
	if ((!IsTileType(tile, MP_CLEAR) && !IsTileType(tile, MP_TREES)) || GetTileSlope(tile, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_0239_SITE_UNSUITABLE);
	}

	/* Check distance to all other towns. */
	if (IsCloseToTown(tile, 20))
		return_cmd_error(STR_0238_TOO_CLOSE_TO_ANOTHER_TOWN);

	uint32 townnameparts;

	/* Get a unique name for the town. */
	if (!CreateTownName(&townnameparts))
		return_cmd_error(STR_023A_TOO_MANY_TOWNS);

	/* Allocate town struct */
	Town *t = new Town(tile);
	if (t == NULL) return_cmd_error(STR_023A_TOO_MANY_TOWNS);
	AutoPtrT<Town> t_auto_delete = t;

	/* Create the town */
	if (flags & DC_EXEC) {
		_generating_world = true;
		DoCreateTown(t, tile, townnameparts, (TownSizeMode)p2, p1);
		_generating_world = false;
		t_auto_delete.Detach();
	}
	return CommandCost();
}

Town *CreateRandomTown(uint attempts, TownSizeMode mode, uint size)
{
	do {
		/* Generate a tile index not too close from the edge */
		TileIndex tile = RandomTile();
		if (DistanceFromEdge(tile) < 20) continue;

		/* Make sure the tile is plain */
		if (!IsTileType(tile, MP_CLEAR) || GetTileSlope(tile, NULL) != SLOPE_FLAT) continue;

		/* Check not too close to a town */
		if (IsCloseToTown(tile, 20)) continue;

		uint32 townnameparts;

		/* Get a unique name for the town. */
		if (!CreateTownName(&townnameparts)) break;

		/* Allocate a town struct */
		Town *t = new Town(tile);
		if (t == NULL) break;

		DoCreateTown(t, tile, townnameparts, mode, size);
		return t;
	} while (--attempts != 0);

	return NULL;
}

static const byte _num_initial_towns[4] = {5, 11, 23, 46};

bool GenerateTowns()
{
	uint num = 0;
	uint n = ScaleByMapSize(_num_initial_towns[_opt.diff.number_towns] + (Random() & 7));
	uint num_cities = _patches.larger_towns == 0 ? 0 : n / _patches.larger_towns;

	SetGeneratingWorldProgress(GWP_TOWN, n);

	do {
		IncreaseGeneratingWorldProgress(GWP_TOWN);
		/* try 20 times to create a random-sized town for the first loop. */
		TownSizeMode mode = num_cities > 0 ? TSM_CITY : TSM_RANDOM;
		if (CreateRandomTown(20, mode, _patches.initial_city_size) != NULL) num++;
		if (num_cities > 0) num_cities--;
	} while (--n);

	/* give it a last try, but now more aggressive */
	if (num == 0 && CreateRandomTown(10000, TSM_RANDOM, 0) == NULL) {
		if (GetNumTowns() == 0) {
			/* XXX - can we handle that more gracefully? */
			if (_game_mode != GM_EDITOR) error("Could not generate any town");

			return false;
		}
	}

	return true;
}


/** Returns the bit corresponding to the town zone of the specified tile
 * @param t Town on which radius is to be found
 * @param tile TileIndex where radius needs to be found
 * @return the bit position of the given zone, as defined in HouseZones
 */
HouseZonesBits GetTownRadiusGroup(const Town *t, TileIndex tile)
{
	uint dist = DistanceSquare(tile, t->xy);

	if (t->fund_buildings_months && dist <= 25) return HZB_TOWN_CENTRE;

	HouseZonesBits smallest = HZB_TOWN_EDGE;
	for (HouseZonesBits i = HZB_BEGIN; i < HZB_END; i++) {
		if (dist < t->radius[i]) smallest = i;
	}

	return smallest;
}

/**
 * Clears tile and builds a house or house part.
 * @param t tile index
 * @param tid Town index
 * @param counter of construction step
 * @param stage of construction (used for drawing)
 * @param type of house. Index into house specs array
 * @param random_bits required for newgrf houses
 * @pre house can be built here
 */
static inline void ClearMakeHouseTile(TileIndex tile, TownID tid, byte counter, byte stage, HouseID type, byte random_bits)
{
	#if !defined(NDEBUG) || defined(WITH_ASSERT)
		CommandCost cc =
	#endif /* !defined(NDEBUG) || defined(WITH_ASSERT) */

	DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR);

	assert(CmdSucceeded(cc));

	MakeHouseTile(tile, tid, counter, stage, type, random_bits);
}


/**
 * Write house information into the map. For houses > 1 tile, all tiles are marked.
 * @param t tile index
 * @param tid Town index
 * @param counter of construction step
 * @param stage of construction (used for drawing)
 * @param type of house. Index into house specs array
 * @param random_bits required for newgrf houses
 * @pre house can be built here
 */
static void MakeTownHouse(TileIndex t, TownID tid, byte counter, byte stage, HouseID type, byte random_bits)
{
	BuildingFlags size = GetHouseSpecs(type)->building_flags;

	ClearMakeHouseTile(t, tid, counter, stage, type, random_bits);
	if (size & BUILDING_2_TILES_Y)   ClearMakeHouseTile(t + TileDiffXY(0, 1), tid, counter, stage, ++type, random_bits);
	if (size & BUILDING_2_TILES_X)   ClearMakeHouseTile(t + TileDiffXY(1, 0), tid, counter, stage, ++type, random_bits);
	if (size & BUILDING_HAS_4_TILES) ClearMakeHouseTile(t + TileDiffXY(1, 1), tid, counter, stage, ++type, random_bits);
}


/**
 * Checks if a house can be built here. Important is slope, bridge above
 * and ability to clear the land.
 * @param tile tile to check
 * @param town town that is checking
 * @param noslope are slopes (foundations) allowed?
 * @return true iff house can be built here
 */
static inline bool CanBuildHouseHere(TileIndex tile, TownID town, bool noslope)
{
	/* cannot build on these slopes... */
	Slope slope = GetTileSlope(tile, NULL);
	if ((noslope && slope != SLOPE_FLAT) || IsSteepSlope(slope)) return false;

	/* building under a bridge? */
	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return false;

	/* do not try to build over house owned by another town */
	if (IsTileType(tile, MP_HOUSE) && GetTownIndex(tile) != town) return false;

	/* can we clear the land? */
	return CmdSucceeded(DoCommand(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR));
}


/**
 * Checks if a house can be built at this tile, must have the same max z as parameter.
 * @param tile tile to check
 * @param town town that is checking
 * @param z max z of this tile so more parts of a house are at the same height (with foundation)
 * @param noslope are slopes (foundations) allowed?
 * @return true iff house can be built here
 * @see CanBuildHouseHere()
 */
static inline bool CheckBuildHouseSameZ(TileIndex tile, TownID town, uint z, bool noslope)
{
	if (!CanBuildHouseHere(tile, town, noslope)) return false;

	/* if building on slopes is allowed, there will be flattening foundation (to tile max z) */
	if (GetTileMaxZ(tile) != z) return false;

	return true;
}


/**
 * Checks if a house of size 2x2 can be built at this tile
 * @param tile tile, N corner
 * @param town town that is checking
 * @param z maximum tile z so all tile have the same max z
 * @param noslope are slopes (foundations) allowed?
 * @return true iff house can be built
 * @see CheckBuildHouseSameZ()
 */
static bool CheckFree2x2Area(TileIndex tile, TownID town, uint z, bool noslope)
{
	/* we need to check this tile too because we can be at different tile now */
	if (!CheckBuildHouseSameZ(tile, town, z, noslope)) return false;

	for (DiagDirection d = DIAGDIR_SE; d < DIAGDIR_END; d++) {
		tile += TileOffsByDiagDir(d);
		if (!CheckBuildHouseSameZ(tile, town, z, noslope)) return false;
	}

	return true;
}


/**
 * Checks if current town layout allows building here
 * @param t town
 * @param tile tile to check
 * @return true iff town layout allows building here
 * @note see layouts
 */
static inline bool TownLayoutAllowsHouseHere(Town *t, TileIndex tile)
{
	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile);

	switch (t->GetActiveLayout()) {
		case TL_2X2_GRID:
			if ((grid_pos.x % 3) == 0 || (grid_pos.y % 3) == 0) return false;
			break;

		case TL_3X3_GRID:
			if ((grid_pos.x % 4) == 0 || (grid_pos.y % 4) == 0) return false;
			break;

		default:
			break;
	}

	return true;
}


/**
 * Checks if current town layout allows 2x2 building here
 * @param t town
 * @param tile tile to check
 * @return true iff town layout allows 2x2 building here
 * @note see layouts
 */
static inline bool TownLayoutAllows2x2HouseHere(Town *t, TileIndex tile)
{
	/* MapSize() is sure dividable by both MapSizeX() and MapSizeY(),
	 * so to do only one memory access, use MapSize() */
	uint dx = MapSize() + TileX(t->xy) - TileX(tile);
	uint dy = MapSize() + TileY(t->xy) - TileY(tile);

	switch (t->GetActiveLayout()) {
		case TL_2X2_GRID:
			if ((dx % 3) != 0 || (dy % 3) != 0) return false;
			break;

		case TL_3X3_GRID:
			if ((dx % 4) < 2 || (dy % 4) < 2) return false;
			break;

		default:
			break;
	}

	return true;
}


/**
 * Checks if 1x2 or 2x1 building is allowed here, also takes into account current town layout
 * Also, tests both building positions that occupy this tile
 * @param tile tile where the building should be built
 * @param t town
 * @param maxz all tiles should have the same height
 * @param noslope are slopes forbidden?
 * @param second diagdir from first tile to second tile
 **/
static bool CheckTownBuild2House(TileIndex *tile, Town *t, uint maxz, bool noslope, DiagDirection second)
{
	/* 'tile' is already checked in BuildTownHouse() - CanBuildHouseHere() and slope test */

	TileIndex tile2 = *tile + TileOffsByDiagDir(second);
	if (TownLayoutAllowsHouseHere(t, tile2) && CheckBuildHouseSameZ(tile2, t->index, maxz, noslope)) return true;

	tile2 = *tile + TileOffsByDiagDir(ReverseDiagDir(second));
	if (TownLayoutAllowsHouseHere(t, tile2) && CheckBuildHouseSameZ(tile2, t->index, maxz, noslope)) {
		*tile = tile2;
		return true;
	}

	return false;
}


/**
 * Checks if 2x2 building is allowed here, also takes into account current town layout
 * Also, tests all four building positions that occupy this tile
 * @param tile tile where the building should be built
 * @param t town
 * @param maxz all tiles should have the same height
 * @param noslope are slopes forbidden?
 **/
static bool CheckTownBuild2x2House(TileIndex *tile, Town *t, uint maxz, bool noslope)
{
	TileIndex tile2 = *tile;

	for (DiagDirection d = DIAGDIR_SE;;d++) { // 'd' goes through DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_END
		if (TownLayoutAllows2x2HouseHere(t, tile2) && CheckFree2x2Area(tile2, t->index, maxz, noslope)) {
			*tile = tile2;
			return true;
		}
		if (d == DIAGDIR_END) break;
		tile2 += TileOffsByDiagDir(ReverseDiagDir(d)); // go clockwise
	}

	return false;
}


/**
 * Tries to build a house at this tile
 * @param t town the house will belong to
 * @param tile where the house will be built
 * @return false iff no house can be built at this tile
 */
static bool BuildTownHouse(Town *t, TileIndex tile)
{
	/* forbidden building here by town layout */
	if (!TownLayoutAllowsHouseHere(t, tile)) return false;

	/* no house allowed at all, bail out */
	if (!CanBuildHouseHere(tile, t->index, false)) return false;

	uint z;
	Slope slope = GetTileSlope(tile, &z);

	/* Get the town zone type of the current tile, as well as the climate.
	 * This will allow to easily compare with the specs of the new house to build */
	HouseZonesBits rad = GetTownRadiusGroup(t, tile);

	/* Above snow? */
	int land = _opt.landscape;
	if (land == LT_ARCTIC && z >= _opt.snow_line) land = -1;

	uint bitmask = (1 << rad) + (1 << (land + 12));

	/* bits 0-4 are used
	 * bits 11-15 are used
	 * bits 5-10 are not used. */
	HouseID houses[HOUSE_MAX];
	uint num = 0;
	uint probs[HOUSE_MAX];
	uint probability_max = 0;

	/* Generate a list of all possible houses that can be built. */
	for (uint i = 0; i < HOUSE_MAX; i++) {
		const HouseSpec *hs = GetHouseSpecs(i);
		/* Verify that the candidate house spec matches the current tile status */
		if ((~hs->building_availability & bitmask) == 0 && hs->enabled) {
			/* Without NewHouses, all houses have probability '1' */
			uint cur_prob = (_loaded_newgrf_features.has_newhouses ? hs->probability : 1);
			probability_max += cur_prob;
			probs[num] = cur_prob;
			houses[num++] = (HouseID)i;
		}
	}

	uint maxz = GetTileMaxZ(tile);

	while (probability_max > 0) {
		uint r = RandomRange(probability_max);
		uint i;
		for (i = 0; i < num; i++) {
			if (probs[i] > r) break;
			r -= probs[i];
		}

		HouseID house = houses[i];
		probability_max -= probs[i];

		/* remove tested house from the set */
		num--;
		houses[i] = houses[num];
		probs[i] = probs[num];

		const HouseSpec *hs = GetHouseSpecs(house);

		if (_loaded_newgrf_features.has_newhouses) {
			if (hs->override != 0) {
				house = hs->override;
				hs = GetHouseSpecs(house);
			}

			if ((hs->extra_flags & BUILDING_IS_HISTORICAL) && !_generating_world) continue;

			if (HasBit(hs->callback_mask, CBM_HOUSE_ALLOW_CONSTRUCTION)) {
				uint16 callback_res = GetHouseCallback(CBID_HOUSE_ALLOW_CONSTRUCTION, 0, 0, house, t, tile);
				if (callback_res != CALLBACK_FAILED && callback_res == 0) continue;
			}
		}

		if (_cur_year < hs->min_date || _cur_year > hs->max_date) continue;

		/* Special houses that there can be only one of. */
		uint oneof = 0;

		if (hs->building_flags & BUILDING_IS_CHURCH) {
			SetBit(oneof, TOWN_HAS_CHURCH);
		} else if (hs->building_flags & BUILDING_IS_STADIUM) {
			SetBit(oneof, TOWN_HAS_STADIUM);
		}

		if (HASBITS(t->flags12 , oneof)) continue;

		/* Make sure there is no slope? */
		bool noslope = (hs->building_flags & TILE_NOT_SLOPED) != 0;
		if (noslope && slope != SLOPE_FLAT) continue;

		if (hs->building_flags & TILE_SIZE_2x2) {
			if (!CheckTownBuild2x2House(&tile, t, maxz, noslope)) continue;
		} else if (hs->building_flags & TILE_SIZE_2x1) {
			if (!CheckTownBuild2House(&tile, t, maxz, noslope, DIAGDIR_SW)) continue;
		} else if (hs->building_flags & TILE_SIZE_1x2) {
			if (!CheckTownBuild2House(&tile, t, maxz, noslope, DIAGDIR_SE)) continue;
		} else {
			/* 1x1 house checks are already done */
		}

		/* build the house */
		t->num_houses++;
		IncreaseBuildingCount(t, house);

		/* Special houses that there can be only one of. */
		t->flags12 |= oneof;

		byte construction_counter = 0;
		byte construction_stage = 0;

		if (_generating_world) {
			uint32 r = Random();

			construction_stage = TOWN_HOUSE_COMPLETED;
			if (Chance16(1, 7)) construction_stage = GB(r, 0, 2);

			if (construction_stage == TOWN_HOUSE_COMPLETED) {
				ChangePopulation(t, hs->population);
			} else {
				construction_counter = GB(r, 2, 2);
			}
		}

		MakeTownHouse(tile, t->index, construction_counter, construction_stage, house, Random());

		return true;
	}

	return false;
}


static void DoClearTownHouseHelper(TileIndex tile)
{
	assert(IsTileType(tile, MP_HOUSE));
	DoClearSquare(tile);
	DeleteAnimatedTile(tile);
}

/**
 * Determines if a given HouseID is part of a multitile house.
 * The given ID is set to the ID of the north tile and the TileDiff to the north tile is returned.
 *
 * @param house Is changed to the HouseID of the north tile of the same house
 * @return TileDiff from the tile of the given HouseID to the north tile
 */
static TileIndex GetHouseNorthPart(HouseID &house)
{
	if (house >= 3) { // house id 0,1,2 MUST be single tile houses, or this code breaks.
		if (GetHouseSpecs(house - 1)->building_flags & TILE_SIZE_2x1) {
			house--;
			return TileDiffXY(-1, 0);
		} else if (GetHouseSpecs(house - 1)->building_flags & BUILDING_2_TILES_Y) {
			house--;
			return TileDiffXY(0, -1);
		} else if (GetHouseSpecs(house - 2)->building_flags & BUILDING_HAS_4_TILES) {
			house -= 2;
			return TileDiffXY(-1, 0);
		} else if (GetHouseSpecs(house - 3)->building_flags & BUILDING_HAS_4_TILES) {
			house -= 3;
			return TileDiffXY(-1, -1);
		}
	}
	return 0;
}

void ClearTownHouse(Town *t, TileIndex tile)
{
	assert(IsTileType(tile, MP_HOUSE));

	HouseID house = GetHouseType(tile);

	/* need to align the tile to point to the upper left corner of the house */
	tile += GetHouseNorthPart(house); // modifies house to the ID of the north tile

	const HouseSpec *hs = GetHouseSpecs(house);

	/* Remove population from the town if the house is finished. */
	if (IsHouseCompleted(tile)) {
		ChangePopulation(t, -hs->population);
	}

	t->num_houses--;
	DecreaseBuildingCount(t, house);

	/* Clear flags for houses that only may exist once/town. */
	if (hs->building_flags & BUILDING_IS_CHURCH) {
		ClrBit(t->flags12, TOWN_HAS_CHURCH);
	} else if (hs->building_flags & BUILDING_IS_STADIUM) {
		ClrBit(t->flags12, TOWN_HAS_STADIUM);
	}

	/* Do the actual clearing of tiles */
	uint eflags = hs->building_flags;
	DoClearTownHouseHelper(tile);
	if (eflags & BUILDING_2_TILES_X)   DoClearTownHouseHelper(tile + TileDiffXY(1, 0));
	if (eflags & BUILDING_2_TILES_Y)   DoClearTownHouseHelper(tile + TileDiffXY(0, 1));
	if (eflags & BUILDING_HAS_4_TILES) DoClearTownHouseHelper(tile + TileDiffXY(1, 1));
}

static bool IsUniqueTownName(const char *name)
{
	const Town *t;
	char buf[512];

	FOR_ALL_TOWNS(t) {
		SetDParam(0, t->index);
		GetString(buf, STR_TOWN, lastof(buf));
		if (strcmp(buf, name) == 0) return false;
	}

	return true;
}

/** Rename a town (server-only).
 * @param tile unused
 * @param flags type of operation
 * @param p1 town ID to rename
 * @param p2 unused
 */
CommandCost CmdRenameTown(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidTownID(p1) || StrEmpty(_cmd_text)) return CMD_ERROR;

	if (!IsUniqueTownName(_cmd_text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);

	if (flags & DC_EXEC) {
		Town *t = GetTown(p1);

		free(t->name);
		t->name = strdup(_cmd_text);

		UpdateTownVirtCoord(t);
		_town_sort_dirty = true;
		UpdateAllStationVirtCoord();
		UpdateAllWaypointSigns();
		MarkWholeScreenDirty();
	}
	return CommandCost();
}

/** Called from GUI */
void ExpandTown(Town *t)
{
	_generating_world = true;

	/* The more houses, the faster we grow */
	uint amount = RandomRange(ClampToU16(t->num_houses / 10)) + 3;
	t->num_houses += amount;
	UpdateTownRadius(t);

	uint n = amount * 10;
	do GrowTown(t); while (--n);

	t->num_houses -= amount;
	UpdateTownRadius(t);

	UpdateTownMaxPass(t);
	_generating_world = false;
}

extern const byte _town_action_costs[8] = {
	2, 4, 9, 35, 48, 53, 117, 175
};

static void TownActionAdvertiseSmall(Town *t)
{
	ModifyStationRatingAround(t->xy, _current_player, 0x40, 10);
}

static void TownActionAdvertiseMedium(Town *t)
{
	ModifyStationRatingAround(t->xy, _current_player, 0x70, 15);
}

static void TownActionAdvertiseLarge(Town *t)
{
	ModifyStationRatingAround(t->xy, _current_player, 0xA0, 20);
}

static void TownActionRoadRebuild(Town *t)
{
	t->road_build_months = 6;

	SetDParam(0, t->index);
	SetDParam(1, _current_player);

	AddNewsItem(STR_2055_TRAFFIC_CHAOS_IN_ROAD_REBUILDING,
		NM_NORMAL, NF_TILE, NT_GENERAL, DNC_NONE, t->xy, 0);
}

static bool DoBuildStatueOfCompany(TileIndex tile, TownID town_id)
{
	/* Statues can be build on slopes, just like houses. Only the steep slopes is a no go. */
	if (IsSteepSlope(GetTileSlope(tile, NULL))) return false;

	if (!IsTileType(tile, MP_HOUSE) &&
			!IsTileType(tile, MP_CLEAR) &&
			!IsTileType(tile, MP_TREES)) {
		return false;
	}

	PlayerID old = _current_player;
	_current_player = OWNER_NONE;
	CommandCost r = DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	_current_player = old;

	if (CmdFailed(r)) return false;

	MakeStatue(tile, _current_player, town_id);
	MarkTileDirtyByTile(tile);

	return true;
}

/**
 * Search callback function for TownActionBuildStatue
 * @param tile on which to perform the search
 * @param town_id The town_id for which we want a statue
 * @return the result of the test
 */
static bool SearchTileForStatue(TileIndex tile, uint32 town_id)
{
	return DoBuildStatueOfCompany(tile, town_id);
}

/**
 * Perform a 9x9 tiles circular search from the center of the town
 * in order to find a free tile to place a statue
 * @param t town to search in
 */
static void TownActionBuildStatue(Town *t)
{
	TileIndex tile = t->xy;

	if (CircularTileSearch(tile, 9, SearchTileForStatue, t->index)) {
		SetBit(t->statues, _current_player); // Once found and built, "inform" the Town
	}
}

static void TownActionFundBuildings(Town *t)
{
	/* Build next tick */
	t->grow_counter = 1;
	/* If we were not already growing */
	SetBit(t->flags12, TOWN_IS_FUNDED);
	/* And grow for 3 months */
	t->fund_buildings_months = 3;
}

static void TownActionBuyRights(Town *t)
{
	/* Check if it's allowed to by the rights */
	if (!_patches.exclusive_rights) return;

	t->exclusive_counter = 12;
	t->exclusivity = _current_player;

	ModifyStationRatingAround(t->xy, _current_player, 130, 17);
}

static void TownActionBribe(Town *t)
{
	if (Chance16(1, 14)) {
		/* set as unwanted for 6 months */
		t->unwanted[_current_player] = 6;

		/* set all close by station ratings to 0 */
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->town == t && st->owner == _current_player) {
				for (CargoID i = 0; i < NUM_CARGO; i++) st->goods[i].rating = 0;
			}
		}

		/* only show errormessage to the executing player. All errors are handled command.c
		 * but this is special, because it can only 'fail' on a DC_EXEC */
		if (IsLocalPlayer()) ShowErrorMessage(STR_BRIBE_FAILED_2, STR_BRIBE_FAILED, 0, 0);

		/* decrease by a lot!
		 * ChangeTownRating is only for stuff in demolishing. Bribe failure should
		 * be independent of any cheat settings
		 */
		if (t->ratings[_current_player] > RATING_BRIBE_DOWN_TO) {
			t->ratings[_current_player] = RATING_BRIBE_DOWN_TO;
		}
	} else {
		ChangeTownRating(t, RATING_BRIBE_UP_STEP, RATING_BRIBE_MAXIMUM);
	}
}

typedef void TownActionProc(Town *t);
static TownActionProc *const _town_action_proc[] = {
	TownActionAdvertiseSmall,
	TownActionAdvertiseMedium,
	TownActionAdvertiseLarge,
	TownActionRoadRebuild,
	TownActionBuildStatue,
	TownActionFundBuildings,
	TownActionBuyRights,
	TownActionBribe
};

extern uint GetMaskOfTownActions(int *nump, PlayerID pid, const Town *t);

/** Do a town action.
 * This performs an action such as advertising, building a statue, funding buildings,
 * but also bribing the town-council
 * @param tile unused
 * @param flags type of operation
 * @param p1 town to do the action at
 * @param p2 action to perform, @see _town_action_proc for the list of available actions
 */
CommandCost CmdDoTownAction(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidTownID(p1) || p2 > lengthof(_town_action_proc)) return CMD_ERROR;

	Town *t = GetTown(p1);

	if (!HasBit(GetMaskOfTownActions(NULL, _current_player, t), p2)) return CMD_ERROR;

	CommandCost cost(EXPENSES_OTHER, (_price.build_industry >> 8) * _town_action_costs[p2]);

	if (flags & DC_EXEC) {
		_town_action_proc[p2](t);
		InvalidateWindow(WC_TOWN_AUTHORITY, p1);
	}

	return cost;
}

static void UpdateTownGrowRate(Town *t)
{
	/* Increase player ratings if they're low */
	const Player *p;
	FOR_ALL_PLAYERS(p) {
		if (p->is_active) {
			t->ratings[p->index] = min((int)RATING_GROWTH_MAXIMUM, t->ratings[p->index] + RATING_GROWTH_UP_STEP);
		}
	}

	int n = 0;

	const Station *st;
	FOR_ALL_STATIONS(st) {
		if (DistanceSquare(st->xy, t->xy) <= t->radius[0]) {
			if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
				n++;
				if (IsValidPlayer(st->owner)) {
					int new_rating = t->ratings[st->owner] + RATING_STATION_UP_STEP;
					t->ratings[st->owner] = min(new_rating, INT16_MAX); // do not let it overflow
				}
			} else {
				if (IsValidPlayer(st->owner)) {
					int new_rating = t->ratings[st->owner] + RATING_STATION_DOWN_STEP;
					t->ratings[st->owner] = max(new_rating, INT16_MIN);
				}
			}
		}
	}

	/* clamp all ratings to valid values */
	for (uint i = 0; i < MAX_PLAYERS; i++) {
		t->ratings[i] = Clamp(t->ratings[i], RATING_MINIMUM, RATING_MAXIMUM);
	}

	ClrBit(t->flags12, TOWN_IS_FUNDED);
	if (_patches.town_growth_rate == 0 && t->fund_buildings_months == 0) return;

	/** Towns are processed every TOWN_GROWTH_FREQUENCY ticks, and this is the
	 * number of times towns are processed before a new building is built. */
	static const uint16 _grow_count_values[2][6] = {
		{ 120, 120, 120, 100,  80,  60 }, // Fund new buildings has been activated
		{ 320, 420, 300, 220, 160, 100 }  // Normal values
	};

	uint16 m;

	if (t->fund_buildings_months != 0) {
		m = _grow_count_values[0][min(n, 5)];
		t->fund_buildings_months--;
	} else {
		m = _grow_count_values[1][min(n, 5)];
		if (n == 0 && !Chance16(1, 12)) return;
	}

	if (_opt.landscape == LT_ARCTIC) {
		if (TilePixelHeight(t->xy) >= GetSnowLine() && t->act_food == 0 && t->population > 90)
			return;
	} else if (_opt.landscape == LT_TROPIC) {
		if (GetTropicZone(t->xy) == TROPICZONE_DESERT && (t->act_food==0 || t->act_water==0) && t->population > 60)
			return;
	}

	/* Use the normal growth rate values if new buildings have been funded in
	 * this town and the growth rate is set to none. */
	uint growth_multiplier = _patches.town_growth_rate != 0 ? _patches.town_growth_rate - 1 : 1;

	m >>= growth_multiplier;
	if (t->larger_town) m /= 2;

	t->growth_rate = m / (t->num_houses / 50 + 1);
	if (m <= t->grow_counter)
		t->grow_counter = m;

	SetBit(t->flags12, TOWN_IS_FUNDED);
}

static void UpdateTownAmounts(Town *t)
{
	/* Using +1 here to prevent overflow and division by zero */
	t->pct_pass_transported = t->new_act_pass * 256 / (t->new_max_pass + 1);

	t->max_pass = t->new_max_pass; t->new_max_pass = 0;
	t->act_pass = t->new_act_pass; t->new_act_pass = 0;
	t->act_food = t->new_act_food; t->new_act_food = 0;
	t->act_water = t->new_act_water; t->new_act_water = 0;

	/* Using +1 here to prevent overflow and division by zero */
	t->pct_mail_transported = t->new_act_mail * 256 / (t->new_max_mail + 1);
	t->max_mail = t->new_max_mail; t->new_max_mail = 0;
	t->act_mail = t->new_act_mail; t->new_act_mail = 0;

	InvalidateWindow(WC_TOWN_VIEW, t->index);
}

static void UpdateTownUnwanted(Town *t)
{
	const Player *p;

	FOR_ALL_PLAYERS(p) {
		if (t->unwanted[p->index] > 0) t->unwanted[p->index]--;
	}
}

bool CheckIfAuthorityAllows(TileIndex tile)
{
	if (!IsValidPlayer(_current_player)) return true;

	Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
	if (t == NULL) return true;

	if (t->ratings[_current_player] > RATING_VERYPOOR) return true;

	_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
	SetDParam(0, t->index);

	return false;
}


Town *CalcClosestTownFromTile(TileIndex tile, uint threshold)
{
	Town *t;
	uint best = threshold;
	Town *best_town = NULL;

	FOR_ALL_TOWNS(t) {
		uint dist = DistanceManhattan(tile, t->xy);
		if (dist < best) {
			best = dist;
			best_town = t;
		}
	}

	return best_town;
}


Town *ClosestTownFromTile(TileIndex tile, uint threshold)
{
	if (IsTileType(tile, MP_HOUSE) || (
				IsTileType(tile, MP_ROAD) &&
				GetRoadOwner(tile, ROADTYPE_ROAD) == OWNER_TOWN
			)) {
		return GetTownByTile(tile);
	} else {
		return CalcClosestTownFromTile(tile, threshold);
	}
}

static bool _town_rating_test = false;

void SetTownRatingTestMode(bool mode)
{
	static int ref_count = 0;
	if (mode) {
		if (ref_count == 0) {
			Town *t;
			FOR_ALL_TOWNS(t) t->test_rating = t->ratings[_current_player];
		}
		ref_count++;
	} else {
		assert(ref_count > 0);
		ref_count--;
	}
	_town_rating_test = !(ref_count == 0);
}

void ChangeTownRating(Town *t, int add, int max)
{
	/* if magic_bulldozer cheat is active, town doesn't penaltize for removing stuff */
	if (t == NULL ||
			!IsValidPlayer(_current_player) ||
			(_cheats.magic_bulldozer.value && add < 0)) {
		return;
	}

	SetBit(t->have_ratings, _current_player);

	int rating = _town_rating_test ? t->test_rating : t->ratings[_current_player];

	if (add < 0) {
		if (rating > max) {
			rating += add;
			if (rating < max) rating = max;
		}
	} else {
		if (rating < max) {
			rating += add;
			if (rating > max) rating = max;
		}
	}
	if (_town_rating_test) {
		t->test_rating = rating;
	} else {
		t->ratings[_current_player] = rating;
	}
}

/* penalty for removing town-owned stuff */
static const int _default_rating_settings [3][3] = {
	/* ROAD_REMOVE, TUNNELBRIDGE_REMOVE, INDUSTRY_REMOVE */
	{  0, 128, 384}, // Permissive
	{ 48, 192, 480}, // Neutral
	{ 96, 384, 768}, // Hostile
};

bool CheckforTownRating(uint32 flags, Town *t, byte type)
{
	/* if magic_bulldozer cheat is active, town doesn't restrict your destructive actions */
	if (t == NULL || !IsValidPlayer(_current_player) || _cheats.magic_bulldozer.value)
		return true;

	/* check if you're allowed to remove the street/bridge/tunnel/industry
	 * owned by a town no removal if rating is lower than ... depends now on
	 * difficulty setting. Minimum town rating selected by difficulty level
	 */
	int modemod = _default_rating_settings[_opt.diff.town_council_tolerance][type];

	if ((_town_rating_test ? t->test_rating : t->ratings[_current_player]) < 16 + modemod && !(flags & DC_NO_TOWN_RATING)) {
		SetDParam(0, t->index);
		_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
		return false;
	}

	return true;
}

void TownsMonthlyLoop()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		if (t->road_build_months != 0) t->road_build_months--;

		if (t->exclusive_counter != 0)
			if (--t->exclusive_counter == 0) t->exclusivity = INVALID_PLAYER;

		UpdateTownGrowRate(t);
		UpdateTownAmounts(t);
		UpdateTownUnwanted(t);
	}
}

void InitializeTowns()
{
	/* Clean the town pool and create 1 block in it */
	_Town_pool.CleanPool();
	_Town_pool.AddBlockToPool();

	memset(_subsidies, 0, sizeof(_subsidies));
	for (Subsidy *s = _subsidies; s != endof(_subsidies); s++) {
		s->cargo_type = CT_INVALID;
	}

	_cur_town_ctr = 0;
	_cur_town_iter = 0;
	_total_towns = 0;
	_town_sort_dirty = true;
}

static CommandCost TerraformTile_Town(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	if (AutoslopeEnabled()) {
		HouseID house = GetHouseType(tile);
		GetHouseNorthPart(house); // modifies house to the ID of the north tile
		const HouseSpec *hs = GetHouseSpecs(house);

		/* Here we differ from TTDP by checking TILE_NOT_SLOPED */
		if (((hs->building_flags & TILE_NOT_SLOPED) == 0) && !IsSteepSlope(tileh_new) &&
			(GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new))) return CommandCost(EXPENSES_CONSTRUCTION, _price.terraform);
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

extern const TileTypeProcs _tile_type_town_procs = {
	DrawTile_Town,           /* draw_tile_proc */
	GetSlopeZ_Town,          /* get_slope_z_proc */
	ClearTile_Town,          /* clear_tile_proc */
	GetAcceptedCargo_Town,   /* get_accepted_cargo_proc */
	GetTileDesc_Town,        /* get_tile_desc_proc */
	GetTileTrackStatus_Town, /* get_tile_track_status_proc */
	ClickTile_Town,          /* click_tile_proc */
	AnimateTile_Town,        /* animate_tile_proc */
	TileLoop_Town,           /* tile_loop_clear */
	ChangeTileOwner_Town,    /* change_tile_owner_clear */
	GetProducedCargo_Town,   /* get_produced_cargo_proc */
	NULL,                    /* vehicle_enter_tile_proc */
	GetFoundation_Town,      /* get_foundation_proc */
	TerraformTile_Town,      /* terraform_tile_proc */
};


/** Save and load of towns. */
static const SaveLoad _town_desc[] = {
	SLE_CONDVAR(Town, xy,                    SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Town, xy,                    SLE_UINT32,                 6, SL_MAX_VERSION),

	SLE_CONDNULL(2, 0, 2),                   ///< population, no longer in use
	SLE_CONDNULL(4, 3, 84),                  ///< population, no longer in use
	SLE_CONDNULL(2, 0, 91),                  ///< num_houses, no longer in use

	SLE_CONDVAR(Town, townnamegrfid,         SLE_UINT32, 66, SL_MAX_VERSION),
	    SLE_VAR(Town, townnametype,          SLE_UINT16),
	    SLE_VAR(Town, townnameparts,         SLE_UINT32),
	SLE_CONDSTR(Town, name,                  SLE_STR, 0, 84, SL_MAX_VERSION),

	    SLE_VAR(Town, flags12,               SLE_UINT8),
	    SLE_VAR(Town, statues,               SLE_UINT8),

	SLE_CONDNULL(1, 0, 1),                   ///< sort_index, no longer in use

	    SLE_VAR(Town, have_ratings,          SLE_UINT8),
	    SLE_ARR(Town, ratings,               SLE_INT16, 8),
	/* failed bribe attempts are stored since savegame format 4 */
	SLE_CONDARR(Town, unwanted,              SLE_INT8, 8, 4, SL_MAX_VERSION),

	SLE_CONDVAR(Town, max_pass,              SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, max_mail,              SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, new_max_pass,          SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, new_max_mail,          SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, act_pass,              SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, act_mail,              SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, new_act_pass,          SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, new_act_mail,          SLE_FILE_U16 | SLE_VAR_U32, 0, 8),

	SLE_CONDVAR(Town, max_pass,              SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, max_mail,              SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, new_max_pass,          SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, new_max_mail,          SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, act_pass,              SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, act_mail,              SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, new_act_pass,          SLE_UINT32,                 9, SL_MAX_VERSION),
	SLE_CONDVAR(Town, new_act_mail,          SLE_UINT32,                 9, SL_MAX_VERSION),

	    SLE_VAR(Town, pct_pass_transported,  SLE_UINT8),
	    SLE_VAR(Town, pct_mail_transported,  SLE_UINT8),

	    SLE_VAR(Town, act_food,              SLE_UINT16),
	    SLE_VAR(Town, act_water,             SLE_UINT16),
	    SLE_VAR(Town, new_act_food,          SLE_UINT16),
	    SLE_VAR(Town, new_act_water,         SLE_UINT16),

	SLE_CONDVAR(Town, time_until_rebuild,    SLE_UINT8,                  0, 53),
	SLE_CONDVAR(Town, grow_counter,          SLE_UINT8,                  0, 53),
	SLE_CONDVAR(Town, growth_rate,           SLE_UINT8,                  0, 53),

	SLE_CONDVAR(Town, time_until_rebuild,    SLE_UINT16,                54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, grow_counter,          SLE_UINT16,                54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, growth_rate,           SLE_INT16,                 54, SL_MAX_VERSION),

	    SLE_VAR(Town, fund_buildings_months, SLE_UINT8),
	    SLE_VAR(Town, road_build_months,     SLE_UINT8),

	SLE_CONDVAR(Town, exclusivity,           SLE_UINT8,                  2, SL_MAX_VERSION),
	SLE_CONDVAR(Town, exclusive_counter,     SLE_UINT8,                  2, SL_MAX_VERSION),

	SLE_CONDVAR(Town, larger_town,           SLE_BOOL,                  56, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 30 bytes) */
	SLE_CONDNULL(30, 2, SL_MAX_VERSION),

	SLE_END()
};

/* Save and load the mapping between the house id on the map, and the grf file
 * it came from. */
static const SaveLoad _house_id_mapping_desc[] = {
	SLE_VAR(EntityIDMapping, grfid,         SLE_UINT32),
	SLE_VAR(EntityIDMapping, entity_id,     SLE_UINT8),
	SLE_VAR(EntityIDMapping, substitute_id, SLE_UINT8),
	SLE_END()
};

static void Save_HOUSEIDS()
{
	uint j = _house_mngr.GetMaxMapping();

	for (uint i = 0; i < j; i++) {
		SlSetArrayIndex(i);
		SlObject(&_house_mngr.mapping_ID[i], _house_id_mapping_desc);
	}
}

static void Load_HOUSEIDS()
{
	int index;

	_house_mngr.ResetMapping();
	uint max_id = _house_mngr.GetMaxMapping();

	while ((index = SlIterateArray()) != -1) {
		if ((uint)index >= max_id) break;
		SlObject(&_house_mngr.mapping_ID[index], _house_id_mapping_desc);
	}
}

static void Save_TOWN()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		SlSetArrayIndex(t->index);
		SlObject(t, _town_desc);
	}
}

static void Load_TOWN()
{
	int index;

	_total_towns = 0;

	while ((index = SlIterateArray()) != -1) {
		Town *t = new (index) Town();
		SlObject(t, _town_desc);

		_total_towns++;
	}

	/* This is to ensure all pointers are within the limits of
	 *  the size of the TownPool */
	if (_cur_town_ctr > GetMaxTownIndex())
		_cur_town_ctr = 0;
}

void AfterLoadTown()
{
	_town_sort_dirty = true;

	Town *t;
	FOR_ALL_TOWNS(t) t->InitializeLayout();
}

extern const ChunkHandler _town_chunk_handlers[] = {
	{ 'HIDS', Save_HOUSEIDS, Load_HOUSEIDS, CH_ARRAY },
	{ 'CITY', Save_TOWN,     Load_TOWN,     CH_ARRAY | CH_LAST},
};

void ResetHouses()
{
	memset(&_house_specs, 0, sizeof(_house_specs));
	memcpy(&_house_specs, &_original_house_specs, sizeof(_original_house_specs));

	/* Reset any overrides that have been set. */
	_house_mngr.ResetOverride();
}
