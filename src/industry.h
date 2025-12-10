/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry.h Base of all industries. */

#ifndef INDUSTRY_H
#define INDUSTRY_H

#include "core/flatset_type.hpp"
#include "misc/history_type.hpp"
#include "newgrf_storage.h"
#include "subsidy_type.h"
#include "industry_map.h"
#include "industrytype.h"
#include "tilearea_type.h"
#include "station_base.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"


typedef Pool<Industry, IndustryID, 64> IndustryPool;
extern IndustryPool _industry_pool;

static const TimerGameEconomy::Year PROCESSING_INDUSTRY_ABANDONMENT_YEARS{5}; ///< If a processing industry doesn't produce for this many consecutive economy years, it may close.

/*
 * Production level maximum, minimum and default values.
 * It is not a value been really used in order to change, but rather an indicator
 * of how the industry is behaving.
 */
static constexpr uint8_t PRODLEVEL_CLOSURE = 0x00; ///< signal set to actually close the industry
static constexpr uint8_t PRODLEVEL_MINIMUM = 0x04; ///< below this level, the industry is set to be closing
static constexpr uint8_t PRODLEVEL_DEFAULT = 0x10; ///< default level set when the industry is created
static constexpr uint8_t PRODLEVEL_MAXIMUM = 0x80; ///< the industry is running at full speed

/**
 * Flags to control/override the behaviour of an industry.
 * These flags are controlled by game scripts.
 */
enum class IndustryControlFlag : uint8_t {
	/** When industry production change is evaluated, rolls to decrease are ignored. */
	NoProductionDecrease = 0,
	/** When industry production change is evaluated, rolls to increase are ignored. */
	NoProductionIncrease = 1,
	/**
	 * Industry can not close regardless of production level or time since last delivery.
	 * This does not prevent a closure already announced. */
	NoClosure = 2,
	/** Indicates that the production level of the industry is externally controlled. */
	ExternalProdLevel = 3,
	End,
};
using IndustryControlFlags = EnumBitSet<IndustryControlFlag, uint8_t, IndustryControlFlag::End>;

/**
 * Defines the internal data of a functional industry.
 */
struct Industry : IndustryPool::PoolItem<&_industry_pool> {
	struct ProducedHistory {
		uint16_t production = 0; ///< Total produced
		uint16_t transported = 0; ///< Total transported

		uint8_t PctTransported() const
		{
			if (this->production == 0) return 0;
			return ClampTo<uint8_t>(this->transported * 256 / this->production);
		}
	};
	struct ProducedCargo {
		CargoType cargo = 0; ///< Cargo type
		uint16_t waiting = 0; ///< Amount of cargo produced
		uint8_t rate = 0; ///< Production rate
		HistoryData<ProducedHistory> history{}; ///< History of cargo produced and transported for this month and 24 previous months
	};

	struct AcceptedHistory {
		uint16_t accepted = 0; /// Total accepted.
		uint16_t waiting = 0; /// Average waiting.
	};

	struct AcceptedCargo {
		CargoType cargo = 0; ///< Cargo type
		uint16_t waiting = 0; ///< Amount of cargo waiting to processed
		uint32_t accumulated_waiting = 0; ///< Accumulated waiting total over the last month, used to calculate average.
		TimerGameEconomy::Date last_accepted{}; ///< Last day cargo was accepted by this industry
		std::unique_ptr<HistoryData<AcceptedHistory>> history{}; ///< History of accepted and waiting cargo.

		/**
		 * Get history data, creating it if necessary.
		 * @return Accepted history data.
		 */
		inline HistoryData<AcceptedHistory> &GetOrCreateHistory()
		{
			if (this->history == nullptr) this->history = std::make_unique<HistoryData<AcceptedHistory>>();
			return *this->history;
		}
	};

	using ProducedCargoes = std::vector<ProducedCargo>;
	using AcceptedCargoes = std::vector<AcceptedCargo>;

