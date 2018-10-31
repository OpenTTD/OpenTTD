/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bitmath_func.hpp Functions related to bit mathematics. */

#ifndef BITMATH_FUNC_HPP
#define BITMATH_FUNC_HPP

/**
 * Fetch \a n bits from \a x, started at bit \a s.
 *
 * This function can be used to fetch \a n bits from the value \a x. The
 * \a s value set the start position to read. The start position is
 * count from the LSB and starts at \c 0. The result starts at a
 * LSB, as this isn't just an and-bitmask but also some
 * bit-shifting operations. GB(0xFF, 2, 1) will so
 * return 0x01 (0000 0001) instead of
 * 0x04 (0000 0100).
 *
 * @param x The value to read some bits.
 * @param s The start position to read some bits.
 * @param n The number of bits to read.
 * @pre n < sizeof(T) * 8
 * @pre s + n <= sizeof(T) * 8
 * @return The selected bits, aligned to a LSB.
 */
template <typename T>
static inline uint GB(const T x, const uint8 s, const uint8 n)
{
	return (x >> s) & (((T)1U << n) - 1);
}

/**
 * Set \a n bits in \a x starting at bit \a s to \a d
 *
 * This function sets \a n bits from \a x which started as bit \a s to the value of
 * \a d. The parameters \a x, \a s and \a n works the same as the parameters of
 * #GB. The result is saved in \a x again. Unused bits in the window
 * provided by n are set to 0 if the value of \a d isn't "big" enough.
 * This is not a bug, its a feature.
 *
 * @note Parameter \a x must be a variable as the result is saved there.
 * @note To avoid unexpected results the value of \a d should not use more
 *       space as the provided space of \a n bits (log2)
 * @param x The variable to change some bits
 * @param s The start position for the new bits
 * @param n The size/window for the new bits
 * @param d The actually new bits to save in the defined position.
 * @pre n < sizeof(T) * 8
 * @pre s + n <= sizeof(T) * 8
 * @return The new value of \a x
 */
template <typename T, typename U>
static inline T SB(T &x, const uint8 s, const uint8 n, const U d)
{
	x &= (T)(~((((T)1U << n) - 1) << s));
	x |= (T)(d << s);
	return x;
}

/**
 * Add \a i to \a n bits of \a x starting at bit \a s.
 *
 * This adds the value of \a i on \a n bits of \a x starting at bit \a s. The parameters \a x,
 * \a s, \a i are similar to #GB. Besides, \ a x must be a variable as the result are
 * saved there. An overflow does not affect the following bits of the given
 * bit window and is simply ignored.
 *
 * @note Parameter x must be a variable as the result is saved there.
 * @param x The variable to add some bits at some position
 * @param s The start position of the addition
 * @param n The size/window for the addition
 * @pre n < sizeof(T) * 8
 * @pre s + n <= sizeof(T) * 8
 * @param i The value to add at the given start position in the given window.
 * @return The new value of \a x
 */
template <typename T, typename U>
static inline T AB(T &x, const uint8 s, const uint8 n, const U i)
{
	const T mask = ((((T)1U << n) - 1) << s);
	x = (T)((x & ~mask) | ((x + (i << s)) & mask));
	return x;
}

/**
 * Checks if a bit in a value is set.
 *
 * This function checks if a bit inside a value is set or not.
 * The \a y value specific the position of the bit, started at the
 * LSB and count from \c 0.
 *
 * @param x The value to check
 * @param y The position of the bit to check, started from the LSB
 * @pre y < sizeof(T) * 8
 * @return True if the bit is set, false else.
 */
template <typename T>
static inline bool HasBit(const T x, const uint8 y)
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
 * @pre y < sizeof(T) * 8
 * @return The new value of the old value with the bit set
 */
