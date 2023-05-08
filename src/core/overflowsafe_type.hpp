/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file overflowsafe_type.hpp An overflow safe integer-like type. */

#ifndef OVERFLOWSAFE_TYPE_HPP
#define OVERFLOWSAFE_TYPE_HPP

#include "math_func.hpp"


#ifdef __has_builtin
#	if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) && __has_builtin(__builtin_mul_overflow)
#		define HAS_OVERFLOW_BUILTINS
#	endif
#endif

/**
 * Overflow safe template for integers, i.e. integers that will never overflow
 * when you multiply the maximum value with 2, or add 2, or subtract something
 * from the minimum value, etc.
 * @param T     the type these integers are stored with.
 */
template <class T>
class OverflowSafeInt
{
private:
	static constexpr T T_MAX = std::numeric_limits<T>::max();
	static constexpr T T_MIN = std::numeric_limits<T>::min();

	/** The non-overflow safe backend to store the value in. */
	T m_value;
public:
	constexpr OverflowSafeInt() : m_value(0) { }

	constexpr OverflowSafeInt(const OverflowSafeInt& other) : m_value(other.m_value) { }
	constexpr OverflowSafeInt(const T int_) : m_value(int_) { }

	inline constexpr OverflowSafeInt& operator = (const OverflowSafeInt& other) { this->m_value = other.m_value; return *this; }
	inline constexpr OverflowSafeInt& operator = (T other) { this->m_value = other; return *this; }

	inline constexpr OverflowSafeInt operator - () const { return OverflowSafeInt(this->m_value == T_MIN ? T_MAX : -this->m_value); }

	/**
	 * Safe implementation of addition.
	 * @param other the amount to add
	 * @note when the addition would yield more than T_MAX, it will be T_MAX.
	 */
	inline constexpr OverflowSafeInt& operator += (const OverflowSafeInt& other)
	{
#ifdef HAS_OVERFLOW_BUILTINS
		if (unlikely(__builtin_add_overflow(this->m_value, other.m_value, &this->m_value))) {
			this->m_value = (other.m_value < 0) ? T_MIN : T_MAX;
		}
#else
		if (this->m_value > 0 && other.m_value > 0 && (T_MAX - other.m_value) < this->m_value) {
			this->m_value = T_MAX;
		} else if (this->m_value < 0 && other.m_value < 0 && (this->m_value == T_MIN || other.m_value == T_MIN || ((T_MAX + this->m_value) + other.m_value < (T_MIN + T_MAX)))) {
			this->m_value = T_MIN;
		} else {
			this->m_value += other.m_value;
		}
#endif
		return *this;
	}

	/**
	 * Safe implementation of subtraction.
	 * @param other the amount to subtract
	 * @note when the subtraction would yield less than T_MIN, it will be T_MIN.
	 */
	inline constexpr OverflowSafeInt& operator -= (const OverflowSafeInt& other)
	{
#ifdef HAS_OVERFLOW_BUILTINS
		if (unlikely(__builtin_sub_overflow(this->m_value, other.m_value, &this->m_value))) {
			this->m_value = (other.m_value < 0) ? T_MAX : T_MIN;
		}
#else
		if (this->m_value > 0 && other.m_value < 0 && (T_MAX + other.m_value) < this->m_value) {
			this->m_value = T_MAX;
		} else if (this->m_value < 0 && other.m_value > 0 && (T_MAX + this->m_value) < (T_MIN + T_MAX) + other.m_value) {
			this->m_value = T_MIN;
		} else {
			this->m_value -= other.m_value;
		}
#endif
		return *this;
	}

	/* Operators for addition and subtraction. */
	inline constexpr OverflowSafeInt  operator +  (const OverflowSafeInt& other) const { OverflowSafeInt result = *this; result += other; return result; }
	inline constexpr OverflowSafeInt  operator +  (const int              other) const { OverflowSafeInt result = *this; result += (int64_t)other; return result; }
	inline constexpr OverflowSafeInt  operator +  (const uint             other) const { OverflowSafeInt result = *this; result += (int64_t)other; return result; }
	inline constexpr OverflowSafeInt  operator -  (const OverflowSafeInt& other) const { OverflowSafeInt result = *this; result -= other; return result; }
	inline constexpr OverflowSafeInt  operator -  (const int              other) const { OverflowSafeInt result = *this; result -= (int64_t)other; return result; }
	inline constexpr OverflowSafeInt  operator -  (const uint             other) const { OverflowSafeInt result = *this; result -= (int64_t)other; return result; }

