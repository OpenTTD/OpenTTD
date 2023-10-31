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
#include "industrytype.h"
#include "tilearea_type.h"
#include "station_base.h"
#include "timer/timer_game_calendar.h"


typedef Pool<Industry, IndustryID, 64, 64000> IndustryPool;
extern IndustryPool _industry_pool;

static const TimerGameCalendar::Year PROCESSING_INDUSTRY_ABANDONMENT_YEARS = 5; ///< If a processing industry doesn't produce for this many consecutive years, it may close.

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
 * Flags to control/override the behaviour of an industry.
 * These flags are controlled by game scripts.
 */
enum IndustryControlFlags : byte {
	/** No flags in effect */
	INDCTL_NONE                   = 0,
	/** When industry production change is evaluated, rolls to decrease are ignored. */
	INDCTL_NO_PRODUCTION_DECREASE = 1 << 0,
	/** When industry production change is evaluated, rolls to increase are ignored. */
	INDCTL_NO_PRODUCTION_INCREASE = 1 << 1,
	/**
	 * Industry can not close regardless of production level or time since last delivery.
	 * This does not prevent a closure already announced. */
	INDCTL_NO_CLOSURE             = 1 << 2,
	/** Indicates that the production level of the industry is externally controlled. */
	INDCTL_EXTERNAL_PROD_LEVEL    = 1 << 3,
	/** Mask of all flags set */
	INDCTL_MASK = INDCTL_NO_PRODUCTION_DECREASE | INDCTL_NO_PRODUCTION_INCREASE | INDCTL_NO_CLOSURE | INDCTL_EXTERNAL_PROD_LEVEL,
};
DECLARE_ENUM_AS_BIT_SET(IndustryControlFlags);

static const int THIS_MONTH = 0;
static const int LAST_MONTH = 1;

/**
 * Defines the internal data of a functional industry.
 */
struct Industry : IndustryPool::PoolItem<&_industry_pool> {
	struct ProducedHistory {
		uint16_t production; ///< Total produced
		uint16_t transported; ///< Total transported

		uint8_t PctTransported() const
		{
			if (this->production == 0) return 0;
			return ClampTo<uint8_t>(this->transported * 256 / this->production);
		}
	};

	struct ProducedCargo {
		CargoID cargo; ///< Cargo type
		uint16_t waiting; ///< Amount of cargo produced
		uint8_t rate; ///< Production rate
		std::array<ProducedHistory, 2> history; ///< History of cargo produced and transported
	};

	struct AcceptedCargo {
		CargoID cargo; ///< Cargo type
		uint16_t waiting; ///< Amount of cargo waiting to processed
		TimerGameCalendar::Date last_accepted; ///< Last day cargo was accepted by this industry
	};

	using ProducedCargoArray = std::array<ProducedCargo, INDUSTRY_NUM_OUTPUTS>;
	using AcceptedCargoArray = std::array<AcceptedCargo, INDUSTRY_NUM_INPUTS>;

	TileArea location;                                     ///< Location of the industry
	Town *town;                                            ///< Nearest town
	Station *neutral_station;                              ///< Associated neutral station
	ProducedCargoArray produced; ///< INDUSTRY_NUM_OUTPUTS production cargo slots
	AcceptedCargoArray accepted; ///< INDUSTRY_NUM_INPUTS input cargo slots
	byte prod_level;                                       ///< general production level
	uint16_t counter;                                        ///< used for animation and/or production (if available cargo)

	IndustryType type;             ///< type of industry.
	Owner owner;                   ///< owner of the industry.  Which SHOULD always be (imho) OWNER_NONE
	byte random_colour;            ///< randomized colour of the industry, for display purpose
	TimerGameCalendar::Year last_prod_year; ///< last year of production
	byte was_cargo_delivered;      ///< flag that indicate this has been the closest industry chosen for cargo delivery by a station. see DeliverGoodsToIndustry
	IndustryControlFlags ctlflags; ///< flags overriding standard behaviours

	PartOfSubsidy part_of_subsidy; ///< NOSAVE: is this industry a source/destination of a subsidy?
	StationList stations_near;     ///< NOSAVE: List of nearby stations.
	mutable std::string cached_name; ///< NOSAVE: Cache of the resolved name of the industry

	Owner founder;                 ///< Founder of the industry
	TimerGameCalendar::Date construction_date; ///< Date of the construction of the industry
	uint8_t construction_type;       ///< Way the industry was constructed (@see IndustryConstructionType)
	byte selected_layout;          ///< Which tile layout was used when creating the industry
	Owner exclusive_supplier;      ///< Which company has exclusive rights to deliver cargo (INVALID_OWNER = anyone)
	Owner exclusive_consumer;      ///< Which company has exclusive rights to take cargo (INVALID_OWNER = anyone)
	std::string text;              ///< General text with additional information.

	uint16_t random;                 ///< Random value used for randomisation of all kinds of things

