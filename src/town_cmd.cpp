/* $Id$ */

/** @file town_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "debug.h"
#include "strings.h"
#include "road_map.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "map.h"
#include "landscape.h"
#include "tile.h"
#include "town_map.h"
#include "tunnel_map.h"
#include "viewport.h"
#include "town.h"
#include "command.h"
#include "gfx.h"
#include "industry.h"
#include "station.h"
#include "vehicle.h"
#include "player.h"
#include "news.h"
#include "saveload.h"
#include "economy.h"
#include "gui.h"
#include "unmovable_map.h"
#include "water_map.h"
#include "variables.h"
#include "bridge.h"
#include "bridge_map.h"
#include "date.h"
#include "table/town_land.h"
#include "genworld.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_house.h"
#include "newgrf_commons.h"

/**
 * Called if a new block is added to the town-pool
 */
static void TownPoolNewBlock(uint start_item)
{
	Town *t;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (t = GetTown(start_item); t != NULL; t = (t->index + 1U < GetTownPoolSize()) ? GetTown(t->index + 1U) : NULL) t->index = start_item++;
}

/* Initialize the town-pool */
DEFINE_OLD_POOL(Town, Town, TownPoolNewBlock, NULL)

/**
 * Removes a specific town as well as all industries
 * under its "juridiction"
 * @param t Town to remove
 */
void DestroyTown(Town *t)
{
	Industry *i;
	TileIndex tile;

	/* Delete town authority window
	 * and remove from list of sorted towns */
	DeleteWindowById(WC_TOWN_VIEW, t->index);
	_town_sort_dirty = true;
	_total_towns--;

	/* Delete all industries belonging to the town */
	FOR_ALL_INDUSTRIES(i) if (i->town == t) DeleteIndustry(i);

	/* Go through all tiles and delete those belonging to the town */
	for (tile = 0; tile < MapSize(); ++tile) {
		switch (GetTileType(tile)) {
			case MP_HOUSE:
				if (GetTownByTile(tile) == t) DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				break;

			case MP_STREET:
			case MP_TUNNELBRIDGE:
				if (IsTileOwner(tile, OWNER_TOWN) &&
						ClosestTownFromTile(tile, (uint)-1) == t)
					DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				break;

			default:
				break;
		}
	}

	DeleteName(t->townnametype);
	DeleteSubsidyWithTown(t->index);

	MarkWholeScreenDirty();
}

// Local
static int _grow_town_result;

static bool BuildTownHouse(Town *t, TileIndex tile);
static void DoBuildTownHouse(Town *t, TileIndex tile);

static void TownDrawHouseLift(const TileInfo *ti)
{
	AddChildSpriteScreen(SPR_LIFT, PAL_NONE, 14, 60 - GetLiftPosition(ti->tile));
}

typedef void TownDrawTileProc(const TileInfo *ti);
static TownDrawTileProc * const _town_draw_tile_procs[1] = {
	TownDrawHouseLift
};

uint OriginalTileRandomiser(uint x, uint y)
{
	uint variant;
	variant  = x >> 4;
	variant ^= x >> 6;
	variant ^= y >> 4;
	variant -= y >> 6;
	variant &= 3;
	return variant;
}

/**
 * House Tile drawing handler.
 * Part of the tile loop process
 * @param ti TileInfo of the tile to draw
 */
static void DrawTile_Town(TileInfo *ti)
{
	const DrawBuildingsTileStruct *dcts;
	SpriteID image;
	SpriteID pal;
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
	dcts = &_town_draw_tile_data[house_id << 4 | OriginalTileRandomiser(ti->x, ti->y) << 2 | GetHouseBuildingStage(ti->tile)];

	if (ti->tileh != SLOPE_FLAT) DrawFoundation(ti, ti->tileh);

	image = dcts->ground.sprite;
	pal   = dcts->ground.pal;
	DrawGroundSprite(image, pal);

	/* Add a house on top of the ground? */
	image = dcts->building.sprite;
	if (image != 0) {
		if (HASBIT(_transparent_opt, TO_HOUSES)) {
			SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
			pal = PALETTE_TO_TRANSPARENT;
		} else {
			pal = dcts->building.pal;
		}

		AddSortableSpriteToDraw(image, pal,
			ti->x + dcts->subtile_x,
			ti->y + dcts->subtile_y,
			dcts->width + 1,
			dcts->height + 1,
			dcts->dz,
			ti->z
		);

		if (HASBIT(_transparent_opt, TO_HOUSES)) return;
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

static Slope GetSlopeTileh_Town(TileIndex tile, Slope tileh)
{
	return SLOPE_FLAT;
}

/**
 * Animate a tile for a town
 * Only certain houses can be animated
 * The newhouses animation superseeds regular ones
 * @param tile TileIndex of the house to animate
 */
static void AnimateTile_Town(TileIndex tile)
{
	int pos, dest;

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
		int i;

		/*  Building has 6 floors, number 0 .. 6, where 1 is illegal.
		 *  This is due to the fact that the first floor is, in the graphics,
		 *  the height of 2 'normal' floors.
		 *  Furthermore, there are 6 lift positions from floor N (incl) to floor N + 1 (excl) */
		do {
			i = (Random() & 7) - 1;
		} while (i < 0 || i == 1 || i * 6 == GetLiftPosition(tile));

		SetLiftDestination(tile, i);
	}

	pos = GetLiftPosition(tile);
	dest = GetLiftDestination(tile) * 6;
	pos += (pos < dest) ? 1 : -1;
	SetLiftPosition(tile, pos);

	if (pos == dest) HaltLift(tile);

	MarkTileDirtyByTile(tile);
}

static void UpdateTownRadius(Town *t);

/**
 * Determines if a town is close to a tile
 * @param tile TileIndex of the tile to query
 * @param dist maximum distance to be accepted
 * @returns true if the tile correspond to the distance criteria
 */
static bool IsCloseToTown(TileIndex tile, uint dist)
{
	const Town* t;

	FOR_ALL_TOWNS(t) {
		if (DistanceManhattan(tile, t->xy) < dist) return true;
	}
	return false;
}

/**
 * Marks the town sign as needing a repaint
 * @param t Town requesting repaint
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
	Point pt;

	MarkTownSignDirty(t);
	pt = RemapCoords2(TileX(t->xy) * TILE_SIZE, TileY(t->xy) * TILE_SIZE);
	SetDParam(0, t->index);
	SetDParam(1, t->population);
	UpdateViewportSignPos(&t->sign, pt.x, pt.y - 24,
		_patches.population_in_label ? STR_TOWN_LABEL_POP : STR_TOWN_LABEL);
	MarkTownSignDirty(t);
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
	uint32 pop;
	const Town* t;

	pop = 0;
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
	if (HASBIT(GetHouseSpecs(GetHouseType(tile))->callback_mask, CBM_CONSTRUCTION_STATE_CHANGE)) {
		uint16 callback_res = GetHouseCallback(CBID_CONSTRUCTION_STATE_CHANGE, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
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
	Town *t;
	uint32 r;
	HouseID house_id = GetHouseType(tile);
	HouseSpec *hs = GetHouseSpecs(house_id);

	/* NewHouseTileLoop returns false if Callback 21 succeeded, i.e. the house
	 * doesn't exist any more, so don't continue here. */
	if (house_id >= NEW_HOUSE_OFFSET && !NewHouseTileLoop(tile)) return;

	if (!IsHouseCompleted(tile)) {
		/* Construction is not completed. See if we can go further in construction*/
		MakeTownHouseBigger(tile);
		return;
	}

	/* If the lift has a destination, it is already an animated tile. */
	if ((hs->building_flags & BUILDING_IS_ANIMATED) &&
			house_id < NEW_HOUSE_OFFSET &&
			!LiftHasDestination(tile) &&
			CHANCE16(1, 2))
		AddAnimatedTile(tile);

	t = GetTownByTile(tile);

	r = Random();

	if (HASBIT(hs->callback_mask, CBM_HOUSE_PRODUCE_CARGO)) {
		for (uint i = 0; i < 256; i++) {
			uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, r, house_id, t, tile);

			if (callback == CALLBACK_FAILED) break;
			if (callback == 0x20FF) break;

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
			uint moved;

			if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
			t->new_max_pass += amt;
			moved = MoveGoodsToStation(tile, 1, 1, CT_PASSENGERS, amt);
			t->new_act_pass += moved;
		}

		if (GB(r, 8, 8) < hs->mail_generation) {
			uint amt = GB(r, 8, 8) / 8 + 1;
			uint moved;

			if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
			t->new_max_mail += amt;
			moved = MoveGoodsToStation(tile, 1, 1, CT_MAIL, amt);
			t->new_act_mail += moved;
		}
	}

	_current_player = OWNER_TOWN;

	if (hs->building_flags & BUILDING_HAS_1_TILE &&
			HASBIT(t->flags12, TOWN_IS_FUNDED) &&
			CanDeleteHouse(tile) &&
			max(_cur_year - GetHouseConstructionYear(tile), 0) >= hs->minimum_life &&
			--t->time_until_rebuild == 0) {
		t->time_until_rebuild = GB(r, 16, 8) + 192;

		ClearTownHouse(t, tile);

		/* Rebuild with another house? */
		if (GB(r, 24, 8) >= 12) DoBuildTownHouse(t, tile);
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

static int32 ClearTile_Town(TileIndex tile, byte flags)
{
	int rating;
	int32 cost;
	Town *t;
	HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));

	if (flags&DC_AUTO && !(flags&DC_AI_BUILDING)) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
	if (!CanDeleteHouse(tile)) return CMD_ERROR;

	cost = _price.remove_house * hs->removal_cost >> 8;

	rating = hs->remove_rating_decrease;
	_cleared_town_rating += rating;
	_cleared_town = t = GetTownByTile(tile);

	if (IsValidPlayer(_current_player)) {
		if (rating > t->ratings[_current_player] && !(flags & DC_NO_TOWN_RATING) && !_cheats.magic_bulldozer.value) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	if (flags & DC_EXEC) {
		ChangeTownRating(t, -rating, RATING_HOUSE_MINIMUM);
		ClearTownHouse(t, tile);
	}

	return cost;
}

