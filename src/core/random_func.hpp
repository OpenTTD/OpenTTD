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

/**
 * Structure to encapsulate the pseudo random number generators.
 */
struct Randomizer {
	/** The state of the randomizer */
	uint32 state[2];

	/**
	 * Generate the next pseudo random number
	 * @return the random number
	 */
	uint32 Next();

	/**
	 * Generate the next pseudo random number scaled to max
	 * @param max the maximum value of the returned random number
	 * @return the random number
	 */
	uint32 Next(uint16 max);

	/**
	 * (Re)set the state of the random number generator.
	 * @param seed the new state
	 */
	void SetSeed(uint32 seed);
};
extern Randomizer _random; ///< Random used in the game state calculations
extern Randomizer _interactive_random; ///< Random used every else where is does not (directly) influence the game state

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	static inline uint32 Random() { return _random.Next(); }
	static inline uint32 RandomRange(uint16 max) { return _random.Next(max); }
#endif

static inline uint32 InteractiveRandom() { return _interactive_random.Next(); }
static inline uint32 InteractiveRandomRange(uint16 max) { return _interactive_random.Next(max); }

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
	return (uint16)r < (uint16)(((a << 16) + b / 2) / b);
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

#endif /* RANDOM_FUNC_HPP */
