/* $Id$ */

/** @file alloc_func.cpp functions to 'handle' memory allocation errors */

#include "../stdafx.h"
#include "alloc_func.hpp"

/**
 * Function to exit with an error message after malloc() or calloc() have failed
 * @param size number of bytes we tried to allocate
 */
void NORETURN MallocError(size_t size)
{
	error("Out of memory. Cannot allocate %i bytes", size);
}

/**
 * Function to exit with an error message after realloc() have failed
 * @param size number of bytes we tried to allocate
 */
void NORETURN ReallocError(size_t size)
{
	error("Out of memory. Cannot reallocate %i bytes", size);
}
