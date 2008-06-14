/* $Id$ */

/** @file sort_func.hpp Functions related to sorting operations. */

#ifndef SORT_FUNC_HPP
#define SORT_FUNC_HPP

#include <stdlib.h>
#include "math_func.hpp"
#include "mem_func.hpp"

/**
 * Type safe qsort()
 *
 * @todo replace the normal qsort with this one
 * @note Use this sort for irregular sorted data.
 *
 * @param base Pointer to the first element of the array to be sorted.
 * @param num Number of elements in the array pointed by base.
 * @param comparator Function that compares two elements.
 * @param desc Sort descending.
 */
template<typename T>
FORCEINLINE void QSortT(T *base, uint num, int (CDECL *comparator)(const T*, const T*), bool desc = false)
{
	if (num < 2) return;

	qsort(base, num, sizeof(T), (int (CDECL *)(const void *, const void *))comparator);

	if (desc) MemReverseT(base, num);
}

/**
 * Type safe Gnome Sort.
 *
 * This is a slightly modifyied Gnome search. The basic
 * Gnome search trys to sort already sorted list parts.
 * The modification skips these.
 *
 * @note Use this sort for presorted / regular sorted data.
 *
 * @param base Pointer to the first element of the array to be sorted.
 * @param num Number of elements in the array pointed by base.
 * @param comparator Function that compares two elements.
 * @param desc Sort descending.
 */
template<typename T>
FORCEINLINE void GSortT(T *base, uint num, int (CDECL *comparator)(const T*, const T*), bool desc = false)
{
	if (num < 2) return;

	assert(base != NULL);
	assert(comparator != NULL);

	T *a = base;
	T *b = base + 1;
	uint offset = 0;

	while (num > 1) {
		const int diff = comparator(a, b);
		if ((!desc && diff <= 0) || (desc && diff >= 0)) {
			if (offset != 0) {
				/* Jump back to the last direction switch point */
				a += offset;
				b += offset;
				offset = 0;
				continue;
			}

			a++;
			b++;
			num--;
		} else {
			Swap(*a, *b);

			if (a == base) continue;

			a--;
			b--;
			offset++;
		}
	}
}

#endif /* SORT_FUNC_HPP */
