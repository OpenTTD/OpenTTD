#include "stdafx.h"
#include "ttd.h"
#include "map.h"

byte _map_type_and_height[TILES_X * TILES_Y];
byte _map5[TILES_X * TILES_Y];
byte _map3_lo[TILES_X * TILES_Y];
byte _map3_hi[TILES_X * TILES_Y];
byte _map_owner[TILES_X * TILES_Y];
byte _map2[TILES_X * TILES_Y];
byte _map_extra_bits[TILES_X * TILES_Y / 4];
