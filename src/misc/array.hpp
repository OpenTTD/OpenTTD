/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file array.hpp Array without an explicit maximum size. */

#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "fixedsizearray.hpp"
#include "../string_func.h"

/**
 * Flexible array with size limit. Implemented as fixed size
 *  array of fixed size arrays
 */
template <class T, uint B = 1024, uint N = B>
class SmallArray {
protected:
	typedef FixedSizeArray<T, B> SubArray; ///< inner array
	typedef FixedSizeArray<SubArray, N> SuperArray; ///< outer array

	static const uint Tcapacity = B * N; ///< total max number of items

	SuperArray data; ///< array of arrays of items

	/** return first sub-array with free space for new item */
	inline SubArray &FirstFreeSubArray()
	{
		uint super_size = data.Length();
		if (super_size > 0) {
			SubArray &s = data[super_size - 1];
			if (!s.IsFull()) return s;
		}
		return *data.AppendC();
	}

public:
	/** implicit constructor */
	inline SmallArray()
	{
	}

	/** Clear (destroy) all items */
	inline void Clear()
	{
		data.Clear();
	}

	/** Return actual number of items */
	inline uint Length() const
	{
		uint super_size = data.Length();
		if (super_size == 0) return 0;
		uint sub_size = data[super_size - 1].Length();
		return (super_size - 1) * B + sub_size;
	}
	/** return true if array is empty */
	inline bool IsEmpty()
	{
		return data.IsEmpty();
	}

	/** return true if array is full */
	inline bool IsFull()
	{
		return data.IsFull() && data[N - 1].IsFull();
	}

	/** allocate but not construct new item */
	inline T *Append()
	{
		return FirstFreeSubArray().Append();
	}

	/** allocate and construct new item */
	inline T *AppendC()
	{
		return FirstFreeSubArray().AppendC();
	}

	/** indexed access (non-const) */
	inline T& operator[](uint index)
	{
		const SubArray &s = data[index / B];
		T &item = s[index % B];
		return item;
	}
	/** indexed access (const) */
	inline const T& operator[](uint index) const
	{
		const SubArray &s = data[index / B];
		const T &item = s[index % B];
		return item;
	}

	/**
	 * Helper for creating a human readable output of this data.
	 * @param dmp The location to dump to.
	 */
	template <typename D> void Dump(D &dmp) const
	{
		dmp.WriteValue("capacity", Tcapacity);
		uint num_items = Length();
		dmp.WriteValue("num_items", num_items);
		for (uint i = 0; i < num_items; i++) {
			const T &item = (*this)[i];
			dmp.WriteStructT(fmt::format("item[{}]", i), &item);
		}
	}
};

#endif /* ARRAY_HPP */
