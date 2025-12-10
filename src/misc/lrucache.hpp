/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file lrucache.hpp Size limited cache map with a least recently used eviction strategy. */

#ifndef LRUCACHE_HPP
#define LRUCACHE_HPP

#include <utility>
#include <unordered_map>

/**
 * Size limited cache with a least recently used eviction strategy.
 * @note If a comparator is provided, the lookup type is a std::map, otherwise std::unordered_map is used.
 * @tparam Tkey Type of the cache key.
 * @tparam Tdata Type of the cache item.
 */
template <class Tkey, class Tdata, class Thash = std::hash<Tkey>, class Tequality = std::equal_to<>>
class LRUCache {
private:
	using PairType = std::pair<Tkey, Tdata>;
	using StorageType = std::list<PairType>;
	using IteratorType = StorageType::iterator;
	using LookupType = std::unordered_map<Tkey, IteratorType, Thash, Tequality>;

	StorageType data; ///< Ordered list of all items.
	LookupType lookup; ///< Map of keys to items.

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
	inline bool Contains(const Tkey &key)
	{
		return this->lookup.find(key) != this->lookup.end();
	}

	/**
	 * Insert a new data item with a specified key.
	 * @param key Key under which the item should be stored.
	 * @param item Item to insert.
	 */
	void Insert(const Tkey &key, Tdata &&item)
	{
		auto it = this->lookup.find(key);
		if (it != this->lookup.end()) {
			/* Replace old value. */
			it->second->second = std::move(item);
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
	inline const Tdata &Get(const Tkey &key)
	{
		auto it = this->lookup.find(key);
		if (it == this->lookup.end()) throw std::out_of_range("item not found");
		/* Move to front if needed. */
		this->data.splice(this->data.begin(), this->data, it->second);

		return this->data.front().second;
	}

	/**
	 * Get an item from the cache.
	 * @param key The key to look up.
	 * @return The item value.
	 * @note Throws if item not found.
	 */
	inline Tdata *GetIfValid(const auto &key)
	{
		auto it = this->lookup.find(key);
		if (it == this->lookup.end()) return nullptr;

		/* Move to front if needed. */
		this->data.splice(this->data.begin(), this->data, it->second);

		return &this->data.front().second;
	}
};

#endif /* LRUCACHE_HPP */
