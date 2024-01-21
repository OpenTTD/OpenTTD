/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file multimap.h Multimap with deterministic ordering of items with equal keys. */

#ifndef MULTIMAP_HPP
#define MULTIMAP_HPP

template<typename Tkey, typename Tvalue, typename Tcompare>
class MultiMap;

/**
 * STL-style iterator for MultiMap.
 * @tparam Tmap_iter Iterator type for the map in the MultiMap.
 * @tparam Tlist_iter Iterator type for the lists in the MultiMap.
 * @tparam Tkey Key type of the MultiMap.
 * @tparam Tvalue Value type of the MultMap.
 * @tparam Tcompare Comparator type for keys of the MultiMap.
 */
template<class Tmap_iter, class Tlist_iter, class Tkey, class Tvalue, class Tcompare>
class MultiMapIterator {
protected:
	friend class MultiMap<Tkey, Tvalue, Tcompare>;
	typedef MultiMapIterator<Tmap_iter, Tlist_iter, Tkey, Tvalue, Tcompare> Self;

	Tlist_iter list_iter; ///< Iterator pointing to current position in the current list of items with equal keys.
	Tmap_iter map_iter;   ///< Iterator pointing to the position of the current list of items with equal keys in the map.

	/**
	 * Flag to show that the iterator has just "walked" a step in the map.
	 * We cannot check the current list for that as we might have reached end() of the map. In that case we'd need to
	 * set list_iter to some sort of "invalid" state, but that's impossible as operator== yields undefined behaviour
	 * if the iterators don't belong to the same list and there is no list at end(). So if we created a static empty
	 * list and an "invalid" iterator in that we could not determine if the iterator is invalid while it's valid. We
	 * can also not determine if the map iterator is valid while we don't have the map; so in the end it's easiest to
	 * just introduce an extra flag.
	 */
	bool list_valid;

public:
	/**
	 * Simple, dangerous constructor to allow later assignment with operator=.
	 */
	MultiMapIterator() : list_valid(false) {}

	/**
	 * Constructor to allow possibly const iterators to be assigned from possibly
	 * non-const map iterators. You can assign end() like this.
	 * @tparam Tnon_const Iterator type assignable to Tmap_iter (which might be const).
	 * @param mi One such iterator.
	 */
	template<class Tnon_const>
	MultiMapIterator(Tnon_const mi) : map_iter(mi), list_valid(false) {}

	/**
	 * Constructor to allow specifying an exact position in map and list. You cannot
	 * construct end() like this as the constructor will actually check li and mi->second
	 * for list_valid.
	 * @param mi Iterator in the map.
	 * @param li Iterator in the list.
	 */
	MultiMapIterator(Tmap_iter mi, Tlist_iter li) : list_iter(li), map_iter(mi)
	{
		this->list_valid = (this->list_iter != this->map_iter->second.begin());
	}

	/**
	 * Assignment iterator like constructor with the same signature.
	 * @tparam Tnon_const Iterator type assignable to Tmap_iter (which might be const).
	 * @param mi One such iterator.
	 * @return This iterator.
	 */
	template<class Tnon_const>
	Self &operator=(Tnon_const mi)
	{
		this->map_iter = mi;
		this->list_valid = false;
		return *this;
	}

	/**
	 * Dereference operator. Works just like usual STL operator*() on various containers.
	 * Doesn't do a lot of checks for sanity, just like STL.
	 * @return The value associated with the item this iterator points to.
	 */
	Tvalue &operator*() const
	{
		assert(!this->map_iter->second.empty());
		return this->list_valid ?
				this->list_iter.operator*() :
				this->map_iter->second.begin().operator*();
	}

	/**
	 * Same as operator*(), but returns a pointer.
	 * @return Pointer to the value this iterator points to.
	 */
	Tvalue *operator->() const
	{
		assert(!this->map_iter->second.empty());
		return this->list_valid ?
				this->list_iter.operator->() :
				this->map_iter->second.begin().operator->();
	}

	inline const Tmap_iter &GetMapIter() const { return this->map_iter; }
	inline const Tlist_iter &GetListIter() const { return this->list_iter; }
	inline bool ListValid() const { return this->list_valid; }

	const Tkey &GetKey() const { return this->map_iter->first; }

	/**
	 * Prefix increment operator. Increment the iterator and set it to the
	 * next item in the MultiMap. This either increments the list iterator
	 * or the map iterator and sets list_valid accordingly.
	 * @return This iterator after incrementing.
	 */
	Self &operator++()
	{
		assert(!this->map_iter->second.empty());
		if (this->list_valid) {
			if (++this->list_iter == this->map_iter->second.end()) {
				++this->map_iter;
				this->list_valid = false;
			}
		} else {
			this->list_iter = ++(this->map_iter->second.begin());
			if (this->list_iter == this->map_iter->second.end()) {
				++this->map_iter;
			} else {
				this->list_valid = true;
			}
		}
		return *this;
	}

