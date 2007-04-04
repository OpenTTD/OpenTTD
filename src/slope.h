/* $Id$ */

/** @file slope.h */

#ifndef SLOPE_H
#define SLOPE_H

enum Slope {
	SLOPE_FLAT     = 0x00,
	SLOPE_W        = 0x01,
	SLOPE_S        = 0x02,
	SLOPE_E        = 0x04,
	SLOPE_N        = 0x08,
	SLOPE_STEEP    = 0x10,
	SLOPE_NW       = SLOPE_N | SLOPE_W,
	SLOPE_SW       = SLOPE_S | SLOPE_W,
	SLOPE_SE       = SLOPE_S | SLOPE_E,
	SLOPE_NE       = SLOPE_N | SLOPE_E,
	SLOPE_EW       = SLOPE_E | SLOPE_W,
	SLOPE_NS       = SLOPE_N | SLOPE_S,
	SLOPE_ELEVATED = SLOPE_N | SLOPE_E | SLOPE_S | SLOPE_W,
	SLOPE_NWS      = SLOPE_N | SLOPE_W | SLOPE_S,
	SLOPE_WSE      = SLOPE_W | SLOPE_S | SLOPE_E,
	SLOPE_SEN      = SLOPE_S | SLOPE_E | SLOPE_N,
	SLOPE_ENW      = SLOPE_E | SLOPE_N | SLOPE_W,
	SLOPE_STEEP_W  = SLOPE_STEEP | SLOPE_NWS,
	SLOPE_STEEP_S  = SLOPE_STEEP | SLOPE_WSE,
	SLOPE_STEEP_E  = SLOPE_STEEP | SLOPE_SEN,
	SLOPE_STEEP_N  = SLOPE_STEEP | SLOPE_ENW
};

static inline bool IsSteepSlope(Slope s)
{
	return (s & SLOPE_STEEP) != 0;
}

static inline Slope ComplementSlope(Slope s)
{
	assert(!IsSteepSlope(s));
	return (Slope)(0xF ^ s);
}

#endif /* SLOPE_H */
