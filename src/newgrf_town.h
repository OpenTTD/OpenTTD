/* $Id$ */

/** @file newgrf_town.h */

#ifndef NEWGRF_TOWN_H
#define NEWGRF_TOWN_H

/* Currently there is no direct town resolver; we only need to get town
 * variable results from inside stations, house tiles and industry tiles. */

uint32 TownGetVariable(byte variable, byte parameter, bool *available, const Town *t);

#endif /* NEWGRF_TOWN_H */
