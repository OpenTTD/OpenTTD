/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
	~SmallStackSafeStackAlloc()
	{
		free(data);
	}
#endif

	/**
	 * Gets a pointer to the data stored in this wrapper.
	 * @return the pointer.
	 */
	inline operator T *()
	{
		return data;
	}

	/**
	 * Gets a pointer to the data stored in this wrapper.
	 * @return the pointer.
	 */
	inline T *operator -> ()
	{
		return data;
	}

	/**
	 * Gets a pointer to the last data element stored in this wrapper.
	 * @note needed because endof does not work properly for pointers.
	 * @return the 'endof' pointer.
	 */
	inline T *EndOf()
	{
#if !defined(__NDS__)
		return endof(data);
#else
		return &data[len];
#endif
	}
};

/**
 * A reusable buffer that can be used for places that temporary allocate
 * a bit of memory and do that very often, or for places where static
 * memory is allocated that might need to be reallocated sometimes.
 *
 * Every time Allocate or ZeroAllocate is called previous results of both
 * functions will become invalid.
 */
template <typename T>
class ReusableBuffer {
private:
	T *buffer;    ///< The real data buffer
	size_t count; ///< Number of T elements in the buffer

public:
	/** Create a new buffer */
	ReusableBuffer() : buffer(NULL), count(0) {}
	/** Clear the buffer */
	~ReusableBuffer() { free(this->buffer); }

	/**
	 * Get buffer of at least count times T.
	 * @note the buffer might be bigger
	 * @note calling this function invalidates any previous buffers given
	 * @param count the minimum buffer size
	 * @return the buffer
	 */
	T *Allocate(size_t count)
	{
		if (this->count < count) {
			free(this->buffer);
			this->buffer = MallocT<T>(count);
			this->count = count;
		}
		return this->buffer;
	}

	/**
	 * Get buffer of at least count times T with zeroed memory.
	 * @note the buffer might be bigger
	 * @note calling this function invalidates any previous buffers given
	 * @param count the minimum buffer size
	 * @return the buffer
	 */
	T *ZeroAllocate(size_t count)
	{
		if (this->count < count) {
			free(this->buffer);
			this->buffer = CallocT<T>(count);
			this->count = count;
		} else {
			memset(this->buffer, 0, sizeof(T) * count);
		}
		return this->buffer;
	}

	/**
	 * Get the currently allocated buffer.
	 * @return the buffer
	 */
	inline const T *GetBuffer() const
	{
		return this->buffer;
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
	inline void *operator new(size_t size) { return CallocT<byte>(size); }

	/**
	 * Memory allocator for an array of class instances.
	 * @param size the amount of bytes to allocate.
	 * @return the given amounts of bytes zeroed.
	 */
	inline void *operator new[](size_t size) { return CallocT<byte>(size); }

	/**
	 * Memory release for a single class instance.
	 * @param ptr  the memory to free.
	 */
	inline void operator delete(void *ptr) { free(ptr); }

	/**
	 * Memory release for an array of class instances.
	 * @param ptr  the memory to free.
	 */
	inline void operator delete[](void *ptr) { free(ptr); }
};

#endif /* ALLOC_TYPE_HPP */