	inline constexpr OverflowSafeInt& operator ++ () { return *this += 1; }
	inline constexpr OverflowSafeInt& operator -- () { return *this += -1; }
	inline constexpr OverflowSafeInt operator ++ (int) { OverflowSafeInt org = *this; *this += 1; return org; }
	inline constexpr OverflowSafeInt operator -- (int) { OverflowSafeInt org = *this; *this += -1; return org; }

	/**
	 * Safe implementation of multiplication.
	 * @param factor the factor to multiply this with.
	 * @note when the multiplication would yield more than T_MAX (or less than T_MIN),
	 *       it will be T_MAX (respectively T_MIN).
	 */
	inline constexpr OverflowSafeInt& operator *= (const int factor)
	{
#ifdef HAS_OVERFLOW_BUILTINS
		const bool is_result_positive = (this->m_value < 0) == (factor < 0); // -ve * -ve == +ve
		if (unlikely(__builtin_mul_overflow(this->m_value, factor, &this->m_value))) {
			this->m_value = is_result_positive ? T_MAX : T_MIN;
		}
#else
		if (factor == -1) {
			this->m_value = (this->m_value == T_MIN) ? T_MAX : -this->m_value;
		} else if (factor > 0 && this->m_value > 0 && (T_MAX / factor) < this->m_value) {
			this->m_value = T_MAX;
		} else if (factor > 0 && this->m_value < 0 && (T_MIN / factor) > this->m_value) {
			this->m_value = T_MIN;
		} else if (factor < 0 && this->m_value > 0 && (T_MIN / factor) < this->m_value) {
			this->m_value = T_MIN;
		} else if (factor < 0 && this->m_value < 0 && (T_MAX / factor) > this->m_value) {
			this->m_value = T_MAX;
		} else {
			this->m_value *= factor;
		}
#endif
		return *this;
	}

	/* Operators for multiplication. */
	inline constexpr OverflowSafeInt operator * (const int64_t  factor) const { OverflowSafeInt result = *this; result *= factor; return result; }
	inline constexpr OverflowSafeInt operator * (const int    factor) const { OverflowSafeInt result = *this; result *= (int64_t)factor; return result; }
	inline constexpr OverflowSafeInt operator * (const uint   factor) const { OverflowSafeInt result = *this; result *= (int64_t)factor; return result; }
	inline constexpr OverflowSafeInt operator * (const uint16_t factor) const { OverflowSafeInt result = *this; result *= (int64_t)factor; return result; }
	inline constexpr OverflowSafeInt operator * (const byte   factor) const { OverflowSafeInt result = *this; result *= (int64_t)factor; return result; }

	/* Operators for division. */
	inline constexpr OverflowSafeInt& operator /= (const int64_t            divisor)       { this->m_value /= divisor; return *this; }
	inline constexpr OverflowSafeInt  operator /  (const OverflowSafeInt& divisor) const { OverflowSafeInt result = *this; result /= divisor.m_value; return result; }
	inline constexpr OverflowSafeInt  operator /  (const int              divisor) const { OverflowSafeInt result = *this; result /= divisor; return result; }
	inline constexpr OverflowSafeInt  operator /  (const uint             divisor) const { OverflowSafeInt result = *this; result /= (int)divisor; return result; }

	/* Operators for modulo */
	inline constexpr OverflowSafeInt& operator %= (const int  divisor)       { this->m_value %= divisor; return *this; }
	inline constexpr OverflowSafeInt  operator %  (const int  divisor) const { OverflowSafeInt result = *this; result %= divisor; return result; }

	/* Operators for shifting. */
	inline constexpr OverflowSafeInt& operator <<= (const int shift)       { this->m_value <<= shift; return *this; }
	inline constexpr OverflowSafeInt  operator <<  (const int shift) const { OverflowSafeInt result = *this; result <<= shift; return result; }
	inline constexpr OverflowSafeInt& operator >>= (const int shift)       { this->m_value >>= shift; return *this; }
	inline constexpr OverflowSafeInt  operator >>  (const int shift) const { OverflowSafeInt result = *this; result >>= shift; return result; }

	/* Operators for (in)equality when comparing overflow safe ints. */
	inline constexpr bool operator == (const OverflowSafeInt& other) const { return this->m_value == other.m_value; }
	inline constexpr bool operator != (const OverflowSafeInt& other) const { return !(*this == other); }
	inline constexpr bool operator >  (const OverflowSafeInt& other) const { return this->m_value > other.m_value; }
	inline constexpr bool operator >= (const OverflowSafeInt& other) const { return this->m_value >= other.m_value; }
	inline constexpr bool operator <  (const OverflowSafeInt& other) const { return !(*this >= other); }
	inline constexpr bool operator <= (const OverflowSafeInt& other) const { return !(*this > other); }