	/**
	 * Postfix increment operator. Same as prefix increment, but return the
	 * previous state.
	 * @param dummy param to mark postfix.
	 * @return This iterator before incrementing.
	 */
	Self operator++(int)
	{
		Self tmp = *this;
		this->operator++();
		return tmp;
	}

	/**
	 * Prefix decrement operator. Decrement the iterator and set it to the
	 * previous item in the MultiMap.
	 * @return This iterator after decrementing.
	 */
	Self &operator--()
	{
		assert(!this->map_iter->second.empty());
		if (!this->list_valid) {
			--this->map_iter;
			this->list_iter = this->map_iter->second.end();
			assert(!this->map_iter->second.empty());
		}

		this->list_valid = (--this->list_iter != this->map_iter->second.begin());
		return *this;
	}

	/**
	 * Postfix decrement operator. Same as prefix decrement, but return the
	 * previous state.
	 * @param dummy param to mark postfix.
	 * @return This iterator before decrementing.
	 */
	Self operator--(int)
	{
		Self tmp = *this;
		this->operator--();
		return tmp;
	}
};

/* Generic comparison functions for const/non-const MultiMap iterators and map iterators */

/**
 * Compare two MultiMap iterators. Iterators are equal if
 * 1. Their map iterators are equal.
 * 2. They agree about list_valid.
 * 3. If list_valid they agree about list_iter.
 * Lots of template parameters to make all possible const and non-const types of MultiMap iterators
 * (on maps with const and non-const values) comparable to each other.
 * @param iter1 First iterator to compare.
 * @param iter2 Second iterator to compare.
 * @return If iter1 and iter2 are equal.
 */
template<class Tmap_iter1, class Tlist_iter1, class Tmap_iter2, class Tlist_iter2, class Tkey, class Tvalue1, class Tvalue2, class Tcompare>
bool operator==(const MultiMapIterator<Tmap_iter1, Tlist_iter1, Tkey, Tvalue1, Tcompare> &iter1, const MultiMapIterator<Tmap_iter2, Tlist_iter2, Tkey, Tvalue2, Tcompare> &iter2)
{
	if (iter1.GetMapIter() != iter2.GetMapIter()) return false;
	if (!iter1.ListValid()) return !iter2.ListValid();
	return iter2.ListValid() ?
			iter1.GetListIter() == iter2.GetListIter() : false;
}

/**
 * Inverse of operator==().
 * Lots of template parameters to make all possible const and non-const types of MultiMap iterators
 * (on maps with const and non-const values) comparable to each other.
 * @param iter1 First iterator to compare.
 * @param iter2 Second iterator to compare.
 * @return If iter1 and iter2 are not equal.
 */
template<class Tmap_iter1, class Tlist_iter1, class Tmap_iter2, class Tlist_iter2, class Tkey, class Tvalue1, class Tvalue2, class Tcompare>
bool operator!=(const MultiMapIterator<Tmap_iter1, Tlist_iter1, Tkey, Tvalue1, Tcompare> &iter1, const MultiMapIterator<Tmap_iter2, Tlist_iter2, Tkey, Tvalue2, Tcompare> &iter2)
{
	return !(iter1 == iter2);
}

/**
 * Check if a MultiMap iterator is at the begin of a list pointed to by the given map iterator.
 * Lots of template parameters to make all possible const and non-const types of MultiMap iterators
 * (on maps with const and non-const values) comparable to all possible types of map iterators.
 * @param iter1 MultiMap iterator.
 * @param iter2 Map iterator.
 * @return If iter1 points to the begin of the list pointed to by iter2.
 */
template<class Tmap_iter1, class Tlist_iter1, class Tmap_iter2, class Tkey, class Tvalue, class Tcompare >
bool operator==(const MultiMapIterator<Tmap_iter1, Tlist_iter1, Tkey, Tvalue, Tcompare> &iter1, const Tmap_iter2 &iter2)
{
	return !iter1.ListValid() && iter1.GetMapIter() == iter2;
}

/**
 * Inverse of operator==() with same signature.
 * @param iter1 MultiMap iterator.
 * @param iter2 Map iterator.
 * @return If iter1 doesn't point to the begin of the list pointed to by iter2.
 */
template<class Tmap_iter1, class Tlist_iter1, class Tmap_iter2, class Tkey, class Tvalue, class Tcompare >
bool operator!=(const MultiMapIterator<Tmap_iter1, Tlist_iter1, Tkey, Tvalue, Tcompare> &iter1, const Tmap_iter2 &iter2)
{
	return iter1.ListValid() || iter1.GetMapIter() != iter2;
}

/**
 * Same as operator==() with reversed order of arguments.
 * @param iter2 Map iterator.
 * @param iter1 MultiMap iterator.
 * @return If iter1 points to the begin of the list pointed to by iter2.
 */
