/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file alloc_func.hpp Functions related to the allocation of memory */

#ifndef ALLOC_FUNC_HPP
#define ALLOC_FUNC_HPP

/*
 * Functions to exit badly with an error message.
 * It has to be linked so the error messages are not
 * duplicated in each object file making the final
 * binary needlessly large.
 */

void NORETURN MallocError(size_t size);
void NORETURN ReallocError(size_t size);

/**
 * Checks whether allocating memory would overflow size_t.
 *
 * @param element_size Size of the structure to allocate.
 * @param num_elements Number of elements to allocate.
 */
static inline void CheckAllocationConstraints(size_t element_size, size_t num_elements)
{
	if (num_elements > SIZE_MAX / element_size) MallocError(SIZE_MAX);
}

/**
 * Checks whether allocating memory would overflow size_t.
 *
 * @tparam T Structure to allocate.
 * @param num_elements Number of elements to allocate.
 */
template <typename T>
static inline void CheckAllocationConstraints(size_t num_elements)
{
	CheckAllocationConstraints(sizeof(T), num_elements);
}

/**
 * Simplified allocation function that allocates the specified number of
 * elements of the given type. It also explicitly casts it to the requested
 * type.
 * @note throws an error when there is no memory anymore.
 * @note the memory contains garbage data (i.e. possibly non-zero values).
 * @tparam T the type of the variable(s) to allocation.
 * @param num_elements the number of elements to allocate of the given type.
 * @return NULL when num_elements == 0, non-NULL otherwise.
 */
template <typename T>
static inline T *MallocT(size_t num_elements)
{
	/*
	 * MorphOS cannot handle 0 elements allocations, or rather that always
	 * returns NULL. So we do that for *all* allocations, thus causing it
	 * to behave the same on all OSes.
	 */
	if (num_elements == 0) return NULL;

	/* Ensure the size does not overflow. */
	CheckAllocationConstraints<T>(num_elements);

	T *t_ptr = (T*)malloc(num_elements * sizeof(T));
	if (t_ptr == NULL) MallocError(num_elements * sizeof(T));
	return t_ptr;
}

/**
 * Simplified allocation function that allocates the specified number of
 * elements of the given type. It also explicitly casts it to the requested
 * type.
 * @note throws an error when there is no memory anymore.
 * @note the memory contains all zero values.
 * @tparam T the type of the variable(s) to allocation.
 * @param num_elements the number of elements to allocate of the given type.
 * @return NULL when num_elements == 0, non-NULL otherwise.
 */
template <typename T>
static inline T *CallocT(size_t num_elements)
{
	/*
	 * MorphOS cannot handle 0 elements allocations, or rather that always
	 * returns NULL. So we do that for *all* allocations, thus causing it
	 * to behave the same on all OSes.
	 */
	if (num_elements == 0) return NULL;

	T *t_ptr = (T*)calloc(num_elements, sizeof(T));
	if (t_ptr == NULL) MallocError(num_elements * sizeof(T));
	return t_ptr;
}

/**
 * Simplified reallocation function that allocates the specified number of
 * elements of the given type. It also explicitly casts it to the requested
 * type. It extends/shrinks the memory allocation given in t_ptr.
 * @note throws an error when there is no memory anymore.
 * @note the pointer to the data may change, but the data will remain valid.
 * @tparam T the type of the variable(s) to allocation.
 * @param t_ptr the previous allocation to extend/shrink.
 * @param num_elements the number of elements to allocate of the given type.
 * @return NULL when num_elements == 0, non-NULL otherwise.
 */
template <typename T>
static inline T *ReallocT(T *t_ptr, size_t num_elements)
{
	/*
	 * MorphOS cannot handle 0 elements allocations, or rather that always
	 * returns NULL. So we do that for *all* allocations, thus causing it
	 * to behave the same on all OSes.
	 */
	if (num_elements == 0) {
		free(t_ptr);
		return NULL;
	}

	/* Ensure the size does not overflow. */
	CheckAllocationConstraints<T>(num_elements);

	t_ptr = (T*)realloc(static_cast<void *>(t_ptr), num_elements * sizeof(T));
	if (t_ptr == NULL) ReallocError(num_elements * sizeof(T));
	return t_ptr;
}

/** alloca() has to be called in the parent function, so define AllocaM() as a macro */
#define AllocaM(T, num_elements) \
	(CheckAllocationConstraints<T>(num_elements), \
	(T*)alloca((num_elements) * sizeof(T)))

#endif /* ALLOC_FUNC_HPP */
