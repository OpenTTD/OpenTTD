/* $Id$ */

#ifndef TREE_MAP_H
#define TREE_MAP_H

#include "macros.h"

typedef enum TreeType {
	TR_INVALID      = -1,
	TR_TEMPERATE    = 0,
	TR_SUB_ARCTIC   = 12,
	TR_RAINFOREST   = 20,
	TR_CACTUS       = 27,
	TR_SUB_TROPICAL = 28,
	TR_TOYLAND      = 32
} TreeType;

enum {
	TR_COUNT_TEMPERATE    = TR_SUB_ARCTIC   - TR_TEMPERATE,
	TR_COUNT_SUB_ARCTIC   = TR_RAINFOREST   - TR_SUB_ARCTIC,
	TR_COUNT_RAINFOREST   = TR_CACTUS       - TR_RAINFOREST,
	TR_COUNT_SUB_TROPICAL = TR_SUB_TROPICAL - TR_CACTUS,
	TR_COUNT_TOYLAND      = 9
};

/* ground type, m2 bits 4...5
 * valid densities (bits 6...7) in comments after the enum */
typedef enum TreeGround {
	TR_GRASS       = 0, // 0
	TR_ROUGH       = 1, // 0
	TR_SNOW_DESERT = 2  // 0-3 for snow, 3 for desert
} TreeGround;

static inline TreeType GetTreeType(TileIndex t) { return _m[t].m3; }
static inline void SetTreeType(TileIndex t, TreeType r) { _m[t].m3 = r; }

static inline TreeGround GetTreeGround(TileIndex t) { return GB(_m[t].m2, 4, 2); }

static inline uint GetTreeDensity(TileIndex t) { return GB(_m[t].m2, 6, 2); }

static inline void SetTreeGroundDensity(TileIndex t, TreeGround g, uint d)
{
	SB(_m[t].m2, 4, 2, g);
	SB(_m[t].m2, 6, 2, d);
}

static inline void AddTreeCount(TileIndex t, int c) { _m[t].m5 += c << 6; }
static inline uint GetTreeCount(TileIndex t) { return GB(_m[t].m5, 6, 2); }
static inline void SetTreeCount(TileIndex t, uint c) { SB(_m[t].m5, 6, 2, c); }

static inline void AddTreeGrowth(TileIndex t, int a) { _m[t].m5 += a; }
static inline uint GetTreeGrowth(TileIndex t) { return GB(_m[t].m5, 0, 3); }
static inline void SetTreeGrowth(TileIndex t, uint g) { SB(_m[t].m5, 0, 3, g); }

static inline void AddTreeCounter(TileIndex t, int a) { _m[t].m2 += a; }
static inline uint GetTreeCounter(TileIndex t) { return GB(_m[t].m2, 0, 4); }
static inline void SetTreeCounter(TileIndex t, uint c) { SB(_m[t].m2, 0, 4, c); }


static inline void MakeTree(TileIndex t, TreeType type, uint count, uint growth, TreeGround ground, uint density)
{
	SetTileType(t, MP_TREES);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = density << 6 | ground << 4 | 0;
	_m[t].m3 = type;
	_m[t].m4 = 0 << 5 | 0 << 2;
	_m[t].m5 = count << 6 | growth;
}

#endif
