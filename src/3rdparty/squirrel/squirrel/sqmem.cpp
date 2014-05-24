/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
void *sq_vm_malloc(SQUnsignedInteger size){	return malloc((size_t)size); }

void *sq_vm_realloc(void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size){ return realloc(p, (size_t)size); }

void sq_vm_free(void *p, SQUnsignedInteger size){	free(p); }
