/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file random_func.hpp Pseudo random number generator. */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**
 * Scale a uint32_t number to be within the range [0,\a limit).
 * @param value The value to scale.
 * @param limit The limit to scale to.
 * @return The scaled value.
 */
static constexpr uint32_t ScaleToLimit(uint32_t value, uint32_t limit)
{
	return ((uint64_t)value * (uint64_t)limit) >> 32;
}

/**
 * Structure to encapsulate the pseudo random number generators.
 */
struct Randomizer {
	/** The state of the randomizer */
	uint32_t state[2];

	uint32_t Next();
	void SetSeed(uint32_t seed);

	/**
	 * Generate the next pseudo random number scaled to \a limit, excluding \a limit
	 * itself.
	 * @param limit Limit of the range to be generated from.
	 * @return Random number in [0,\a limit)
	 */
	inline uint32_t Next(uint32_t limit) { return ScaleToLimit(this->Next(), limit); }
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
inline void SaveRandomSeeds(SavedRandomSeeds *storage)
{
	storage->random = _random;
	storage->interactive_random = _interactive_random;
}

/**
 * Restores previously saved seeds
 * @param storage Storage where SaveRandomSeeds() stored th seeds
 */
inline void RestoreRandomSeeds(const SavedRandomSeeds &storage)
{
	_random = storage.random;
	_interactive_random = storage.interactive_random;
}

void SetRandomSeed(uint32_t seed);
#ifdef RANDOM_DEBUG
	uint32_t Random(const std::source_location location = std::source_location::current());
#else
	inline uint32_t Random([[maybe_unused]] const std::source_location location = std::source_location::current())
	{
		return _random.Next();
	}
#endif

/**
 * Pick a random number between 0 and \a limit - 1, inclusive. That means 0
 * can be returned and \a limit - 1 can be returned, but \a limit can not be
 * returned.
 * @param limit Limit for the range to be picked from.
 * @return A random number in [0,\a limit).
 */
inline uint32_t RandomRange(uint32_t limit, const std::source_location location = std::source_location::current())
{
	return ScaleToLimit(Random(location), limit);
}

inline uint32_t InteractiveRandom()
{
	return _interactive_random.Next();
}

inline uint32_t InteractiveRandomRange(uint32_t limit)
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
inline bool Chance16I(const uint32_t a, const uint32_t b, const uint32_t r)
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
inline bool Chance16(const uint32_t a, const uint32_t b, const std::source_location location = std::source_location::current())
{
	return Chance16I(a, b, Random(location));
}

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
inline bool Chance16R(const uint32_t a, const uint32_t b, uint32_t &r, const std::source_location location = std::source_location::current())
{
	r = Random(location);
	return Chance16I(a, b, r);
}

void RandomBytesWithFallback(std::span<uint8_t> buf);

#endif /* RANDOM_FUNC_HPP */