	TileArea location{INVALID_TILE, 0, 0}; ///< Location of the industry
	Town *town = nullptr; ///< Nearest town
	Station *neutral_station = nullptr; ///< Associated neutral station
	ValidHistoryMask valid_history = 0; ///< Mask of valid history records.
	ProducedCargoes produced{}; ///< produced cargo slots
	AcceptedCargoes accepted{}; ///< accepted cargo slots
	uint8_t prod_level = 0; ///< general production level
	uint16_t counter = 0; ///< used for animation and/or production (if available cargo)

	IndustryType type = 0; ///< type of industry.
	Owner owner = INVALID_OWNER; ///< owner of the industry.  Which SHOULD always be (imho) OWNER_NONE
	Colours random_colour = COLOUR_BEGIN; ///< randomized colour of the industry, for display purpose
	TimerGameEconomy::Year last_prod_year{}; ///< last economy year of production
	uint8_t was_cargo_delivered = 0; ///< flag that indicate this has been the closest industry chosen for cargo delivery by a station. see DeliverGoodsToIndustry
	IndustryControlFlags ctlflags{}; ///< flags overriding standard behaviours

	PartsOfSubsidy part_of_subsidy{}; ///< NOSAVE: is this industry a source/destination of a subsidy?
	StationList stations_near{}; ///< NOSAVE: List of nearby stations.
	mutable std::string cached_name{}; ///< NOSAVE: Cache of the resolved name of the industry

	Owner founder = INVALID_OWNER; ///< Founder of the industry
	TimerGameCalendar::Date construction_date{}; ///< Date of the construction of the industry
	uint8_t construction_type = 0; ///< Way the industry was constructed (@see IndustryConstructionType)
	uint8_t selected_layout = 0; ///< Which tile layout was used when creating the industry
	Owner exclusive_supplier = INVALID_OWNER; ///< Which company has exclusive rights to deliver cargo (INVALID_OWNER = anyone)
	Owner exclusive_consumer = INVALID_OWNER; ///< Which company has exclusive rights to take cargo (INVALID_OWNER = anyone)
	EncodedString text{}; ///< General text with additional information.

	uint16_t random = 0; ///< Random value used for randomisation of all kinds of things

	PersistentStorage *psa = nullptr; ///< Persistent storage for NewGRF industries.

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
	 * Safely get a produced cargo slot, or an empty data if the slot does not exist.
	 * @param slot produced cargo slot to retrieve.
	 * @return the real slot, or an empty slot.
	 */
	inline const ProducedCargo &GetProduced(size_t slot) const
	{
		static const ProducedCargo empty{INVALID_CARGO, 0, 0, {}};
		return slot < this->produced.size() ? this->produced[slot] : empty;
	}

	/**
	 * Safely get an accepted cargo slot, or an empty data if the slot does not exist.
	 * @param slot accepted cargo slot to retrieve.
	 * @return the real slot, or an empty slot.
	 */
	inline const AcceptedCargo &GetAccepted(size_t slot) const
	{
		static const AcceptedCargo empty{INVALID_CARGO, 0, 0, {}, {}};
		return slot < this->accepted.size() ? this->accepted[slot] : empty;
	}

	/**
	 * Get produced cargo slot for a specific cargo type.
	 * @param cargo CargoType to find.
	 * @return Iterator pointing to produced cargo slot if it exists, or the end iterator.
	 */
	inline ProducedCargoes::iterator GetCargoProduced(CargoType cargo)
	{
		if (!IsValidCargoType(cargo)) return std::end(this->produced);
		return std::ranges::find(this->produced, cargo, &ProducedCargo::cargo);
	}

	/**
	 * Get produced cargo slot for a specific cargo type (const-variant).
	 * @param cargo CargoType to find.
	 * @return Iterator pointing to produced cargo slot if it exists, or the end iterator.
	 */
	inline ProducedCargoes::const_iterator GetCargoProduced(CargoType cargo) const
	{
		if (!IsValidCargoType(cargo)) return std::end(this->produced);
		return std::ranges::find(this->produced, cargo, &ProducedCargo::cargo);
	}

