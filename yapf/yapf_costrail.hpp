/* $Id$ */

#ifndef  YAPF_COSTRAIL_HPP
#define  YAPF_COSTRAIL_HPP


template <class Types>
class CYapfCostRailT
	: public CYapfCostBase
	, public CostRailSettings
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables
	typedef typename Node::CachedData CachedData;

protected:
	int           m_max_cost;
	CBlobT<int>   m_sig_look_ahead_costs;

	static const int s_max_segment_cost = 10000;

	CYapfCostRailT() : m_max_cost(0)
	{
		// pre-compute look-ahead penalties into array
		int p0 = Yapf().PfGetSettings().rail_look_ahead_signal_p0;
		int p1 = Yapf().PfGetSettings().rail_look_ahead_signal_p1;
		int p2 = Yapf().PfGetSettings().rail_look_ahead_signal_p2;
		int *pen = m_sig_look_ahead_costs.GrowSizeNC(Yapf().PfGetSettings().rail_look_ahead_max_signals);
		for (uint i = 0; i < Yapf().PfGetSettings().rail_look_ahead_max_signals; i++)
			pen[i] = p0 + i * (p1 + i * p2);
	}

	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	FORCEINLINE int SlopeCost(TileIndex tile, Trackdir td)
	{
		CPerfStart perf_cost(Yapf().m_perf_slope_cost);
		if (!stSlopeCost(tile, td)) return 0;
		return Yapf().PfGetSettings().rail_slope_penalty;
	}

	FORCEINLINE int CurveCost(Trackdir td1, Trackdir td2)
	{
		int cost = 0;
		if (TrackFollower::Allow90degTurns()
				&& ((TrackdirToTrackdirBits(td2) & (TrackdirBits)TrackdirCrossesTrackdirs(td1)) != 0)) {
			// 90-deg curve penalty
			cost += Yapf().PfGetSettings().rail_curve90_penalty;
		} else if (td2 != NextTrackdir(td1)) {
			// 45-deg curve penalty
			cost += Yapf().PfGetSettings().rail_curve45_penalty;
		}
		return cost;
	}

	/** return one tile cost. If tile is a tunnel entry, it is moved to the end of tunnel */
	FORCEINLINE int OneTileCost(TileIndex& tile, Trackdir trackdir)
	{
		int cost = 0;
		// set base cost
		if (IsDiagonalTrackdir(trackdir)) {
			cost += YAPF_TILE_LENGTH;
			switch (GetTileType(tile)) {
				case MP_STREET:
					/* Increase the cost for level crossings */
					if (IsLevelCrossing(tile))
						cost += Yapf().PfGetSettings().rail_crossing_penalty;
					break;

				case MP_STATION:
					// penalty for passing station tiles
					cost += Yapf().PfGetSettings().rail_station_penalty;
					break;

				default:
					break;
			}
		} else {
			// non-diagonal trackdir
			cost = YAPF_TILE_CORNER_LENGTH;
		}
		return cost;
	}

	int SignalCost(Node& n, TileIndex tile, Trackdir trackdir)
	{
		int cost = 0;
		// if there is one-way signal in the opposite direction, then it is not our way
		CPerfStart perf_cost(Yapf().m_perf_other_cost);
		if (IsTileType(tile, MP_RAILWAY)) {
			bool has_signal_against = HasSignalOnTrackdir(tile, ReverseTrackdir(trackdir));
			bool has_signal_along = HasSignalOnTrackdir(tile, trackdir);
			if (has_signal_against && !has_signal_along) {
				// one-way signal in opposite direction
				n.m_segment->flags_u.flags_s.m_end_of_line = true;
			} else if (has_signal_along) {
				SignalState sig_state = GetSignalStateByTrackdir(tile, trackdir);
				// cache the look-ahead polynomial constant only if we didn't pass more signals than the look-ahead limit is
				int look_ahead_cost = (n.m_num_signals_passed < m_sig_look_ahead_costs.Size()) ? m_sig_look_ahead_costs.Data()[n.m_num_signals_passed] : 0;
				if (sig_state != SIGNAL_STATE_RED) {
					// green signal
					n.flags_u.flags_s.m_last_signal_was_red = false;
					// negative look-ahead red-signal penalties would cause problems later, so use them as positive penalties for green signal
					if (look_ahead_cost < 0) {
						// add its negation to the cost
						cost -= look_ahead_cost;
					}
				} else {
					// we have a red signal in our direction
					// was it first signal which is two-way?
					if (Yapf().TreatFirstRedTwoWaySignalAsEOL() && n.flags_u.flags_s.m_choice_seen && has_signal_against && n.m_num_signals_passed == 0) {
						// yes, the first signal is two-way red signal => DEAD END
						n.m_segment->flags_u.flags_s.m_end_of_line = true;
						return -1;
					}
					SignalType sig_type = GetSignalType(tile);
					n.m_last_red_signal_type = sig_type;
					n.flags_u.flags_s.m_last_signal_was_red = true;

					// look-ahead signal penalty
					if (look_ahead_cost > 0) {
						// add the look ahead penalty only if it is positive
						cost += look_ahead_cost;
					}

					// special signal penalties
					if (n.m_num_signals_passed == 0) {
						switch (sig_type) {
							case SIGTYPE_COMBO:
							case SIGTYPE_EXIT:   cost += Yapf().PfGetSettings().rail_firstred_exit_penalty; break; // first signal is red pre-signal-exit
							case SIGTYPE_NORMAL:
							case SIGTYPE_ENTRY:  cost += Yapf().PfGetSettings().rail_firstred_penalty; break;
						};
					}
				}
				n.m_num_signals_passed++;
				n.m_segment->m_last_signal_tile = tile;
				n.m_segment->m_last_signal_td = trackdir;
			}
		}
		return cost;
	}

	FORCEINLINE int PlatformLengthPenalty(int platform_length)
	{
		int cost = 0;
		const Vehicle* v = Yapf().GetVehicle();
		assert(v != NULL);
		assert(v->type == VEH_Train);
		assert(v->u.rail.cached_total_length != 0);
		int needed_platform_length = (v->u.rail.cached_total_length + TILE_SIZE - 1) / TILE_SIZE;
		if (platform_length > needed_platform_length) {
			// apply penalty for longer platform than needed
			cost += Yapf().PfGetSettings().rail_longer_platform_penalty;
		} else if (needed_platform_length > platform_length) {
			// apply penalty for shorter platform than needed
			cost += Yapf().PfGetSettings().rail_shorter_platform_penalty;
		}
		return cost;
	}

