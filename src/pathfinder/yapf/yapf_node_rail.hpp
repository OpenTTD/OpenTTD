/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_node_rail.hpp Node tailored for rail pathfinding. */

#ifndef YAPF_NODE_RAIL_HPP
#define YAPF_NODE_RAIL_HPP

/** key for cached segment cost for rail YAPF */
struct CYapfRailSegmentKey
{
	uint32_t    m_value;

	inline CYapfRailSegmentKey(const CYapfNodeKeyTrackDir &node_key)
	{
		Set(node_key);
	}

	inline void Set(const CYapfRailSegmentKey &src)
	{
		m_value = src.m_value;
	}

	inline void Set(const CYapfNodeKeyTrackDir &node_key)
	{
		m_value = (node_key.m_tile.base() << 4) | node_key.m_td;
	}

	inline int32_t CalcHash() const
	{
		return m_value;
	}

	inline TileIndex GetTile() const
	{
		return (TileIndex)(m_value >> 4);
	}

	inline Trackdir GetTrackdir() const
	{
		return (Trackdir)(m_value & 0x0F);
	}

	inline bool operator==(const CYapfRailSegmentKey &other) const
	{
		return m_value == other.m_value;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteTile("tile", GetTile());
		dmp.WriteEnumT("td", GetTrackdir());
	}
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
	EndSegmentReasonBits   m_end_segment_reason;
	CYapfRailSegment      *m_hash_next;

	inline CYapfRailSegment(const CYapfRailSegmentKey &key)
		: m_key(key)
		, m_last_tile(INVALID_TILE)
		, m_last_td(INVALID_TRACKDIR)
		, m_cost(-1)
		, m_last_signal_tile(INVALID_TILE)
		, m_last_signal_td(INVALID_TRACKDIR)
		, m_end_segment_reason(ESRB_NONE)
		, m_hash_next(nullptr)
	{}

	inline const Key &GetKey() const
	{
		return m_key;
	}

	inline TileIndex GetTile() const
	{
		return m_key.GetTile();
	}

	inline CYapfRailSegment *GetHashNext()
	{
		return m_hash_next;
	}

	inline void SetHashNext(CYapfRailSegment *next)
	{
		m_hash_next = next;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("m_key", &m_key);
		dmp.WriteTile("m_last_tile", m_last_tile);
		dmp.WriteEnumT("m_last_td", m_last_td);
		dmp.WriteValue("m_cost", m_cost);
		dmp.WriteTile("m_last_signal_tile", m_last_signal_tile);
		dmp.WriteEnumT("m_last_signal_td", m_last_signal_td);
		dmp.WriteEnumT("m_end_segment_reason", m_end_segment_reason);
	}
};

/** Yapf Node for rail YAPF */
template <class Tkey_>
struct CYapfRailNodeT
	: CYapfNodeT<Tkey_, CYapfRailNodeT<Tkey_> >
{
	typedef CYapfNodeT<Tkey_, CYapfRailNodeT<Tkey_> > base;
	typedef CYapfRailSegment CachedData;

	CYapfRailSegment *m_segment;
	uint16_t            m_num_signals_passed;
	union {
		uint32_t          m_inherited_flags;
		struct {
			bool          m_targed_seen;
			bool          m_choice_seen;
			bool          m_last_signal_was_red;
		} flags_s;
	} flags_u;
	SignalType        m_last_red_signal_type;
	SignalType        m_last_signal_type;

	inline void Set(CYapfRailNodeT *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		base::Set(parent, tile, td, is_choice);
		m_segment = nullptr;
		if (parent == nullptr) {
			m_num_signals_passed      = 0;
			flags_u.m_inherited_flags = 0;
			m_last_red_signal_type    = SIGTYPE_NORMAL;
			/* We use PBS as initial signal type because if we are in
			 * a PBS section and need to route, i.e. we're at a safe
			 * waiting point of a station, we need to account for the
			 * reservation costs. If we are in a normal block then we
			 * should be alone in there and as such the reservation
			 * costs should be 0 anyway. If there would be another
			 * train in the block, i.e. passing signals at danger
			 * then avoiding that train with help of the reservation
			 * costs is not a bad thing, actually it would probably
			 * be a good thing to do. */
			m_last_signal_type        = SIGTYPE_PBS;
		} else {
			m_num_signals_passed      = parent->m_num_signals_passed;
			flags_u.m_inherited_flags = parent->flags_u.m_inherited_flags;
			m_last_red_signal_type    = parent->m_last_red_signal_type;
			m_last_signal_type        = parent->m_last_signal_type;
		}
		flags_u.flags_s.m_choice_seen |= is_choice;
	}

	inline TileIndex GetLastTile() const
	{
		assert(m_segment != nullptr);
		return m_segment->m_last_tile;
	}

	inline Trackdir GetLastTrackdir() const
	{
		assert(m_segment != nullptr);
		return m_segment->m_last_td;
	}

	inline void SetLastTileTrackdir(TileIndex tile, Trackdir td)
	{
		assert(m_segment != nullptr);
		m_segment->m_last_tile = tile;
		m_segment->m_last_td = td;
	}

	template <class Tbase, class Tfunc, class Tpf>
	bool IterateTiles(const Train *v, Tpf &yapf, Tbase &obj, bool (Tfunc::*func)(TileIndex, Trackdir)) const
	{
		typename Tbase::TrackFollower ft(v, yapf.GetCompatibleRailTypes());
		TileIndex cur = base::GetTile();
		Trackdir  cur_td = base::GetTrackdir();

		while (cur != GetLastTile() || cur_td != GetLastTrackdir()) {
			if (!((obj.*func)(cur, cur_td))) return false;

			if (!ft.Follow(cur, cur_td)) break;
			cur = ft.m_new_tile;
			assert(KillFirstBit(ft.m_new_td_bits) == TRACKDIR_BIT_NONE);
			cur_td = FindFirstTrackdir(ft.m_new_td_bits);
		}

		return (obj.*func)(cur, cur_td);
	}

	void Dump(DumpTarget &dmp) const
	{
		base::Dump(dmp);
		dmp.WriteStructT("m_segment", m_segment);
		dmp.WriteValue("m_num_signals_passed", m_num_signals_passed);
		dmp.WriteValue("m_targed_seen", flags_u.flags_s.m_targed_seen ? "Yes" : "No");
		dmp.WriteValue("m_choice_seen", flags_u.flags_s.m_choice_seen ? "Yes" : "No");
		dmp.WriteValue("m_last_signal_was_red", flags_u.flags_s.m_last_signal_was_red ? "Yes" : "No");
		dmp.WriteEnumT("m_last_red_signal_type", m_last_red_signal_type);
	}
};

/* now define two major node types (that differ by key type) */
typedef CYapfRailNodeT<CYapfNodeKeyExitDir>  CYapfRailNodeExitDir;
typedef CYapfRailNodeT<CYapfNodeKeyTrackDir> CYapfRailNodeTrackDir;

/* Default NodeList types */
typedef CNodeList_HashTableT<CYapfRailNodeExitDir , 8, 10> CRailNodeListExitDir;
typedef CNodeList_HashTableT<CYapfRailNodeTrackDir, 8, 10> CRailNodeListTrackDir;

#endif /* YAPF_NODE_RAIL_HPP */
