#ifndef PLAYER_H
#define PLAYER_H

typedef struct PlayerEconomyEntry {
	int32 income;
	int32 expenses;
	int32 delivered_cargo;
	int32 performance_history;
	int32 company_value;
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
	uint16 state_counter;
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
	byte max_railtype;
	byte block_preview;
	byte index;

	uint16 cargo_types; /* which cargo types were transported the last year */

	TileIndex location_of_house;
	TileIndex last_build_coordinate;
	
	byte share_owners[4];
	
	byte inaugurated_year;
	byte num_valid_stat_ent;
	
	byte quarters_of_bankrupcy;
	byte bankrupt_asked; // which players were asked about buying it?
	int16 bankrupt_timeout;
	int32 bankrupt_value;

	bool is_active;
	byte is_ai;
	PlayerAI ai;
	
	int64 yearly_expenses[3][13];
	PlayerEconomyEntry cur_economy;
	PlayerEconomyEntry old_economy[24];
} Player;

void ChangeOwnershipOfPlayerItems(byte old_player, byte new_player);
void GetNameOfOwner(byte owner, uint tile);
uint32 CalculateCompanyValue(Player *p);
void InvalidatePlayerWindows(Player *p);
void AiDoGameLoop(Player *p);
void UpdatePlayerMoney32(Player *p);
#define DEREF_PLAYER(i) (&_players[i])
#define FOR_ALL_PLAYERS(p) for(p=_players; p != endof(_players); p++)

#define MAX_PLAYERS 8
VARDEF Player _players[MAX_PLAYERS];

#define IS_HUMAN_PLAYER(p) (!DEREF_PLAYER((byte)(p))->is_ai)
#define IS_INTERACTIVE_PLAYER(p) (((byte)p) == _local_player)

#endif /* PLAYER_H */
