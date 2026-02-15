/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file pool_type.hpp Definition of Pool, structure used to access PoolItems, and PoolItem, base structure for Vehicle, Town, and other indexed items. */

#ifndef POOL_TYPE_HPP
#define POOL_TYPE_HPP

#include "enum_type.hpp"

/** Various types of a pool. */
enum class PoolType : uint8_t {
	Normal, ///< Normal pool containing game objects.
	NetworkClient, ///< Network client pools.
	NetworkAdmin, ///< Network admin pool.
	Data, ///< NewGRF or other data, that is not reset together with normal pools.
};
using PoolTypes = EnumBitSet<PoolType, uint8_t>;
static constexpr PoolTypes PT_ALL = {PoolType::Normal, PoolType::NetworkClient, PoolType::NetworkAdmin, PoolType::Data};

typedef std::vector<struct PoolBase *> PoolVector; ///< Vector of pointers to PoolBase

template <typename Tindex>
using AllocationResult = std::pair<void *, Tindex>;


/** Non-templated base for #PoolID for use with type trait queries. */
struct PoolIDBase {};

/**
 * Templated helper to make a PoolID a single POD value.
 *
 * Example usage:
 *
 *   using MyType = PoolID<int, struct MyTypeTag, 16, 0xFF>;
 *
 * @tparam TBaseType Type of the derived class (i.e. the concrete usage of this class).
 * @tparam TTag An unique struct to keep types of the same TBaseType distinct.
 * @tparam TEnd The PoolID at the end of the pool (equivalent to size).
 * @tparam TInvalid The PoolID denoting an invalid value.
 */
template <typename TBaseType, typename TTag, TBaseType TEnd, TBaseType TInvalid>
struct EMPTY_BASES PoolID : PoolIDBase {
	using BaseType = TBaseType;

	constexpr PoolID() = default;
	constexpr PoolID(const PoolID &) = default;
	constexpr PoolID(PoolID &&) = default;

	explicit constexpr PoolID(const TBaseType &value) : value(value) {}

	constexpr PoolID &operator =(const PoolID &rhs) { this->value = rhs.value; return *this; }
	constexpr PoolID &operator =(PoolID &&rhs) { this->value = std::move(rhs.value); return *this; }

	/* Only allow conversion to BaseType via method. */
	constexpr TBaseType base() const noexcept { return this->value; }

	static constexpr PoolID Begin() { return PoolID{}; }
	static constexpr PoolID End() { return PoolID{static_cast<TBaseType>(TEnd)}; }
	static constexpr PoolID Invalid() { return PoolID{static_cast<TBaseType>(TInvalid)}; }

	constexpr auto operator++() { ++this->value; return this; }
	constexpr auto operator+(const std::integral auto &val) const { return this->value + val; }
	constexpr auto operator-(const std::integral auto &val) const { return this->value - val; }
	constexpr auto operator%(const std::integral auto &val) const { return this->value % val; }

	constexpr bool operator==(const PoolID<TBaseType, TTag, TEnd, TInvalid> &rhs) const { return this->value == rhs.value; }
	constexpr auto operator<=>(const PoolID<TBaseType, TTag, TEnd, TInvalid> &rhs) const { return this->value <=> rhs.value; }

	constexpr bool operator==(const size_t &rhs) const { return this->value == rhs; }
	constexpr auto operator<=>(const size_t &rhs) const { return this->value <=> rhs; }
private:
	/* Do not explicitly initialize. */
	TBaseType value;
};

template <typename T> requires std::is_base_of_v<PoolIDBase, T>
constexpr auto operator+(const std::integral auto &val, const T &pool_id) { return pool_id + val; }
template <typename Te, typename Tp> requires std::is_enum_v<Te> && std::is_base_of_v<PoolIDBase, Tp>
constexpr auto operator+(const Te &val, const Tp &pool_id) { return pool_id + to_underlying(val); }

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

	static void Clean(PoolTypes);

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
	 * @param other The pool not to copy from.
	 */
	PoolBase(const PoolBase &other);
};

/**
 * Base class for all pools.
 * @tparam Titem        Type of the class/struct that is going to be pooled
 * @tparam Tindex       Type of the index for this pool
 * @tparam Tgrowth_step Size of growths; if the pool is full increase the size by this amount
 * @tparam Tpool_type   Type of this pool
 * @tparam Tcache       Whether to perform 'alloc' caching, i.e. don't actually deallocated/allocate just reuse the memory
 * @warning when Tcache is enabled *all* instances of this pool's item must be of the same size.
 */
template <class Titem, typename Tindex, size_t Tgrowth_step, PoolType Tpool_type = PoolType::Normal, bool Tcache = false>
requires std::is_base_of_v<PoolIDBase, Tindex>
struct Pool : PoolBase {
public:
	static constexpr size_t MAX_SIZE = Tindex::End().base(); ///< Make template parameter accessible from outside

	using IndexType = Tindex;
	using BitmapStorage = size_t;
	static constexpr size_t BITMAP_SIZE = std::numeric_limits<BitmapStorage>::digits;

