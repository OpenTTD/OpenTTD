/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file queue.cpp Implementation of the #BinaryHeap/#Hash. */

#include "../../stdafx.h"
#include "../../core/alloc_func.hpp"
#include "queue.h"

#include "../../safeguards.h"


/*
 * Binary Heap
 * For information, see: http://www.policyalmanac.org/games/binaryHeaps.htm
 */

const int BinaryHeap::BINARY_HEAP_BLOCKSIZE_BITS = 10; ///< The number of elements that will be malloc'd at a time.
const int BinaryHeap::BINARY_HEAP_BLOCKSIZE      = 1 << BinaryHeap::BINARY_HEAP_BLOCKSIZE_BITS;
const int BinaryHeap::BINARY_HEAP_BLOCKSIZE_MASK = BinaryHeap::BINARY_HEAP_BLOCKSIZE - 1;

/**
 * Clears the queue, by removing all values from it. Its state is
 * effectively reset. If free_items is true, each of the items cleared
 * in this way are free()'d.
 */
void BinaryHeap::Clear(bool free_values)
{
	/* Free all items if needed and free all but the first blocks of memory */
	uint i;
	uint j;

	for (i = 0; i < this->blocks; i++) {
		if (this->elements[i] == nullptr) {
			/* No more allocated blocks */
			break;
		}
		/* For every allocated block */
		if (free_values) {
			for (j = 0; j < (1 << BINARY_HEAP_BLOCKSIZE_BITS); j++) {
				/* For every element in the block */
				if ((this->size >> BINARY_HEAP_BLOCKSIZE_BITS) == i &&
						(this->size & BINARY_HEAP_BLOCKSIZE_MASK) == j) {
					break; // We're past the last element
				}
				free(this->elements[i][j].item);
			}
		}
		if (i != 0) {
			/* Leave the first block of memory alone */
			free(this->elements[i]);
			this->elements[i] = nullptr;
		}
	}
	this->size = 0;
	this->blocks = 1;
}

/**
 * Frees the queue, by reclaiming all memory allocated by it. After
 * this it is no longer usable. If free_items is true, any remaining
 * items are free()'d too.
 */
void BinaryHeap::Free(bool free_values)
{
	uint i;

	this->Clear(free_values);
	for (i = 0; i < this->blocks; i++) {
		if (this->elements[i] == nullptr) break;
		free(this->elements[i]);
	}
	free(this->elements);
}

/**
 * Pushes an element into the queue, at the appropriate place for the queue.
 * Requires the queue pointer to be of an appropriate type, of course.
 */
bool BinaryHeap::Push(void *item, int priority)
{
	if (this->size == this->max_size) return false;
	assert(this->size < this->max_size);

	if (this->elements[this->size >> BINARY_HEAP_BLOCKSIZE_BITS] == nullptr) {
		/* The currently allocated blocks are full, allocate a new one */
		assert((this->size & BINARY_HEAP_BLOCKSIZE_MASK) == 0);
		this->elements[this->size >> BINARY_HEAP_BLOCKSIZE_BITS] = MallocT<BinaryHeapNode>(BINARY_HEAP_BLOCKSIZE);
		this->blocks++;
	}

	/* Add the item at the end of the array */
	this->GetElement(this->size + 1).priority = priority;
	this->GetElement(this->size + 1).item = item;
	this->size++;

	/* Now we are going to check where it belongs. As long as the parent is
	 * bigger, we switch with the parent */
	{
		BinaryHeapNode temp;
		int i;
		int j;

		i = this->size;
		while (i > 1) {
			/* Get the parent of this object (divide by 2) */
			j = i / 2;
			/* Is the parent bigger than the current, switch them */
			if (this->GetElement(i).priority <= this->GetElement(j).priority) {
				temp = this->GetElement(j);
				this->GetElement(j) = this->GetElement(i);
				this->GetElement(i) = temp;
				i = j;
			} else {
				/* It is not, we're done! */
				break;
			}
		}
	}

	return true;
}

/**
 * Deletes the item from the queue. priority should be specified if
 * known, which speeds up the deleting for some queue's. Should be -1
 * if not known.
 */
