/* $Id$ */

/** @file yapf_destrail.hpp Determining the destination for rail vehicles. */

#ifndef  YAPF_DESTRAIL_HPP
#define  YAPF_DESTRAIL_HPP

class CYapfDestinationRailBase
{
protected:
	RailTypes m_compatible_railtypes;

public:
	void SetDestination(const Vehicle* v)
	{
		m_compatible_railtypes = v->u.rail.compatible_railtypes;
	}

	bool IsCompatibleRailType(RailType rt)
	{
		return HasBit(m_compatible_railtypes, rt);
	}
};

template <class Types>
class CYapfDestinationAnyDepotRailT
	: public CYapfDestinationRailBase
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		return PfDetectDestination(n.GetLastTile(), n.GetLastTrackdir());
	}

	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(TileIndex tile, Trackdir td)
	{
		bool bDest = IsRailDepotTile(tile);
		return bDest;
	}

	/** Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate */
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
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	TileIndex    m_destTile;
	TrackdirBits m_destTrackdirs;
	StationID    m_dest_station_id;

	/// to access inherited path finder
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
	void SetDestination(const Vehicle* v)
	{
		switch (v->current_order.GetType()) {
			case OT_GOTO_STATION:
				m_destTile = CalcStationCenterTile(v->current_order.GetDestination());
				m_dest_station_id = v->current_order.GetDestination();
				m_destTrackdirs = INVALID_TRACKDIR_BIT;
				break;

			case OT_GOTO_WAYPOINT: {
				Waypoint *wp = GetWaypoint(v->current_order.GetDestination());
				if (wp == NULL) {
					/* Invalid waypoint in orders! */
					DEBUG(yapf, 0, "Invalid waypoint in orders == 0x%04X (train %d, player %d)", v->current_order.GetDestination(), v->unitnumber, (PlayerID)v->owner);
					break;
				}
				m_destTile = wp->xy;
				if (m_destTile != v->dest_tile) {
					/* Something is wrong with orders! */
					DEBUG(yapf, 0, "Invalid v->dest_tile == 0x%04X (train %d, player %d)", v->dest_tile, v->unitnumber, (PlayerID)v->owner);
				}
				m_dest_station_id = INVALID_STATION;
				m_destTrackdirs = TrackToTrackdirBits(AxisToTrack(GetWaypointAxis(wp->xy)));
				break;
			}

			default:
				m_destTile = v->dest_tile;
				m_dest_station_id = INVALID_STATION;
				m_destTrackdirs = TrackStatusToTrackdirBits(GetTileTrackStatus(v->dest_tile, TRANSPORT_RAIL, 0));
				break;
		}
		CYapfDestinationRailBase::SetDestination(v);
	}

	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		return PfDetectDestination(n.GetLastTile(), n.GetLastTrackdir());
	}

	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(TileIndex tile, Trackdir td)
	{
		bool bDest;
		if (m_dest_station_id != INVALID_STATION) {
			bDest = IsRailwayStationTile(tile)
				&& (GetStationIndex(tile) == m_dest_station_id)
				&& (GetRailStationTrack(tile) == TrackdirToTrack(td));
		} else {
			bDest = (tile == m_destTile)
				&& ((m_destTrackdirs & TrackdirToTrackdirBits(td)) != TRACKDIR_BIT_NONE);
		}
		return bDest;
	}

	/** Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate */
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
