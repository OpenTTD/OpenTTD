/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file math_func.hpp Integer math functions */

#ifndef MATH_FUNC_HPP
#define MATH_FUNC_HPP

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef abs
#undef abs
#endif

/**
 * Returns the maximum of two values.
 *
 * This function returns the greater value of two given values.
 * If they are equal the value of a is returned.
 *
 * @param a The first value
 * @param b The second value
 * @return The greater value or a if equals
 */
template <typename T>
static inline T max(const T a, const T b)
{
	return (a >= b) ? a : b;
}

/**
 * Returns the minimum of two values.
 *
 * This function returns the smaller value of two given values.
 * If they are equal the value of b is returned.
 *
 * @param a The first value
 * @param b The second value
 * @return The smaller value or b if equals
 */
template <typename T>
static inline T min(const T a, const T b)
{
	return (a < b) ? a : b;
}

/**
 * Returns the minimum of two integer.
 *
 * This function returns the smaller value of two given integers.
 *
 * @param a The first integer
 * @param b The second integer
 * @return The smaller value
 */
static inline int min(const int a, const int b)
{
	return min<int>(a, b);
}

/**
 * Returns the minimum of two unsigned integers.
 *
 * This function returns the smaller value of two given unsigned integers.
 *
 * @param a The first unsigned integer
 * @param b The second unsigned integer
 * @return The smaller value
 */
static inline uint minu(const uint a, const uint b)
{
	return min<uint>(a, b);
}

/**
 * Returns the absolute value of (scalar) variable.
 *
 * @note assumes variable to be signed
 * @param a The value we want to unsign
 * @return The unsigned value
 */
template <typename T>
static inline T abs(const T a)
{
	return (a < (T)0) ? -a : a;
}

/**
 * Return the smallest multiple of n equal or greater than x
 *
 * @note n must be a power of 2
 * @param x The min value
 * @param n The base of the number we are searching
 * @return The smallest multiple of n equal or greater than x
 */
template <typename T>
static inline T Align(const T x, uint n)
{
	assert((n & (n - 1)) == 0 && n != 0);
	n--;
	return (T)((x + n) & ~((T)n));
}

/**
 * Return the smallest multiple of n equal or greater than x
 * Applies to pointers only
 *
 * @note n must be a power of 2
 * @param x The min value
 * @param n The base of the number we are searching
 * @return The smallest multiple of n equal or greater than x
 * @see Align()
 */
template <typename T>
static inline T *AlignPtr(T *x, uint n)
{
	assert_compile(sizeof(size_t) == sizeof(void *));
	return (T *)Align((size_t)x, n);
}

/**
 * Clamp a value between an interval.
 *
 * This function returns a value which is between the given interval of
 * min and max. If the given value is in this interval the value itself
 * is returned otherwise the border of the interval is returned, according
 * which side of the interval was 'left'.
 *
 * @note The min value must be less or equal of max or you get some
 *       unexpected results.
 * @param a The value to clamp/truncate.
 * @param min The minimum of the interval.
 * @param max the maximum of the interval.
 * @returns A value between min and max which is closest to a.
 * @see ClampU(uint, uint, uint)
 * @see Clamp(int, int, int)
 */
template <typename T>
static inline T Clamp(const T a, const T min, const T max)
{
	assert(min <= max);
	if (a <= min) return min;
	if (a >= max) return max;
	return a;
}

/**
 * Clamp an integer between an interval.
 *
 * This function returns a value which is between the given interval of
 * min and max. If the given value is in this interval the value itself
 * is returned otherwise the border of the interval is returned, according
 * which side of the interval was 'left'.
 *
 * @note The min value must be less or equal of max or you get some
 *       unexpected results.
 * @param a The value to clamp/truncate.
 * @param min The minimum of the interval.
 * @param max the maximum of the interval.
 * @returns A value between min and max which is closest to a.
 * @see ClampU(uint, uint, uint)
 */
static inline int Clamp(const int a, const int min, const int max)
{
	return Clamp<int>(a, min, max);
}

/**
 * Clamp an unsigned integer between an interval.
 *
 * This function returns a value which is between the given interval of
 * min and max. If the given value is in this interval the value itself
 * is returned otherwise the border of the interval is returned, according
 * which side of the interval was 'left'.
 *
 * @note The min value must be less or equal of max or you get some
 *       unexpected results.
 * @param a The value to clamp/truncate.
 * @param min The minimum of the interval.
 * @param max the maximum of the interval.
 * @returns A value between min and max which is closest to a.
 * @see Clamp(int, int, int)
 */
static inline uint ClampU(const uint a, const uint min, const uint max)
{
	return Clamp<uint>(a, min, max);
}

