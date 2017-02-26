/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_sl.cpp Code handling saving and loading of towns and houses */

#include "../stdafx.h"
#include "../newgrf_house.h"
#include "../town.h"
#include "../landscape.h"
#include "../subsidy_func.h"
#include "../strings_func.h"

#include "saveload.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

/**
 * Rebuild all the cached variables of towns.
 */
void RebuildTownCaches()
{
	Town *town;
	InitializeBuildingCounts();

	/* Reset town population and num_houses */
	FOR_ALL_TOWNS(town) {
		town->cache.population = 0;
		town->cache.num_houses = 0;
	}

	for (TileIndex t = 0; t < MapSize(); t++) {
		if (!IsTileType(t, MP_HOUSE)) continue;

		HouseID house_id = GetHouseType(t);
		town = Town::GetByTile(t);
		IncreaseBuildingCount(town, house_id);
		if (IsHouseCompleted(t)) town->cache.population += HouseSpec::Get(house_id)->population;

		/* Increase the number of houses for every house, but only once. */
		if (GetHouseNorthPart(house_id) == 0) town->cache.num_houses++;
	}

	/* Update the population and num_house dependent values */
	FOR_ALL_TOWNS(town) {
		UpdateTownRadius(town);
		UpdateTownCargoes(town);
	}
	UpdateTownCargoBitmap();
}

/**
 * Check and update town and house values.
 *
 * Checked are the HouseIDs. Updated are the
 * town population the number of houses per
 * town, the town radius and the max passengers
 * of the town.
 */
