/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf_node.hpp Node in the pathfinder's graph. */

#ifndef YAPF_NODE_HPP
#define YAPF_NODE_HPP

#include "../../track_func.h"
#include "../../misc/dbg_helpers.h"

/** Yapf Node Key that evaluates hash from (and compares) tile & exit dir. */
struct CYapfNodeKeyExitDir {
	TileIndex tile;
	Trackdir td;
	DiagDirection exitdir;

	inline void Set(TileIndex tile, Trackdir td)
	{
		this->tile = tile;
		this->td = td;
		this->exitdir = (this->td == INVALID_TRACKDIR) ? INVALID_DIAGDIR : TrackdirToExitdir(this->td);
	}

	inline int CalcHash() const
	{
		return this->exitdir | (this->tile.base() << 2);
	}

	inline bool operator==(const CYapfNodeKeyExitDir &other) const
	{
		return this->tile == other.tile && this->exitdir == other.exitdir;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteTile("tile", this->tile);
		dmp.WriteEnumT("td", this->td);
		dmp.WriteEnumT("exitdir", this->exitdir);
	}
};

struct CYapfNodeKeyTrackDir : public CYapfNodeKeyExitDir {
	inline int CalcHash() const
	{
		return this->td | (this->tile.base() << 4);
	}

	inline bool operator==(const CYapfNodeKeyTrackDir &other) const
	{
		return this->tile == other.tile && this->td == other.td;
	}
};

/** Yapf Node base */
template <class Tkey_, class Tnode>
struct CYapfNodeT {
	typedef Tkey_ Key;
	typedef Tnode Node;

	Tkey_ key;
	Node *hash_next;
	Node *parent;
	int cost;
	int estimate;
	bool is_choice;

	inline void Set(Node *parent, TileIndex tile, Trackdir td, bool is_choice)
	{
		this->key.Set(tile, td);
		this->hash_next = nullptr;
		this->parent = parent;
		this->cost = 0;
		this->estimate = 0;
		this->is_choice = is_choice;
	}

	inline Node *GetHashNext()
	{
		return this->hash_next;
	}

	inline void SetHashNext(Node *pNext)
	{
		this->hash_next = pNext;
	}

	inline TileIndex GetTile() const
	{
		return this->key.tile;
	}

	inline Trackdir GetTrackdir() const
	{
		return this->key.td;
	}

	inline const Tkey_ &GetKey() const
	{
		return this->key;
	}

	inline int GetCost() const
	{
		return this->cost;
	}

	inline int GetCostEstimate() const
	{
		return this->estimate;
	}

	inline bool GetIsChoice() const
	{
		return this->is_choice;
	}

	inline bool operator<(const Node &other) const
	{
		return this->estimate < other.estimate;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("key", &this->key);
		dmp.WriteStructT("parent", this->parent);
		dmp.WriteValue("cost", this->cost);
		dmp.WriteValue("estimate", this->estimate);
	}
};

#endif /* YAPF_NODE_HPP */
