#ifndef NAMEGEN_H
#define NAMEGEN_H

typedef byte TownNameGenerator(byte *buf, uint32 seed);

extern TownNameGenerator * const _town_name_generators[];

#endif