	PersistentStorage *psa;        ///< Persistent storage for NewGRF industries.

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
	 * Get produced cargo slot for a specific cargo type.
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to produced cargo slot if it exists, or the end iterator.
	 */
	inline ProducedCargoArray::iterator GetCargoProduced(CargoID cargo)
	{
		if (!IsValidCargoID(cargo)) return std::end(this->produced);
		return std::find_if(std::begin(this->produced), std::end(this->produced), [&cargo](const auto &p) { return p.cargo == cargo; });
	}

	/**
	 * Get produced cargo slot for a specific cargo type (const-variant).
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to produced cargo slot if it exists, or the end iterator.
	 */
	inline ProducedCargoArray::const_iterator GetCargoProduced(CargoID cargo) const
	{
		if (!IsValidCargoID(cargo)) return std::end(this->produced);
		return std::find_if(std::begin(this->produced), std::end(this->produced), [&cargo](const auto &p) { return p.cargo == cargo; });
	}

	/**
	 * Get accepted cargo slot for a specific cargo type.
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to accepted cargo slot if it exists, or the end iterator.
	 */
	inline AcceptedCargoArray::iterator GetCargoAccepted(CargoID cargo)
	{
		if (!IsValidCargoID(cargo)) return std::end(this->accepted);
		return std::find_if(std::begin(this->accepted), std::end(this->accepted), [&cargo](const auto &a) { return a.cargo == cargo; });
	}

	/**
	 * Test if this industry accepts any cargo.
	 * @return true iff the industry accepts any cargo.
	 */
	bool IsCargoAccepted() const { return std::any_of(std::begin(this->accepted), std::end(this->accepted), [](const auto &a) { return IsValidCargoID(a.cargo); }); }

	/**
	 * Test if this industry produces any cargo.
	 * @return true iff the industry produces any cargo.
	 */
	bool IsCargoProduced() const { return std::any_of(std::begin(this->produced), std::end(this->produced), [](const auto &p) { return IsValidCargoID(p.cargo); }); }

	/**
	 * Test if this industry accepts a specific cargo.
	 * @param cargo Cargo type to test.
	 * @return true iff the industry accepts the given cargo type.
	 */
	bool IsCargoAccepted(CargoID cargo) const { return std::any_of(std::begin(this->accepted), std::end(this->accepted), [&cargo](const auto &a) { return a.cargo == cargo; }); }

	/**
	 * Test if this industry produces a specific cargo.
	 * @param cargo Cargo type to test.
	 * @return true iff the industry produces the given cargo types.
	 */
	bool IsCargoProduced(CargoID cargo) const { return std::any_of(std::begin(this->produced), std::end(this->produced), [&cargo](const auto &p) { return p.cargo == cargo; }); }

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
	static inline uint16_t GetIndustryTypeCount(IndustryType type)
	{
		assert(type < NUM_INDUSTRYTYPES);
		return counts[type];
	}

	/** Resets industry counts. */
	static inline void ResetIndustryCounts()
	{
		memset(&counts, 0, sizeof(counts));
	}

	inline const std::string &GetCachedName() const
	{
		if (this->cached_name.empty()) this->FillCachedName();
		return this->cached_name;
	}

private:
	void FillCachedName() const;

protected:
	static uint16_t counts[NUM_INDUSTRYTYPES]; ///< Number of industries per type ingame
};

void ClearAllIndustryCachedNames();

void PlantRandomFarmField(const Industry *i);

void ReleaseDisastersTargetingIndustry(IndustryID);

bool IsTileForestIndustry(TileIndex tile);

/** Data for managing the number of industries of a single industry type. */
struct IndustryTypeBuildData {
	uint32_t probability;  ///< Relative probability of building this industry.
	byte   min_number;   ///< Smallest number of industries that should exist (either \c 0 or \c 1).
	uint16_t target_count; ///< Desired number of industries of this type.
	uint16_t max_wait;     ///< Starting number of turns to wait (copied to #wait_count).
	uint16_t wait_count;   ///< Number of turns to wait before trying to build again.

	void Reset();

	bool GetIndustryTypeData(IndustryType it);
};

/**
 * Data for managing the number and type of industries in the game.
 */
struct IndustryBuildData {
	IndustryTypeBuildData builddata[NUM_INDUSTRYTYPES]; ///< Industry build data for every industry type.
	uint32_t wanted_inds; ///< Number of wanted industries (bits 31-16), and a fraction (bits 15-0).

	void Reset();

	void SetupTargetCount();
	void TryBuildNewIndustry();

	void MonthlyLoop();
};

extern IndustryBuildData _industry_builder;


/** Special values for the industry list window for the data parameter of #InvalidateWindowData. */
enum IndustryDirectoryInvalidateWindowData {
	IDIWD_FORCE_REBUILD,
	IDIWD_PRODUCTION_CHANGE,
	IDIWD_FORCE_RESORT,
};

#endif /* INDUSTRY_H */
