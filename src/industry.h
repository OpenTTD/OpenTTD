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
#include "timer/timer_game_economy.h"
#include "industry_kdtree.h"
#include "town.h"


typedef Pool<Industry, IndustryID, 64, 64000> IndustryPool;
extern IndustryPool _industry_pool;

static const TimerGameEconomy::Year PROCESSING_INDUSTRY_ABANDONMENT_YEARS = 5; ///< If a processing industry doesn't produce for this many consecutive economy years, it may close.

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
enum IndustryControlFlags : uint8_t {
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
		std::array<ProducedHistory, 25> history; ///< History of cargo produced and transported for this month and 24 previous months
	};

	struct AcceptedCargo {
		CargoID cargo; ///< Cargo type
		uint16_t waiting; ///< Amount of cargo waiting to processed
		TimerGameEconomy::Date last_accepted; ///< Last day cargo was accepted by this industry
	};

	using ProducedCargoes = std::vector<ProducedCargo>;
	using AcceptedCargoes = std::vector<AcceptedCargo>;

	TileArea location;                                     ///< Location of the industry
	Town *town;                                            ///< Nearest town
	Station *neutral_station;                              ///< Associated neutral station
	ProducedCargoes produced; ///< produced cargo slots
	AcceptedCargoes accepted; ///< accepted cargo slots
	uint8_t prod_level;                                       ///< general production level
	uint16_t counter;                                        ///< used for animation and/or production (if available cargo)

	IndustryType type;             ///< type of industry.
	Owner owner;                   ///< owner of the industry.  Which SHOULD always be (imho) OWNER_NONE
	Colours random_colour;         ///< randomized colour of the industry, for display purpose
	TimerGameEconomy::Year last_prod_year; ///< last economy year of production
	uint8_t was_cargo_delivered;      ///< flag that indicate this has been the closest industry chosen for cargo delivery by a station. see DeliverGoodsToIndustry
	IndustryControlFlags ctlflags; ///< flags overriding standard behaviours

	PartOfSubsidy part_of_subsidy; ///< NOSAVE: is this industry a source/destination of a subsidy?
	StationList stations_near;     ///< NOSAVE: List of nearby stations.
	mutable std::string cached_name; ///< NOSAVE: Cache of the resolved name of the industry

	Owner founder;                 ///< Founder of the industry
	TimerGameCalendar::Date construction_date; ///< Date of the construction of the industry
	uint8_t construction_type;       ///< Way the industry was constructed (@see IndustryConstructionType)
	uint8_t selected_layout;          ///< Which tile layout was used when creating the industry
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
		static const AcceptedCargo empty{INVALID_CARGO, 0, {}};
		return slot < this->accepted.size() ? this->accepted[slot] : empty;
	}

	/**
	 * Get produced cargo slot for a specific cargo type.
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to produced cargo slot if it exists, or the end iterator.
	 */
	inline ProducedCargoes::iterator GetCargoProduced(CargoID cargo)
	{
		if (!IsValidCargoID(cargo)) return std::end(this->produced);
		return std::find_if(std::begin(this->produced), std::end(this->produced), [&cargo](const auto &p) { return p.cargo == cargo; });
	}

	/**
	 * Get produced cargo slot for a specific cargo type (const-variant).
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to produced cargo slot if it exists, or the end iterator.
	 */
	inline ProducedCargoes::const_iterator GetCargoProduced(CargoID cargo) const
	{
		if (!IsValidCargoID(cargo)) return std::end(this->produced);
		return std::find_if(std::begin(this->produced), std::end(this->produced), [&cargo](const auto &p) { return p.cargo == cargo; });
	}

	/**
	 * Get accepted cargo slot for a specific cargo type.
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to accepted cargo slot if it exists, or the end iterator.
	 */
	inline AcceptedCargoes::iterator GetCargoAccepted(CargoID cargo)
	{
		if (!IsValidCargoID(cargo)) return std::end(this->accepted);
		return std::find_if(std::begin(this->accepted), std::end(this->accepted), [&cargo](const auto &a) { return a.cargo == cargo; });
	}

	/**
	 * Get accepted cargo slot for a specific cargo type (const-variant).
	 * @param cargo CargoID to find.
	 * @return Iterator pointing to accepted cargo slot if it exists, or the end iterator.
	 */
	inline AcceptedCargoes::const_iterator GetCargoAccepted(CargoID cargo) const
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
	 * Struct representing the cache of industries for a specific town.
	 */
	struct IndustryTownCache {
		TownID town_id;                       ///< The ID of the town.
		std::vector<IndustryID> industry_ids; ///< A vector of IDs of the industries associated with the town.
	};

	/**
	 * Struct representing the cache of industry counts for a specific industry type.
	 */
	struct IndustryTypeCountCaches {
		IndustryKdtree kdtree;                ///< A k-d tree for spatial indexing of industries.
		std::vector<IndustryTownCache> towns; ///< A vector of IndustryTownCache entries, each representing a town and its associated industries.
	};

	/**
	 * Increment the count of industries of the specified type in a town.
	 *
	 * This function updates the k-d tree by inserting the industry's index and adjusts the
	 * industry counts for the specific industry type in the corresponding town. If the industry
	 * is not already in the town's list, it is added in the correct sorted position.
	 */
	inline void IncIndustryTypeCount()
	{
		auto &kdtree = this->counts[this->type].kdtree;
		kdtree.Insert(this->index);

		/* Find the correct position to insert or update the town entry using lower_bound. */
		auto &type_vector = this->counts[this->type].towns;
		auto pair_it = std::ranges::lower_bound(type_vector, this->town->index, {}, &IndustryTownCache::town_id);

		if (pair_it != std::end(type_vector) && pair_it->town_id == this->town->index) {
			/* Create a reference to the town's industry list. */
			auto &iid_vector = pair_it->industry_ids;

			/* Ensure the town's industry list is not empty. */
			assert(!std::empty(iid_vector));

			/* Ensure the industry is not already in the town's industry list. */
			assert(std::ranges::all_of(iid_vector, [&](auto &iid) {
				return iid != this->index;
			}));

			/* Find the correct position to insert the industry ID in sorted order. */
			auto iid_it = std::ranges::lower_bound(iid_vector, this->index);

			/* Add the industry ID to the town's industry list in the correct position. */
			iid_vector.insert(iid_it, this->index);
		} else {
			/* Create a new vector for the industry IDs and add the industry ID. */
			std::vector<IndustryID> iid_vector;
			iid_vector.emplace_back(this->index);

			/* Insert the new entry (town index and vector of industry IDs) into the correct position. */
			type_vector.emplace(pair_it, IndustryTownCache{ this->town->index, std::move(iid_vector) });
		}
	}

	/**
	 * Decrement the count of industries of the specified type in a town.
	 *
	 * This function updates the k-d tree by removing the industry's index and adjusts the
	 * industry counts for the specific industry type in the corresponding town. If the town's
	 * industry list becomes empty after removal, the town entry is removed from the list.
	 */
	inline void DecIndustryTypeCount()
	{
		auto &kdtree = this->counts[this->type].kdtree;
		kdtree.Remove(this->index);

		/* Find the correct position of the town entry using lower_bound. */
		auto &type_vector = this->counts[this->type].towns;
		auto pair_it = std::ranges::lower_bound(type_vector, this->town->index, {}, &IndustryTownCache::town_id);

		/* Ensure the pair was found in the array. */
		assert(pair_it != std::end(type_vector) && pair_it->town_id == this->town->index);

		/* Create a reference to the town's industry list. */
		auto &iid_vector = pair_it->industry_ids;

		/* Ensure the town's industry list is not empty. */
		assert(!std::empty(iid_vector));

		/* Find the industry ID within the town's industry list. */
		auto iid_it = std::ranges::lower_bound(iid_vector, this->index);

		/* Ensure the industry ID was found in the list. */
		assert(iid_it != std::end(iid_vector) && *iid_it == this->index);

		/* Erase the industry ID from the town's industry list. */
		iid_vector.erase(iid_it);

		/* If the town's industry list is now empty, erase the town from the counts array. */
		if (std::empty(iid_vector)) {
			type_vector.erase(pair_it);
		}
	}

