/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fixedsizearray.hpp A fixed size array that doesn't create items until needed. */

#ifndef FIXEDSIZEARRAY_HPP
#define FIXEDSIZEARRAY_HPP

#include "../core/alloc_func.hpp"

/**
 * fixed size array
 *  Upon construction it preallocates fixed size block of memory
 *  for all items, but doesn't construct them. Item's construction
 *  is delayed.
 */
template <class T, uint C>
struct FixedSizeArray {
protected:
	/** header for fixed size array */
	struct ArrayHeader
	{
		uint items;           ///< number of items in the array
		uint reference_count; ///< block reference counter (used by copy constructor and by destructor)
	};

	/* make constants visible from outside */
	static const uint Tsize = sizeof(T);                ///< size of item
	static const uint HeaderSize = sizeof(ArrayHeader); ///< size of header

	/**
	 * the only member of fixed size array is pointer to the block
	 *  of C array of items. Header can be found on the offset -sizeof(ArrayHeader).
	 */
	T *data;

	/** return reference to the array header (non-const) */
	inline ArrayHeader &Hdr()
	{
		return *(ArrayHeader*)(((byte*)data) - HeaderSize);
	}

	/** return reference to the array header (const) */
	inline const ArrayHeader &Hdr() const
	{
		return *(ArrayHeader*)(((byte*)data) - HeaderSize);
	}

	/** return reference to the block reference counter */
	inline uint &RefCnt()
	{
		return Hdr().reference_count;
	}

	/** return reference to number of used items */
	inline uint &SizeRef()
	{
		return Hdr().items;
	}

public:
	/** Default constructor. Preallocate space for items and header, then initialize header. */
	FixedSizeArray()
	{
		/* Ensure the size won't overflow. */
		static_assert(C < (SIZE_MAX - HeaderSize) / Tsize);

		/* allocate block for header + items (don't construct items) */
		data = (T*)((MallocT<byte>(HeaderSize + C * Tsize)) + HeaderSize);
		SizeRef() = 0; // initial number of items
		RefCnt() = 1; // initial reference counter
	}

	/** Copy constructor. Preallocate space for items and header, then initialize header. */
	FixedSizeArray(const FixedSizeArray<T, C> &src)
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
		free(((byte*)data) - HeaderSize);
		data = nullptr;
	}

	/** Clear (destroy) all items */
	inline void Clear()
	{
		/* Walk through all allocated items backward and destroy them
		 * Note: this->Length() can be zero. In that case data[this->Length() - 1] is evaluated unsigned
		 *       on some compilers with some architectures. (e.g. gcc with x86) */
		for (T *pItem = this->data + this->Length() - 1; pItem >= this->data; pItem--) {
			pItem->~T();
		}
		/* number of items become zero */
		SizeRef() = 0;
	}

	/** return number of used items */
	inline uint Length() const
	{
		return Hdr().items;
	}

	/** return true if array is full */
	inline bool IsFull() const
	{
		return Length() >= C;
	}

	/** return true if array is empty */
	inline bool IsEmpty() const
	{
		return Length() <= 0;
	}

	/** add (allocate), but don't construct item */
	inline T *Append()
	{
		assert(!IsFull());
		return &data[SizeRef()++];
	}

	/** add and construct item using default constructor */
	inline T *AppendC()
	{
		T *item = Append();
		new(item)T;
		return item;
	}
	/** return item by index (non-const version) */
	inline T& operator[](uint index)
	{
		assert(index < Length());
		return data[index];
	}

	/** return item by index (const version) */
	inline const T& operator[](uint index) const
	{
		assert(index < Length());
		return data[index];
	}
};

#endif /* FIXEDSIZEARRAY_HPP */
