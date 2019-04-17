/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sort_func.hpp Functions related to sorting operations. */

#ifndef SORT_FUNC_HPP
#define SORT_FUNC_HPP

#include "mem_func.hpp"

/**
 * Type safe qsort()
 *
 * @note Use this sort for irregular sorted data.
 *
 * @param base Pointer to the first element of the array to be sorted.
 * @param num Number of elements in the array pointed by base.
 * @param comparator Function that compares two elements.
 * @param desc Sort descending.
 */
template <typename T>
static inline void QSortT(T *base, size_t num, int (CDECL *comparator)(const T*, const T*), bool desc = false)
{
	if (num < 2) return;

	qsort(base, num, sizeof(T), (int (CDECL *)(const void *, const void *))comparator);

	if (desc) MemReverseT(base, num);
}

#endif /* SORT_FUNC_HPP */
