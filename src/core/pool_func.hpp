/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pool_func.hpp Some methods of Pool are placed here in order to reduce compilation time and binary size. */

#ifndef POOL_FUNC_HPP
#define POOL_FUNC_HPP

#include "alloc_func.hpp"
#include "mem_func.hpp"
#include "pool_type.hpp"

#define DEFINE_POOL_METHOD(type) \
	template <class Titem, typename Tindex, size_t Tgrowth_step, size_t Tmax_size, bool Tcache, bool Tzero> \
	type Pool<Titem, Tindex, Tgrowth_step, Tmax_size, Tcache, Tzero>

/**
 * Create a clean pool.
 * @param name The name for the pool.
 */
DEFINE_POOL_METHOD(inline)::Pool(const char *name) :
		name(name),
		size(0),
		first_free(0),
		first_unused(0),
		items(0),
		cleaning(false),
		data(NULL),
		alloc_cache(NULL)
{ }

/**
 * Resizes the pool so 'index' can be addressed
 * @param index index we will allocate later
 * @pre index >= this->size
 * @pre index < Tmax_size
 */
DEFINE_POOL_METHOD(inline void)::ResizeFor(size_t index)
{
	assert(index >= this->size);
	assert(index < Tmax_size);

	size_t new_size = min(Tmax_size, Align(index + 1, Tgrowth_step));

	this->data = ReallocT(this->data, new_size);
	MemSetT(this->data + this->size, 0, new_size - this->size);

	this->size = new_size;
}

/**
 * Searches for first free index
 * @return first free index, NO_FREE_ITEM on failure
 */
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

/**
 * Makes given index valid
 * @param size size of item
 * @param index index of item
 * @pre index < this->size
 * @pre this->Get(index) == NULL
 */
DEFINE_POOL_METHOD(inline void *)::AllocateItem(size_t size, size_t index)
{
	assert(this->data[index] == NULL);

	this->first_unused = max(this->first_unused, index + 1);
	this->items++;

	Titem *item;
	if (Tcache && this->alloc_cache != NULL) {
		assert(sizeof(Titem) == size);
		item = (Titem *)this->alloc_cache;
		this->alloc_cache = this->alloc_cache->next;
		if (Tzero) MemSetT(item, 0);
	} else if (Tzero) {
		item = (Titem *)CallocT<byte>(size);
	} else {
		item = (Titem *)MallocT<byte>(size);
	}
	this->data[index] = item;
	item->index = (uint)index;
	return item;
}

/**
 * Allocates new item
 * @param size size of item
 * @return pointer to allocated item
 * @note error() on failure! (no free item)
 */
DEFINE_POOL_METHOD(void *)::GetNew(size_t size)
{
	size_t index = this->FindFirstFree();

	if (index == NO_FREE_ITEM) {
		error("%s: no more free items", this->name);
	}

	this->first_free = index + 1;
	return this->AllocateItem(size, index);
}

/**
 * Allocates new item with given index
 * @param size size of item
 * @param index index of item
 * @return pointer to allocated item
 * @note usererror() on failure! (index out of range or already used)
 */
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

/**
 * Deallocates memory used by this index and marks item as free
 * @param index item to deallocate
 * @pre unit is allocated (non-NULL)
 * @note 'delete NULL' doesn't cause call of this function, so it is safe
 */
DEFINE_POOL_METHOD(void)::FreeItem(size_t index)
{
	assert(index < this->size);
	assert(this->data[index] != NULL);
	if (Tcache) {
		AllocCache *ac = (AllocCache *)this->data[index];
		ac->next = this->alloc_cache;
		this->alloc_cache = ac;
	} else {
		free(this->data[index]);
	}
	this->data[index] = NULL;
	this->first_free = min(this->first_free, index);
	this->items--;
	if (!this->cleaning) Titem::PostDestructor(index);
}

/** Destroys all items in the pool and resets all member variables. */
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

	if (Tcache) {
		while (this->alloc_cache != NULL) {
			AllocCache *ac = this->alloc_cache;
			this->alloc_cache = ac->next;
			free(ac);
		}
	}
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
