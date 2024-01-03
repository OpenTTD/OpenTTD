/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file queue.h Binary heap implementation, hash implementation. */

#ifndef QUEUE_H
#define QUEUE_H

#include "../../tile_type.h"
#include "../../track_type.h"

//#define HASH_STATS


struct BinaryHeapNode {
	void *item;
	int priority;
};


/**
 * Binary Heap.
 * For information, see: http://www.policyalmanac.org/games/binaryHeaps.htm
 */
struct BinaryHeap {
	static const int BINARY_HEAP_BLOCKSIZE;
	static const int BINARY_HEAP_BLOCKSIZE_BITS;
	static const int BINARY_HEAP_BLOCKSIZE_MASK;

	void Init(uint max_size);

	bool Push(void *item, int priority);
	void *Pop();
	bool Delete(void *item, int priority);
	void Clear(bool free_values);
	void Free(bool free_values);

	/**
	 * Get an element from the #elements.
	 * @param i Element to access (starts at offset \c 1).
	 * @return Value of the element.
	 */
	inline BinaryHeapNode &GetElement(uint i)
	{
		assert(i > 0);
		return this->elements[(i - 1) >> BINARY_HEAP_BLOCKSIZE_BITS][(i - 1) & BINARY_HEAP_BLOCKSIZE_MASK];
	}

	uint max_size;
	uint size;
	uint blocks; ///< The amount of blocks for which space is reserved in elements
	BinaryHeapNode **elements;
};


/*
 * Hash
 */
struct HashNode {
	TileIndex tile;
	Trackdir dir;
	void *value;
	HashNode *next;
};
/**
 * Generates a hash code from the given key pair. You should make sure that
 * the resulting range is clearly defined.
 */
typedef uint Hash_HashProc(TileIndex tile, Trackdir dir);
struct Hash {
	/* The hash function used */
	Hash_HashProc *hash;
	/* The amount of items in the hash */
	uint size;
	/* The number of buckets allocated */
	uint num_buckets;
	/* A pointer to an array of num_buckets buckets. */
	HashNode *buckets;
	/* A pointer to an array of numbuckets booleans, which will be true if
	 * there are any Nodes in the bucket */
	bool *buckets_in_use;

	void Init(Hash_HashProc *hash, uint num_buckets);

	void *Get(TileIndex tile, Trackdir dir) const;
	void *Set(TileIndex tile, Trackdir dir, void *value);

	void *DeleteValue(TileIndex tile, Trackdir dir);

	void Clear(bool free_values);
	void Delete(bool free_values);

	/**
	 * Gets the current size of the hash.
	 */
	inline uint GetSize() const
	{
		return this->size;
	}

protected:
#ifdef HASH_STATS
	void PrintStatistics() const;
#endif
	HashNode *FindNode(TileIndex tile, Trackdir dir, HashNode** prev_out) const;
};

#endif /* QUEUE_H */
