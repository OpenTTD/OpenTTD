/* $Id$ */

/** @file player.h */

#ifndef PLAYER_H
#define PLAYER_H

#include "road_func.h"
#include "date_type.h"
#include "engine.h"
#include "livery.h"
#include "genworld.h"
#include "autoreplace_type.h"

struct PlayerEconomyEntry {
	Money income;
	Money expenses;
	int32 delivered_cargo;
	int32 performance_history; ///< player score (scale 0-1000)
	Money company_value;
};

/* The "steps" in loan size, in British Pounds! */
enum {
	LOAN_INTERVAL        = 10000,
	LOAN_INTERVAL_OLD_AI = 50000,
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
	byte avail_railtypes;
	byte avail_roadtypes;
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

uint16 GetDrawStringPlayerColor(PlayerID player);

void ChangeOwnershipOfPlayerItems(PlayerID old_player, PlayerID new_player);
void GetNameOfOwner(Owner owner, TileIndex tile);
Money CalculateCompanyValue(const Player *p);
void InvalidatePlayerWindows(const Player *p);
void SetLocalPlayer(PlayerID new_player);
#define FOR_ALL_PLAYERS(p) for (p = _players; p != endof(_players); p++)

VARDEF PlayerByte _local_player;
VARDEF PlayerByte _current_player;

VARDEF Player _players[MAX_PLAYERS];
/* NOSAVE: can be determined from player structs */
VARDEF byte _player_colors[MAX_PLAYERS];

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

static inline bool IsLocalPlayer()
{
	return _local_player == _current_player;
}

static inline bool IsValidPlayer(PlayerID pi)
{
	return IsInsideBS(pi, PLAYER_FIRST, MAX_PLAYERS);
}

byte GetPlayerRailtypes(PlayerID p);
byte GetPlayerRoadtypes(PlayerID p);

/** Finds out if a Player has a certain railtype available
 * @param p Player in question
 * @param Railtype requested RailType
 * @return true if player has requested RailType available
 */
static inline bool HasRailtypeAvail(const Player *p, const RailType Railtype)
{
	return HasBit(p->avail_railtypes, Railtype);
}

/** Finds out, whether given player has all given RoadTypes available
 * @param PlayerID ID of player
 * @param rts RoadTypes to test
 * @return true if player has all requested RoadTypes available
 */
static inline bool HasRoadTypesAvail(const PlayerID p, const RoadTypes rts)
{
	RoadTypes avail_roadtypes;

	if (p == OWNER_TOWN || _game_mode == GM_EDITOR || IsGeneratingWorld()) {
		avail_roadtypes = ROADTYPES_ROAD;
	} else {
		if (!IsValidPlayer(p)) return false;
		avail_roadtypes = (RoadTypes)GetPlayer(p)->avail_roadtypes | ROADTYPES_ROAD; // road is available for always for everybody
	}
	return (rts & ~avail_roadtypes) == 0;
}

static inline bool IsHumanPlayer(PlayerID pi)
{
	return !GetPlayer(pi)->is_ai;
}

static inline bool IsInteractivePlayer(PlayerID pi)
{
	return pi == _local_player;
}

void DrawPlayerIcon(PlayerID p, int x, int y);

/* Validate functions for rail building */
static inline bool ValParamRailtype(const uint32 rail) { return HasBit(GetPlayer(_current_player)->avail_railtypes, rail);}

/* Validate functions for road building */
static inline bool ValParamRoadType(const RoadType rt) { return HasRoadTypesAvail(_current_player, RoadTypeToRoadTypes(rt));}

/** Returns the "best" railtype a player can build.
 * As the AI doesn't know what the BEST one is, we have our own priority list
 * here. When adding new railtypes, modify this function
 * @param p the player "in action"
 * @return The "best" railtype a player has available
 */
static inline RailType GetBestRailtype(const Player *p)
{
	if (HasRailtypeAvail(p, RAILTYPE_MAGLEV)) return RAILTYPE_MAGLEV;
	if (HasRailtypeAvail(p, RAILTYPE_MONO)) return RAILTYPE_MONO;
	if (HasRailtypeAvail(p, RAILTYPE_ELECTRIC)) return RAILTYPE_ELECTRIC;
	return RAILTYPE_RAIL;
}

struct HighScore {
	char company[100];
	StringID title; ///< NO_SAVE, has troubles with changing string-numbers.
	uint16 score;   ///< do NOT change type, will break hs.dat
};

VARDEF HighScore _highscore_table[5][5]; // 4 difficulty-settings (+ network); top 5
void SaveToHighScore();
void LoadFromHighScore();
int8 SaveHighScoreValue(const Player *p);
int8 SaveHighScoreValueNetwork();

/**
 * Reset the livery schemes to the player's primary colour.
 * This is used on loading games without livery information and on new player start up.
 * @param p Player to reset.
 */
void ResetPlayerLivery(Player *p);

#endif /* PLAYER_H */
