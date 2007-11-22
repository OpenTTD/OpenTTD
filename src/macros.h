/* $Id$ */

/** @file macros.h */

#ifndef MACROS_H
#define MACROS_H

#include "core/math_func.hpp"

/**
 * Fetch n bits from x, started at bit s.
 *
 * This function can be used to fetch n bits from the value x. The
 * s value set the startposition to read. The startposition is
 * count from the LSB and starts at 0. The result starts at a
 * LSB, as this isn't just an and-bitmask but also some
 * bit-shifting operations. GB(0xFF, 2, 1) will so
 * return 0x01 (0000 0001) instead of
 * 0x04 (0000 0100).
 *
 * @param x The value to read some bits.
 * @param s The startposition to read some bits.
 * @param n The number of bits to read.
 * @return The selected bits, aligned to a LSB.
 */
template<typename T> static inline uint GB(const T x, const uint8 s, const uint8 n)
{
	return (x >> s) & ((1U << n) - 1);
}

/** Set n bits from x starting at bit s to d
 *
 * This function sets n bits from x which started as bit s to the value of
 * d. The parameters x, s and n works the same as the parameters of
 * #GB. The result is saved in x again. Unused bits in the window
 * provided by n are set to 0 if the value of b isn't "big" enough.
 * This is not a bug, its a feature.
 *
 * @note Parameter x must be a variable as the result is saved there.
 * @note To avoid unexpecting results the value of b should not use more
 *       space as the provided space of n bits (log2)
 * @param x The variable to change some bits
 * @param s The startposition for the new bits
 * @param n The size/window for the new bits
 * @param d The actually new bits to save in the defined position.
 * @return The new value of x
 */
template<typename T, typename U> static inline T SB(T& x, const uint8 s, const uint8 n, const U d)
{
	x &= (T)(~(((1U << n) - 1) << s));
	x |= (T)(d << s);
	return x;
}

/** Add i to n bits of x starting at bit s.
 *
 * This add the value of i on n bits of x starting at bit s. The parameters x,
 * s, i are similar to #GB besides x must be a variable as the result are
 * saved there. An overflow does not affect the following bits of the given
 * bit window and is simply ignored.
 *
 * @note Parameter x must be a variable as the result is saved there.
 * @param x The variable to add some bits at some position
 * @param s The startposition of the addition
 * @param n The size/window for the addition
 * @param i The value to add at the given startposition in the given window.
 * @return The new value of x
 */
template<typename T, typename U> static inline T AB(T& x, const uint8 s, const uint8 n, const U i)
{
	const T mask = (T)(((1U << n) - 1) << s);
	x = (T)((x & ~mask) | ((x + (i << s)) & mask));
	return x;
}

/**
 * Checks if a bit in a value is set.
 *
 * This function checks if a bit inside a value is set or not.
 * The y value specific the position of the bit, started at the
 * LSB and count from 0.
 *
 * @param x The value to check
 * @param y The position of the bit to check, started from the LSB
 * @return True if the bit is set, false else.
 */
template<typename T> static inline bool HasBit(const T x, const uint8 y)
{
	return (x & ((T)1U << y)) != 0;
}

/**
 * Set a bit in a variable.
 *
 * This function sets a bit in a variable. The variable is changed
 * and the value is also returned. Parameter y defines the bit and
 * starts at the LSB with 0.
 *
 * @param x The variable to set a bit
 * @param y The bit position to set
 * @return The new value of the old value with the bit set
 */
template<typename T> static inline T SetBit(T& x, const uint8 y)
{
	return x = (T)(x | (T)(1U << y));
}

/**
 * Clears a bit in a variable.
 *
 * This function clears a bit in a variable. The variable is
 * changed and the value is also returned. Parameter y defines the bit
 * to clear and starts at the LSB with 0.
 *
 * @param x The variable to clear the bit
 * @param y The bit position to clear
 * @return The new value of the old value with the bit cleared
 */
template<typename T> static inline T ClrBit(T& x, const uint8 y)
{
	return x = (T)(x & ~((T)1U << y));
}

/**
 * Toggles a bit in a variable.
 *
 * This function toggles a bit in a variable. The variable is
 * changed and the value is also returned. Parameter y defines the bit
 * to toggle and starts at the LSB with 0.
 *
 * @param x The varliable to toggle the bit
 * @param y The bit position to toggle
 * @return The new value of the old value with the bit toggled
 */
template<typename T> static inline T ToggleBit(T& x, const uint8 y)
{
	return x = (T)(x ^ (T)(1U << y));
}


/* checking more bits. Maybe unneccessary, but easy to use */
/**
 * Check several bits in a value.
 *
 * This macro checks if a value contains at least one bit of an other
 * value.
 *
 * @param x The first value
 * @param y The second value
 * @return True if at least one bit is set in both values, false else.
 */
#define HASBITS(x, y) ((x) & (y))

/**
 * Sets several bits in a variable.
 *
 * This macro sets several bits in a variable. The bits to set are provided
 * by a value. The new value is also returned.
 *
 * @param x The variable to set some bits
 * @param y The value with set bits for setting them in the variable
 * @return The new value of x
 */
#define SETBITS(x, y) ((x) |= (y))

/**
 * Clears several bits in a variable.
 *
 * This macro clears several bits in a variable. The bits to clear are
 * provided by a value. The new value is also returned.
 *
 * @param x The variable to clear some bits
 * @param y The value with set bits for clearing them in the variable
 * @return The new value of x
 */
#define CLRBITS(x, y) ((x) &= ~(y))

