/*
 * see copyright notice in squirrel.h
 */

#include "../../../stdafx.h"

#include "sqpcheader.h"

#include "../../../core/alloc_func.hpp"
#include "../../../safeguards.h"

#ifdef SQUIRREL_DEFAULT_ALLOCATOR
void *sq_vm_malloc(SQUnsignedInteger size) { return MallocT<char>((size_t)size); }

void *sq_vm_realloc(void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size) { return ReallocT<char>(static_cast<char*>(p), (size_t)size); }

void sq_vm_free(void *p, SQUnsignedInteger size) { free(p); }
#endif
