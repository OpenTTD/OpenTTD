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
#include "bitmath_func.hpp"
#include "mem_func.hpp"
#include "pool_type.hpp"
#include "../error_func.h"

#include "../saveload/saveload_error.hpp" // SlErrorCorruptFmt

/**
 * Helper for defining the method's signature.
 * @param type The return type of the method.
 */
#define DEFINE_POOL_METHOD(type) \
	template <class Titem, typename Tindex, size_t Tgrowth_step, size_t Tmax_size, PoolType Tpool_type, bool Tcache, bool Tzero> \
	type Pool<Titem, Tindex, Tgrowth_step, Tmax_size, Tpool_type, Tcache, Tzero>

/**
 * Create a clean pool.
 * @param name The name for the pool.
 */
DEFINE_POOL_METHOD(inline)::Pool(const char *name) :
		PoolBase(Tpool_type),
		name(name),
		size(0),
		first_free(0),
		first_unused(0),
		items(0),
#ifdef WITH_ASSERT
		checked(0),
#endif /* WITH_ASSERT */
		cleaning(false),
		data(nullptr),
		alloc_cache(nullptr)
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

	size_t new_size = std::min(Tmax_size, Align(index + 1, Tgrowth_step));

	this->data = ReallocT(this->data, new_size);
	MemSetT(this->data + this->size, 0, new_size - this->size);

	this->used_bitmap.resize(Align(new_size, BITMAP_SIZE) / BITMAP_SIZE);
	if (this->size % BITMAP_SIZE != 0) {
		/* Already-allocated bits above old size are now unused. */
		this->used_bitmap[this->size / BITMAP_SIZE] &= ~((~static_cast<BitmapStorage>(0)) << (this->size % BITMAP_SIZE));
	}
	if (new_size % BITMAP_SIZE != 0) {
		/* Bits above new size are considered used. */
		this->used_bitmap[new_size / BITMAP_SIZE] |= (~static_cast<BitmapStorage>(0)) << (new_size % BITMAP_SIZE);
	}

	this->size = new_size;
}

/**
 * Searches for first free index
 * @return first free index, NO_FREE_ITEM on failure
 */
DEFINE_POOL_METHOD(inline size_t)::FindFirstFree()
{
	for (auto it = std::next(std::begin(this->used_bitmap), this->first_free / BITMAP_SIZE); it != std::end(this->used_bitmap); ++it) {
		BitmapStorage available = ~(*it);
		if (available == 0) continue;
		return std::distance(std::begin(this->used_bitmap), it) * BITMAP_SIZE + FindFirstBit(available);
	}

	assert(this->first_unused == this->size);

	if (this->first_unused < Tmax_size) {
		this->ResizeFor(this->first_unused);
		return this->first_unused;
	}

	assert(this->first_unused == Tmax_size);

	return NO_FREE_ITEM;
}

/**
 * Makes given index valid
 * @param size size of item
 * @param index index of item
 * @pre index < this->size
 * @pre this->Get(index) == nullptr
 */
DEFINE_POOL_METHOD(inline void *)::AllocateItem(size_t size, size_t index)
{
	assert(this->data[index] == nullptr);

	this->first_unused = std::max(this->first_unused, index + 1);
	this->items++;

	Titem *item;
	if (Tcache && this->alloc_cache != nullptr) {
		assert(sizeof(Titem) == size);
		item = (Titem *)this->alloc_cache;
		this->alloc_cache = this->alloc_cache->next;
		if (Tzero) {
			/* Explicitly casting to (void *) prevents a clang warning -
			 * we are actually memsetting a (not-yet-constructed) object */
			memset((void *)item, 0, sizeof(Titem));
		}
	} else if (Tzero) {
		item = (Titem *)CallocT<uint8_t>(size);
	} else {
		item = (Titem *)MallocT<uint8_t>(size);
	}
	this->data[index] = item;
	SetBit(this->used_bitmap[index / BITMAP_SIZE], index % BITMAP_SIZE);
	item->index = (Tindex)(uint)index;
	return item;
}

/**
 * Allocates new item
 * @param size size of item
 * @return pointer to allocated item
 * @note FatalError() on failure! (no free item)
 */
DEFINE_POOL_METHOD(void *)::GetNew(size_t size)
{
	size_t index = this->FindFirstFree();

#ifdef WITH_ASSERT
	assert(this->checked != 0);
	this->checked--;
#endif /* WITH_ASSERT */
	if (index == NO_FREE_ITEM) {
		FatalError("{}: no more free items", this->name);
	}

	this->first_free = index + 1;
	return this->AllocateItem(size, index);
}

/**
 * Allocates new item with given index
 * @param size size of item
 * @param index index of item
 * @return pointer to allocated item
 * @note SlErrorCorruptFmt() on failure! (index out of range or already used)
 */
DEFINE_POOL_METHOD(void *)::GetNew(size_t size, size_t index)
{
	if (index >= Tmax_size) {
		SlErrorCorruptFmt("{} index {} out of range ({})", this->name, index, Tmax_size);
	}

	if (index >= this->size) this->ResizeFor(index);

	if (this->data[index] != nullptr) {
		SlErrorCorruptFmt("{} index {} already in use", this->name, index);
	}

	return this->AllocateItem(size, index);
}

/**
 * Deallocates memory used by this index and marks item as free
 * @param index item to deallocate
 * @pre unit is allocated (non-nullptr)
 * @note 'delete nullptr' doesn't cause call of this function, so it is safe
 */
DEFINE_POOL_METHOD(void)::FreeItem(size_t index)
{
	assert(index < this->size);
	assert(this->data[index] != nullptr);
	if (Tcache) {
		AllocCache *ac = (AllocCache *)this->data[index];
		ac->next = this->alloc_cache;
		this->alloc_cache = ac;
	} else {
		free(this->data[index]);
	}
	this->data[index] = nullptr;
	this->first_free = std::min(this->first_free, index);
	this->items--;
	if (!this->cleaning) {
		ClrBit(this->used_bitmap[index / BITMAP_SIZE], index % BITMAP_SIZE);
		Titem::PostDestructor(index);
	}
}

/** Destroys all items in the pool and resets all member variables. */
DEFINE_POOL_METHOD(void)::CleanPool()
{
	this->cleaning = true;
	for (size_t i = 0; i < this->first_unused; i++) {
		delete this->Get(i); // 'delete nullptr;' is very valid
	}
	assert(this->items == 0);
	free(this->data);
	this->used_bitmap.clear();
	this->used_bitmap.shrink_to_fit();
	this->first_unused = this->first_free = this->size = 0;
	this->data = nullptr;
	this->cleaning = false;

	if (Tcache) {
		while (this->alloc_cache != nullptr) {
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
