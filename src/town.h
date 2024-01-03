/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town.h Base of the town class. */

#ifndef TOWN_H
#define TOWN_H

#include "viewport_type.h"
#include "timer/timer_game_tick.h"
#include "town_map.h"
#include "subsidy_type.h"
#include "newgrf_storage.h"
#include "cargotype.h"

template <typename T>
struct BuildingCounts {
	T id_count[NUM_HOUSES];
	T class_count[HOUSE_CLASS_MAX];
};

static const uint CUSTOM_TOWN_NUMBER_DIFFICULTY  = 4; ///< value for custom town number in difficulty settings
static const uint CUSTOM_TOWN_MAX_NUMBER = 5000;  ///< this is the maximum number of towns a user can specify in customisation

static const TownID INVALID_TOWN = 0xFFFF;

static const uint TOWN_GROWTH_WINTER = 0xFFFFFFFE; ///< The town only needs this cargo in the winter (any amount)
static const uint TOWN_GROWTH_DESERT = 0xFFFFFFFF; ///< The town needs the cargo for growth when on desert (any amount)
static const uint16_t TOWN_GROWTH_RATE_NONE = 0xFFFF; ///< Special value for Town::growth_rate to disable town growth.
static const uint16_t MAX_TOWN_GROWTH_TICKS = 930; ///< Max amount of original town ticks that still fit into uint16_t, about equal to UINT16_MAX / TOWN_GROWTH_TICKS but slightly less to simplify calculations

typedef Pool<Town, TownID, 64, 64000> TownPool;
extern TownPool _town_pool;

/** Data structure with cached data of towns. */
struct TownCache {
	uint32_t num_houses;                        ///< Amount of houses
	uint32_t population;                        ///< Current population of people
	TrackedViewportSign sign;                 ///< Location of name sign, UpdateVirtCoord updates this
	PartOfSubsidy part_of_subsidy;            ///< Is this town a source/destination of a subsidy?
	uint32_t squared_town_zone_radius[HZB_END]; ///< UpdateTownRadius updates this given the house count
	BuildingCounts<uint16_t> building_counts;   ///< The number of each type of building in the town
};

/** Town data structure. */
struct Town : TownPool::PoolItem<&_town_pool> {
	TileIndex xy;                  ///< town center tile

	TownCache cache; ///< Container for all cacheable data.

	/* Town name */
	uint32_t townnamegrfid;
	uint16_t townnametype;
	uint32_t townnameparts;
	std::string name;                ///< Custom town name. If empty, the town was not renamed and uses the generated name.
	mutable std::string cached_name; ///< NOSAVE: Cache of the resolved name of the town, if not using a custom town name

	byte flags;                    ///< See #TownFlags.

	uint16_t noise_reached;          ///< level of noise that all the airports are generating

	CompanyMask statues;           ///< which companies have a statue?

	/* Company ratings. */
	CompanyMask have_ratings;      ///< which companies have a rating
	uint8_t unwanted[MAX_COMPANIES]; ///< how many months companies aren't wanted by towns (bribe)
	CompanyID exclusivity;         ///< which company has exclusivity
	uint8_t exclusive_counter;       ///< months till the exclusivity expires
	int16_t ratings[MAX_COMPANIES];  ///< ratings of each company for this town

	TransportedCargoStat<uint32_t> supplied[NUM_CARGO]; ///< Cargo statistics about supplied cargo.
	TransportedCargoStat<uint16_t> received[NUM_TE];    ///< Cargo statistics about received cargotypes.
	uint32_t goal[NUM_TE];                              ///< Amount of cargo required for the town to grow.

	std::string text; ///< General text with additional information.

	inline byte GetPercentTransported(CargoID cid) const { return this->supplied[cid].old_act * 256 / (this->supplied[cid].old_max + 1); }

	StationList stations_near;       ///< NOSAVE: List of nearby stations.

	uint16_t time_until_rebuild;       ///< time until we rebuild a house

	uint16_t grow_counter;             ///< counter to count when to grow, value is smaller than or equal to growth_rate
	uint16_t growth_rate;              ///< town growth rate

	byte fund_buildings_months;      ///< fund buildings program in action?
	byte road_build_months;          ///< fund road reconstruction in action?

	bool larger_town;                ///< if this is a larger town and should grow more quickly
	TownLayout layout;               ///< town specific road layout

	bool show_zone;                  ///< NOSAVE: mark town to show the local authority zone in the viewports

