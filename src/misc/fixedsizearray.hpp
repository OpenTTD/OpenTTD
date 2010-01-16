/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fixedsizearray.hpp A fixed size array that doesn't create items until needed. */

#ifndef  FIXEDSIZEARRAY_HPP
#define  FIXEDSIZEARRAY_HPP

#include "../core/alloc_func.hpp"

/** fixed size array
 *  Upon construction it preallocates fixed size block of memory
 *  for all items, but doesn't construct them. Item's construction
 *  is delayed. */
template <class T, int C>
struct FixedSizeArray {
	/** the only member of fixed size array is pointer to the block
	 *  of C array of items. Header can be found on the offset -sizeof(ArrayHeader). */
	T *data;

	/** header for fixed size array */
	struct ArrayHeader
	{
		int items;           ///< number of items in the array
		int reference_count; ///< block reference counter (used by copy constructor and by destructor)
	};

	/* make types and constants visible from outside */
	typedef T Titem; // type of array item

	static const int Tcapacity = C;                    // the array capacity (maximum size)
	static const int Tsize = sizeof(T);                // size of item
	static const int HeaderSize = sizeof(ArrayHeader); // size of header

	/** Default constructor. Preallocate space for items and header, then initialize header. */
	FixedSizeArray()
	{
		/* allocate block for header + items (don't construct items) */
		data = (Titem*)((MallocT<int8>(HeaderSize + Tcapacity * Tsize)) + HeaderSize);
		SizeRef() = 0; // initial number of items
		RefCnt() = 1; // initial reference counter
	}

	/** Copy constructor. Preallocate space for items and header, then initialize header. */
	FixedSizeArray(const FixedSizeArray<T, C>& src)
	{
		/* share block (header + items) with the source array */
		data = src.data;
		RefCnt()++; // now we share block with the source
	}

	/** destroy remaining items and free the memory block */
	~FixedSizeArray()
	{
		/* release one reference to the shared block */
		if ((--RefCnt()) > 0) return; // and return if there is still some owner

		Clear();
		/* free the memory block occupied by items */
		free(((int8*)data) - HeaderSize);
		data = NULL;
	}

	/** Clear (destroy) all items */
	FORCEINLINE void Clear()
	{
		/* walk through all allocated items backward and destroy them */
		for (Titem *pItem = &data[Length() - 1]; pItem >= data; pItem--) {
			pItem->~T();
		}
		/* number of items become zero */
		SizeRef() = 0;
	}

protected:
	/** return reference to the array header (non-const) */
	FORCEINLINE ArrayHeader& Hdr() { return *(ArrayHeader*)(((int8*)data) - HeaderSize); }
	/** return reference to the array header (const) */
	FORCEINLINE const ArrayHeader& Hdr() const { return *(ArrayHeader*)(((int8*)data) - HeaderSize); }
	/** return reference to the block reference counter */
	FORCEINLINE int& RefCnt() { return Hdr().reference_count; }
	/** return reference to number of used items */
	FORCEINLINE int& SizeRef() { return Hdr().items; }
public:
	/** return number of used items */
	FORCEINLINE int Length() const { return Hdr().items; }
	/** return true if array is full */
	FORCEINLINE bool IsFull() const { return Length() >= Tcapacity; };
	/** return true if array is empty */
	FORCEINLINE bool IsEmpty() const { return Length() <= 0; };
	/** index validation */
	FORCEINLINE void CheckIdx(int index) const { assert(index >= 0); assert(index < Length()); }
	/** add (allocate), but don't construct item */
	FORCEINLINE Titem& Append() { assert(!IsFull()); return data[SizeRef()++]; }
	/** add and construct item using default constructor */
	FORCEINLINE Titem& AppendC() { Titem& item = Append(); new(&item)Titem; return item; }
	/** return item by index (non-const version) */
	FORCEINLINE Titem& operator [] (int index) { CheckIdx(index); return data[index]; }
	/** return item by index (const version) */
	FORCEINLINE const Titem& operator [] (int index) const { CheckIdx(index); return data[index]; }
};

#endif /* FIXEDSIZEARRAY_HPP */
