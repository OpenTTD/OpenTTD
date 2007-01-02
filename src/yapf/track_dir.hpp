/* $Id$ */

#ifndef  TRACK_DIR_HPP
#define  TRACK_DIR_HPP

EXTERN_C_BEGIN
#include "../tile.h"
#include "../openttd.h"
#include "../map.h"
#include "../rail.h"
EXTERN_C_END

/** Helpers to allow to work with enum as with type safe bit set in C++ */
#define DECLARE_ENUM_AS_BIT_MASK(mask_t) \
	FORCEINLINE mask_t operator | (mask_t m1, mask_t m2) {return (mask_t)((int)m1 | m2);} \
	FORCEINLINE mask_t operator & (mask_t m1, mask_t m2) {return (mask_t)((int)m1 & m2);} \
	FORCEINLINE mask_t operator ^ (mask_t m1, mask_t m2) {return (mask_t)((int)m1 ^ m2);} \
	FORCEINLINE mask_t& operator |= (mask_t& m1, mask_t m2) {m1 = m1 | m2; return m1;} \
	FORCEINLINE mask_t& operator &= (mask_t& m1, mask_t m2) {m1 = m1 & m2; return m1;} \
	FORCEINLINE mask_t& operator ^= (mask_t& m1, mask_t m2) {m1 = m1 ^ m2; return m1;} \
	FORCEINLINE mask_t operator ~(mask_t m) {return (mask_t)(~(int)m);}

/** probably redundant enum combining operators (as we have conversion functions) */
#define DECLARE_ENUM_AS_BIT_INDEX(idx_t, mask_t) \
	FORCEINLINE mask_t operator << (int m, idx_t i) {return (mask_t)(m << (int)i);} \
	FORCEINLINE mask_t operator << (mask_t m, int i) {return (mask_t)(((int)m) << i);} \
	FORCEINLINE mask_t operator >> (mask_t m, int i) {return (mask_t)(((int)m) >> i);}

DECLARE_ENUM_AS_BIT_MASK(TrackBits)
DECLARE_ENUM_AS_BIT_INDEX(Track, TrackBits)

DECLARE_ENUM_AS_BIT_MASK(TrackdirBits)
DECLARE_ENUM_AS_BIT_INDEX(Trackdir, TrackdirBits)

#endif /* TRACK_DIR_HPP */
