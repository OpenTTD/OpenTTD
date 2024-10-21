/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file hashtable.hpp Hash table support. */

#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include "../core/math_func.hpp"

template <class TItem>
struct HashTableSlot
{
	typedef typename TItem::Key Key; // make Titem::Key a property of HashTable

	TItem *first_item = nullptr;

	/** hash table slot helper - clears the slot by simple forgetting its items */
	inline void Clear()
	{
		this->first_item = nullptr;
	}

	/** hash table slot helper - linear search for item with given key through the given blob - const version */
	inline const TItem *Find(const Key &key) const
	{
		for (const TItem *item = this->first_item; item != nullptr; item = item->GetHashNext()) {
			if (item->GetKey() == key) {
				/* we have found the item, return it */
				return item;
			}
		}
		return nullptr;
	}

	/** hash table slot helper - linear search for item with given key through the given blob - non-const version */
	inline TItem *Find(const Key &key)
	{
		for (TItem *item = this->first_item; item != nullptr; item = item->GetHashNext()) {
			if (item->GetKey() == key) {
				/* we have found the item, return it */
				return item;
			}
		}
		return nullptr;
	}

	/** hash table slot helper - add new item to the slot */
	inline void Attach(TItem &new_item)
	{
		assert(new_item.GetHashNext() == nullptr);
		new_item.SetHashNext(this->first_item);
		this->first_item = &new_item;
	}

	/** hash table slot helper - remove item from a slot */
	inline bool Detach(TItem &item_to_remove)
	{
		if (this->first_item == &item_to_remove) {
			this->first_item = item_to_remove.GetHashNext();
			item_to_remove.SetHashNext(nullptr);
			return true;
		}
		TItem *item = this->first_item;
		for (;;) {
			if (item == nullptr) {
				return false;
			}
			TItem *next_item = item->GetHashNext();
			if (next_item == &item_to_remove) break;
			item = next_item;
		}
		item->SetHashNext(item_to_remove.GetHashNext());
		item_to_remove.SetHashNext(nullptr);
		return true;
	}

	/** hash table slot helper - remove and return item from a slot */
	inline TItem *Detach(const Key &key)
	{
		/* do we have any items? */
		if (this->first_item == nullptr) {
			return nullptr;
		}
		/* is it our first item? */
		if (this->first_item->GetKey() == key) {
			TItem &ret_item = *this->first_item;
			this->first_item = this->first_item->GetHashNext();
			ret_item.SetHashNext(nullptr);
			return &ret_item;
		}
		/* find it in the following items */
		TItem *previous_item = this->first_item;
		for (TItem *item = this->first_item->GetHashNext(); item != nullptr; previous_item = item, item = item->GetHashNext()) {
			if (item->GetKey() == key) {
				/* we have found the item, unlink and return it */
				previous_item->SetHashNext(item->GetHashNext());
				item->SetHashNext(nullptr);
				return item;
			}
		}
		return nullptr;
	}
};

/**
 * class HashTable<Titem, HASH_BITS> - simple hash table
 *  of pointers allocated elsewhere.
 *
 *  Supports: Add/Find/Remove of Titems.
 *
 *  Your Titem must meet some extra requirements to be HashTable
 *  compliant:
 *    - its constructor/destructor (if any) must be public
 *    - if the copying of item requires an extra resource management,
 *        you must define also copy constructor
 *    - must support nested type (struct, class or typedef) Titem::Key
 *        that defines the type of key class for that item
 *    - must support public method:
 *        const Key& GetKey() const; // return the item's key object
 *
 *  In addition, the Titem::Key class must support:
 *    - public method that calculates key's hash:
 *        int CalcHash() const;
 *    - public 'equality' operator to compare the key with another one
 *        bool operator==(const Key &other) const;
 */
template <class Titem, int Thash_bits_>
class HashTable {
public:
	typedef typename Titem::Key Tkey; // make Titem::Key a property of HashTable
	static constexpr int HASH_BITS = Thash_bits_; // publish num of hash bits
	static constexpr int CAPACITY = 1 << HASH_BITS; // and num of slots 2^bits

protected:
	/**
	 * each slot contains pointer to the first item in the list,
	 *  Titem contains pointer to the next item - GetHashNext(), SetHashNext()
	 */
	typedef HashTableSlot<Titem> Slot;

	Slot slots[CAPACITY]; // here we store our data (array of blobs)
	int number_of_items = 0; // item counter

	/** static helper - return hash for the given key modulo number of slots */
	inline static int CalcHash(const Tkey &key)
	{
		uint32_t hash = key.CalcHash();
		hash -= (hash >> 17);          // hash * 131071 / 131072
		hash -= (hash >> 5);           //   * 31 / 32
		hash &= (1 << HASH_BITS) - 1; //   modulo slots
		return hash;
	}

	/** static helper - return hash for the given item modulo number of slots */
	inline static int CalcHash(const Titem &item)
	{
		return CalcHash(item.GetKey());
	}

public:
	/** item count */
	inline int Count() const
	{
		return this->number_of_items;
	}

	/** simple clear - forget all items - used by CSegmentCostCacheT.Flush() */
	inline void Clear()
	{
		for (int i = 0; i < CAPACITY; i++) this->slots[i].Clear();
		this->number_of_items = 0;
	}

	/** const item search */
	const Titem *Find(const Tkey &key) const
	{
		int hash = CalcHash(key);
		const Slot &slot = this->slots[hash];
		const Titem *item = slot.Find(key);
		return item;
	}

	/** non-const item search */
	Titem *Find(const Tkey &key)
	{
		int hash = CalcHash(key);
		Slot &slot = this->slots[hash];
		Titem *item = slot.Find(key);
		return item;
	}

	/** non-const item search & optional removal (if found) */
	Titem *TryPop(const Tkey &key)
	{
		int hash = CalcHash(key);
		Slot &slot = this->slots[hash];
		Titem *item = slot.Detach(key);
		if (item != nullptr) {
			this->number_of_items--;
		}
		return item;
	}

	/** non-const item search & removal */
	Titem &Pop(const Tkey &key)
	{
		Titem *item = TryPop(key);
		assert(item != nullptr);
		return *item;
	}

	/** non-const item search & optional removal (if found) */
	bool TryPop(Titem &item)
	{
		const Tkey &key = item.GetKey();
		int hash = CalcHash(key);
		Slot &slot = this->slots[hash];
		bool ret = slot.Detach(item);
		if (ret) {
			this->number_of_items--;
		}
		return ret;
	}

	/** non-const item search & removal */
	void Pop(Titem &item)
	{
		[[maybe_unused]] bool ret = TryPop(item);
		assert(ret);
	}

	/** add one item - copy it from the given item */
	void Push(Titem &new_item)
	{
		int hash = CalcHash(new_item);
		Slot &slot = this->slots[hash];
		assert(slot.Find(new_item.GetKey()) == nullptr);
		slot.Attach(new_item);
		this->number_of_items++;
	}
};

#endif /* HASHTABLE_HPP */
