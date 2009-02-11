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
	void Reset()
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
	 */
	FORCEINLINE T *Append()
	{
		if (this->items == this->capacity) {
			this->capacity += S;
			this->data = ReallocT(this->data, this->capacity);
		}

		return &this->data[this->items++];
	}

	/**
	 * Search for the first occurence of an item.
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
	 * Search for the first occurence of an item.
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
	 * Tests whether a item is present in the vector.
	 * The '!=' operator of T is used for comparison.
	 * @param item Item to test for
	 * @return true iff the item is present
	 */
	FORCEINLINE bool Contains(const T &item) const
	{
		return this->Find(item) != this->End();
	}

	/** Removes given item from this map
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
		return &this->data[index];
	}

	/**
	 * Get item "number" (const)
	 *
	 * @param index the positon of the item
	 * @return the item
	 */
	FORCEINLINE const T &operator[](uint index) const
	{
		return this->data[index];
	}

	/**
	 * Get item "number"
	 *
	 * @param index the positon of the item
	 * @return the item
	 */
	FORCEINLINE T &operator[](uint index)
	{
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

#endif /* SMALLVEC_TYPE_HPP */
