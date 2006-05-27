/* $Id$ */

#ifndef  YAPF_COMMON_HPP
#define  YAPF_COMMON_HPP


template <class Types>
class CYapfOriginTileT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	TileIndex    m_orgTile;
	TrackdirBits m_orgTrackdirs;

	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	void SetOrigin(TileIndex tile, TrackdirBits trackdirs)
	{
		m_orgTile = tile;
		m_orgTrackdirs = trackdirs;
	}

	void PfSetStartupNodes()
	{
		for (TrackdirBits tdb = m_orgTrackdirs; tdb != TRACKDIR_BIT_NONE; tdb = (TrackdirBits)KillFirstBit2x64(tdb)) {
			Trackdir td = (Trackdir)FindFirstBit2x64(tdb);
			Node& n1 = Yapf().CreateNewNode();
			n1.Set(NULL, m_orgTile, td);
			Yapf().AddStartupNode(n1);
		}
	}
};

template <class Types>
class CYapfOriginTileTwoWayT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	TileIndex   m_orgTile;
	Trackdir    m_orgTd;
	TileIndex   m_revTile;
	Trackdir    m_revTd;
	int         m_reverse_penalty;
	bool        m_treat_first_red_two_way_signal_as_eol;

	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	void SetOrigin(TileIndex tile, Trackdir td, TileIndex tiler = INVALID_TILE, Trackdir tdr = INVALID_TRACKDIR, int reverse_penalty = 0, bool treat_first_red_two_way_signal_as_eol = true)
	{
		m_orgTile = tile;
		m_orgTd = td;
		m_revTile = tiler;
		m_revTd = tdr;
		m_reverse_penalty = reverse_penalty;
		m_treat_first_red_two_way_signal_as_eol = treat_first_red_two_way_signal_as_eol;
	}

	void PfSetStartupNodes()
	{
		if (m_orgTile != INVALID_TILE && m_orgTd != INVALID_TRACKDIR) {
			Node& n1 = Yapf().CreateNewNode();
			n1.Set(NULL, m_orgTile, m_orgTd);
			Yapf().AddStartupNode(n1);
		}
		if (m_revTile != INVALID_TILE && m_revTd != INVALID_TRACKDIR) {
			Node& n2 = Yapf().CreateNewNode();
			n2.Set(NULL, m_revTile, m_revTd);
			n2.m_cost = m_reverse_penalty;
			Yapf().AddStartupNode(n2);
		}
	}

	FORCEINLINE bool TreatFirstRedTwoWaySignalAsEOL()
	{
		return Yapf().PfGetSettings().rail_firstred_twoway_eol && m_treat_first_red_two_way_signal_as_eol;
	}
};

template <class Types>
class CYapfDestinationTileT
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	TileIndex    m_destTile;
	TrackdirBits m_destTrackdirs;

public:
	void SetDestination(TileIndex tile, TrackdirBits trackdirs)
	{
		m_destTile = tile;
		m_destTrackdirs = trackdirs;
	}

protected:
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = (n.m_key.m_tile == m_destTile) && ((m_destTrackdirs & TrackdirToTrackdirBits(n.GetTrackdir())) != TRACKDIR_BIT_NONE);
		return bDest;
	}

	inline bool PfCalcEstimate(Node& n)
	{
		int dx = abs(TileX(n.GetTile()) - TileX(m_destTile));
		int dy = abs(TileY(n.GetTile()) - TileY(m_destTile));
		assert(dx >= 0 && dy >= 0);
		int dd = min(dx, dy);
		int dxy = abs(dx - dy);
		int d = 14 * dd + 10 * dxy;
		n.m_estimate = n.m_cost + d /*+ d / 8*/;
		return true;
	}
};

template <class Ttypes>
class CYapfT
	: public Ttypes::PfBase
	, public Ttypes::PfCost
	, public Ttypes::PfCache
	, public Ttypes::PfOrigin
	, public Ttypes::PfDestination
	, public Ttypes::PfFollow
{
};



#endif /* YAPF_COMMON_HPP */
