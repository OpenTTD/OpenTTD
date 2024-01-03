/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pool_type.hpp Definition of Pool, structure used to access PoolItems, and PoolItem, base structure for Vehicle, Town, and other indexed items. */

#ifndef POOL_TYPE_HPP
#define POOL_TYPE_HPP

#include "enum_type.hpp"

/** Various types of a pool. */
enum PoolType {
	PT_NONE    = 0x00, ///< No pool is selected.
	PT_NORMAL  = 0x01, ///< Normal pool containing game objects.
	PT_NCLIENT = 0x02, ///< Network client pools.
	PT_NADMIN  = 0x04, ///< Network admin pool.
	PT_DATA    = 0x08, ///< NewGRF or other data, that is not reset together with normal pools.
	PT_ALL     = 0x0F, ///< All pool types.
};
DECLARE_ENUM_AS_BIT_SET(PoolType)

typedef std::vector<struct PoolBase *> PoolVector; ///< Vector of pointers to PoolBase

/** Base class for base of all pools. */
struct PoolBase {
	const PoolType type; ///< Type of this pool.

	/**
	 * Function used to access the vector of all pools.
	 * @return pointer to vector of all pools
	 */
	static PoolVector *GetPools()
	{
		static PoolVector *pools = new PoolVector();
		return pools;
	}

	static void Clean(PoolType);

	/**
	 * Constructor registers this object in the pool vector.
	 * @param pt type of this pool.
	 */
	PoolBase(PoolType pt) : type(pt)
	{
		PoolBase::GetPools()->push_back(this);
	}

	virtual ~PoolBase();

	/**
	 * Virtual method that deletes all items in the pool.
	 */
	virtual void CleanPool() = 0;

private:
	/**
	 * Dummy private copy constructor to prevent compilers from
	 * copying the structure, which fails due to GetPools().
	 */
	PoolBase(const PoolBase &other);
};

/**
 * Base class for all pools.
 * @tparam Titem        Type of the class/struct that is going to be pooled
 * @tparam Tindex       Type of the index for this pool
 * @tparam Tgrowth_step Size of growths; if the pool is full increase the size by this amount
 * @tparam Tmax_size    Maximum size of the pool
 * @tparam Tpool_type   Type of this pool
 * @tparam Tcache       Whether to perform 'alloc' caching, i.e. don't actually free/malloc just reuse the memory
 * @tparam Tzero        Whether to zero the memory
 * @warning when Tcache is enabled *all* instances of this pool's item must be of the same size.
 */
template <class Titem, typename Tindex, size_t Tgrowth_step, size_t Tmax_size, PoolType Tpool_type = PT_NORMAL, bool Tcache = false, bool Tzero = true>
struct Pool : PoolBase {
	/* Ensure Tmax_size is within the bounds of Tindex. */
	static_assert((uint64_t)(Tmax_size - 1) >> 8 * sizeof(Tindex) == 0);

	static constexpr size_t MAX_SIZE = Tmax_size; ///< Make template parameter accessible from outside

	const char * const name; ///< Name of this pool

	size_t size;         ///< Current allocated size
	size_t first_free;   ///< No item with index lower than this is free (doesn't say anything about this one!)
	size_t first_unused; ///< This and all higher indexes are free (doesn't say anything about first_unused-1 !)
	size_t items;        ///< Number of used indexes (non-nullptr)
#ifdef WITH_ASSERT
	size_t checked;      ///< Number of items we checked for
#endif /* WITH_ASSERT */
	bool cleaning;       ///< True if cleaning pool (deleting all items)

	Titem **data;        ///< Pointer to array of pointers to Titem

	Pool(const char *name);
	void CleanPool() override;

	/**
	 * Returns Titem with given index
	 * @param index of item to get
	 * @return pointer to Titem
	 * @pre index < this->first_unused
	 */
	inline Titem *Get(size_t index)
	{
		assert(index < this->first_unused);
		return this->data[index];
	}

	/**
	 * Tests whether given index can be used to get valid (non-nullptr) Titem
	 * @param index index to examine
	 * @return true if PoolItem::Get(index) will return non-nullptr pointer
	 */
	inline bool IsValidID(size_t index)
	{
		return index < this->first_unused && this->Get(index) != nullptr;
	}

	/**
	 * Tests whether we can allocate 'n' items
	 * @param n number of items we want to allocate
	 * @return true if 'n' items can be allocated
	 */
	inline bool CanAllocate(size_t n = 1)
	{
		bool ret = this->items <= Tmax_size - n;
#ifdef WITH_ASSERT
		this->checked = ret ? n : 0;
#endif /* WITH_ASSERT */
		return ret;
	}

