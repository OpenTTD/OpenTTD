/* $Id$ */

#ifndef MACROS_H
#define MACROS_H

#include "map.h"

/// Fetch n bits starting at bit s from x
#define GB(x, s, n) (((x) >> (s)) & ((1U << (n)) - 1))
/// Set n bits starting at bit s in x to d
#define SB(x, s, n, d) ((x) = ((x) & ~(((1U << (n)) - 1) << (s))) | ((d) << (s)))
/// Add i to the n bits starting at bit s in x
#define AB(x, s, n, i) ((x) = ((x) & ~(((1U << (n)) - 1) << (s))) | (((x) + ((i) << (s))) & (((1U << (n)) - 1) << (s))))

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

static inline int min(int a, int b) { if (a <= b) return a; return b; }
static inline int max(int a, int b) { if (a >= b) return a; return b; }
static inline int64 max64(int64 a, int64 b) { if (a >= b) return a; return b; }

static inline uint minu(uint a, uint b) { if (a <= b) return a; return b; }
static inline uint maxu(uint a, uint b) { if (a >= b) return a; return b; }


static inline int clamp(int a, int min, int max)
{
	if (a <= min) return min;
	if (a >= max) return max;
	return a;
}

static inline uint clampu(uint a, uint min, uint max)
{
	if (a <= min) return min;
	if (a >= max) return max;
	return a;
}

static inline int32 BIGMULSS(int32 a, int32 b, int shift) {
	return (int32)(((int64)(a) * (int64)(b)) >> (shift));
}

static inline int64 BIGMULSS64(int64 a, int64 b, int shift) {
	return ((a) * (b)) >> (shift);
}

static inline uint32 BIGMULUS(uint32 a, uint32 b, int shift) {
	return (uint32)(((uint64)(a) * (uint64)(b)) >> (shift));
}

static inline int64 BIGMULS(int32 a, int32 b) {
	return (int64)(a) * (int64)(b);
}

/* OPT: optimized into an unsigned comparison */
//#define IS_INSIDE_1D(x, base, size) ((x) >= (base) && (x) < (base) + (size))
#define IS_INSIDE_1D(x, base, size) ( (uint)((x) - (base)) < ((uint)(size)) )


#define HASBIT(x,y)    (((x) & (1 << (y))) != 0)
#define SETBIT(x,y)    ((x) |=  (1 << (y)))
#define CLRBIT(x,y)    ((x) &= ~(1 << (y)))
#define TOGGLEBIT(x,y) ((x) ^=  (1 << (y)))

// checking more bits. Maybe unneccessary, but easy to use
#define HASBITS(x,y) ((x) & (y))
#define SETBITS(x,y) ((x) |= (y))
#define CLRBITS(x,y) ((x) &= ~(y))

#define GENERAL_SPRITE_COLOR(color) ( ((color) + PALETTE_RECOLOR_START) << PALETTE_SPRITE_START)
#define PLAYER_SPRITE_COLOR(owner) ( GENERAL_SPRITE_COLOR(_player_colors[owner]))
#define SPRITE_PALETTE(x) ((x) | PALETTE_MODIFIER_COLOR)

extern const byte _ffb_64[128];
/* Returns the position of the first bit that is not zero, counted from the
 * left. Ie, 10110100 returns 2, 00000001 returns 0, etc. When x == 0 returns
 * 0.
 */
#define FIND_FIRST_BIT(x) _ffb_64[(x)]
/* Returns x with the first bit that is not zero, counted from the left, set
 * to zero. So, 10110100 returns 10110000, 00000001 returns 00000000, etc.
 */
#define KILL_FIRST_BIT(x) _ffb_64[(x)+64]

