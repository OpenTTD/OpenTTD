#ifndef MAP_H
#define MAP_H

#define TILE_FROM_XY(x,y) (int)((((y) >> 4) << MapLogX()) + ((x) >> 4))
#define TILE_XY(x,y) (((y) << MapLogX()) + (x))

#define TILE_MASK(x) ((x) & ((1 << (MapLogX() + MapLogY())) - 1))

extern byte   _map_type_and_height[];
extern byte   _map5[];
extern byte   _map3_lo[];
extern byte   _map3_hi[];
extern byte   _map_owner[];
extern uint16 _map2[];
extern byte   _map_extra_bits[];

// binary logarithm of the map size, try to avoid using this one
static inline uint MapLogX(void)  { extern uint _map_log_x; return _map_log_x; }
static inline uint MapLogY(void)  { extern uint _map_log_y; return _map_log_y; }
/* The size of the map */
static inline uint MapSizeX(void) { return 1 << MapLogX(); }
static inline uint MapSizeY(void) { return 1 << MapLogY(); }
/* The maximum coordinates */
static inline uint MapMaxX(void) { return MapSizeX() - 1; }
static inline uint MapMaxY(void) { return MapSizeY() - 1; }
/* The number of tiles in the map */
static inline uint MapSize(void) { return MapSizeX() * MapSizeY(); }

// Scale a number relative to the map size
uint ScaleByMapSize(uint); // Scale relative to the number of tiles
uint ScaleByMapSize1D(uint); // Scale relative to the circumference of the map

typedef uint32 TileIndex;


static inline uint TileX(TileIndex tile)
{
	return tile & MapMaxX();
}

static inline uint TileY(TileIndex tile)
{
	return tile >> MapLogX();
}


typedef int32 TileIndexDiff;

typedef struct TileIndexDiffC {
	int16 x;
	int16 y;
} TileIndexDiffC;

static inline TileIndexDiff ToTileIndexDiff(TileIndexDiffC tidc)
{
	return (tidc.y << MapLogX()) + tidc.x;
}


#ifndef _DEBUG
	#define TILE_ADD(x,y) ((x) + (y))
#else
	extern TileIndex TileAdd(TileIndex tile, TileIndexDiff add,
		const char *exp, const char *file, int line);
	#define TILE_ADD(x, y) (TileAdd((x), (y), #x " + " #y, __FILE__, __LINE__))
#endif

#define TILE_ADDXY(tile, x, y) TILE_ADD(tile, TILE_XY(x, y))


static inline TileIndexDiff TileOffsByDir(uint dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[4];

	assert(dir < lengthof(_tileoffs_by_dir));
	return ToTileIndexDiff(_tileoffs_by_dir[dir]);
}


static inline uint TileHeight(TileIndex tile)
{
	assert(tile < MapSize());
	return _map_type_and_height[tile] & 0xf;
}

static inline void SetTileHeight(TileIndex tile, uint height)
{
	assert(tile < MapSize());
	assert(height < 16);
	_map_type_and_height[tile] &= ~0x0F;
	_map_type_and_height[tile] |= height;
}

static inline uint TilePixelHeight(TileIndex tile)
{
	return TileHeight(tile) * 8;
}

static inline int TileType(TileIndex tile)
{
	assert(tile < MapSize());
	return _map_type_and_height[tile] >> 4;
}

static inline void SetTileType(TileIndex tile, uint type)
{
	assert(tile < MapSize());
	_map_type_and_height[tile] &= ~0xF0;
	_map_type_and_height[tile] |= type << 4;
}

static inline bool IsTileType(TileIndex tile, int type)
{
	return TileType(tile) == type;
}

#endif
