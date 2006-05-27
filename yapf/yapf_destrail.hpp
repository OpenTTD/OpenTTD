/* $Id$ */

#ifndef  YAPF_DESTRAIL_HPP
#define  YAPF_DESTRAIL_HPP

class CYapfDestinationRailBase
{
protected:
	RailTypeMask m_compatible_railtypes;

public:
	void SetDestination(Vehicle* v)
	{
		m_compatible_railtypes = v->u.rail.compatible_railtypes;
	}

	bool IsCompatibleRailType(RailType rt)
	{
		return HASBIT(m_compatible_railtypes, rt);
	}
};

template <class Types>
class CYapfDestinationAnyDepotRailT
	: public CYapfDestinationRailBase
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = IsTileDepotType(n.GetLastTile(), TRANSPORT_RAIL);
		return bDest;
	}

	FORCEINLINE bool PfCalcEstimate(Node& n)
	{
		n.m_estimate = n.m_cost;
		return true;
	}
};

template <class Types>
class CYapfDestinationTileOrStationRailT
	: public CYapfDestinationRailBase
{
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;    ///< key to hash tables

protected:
	TileIndex    m_destTile;
	TrackdirBits m_destTrackdirs;
	StationID    m_dest_station_id;

	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	static TileIndex CalcStationCenterTile(StationID station)
	{
		const Station* st = GetStation(station);

		uint x = TileX(st->train_tile) + st->trainst_w / 2;
		uint y = TileY(st->train_tile) + st->trainst_h / 2;
		// return the tile of our target coordinates
		return TileXY(x, y);
	}

public:
	void SetDestination(Vehicle* v)
	{
		if (v->current_order.type == OT_GOTO_STATION) {
			m_destTile = CalcStationCenterTile(v->current_order.station);
			m_dest_station_id = v->current_order.station;
			m_destTrackdirs = INVALID_TRACKDIR_BIT;
		} else {
			m_destTile = v->dest_tile;
			m_dest_station_id = INVALID_STATION;
			m_destTrackdirs = (TrackdirBits)(GetTileTrackStatus(v->dest_tile, TRANSPORT_RAIL) & TRACKDIR_BIT_MASK);
		}
		CYapfDestinationRailBase::SetDestination(v);
	}

	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest;
		if (m_dest_station_id != INVALID_STATION) {
			bDest = IsRailwayStationTile(n.GetLastTile())
				&& (GetStationIndex(n.GetLastTile()) == m_dest_station_id)
				&& (GetRailStationTrack(n.GetLastTile()) == TrackdirToTrack(n.GetLastTrackdir()));
		} else {
			bDest = (n.GetLastTile() == m_destTile)
				&& ((m_destTrackdirs & TrackdirToTrackdirBits(n.GetLastTrackdir())) != TRACKDIR_BIT_NONE);
		}
		return bDest;
	}

	FORCEINLINE bool PfCalcEstimate(Node& n)
	{
		static int dg_dir_to_x_offs[] = {-1, 0, 1, 0};
		static int dg_dir_to_y_offs[] = {0, 1, 0, -1};
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		TileIndex tile = n.GetLastTile();
		DiagDirection exitdir = TrackdirToExitdir(n.GetLastTrackdir());
		int x1 = 2 * TileX(tile) + dg_dir_to_x_offs[(int)exitdir];
		int y1 = 2 * TileY(tile) + dg_dir_to_y_offs[(int)exitdir];
		int x2 = 2 * TileX(m_destTile);
		int y2 = 2 * TileY(m_destTile);
		int dx = abs(x1 - x2);
		int dy = abs(y1 - y2);
		int dmin = min(dx, dy);
		int dxy = abs(dx - dy);
		int d = dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};


#endif /* YAPF_DESTRAIL_HPP */