void UpdateHousesAndTowns()
{
	for (TileIndex t = 0; t < MapSize(); t++) {
		if (!IsTileType(t, MP_HOUSE)) continue;

		HouseID house_id = GetCleanHouseType(t);
		if (!HouseSpec::Get(house_id)->enabled && house_id >= NEW_HOUSE_OFFSET) {
			/* The specs for this type of house are not available any more, so
			 * replace it with the substitute original house type. */
			house_id = _house_mngr.GetSubstituteID(house_id);
			SetHouseType(t, house_id);
		}
	}

	/* Check for cases when a NewGRF has set a wrong house substitute type. */
	for (TileIndex t = 0; t < MapSize(); t++) {
		if (!IsTileType(t, MP_HOUSE)) continue;

		HouseID house_type = GetCleanHouseType(t);
		TileIndex north_tile = t + GetHouseNorthPart(house_type); // modifies 'house_type'!
		if (t == north_tile) {
			const HouseSpec *hs = HouseSpec::Get(house_type);
			bool valid_house = true;
			if (hs->building_flags & TILE_SIZE_2x1) {
				TileIndex tile = t + TileDiffXY(1, 0);
				if (!IsTileType(tile, MP_HOUSE) || GetCleanHouseType(tile) != house_type + 1) valid_house = false;
			} else if (hs->building_flags & TILE_SIZE_1x2) {
				TileIndex tile = t + TileDiffXY(0, 1);
				if (!IsTileType(tile, MP_HOUSE) || GetCleanHouseType(tile) != house_type + 1) valid_house = false;
			} else if (hs->building_flags & TILE_SIZE_2x2) {
				TileIndex tile = t + TileDiffXY(0, 1);
				if (!IsTileType(tile, MP_HOUSE) || GetCleanHouseType(tile) != house_type + 1) valid_house = false;
				tile = t + TileDiffXY(1, 0);
				if (!IsTileType(tile, MP_HOUSE) || GetCleanHouseType(tile) != house_type + 2) valid_house = false;
				tile = t + TileDiffXY(1, 1);
				if (!IsTileType(tile, MP_HOUSE) || GetCleanHouseType(tile) != house_type + 3) valid_house = false;
			}
			/* If not all tiles of this house are present remove the house.
			 * The other tiles will get removed later in this loop because
			 * their north tile is not the correct type anymore. */
			if (!valid_house) DoClearSquare(t);
		} else if (!IsTileType(north_tile, MP_HOUSE) || GetCleanHouseType(north_tile) != house_type) {
			/* This tile should be part of a multi-tile building but the
			 * north tile of this house isn't on the map. */
			DoClearSquare(t);
		}
	}

	RebuildTownCaches();
}

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
	SLE_CONDSTR(Town, name,                  SLE_STR | SLF_ALLOW_CONTROL, 0, 84, SL_MAX_VERSION),

	    SLE_VAR(Town, flags,                 SLE_UINT8),
	SLE_CONDVAR(Town, statues,               SLE_FILE_U8  | SLE_VAR_U16, 0, 103),
	SLE_CONDVAR(Town, statues,               SLE_UINT16,               104, SL_MAX_VERSION),

	SLE_CONDNULL(1, 0, 1),                   ///< sort_index, no longer in use

	SLE_CONDVAR(Town, have_ratings,          SLE_FILE_U8  | SLE_VAR_U16, 0, 103),
	SLE_CONDVAR(Town, have_ratings,          SLE_UINT16,               104, SL_MAX_VERSION),
	SLE_CONDARR(Town, ratings,               SLE_INT16, 8,               0, 103),
	SLE_CONDARR(Town, ratings,               SLE_INT16, MAX_COMPANIES, 104, SL_MAX_VERSION),
	/* failed bribe attempts are stored since savegame format 4 */
	SLE_CONDARR(Town, unwanted,              SLE_INT8,  8,               4, 103),
	SLE_CONDARR(Town, unwanted,              SLE_INT8,  MAX_COMPANIES, 104, SL_MAX_VERSION),

	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_max, SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_max,       SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_max, SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_max,       SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_act, SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_act,       SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_act, SLE_FILE_U16 | SLE_VAR_U32, 0, 8),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_act,       SLE_FILE_U16 | SLE_VAR_U32, 0, 8),

	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_max, SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_max,       SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_max, SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_max,       SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_act, SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_act,       SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_act, SLE_UINT32,                 9, 164),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_act,       SLE_UINT32,                 9, 164),

	SLE_CONDNULL(2, 0, 163),                 ///< pct_pass_transported / pct_mail_transported, now computed on the fly

	SLE_CONDVAR(Town, received[TE_FOOD].old_act,       SLE_UINT16,                 0, 164),
	SLE_CONDVAR(Town, received[TE_WATER].old_act,      SLE_UINT16,                 0, 164),
	SLE_CONDVAR(Town, received[TE_FOOD].new_act,       SLE_UINT16,                 0, 164),
	SLE_CONDVAR(Town, received[TE_WATER].new_act,      SLE_UINT16,                 0, 164),

	SLE_CONDARR(Town, goal, SLE_UINT32, NUM_TE, 165, SL_MAX_VERSION),

	SLE_CONDSTR(Town, text,                  SLE_STR | SLF_ALLOW_CONTROL, 0, 168, SL_MAX_VERSION),

	SLE_CONDVAR(Town, time_until_rebuild,    SLE_FILE_U8 | SLE_VAR_U16,  0, 53),
	SLE_CONDVAR(Town, grow_counter,          SLE_FILE_U8 | SLE_VAR_U16,  0, 53),
	SLE_CONDVAR(Town, growth_rate,           SLE_FILE_U8 | SLE_VAR_I16,  0, 53),

	SLE_CONDVAR(Town, time_until_rebuild,    SLE_UINT16,                54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, grow_counter,          SLE_UINT16,                54, SL_MAX_VERSION),

	SLE_CONDVAR(Town, growth_rate,           SLE_FILE_I16 | SLE_VAR_U16, 54, 164),
	SLE_CONDVAR(Town, growth_rate,           SLE_UINT16,                 165, SL_MAX_VERSION),

	    SLE_VAR(Town, fund_buildings_months, SLE_UINT8),
	    SLE_VAR(Town, road_build_months,     SLE_UINT8),

	SLE_CONDVAR(Town, exclusivity,           SLE_UINT8,                  2, SL_MAX_VERSION),
	SLE_CONDVAR(Town, exclusive_counter,     SLE_UINT8,                  2, SL_MAX_VERSION),

	SLE_CONDVAR(Town, larger_town,           SLE_BOOL,                  56, SL_MAX_VERSION),
	SLE_CONDVAR(Town, layout,                SLE_UINT8,                113, SL_MAX_VERSION),

	SLE_CONDLST(Town, psa_list,            REF_STORAGE,                161, SL_MAX_VERSION),

	SLE_CONDVAR(Town, cargo_produced,       SLE_UINT32,                166, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 30 bytes) */
	SLE_CONDNULL(30, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _town_supplied_desc[] = {
	SLE_CONDVAR(TransportedCargoStat<uint32>, old_max, SLE_UINT32, 165, SL_MAX_VERSION),
	SLE_CONDVAR(TransportedCargoStat<uint32>, new_max, SLE_UINT32, 165, SL_MAX_VERSION),
	SLE_CONDVAR(TransportedCargoStat<uint32>, old_act, SLE_UINT32, 165, SL_MAX_VERSION),
	SLE_CONDVAR(TransportedCargoStat<uint32>, new_act, SLE_UINT32, 165, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _town_received_desc[] = {
	SLE_CONDVAR(TransportedCargoStat<uint16>, old_max, SLE_UINT16, 165, SL_MAX_VERSION),
	SLE_CONDVAR(TransportedCargoStat<uint16>, new_max, SLE_UINT16, 165, SL_MAX_VERSION),
	SLE_CONDVAR(TransportedCargoStat<uint16>, old_act, SLE_UINT16, 165, SL_MAX_VERSION),
	SLE_CONDVAR(TransportedCargoStat<uint16>, new_act, SLE_UINT16, 165, SL_MAX_VERSION),

	SLE_END()
};

static void Save_HIDS()
{
	Save_NewGRFMapping(_house_mngr);
}

static void Load_HIDS()
{
	Load_NewGRFMapping(_house_mngr);
}

const SaveLoad *GetTileMatrixDesc()
{
	/* Here due to private member vars. */
	static const SaveLoad _tilematrix_desc[] = {
		SLE_VAR(AcceptanceMatrix, area.tile, SLE_UINT32),
		SLE_VAR(AcceptanceMatrix, area.w,    SLE_UINT16),
		SLE_VAR(AcceptanceMatrix, area.h,    SLE_UINT16),
		SLE_END()
	};

	return _tilematrix_desc;
}

static void RealSave_Town(Town *t)
{
	SlObject(t, _town_desc);

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		SlObject(&t->supplied[i], _town_supplied_desc);
	}
	for (int i = TE_BEGIN; i < NUM_TE; i++) {
		SlObject(&t->received[i], _town_received_desc);
	}

	if (IsSavegameVersionBefore(166)) return;

	SlObject(&t->cargo_accepted, GetTileMatrixDesc());
	if (t->cargo_accepted.area.w != 0) {
		uint arr_len = t->cargo_accepted.area.w / AcceptanceMatrix::GRID * t->cargo_accepted.area.h / AcceptanceMatrix::GRID;
		SlArray(t->cargo_accepted.data, arr_len, SLE_UINT32);
	}
}

static void Save_TOWN()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		SlSetArrayIndex(t->index);
		SlAutolength((AutolengthProc*)RealSave_Town, t);
	}
}

