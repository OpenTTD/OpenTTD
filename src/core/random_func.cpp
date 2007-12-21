/* $Id$ */

/** @file random_func.cpp */

#include "../stdafx.h"
#include "../variables.h"
#include "random_func.hpp"
#include "bitmath_func.hpp"

uint32 InteractiveRandom()
{
	const uint32 s = _random_seeds[1][0];
	const uint32 t = _random_seeds[1][1];

	_random_seeds[1][0] = s + ROR(t ^ 0x1234567F, 7) + 1;
	return _random_seeds[1][1] = ROR(s, 3) - 1;
}

uint InteractiveRandomRange(uint max)
{
	return GB(InteractiveRandom(), 0, 16) * max >> 16;
}

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

static uint32 _mt_state[N+1];     // _mt_state vector + 1 extra to not violate ANSI C
static uint32 *_mt_next;          // _mt_next random value is computed from here
static int    _mt_left = -1;      // can *_mt_next++ this many times before reloading

void SetRandomSeed(register uint32 seed)
{
	register uint32 *s = _mt_state;
	_mt_left = 0;

	seed |= 1U;
	seed &= 0xFFFFFFFFU;

	*s = seed;

	for (register uint i = N; i != 0; i--) {
		seed *= 69069U;
		*s++;
		*s = seed & 0xFFFFFFFFU;
	}
}

static uint32 ReloadRandom()
{
	if (_mt_left < -1) SetRandomSeed(4357U);

	_mt_left = N - 1;
	_mt_next = _mt_state + 1;

	register uint32 *p0 = _mt_state;
	register uint32 *p2 = _mt_state + 2;
	register uint32 *pM = _mt_state + M;

	register uint32 s0 = _mt_state[0];
	register uint32 s1 = _mt_state[1];

	register uint i = 0;

	for (i = (N - M + 1); i != 0; i--) {
		s0 = s1;
		s1 = *p2;
		*p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
		*p0++;
		*p2++;
		*pM++;
	}

	pM = _mt_state;

	for (i = M; i != 0; i--) {
		s0 = s1;
		s1 = *p2;
		*p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
		*p0++;
		*p2++;
		*pM++;
	}

	s1 = _mt_state[0];
	*p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

	s1 ^= (s1 >> 11);
	s1 ^= (s1 <<  7) & 0x9D2C5680U;
	s1 ^= (s1 << 15) & 0xEFC60000U;
	s1 ^= (s1 >> 18);
	return s1;
}

uint32 Random()
{
	_mt_left--;
	if (_mt_left < 0) return ReloadRandom();

	uint32 y = *_mt_next;
	*_mt_next++;

	y ^= (y >> 11);
	y ^= (y <<  7) & 0x9D2C5680U;
	y ^= (y << 15) & 0xEFC60000U;
	y ^= (y >> 18);
	return y;
}

#else /* MERSENNE_TWISTER */
void SetRandomSeed(uint32 seed)
{
	_random_seeds[0][0] = seed;
	_random_seeds[0][1] = seed;
	_random_seeds[1][0] = seed * 0x1234567;
	_random_seeds[1][1] = _random_seeds[1][0];
}

#ifdef RANDOM_DEBUG
#include "../network/network_data.h"
uint32 DoRandom(int line, const char *file)
{
	if (_networking && (DEREF_CLIENT(0)->status != STATUS_INACTIVE || !_network_server))
		printf("Random [%d/%d] %s:%d\n",_frame_counter, (byte)_current_player, file, line);
#else /* RANDOM_DEBUG */
uint32 Random()
{
#endif /* RANDOM_DEBUG */
	const uint32 s = _random_seeds[0][0];
	const uint32 t = _random_seeds[0][1];

	_random_seeds[0][0] = s + ROR(t ^ 0x1234567F, 7) + 1;
	return _random_seeds[0][1] = ROR(s, 3) - 1;
}
#endif /* MERSENNE_TWISTER */

#if defined(RANDOM_DEBUG) && !defined(MERSENNE_TWISTER)
uint DoRandomRange(uint max, int line, const char *file)
{
	return GB(DoRandom(line, file), 0, 16) * max >> 16;
}
#else /* RANDOM_DEBUG & !MERSENNE_TWISTER */
uint RandomRange(uint max)
{
	return GB(Random(), 0, 16) * max >> 16;
}
#endif /* RANDOM_DEBUG & !MERSENNE_TWISTER */
