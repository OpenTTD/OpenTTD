/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file queue.h Binary heap implementation, hash implementation. */

#ifndef QUEUE_H
#define QUEUE_H

//#define NOFREE
//#define QUEUE_DEBUG
//#define HASH_DEBUG
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
	FORCEINLINE BinaryHeapNode &GetElement(uint i)
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
	uint key1;
	uint key2;
	void *value;
	HashNode *next;
};
/**
 * Generates a hash code from the given key pair. You should make sure that
 * the resulting range is clearly defined.
 */
typedef uint Hash_HashProc(uint key1, uint key2);
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
};

/* Call these function to manipulate a hash */

/**
 * Deletes the value with the specified key pair from the hash and returns
 * that value. Returns NULL when the value was not present. The value returned
 * is _not_ free()'d!
 */
void *Hash_Delete(Hash *h, uint key1, uint key2);
/**
 * Sets the value associated with the given key pair to the given value.
 * Returns the old value if the value was replaced, NULL when it was not yet present.
 */
void *Hash_Set(Hash *h, uint key1, uint key2, void *value);
/**
 * Gets the value associated with the given key pair, or NULL when it is not
 * present.
 */
void *Hash_Get(const Hash *h, uint key1, uint key2);

/* Call these function to create/destroy a hash */

/**
 * Builds a new hash in an existing struct. Make sure that hash() always
 * returns a hash less than num_buckets! Call delete_hash after use
 */
void init_Hash(Hash *h, Hash_HashProc *hash, uint num_buckets);
/**
 * Deletes the hash and cleans up. Only cleans up memory allocated by new_Hash
 * & friends. If free is true, it will call free() on all the values that
 * are left in the hash.
 */
void delete_Hash(Hash *h, bool free_values);
/**
 * Cleans the hash, but keeps the memory allocated
 */
void clear_Hash(Hash *h, bool free_values);
/**
 * Gets the current size of the Hash
 */
uint Hash_Size(const Hash *h);

#endif /* QUEUE_H */