	/* Operators for (in)equality when comparing non-overflow safe ints. */
	inline constexpr bool operator == (const int other) const { return this->m_value == other; }
	inline constexpr bool operator != (const int other) const { return !(*this == other); }
	inline constexpr bool operator >  (const int other) const { return this->m_value > other; }
	inline constexpr bool operator >= (const int other) const { return this->m_value >= other; }
	inline constexpr bool operator <  (const int other) const { return !(*this >= other); }
	inline constexpr bool operator <= (const int other) const { return !(*this > other); }

	inline constexpr operator T () const { return this->m_value; }

	static inline constexpr OverflowSafeInt<T> max() { return T_MAX; }
	static inline constexpr OverflowSafeInt<T> min() { return T_MIN; }
};


/* Sometimes we got int64_t operator OverflowSafeInt instead of vice versa. Handle that properly. */
template <class T> inline constexpr OverflowSafeInt<T> operator + (const int64_t a, const OverflowSafeInt<T> b) { return b + a; }
template <class T> inline constexpr OverflowSafeInt<T> operator - (const int64_t a, const OverflowSafeInt<T> b) { return -b + a; }
template <class T> inline constexpr OverflowSafeInt<T> operator * (const int64_t a, const OverflowSafeInt<T> b) { return b * a; }
template <class T> inline constexpr OverflowSafeInt<T> operator / (const int64_t a, const OverflowSafeInt<T> b) { return (OverflowSafeInt<T>)a / (int)b; }

/* Sometimes we got int operator OverflowSafeInt instead of vice versa. Handle that properly. */
template <class T> inline constexpr OverflowSafeInt<T> operator + (const int   a, const OverflowSafeInt<T> b) { return b + a; }
template <class T> inline constexpr OverflowSafeInt<T> operator - (const int   a, const OverflowSafeInt<T> b) { return -b + a; }
template <class T> inline constexpr OverflowSafeInt<T> operator * (const int   a, const OverflowSafeInt<T> b) { return b * a; }
template <class T> inline constexpr OverflowSafeInt<T> operator / (const int   a, const OverflowSafeInt<T> b) { return (OverflowSafeInt<T>)a / (int)b; }

/* Sometimes we got uint operator OverflowSafeInt instead of vice versa. Handle that properly. */
template <class T> inline constexpr OverflowSafeInt<T> operator + (const uint  a, const OverflowSafeInt<T> b) { return b + a; }
template <class T> inline constexpr OverflowSafeInt<T> operator - (const uint  a, const OverflowSafeInt<T> b) { return -b + a; }
template <class T> inline constexpr OverflowSafeInt<T> operator * (const uint  a, const OverflowSafeInt<T> b) { return b * a; }
template <class T> inline constexpr OverflowSafeInt<T> operator / (const uint  a, const OverflowSafeInt<T> b) { return (OverflowSafeInt<T>)a / (int)b; }

/* Sometimes we got byte operator OverflowSafeInt instead of vice versa. Handle that properly. */
template <class T> inline constexpr OverflowSafeInt<T> operator + (const byte  a, const OverflowSafeInt<T> b) { return b + (uint)a; }
template <class T> inline constexpr OverflowSafeInt<T> operator - (const byte  a, const OverflowSafeInt<T> b) { return -b + (uint)a; }
template <class T> inline constexpr OverflowSafeInt<T> operator * (const byte  a, const OverflowSafeInt<T> b) { return b * (uint)a; }
template <class T> inline constexpr OverflowSafeInt<T> operator / (const byte  a, const OverflowSafeInt<T> b) { return (OverflowSafeInt<T>)a / (int)b; }

typedef OverflowSafeInt<int64_t> OverflowSafeInt64;
typedef OverflowSafeInt<int32_t> OverflowSafeInt32;

/* Some basic "unit tests". Also has the bonus of confirming that constexpr is working. */
static_assert(OverflowSafeInt32(INT32_MIN) - 1 == OverflowSafeInt32(INT32_MIN));
static_assert(OverflowSafeInt32(INT32_MAX) + 1 == OverflowSafeInt32(INT32_MAX));
static_assert(OverflowSafeInt32(INT32_MAX) * 2 == OverflowSafeInt32(INT32_MAX));
static_assert(OverflowSafeInt32(INT32_MIN) * 2 == OverflowSafeInt32(INT32_MIN));

/* Specialisation of the generic ClampTo function for overflow safe integers to normal integers. */
template <typename To, typename From>
constexpr To ClampTo(OverflowSafeInt<From> value) { return ClampTo<To>(From(value)); }

#undef HAS_OVERFLOW_BUILTINS

#endif /* OVERFLOWSAFE_TYPE_HPP */
