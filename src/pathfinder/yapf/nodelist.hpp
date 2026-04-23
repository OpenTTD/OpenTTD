/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file nodelist.hpp List of nodes used for the A-star pathfinder. */

#ifndef NODELIST_HPP
#define NODELIST_HPP

#include "../../misc/hashtable.hpp"
#include "../../misc/binaryheap.hpp"
#include "../../misc/dbg_helpers.h"

/**
 * Hash table based node list multi-container class.
 *  Implements open list, closed list and priority queue for A-star pathfinder.
 */
template <class Titem, int Thash_bits_open, int Thash_bits_closed>
class NodeList {
public:
	using Item = Titem;
	using Key = typename Titem::Key;

protected:
	std::deque<Titem> items; ///< Storage of the nodes.
	HashTable<Titem, Thash_bits_open> open_nodes; ///< Hash table of pointers to open nodes.
	HashTable<Titem, Thash_bits_closed> closed_nodes; ///< Hash table of pointers to closed nodes.
	CBinaryHeapT<Titem> open_queue; ///< Priority queue of pointers to open nodes.
	Titem *new_node; ///< New node under construction.

public:
	/** default constructor */
	NodeList() : open_queue(2048)
	{
		this->new_node = nullptr;
	}

	/**
	 * Get open node count.
	 * @return Number of open nodes.
	 */
	inline int OpenCount()
	{
		return this->open_nodes.Count();
	}

	/**
	 * Get closed node count.
	 * @return Number of closed nodes.
	 */
	inline int ClosedCount()
	{
		return this->closed_nodes.Count();
	}

	/**
	 * Get the total node count.
	 * @return The total number of nodes.
	 */
	inline int TotalCount()
	{
		return this->items.Length();
	}

	/**
	 * Allocate new data item from items.
	 * @return The allocated node.
	 */
	inline Titem &CreateNewNode()
	{
		if (this->new_node == nullptr) this->new_node = &this->items.emplace_back();
		return *this->new_node;
	}

	/**
	 * Notify the nodelist that we don't want to discard the given node.
	 * @param item The new best node.
	 */
	inline void FoundBestNode(Titem &item)
	{
		/* for now it is enough to invalidate new_node if it is our given node */
		if (&item == this->new_node) {
			this->new_node = nullptr;
		}
		/* TODO: do we need to store best nodes found in some extra list/array? Probably not now. */
	}

	/**
	 * Insert given item as open node (into open_nodes and open_queue).
	 * @param item The node to add.
	 */
	inline void InsertOpenNode(Titem &item)
	{
		assert(this->closed_nodes.Find(item.GetKey()) == nullptr);
		this->open_nodes.Push(item);
		this->open_queue.Include(&item);
		if (&item == this->new_node) {
			this->new_node = nullptr;
		}
	}

	/**
	 * Get the open node at the begin of the open queue.
	 * @return The best open node, or \c nullptr when there isn't any.
	 */
	inline Titem *GetBestOpenNode()
	{
		if (!this->open_queue.IsEmpty()) {
			return this->open_queue.Begin();
		}
		return nullptr;
	}

	/**
	 * Remove and return the best open node.
	 * @return The best open node, or \c nullptr when there isn't any.
	 */
	inline Titem *PopBestOpenNode()
	{
		if (!this->open_queue.IsEmpty()) {
			Titem *item = this->open_queue.Shift();
			this->open_nodes.Pop(*item);
			return item;
		}
		return nullptr;
	}

	/**
	 * Find an open node by key.
	 * @param key The key to look for.
	 * @return The open node specified by a key or \c nullptr if not found.
	 */
	inline Titem *FindOpenNode(const Key &key)
	{
		return this->open_nodes.Find(key);
	}

	/**
	 * Find and remove an open node by key.
	 * @param key The key to look for.
	 * @return The open node specified by a key.
	 */
	inline Titem &PopOpenNode(const Key &key)
	{
		Titem &item = this->open_nodes.Pop(key);
		size_t index = this->open_queue.FindIndex(item);
		this->open_queue.Remove(index);
		return item;
	}

	/**
	 * Insert the given item into the closed nodes set.
	 * @param item The item to add.
	 */
	inline void InsertClosedNode(Titem &item)
	{
		assert(this->open_nodes.Find(item.GetKey()) == nullptr);
		this->closed_nodes.Push(item);
	}

	/**
	 * Find a closed node by its key.
	 * @param key The key to look for.
	 * @return The closed node specified by a key or \c nullptr if not found.
	 */
	inline Titem *FindClosedNode(const Key &key)
	{
		return this->closed_nodes.Find(key);
	}

	/**
	 * Get a particular item.
	 * @param index The index of the item.
	 * @return The item.
	 */
	inline Titem &ItemAt(int index)
	{
		return this->items[index];
	}

	/**
	 * Helper for creating output of this array.
	 * @param dmp The data to dump.
	 */
	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("data", &this->items);
	}
};

#endif /* NODELIST_HPP */
