/* $Id$ */

#ifndef CLEAR_H
#define CLEAR_H

#include "macros.h"

/* ground type, m5 bits 2...4
 * valid densities (bits 0...1) in comments after the enum
 */
typedef enum ClearGround {
	CL_GRASS  = 0, // 0-3
	CL_ROUGH  = 1, // 3
	CL_ROCKS  = 2, // 3
	CL_FIELDS = 3, // 3
	CL_SNOW   = 4, // 0-3
	CL_DESERT = 5  // 1,3
} ClearGround;

static inline ClearGround GetClearGround(TileIndex t) { return GB(_m[t].m5, 2, 3); }
static inline bool IsClearGround(TileIndex t, ClearGround ct) { return GetClearGround(t) == ct; }

static inline void AddClearDensity(TileIndex t, int d) { _m[t].m5 += d; }
static inline uint GetClearDensity(TileIndex t) { return GB(_m[t].m5, 0, 2); }

static inline void AddClearCounter(TileIndex t, int c) { _m[t].m5 += c << 5; }
static inline uint GetClearCounter(TileIndex t) { return GB(_m[t].m5, 5, 3); }
static inline void SetClearCounter(TileIndex t, uint c) { SB(_m[t].m5, 5, 3, c); }

/* Sets type and density in one go, also sets the counter to 0 */
static inline void SetClearGroundDensity(TileIndex t, ClearGround type, uint density)
{
	_m[t].m5 = 0 << 5 | type << 2 | density;
}

#endif