	std::list<PersistentStorage *> psa_list;

	/**
	 * Creates a new town.
	 * @param tile center tile of the town
	 */
	Town(TileIndex tile = INVALID_TILE) : xy(tile) { }

	/** Destroy the town. */
	~Town();

	void InitializeLayout(TownLayout layout);

	/**
	 * Calculate the max town noise.
	 * The value is counted using the population divided by the content of the
	 * entry in town_noise_population corresponding to the town's tolerance.
	 * @return the maximum noise level the town will tolerate.
	 */
	inline uint16_t MaxTownNoise() const
	{
		if (this->cache.population == 0) return 0; // no population? no noise

		/* 3 is added (the noise of the lowest airport), so the  user can at least build a small airfield. */
		return ClampTo<uint16_t>((this->cache.population / _settings_game.economy.town_noise_population[_settings_game.difficulty.town_council_tolerance]) + 3);
	}

	void UpdateVirtCoord();

	inline const std::string &GetCachedName() const
	{
		if (!this->name.empty()) return this->name;
		if (this->cached_name.empty()) this->FillCachedName();
		return this->cached_name;
	}

	static inline Town *GetByTile(TileIndex tile)
	{
		return Town::Get(GetTownIndex(tile));
	}

	static Town *GetRandom();
	static void PostDestructor(size_t index);

private:
	void FillCachedName() const;
};

uint32_t GetWorldPopulation();

void UpdateAllTownVirtCoords();
void ClearAllTownCachedNames();
void ShowTownViewWindow(TownID town);
void ExpandTown(Town *t);

void RebuildTownKdtree();

/** Settings for town council attitudes. */
enum TownCouncilAttitudes {
	TOWN_COUNCIL_LENIENT    = 0,
	TOWN_COUNCIL_TOLERANT   = 1,
	TOWN_COUNCIL_HOSTILE    = 2,
	TOWN_COUNCIL_PERMISSIVE = 3,
};

/**
 * Action types that a company must ask permission for to a town authority.
 * @see CheckforTownRating
 */
enum TownRatingCheckType {
	ROAD_REMOVE         = 0,      ///< Removal of a road owned by the town.
	TUNNELBRIDGE_REMOVE = 1,      ///< Removal of a tunnel or bridge owned by the towb.
	TOWN_RATING_CHECK_TYPE_COUNT, ///< Number of town checking action types.
};

/** Special values for town list window for the data parameter of #InvalidateWindowData. */
enum TownDirectoryInvalidateWindowData {
	TDIWD_FORCE_REBUILD,
	TDIWD_POPULATION_CHANGE,
	TDIWD_FORCE_RESORT,
};

/**
 * This enum is used in conjunction with town->flags.
 * IT simply states what bit is used for.
 * It is pretty unrealistic (IMHO) to only have one church/stadium
 * per town, NO MATTER the population of it.
 * And there are 5 more bits available on flags...
 */
enum TownFlags {
	TOWN_IS_GROWING     = 0,   ///< Conditions for town growth are met. Grow according to Town::growth_rate.
	TOWN_HAS_CHURCH     = 1,   ///< There can be only one church by town.
	TOWN_HAS_STADIUM    = 2,   ///< There can be only one stadium by town.
	TOWN_CUSTOM_GROWTH  = 3,   ///< Growth rate is controlled by GS.
};

CommandCost CheckforTownRating(DoCommandFlag flags, Town *t, TownRatingCheckType type);


TileIndexDiff GetHouseNorthPart(HouseID &house);

Town *CalcClosestTownFromTile(TileIndex tile, uint threshold = UINT_MAX);

void ResetHouses();

/** Town actions of a company. */
enum TownActions {
	TACT_NONE             = 0x00, ///< Empty action set.

	TACT_ADVERTISE_SMALL  = 0x01, ///< Small advertising campaign.
	TACT_ADVERTISE_MEDIUM = 0x02, ///< Medium advertising campaign.
	TACT_ADVERTISE_LARGE  = 0x04, ///< Large advertising campaign.
	TACT_ROAD_REBUILD     = 0x08, ///< Rebuild the roads.
	TACT_BUILD_STATUE     = 0x10, ///< Build a statue.
	TACT_FUND_BUILDINGS   = 0x20, ///< Fund new buildings.
	TACT_BUY_RIGHTS       = 0x40, ///< Buy exclusive transport rights.
	TACT_BRIBE            = 0x80, ///< Try to bribe the council.

