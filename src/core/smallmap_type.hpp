/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallmap_type.hpp Simple mapping class targeted for small sets of data. Stored data shall be POD ("Plain Old Data")! */

#ifndef SMALLMAP_TYPE_HPP
#define SMALLMAP_TYPE_HPP

#include "smallvec_type.hpp"
#include <utility>

/**
 * Implementation of simple mapping class.
 * It has inherited accessors from std::vector().
 * @tparam T Key type.
 * @tparam U Value type.
 * @tparam S Unit of allocation.
 *
 * @see std::vector
 */
template <typename T, typename U>
struct SmallMap : std::vector<std::pair<T, U> > {
	typedef std::pair<T, U> Pair;
	typedef Pair *iterator;
	typedef const Pair *const_iterator;

	/** Creates new SmallMap. Data are initialized in std::vector constructor */
	inline SmallMap() { }
	/** Data are freed in std::vector destructor */
	inline ~SmallMap() { }

	/**
	 * Finds given key in this map
	 * @param key key to find
	 * @return &Pair(key, data) if found, this->End() if not
	 */
	inline typename std::vector<Pair>::const_iterator Find(const T &key) const
	{
		typename std::vector<Pair>::const_iterator it;
		for (it = std::vector<Pair>::begin(); it != std::vector<Pair>::end(); it++) {
			if (key == it->first) return it;
		}
		return it;
	}

	/**
	 * Finds given key in this map
	 * @param key key to find
	 * @return &Pair(key, data) if found, this->End() if not
	 */
	inline Pair *Find(const T &key)
	{
		for (uint i = 0; i < std::vector<Pair>::size(); i++) {
			if (key == std::vector<Pair>::operator[](i).first) return &std::vector<Pair>::operator[](i);
		}
		return this->End();
	}

	inline const Pair *End() const
	{
		return std::vector<Pair>::data() + std::vector<Pair>::size();
	}

	inline Pair *End()
	{
		return std::vector<Pair>::data() + std::vector<Pair>::size();
	}


	/**
	 * Tests whether a key is assigned in this map.
	 * @param key key to test
	 * @return true iff the item is present
	 */
	inline bool Contains(const T &key) const
	{
		return this->Find(key) != std::vector<Pair>::end();
	}

	/**
	 * Tests whether a key is assigned in this map.
	 * @param key key to test
	 * @return true iff the item is present
	 */
	inline bool Contains(const T &key)
	{
		return this->Find(key) != this->End();
	}

	/**
	 * Removes given pair from this map
	 * @param pair pair to remove
	 * @note it has to be pointer to pair in this map. It is overwritten by the last item.
	 */
	inline void Erase(Pair *pair)
	{
		assert(pair >= std::vector<Pair>::data() && pair < this->End());
		auto distance = pair - std::vector<Pair>::data();
		std::vector<Pair>::erase(std::vector<Pair>::begin() + distance);
	}

	/**
	 * Removes given key from this map
	 * @param key key to remove
	 * @return true iff the key was found
	 * @note last item is moved to its place, so don't increase your iterator if true is returned!
	 */
	inline bool Erase(const T &key)
	{
		auto *pair = this->Find(key);
		if (pair == this->End()) return false;

		this->Erase(pair);
		return true;
	}

	/**
	 * Adds new item to this map.
	 * @param key key
	 * @param data data
	 * @return true iff the key wasn't already present
	 */
	inline bool Insert(const T &key, const U &data)
	{
		if (this->Contains(key)) return false;
		std::vector<Pair>::emplace_back(key, data);
		return true;
	}

	/**
	 * Returns data belonging to this key
	 * @param key key
	 * @return data belonging to this key
	 * @note if this key wasn't present, new entry is created
	 */
	inline U &operator[](const T &key)
	{
		for (uint i = 0; i < std::vector<Pair>::size(); i++) {
			if (key == std::vector<Pair>::operator[](i).first) return std::vector<Pair>::operator[](i).second;
		}
		/*C++17: Pair &n = */ std::vector<Pair>::emplace_back();
		Pair &n = std::vector<Pair>::back();
		n.first = key;
		return n.second;
	}
};

#endif /* SMALLMAP_TYPE_HPP */
