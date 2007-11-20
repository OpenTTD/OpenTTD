/* $Id$ */

/** @file helpers.hpp */

#ifndef HELPERS_HPP
#define HELPERS_HPP

#include "macros.h"

/** When allocating using malloc/calloc in C++ it is usually needed to cast the return value
*  from void* to the proper pointer type. Another alternative would be MallocT<> as follows */
template <typename T> FORCEINLINE T* MallocT(size_t num_elements)
{
	T *t_ptr = (T*)malloc(num_elements * sizeof(T));
	return t_ptr;
}
/** When allocating using malloc/calloc in C++ it is usually needed to cast the return value
*  from void* to the proper pointer type. Another alternative would be MallocT<> as follows */
template <typename T> FORCEINLINE T* CallocT(size_t num_elements)
{
	T *t_ptr = (T*)calloc(num_elements, sizeof(T));
	return t_ptr;
}
/** When allocating using malloc/calloc in C++ it is usually needed to cast the return value
*  from void* to the proper pointer type. Another alternative would be MallocT<> as follows */
template <typename T> FORCEINLINE T* ReallocT(T* t_ptr, size_t num_elements)
{
	t_ptr = (T*)realloc(t_ptr, num_elements * sizeof(T));
	return t_ptr;
}


/** type safe swap operation */
template<typename T> void Swap(T& a, T& b)
{
	T t = a;
	a = b;
	b = t;
}


/** returns the (absolute) difference between two (scalar) variables */
template <typename T> static inline T delta(T a, T b) { return a < b ? b - a : a - b; }

/** Some enums need to have allowed incrementing (i.e. StationClassID) */
#define DECLARE_POSTFIX_INCREMENT(type) \
	FORCEINLINE type operator ++(type& e, int) \
	{ \
		type e_org = e; \
		e = (type)((int)e + 1); \
		return e_org; \
	} \
	FORCEINLINE type operator --(type& e, int) \
	{ \
		type e_org = e; \
		e = (type)((int)e - 1); \
		return e_org; \
	}



/** Operators to allow to work with enum as with type safe bit set in C++ */
# define DECLARE_ENUM_AS_BIT_SET(mask_t) \
	FORCEINLINE mask_t operator | (mask_t m1, mask_t m2) {return (mask_t)((int)m1 | m2);} \
	FORCEINLINE mask_t operator & (mask_t m1, mask_t m2) {return (mask_t)((int)m1 & m2);} \
	FORCEINLINE mask_t operator ^ (mask_t m1, mask_t m2) {return (mask_t)((int)m1 ^ m2);} \
	FORCEINLINE mask_t& operator |= (mask_t& m1, mask_t m2) {m1 = m1 | m2; return m1;} \
	FORCEINLINE mask_t& operator &= (mask_t& m1, mask_t m2) {m1 = m1 & m2; return m1;} \
	FORCEINLINE mask_t& operator ^= (mask_t& m1, mask_t m2) {m1 = m1 ^ m2; return m1;} \
	FORCEINLINE mask_t operator ~(mask_t m) {return (mask_t)(~(int)m);}


/** Informative template class exposing basic enumeration properties used by several
 *  other templates below. Here we have only forward declaration. For each enum type
 *  we will create specialization derived from MakeEnumPropsT<>.
 *  i.e.:
 *    template <> struct EnumPropsT<Track> : MakeEnumPropsT<Track, byte, TRACK_BEGIN, TRACK_END, INVALID_TRACK> {};
 *  followed by:
 *    typedef TinyEnumT<Track> TrackByte;
 */
template <typename Tenum_t> struct EnumPropsT;

/** Helper template class that makes basic properties of given enumeration type visible
 *  from outsize. It is used as base class of several EnumPropsT specializations each
 *  dedicated to one of commonly used enumeration types.
 *  @param Tenum_t enumeration type that you want to describe
 *  @param Tstorage_t what storage type would be sufficient (i.e. byte)
 *  @param Tbegin first valid value from the contiguous range (i.e. TRACK_BEGIN)
 *  @param Tend one past the last valid value from the contiguous range (i.e. TRACK_END)
 *  @param Tinvalid value used as invalid value marker (i.e. INVALID_TRACK)
 */
template <typename Tenum_t, typename Tstorage_t, Tenum_t Tbegin, Tenum_t Tend, Tenum_t Tinvalid>
struct MakeEnumPropsT {
	typedef Tenum_t type;                     ///< enum type (i.e. Trackdir)
	typedef Tstorage_t storage;               ///< storage type (i.e. byte)
	static const Tenum_t begin = Tbegin;      ///< lowest valid value (i.e. TRACKDIR_BEGIN)
	static const Tenum_t end = Tend;          ///< one after the last valid value (i.e. TRACKDIR_END)
	static const Tenum_t invalid = Tinvalid;  ///< what value is used as invalid value (i.e. INVALID_TRACKDIR)
};



/** In some cases we use byte or uint16 to store values that are defined as enum. It is
	*  necessary in order to control the sizeof() such values. Some compilers make enum
	*  the same size as int (4 or 8 bytes instead of 1 or 2). As a consequence the strict
	*  compiler type-checking causes errors like:
	*     'HasPowerOnRail' : cannot convert parameter 1 from 'byte' to 'RailType' when
	*  u->u.rail.railtype is passed as argument or type RailType. In such cases it is better
	*  to teach the compiler that u->u.rail.railtype is to be treated as RailType. */
template <typename Tenum_t> struct TinyEnumT;

/** The general declaration of TinyEnumT<> (above) */
template <typename Tenum_t> struct TinyEnumT
{
	typedef Tenum_t enum_type;                      ///< expose our enumeration type (i.e. Trackdir) to outside
	typedef EnumPropsT<Tenum_t> Props;              ///< make easier access to our enumeration propeties
	typedef typename Props::storage storage_type;   ///< small storage type
	static const enum_type begin = Props::begin;    ///< enum beginning (i.e. TRACKDIR_BEGIN)
	static const enum_type end = Props::end;        ///< enum end (i.e. TRACKDIR_END)
	static const enum_type invalid = Props::invalid;///< invalid value (i.e. INVALID_TRACKDIR)

	storage_type m_val;  ///< here we hold the actual value in small (i.e. byte) form

	/** Cast operator - invoked then the value is assigned to the Tenum_t type */
	FORCEINLINE operator enum_type () const
	{
		return (enum_type)m_val;
	}

	/** Assignment operator (from Tenum_t type) */
	FORCEINLINE TinyEnumT& operator = (enum_type e)
	{
		m_val = (storage_type)e; return *this;
	}

	/** postfix ++ operator on tiny type */
	FORCEINLINE TinyEnumT operator ++ (int)
	{
		TinyEnumT org = *this;
		if (++m_val >= end) m_val -= (storage_type)(end - begin);
		return org;
	}

	/** prefix ++ operator on tiny type */
	FORCEINLINE TinyEnumT& operator ++ ()
	{
		if (++m_val >= end) m_val -= (storage_type)(end - begin);
		return *this;
	}
};

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
	FORCEINLINE OverflowSafeInt& operator /= (const int              divisor)       { this->m_value /= divisor; return *this; }
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
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator + (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator - (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return -b + a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator * (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return b * a; }
template <class T, int64 T_MAX, int64 T_MIN> FORCEINLINE OverflowSafeInt<T, T_MAX, T_MIN> operator / (byte  a, OverflowSafeInt<T, T_MAX, T_MIN> b) { return (OverflowSafeInt<T, T_MAX, T_MIN>)a / (int)b; }

#endif /* HELPERS_HPP */