/**
 * Reduce a signed 64-bit int to a signed 32-bit one
 *
 * This function clamps a 64-bit integer to a 32-bit integer.
 * If the 64-bit value is smaller than the smallest 32-bit integer
 * value 0x80000000 this value is returned (the left one bit is the sign bit).
 * If the 64-bit value is greater than the greatest 32-bit integer value 0x7FFFFFFF
 * this value is returned. In all other cases the 64-bit value 'fits' in a
 * 32-bits integer field and so the value is casted to int32 and returned.
 *
 * @param a The 64-bit value to clamps
 * @return The 64-bit value reduced to a 32-bit value
 * @see Clamp(int, int, int)
 */
static inline int32 ClampToI32(const int64 a)
{
	return (int32)Clamp<int64>(a, INT32_MIN, INT32_MAX);
}

/**
 * Reduce an unsigned 64-bit int to an unsigned 16-bit one
 *
 * @param a The 64-bit value to clamp
 * @return The 64-bit value reduced to a 16-bit value
 * @see ClampU(uint, uint, uint)
 */
static inline uint16 ClampToU16(const uint64 a)
{
	/* MSVC thinks, in its infinite wisdom, that int min(int, int) is a better
	 * match for min(uint64, uint) than uint64 min(uint64, uint64). As such we
	 * need to cast the UINT16_MAX to prevent MSVC from displaying its
	 * infinite loads of warnings. */
	return (uint16)min<uint64>(a, (uint64)UINT16_MAX);
}

/**
 * Returns the (absolute) difference between two (scalar) variables
 *
 * @param a The first scalar
 * @param b The second scalar
 * @return The absolute difference between the given scalars
 */
template <typename T>
static inline T Delta(const T a, const T b)
{
	return (a < b) ? b - a : a - b;
}

/**
 * Checks if a value is between a window started at some base point.
 *
 * This function checks if the value x is between the value of base
 * and base+size. If x equals base this returns true. If x equals
 * base+size this returns false.
 *
 * @param x The value to check
 * @param base The base value of the interval
 * @param size The size of the interval
 * @return True if the value is in the interval, false else.
 */
template <typename T>
static inline bool IsInsideBS(const T x, const uint base, const uint size)
{
	return (uint)(x - base) < size;
}

/**
 * Checks if a value is in an interval.
 *
 * Returns true if a value is in the interval of [min, max).
 *
 * @param x The value to check
 * @param min The minimum of the interval
 * @param max The maximum of the interval
 * @see IsInsideBS()
 */
template <typename T>
static inline bool IsInsideMM(const T x, const uint min, const uint max)
{
	return (uint)(x - min) < (max - min);
}

/**
 * Type safe swap operation
 * @param a variable to swap with b
 * @param b variable to swap with a
 */
template <typename T>
static inline void Swap(T &a, T &b)
{
	T t = a;
	a = b;
	b = t;
}

/**
 * Converts a "fract" value 0..255 to "percent" value 0..100
 * @param i value to convert, range 0..255
 * @return value in range 0..100
 */
static inline uint ToPercent8(uint i)
{
	assert(i < 256);
	return i * 101 >> 8;
}

/**
 * Converts a "fract" value 0..65535 to "percent" value 0..100
 * @param i value to convert, range 0..65535
 * @return value in range 0..100
 */
static inline uint ToPercent16(uint i)
{
	assert(i < 65536);
	return i * 101 >> 16;
}

int LeastCommonMultiple(int a, int b);
int GreatestCommonDivisor(int a, int b);
int DivideApprox(int a, int b);

/**
 * Computes ceil(a / b) for non-negative a and b.
 * @param a Numerator
 * @param b Denominator
 * @return Quotient, rounded up
 */
static inline uint CeilDiv(uint a, uint b)
{
	return (a + b - 1) / b;
}

/**
 * Computes ceil(a / b) * b for non-negative a and b.
 * @param a Numerator
 * @param b Denominator
 * @return a rounded up to the nearest multiple of b.
 */
static inline uint Ceil(uint a, uint b)
{
	return CeilDiv(a, b) * b;
}

/**
 * Computes round(a / b) for signed a and unsigned b.
 * @param a Numerator
 * @param b Denominator
 * @return Quotient, rounded to nearest
 */
static inline int RoundDivSU(int a, uint b)
{
	if (a > 0) {
		/* 0.5 is rounded to 1 */
		return (a + (int)b / 2) / (int)b;
	} else {
		/* -0.5 is rounded to 0 */
		return (a - ((int)b - 1) / 2) / (int)b;
	}
}

uint32 IntSqrt(uint32 num);

#endif /* MATH_FUNC_HPP */
