#ifndef MACROS_H
#define MACROS_H

#include "map.h"

#define MAX_INT 0x7FFFFFFF

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


static inline int clamp(int a, int min, int max) { if (a <= min) return min; if (a >= max) return max; return a; }
static inline int clamp2(int a, int min, int max) { if (a <= min) a=min; if (a >= max) a=max; return a; }
static inline bool int32_add_overflow(int32 a, int32 b) { return (int32)(a^b)>=0 && (int32)(a^(a+b))<0; }
static inline bool int32_sub_overflow(int32 a, int32 b) { return (int32)(a^b)<0 && (int32)(a^(a-b))<0; }

static inline bool str_eq(const byte *a, const byte *b)
{
	int i=0;
	while (a[i] == b[i]) {
		if (a[i] == 0)
			return true;
		i++;
	}
	return false;
}

// Will crash if strings are equal
static inline bool str_is_below(byte *a, byte *b) {
	while (*a <= *b) {
		if (*a < *b) return true;
		a++;
		b++;
	}
	return false;
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
	return (int32)(((int64)(a) * (int64)(b)));
}

/* OPT: optimized into an unsigned comparison */
//#define IS_INSIDE_1D(x, base, size) ((x) >= (base) && (x) < (base) + (size))
#define IS_INSIDE_1D(x, base, size) ( (uint)((x) - (base)) < ((uint)(size)) )

#define LANDSCAPE_SIZE_FACTOR 1

enum {
	CORRECT_Z_BITS = 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7
};
#define CORRECT_Z(tileh) (CORRECT_Z_BITS & (1 << tileh))

#define TILE_ASSERT(x) assert( TILE_MASK(x) == (x) );

//#define REMADP_COORDS(x,y,z) { int t = x; x = (y-t)*2; y+=t-z; }

#define PACK_POINT(x,y) ((x) | ((y) << 16))
#define UNPACK_POINT_X(p) ((uint16)(p))
#define UNPACK_POINT_Y(p) ((uint16)(p>>16))

#define PACK_PPOINT(p) PACK_POINT((p).x, (p).y)

#define HASBIT(x,y)    ((x) &   (1 << (y)))
#define SETBIT(x,y)    ((x) |=  (1 << (y)))
#define CLRBIT(x,y)    ((x) &= ~(1 << (y)))
#define TOGGLEBIT(x,y) ((x) ^=  (1 << (y)))

// checking more bits. Maybe unneccessary, but easy to use
#define HASBITS(x,y) ((x) & (y))
#define SETBITS(x,y) ((x) |= (y))
#define CLRBITS(x,y) ((x) &= ~(y))

#define PLAYER_SPRITE_COLOR(owner) ((_player_colors[owner] << 16) + 0x3070000)
#define SPRITE_PALETTE(x) ((x) + 0x8000)

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
	int i = 0;
	if ( (byte) value == 0) {
		i += 8;
		value >>= 8;
	}
	return i + FIND_FIRST_BIT(value & 0x3F);
}


/* [min,max), strictly less than */
#define IS_BYTE_INSIDE(a,min,max) ((byte)((a)-(min)) < (byte)((max)-(min)))
#define IS_INT_INSIDE(a,min,max) ((uint)((a)-(min)) < (uint)((max)-(min)))


#define CHANCE16(a,b) ((uint16)Random() <= (uint16)((65536 * a) / b))
#define CHANCE16R(a,b,r) ((uint16)(r=Random()) <= (uint16)((65536 * a) / b))
#define CHANCE16I(a,b,v) ((uint16)(v) <= (uint16)((65536 * a) / b))

#define BEGIN_TILE_LOOP(var,w,h,tile)		\
		{int h_cur = h;									\
		uint var = tile;									\
		do {														\
			int w_cur = w;								\
			do {

#define END_TILE_LOOP(var,w,h,tile)			\
			} while (++var, --w_cur != 0);						\
		} while (var += TILE_XY(0,1) - (w), --h_cur != 0);}


#define for_each_bit(_i,_b)										\
	for(_i=0; _b!=0; _i++,_b>>=1)								\
		if (_b&1)

#define assert_array(i,j) assert(i < lengthof(j))

#define abs myabs


static inline int intxchg_(int *a, int b) { int t = *a; *a = b; return t; }
#define intxchg(a,b) intxchg_(&(a), (b))
#define intswap(a,b) ((b) = intxchg_(&(a), (b)))

static inline int myabs(int a) { if (a<0) a = -a; return a; }
static inline int64 myabs64(int64 a) { if (a<0) a = -a; return a; }

static inline void swap_byte(byte *a, byte *b) { byte t = *a; *a = *b; *b = t; }
static inline void swap_uint16(uint16 *a, uint16 *b) { uint16 t = *a; *a = *b; *b = t; }
static inline void swap_int16(int16 *a, int16 *b) { int16 t = *a; *a = *b; *b = t; }
static inline void swap_int32(int32 *a, int32 *b) { int32 t = *a; *a = *b; *b = t; }
static inline void swap_tile(TileIndex *a, TileIndex *b) { TileIndex t = *a; *a = *b; *b = t; }



#if defined(TTD_LITTLE_ENDIAN)
#	define READ_LE_UINT16(b) (*(const uint16*)(b))
#	define ADD_WORD(x) (x)&0xFF, ((x) >> 8)&0xFF
#	define ADD_DWORD(x) (x)&0xFF, ((x) >> 8)&0xFF, ((x) >> 16)&0xFF, ((x) >> 24)&0xFF
#elif defined(TTD_BIG_ENDIAN)
	static inline uint16 READ_LE_UINT16(const void *b) {
		return ((const byte*)b)[0] + (((const byte*)b)[1] << 8);
	}
#	define ADD_WORD(x) ((x) >> 8)&0xFF, (x)&0xFF
#	define ADD_DWORD(x) ((x) >> 24)&0xFF, ((x) >> 16)&0xFF, ((x) >> 8)&0xFF,  (x)&0xFF
#endif

static inline void WRITE_LE_UINT16(void *b, uint16 x) {
	((byte*)b)[0] = (byte)x;
	((byte*)b)[1] = (byte)(x >> 8);
}

#define MAX_DETOUR 6

#endif /* MACROS_H */
