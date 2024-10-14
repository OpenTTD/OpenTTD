/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_node.hpp Node in the pathfinder's graph. */

#ifndef YAPF_NODE_HPP
#define YAPF_NODE_HPP

#include "../../track_func.h"
#include "../../misc/dbg_helpers.h"

/** Yapf Node Key that evaluates hash from (and compares) tile & exit dir. */
struct CYapfNodeKeyExitDir {
	TileIndex      m_tile;
	Trackdir       m_td;
	DiagDirection  m_exitdir;

	inline void Set(TileIndex tile, Trackdir td)
	{
		this->m_tile = tile;
		this->m_td = td;
		this->m_exitdir = (this->m_td == INVALID_TRACKDIR) ? INVALID_DIAGDIR : TrackdirToExitdir(this->m_td);
	}

	inline int CalcHash() const
	{
		return this->m_exitdir | (this->m_tile.base() << 2);
	}

	inline bool operator==(const CYapfNodeKeyExitDir &other) const
	{
		return this->m_tile == other.m_tile && this->m_exitdir == other.m_exitdir;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteTile("m_tile", this->m_tile);
		dmp.WriteEnumT("m_td", this->m_td);
		dmp.WriteEnumT("m_exitdir", this->m_exitdir);
	}
};

struct CYapfNodeKeyTrackDir : public CYapfNodeKeyExitDir
{
	inline int CalcHash() const
	{
		return this->m_td | (this->m_tile.base() << 4);
	}

	inline bool operator==(const CYapfNodeKeyTrackDir &other) const
	{
		return this->m_tile == other.m_tile && this->m_td == other.m_td;
	}
};

/** Yapf Node base */
template <class Tkey_, class Tnode>
struct CYapfNodeT {
	typedef Tkey_ Key;
	typedef Tnode Node;

	Tkey_       m_key;
	Node       *m_hash_next;
	Node       *m_parent;
	int         m_cost;
	int         m_estimate;
	bool        m_is_choice;

	inline void Set(Node *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		this->m_key.Set(tile, td);
		this->m_hash_next = nullptr;
		this->m_parent = parent;
		this->m_cost = 0;
		this->m_estimate = 0;
		this->m_is_choice = is_choice;
	}

	inline Node *GetHashNext()
	{
		return this->m_hash_next;
	}

	inline void SetHashNext(Node *pNext)
	{
		this->m_hash_next = pNext;
	}

	inline TileIndex GetTile() const
	{
		return this->m_key.m_tile;
	}

	inline Trackdir GetTrackdir() const
	{
		return this->m_key.m_td;
	}

	inline const Tkey_ &GetKey() const
	{
		return this->m_key;
	}

	inline int GetCost() const
	{
		return this->m_cost;
	}

	inline int GetCostEstimate() const
	{
		return this->m_estimate;
	}

	inline bool GetIsChoice() const
	{
		return this->m_is_choice;
	}

	inline bool operator<(const Node &other) const
	{
		return this->m_estimate < other.m_estimate;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("m_key", &this->m_key);
		dmp.WriteStructT("m_parent", this->m_parent);
		dmp.WriteValue("m_cost", this->m_cost);
		dmp.WriteValue("m_estimate", this->m_estimate);
	}
};

#endif /* YAPF_NODE_HPP */
