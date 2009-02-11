/* $Id$ */

/** @file smallmap_type.hpp Simple mapping class targeted for small sets of data. Stored data shall be POD ("Plain Old Data")! */

#ifndef SMALLMAP_TYPE_HPP
#define SMALLMAP_TYPE_HPP

#include "smallvec_type.hpp"

/** Simple pair of data. Both types have to be POD ("Plain Old Data")! */
template <typename T, typename U>
struct SmallPair {
	T first;
	U second;

	/** Initializes this Pair with data */
	FORCEINLINE SmallPair(const T &first, const U &second) : first(first), second(second) { }
};

/** Implementation of simple mapping class. Both types have to be POD ("Plain Old Data")!
 * It has inherited accessors from SmallVector().
 * @see SmallVector
 */
template <typename T, typename U, uint S = 16>
struct SmallMap : SmallVector<SmallPair<T, U>, S> {
	typedef ::SmallPair<T, U> Pair;
	typedef Pair *iterator;

	/** Creates new SmallMap. Data are initialized in SmallVector constructor */
	FORCEINLINE SmallMap() { }
	/** Data are freed in SmallVector destructor */
	FORCEINLINE ~SmallMap() { }

	/** Finds given key in this map
	 * @param key key to find
	 * @return &Pair(key, data) if found, this->End() if not
	 */
	FORCEINLINE Pair *Find(const T &key)
	{
		for (uint i = 0; i < this->items; i++) {
			if (key == this->data[i].first) return &this->data[i];
		}
		return this->End();
	}

	/** Removes given pair from this map
	 * @param pair pair to remove
	 * @note it has to be pointer to pair in this map. It is overwritten by the last item.
	 */
	FORCEINLINE void Erase(Pair *pair)
	{
		assert(pair >= this->Begin() && pair < this->End());
		*pair = this->data[--this->items];
	}

	/** Removes given key from this map
	 * @param key key to remove
	 * @return true iff the key was found
	 * @note last item is moved to its place, so don't increase your iterator if true is returned!
	 */
	FORCEINLINE bool Erase(const T &key)
	{
		for (uint i = 0; i < this->items; i++) {
			if (key == this->data[i].first) {
				this->data[i] = this->data[--this->items];
				return true;
			}
		}
		return false;
	}

	/** Adds new item to this map.
	 * @param key key
	 * @param data data
	 * @return true iff the key wasn't already present
	 */
	FORCEINLINE bool Insert(const T &key, const U &data)
	{
		if (this->Find(key) != this->End()) return false;
		new (this->Append()) Pair(key, data);
		return true;
	}

	/** Returns data belonging to this key
	 * @param key key
	 * @return data belonging to this key
	 * @note if this key wasn't present, new entry is created
	 */
	FORCEINLINE U &operator[](const T &key)
	{
		for (uint i = 0; i < this->items; i++) {
			if (key == this->data[i].first) return this->data[i].second;
		}
		Pair *n = this->Append();
		n->first = key;
		return n->second;
	}

	FORCEINLINE void SortByKey()
	{
		qsort(this->Begin(), this->items, sizeof(Pair), KeySorter);
	}

	static int CDECL KeySorter(const void *a, const void *b)
	{
		const Pair *pa = (const Pair*)a;
		const Pair *pb = (const Pair*)b;
		return pa->first - pb->first;
	}
};

#endif /* SMALLMAP_TYPE_HPP */
