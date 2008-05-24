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
};

#endif /* SMALLVEC_H */
