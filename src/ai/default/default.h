/* $Id$ */

/** @file default.h The original AI. */

#ifndef DEFAULT_H
#define DEFAULT_H

#include "../../direction_type.h"
#include "../../vehicle_type.h"
#include "../../rail_type.h"

void AiDoGameLoop(Player*);
void SaveLoad_AI(PlayerID id);

struct AiBuildRec {
	TileIndex spec_tile;
	TileIndex use_tile;
	byte rand_rng;
	byte cur_building_rule;
	byte unk6;
	byte unk7;
	byte buildcmd_a;
	byte buildcmd_b;
	byte direction;
	CargoID cargo;
};

struct PlayerAI {
	byte state;
	byte tick;            ///< Used to determine how often to move
	uint32 state_counter; ///< Can hold tile index!
	uint16 timeout_counter;

	byte state_mode;
	byte banned_tile_count;
	RailTypeByte railtype_to_use;

	CargoID cargo_type;
	byte num_wagons;
	byte build_kind;
	byte num_build_rec;
	byte num_loco_to_build;
	byte num_want_fullload;

	byte route_type_mask;

	TileIndex start_tile_a;
	TileIndex cur_tile_a;
	DiagDirectionByte cur_dir_a;
	DiagDirectionByte start_dir_a;

	TileIndex start_tile_b;
	TileIndex cur_tile_b;
	DiagDirectionByte cur_dir_b;
	DiagDirectionByte start_dir_b;

	Vehicle *cur_veh; ///< only used by some states

	AiBuildRec src, dst, mid1, mid2;

	VehicleID wagon_list[9];
	byte order_list_blocks[20];

	TileIndex banned_tiles[16];
	byte banned_val[16];
};

extern PlayerAI _players_ai[MAX_PLAYERS];

#endif
