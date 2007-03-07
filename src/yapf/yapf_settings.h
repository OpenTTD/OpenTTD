/* $Id$ */

/** @file yapf_settings.h */

#if !defined(YAPF_SETTINGS_H) || defined(YS_DEF)

# ifndef  YAPF_SETTINGS_H
#  define  YAPF_SETTINGS_H
# endif

# ifndef YS_DEF
/*
 *  if YS_DEF is not defined, we will only do following declaration:
 *  struct YapfSettings {
 *    bool   disable_node_optimization;
 *    uint32 max_search_nodes;
 *    .... all other yapf related settings ...
 *  };
 *
 *  otherwise we will just expand YS_DEF_xx macros and then #undef them
 */
#  define YS_DEF_BEGIN struct YapfSettings {
#  define YS_DEF(type, name) type name;
#  define YS_DEF_END };

# endif /* !YS_DEF */

# ifndef   YS_DEF_BEGIN
#  define  YS_DEF_BEGIN
# endif // YS_DEF_BEGIN

# ifndef   YS_DEF_END
#  define  YS_DEF_END
# endif // YS_DEF_END

YS_DEF_BEGIN
	YS_DEF(bool  , disable_node_optimization)  ///< whether to use exit-dir instead of trackdir in node key
	YS_DEF(uint32, max_search_nodes)           ///< stop path-finding when this number of nodes visited
	YS_DEF(bool  , ship_use_yapf)              ///< use YAPF for ships
	YS_DEF(bool  , road_use_yapf)              ///< use YAPF for road
	YS_DEF(bool  , rail_use_yapf)              ///< use YAPF for rail
	YS_DEF(uint32, road_slope_penalty)         ///< penalty for up-hill slope
	YS_DEF(uint32, road_curve_penalty)         ///< penalty for curves
	YS_DEF(uint32, road_crossing_penalty)      ///< penalty for level crossing
	YS_DEF(uint32, road_stop_penalty)          ///< penalty for going through a drive-through road stop
	YS_DEF(bool  , rail_firstred_twoway_eol)   ///< treat first red two-way signal as dead end
	YS_DEF(uint32, rail_firstred_penalty)      ///< penalty for first red signal
	YS_DEF(uint32, rail_firstred_exit_penalty) ///< penalty for first red exit signal
	YS_DEF(uint32, rail_lastred_penalty)       ///< penalty for last red signal
	YS_DEF(uint32, rail_lastred_exit_penalty)  ///< penalty for last red exit signal
	YS_DEF(uint32, rail_station_penalty)       ///< penalty for non-target station tile
	YS_DEF(uint32, rail_slope_penalty)         ///< penalty for up-hill slope
	YS_DEF(uint32, rail_curve45_penalty)       ///< penalty for curve
	YS_DEF(uint32, rail_curve90_penalty)       ///< penalty for 90-deg curve
	YS_DEF(uint32, rail_depot_reverse_penalty) ///< penalty for reversing in the depot
	YS_DEF(uint32, rail_crossing_penalty)      ///< penalty for level crossing
	YS_DEF(uint32, rail_look_ahead_max_signals)///< max. number of signals taken into consideration in look-ahead load balancer
	YS_DEF(int32 , rail_look_ahead_signal_p0)  ///< constant in polynomial penalty function
	YS_DEF(int32 , rail_look_ahead_signal_p1)  ///< constant in polynomial penalty function
	YS_DEF(int32 , rail_look_ahead_signal_p2)  ///< constant in polynomial penalty function

	YS_DEF(uint32, rail_longer_platform_penalty)           ///< penalty for longer  station platform than train
	YS_DEF(uint32, rail_longer_platform_per_tile_penalty)  ///< penalty for longer  station platform than train (per tile)
	YS_DEF(uint32, rail_shorter_platform_penalty)          ///< penalty for shorter station platform than train
	YS_DEF(uint32, rail_shorter_platform_per_tile_penalty) ///< penalty for shorter station platform than train (per tile)
YS_DEF_END

#undef YS_DEF_BEGIN
#undef YS_DEF
#undef YS_DEF_END

#endif /* !YAPF_SETTINGS_H || YS_DEF */
