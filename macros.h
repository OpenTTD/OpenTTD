#ifndef MACROS_H
#define MACROS_H

#define MAX_INT 0x7FFFFFFF

#ifdef min 
#undef min
#endif

#ifdef max
#undef max
#endif

static INLINE int min(int a, int b) { if (a <= b) return a; return b; }
static INLINE int max(int a, int b) { if (a >= b) return a; return b; }

static INLINE uint minu(uint a, uint b) { if (a <= b) return a; return b; }
static INLINE uint maxu(uint a, uint b) { if (a >= b) return a; return b; }


static INLINE int clamp(int a, int min, int max) { if (a <= min) return min; if (a >= max) return max; return a; }
static INLINE int clamp2(int a, int min, int max) { if (a <= min) a=min; if (a >= max) a=max; return a; }
static INLINE bool int32_add_overflow(int32 a, int32 b) { return (int32)(a^b)>=0 && (int32)(a^(a+b))<0; }
static INLINE bool int32_sub_overflow(int32 a, int32 b) { return (int32)(a^b)<0 && (int32)(a^(a-b))<0; }

static INLINE bool str_eq(const byte *a, const byte *b)
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
static INLINE bool str_is_below(byte *a, byte *b) {		
	while (*a <= *b) {
		if (*a < *b) return true;
		a++;
		b++;
	}
	return false;
}


static INLINE int32 BIGMULSS(int32 a, int32 b, int shift) {
	return (int32)(((int64)(a) * (int64)(b)) >> (shift));
}

static INLINE uint32 BIGMULUS(uint32 a, uint32 b, int shift) {
	return (uint32)(((uint64)(a) * (uint64)(b)) >> (shift));
}

static INLINE int64 BIGMULS(int32 a, int32 b) {
	return (int32)(((int64)(a) * (int64)(b)));
}

/* OPT: optimized into an unsigned comparison */
//#define IS_INSIDE_1D(x, base, size) ((x) >= (base) && (x) < (base) + (size))
#define IS_INSIDE_1D(x, base, size) ( (uint)((x) - (base)) < ((uint)(size)) )

#define TILE_X_BITS 8
#define TILE_Y_BITS 8
#define LANDSCAPE_SIZE_FACTOR 1

#define TILE_FROM_XY(x,y) (int)((((y) >> 4) << TILE_X_BITS) + ((x) >> 4))
#define TILE_XY(x,y) (int)(((y) << TILE_X_BITS) + (x))

#define IS_TILETYPE(_t_, _s_) (_map_type_and_height[(_t_)] >> 4 == (_s_))
#define GET_TILETYPE(_t_) (_map_type_and_height[(_t_)] >> 4)
#define GET_TILEHEIGHT(_t_) ((_map_type_and_height[_t_] & 0xF) * 8)

enum {
	CORRECT_Z_BITS = 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7
};
#define CORRECT_Z(tileh) (CORRECT_Z_BITS & (1 << tileh))

#define TILES_X (1 << TILE_X_BITS)
#define TILES_Y (1 << TILE_Y_BITS)

#define TILE_X_MAX (TILES_X-1)
#define TILE_Y_MAX (TILES_Y-1)

#define TILE_ASSERT(x) assert( TILE_MASK(x) == (x) );

extern uint SafeTileAdd(uint x, int add, const char *exp, const char *file, int line);

#if !defined(_DEBUG) || TILE_X_BITS != 8
#	define TILE_ADD(x,y) ((x)+(y))
#else
#	if defined(__GNUC__)
#		define TILE_ADD(x,y) (SafeTileAdd((x),(y), "??",  __FILE__, __LINE__))
#	else
#		define TILE_ADD(x,y) (SafeTileAdd((x),(y), #x ## ", " ## #y,  __FILE__, __LINE__))
#	endif
#endif

#define TILE_ADDXY(tile, x, y) TILE_ADD(tile, TILE_XY(x,y))