	/**
	 * Iterator to iterate all valid T of a pool
	 * @tparam T Type of the class/struct that is going to be iterated
	 */
	template <class T>
	struct PoolIterator {
		typedef T value_type;
		typedef T *pointer;
		typedef T &reference;
		typedef size_t difference_type;
		typedef std::forward_iterator_tag iterator_category;

		explicit PoolIterator(size_t index) : index(index)
		{
			this->ValidateIndex();
		};

		bool operator==(const PoolIterator &other) const { return this->index == other.index; }
		bool operator!=(const PoolIterator &other) const { return !(*this == other); }
		T * operator*() const { return T::Get(this->index); }
		PoolIterator & operator++() { this->index++; this->ValidateIndex(); return *this; }

	private:
		size_t index;
		void ValidateIndex()
		{
			while (this->index < T::GetPoolSize() && !(T::IsValidID(this->index))) this->index++;
			if (this->index >= T::GetPoolSize()) this->index = T::Pool::MAX_SIZE;
		}
	};

	/*
	 * Iterable ensemble of all valid T
	 * @tparam T Type of the class/struct that is going to be iterated
	 */
	template <class T>
	struct IterateWrapper {
		size_t from;
		IterateWrapper(size_t from = 0) : from(from) {}
		PoolIterator<T> begin() { return PoolIterator<T>(this->from); }
		PoolIterator<T> end() { return PoolIterator<T>(T::Pool::MAX_SIZE); }
		bool empty() { return this->begin() == this->end(); }
	};

	/**
	 * Iterator to iterate all valid T of a pool
	 * @tparam T Type of the class/struct that is going to be iterated
	 */
	template <class T, class F>
	struct PoolIteratorFiltered {
		typedef T value_type;
		typedef T *pointer;
		typedef T &reference;
		typedef size_t difference_type;
		typedef std::forward_iterator_tag iterator_category;

		explicit PoolIteratorFiltered(size_t index, F filter) : index(index), filter(filter)
		{
			this->ValidateIndex();
		};

		bool operator==(const PoolIteratorFiltered &other) const { return this->index == other.index; }
		bool operator!=(const PoolIteratorFiltered &other) const { return !(*this == other); }
		T * operator*() const { return T::Get(this->index); }
		PoolIteratorFiltered & operator++() { this->index++; this->ValidateIndex(); return *this; }

	private:
		size_t index;
		F filter;
		void ValidateIndex()
		{
			while (this->index < T::GetPoolSize() && !(T::IsValidID(this->index) && this->filter(this->index))) this->index++;
			if (this->index >= T::GetPoolSize()) this->index = T::Pool::MAX_SIZE;
		}
	};

	/*
	 * Iterable ensemble of all valid T
	 * @tparam T Type of the class/struct that is going to be iterated
	 */
	template <class T, class F>
	struct IterateWrapperFiltered {
		size_t from;
		F filter;
		IterateWrapperFiltered(size_t from, F filter) : from(from), filter(filter) {}
		PoolIteratorFiltered<T, F> begin() { return PoolIteratorFiltered<T, F>(this->from, this->filter); }
		PoolIteratorFiltered<T, F> end() { return PoolIteratorFiltered<T, F>(T::Pool::MAX_SIZE, this->filter); }
		bool empty() { return this->begin() == this->end(); }
	};

	/**
	 * Base class for all PoolItems
	 * @tparam Tpool The pool this item is going to be part of
	 */
	template <struct Pool<Titem, Tindex, Tgrowth_step, Tmax_size, Tpool_type, Tcache, Tzero> *Tpool>
	struct PoolItem {
		Tindex index; ///< Index of this pool item

		/** Type of the pool this item is going to be part of */
		typedef struct Pool<Titem, Tindex, Tgrowth_step, Tmax_size, Tpool_type, Tcache, Tzero> Pool;

		/**
		 * Allocates space for new Titem
		 * @param size size of Titem
		 * @return pointer to allocated memory
		 * @note can never fail (return nullptr), use CanAllocate() to check first!
		 */
		inline void *operator new(size_t size)
		{
			return Tpool->GetNew(size);
		}

		/**
		 * Marks Titem as free. Its memory is released
		 * @param p memory to free
		 * @note the item has to be allocated in the pool!
		 */
		inline void operator delete(void *p)
		{
			if (p == nullptr) return;
			Titem *pn = (Titem *)p;
			assert(pn == Tpool->Get(pn->index));
			Tpool->FreeItem(pn->index);
		}

