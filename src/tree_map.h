/* $Id$ */

#ifndef TREE_MAP_H
#define TREE_MAP_H

#include "macros.h"

typedef enum TreeType {
	TREE_INVALID      = -1,
	TREE_TEMPERATE    = 0,
	TREE_SUB_ARCTIC   = 12,
	TREE_RAINFOREST   = 20,
	TREE_CACTUS       = 27,
	TREE_SUB_TROPICAL = 28,
	TREE_TOYLAND      = 32
} TreeType;

enum {
	TREE_COUNT_TEMPERATE    = TREE_SUB_ARCTIC   - TREE_TEMPERATE,
	TREE_COUNT_SUB_ARCTIC   = TREE_RAINFOREST   - TREE_SUB_ARCTIC,
	TREE_COUNT_RAINFOREST   = TREE_CACTUS       - TREE_RAINFOREST,
	TREE_COUNT_SUB_TROPICAL = TREE_SUB_TROPICAL - TREE_CACTUS,
	TREE_COUNT_TOYLAND      = 9
};

/* ground type, m2 bits 4...5
 * valid densities (bits 6...7) in comments after the enum */
typedef enum TreeGround {
	TREE_GROUND_GRASS       = 0, // 0
	TREE_GROUND_ROUGH       = 1, // 0
	TREE_GROUND_SNOW_DESERT = 2  // 0-3 for snow, 3 for desert
} TreeGround;


static inline TreeType GetTreeType(TileIndex t)
{
	assert(IsTileType(t, MP_TREES));
	return (TreeType)_m[t].m3;
}


static inline TreeGround GetTreeGround(TileIndex t)
{
	assert(IsTileType(t, MP_TREES));
	return (TreeGround)GB(_m[t].m2, 4, 2);
}


static inline uint GetTreeDensity(TileIndex t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(_m[t].m2, 6, 2);
}


static inline void SetTreeGroundDensity(TileIndex t, TreeGround g, uint d)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m2, 4, 2, g);
	SB(_m[t].m2, 6, 2, d);
}


static inline uint GetTreeCount(TileIndex t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(_m[t].m5, 6, 2);
}

static inline void AddTreeCount(TileIndex t, int c)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	_m[t].m5 += c << 6;
}

static inline void SetTreeCount(TileIndex t, uint c)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m5, 6, 2, c);
}


static inline uint GetTreeGrowth(TileIndex t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(_m[t].m5, 0, 3);
}

static inline void AddTreeGrowth(TileIndex t, int a)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	_m[t].m5 += a;
}

static inline void SetTreeGrowth(TileIndex t, uint g)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m5, 0, 3, g);
}


static inline uint GetTreeCounter(TileIndex t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(_m[t].m2, 0, 4);
}

static inline void AddTreeCounter(TileIndex t, int a)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	_m[t].m2 += a;
}

static inline void SetTreeCounter(TileIndex t, uint c)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m2, 0, 4, c);
}


static inline void MakeTree(TileIndex t, TreeType type, uint count, uint growth, TreeGround ground, uint density)
{
	SetTileType(t, MP_TREES);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = density << 6 | ground << 4 | 0;
	_m[t].m3 = type;
	_m[t].m4 = 0 << 5 | 0 << 2;
	_m[t].m5 = count << 6 | growth;
}

#endif /* TREE_MAP_H */
