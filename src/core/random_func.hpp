/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file random_func.hpp Pseudo random number generator. */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

#if defined(__APPLE__)
	/* Apple already has Random declared */
#	define Random OTTD_Random
#endif /* __APPLE__ */

/**
 * Structure to encapsulate the pseudo random number generators.
 */
struct Randomizer {
	/** The state of the randomizer */
	uint32_t state[2];

	uint32_t Next();
	uint32_t Next(uint32_t limit);
	void SetSeed(uint32_t seed);
};
extern Randomizer _random; ///< Random used in the game state calculations
extern Randomizer _interactive_random; ///< Random used everywhere else, where it does not (directly) influence the game state

/** Stores the state of all random number generators */
struct SavedRandomSeeds {
	Randomizer random;
	Randomizer interactive_random;
};

/**
 * Saves the current seeds
 * @param storage Storage for saving
 */
static inline void SaveRandomSeeds(SavedRandomSeeds *storage)
{
	storage->random = _random;
	storage->interactive_random = _interactive_random;
}

/**
 * Restores previously saved seeds
 * @param storage Storage where SaveRandomSeeds() stored th seeds
 */
static inline void RestoreRandomSeeds(const SavedRandomSeeds &storage)
{
	_random = storage.random;
	_interactive_random = storage.interactive_random;
}

void SetRandomSeed(uint32_t seed);
#ifdef RANDOM_DEBUG
#	ifdef __APPLE__
#		define OTTD_Random() DoRandom(__LINE__, __FILE__)
#	else
#		define Random() DoRandom(__LINE__, __FILE__)
#	endif
	uint32_t DoRandom(int line, const char *file);
#	define RandomRange(limit) DoRandomRange(limit, __LINE__, __FILE__)
	uint32_t DoRandomRange(uint32_t limit, int line, const char *file);
#else
	static inline uint32_t Random()
	{
		return _random.Next();
	}

	/**
	 * Pick a random number between 0 and \a limit - 1, inclusive. That means 0
	 * can be returned and \a limit - 1 can be returned, but \a limit can not be
	 * returned.
	 * @param limit Limit for the range to be picked from.
	 * @return A random number in [0,\a limit).
	 */
	static inline uint32_t RandomRange(uint32_t limit)
	{
		return _random.Next(limit);
	}
#endif

static inline uint32_t InteractiveRandom()
{
	return _interactive_random.Next();
}

static inline uint32_t InteractiveRandomRange(uint32_t limit)
{
	return _interactive_random.Next(limit);
}

/**
 * Checks if a given randomize-number is below a given probability.
 *
 * This function is used to check if the given probability by the fraction of (a/b)
 * is greater than low 16 bits of the given randomize-number r.
 *
 * Do not use this function twice on the same random 16 bits as it will yield
 * the same result. One can use a random number for two calls to Chance16I,
 * where one call sends the low 16 bits and the other the high 16 bits.
 *
 * @param a The numerator of the fraction
 * @param b The denominator of the fraction, must of course not be null
 * @param r The given randomize-number
 * @return True if the probability given by r is less or equal to (a/b)
 */
static inline bool Chance16I(const uint a, const uint b, const uint32_t r)
{
	assert(b != 0);
	return (((uint16_t)r * b + b / 2) >> 16) < a;
}

/**
 * Flips a coin with given probability.
 *
 * This function returns true with (a/b) probability.
 *
 * @see Chance16I()
 * @param a The nominator of the fraction
 * @param b The denominator of the fraction
 * @return True with (a/b) probability
 */
#ifdef RANDOM_DEBUG
#	define Chance16(a, b) Chance16I(a, b, DoRandom(__LINE__, __FILE__))
#else
static inline bool Chance16(const uint a, const uint b)
{
	return Chance16I(a, b, Random());
}
#endif /* RANDOM_DEBUG */

/**
 * Flips a coin with a given probability and saves the randomize-number in a variable.
 *
 * This function uses the same parameters as Chance16. The third parameter
 * must be a variable the randomize-number from Random() is saved in.
 *
 * The low 16 bits of r will already be used and can therefore not be passed to
 * Chance16I. One can only send the high 16 bits to Chance16I.
 *
 * @see Chance16I()
 * @param a The numerator of the fraction
 * @param b The denominator of the fraction
 * @param r The variable to save the randomize-number from Random()
 * @return True in (a/b) percent
 */
#ifdef RANDOM_DEBUG
#	define Chance16R(a, b, r) (r = DoRandom(__LINE__, __FILE__), Chance16I(a, b, r))
#else
static inline bool Chance16R(const uint a, const uint b, uint32_t &r)
{
	r = Random();
	return Chance16I(a, b, r);
}
#endif /* RANDOM_DEBUG */

#endif /* RANDOM_FUNC_HPP */
