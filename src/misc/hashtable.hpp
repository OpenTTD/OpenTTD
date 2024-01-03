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

template <class Titem_>
struct CHashTableSlotT
{
	typedef typename Titem_::Key Key;          // make Titem_::Key a property of HashTable

	Titem_ *m_pFirst;

	inline CHashTableSlotT() : m_pFirst(nullptr) {}

	/** hash table slot helper - clears the slot by simple forgetting its items */
	inline void Clear()
	{
		m_pFirst = nullptr;
	}

	/** hash table slot helper - linear search for item with given key through the given blob - const version */
	inline const Titem_ *Find(const Key &key) const
	{
		for (const Titem_ *pItem = m_pFirst; pItem != nullptr; pItem = pItem->GetHashNext()) {
			if (pItem->GetKey() == key) {
				/* we have found the item, return it */
				return pItem;
			}
		}
		return nullptr;
	}

	/** hash table slot helper - linear search for item with given key through the given blob - non-const version */
	inline Titem_ *Find(const Key &key)
	{
		for (Titem_ *pItem = m_pFirst; pItem != nullptr; pItem = pItem->GetHashNext()) {
			if (pItem->GetKey() == key) {
				/* we have found the item, return it */
				return pItem;
			}
		}
		return nullptr;
	}

	/** hash table slot helper - add new item to the slot */
	inline void Attach(Titem_ &new_item)
	{
		assert(new_item.GetHashNext() == nullptr);
		new_item.SetHashNext(m_pFirst);
		m_pFirst = &new_item;
	}

	/** hash table slot helper - remove item from a slot */
	inline bool Detach(Titem_ &item_to_remove)
	{
		if (m_pFirst == &item_to_remove) {
			m_pFirst = item_to_remove.GetHashNext();
			item_to_remove.SetHashNext(nullptr);
			return true;
		}
		Titem_ *pItem = m_pFirst;
		for (;;) {
			if (pItem == nullptr) {
				return false;
			}
			Titem_ *pNextItem = pItem->GetHashNext();
			if (pNextItem == &item_to_remove) break;
			pItem = pNextItem;
		}
		pItem->SetHashNext(item_to_remove.GetHashNext());
		item_to_remove.SetHashNext(nullptr);
		return true;
	}

	/** hash table slot helper - remove and return item from a slot */
	inline Titem_ *Detach(const Key &key)
	{
		/* do we have any items? */
		if (m_pFirst == nullptr) {
			return nullptr;
		}
		/* is it our first item? */
		if (m_pFirst->GetKey() == key) {
			Titem_ &ret_item = *m_pFirst;
			m_pFirst = m_pFirst->GetHashNext();
			ret_item.SetHashNext(nullptr);
			return &ret_item;
		}
		/* find it in the following items */
		Titem_ *pPrev = m_pFirst;
		for (Titem_ *pItem = m_pFirst->GetHashNext(); pItem != nullptr; pPrev = pItem, pItem = pItem->GetHashNext()) {
			if (pItem->GetKey() == key) {
				/* we have found the item, unlink and return it */
				pPrev->SetHashNext(pItem->GetHashNext());
				pItem->SetHashNext(nullptr);
				return pItem;
			}
		}
		return nullptr;
	}
};

/**
 * class CHashTableT<Titem, Thash_bits> - simple hash table
 *  of pointers allocated elsewhere.
 *
 *  Supports: Add/Find/Remove of Titems.
 *
 *  Your Titem must meet some extra requirements to be CHashTableT
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
template <class Titem_, int Thash_bits_>
class CHashTableT {
public:
	typedef Titem_ Titem;                         // make Titem_ visible from outside of class
	typedef typename Titem_::Key Tkey;            // make Titem_::Key a property of HashTable
	static const int Thash_bits = Thash_bits_;    // publish num of hash bits
	static const int Tcapacity = 1 << Thash_bits; // and num of slots 2^bits

protected:
	/**
	 * each slot contains pointer to the first item in the list,
	 *  Titem contains pointer to the next item - GetHashNext(), SetHashNext()
	 */
	typedef CHashTableSlotT<Titem_> Slot;

	Slot  m_slots[Tcapacity]; // here we store our data (array of blobs)
	int   m_num_items;        // item counter

public:
	/* default constructor */
	inline CHashTableT() : m_num_items(0)
	{
	}

protected:
	/** static helper - return hash for the given key modulo number of slots */
	inline static int CalcHash(const Tkey &key)
	{
		uint32_t hash = key.CalcHash();
		hash -= (hash >> 17);          // hash * 131071 / 131072
		hash -= (hash >> 5);           //   * 31 / 32
		hash &= (1 << Thash_bits) - 1; //   modulo slots
		return hash;
	}

	/** static helper - return hash for the given item modulo number of slots */
	inline static int CalcHash(const Titem_ &item)
	{
		return CalcHash(item.GetKey());
	}

public:
	/** item count */
	inline int Count() const
	{
		return m_num_items;
	}

	/** simple clear - forget all items - used by CSegmentCostCacheT.Flush() */
	inline void Clear()
	{
		for (int i = 0; i < Tcapacity; i++) m_slots[i].Clear();
	}

	/** const item search */
	const Titem_ *Find(const Tkey &key) const
	{
		int hash = CalcHash(key);
		const Slot &slot = m_slots[hash];
		const Titem_ *item = slot.Find(key);
		return item;
	}

	/** non-const item search */
	Titem_ *Find(const Tkey &key)
	{
		int hash = CalcHash(key);
		Slot &slot = m_slots[hash];
		Titem_ *item = slot.Find(key);
		return item;
	}

	/** non-const item search & optional removal (if found) */
	Titem_ *TryPop(const Tkey &key)
	{
		int hash = CalcHash(key);
		Slot &slot = m_slots[hash];
		Titem_ *item = slot.Detach(key);
		if (item != nullptr) {
			m_num_items--;
		}
		return item;
	}

	/** non-const item search & removal */
	Titem_& Pop(const Tkey &key)
	{
		Titem_ *item = TryPop(key);
		assert(item != nullptr);
		return *item;
	}

	/** non-const item search & optional removal (if found) */
	bool TryPop(Titem_ &item)
	{
		const Tkey &key = item.GetKey();
		int hash = CalcHash(key);
		Slot &slot = m_slots[hash];
		bool ret = slot.Detach(item);
		if (ret) {
			m_num_items--;
		}
		return ret;
	}

	/** non-const item search & removal */
	void Pop(Titem_ &item)
	{
		[[maybe_unused]] bool ret = TryPop(item);
		assert(ret);
	}

	/** add one item - copy it from the given item */
	void Push(Titem_ &new_item)
	{
		int hash = CalcHash(new_item);
		Slot &slot = m_slots[hash];
		assert(slot.Find(new_item.GetKey()) == nullptr);
		slot.Attach(new_item);
		m_num_items++;
	}
};

#endif /* HASHTABLE_HPP */
