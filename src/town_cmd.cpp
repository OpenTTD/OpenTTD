/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_cmd.cpp Handling of town tiles. */

#include "stdafx.h"
#include "road_internal.h" /* Cleaning up road bits */
#include "road_cmd.h"
#include "landscape.h"
#include "viewport_func.h"
#include "cmd_helper.h"
#include "command_func.h"
#include "industry.h"
#include "station_base.h"
#include "company_base.h"
#include "news_func.h"
#include "gui.h"
#include "object.h"
#include "genworld.h"
#include "newgrf_debug.h"
#include "newgrf_house.h"
#include "newgrf_text.h"
#include "newgrf_config.h"
#include "autoslope.h"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "window_func.h"
#include "string_func.h"
#include "newgrf_cargo.h"
#include "cheat_type.h"
#include "animated_tile_func.h"
#include "date_func.h"
#include "subsidy_func.h"
#include "core/pool_func.hpp"
#include "town.h"
#include "townname_func.h"
#include "townname_type.h"
#include "core/random_func.hpp"
#include "core/backup_type.hpp"
#include "depot_base.h"
#include "object_map.h"
#include "object_base.h"
#include "ai/ai.hpp"

#include "table/strings.h"
#include "table/town_land.h"

TownID _new_town_id;

/* Initialize the town-pool */
TownPool _town_pool("Town");
INSTANTIATE_POOL_METHODS(Town)

Town::~Town()
{
	free(this->name);

	if (CleaningPool()) return;

	/* Delete town authority window
	 * and remove from list of sorted towns */
	DeleteWindowById(WC_TOWN_VIEW, this->index);

	/* Check no industry is related to us. */
	const Industry *i;
	FOR_ALL_INDUSTRIES(i) assert(i->town != this);

	/* ... and no object is related to us. */
	const Object *o;
	FOR_ALL_OBJECTS(o) assert(o->town != this);

	/* Check no tile is related to us. */
	for (TileIndex tile = 0; tile < MapSize(); ++tile) {
		switch (GetTileType(tile)) {
			case MP_HOUSE:
				assert(GetTownIndex(tile) != this->index);
				break;

			case MP_ROAD:
				assert(!HasTownOwnedRoad(tile) || GetTownIndex(tile) != this->index);
				break;

			case MP_TUNNELBRIDGE:
				assert(!IsTileOwner(tile, OWNER_TOWN) || ClosestTownFromTile(tile, UINT_MAX) != this);
				break;

			default:
				break;
		}
	}

	DeleteSubsidyWith(ST_TOWN, this->index);
	DeleteNewGRFInspectWindow(GSF_FAKE_TOWNS, this->index);
	CargoPacket::InvalidateAllFrom(ST_TOWN, this->index);
	MarkWholeScreenDirty();
}


/**
 * Invalidating of the "nearest town cache" has to be done
 * after removing item from the pool.
 * @param index index of deleted item
 */
void Town::PostDestructor(size_t index)
{
	InvalidateWindowData(WC_TOWN_DIRECTORY, 0, 0);
	UpdateNearestTownForRoadTiles(false);

	/* Give objects a new home! */
	Object *o;
	FOR_ALL_OBJECTS(o) {
		if (o->town == NULL) o->town = CalcClosestTownFromTile(o->location.tile, UINT_MAX);
	}
}

/**
 * Assigns town layout. If Random, generates one based on TileHash.
 */
void Town::InitializeLayout(TownLayout layout)
{
	if (layout != TL_RANDOM) {
		this->layout = layout;
		return;
	}

	this->layout = TileHash(TileX(this->xy), TileY(this->xy)) % (NUM_TLS - 1);
}

/**
 * Return a random valid town.
 * @return random town, NULL if there are no towns
 */
/* static */ Town *Town::GetRandom()
{
	if (Town::GetNumItems() == 0) return NULL;
	int num = RandomRange((uint16)Town::GetNumItems());
	size_t index = MAX_UVALUE(size_t);

	while (num >= 0) {
		num--;
		index++;

		/* Make sure we have a valid town */
		while (!Town::IsValidID(index)) {
			index++;
			assert(index < Town::GetPoolSize());
		}
	}

	return Town::Get(index);
}

/**
 * Get the cost for removing this house
 * @return the cost (inflation corrected etc)
 */
