/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file yapf_common.hpp Commonly used classes and utilities for YAPF. */

#ifndef YAPF_COMMON_HPP
#define YAPF_COMMON_HPP

#include "../../core/bitmath_func.hpp"
#include "../../tile_type.h"
#include "../../track_type.h"

/** YAPF origin provider base class - used when origin is one tile / multiple trackdirs */
template <class Types>
class CYapfOriginTileT {
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables

protected:
	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Set origin tile / trackdir mask */
	void SetOrigin(TileIndex tile, TrackdirBits trackdirs)
	{
		bool is_choice = (KillFirstBit(trackdirs) != TRACKDIR_BIT_NONE);
		for (TrackdirBits tdb = trackdirs; tdb != TRACKDIR_BIT_NONE; tdb = KillFirstBit(tdb)) {
			Trackdir td = (Trackdir)FindFirstBit(tdb);
			Node &node = Yapf().CreateNewNode();
			node.Set(nullptr, tile, td, is_choice);
			Yapf().AddStartupNode(node);
		}
	}
};

/** YAPF origin provider base class - used when there are two tile/trackdir origins */
template <class Types>
class CYapfOriginTileTwoWayT {
public:
	typedef typename Types::Tpf Tpf; ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Item Node; ///< this will be our node type
	typedef typename Node::Key Key; ///< key to hash tables

protected:
	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** set origin (tiles, trackdirs, etc.) */
	void SetOrigin(TileIndex forward_tile, Trackdir forward_td, TileIndex reverse_tile = INVALID_TILE,
			Trackdir reverse_td = INVALID_TRACKDIR, int reverse_penalty = 0)
	{
		if (forward_tile != INVALID_TILE && forward_td != INVALID_TRACKDIR) {
			Node &node = Yapf().CreateNewNode();
			node.Set(nullptr, forward_tile, forward_td, false);
			Yapf().AddStartupNode(node);
		}
		if (reverse_tile != INVALID_TILE && reverse_td != INVALID_TRACKDIR) {
			Node &node = Yapf().CreateNewNode();
			node.Set(nullptr, reverse_tile, reverse_td, false);
			node.cost = reverse_penalty;
			Yapf().AddStartupNode(node);
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

/**
 * Calculates the octile distance cost between a starting tile / trackdir and a destination tile.
 * @param start_tile Starting tile.
 * @param start_td Starting trackdir.
 * @param destination_tile Destination tile.
 * @return Octile distance cost between starting tile / trackdir and destination tile.
 */
inline int OctileDistanceCost(TileIndex start_tile, Trackdir start_td, TileIndex destination_tile)
{
	static constexpr int dg_dir_to_x_offs[] = {-1, 0, 1, 0};
	static constexpr int dg_dir_to_y_offs[] = {0, 1, 0, -1};

	const DiagDirection exitdir = TrackdirToExitdir(start_td);

	const int x1 = 2 * TileX(start_tile) + dg_dir_to_x_offs[static_cast<int>(exitdir)];
	const int y1 = 2 * TileY(start_tile) + dg_dir_to_y_offs[static_cast<int>(exitdir)];
	const int x2 = 2 * TileX(destination_tile);
	const int y2 = 2 * TileY(destination_tile);
	const int dx = abs(x1 - x2);
	const int dy = abs(y1 - y2);
	const int dmin = std::min(dx, dy);
	const int dxy = abs(dx - dy);

	return dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
}

#endif /* YAPF_COMMON_HPP */
