/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_sl.cpp Code handling saving and loading of towns and houses */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/town_sl_compat.h"

#include "newgrf_sl.h"
#include "../newgrf_house.h"
#include "../town.h"
#include "../landscape.h"
#include "../subsidy_func.h"
#include "../strings_func.h"
#include "../tilematrix_type.hpp"

#include "../safeguards.h"

typedef TileMatrix<CargoTypes, 4> AcceptanceMatrix;

/**
 * Rebuild all the cached variables of towns.
 */
void RebuildTownCaches()
{
	InitializeBuildingCounts();
	RebuildTownKdtree();

	/* Reset town population and num_houses */
	for (Town *town : Town::Iterate()) {
		town->cache.population = 0;
		town->cache.num_houses = 0;
	}

	for (TileIndex t = 0; t < Map::Size(); t++) {
		if (!IsTileType(t, MP_HOUSE)) continue;

		HouseID house_id = GetHouseType(t);
		Town *town = Town::GetByTile(t);
		IncreaseBuildingCount(town, house_id);
		if (IsHouseCompleted(t)) town->cache.population += HouseSpec::Get(house_id)->population;

		/* Increase the number of houses for every house, but only once. */
		if (GetHouseNorthPart(house_id) == 0) town->cache.num_houses++;
	}

	/* Update the population and num_house dependent values */
	for (Town *town : Town::Iterate()) {
		UpdateTownRadius(town);
	}
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
	for (TileIndex t = 0; t < Map::Size(); t++) {
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
	for (TileIndex t = 0; t < Map::Size(); t++) {
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

class SlTownSupplied : public DefaultSaveLoadHandler<SlTownSupplied, Town> {
public:
	inline static const SaveLoad description[] = {
		SLE_CONDVAR(TransportedCargoStat<uint32_t>, old_max, SLE_UINT32, SLV_165, SL_MAX_VERSION),
		SLE_CONDVAR(TransportedCargoStat<uint32_t>, new_max, SLE_UINT32, SLV_165, SL_MAX_VERSION),
		SLE_CONDVAR(TransportedCargoStat<uint32_t>, old_act, SLE_UINT32, SLV_165, SL_MAX_VERSION),
		SLE_CONDVAR(TransportedCargoStat<uint32_t>, new_act, SLE_UINT32, SLV_165, SL_MAX_VERSION),
	};
	inline const static SaveLoadCompatTable compat_description = _town_supplied_sl_compat;

	/**
	 * Get the number of cargoes used by this savegame version.
	 * @return The number of cargoes used by this savegame version.
	 */
	size_t GetNumCargo() const
	{
		if (IsSavegameVersionBefore(SLV_EXTEND_CARGOTYPES)) return 32;
		if (IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH)) return NUM_CARGO;
		/* Read from the savegame how long the list is. */
		return SlGetStructListLength(NUM_CARGO);
	}

	void Save(Town *t) const override
	{
		SlSetStructListLength(std::size(t->supplied));
		for (auto &supplied : t->supplied) {
			SlObject(&supplied, this->GetDescription());
		}
	}

	void Load(Town *t) const override
	{
		size_t num_cargo = this->GetNumCargo();
		for (size_t i = 0; i < num_cargo; i++) {
			SlObject(&t->supplied[i], this->GetLoadDescription());
		}
	}
};

class SlTownReceived : public DefaultSaveLoadHandler<SlTownReceived, Town> {
public:
	inline static const SaveLoad description[] = {
		SLE_CONDVAR(TransportedCargoStat<uint16_t>, old_max, SLE_UINT16, SLV_165, SL_MAX_VERSION),
		SLE_CONDVAR(TransportedCargoStat<uint16_t>, new_max, SLE_UINT16, SLV_165, SL_MAX_VERSION),
		SLE_CONDVAR(TransportedCargoStat<uint16_t>, old_act, SLE_UINT16, SLV_165, SL_MAX_VERSION),
		SLE_CONDVAR(TransportedCargoStat<uint16_t>, new_act, SLE_UINT16, SLV_165, SL_MAX_VERSION),
	};
	inline const static SaveLoadCompatTable compat_description = _town_received_sl_compat;

	void Save(Town *t) const override
	{
		SlSetStructListLength(std::size(t->received));
		for (auto &received : t->received) {
			SlObject(&received, this->GetDescription());
		}
	}

	void Load(Town *t) const override
	{
		size_t length = IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH) ? (size_t)TE_END : SlGetStructListLength(TE_END);
		for (size_t i = 0; i < length; i++) {
			SlObject(&t->received[i], this->GetLoadDescription());
		}
	}
};

class SlTownAcceptanceMatrix : public DefaultSaveLoadHandler<SlTownAcceptanceMatrix, Town> {
public:
	inline static const SaveLoad description[] = {
		SLE_VAR(AcceptanceMatrix, area.tile, SLE_UINT32),
		SLE_VAR(AcceptanceMatrix, area.w,    SLE_UINT16),
		SLE_VAR(AcceptanceMatrix, area.h,    SLE_UINT16),
	};
	inline const static SaveLoadCompatTable compat_description = _town_acceptance_matrix_sl_compat;

	void Load(Town *) const override
	{
		/* Discard now unused acceptance matrix. */
		AcceptanceMatrix dummy;
		SlObject(&dummy, this->GetLoadDescription());
		if (dummy.area.w != 0) {
			uint arr_len = dummy.area.w / AcceptanceMatrix::GRID * dummy.area.h / AcceptanceMatrix::GRID;
			SlSkipBytes(4 * arr_len);
		}
	}
};

static const SaveLoad _town_desc[] = {
	SLE_CONDVAR(Town, xy,                    SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(Town, xy,                    SLE_UINT32,                 SLV_6, SL_MAX_VERSION),

	SLE_CONDVAR(Town, townnamegrfid,         SLE_UINT32, SLV_66, SL_MAX_VERSION),
	    SLE_VAR(Town, townnametype,          SLE_UINT16),
	    SLE_VAR(Town, townnameparts,         SLE_UINT32),
	SLE_CONDSSTR(Town, name,                 SLE_STR | SLF_ALLOW_CONTROL, SLV_84, SL_MAX_VERSION),

	    SLE_VAR(Town, flags,                 SLE_UINT8),
	SLE_CONDVAR(Town, statues,               SLE_FILE_U8  | SLE_VAR_U16, SL_MIN_VERSION, SLV_104),
	SLE_CONDVAR(Town, statues,               SLE_UINT16,               SLV_104, SL_MAX_VERSION),

	SLE_CONDVAR(Town, have_ratings,          SLE_FILE_U8  | SLE_VAR_U16, SL_MIN_VERSION, SLV_104),
	SLE_CONDVAR(Town, have_ratings,          SLE_UINT16,               SLV_104, SL_MAX_VERSION),
	SLE_CONDARR(Town, ratings,               SLE_INT16, 8,               SL_MIN_VERSION, SLV_104),
	SLE_CONDARR(Town, ratings,               SLE_INT16, MAX_COMPANIES, SLV_104, SL_MAX_VERSION),
	SLE_CONDARR(Town, unwanted,              SLE_INT8,  8,               SLV_4, SLV_104),
	SLE_CONDARR(Town, unwanted,              SLE_INT8,  MAX_COMPANIES, SLV_104, SL_MAX_VERSION),

	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_max, SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_max, SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_max,       SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_max,       SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_max, SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_max, SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_max,       SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_max,       SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_act, SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].old_act, SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_act,       SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_MAIL].old_act,       SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_act, SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_PASSENGERS].new_act, SLE_UINT32,                 SLV_9, SLV_165),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_act,       SLE_FILE_U16 | SLE_VAR_U32, SL_MIN_VERSION, SLV_9),
	SLE_CONDVAR(Town, supplied[CT_MAIL].new_act,       SLE_UINT32,                 SLV_9, SLV_165),

	SLE_CONDVAR(Town, received[TE_FOOD].old_act,       SLE_UINT16,                 SL_MIN_VERSION, SLV_165),
	SLE_CONDVAR(Town, received[TE_WATER].old_act,      SLE_UINT16,                 SL_MIN_VERSION, SLV_165),
	SLE_CONDVAR(Town, received[TE_FOOD].new_act,       SLE_UINT16,                 SL_MIN_VERSION, SLV_165),
	SLE_CONDVAR(Town, received[TE_WATER].new_act,      SLE_UINT16,                 SL_MIN_VERSION, SLV_165),

	SLE_CONDARR(Town, goal, SLE_UINT32, NUM_TE, SLV_165, SL_MAX_VERSION),

	SLE_CONDSSTR(Town, text,                 SLE_STR | SLF_ALLOW_CONTROL, SLV_168, SL_MAX_VERSION),

	SLE_CONDVAR(Town, time_until_rebuild,    SLE_FILE_U8 | SLE_VAR_U16,  SL_MIN_VERSION, SLV_54),
	SLE_CONDVAR(Town, time_until_rebuild,    SLE_UINT16,                SLV_54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, grow_counter,          SLE_FILE_U8 | SLE_VAR_U16,  SL_MIN_VERSION, SLV_54),
	SLE_CONDVAR(Town, grow_counter,          SLE_UINT16,                SLV_54, SL_MAX_VERSION),
	SLE_CONDVAR(Town, growth_rate,           SLE_FILE_U8 | SLE_VAR_I16,  SL_MIN_VERSION, SLV_54),
	SLE_CONDVAR(Town, growth_rate,           SLE_FILE_I16 | SLE_VAR_U16, SLV_54, SLV_165),
	SLE_CONDVAR(Town, growth_rate,           SLE_UINT16,                 SLV_165, SL_MAX_VERSION),

	    SLE_VAR(Town, fund_buildings_months, SLE_UINT8),
	    SLE_VAR(Town, road_build_months,     SLE_UINT8),

	SLE_CONDVAR(Town, exclusivity,           SLE_UINT8,                  SLV_2, SL_MAX_VERSION),
	SLE_CONDVAR(Town, exclusive_counter,     SLE_UINT8,                  SLV_2, SL_MAX_VERSION),

	SLE_CONDVAR(Town, larger_town,           SLE_BOOL,                  SLV_56, SL_MAX_VERSION),
	SLE_CONDVAR(Town, layout,                SLE_UINT8,                SLV_113, SL_MAX_VERSION),

	SLE_CONDREFLIST(Town, psa_list,          REF_STORAGE,              SLV_161, SL_MAX_VERSION),

	SLEG_CONDSTRUCTLIST("supplied", SlTownSupplied,                    SLV_165, SL_MAX_VERSION),
	SLEG_CONDSTRUCTLIST("received", SlTownReceived,                    SLV_165, SL_MAX_VERSION),
	SLEG_CONDSTRUCTLIST("acceptance_matrix", SlTownAcceptanceMatrix,   SLV_166, SLV_REMOVE_TOWN_CARGO_CACHE),
};