		/**
		 * Allocates space for new Titem with given index
		 * @param size size of Titem
		 * @param index index of item
		 * @return pointer to allocated memory
		 * @note can never fail (return nullptr), use CanAllocate() to check first!
		 * @pre index has to be unused! Else it will crash
		 */
		inline void *operator new(size_t size, size_t index)
		{
			return Tpool->GetNew(size, index);
		}

		/**
		 * Allocates space for new Titem at given memory address
		 * @param ptr where are we allocating the item?
		 * @return pointer to allocated memory (== ptr)
		 * @note use of this is strongly discouraged
		 * @pre the memory must not be allocated in the Pool!
		 */
		inline void *operator new(size_t, void *ptr)
		{
			for (size_t i = 0; i < Tpool->first_unused; i++) {
				/* Don't allow creating new objects over existing.
				 * Even if we called the destructor and reused this memory,
				 * we don't know whether 'size' and size of currently allocated
				 * memory are the same (because of possible inheritance).
				 * Use { size_t index = item->index; delete item; new (index) item; }
				 * instead to make sure destructor is called and no memory leaks. */
				assert(ptr != Tpool->data[i]);
			}
			return ptr;
		}


		/** Helper functions so we can use PoolItem::Function() instead of _poolitem_pool.Function() */

		/**
		 * Tests whether we can allocate 'n' items
		 * @param n number of items we want to allocate
		 * @return true if 'n' items can be allocated
		 */
		static inline bool CanAllocateItem(size_t n = 1)
		{
			return Tpool->CanAllocate(n);
		}

		/**
		 * Returns current state of pool cleaning - yes or no
		 * @return true iff we are cleaning the pool now
		 */
		static inline bool CleaningPool()
		{
			return Tpool->cleaning;
		}

		/**
		 * Tests whether given index can be used to get valid (non-nullptr) Titem
		 * @param index index to examine
		 * @return true if PoolItem::Get(index) will return non-nullptr pointer
		 */
		static inline bool IsValidID(size_t index)
		{
			return Tpool->IsValidID(index);
		}

		/**
		 * Returns Titem with given index
		 * @param index of item to get
		 * @return pointer to Titem
		 * @pre index < this->first_unused
		 */
		static inline Titem *Get(size_t index)
		{
			return Tpool->Get(index);
		}

		/**
		 * Returns Titem with given index
		 * @param index of item to get
		 * @return pointer to Titem
		 * @note returns nullptr for invalid index
		 */
		static inline Titem *GetIfValid(size_t index)
		{
			return index < Tpool->first_unused ? Tpool->Get(index) : nullptr;
		}

		/**
		 * Returns first unused index. Useful when iterating over
		 * all pool items.
		 * @return first unused index
		 */
		static inline size_t GetPoolSize()
		{
			return Tpool->first_unused;
		}

		/**
		 * Returns number of valid items in the pool
		 * @return number of valid items in the pool
		 */
		static inline size_t GetNumItems()
		{
			return Tpool->items;
		}

		/**
		 * Dummy function called after destructor of each member.
		 * If you want to use it, override it in PoolItem's subclass.
		 * @param index index of deleted item
		 * @note when this function is called, PoolItem::Get(index) == nullptr.
		 * @note it's called only when !CleaningPool()
		 */
		static inline void PostDestructor([[maybe_unused]] size_t index) { }

		/**
		 * Returns an iterable ensemble of all valid Titem
		 * @param from index of the first Titem to consider
		 * @return an iterable ensemble of all valid Titem
		 */
		static Pool::IterateWrapper<Titem> Iterate(size_t from = 0) { return Pool::IterateWrapper<Titem>(from); }
	};

private:
	static const size_t NO_FREE_ITEM = MAX_UVALUE(size_t); ///< Constant to indicate we can't allocate any more items

	/**
	 * Helper struct to cache 'freed' PoolItems so we
	 * do not need to allocate them again.
	 */
	struct AllocCache {
		/** The next in our 'cache' */
		AllocCache *next;
	};

	/** Cache of freed pointers */
	AllocCache *alloc_cache;

	void *AllocateItem(size_t size, size_t index);
	void ResizeFor(size_t index);
	size_t FindFirstFree();

	void *GetNew(size_t size);
	void *GetNew(size_t size, size_t index);

	void FreeItem(size_t index);
};

#endif /* POOL_TYPE_HPP */
