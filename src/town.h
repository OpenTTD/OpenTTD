/* $Id$ */

#ifndef TOWN_H
#define TOWN_H

#include "oldpool.h"
#include "player.h"

enum {
	INVALID_TOWN = 0xFFFF,
};

struct Town {
	TileIndex xy;

	// Current population of people and amount of houses.
	uint16 num_houses;
	uint32 population;

	// Town name
	uint16 townnametype;
	uint32 townnameparts;

	// NOSAVE: Location of name sign, UpdateTownVirtCoord updates this.
	ViewportSign sign;

	// Makes sure we don't build certain house types twice.
	// bit 0 = Building funds received
	// bit 1 = CHURCH
	// bit 2 = STADIUM
	byte flags12;

	// Which players have a statue?
	byte statues;

	// Player ratings as well as a mask that determines which players have a rating.
	byte have_ratings;
	uint8 unwanted[MAX_PLAYERS]; // how many months companies aren't wanted by towns (bribe)
	PlayerByte exclusivity;        // which player has exslusivity
	uint8 exclusive_counter;     // months till the exclusivity expires
	int16 ratings[MAX_PLAYERS];

	// Maximum amount of passengers and mail that can be transported.
	uint32 max_pass;
	uint32 max_mail;
	uint32 new_max_pass;
	uint32 new_max_mail;
	uint32 act_pass;
	uint32 act_mail;
	uint32 new_act_pass;
	uint32 new_act_mail;

	// Amount of passengers that were transported.
	byte pct_pass_transported;
	byte pct_mail_transported;

	// Amount of food and paper that was transported. Actually a bit mask would be enough.
	uint16 act_food;
	uint16 act_water;
	uint16 new_act_food;
	uint16 new_act_water;

	// Time until we rebuild a house.
	byte time_until_rebuild;

	// When to grow town next time.
	byte grow_counter;
	byte growth_rate;

	// Fund buildings program in action?
	byte fund_buildings_months;

	// Fund road reconstruction in action?
	byte road_build_months;

	// Index in town array
	TownID index;

	// NOSAVE: UpdateTownRadius updates this given the house count.
	uint16 radius[5];
};

uint32 GetWorldPopulation();

void UpdateTownVirtCoord(Town *t);
void InitializeTown();
void ShowTownViewWindow(TownID town);
void ExpandTown(Town *t);
Town *CreateRandomTown(uint attempts, uint size_mode);

enum {
	ROAD_REMOVE = 0,
	UNMOVEABLE_REMOVE = 1,
	TUNNELBRIDGE_REMOVE = 1,
	INDUSTRY_REMOVE = 2
};

enum {
	// These refer to the maximums, so Appalling is -1000 to -400
	// MAXIMUM RATINGS BOUNDARIES
	RATING_MINIMUM     = -1000,
	RATING_APPALLING   =  -400,
	RATING_VERYPOOR    =  -200,
	RATING_POOR        =     0,
	RATING_MEDIOCRE    =   200,
	RATING_GOOD        =   400,
	RATING_VERYGOOD    =   600,
	RATING_EXCELLENT   =   800,
	RATING_OUTSTANDING =  1000,         // OUTSTANDING

	RATING_MAXIMUM = RATING_OUTSTANDING,

	// RATINGS AFFECTING NUMBERS
	RATING_TREE_DOWN_STEP = -35,
	RATING_TREE_MINIMUM   = RATING_MINIMUM,
	RATING_TREE_UP_STEP   = 7,
	RATING_TREE_MAXIMUM   = 220,

	RATING_TUNNEL_BRIDGE_DOWN_STEP = -250,
	RATING_TUNNEL_BRIDGE_MINIMUM   = 0,

	RATING_INDUSTRY_DOWN_STEP = -1500,
	RATING_INDUSTRY_MINIMUM   = RATING_MINIMUM,

	RATING_ROAD_DOWN_STEP = -50,
	RATING_ROAD_MINIMUM   = -100,
	RATING_HOUSE_MINIMUM  = RATING_MINIMUM,

	RATING_BRIBE_UP_STEP = 200,
	RATING_BRIBE_MAXIMUM = 800,
	RATING_BRIBE_DOWN_TO = -50        // XXX SHOULD BE SOMETHING LOWER?
};

enum {
/* This is the base "normal" number of towns on the 8x8 map, when
 * one town should get grown per tick. The other numbers of towns
 * are then scaled based on that. */
	TOWN_GROWTH_FREQUENCY = 23,
/* Simple value that indicates the house has reached final stage of construction*/
	TOWN_HOUSE_COMPLETED  =  3,
};

/* This enum is used in conjonction with town->flags12.
 * IT simply states what bit is used for.
 * It is pretty unrealistic (IMHO) to only have one church/stadium
 * per town, NO MATTER the population of it.
 * And there are 5 more bits available on flags12...
 */
enum {
	TOWN_IS_FUNDED      = 0,   // Town has received some funds for
	TOWN_HAS_CHURCH     = 1,   // There can be only one church by town.
	TOWN_HAS_STADIUM    = 2    // There can be only one stadium by town.
};

bool CheckforTownRating(uint32 flags, Town *t, byte type);

VARDEF const Town** _town_sort;

DECLARE_OLD_POOL(Town, Town, 3, 8000)

/**
 * Check if a Town really exists.
 */
static inline bool IsValidTown(const Town* town)
{
	return town->xy != 0;
}

static inline bool IsValidTownID(TownID index)
{
	return index < GetTownPoolSize() && IsValidTown(GetTown(index));
}

VARDEF uint _total_towns;

static inline TownID GetMaxTownIndex()
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetTownPoolSize() - 1;
}

static inline uint GetNumTowns()
{
	return _total_towns;
}

/**
 * Return a random valid town.
 */
static inline Town *GetRandomTown()
{
	int num = RandomRange(GetNumTowns());
	TownID index = INVALID_TOWN;

	while (num >= 0) {
		num--;

		index++;
		/* Make sure we have a valid town */
		while (!IsValidTownID(index)) {
			index++;
			assert(index <= GetMaxTownIndex());
		}
	}

	return GetTown(index);
}

void DestroyTown(Town *t);

static inline void DeleteTown(Town *t)
{
	DestroyTown(t);
	t->xy = 0;
}

Town* CalcClosestTownFromTile(TileIndex tile, uint threshold);

#define FOR_ALL_TOWNS_FROM(t, start) for (t = GetTown(start); t != NULL; t = (t->index + 1U < GetTownPoolSize()) ? GetTown(t->index + 1U) : NULL) if (IsValidTown(t))
#define FOR_ALL_TOWNS(t) FOR_ALL_TOWNS_FROM(t, 0)

VARDEF bool _town_sort_dirty;
VARDEF byte _town_sort_order;

VARDEF Town *_cleared_town;
VARDEF int _cleared_town_rating;

#endif /* TOWN_H */
