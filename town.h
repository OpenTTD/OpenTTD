#ifndef TOWN_H
#define TOWN_H

#include "player.h"

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
	byte flags12;

	// Which players have a statue?
	byte statues;

	// Sort index in listings
	byte sort_index_obsolete;

	// Player ratings as well as a mask that determines which players have a rating.
	byte have_ratings;
	uint8 unwanted[MAX_PLAYERS]; // how many months companies aren't wanted by towns (bribe)
	uint8 exclusivity;	     // which player has exslusivity
	uint8 exclusive_counter;     // months till the exclusivity expires
	int16 ratings[MAX_PLAYERS];

	// Maximum amount of passengers and mail that can be transported.
	uint16 max_pass;
	uint16 max_mail;
	uint16 new_max_pass;
	uint16 new_max_mail;
	uint16 act_pass;
	uint16 act_mail;
	uint16 new_act_pass;
	uint16 new_act_mail;

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
	uint16 index;

	// NOSAVE: UpdateTownRadius updates this given the house count.
	uint16 radius[5];
};

uint32 GetWorldPopulation();

void UpdateTownVirtCoord(Town *t);
void InitializeTown();
void ShowTownViewWindow(uint town);
void DeleteTown(Town *t);
void ExpandTown(Town *t);
bool GrowTown(Town *t);
Town *CreateRandomTown();

enum {
	ROAD_REMOVE = 0,
	UNMOVEABLE_REMOVE = 1,
	TUNNELBRIDGE_REMOVE = 1,
	INDUSTRY_REMOVE = 2
};

enum {
	// These refer to the maximums, so Appalling is -1000 to -400
	// MAXIMUM RATINGS BOUNDARIES
	RATING_MINIMUM 		= -1000,
	RATING_APPALLING 	= -400,
	RATING_VERYPOOR 	= -200,
	RATING_POOR 			= 0,
	RATING_MEDIOCRE		= 200,
	RATING_GOOD				= 400,
	RATING_VERYGOOD		= 600,
	RATING_EXCELLENT	= 800,
	RATING_OUTSTANDING= 1000, 	// OUTSTANDING

	RATING_MAXIMUM = RATING_OUTSTANDING,

	// RATINGS AFFECTING NUMBERS
	RATING_TREE_DOWN_STEP = -35,
	RATING_TREE_MINIMUM = RATING_MINIMUM,
	RATING_TREE_UP_STEP = 7,
	RATING_TREE_MAXIMUM = 220,

	RATING_TUNNEL_BRIDGE_DOWN_STEP = -250,
	RATING_TUNNEL_BRIDGE_MINIMUM = 0,

	RATING_INDUSTRY_DOWN_STEP = -1500,
	RATING_INDUSTRY_MINIMUM = RATING_MINIMUM,

	RATING_ROAD_DOWN_STEP = -50,
	RATING_ROAD_MINIMUM = -100,
	RATING_HOUSE_MINIMUM = RATING_MINIMUM,

	RATING_BRIBE_UP_STEP = 200,
	RATING_BRIBE_MAXIMUM = 800,
	RATING_BRIBE_DOWN_TO = -50 					// XXX SHOULD BE SOMETHING LOWER?
};

bool CheckforTownRating(uint tile, uint32 flags, Town *t, byte type);

VARDEF Town _towns[70];
VARDEF uint _towns_size;

VARDEF uint16 *_town_sort;

static inline Town *GetTown(uint index)
{
	assert(index < _towns_size);
	return &_towns[index];
}

#define FOR_ALL_TOWNS(t) for(t = _towns; t != &_towns[_towns_size]; t++)

VARDEF int _total_towns; // For the AI: the amount of towns active

VARDEF bool _town_sort_dirty;
VARDEF byte _town_sort_order;

VARDEF Town *_cleared_town;
VARDEF int _cleared_town_rating;

#endif /* TOWN_H */
