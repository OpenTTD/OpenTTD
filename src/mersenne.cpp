/* $Id$ */

/** @file mersenne.cpp */

#include "stdafx.h"
#include "openttd.h"

#ifdef MERSENNE_TWISTER

// Source code for Mersenne Twister.
// A Random number generator with much higher quality random numbers.

#define N              (624)                 // length of _mt_state vector
#define M              (397)                 // a period parameter
#define K              (0x9908B0DFU)         // a magic constant
#define hiBit(u)       ((u) & 0x80000000U)   // mask all but highest   bit of u
#define loBit(u)       ((u) & 0x00000001U)   // mask all but lowest    bit of u
#define loBits(u)      ((u) & 0x7FFFFFFFU)   // mask     the highest   bit of u
#define mixBits(u, v)  (hiBit(u)|loBits(v))  // move hi bit of u to hi bit of v

static uint32   _mt_state[N+1];     // _mt_state vector + 1 extra to not violate ANSI C
static uint32   *_mt_next;          // _mt_next random value is computed from here
static int      _mt_left = -1;      // can *_mt_next++ this many times before reloading

void SeedMT(uint32 seed)
{
    register uint32 x = (seed | 1U) & 0xFFFFFFFFU, *s = _mt_state;
    register int    j;

    for (_mt_left=0, *s++=x, j=N; --j;
        *s++ = (x*=69069U) & 0xFFFFFFFFU);
 }


static uint32 ReloadMT(void)
 {
    register uint32 *p0=_mt_state, *p2=_mt_state+2, *pM=_mt_state+M, s0, s1;
    register int    j;

    if (_mt_left < -1)
        SeedMT(4357U);

    _mt_left=N-1, _mt_next=_mt_state+1;

    for (s0=_mt_state[0], s1=_mt_state[1], j=N-M+1; --j; s0=s1, s1=*p2++)
        *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

    for (pM=_mt_state, j=M; --j; s0=s1, s1=*p2++)
        *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

    s1=_mt_state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 <<  7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return(s1 ^ (s1 >> 18));
 }


uint32 RandomMT(void)
{
	uint32 y;

	if (--_mt_left < 0)
		return ReloadMT();

	y  = *_mt_next++;
	y ^= (y >> 11);
	y ^= (y <<  7) & 0x9D2C5680U;
	y ^= (y << 15) & 0xEFC60000U;
	return y ^ (y >> 18);
}
#else

void SeedMT(uint32 seed) {}

#endif /* MERSENNE_TWISTER */
