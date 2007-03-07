/* $Id$ */

#ifndef QUEUE_H
#define QUEUE_H

//#define NOFREE
//#define QUEUE_DEBUG
//#define HASH_DEBUG
//#define HASH_STATS


struct Queue;
typedef bool Queue_PushProc(Queue* q, void* item, int priority);
typedef void* Queue_PopProc(Queue* q);
typedef bool Queue_DeleteProc(Queue* q, void* item, int priority);
typedef void Queue_ClearProc(Queue* q, bool free_values);
typedef void Queue_FreeProc(Queue* q, bool free_values);

struct InsSortNode {
	void* item;
	int priority;
	InsSortNode* next;
};

struct BinaryHeapNode {
	void* item;
	int priority;
};


struct Queue{
	/*
	 * Pushes an element into the queue, at the appropriate place for the queue.
	 * Requires the queue pointer to be of an appropriate type, of course.
	 */
	Queue_PushProc* push;
	/*
	 * Pops the first element from the queue. What exactly is the first element,
	 * is defined by the exact type of queue.
	 */
	Queue_PopProc* pop;
	/*
	 * Deletes the item from the queue. priority should be specified if
	 * known, which speeds up the deleting for some queue's. Should be -1
	 * if not known.
	 */
	Queue_DeleteProc* del;

	/* Clears the queue, by removing all values from it. It's state is
	 * effectively reset. If free_items is true, each of the items cleared
	 * in this way are free()'d.
	 */
	Queue_ClearProc* clear;
	/* Frees the queue, by reclaiming all memory allocated by it. After
	 * this it is no longer usable. If free_items is true, any remaining
	 * items are free()'d too.
	 */
	Queue_FreeProc* free;

	union {
		struct {
			InsSortNode* first;
		} inssort;
		struct {
			uint max_size;
			uint size;
			uint blocks; /* The amount of blocks for which space is reserved in elements */
			BinaryHeapNode** elements;
		} binaryheap;
	} data;
};


/*
 * Insertion Sorter
 */

/* Initializes a inssort and allocates internal memory. There is no maximum
 * size */
void init_InsSort(Queue* q);


/*
 *  Binary Heap
 *  For information, see:
 *   http://www.policyalmanac.org/games/binaryHeaps.htm
 */

/* The amount of elements that will be malloc'd at a time */
#define BINARY_HEAP_BLOCKSIZE_BITS 10

/* Initializes a binary heap and allocates internal memory for maximum of
 * max_size elements */
void init_BinaryHeap(Queue* q, uint max_size);


/*
 * Hash
 */
struct HashNode {
	uint key1;
	uint key2;
	void* value;
	HashNode* next;
};
/**
 * Generates a hash code from the given key pair. You should make sure that
 * the resulting range is clearly defined.
 */
typedef uint Hash_HashProc(uint key1, uint key2);
struct Hash {
	/* The hash function used */
	Hash_HashProc* hash;
	/* The amount of items in the hash */
	uint size;
	/* The number of buckets allocated */
	uint num_buckets;
	/* A pointer to an array of num_buckets buckets. */
	HashNode* buckets;
	/* A pointer to an array of numbuckets booleans, which will be true if
	 * there are any Nodes in the bucket */
	bool* buckets_in_use;
};

/* Call these function to manipulate a hash */

/* Deletes the value with the specified key pair from the hash and returns
 * that value. Returns NULL when the value was not present. The value returned
 * is _not_ free()'d! */
void* Hash_Delete(Hash* h, uint key1, uint key2);
/* Sets the value associated with the given key pair to the given value.
 * Returns the old value if the value was replaced, NULL when it was not yet present. */
void* Hash_Set(Hash* h, uint key1, uint key2, void* value);
/* Gets the value associated with the given key pair, or NULL when it is not
 * present. */
void* Hash_Get(const Hash* h, uint key1, uint key2);

/* Call these function to create/destroy a hash */

/* Builds a new hash in an existing struct. Make sure that hash() always
 * returns a hash less than num_buckets! Call delete_hash after use */
void init_Hash(Hash* h, Hash_HashProc* hash, uint num_buckets);
/*
 * Deletes the hash and cleans up. Only cleans up memory allocated by new_Hash
 * & friends. If free is true, it will call free() on all the values that
 * are left in the hash.
 */
void delete_Hash(Hash* h, bool free_values);
/*
 * Cleans the hash, but keeps the memory allocated
 */
void clear_Hash(Hash* h, bool free_values);
/*
 * Gets the current size of the Hash
 */
uint Hash_Size(const Hash* h);

#endif /* QUEUE_H */
