/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town.h Base of the town class. */

#ifndef TOWN_H
#define TOWN_H

#include "core/pool_type.hpp"
#include "viewport_type.h"
#include "command_type.h"
#include "town_map.h"
#include "subsidy_type.h"

template <typename T>
struct BuildingCounts {
	T id_count[HOUSE_MAX];
	T class_count[HOUSE_CLASS_MAX];
};

static const uint CUSTOM_TOWN_NUMBER_DIFFICULTY  = 4; ///< value for custom town number in difficulty settings
static const uint CUSTOM_TOWN_MAX_NUMBER = 5000;  ///< this is the maximum number of towns a user can specify in customisation

static const uint INVALID_TOWN = 0xFFFF;

typedef Pool<Town, TownID, 64, 64000> TownPool;
extern TownPool _town_pool;

/** Town data structure. */
struct Town : TownPool::PoolItem<&_town_pool> {
	TileIndex xy;

	/* Current population of people and amount of houses. */
	uint32 num_houses;
	uint32 population;

	/* Town name */
	uint32 townnamegrfid;
	uint16 townnametype;
	uint32 townnameparts;
	char *name;

	/* NOSAVE: Location of name sign, UpdateVirtCoord updates this. */
	ViewportSign sign;

	/* Makes sure we don't build certain house types twice.
	 * bit 0 = Building funds received
	 * bit 1 = CHURCH
	 * bit 2 = STADIUM */
	byte flags;

	/* level of noise that all the airports are generating */
	uint16 noise_reached;

	/* Which companies have a statue? */
	CompanyMask statues;

	/* Company ratings as well as a mask that determines which companies have a rating. */
	CompanyMask have_ratings;
	uint8 unwanted[MAX_COMPANIES]; ///< how many months companies aren't wanted by towns (bribe)
	CompanyByte exclusivity;       ///< which company has exclusivity
	uint8 exclusive_counter;       ///< months till the exclusivity expires
	int16 ratings[MAX_COMPANIES];  ///< Ratings of each company for this town.

	/* Maximum amount of passengers and mail that can be transported. */
	uint32 max_pass;
	uint32 max_mail;
	uint32 new_max_pass;
	uint32 new_max_mail;
	uint32 act_pass;
	uint32 act_mail;
	uint32 new_act_pass;
	uint32 new_act_mail;

	/* Amount of passengers that were transported. */
	byte pct_pass_transported;
	byte pct_mail_transported;

	/* Amount of food and paper that was transported. Actually a bit mask would be enough. */
	uint16 act_food;
	uint16 act_water;
	uint16 new_act_food;
	uint16 new_act_water;

	/* Time until we rebuild a house. */
	uint16 time_until_rebuild;

	/* When to grow town next time. */
	uint16 grow_counter;
	int16 growth_rate;

	/* Fund buildings program in action? */
	byte fund_buildings_months;

	/* Fund road reconstruction in action? */
	byte road_build_months;

	/* If this is a larger town, and should grow more quickly. */
	bool larger_town;
	TownLayoutByte layout; ///< town specific road layout

	PartOfSubsidyByte part_of_subsidy; ///< NOSAVE: is this town a source/destination of a subsidy?

	/* NOSAVE: UpdateTownRadius updates this given the house count. */
	uint32 squared_town_zone_radius[HZB_END];

	BuildingCounts<uint16> building_counts; ///< NOSAVE: The number of each type of building in the town.

	/**
	 * Creates a new town
	 */
	Town(TileIndex tile = INVALID_TILE) : xy(tile) { }

	/** Destroy the town */
	~Town();

	void InitializeLayout(TownLayout layout);

	/**
	 * Calculate the max town noise.
	 * The value is counted using the population divided by the content of the
	 * entry in town_noise_population corresponding to the town's tolerance.
	 * @return the maximum noise level the town will tolerate.
	 */
	inline uint16 MaxTownNoise() const
	{
		if (this->population == 0) return 0; // no population? no noise

		/* 3 is added (the noise of the lowest airport), so the  user can at least build a small airfield. */
		return (this->population / _settings_game.economy.town_noise_population[_settings_game.difficulty.town_council_tolerance]) + 3;
	}

	void UpdateVirtCoord();

	static FORCEINLINE Town *GetByTile(TileIndex tile)
	{
		return Town::Get(GetTownIndex(tile));
	}

	static Town *GetRandom();
	static void PostDestructor(size_t index);
};

uint32 GetWorldPopulation();

void UpdateAllTownVirtCoords();
void ShowTownViewWindow(TownID town);
void ExpandTown(Town *t);

/**
 * Action types that a company must ask permission for to a town authority.
 * @see CheckforTownRating
 */
