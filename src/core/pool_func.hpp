/* $Id$ */

/** @file pool_func.hpp Some methods of Pool are placed here in order to reduce compilation time and binary size. */

#ifndef POOL_FUNC_HPP
#define POOL_FUNC_HPP

#include "alloc_func.hpp"
#include "mem_func.hpp"
#include "pool_type.hpp"

#define DEFINE_POOL_METHOD(type) \
	template <class Titem, typename Tindex, size_t Tgrowth_step, size_t Tmax_size> \
	type Pool<Titem, Tindex, Tgrowth_step, Tmax_size>

DEFINE_POOL_METHOD(inline)::Pool(const char *name) :
		name(name),
		size(0),
		first_free(0),
		first_unused(0),
		items(0),
		cleaning(false),
		data(NULL)
{ }

DEFINE_POOL_METHOD(inline void)::ResizeFor(size_t index)
{
	assert(index >= this->size);
	assert(index < Tmax_size);

	size_t new_size = min(Tmax_size, Align(index + 1, Tgrowth_step));

	this->data = ReallocT(this->data, new_size);
	MemSetT(this->data + this->size, 0, new_size - this->size);

	this->size = new_size;
}

DEFINE_POOL_METHOD(inline size_t)::FindFirstFree()
{
	size_t index = this->first_free;

	for (; index < this->first_unused; index++) {
		if (this->data[index] == NULL) return index;
	}

	if (index < this->size) {
		return index;
	}

	assert(index == this->size);
	assert(this->first_unused == this->size);

	if (index < Tmax_size) {
		this->ResizeFor(index);
		return index;
	}

	assert(this->items == Tmax_size);

	return NO_FREE_ITEM;
}

DEFINE_POOL_METHOD(inline void *)::AllocateItem(size_t size, size_t index)
{
	assert(this->data[index] == NULL);

	this->first_unused = max(this->first_unused, index + 1);
	this->items++;

	Titem *item = this->data[index] = (Titem *)CallocT<byte>(size);
	item->index = (uint)index;
	return item;
}

DEFINE_POOL_METHOD(void *)::GetNew(size_t size)
{
	size_t index = this->FindFirstFree();

	if (index == NO_FREE_ITEM) {
		error("%s: no more free items", this->name);
	}

	this->first_free = index + 1;
	return this->AllocateItem(size, index);
}

DEFINE_POOL_METHOD(void *)::GetNew(size_t size, size_t index)
{
	if (index >= Tmax_size) {
		usererror("failed loading savegame: %s index " PRINTF_SIZE " out of range (" PRINTF_SIZE ")", this->name, index, Tmax_size);
	}

	if (index >= this->size) this->ResizeFor(index);

	if (this->data[index] != NULL) {
		usererror("failed loading savegame: %s index " PRINTF_SIZE " already in use", this->name, index);
	}

	return this->AllocateItem(size, index);
}

DEFINE_POOL_METHOD(void)::FreeItem(size_t index)
{
	assert(index < this->size);
	assert(this->data[index] != NULL);
	free(this->data[index]);
	this->data[index] = NULL;
	this->first_free = min(this->first_free, index);
	this->items--;
}

DEFINE_POOL_METHOD(void)::CleanPool()
{
	this->cleaning = true;
	for (size_t i = 0; i < this->first_unused; i++) {
		delete this->Get(i); // 'delete NULL;' is very valid
	}
	assert(this->items == 0);
	free(this->data);
	this->first_unused = this->first_free = this->size = 0;
	this->data = NULL;
	this->cleaning = false;
}

#undef DEFINE_POOL_METHOD

/**
 * Force instantiation of pool methods so we don't get linker errors.
 * Only methods accessed from methods defined in pool.hpp need to be
 * forcefully instantiated.
 */
#define INSTANTIATE_POOL_METHODS(name) \
	template void * name ## Pool::GetNew(size_t size); \
	template void * name ## Pool::GetNew(size_t size, size_t index); \
	template void name ## Pool::FreeItem(size_t index); \
	template void name ## Pool::CleanPool();

#endif /* POOL_FUNC_HPP */