static void GetAcceptedCargo_Town(TileIndex tile, AcceptedCargo ac)
{
	HouseSpec *hs = GetHouseSpecs(GetHouseType(tile));
	CargoID accepts[3];

	/* Set the initial accepted cargo types */
	for (uint8 i = 0; i < lengthof(accepts); i++) {
		accepts[i] = hs->accepts_cargo[i];
	}

	/* Check for custom accepted cargo types */
	if (HASBIT(hs->callback_mask, CBM_HOUSE_ACCEPT_CARGO)) {
		uint16 callback = GetHouseCallback(CBID_HOUSE_ACCEPT_CARGO, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback != CALLBACK_FAILED) {
			/* Replace accepted cargo types with translated values from callback */
			accepts[0] = GetCargoTranslation(GB(callback,  0, 5), hs->grffile);
			accepts[1] = GetCargoTranslation(GB(callback,  5, 5), hs->grffile);
			accepts[2] = GetCargoTranslation(GB(callback, 10, 5), hs->grffile);
		}
	}

	/* Check for custom cargo acceptance */
	if (HASBIT(hs->callback_mask, CBM_CARGO_ACCEPTANCE)) {
		uint16 callback = GetHouseCallback(CBID_HOUSE_CARGO_ACCEPTANCE, 0, 0, GetHouseType(tile), GetTownByTile(tile), tile);
		if (callback != CALLBACK_FAILED) {
			if (accepts[0] != CT_INVALID) ac[accepts[0]] = GB(callback, 0, 4);
			if (accepts[1] != CT_INVALID) ac[accepts[1]] = GB(callback, 4, 4);
			if (_opt.landscape != LT_TEMPERATE && HASBIT(callback, 12)) {
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

static uint32 GetTileTrackStatus_Town(TileIndex tile, TransportType mode, uint sub_mode)
{
	/* not used */
	return 0;
}

static void ChangeTileOwner_Town(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	/* not used */
}


static const TileIndexDiffC _roadblock_tileadd[] = {
	{ 0, -1},
	{ 1,  0},
	{ 0,  1},
	{-1,  0},

	/* Store the first 3 elements again.
	 * Lets us rotate without using &3. */
	{ 0, -1},
	{ 1,  0},
	{ 0,  1}
};

/**
 * Distance multiplyer
 * Defines the possible distances between 2 road tiles
 */
enum RoadBlockTitleDistance {
	RB_TILE_DIST1 = 1, ///< 1 tile between
	RB_TILE_DIST2,     ///< 2 tiles between
};

static bool GrowTown(Town *t);

static void TownTickHandler(Town *t)
{
	if (HASBIT(t->flags12, TOWN_IS_FUNDED)) {
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

static RoadBits GetTownRoadMask(TileIndex tile)
{
	TrackBits b = GetAnyRoadTrackBits(tile, ROADTYPE_ROAD);
	RoadBits r = ROAD_NONE;

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
 *  given distance is a road tile else
 */
static bool NeighborIsRoadTile(TileIndex tile, int dir, RoadBlockTitleDistance dist_multi)
{
	return (HASBIT(GetTownRoadMask(TILE_ADD(tile, dist_multi * ToTileIndexDiff(_roadblock_tileadd[dir + 1]))), dir ^ 2) ||
			HASBIT(GetTownRoadMask(TILE_ADD(tile, dist_multi * ToTileIndexDiff(_roadblock_tileadd[dir + 3]))), dir ^ 2) ||
			HASBIT(GetTownRoadMask(TILE_ADD(tile, dist_multi * (ToTileIndexDiff(_roadblock_tileadd[dir + 1]) + ToTileIndexDiff(_roadblock_tileadd[dir + 2])))), dir) ||
			HASBIT(GetTownRoadMask(TILE_ADD(tile, dist_multi * (ToTileIndexDiff(_roadblock_tileadd[dir + 3]) + ToTileIndexDiff(_roadblock_tileadd[dir + 2])))), dir));
}

static bool IsRoadAllowedHere(TileIndex tile, int dir)
{
	Slope k;
	Slope slope;

	/* If this assertion fails, it might be because the world contains
	 *  land at the edges. This is not ok. */
	TILE_ASSERT(tile);

	for (;;) {
		/* Check if there already is a road at this point? */
		if (GetAnyRoadTrackBits(tile, ROADTYPE_ROAD) == 0) {
			/* No, try to build one in the direction.
			 * if that fails clear the land, and if that fails exit.
			 * This is to make sure that we can build a road here later. */
			if (CmdFailed(DoCommand(tile, (dir & 1 ? ROAD_X : ROAD_Y), 0, DC_AUTO, CMD_BUILD_ROAD)) &&
					CmdFailed(DoCommand(tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR)))
				return false;
		}

		slope = GetTileSlope(tile, NULL);
		if (slope == SLOPE_FLAT) {
no_slope:
			/* Tile has no slope */
			switch (_patches.town_layout) {
				default: NOT_REACHED();

				case TL_ORIGINAL: /* Disallow the road if any neighboring tile has a road (distance: 1) */
					return !NeighborIsRoadTile(tile, dir, RB_TILE_DIST1);

				case TL_BETTER_ROADS: /* Disallow the road if any neighboring tile has a road (distance: 1 and 2). */
					return !(NeighborIsRoadTile(tile, dir, RB_TILE_DIST1) ||
							NeighborIsRoadTile(tile, dir, RB_TILE_DIST2));
			}
		}

		/* If the tile is not a slope in the right direction, then
		 * maybe terraform some. */
		k = (dir & 1) ? SLOPE_NE : SLOPE_NW;
		if (k != slope && ComplementSlope(k) != slope) {
			uint32 r = Random();

			if (CHANCE16I(1, 8, r) && !_generating_world) {
				int32 res;

				if (CHANCE16I(1, 16, r)) {
					res = DoCommand(tile, slope, 0, DC_EXEC | DC_AUTO | DC_NO_WATER,
					                      CMD_TERRAFORM_LAND);
				} else {
					res = DoCommand(tile, slope ^ 0xF, 1, DC_EXEC | DC_AUTO | DC_NO_WATER,
					                      CMD_TERRAFORM_LAND);
				}
				if (CmdFailed(res) && CHANCE16I(1, 3, r)) {
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
	int32 r;

	TILE_ASSERT(tile);

	r = DoCommand(tile, edges, dir, DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
	if (CmdFailed(r) || r >= 126 * 16) return false;
	DoCommand(tile, edges, dir, DC_AUTO | DC_NO_WATER | DC_EXEC, CMD_TERRAFORM_LAND);
	return true;
}

static void LevelTownLand(TileIndex tile)
{
	Slope tileh;

	TILE_ASSERT(tile);

	/* Don't terraform if land is plain or if there's a house there. */
	if (IsTileType(tile, MP_HOUSE)) return;
	tileh = GetTileSlope(tile, NULL);
	if (tileh == SLOPE_FLAT) return;

	/* First try up, then down */
	if (!TerraformTownTile(tile, ~tileh & 0xF, 1)) {
		TerraformTownTile(tile, tileh & 0xF, 0);
	}
}

/**
 * Generate the RoadBits of a grid tile
 *
 * @param t current town
 * @param tile tile in reference to the town
 * @return the RoadBit of the current tile regarding
 *  the selected town layout
 */
static RoadBits GetTownRoadGridElement(Town* t, TileIndex tile)
{
	/* align the grid to the downtown */
	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile); ///< Vector from downtown to the tile

	/* lx, ly description:
	 * @li lx and ly are true  if the tile is a crossing tile.
	 * @li lx xor ly are true  if the tile is a straight road tile.
	 * @li lx and ly are false if the tile is a house tile.
	 */
	bool lx, ly;

	switch (_patches.town_layout) {
		default: NOT_REACHED();

		case TL_2X2_GRID:
			lx = ((grid_pos.x % 3) == 0);
			ly = ((grid_pos.y % 3) == 0);
			break;

		case TL_3X3_GRID:
			lx = ((grid_pos.x % 4) == 0);
			ly = ((grid_pos.y % 4) == 0);
			break;
	}

	/* generate the basic grid structure */
	if (!lx && !ly) {         ///< It is a house tile
		return ROAD_NONE;
	} else if (lx && !ly) {   ///< It is a Y-dir road tile
		return ROAD_Y;
	} else if (!lx && ly) {   ///< It is a X-dir road tile
		return ROAD_X;
	} else {                  ///< It is a crossing tile
		/* Presets for junctions on slopes
		 * not nice :( */
		switch (GetTileSlope(tile, NULL)) {
			case SLOPE_W:
				return ROAD_NW | ROAD_SW;
			case SLOPE_S:
				return ROAD_SE | ROAD_SW;
			case SLOPE_SW:
				return ROAD_Y | ROAD_SW;
			case SLOPE_E:
				return ROAD_NE | ROAD_SE;
			case SLOPE_SE:
				return ROAD_X | ROAD_SE;
			case SLOPE_N:
				return ROAD_NW | ROAD_NE;
			case SLOPE_NW:
				return ROAD_X | ROAD_NW;
			case SLOPE_NE:
				return ROAD_Y | ROAD_NE;
			case SLOPE_STEEP_W:
			case SLOPE_STEEP_N:
				return ROAD_X;
			case SLOPE_STEEP_S:
			case SLOPE_STEEP_E:
				return ROAD_Y;
			default:
				return ROAD_ALL;
		}
	}
}

/**
 * Check there are enougth neighbor house tiles next to the current tile
 *
 * @param tile current tile
 * @return true if there are more than 2 house tiles next
 *  to the current one
 */
static bool NeighborsAreHouseTiles(TileIndex tile)
{
	uint counter = 0; ///< counts the house neighbor tiles

	/* We can't look further than that. */
	if (TileX(tile) < 1 || TileY(tile) < 1) {
		return false;
	}

	/* Check the tiles E,N,W and S of the current tile. */
	for (uint i = 0; i < 4; i++) {
		if (IsTileType(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[i])), MP_HOUSE)) {
			counter++;
		}

		/* If there are enougth neighbor's stop it here */
		if (counter >= 3) {
			return true;
		}
	}
	return false;
}

/**
 * Grows the given town.
 * There are at the moment 3 possible way's for
 * the town expansion:
 * @li Generate a random tile and check if there is a road allowed
 * 	@li TL_ORIGINAL
 * 	@li TL_BETTER_ROADS
 * @li Check if the town geometry allows a road and which one
 * 	@li TL_2X2_GRID
 * 	@li TL_3X3_GRID
 * @li Forbid roads, only build houses
 * 	@li TL_NO_ROADS
 *
 * @param tile_ptr current tile
 * @param mask current tiles RoadBits
 * @param block road block
 * @param t1 current town
 */
static void GrowTownInTile(TileIndex* tile_ptr, RoadBits mask, int block, Town* t1)
{
	RoadBits rcmd;
	TileIndex tmptile;
	DiagDirection i;
	int j;
	TileIndex tile = *tile_ptr;

	TILE_ASSERT(tile);

	if (mask == 0) {
		int a;
		int b;

		/* Tile has no road. First reset the status counter
		 * to say that this is the last iteration. */
		_grow_town_result = 0;

		/* Remove hills etc */
		LevelTownLand(tile);

		/* Is a road allowed here? */
		switch (_patches.town_layout) {
			default: NOT_REACHED();

			case TL_NO_ROADS: /* Disallow Roads */
				return;

			case TL_3X3_GRID:
			case TL_2X2_GRID:
				rcmd = GetTownRoadGridElement(t1, tile);
				if (rcmd == ROAD_NONE) {
					return;
				}
				break;

			case TL_BETTER_ROADS:
			case TL_ORIGINAL:
				if (!IsRoadAllowedHere(tile, block)) {
					return;
				}

				/* Randomize new road block numbers */
				a = block;
				b = block ^ 2;
				if (CHANCE16(1, 4)) {
					do {
						a = GB(Random(), 0, 2);
					} while (a == b);
				}

				if (!IsRoadAllowedHere(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[a])), a)) {
					/* A road is not allowed to continue the randomized road,
					 *   return if the road we're trying to build is curved. */
					if (a != (b ^ 2)) {
						return;
					}

					/* Return if neither side of the new road is a house */
					if (!IsTileType(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[a + 1])), MP_HOUSE) &&
							!IsTileType(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[a + 3])), MP_HOUSE)) {
						return;
					}

					/* That means that the road is only allowed if there is a house
					 *  at any side of the new road. */
				}

				rcmd = (RoadBits)((1 << a) + (1 << b));
				break;
		}

	} else if (block < 5 && !HASBIT(mask, block ^ 2)) {
		/* Continue building on a partial road.
		 * Always OK. */
		_grow_town_result = 0;

		switch (_patches.town_layout) {
			default: NOT_REACHED();

			case TL_NO_ROADS: /* Disallow Roads */
				return;

			case TL_3X3_GRID:
			case TL_2X2_GRID:
			 	rcmd = GetTownRoadGridElement(t1, tile);
				break;

			case TL_BETTER_ROADS:
			case TL_ORIGINAL:
				rcmd = (RoadBits)(1 << (block ^ 2));
				break;
		}
	} else {
		int i;
		bool allow_house = false;
		TileIndex tmptile2;

		/* Reached a tunnel/bridge? Then continue at the other side of it. */
		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			if (IsTunnel(tile) && GetTunnelTransportType(tile) == TRANSPORT_ROAD) {
				*tile_ptr = GetOtherTunnelEnd(tile);
			} else if (IsBridge(tile) && GetBridgeTransportType(tile) == TRANSPORT_ROAD) {
				*tile_ptr = GetOtherBridgeEnd(tile);
			}
			return;
		}

		/* Possibly extend the road in a direction.
		 * Randomize a direction and if it has a road, bail out. */
		i = GB(Random(), 0, 2);
		if (HASBIT(mask, i)) return;

		/* This is the tile we will reach if we extend to this direction. */
		tmptile = TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[i]));

		/* Don't do it if it reaches to water. */
		if (IsClearWaterTile(tmptile)) return;

		switch (_patches.town_layout) {
			default: NOT_REACHED();

			case TL_NO_ROADS:
				allow_house = true;
				break;

			case TL_3X3_GRID: /* Use 2x2 grid afterwards! */
				/* Fill gap if house has enougth neighbors */
				tmptile2 = TILE_ADD(tmptile, ToTileIndexDiff(_roadblock_tileadd[i]));
				if (NeighborsAreHouseTiles(tmptile2) && BuildTownHouse(t1, tmptile2)) {
					_grow_town_result = -1;
				}

			case TL_2X2_GRID:
				rcmd = GetTownRoadGridElement(t1, tmptile);
				allow_house = (rcmd == ROAD_NONE);
				break;

			case TL_BETTER_ROADS: /* Use original afterwards! */
				/* Fill gap if house has enougth neighbors */
				tmptile2 = TILE_ADD(tmptile, ToTileIndexDiff(_roadblock_tileadd[i]));
				if (NeighborsAreHouseTiles(tmptile2) && BuildTownHouse(t1, tmptile2)) {
					_grow_town_result = -1;
				}

			case TL_ORIGINAL:
				 /* Allow a house at the edge. 60% chance or
				  * always ok if no road allowed. */
				allow_house = (!IsRoadAllowedHere(tmptile, i) || CHANCE16(6, 10));
				break;
		}


		if (allow_house) {
			/* Build a house, but not if there already is a house there. */
			if (!IsTileType(tmptile, MP_HOUSE)) {
				/* Level the land if possible */
				LevelTownLand(tmptile);

				/* And build a house.
				 * Set result to -1 if we managed to build it. */
				if (BuildTownHouse(t1, tmptile)) {
					_grow_town_result = -1;
				}
			}
			return;
		}

		_grow_town_result = 0;
		rcmd = (RoadBits)(1 << i);
	}

	/* Return if a water tile */
	if (IsClearWaterTile(tile)) return;

	/* Determine direction of slope,
	 *  and build a road if not a special slope. */
	switch (GetTileSlope(tile, NULL)) {
		case SLOPE_SW: i = DIAGDIR_NE; break;
		case SLOPE_SE: i = DIAGDIR_NW; break;
		case SLOPE_NW: i = DIAGDIR_SE; break;
		case SLOPE_NE: i = DIAGDIR_SW; break;

		default:
build_road_and_exit:
			if (!CmdFailed(DoCommand(tile, rcmd, t1->index, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD))) {
				_grow_town_result = -1;
			}
			return;
	}

	/* Check if the bridge is in the right direction */
	if ((rcmd == ROAD_X && (i == DIAGDIR_NW || i == DIAGDIR_SE)) ||
			(rcmd == ROAD_Y && (i == DIAGDIR_NE || i == DIAGDIR_SW))) {
		goto build_road_and_exit;
	}

	tmptile = tile;

	/* Now it contains the direction of the slope */
	j = -11; // max 11 tile long bridges
	do {
		if (++j == 0)
			goto build_road_and_exit;
		tmptile = TILE_MASK(tmptile + TileOffsByDiagDir(i));
	} while (IsClearWaterTile(tmptile));

	/* no water tiles in between? */
	if (j == -10)
		goto build_road_and_exit;

	/* Quit if it selecting an appropiate bridge type fails a large number of times. */
	j = 22;
	{
		int32 bridge_len = GetBridgeLength(tile, tmptile);
		do {
			byte bridge_type = RandomRange(MAX_BRIDGES - 1);
			if (CheckBridge_Stuff(bridge_type, bridge_len)) {
				if (!CmdFailed(DoCommand(tile, tmptile, 0x8000 + bridge_type, DC_EXEC | DC_AUTO, CMD_BUILD_BRIDGE)))
					_grow_town_result = -1;

				/* obviously, if building any bridge would fail, there is no need to try other bridge-types */
				return;
			}
		} while (--j != 0);
	}
}

