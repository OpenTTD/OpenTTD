/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_town.h Functions to handle the town part of NewGRF towns. */

#ifndef NEWGRF_TOWN_H
#define NEWGRF_TOWN_H

#include "town_type.h"

/* Currently there is no direct town resolver; we only need to get town
 * variable results from inside stations, house tiles and industries,
 * and to check the town's persistent storage. */
uint32 TownGetVariable(byte variable, byte parameter, bool *available, Town *t, const GRFFile *caller_grffile);
void TownStorePSA(Town *t, const GRFFile *caller_grffile, uint pos, int32 value);

#endif /* NEWGRF_TOWN_H */