template<class Tmap_iter1, class Tlist_iter1, class Tmap_iter2, class Tkey, class Tvalue, class Tcompare >
bool operator==(const Tmap_iter2 &iter2, const MultiMapIterator<Tmap_iter1, Tlist_iter1, Tkey, Tvalue, Tcompare> &iter1)
{
	return !iter1.ListValid() && iter1.GetMapIter() == iter2;
}

/**
 * Same as operator!=() with reversed order of arguments.
 * @param iter2 Map iterator.
 * @param iter1 MultiMap iterator.
 * @return If iter1 doesn't point to the begin of the list pointed to by iter2.
 */
template<class Tmap_iter1, class Tlist_iter1, class Tmap_iter2, class Tkey, class Tvalue, class Tcompare >
bool operator!=(const Tmap_iter2 &iter2, const MultiMapIterator<Tmap_iter1, Tlist_iter1, Tkey, Tvalue, Tcompare> &iter1)
{
	return iter1.ListValid() || iter1.GetMapIter() != iter2;
}


/**
 * Hand-rolled multimap as map of lists. Behaves mostly like a list, but is sorted
 * by Tkey so that you can easily look up ranges of equal keys. Those ranges are
 * internally ordered in a deterministic way (contrary to STL multimap). All
 * STL-compatible members are named in STL style, all others are named in OpenTTD
 * style.
 */
template<typename Tkey, typename Tvalue, typename Tcompare = std::less<Tkey> >
class MultiMap : public std::map<Tkey, std::list<Tvalue>, Tcompare > {
public:
	typedef typename std::list<Tvalue> List;
	typedef typename List::iterator ListIterator;
	typedef typename List::const_iterator ConstListIterator;

	typedef typename std::map<Tkey, List, Tcompare > Map;
	typedef typename Map::iterator MapIterator;
	typedef typename Map::const_iterator ConstMapIterator;

	typedef MultiMapIterator<MapIterator, ListIterator, Tkey, Tvalue, Tcompare> iterator;
	typedef MultiMapIterator<ConstMapIterator, ConstListIterator, Tkey, const Tvalue, Tcompare> const_iterator;

	/**
	 * Erase the value pointed to by an iterator. The iterator may be invalid afterwards.
	 * @param it Iterator pointing at some value.
	 * @return Iterator to the element after the deleted one (or invalid).
	 */
	iterator erase(iterator it)
	{
		List &list = it.map_iter->second;
		assert(!list.empty());
		if (it.list_valid) {
			it.list_iter = list.erase(it.list_iter);
			/* This can't be the first list element as otherwise list_valid would have
			 * to be false. So the list cannot be empty here. */
			if (it.list_iter == list.end()) {
				++it.map_iter;
				it.list_valid = false;
			}
		} else {
			list.erase(list.begin());
			if (list.empty()) this->Map::erase(it.map_iter++);
		}
		return it;
	}

	/**
	 * Insert a value at the end of the range with the specified key.
	 * @param key Key to be inserted at.
	 * @param val Value to be inserted.
	 */
	void Insert(const Tkey &key, const Tvalue &val)
	{
		List &list = (*this)[key];
		list.push_back(val);
		assert(!list.empty());
	}

	/**
	 * Count all items in this MultiMap. This involves iterating over the map.
	 * @return Number of items in the MultiMap.
	 */
	size_t size() const
	{
		size_t ret = 0;
		for (ConstMapIterator it = this->Map::begin(); it != this->Map::end(); ++it) {
			ret += it->second.size();
		}
		return ret;
	}

	/**
	 * Count the number of ranges with equal keys in this MultiMap.
	 * @return Number of ranges with equal keys.
	 */
	size_t MapSize() const
	{
		return this->Map::size();
	}

	/**
	 * Get a pair of iterators specifying a range of items with equal keys.
	 * @param key Key to look for.
	 * @return Range of items with given key.
	 */
	std::pair<iterator, iterator> equal_range(const Tkey &key)
	{
		MapIterator begin(this->lower_bound(key));
		if (begin != this->Map::end() && begin->first == key) {
			MapIterator end = begin;
			return std::make_pair(begin, ++end);
		}
		return std::make_pair(begin, begin);
	}

	/**
	 * Get a pair of constant iterators specifying a range of items with equal keys.
	 * @param key Key to look for.
	 * @return Constant range of items with given key.
	 */
	std::pair<const_iterator, const_iterator> equal_range(const Tkey &key) const
	{
		ConstMapIterator begin(this->lower_bound(key));
		if (begin != this->Map::end() && begin->first == key) {
			ConstMapIterator end = begin;
			return std::make_pair(begin, ++end);
		}
		return std::make_pair(begin, begin);
	}
};

#endif /* MULTIMAP_HPP */
