/* $Id$ */

#ifndef PLAYER_H
#define PLAYER_H

#include "aystar.h"
#include "rail.h"

typedef struct PlayerEconomyEntry {
	int32 income;
	int32 expenses;
	int32 delivered_cargo;
	int32 performance_history;	// player score (scale 0-1000)
	int64 company_value;
} PlayerEconomyEntry;

typedef struct AiBuildRec {
	TileIndex spec_tile;
	TileIndex use_tile;
	byte rand_rng;
	byte cur_building_rule;
	byte unk6;
	byte unk7;
	byte buildcmd_a;
	byte buildcmd_b;
	byte direction;
	byte cargo;
} AiBuildRec;

typedef struct PlayerAI {
	byte state;
	byte tick; // Used to determine how often to move
	uint32 state_counter; // Can hold tile index!
	uint16 timeout_counter;

	byte state_mode;
	byte banned_tile_count;
	byte railtype_to_use;

	byte cargo_type;
	byte num_wagons;
	byte build_kind;
	byte num_build_rec;
	byte num_loco_to_build;
	byte num_want_fullload;

	byte route_type_mask;

	TileIndex start_tile_a;
	TileIndex cur_tile_a;
	byte cur_dir_a;
	byte start_dir_a;

	TileIndex start_tile_b;
	TileIndex cur_tile_b;
	byte cur_dir_b;
	byte start_dir_b;

	Vehicle *cur_veh; /* only used by some states */

	AiBuildRec src, dst, mid1, mid2;

	VehicleID wagon_list[9];
	byte order_list_blocks[20];

	TileIndex banned_tiles[16];
	byte banned_val[16];
} PlayerAI;

typedef struct Ai_PathFinderInfo {
	TileIndex start_tile_tl; // tl = top-left
	TileIndex start_tile_br; // br = bottom-right
	TileIndex end_tile_tl; // tl = top-left
	TileIndex end_tile_br; // br = bottom-right
	byte start_direction; // 0 to 3 or AI_PATHFINDER_NO_DIRECTION
	byte end_direction; // 0 to 3 or AI_PATHFINDER_NO_DIRECTION

	TileIndex route[500];
	byte route_extra[500]; // Some extra information about the route like bridge/tunnel
	int route_length;
	int position; // Current position in the build-path, needed to build the path

	bool rail_or_road; // true = rail, false = road
} Ai_PathFinderInfo;

// The amount of memory reserved for the AI-special-vehicles
#define AI_MAX_SPECIAL_VEHICLES 100

typedef struct Ai_SpecialVehicle {
	VehicleID veh_id;
	uint32 flag;
} Ai_SpecialVehicle;

typedef struct PlayerAiNew {
	uint8 state;
	uint tick;
	uint idle;

	int temp; 	// A value used in more than one function, but it just temporary
				// The use is pretty simple: with this we can 'think' about stuff
				//   in more than one tick, and more than one AI. A static will not
				//   do, because they are not saved. This way, the AI is almost human ;)
	int counter; 	// For the same reason as temp, we have counter. It can count how
					//  long we are trying something, and just abort if it takes too long

	// Pathfinder stuff
	Ai_PathFinderInfo path_info;
	AyStar *pathfinder;

	// Route stuff

	byte cargo;
	byte tbt; // train/bus/truck 0/1/2 AI_TRAIN/AI_BUS/AI_TRUCK
	int new_cost;

	byte action;

	int last_id; // here is stored the last id of the searched city/industry
	uint last_vehiclecheck_date; // Used in CheckVehicle
	Ai_SpecialVehicle special_vehicles[AI_MAX_SPECIAL_VEHICLES]; // Some vehicles have some special flags

	TileIndex from_tile;
	TileIndex to_tile;

	byte from_direction;
	byte to_direction;

	bool from_deliver; // True if this is the station that GIVES cargo
	bool to_deliver;

	TileIndex depot_tile;
	byte depot_direction;

	byte amount_veh; // How many vehicles we are going to build in this route
	byte cur_veh; // How many vehicles did we bought?
	VehicleID veh_id; // Used when bought a vehicle
	VehicleID veh_main_id; // The ID of the first vehicle, for shared copy

	int from_ic; // ic = industry/city. This is the ID of them
	byte from_type; // AI_NO_TYPE/AI_CITY/AI_INDUSTRY
	int to_ic;
	byte to_type;

} PlayerAiNew;



