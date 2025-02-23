/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file lrucache.hpp Size limited cache map with a least recently used eviction strategy. */

#ifndef LRUCACHE_HPP
#define LRUCACHE_HPP

#include <utility>
#include <unordered_map>

/**
 * Size limited cache with a least recently used eviction strategy.
 * @tparam Tkey Type of the cache key.
 * @tparam Tdata Type of the cache item. The cache will store a pointer of this type.
 */
template <class Tkey, class Tdata>
class LRUCache {
private:
	typedef std::pair<Tkey, std::unique_ptr<Tdata>> Tpair;
	typedef typename std::list<Tpair>::iterator Titer;

	std::list<Tpair> data; ///< Ordered list of all items.
	std::unordered_map<Tkey, Titer> lookup;  ///< Map of keys to items.

	const size_t capacity; ///< Number of items to cache.

public:
	/**
	 * Construct new LRU cache map.
	 * @param max_items Number of items to store at most.
	 */
	LRUCache(size_t max_items) : capacity(max_items) {}

	/**
	 * Test if a key is already contained in the cache.
	 * @param key The key to search.
	 * @return True, if the key was found.
	 */
	inline bool Contains(const Tkey key)
	{
		return this->lookup.find(key) != this->lookup.end();
	}

	/**
	 * Insert a new data item with a specified key.
	 * @param key Key under which the item should be stored.
	 * @param item Item to insert.
	 */
	void Insert(const Tkey key, std::unique_ptr<Tdata> &&item)
	{
		if (this->Contains(key)) {
			/* Replace old value. */
			this->lookup[key]->second = std::move(item);
			return;
		}

		/* Delete least used item if maximum items cached. */
		if (this->data.size() >= this->capacity) {
			this->lookup.erase(data.back().first);
			this->data.pop_back();
		}

		/* Insert new item. */
		this->data.emplace_front(key, std::move(item));
		this->lookup.emplace(key, this->data.begin());
	}

	/**
	 * Clear the cache.
	 */
	void Clear()
	{
		this->lookup.clear();
		this->data.clear();
	}

	/**
	 * Get an item from the cache.
	 * @param key The key to look up.
	 * @return The item value.
	 * @note Throws if item not found.
	 */
	inline Tdata *Get(const Tkey key)
	{
		if (this->lookup.find(key) == this->lookup.end()) throw std::out_of_range("item not found");
		/* Move to front if needed. */
		this->data.splice(this->data.begin(), this->data, this->lookup[key]);

		return this->data.front().second.get();
	}
};

#endif /* LRUCACHE_HPP */
