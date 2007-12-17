/* $Id$ */

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
template<typename T> static inline T max(const T a, const T b)
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
template<typename T> static inline T min(const T a, const T b)
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
	return (a < b) ? a : b;
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
	return (a < b) ? a : b;
}

/**
 * Returns the absolute value of (scalar) variable.
 *
 * @note assumes variable to be signed
 * @param a The value we want to unsign
 * @return The unsigned value
 */
template <typename T> static inline T abs(const T a)
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
template<typename T> static inline T Align(const T x, uint n)
{
	n--;
	return (T)((x + n) & ~(n));
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
	if (a <= min) return min;
	if (a >= max) return max;
	return a;
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
	if (a <= min) return min;
	if (a >= max) return max;
	return a;
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
	if (a <= (int32)0x80000000) return 0x80000000;
	if (a >= (int32)0x7FFFFFFF) return 0x7FFFFFFF;
	return (int32)a;
}

/**
 * Returns the (absolute) difference between two (scalar) variables
 *
 * @param a The first scalar
 * @param b The second scalar
 * @return The absolute difference between the given scalars
 */
template <typename T> static inline T Delta(const T a, const T b) {
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
template<typename T> static inline bool IsInsideBS(const T x, const uint base, const uint size)
{
	return (uint)(x - base) < size;
}

/**
 * Checks if a value is in an interval.
 *
 * Returns true if a value is in the interval of [min, max).
 *
 * @param a The value to check
 * @param min The minimum of the interval
 * @param max The maximum of the interval
 * @see IsInsideBS()
 */
template<typename T> static inline bool IsInsideMM(const T x, const uint min, const uint max)
{
	return (uint)(x - min) < (max - min);
}

#endif /* MATH_FUNC_HPP */
