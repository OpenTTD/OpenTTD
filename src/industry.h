/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry.h Base of all industries. */

#ifndef INDUSTRY_H
#define INDUSTRY_H

#include "newgrf_storage.h"
#include "subsidy_type.h"
#include "industry_map.h"
#include "tilearea_type.h"


typedef Pool<Industry, IndustryID, 64, 64000> IndustryPool;
extern IndustryPool _industry_pool;

/**
 * Production level maximum, minimum and default values.
 * It is not a value been really used in order to change, but rather an indicator
 * of how the industry is behaving.
 */
enum ProductionLevels {
	PRODLEVEL_CLOSURE = 0x00,  ///< signal set to actually close the industry
	PRODLEVEL_MINIMUM = 0x04,  ///< below this level, the industry is set to be closing
	PRODLEVEL_DEFAULT = 0x10,  ///< default level set when the industry is created
	PRODLEVEL_MAXIMUM = 0x80,  ///< the industry is running at full speed
};

/**
 * Defines the internal data of a functional industry.
 */
struct Industry : IndustryPool::PoolItem<&_industry_pool> {
	TileArea location;                  ///< Location of the industry
	Town *town;                         ///< Nearest town
	CargoID produced_cargo[2];          ///< 2 production cargo slots
	uint16 produced_cargo_waiting[2];   ///< amount of cargo produced per cargo
	uint16 incoming_cargo_waiting[3];   ///< incoming cargo waiting to be processed
	byte production_rate[2];            ///< production rate for each cargo
	byte prod_level;                    ///< general production level
	CargoID accepts_cargo[3];           ///< 3 input cargo slots
	uint16 this_month_production[2];    ///< stats of this month's production per cargo
	uint16 this_month_transported[2];   ///< stats of this month's transport per cargo
	byte last_month_pct_transported[2]; ///< percentage transported per cargo in the last full month
	uint16 last_month_production[2];    ///< total units produced per cargo in the last full month
	uint16 last_month_transported[2];   ///< total units transported per cargo in the last full month
	uint16 counter;                     ///< used for animation and/or production (if available cargo)

	IndustryType type;                  ///< type of industry.
	OwnerByte owner;                    ///< owner of the industry.  Which SHOULD always be (imho) OWNER_NONE
	byte random_colour;                 ///< randomized colour of the industry, for display purpose
	Year last_prod_year;                ///< last year of production
	byte was_cargo_delivered;           ///< flag that indicate this has been the closest industry chosen for cargo delivery by a station. see DeliverGoodsToIndustry

	PartOfSubsidyByte part_of_subsidy;  ///< NOSAVE: is this industry a source/destination of a subsidy?

	OwnerByte founder;                  ///< Founder of the industry
	Date construction_date;             ///< Date of the construction of the industry
	uint8 construction_type;            ///< Way the industry was constructed (@see IndustryConstructionType)
	Date last_cargo_accepted_at;        ///< Last day cargo was accepted by this industry
	byte selected_layout;               ///< Which tile layout was used when creating the industry

	uint16 random;                      ///< Random value used for randomisation of all kinds of things

	PersistentStorage *psa;             ///< Persistent storage for NewGRF industries.

	Industry(TileIndex tile = INVALID_TILE) : location(tile, 0, 0) {}
	~Industry();

	void RecomputeProductionMultipliers();

	/**
	 * Check if a given tile belongs to this industry.
	 * @param tile The tile to check.
	 * @return True if the tile is part of this industry.
	 */
	inline bool TileBelongsToIndustry(TileIndex tile) const
	{
		return IsTileType(tile, MP_INDUSTRY) && GetIndustryIndex(tile) == this->index;
	}

	/**
	 * Get the industry of the given tile
	 * @param tile the tile to get the industry from
	 * @pre IsTileType(t, MP_INDUSTRY)
	 * @return the industry
	 */
	static inline Industry *GetByTile(TileIndex tile)
	{
		return Industry::Get(GetIndustryIndex(tile));
	}

	static Industry *GetRandom();
	static void PostDestructor(size_t index);

	/**
	 * Increment the count of industries for this type.
	 * @param type IndustryType to increment
	 * @pre type < NUM_INDUSTRYTYPES
	 */
	static inline void IncIndustryTypeCount(IndustryType type)
	{
		assert(type < NUM_INDUSTRYTYPES);
		counts[type]++;
	}

	/**
	 * Decrement the count of industries for this type.
	 * @param type IndustryType to decrement
	 * @pre type < NUM_INDUSTRYTYPES
	 */
	static inline void DecIndustryTypeCount(IndustryType type)
	{
		assert(type < NUM_INDUSTRYTYPES);
		counts[type]--;
	}

	/**
	 * Get the count of industries for this type.
	 * @param type IndustryType to query
	 * @pre type < NUM_INDUSTRYTYPES
	 */
	static inline uint16 GetIndustryTypeCount(IndustryType type)
	{
		assert(type < NUM_INDUSTRYTYPES);
		return counts[type];
	}

	/** Resets industry counts. */
	static inline void ResetIndustryCounts()
	{
		memset(&counts, 0, sizeof(counts));
	}

protected:
	static uint16 counts[NUM_INDUSTRYTYPES]; ///< Number of industries per type ingame
};

void PlantRandomFarmField(const Industry *i);

void ReleaseDisastersTargetingIndustry(IndustryID);

bool IsTileForestIndustry(TileIndex tile);

#define FOR_ALL_INDUSTRIES_FROM(var, start) FOR_ALL_ITEMS_FROM(Industry, industry_index, var, start)
#define FOR_ALL_INDUSTRIES(var) FOR_ALL_INDUSTRIES_FROM(var, 0)

/** Data for managing the number of industries of a single industry type. */
struct IndustryTypeBuildData {
	uint32 probability;  ///< Relative probability of building this industry.
	byte   min_number;   ///< Smallest number of industries that should exist (either \c 0 or \c 1).
	uint16 target_count; ///< Desired number of industries of this type.
	uint16 max_wait;     ///< Starting number of turns to wait (copied to #wait_count).
	uint16 wait_count;   ///< Number of turns to wait before trying to build again.

	void Reset();

	bool GetIndustryTypeData(IndustryType it);
};

/**
 * Data for managing the number and type of industries in the game.
 */
struct IndustryBuildData {
	IndustryTypeBuildData builddata[NUM_INDUSTRYTYPES]; ///< Industry build data for every industry type.
	uint32 wanted_inds; ///< Number of wanted industries (bits 31-16), and a fraction (bits 15-0).

	void Reset();

	void SetupTargetCount();
	void TryBuildNewIndustry();

	void MonthlyLoop();
};

extern IndustryBuildData _industry_builder;

#endif /* INDUSTRY_H */
