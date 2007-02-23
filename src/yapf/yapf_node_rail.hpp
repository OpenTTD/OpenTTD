/* $Id$ */

/** @file yapf_node_rail.hpp */

#ifndef  YAPF_NODE_RAIL_HPP
#define  YAPF_NODE_RAIL_HPP

/** key for cached segment cost for rail YAPF */
struct CYapfRailSegmentKey
{
	uint32    m_value;

	FORCEINLINE CYapfRailSegmentKey(const CYapfRailSegmentKey& src) : m_value(src.m_value) {}
	FORCEINLINE CYapfRailSegmentKey(const CYapfNodeKeyExitDir& node_key) {Set(node_key);}

	FORCEINLINE void Set(const CYapfRailSegmentKey& src) {m_value = src.m_value;}
	FORCEINLINE void Set(const CYapfNodeKeyExitDir& node_key) {m_value = (((int)node_key.m_tile) << 2) | node_key.m_exitdir;}

	FORCEINLINE int32 CalcHash() const {return m_value;}
	FORCEINLINE TileIndex GetTile() const {return (TileIndex)(m_value >> 2);}
	FORCEINLINE DiagDirection GetExitDir() const {return (DiagDirection)(m_value & 3);}
	FORCEINLINE bool operator == (const CYapfRailSegmentKey& other) const {return m_value == other.m_value;}
};

/** cached segment cost for rail YAPF */
struct CYapfRailSegment
{
	typedef CYapfRailSegmentKey Key;

	CYapfRailSegmentKey    m_key;
	TileIndex              m_last_tile;
	Trackdir               m_last_td;
	int                    m_cost;
	TileIndex              m_last_signal_tile;
	Trackdir               m_last_signal_td;
	CYapfRailSegment*      m_hash_next;
	union {
		byte                 m_flags;
		struct {
			bool                   m_end_of_line : 1;
		} flags_s;
	} flags_u;
	byte m_reserve[3];

	FORCEINLINE CYapfRailSegment(const CYapfRailSegmentKey& key)
		: m_key(key)
		, m_last_tile(INVALID_TILE)
		, m_last_td(INVALID_TRACKDIR)
		, m_cost(-1)
		, m_last_signal_tile(INVALID_TILE)
		, m_last_signal_td(INVALID_TRACKDIR)
		, m_hash_next(NULL)
	{
		flags_u.m_flags = 0;
	}

	FORCEINLINE const Key& GetKey() const {return m_key;}
	FORCEINLINE TileIndex GetTile() const {return m_key.GetTile();}
	FORCEINLINE DiagDirection GetExitDir() const {return m_key.GetExitDir();}
	FORCEINLINE CYapfRailSegment* GetHashNext() {return m_hash_next;}
	FORCEINLINE void SetHashNext(CYapfRailSegment* next) {m_hash_next = next;}
};

/** Yapf Node for rail YAPF */
template <class Tkey_>
struct CYapfRailNodeT
	: CYapfNodeT<Tkey_, CYapfRailNodeT<Tkey_> >
{
	typedef CYapfNodeT<Tkey_, CYapfRailNodeT<Tkey_> > base;
	typedef CYapfRailSegment CachedData;

	CYapfRailSegment *m_segment;
	uint16            m_num_signals_passed;
	union {
		uint32          m_inherited_flags;
		struct {
			bool          m_targed_seen : 1;
			bool          m_choice_seen : 1;
			bool          m_last_signal_was_red : 1;
		} flags_s;
	} flags_u;
	SignalType        m_last_red_signal_type;

	FORCEINLINE void Set(CYapfRailNodeT* parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		base::Set(parent, tile, td, is_choice);
		m_segment = NULL;
		if (parent == NULL) {
			m_num_signals_passed      = 0;
			flags_u.m_inherited_flags = 0;
			m_last_red_signal_type    = SIGTYPE_NORMAL;
		} else {
			m_num_signals_passed      = parent->m_num_signals_passed;
			flags_u.m_inherited_flags = parent->flags_u.m_inherited_flags;
			m_last_red_signal_type    = parent->m_last_red_signal_type;
		}
		flags_u.flags_s.m_choice_seen |= is_choice;
	}

	FORCEINLINE TileIndex GetLastTile() const {assert(m_segment != NULL); return m_segment->m_last_tile;}
	FORCEINLINE Trackdir GetLastTrackdir() const {assert(m_segment != NULL); return m_segment->m_last_td;}
	FORCEINLINE void SetLastTileTrackdir(TileIndex tile, Trackdir td) {assert(m_segment != NULL); m_segment->m_last_tile = tile; m_segment->m_last_td = td;}
};

// now define two major node types (that differ by key type)
typedef CYapfRailNodeT<CYapfNodeKeyExitDir>  CYapfRailNodeExitDir;
typedef CYapfRailNodeT<CYapfNodeKeyTrackDir> CYapfRailNodeTrackDir;

// Default NodeList types
typedef CNodeList_HashTableT<CYapfRailNodeExitDir , 10, 12> CRailNodeListExitDir;
typedef CNodeList_HashTableT<CYapfRailNodeTrackDir, 12, 16> CRailNodeListTrackDir;



#endif /* YAPF_NODE_RAIL_HPP */