	/**
	 * Get accepted cargo slot for a specific cargo type.
	 * @param cargo CargoType to find.
	 * @return Iterator pointing to accepted cargo slot if it exists, or the end iterator.
	 */
	inline AcceptedCargoes::iterator GetCargoAccepted(CargoType cargo)
	{
		if (!IsValidCargoType(cargo)) return std::end(this->accepted);
		return std::ranges::find(this->accepted, cargo, &AcceptedCargo::cargo);
	}

	/**
	 * Get accepted cargo slot for a specific cargo type (const-variant).
	 * @param cargo CargoType to find.
	 * @return Iterator pointing to accepted cargo slot if it exists, or the end iterator.
	 */
	inline AcceptedCargoes::const_iterator GetCargoAccepted(CargoType cargo) const
	{
		if (!IsValidCargoType(cargo)) return std::end(this->accepted);
		return std::ranges::find(this->accepted, cargo, &AcceptedCargo::cargo);
	}

	/**
	 * Test if this industry accepts any cargo.
	 * @return true iff the industry accepts any cargo.
	 */
	bool IsCargoAccepted() const { return std::any_of(std::begin(this->accepted), std::end(this->accepted), [](const auto &a) { return IsValidCargoType(a.cargo); }); }

	/**
	 * Test if this industry produces any cargo.
	 * @return true iff the industry produces any cargo.
	 */
	bool IsCargoProduced() const { return std::any_of(std::begin(this->produced), std::end(this->produced), [](const auto &p) { return IsValidCargoType(p.cargo); }); }

	/**
	 * Test if this industry accepts a specific cargo.
	 * @param cargo Cargo type to test.
	 * @return true iff the industry accepts the given cargo type.
	 */
	bool IsCargoAccepted(CargoType cargo) const { return std::any_of(std::begin(this->accepted), std::end(this->accepted), [&cargo](const auto &a) { return a.cargo == cargo; }); }

	/**
	 * Test if this industry produces a specific cargo.
	 * @param cargo Cargo type to test.
	 * @return true iff the industry produces the given cargo types.
	 */
	bool IsCargoProduced(CargoType cargo) const { return std::any_of(std::begin(this->produced), std::end(this->produced), [&cargo](const auto &p) { return p.cargo == cargo; }); }

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
	 * Get the count of industries for this type.
	 * @param type IndustryType to query
	 * @pre type < NUM_INDUSTRYTYPES
	 */
	static inline uint16_t GetIndustryTypeCount(IndustryType type)
	{
		assert(type < NUM_INDUSTRYTYPES);
		return static_cast<uint16_t>(std::size(industries[type]));
	}

	inline const std::string &GetCachedName() const
	{
		if (this->cached_name.empty()) this->FillCachedName();
		return this->cached_name;
	}

	static std::array<FlatSet<IndustryID>, NUM_INDUSTRYTYPES> industries; ///< List of industries of each type.

private:
	void FillCachedName() const;
};

void ClearAllIndustryCachedNames();

void PlantRandomFarmField(const Industry *i);

void ReleaseDisastersTargetingIndustry(IndustryID);

bool IsTileForestIndustry(TileIndex tile);

/** Data for managing the number of industries of a single industry type. */
struct IndustryTypeBuildData {
	uint32_t probability;  ///< Relative probability of building this industry.
	uint8_t   min_number;   ///< Smallest number of industries that should exist (either \c 0 or \c 1).
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

	void EconomyMonthlyLoop();
};

extern IndustryBuildData _industry_builder;


/** Special values for the industry list window for the data parameter of #InvalidateWindowData. */
enum IndustryDirectoryInvalidateWindowData : uint8_t {
	IDIWD_FORCE_REBUILD,
	IDIWD_PRODUCTION_CHANGE,
	IDIWD_FORCE_RESORT,
};

void TrimIndustryAcceptedProduced(Industry *ind);

#endif /* INDUSTRY_H */
