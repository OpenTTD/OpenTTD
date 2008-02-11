/* $Id$ */

/** @file alloc_func.hpp Functions related to the allocation of memory */

#ifndef ALLOC_FUNC_HPP
#define ALLOC_FUNC_HPP

/**
 * Functions to exit badly with an error message.
 * It has to be linked so the error messages are not
 * duplicated in each object file making the final
 * binary needlessly large.
 */
void MallocError(size_t size);
void ReallocError(size_t size);

/**
 * Simplified allocation function that allocates the specified number of
 * elements of the given type. It also explicitly casts it to the requested
 * type.
 * @note throws an error when there is no memory anymore.
 * @note the memory contains garbage data (i.e. possibly non-zero values).
 * @param T the type of the variable(s) to allocation.
 * @param num_elements the number of elements to allocate of the given type.
 * @return NULL when num_elements == 0, non-NULL otherwise.
 */
template <typename T> FORCEINLINE T* MallocT(size_t num_elements)
{
	/*
	 * MorphOS cannot handle 0 elements allocations, or rather that always
	 * returns NULL. So we do that for *all* allocations, thus causing it
	 * to behave the same on all OSes.
	 */
	if (num_elements == 0) return NULL;

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
 * @param T the type of the variable(s) to allocation.
 * @param num_elements the number of elements to allocate of the given type.
 * @return NULL when num_elements == 0, non-NULL otherwise.
 */
template <typename T> FORCEINLINE T* CallocT(size_t num_elements)
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
 * @note the memory contains all zero values.
 * @param T the type of the variable(s) to allocation.
 * @param t_ptr the previous allocation to extend/shrink.
 * @param num_elements the number of elements to allocate of the given type.
 * @return NULL when num_elements == 0, non-NULL otherwise.
 */
template <typename T> FORCEINLINE T* ReallocT(T *t_ptr, size_t num_elements)
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

	t_ptr = (T*)realloc(t_ptr, num_elements * sizeof(T));
	if (t_ptr == NULL) ReallocError(num_elements * sizeof(T));
	return t_ptr;
}

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

	/** Allocating the memory */
	SmallStackSafeStackAlloc() : data(MallocT<T>(length)) {}
	/** And freeing when it goes out of scope */
	~SmallStackSafeStackAlloc() { free(data); }
#endif

	/**
	 * Gets a pointer to the data stored in this wrapper.
	 * @return the pointer.
	 */
	operator T* () { return data; }
};

#endif /* ALLOC_FUNC_HPP */