#if TILE_X_BITS == 8
#define GET_TILE_X(t) (uint)((byte)(t))
#define GET_TILE_Y(t) (uint)((t) >> 8)
#define TILE_MASK(x) ((uint16)(x))
#else
#define GET_TILE_X(t) (uint)((t) & ((1 << TILE_X_BITS)-1))
#define GET_TILE_Y(t) (uint)((t) >> TILE_X_BITS)
#define TILE_MASK(x) (int)((x) & ((1 << (TILE_X_BITS + TILE_Y_BITS))-1))
#endif

//#define REMADP_COORDS(x,y,z) { int t = x; x = (y-t)*2; y+=t-z; }

#define PACK_POINT(x,y) ((x) | ((y) << 16))
#define UNPACK_POINT_X(p) ((uint16)(p))
#define UNPACK_POINT_Y(p) ((uint16)(p>>16))

#define PACK_PPOINT(p) PACK_POINT((p).x, (p).y)

#define HASBIT(x,y) ((x) & (1 << (y)))
#define SETBIT(x,y) ((x) |= (1 << (y)))
#define CLRBIT(x,y) ((x) &= ~(1 << (y)))

// checking more bits. Maybe unneccessary, but easy to use
#define HASBITS(x,y) ((x) & (y))
#define SETBITS(x,y) ((x) |= (y))
#define CLRBITS(x,y) ((x) &= ~(y))

#define PLAYER_SPRITE_COLOR(owner) ((_player_colors[owner] << 16) + 0x3070000)
#define SPRITE_PALETTE(x) ((x) + 0x8000)

extern const byte _ffb_64[128];
#define FIND_FIRST_BIT(x) _ffb_64[(x)]
#define KILL_FIRST_BIT(x) _ffb_64[(x)+64]

static INLINE int FindFirstBit2x64(int value)
{
	int i = 0;
	if ( (byte) value == 0) {
		i += 8;
		value >>= 8;
	}
	return i + FIND_FIRST_BIT(value & 0x3F);
}


#if TILE_X_BITS + TILE_Y_BITS <= 16
	typedef uint16 TileIndex;	
	typedef int16 TileIndexDiff;
#else
	typedef uint32 TileIndex;
	typedef int32 TileIndexDiff;
#endif

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


static INLINE int intxchg_(int *a, int b) { int t = *a; *a = b; return t; }
#define intxchg(a,b) intxchg_(&(a), (b))
#define intswap(a,b) ((b) = intxchg_(&(a), (b)))

static INLINE int myabs(int a) { if (a<0) a = -a; return a; }

static INLINE void swap_byte(byte *a, byte *b) { byte t = *a; *a = *b; *b = t; }
static INLINE void swap_uint16(uint16 *a, uint16 *b) { uint16 t = *a; *a = *b; *b = t; }
static INLINE void swap_int16(int16 *a, int16 *b) { int16 t = *a; *a = *b; *b = t; }
static INLINE void swap_tile(TileIndex *a, TileIndex *b) { TileIndex t = *a; *a = *b; *b = t; }



#if defined(TTD_LITTLE_ENDIAN)
#	define READ_LE_UINT16(b) (*(uint16*)(b))
#	define ADD_WORD(x) (x)&0xFF, ((x) >> 8)&0xFF
#	define ADD_DWORD(x) (x)&0xFF, ((x) >> 8)&0xFF, ((x) >> 16)&0xFF, ((x) >> 24)&0xFF
#elif defined(TTD_BIG_ENDIAN)
	static INLINE uint16 READ_LE_UINT16(const void *b) {
		return ((byte*)b)[0] + (((byte*)b)[1] << 8);
	}
#	define ADD_WORD(x) ((x) >> 8)&0xFF, (x)&0xFF 
#	define ADD_DWORD(x) ((x) >> 24)&0xFF, ((x) >> 16)&0xFF, ((x) >> 8)&0xFF,  (x)&0xFF
#endif

static INLINE void WRITE_LE_UINT16(const void *b, uint16 x) {
	((byte*)b)[0] = (byte)x;
	((byte*)b)[1] = (byte)(x >> 8);
}

#endif /* MACROS_H */
