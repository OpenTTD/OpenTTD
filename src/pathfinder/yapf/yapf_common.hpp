/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_common.hpp Commonly used classes for YAPF. */

#ifndef YAPF_COMMON_HPP
#define YAPF_COMMON_HPP

/** YAPF origin provider base class - used when origin is one tile / multiple trackdirs */
template <class Types>
class CYapfOriginTileT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	TileIndex    m_orgTile;                       ///< origin tile
	TrackdirBits m_orgTrackdirs;                  ///< origin trackdir mask

	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Set origin tile / trackdir mask */
	void SetOrigin(TileIndex tile, TrackdirBits trackdirs)
	{
		m_orgTile = tile;
		m_orgTrackdirs = trackdirs;
	}

	/** Called when YAPF needs to place origin nodes into open list */
	void PfSetStartupNodes()
	{
		bool is_choice = (KillFirstBit(m_orgTrackdirs) != TRACKDIR_BIT_NONE);
		for (TrackdirBits tdb = m_orgTrackdirs; tdb != TRACKDIR_BIT_NONE; tdb = KillFirstBit(tdb)) {
			Trackdir td = (Trackdir)FindFirstBit2x64(tdb);
			Node &n1 = Yapf().CreateNewNode();
			n1.Set(nullptr, m_orgTile, td, is_choice);
			Yapf().AddStartupNode(n1);
		}
	}
};

/** YAPF origin provider base class - used when there are two tile/trackdir origins */
template <class Types>
class CYapfOriginTileTwoWayT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	TileIndex   m_orgTile;                        ///< first origin tile
	Trackdir    m_orgTd;                          ///< first origin trackdir
	TileIndex   m_revTile;                        ///< second (reversed) origin tile
	Trackdir    m_revTd;                          ///< second (reversed) origin trackdir
	int         m_reverse_penalty;                ///< penalty to be added for using the reversed origin
	bool        m_treat_first_red_two_way_signal_as_eol; ///< in some cases (leaving station) we need to handle first two-way signal differently

	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** set origin (tiles, trackdirs, etc.) */
	void SetOrigin(TileIndex tile, Trackdir td, TileIndex tiler = INVALID_TILE, Trackdir tdr = INVALID_TRACKDIR, int reverse_penalty = 0, bool treat_first_red_two_way_signal_as_eol = true)
	{
		m_orgTile = tile;
		m_orgTd = td;
		m_revTile = tiler;
		m_revTd = tdr;
		m_reverse_penalty = reverse_penalty;
		m_treat_first_red_two_way_signal_as_eol = treat_first_red_two_way_signal_as_eol;
	}

	/** Called when YAPF needs to place origin nodes into open list */
	void PfSetStartupNodes()
	{
		if (m_orgTile != INVALID_TILE && m_orgTd != INVALID_TRACKDIR) {
			Node &n1 = Yapf().CreateNewNode();
			n1.Set(nullptr, m_orgTile, m_orgTd, false);
			Yapf().AddStartupNode(n1);
		}
		if (m_revTile != INVALID_TILE && m_revTd != INVALID_TRACKDIR) {
			Node &n2 = Yapf().CreateNewNode();
			n2.Set(nullptr, m_revTile, m_revTd, false);
			n2.m_cost = m_reverse_penalty;
			Yapf().AddStartupNode(n2);
		}
	}

	/** return true if first two-way signal should be treated as dead end */
	inline bool TreatFirstRedTwoWaySignalAsEOL()
	{
		return Yapf().PfGetSettings().rail_firstred_twoway_eol && m_treat_first_red_two_way_signal_as_eol;
	}
};

/** YAPF destination provider base class - used when destination is single tile / multiple trackdirs */
template <class Types>
class CYapfDestinationTileT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	TileIndex    m_destTile;                      ///< destination tile
	TrackdirBits m_destTrackdirs;                 ///< destination trackdir mask

public:
	/** set the destination tile / more trackdirs */
	void SetDestination(TileIndex tile, TrackdirBits trackdirs)
	{
		m_destTile = tile;
		m_destTrackdirs = trackdirs;
	}

protected:
	/** to access inherited path finder */
	Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(Node &n)
	{
		return (n.m_key.m_tile == m_destTile) && HasTrackdir(m_destTrackdirs, n.GetTrackdir());
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate
	 */
	inline bool PfCalcEstimate(Node &n)
	{
		static const int dg_dir_to_x_offs[] = {-1, 0, 1, 0};
		static const int dg_dir_to_y_offs[] = {0, 1, 0, -1};
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		TileIndex tile = n.GetTile();
		DiagDirection exitdir = TrackdirToExitdir(n.GetTrackdir());
		int x1 = 2 * TileX(tile) + dg_dir_to_x_offs[(int)exitdir];
		int y1 = 2 * TileY(tile) + dg_dir_to_y_offs[(int)exitdir];
		int x2 = 2 * TileX(m_destTile);
		int y2 = 2 * TileY(m_destTile);
		int dx = abs(x1 - x2);
		int dy = abs(y1 - y2);
		int dmin = std::min(dx, dy);
		int dxy = abs(dx - dy);
		int d = dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
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