bool BinaryHeap::Delete(void *item, int)
{
	uint i = 0;

	/* First, we try to find the item.. */
	do {
		if (this->GetElement(i + 1).item == item) break;
		i++;
	} while (i < this->size);
	/* We did not find the item, so we return false */
	if (i == this->size) return false;

	/* Now we put the last item over the current item while decreasing the size of the elements */
	this->size--;
	this->GetElement(i + 1) = this->GetElement(this->size + 1);

	/* Now the only thing we have to do, is resort it..
	 * On place i there is the item to be sorted.. let's start there */
	{
		uint j;
		BinaryHeapNode temp;
		/* Because of the fact that Binary Heap uses array from 1 to n, we need to
		 * increase i by 1
		 */
		i++;

		for (;;) {
			j = i;
			/* Check if we have 2 children */
			if (2 * j + 1 <= this->size) {
				/* Is this child smaller than the parent? */
				if (this->GetElement(j).priority >= this->GetElement(2 * j).priority) i = 2 * j;
				/* Yes, we _need_ to use i here, not j, because we want to have the smallest child
				 *  This way we get that straight away! */
				if (this->GetElement(i).priority >= this->GetElement(2 * j + 1).priority) i = 2 * j + 1;
			/* Do we have one child? */
			} else if (2 * j <= this->size) {
				if (this->GetElement(j).priority >= this->GetElement(2 * j).priority) i = 2 * j;
			}

			/* One of our children is smaller than we are, switch */
			if (i != j) {
				temp = this->GetElement(j);
				this->GetElement(j) = this->GetElement(i);
				this->GetElement(i) = temp;
			} else {
				/* None of our children is smaller, so we stay here.. stop :) */
				break;
			}
		}
	}

	return true;
}

/**
 * Pops the first element from the queue. What exactly is the first element,
 * is defined by the exact type of queue.
 */
void *BinaryHeap::Pop()
{
	void *result;

	if (this->size == 0) return nullptr;

	/* The best item is always on top, so give that as result */
	result = this->GetElement(1).item;
	/* And now we should get rid of this item... */
	this->Delete(this->GetElement(1).item, this->GetElement(1).priority);

	return result;
}

/**
 * Initializes a binary heap and allocates internal memory for maximum of
 * max_size elements
 */
void BinaryHeap::Init(uint max_size)
{
	this->max_size = max_size;
	this->size = 0;
	/* We malloc memory in block of BINARY_HEAP_BLOCKSIZE
	 *   It autosizes when it runs out of memory */
	this->elements = CallocT<BinaryHeapNode*>((max_size - 1) / BINARY_HEAP_BLOCKSIZE + 1);
	this->elements[0] = MallocT<BinaryHeapNode>(BINARY_HEAP_BLOCKSIZE);
	this->blocks = 1;
}

/* Because we don't want anyone else to bother with our defines */
#undef BIN_HEAP_ARR

/*
 * Hash
 */

/**
 * Builds a new hash in an existing struct. Make sure that hash() always
 * returns a hash less than num_buckets! Call delete_hash after use
 */
void Hash::Init(Hash_HashProc *hash, uint num_buckets)
{
	/* Allocate space for the Hash, the buckets and the bucket flags */
	uint i;

	/* Ensure the size won't overflow. */
	CheckAllocationConstraints(sizeof(*this->buckets) + sizeof(*this->buckets_in_use), num_buckets);

	this->hash = hash;
	this->size = 0;
	this->num_buckets = num_buckets;
	this->buckets = (HashNode*)MallocT<byte>(num_buckets * (sizeof(*this->buckets) + sizeof(*this->buckets_in_use)));
	this->buckets_in_use = (bool*)(this->buckets + num_buckets);
	for (i = 0; i < num_buckets; i++) this->buckets_in_use[i] = false;
}

/**
 * Deletes the hash and cleans up. Only cleans up memory allocated by new_Hash
 * & friends. If free is true, it will call free() on all the values that
 * are left in the hash.
 */
void Hash::Delete(bool free_values)
{
	uint i;

	/* Iterate all buckets */
	for (i = 0; i < this->num_buckets; i++) {
		if (this->buckets_in_use[i]) {
			HashNode *node;

			/* Free the first value */
			if (free_values) free(this->buckets[i].value);
			node = this->buckets[i].next;
			while (node != nullptr) {
				HashNode *prev = node;

				node = node->next;
				/* Free the value */
				if (free_values) free(prev->value);
				/* Free the node */
				free(prev);
			}
		}
	}
	free(this->buckets);
	/* No need to free buckets_in_use, it is always allocated in one
	 * malloc with buckets */
}

#ifdef HASH_STATS
void Hash::PrintStatistics() const
{
	uint used_buckets = 0;
	uint max_collision = 0;
	uint max_usage = 0;
	uint usage[200];
	uint i;

	for (i = 0; i < lengthof(usage); i++) usage[i] = 0;
	for (i = 0; i < this->num_buckets; i++) {
		uint collision = 0;
		if (this->buckets_in_use[i]) {
			const HashNode *node;

			used_buckets++;
			for (node = &this->buckets[i]; node != nullptr; node = node->next) collision++;
			if (collision > max_collision) max_collision = collision;
		}
		if (collision >= lengthof(usage)) collision = lengthof(usage) - 1;
		usage[collision]++;
		if (collision > 0 && usage[collision] >= max_usage) {
			max_usage = usage[collision];
		}
	}
	Debug(misc, 0, "Hash size: {}, Nodes used: {}, Non empty buckets: {}, Max collision: {}",
		this->num_buckets, this->size, used_buckets, max_collision
	);
	std::string line;
	line += "{ ";
	for (i = 0; i <= max_collision; i++) {
		if (usage[i] > 0) {
			fmt::format_to(std::back_inserter(line), "{}:{} ", i, usage[i]);
#if 0
			if (i > 0) {
				uint j;

				for (j = 0; j < usage[i] * 160 / 800; j++) line += "#";
			}
			line += "\n";
#endif
		}
	}
	line += "}";
	Debug(misc, 0, "{}", line);
}
#endif