static inline int FindFirstBit2x64(int value)
{
/*
	int i = 0;
	if ( (byte) value == 0) {
		i += 8;
		value >>= 8;
	}
	return i + FIND_FIRST_BIT(value & 0x3F);

Faster ( or at least cleaner ) implementation below?
*/
	if (GB(value, 0, 8) == 0) {
		return FIND_FIRST_BIT(GB(value, 8, 6)) + 8;
	} else {
		return FIND_FIRST_BIT(GB(value, 0, 6));
	}

}

static inline int KillFirstBit2x64(int value)
{
	if (GB(value, 0, 8) == 0) {
		return KILL_FIRST_BIT(GB(value, 8, 6)) << 8;
	} else {
		return value & (KILL_FIRST_BIT(GB(value, 0, 6)) | 0x3F00);
	}
}

/** returns true if value a has only one bit set to 1 */
#define HAS_SINGLE_BIT(a) ( ((a) & ((a) - 1)) == 0)

/* [min,max), strictly less than */
#define IS_BYTE_INSIDE(a,min,max) ((byte)((a)-(min)) < (byte)((max)-(min)))
#define IS_INT_INSIDE(a,min,max) ((uint)((a)-(min)) < (uint)((max)-(min)))


#define CHANCE16(a,b) ((uint16)Random() <= (uint16)((65536 * (a)) / (b)))
#define CHANCE16R(a,b,r) ((uint16)(r=Random()) <= (uint16)((65536 * (a)) / (b)))
#define CHANCE16I(a,b,v) ((uint16)(v) <= (uint16)((65536 * (a)) / (b)))


#define for_each_bit(_i, _b)            \
	for (_i = 0; _b != 0; _i++, _b >>= 1) \
		if (_b & 1)

#define abs myabs


static inline int intxchg_(int *a, int b) { int t = *a; *a = b; return t; }
#define intswap(a,b) ((b) = intxchg_(&(a), (b)))
static inline int uintxchg_(uint *a, uint b) { uint t = *a; *a = b; return t; }
#define uintswap(a,b) ((b) = uintxchg_(&(a), (b)))

static inline int myabs(int a) { if (a<0) a = -a; return a; }
static inline int64 myabs64(int64 a) { if (a<0) a = -a; return a; }

static inline void swap_byte(byte *a, byte *b) { byte t = *a; *a = *b; *b = t; }
static inline void swap_uint16(uint16 *a, uint16 *b) { uint16 t = *a; *a = *b; *b = t; }
static inline void swap_int16(int16 *a, int16 *b) { int16 t = *a; *a = *b; *b = t; }
static inline void swap_uint32(uint32 *a, uint32 *b) { uint32 t = *a; *a = *b; *b = t; }
static inline void swap_int32(int32 *a, int32 *b) { int32 t = *a; *a = *b; *b = t; }
static inline void swap_tile(TileIndex *a, TileIndex *b) { TileIndex t = *a; *a = *b; *b = t; }


static inline uint16 ReadLE16Aligned(const void* x)
{
	return FROM_LE16(*(const uint16*)x);
}

static inline uint16 ReadLE16Unaligned(const void* x)
{
#ifdef OTTD_ALIGNMENT
	return ((const byte*)x)[0] | ((const byte*)x)[1] << 8;
#else
	return FROM_LE16(*(const uint16*)x);
#endif
}


/**
 * ROtate x Left/Right by n (must be >= 0)
 * @note Assumes a byte has 8 bits
 */
#define ROL(x, n) ((x) << (n) | (x) >> (sizeof(x) * 8 - (n)))
#define ROR(x, n) ((x) >> (n) | (x) << (sizeof(x) * 8 - (n)))

/**
 * Return the smallest multiple of n equal or greater than x
 * @note n must be a power of 2
 */
#define ALIGN(x, n) (((x) + (n) - 1) & ~((n) - 1))

/** return the largest value that can be entered in a variable.
 *  known to work for uint32.
 *  used by TGP to set the max value of the _patches.generation_seed in its definition
 */
#define MAX_UVALUE(type) ((type)~(type)0)

#endif /* MACROS_H */
