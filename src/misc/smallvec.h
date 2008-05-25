/* $Id$ */

/** @file smallvec.h Simple vector class that allows allocating an item without the need to copy this->data needlessly. */

#ifndef SMALLVEC_H
#define SMALLVEC_H

template <typename T, uint S>
struct SmallVector {
	T *data;
	uint items;
	uint capacity;

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

	const T *Begin() const
	{
		return this->data;
	}

	T *Begin()
	{
		return this->data;
	}

	const T *End() const
	{
		return &this->data[this->items];
	}

	T *End()
	{
		return &this->data[this->items];
	}

	const T *Get(uint index) const
	{
		return &this->data[index];
	}

	T *Get(uint index)
	{
		return &this->data[index];
	}

	const T &operator[](uint index) const
	{
		return this->data[index];
	}

	T &operator[](uint index)
	{
		return this->data[index];
	}
};

#endif /* SMALLVEC_H */
