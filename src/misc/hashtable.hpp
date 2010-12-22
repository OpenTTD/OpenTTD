/* $Id$ */

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

	FORCEINLINE CHashTableSlotT() : m_pFirst(NULL) {}

	/** hash table slot helper - clears the slot by simple forgetting its items */
	FORCEINLINE void Clear() {m_pFirst = NULL;}

	/** hash table slot helper - linear search for item with given key through the given blob - const version */
	FORCEINLINE const Titem_ *Find(const Key& key) const
	{
		for (const Titem_ *pItem = m_pFirst; pItem != NULL; pItem = pItem->GetHashNext()) {
			if (pItem->GetKey() == key) {
				/* we have found the item, return it */
				return pItem;
			}
		}
		return NULL;
	}

	/** hash table slot helper - linear search for item with given key through the given blob - non-const version */
	FORCEINLINE Titem_ *Find(const Key& key)
	{
		for (Titem_ *pItem = m_pFirst; pItem != NULL; pItem = pItem->GetHashNext()) {
			if (pItem->GetKey() == key) {
				/* we have found the item, return it */
				return pItem;
			}
		}
		return NULL;
	}

	/** hash table slot helper - add new item to the slot */
	FORCEINLINE void Attach(Titem_& new_item)
	{
		assert(new_item.GetHashNext() == NULL);
		new_item.SetHashNext(m_pFirst);
		m_pFirst = &new_item;
	}

	/** hash table slot helper - remove item from a slot */
	FORCEINLINE bool Detach(Titem_& item_to_remove)
	{
		if (m_pFirst == &item_to_remove) {
			m_pFirst = item_to_remove.GetHashNext();
			item_to_remove.SetHashNext(NULL);
			return true;
		}
		Titem_ *pItem = m_pFirst;
		while (true) {
			if (pItem == NULL) {
				return false;
			}
			Titem_ *pNextItem = pItem->GetHashNext();
			if (pNextItem == &item_to_remove) break;
			pItem = pNextItem;
		}
		pItem->SetHashNext(item_to_remove.GetHashNext());
		item_to_remove.SetHashNext(NULL);
		return true;
	}

	/** hash table slot helper - remove and return item from a slot */
	FORCEINLINE Titem_ *Detach(const Key& key)
	{
		/* do we have any items? */
		if (m_pFirst == NULL) {
			return NULL;
		}
		/* is it our first item? */
		if (m_pFirst->GetKey() == key) {
			Titem_& ret_item = *m_pFirst;
			m_pFirst = m_pFirst->GetHashNext();
			ret_item.SetHashNext(NULL);
			return &ret_item;
		}
		/* find it in the following items */
		Titem_ *pPrev = m_pFirst;
		for (Titem_ *pItem = m_pFirst->GetHashNext(); pItem != NULL; pPrev = pItem, pItem = pItem->GetHashNext()) {
			if (pItem->GetKey() == key) {
				/* we have found the item, unlink and return it */
				pPrev->SetHashNext(pItem->GetHashNext());
				pItem->SetHashNext(NULL);
				return pItem;
			}
		}
		return NULL;
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
 *        bool operator == (const Key& other) const;
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

	Slot *m_slots;     // here we store our data (array of blobs)
	int   m_num_items; // item counter

public:
	/* default constructor */
	FORCEINLINE CHashTableT()
	{
		/* construct all slots */
		m_slots = new Slot[Tcapacity];
		m_num_items = 0;
	}

	~CHashTableT() {delete [] m_slots; m_num_items = 0; m_slots = NULL;}

protected:
	/** static helper - return hash for the given key modulo number of slots */
	FORCEINLINE static int CalcHash(const Tkey& key)
	{
		int32 hash = key.CalcHash();
		if ((8 * Thash_bits) < 32) hash ^= hash >> (min(8 * Thash_bits, 31));
		if ((4 * Thash_bits) < 32) hash ^= hash >> (min(4 * Thash_bits, 31));
		if ((2 * Thash_bits) < 32) hash ^= hash >> (min(2 * Thash_bits, 31));
		if ((1 * Thash_bits) < 32) hash ^= hash >> (min(1 * Thash_bits, 31));
		hash &= (1 << Thash_bits) - 1;
		return hash;
	}

	/** static helper - return hash for the given item modulo number of slots */
	FORCEINLINE static int CalcHash(const Titem_& item) {return CalcHash(item.GetKey());}

public:
	/** item count */
	FORCEINLINE int Count() const {return m_num_items;}

	/** simple clear - forget all items - used by CSegmentCostCacheT.Flush() */
	FORCEINLINE void Clear() const {for (int i = 0; i < Tcapacity; i++) m_slots[i].Clear();}

	/** const item search */
	const Titem_ *Find(const Tkey& key) const
	{
		int hash = CalcHash(key);
		const Slot& slot = m_slots[hash];
		const Titem_ *item = slot.Find(key);
		return item;
	}

	/** non-const item search */
	Titem_ *Find(const Tkey& key)
	{
		int hash = CalcHash(key);
		Slot& slot = m_slots[hash];
		Titem_ *item = slot.Find(key);
		return item;
	}

	/** non-const item search & optional removal (if found) */
	Titem_ *TryPop(const Tkey& key)
	{
		int hash = CalcHash(key);
		Slot& slot = m_slots[hash];
		Titem_ *item = slot.Detach(key);
		if (item != NULL) {
			m_num_items--;
		}
		return item;
	}

	/** non-const item search & removal */
	Titem_& Pop(const Tkey& key)
	{
		Titem_ *item = TryPop(key);
		assert(item != NULL);
		return *item;
	}

	/** non-const item search & optional removal (if found) */
	bool TryPop(Titem_& item)
	{
		const Tkey& key = item.GetKey();
		int hash = CalcHash(key);
		Slot& slot = m_slots[hash];
		bool ret = slot.Detach(item);
		if (ret) {
			m_num_items--;
		}
		return ret;
	}

	/** non-const item search & removal */
	void Pop(Titem_& item)
	{
		bool ret = TryPop(item);
		assert(ret);
	}

	/** add one item - copy it from the given item */
	void Push(Titem_& new_item)
	{
		int hash = CalcHash(new_item);
		Slot& slot = m_slots[hash];
		assert(slot.Find(new_item.GetKey()) == NULL);
		slot.Attach(new_item);
		m_num_items++;
	}
};

#endif /* HASHTABLE_HPP */
