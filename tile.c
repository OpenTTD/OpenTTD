#include "tile.h"

void SetMapExtraBits(TileIndex tile, byte bits)
{
	assert(tile < MapSize());
	_map_extra_bits[tile >> 2] &= ~(3 << ((tile & 3) * 2));
	_map_extra_bits[tile >> 2] |= (bits&3) << ((tile & 3) * 2);
}

uint GetMapExtraBits(TileIndex tile)
{
	assert(tile < MapSize());
	return (_map_extra_bits[tile >> 2] >> (tile & 3) * 2) & 3;
}
