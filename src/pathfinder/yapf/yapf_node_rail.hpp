/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf_node_rail.hpp Node tailored for rail pathfinding. */

#ifndef YAPF_NODE_RAIL_HPP
#define YAPF_NODE_RAIL_HPP

#include "../../misc/dbg_helpers.h"
#include "../../train.h"
#include "nodelist.hpp"
#include "yapf_node.hpp"
#include "yapf_type.hpp"

/** key for cached segment cost for rail YAPF */
struct CYapfRailSegmentKey {
	uint32_t value;

	inline CYapfRailSegmentKey(const CYapfNodeKeyTrackDir &node_key)
	{
		this->Set(node_key);
	}

	inline void Set(const CYapfRailSegmentKey &src)
	{
		this->value = src.value;
	}

	inline void Set(const CYapfNodeKeyTrackDir &node_key)
	{
		this->value = (node_key.tile.base() << 4) | node_key.td;
	}

	inline int32_t CalcHash() const
	{
		return this->value;
	}

	inline TileIndex GetTile() const
	{
		return (TileIndex)(this->value >> 4);
	}

	inline Trackdir GetTrackdir() const
	{
		return (Trackdir)(this->value & 0x0F);
	}

	inline bool operator==(const CYapfRailSegmentKey &other) const
	{
		return this->value == other.value;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteTile("tile", this->GetTile());
		dmp.WriteEnumT("td", this->GetTrackdir());
	}
};

/** cached segment cost for rail YAPF */
struct CYapfRailSegment {
	typedef CYapfRailSegmentKey Key;

	CYapfRailSegmentKey key;
	TileIndex last_tile = INVALID_TILE;
	Trackdir last_td = INVALID_TRACKDIR;
	int cost = -1;
	TileIndex last_signal_tile = INVALID_TILE;
	Trackdir last_signal_td = INVALID_TRACKDIR;
	EndSegmentReasons end_segment_reason{};
	CYapfRailSegment *hash_next = nullptr;

	inline CYapfRailSegment(const CYapfRailSegmentKey &key) : key(key) {}

	inline const Key &GetKey() const
	{
		return this->key;
	}

	inline TileIndex GetTile() const
	{
		return this->key.GetTile();
	}

	inline CYapfRailSegment *GetHashNext()
	{
		return this->hash_next;
	}

	inline void SetHashNext(CYapfRailSegment *next)
	{
		this->hash_next = next;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("key", &this->key);
		dmp.WriteTile("last_tile", this->last_tile);
		dmp.WriteEnumT("last_td", this->last_td);
		dmp.WriteValue("cost", this->cost);
		dmp.WriteTile("last_signal_tile", this->last_signal_tile);
		dmp.WriteEnumT("last_signal_td", this->last_signal_td);
		dmp.WriteEnumT("end_segment_reason", this->end_segment_reason);
	}
};

/** Yapf Node for rail YAPF */
struct CYapfRailNode : CYapfNodeT<CYapfNodeKeyTrackDir, CYapfRailNode> {
	typedef CYapfNodeT<CYapfNodeKeyTrackDir, CYapfRailNode> base;
	typedef CYapfRailSegment CachedData;

	CYapfRailSegment *segment;
	uint16_t num_signals_passed;
	union {
		uint32_t inherited_flags;
		struct {
			bool target_seen;
			bool choice_seen;
			bool last_signal_was_red;
		} flags_s;
	} flags_u;
	SignalType last_red_signal_type;
	SignalType last_signal_type;

	inline void Set(CYapfRailNode *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		this->base::Set(parent, tile, td, is_choice);
		this->segment = nullptr;
		if (parent == nullptr) {
			this->num_signals_passed      = 0;
			this->flags_u.inherited_flags = 0;
			this->last_red_signal_type    = SIGTYPE_BLOCK;
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
			this->last_signal_type        = SIGTYPE_PBS;
		} else {
			this->num_signals_passed      = parent->num_signals_passed;
			this->flags_u.inherited_flags = parent->flags_u.inherited_flags;
			this->last_red_signal_type    = parent->last_red_signal_type;
			this->last_signal_type        = parent->last_signal_type;
		}
		this->flags_u.flags_s.choice_seen |= is_choice;
	}

	inline TileIndex GetLastTile() const
	{
		assert(this->segment != nullptr);
		return this->segment->last_tile;
	}

	inline Trackdir GetLastTrackdir() const
	{
		assert(this->segment != nullptr);
		return this->segment->last_td;
	}

	inline void SetLastTileTrackdir(TileIndex tile, Trackdir td)
	{
		assert(this->segment != nullptr);
		this->segment->last_tile = tile;
		this->segment->last_td = td;
	}

	template <class Tbase, class Tfunc, class Tpf>
	bool IterateTiles(const Train *v, Tpf &yapf, Tbase &obj, bool (Tfunc::*func)(TileIndex, Trackdir)) const
	{
		typename Tbase::TrackFollower follower{v, yapf.GetCompatibleRailTypes()};
		TileIndex cur = this->base::GetTile();
		Trackdir  cur_td = this->base::GetTrackdir();

		while (cur != this->GetLastTile() || cur_td != this->GetLastTrackdir()) {
			if (!((obj.*func)(cur, cur_td))) return false;

			if (!follower.Follow(cur, cur_td)) break;
			cur = follower.new_tile;
			assert(KillFirstBit(follower.new_td_bits) == TRACKDIR_BIT_NONE);
			cur_td = FindFirstTrackdir(follower.new_td_bits);
		}

		return (obj.*func)(cur, cur_td);
	}

	void Dump(DumpTarget &dmp) const
	{
		this->base::Dump(dmp);
		dmp.WriteStructT("segment", this->segment);
		dmp.WriteValue("num_signals_passed", this->num_signals_passed);
		dmp.WriteValue("target_seen", this->flags_u.flags_s.target_seen ? "Yes" : "No");
		dmp.WriteValue("choice_seen", this->flags_u.flags_s.choice_seen ? "Yes" : "No");
		dmp.WriteValue("last_signal_was_red", this->flags_u.flags_s.last_signal_was_red ? "Yes" : "No");
		dmp.WriteEnumT("last_red_signal_type", this->last_red_signal_type);
	}
};

typedef NodeList<CYapfRailNode, 8, 10> CRailNodeList;

#endif /* YAPF_NODE_RAIL_HPP */