struct HIDSChunkHandler : NewGRFMappingChunkHandler {
	HIDSChunkHandler() : NewGRFMappingChunkHandler('HIDS', _house_mngr) {}
};

struct CITYChunkHandler : ChunkHandler {
	CITYChunkHandler() : ChunkHandler('CITY', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_town_desc);

		for (Town *t : Town::Iterate()) {
			SlSetArrayIndex(t->index);
			SlObject(t, _town_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_town_desc, _town_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			Town *t = new (index) Town();
			SlObject(t, slt);

			if (t->townnamegrfid == 0 && !IsInsideMM(t->townnametype, SPECSTR_TOWNNAME_START, SPECSTR_TOWNNAME_LAST + 1) && GetStringTab(t->townnametype) != TEXT_TAB_OLD_CUSTOM) {
				SlErrorCorrupt("Invalid town name generator");
			}
		}
	}

	void FixPointers() const override
	{
		if (IsSavegameVersionBefore(SLV_161)) return;

		for (Town *t : Town::Iterate()) {
			SlObject(t, _town_desc);
		}
	}
};

static const HIDSChunkHandler HIDS;
static const CITYChunkHandler CITY;
static const ChunkHandlerRef town_chunk_handlers[] = {
	HIDS,
	CITY,
};

extern const ChunkHandlerTable _town_chunk_handlers(town_chunk_handlers);
