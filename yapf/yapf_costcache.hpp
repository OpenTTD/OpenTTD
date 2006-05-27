/* $Id$ */
#ifndef  YAPF_COSTCACHE_HPP
#define  YAPF_COSTCACHE_HPP


/** CYapfSegmentCostCacheNoneT - the formal only yapf cost cache provider that implements
PfNodeCacheFetch() and PfNodeCacheFlush() callbacks. Used when nodes don't have CachedData
defined (they don't count with any segment cost caching).
*/
template <class Types>
class CYapfSegmentCostCacheNoneT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type

	FORCEINLINE bool PfNodeCacheFetch(Node& n)
	{
		return false;
	};

	FORCEINLINE void PfNodeCacheFlush(Node& n)
	{
	};
};


/** CYapfSegmentCostCacheLocalT - the yapf cost cache provider that implements fake segment
cost caching functionality for yapf. Used when node needs caching, but you don't want to
cache the segment costs.
*/
template <class Types>
class CYapfSegmentCostCacheLocalT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables
	typedef typename Node::CachedData CachedData;
	typedef typename CachedData::Key CacheKey;
	typedef CArrayT<CachedData> LocalCache;

protected:
	LocalCache      m_local_cache;

	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	FORCEINLINE bool PfNodeCacheFetch(Node& n)
	{
		CacheKey key(n.GetKey());
		Yapf().ConnectNodeToCachedData(n, *new (&m_local_cache.AddNC()) CachedData(key));
		return false;
	};

	FORCEINLINE void PfNodeCacheFlush(Node& n)
	{
	};
};



struct CSegmentCostCacheBase
{
	static int   s_rail_change_counter;

	static void NotifyTrackLayoutChange(TileIndex tile, Track track) {s_rail_change_counter++;}
};


/** CSegmentCostCacheT - template class providing hash-map and storage (heap)
    of Tsegment structures. Each rail node contains pointer to the segment
    that contains cached (or non-cached) segment cost information. Nodes can
    differ by key type, but they use the same segment type. Segment key should
    be always the same (TileIndex + DiagDirection) that represent the beginning
    of the segment (origin tile and exit-dir from this tile).
    Different CYapfCachedCostT types can share the same type of CSegmentCostCacheT.
    Look at CYapfRailSegment (yapf_node_rail.hpp) for the segment example */

template <class Tsegment>
struct CSegmentCostCacheT
	: public CSegmentCostCacheBase
{
	enum {c_hash_bits = 14};

	typedef CHashTableT<Tsegment, c_hash_bits> HashTable;
	typedef CArrayT<Tsegment> Heap;
	typedef typename Tsegment::Key Key;    ///< key to hash table

	HashTable    m_map;
	Heap         m_heap;

	FORCEINLINE CSegmentCostCacheT() {}

	FORCEINLINE Tsegment& Get(Key& key, bool *found)
	{
		Tsegment* item = &m_map.Find(key);
		if (item == NULL) {
			*found = false;
			item = new (&m_heap.AddNC()) Tsegment(key);
			m_map.Push(*item);
		} else {
			*found = true;
		}
		return *item;
	}
};

/** CYapfSegmentCostCacheGlobalT - the yapf cost cache provider that adds the segment cost
    caching functionality to yapf. Using this class as base of your will provide the global
		segment cost caching services for your Nodes.
*/
template <class Types>
class CYapfSegmentCostCacheGlobalT
	: public CYapfSegmentCostCacheLocalT<Types>
{
public:
	typedef CYapfSegmentCostCacheLocalT<Types> Tlocal;
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables
	typedef typename Node::CachedData CachedData;
	typedef typename CachedData::Key CacheKey;
	typedef CSegmentCostCacheT<CachedData> Cache;

protected:
	Cache&      m_global_cache;

	FORCEINLINE CYapfSegmentCostCacheGlobalT() : m_global_cache(stGetGlobalCache()) {};

	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	FORCEINLINE static Cache*& stGlobalCachePtr() {static Cache* pC = NULL; return pC;}

	FORCEINLINE static Cache& stGetGlobalCache()
	{
		static int last_rail_change_counter = 0;
		static uint32 last_day = 0;

		// some statistics
		if (last_day != _date) {
			last_day = _date;
			DEBUG(yapf, 1)("pf time today:%5d ms\n", _total_pf_time_us / 1000);
			_total_pf_time_us = 0;
		}

		Cache*& pC = stGlobalCachePtr();

		// delete the cache sometimes...
		if (pC != NULL && last_rail_change_counter != Cache::s_rail_change_counter) {
			last_rail_change_counter = Cache::s_rail_change_counter;
			delete pC;
			pC = NULL;
		}

		if (pC == NULL)
			pC = new Cache();
		return *pC;
	}

public:
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
	};

	FORCEINLINE void PfNodeCacheFlush(Node& n)
	{
	};

};




#endif /* YAPF_COSTCACHE_HPP */
