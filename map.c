#include "stdafx.h"
#include "ttd.h"
#include "map.h"

uint _map_log_x = TILE_X_BITS;
uint _map_log_y = TILE_Y_BITS;

#define MAP_SIZE ((1 << TILE_X_BITS) * (1 << TILE_Y_BITS))

byte   _map_type_and_height [MAP_SIZE];
byte   _map5                [MAP_SIZE];
byte   _map3_lo             [MAP_SIZE];
byte   _map3_hi             [MAP_SIZE];
byte   _map_owner           [MAP_SIZE];
uint16 _map2                [MAP_SIZE];
byte   _map_extra_bits      [MAP_SIZE / 4];
