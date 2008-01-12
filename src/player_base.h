/* $Id$ */

/** @file player_base.h Definition of stuff that is very close to a player, like the player struct itself. */

#ifndef PLAYER_BASE_H
#define PLAYER_BASE_H

#include "road_type.h"
#include "rail_type.h"
#include "date_type.h"
#include "engine.h"
#include "livery.h"
#include "autoreplace_type.h"
#include "economy_type.h"
#include "tile_type.h"

struct PlayerEconomyEntry {
	Money income;
	Money expenses;
	int32 delivered_cargo;
	int32 performance_history; ///< player score (scale 0-1000)
	Money company_value;
};

struct Player {
	uint32 name_2;
	uint16 name_1;

	uint16 president_name_1;
	uint32 president_name_2;

	PlayerFace face;

	Money player_money;
	Money current_loan;

	byte player_color;
	Livery livery[LS_END];
	byte player_money_fraction;
	RailTypes avail_railtypes;
	RoadTypes avail_roadtypes;
	byte block_preview;
	PlayerByte index;

	uint16 cargo_types; ///< which cargo types were transported the last year

	TileIndex location_of_house;
	TileIndex last_build_coordinate;

	PlayerByte share_owners[4];

	Year inaugurated_year;
	byte num_valid_stat_ent;

	byte quarters_of_bankrupcy;
	byte bankrupt_asked; ///< which players were asked about buying it?
	int16 bankrupt_timeout;
	Money bankrupt_value;

	bool is_active;
	bool is_ai;

	Money yearly_expenses[3][13];
	PlayerEconomyEntry cur_economy;
	PlayerEconomyEntry old_economy[24];
	EngineRenewList engine_renew_list; ///< Defined later
	bool engine_renew;
	bool renew_keep_length;
	int16 engine_renew_months;
	uint32 engine_renew_money;
	uint16 num_engines[TOTAL_NUM_ENGINES]; ///< caches the number of engines of each type the player owns (no need to save this)
};

extern Player _players[MAX_PLAYERS];
#define FOR_ALL_PLAYERS(p) for (p = _players; p != endof(_players); p++)

static inline byte ActivePlayerCount()
{
	const Player *p;
	byte count = 0;

	FOR_ALL_PLAYERS(p) {
		if (p->is_active) count++;
	}

	return count;
}

static inline Player *GetPlayer(PlayerID i)
{
	assert(IsInsideBS(i, PLAYER_FIRST, lengthof(_players)));
	return &_players[i];
}

Money CalculateCompanyValue(const Player *p);

#endif /* PLAYER_BASE_H */