private:
	/**
	 * Applies the function to each industry and counts those that satisfy the function.
	 * Also verifies that all industries belong to the specified town.
	 *
	 * @param industries Vector of IndustryIDs to be processed.
	 * @param town Pointer to the Town for validation.
	 * @param return_early Return as soon as the count is different than zero.
	 * @param func Function to apply to each industry.
	 * @return The count of industries that satisfy the function.
	 */
	template<typename Func>
	static inline uint16_t ProcessIndustries(const std::vector<IndustryID> &industries, [[maybe_unused]] const Town *town, bool return_early, Func func)
	{
		uint16_t count = 0;

		for (const auto &iid : industries) {
			const Industry *ind = Industry::Get(iid);

			/* Verify the industry belongs to the specified town. */
			assert(ind->town == town);

			if (func(ind)) count++;
			if (return_early && count != 0) break;
		}

		return count;
	}

public:
	/**
	 * Get the count of industries for a specified type in a town or in all towns.
	 *
	 * @param type IndustryType to query.
	 * @param town Town to query, or nullptr to query all towns.
	 * @param return_early Return as soon as the count is different than zero.
	 * @param func Function to apply to each industry.
	 * @return The count of industries for the specified type.
	 * @pre type < NUM_INDUSTRYTYPES
	 */
	template<typename Func>
	static inline uint16_t CountTownIndustriesOfTypeMatchingCondition(IndustryType type, const Town *town, bool return_early, Func func)
	{
		assert(type < NUM_INDUSTRYTYPES);

		uint16_t count = 0;
		if (town != nullptr) {
			/* Find the correct position of the town entry using lower_bound. */
			auto &type_vector = counts[type].towns;
			auto pair_it = std::ranges::lower_bound(type_vector, town->index, {}, &IndustryTownCache::town_id);

			if (pair_it != std::end(type_vector) && pair_it->town_id == town->index) {
				/* Create a reference to the town's industry list. */
				auto &iid_vector = pair_it->industry_ids;

				/* Ensure the town's industry list is not empty. */
				assert(!std::empty(iid_vector));
				count = ProcessIndustries(iid_vector, town, return_early, func);
			}
		} else {
			/* Count industries in all towns. */
			for (auto &pair : counts[type].towns) {
				/* Create a reference to the town's industry list. */
				auto &iid_vector = pair.industry_ids;

				/* Ensure the town's industry list is not empty. */
				assert(!std::empty(iid_vector));
				count += ProcessIndustries(iid_vector, Town::Get(pair.town_id), return_early, func);
				if (return_early && count != 0) break;
			}
		}

		return count;
	}

	/**
	 * Get the count of industries for a specified type.
	 *
	 * @param type The type of industry to count.
	 * @return uint16_t The count of industries of the specified type.
	 */
	static uint16_t GetIndustryTypeCount(IndustryType type)
	{
		uint16_t count = static_cast<uint16_t>(counts[type].kdtree.Count());

		/* Sanity check. Both the Kdtree and the count of industries in all towns should match. */
		assert(count == CountTownIndustriesOfTypeMatchingCondition(type, nullptr, false, [](const Industry *) { return true; }));
		return count;
	}

	/**
	 * Check if a town has an industry of the specified type.
	 *
	 * @param type The type of industry to check for.
	 * @param town Pointer to the town to check.
	 * @return bool True if the town has an industry of the specified type, false otherwise.
	 */
	static inline bool HasTownIndustryOfType(IndustryType type, const Town *town)
	{
		assert(town != nullptr);

		return CountTownIndustriesOfTypeMatchingCondition(type, town, true, [](const Industry *) { return true; }) != 0;
	}

	/**
	 * Resets the industry counts for all industry types.
	 *
	 * Clears the vector of IndustryTownCache entries for each industry type,
	 * effectively resetting the count of industries per type in all towns.
	 */
	static inline void ResetIndustryCounts()
	{
		/* Clear the vector for each industry type in the counts array. */
		std::ranges::for_each(counts, [](auto &type) { type.towns.clear(); });
	}

	/**
	 * Rebuilds the k-d tree for each industry type.
	 *
	 * Iterates over all industries, categorizes them by type, and rebuilds the
	 * k-d tree for each industry type using the collected industry IDs.
	 */
	static inline void RebuildIndustryKdtree()
	{
		std::array<std::vector<IndustryID>, NUM_INDUSTRYTYPES> industryids;
		for (const Industry *industry : Industry::Iterate()) {
			industryids[industry->type].push_back(industry->index);
		}

		for (IndustryType type = 0; type < NUM_INDUSTRYTYPES; type++) {
			counts[type].kdtree.Build(industryids[type].begin(), industryids[type].end());
		}
	}

	/**
	 * Find all industries within a specified radius of a given tile.
	 *
	 * This function calculates the rectangular area around the given tile, constrained by the map boundaries,
	 * and finds all industries contained within this area using the k-d tree.
	 *
	 * @param tile The central tile index.
	 * @param type The industry type to search for.
	 * @param radius The search radius around the tile.
	 * @return std::vector<IndustryID> A vector of industry IDs found within the specified area.
	 */
	static inline std::vector<IndustryID> FindContained(TileIndex tile, IndustryType type, int radius)
	{
		static uint16_t x1, x2, y1, y2;
		x1 = (uint16_t)std::max<int>(0, TileX(tile) - radius);
		x2 = (uint16_t)std::min<int>(TileX(tile) + radius + 1, Map::SizeX());
		y1 = (uint16_t)std::max<int>(0, TileY(tile) - radius);
		y2 = (uint16_t)std::min<int>(TileY(tile) + radius + 1, Map::SizeY());

		return counts[type].kdtree.FindContained(x1, y1, x2, y2);
	}

	/**
	 * Find the industry nearest to a given tile.
	 *
	 * This function uses the k-d tree to find the industry of the specified type
	 * that is closest to the given tile based on its coordinates.
	 *
	 * @param tile The tile index to search from.
	 * @param type The industry type to search for.
	 * @return IndustryID The ID of the nearest industry.
	 */
	static inline IndustryID FindNearest(TileIndex tile, IndustryType type)
	{
		return counts[type].kdtree.FindNearest(TileX(tile), TileY(tile));
	}

	/**
	 * Find the industry nearest to a given tile, excluding a specified industry.
	 *
	 * This function uses the k-d tree to find the industry of the specified type
	 * that is closest to the given tile, excluding the industry with the given ID.
	 *
	 * @param tile The tile index to search from.
	 * @param type The industry type to search for.
	 * @param iid The ID of the industry to exclude from the search.
	 * @return IndustryID The ID of the nearest industry, excluding the specified one.
	 */
	static inline IndustryID FindNearestExcept(TileIndex tile, IndustryType type, IndustryID iid)
	{
		return counts[type].kdtree.FindNearestExcept(TileX(tile), TileY(tile), iid);
	}

	inline const std::string &GetCachedName() const
	{
		if (this->cached_name.empty()) this->FillCachedName();
		return this->cached_name;
	}

private:
	void FillCachedName() const;

protected:
	/**
	 * Array containing data for each industry type.
	 * Each element corresponds to a specific IndustryType and contains:
	 * - An IndustryKdtree for spatial indexing.
	 * - A vector of IndustryTownCache entries, where each entry holds:
	 *   - A TownID representing a town.
	 *   - A vector of IndustryIDs associated with that town.
	 */
	static std::array<IndustryTypeCountCaches, NUM_INDUSTRYTYPES> counts;
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
enum IndustryDirectoryInvalidateWindowData {
	IDIWD_FORCE_REBUILD,
	IDIWD_PRODUCTION_CHANGE,
	IDIWD_FORCE_RESORT,
};

void TrimIndustryAcceptedProduced(Industry *ind);

#endif /* INDUSTRY_H */
