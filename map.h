#ifndef MAP_H
#define MAP_H

#define TILE_X_BITS 8
#define TILE_Y_BITS 8

#define TILES_X (1 << TILE_X_BITS)
#define TILES_Y (1 << TILE_Y_BITS)

#define TILE_X_MAX (TILES_X - 1)
#define TILE_Y_MAX (TILES_Y - 1)

extern byte _map_type_and_height[TILES_X * TILES_Y];
extern byte _map5[TILES_X * TILES_Y];
extern byte _map3_lo[TILES_X * TILES_Y];
extern byte _map3_hi[TILES_X * TILES_Y];
extern byte _map_owner[TILES_X * TILES_Y];
extern byte _map2[TILES_X * TILES_Y];
extern byte _map_extra_bits[TILES_X * TILES_Y / 4];

#endif
