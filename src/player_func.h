/* $Id$ */

/** @file player_func.h Functions related to players. */

#ifndef PLAYER_FUNC_H
#define PLAYER_FUNC_H

#include "core/math_func.hpp"
#include "player_type.h"
#include "tile_type.h"

void ChangeOwnershipOfPlayerItems(PlayerID old_player, PlayerID new_player);
void GetNameOfOwner(Owner owner, TileIndex tile);
void SetLocalPlayer(PlayerID new_player);

extern PlayerByte _local_player;
extern PlayerByte _current_player;
/* NOSAVE: can be determined from player structs */
extern byte _player_colors[MAX_PLAYERS];
extern PlayerFace _player_face; ///< for player face storage in openttd.cfg

bool IsHumanPlayer(PlayerID pi);

static inline bool IsLocalPlayer()
{
	return _local_player == _current_player;
}

static inline bool IsValidPlayer(PlayerID pi)
{
	return IsInsideBS(pi, PLAYER_FIRST, MAX_PLAYERS);
}

static inline bool IsInteractivePlayer(PlayerID pi)
{
	return pi == _local_player;
}



struct HighScore {
	char company[100];
	StringID title; ///< NO_SAVE, has troubles with changing string-numbers.
	uint16 score;   ///< do NOT change type, will break hs.dat
};

extern HighScore _highscore_table[5][5]; // 4 difficulty-settings (+ network); top 5
void SaveToHighScore();
void LoadFromHighScore();
int8 SaveHighScoreValue(const Player *p);
int8 SaveHighScoreValueNetwork();

#endif /* PLAYER_FUNC_H */
