/* $Id$ */

/** @file yapf_costrail.hpp */

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

	/* Structure used inside PfCalcCost() to keep basic tile information. */
	struct TILE {
		TileIndex   tile;
		Trackdir    td;
		TileType    tile_type;
		RailType    rail_type;

		TILE()
		{
			tile = INVALID_TILE;
			td = INVALID_TRACKDIR;
			tile_type = MP_VOID;
			rail_type = INVALID_RAILTYPE;
		}

		TILE(TileIndex tile, Trackdir td)
		{
			this->tile = tile;
			this->td = td;
			this->tile_type = GetTileType(tile);
			this->rail_type = GetTileRailType(tile);
		}

		TILE(const TILE &src)
		{
			tile = src.tile;
			td = src.td;
			tile_type = src.tile_type;
			rail_type = src.rail_type;
		}
	};

protected:
	int           m_max_cost;
	CBlobT<int>   m_sig_look_ahead_costs;
	bool          m_disable_cache;

public:
	bool          m_stopped_on_first_two_way_signal;
protected:

	static const int s_max_segment_cost = 10000;

	CYapfCostRailT()
		: m_max_cost(0)
		, m_disable_cache(false)
		, m_stopped_on_first_two_way_signal(false)
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
		assert(IsValidTrackdir(td1));
		assert(IsValidTrackdir(td2));
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

	/** Return one tile cost (base cost + level crossing penalty). */
	FORCEINLINE int OneTileCost(TileIndex& tile, Trackdir trackdir)
	{
		int cost = 0;
		// set base cost
		if (IsDiagonalTrackdir(trackdir)) {
			cost += YAPF_TILE_LENGTH;
			switch (GetTileType(tile)) {
				case MP_ROAD:
					/* Increase the cost for level crossings */
					if (IsLevelCrossing(tile))
						cost += Yapf().PfGetSettings().rail_crossing_penalty;
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
				n.m_segment->m_end_segment_reason |= ESRB_DEAD_END;
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
						n.m_segment->m_end_segment_reason |= ESRB_DEAD_END;
						Yapf().m_stopped_on_first_two_way_signal = true;
						return -1;
					}
					SignalType sig_type = GetSignalType(tile, TrackdirToTrack(trackdir));
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
		assert(v->type == VEH_TRAIN);
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
	FORCEINLINE bool PfCalcCost(Node &n, const TrackFollower *tf)
	{
		assert(!n.flags_u.flags_s.m_targed_seen);
		assert(tf->m_new_tile == n.m_key.m_tile);
		assert((TrackdirToTrackdirBits(n.m_key.m_td) & tf->m_new_td_bits) != TRACKDIR_BIT_NONE);

		CPerfStart perf_cost(Yapf().m_perf_cost);

		/* Does the node have some parent node? */
		bool has_parent = (n.m_parent != NULL);

		/* Do we already have a cached segment? */
		CachedData &segment = *n.m_segment;
		bool is_cached_segment = (segment.m_cost >= 0);

		int parent_cost = has_parent ? n.m_parent->m_cost : 0;

		/* Each node cost contains 2 or 3 main components:
		 *  1. Transition cost - cost of the move from previous node (tile):
		 *    - curve cost (or zero for straight move)
		 *  2. Tile cost:
		 *    - base tile cost
		 *      - YAPF_TILE_LENGTH for diagonal tiles
		 *      - YAPF_TILE_CORNER_LENGTH for non-diagonal tiles
		 *    - tile penalties
		 *      - tile slope penalty (upward slopes)
		 *      - red signal penalty
		 *      - level crossing penalty
		 *      - speed-limit penalty (bridges)
		 *      - station platform penalty
		 *      - penalty for reversing in the depot
		 *      - etc.
		 *  3. Extra cost (applies to the last node only)
		 *    - last red signal penalty
		 *    - penalty for too long or too short platform on the destination station
		 */
		int transition_cost = 0;
		int extra_cost = 0;

		/* Segment: one or more tiles connected by contiguous tracks of the same type.
		 * Each segment cost includes 'Tile cost' for all its tiles (including the first
		 * and last), and the 'Transition cost' between its tiles. The first transition
		 * cost of segment entry (move from the 'parent' node) is not included!
		 */
		int segment_entry_cost = 0;
		int segment_cost = 0;

		const Vehicle* v = Yapf().GetVehicle();

		// start at n.m_key.m_tile / n.m_key.m_td and walk to the end of segment
		TILE cur(n.m_key.m_tile, n.m_key.m_td);

		// the previous tile will be needed for transition cost calculations
		TILE prev = !has_parent ? TILE() : TILE(n.m_parent->GetLastTile(), n.m_parent->GetLastTrackdir());

		EndSegmentReasonBits end_segment_reason = ESRB_NONE;

		TrackFollower tf_local(v, &Yapf().m_perf_ts_cost);

		if (!has_parent) {
			/* We will jump to the middle of the cost calculator assuming that segment cache is not used. */
			assert(!is_cached_segment);
			/* Skip the first transition cost calculation. */
			goto no_entry_cost;
		}

		for (;;) {
			/* Transition cost (cost of the move from previous tile) */
			transition_cost = Yapf().CurveCost(prev.td, cur.td);

			/* First transition cost counts against segment entry cost, other transitions
			 * inside segment will come to segment cost (and will be cached) */
			if (segment_cost == 0) {
				/* We just entered the loop. First transition cost goes to segment entry cost)*/
				segment_entry_cost = transition_cost;
				transition_cost = 0;

				/* It is the right time now to look if we can reuse the cached segment cost. */
				if (is_cached_segment) {
					/* Yes, we already know the segment cost. */
					segment_cost = segment.m_cost;
					/* We know also the reason why the segment ends. */
					end_segment_reason = segment.m_end_segment_reason;
					/* We will need also some information about the last signal (if it was red). */
					if (segment.m_last_signal_tile != INVALID_TILE) {
						assert(HasSignalOnTrackdir(segment.m_last_signal_tile, segment.m_last_signal_td));
						SignalState sig_state = GetSignalStateByTrackdir(segment.m_last_signal_tile, segment.m_last_signal_td);
						bool is_red = (sig_state == SIGNAL_STATE_RED);
						n.flags_u.flags_s.m_last_signal_was_red = is_red;
						if (is_red) {
							n.m_last_red_signal_type = GetSignalType(segment.m_last_signal_tile, TrackdirToTrack(segment.m_last_signal_td));
						}
					}
					/* No further calculation needed. */
					cur = TILE(n.GetLastTile(), n.GetLastTrackdir());
					break;
				}
			} else {
				/* Other than first transition cost count as the regular segment cost. */
				segment_cost += transition_cost;
			}

no_entry_cost: // jump here at the beginning if the node has no parent (it is the first node)

			/* All other tile costs will be calculated here. */
			segment_cost += Yapf().OneTileCost(cur.tile, cur.td);

			/* If we skipped some tunnel/bridge/station tiles, add their base cost */
			segment_cost += YAPF_TILE_LENGTH * tf->m_tiles_skipped;

			/* Slope cost. */
			segment_cost += Yapf().SlopeCost(cur.tile, cur.td);

			/* Signal cost (routine can modify segment data). */
			segment_cost += Yapf().SignalCost(n, cur.tile, cur.td);
			end_segment_reason = segment.m_end_segment_reason;

			/* Tests for 'potential target' reasons to close the segment. */
			if (cur.tile == prev.tile) {
				/* Penalty for reversing in a depot. */
				assert(IsRailDepot(cur.tile));
				segment_cost += Yapf().PfGetSettings().rail_depot_reverse_penalty;
				/* We will end in this pass (depot is possible target) */
				end_segment_reason |= ESRB_DEPOT;

			} else if (tf->m_is_station) {
				/* Station penalties. */
				uint platform_length = tf->m_tiles_skipped + 1;
				/* We don't know yet if the station is our target or not. Act like
				 * if it is pass-through station (not our destination). */
				segment_cost += Yapf().PfGetSettings().rail_station_penalty * platform_length;
				/* We will end in this pass (station is possible target) */
				end_segment_reason |= ESRB_STATION;

			} else if (cur.tile_type == MP_RAILWAY && IsRailWaypoint(cur.tile)) {
				/* Waypoint is also a good reason to finish. */
				end_segment_reason |= ESRB_WAYPOINT;
			}

			/* Apply min/max speed penalties only when inside the look-ahead radius. Otherwise
			 * it would cause desync in MP. */
			if (n.m_num_signals_passed < m_sig_look_ahead_costs.Size())
			{
				int min_speed = 0;
				int max_speed = tf->GetSpeedLimit(&min_speed);
				if (max_speed < v->max_speed)
					extra_cost += YAPF_TILE_LENGTH * (v->max_speed - max_speed) * (4 + tf->m_tiles_skipped) / v->max_speed;
				if (min_speed > v->max_speed)
					extra_cost += YAPF_TILE_LENGTH * (min_speed - v->max_speed);
			}

			/* Finish if we already exceeded the maximum path cost (i.e. when
			 * searching for the nearest depot). */
			if (m_max_cost > 0 && (parent_cost + segment_entry_cost + segment_cost) > m_max_cost) {
				end_segment_reason |= ESRB_PATH_TOO_LONG;
			}

			/* Move to the next tile/trackdir. */
			tf = &tf_local;
			tf_local.Init(v, &Yapf().m_perf_ts_cost);

			if (!tf_local.Follow(cur.tile, cur.td)) {
				assert(tf_local.m_err != TrackFollower::EC_NONE);
				/* Can't move to the next tile (EOL?). */
				if (tf_local.m_err == TrackFollower::EC_RAIL_TYPE) {
					end_segment_reason |= ESRB_RAIL_TYPE;
				} else {
					end_segment_reason |= ESRB_DEAD_END;
				}
				break;
			}

			/* Check if the next tile is not a choice. */
			if (KillFirstBit(tf_local.m_new_td_bits) != TRACKDIR_BIT_NONE) {
				/* More than one segment will follow. Close this one. */
				end_segment_reason |= ESRB_CHOICE_FOLLOWS;
				break;
			}

			/* Gather the next tile/trackdir/tile_type/rail_type. */
			TILE next(tf_local.m_new_tile, (Trackdir)FindFirstBit2x64(tf_local.m_new_td_bits));

			/* Check the next tile for the rail type. */
			if (next.rail_type != cur.rail_type) {
				/* Segment must consist from the same rail_type tiles. */
				end_segment_reason |= ESRB_RAIL_TYPE;
				break;
			}

			/* Avoid infinite looping. */
			if (next.tile == n.m_key.m_tile && next.td == n.m_key.m_td) {
				end_segment_reason |= ESRB_INFINITE_LOOP;
				break;
			}

			if (segment_cost > s_max_segment_cost) {
				/* Potentially in the infinite loop (or only very long segment?). We should
				 * not force it to finish prematurely unless we are on a regular tile. */
				if (IsTileType(tf->m_new_tile, MP_RAILWAY)) {
					end_segment_reason |= ESRB_SEGMENT_TOO_LONG;
					break;
				}
			}

			/* Any other reason bit set? */
			if (end_segment_reason != ESRB_NONE) {
				break;
			}

			/* For the next loop set new prev and cur tile info. */
			prev = cur;
			cur = next;

		} // for (;;)

		bool target_seen = false;
		if ((end_segment_reason & ESRB_POSSIBLE_TARGET) != ESRB_NONE) {
			/* Depot, station or waypoint. */
			if (Yapf().PfDetectDestination(cur.tile, cur.td)) {
				/* Destination found. */
				target_seen = true;
			}
		}

		/* Update the segment if needed. */
		if (!is_cached_segment) {
			/* Write back the segment information so it can be reused the next time. */
			segment.m_cost = segment_cost;
			segment.m_end_segment_reason = end_segment_reason & ESRB_CACHED_MASK;
			/* Save end of segment back to the node. */
			n.SetLastTileTrackdir(cur.tile, cur.td);
		}

		/* Do we have an excuse why not to continue pathfinding in this direction? */
		if (!target_seen && (end_segment_reason & ESRB_ABORT_PF_MASK) != ESRB_NONE) {
			/* Reason to not continue. Stop this PF branch. */
			return false;
		}

		/* Special costs for the case we have reached our target. */
		if (target_seen) {
			n.flags_u.flags_s.m_targed_seen = true;
			/* Last-red and last-red-exit penalties. */
			if (n.flags_u.flags_s.m_last_signal_was_red) {
				if (n.m_last_red_signal_type == SIGTYPE_EXIT) {
					// last signal was red pre-signal-exit
					extra_cost += Yapf().PfGetSettings().rail_lastred_exit_penalty;
				} else {
					// last signal was red, but not exit
					extra_cost += Yapf().PfGetSettings().rail_lastred_penalty;
				}
			}

			/* Station platform-length penalty. */
			if ((end_segment_reason & ESRB_STATION) != ESRB_NONE) {
				Station *st = GetStationByTile(n.GetLastTile());
				assert(st != NULL);
				uint platform_length = st->GetPlatformLength(n.GetLastTile(), ReverseDiagDir(TrackdirToExitdir(n.GetLastTrackdir())));
				/* Reduce the extra cost caused by passing-station penalty (each station receives it in the segment cost). */
				extra_cost -= Yapf().PfGetSettings().rail_station_penalty * platform_length;
				/* Add penalty for the inappropriate platform length. */
				extra_cost += PlatformLengthPenalty(platform_length);
			}
		}

		// total node cost
		n.m_cost = parent_cost + segment_entry_cost + segment_cost + extra_cost;

		return true;
	}

	FORCEINLINE bool CanUseGlobalCache(Node& n) const
	{
		return !m_disable_cache
			&& (n.m_parent != NULL)
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

	void DisableCache(bool disable)
	{
		m_disable_cache = disable;
	}
};



#endif /* YAPF_COSTRAIL_HPP */
