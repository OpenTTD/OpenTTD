/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file nodelist.hpp List of nodes used for the A-star pathfinder. */

#ifndef NODELIST_HPP
#define NODELIST_HPP

#include "../../misc/array.hpp"
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
	typedef SmallArray<Titem_, 65536, 256> CItemArray;           ///< Type that we will use as item container.
	typedef CHashTableT<Titem_, Thash_bits_open_  > COpenList;   ///< How pointers to open nodes will be stored.
	typedef CHashTableT<Titem_, Thash_bits_closed_> CClosedList; ///< How pointers to closed nodes will be stored.
	typedef CBinaryHeapT<Titem_> CPriorityQueue;                 ///< How the priority queue will be managed.

protected:
	CItemArray      m_arr;        ///< Here we store full item data (Titem_).
	COpenList       m_open;       ///< Hash table of pointers to open item data.
	CClosedList     m_closed;     ///< Hash table of pointers to closed item data.
	CPriorityQueue  m_open_queue; ///< Priority queue of pointers to open item data.
	Titem          *m_new_node;   ///< New open node under construction.

public:
	/** default constructor */
	CNodeList_HashTableT() : m_open_queue(2048)
	{
		m_new_node = nullptr;
	}

	/** destructor */
	~CNodeList_HashTableT()
	{
	}

	/** return number of open nodes */
	inline int OpenCount()
	{
		return m_open.Count();
	}

	/** return number of closed nodes */
	inline int ClosedCount()
	{
		return m_closed.Count();
	}

	/** allocate new data item from m_arr */
	inline Titem_ *CreateNewNode()
	{
		if (m_new_node == nullptr) m_new_node = m_arr.AppendC();
		return m_new_node;
	}

	/** Notify the nodelist that we don't want to discard the given node. */
	inline void FoundBestNode(Titem_ &item)
	{
		/* for now it is enough to invalidate m_new_node if it is our given node */
		if (&item == m_new_node) {
			m_new_node = nullptr;
		}
		/* TODO: do we need to store best nodes found in some extra list/array? Probably not now. */
	}

	/** insert given item as open node (into m_open and m_open_queue) */
	inline void InsertOpenNode(Titem_ &item)
	{
		assert(m_closed.Find(item.GetKey()) == nullptr);
		m_open.Push(item);
		m_open_queue.Include(&item);
		if (&item == m_new_node) {
			m_new_node = nullptr;
		}
	}

	/** return the best open node */
	inline Titem_ *GetBestOpenNode()
	{
		if (!m_open_queue.IsEmpty()) {
			return m_open_queue.Begin();
		}
		return nullptr;
	}

	/** remove and return the best open node */
	inline Titem_ *PopBestOpenNode()
	{
		if (!m_open_queue.IsEmpty()) {
			Titem_ *item = m_open_queue.Shift();
			m_open.Pop(*item);
			return item;
		}
		return nullptr;
	}

	/** return the open node specified by a key or nullptr if not found */
	inline Titem_ *FindOpenNode(const Key &key)
	{
		Titem_ *item = m_open.Find(key);
		return item;
	}

	/** remove and return the open node specified by a key */
	inline Titem_ &PopOpenNode(const Key &key)
	{
		Titem_ &item = m_open.Pop(key);
		uint idxPop = m_open_queue.FindIndex(item);
		m_open_queue.Remove(idxPop);
		return item;
	}

	/** close node */
	inline void InsertClosedNode(Titem_ &item)
	{
		assert(m_open.Find(item.GetKey()) == nullptr);
		m_closed.Push(item);
	}

	/** return the closed node specified by a key or nullptr if not found */
	inline Titem_ *FindClosedNode(const Key &key)
	{
		Titem_ *item = m_closed.Find(key);
		return item;
	}

	/** The number of items. */
	inline int TotalCount()
	{
		return m_arr.Length();
	}

	/** Get a particular item. */
	inline Titem_ &ItemAt(int idx)
	{
		return m_arr[idx];
	}

	/** Helper for creating output of this array. */
	template <class D> void Dump(D &dmp) const
	{
		dmp.WriteStructT("m_arr", &m_arr);
	}
};

#endif /* NODELIST_HPP */