Money HouseSpec::GetRemovalCost() const
{
	return (_price[PR_CLEAR_HOUSE] * this->removal_cost) >> 8;
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
static Town *CreateRandomTown(uint attempts, uint32 townnameparts, TownSize size, bool city, TownLayout layout);

static void TownDrawHouseLift(const TileInfo *ti)
{
	AddChildSpriteScreen(SPR_LIFT, PAL_NONE, 14, 60 - GetLiftPosition(ti->tile));
}

typedef void TownDrawTileProc(const TileInfo *ti);
static TownDrawTileProc * const _town_draw_tile_procs[1] = {
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
		if (HouseSpec::Get(house_id)->grf_prop.spritegroup[0] != NULL) {
			DrawNewHouseTile(ti, house_id);
			return;
		} else {
			house_id = HouseSpec::Get(house_id)->grf_prop.subst_id;
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

/** Tile callback routine */
static Foundation GetFoundation_Town(TileIndex tile, Slope tileh)
{
	HouseID hid = GetHouseType(tile);

	/* For NewGRF house tiles we might not be drawing a foundation. We need to
	 * account for this, as other structures should
	 * draw the wall of the foundation in this case.
	 */
	if (hid >= NEW_HOUSE_OFFSET) {
		const HouseSpec *hs = HouseSpec::Get(hid);
		if (hs->grf_prop.spritegroup[0] != NULL && HasBit(hs->callback_mask, CBM_HOUSE_DRAW_FOUNDATIONS)) {
			uint32 callback_res = GetHouseCallback(CBID_HOUSE_DRAW_FOUNDATIONS, 0, 0, hid, Town::GetByTile(tile), tile);
			if (callback_res == 0) return FOUNDATION_NONE;
		}
	}
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
	if (!(HouseSpec::Get(GetHouseType(tile))->building_flags & BUILDING_IS_ANIMATED)) {
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

	if (pos == dest) {
		HaltLift(tile);
		DeleteAnimatedTile(tile);
	}

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
 * Resize the sign(label) of the town after changes in
 * population (creation or growth or else)
 */
void Town::UpdateVirtCoord()
{
	Point pt = RemapCoords2(TileX(this->xy) * TILE_SIZE, TileY(this->xy) * TILE_SIZE);
	SetDParam(0, this->index);
	SetDParam(1, this->population);
	this->sign.UpdatePosition(pt.x, pt.y - 24,
		_settings_client.gui.population_in_label ? STR_VIEWPORT_TOWN_POP : STR_VIEWPORT_TOWN);

	SetWindowDirty(WC_TOWN_VIEW, this->index);
}

/** Update the virtual coords needed to draw the town sign for all towns. */
void UpdateAllTownVirtCoords()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		t->UpdateVirtCoord();
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
	SetWindowDirty(WC_TOWN_VIEW, t->index);
	t->UpdateVirtCoord();

	InvalidateWindowData(WC_TOWN_DIRECTORY, 0, 1);
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

	/* progress in construction stages */
	IncHouseConstructionTick(tile);
	if (GetHouseConstructionTick(tile) != 0) return;

	AnimateNewHouseConstruction(tile);

	if (IsHouseCompleted(tile)) {
		/* Now that construction is complete, we can add the population of the
		 * building to the town. */
		ChangePopulation(Town::GetByTile(tile), HouseSpec::Get(GetHouseType(tile))->population);
		ResetHouseAge(tile);
	}
	MarkTileDirtyByTile(tile);
}

/**
 * Make the house advance in its construction stages until completion
 * @param tile TileIndex of house
 */
static void MakeTownHouseBigger(TileIndex tile)
{
	uint flags = HouseSpec::Get(GetHouseType(tile))->building_flags;
	if (flags & BUILDING_HAS_1_TILE)  MakeSingleHouseBigger(TILE_ADDXY(tile, 0, 0));
	if (flags & BUILDING_2_TILES_Y)   MakeSingleHouseBigger(TILE_ADDXY(tile, 0, 1));
	if (flags & BUILDING_2_TILES_X)   MakeSingleHouseBigger(TILE_ADDXY(tile, 1, 0));
	if (flags & BUILDING_HAS_4_TILES) MakeSingleHouseBigger(TILE_ADDXY(tile, 1, 1));
}

/**
 * Tile callback function.
 *
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

	const HouseSpec *hs = HouseSpec::Get(house_id);

	/* If the lift has a destination, it is already an animated tile. */
	if ((hs->building_flags & BUILDING_IS_ANIMATED) &&
			house_id < NEW_HOUSE_OFFSET &&
			!LiftHasDestination(tile) &&
			Chance16(1, 2)) {
		AddAnimatedTile(tile);
	}

	Town *t = Town::GetByTile(tile);
	uint32 r = Random();

	StationFinder stations(TileArea(tile, 1, 1));

	if (HasBit(hs->callback_mask, CBM_HOUSE_PRODUCE_CARGO)) {
		for (uint i = 0; i < 256; i++) {
			uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, r, house_id, t, tile);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoID cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);
			if (cargo == CT_INVALID) continue;

			uint amt = GB(callback, 0, 8);
			if (amt == 0) continue;

			uint moved = MoveGoodsToStation(cargo, amt, ST_TOWN, t->index, stations.GetStations());

			const CargoSpec *cs = CargoSpec::Get(cargo);
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

			if (EconomyIsInRecession()) amt = (amt + 1) >> 1;
			t->new_max_pass += amt;
			t->new_act_pass += MoveGoodsToStation(CT_PASSENGERS, amt, ST_TOWN, t->index, stations.GetStations());
		}

		if (GB(r, 8, 8) < hs->mail_generation) {
			uint amt = GB(r, 8, 8) / 8 + 1;

			if (EconomyIsInRecession()) amt = (amt + 1) >> 1;
			t->new_max_mail += amt;
			t->new_act_mail += MoveGoodsToStation(CT_MAIL, amt, ST_TOWN, t->index, stations.GetStations());
		}
	}

	Backup<CompanyByte> cur_company(_current_company, OWNER_TOWN, FILE_LINE);

	if ((hs->building_flags & BUILDING_HAS_1_TILE) &&
			HasBit(t->flags, TOWN_IS_FUNDED) &&
			CanDeleteHouse(tile) &&
			GetHouseAge(tile) >= hs->minimum_life &&
			--t->time_until_rebuild == 0) {
		t->time_until_rebuild = GB(r, 16, 8) + 192;

		ClearTownHouse(t, tile);

		/* Rebuild with another house? */
		if (GB(r, 24, 8) >= 12) BuildTownHouse(t, tile);
	}

	cur_company.Restore();
}

static CommandCost ClearTile_Town(TileIndex tile, DoCommandFlag flags)
{
	if (flags & DC_AUTO) return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
	if (!CanDeleteHouse(tile)) return CMD_ERROR;

	const HouseSpec *hs = HouseSpec::Get(GetHouseType(tile));

	CommandCost cost(EXPENSES_CONSTRUCTION);
	cost.AddCost(hs->GetRemovalCost());

	int rating = hs->remove_rating_decrease;
	Town *t = Town::GetByTile(tile);

	if (Company::IsValidID(_current_company)) {
		if (rating > t->ratings[_current_company] && !(flags & DC_NO_TEST_TOWN_RATING) && !_cheats.magic_bulldozer.value) {
			SetDParam(0, t->index);
			return_cmd_error(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
		}
	}

	ChangeTownRating(t, -rating, RATING_HOUSE_MINIMUM, flags);
	if (flags & DC_EXEC) {
		ClearTownHouse(t, tile);
	}

	return cost;
}

static void AddProducedCargo_Town(TileIndex tile, CargoArray &produced)
{
	HouseID house_id = GetHouseType(tile);
	const HouseSpec *hs = HouseSpec::Get(house_id);
	Town *t = Town::GetByTile(tile);

	if (HasBit(hs->callback_mask, CBM_HOUSE_PRODUCE_CARGO)) {
		for (uint i = 0; i < 256; i++) {
			uint16 callback = GetHouseCallback(CBID_HOUSE_PRODUCE_CARGO, i, 0, house_id, t, tile);

			if (callback == CALLBACK_FAILED || callback == CALLBACK_HOUSEPRODCARGO_END) break;

			CargoID cargo = GetCargoTranslation(GB(callback, 8, 7), hs->grf_prop.grffile);

			if (cargo == CT_INVALID) continue;
			produced[cargo]++;
		}
	} else {
		if (hs->population > 0) {
			produced[CT_PASSENGERS]++;
		}
		if (hs->mail_generation > 0) {
			produced[CT_MAIL]++;
		}
	}
}

static inline void AddAcceptedCargoSetMask(CargoID cargo, uint amount, CargoArray &acceptance, uint32 *always_accepted)
{
	if (cargo == CT_INVALID || amount == 0) return;
	acceptance[cargo] += amount;
	SetBit(*always_accepted, cargo);
}

static void AddAcceptedCargo_Town(TileIndex tile, CargoArray &acceptance, uint32 *always_accepted)
{
	const HouseSpec *hs = HouseSpec::Get(GetHouseType(tile));
	CargoID accepts[3];

	/* Set the initial accepted cargo types */
	for (uint8 i = 0; i < lengthof(accepts); i++) {
		accepts[i] = hs->accepts_cargo[i];
	}

	/* Check for custom accepted cargo types */
	if (HasBit(hs->callback_mask, CBM_HOUSE_ACCEPT_CARGO)) {
		uint16 callback = GetHouseCallback(CBID_HOUSE_ACCEPT_CARGO, 0, 0, GetHouseType(tile), Town::GetByTile(tile), tile);
		if (callback != CALLBACK_FAILED) {
			/* Replace accepted cargo types with translated values from callback */
			accepts[0] = GetCargoTranslation(GB(callback,  0, 5), hs->grf_prop.grffile);
			accepts[1] = GetCargoTranslation(GB(callback,  5, 5), hs->grf_prop.grffile);
			accepts[2] = GetCargoTranslation(GB(callback, 10, 5), hs->grf_prop.grffile);
		}
	}

	/* Check for custom cargo acceptance */
	if (HasBit(hs->callback_mask, CBM_HOUSE_CARGO_ACCEPTANCE)) {
		uint16 callback = GetHouseCallback(CBID_HOUSE_CARGO_ACCEPTANCE, 0, 0, GetHouseType(tile), Town::GetByTile(tile), tile);
		if (callback != CALLBACK_FAILED) {
			AddAcceptedCargoSetMask(accepts[0], GB(callback, 0, 4), acceptance, always_accepted);
			AddAcceptedCargoSetMask(accepts[1], GB(callback, 4, 4), acceptance, always_accepted);
			if (_settings_game.game_creation.landscape != LT_TEMPERATE && HasBit(callback, 12)) {
				/* The 'S' bit indicates food instead of goods */
				AddAcceptedCargoSetMask(CT_FOOD, GB(callback, 8, 4), acceptance, always_accepted);
			} else {
				AddAcceptedCargoSetMask(accepts[2], GB(callback, 8, 4), acceptance, always_accepted);
			}
			return;
		}
	}

	/* No custom acceptance, so fill in with the default values */
	for (uint8 i = 0; i < lengthof(accepts); i++) {
		AddAcceptedCargoSetMask(accepts[i], hs->cargo_acceptance[i], acceptance, always_accepted);
	}
}

static void GetTileDesc_Town(TileIndex tile, TileDesc *td)
{
	const HouseID house = GetHouseType(tile);
	const HouseSpec *hs = HouseSpec::Get(house);
	bool house_completed = IsHouseCompleted(tile);

	td->str = hs->building_name;

	uint16 callback_res = GetHouseCallback(CBID_HOUSE_CUSTOM_NAME, house_completed ? 1 : 0, 0, house, Town::GetByTile(tile), tile);
	if (callback_res != CALLBACK_FAILED) {
		StringID new_name = GetGRFStringID(hs->grf_prop.grffile->grfid, 0xD000 + callback_res);
		if (new_name != STR_NULL && new_name != STR_UNDEFINED) {
			td->str = new_name;
		}
	}

	if (!house_completed) {
		SetDParamX(td->dparam, 0, td->str);
		td->str = STR_LAI_TOWN_INDUSTRY_DESCRIPTION_UNDER_CONSTRUCTION;
	}

	if (hs->grf_prop.grffile != NULL) {
		const GRFConfig *gc = GetGRFConfig(hs->grf_prop.grffile->grfid);
		td->grf = gc->GetName();
	}

	td->owner[0] = OWNER_TOWN;
}

static TrackStatus GetTileTrackStatus_Town(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	/* not used */
	return 0;
}

static void ChangeTileOwner_Town(TileIndex tile, Owner old_owner, Owner new_owner)
{
	/* not used */
}

static bool GrowTown(Town *t);

static void TownTickHandler(Town *t)
{
	if (HasBit(t->flags, TOWN_IS_FUNDED)) {
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

	Town *t;
	FOR_ALL_TOWNS(t) {
		/* Run town tick at regular intervals, but not all at once. */
		if ((_tick_counter + t->index) % TOWN_GROWTH_FREQUENCY == 0) {
			TownTickHandler(t);
		}
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
	if (IsRoadDepotTile(tile) || IsStandardRoadStopTile(tile)) return ROAD_NONE;

	return GetAnyRoadBits(tile, ROADTYPE_ROAD, true);
}

/**
 * Check for parallel road inside a given distance.
 *   Assuming a road from (tile - TileOffsByDiagDir(dir)) to tile,
 *   is there a parallel road left or right of it within distance dist_multi?
 *
 * @param tile curent tile
 * @param dir target direction
 * @param dist_multi distance multiplyer
 * @return true if there is a parallel road
 */
static bool IsNeighborRoadTile(TileIndex tile, const DiagDirection dir, uint dist_multi)
{
	if (!IsValidTile(tile)) return false;

	/* Lookup table for the used diff values */
	const TileIndexDiff tid_lt[3] = {
		TileOffsByDiagDir(ChangeDiagDir(dir, DIAGDIRDIFF_90RIGHT)),
		TileOffsByDiagDir(ChangeDiagDir(dir, DIAGDIRDIFF_90LEFT)),
		TileOffsByDiagDir(ReverseDiagDir(dir)),
	};

	dist_multi = (dist_multi + 1) * 4;
	for (uint pos = 4; pos < dist_multi; pos++) {
		/* Go (pos / 4) tiles to the left or the right */
		TileIndexDiff cur = tid_lt[(pos & 1) ? 0 : 1] * (pos / 4);

		/* Use the current tile as origin, or go one tile backwards */
		if (pos & 2) cur += tid_lt[2];

		/* Test for roadbit parallel to dir and facing towards the middle axis */
		if (IsValidTile(tile + cur) &&
				GetTownRoadBits(TILE_ADD(tile, cur)) & DiagDirToRoadBits((pos & 2) ? dir : ReverseDiagDir(dir))) return true;
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
	if (DistanceFromEdge(tile) == 0) return false;

	Slope cur_slope, desired_slope;

	for (;;) {
		/* Check if there already is a road at this point? */
		if (GetTownRoadBits(tile) == ROAD_NONE) {
			/* No, try if we are able to build a road piece there.
			 * If that fails clear the land, and if that fails exit.
			 * This is to make sure that we can build a road here later. */
			if (DoCommand(tile, ((dir == DIAGDIR_NW || dir == DIAGDIR_SE) ? ROAD_Y : ROAD_X), 0, DC_AUTO, CMD_BUILD_ROAD).Failed() &&
					DoCommand(tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR).Failed())
				return false;
		}

		cur_slope = _settings_game.construction.build_on_slopes ? GetFoundationSlope(tile, NULL) : GetTileSlope(tile, NULL);
		bool ret = !IsNeighborRoadTile(tile, dir, t->layout == TL_ORIGINAL ? 1 : 2);
		if (cur_slope == SLOPE_FLAT) return ret;

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
				if (res.Failed() && Chance16(1, 3)) {
					/* We can consider building on the slope, though. */
					return ret;
				}
			}
			return false;
		}
		return ret;
	}
}

static bool TerraformTownTile(TileIndex tile, int edges, int dir)
{
	assert(tile < MapSize());

	CommandCost r = DoCommand(tile, edges, dir, DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
	if (r.Failed() || r.GetCost() >= (_price[PR_TERRAFORM] + 2) * 8) return false;
	DoCommand(tile, edges, dir, DC_AUTO | DC_NO_WATER | DC_EXEC, CMD_TERRAFORM_LAND);
	return true;
}

static void LevelTownLand(TileIndex tile)
{
	assert(tile < MapSize());

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

	switch (t->layout) {
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
	if (DistanceFromEdge(tile) == 0) return false;

	uint counter = 0; // counts the house neighbor tiles

	/* Check the tiles E,N,W and S of the current tile for houses */
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		/* Count both void and house tiles for checking whether there
		 * are enough houses in the area. This to make it likely that
		 * houses get build up to the edge of the map. */
		switch (GetTileType(TileAddByDiagDir(tile, dir))) {
			case MP_HOUSE:
			case MP_VOID:
				counter++;
				break;

			default:
				break;
		}

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
	if (DoCommand(tile, rcmd, t->index, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD).Succeeded()) {
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
	if (slope & InclinedSlope(bridge_dir)) return false;

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
		if (DoCommand(tile, bridge_tile, bridge_type | ROADTYPES_ROAD << 8 | TRANSPORT_ROAD << 15, CommandFlagsToDCFlags(GetCommandFlags(CMD_BUILD_BRIDGE)), CMD_BUILD_BRIDGE).Succeeded()) {
			DoCommand(tile, bridge_tile, bridge_type | ROADTYPES_ROAD << 8 | TRANSPORT_ROAD << 15, DC_EXEC | CommandFlagsToDCFlags(GetCommandFlags(CMD_BUILD_BRIDGE)), CMD_BUILD_BRIDGE);
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

	assert(tile < MapSize());

	if (cur_rb == ROAD_NONE) {
		/* Tile has no road. First reset the status counter
		 * to say that this is the last iteration. */
		_grow_town_result = GROWTH_SEARCH_STOPPED;

		if (!_settings_game.economy.allow_town_roads && !_generating_world) return;
		if (!_settings_game.economy.allow_town_level_crossings && IsTileType(tile, MP_RAILWAY)) return;

		/* Remove hills etc */
		if (!_settings_game.construction.build_on_slopes || Chance16(1, 6)) LevelTownLand(tile);

		/* Is a road allowed here? */
		switch (t1->layout) {
			default: NOT_REACHED();

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

		if (!_settings_game.economy.allow_town_roads && !_generating_world) return;

		switch (t1->layout) {
			default: NOT_REACHED();

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
		bool allow_house = true; // Value which decides if we want to construct a house

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
		if (HasTileWaterGround(house_tile)) return;

		if (!IsValidTile(house_tile)) return;

		if (_settings_game.economy.allow_town_roads || _generating_world) {
			switch (t1->layout) {
				default: NOT_REACHED();

				case TL_3X3_GRID: // Use 2x2 grid afterwards!
					GrowTownWithExtraHouse(t1, TileAddByDiagDir(house_tile, target_dir));
					/* FALL THROUGH */

				case TL_2X2_GRID:
					rcmd = GetTownRoadGridElement(t1, house_tile, target_dir);
					allow_house = (rcmd == ROAD_NONE);
					break;

				case TL_BETTER_ROADS: // Use original afterwards!
					GrowTownWithExtraHouse(t1, TileAddByDiagDir(house_tile, target_dir));
					/* FALL THROUGH */

				case TL_ORIGINAL:
					/* Allow a house at the edge. 60% chance or
					 * always ok if no road allowed. */
					rcmd = DiagDirToRoadBits(target_dir);
					allow_house = (!IsRoadAllowedHere(t1, house_tile, target_dir) || Chance16(6, 10));
					break;
			}
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
	if (HasTileWaterGround(tile)) return;

	/* Make the roads look nicer */
	rcmd = CleanUpRoadBits(tile, rcmd);
	if (rcmd == ROAD_NONE) return;

	/* Only use the target direction for bridges to ensure they're connected.
	 * The target_dir is as computed previously according to town layout, so
	 * it will match it perfectly. */
	if (GrowTownWithBridge(t1, tile, target_dir)) return;

	GrowTownWithRoad(t1, tile, rcmd);
}

/**
 * Returns "growth" if a house was built, or no if the build failed.
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

	assert(tile < MapSize());

	/* Number of times to search.
	 * Better roads, 2X2 and 3X3 grid grow quite fast so we give
	 * them a little handicap. */
	switch (t->layout) {
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
		if (cur_rb == ROAD_NONE) {
			return _grow_town_result;
		}

		/* Select a random bit from the blockmask, walk a step
		 * and continue the search from there. */
		do target_dir = RandomDiagDir(); while (!(cur_rb & DiagDirToRoadBits(target_dir)));
		tile = TileAddByDiagDir(tile, target_dir);

		if (IsTileType(tile, MP_ROAD) && !IsRoadDepot(tile) && HasTileRoadType(tile, ROADTYPE_ROAD)) {
			/* Don't allow building over roads of other cities */
			if (IsRoadOwner(tile, ROADTYPE_ROAD, OWNER_TOWN) && Town::GetByTile(tile) != t) {
				_grow_town_result = GROWTH_SUCCEED;
			} else if (IsRoadOwner(tile, ROADTYPE_ROAD, OWNER_NONE) && _game_mode == GM_EDITOR) {
				/* If we are in the SE, and this road-piece has no town owner yet, it just found an
				 * owner :) (happy happy happy road now) */
				SetRoadOwner(tile, ROADTYPE_ROAD, OWNER_TOWN);
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

/**
 * Grow the town
 * @param t town to grow
 * @return true iff a house was built
 */
static bool GrowTown(Town *t)
{
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

	/* Current "company" is a town */
	Backup<CompanyByte> cur_company(_current_company, OWNER_TOWN, FILE_LINE);

	TileIndex tile = t->xy; // The tile we are working with ATM

	/* Find a road that we can base the construction on. */
	const TileIndexDiffC *ptr;
	for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
		if (GetTownRoadBits(tile) != ROAD_NONE) {
			int r = GrowTownAtRoad(t, tile);
			cur_company.Restore();
			return r != 0;
		}
		tile = TILE_ADD(tile, ToTileIndexDiff(*ptr));
	}

	/* No road available, try to build a random road block by
	 * clearing some land and then building a road there. */
	if (_settings_game.economy.allow_town_roads || _generating_world) {
		tile = t->xy;
		for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
			/* Only work with plain land that not already has a house */
			if (!IsTileType(tile, MP_HOUSE) && GetTileSlope(tile, NULL) == SLOPE_FLAT) {
				if (DoCommand(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR).Succeeded()) {
					DoCommand(tile, GenRandomRoadBits(), t->index, DC_EXEC | DC_AUTO, CMD_BUILD_ROAD);
					cur_company.Restore();
					return true;
				}
			}
			tile = TILE_ADD(tile, ToTileIndexDiff(*ptr));
		}
	}

	cur_company.Restore();
	return false;
}

void UpdateTownRadius(Town *t)
{
	static const uint32 _town_squared_town_zone_radius_data[23][5] = {
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
		memcpy(t->squared_town_zone_radius, _town_squared_town_zone_radius_data[t->num_houses / 4], sizeof(t->squared_town_zone_radius));
	} else {
		int mass = t->num_houses / 8;
		/* Actually we are proportional to sqrt() but that's right because we are covering an area.
		 * The offsets are to make sure the radii do not decrease in size when going from the table
		 * to the calculated value.*/
		t->squared_town_zone_radius[0] = mass * 15 - 40;
		t->squared_town_zone_radius[1] = mass * 9 - 15;
		t->squared_town_zone_radius[2] = 0;
		t->squared_town_zone_radius[3] = mass * 5 - 5;
		t->squared_town_zone_radius[4] = mass * 3 + 5;
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
 * @param size Parameter for size determination
 * @param city whether to build a city or town
 * @param layout the (road) layout of the town
 * @param manual was the town placed manually?
 */
static void DoCreateTown(Town *t, TileIndex tile, uint32 townnameparts, TownSize size, bool city, TownLayout layout, bool manual)
{
	t->xy = tile;
	t->num_houses = 0;
	t->time_until_rebuild = 10;
	UpdateTownRadius(t);
	t->flags = 0;
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

	for (uint i = 0; i != MAX_COMPANIES; i++) t->ratings[i] = RATING_INITIAL;

	t->have_ratings = 0;
	t->exclusivity = INVALID_COMPANY;
	t->exclusive_counter = 0;
	t->statues = 0;

	extern int _nb_orig_names;
	if (_settings_game.game_creation.town_name < _nb_orig_names) {
		/* Original town name */
		t->townnamegrfid = 0;
		t->townnametype = SPECSTR_TOWNNAME_START + _settings_game.game_creation.town_name;
	} else {
		/* Newgrf town name */
		t->townnamegrfid = GetGRFTownNameId(_settings_game.game_creation.town_name  - _nb_orig_names);
		t->townnametype  = GetGRFTownNameType(_settings_game.game_creation.town_name - _nb_orig_names);
	}
	t->townnameparts = townnameparts;

	t->UpdateVirtCoord();
	InvalidateWindowData(WC_TOWN_DIRECTORY, 0, 0);

	t->InitializeLayout(layout);

	t->larger_town = city;

	int x = (int)size * 16 + 3;
	if (size == TSZ_RANDOM) x = (Random() & 0xF) + 8;
	/* Don't create huge cities when founding town in-game */
	if (city && (!manual || _game_mode == GM_EDITOR)) x *= _settings_game.economy.initial_city_size;

	t->num_houses += x;
	UpdateTownRadius(t);

	int i = x * 4;
	do {
		GrowTown(t);
	} while (--i);

	t->num_houses -= x;
	UpdateTownRadius(t);
	UpdateTownMaxPass(t);
	UpdateAirportsNoise();
}

/**
 * Checks if it's possible to place a town at given tile
 * @param tile tile to check
 * @return error value or zero cost
 */
static CommandCost TownCanBePlacedHere(TileIndex tile)
{
	/* Check if too close to the edge of map */
	if (DistanceFromEdge(tile) < 12) {
		return_cmd_error(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP_SUB);
	}

	/* Check distance to all other towns. */
	if (IsCloseToTown(tile, 20)) {
		return_cmd_error(STR_ERROR_TOO_CLOSE_TO_ANOTHER_TOWN);
	}

	/* Can only build on clear flat areas, possibly with trees. */
	if ((!IsTileType(tile, MP_CLEAR) && !IsTileType(tile, MP_TREES)) || GetTileSlope(tile, NULL) != SLOPE_FLAT) {
		return_cmd_error(STR_ERROR_SITE_UNSUITABLE);
	}

	return CommandCost(EXPENSES_OTHER);
}

/**
 * Verifies this custom name is unique. Only custom names are checked.
 * @param name name to check
 * @return is this name unique?
 */
static bool IsUniqueTownName(const char *name)
{
	const Town *t;

	FOR_ALL_TOWNS(t) {
		if (t->name != NULL && strcmp(t->name, name) == 0) return false;
	}

	return true;
}

/**
 * Create a new town.
 * @param tile coordinates where town is built
 * @param flags type of operation
 * @param p1  0..1 size of the town (@see TownSize)
 *               2 true iff it should be a city
 *            3..5 town road layout (@see TownLayout)
 *               6 use random location (randomize \c tile )
 * @param p2 town name parts
 * @param text Custom name for the town. If empty, the town name parts will be used.
 * @return the cost of this operation or an error
 */
CommandCost CmdFoundTown(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	TownSize size = Extract<TownSize, 0, 2>(p1);
	bool city = HasBit(p1, 2);
	TownLayout layout = Extract<TownLayout, 3, 3>(p1);
	TownNameParams par(_settings_game.game_creation.town_name);
	bool random = HasBit(p1, 6);
	uint32 townnameparts = p2;

	if (size >= TSZ_END) return CMD_ERROR;
	if (layout >= NUM_TLS) return CMD_ERROR;

	/* Some things are allowed only in the scenario editor */
	if (_game_mode != GM_EDITOR) {
		if (_settings_game.economy.found_town == TF_FORBIDDEN) return CMD_ERROR;
		if (size == TSZ_LARGE) return CMD_ERROR;
		if (random) return CMD_ERROR;
		if (_settings_game.economy.found_town != TF_CUSTOM_LAYOUT && layout != _settings_game.economy.town_layout) {
			return CMD_ERROR;
		}
	}

	if (StrEmpty(text)) {
		/* If supplied name is empty, townnameparts has to generate unique automatic name */
		if (!VerifyTownName(townnameparts, &par)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	} else {
		/* If name is not empty, it has to be unique custom name */
		if (Utf8StringLength(text) >= MAX_LENGTH_TOWN_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueTownName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	/* Allocate town struct */
	if (!Town::CanAllocateItem()) return_cmd_error(STR_ERROR_TOO_MANY_TOWNS);

	if (!random) {
		CommandCost ret = TownCanBePlacedHere(tile);
		if (ret.Failed()) return ret;
	}

	static const byte price_mult[][TSZ_RANDOM + 1] = {{ 15, 25, 40, 25 }, { 20, 35, 55, 35 }};
	/* multidimensional arrays have to have defined length of non-first dimension */
	assert_compile(lengthof(price_mult[0]) == 4);

	CommandCost cost(EXPENSES_OTHER, _price[PR_BUILD_TOWN]);
	byte mult = price_mult[city][size];

	cost.MultiplyCost(mult);

	/* Create the town */
	if (flags & DC_EXEC) {
		if (cost.GetCost() > GetAvailableMoneyForCommand()) {
			_additional_cash_required = cost.GetCost();
			return CommandCost(EXPENSES_OTHER);
		}

		_generating_world = true;
		UpdateNearestTownForRoadTiles(true);
		Town *t;
		if (random) {
			t = CreateRandomTown(20, townnameparts, size, city, layout);
			if (t == NULL) {
				cost = CommandCost(STR_ERROR_NO_SPACE_FOR_TOWN);
			} else {
				_new_town_id = t->index;
			}
		} else {
			t = new Town(tile);
			DoCreateTown(t, tile, townnameparts, size, city, layout, true);
		}
		UpdateNearestTownForRoadTiles(false);
		_generating_world = false;

		if (t != NULL && !StrEmpty(text)) {
			t->name = strdup(text);
			t->UpdateVirtCoord();
		}

		if (_game_mode != GM_EDITOR) {
			/* 't' can't be NULL since 'random' is false outside scenedit */
			assert(!random);
			char company_name[MAX_LENGTH_COMPANY_NAME_CHARS * MAX_CHAR_LENGTH];
			SetDParam(0, _current_company);
			GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

			char *cn = strdup(company_name);
			SetDParamStr(0, cn);
			SetDParam(1, t->index);

			AddNewsItem(STR_NEWS_NEW_TOWN, NS_INDUSTRY_OPEN, NR_TILE, tile, NR_NONE, UINT32_MAX, cn);
			AI::BroadcastNewEvent(new AIEventTownFounded(t->index));
		}
	}
	return cost;
}

/**
 * Towns must all be placed on the same grid or when they eventually
 * interpenetrate their road networks will not mesh nicely; this
 * function adjusts a tile so that it aligns properly.
 *
 * @param tile the tile to start at
 * @param layout which town layout algo is in effect
 * @return the adjusted tile
 */
static TileIndex AlignTileToGrid(TileIndex tile, TownLayout layout)
{
	switch (layout) {
		case TL_2X2_GRID: return TileXY(TileX(tile) - TileX(tile) % 3, TileY(tile) - TileY(tile) % 3);
		case TL_3X3_GRID: return TileXY(TileX(tile) & ~3, TileY(tile) & ~3);
		default:          return tile;
	}
}

/**
 * Towns must all be placed on the same grid or when they eventually
 * interpenetrate their road networks will not mesh nicely; this
 * function tells you if a tile is properly aligned.
 *
 * @param tile the tile to start at
 * @param layout which town layout algo is in effect
 * @return true if the tile is in the correct location
 */
static bool IsTileAlignedToGrid(TileIndex tile, TownLayout layout)
{
	switch (layout) {
		case TL_2X2_GRID: return TileX(tile) % 3 == 0 && TileY(tile) % 3 == 0;
		case TL_3X3_GRID: return TileX(tile) % 4 == 0 && TileY(tile) % 4 == 0;
		default:          return true;
	}
}

/**
 * Used as the user_data for FindFurthestFromWater
 */
struct SpotData {
	TileIndex tile; ///< holds the tile that was found
	uint max_dist;  ///< holds the distance that tile is from the water
	TownLayout layout; ///< tells us what kind of town we're building
};

/**
 * CircularTileSearch callback; finds the tile furthest from any
 * water. slightly bit tricky, since it has to do a search of its own
 * in order to find the distance to the water from each square in the
 * radius.
 *
 * Also, this never returns true, because it needs to take into
 * account all locations being searched before it knows which is the
 * furthest.
 *
 * @param tile Start looking from this tile
 * @param user_data Storage area for data that must last across calls;
 * must be a pointer to struct SpotData
 *
 * @return always false
 */
static bool FindFurthestFromWater(TileIndex tile, void *user_data)
{
	SpotData *sp = (SpotData*)user_data;
	uint dist = GetClosestWaterDistance(tile, true);

	if (IsTileType(tile, MP_CLEAR) &&
			GetTileSlope(tile, NULL) == SLOPE_FLAT &&
			IsTileAlignedToGrid(tile, sp->layout) &&
			dist > sp->max_dist) {
		sp->tile = tile;
		sp->max_dist = dist;
	}

	return false;
}

/**
 * CircularTileSearch callback; finds the nearest land tile
 *
 * @param tile Start looking from this tile
 * @param user_data not used
 */
static bool FindNearestEmptyLand(TileIndex tile, void *user_data)
{
	return IsTileType(tile, MP_CLEAR);
}

/**
 * Given a spot on the map (presumed to be a water tile), find a good
 * coastal spot to build a city. We don't want to build too close to
 * the edge if we can help it (since that retards city growth) hence
 * the search within a search within a search. O(n*m^2), where n is
 * how far to search for land, and m is how far inland to look for a
 * flat spot.
 *
 * @param tile Start looking from this spot.
 * @param layout the road layout to search for
 * @return tile that was found
 */
static TileIndex FindNearestGoodCoastalTownSpot(TileIndex tile, TownLayout layout)
{
	SpotData sp = { INVALID_TILE, 0, layout };

	TileIndex coast = tile;
	if (CircularTileSearch(&coast, 40, FindNearestEmptyLand, NULL)) {
		CircularTileSearch(&coast, 10, FindFurthestFromWater, &sp);
		return sp.tile;
	}

	/* if we get here just give up */
	return INVALID_TILE;
}

static Town *CreateRandomTown(uint attempts, uint32 townnameparts, TownSize size, bool city, TownLayout layout)
{
	if (!Town::CanAllocateItem()) return NULL;

	do {
		/* Generate a tile index not too close from the edge */
		TileIndex tile = AlignTileToGrid(RandomTile(), layout);

		/* if we tried to place the town on water, slide it over onto
		 * the nearest likely-looking spot */
		if (IsTileType(tile, MP_WATER)) {
			tile = FindNearestGoodCoastalTownSpot(tile, layout);
			if (tile == INVALID_TILE) continue;
		}

		/* Make sure town can be placed here */
		if (TownCanBePlacedHere(tile).Failed()) continue;

		/* Allocate a town struct */
		Town *t = new Town(tile);

		DoCreateTown(t, tile, townnameparts, size, city, layout, false);

		/* if the population is still 0 at the point, then the
		 * placement is so bad it couldn't grow at all */
		if (t->population > 0) return t;
		DoCommand(t->xy, t->index, 0, DC_EXEC, CMD_DELETE_TOWN);
	} while (--attempts != 0);

	return NULL;
}

static const byte _num_initial_towns[4] = {5, 11, 23, 46};  // very low, low, normal, high

/**
 * This function will generate a certain amount of towns, with a certain layout
 * It can be called from the scenario editor (i.e.: generate Random Towns)
 * as well as from world creation.
 * @param layout which towns will be set to, when created
 * @return true if towns have been successfully created
 */
bool GenerateTowns(TownLayout layout)
{
	uint current_number = 0;
	uint difficulty = (_game_mode != GM_EDITOR) ? _settings_game.difficulty.number_towns : 0;
	uint total = (difficulty == (uint)CUSTOM_TOWN_NUMBER_DIFFICULTY) ? _settings_game.game_creation.custom_town_number : ScaleByMapSize(_num_initial_towns[difficulty] + (Random() & 7));
	uint32 townnameparts;

	SetGeneratingWorldProgress(GWP_TOWN, total);

	/* First attempt will be made at creating the suggested number of towns.
	 * Note that this is really a suggested value, not a required one.
	 * We would not like the system to lock up just because the user wanted 100 cities on a 64*64 map, would we? */
	do {
		bool city = (_settings_game.economy.larger_towns != 0 && Chance16(1, _settings_game.economy.larger_towns));
		IncreaseGeneratingWorldProgress(GWP_TOWN);
		/* Get a unique name for the town. */
		if (!GenerateTownName(&townnameparts)) continue;
		/* try 20 times to create a random-sized town for the first loop. */
		if (CreateRandomTown(20, townnameparts, TSZ_RANDOM, city, layout) != NULL) current_number++; // If creation was successful, raise a flag.
	} while (--total);

	if (current_number != 0) return true;

	/* If current_number is still zero at this point, it means that not a single town has been created.
	 * So give it a last try, but now more aggressive */
	if (GenerateTownName(&townnameparts) &&
			CreateRandomTown(10000, townnameparts, TSZ_RANDOM, _settings_game.economy.larger_towns != 0, layout) != NULL) {
		return true;
	}

	/* If there are no towns at all and we are generating new game, bail out */
	if (Town::GetNumItems() == 0 && _game_mode != GM_EDITOR) {
		extern StringID _switch_mode_errorstr;
		_switch_mode_errorstr = STR_ERROR_COULD_NOT_CREATE_TOWN;
	}

	return false;  // we are still without a town? we failed, simply
}


/**
 * Returns the bit corresponding to the town zone of the specified tile
 * @param t Town on which town zone is to be found
 * @param tile TileIndex where town zone needs to be found
 * @return the bit position of the given zone, as defined in HouseZones
 */
HouseZonesBits GetTownRadiusGroup(const Town *t, TileIndex tile)
{
	uint dist = DistanceSquare(tile, t->xy);

	if (t->fund_buildings_months && dist <= 25) return HZB_TOWN_CENTRE;

	HouseZonesBits smallest = HZB_TOWN_EDGE;
	for (HouseZonesBits i = HZB_BEGIN; i < HZB_END; i++) {
		if (dist < t->squared_town_zone_radius[i]) smallest = i;
	}

	return smallest;
}

/**
 * Clears tile and builds a house or house part.
 * @param tile tile index
 * @param t The town to clear the house for
 * @param counter of construction step
 * @param stage of construction (used for drawing)
 * @param type of house. Index into house specs array
 * @param random_bits required for newgrf houses
 * @pre house can be built here
 */
static inline void ClearMakeHouseTile(TileIndex tile, Town *t, byte counter, byte stage, HouseID type, byte random_bits)
{
	CommandCost cc = DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR);

	assert(cc.Succeeded());

	IncreaseBuildingCount(t, type);
	MakeHouseTile(tile, t->index, counter, stage, type, random_bits);
	if (HouseSpec::Get(type)->building_flags & BUILDING_IS_ANIMATED) AddAnimatedTile(tile);

	MarkTileDirtyByTile(tile);
}


/**
 * Write house information into the map. For houses > 1 tile, all tiles are marked.
 * @param t tile index
 * @param town The town related to this house
 * @param counter of construction step
 * @param stage of construction (used for drawing)
 * @param type of house. Index into house specs array
 * @param random_bits required for newgrf houses
 * @pre house can be built here
 */
static void MakeTownHouse(TileIndex t, Town *town, byte counter, byte stage, HouseID type, byte random_bits)
{
	BuildingFlags size = HouseSpec::Get(type)->building_flags;

	ClearMakeHouseTile(t, town, counter, stage, type, random_bits);
	if (size & BUILDING_2_TILES_Y)   ClearMakeHouseTile(t + TileDiffXY(0, 1), town, counter, stage, ++type, random_bits);
	if (size & BUILDING_2_TILES_X)   ClearMakeHouseTile(t + TileDiffXY(1, 0), town, counter, stage, ++type, random_bits);
	if (size & BUILDING_HAS_4_TILES) ClearMakeHouseTile(t + TileDiffXY(1, 1), town, counter, stage, ++type, random_bits);
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
	return DoCommand(tile, 0, 0, DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR).Succeeded();
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
	/* Allow towns everywhere when we don't build roads */
	if (!_settings_game.economy.allow_town_roads && !_generating_world) return true;

	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile);

	switch (t->layout) {
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
	/* Allow towns everywhere when we don't build roads */
	if (!_settings_game.economy.allow_town_roads && !_generating_world) return true;

	/* Compute relative position of tile. (Positive offsets are towards north) */
	TileIndexDiffC grid_pos = TileIndexToTileIndexDiffC(t->xy, tile);

	switch (t->layout) {
		case TL_2X2_GRID:
			grid_pos.x %= 3;
			grid_pos.y %= 3;
			if ((grid_pos.x != 2 && grid_pos.x != -1) ||
				(grid_pos.y != 2 && grid_pos.y != -1)) return false;
			break;

		case TL_3X3_GRID:
			if ((grid_pos.x & 3) < 2 || (grid_pos.y & 3) < 2) return false;
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
 */
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
 */
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
	int land = _settings_game.game_creation.landscape;
	if (land == LT_ARCTIC && z >= _settings_game.game_creation.snow_line) land = -1;

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
		const HouseSpec *hs = HouseSpec::Get(i);

		/* Verify that the candidate house spec matches the current tile status */
		if ((~hs->building_availability & bitmask) != 0 || !hs->enabled || hs->grf_prop.override != INVALID_HOUSE_ID) continue;

		/* Don't let these counters overflow. Global counters are 32bit, there will never be that many houses. */
		if (hs->class_id != HOUSE_NO_CLASS) {
			/* id_count is always <= class_count, so it doesn't need to be checked */
			if (t->building_counts.class_count[hs->class_id] == UINT16_MAX) continue;
		} else {
			/* If the house has no class, check id_count instead */
			if (t->building_counts.id_count[i] == UINT16_MAX) continue;
		}

		/* Without NewHouses, all houses have probability '1' */
		uint cur_prob = (_loaded_newgrf_features.has_newhouses ? hs->probability : 1);
		probability_max += cur_prob;
		probs[num] = cur_prob;
		houses[num++] = (HouseID)i;
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

		const HouseSpec *hs = HouseSpec::Get(house);

		if (_loaded_newgrf_features.has_newhouses && !_generating_world &&
				_game_mode != GM_EDITOR && (hs->extra_flags & BUILDING_IS_HISTORICAL) != 0) {
			continue;
		}

		if (_cur_year < hs->min_year || _cur_year > hs->max_year) continue;

		/* Special houses that there can be only one of. */
		uint oneof = 0;

		if (hs->building_flags & BUILDING_IS_CHURCH) {
			SetBit(oneof, TOWN_HAS_CHURCH);
		} else if (hs->building_flags & BUILDING_IS_STADIUM) {
			SetBit(oneof, TOWN_HAS_STADIUM);
		}

		if (t->flags & oneof) continue;

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

		byte random_bits = Random();

		if (HasBit(hs->callback_mask, CBM_HOUSE_ALLOW_CONSTRUCTION)) {
			uint16 callback_res = GetHouseCallback(CBID_HOUSE_ALLOW_CONSTRUCTION, 0, 0, house, t, tile, true, random_bits);
			if (callback_res != CALLBACK_FAILED && GB(callback_res, 0, 8) == 0) continue;
		}

		/* build the house */
		t->num_houses++;

		/* Special houses that there can be only one of. */
		t->flags |= oneof;

		byte construction_counter = 0;
		byte construction_stage = 0;

		if (_generating_world || _game_mode == GM_EDITOR) {
			uint32 r = Random();

			construction_stage = TOWN_HOUSE_COMPLETED;
			if (Chance16(1, 7)) construction_stage = GB(r, 0, 2);

			if (construction_stage == TOWN_HOUSE_COMPLETED) {
				ChangePopulation(t, hs->population);
			} else {
				construction_counter = GB(r, 2, 2);
			}
		}

		MakeTownHouse(tile, t, construction_counter, construction_stage, house, random_bits);

		return true;
	}

	return false;
}

/**
 * Update data structures when a house is removed
 * @param tile  Tile of the house
 * @param t     Town owning the house
 * @param house House type
 */
static void DoClearTownHouseHelper(TileIndex tile, Town *t, HouseID house)
{
	assert(IsTileType(tile, MP_HOUSE));
	DecreaseBuildingCount(t, house);
	DoClearSquare(tile);
	DeleteAnimatedTile(tile);

	DeleteNewGRFInspectWindow(GSF_HOUSES, tile);
}

/**
 * Determines if a given HouseID is part of a multitile house.
 * The given ID is set to the ID of the north tile and the TileDiff to the north tile is returned.
 *
 * @param house Is changed to the HouseID of the north tile of the same house
 * @return TileDiff from the tile of the given HouseID to the north tile
 */
TileIndexDiff GetHouseNorthPart(HouseID &house)
{
	if (house >= 3) { // house id 0,1,2 MUST be single tile houses, or this code breaks.
		if (HouseSpec::Get(house - 1)->building_flags & TILE_SIZE_2x1) {
			house--;
			return TileDiffXY(-1, 0);
		} else if (HouseSpec::Get(house - 1)->building_flags & BUILDING_2_TILES_Y) {
			house--;
			return TileDiffXY(0, -1);
		} else if (HouseSpec::Get(house - 2)->building_flags & BUILDING_HAS_4_TILES) {
			house -= 2;
			return TileDiffXY(-1, 0);
		} else if (HouseSpec::Get(house - 3)->building_flags & BUILDING_HAS_4_TILES) {
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

	const HouseSpec *hs = HouseSpec::Get(house);

	/* Remove population from the town if the house is finished. */
	if (IsHouseCompleted(tile)) {
		ChangePopulation(t, -hs->population);
	}

	t->num_houses--;

	/* Clear flags for houses that only may exist once/town. */
	if (hs->building_flags & BUILDING_IS_CHURCH) {
		ClrBit(t->flags, TOWN_HAS_CHURCH);
	} else if (hs->building_flags & BUILDING_IS_STADIUM) {
		ClrBit(t->flags, TOWN_HAS_STADIUM);
	}

	/* Do the actual clearing of tiles */
	uint eflags = hs->building_flags;
	DoClearTownHouseHelper(tile, t, house);
	if (eflags & BUILDING_2_TILES_Y)   DoClearTownHouseHelper(tile + TileDiffXY(0, 1), t, ++house);
	if (eflags & BUILDING_2_TILES_X)   DoClearTownHouseHelper(tile + TileDiffXY(1, 0), t, ++house);
	if (eflags & BUILDING_HAS_4_TILES) DoClearTownHouseHelper(tile + TileDiffXY(1, 1), t, ++house);
}

/**
 * Rename a town (server-only).
 * @param tile unused
 * @param flags type of operation
 * @param p1 town ID to rename
 * @param p2 unused
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameTown(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Town *t = Town::GetIfValid(p1);
	if (t == NULL) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_TOWN_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueTownName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(t->name);
		t->name = reset ? NULL : strdup(text);

		t->UpdateVirtCoord();
		InvalidateWindowData(WC_TOWN_DIRECTORY, 0, 1);
		UpdateAllStationVirtCoords();
	}
	return CommandCost();
}

/**
 * Expand a town (scenario editor only).
 * @param tile Unused.
 * @param flags Type of operation.
 * @param p1 Town ID to expand.
 * @param p2 Unused.
 * @param text Unused.
 * @return Empty cost or an error.
 */
CommandCost CmdExpandTown(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_game_mode != GM_EDITOR) return CMD_ERROR;
	Town *t = Town::GetIfValid(p1);
	if (t == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		/* The more houses, the faster we grow */
		uint amount = RandomRange(ClampToU16(t->num_houses / 10)) + 3;
		t->num_houses += amount;
		UpdateTownRadius(t);

		uint n = amount * 10;
		do GrowTown(t); while (--n);

		t->num_houses -= amount;
		UpdateTownRadius(t);

		UpdateTownMaxPass(t);
	}

	return CommandCost();
}

/**
 * Delete a town (scenario editor only).
 * @param tile Unused.
 * @param flags Type of operation.
 * @param p1 Town ID to delete.
 * @param p2 Unused.
 * @param text Unused.
 * @return Empty cost or an error.
 */
CommandCost CmdDeleteTown(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (_game_mode != GM_EDITOR) return CMD_ERROR;
	Town *t = Town::GetIfValid(p1);
	if (t == NULL) return CMD_ERROR;

	/* Stations refer to towns. */
	const Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->town == t) {
			/* Non-oil rig stations are always a problem. */
			if (!(st->facilities & FACIL_AIRPORT) || st->airport.type != AT_OILRIG) return CMD_ERROR;
			/* We can only automatically delete oil rigs *if* there's no vehicle on them. */
			CommandCost ret = DoCommand(st->airport.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (ret.Failed()) return ret;
		}
	}

	/* Depots refer to towns. */
	const Depot *d;
	FOR_ALL_DEPOTS(d) {
		if (d->town == t) return CMD_ERROR;
	}

	/* Check all tiles for town ownership. */
	for (TileIndex tile = 0; tile < MapSize(); ++tile) {
		bool try_clear = false;
		switch (GetTileType(tile)) {
			case MP_ROAD:
				try_clear = HasTownOwnedRoad(tile) && GetTownIndex(tile) == t->index;
				break;

			case MP_TUNNELBRIDGE:
				try_clear = IsTileOwner(tile, OWNER_TOWN) && ClosestTownFromTile(tile, UINT_MAX) == t;
				break;

			case MP_HOUSE:
				try_clear = GetTownIndex(tile) == t->index;
				break;

			case MP_INDUSTRY:
				try_clear = Industry::GetByTile(tile)->town == t;
				break;

			case MP_OBJECT:
				if (Town::GetNumItems() == 1) {
					/* No towns will be left, remove it! */
					try_clear = true;
				} else {
					Object *o = Object::GetByTile(tile);
					if (o->town == t) {
						if (GetObjectType(tile) == OBJECT_STATUE) {
							/* Statue... always remove. */
							try_clear = true;
						} else {
							/* Tell to find a new town. */
							o->town = NULL;
						}
					}
				}
				break;

			default:
				break;
		}
		if (try_clear) {
			CommandCost ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
			if (ret.Failed()) return ret;
		}
	}

	/* The town destructor will delete the other things related to the town. */
	if (flags & DC_EXEC) delete t;

	return CommandCost();
}

/**
 * Factor in the cost of each town action.
 * @see TownActions
 */
const byte _town_action_costs[TACT_COUNT] = {
	2, 4, 9, 35, 48, 53, 117, 175
};

static CommandCost TownActionAdvertiseSmall(Town *t, DoCommandFlag flags)
{
	if (flags & DC_EXEC) {
		ModifyStationRatingAround(t->xy, _current_company, 0x40, 10);
	}
	return CommandCost();
}

static CommandCost TownActionAdvertiseMedium(Town *t, DoCommandFlag flags)
{
	if (flags & DC_EXEC) {
		ModifyStationRatingAround(t->xy, _current_company, 0x70, 15);
	}
	return CommandCost();
}

static CommandCost TownActionAdvertiseLarge(Town *t, DoCommandFlag flags)
{
	if (flags & DC_EXEC) {
		ModifyStationRatingAround(t->xy, _current_company, 0xA0, 20);
	}
	return CommandCost();
}

static CommandCost TownActionRoadRebuild(Town *t, DoCommandFlag flags)
{
	/* Check if the company is allowed to fund new roads. */
	if (!_settings_game.economy.fund_roads) return CMD_ERROR;

	if (flags & DC_EXEC) {
		t->road_build_months = 6;

		char company_name[MAX_LENGTH_COMPANY_NAME_CHARS * MAX_CHAR_LENGTH];
		SetDParam(0, _current_company);
		GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

		char *cn = strdup(company_name);
		SetDParam(0, t->index);
		SetDParamStr(1, cn);

		AddNewsItem(STR_NEWS_ROAD_REBUILDING, NS_GENERAL, NR_TOWN, t->index, NR_NONE, UINT32_MAX, cn);
	}
	return CommandCost();
}

/**
 * Search callback function for TownActionBuildStatue.
 * @param tile Tile on which to perform the search.
 * @param user_data Unused.
 * @return Result of the test.
 */
static bool SearchTileForStatue(TileIndex tile, void *user_data)
{
	/* Statues can be build on slopes, just like houses. Only the steep slopes is a no go. */
	if (IsSteepSlope(GetTileSlope(tile, NULL))) return false;
	/* Don't build statues under bridges. */
	if (MayHaveBridgeAbove(tile) && IsBridgeAbove(tile)) return false;

	if (!IsTileType(tile, MP_HOUSE) &&
			!IsTileType(tile, MP_CLEAR) &&
			!IsTileType(tile, MP_TREES)) {
		return false;
	}

	Backup<CompanyByte> cur_company(_current_company, OWNER_NONE, FILE_LINE);
	CommandCost r = DoCommand(tile, 0, 0, DC_NONE, CMD_LANDSCAPE_CLEAR);
	cur_company.Restore();

	if (r.Failed()) return false;

	return true;
}

/**
 * Perform a 9x9 tiles circular search from the center of the town
 * in order to find a free tile to place a statue
 * @param t town to search in
 * @param flags Used to check if the statue must be built or not.
 * @return Empty cost or an error.
 */
static CommandCost TownActionBuildStatue(Town *t, DoCommandFlag flags)
{
	TileIndex tile = t->xy;
	if (CircularTileSearch(&tile, 9, SearchTileForStatue, NULL)) {
		if (flags & DC_EXEC) {
			Backup<CompanyByte> cur_company(_current_company, OWNER_NONE, FILE_LINE);
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			cur_company.Restore();
			BuildObject(OBJECT_STATUE, tile, _current_company, t);
			SetBit(t->statues, _current_company); // Once found and built, "inform" the Town.
			MarkTileDirtyByTile(tile);
		}
		return CommandCost();
	}
	return_cmd_error(STR_ERROR_STATUE_NO_SUITABLE_PLACE);
}

static CommandCost TownActionFundBuildings(Town *t, DoCommandFlag flags)
{
	if (flags & DC_EXEC) {
		/* Build next tick */
		t->grow_counter = 1;
		/* If we were not already growing */
		SetBit(t->flags, TOWN_IS_FUNDED);
		/* And grow for 3 months */
		t->fund_buildings_months = 3;
	}
	return CommandCost();
}

static CommandCost TownActionBuyRights(Town *t, DoCommandFlag flags)
{
	/* Check if it's allowed to buy the rights */
	if (!_settings_game.economy.exclusive_rights) return CMD_ERROR;

	if (flags & DC_EXEC) {
		t->exclusive_counter = 12;
		t->exclusivity = _current_company;

		ModifyStationRatingAround(t->xy, _current_company, 130, 17);
	}
	return CommandCost();
}

static CommandCost TownActionBribe(Town *t, DoCommandFlag flags)
{
	if (flags & DC_EXEC) {
		if (Chance16(1, 14)) {
			/* set as unwanted for 6 months */
			t->unwanted[_current_company] = 6;

			/* set all close by station ratings to 0 */
			Station *st;
			FOR_ALL_STATIONS(st) {
				if (st->town == t && st->owner == _current_company) {
					for (CargoID i = 0; i < NUM_CARGO; i++) st->goods[i].rating = 0;
				}
			}

			/* only show errormessage to the executing player. All errors are handled command.c
			 * but this is special, because it can only 'fail' on a DC_EXEC */
			if (IsLocalCompany()) ShowErrorMessage(STR_ERROR_BRIBE_FAILED, STR_ERROR_BRIBE_FAILED_2, WL_INFO);

			/* decrease by a lot!
			 * ChangeTownRating is only for stuff in demolishing. Bribe failure should
			 * be independent of any cheat settings
			 */
			if (t->ratings[_current_company] > RATING_BRIBE_DOWN_TO) {
				t->ratings[_current_company] = RATING_BRIBE_DOWN_TO;
				SetWindowDirty(WC_TOWN_AUTHORITY, t->index);
			}
		} else {
			ChangeTownRating(t, RATING_BRIBE_UP_STEP, RATING_BRIBE_MAXIMUM, DC_EXEC);
		}
	}
	return CommandCost();
}

typedef CommandCost TownActionProc(Town *t, DoCommandFlag flags);
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

/**
 * Get a list of available actions to do at a town.
 * @param nump if not NULL add put the number of available actions in it
 * @param cid the company that is querying the town
 * @param t the town that is queried
 * @return bitmasked value of enabled actions
 */
uint GetMaskOfTownActions(int *nump, CompanyID cid, const Town *t)
{
	int num = 0;
	TownActions buttons = TACT_NONE;

	/* Spectators and unwanted have no options */
	if (cid != COMPANY_SPECTATOR && !(_settings_game.economy.bribe && t->unwanted[cid])) {

		/* Things worth more than this are not shown */
		Money avail = Company::Get(cid)->money + _price[PR_STATION_VALUE] * 200;

		/* Check the action bits for validity and
		 * if they are valid add them */
		for (uint i = 0; i != lengthof(_town_action_costs); i++) {
			const TownActions cur = (TownActions)(1 << i);

			/* Is the company not able to bribe ? */
			if (cur == TACT_BRIBE && (!_settings_game.economy.bribe || t->ratings[cid] >= RATING_BRIBE_MAXIMUM)) continue;

			/* Is the company not able to buy exclusive rights ? */
			if (cur == TACT_BUY_RIGHTS && !_settings_game.economy.exclusive_rights) continue;

			/* Is the company not able to fund local road reconstruction? */
			if (cur == TACT_ROAD_REBUILD && !_settings_game.economy.fund_roads) continue;

			/* Is the company not able to build a statue ? */
			if (cur == TACT_BUILD_STATUE && HasBit(t->statues, cid)) continue;

			if (avail >= _town_action_costs[i] * _price[PR_TOWN_ACTION] >> 8) {
				buttons |= cur;
				num++;
			}
		}
	}

	if (nump != NULL) *nump = num;
	return buttons;
}

/**
 * Do a town action.
 * This performs an action such as advertising, building a statue, funding buildings,
 * but also bribing the town-council
 * @param tile unused
 * @param flags type of operation
 * @param p1 town to do the action at
 * @param p2 action to perform, @see _town_action_proc for the list of available actions
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdDoTownAction(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Town *t = Town::GetIfValid(p1);
	if (t == NULL || p2 >= lengthof(_town_action_proc)) return CMD_ERROR;

	if (!HasBit(GetMaskOfTownActions(NULL, _current_company, t), p2)) return CMD_ERROR;

	CommandCost cost(EXPENSES_OTHER, _price[PR_TOWN_ACTION] * _town_action_costs[p2] >> 8);

	CommandCost ret = _town_action_proc[p2](t, flags);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		SetWindowDirty(WC_TOWN_AUTHORITY, p1);
	}

	return cost;
}

static void UpdateTownGrowRate(Town *t)
{
	/* Increase company ratings if they're low */
	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (t->ratings[c->index] < RATING_GROWTH_MAXIMUM) {
			t->ratings[c->index] = min((int)RATING_GROWTH_MAXIMUM, t->ratings[c->index] + RATING_GROWTH_UP_STEP);
		}
	}

	int n = 0;

	const Station *st;
	FOR_ALL_STATIONS(st) {
		if (DistanceSquare(st->xy, t->xy) <= t->squared_town_zone_radius[0]) {
			if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
				n++;
				if (Company::IsValidID(st->owner)) {
					int new_rating = t->ratings[st->owner] + RATING_STATION_UP_STEP;
					t->ratings[st->owner] = min(new_rating, INT16_MAX); // do not let it overflow
				}
			} else {
				if (Company::IsValidID(st->owner)) {
					int new_rating = t->ratings[st->owner] + RATING_STATION_DOWN_STEP;
					t->ratings[st->owner] = max(new_rating, INT16_MIN);
				}
			}
		}
	}

	/* clamp all ratings to valid values */
	for (uint i = 0; i < MAX_COMPANIES; i++) {
		t->ratings[i] = Clamp(t->ratings[i], RATING_MINIMUM, RATING_MAXIMUM);
	}

	SetWindowDirty(WC_TOWN_AUTHORITY, t->index);

	ClrBit(t->flags, TOWN_IS_FUNDED);
	if (_settings_game.economy.town_growth_rate == 0 && t->fund_buildings_months == 0) return;

	/**
	 * Towns are processed every TOWN_GROWTH_FREQUENCY ticks, and this is the
	 * number of times towns are processed before a new building is built.
	 */
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

	if (_settings_game.game_creation.landscape == LT_ARCTIC) {
		if (TilePixelHeight(t->xy) >= GetSnowLine() && t->act_food == 0 && t->population > 90) return;

	} else if (_settings_game.game_creation.landscape == LT_TROPIC) {
		if (GetTropicZone(t->xy) == TROPICZONE_DESERT && (t->act_food == 0 || t->act_water == 0) && t->population > 60) return;
	}

	/* Use the normal growth rate values if new buildings have been funded in
	 * this town and the growth rate is set to none. */
	uint growth_multiplier = _settings_game.economy.town_growth_rate != 0 ? _settings_game.economy.town_growth_rate - 1 : 1;

	m >>= growth_multiplier;
	if (t->larger_town) m /= 2;

	t->growth_rate = m / (t->num_houses / 50 + 1);
	if (m <= t->grow_counter) {
		t->grow_counter = m;
	}

	SetBit(t->flags, TOWN_IS_FUNDED);
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

	SetWindowDirty(WC_TOWN_VIEW, t->index);
}

static void UpdateTownUnwanted(Town *t)
{
	const Company *c;

	FOR_ALL_COMPANIES(c) {
		if (t->unwanted[c->index] > 0) t->unwanted[c->index]--;
	}
}

/**
 * Checks whether the local authority allows construction of a new station (rail, road, airport, dock) on the given tile
 * @param tile The tile where the station shall be constructed.
 * @param flags Command flags. DC_NO_TEST_TOWN_RATING is tested.
 * @return Succeeded or failed command.
 */
CommandCost CheckIfAuthorityAllowsNewStation(TileIndex tile, DoCommandFlag flags)
{
	if (!Company::IsValidID(_current_company) || (flags & DC_NO_TEST_TOWN_RATING)) return CommandCost();

	Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);
	if (t == NULL) return CommandCost();

	if (t->ratings[_current_company] > RATING_VERYPOOR) return CommandCost();

	SetDParam(0, t->index);
	return_cmd_error(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
}

/**
 * Return the town closest to the given tile within \a threshold.
 * @param tile      Starting point of the search.
 * @param threshold Biggest allowed distance to the town.
 * @return Closest town to \a tile within \a threshold, or \c NULL if there is no such town.
 *
 * @note This function only uses distance, the #ClosestTownFromTile function also takes town ownership into account.
 */
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

/**
 * Return the town closest (in distance or ownership) to a given tile, within a given threshold.
 * @param tile      Starting point of the search.
 * @param threshold Biggest allowed distance to the town.
 * @return Closest town to \a tile within \a threshold, or \c NULL if there is no such town.
 *
 * @note If you only care about distance, you can use the #CalcClosestTownFromTile function.
 */
Town *ClosestTownFromTile(TileIndex tile, uint threshold)
{
	switch (GetTileType(tile)) {
		case MP_ROAD:
			if (IsRoadDepot(tile)) return CalcClosestTownFromTile(tile, threshold);

			if (!HasTownOwnedRoad(tile)) {
				TownID tid = GetTownIndex(tile);

				if (tid == (TownID)INVALID_TOWN) {
					/* in the case we are generating "many random towns", this value may be INVALID_TOWN */
					if (_generating_world) return CalcClosestTownFromTile(tile, threshold);
					assert(Town::GetNumItems() == 0);
					return NULL;
				}

				assert(Town::IsValidID(tid));
				Town *town = Town::Get(tid);

				if (DistanceManhattan(tile, town->xy) >= threshold) town = NULL;

				return town;
			}
			/* FALL THROUGH */

		case MP_HOUSE:
			return Town::GetByTile(tile);

		default:
			return CalcClosestTownFromTile(tile, threshold);
	}
}

static bool _town_rating_test = false; ///< If \c true, town rating is in test-mode.
static SmallMap<const Town *, int, 4> _town_test_ratings; ///< Map of towns to modified ratings, while in town rating test-mode.

/**
 * Switch the town rating to test-mode, to allow commands to be tested without affecting current ratings.
 * The function is safe to use in nested calls.
 * @param mode Test mode switch (\c true means go to test-mode, \c false means leave test-mode).
 */
void SetTownRatingTestMode(bool mode)
{
	static int ref_count = 0; // Number of times test-mode is switched on.
	if (mode) {
		if (ref_count == 0) {
			_town_test_ratings.Clear();
		}
		ref_count++;
	} else {
		assert(ref_count > 0);
		ref_count--;
	}
	_town_rating_test = !(ref_count == 0);
}

/**
 * Get the rating of a town for the #_current_company.
 * @param t Town to get the rating from.
 * @return Rating of the current company in the given town.
 */
static int GetRating(const Town *t)
{
	if (_town_rating_test) {
		SmallMap<const Town *, int>::iterator it = _town_test_ratings.Find(t);
		if (it != _town_test_ratings.End()) {
			return it->second;
		}
	}
	return t->ratings[_current_company];
}

/**
 * Changes town rating of the current company
 * @param t Town to affect
 * @param add Value to add
 * @param max Minimum (add < 0) resp. maximum (add > 0) rating that should be achievable with this change.
 * @param flags Command flags, especially DC_NO_MODIFY_TOWN_RATING is tested
 */
void ChangeTownRating(Town *t, int add, int max, DoCommandFlag flags)
{
	/* if magic_bulldozer cheat is active, town doesn't penalize for removing stuff */
	if (t == NULL || (flags & DC_NO_MODIFY_TOWN_RATING) ||
			!Company::IsValidID(_current_company) ||
			(_cheats.magic_bulldozer.value && add < 0)) {
		return;
	}

	int rating = GetRating(t);
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
		_town_test_ratings[t] = rating;
	} else {
		SetBit(t->have_ratings, _current_company);
		t->ratings[_current_company] = rating;
		SetWindowDirty(WC_TOWN_AUTHORITY, t->index);
	}
}

/**
 * Does the town authority allow the (destructive) action of the current company?
 * @param flags Checking flags of the command.
 * @param t     Town that must allow the company action.
 * @param type  Type of action that is wanted.
 * @return A succeeded command if the action is allowed, a failed command if it is not allowed.
 */
CommandCost CheckforTownRating(DoCommandFlag flags, Town *t, TownRatingCheckType type)
{
	/* if magic_bulldozer cheat is active, town doesn't restrict your destructive actions */
	if (t == NULL || !Company::IsValidID(_current_company) ||
			_cheats.magic_bulldozer.value || (flags & DC_NO_TEST_TOWN_RATING)) {
		return CommandCost();
	}

	/* minimum rating needed to be allowed to remove stuff */
	static const int needed_rating[][TOWN_RATING_CHECK_TYPE_COUNT] = {
		/*                  ROAD_REMOVE,                    TUNNELBRIDGE_REMOVE */
		{ RATING_ROAD_NEEDED_PERMISSIVE, RATING_TUNNEL_BRIDGE_NEEDED_PERMISSIVE}, // Permissive
		{    RATING_ROAD_NEEDED_NEUTRAL,    RATING_TUNNEL_BRIDGE_NEEDED_NEUTRAL}, // Neutral
		{    RATING_ROAD_NEEDED_HOSTILE,    RATING_TUNNEL_BRIDGE_NEEDED_HOSTILE}, // Hostile
	};

	/* check if you're allowed to remove the road/bridge/tunnel
	 * owned by a town no removal if rating is lower than ... depends now on
	 * difficulty setting. Minimum town rating selected by difficulty level
	 */
	int needed = needed_rating[_settings_game.difficulty.town_council_tolerance][type];

	if (GetRating(t) < needed) {
		SetDParam(0, t->index);
		return_cmd_error(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
	}

	return CommandCost();
}

void TownsMonthlyLoop()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		if (t->road_build_months != 0) t->road_build_months--;

		if (t->exclusive_counter != 0) {
			if (--t->exclusive_counter == 0) t->exclusivity = INVALID_COMPANY;
		}

		UpdateTownGrowRate(t);
		UpdateTownAmounts(t);
		UpdateTownUnwanted(t);
	}
}

void TownsYearlyLoop()
{
	/* Increment house ages */
	for (TileIndex t = 0; t < MapSize(); t++) {
		if (!IsTileType(t, MP_HOUSE)) continue;
		IncrementHouseAge(t);
	}
}

void InitializeTowns()
{
	_town_pool.CleanPool();
}

static CommandCost TerraformTile_Town(TileIndex tile, DoCommandFlag flags, uint z_new, Slope tileh_new)
{
	if (AutoslopeEnabled()) {
		HouseID house = GetHouseType(tile);
		GetHouseNorthPart(house); // modifies house to the ID of the north tile
		const HouseSpec *hs = HouseSpec::Get(house);

		/* Here we differ from TTDP by checking TILE_NOT_SLOPED */
		if (((hs->building_flags & TILE_NOT_SLOPED) == 0) && !IsSteepSlope(tileh_new) &&
				(GetTileMaxZ(tile) == z_new + GetSlopeMaxZ(tileh_new))) {
			bool allow_terraform = true;

			/* Call the autosloping callback per tile, not for the whole building at once. */
			house = GetHouseType(tile);
			hs = HouseSpec::Get(house);
			if (HasBit(hs->callback_mask, CBM_HOUSE_AUTOSLOPE)) {
				/* If the callback fails, allow autoslope. */
				uint16 res = GetHouseCallback(CBID_HOUSE_AUTOSLOPE, 0, 0, house, Town::GetByTile(tile), tile);
				if ((res != 0) && (res != CALLBACK_FAILED)) allow_terraform = false;
			}

			if (allow_terraform) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
		}
	}

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

/** Tile callback functions for a town */
extern const TileTypeProcs _tile_type_town_procs = {
	DrawTile_Town,           // draw_tile_proc
	GetSlopeZ_Town,          // get_slope_z_proc
	ClearTile_Town,          // clear_tile_proc
	AddAcceptedCargo_Town,   // add_accepted_cargo_proc
	GetTileDesc_Town,        // get_tile_desc_proc
	GetTileTrackStatus_Town, // get_tile_track_status_proc
	NULL,                    // click_tile_proc
	AnimateTile_Town,        // animate_tile_proc
	TileLoop_Town,           // tile_loop_clear
	ChangeTileOwner_Town,    // change_tile_owner_clear
	AddProducedCargo_Town,   // add_produced_cargo_proc
	NULL,                    // vehicle_enter_tile_proc
	GetFoundation_Town,      // get_foundation_proc
	TerraformTile_Town,      // terraform_tile_proc
};


HouseSpec _house_specs[HOUSE_MAX];

void ResetHouses()
{
	memset(&_house_specs, 0, sizeof(_house_specs));
	memcpy(&_house_specs, &_original_house_specs, sizeof(_original_house_specs));

	/* Reset any overrides that have been set. */
	_house_mngr.ResetOverride();
}
