/* $Id$ */

/** @file random_func.hpp */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

#if defined(__APPLE__)
	/* Apple already has Random declared */
	#define Random OTTD_Random
#endif /* __APPLE__ */

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

/**
 * Checks if a given randomize-number is below a given probability.
 *
 * This function is used to check if the given probability by the fraction of (a/b)
 * is greater than low 16 bits of the given randomize-number v.
 *
 * Do not use this function twice on the same random 16 bits as it will yield
 * the same result. One can use a random number for two calls to Chance16I,
 * where one call sends the low 16 bits and the other the high 16 bits.
 *
 * @param a The numerator of the fraction
 * @param b The denominator of the fraction, must of course not be null
 * @param r The given randomize-number
 * @return True if v is less or equals (a/b)
 */
static inline bool Chance16I(const uint a, const uint b, const uint32 r)
{
	assert(b != 0);
	return (uint16)r < (uint16)((a << 16) / b);
}

/**
 * Flips a coin with a given probability.
 *
 * This macro can be used to get true or false randomized according to a
 * given probability. The parameter a and b create a percent value with
 * (a/b). The macro returns true in (a/b) percent.
 *
 * @see Chance16I()
 * @param a The numerator of the fraction
 * @param b The denominator of the fraction
 * @return True in (a/b) percent
 */
static inline bool Chance16(const uint a, const uint b)
{
	return Chance16I(a, b, Random());
}

/**
 * Flips a coin with a given probability and saves the randomize-number in a variable.
 *
 * This function uses the same parameters as Chance16. The third parameter
 * must be a variable the randomize-number from Random() is saved in.
 *
 * The low 16 bits of r will already be used and can therefor not be passed to
 * Chance16I. One can only send the high 16 bits to Chance16I.
 *
 * @see Chance16I()
 * @param a The numerator of the fraction
 * @param b The denominator of the fraction
 * @param r The variable to save the randomize-number from Random()
 * @return True in (a/b) percent
 */
static inline bool Chance16R(const uint a, const uint b, uint32 &r)
{
	r = Random();
	return Chance16I(a, b, r);
}

extern uint32 _random_seeds[2][2];

#endif /* RANDOM_FUNC_HPP */
