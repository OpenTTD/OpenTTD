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
#include <vector>
#include <algorithm>

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
class SmallVector : public std::vector<T> {
public:
	SmallVector() = default;

	/**
	 * Copy constructor.
	 * @param other The other vector to copy.
	 */
	SmallVector(const SmallVector &other) = default;

	/**
	 * Generic copy constructor.
	 * @param other The other vector to copy.
	 */
	template <uint X>
	SmallVector(const SmallVector<T, X> &other) : std::vector<T>(other)
	{
	}

	/**
	 * Assignment.
	 * @param other The other vector to assign.
	 */
	SmallVector &operator=(const SmallVector &other) = default;

	/**
	 * Generic assignment.
	 * @param other The other vector to assign.
	 */
	template <uint X>
	SmallVector &operator=(const SmallVector<T, X> &other)
	{
		this->Assign(other);
		return *this;
	}

	~SmallVector() = default;

	/**
	 * Assign items from other vector.
	 */
	template <uint X>
	inline void Assign(const SmallVector<T, X> &other)
	{
		if ((const void *)&other == (void *)this) return;

		std::vector<T>::operator=(other);
	}

	/**
	 * Remove all items from the list and free allocated memory.
	 */
	inline void Reset()
	{
		std::vector<T>::clear();
		std::vector<T>::shrink_to_fit();
	}

	/**
	 * Append an item and return it.
	 * @param to_add the number of items to append
	 * @return pointer to newly allocated item
	 */
	inline T *Append(uint to_add = 1)
	{
		std::vector<T>::resize(std::vector<T>::size() + to_add);
		return this->End() - to_add;
	}

	/**
	 * Set the size of the vector, effectively truncating items from the end or appending uninitialised ones.
	 * @param num_items Target size.
	 */
	inline void Resize(uint num_items)
	{
		std::vector<T>::resize(num_items);
	}

	/**
	 * Insert a new item at a specific position into the vector, moving all following items.
	 * @param item Position at which the new item should be inserted
	 * @return pointer to the new item
	 */
	inline T *Insert(T *item)
	{
		assert(item >= this->Begin() && item <= this->End());

		size_t start = item - this->Begin();
		std::vector<T>::insert(std::vector<T>::begin() + start);
		return this->Begin() + start;
	}

	/**
	 * Search for the first occurrence of an item.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to search for
	 * @return The position of the item, or End() when not present
	 */
	inline const T *Find(const T &item) const
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
	inline T *Find(const T &item)
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
	inline int FindIndex(const T &item) const
	{
		auto const it = this->Find(item);
		return it == this->End() ? -1 : it - this->Begin();
	}

	/**
	 * Tests whether a item is present in the vector.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to test for
	 * @return true iff the item is present
	 */
	inline bool Contains(const T &item) const
	{
		return this->Find(item) != this->End();
	}

	/**
	 * Removes given item from this vector
	 * @param item item to remove
	 * @note it has to be pointer to item in this map. It is overwritten by the last item.
	 */
	inline void Erase(T *item)
	{
		assert(item >= this->Begin() && item < this->End());
		*item = std::vector<T>::back();
		std::vector<T>::pop_back();
	}

	/**
	 * Remove items from the vector while preserving the order of other items.
	 * @param pos First item to remove.
	 * @param count Number of consecutive items to remove.
	 */
	void ErasePreservingOrder(uint pos, uint count = 1)
	{
		auto const it = std::vector<T>::begin() + pos;
		std::vector<T>::erase(it, it + count);
	}

	/**
	 * Remove items from the vector while preserving the order of other items.
	 * @param item First item to remove.
	 * @param count Number of consecutive items to remove.
	 */
	inline void ErasePreservingOrder(T *item, uint count = 1)
	{
		this->ErasePreservingOrder(item - this->Begin(), count);
	}

	/**
	 * Tests whether a item is present in the vector, and appends it to the end if not.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to test for
	 * @return true iff the item is was already present
	 */
	inline bool Include(const T &item)
	{
		bool is_member = this->Contains(item);
		if (!is_member) *this->Append() = item;
		return is_member;
	}

	/**
	 * Get the number of items in the list.
	 *
	 * @return The number of items in the list.
	 */
	inline uint Length() const
	{
		return std::vector<T>::size();
	}

	/**
	 * Get the pointer to the first item (const)
	 *
	 * @return the pointer to the first item
	 */
	inline const T *Begin() const
	{
		return std::vector<T>::data();
	}

	/**
	 * Get the pointer to the first item
	 *
	 * @return the pointer to the first item
	 */
	inline T *Begin()
	{
		return std::vector<T>::data();
	}

	/**
	 * Get the pointer behind the last valid item (const)
	 *
	 * @return the pointer behind the last valid item
	 */
	inline const T *End() const
	{
		return std::vector<T>::data() + std::vector<T>::size();
	}

	/**
	 * Get the pointer behind the last valid item
	 *
	 * @return the pointer behind the last valid item
	 */
	inline T *End()
	{
		return std::vector<T>::data() + std::vector<T>::size();
	}

	/**
	 * Get the pointer to item "number" (const)
	 *
	 * @param index the position of the item
	 * @return the pointer to the item
	 */
	inline const T *Get(uint index) const
	{
		/* Allow access to the 'first invalid' item */
		assert(index <= std::vector<T>::size());
		return this->Begin() + index;
	}

	/**
	 * Get the pointer to item "number"
	 *
	 * @param index the position of the item
	 * @return the pointer to the item
	 */
	inline T *Get(uint index)
	{
		/* Allow access to the 'first invalid' item */
		assert(index <= std::vector<T>::size());
		return this->Begin() + index;
	}

	/**
	 * Get item "number" (const)
	 *
	 * @param index the position of the item
	 * @return the item
	 */
	inline const T &operator[](uint index) const
	{
		assert(index < std::vector<T>::size());
		return std::vector<T>::operator[](index);
	}

	/**
	 * Get item "number"
	 *
	 * @param index the position of the item
	 * @return the item
	 */
	inline T &operator[](uint index)
	{
		assert(index < std::vector<T>::size());
		return std::vector<T>::operator[](index);
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
	inline void Clear()
	{
		for (uint i = 0; i < std::vector<T>::size(); i++) {
			free(std::vector<T>::operator[](i));
		}

		std::vector<T>::clear();
	}
};

/**
 * Simple vector template class, with automatic delete.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @param T The type of the items stored, must be a pointer
 * @param S The steps of allocation
 */
template <typename T, uint S>
class AutoDeleteSmallVector : public SmallVector<T, S> {
public:
	~AutoDeleteSmallVector()
	{
		this->Clear();
	}

	/**
	 * Remove all items from the list.
	 */
	inline void Clear()
	{
		for (uint i = 0; i < std::vector<T>::size(); i++) {
			delete std::vector<T>::operator[](i);
		}

		std::vector<T>::clear();
	}
};

typedef AutoFreeSmallVector<char*, 4> StringList; ///< Type for a list of strings.

#endif /* SMALLVEC_TYPE_HPP */