public:
	FORCEINLINE void SetMaxCost(int max_cost) {m_max_cost = max_cost;}

	/** Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member */
	FORCEINLINE bool PfCalcCost(Node& n)
	{
		assert(!n.flags_u.flags_s.m_targed_seen);
		CPerfStart perf_cost(Yapf().m_perf_cost);
		int parent_cost = (n.m_parent != NULL) ? n.m_parent->m_cost : 0;
		int first_tile_cost = 0;
		int segment_cost = 0;
		int extra_cost = 0;
		const Vehicle* v = Yapf().GetVehicle();

		// start at n.m_key.m_tile / n.m_key.m_td and walk to the end of segment
		TileIndex prev_tile      = (n.m_parent != NULL) ? n.m_parent->GetLastTile() : INVALID_TILE;
		Trackdir  prev_trackdir  = (n.m_parent != NULL) ? n.m_parent->GetLastTrackdir() : INVALID_TRACKDIR;
		TileType  prev_tile_type = (n.m_parent != NULL) ? GetTileType(n.m_parent->GetLastTile()) : MP_VOID;

		TileIndex tile = n.m_key.m_tile;
		Trackdir trackdir = n.m_key.m_td;
		TileType tile_type = GetTileType(tile);

		RailType rail_type = GetTileRailType(tile, trackdir);

		bool target_seen = Yapf().PfDetectDestination(tile, trackdir);

		while (true) {
			segment_cost += Yapf().OneTileCost(tile, trackdir);
			segment_cost += Yapf().CurveCost(prev_trackdir, trackdir);
			segment_cost += Yapf().SlopeCost(tile, trackdir);
			segment_cost += Yapf().SignalCost(n, tile, trackdir);
			if (n.m_segment->flags_u.flags_s.m_end_of_line) {
				break;
			}

			// finish if we have reached the destination
			if (target_seen) {
				break;
			}

			// finish on first station tile - segment should end here to avoid target skipping
			// when cached segments are used
			if (tile_type == MP_STATION && prev_tile_type != MP_STATION) {
				break;
			}

			// finish also on waypoint - same workaround as for first station tile
			if (tile_type == MP_RAILWAY && IsRailWaypoint(tile)) {
				break;
			}

			// if there are no reachable trackdirs on the next tile, we have end of road
			TrackFollower F(v, &Yapf().m_perf_ts_cost);
			if (!F.Follow(tile, trackdir)) {
				// we can't continue?
				// n.m_segment->flags_u.flags_s.m_end_of_line = true;
				break;
			}

			// if there are more trackdirs available & reachable, we are at the end of segment
			if (KillFirstBit2x64(F.m_new_td_bits) != 0) {
				break;
			}

			Trackdir new_td = (Trackdir)FindFirstBit2x64(F.m_new_td_bits);

			{
				// end segment if train is about to enter simple loop with no junctions
				// so next time it should stop on the next if
				if (segment_cost > s_max_segment_cost && IsTileType(F.m_new_tile, MP_RAILWAY))
					break;

				// stop if train is on simple loop with no junctions
				if (F.m_new_tile == n.m_key.m_tile && new_td == n.m_key.m_td)
					return false;
			}

			// if tail type changes, finish segment (cached segment can't contain more rail types)
			{
				RailType new_rail_type = GetTileRailType(F.m_new_tile, (Trackdir)FindFirstBit2x64(F.m_new_td_bits));
				if (new_rail_type != rail_type) {
					break;
				}
				rail_type = new_rail_type;
			}

			// move to the next tile
			prev_tile = tile;
			prev_trackdir = trackdir;
			prev_tile_type = tile_type;

			tile = F.m_new_tile;
			trackdir = new_td;
			tile_type = GetTileType(tile);

			target_seen = Yapf().PfDetectDestination(tile, trackdir);

			// reversing in depot penalty
			if (tile == prev_tile) {
				segment_cost += Yapf().PfGetSettings().rail_depot_reverse_penalty;
				break;
			}

			// if we skipped some tunnel tiles, add their cost
			segment_cost += YAPF_TILE_LENGTH * F.m_tiles_skipped;

			// add penalty for skipped station tiles
			if (F.m_is_station)
			{
				if (target_seen) {
					// it is our destination station
					uint platform_length = F.m_tiles_skipped + 1;
					segment_cost += PlatformLengthPenalty(platform_length);
				} else {
					// station is not our destination station, apply penalty for skipped platform tiles
					segment_cost += Yapf().PfGetSettings().rail_station_penalty * F.m_tiles_skipped;
				}
			}

			// add min/max speed penalties
			int min_speed = 0;
			int max_speed = F.GetSpeedLimit(&min_speed);
			if (max_speed < v->max_speed)
				segment_cost += YAPF_TILE_LENGTH * (v->max_speed - max_speed) / v->max_speed;
			if (min_speed > v->max_speed)
				segment_cost += YAPF_TILE_LENGTH * (min_speed - v->max_speed);

			// finish if we already exceeded the maximum cost
			if (m_max_cost > 0 && (parent_cost + first_tile_cost + segment_cost) > m_max_cost) {
				return false;
			}

			if (first_tile_cost == 0) {
				// we just have done first tile
				first_tile_cost = segment_cost;
				segment_cost = 0;

				// look if we can reuse existing (cached) segment cost
				if (n.m_segment->m_cost >= 0) {
					// reuse the cached segment cost
					break;
				}
			}
			// segment cost was not filled yes, we have not cached it yet
			n.SetLastTileTrackdir(tile, trackdir);

		} // while (true)

		if (first_tile_cost == 0) {
			// we have just finished first tile
			first_tile_cost = segment_cost;
			segment_cost = 0;
		}

		// do we have cached segment cost?
		if (n.m_segment->m_cost >= 0) {
			// reuse the cached segment cost
			segment_cost = n.m_segment->m_cost;
		} else {
			// save segment cost
			n.m_segment->m_cost = segment_cost;

			// save end of segment back to the node
			n.SetLastTileTrackdir(tile, trackdir);
		}

		// special costs for the case we have reached our target
		if (target_seen) {
			n.flags_u.flags_s.m_targed_seen = true;
			if (n.flags_u.flags_s.m_last_signal_was_red) {
				if (n.m_last_red_signal_type == SIGTYPE_EXIT) {
					// last signal was red pre-signal-exit
					extra_cost += Yapf().PfGetSettings().rail_lastred_exit_penalty;
				} else {
					// last signal was red, but not exit
					extra_cost += Yapf().PfGetSettings().rail_lastred_penalty;
				}
			}
		}

		// total node cost
		n.m_cost = parent_cost + first_tile_cost + segment_cost + extra_cost;

		return !n.m_segment->flags_u.flags_s.m_end_of_line;
	}

	FORCEINLINE bool CanUseGlobalCache(Node& n) const
	{
		return (n.m_parent != NULL)
			&& (n.m_parent->m_num_signals_passed >= m_sig_look_ahead_costs.Size());
	}

	FORCEINLINE void ConnectNodeToCachedData(Node& n, CachedData& ci)
	{
		n.m_segment = &ci;
		if (n.m_segment->m_cost < 0) {
			n.m_segment->m_last_tile = n.m_key.m_tile;
			n.m_segment->m_last_td = n.m_key.m_td;
		}
	}

};



#endif /* YAPF_COSTRAIL_HPP */
