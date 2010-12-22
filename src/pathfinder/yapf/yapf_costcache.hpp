/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_costcache.hpp Caching of segment costs. */

#ifndef YAPF_COSTCACHE_HPP
#define YAPF_COSTCACHE_HPP

#include "../../date_func.h"

/**
 * CYapfSegmentCostCacheNoneT - the formal only yapf cost cache provider that implements
 * PfNodeCacheFetch() and PfNodeCacheFlush() callbacks. Used when nodes don't have CachedData
 * defined (they don't count with any segment cost caching).
 */
template <class Types>
class CYapfSegmentCostCacheNoneT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type

	/**
	 * Called by YAPF to attach cached or local segment cost data to the given node.
	 *  @return true if globally cached data were used or false if local data was used
	 */
	FORCEINLINE bool PfNodeCacheFetch(Node& n)
	{
		return false;
	}

	/**
	 * Called by YAPF to flush the cached segment cost data back into cache storage.
	 *  Current cache implementation doesn't use that.
	 */
	FORCEINLINE void PfNodeCacheFlush(Node& n)
	{
	}
};


/**
 * CYapfSegmentCostCacheLocalT - the yapf cost cache provider that implements fake segment
 * cost caching functionality for yapf. Used when node needs caching, but you don't want to
 * cache the segment costs.
 */
template <class Types>
class CYapfSegmentCostCacheLocalT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables
	typedef typename Node::CachedData CachedData;
	typedef typename CachedData::Key CacheKey;
	typedef SmallArray<CachedData> LocalCache;

protected:
	LocalCache      m_local_cache;

	/** to access inherited path finder */
	FORCEINLINE Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	/**
	 * Called by YAPF to attach cached or local segment cost data to the given node.
	 *  @return true if globally cached data were used or false if local data was used
	 */
	FORCEINLINE bool PfNodeCacheFetch(Node& n)
	{
		CacheKey key(n.GetKey());
		Yapf().ConnectNodeToCachedData(n, *new (m_local_cache.Append()) CachedData(key));
		return false;
	}

	/**
	 * Called by YAPF to flush the cached segment cost data back into cache storage.
	 *  Current cache implementation doesn't use that.
	 */
	FORCEINLINE void PfNodeCacheFlush(Node& n)
	{
	}
};


/**
 * Base class for segment cost cache providers. Contains global counter
 *  of track layout changes and static notification function called whenever
 *  the track layout changes. It is implemented as base class because it needs
 *  to be shared between all rail YAPF types (one shared counter, one notification
 *  function.
 */
struct CSegmentCostCacheBase
{
	static int   s_rail_change_counter;

	static void NotifyTrackLayoutChange(TileIndex tile, Track track)
	{
		s_rail_change_counter++;
	}
};


/**
 * CSegmentCostCacheT - template class providing hash-map and storage (heap)
 *  of Tsegment structures. Each rail node contains pointer to the segment
 *  that contains cached (or non-cached) segment cost information. Nodes can
 *  differ by key type, but they use the same segment type. Segment key should
 *  be always the same (TileIndex + DiagDirection) that represent the beginning
 *  of the segment (origin tile and exit-dir from this tile).
 *  Different CYapfCachedCostT types can share the same type of CSegmentCostCacheT.
 *  Look at CYapfRailSegment (yapf_node_rail.hpp) for the segment example
 */
template <class Tsegment>
struct CSegmentCostCacheT
	: public CSegmentCostCacheBase
{
	static const int C_HASH_BITS = 14;

	typedef CHashTableT<Tsegment, C_HASH_BITS> HashTable;
	typedef SmallArray<Tsegment> Heap;
	typedef typename Tsegment::Key Key;    ///< key to hash table

	HashTable    m_map;
	Heap         m_heap;

	FORCEINLINE CSegmentCostCacheT() {}

	/** flush (clear) the cache */
	FORCEINLINE void Flush()
	{
		m_map.Clear();
		m_heap.Clear();
	}

	FORCEINLINE Tsegment& Get(Key& key, bool *found)
	{
		Tsegment *item = m_map.Find(key);
		if (item == NULL) {
			*found = false;
			item = new (m_heap.Append()) Tsegment(key);
			m_map.Push(*item);
		} else {
			*found = true;
		}
		return *item;
	}
};

/**
 * CYapfSegmentCostCacheGlobalT - the yapf cost cache provider that adds the segment cost
 *  caching functionality to yapf. Using this class as base of your will provide the global
 *  segment cost caching services for your Nodes.
 */
template <class Types>
class CYapfSegmentCostCacheGlobalT
	: public CYapfSegmentCostCacheLocalT<Types>
{
public:
	typedef CYapfSegmentCostCacheLocalT<Types> Tlocal;
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables
	typedef typename Node::CachedData CachedData;
	typedef typename CachedData::Key CacheKey;
	typedef CSegmentCostCacheT<CachedData> Cache;

protected:
	Cache&      m_global_cache;

	FORCEINLINE CYapfSegmentCostCacheGlobalT() : m_global_cache(stGetGlobalCache()) {};

	/** to access inherited path finder */
	FORCEINLINE Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

	FORCEINLINE static Cache& stGetGlobalCache()
	{
		static int last_rail_change_counter = 0;
		static Date last_date = 0;
		static Cache C;

		/* some statistics */
		if (last_date != _date) {
			last_date = _date;
			DEBUG(yapf, 2, "Pf time today: %5d ms", _total_pf_time_us / 1000);
			_total_pf_time_us = 0;
		}

		/* delete the cache sometimes... */
		if (last_rail_change_counter != Cache::s_rail_change_counter) {
			last_rail_change_counter = Cache::s_rail_change_counter;
			C.Flush();
		}
		return C;
	}

public:
	/**
	 * Called by YAPF to attach cached or local segment cost data to the given node.
	 *  @return true if globally cached data were used or false if local data was used
	 */
	FORCEINLINE bool PfNodeCacheFetch(Node& n)
	{
		if (!Yapf().CanUseGlobalCache(n)) {
			return Tlocal::PfNodeCacheFetch(n);
		}
		CacheKey key(n.GetKey());
		bool found;
		CachedData& item = m_global_cache.Get(key, &found);
		Yapf().ConnectNodeToCachedData(n, item);
		return found;
	}

	/**
	 * Called by YAPF to flush the cached segment cost data back into cache storage.
	 *  Current cache implementation doesn't use that.
	 */
	FORCEINLINE void PfNodeCacheFlush(Node& n)
	{
	}
};

#endif /* YAPF_COSTCACHE_HPP */