	const std::string_view name{}; ///< Name of this pool

	size_t first_free = 0; ///< No item with index lower than this is free (doesn't say anything about this one!)
	size_t first_unused = 0; ///< This and all higher indexes are free (doesn't say anything about first_unused-1 !)
	size_t items = 0; ///< Number of used indexes (non-nullptr)
#ifdef WITH_ASSERT
	size_t checked = 0; ///< Number of items we checked for
#endif /* WITH_ASSERT */
	bool cleaning = false; ///< True if cleaning pool (deleting all items)

	std::vector<Titem *> data{}; ///< Pointers to Titem
	std::vector<BitmapStorage> used_bitmap{}; ///< Bitmap of used indices.

	Pool(std::string_view name) : PoolBase(Tpool_type), name(name) {}
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
		bool ret = this->items <= MAX_SIZE - n;
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
	template <struct Pool<Titem, Tindex, Tgrowth_step, Tpool_type, Tcache> *Tpool>
	struct PoolItem {
		const Tindex index; ///< Index of this pool item

		/**
		 * Construct the item.
		 * @param index The index of this PoolItem in the pool.
		 */
		PoolItem(Tindex index) : index(index) {}

		/** Type of the pool this item is going to be part of */
		typedef struct Pool<Titem, Tindex, Tgrowth_step, Tpool_type, Tcache> Pool;

		/** Do not use new PoolItem, but rather PoolItem::Create. */
		inline void *operator new(size_t) = delete;

		/**
		 * Marks Titem as free. Its memory is released
		 * @param p memory to free
		 * @param size The size that was allocated during allocation.
		 * @note the item has to be allocated in the pool!
		 */
		inline void operator delete(void *p, size_t size)
		{
			if (p == nullptr) return;
			Titem *pn = static_cast<Titem *>(p);
			assert(pn == Tpool->Get(Pool::GetRawIndex(pn->index)));
			Tpool->FreeItem(size, Pool::GetRawIndex(pn->index));
		}

		/** Do not use new (index) PoolItem(...), but rather PoolItem::CreateAtIndex(index, ...). */
		inline void *operator new(size_t size, Tindex index) = delete;

		/** Do not use new (address) PoolItem(...). */
		inline void *operator new(size_t, void *ptr) = delete;


		/**
		 * Creates a new T-object in the associated pool.
		 * @param args The arguments to the constructor.
		 * @return The created object.
		 */
		template <typename T = Titem, typename... Targs>
		requires std::is_base_of_v<Titem, T>
		static inline T *Create(Targs &&... args)
		{
			auto [data, index] = Tpool->GetNew(sizeof(T));
			return ::new (data) T(index, std::forward<Targs&&>(args)...);
		}

		/**
		 * Creates a new T-object in the associated pool.
		 * @param index The to allocate the object at.
		 * @param args The arguments to the constructor.
		 * @return The created object.
		 */
		template <typename T = Titem, typename... Targs>
		requires std::is_base_of_v<Titem, T>
		static inline T *CreateAtIndex(Tindex index, Targs &&... args)
		{
			auto [data, _] = Tpool->GetNew(sizeof(T), index.base());
			return ::new (data) T(index, std::forward<Targs&&>(args)...);
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
		static inline bool IsValidID(auto index)
		{
			return Tpool->IsValidID(GetRawIndex(index));
		}

		/**
		 * Returns Titem with given index
		 * @param index of item to get
		 * @return pointer to Titem
		 * @pre index < this->first_unused
		 */
		static inline Titem *Get(auto index)
		{
			return Tpool->Get(GetRawIndex(index));
		}

		/**
		 * Returns Titem with given index
		 * @param index of item to get
		 * @return pointer to Titem
		 * @note returns nullptr for invalid index
		 */
		static inline Titem *GetIfValid(auto index)
		{
			return GetRawIndex(index) < Tpool->first_unused ? Tpool->Get(GetRawIndex(index)) : nullptr;
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
	static const size_t NO_FREE_ITEM = std::numeric_limits<size_t>::max(); ///< Constant to indicate we can't allocate any more items

	/**
	 * Helper struct to cache 'freed' PoolItems so we
	 * do not need to allocate them again.
	 */
	struct AllocCache {
		/** The next in our 'cache' */
		AllocCache *next;
	};

	/** Cache of freed pointers */
	AllocCache *alloc_cache = nullptr;
	std::allocator<uint8_t> allocator{};

	AllocationResult<Tindex> AllocateItem(size_t size, size_t index);
	void ResizeFor(size_t index);
	size_t FindFirstFree();

	AllocationResult<Tindex> GetNew(size_t size);
	AllocationResult<Tindex> GetNew(size_t size, size_t index);

	void FreeItem(size_t size, size_t index);

	static constexpr size_t GetRawIndex(size_t index) { return index; }
	template <typename T> requires std::is_base_of_v<PoolIDBase, T>
	static constexpr size_t GetRawIndex(const T &index) { return index.base(); }
};

#endif /* POOL_TYPE_HPP */
