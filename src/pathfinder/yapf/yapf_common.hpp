/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_common.hpp Commonly used classes for YAPF. */

#ifndef YAPF_COMMON_HPP
#define YAPF_COMMON_HPP

#include "../../core/bitmath_func.hpp"
#include "../../tile_type.h"
#include "../../track_type.h"

/** YAPF origin provider base class - used when origin is one tile / multiple trackdirs */
template <class Types>
class CYapfOriginTileT
{
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables

protected:
	TileIndex origin_tile; ///< origin tile
	TrackdirBits origin_trackdirs; ///< origin trackdir mask

	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Set origin tile / trackdir mask */
	void SetOrigin(TileIndex tile, TrackdirBits trackdirs)
	{
		this->origin_tile = tile;
		this->origin_trackdirs = trackdirs;
	}

	/** Called when YAPF needs to place origin nodes into open list */
	void PfSetStartupNodes()
	{
		bool is_choice = (KillFirstBit(this->origin_trackdirs) != TRACKDIR_BIT_NONE);
		for (TrackdirBits tdb = this->origin_trackdirs; tdb != TRACKDIR_BIT_NONE; tdb = KillFirstBit(tdb)) {
			Trackdir td = (Trackdir)FindFirstBit(tdb);
			Node &n1 = Yapf().CreateNewNode();
			n1.Set(nullptr, this->origin_tile, td, is_choice);
			Yapf().AddStartupNode(n1);
		}
	}
};

/** YAPF origin provider base class - used when there are two tile/trackdir origins */
template <class Types>
class CYapfOriginTileTwoWayT
{
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables

protected:
	TileIndex origin_tile; ///< first origin tile
	Trackdir origin_td; ///< first origin trackdir
	TileIndex reverse_tile; ///< second (reverse) origin tile
	Trackdir reverse_td; ///< second (reverse) origin trackdir
	int reverse_penalty; ///< penalty to be added for using the reverse origin

	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** set origin (tiles, trackdirs, etc.) */
	void SetOrigin(TileIndex tile, Trackdir td, TileIndex tiler = INVALID_TILE, Trackdir tdr = INVALID_TRACKDIR, int reverse_penalty = 0)
	{
		this->origin_tile = tile;
		this->origin_td = td;
		this->reverse_tile = tiler;
		this->reverse_td = tdr;
		this->reverse_penalty = reverse_penalty;
	}

	/** Called when YAPF needs to place origin nodes into open list */
	void PfSetStartupNodes()
	{
		if (this->origin_tile != INVALID_TILE && this->origin_td != INVALID_TRACKDIR) {
			Node &n1 = Yapf().CreateNewNode();
			n1.Set(nullptr, this->origin_tile, this->origin_td, false);
			Yapf().AddStartupNode(n1);
		}
		if (this->reverse_tile != INVALID_TILE && this->reverse_td != INVALID_TRACKDIR) {
			Node &n2 = Yapf().CreateNewNode();
			n2.Set(nullptr, this->reverse_tile, this->reverse_td, false);
			n2.cost = this->reverse_penalty;
			Yapf().AddStartupNode(n2);
		}
	}
};

/**
 * YAPF template that uses Ttypes template argument to determine all YAPF
 *  components (base classes) from which the actual YAPF is composed.
 *  For example classes consult: CYapfRail_TypesT template and its instantiations:
 *  CYapfRail1, CYapfRail2, CYapfRail3, CYapfAnyDepotRail1, CYapfAnyDepotRail2, CYapfAnyDepotRail3
 */
template <class Ttypes>
class CYapfT
	: public Ttypes::PfBase         ///< Instance of CYapfBaseT - main YAPF loop and support base class
	, public Ttypes::PfCost         ///< Cost calculation provider base class
	, public Ttypes::PfCache        ///< Segment cost cache provider
	, public Ttypes::PfOrigin       ///< Origin (tile or two-tile origin)
	, public Ttypes::PfDestination  ///< Destination detector and distance (estimate) calculation provider
	, public Ttypes::PfFollow       ///< Node follower (stepping provider)
{
};



#endif /* YAPF_COMMON_HPP */
