/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file math_func.hpp Integer math functions */

#ifndef MATH_FUNC_HPP
#define MATH_FUNC_HPP

#include "strong_typedef_type.hpp"

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
	static_assert(sizeof(size_t) == sizeof(void *));
	return reinterpret_cast<T *>(Align((size_t)x, n));
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
 * Clamp a value between an interval.
 *
 * This function returns a value which is between the given interval of
 * min and max. If the given value is in this interval the value itself
 * is returned otherwise the border of the interval is returned, according
 * which side of the interval was 'left'.
 *
 * @note If the min value is greater than the max, return value is the average of the min and max.
 * @param a The value to clamp/truncate.
 * @param min The minimum of the interval.
 * @param max the maximum of the interval.
 * @returns A value between min and max which is closest to a.
 */
template <typename T>
static inline T SoftClamp(const T a, const T min, const T max)
{
	if (min > max) {
		using U = std::make_unsigned_t<T>;
		return min - (U(min) - max) / 2;
	}
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
 * Clamp the given value down to lie within the requested type.
 *
 * For example ClampTo<uint8_t> will return a value clamped to the range of 0
 * to 255. Anything smaller will become 0, anything larger will become 255.
 *
 * @param a The 64-bit value to clamp.
 * @return The 64-bit value reduced to a value within the given allowed range
 * for the return type.
 * @see Clamp(int, int, int)
 */
template <typename To, typename From, std::enable_if_t<std::is_integral<From>::value, int> = 0>
constexpr To ClampTo(From value)
{
	static_assert(std::numeric_limits<To>::is_integer, "Do not clamp from non-integer values");
	static_assert(std::numeric_limits<From>::is_integer, "Do not clamp to non-integer values");

	if constexpr (sizeof(To) >= sizeof(From) && std::numeric_limits<To>::is_signed == std::numeric_limits<From>::is_signed) {
		/* Same signedness and To type is larger or equal than From type, no clamping is required. */
		return static_cast<To>(value);
	}

	if constexpr (sizeof(To) > sizeof(From) && std::numeric_limits<To>::is_signed) {
		/* Signed destination and a larger To type, no clamping is required. */
		return static_cast<To>(value);
	}

	/* Get the bigger of the two types based on essentially the number of bits. */
	using BiggerType = typename std::conditional<sizeof(From) >= sizeof(To), From, To>::type;

	if constexpr (std::numeric_limits<To>::is_signed) {
		/* The output is a signed number. */
		if constexpr (std::numeric_limits<From>::is_signed) {
			/* Both input and output are signed. */
			return static_cast<To>(std::clamp<BiggerType>(value,
					std::numeric_limits<To>::lowest(), std::numeric_limits<To>::max()));
		}

		/* The input is unsigned, so skip the minimum check and use unsigned variant of the biggest type as intermediate type. */
		using BiggerUnsignedType = typename std::make_unsigned<BiggerType>::type;
		return static_cast<To>(std::min<BiggerUnsignedType>(std::numeric_limits<To>::max(), value));
	}

	/* The output is unsigned. */

	if constexpr (std::numeric_limits<From>::is_signed) {
		/* Input is signed; account for the negative numbers in the input. */
		if constexpr (sizeof(To) >= sizeof(From)) {
			/* If the output type is larger or equal to the input type, then only clamp the negative numbers. */
			return static_cast<To>(std::max<From>(value, 0));
		}

		/* The output type is smaller than the input type. */
		using BiggerSignedType = typename std::make_signed<BiggerType>::type;
		return static_cast<To>(std::clamp<BiggerSignedType>(value,
				std::numeric_limits<To>::lowest(), std::numeric_limits<To>::max()));
	}

	/* The input and output are unsigned, just clamp at the high side. */
	return static_cast<To>(std::min<BiggerType>(value, std::numeric_limits<To>::max()));
}

/**
 * Specialization of ClampTo for #StrongType::Typedef.
 */
template <typename To, typename From, std::enable_if_t<std::is_base_of<StrongTypedefBase, From>::value, int> = 0>
constexpr To ClampTo(From value)
{
	return ClampTo<To>(value.base());
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
static inline bool IsInsideBS(const T x, const size_t base, const size_t size)
{
	return (size_t)(x - base) < size;
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
template <typename T, std::enable_if_t<std::disjunction_v<std::is_convertible<T, size_t>, std::is_base_of<StrongTypedefBase, T>>, int> = 0>
static constexpr inline bool IsInsideMM(const T x, const size_t min, const size_t max) noexcept
{
	if constexpr (std::is_base_of_v<StrongTypedefBase, T>) {
		return (size_t)(x.base() - min) < (max - min);
	} else {
		return (size_t)(x - min) < (max - min);
	}
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
		return (a + static_cast<int>(b) / 2) / static_cast<int>(b);
	} else {
		/* -0.5 is rounded to 0 */
		return (a - (static_cast<int>(b) - 1) / 2) / static_cast<int>(b);
	}
}

/**
 * Computes (a / b) rounded away from zero.
 * @param a Numerator
 * @param b Denominator
 * @return Quotient, rounded away from zero
 */
static inline int DivAwayFromZero(int a, uint b)
{
	const int _b = static_cast<int>(b);
	if (a > 0) {
		return (a + _b - 1) / _b;
	} else {
		/* Note: Behaviour of negative numerator division is truncation toward zero. */
		return (a - _b + 1) / _b;
	}
}

uint32_t IntSqrt(uint32_t num);

#endif /* MATH_FUNC_HPP */
