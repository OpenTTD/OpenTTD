/* $Id$ */

/** @file yapf_node_rail.hpp */

#ifndef  YAPF_NODE_RAIL_HPP
#define  YAPF_NODE_RAIL_HPP

/** key for cached segment cost for rail YAPF */
struct CYapfRailSegmentKey
{
	uint32    m_value;

	FORCEINLINE CYapfRailSegmentKey(const CYapfRailSegmentKey& src) : m_value(src.m_value) {}
	FORCEINLINE CYapfRailSegmentKey(const CYapfNodeKeyTrackDir& node_key) {Set(node_key);}

	FORCEINLINE void Set(const CYapfRailSegmentKey& src) {m_value = src.m_value;}
	FORCEINLINE void Set(const CYapfNodeKeyTrackDir& node_key) {m_value = (((int)node_key.m_tile) << 3) | node_key.m_td;}

	FORCEINLINE int32 CalcHash() const {return m_value;}
	FORCEINLINE TileIndex GetTile() const {return (TileIndex)(m_value >> 3);}
	FORCEINLINE bool operator == (const CYapfRailSegmentKey& other) const {return m_value == other.m_value;}
};

/* Enum used in PfCalcCost() to see why was the segment closed. */
enum EndSegmentReason {
	/* The following reasons can be saved into cached segment */
	ESR_DEAD_END = 0,      ///< track ends here
	ESR_RAIL_TYPE,         ///< the next tile has a different rail type than our tiles
	ESR_INFINITE_LOOP,     ///< infinite loop detected
	ESR_SEGMENT_TOO_LONG,  ///< the segment is too long (possible infinite loop)
	ESR_CHOICE_FOLLOWS,    ///< the next tile contains a choice (the track splits to more than one segments)
	ESR_DEPOT,             ///< stop in the depot (could be a target next time)
	ESR_WAYPOINT,          ///< waypoint encountered (could be a target next time)
	ESR_STATION,           ///< station encountered (could be a target next time)

	/* The following reasons are used only internally by PfCalcCost().
	*   They should not be found in the cached segment. */
	ESR_PATH_TOO_LONG,     ///< the path is too long (searching for the nearest depot in the given radius)
	ESR_FIRST_TWO_WAY_RED, ///< first signal was 2-way and it was red
	ESR_LOOK_AHEAD_END,    ///< we have just passed the last look-ahead signal
	ESR_TARGET_REACHED,    ///< we have just reached the destination

	/* Special values */
	ESR_NONE = 0xFF,          ///< no reason to end the segment here
};

enum EndSegmentReasonBits {
	ESRB_NONE = 0,

	ESRB_DEAD_END          = 1 << ESR_DEAD_END,
	ESRB_RAIL_TYPE         = 1 << ESR_RAIL_TYPE,
	ESRB_INFINITE_LOOP     = 1 << ESR_INFINITE_LOOP,
	ESRB_SEGMENT_TOO_LONG  = 1 << ESR_SEGMENT_TOO_LONG,
	ESRB_CHOICE_FOLLOWS    = 1 << ESR_CHOICE_FOLLOWS,
	ESRB_DEPOT             = 1 << ESR_DEPOT,
	ESRB_WAYPOINT          = 1 << ESR_WAYPOINT,
	ESRB_STATION           = 1 << ESR_STATION,

	ESRB_PATH_TOO_LONG     = 1 << ESR_PATH_TOO_LONG,
	ESRB_FIRST_TWO_WAY_RED = 1 << ESR_FIRST_TWO_WAY_RED,
	ESRB_LOOK_AHEAD_END    = 1 << ESR_LOOK_AHEAD_END,
	ESRB_TARGET_REACHED    = 1 << ESR_TARGET_REACHED,

	/* Additional (composite) values. */

	/* What reasons mean that the target can be fond and needs to be detected. */
	ESRB_POSSIBLE_TARGET = ESRB_DEPOT | ESRB_WAYPOINT | ESRB_STATION,

	/* What reasons can be stored back into cached segment. */
	ESRB_CACHED_MASK = ESRB_DEAD_END | ESRB_RAIL_TYPE | ESRB_INFINITE_LOOP | ESRB_SEGMENT_TOO_LONG | ESRB_CHOICE_FOLLOWS | ESRB_DEPOT | ESRB_WAYPOINT | ESRB_STATION,

	/* Reasons to abort pathfinding in this direction. */
	ESRB_ABORT_PF_MASK = ESRB_DEAD_END | ESRB_PATH_TOO_LONG | ESRB_INFINITE_LOOP | ESRB_FIRST_TWO_WAY_RED,
};

DECLARE_ENUM_AS_BIT_SET(EndSegmentReasonBits);

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
	EndSegmentReasonBits   m_end_segment_reason;
	CYapfRailSegment*      m_hash_next;

	FORCEINLINE CYapfRailSegment(const CYapfRailSegmentKey& key)
		: m_key(key)
		, m_last_tile(INVALID_TILE)
		, m_last_td(INVALID_TRACKDIR)
		, m_cost(-1)
		, m_last_signal_tile(INVALID_TILE)
		, m_last_signal_td(INVALID_TRACKDIR)
		, m_end_segment_reason(ESRB_NONE)
		, m_hash_next(NULL)
	{}

	FORCEINLINE const Key& GetKey() const {return m_key;}
	FORCEINLINE TileIndex GetTile() const {return m_key.GetTile();}
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
