/* $Id$ */

/* @file smallvec.h */

#ifndef SMALLVEC_H
#define SMALLVEC_H

template <typename T, uint S> struct SmallVector {
	T *data;
	uint items;
	uint capacity;

	SmallVector() : data(NULL), items(0), capacity(0) { }

	~SmallVector()
	{
		free(data);
	}

	/**
	 * Append an item and return it.
	 */
	T *Append()
	{
		if (items == capacity) {
			capacity += S;
			data = ReallocT(data, capacity);
		}

		return &data[items++];
	}

	const T *Begin() const
	{
		return data;
	}

	T *Begin()
	{
		return data;
	}

	const T *End() const
	{
		return &data[items];
	}

	T *End()
	{
		return &data[items];
	}

	const T *Get(size_t index) const
	{
		return &data[index];
	}

	T *Get(size_t index)
	{
		return &data[index];
	}
};

#endif /* SMALLVEC_H */
