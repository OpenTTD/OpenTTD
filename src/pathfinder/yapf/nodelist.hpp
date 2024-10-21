/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file nodelist.hpp List of nodes used for the A-star pathfinder. */

#ifndef NODELIST_HPP
#define NODELIST_HPP

#include "../../misc/hashtable.hpp"
#include "../../misc/binaryheap.hpp"

/**
 * Hash table based node list multi-container class.
 *  Implements open list, closed list and priority queue for A-star
 *  path finder.
 */
template <class Titem_, int Thash_bits_open_, int Thash_bits_closed_>
class CNodeList_HashTableT {
public:
	typedef Titem_ Titem;                                        ///< Make #Titem_ visible from outside of class.
	typedef typename Titem_::Key Key;                            ///< Make Titem_::Key a property of this class.
	using CItemArray = std::deque<Titem_>;                       ///< Type that we will use as item container.
	typedef HashTable<Titem_, Thash_bits_open_  > COpenList;     ///< How pointers to open nodes will be stored.
	typedef HashTable<Titem_, Thash_bits_closed_> CClosedList;   ///< How pointers to closed nodes will be stored.
	typedef CBinaryHeapT<Titem_> CPriorityQueue;                 ///< How the priority queue will be managed.

protected:
	CItemArray items; ///< Here we store full item data (Titem_).
	COpenList open_nodes; ///< Hash table of pointers to open item data.
	CClosedList closed_nodes; ///< Hash table of pointers to closed item data.
	CPriorityQueue open_queue; ///< Priority queue of pointers to open item data.
	Titem *new_node; ///< New open node under construction.

public:
	/** default constructor */
	CNodeList_HashTableT() : open_queue(2048)
	{
		this->new_node = nullptr;
	}

	/** destructor */
	~CNodeList_HashTableT()
	{
	}

	/** return number of open nodes */
	inline int OpenCount()
	{
		return this->open_nodes.Count();
	}

	/** return number of closed nodes */
	inline int ClosedCount()
	{
		return this->closed_nodes.Count();
	}

	/** allocate new data item from items */
	inline Titem_ *CreateNewNode()
	{
		if (this->new_node == nullptr) this->new_node = &this->items.emplace_back();
		return this->new_node;
	}

	/** Notify the nodelist that we don't want to discard the given node. */
	inline void FoundBestNode(Titem_ &item)
	{
		/* for now it is enough to invalidate m_new_node if it is our given node */
		if (&item == this->new_node) {
			this->new_node = nullptr;
		}
		/* TODO: do we need to store best nodes found in some extra list/array? Probably not now. */
	}

	/** insert given item as open node (into m_open and m_open_queue) */
	inline void InsertOpenNode(Titem_ &item)
	{
		assert(this->closed_nodes.Find(item.GetKey()) == nullptr);
		this->open_nodes.Push(item);
		this->open_queue.Include(&item);
		if (&item == this->new_node) {
			this->new_node = nullptr;
		}
	}

	/** return the best open node */
	inline Titem_ *GetBestOpenNode()
	{
		if (!this->open_queue.IsEmpty()) {
			return this->open_queue.Begin();
		}
		return nullptr;
	}

	/** remove and return the best open node */
	inline Titem_ *PopBestOpenNode()
	{
		if (!this->open_queue.IsEmpty()) {
			Titem_ *item = this->open_queue.Shift();
			this->open_nodes.Pop(*item);
			return item;
		}
		return nullptr;
	}

	/** return the open node specified by a key or nullptr if not found */
	inline Titem_ *FindOpenNode(const Key &key)
	{
		Titem_ *item = this->open_nodes.Find(key);
		return item;
	}

	/** remove and return the open node specified by a key */
	inline Titem_ &PopOpenNode(const Key &key)
	{
		Titem_ &item = this->open_nodes.Pop(key);
		size_t idxPop = this->open_queue.FindIndex(item);
		this->open_queue.Remove(idxPop);
		return item;
	}

	/** close node */
	inline void InsertClosedNode(Titem_ &item)
	{
		assert(this->open_nodes.Find(item.GetKey()) == nullptr);
		this->closed_nodes.Push(item);
	}

	/** return the closed node specified by a key or nullptr if not found */
	inline Titem_ *FindClosedNode(const Key &key)
	{
		Titem_ *item = this->closed_nodes.Find(key);
		return item;
	}

	/** The number of items. */
	inline int TotalCount()
	{
		return this->items.Length();
	}

	/** Get a particular item. */
	inline Titem_ &ItemAt(int idx)
	{
		return this->items[idx];
	}

	/** Helper for creating output of this array. */
	template <class D> void Dump(D &dmp) const
	{
		dmp.WriteStructT("data", &this->items);
	}
};

#endif /* NODELIST_HPP */
