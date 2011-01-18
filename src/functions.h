/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file functions.h Some generic functions that actually shouldn't be here. */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "tile_cmd.h"

/* clear_land.cpp */
void DrawHillyLandTile(const TileInfo *ti);
void DrawClearLandTile(const TileInfo *ti, byte set);
void DrawClearLandFence(const TileInfo *ti);
void TileLoopClearHelper(TileIndex tile);

/* company_cmd.cpp */
bool CheckCompanyHasMoney(CommandCost &cost);
void SubtractMoneyFromCompany(CommandCost cost);
void SubtractMoneyFromCompanyFract(CompanyID company, CommandCost cost);
CommandCost CheckOwnership(Owner owner, TileIndex tile = 0);
CommandCost CheckTileOwnership(TileIndex tile);

/* misc functions */
void MarkTileDirtyByTile(TileIndex tile);

void ShowCostOrIncomeAnimation(int x, int y, int z, Money cost);
void ShowFeederIncomeAnimation(int x, int y, int z, Money cost);

void AskExitGame();
void AskExitToGameMenu();

int ttd_main(int argc, char *argv[]);
void HandleExitGameRequest();

#endif /* FUNCTIONS_H */
