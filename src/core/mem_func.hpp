/* $Id$ */

/** @file mem_func.hpp Functions related to memory operations. */

#ifndef MEM_FUNC_HPP
#define MEM_FUNC_HPP

#include <string.h>
#include "math_func.hpp"

/**
 * Type-safe version of memcpy().
 *
 * @param destination Pointer to the destination buffer
 * @param source Pointer to the source buffer
 * @param num number of items to be copied. (!not number of bytes!)
 */
template <typename T>
FORCEINLINE void MemCpyT(T *destination, const T *source, uint num = 1)
{
	memcpy(destination, source, num * sizeof(T));
}

/**
 * Type-safe version of memmove().
 *
 * @param destination Pointer to the destination buffer
 * @param source Pointer to the source buffer
 * @param num number of items to be copied. (!not number of bytes!)
 */
template <typename T>
FORCEINLINE void MemMoveT(T *destination, const T *source, uint num = 1)
{
	memmove(destination, source, num * sizeof(T));
}

/**
 * Type-safe version of memset().
 *
 * @param ptr Pointer to the destination buffer
 * @param value Value to be set
 * @param num number of items to be set (!not number of bytes!)
 */
template <typename T>
FORCEINLINE void MemSetT(T *ptr, int value, uint num = 1)
{
	memset(ptr, value, num * sizeof(T));
}

/**
 * Type-safe version of memcmp().
 *
 * @param ptr1 Pointer to the first buffer
 * @param ptr2 Pointer to the second buffer
 * @param num Number of items to compare. (!not number of bytes!)
 * @return an int value indicating the relationship between the content of the two buffers
 */
template <typename T>
FORCEINLINE int MemCmpT(const T *ptr1, const T *ptr2, uint num = 1)
{
	return memcmp(ptr1, ptr2, num * sizeof(T));
}

/**
 * Type safe memory reverse operation.
 *  Reverse a block of memory in steps given by the
 *  type of the pointers.
 *
 * @param ptr1 Start-pointer to the block of memory.
 * @param ptr2 End-pointer to the block of memory.
 */
template<typename T>
FORCEINLINE void MemReverseT(T *ptr1, T *ptr2)
{
	assert(ptr1 != NULL && ptr2 != NULL);
	assert(ptr1 < ptr2);

	do {
		Swap(*ptr1, *ptr2);
	} while (++ptr1 < --ptr2);
}

/**
 * Type safe memory reverse operation (overloaded)
 *
 * @param ptr Pointer to the block of memory.
 * @param num The number of items we want to reverse.
 */
template<typename T>
FORCEINLINE void MemReverseT(T *ptr, uint num)
{
	assert(ptr != NULL);

	MemReverseT(ptr, ptr + (num - 1));
}

#endif /* MEM_FUNC_HPP */
