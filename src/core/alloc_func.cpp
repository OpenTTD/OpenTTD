/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file alloc_func.cpp Functions to 'handle' memory allocation errors */

#include "../stdafx.h"

#include "../error_func.h"

#include "../safeguards.h"

/**
 * Function to exit with an error message after malloc() or calloc() have failed
 * @param size number of bytes we tried to allocate
 */
[[noreturn]] void MallocError(size_t size)
{
	FatalError("Out of memory. Cannot allocate {} bytes", size);
}

/**
 * Function to exit with an error message after realloc() have failed
 * @param size number of bytes we tried to allocate
 */
[[noreturn]] void ReallocError(size_t size)
{
	FatalError("Out of memory. Cannot reallocate {} bytes", size);
}