	TACT_COUNT            = 8,    ///< Number of available town actions.

	TACT_ADVERTISE        = TACT_ADVERTISE_SMALL | TACT_ADVERTISE_MEDIUM | TACT_ADVERTISE_LARGE, ///< All possible advertising actions.
	TACT_CONSTRUCTION     = TACT_ROAD_REBUILD | TACT_BUILD_STATUE | TACT_FUND_BUILDINGS,         ///< All possible construction actions.
	TACT_FUNDS            = TACT_BUY_RIGHTS | TACT_BRIBE,                                        ///< All possible funding actions.
	TACT_ALL              = TACT_ADVERTISE | TACT_CONSTRUCTION | TACT_FUNDS,                     ///< All possible actions.
};
DECLARE_ENUM_AS_BIT_SET(TownActions)

void ClearTownHouse(Town *t, TileIndex tile);
void UpdateTownMaxPass(Town *t);
void UpdateTownRadius(Town *t);
CommandCost CheckIfAuthorityAllowsNewStation(TileIndex tile, DoCommandFlag flags);
Town *ClosestTownFromTile(TileIndex tile, uint threshold);
void ChangeTownRating(Town *t, int add, int max, DoCommandFlag flags);
HouseZonesBits GetTownRadiusGroup(const Town *t, TileIndex tile);
void SetTownRatingTestMode(bool mode);
TownActions GetMaskOfTownActions(CompanyID cid, const Town *t);
bool GenerateTowns(TownLayout layout);
const CargoSpec *FindFirstCargoWithTownEffect(TownEffect effect);

extern const byte _town_action_costs[TACT_COUNT];

/**
 * Set the default name for a depot/waypoint
 * @tparam T The type/class to make a default name for
 * @param obj The object/instance we want to find the name for
 */
template <class T>
void MakeDefaultName(T *obj)
{
	/* We only want to set names if it hasn't been set before, or when we're calling from afterload. */
	assert(obj->name.empty() || obj->town_cn == UINT16_MAX);

	obj->town = ClosestTownFromTile(obj->xy, UINT_MAX);

	/* Find first unused number belonging to this town. This can never fail,
	 * as long as there can be at most 65535 waypoints/depots in total.
	 *
	 * This does 'n * m' search, but with 32bit 'used' bitmap, it needs at
	 * most 'n * (1 + ceil(m / 32))' steps (n - number of waypoints in pool,
	 * m - number of waypoints near this town).
	 * Usually, it needs only 'n' steps.
	 *
	 * If it wasn't using 'used' and 'idx', it would just search for increasing 'next',
	 * but this way it is faster */

	uint32_t used = 0; // bitmap of used waypoint numbers, sliding window with 'next' as base
	uint32_t next = 0; // first number in the bitmap
	uint32_t idx  = 0; // index where we will stop
	uint32_t cid  = 0; // current index, goes to T::GetPoolSize()-1, then wraps to 0

	do {
		T *lobj = T::GetIfValid(cid);

		/* check only valid waypoints... */
		if (lobj != nullptr && obj != lobj) {
			/* only objects within the same city and with the same type */
			if (lobj->town == obj->town && lobj->IsOfType(obj)) {
				/* if lobj->town_cn < next, uint will overflow to '+inf' */
				uint i = (uint)lobj->town_cn - next;

				if (i < 32) {
					SetBit(used, i); // update bitmap
					if (i == 0) {
						/* shift bitmap while the lowest bit is '1';
						 * increase the base of the bitmap too */
						do {
							used >>= 1;
							next++;
						} while (HasBit(used, 0));
						/* when we are at 'idx' again at end of the loop and
						 * 'next' hasn't changed, then no object had town_cn == next,
						 * so we can safely use it */
						idx = cid;
					}
				}
			}
		}

		cid++;
		if (cid == T::GetPoolSize()) cid = 0; // wrap to zero...
	} while (cid != idx);

	obj->town_cn = (uint16_t)next; // set index...
}

/*
 * Converts original town ticks counters to plain game ticks. Note that
 * tick 0 is a valid tick so actual amount is one more than the counter value.
 */
static inline uint16_t TownTicksToGameTicks(uint16_t ticks)
{
	return (std::min(ticks, MAX_TOWN_GROWTH_TICKS) + 1) * Ticks::TOWN_GROWTH_TICKS - 1;
}


RoadType GetTownRoadType();

#endif /* TOWN_H */
