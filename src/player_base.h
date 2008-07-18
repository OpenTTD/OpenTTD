/* $Id$ */

/** @file player_base.h Definition of stuff that is very close to a player, like the player struct itself. */

#ifndef PLAYER_BASE_H
#define PLAYER_BASE_H

#include "player_type.h"
#include "oldpool.h"
#include "road_type.h"
#include "rail_type.h"
#include "date_type.h"
#include "engine_type.h"
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

DECLARE_OLD_POOL(Player, Player, 1, MAX_PLAYERS)

struct Player : PoolItem<Player, PlayerByte, &_Player_pool> {
	Player(uint16 name_1 = 0, bool is_ai = false);
	~Player();

	uint32 name_2;
	uint16 name_1;
	char *name;

	uint16 president_name_1;
	uint32 president_name_2;
	char *president_name;

	PlayerFace face;

	Money player_money;
	Money current_loan;

	byte player_color;
	Livery livery[LS_END];
	byte player_money_fraction;
	RailTypes avail_railtypes;
	RoadTypes avail_roadtypes;
	byte block_preview;

	uint32 cargo_types; ///< which cargo types were transported the last year

	TileIndex location_of_house;
	TileIndex last_build_coordinate;

	PlayerByte share_owners[4];

	Year inaugurated_year;
	byte num_valid_stat_ent;

	byte quarters_of_bankrupcy;
	byte bankrupt_asked; ///< which players were asked about buying it?
	int16 bankrupt_timeout;
	Money bankrupt_value;

	bool is_ai;

	Money yearly_expenses[3][EXPENSES_END];
	PlayerEconomyEntry cur_economy;
	PlayerEconomyEntry old_economy[24];
	EngineRenewList engine_renew_list; ///< Defined later
	bool engine_renew;
	bool renew_keep_length;
	int16 engine_renew_months;
	uint32 engine_renew_money;
	uint16 *num_engines; ///< caches the number of engines of each type the player owns (no need to save this)

	inline bool IsValid() const { return this->name_1 != 0; }
};

inline bool operator < (PlayerID p, uint u) {return (uint)p < u;}

static inline bool IsValidPlayerID(PlayerID index)
{
	return index < GetPlayerPoolSize() && GetPlayer(index)->IsValid();
}

#define FOR_ALL_PLAYERS_FROM(d, start) for (d = GetPlayer(start); d != NULL; d = (d->index + 1U < GetPlayerPoolSize()) ? GetPlayer(d->index + 1U) : NULL) if (d->IsValid())
#define FOR_ALL_PLAYERS(d) FOR_ALL_PLAYERS_FROM(d, 0)

struct PlayerMoneyBackup {
private:
	Money backup_yearly_expenses[EXPENSES_END];
	PlayerEconomyEntry backup_cur_economy;
	Player *p;

public:
	PlayerMoneyBackup(Player *player);

	void Restore();
};

static inline byte ActivePlayerCount()
{
	const Player *p;
	byte count = 0;

	FOR_ALL_PLAYERS(p) count++;

	return count;
}

Money CalculateCompanyValue(const Player *p);

#endif /* PLAYER_BASE_H */