/**
 * Cleans the hash, but keeps the memory allocated
 */
void Hash::Clear(bool free_values)
{
	uint i;

#ifdef HASH_STATS
	if (this->size > 2000) this->PrintStatistics();
#endif

	/* Iterate all buckets */
	for (i = 0; i < this->num_buckets; i++) {
		if (this->buckets_in_use[i]) {
			HashNode *node;

			this->buckets_in_use[i] = false;
			/* Free the first value */
			if (free_values) free(this->buckets[i].value);
			node = this->buckets[i].next;
			while (node != nullptr) {
				HashNode *prev = node;

				node = node->next;
				if (free_values) free(prev->value);
				free(prev);
			}
		}
	}
	this->size = 0;
}

/**
 * Finds the node that that saves this key pair. If it is not
 * found, returns nullptr. If it is found, *prev is set to the
 * node before the one found, or if the node found was the first in the bucket
 * to nullptr. If it is not found, *prev is set to the last HashNode in the
 * bucket, or nullptr if it is empty. prev can also be nullptr, in which case it is
 * not used for output.
 */
HashNode *Hash::FindNode(TileIndex tile, Trackdir dir, HashNode** prev_out) const
{
	uint hash = this->hash(tile, dir);
	HashNode *result = nullptr;

	/* Check if the bucket is empty */
	if (!this->buckets_in_use[hash]) {
		if (prev_out != nullptr) *prev_out = nullptr;
		result = nullptr;
	/* Check the first node specially */
	} else if (this->buckets[hash].tile == tile && this->buckets[hash].dir == dir) {
		/* Save the value */
		result = this->buckets + hash;
		if (prev_out != nullptr) *prev_out = nullptr;
	/* Check all other nodes */
	} else {
		HashNode *prev = this->buckets + hash;
		HashNode *node;

		for (node = prev->next; node != nullptr; node = node->next) {
			if (node->tile == tile && node->dir == dir) {
				/* Found it */
				result = node;
				break;
			}
			prev = node;
		}
		if (prev_out != nullptr) *prev_out = prev;
	}
	return result;
}

/**
 * Deletes the value with the specified key pair from the hash and returns
 * that value. Returns nullptr when the value was not present. The value returned
 * is _not_ free()'d!
 */
void *Hash::DeleteValue(TileIndex tile, Trackdir dir)
{
	void *result;
	HashNode *prev; // Used as output var for below function call
	HashNode *node = this->FindNode(tile, dir, &prev);

	if (node == nullptr) {
		/* not found */
		result = nullptr;
	} else if (prev == nullptr) {
		/* It is in the first node, we can't free that one, so we free
		 * the next one instead (if there is any)*/
		/* Save the value */
		result = node->value;
		if (node->next != nullptr) {
			HashNode *next = node->next;
			/* Copy the second to the first */
			*node = *next;
			/* Free the second */
			free(next);
		} else {
			/* This was the last in this bucket
			 * Mark it as empty */
			uint hash = this->hash(tile, dir);
			this->buckets_in_use[hash] = false;
		}
	} else {
		/* It is in another node
		 * Save the value */
		result = node->value;
		/* Link previous and next nodes */
		prev->next = node->next;
		/* Free the node */
		free(node);
	}
	if (result != nullptr) this->size--;
	return result;
}

/**
 * Sets the value associated with the given key pair to the given value.
 * Returns the old value if the value was replaced, nullptr when it was not yet present.
 */
void *Hash::Set(TileIndex tile, Trackdir dir, void *value)
{
	HashNode *prev;
	HashNode *node = this->FindNode(tile, dir, &prev);

	if (node != nullptr) {
		/* Found it */
		void *result = node->value;

		node->value = value;
		return result;
	}
	/* It is not yet present, let's add it */
	if (prev == nullptr) {
		/* The bucket is still empty */
		uint hash = this->hash(tile, dir);
		this->buckets_in_use[hash] = true;
		node = this->buckets + hash;
	} else {
		/* Add it after prev */
		node = MallocT<HashNode>(1);
		prev->next = node;
	}
	node->next = nullptr;
	node->tile = tile;
	node->dir = dir;
	node->value = value;
	this->size++;
	return nullptr;
}

/**
 * Gets the value associated with the given key pair, or nullptr when it is not
 * present.
 */
void *Hash::Get(TileIndex tile, Trackdir dir) const
{
	HashNode *node = this->FindNode(tile, dir, nullptr);

	return (node != nullptr) ? node->value : nullptr;
}
