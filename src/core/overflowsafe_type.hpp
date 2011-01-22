/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file overflowsafe_type.hpp An overflow safe integer-like type. */

#ifndef OVERFLOWSAFE_TYPE_HPP
#define OVERFLOWSAFE_TYPE_HPP


/**
 * Overflow safe template for integers, i.e. integers that will never overflow
 * you multiply the maximum value with 2, or add 2, or substract somethng from
 * the minimum value, etc.
 * @param T     the type these integers are stored with.
 * @param T_MAX the maximum value for the integers.
 * @param T_MIN the minimum value for the integers.
 */
template <class T, T T_MAX, T T_MIN>
class OverflowSafeInt
{
private:
	/** The non-overflow safe backend to store the value in. */
	T m_value;
public:
	OverflowSafeInt() : m_value(0) { }

	OverflowSafeInt(const OverflowSafeInt& other) { this->m_value = other.m_value; }
	OverflowSafeInt(const int64 int_)             { this->m_value = int_; }

	FORCEINLINE OverflowSafeInt& operator = (const OverflowSafeInt& other) { this->m_value = other.m_value; return *this; }

	FORCEINLINE OverflowSafeInt operator - () const { return OverflowSafeInt(-this->m_value); }

	/**
	 * Safe implementation of addition.
	 * @param other the amount to add
	 * @note when the addition would yield more than T_MAX (or less than T_MIN),
	 *       it will be T_MAX (respectively T_MIN).
	 */
	FORCEINLINE OverflowSafeInt& operator += (const OverflowSafeInt& other)
	{
		if ((T_MAX - abs(other.m_value)) < abs(this->m_value) &&
				(this->m_value < 0) == (other.m_value < 0)) {
			this->m_value = (this->m_value < 0) ? T_MIN : T_MAX ;
		} else {
			this->m_value += other.m_value;
		}
		return *this;
	}

	/* Operators for addition and substraction */
	FORCEINLINE OverflowSafeInt  operator +  (const OverflowSafeInt& other) const { OverflowSafeInt result = *this; result += other; return result; }
	FORCEINLINE OverflowSafeInt  operator +  (const int              other) const { OverflowSafeInt result = *this; result += (int64)other; return result; }
	FORCEINLINE OverflowSafeInt  operator +  (const uint             other) const { OverflowSafeInt result = *this; result += (int64)other; return result; }
	FORCEINLINE OverflowSafeInt& operator -= (const OverflowSafeInt& other)       { return *this += (-other); }
	FORCEINLINE OverflowSafeInt  operator -  (const OverflowSafeInt& other) const { OverflowSafeInt result = *this; result -= other; return result; }
	FORCEINLINE OverflowSafeInt  operator -  (const int              other) const { OverflowSafeInt result = *this; result -= (int64)other; return result; }
	FORCEINLINE OverflowSafeInt  operator -  (const uint             other) const { OverflowSafeInt result = *this; result -= (int64)other; return result; }

	FORCEINLINE OverflowSafeInt& operator ++ () { return *this += 1; }
	FORCEINLINE OverflowSafeInt& operator -- () { return *this += -1; }
	FORCEINLINE OverflowSafeInt operator ++ (int) { OverflowSafeInt org = *this; *this += 1; return org; }
	FORCEINLINE OverflowSafeInt operator -- (int) { OverflowSafeInt org = *this; *this += -1; return org; }

	/**
	 * Safe implementation of multiplication.
	 * @param factor the factor to multiply this with.
	 * @note when the multiplication would yield more than T_MAX (or less than T_MIN),
	 *       it will be T_MAX (respectively T_MIN).
	 */
	FORCEINLINE OverflowSafeInt& operator *= (const int factor)
	{
		if (factor != 0 && (T_MAX / abs(factor)) < abs(this->m_value)) {
			 this->m_value = ((this->m_value < 0) == (factor < 0)) ? T_MAX : T_MIN ;
		} else {
			this->m_value *= factor ;
		}
		return *this;
	}