/** Returns "growth" if a house was built, or no if the build failed.
 * @param t town to inquiry
 * @param tile to inquiry
 * @return something other than zero(0)if town expansion was possible
 */
static int GrowTownAtRoad(Town *t, TileIndex tile)
{
	int block = 5; // special case

	TILE_ASSERT(tile);

	/* Number of times to search.
	 * Better roads, 2X2 and 3X3 grid grow quite fast so we give
	 * them a little handicap. */
	switch (_patches.town_layout) {
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
		/* Get a bitmask of the road blocks on a tile */
		RoadBits mask = GetTownRoadMask(tile);

		/* Try to grow the town from this point */
		GrowTownInTile(&tile, mask, block, t);

		/* Exclude the source position from the bitmask
		 * and return if no more road blocks available */
		ClrBitT(mask, (block ^ 2));
		if (mask == 0)
			return _grow_town_result;

		/* Select a random bit from the blockmask, walk a step
		 * and continue the search from there. */
		do block = Random() & 3; while (!HASBIT(mask, block));
		tile += ToTileIndexDiff(_roadblock_tileadd[block]);

		if (IsTileType(tile, MP_STREET)) {
			/* Don't allow building over roads of other cities */
			if (IsTileOwner(tile, OWNER_TOWN) && GetTownByTile(tile) != t) {
				_grow_town_result = -1;
			} else if (_game_mode == GM_EDITOR) {
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

/** Generate a random road block
 * The probability of a straight road
 * is somewhat higher than a curved. */
static RoadBits GenRandomRoadBits()
{
	uint32 r = Random();
	uint a = GB(r, 0, 2);
	uint b = GB(r, 8, 2);
	if (a == b) b ^= 2;
	return (RoadBits)((1 << a) + (1 << b));
}

/** Grow the town
 * @Return true if a house was built, or no if the build failed. */
static bool GrowTown(Town *t)
{
	TileIndex tile;
	const TileIndexDiffC *ptr;
	PlayerID old_player;

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

	/* Let the town be a ghost town
	 * The player wanted it in such a way. Thus there he has it. ;)
	 * Never reached in editor mode. */
	if (_patches.town_layout == TL_NO_ROADS && _generating_world) {
		return false;
	}

	/* Current player is a town */
	old_player = _current_player;
	_current_player = OWNER_TOWN;

	/* Find a road that we can base the construction on. */
	tile = t->xy;
	for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
		if (GetAnyRoadTrackBits(tile, ROADTYPE_ROAD) != 0) {
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
			if (!CmdFailed(DoCommand(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR))) {
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

static void UpdateTownRadius(Town *t)
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
	uint16 townnametype = SPECSTR_TOWNNAME_START + _opt.town_name;

	assert(townnameparts);

	for (;;) {
restart:
		r = Random();

		SetDParam(0, r);
		GetString(buf1, townnametype, lastof(buf1));

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
	int x, i;

	/* clear the town struct */
	i = t->index;
	memset(t, 0, sizeof(Town));
	t->index = i;
	_total_towns++;

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

	for (i = 0; i != MAX_PLAYERS; i++)
		t->ratings[i] = 500;

	t->have_ratings = 0;
	t->exclusivity = INVALID_PLAYER;
	t->exclusive_counter = 0;
	t->statues = 0;

	t->townnametype = SPECSTR_TOWNNAME_START + _opt.town_name;
	t->townnameparts = townnameparts;

	UpdateTownVirtCoord(t);
	_town_sort_dirty = true;

	/* Random town size. */
	x = (Random() & 0xF) + 8;

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

	i = x * 4;
	do {
		GrowTown(t);
	} while (--i);

	t->num_houses -= x;
	UpdateTownRadius(t);
	UpdateTownMaxPass(t);
}

static Town *AllocateTown()
{
	Town *t;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (t = GetTown(0); t != NULL; t = (t->index + 1U < GetTownPoolSize()) ? GetTown(t->index + 1U) : NULL) {
		if (!IsValidTown(t)) {
			TownID index = t->index;

			memset(t, 0, sizeof(Town));
			t->index = index;

			return t;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_Town_pool))
		return AllocateTown();

	return NULL;
}

/** Create a new town.
 * This obviously only works in the scenario editor. Function not removed
 * as it might be possible in the future to fund your own town :)
 * @param tile coordinates where town is built
 * @param flags type of operation
 * @param p1 size of the town (0 = small, 1 = medium, 2 = large)
 * @param p2 size mode (@see TownSizeMode)
 */
int32 CmdBuildTown(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Town *t;
	uint32 townnameparts;

	/* Only in the scenario editor */
	if (_game_mode != GM_EDITOR) return CMD_ERROR;
	if (p2 > TSM_CITY) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

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

	/* Get a unique name for the town. */
	if (!CreateTownName(&townnameparts))
		return_cmd_error(STR_023A_TOO_MANY_TOWNS);

	/* Allocate town struct */
	t = AllocateTown();
	if (t == NULL) return_cmd_error(STR_023A_TOO_MANY_TOWNS);

	/* Create the town */
	if (flags & DC_EXEC) {
		_generating_world = true;
		DoCreateTown(t, tile, townnameparts, (TownSizeMode)p2, p1);
		_generating_world = false;
	}
	return 0;
}

Town *CreateRandomTown(uint attempts, TownSizeMode mode, uint size)
{
	TileIndex tile;
	Town *t;
	uint32 townnameparts;

	do {
		/* Generate a tile index not too close from the edge */
		tile = RandomTile();
		if (DistanceFromEdge(tile) < 20) continue;

		/* Make sure the tile is plain */
		if (!IsTileType(tile, MP_CLEAR) || GetTileSlope(tile, NULL) != SLOPE_FLAT) continue;

		/* Check not too close to a town */
		if (IsCloseToTown(tile, 20)) continue;

		/* Get a unique name for the town. */
		if (!CreateTownName(&townnameparts)) break;

		/* Allocate a town struct */
		t = AllocateTown();
		if (t == NULL) break;

		DoCreateTown(t, tile, townnameparts, mode, size);
		return t;
	} while (--attempts);
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

static bool CheckBuildHouseMode(TileIndex tile, Slope tileh, int mode)
{
	int b;
	Slope slope;

	static const byte _masks[8] = {
		0xC,0x3,0x9,0x6,
		0x3,0xC,0x6,0x9,
	};

	slope = GetTileSlope(tile, NULL);
	if (IsSteepSlope(slope)) return false;

	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return false;

	b = 0;
	if ((slope != SLOPE_FLAT && ~slope & _masks[mode])) b = ~b;
	if ((tileh != SLOPE_FLAT && ~tileh & _masks[mode+4])) b = ~b;
	if (b)
		return false;

	return !CmdFailed(DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR));
}


uint GetTownRadiusGroup(const Town* t, TileIndex tile)
{
	uint dist = DistanceSquare(tile, t->xy);
	uint smallest;
	uint i;

	if (t->fund_buildings_months && dist <= 25) return 4;

	smallest = 0;
	for (i = 0; i != lengthof(t->radius); i++) {
		if (dist < t->radius[i]) smallest = i;
	}

	return smallest;
}

static bool CheckFree2x2Area(TileIndex tile)
{
	int i;

	static const TileIndexDiffC _tile_add[] = {
		{0    , 0    },
		{0 - 0, 1 - 0},
		{1 - 0, 0 - 1},
		{1 - 1, 1 - 0}
	};

	for (i = 0; i != 4; i++) {
		tile += ToTileIndexDiff(_tile_add[i]);

		if (GetTileSlope(tile, NULL) != SLOPE_FLAT) return false;

		if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return false;

		if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER | DC_FORCETEST, CMD_LANDSCAPE_CLEAR)))
			return false;
	}

	return true;
}

static void DoBuildTownHouse(Town *t, TileIndex tile)
{
	int i;
	uint bitmask;
	HouseID house;
	Slope slope;
	uint z;
	uint oneof = 0;
	HouseSpec *hs;

	/* Above snow? */
	slope = GetTileSlope(tile, &z);

	/* Get the town zone type */
	{
		uint rad = GetTownRadiusGroup(t, tile);

		int land = _opt.landscape;
		if (land == LT_ARCTIC && z >= _opt.snow_line) land = -1;

		bitmask = (1 << rad) + (1 << (land + 12));
	}

	/* bits 0-4 are used
	 * bits 11-15 are used
	 * bits 5-10 are not used. */
	{
		HouseID houses[HOUSE_MAX];
		int num = 0;
		uint cumulative_probs[HOUSE_MAX];
		uint probability_max = 0;

		/* Generate a list of all possible houses that can be built. */
		for (i = 0; i < HOUSE_MAX; i++) {
			hs = GetHouseSpecs(i);
			if ((~hs->building_availability & bitmask) == 0 && hs->enabled) {
				if (_have_newhouses) {
					probability_max += hs->probability;
					cumulative_probs[num] = probability_max;
				}
				houses[num++] = (HouseID)i;
			}
		}

		for (;;) {
			if (_have_newhouses) {
				uint r = RandomRange(probability_max);
				for (i = 0; i < num; i++) if (cumulative_probs[i] >= r) break;

				house = houses[i];
			} else {
				house = houses[RandomRange(num)];
			}

			hs = GetHouseSpecs(house);

			if (_have_newhouses) {
				if (hs->override != 0) hs = GetHouseSpecs(hs->override);

				if ((hs->extra_flags & BUILDING_IS_HISTORICAL) && !_generating_world) continue;

				if (HASBIT(hs->callback_mask, CBM_HOUSE_ALLOW_CONSTRUCTION)) {
					uint16 callback_res = GetHouseCallback(CBID_HOUSE_ALLOW_CONSTRUCTION, 0, 0, house, t, tile);
					if (callback_res != CALLBACK_FAILED && callback_res == 0) continue;
				}
			}

			if (_cur_year < hs->min_date || _cur_year > hs->max_date) continue;

			/* Special houses that there can be only one of. */
			if (hs->building_flags & BUILDING_IS_CHURCH) {
				SETBIT(oneof, TOWN_HAS_CHURCH);
			} else if (hs->building_flags & BUILDING_IS_STADIUM) {
				SETBIT(oneof, TOWN_HAS_STADIUM);
			} else {
				oneof = 0;
			}

			if (HASBITS(t->flags12 , oneof)) continue;

			/* Make sure there is no slope? */
			if (hs->building_flags & TILE_NOT_SLOPED && slope != SLOPE_FLAT) continue;

			if (hs->building_flags & TILE_SIZE_2x2) {
				if (CheckFree2x2Area(tile) ||
						CheckFree2x2Area(tile += TileDiffXY(-1,  0)) ||
						CheckFree2x2Area(tile += TileDiffXY( 0, -1)) ||
						CheckFree2x2Area(tile += TileDiffXY( 1,  0))) {
					break;
				}
				tile += TileDiffXY(0, 1);
			} else if (hs->building_flags & TILE_SIZE_2x1) {
				if (CheckBuildHouseMode(tile + TileDiffXY(1, 0), slope, 0)) break;

				if (CheckBuildHouseMode(tile + TileDiffXY(-1, 0), slope, 1)) {
					tile += TileDiffXY(-1, 0);
					break;
				}
			} else if (hs->building_flags & TILE_SIZE_1x2) {
				if (CheckBuildHouseMode(tile + TileDiffXY(0, 1), slope, 2)) break;

				if (CheckBuildHouseMode(tile + TileDiffXY(0, -1), slope, 3)) {
					tile += TileDiffXY(0, -1);
					break;
				}
			} else {
				break;
			}
		}
	}

	t->num_houses++;
	IncreaseBuildingCount(t, house);

	/* Special houses that there can be only one of. */
	t->flags12 |= oneof;

	{
		byte construction_counter = 0, construction_stage = 0;

		if (_generating_world) {
			uint32 r = Random();

			construction_stage = TOWN_HOUSE_COMPLETED;
			if (CHANCE16(1, 7)) construction_stage = GB(r, 0, 2);

			if (construction_stage == TOWN_HOUSE_COMPLETED) {
				ChangePopulation(t, hs->population);
			} else {
				construction_counter = GB(r, 2, 2);
			}
		}
		MakeTownHouse(tile, t->index, construction_counter, construction_stage, house, VehicleRandomBits());
	}
}

static bool BuildTownHouse(Town *t, TileIndex tile)
{
	int32 r;

	if (IsSteepSlope(GetTileSlope(tile, NULL))) return false;
	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return false;

	r = DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR);
	if (CmdFailed(r)) return false;

	DoBuildTownHouse(t, tile);
	return true;
}


static void DoClearTownHouseHelper(TileIndex tile)
{
	assert(IsTileType(tile, MP_HOUSE));
	DoClearSquare(tile);
	DeleteAnimatedTile(tile);
}

void ClearTownHouse(Town *t, TileIndex tile)
{
	HouseID house = GetHouseType(tile);
	uint eflags;
	HouseSpec *hs;

	assert(IsTileType(tile, MP_HOUSE));

	/* need to align the tile to point to the upper left corner of the house */
	if (house >= 3) { // house id 0,1,2 MUST be single tile houses, or this code breaks.
		if (GetHouseSpecs(house-1)->building_flags & TILE_SIZE_2x1) {
			house--;
			tile += TileDiffXY(-1, 0);
		} else if (GetHouseSpecs(house-1)->building_flags & BUILDING_2_TILES_Y) {
			house--;
			tile += TileDiffXY(0, -1);
		} else if (GetHouseSpecs(house-2)->building_flags & BUILDING_HAS_4_TILES) {
			house-=2;
			tile += TileDiffXY(-1, 0);
		} else if (GetHouseSpecs(house-3)->building_flags & BUILDING_HAS_4_TILES) {
			house-=3;
			tile += TileDiffXY(-1, -1);
		}
	}

	hs = GetHouseSpecs(house);

	/* Remove population from the town if the house is finished. */
	if (IsHouseCompleted(tile)) {
		ChangePopulation(t, -hs->population);
	}

	t->num_houses--;
	DecreaseBuildingCount(t, house);

	/* Clear flags for houses that only may exist once/town. */
	if (hs->building_flags & BUILDING_IS_CHURCH) {
		CLRBIT(t->flags12, TOWN_HAS_CHURCH);
	} else if (hs->building_flags & BUILDING_IS_STADIUM) {
		CLRBIT(t->flags12, TOWN_HAS_STADIUM);
	}

	/* Do the actual clearing of tiles */
	eflags = hs->building_flags;
	DoClearTownHouseHelper(tile);
	if (eflags & BUILDING_2_TILES_X)   DoClearTownHouseHelper(tile + TileDiffXY(1, 0));
	if (eflags & BUILDING_2_TILES_Y)   DoClearTownHouseHelper(tile + TileDiffXY(0, 1));
	if (eflags & BUILDING_HAS_4_TILES) DoClearTownHouseHelper(tile + TileDiffXY(1, 1));
}

/** Rename a town (server-only).
 * @param tile unused
 * @param flags type of operation
 * @param p1 town ID to rename
 * @param p2 unused
 */
int32 CmdRenameTown(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Town *t;

	if (!IsValidTownID(p1) || _cmd_text[0] == '\0') return CMD_ERROR;

	t = GetTown(p1);

	str = AllocateNameUnique(_cmd_text, 4);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DeleteName(t->townnametype);
		t->townnametype = str;

		UpdateTownVirtCoord(t);
		_town_sort_dirty = true;
		UpdateAllStationVirtCoord();
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}
	return 0;
}

/** Called from GUI */
void ExpandTown(Town *t)
{
	int amount, n;

	_generating_world = true;

	/* The more houses, the faster we grow */
	amount = RandomRange(t->num_houses / 10) + 3;
	t->num_houses += amount;
	UpdateTownRadius(t);

	n = amount * 10;
	do GrowTown(t); while (--n);

	t->num_houses -= amount;
	UpdateTownRadius(t);

	UpdateTownMaxPass(t);
	_generating_world = false;
}

extern const byte _town_action_costs[8] = {
	2, 4, 9, 35, 48, 53, 117, 175
};

static void TownActionAdvertiseSmall(Town* t)
{
	ModifyStationRatingAround(t->xy, _current_player, 0x40, 10);
}

static void TownActionAdvertiseMedium(Town* t)
{
	ModifyStationRatingAround(t->xy, _current_player, 0x70, 15);
}

static void TownActionAdvertiseLarge(Town* t)
{
	ModifyStationRatingAround(t->xy, _current_player, 0xA0, 20);
}

static void TownActionRoadRebuild(Town* t)
{
	const Player* p;

	t->road_build_months = 6;

	SetDParam(0, t->index);

	p = GetPlayer(_current_player);
	SetDParam(1, p->name_1);
	SetDParam(2, p->name_2);

	AddNewsItem(STR_2055_TRAFFIC_CHAOS_IN_ROAD_REBUILDING,
		NEWS_FLAGS(NM_NORMAL, NF_TILE, NT_GENERAL, 0), t->xy, 0);
}

static bool DoBuildStatueOfCompany(TileIndex tile, TownID town_id)
{
	PlayerID old;
	int32 r;

	if (GetTileSlope(tile, NULL) != SLOPE_FLAT) return false;

	if (!IsTileType(tile, MP_HOUSE) &&
			!IsTileType(tile, MP_CLEAR) &&
			!IsTileType(tile, MP_TREES)) {
		return false;
	}

	old = _current_player;
	_current_player = OWNER_NONE;
	r = DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
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
static void TownActionBuildStatue(Town* t)
{
	TileIndex tile = t->xy;

	if (CircularTileSearch(tile, 9, SearchTileForStatue, t->index))
		SETBIT(t->statues, _current_player); // Once found and built, "inform" the Town
}

static void TownActionFundBuildings(Town* t)
{
	/* Build next tick */
	t->grow_counter = 1;
	/* If we were not already growing */
	SETBIT(t->flags12, TOWN_IS_FUNDED);
	/* And grow for 3 months */
	t->fund_buildings_months = 3;
}

static void TownActionBuyRights(Town* t)
{
	t->exclusive_counter = 12;
	t->exclusivity = _current_player;

	ModifyStationRatingAround(t->xy, _current_player, 130, 17);
}

static void TownActionBribe(Town* t)
{
	if (!RandomRange(15)) {
		Station *st;

		/* set as unwanted for 6 months */
		t->unwanted[_current_player] = 6;

		/* set all close by station ratings to 0 */
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

typedef void TownActionProc(Town* t);
static TownActionProc * const _town_action_proc[] = {
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
int32 CmdDoTownAction(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	Town *t;

	if (!IsValidTownID(p1) || p2 > lengthof(_town_action_proc)) return CMD_ERROR;

	t = GetTown(p1);

	if (!HASBIT(GetMaskOfTownActions(NULL, _current_player, t), p2)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	cost = (_price.build_industry >> 8) * _town_action_costs[p2];

	if (flags & DC_EXEC) {
		_town_action_proc[p2](t);
		InvalidateWindow(WC_TOWN_AUTHORITY, p1);
	}

	return cost;
}

static void UpdateTownGrowRate(Town *t)
{
	int n;
	Station *st;
	uint16 m;
	Player *p;

	/* Reset player ratings if they're low */
	FOR_ALL_PLAYERS(p) {
		if (p->is_active && t->ratings[p->index] <= 200) {
			t->ratings[p->index] += 5;
		}
	}

	n = 0;
	FOR_ALL_STATIONS(st) {
		if (DistanceSquare(st->xy, t->xy) <= t->radius[0]) {
			if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
				n++;
				if (IsValidPlayer(st->owner) && t->ratings[st->owner] <= 1000-12)
					t->ratings[st->owner] += 12;
			} else {
				if (IsValidPlayer(st->owner) && t->ratings[st->owner] >= -1000+15)
					t->ratings[st->owner] -= 15;
			}
		}
	}

	CLRBIT(t->flags12, TOWN_IS_FUNDED);
	if (_patches.town_growth_rate == 0 && t->fund_buildings_months == 0) return;

	/** Towns are processed every TOWN_GROWTH_FREQUENCY ticks, and this is the
	 * number of times towns are processed before a new building is built. */
	static const uint16 _grow_count_values[2][6] = {
		{ 120, 120, 120, 100,  80,  60 }, ///< Fund new buildings has been activated
		{ 320, 420, 300, 220, 160, 100 }  ///< Normal values
	};

	if (t->fund_buildings_months != 0) {
		m = _grow_count_values[0][min(n, 5)];
		t->fund_buildings_months--;
	} else {
		m = _grow_count_values[1][min(n, 5)];
		if (n == 0 && !CHANCE16(1, 12)) return;
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

	SETBIT(t->flags12, TOWN_IS_FUNDED);
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
	const Player* p;

	FOR_ALL_PLAYERS(p) {
		if (t->unwanted[p->index] > 0) t->unwanted[p->index]--;
	}
}

bool CheckIfAuthorityAllows(TileIndex tile)
{
	Town *t;

	if (!IsValidPlayer(_current_player)) return true;

	t = ClosestTownFromTile(tile, _patches.dist_local_authority);
	if (t == NULL) return true;

	if (t->ratings[_current_player] > -200) return true;

	_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
	SetDParam(0, t->index);

	return false;
}


Town* CalcClosestTownFromTile(TileIndex tile, uint threshold)
{
	Town *t;
	uint dist, best = threshold;
	Town *best_town = NULL;

	FOR_ALL_TOWNS(t) {
		dist = DistanceManhattan(tile, t->xy);
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
				IsTileType(tile, MP_STREET) &&
				GetRoadOwner(tile, ROADTYPE_ROAD) == OWNER_TOWN
			)) {
		return GetTownByTile(tile);
	} else {
		return CalcClosestTownFromTile(tile, threshold);
	}
}


void ChangeTownRating(Town *t, int add, int max)
{
	int rating;

	/* if magic_bulldozer cheat is active, town doesn't penaltize for removing stuff */
	if (t == NULL ||
			!IsValidPlayer(_current_player) ||
			(_cheats.magic_bulldozer.value && add < 0)) {
		return;
	}

	SETBIT(t->have_ratings, _current_player);

	rating = t->ratings[_current_player];

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
	t->ratings[_current_player] = rating;
}

/* penalty for removing town-owned stuff */
static const int _default_rating_settings [3][3] = {
	/* ROAD_REMOVE, TUNNELBRIDGE_REMOVE, INDUSTRY_REMOVE */
	{  0, 128, 384}, ///< Permissive
	{ 48, 192, 480}, ///< Neutral
	{ 96, 384, 768}, ///< Hostile
};

bool CheckforTownRating(uint32 flags, Town *t, byte type)
{
	int modemod;

	/* if magic_bulldozer cheat is active, town doesn't restrict your destructive actions */
	if (t == NULL || !IsValidPlayer(_current_player) || _cheats.magic_bulldozer.value)
		return true;

	/* check if you're allowed to remove the street/bridge/tunnel/industry
	 * owned by a town no removal if rating is lower than ... depends now on
	 * difficulty setting. Minimum town rating selected by difficulty level
	 */
	modemod = _default_rating_settings[_opt.diff.town_council_tolerance][type];

	if (t->ratings[_current_player] < 16 + modemod && !(flags & DC_NO_TOWN_RATING)) {
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
	Subsidy *s;

	/* Clean the town pool and create 1 block in it */
	CleanPool(&_Town_pool);
	AddBlockToPool(&_Town_pool);

	memset(_subsidies, 0, sizeof(_subsidies));
	for (s=_subsidies; s != endof(_subsidies); s++)
		s->cargo_type = CT_INVALID;

	_cur_town_ctr = 0;
	_cur_town_iter = 0;
	_total_towns = 0;
	_town_sort_dirty = true;
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
	NULL,                    /* get_produced_cargo_proc */
	NULL,                    /* vehicle_enter_tile_proc */
	GetSlopeTileh_Town,      /* get_slope_tileh_proc */
};


/** Save and load of towns. */
static const SaveLoad _town_desc[] = {
	SLE_CONDVAR(Town, xy,                    SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Town, xy,                    SLE_UINT32,                 6, SL_MAX_VERSION),

	SLE_CONDVAR(Town, population,            SLE_FILE_U16 | SLE_VAR_U32, 0, 2),
	SLE_CONDVAR(Town, population,            SLE_UINT32,                 3, SL_MAX_VERSION),


	    SLE_VAR(Town, num_houses,            SLE_UINT16),
	    SLE_VAR(Town, townnametype,          SLE_UINT16),
	    SLE_VAR(Town, townnameparts,         SLE_UINT32),

	    SLE_VAR(Town, flags12,               SLE_UINT8),
	    SLE_VAR(Town, statues,               SLE_UINT8),

	/* sort_index_obsolete was stored here in savegame format 0 - 1 */
	SLE_CONDNULL(1, 0, 1),

	    SLE_VAR(Town, have_ratings,          SLE_UINT8),
	    SLE_ARR(Town, ratings,               SLE_INT16, 8),
	/* failed bribe attempts are stored since savegame format 4 */
	SLE_CONDARR(Town, unwanted,              SLE_INT8, 8, 4,SL_MAX_VERSION),

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

	SLE_CONDVAR(Town, time_until_rebuild,    SLE_UINT16,                 54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, grow_counter,          SLE_UINT16,                 54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, growth_rate,           SLE_INT16,                  54, SL_MAX_VERSION),

	    SLE_VAR(Town, fund_buildings_months, SLE_UINT8),
	    SLE_VAR(Town, road_build_months,     SLE_UINT8),

	    SLE_VAR(Town, exclusivity,           SLE_UINT8),
	    SLE_VAR(Town, exclusive_counter,     SLE_UINT8),

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
		Town *t;

		if (!AddBlockIfNeeded(&_Town_pool, index))
			error("Towns: failed loading savegame: too many towns");

		t = GetTown(index);
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
	Town *t;
	FOR_ALL_TOWNS(t) {
		UpdateTownRadius(t);
		UpdateTownVirtCoord(t);
	}
	_town_sort_dirty = true;
}

extern const ChunkHandler _town_chunk_handlers[] = {
	{ 'HIDS', Save_HOUSEIDS, Load_HOUSEIDS, CH_ARRAY },
	{ 'CITY', Save_TOWN,     Load_TOWN,     CH_ARRAY | CH_LAST},
};

void ResetHouses()
{
	memset(&_house_specs, 0, sizeof(_house_specs));
	memcpy(&_house_specs, &_original_house_specs, sizeof(_original_house_specs));
}
