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
#include "../stdafx.h"
#include <limits>

/**
 * Overflow safe template for integers, i.e. integers that will never overflow
 * you multiply the maximum value with 2, or add 2, or subtract something from
 * the minimum value, etc.
 * @tparam T     the type these integers are stored with.
 * @tparam T_MAX the maximum value for the integers.
 * @tparam T_MIN the minimum value for the integers.
 */
template <class T, T T_MAX, T T_MIN>
class OverflowSafeInt
{
private:
	/** Unsigned datatype for comparing absolute values. */
	typedef typename std::make_unsigned<T>::type U;

	/** The non-overflow safe backend to store the value in. */
	T m_value;

	/**
	 * Overflow-safe version of the absolute value function.
	 */
	static inline U abs(const T value)
	{
		/* Verify that the datatypes meet the requirements (U must hold all absolute values of T) */
		assert_compile(std::numeric_limits<U>::max() >= static_cast<U>(std::numeric_limits<T>::max()));
		assert_compile(std::numeric_limits<U>::max() >= static_cast<U>(std::numeric_limits<T>::max()) + static_cast<U>(((std::numeric_limits<T>::min() + std::numeric_limits<T>::max()) < 0) ? -(std::numeric_limits<T>::min() + std::numeric_limits<T>::max()) : (std::numeric_limits<T>::min() + std::numeric_limits<T>::max())));

		/* Test for positive value */
		if (likely(value >= 0)) return static_cast<U>(value);

		/* Test for "normal" negative value */
		if (likely(std::numeric_limits<T>::max() + value >= 0)) return static_cast<U>(-value);

		/* Handle "abnormal" negative value */
		U delta = static_cast<U>(-(std::numeric_limits<T>::max() + value));
		return static_cast<U>(std::numeric_limits<T>::max()) + delta;
	}

	/**
	 * Return the saturation limit in the given direction.
	 */
	static inline T Limit(const T predicate)
	{
		return (predicate > 0) ? T_MAX : T_MIN;
	}

	/**
	 * Return the saturation limit in the opposite direction.
	 */
	static inline T InvLimit(const T predicate)
	{
		return (predicate < 0) ? T_MAX : T_MIN;
	}

public:
	OverflowSafeInt() : m_value(0) { }

	OverflowSafeInt(const OverflowSafeInt& other) { this->m_value = other.m_value; }
	OverflowSafeInt(const int64 int_)             { this->m_value = int_; }

	inline OverflowSafeInt& operator = (const OverflowSafeInt& other) { this->m_value = other.m_value; return *this; }

	inline OverflowSafeInt operator - () const
	{
		if (unlikely(abs(this->m_value) > abs(InvLimit(this->m_value)))) {
			return OverflowSafeInt(InvLimit(this->m_value));
		} else {
			return OverflowSafeInt(-this->m_value);
		}
	}

	/**
	 * Safe implementation of addition.
	 * @param other the amount to add
	 * @note when the addition would yield more than T_MAX (or less than T_MIN),
	 *       it will be T_MAX (respectively T_MIN).
	 */
	inline OverflowSafeInt& operator += (const OverflowSafeInt& other)
	{
		if (likely(signum(this->m_value) == signum(other.m_value)) &&
				unlikely((abs(Limit(this->m_value)) - abs(other.m_value)) < abs(this->m_value))) {
			this->m_value = Limit(this->m_value);
		} else {
			this->m_value += other.m_value;
		}
		return *this;
	}

	/* Operators for addition and subtraction */
	inline OverflowSafeInt  operator +  (const OverflowSafeInt& other) const { OverflowSafeInt result = *this; result += other; return result; }
	inline OverflowSafeInt  operator +  (const int              other) const { OverflowSafeInt result = *this; result += (int64)other; return result; }
	inline OverflowSafeInt  operator +  (const uint             other) const { OverflowSafeInt result = *this; result += (int64)other; return result; }
	inline OverflowSafeInt& operator -= (const OverflowSafeInt& other)       { return *this += (-other); }
	inline OverflowSafeInt  operator -  (const OverflowSafeInt& other) const { OverflowSafeInt result = *this; result -= other; return result; }
	inline OverflowSafeInt  operator -  (const int              other) const { OverflowSafeInt result = *this; result -= (int64)other; return result; }
	inline OverflowSafeInt  operator -  (const uint             other) const { OverflowSafeInt result = *this; result -= (int64)other; return result; }

