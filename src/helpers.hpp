/* $Id$ */

#ifndef HELPERS_HPP
#define HELPERS_HPP

/** @file helpers.hpp */
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


/** returns the absolute value of (scalar) variable. @note assumes variable to be signed */
template <typename T> static inline T myabs(T a) { return a < (T)0 ? -a : a; }
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

/** probably redundant enum combining operators (as we have conversion functions)
 *  but the old code is full of such arithmetics */
# define DECLARE_ENUM_AS_BIT_INDEX(idx_t, mask_t) \
	FORCEINLINE mask_t operator << (int m, idx_t i) {return (mask_t)(m << (int)i);} \
	FORCEINLINE mask_t operator << (mask_t m, int i) {return (mask_t)(((int)m) << i);} \
	FORCEINLINE mask_t operator >> (mask_t m, int i) {return (mask_t)(((int)m) >> i);}


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
	FORCEINLINE TinyEnumT& operator ++ (int)
	{
		if (++m_val >= end) m_val -= (storage_type)(end - begin);
		return *this;
	}
};

template <typename T> void ClrBitT(T &t, int bit_index)
{
	t = (T)(t & ~((T)1 << bit_index));
}

template <typename T> void SetBitT(T &t, int bit_index)
{
	t = (T)(t | ((T)1 << bit_index));
}

template <typename T> void ToggleBitT(T &t, int bit_index)
{
	t = (T)(t ^ ((T)1 << bit_index));
}

#endif /* HELPERS_HPP */