template <typename T>
static inline T SetBit(T &x, const uint8 y)
{
	return x = (T)(x | ((T)1U << y));
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
 * @pre y < sizeof(T) * 8
 * @return The new value of the old value with the bit cleared
 */
template <typename T>
static inline T ClrBit(T &x, const uint8 y)
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
 * @param x The variable to toggle the bit
 * @param y The bit position to toggle
 * @pre y < sizeof(T) * 8
 * @return The new value of the old value with the bit toggled
 */
template <typename T>
static inline T ToggleBit(T &x, const uint8 y)
{
	return x = (T)(x ^ ((T)1U << y));
}


/** Lookup table to check which bit is set in a 6 bit variable */
extern const uint8 _ffb_64[64];

/**
 * Returns the first non-zero bit in a 6-bit value (from right).
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
 * Finds the position of the first non-zero bit in an integer.
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
template <typename T>
static inline T KillFirstBit(T value)
{
	return value &= (T)(value - 1);
}

/**
 * Counts the number of set bits in a variable.
 *
 * @param value the value to count the number of bits in.
 * @return the number of bits.
 */
template <typename T>
static inline uint CountBits(T value)
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
 * Test whether \a value has exactly 1 bit set
 *
 * @param value the value to test.
 * @return does \a value have exactly 1 bit set?
 */
template <typename T>
static inline bool HasExactlyOneBit(T value)
{
	return value != 0 && (value & (value - 1)) == 0;
}

/**
 * Test whether \a value has at most 1 bit set
 *
 * @param value the value to test.
 * @return does \a value have at most 1 bit set?
 */
template <typename T>
static inline bool HasAtMostOneBit(T value)
{
	return (value & (value - 1)) == 0;
}

/**
 * ROtate \a x Left by \a n
 *
 * @note Assumes a byte has 8 bits
 * @param x The value which we want to rotate
 * @param n The number how many we want to rotate
 * @pre n < sizeof(T) * 8
 * @return A bit rotated number
 */
template <typename T>
static inline T ROL(const T x, const uint8 n)
{
	if (n == 0) return x;
	return (T)(x << n | x >> (sizeof(x) * 8 - n));
}

/**
 * ROtate \a x Right by \a n
 *
 * @note Assumes a byte has 8 bits
 * @param x The value which we want to rotate
 * @param n The number how many we want to rotate
 * @pre n < sizeof(T) * 8
 * @return A bit rotated number
 */
template <typename T>
static inline T ROR(const T x, const uint8 n)
{
	if (n == 0) return x;
	return (T)(x >> n | x << (sizeof(x) * 8 - n));
}

/**
 * Do an operation for each set bit in a value.
 *
 * This macros is used to do an operation for each set
 * bit in a variable. The second parameter is a
 * variable that is used as the bit position counter.
 * The fourth parameter is an expression of the bits
 * we need to iterate over. This expression will be
 * evaluated once.
 *
 * @param Tbitpos_type Type of the position counter variable.
 * @param bitpos_var   The position counter variable.
 * @param Tbitset_type Type of the bitset value.
 * @param bitset_value The bitset value which we check for bits.
 *
 * @see FOR_EACH_SET_BIT
 */
#define FOR_EACH_SET_BIT_EX(Tbitpos_type, bitpos_var, Tbitset_type, bitset_value) \
	for (                                                                           \
		Tbitset_type ___FESBE_bits = (bitpos_var = (Tbitpos_type)0, bitset_value);    \
		___FESBE_bits != (Tbitset_type)0;                                             \
		___FESBE_bits = (Tbitset_type)(___FESBE_bits >> 1), bitpos_var++              \
	)                                                                               \
		if ((___FESBE_bits & 1) != 0)

/**
 * Do an operation for each set set bit in a value.
 *
 * This macros is used to do an operation for each set
 * bit in a variable. The first parameter is a variable
 * that is used as the bit position counter.
 * The second parameter is an expression of the bits
 * we need to iterate over. This expression will be
 * evaluated once.
 *
 * @param bitpos_var   The position counter variable.
 * @param bitset_value The value which we check for set bits.
 */
#define FOR_EACH_SET_BIT(bitpos_var, bitset_value) FOR_EACH_SET_BIT_EX(uint, bitpos_var, uint, bitset_value)

#if defined(__APPLE__)
	/* Make endian swapping use Apple's macros to increase speed
	 * (since it will use hardware swapping if available).
	 * Even though they should return uint16 and uint32, we get
	 * warnings if we don't cast those (why?) */
	#define BSWAP32(x) ((uint32)CFSwapInt32(x))
	#define BSWAP16(x) ((uint16)CFSwapInt16(x))
#elif defined(_MSC_VER)
	/* MSVC has intrinsics for swapping, resulting in faster code */
	#define BSWAP32(x) (_byteswap_ulong(x))
	#define BSWAP16(x) (_byteswap_ushort(x))
#else
	/**
	 * Perform a 32 bits endianness bitswap on x.
	 * @param x the variable to bitswap
	 * @return the bitswapped value.
	 */
	static inline uint32 BSWAP32(uint32 x)
	{
#if !defined(__ICC) && defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4)  && __GNUC_MINOR__ >= 3))
		/* GCC >= 4.3 provides a builtin, resulting in faster code */
		return (uint32)__builtin_bswap32((int32)x);
#else
		return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
#endif /* defined(__GNUC__) */
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