static void Load_TOWN()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Town *t = new (index) Town();
		SlObject(t, _town_desc);

		for (CargoID i = 0; i < NUM_CARGO; i++) {
			SlObject(&t->supplied[i], _town_supplied_desc);
		}
		for (int i = TE_BEGIN; i < TE_END; i++) {
			SlObject(&t->received[i], _town_received_desc);
		}

		if (t->townnamegrfid == 0 && !IsInsideMM(t->townnametype, SPECSTR_TOWNNAME_START, SPECSTR_TOWNNAME_LAST + 1) && GetStringTab(t->townnametype) != TEXT_TAB_OLD_CUSTOM) {
			SlErrorCorrupt("Invalid town name generator");
		}

		if (IsSavegameVersionBefore(166)) continue;

		SlObject(&t->cargo_accepted, GetTileMatrixDesc());
		if (t->cargo_accepted.area.w != 0) {
			uint arr_len = t->cargo_accepted.area.w / AcceptanceMatrix::GRID * t->cargo_accepted.area.h / AcceptanceMatrix::GRID;
			t->cargo_accepted.data = MallocT<uint32>(arr_len);
			SlArray(t->cargo_accepted.data, arr_len, SLE_UINT32);

			/* Rebuild total cargo acceptance. */
			UpdateTownCargoTotal(t);
		}
	}
}

/** Fix pointers when loading town data. */
static void Ptrs_TOWN()
{
	/* Don't run when savegame version lower than 161. */
	if (IsSavegameVersionBefore(161)) return;

	Town *t;
	FOR_ALL_TOWNS(t) {
		SlObject(t, _town_desc);
	}
}

/** Chunk handler for towns. */
extern const ChunkHandler _town_chunk_handlers[] = {
	{ 'HIDS', Save_HIDS, Load_HIDS,      NULL, NULL, CH_ARRAY },
	{ 'CITY', Save_TOWN, Load_TOWN, Ptrs_TOWN, NULL, CH_ARRAY | CH_LAST},
};