#define GENERAL_SPRITE_COLOR(color) ((color) + PALETTE_RECOLOR_START)
#define PLAYER_SPRITE_COLOR(owner) (GENERAL_SPRITE_COLOR(_player_colors[owner]))

/**
 * Whether a sprite comes from the original graphics files or a new grf file
 * (either supplied by OpenTTD or supplied by the user).
 *
 * @param sprite The sprite to check
 * @return True if it is a new sprite, or false if it is original.
 */
#define IS_CUSTOM_SPRITE(sprite) ((sprite) >= SPR_SIGNALS_BASE)

extern const byte _ffb_64[64];

/**
 * Returns the first occure of a bit in a 6-bit value (from right).
 *
 * Returns the position of the first bit that is not zero, counted from the
 * LSB. Ie, 110100 returns 2, 000001 returns 0, etc. When x == 0 returns
 * 0.
 *
 * @param x The 6-bit value to check the first zero-bit
 * @return The first position of a bit started from the LSB or 0 if x is 0.
 */
#define FIND_FIRST_BIT(x) _ffb_64[(x)]

/**
 * Finds the position of the first bit in an integer.
 *
 * This function returns the position of the first bit set in the
 * integer. It does only check the bits of the bitmask
 * 0x3F3F (0011111100111111) and checks only the
 * bits of the bitmask 0x3F00 if and only if the
 * lower part 0x00FF is 0. This results the bits at 0x00C0 must
 * be also zero to check the bits at 0x3F00.
 *
 * @param value The value to check the first bits
 * @return The position of the first bit which is set
 * @see FIND_FIRST_BIT
 */
static inline int FindFirstBit2x64(int value)
{
	if ((value & 0xFF) == 0) {
		return FIND_FIRST_BIT((value >> 8) & 0x3F) + 8;
	} else {
		return FIND_FIRST_BIT(value & 0x3F);
	}
}

/**
 * Clear the first bit in an integer.
 *
 * This function returns a value where the first bit (from LSB)
 * is cleared.
 * So, 110100 returns 110000, 000001 returns 000000, etc.
 *
 * @param value The value to clear the first bit
 * @return The new value with the first bit cleared
 */
template<typename T> static inline T KillFirstBit(T value)
{
	return value &= (T)(value - 1);
}

/**
 * Counts the number of set bits in a variable.
 *
 * @param value the value to count the number of bits in.
 * @return the number of bits.
 */
template<typename T> static inline uint CountBits(T value)
{
	uint num;

	/* This loop is only called once for every bit set by clearing the lowest
	 * bit in each loop. The number of bits is therefore equal to the number of
	 * times the loop was called. It was found at the following website:
	 * http://graphics.stanford.edu/~seander/bithacks.html */

	for (num = 0; value != 0; num++) {
		value &= (T)(value - 1);
	}

	return num;
}

/**
 * Flips a coin with a given probability.
 *
 * This macro can be used to get true or false randomized according to a
 * given probability. The parameter a and b create a percent value with
 * (a/b). The macro returns true in (a/b) percent.
 *
 * @param a The numerator of the fraction
 * @param b The denominator of the fraction, must of course not be null
 * @return True in (a/b) percent
 */
#define CHANCE16(a, b) CHANCE16I(a, b, Random())

/**
 * Flips a coin with a given probability and saves the randomize-number in a variable.
 *
 * This macro uses the same parameters as the CHANCE16 marco. The third parameter
 * must be a variable the randomize-number from Random() is saved in.
 *
 * The low 16 bits of r will already be used and can therefor not be passed to
 * CHANCE16I. One can only send the high 16 bits to CHANCE16I.
 *
 * @param a The numerator of the fraction, see CHANCE16
 * @param b The denominator of the fraction, see CHANCE16
 * @param r The variable to save the randomize-number from Random()
 * @return True in (a/b) percent
 */
#define CHANCE16R(a, b, r) CHANCE16I(a, b, r = Random())

/**
 * Checks if a given randomize-number is below a given probability.
 *
 * This macro is used to check if the given probability by the fraction of (a/b)
 * is greater than low 16 bits of the given randomize-number v.
 *
 * Do not use this function twice on the same random 16 bits as it will yield
 * the same result. One can use a random number for two calls to CHANCE16I,
 * where one call sends the low 16 bits and the other the high 16 bits.
 *
 * @param a The numerator of the fraction, see CHANCE16
 * @param b The denominator of the fraction, see CHANCE16
 * @param r The given randomize-number
 * @return True if v is less or equals (a/b)
 */
static inline bool CHANCE16I(const uint a, const uint b, const uint32 r)
{
	return (uint16)r < (uint16)((65536 * a) / b);
}


#define for_each_bit(_i, _b)            \
	for (_i = 0; _b != 0; _i++, _b >>= 1) \
		if (_b & 1)


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
 * ROtate x Left by n
 *
 * @note Assumes a byte has 8 bits
 * @param x The value which we want to rotate
 * @param n The number how many we waht to rotate
 * @return A bit rotated number
 */
template<typename T> static inline T ROL(const T x, const uint8 n)
{
	return (T)(x << n | x >> (sizeof(x) * 8 - n));
}

/**
 * ROtate x Right by n
 *
 * @note Assumes a byte has 8 bits
 * @param x The value which we want to rotate
 * @param n The number how many we waht to rotate
 * @return A bit rotated number
 */
template<typename T> static inline T ROR(const T x, const uint8 n)
{
	return (T)(x >> n | x << (sizeof(x) * 8 - n));
}

/** return the largest value that can be entered in a variable.
 */
#define MAX_UVALUE(type) ((type)~(type)0)

#endif /* MACROS_H */
