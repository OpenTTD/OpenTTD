/* $Id$ */

#ifndef NAMEGEN_H
#define NAMEGEN_H

typedef byte TownNameGenerator(char *buf, uint32 seed, const char *last);

extern TownNameGenerator * const _town_name_generators[];

#endif /* NAMEGEN_H */
