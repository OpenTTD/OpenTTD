/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mem_func.hpp Functions related to memory operations. */

#ifndef MEM_FUNC_HPP
#define MEM_FUNC_HPP

#include "math_func.hpp"

/**
 * Type-safe version of memcpy().
 *
 * @param destination Pointer to the destination buffer
 * @param source Pointer to the source buffer
 * @param num number of items to be copied. (!not number of bytes!)
 */
template <typename T>
inline void MemCpyT(T *destination, const T *source, size_t num = 1)
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
inline void MemMoveT(T *destination, const T *source, size_t num = 1)
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
inline void MemSetT(T *ptr, byte value, size_t num = 1)
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
inline int MemCmpT(const T *ptr1, const T *ptr2, size_t num = 1)
{
	return memcmp(ptr1, ptr2, num * sizeof(T));
}

#endif /* MEM_FUNC_HPP */
