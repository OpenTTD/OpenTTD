/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file array.hpp Array without an explicit maximum size. */

#ifndef  ARRAY_HPP
#define  ARRAY_HPP

#include "fixedsizearray.hpp"
#include "str.hpp"

/** Flexible array with size limit. Implemented as fixed size
 *  array of fixed size arrays */
template <class T, int B = 1024, int N = B>
class SmallArray {
public:
	typedef T Titem; ///< Titem is now visible from outside
	typedef FixedSizeArray<T, B> SubArray; ///< inner array
	typedef FixedSizeArray<SubArray, N> SuperArray; ///< outer array

protected:
	SuperArray data; ///< array of arrays of items

public:
	static const int Tblock_size = B; ///< block size is now visible from outside
	static const int Tnum_blocks = N; ///< number of blocks is now visible from outside
	static const int Tcapacity = B * N; ///< total max number of items

	/** implicit constructor */
	FORCEINLINE SmallArray() { }
	/** Clear (destroy) all items */
	FORCEINLINE void Clear() {data.Clear();}
	/** Return actual number of items */
	FORCEINLINE int Length() const
	{
		int super_size = data.Length();
		if (super_size == 0) return 0;
		int sub_size = data[super_size - 1].Length();
		return (super_size - 1) * Tblock_size + sub_size;
	}
	/** return true if array is empty */
	FORCEINLINE bool IsEmpty() { return data.IsEmpty(); }
	/** return true if array is full */
	FORCEINLINE bool IsFull() { return data.IsFull() && data[Tnum_blocks - 1].IsFull(); }
	/** return first sub-array with free space for new item */
	FORCEINLINE SubArray& FirstFreeSubArray()
	{
		int super_size = data.Length();
		if (super_size > 0) {
			SubArray& s = data[super_size - 1];
			if (!s.IsFull()) return s;
		}
		return data.AppendC();
	}
	/** allocate but not construct new item */
	FORCEINLINE T& Append() { return FirstFreeSubArray().Append(); }
	/** allocate and construct new item */
	FORCEINLINE T& AppendC() { return FirstFreeSubArray().AppendC(); }
	/** indexed access (non-const) */
	FORCEINLINE Titem& operator [] (int index)
	{
		SubArray& s = data[index / Tblock_size];
		Titem& item = s[index % Tblock_size];
		return item;
	}
	/** indexed access (const) */
	FORCEINLINE const Titem& operator [] (int index) const
	{
		const SubArray& s = data[index / Tblock_size];
		const Titem& item = s[index % Tblock_size];
		return item;
	}

	template <typename D> void Dump(D &dmp) const
	{
		dmp.WriteLine("capacity = %d", Tcapacity);
		int num_items = Length();
		dmp.WriteLine("num_items = %d", num_items);
		CStrA name;
		for (int i = 0; i < num_items; i++) {
			const Titem& item = (*this)[i];
			name.Format("item[%d]", i);
			dmp.WriteStructT(name.Data(), &item);
		}
	}
};

#endif /* ARRAY_HPP */
