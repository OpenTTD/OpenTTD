/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallvec_type.hpp Simple vector class that allows allocating an item without the need to copy this->data needlessly. */

#ifndef SMALLVEC_TYPE_HPP
#define SMALLVEC_TYPE_HPP

#include "alloc_func.hpp"
#include "mem_func.hpp"

/**
 * Simple vector template class.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @tparam T The type of the items stored
 * @tparam S The steps of allocation
 */
template <typename T, uint S>
class SmallVector {
protected:
	T *data;       ///< The pointer to the first item
	uint items;    ///< The number of items stored
	uint capacity; ///< The available space for storing items

public:
	SmallVector() : data(NULL), items(0), capacity(0) { }

	template <uint X>
	SmallVector(const SmallVector<T, X> &other) : data(NULL), items(0), capacity(0)
	{
		MemCpyT<T>(this->Append(other.Length()), other.Begin(), other.Length());
	}

	template <uint X>
	SmallVector &operator=(const SmallVector<T, X> &other)
	{
		this->Reset();
		MemCpyT<T>(this->Append(other.Length()), other.Begin(), other.Length());
		return *this;
	}

	~SmallVector()
	{
		free(this->data);
	}

	/**
	 * Remove all items from the list.
	 */
	FORCEINLINE void Clear()
	{
		/* In fact we just reset the item counter avoiding the need to
		 * probably reallocate the same amount of memory the list was
		 * previously using. */
		this->items = 0;
	}

	/**
	 * Remove all items from the list and free allocated memory.
	 */
	FORCEINLINE void Reset()
	{
		this->items = 0;
		this->capacity = 0;
		free(data);
		data = NULL;
	}

	/**
	 * Compact the list down to the smallest block size boundary.
	 */
	FORCEINLINE void Compact()
	{
		uint capacity = Align(this->items, S);
		if (capacity >= this->capacity) return;

		this->capacity = capacity;
		this->data = ReallocT(this->data, this->capacity);
	}

	/**
	 * Append an item and return it.
	 * @param to_add the number of items to append
	 * @return pointer to newly allocated item
	 */
	FORCEINLINE T *Append(uint to_add = 1)
	{
		uint begin = this->items;
		this->items += to_add;

		if (this->items > this->capacity) {
			this->capacity = Align(this->items, S);
			this->data = ReallocT(this->data, this->capacity);
		}

		return &this->data[begin];
	}

	/**
	 * Search for the first occurrence of an item.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to search for
	 * @return The position of the item, or End() when not present
	 */
	FORCEINLINE const T *Find(const T &item) const
	{
		const T *pos = this->Begin();
		const T *end = this->End();
		while (pos != end && *pos != item) pos++;
		return pos;
	}

	/**
	 * Search for the first occurrence of an item.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to search for
	 * @return The position of the item, or End() when not present
	 */
	FORCEINLINE T *Find(const T &item)
	{
		T *pos = this->Begin();
		const T *end = this->End();
		while (pos != end && *pos != item) pos++;
		return pos;
	}

	/**
	 * Search for the first occurrence of an item.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to search for
	 * @return The position of the item, or -1 when not present
	 */
	FORCEINLINE int FindIndex(const T &item)
	{
		int index = 0;
		T *pos = this->Begin();
		const T *end = this->End();
		while (pos != end && *pos != item) {
			pos++;
			index++;
		}
		return pos == end ? -1 : index;
	}

	/**
	 * Tests whether a item is present in the vector.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to test for
	 * @return true iff the item is present
	 */
	FORCEINLINE bool Contains(const T &item) const
	{
		return this->Find(item) != this->End();
	}

	/**
	 * Removes given item from this vector
	 * @param item item to remove
	 * @note it has to be pointer to item in this map. It is overwritten by the last item.
	 */
	FORCEINLINE void Erase(T *item)
	{
		assert(item >= this->Begin() && item < this->End());
		*item = this->data[--this->items];
	}

	/**
	 * Tests whether a item is present in the vector, and appends it to the end if not.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to test for
	 * @return true iff the item is was already present
	 */
	FORCEINLINE bool Include(const T &item)
	{
		bool is_member = this->Contains(item);
		if (!is_member) *this->Append() = item;
		return is_member;
	}

	/**
	 * Get the number of items in the list.
	 */
	FORCEINLINE uint Length() const
	{
		return this->items;
	}

	/**
	 * Get the pointer to the first item (const)
	 *
	 * @return the pointer to the first item
	 */
	FORCEINLINE const T *Begin() const
	{
		return this->data;
	}

	/**
	 * Get the pointer to the first item
	 *
	 * @return the pointer to the first item
	 */
	FORCEINLINE T *Begin()
	{
		return this->data;
	}

	/**
	 * Get the pointer behind the last valid item (const)
	 *
	 * @return the pointer behind the last valid item
	 */
	FORCEINLINE const T *End() const
	{
		return &this->data[this->items];
	}

	/**
	 * Get the pointer behind the last valid item
	 *
	 * @return the pointer behind the last valid item
	 */
	FORCEINLINE T *End()
	{
		return &this->data[this->items];
	}

	/**
	 * Get the pointer to item "number" (const)
	 *
	 * @param index the position of the item
	 * @return the pointer to the item
	 */
	FORCEINLINE const T *Get(uint index) const
	{
		/* Allow access to the 'first invalid' item */
		assert(index <= this->items);
		return &this->data[index];
	}

	/**
	 * Get the pointer to item "number"
	 *
	 * @param index the position of the item
	 * @return the pointer to the item
	 */
	FORCEINLINE T *Get(uint index)
	{
		/* Allow access to the 'first invalid' item */
		assert(index <= this->items);
		return &this->data[index];
	}

	/**
	 * Get item "number" (const)
	 *
	 * @param index the position of the item
	 * @return the item
	 */
	FORCEINLINE const T &operator[](uint index) const
	{
		assert(index < this->items);
		return this->data[index];
	}

	/**
	 * Get item "number"
	 *
	 * @param index the position of the item
	 * @return the item
	 */
	FORCEINLINE T &operator[](uint index)
	{
		assert(index < this->items);
		return this->data[index];
	}
};


/**
 * Simple vector template class, with automatic free.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @param T The type of the items stored, must be a pointer
 * @param S The steps of allocation
 */
template <typename T, uint S>
class AutoFreeSmallVector : public SmallVector<T, S> {
public:
	~AutoFreeSmallVector()
	{
		this->Clear();
	}

	/**
	 * Remove all items from the list.
	 */
	FORCEINLINE void Clear()
	{
		for (uint i = 0; i < this->items; i++) {
			free(this->data[i]);
		}

		this->items = 0;
	}
};

typedef AutoFreeSmallVector<char*, 4> StringList;

#endif /* SMALLVEC_TYPE_HPP */
