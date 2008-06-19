/* $Id$ */

/** @file smallvec_type.hpp Simple vector class that allows allocating an item without the need to copy this->data needlessly. */

#ifndef SMALLVEC_TYPE_HPP
#define SMALLVEC_TYPE_HPP

#include "alloc_func.hpp"
#include "math_func.hpp"

/**
 * Simple vector template class.
 *
 * @note There are no asserts in the class so you have
 *       to care about that you grab an item which is
 *       inside the list.
 *
 * @param T The type of the items stored
 * @param S The steps of allocation
 */
template <typename T, uint S>
class SmallVector {
protected:
	T *data;       ///< The pointer to the first item
	uint items;    ///< The number of items stored
	uint capacity; ///< The avalible space for storing items

public:
	SmallVector() : data(NULL), items(0), capacity(0) { }

	~SmallVector()
	{
		free(this->data);
	}

	/**
	 * Remove all items from the list.
	 */
	void Clear()
	{
		/* In fact we just reset the item counter avoiding the need to
		 * probably reallocate the same amount of memory the list was
		 * previously using. */
		this->items = 0;
	}

	/**
	 * Compact the list down to the smallest block size boundary.
	 */
	void Compact()
	{
		uint capacity = Align(this->items, S);
		if (capacity >= this->capacity) return;

		this->capacity = capacity;
		this->data = ReallocT(this->data, this->capacity);
	}

	/**
	 * Append an item and return it.
	 */
	T *Append()
	{
		if (this->items == this->capacity) {
			this->capacity += S;
			this->data = ReallocT(this->data, this->capacity);
		}

		return &this->data[this->items++];
	}

	/**
	 * Get the number of items in the list.
	 */
	uint Length() const
	{
		return this->items;
	}

	/**
	 * Get the pointer to the first item (const)
	 *
	 * @return the pointer to the first item
	 */
	const T *Begin() const
	{
		return this->data;
	}

	/**
	 * Get the pointer to the first item
	 *
	 * @return the pointer to the first item
	 */
	T *Begin()
	{
		return this->data;
	}

	/**
	 * Get the pointer behind the last valid item (const)
	 *
	 * @return the pointer behind the last valid item
	 */
	const T *End() const
	{
		return &this->data[this->items];
	}

	/**
	 * Get the pointer behind the last valid item
	 *
	 * @return the pointer behind the last valid item
	 */
	T *End()
	{
		return &this->data[this->items];
	}

	/**
	 * Get the pointer to item "number" (const)
	 *
	 * @param index the position of the item
	 * @return the pointer to the item
	 */
	const T *Get(uint index) const
	{
		return &this->data[index];
	}

	/**
	 * Get the pointer to item "number"
	 *
	 * @param index the position of the item
	 * @return the pointer to the item
	 */
	T *Get(uint index)
	{
		return &this->data[index];
	}

	/**
	 * Get item "number" (const)
	 *
	 * @param index the positon of the item
	 * @return the item
	 */
	const T &operator[](uint index) const
	{
		return this->data[index];
	}

	/**
	 * Get item "number"
	 *
	 * @param index the positon of the item
	 * @return the item
	 */
	T &operator[](uint index)
	{
		return this->data[index];
	}
};

#endif /* SMALLVEC_TYPE_HPP */