enum TownRatingCheckType {
	ROAD_REMOVE         = 0,      ///< Removal of a road owned by the town.
	TUNNELBRIDGE_REMOVE = 1,      ///< Removal of a tunnel or bridge owned by the towb.
	TOWN_RATING_CHECK_TYPE_COUNT, ///< Number of town checking action types.
};

/**
 * This enum is used in conjunction with town->flags.
 * IT simply states what bit is used for.
 * It is pretty unrealistic (IMHO) to only have one church/stadium
 * per town, NO MATTER the population of it.
 * And there are 5 more bits available on flags...
 */
enum TownFlags {
	TOWN_IS_FUNDED      = 0,   ///< Town has received some funds for
	TOWN_HAS_CHURCH     = 1,   ///< There can be only one church by town.
	TOWN_HAS_STADIUM    = 2    ///< There can be only one stadium by town.
};

CommandCost CheckforTownRating(DoCommandFlag flags, Town *t, TownRatingCheckType type);


TileIndexDiff GetHouseNorthPart(HouseID &house);

Town *CalcClosestTownFromTile(TileIndex tile, uint threshold = UINT_MAX);

#define FOR_ALL_TOWNS_FROM(var, start) FOR_ALL_ITEMS_FROM(Town, town_index, var, start)
#define FOR_ALL_TOWNS(var) FOR_ALL_TOWNS_FROM(var, 0)

void ResetHouses();

void ClearTownHouse(Town *t, TileIndex tile);
void UpdateTownMaxPass(Town *t);
void UpdateTownRadius(Town *t);
CommandCost CheckIfAuthorityAllowsNewStation(TileIndex tile, DoCommandFlag flags);
Town *ClosestTownFromTile(TileIndex tile, uint threshold);
void ChangeTownRating(Town *t, int add, int max, DoCommandFlag flags);
HouseZonesBits GetTownRadiusGroup(const Town *t, TileIndex tile);
void SetTownRatingTestMode(bool mode);
uint GetMaskOfTownActions(int *nump, CompanyID cid, const Town *t);
bool GenerateTowns(TownLayout layout);


/** Town actions of a company. */
enum TownActions {
	TACT_NONE             = 0x00, ///< Empty action set.

	TACT_ADVERTISE_SMALL  = 0x01, ///< Small advertising campaign.
	TACT_ADVERTISE_MEDIUM = 0x02, ///< Medium advertising campaign.
	TACT_ADVERTISE_LARGE  = 0x04, ///< Large advertising campaign.
	TACT_ROAD_REBUILD     = 0x08, ///< Rebuild the roads.
	TACT_BUILD_STATUE     = 0x10, ///< Build a statue.
	TACT_FOUND_BUILDINGS  = 0x20, ///< Found new buildings.
	TACT_BUY_RIGHTS       = 0x40, ///< Buy exclusive transport rights.
	TACT_BRIBE            = 0x80, ///< Try to bribe the counsil.

	TACT_COUNT            = 8,    ///< Number of available town actions.

	TACT_ADVERTISE        = TACT_ADVERTISE_SMALL | TACT_ADVERTISE_MEDIUM | TACT_ADVERTISE_LARGE, ///< All possible advertising actions.
	TACT_CONSTRUCTION     = TACT_ROAD_REBUILD | TACT_BUILD_STATUE | TACT_FOUND_BUILDINGS,        ///< All possible construction actions.
	TACT_FUNDS            = TACT_BUY_RIGHTS | TACT_BRIBE,                                        ///< All possible funding actions.
	TACT_ALL              = TACT_ADVERTISE | TACT_CONSTRUCTION | TACT_FUNDS,                     ///< All possible actions.
};
DECLARE_ENUM_AS_BIT_SET(TownActions)

extern const byte _town_action_costs[TACT_COUNT];
extern TownID _new_town_id;

/**
 * Set the default name for a depot/waypoint
 * @tparam T The type/class to make a default name for
 * @param obj The object/instance we want to find the name for
 */
template <class T>
void MakeDefaultName(T *obj)
{
	/* We only want to set names if it hasn't been set before, or when we're calling from afterload. */
	assert(obj->name == NULL || obj->town_cn == UINT16_MAX);

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

	uint32 used = 0; // bitmap of used waypoint numbers, sliding window with 'next' as base
	uint32 next = 0; // first number in the bitmap
	uint32 idx  = 0; // index where we will stop
	uint32 cid  = 0; // current index, goes to T::GetPoolSize()-1, then wraps to 0

	do {
		T *lobj = T::GetIfValid(cid);

		/* check only valid waypoints... */
		if (lobj != NULL && obj != lobj) {
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

	obj->town_cn = (uint16)next; // set index...
}

#endif /* TOWN_H */
