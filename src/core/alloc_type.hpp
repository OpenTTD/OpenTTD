
/* $Id$ */

/** @file alloc_type.hpp Helper types related to the allocation of memory */

#ifndef ALLOC_TYPE_HPP
#define ALLOC_TYPE_HPP

#include "alloc_func.hpp"

/**
 * A small 'wrapper' for allocations that can be done on most OSes on the
 * stack, but are just too large to fit in the stack on devices with a small
 * stack such as the NDS.
 * So when it is possible a stack allocation is made, otherwise a heap
 * allocation is made and this is freed once the struct goes out of scope.
 * @param T      the type to make the allocation for
 * @param length the amount of items to allocate
 */
template <typename T, size_t length>
struct SmallStackSafeStackAlloc {
#if !defined(__NDS__)
	/** Storing the data on the stack */
	T data[length];
#else
	/** Storing it on the heap */
	T *data;
	/** The length (in elements) of data in this allocator. */
	size_t len;

	/** Allocating the memory */
	SmallStackSafeStackAlloc() : data(MallocT<T>(length)), len(length) {}
	/** And freeing when it goes out of scope */
	~SmallStackSafeStackAlloc() { free(data); }
#endif

	/**
	 * Gets a pointer to the data stored in this wrapper.
	 * @return the pointer.
	 */
	inline operator T* () { return data; }

	/**
	 * Gets a pointer to the data stored in this wrapper.
	 * @return the pointer.
	 */
	inline T* operator -> () { return data; }

	/**
	 * Gets a pointer to the last data element stored in this wrapper.
	 * @note needed because endof does not work properly for pointers.
	 * @return the 'endof' pointer.
	 */
	inline T* EndOf() {
#if !defined(__NDS__)
		return endof(data);
#else
		return &data[len];
#endif
	}
};

/**
 * Base class that provides memory initialization on dynamically created objects.
 * All allocated memory will be zeroed.
 */
class ZeroedMemoryAllocator
{
public:
	ZeroedMemoryAllocator() {}
	virtual ~ZeroedMemoryAllocator() {}

	/**
	 * Memory allocator for a single class instance.
	 * @param size the amount of bytes to allocate.
	 * @return the given amounts of bytes zeroed.
	 */
	void *operator new(size_t size) { return CallocT<byte>(size); }

	/**
	 * Memory allocator for an array of class instances.
	 * @param size the amount of bytes to allocate.
	 * @return the given amounts of bytes zeroed.
	 */
	void *operator new[](size_t size) { return CallocT<byte>(size); }

	/**
	 * Memory release for a single class instance.
	 * @param ptr  the memory to free.
	 * @param size the amount of allocated memory (unused).
	 */
	void operator delete(void *ptr, size_t size) { free(ptr); }

	/**
	 * Memory release for an array of class instances.
	 * @param ptr  the memory to free.
	 * @param size the amount of allocated memory (unused).
	 */
	void operator delete[](void *ptr, size_t size) { free(ptr); }
};

#endif /* ALLOC_TYPE_HPP */
