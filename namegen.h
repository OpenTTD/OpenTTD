#ifndef NAMEGEN_H
#define NAMEGEN_H

typedef byte TownNameGenerator(char *buf, uint32 seed);

extern TownNameGenerator * const _town_name_generators[];

#endif