	/* Operators for multiplication */
	FORCEINLINE OverflowSafeInt operator * (const int64  factor) const { OverflowSafeInt result = *this; result *= factor; return result; }
	FORCEINLINE OverflowSafeInt operator * (const int    factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }
	FORCEINLINE OverflowSafeInt operator * (const uint   factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }
	FORCEINLINE OverflowSafeInt operator * (const uint16 factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }
	FORCEINLINE OverflowSafeInt operator * (const byte   factor) const { OverflowSafeInt result = *this; result *= (int64)factor; return result; }

	/* Operators for division */
	FORCEINLINE OverflowSafeInt& operator /= (const int64            divisor)       { this->m_value /= divisor; return *this; }
	FORCEINLINE OverflowSafeInt  operator /  (const OverflowSafeInt& divisor) const { OverflowSafeInt result = *this; result /= divisor.m_value; return result; }
	FORCEINLINE OverflowSafeInt  operator /  (const int              divisor) const { OverflowSafeInt result = *this; result /= divisor; return result; }
	FORCEINLINE OverflowSafeInt  operator /  (const uint             divisor) const { OverflowSafeInt result = *this; result /= (int)divisor; return result; }

	/* Operators for modulo */
	FORCEINLINE OverflowSafeInt& operator %= (const int  divisor)       { this->m_value %= divisor; return *this; }
	FORCEINLINE OverflowSafeInt  operator %  (const int  divisor) const { OverflowSafeInt result = *this; result %= divisor; return result; }

	/* Operators for shifting */
	FORCEINLINE OverflowSafeInt& operator <<= (const int shift)       { this->m_value <<= shift; return *this; }
	FORCEINLINE OverflowSafeInt  operator <<  (const int shift) const { OverflowSafeInt result = *this; result <<= shift; return result; }
	FORCEINLINE OverflowSafeInt& operator >>= (const int shift)       { this->m_value >>= shift; return *this; }
	FORCEINLINE OverflowSafeInt  operator >>  (const int shift) const { OverflowSafeInt result = *this; result >>= shift; return result; }

	/* Operators for (in)equality when comparing overflow safe ints */
	FORCEINLINE bool operator == (const OverflowSafeInt& other) const { return this->m_value == other.m_value; }
	FORCEINLINE bool operator != (const OverflowSafeInt& other) const { return !(*this == other); }
	FORCEINLINE bool operator >  (const OverflowSafeInt& other) const { return this->m_value > other.m_value; }
	FORCEINLINE bool operator >= (const OverflowSafeInt& other) const { return this->m_value >= other.m_value; }
	FORCEINLINE bool operator <  (const OverflowSafeInt& other) const { return !(*this >= other); }
	FORCEINLINE bool operator <= (const OverflowSafeInt& other) const { return !(*this > other); }

	/* Operators for (in)equality when comparing non-overflow safe ints */
	FORCEINLINE bool operator == (const int other) const { return this->m_value == other; }
	FORCEINLINE bool operator != (const int other) const { return !(*this == other); }
	FORCEINLINE bool operator >  (const int other) const { return this->m_value > other; }
	FORCEINLINE bool operator >= (const int other) const { return this->m_value >= other; }
	FORCEINLINE bool operator <  (const int other) const { return !(*this >= other); }
	FORCEINLINE bool operator <= (const int other) const { return !(*this > other); }

	FORCEINLINE operator int64 () const { return this->m_value; }
};

/* Sometimes we got int64 operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator + (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator - (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator * (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator / (int64 a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

/* Sometimes we got int operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator + (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator - (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator * (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator / (int   a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

/* Sometimes we got uint operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator + (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator - (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator * (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator / (uint  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

/* Sometimes we got byte operator OverflowSafeInt instead of vice versa. Handle that properly */
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator + (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + (uint)a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator - (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + (uint)a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator * (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * (uint)a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator / (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

typedef OverflowSafeInt<int64, INT64_MAX, INT64_MIN> OverflowSafeInt64;

#endif /* OVERFLOWSAFE_TYPE_HPP */
