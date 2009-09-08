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

#include "core/pool_type.hpp"
#include "core/random_func.hpp"
#include "newgrf_storage.h"
#include "cargo_type.h"
#include "economy_type.h"
#include "map_type.h"
#include "industry_type.h"
#include "tile_type.h"
#include "subsidy_type.h"
#include "industry_map.h"


typedef Pool<Industry, IndustryID, 64, 64000> IndustryPool;
extern IndustryPool _industry_pool;

/**
 * Defines the internal data of a functionnal industry
 */
struct Industry : IndustryPool::PoolItem<&_industry_pool> {
	typedef PersistentStorageArray<uint32, 16> PersistentStorage;

	TileIndex xy;                       ///< coordinates of the primary tile the industry is built one
	byte width;
	byte height;
	const Town *town;                   ///< Nearest town
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

	byte random_triggers;               ///< Triggers for the random
	uint16 random;                      ///< Random value used for randomisation of all kinds of things

	PersistentStorage psa;              ///< Persistent storage for NewGRF industries.

	Industry(TileIndex tile = INVALID_TILE) : xy(tile) {}
	~Industry();

	/**
	 * Get the industry of the given tile
	 * @param t the tile to get the industry from
	 * @pre IsTileType(t, MP_INDUSTRY)
	 * @return the industry
	 */
	static FORCEINLINE Industry *GetByTile(TileIndex tile)
	{
		return Industry::Get(GetIndustryIndex(tile));
	}

	static Industry *GetRandom();
	static void PostDestructor(size_t index);
};

void PlantRandomFarmField(const Industry *i);

void ReleaseDisastersTargetingIndustry(IndustryID);

/* smallmap_gui.cpp */
void BuildIndustriesLegend();
/* industry_cmd.cpp */
void SetIndustryDailyChanges();

#define FOR_ALL_INDUSTRIES_FROM(var, start) FOR_ALL_ITEMS_FROM(Industry, industry_index, var, start)
#define FOR_ALL_INDUSTRIES(var) FOR_ALL_INDUSTRIES_FROM(var, 0)

#endif /* INDUSTRY_H */
