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

bool CheckforTownRating(uint tile, uint32 flags, Town *t, byte type);

#define DEREF_TOWN(i) (&_towns[i])
#define FOR_ALL_TOWNS(c) for(c=_towns; c != endof(_towns); c++)

VARDEF Town _towns[70];
VARDEF int _total_towns; // For the AI: the amount of towns active

VARDEF bool _town_sort_dirty;
VARDEF byte _town_sort_order;

VARDEF Town *_cleared_town;
VARDEF int _cleared_town_rating;

#endif /* TOWN_H */
