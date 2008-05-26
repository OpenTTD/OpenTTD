/* $Id$ */

/** @file bitmath_func.hpp Functions related to bit mathematics. */

#ifndef BITMATH_FUNC_HPP
#define BITMATH_FUNC_HPP

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
 * Check several bits in a value.
 *
 * This macro checks if a value contains at least one bit of an other
 * value.
 *
 * @param x The first value
 * @param y The second value
 * @return True if at least one bit is set in both values, false else.
 */
#define HASBITS(x, y) (((x) & (y)) != 0)

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


/** Lookup table to check which bit is set in a 6 bit variable */
extern const uint8 _ffb_64[64];

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
static inline uint8 FindFirstBit2x64(const int value)
{
	if ((value & 0xFF) == 0) {
		return FIND_FIRST_BIT((value >> 8) & 0x3F) + 8;
	} else {
		return FIND_FIRST_BIT(value & 0x3F);
	}
}

uint8 FindFirstBit(uint32 x);
uint8 FindLastBit(uint64 x);

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

/**
 * Do an operation for each set set bit in a value.
 *
 * This macros is used to do an operation for each set
 * bit in a variable. The first variable can be reused
 * in the operation due to it's the bit position counter.
 * The second variable will be cleared during the usage
 *
 * @param i The position counter
 * @param b The value which we check for set bits
 */
#define FOR_EACH_SET_BIT(i, b)      \
	for (i = 0; b != 0; i++, b >>= 1) \
		if (b & 1)


#if defined(__APPLE__)
	/* Make endian swapping use Apple's macros to increase speed
	 * (since it will use hardware swapping if available).
	 * Even though they should return uint16 and uint32, we get
	 * warnings if we don't cast those (why?) */
	#define BSWAP32(x) ((uint32)Endian32_Swap(x))
	#define BSWAP16(x) ((uint16)Endian16_Swap(x))
#else
	/**
	 * Perform a 32 bits endianness bitswap on x.
	 * @param x the variable to bitswap
	 * @return the bitswapped value.
	 */
	static inline uint32 BSWAP32(uint32 x)
	{
		return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
	}

	/**
	 * Perform a 16 bits endianness bitswap on x.
	 * @param x the variable to bitswap
	 * @return the bitswapped value.
	 */
	static inline uint16 BSWAP16(uint16 x)
	{
		return (x >> 8) | (x << 8);
	}
#endif /* __APPLE__ */

#endif /* BITMATH_FUNC_HPP */
