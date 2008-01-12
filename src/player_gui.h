/* $Id$ */

/** @file player_gui.h GUI Functions related to players. */

#ifndef PLAYER_GUI_H
#define PLAYER_GUI_H

#include "player_type.h"

uint16 GetDrawStringPlayerColor(PlayerID player);
void DrawPlayerIcon(PlayerID p, int x, int y);

void ShowPlayerStations(PlayerID player);
void ShowPlayerFinances(PlayerID player);
void ShowPlayerCompany(PlayerID player);

void InvalidatePlayerWindows(const Player *p);
void DeletePlayerWindows(PlayerID pi);

#endif /* PLAYER_GUI_H */
