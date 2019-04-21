/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file enum_type.hpp Type (helpers) for enums */

#ifndef ENUM_TYPE_HPP
#define ENUM_TYPE_HPP

#include <type_traits>

/** Some enums need to have allowed incrementing (i.e. StationClassID) */
#define DECLARE_POSTFIX_INCREMENT(enum_type) \
	inline enum_type operator ++(enum_type& e, int) \
	{ \
		enum_type e_org = e; \
		e = (enum_type)((std::underlying_type<enum_type>::type)e + 1); \
		return e_org; \
	} \
	inline enum_type operator --(enum_type& e, int) \
	{ \
		enum_type e_org = e; \
		e = (enum_type)((std::underlying_type<enum_type>::type)e - 1); \
		return e_org; \
	}



/** Operators to allow to work with enum as with type safe bit set in C++ */
# define DECLARE_ENUM_AS_BIT_SET(mask_t) \
	inline mask_t operator | (mask_t m1, mask_t m2) {return (mask_t)((std::underlying_type<mask_t>::type)m1 | m2);} \
	inline mask_t operator & (mask_t m1, mask_t m2) {return (mask_t)((std::underlying_type<mask_t>::type)m1 & m2);} \
	inline mask_t operator ^ (mask_t m1, mask_t m2) {return (mask_t)((std::underlying_type<mask_t>::type)m1 ^ m2);} \
	inline mask_t& operator |= (mask_t& m1, mask_t m2) {m1 = m1 | m2; return m1;} \
	inline mask_t& operator &= (mask_t& m1, mask_t m2) {m1 = m1 & m2; return m1;} \
	inline mask_t& operator ^= (mask_t& m1, mask_t m2) {m1 = m1 ^ m2; return m1;} \
	inline mask_t operator ~(mask_t m) {return (mask_t)(~(std::underlying_type<mask_t>::type)m);}


/**
 * Informative template class exposing basic enumeration properties used by several
 *  other templates below. Here we have only forward declaration. For each enum type
 *  we will create specialization derived from MakeEnumPropsT<>.
 *  i.e.:
 *    template <> struct EnumPropsT<Track> : MakeEnumPropsT<Track, byte, TRACK_BEGIN, TRACK_END, INVALID_TRACK> {};
 *  followed by:
 *    typedef TinyEnumT<Track> TrackByte;
 */
template <typename Tenum_t> struct EnumPropsT;

/**
 * Helper template class that makes basic properties of given enumeration type visible
 *  from outsize. It is used as base class of several EnumPropsT specializations each
 *  dedicated to one of commonly used enumeration types.
 *  @param Tenum_t enumeration type that you want to describe
 *  @param Tstorage_t what storage type would be sufficient (i.e. byte)
 *  @param Tbegin first valid value from the contiguous range (i.e. TRACK_BEGIN)
 *  @param Tend one past the last valid value from the contiguous range (i.e. TRACK_END)
 *  @param Tinvalid value used as invalid value marker (i.e. INVALID_TRACK)
 *  @param Tnum_bits Number of bits for storing the enum in command parameters
 */
template <typename Tenum_t, typename Tstorage_t, Tenum_t Tbegin, Tenum_t Tend, Tenum_t Tinvalid, uint Tnum_bits = 8 * sizeof(Tstorage_t)>
struct MakeEnumPropsT {
	typedef Tenum_t type;                     ///< enum type (i.e. Trackdir)
	typedef Tstorage_t storage;               ///< storage type (i.e. byte)
	static const Tenum_t begin = Tbegin;      ///< lowest valid value (i.e. TRACKDIR_BEGIN)
	static const Tenum_t end = Tend;          ///< one after the last valid value (i.e. TRACKDIR_END)
	static const Tenum_t invalid = Tinvalid;  ///< what value is used as invalid value (i.e. INVALID_TRACKDIR)
	static const uint num_bits = Tnum_bits;   ///< Number of bits for storing the enum in command parameters
};



/**
 * In some cases we use byte or uint16 to store values that are defined as enum. It is
 *  necessary in order to control the sizeof() such values. Some compilers make enum
 *  the same size as int (4 or 8 bytes instead of 1 or 2). As a consequence the strict
 *  compiler type - checking causes errors like:
 *     'HasPowerOnRail' : cannot convert parameter 1 from 'byte' to 'RailType' when
 *  u->u.rail.railtype is passed as argument or type RailType. In such cases it is better
 *  to teach the compiler that u->u.rail.railtype is to be treated as RailType.
 */
template <typename Tenum_t> struct TinyEnumT;

/** The general declaration of TinyEnumT<> (above) */
template <typename Tenum_t>
struct TinyEnumT {
	typedef Tenum_t enum_type;                      ///< expose our enumeration type (i.e. Trackdir) to outside
	typedef EnumPropsT<Tenum_t> Props;              ///< make easier access to our enumeration properties
	typedef typename Props::storage storage_type;   ///< small storage type
	static const enum_type begin = Props::begin;    ///< enum beginning (i.e. TRACKDIR_BEGIN)
	static const enum_type end = Props::end;        ///< enum end (i.e. TRACKDIR_END)
	static const enum_type invalid = Props::invalid;///< invalid value (i.e. INVALID_TRACKDIR)

	storage_type m_val;  ///< here we hold the actual value in small (i.e. byte) form

	/** Cast operator - invoked then the value is assigned to the Tenum_t type */
	inline operator enum_type () const
	{
		return (enum_type)m_val;
	}

	/** Assignment operator (from Tenum_t type) */
	inline TinyEnumT& operator = (enum_type e)
	{
		m_val = (storage_type)e;
		return *this;
	}

	/** Assignment operator (from Tenum_t type) */
	inline TinyEnumT& operator = (uint u)
	{
		m_val = (storage_type)u;
		return *this;
	}

	/** postfix ++ operator on tiny type */
	inline TinyEnumT operator ++ (int)
	{
		TinyEnumT org = *this;
		if (++m_val >= end) m_val -= (storage_type)(end - begin);
		return org;
	}

	/** prefix ++ operator on tiny type */
	inline TinyEnumT& operator ++ ()
	{
		if (++m_val >= end) m_val -= (storage_type)(end - begin);
		return *this;
	}
};


/** Template of struct holding enum types (on most archs, enums are stored in an int32). No math operators are provided. */
template <typename enum_type, typename storage_type>
struct SimpleTinyEnumT {
	storage_type m_val;  ///< here we hold the actual value in small (i.e. byte) form

	/** Cast operator - invoked then the value is assigned to the storage_type */
	inline operator enum_type () const
	{
		return (enum_type)this->m_val;
	}

	/** Assignment operator (from enum_type) */
	inline SimpleTinyEnumT &operator = (enum_type e)
	{
		this->m_val = (storage_type)e;
		return *this;
	}

	/** Assignment operator (from general uint) */
	inline SimpleTinyEnumT &operator = (uint u)
	{
		this->m_val = (storage_type)u;
		return *this;
	}

	/** Bit math (or) assignment operator (from enum_type) */
	inline SimpleTinyEnumT &operator |= (enum_type e)
	{
		this->m_val = (storage_type)((enum_type)this->m_val | e);
		return *this;
	}

	/** Bit math (and) assignment operator (from enum_type) */
	inline SimpleTinyEnumT &operator &= (enum_type e)
	{
		this->m_val = (storage_type)((enum_type)this->m_val & e);
		return *this;
	}
};

#endif /* ENUM_TYPE_HPP */