typedef struct Player {
	uint32 name_2;
	uint16 name_1;

	uint16 president_name_1;
	uint32 president_name_2;

	uint32 face;

	int32 player_money;
	int32 current_loan;
	int64 money64; // internal 64-bit version of the money. the 32-bit field will be clamped to plus minus 2 billion

	byte player_color;
	byte player_money_fraction;
	byte avail_railtypes;
	byte block_preview;
	PlayerID index;

	uint16 cargo_types; /* which cargo types were transported the last year */

	TileIndex location_of_house;
	TileIndex last_build_coordinate;

	PlayerID share_owners[4];

	byte inaugurated_year;
	byte num_valid_stat_ent;

	byte quarters_of_bankrupcy;
	byte bankrupt_asked; // which players were asked about buying it?
	int16 bankrupt_timeout;
	int32 bankrupt_value;

	bool is_active;
	byte is_ai;
	PlayerAI ai;
	PlayerAiNew ainew;

	int64 yearly_expenses[3][13];
	PlayerEconomyEntry cur_economy;
	PlayerEconomyEntry old_economy[24];
	EngineID engine_replacement[256];
	bool engine_renew;
	int16 engine_renew_months;
	uint32 engine_renew_money;
} Player;

void ChangeOwnershipOfPlayerItems(PlayerID old_player, PlayerID new_player);
void GetNameOfOwner(PlayerID owner, TileIndex tile);
int64 CalculateCompanyValue(const Player* p);
void InvalidatePlayerWindows(const Player* p);
void UpdatePlayerMoney32(Player *p);
#define FOR_ALL_PLAYERS(p) for(p=_players; p != endof(_players); p++)

VARDEF PlayerID _local_player;
VARDEF PlayerID _current_player;

#define MAX_PLAYERS 8
VARDEF Player _players[MAX_PLAYERS];
// NOSAVE: can be determined from player structs
VARDEF byte _player_colors[MAX_PLAYERS];

static inline Player* GetPlayer(PlayerID i)
{
	assert(i < lengthof(_players));
	return &_players[i];
}

static inline bool IsLocalPlayer(void)
{
	return _local_player == _current_player;
}

/** Returns the number of rail types the player can build
  * @param *p Player in question
  */
static inline int GetNumRailtypes(const Player *p)
{
	int num = 0;
	int i;

	for (i = 0; i < (int)sizeof(p->avail_railtypes) * 8; i++)
		if (HASBIT(p->avail_railtypes, i)) num++;

	assert(num <= RAILTYPE_END);
	return num;
}

byte GetPlayerRailtypes(int p);

/** Finds out if a Player has a certain railtype available
  */
static inline bool HasRailtypeAvail(const Player *p, RailType Railtype)
{
	return HASBIT(p->avail_railtypes, Railtype);
}

/* Validate functions for rail building */
static inline bool ValParamRailtype(uint32 rail) { return HASBIT(GetPlayer(_current_player)->avail_railtypes, rail);}

/** Returns the "best" railtype a player can build.
  * As the AI doesn't know what the BEST one is, we
  * have our own priority list here. When adding
  * new railtypes, modify this function
  * @param p the player "in action"
  * @return The "best" railtype a player has available
  */
static inline byte GetBestRailtype(const Player *p)
{
	if (HasRailtypeAvail(p, RAILTYPE_MAGLEV)) return RAILTYPE_MAGLEV;
	if (HasRailtypeAvail(p, RAILTYPE_MONO)) return RAILTYPE_MONO;
	return RAILTYPE_RAIL;
}

#define IS_HUMAN_PLAYER(p) (!GetPlayer(p)->is_ai)
#define IS_INTERACTIVE_PLAYER(p) ((p) == _local_player)

typedef struct HighScore {
	char company[100];
	StringID title;
	uint16 score;
} HighScore;

VARDEF HighScore _highscore_table[5][5]; // 4 difficulty-settings (+ network); top 5
void SaveToHighScore(void);
void LoadFromHighScore(void);
int8 SaveHighScoreValue(const Player *p);
int8 SaveHighScoreValueNetwork(void);

#endif /* PLAYER_H */
