/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pool_func.hpp Some methods of Pool are placed here in order to reduce compilation time and binary size. */

#ifndef POOL_FUNC_HPP
#define POOL_FUNC_HPP

#include "bitmath_func.hpp"
#include "math_func.hpp"
#include "pool_type.hpp"
#include "../error_func.h"

#include "../saveload/saveload_error.hpp" // SlErrorCorruptFmt

/**
 * Helper for defining the method's signature.
 * @param type The return type of the method.
 */
#define DEFINE_POOL_METHOD(type) \
	template <class Titem, typename Tindex, size_t Tgrowth_step, PoolType Tpool_type, bool Tcache> \
	requires std::is_base_of_v<PoolIDBase, Tindex> \
	type Pool<Titem, Tindex, Tgrowth_step, Tpool_type, Tcache>

/**
 * Resizes the pool so 'index' can be addressed
 * @param index index we will allocate later
 * @pre index >= this->size
 * @pre index < MAX_SIZE
 */
DEFINE_POOL_METHOD(inline void)::ResizeFor(size_t index)
{
	assert(index >= this->data.size());
	assert(index < MAX_SIZE);

	size_t old_size = this->data.size();
	size_t new_size = std::min(MAX_SIZE, Align(index + 1, Tgrowth_step));

	this->data.resize(new_size);
	this->used_bitmap.resize(Align(new_size, BITMAP_SIZE) / BITMAP_SIZE);
	if (old_size % BITMAP_SIZE != 0) {
		/* Already-allocated bits above old size are now unused. */
		this->used_bitmap[old_size / BITMAP_SIZE] &= ~((~static_cast<BitmapStorage>(0)) << (old_size % BITMAP_SIZE));
	}
	if (new_size % BITMAP_SIZE != 0) {
		/* Bits above new size are considered used. */
		this->used_bitmap[new_size / BITMAP_SIZE] |= (~static_cast<BitmapStorage>(0)) << (new_size % BITMAP_SIZE);
	}
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

	assert(this->first_unused == this->data.size());

	if (this->first_unused < MAX_SIZE) {
		this->ResizeFor(this->first_unused);
		return this->first_unused;
	}

	assert(this->first_unused == MAX_SIZE);

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
		item = reinterpret_cast<Titem *>(this->alloc_cache);
		this->alloc_cache = this->alloc_cache->next;
	} else {
		item = reinterpret_cast<Titem *>(this->allocator.allocate(size));
	}
	this->data[index] = item;
	SetBit(this->used_bitmap[index / BITMAP_SIZE], index % BITMAP_SIZE);
	/* MSVC complains about casting to narrower type, so first cast to the base type... then to the strong type. */
	item->index = static_cast<Tindex>(static_cast<Tindex::BaseType>(index));
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
	if (index >= MAX_SIZE) {
		SlErrorCorruptFmt("{} index {} out of range ({})", this->name, index, MAX_SIZE);
	}

	if (index >= this->data.size()) this->ResizeFor(index);

	if (this->data[index] != nullptr) {
		SlErrorCorruptFmt("{} index {} already in use", this->name, index);
	}

	return this->AllocateItem(size, index);
}

/**
 * Deallocates memory used by this index and marks item as free
 * @param size the size of the freed object
 * @param index item to deallocate
 * @pre unit is allocated (non-nullptr)
 * @note 'delete nullptr' doesn't cause call of this function, so it is safe
 */
DEFINE_POOL_METHOD(void)::FreeItem(size_t size, size_t index)
{
	assert(index < this->data.size());
	assert(this->data[index] != nullptr);
	if (Tcache) {
		AllocCache *ac = reinterpret_cast<AllocCache *>(this->data[index]);
		ac->next = this->alloc_cache;
		this->alloc_cache = ac;
	} else {
		this->allocator.deallocate(reinterpret_cast<uint8_t*>(this->data[index]), size);
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
	this->data.clear();
	this->data.shrink_to_fit();
	this->used_bitmap.clear();
	this->used_bitmap.shrink_to_fit();
	this->first_unused = this->first_free = 0;
	this->cleaning = false;

	if (Tcache) {
		while (this->alloc_cache != nullptr) {
			AllocCache *ac = this->alloc_cache;
			this->alloc_cache = ac->next;
			this->allocator.deallocate(reinterpret_cast<uint8_t*>(ac), sizeof(Titem));
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
	template void name ## Pool::FreeItem(size_t size, size_t index); \
	template void name ## Pool::CleanPool();

#endif /* POOL_FUNC_HPP */