	inline OverflowSafeInt& operator ++ () { return *this += 1; }
	inline OverflowSafeInt& operator -- () { return *this += -1; }
	inline OverflowSafeInt operator ++ (int) { OverflowSafeInt org = *this; *this += 1; return org; }
	inline OverflowSafeInt operator -- (int) { OverflowSafeInt org = *this; *this += -1; return org; }

	/**
	 * Safe implementation of multiplication.
	 * @param factor the factor to multiply this with.
	 * @note when the multiplication would yield more than T_MAX (or less than T_MIN),
	 *       it will be T_MAX (respectively T_MIN).
	 */
	inline OverflowSafeInt& operator *= (const int factor)
	{
		const T limit = Limit(signum(this->m_value) * signum(factor));
		if (likely(factor != 0) && unlikely((abs(limit) / abs(factor)) < abs(this->m_value))) {
			 this->m_value = limit;
		} else {
			this->m_value *= factor;
		}
		return *this;
	}

	/* Operators for multiplication */
	inline OverflowSafeInt operator * (const int64  factor) const { OverflowSafeInt result = *this; result *= factor; return result; }
	inline OverflowSafeInt operator * (const int    factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }
	inline OverflowSafeInt operator * (const uint   factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }
	inline OverflowSafeInt operator * (const uint16 factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }
	inline OverflowSafeInt operator * (const byte   factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }

	/* Operators for division */
	inline OverflowSafeInt& operator /= (const int64            divisor)       { this->m_value /= divisor; return *this; }
	inline OverflowSafeInt  operator /  (const OverflowSafeInt& divisor) const { OverflowSafeInt result = *this; result /= divisor.m_value; return result; }
	inline OverflowSafeInt  operator /  (const int              divisor) const { OverflowSafeInt result = *this; result /= divisor; return result; }
	inline OverflowSafeInt  operator /  (const uint             divisor) const { OverflowSafeInt result = *this; result /= (int)divisor; return result; }

	/* Operators for modulo */
	inline OverflowSafeInt& operator %= (const int  divisor)       { this->m_value %= divisor; return *this; }
	inline OverflowSafeInt  operator %  (const int  divisor) const { OverflowSafeInt result = *this; result %= divisor; return result; }

	/* Operators for shifting */
	inline OverflowSafeInt& operator <<= (const int shift)       { this->m_value <<= shift; return *this; }
	inline OverflowSafeInt  operator <<  (const int shift) const { OverflowSafeInt result = *this; result <<= shift; return result; }
	inline OverflowSafeInt& operator >>= (const int shift)       { this->m_value >>= shift; return *this; }
	inline OverflowSafeInt  operator >>  (const int shift) const { OverflowSafeInt result = *this; result >>= shift; return result; }

	/* Operators for (in)equality when comparing overflow safe ints */
	inline bool operator == (const OverflowSafeInt& other) const { return this->m_value == other.m_value; }
	inline bool operator != (const OverflowSafeInt& other) const { return !(*this == other); }
	inline bool operator >  (const OverflowSafeInt& other) const { return this->m_value > other.m_value; }
	inline bool operator >= (const OverflowSafeInt& other) const { return this->m_value >= other.m_value; }
	inline bool operator <  (const OverflowSafeInt& other) const { return !(*this >= other); }
	inline bool operator <= (const OverflowSafeInt& other) const { return !(*this > other); }

	/* Operators for (in)equality when comparing non-overflow safe ints */
	inline bool operator == (const int other) const { return this->m_value == other; }
	inline bool operator != (const int other) const { return !(*this == other); }
	inline bool operator >  (const int other) const { return this->m_value > other; }
	inline bool operator >= (const int other) const { return this->m_value >= other; }
	inline bool operator <  (const int other) const { return !(*this >= other); }
	inline bool operator <= (const int other) const { return !(*this > other); }

	inline operator int64 () const { return this->m_value; }
};

/* Sometimes we got int64 operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator + (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator - (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator * (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator / (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

/* Sometimes we got int operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator + (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator - (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator * (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator / (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

/* Sometimes we got uint operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator + (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator - (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator * (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator / (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

/* Sometimes we got byte operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator + (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + (uint)a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator - (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + (uint)a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator * (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * (uint)a; }
template <class T, int64 T_MAX, int64 T_MIN> inline OverflowSafeInt<T, T_MAX, T_MIN> operator / (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

typedef OverflowSafeInt<int64, INT64_MAX, INT64_MIN> OverflowSafeInt64;
typedef OverflowSafeInt<int32, INT32_MAX, INT32_MIN> OverflowSafeInt32;

#endif /* OVERFLOWSAFE_TYPE_HPP */
