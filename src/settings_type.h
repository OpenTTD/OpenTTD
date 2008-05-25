/* $Id$ */

/** @file settings_type.h Types related to global configuration settings. */

#ifndef SETTINGS_TYPE_H
#define SETTINGS_TYPE_H

#include "date_type.h"
#include "town_type.h"
#include "transport_type.h"

enum {
	GAME_DIFFICULTY_AI_NUMBER,
	GAME_DIFFICULTY_AI_STARTTIME,
	GAME_DIFFICULTY_TOWN_NUMBER,
	GAME_DIFFICULTY_INDUSTRIE_NUMBER,
	GAME_DIFFICULTY_MAX_LOAN,
	GAME_DIFFICULTY_INITIAL_INTEREST,
	GAME_DIFFICULTY_VEHICLE_COST,
	GAME_DIFFICULTY_AI_SPEED,
	GAME_DIFFICULTY_AI_INTELLIGENCE,       ///< no longer in use
	GAME_DIFFICULTY_VEHICLES_BREAKDOWN,
	GAME_DIFFICULTY_SUBSIDY_MULTIPLIER,
	GAME_DIFFICULTY_CONSTRUCTION_COST,
	GAME_DIFFICULTY_TYPE_TERRAIN,
	GAME_DIFFICULTY_SEALAKE_NUMBER,
	GAME_DIFFICULTY_ECONOMY,
	GAME_DIFFICULTY_LINE_REVERSEMODE,
	GAME_DIFFICULTY_DISASTERS,
	GAME_DIFFICULTY_TOWNCOUNCIL_TOLERANCE, ///< minimum required town ratings to be allowed to demolish stuff
	GAME_DIFFICULTY_NUM,
};

/** Specific type for Game Difficulty to ease changing the type */
typedef uint16 GDType;
struct GameDifficulty {
	GDType max_no_competitors;
	GDType competitor_start_time;
	GDType number_towns;
	GDType number_industries;
	GDType max_loan;
	GDType initial_interest;
	GDType vehicle_costs;
	GDType competitor_speed;
	GDType competitor_intelligence; ///< no longer in use
	GDType vehicle_breakdowns;
	GDType subsidy_multiplier;
	GDType construction_cost;
	GDType terrain_type;
	GDType quantity_sea_lakes;
	GDType economy;
	GDType line_reverse_mode;
	GDType disasters;
	GDType town_council_tolerance;  ///< minimum required town ratings to be allowed to demolish stuff
};

struct GameOptions {
	GameDifficulty diff;
	byte diff_level;
	byte currency;
	byte units;
	byte town_name;
	byte landscape;
	byte snow_line;
	byte autosave;
	byte road_side;
};

/* These are the options for the current game
 * either ingame, or loaded. Also used for networking games */
extern GameOptions _opt;

/* These are the default options for a new game */
extern GameOptions _opt_newgame;

/** Settings related to the GUI and other stuff that is not saved in the savegame. */
struct GUISettings {
	bool   vehicle_speed;                    ///< show vehicle speed
	bool   sg_full_load_any;                 ///< new full load calculation, any cargo must be full read from pre v93 savegames
	bool   lost_train_warn;                  ///< if a train can't find its destination, show a warning
	uint8  order_review_system;              ///< perform order reviews on vehicles
	bool   train_income_warn;                ///< if train is generating little income, show a warning
	bool   status_long_date;                 ///< always show long date in status bar
	bool   show_finances;                    ///< show finances at end of year
	bool   sg_new_nonstop;                   ///< ttdpatch compatible nonstop handling read from pre v93 savegames
	bool   new_nonstop;                      ///< ttdpatch compatible nonstop handling
	bool   autoscroll;                       ///< scroll when moving mouse to the edge
	byte   errmsg_duration;                  ///< duration of error message
	bool   link_terraform_toolbar;           ///< display terraform toolbar when displaying rail, road, water and airport toolbars
	bool   reverse_scroll;                   ///< right-Click-Scrolling scrolls in the opposite direction
	bool   smooth_scroll;                    ///< smooth scroll viewports
	bool   measure_tooltip;                  ///< show a permanent tooltip when dragging tools
	byte   liveries;                         ///< options for displaying company liveries, 0=none, 1=self, 2=all
	bool   prefer_teamchat;                  ///< choose the chat message target with <ENTER>, true=all players, false=your team
	uint8  advanced_vehicle_list;            ///< use the "advanced" vehicle list
	uint8  loading_indicators;               ///< show loading indicators
	uint8  default_rail_type;                ///< the default rail type for the rail GUI
	uint8  toolbar_pos;                      ///< position of toolbars, 0=left, 1=center, 2=right
	uint8  window_snap_radius;               ///< windows snap at each other if closer than this
	bool   always_build_infrastructure;      ///< always allow building of infrastructure, even when you do not have the vehicles for it
	bool   keep_all_autosave;                ///< name the autosave in a different way
	bool   autosave_on_exit;                 ///< save an autosave when you quit the game, but do not ask "Do you really want to quit?"
	byte   max_num_autosaves;                ///< controls how many autosavegames are made before the game starts to overwrite (names them 0 to max_num_autosaves - 1)
	bool   population_in_label;              ///< show the population of a town in his label?
	uint8  right_mouse_btn_emulation;        ///< should we emulate right mouse clicking?
	uint8  scrollwheel_scrolling;            ///< scrolling using the scroll wheel?
	uint8  scrollwheel_multiplier;           ///< how much 'wheel' per incoming event from the OS?
	bool   pause_on_newgame;                 ///< whether to start new games paused or not
	bool   enable_signal_gui;                ///< show the signal GUI when the signal button is pressed
	Year   ending_year;                      ///< end of the game (just show highscore)
	Year   colored_news_year;                ///< when does newspaper become colored?
	bool   timetable_in_ticks;               ///< whether to show the timetable in ticks rather than days
	bool   bridge_pillars;                   ///< show bridge pillars for high bridges
	bool   auto_euro;                        ///< automatically switch to euro in 2002
	byte   drag_signals_density;             ///< many signals density
	Year   semaphore_build_before;           ///< build semaphore signals automatically before this year
	bool   autorenew;                        ///< should autorenew be enabled for new companies?
	int16  autorenew_months;                 ///< how many months from EOL of vehicles should autorenew trigger for new companies?
	int32  autorenew_money;                  ///< how much money before autorenewing for new companies?
};

/** Settings related to the creation of games. */
struct GameCreationSettings {
	uint32 generation_seed;                  ///< noise seed for world generation
	Year   starting_year;                    ///< starting date
	uint8  map_x;                            ///< X size of map
	uint8  map_y;                            ///< Y size of map
	byte   land_generator;                   ///< the landscape generator
	byte   oil_refinery_limit;               ///< distance oil refineries allowed from map edge
	byte   snow_line_height;                 ///< a number 0-15 that configured snow line height
	byte   tgen_smoothness;                  ///< how rough is the terrain from 0-3
	byte   tree_placer;                      ///< the tree placer algorithm
	byte   heightmap_rotation;               ///< rotation director for the heightmap
	byte   se_flat_world_height;             ///< land height a flat world gets in SE
};

/** Settings related to construction in-game */
struct ConstructionSettings {
	bool   build_on_slopes;                  ///< allow building on slopes
	bool   autoslope;                        ///< allow terraforming under things
	bool   longbridges;                      ///< allow 100 tile long bridges
	bool   signal_side;                      ///< show signals on right side
	bool   extra_dynamite;                   ///< extra dynamite
	bool   road_stop_on_town_road;           ///< allow building of drive-through road stops on town owned roads
	uint8  raw_industry_construction;        ///< type of (raw) industry construction (none, "normal", prospecting)
};

/** Settings related to the AI. */
struct AISettings {
	bool   ainew_active;                     ///< is the new AI active?
	bool   ai_in_multiplayer;                ///< so we allow AIs in multiplayer
	bool   ai_disable_veh_train;             ///< disable types for AI
	bool   ai_disable_veh_roadveh;           ///< disable types for AI
	bool   ai_disable_veh_aircraft;          ///< disable types for AI
	bool   ai_disable_veh_ship;              ///< disable types for AI
};

/** Settings related to the old pathfinder. */
struct OPFSettings {
	uint16 pf_maxlength;                     ///< maximum length when searching for a train route for new pathfinder
	byte   pf_maxdepth;                      ///< maximum recursion depth when searching for a train route for new pathfinder
};

/** Settings related to the new pathfinder. */
struct NPFSettings {
	/**
	 * The maximum amount of search nodes a single NPF run should take. This
	 * limit should make sure performance stays at acceptable levels at the cost
	 * of not being perfect anymore.
	 */
	uint32 npf_max_search_nodes;

	uint32 npf_rail_firstred_penalty;        ///< the penalty for when the first signal is red (and it is not an exit or combo signal)
	uint32 npf_rail_firstred_exit_penalty;   ///< the penalty for when the first signal is red (and it is an exit or combo signal)
	uint32 npf_rail_lastred_penalty;         ///< the penalty for when the last signal is red
	uint32 npf_rail_station_penalty;         ///< the penalty for station tiles
	uint32 npf_rail_slope_penalty;           ///< the penalty for sloping upwards
	uint32 npf_rail_curve_penalty;           ///< the penalty for curves
	uint32 npf_rail_depot_reverse_penalty;   ///< the penalty for reversing in depots
	uint32 npf_buoy_penalty;                 ///< the penalty for going over (through) a buoy
	uint32 npf_water_curve_penalty;          ///< the penalty for curves
	uint32 npf_road_curve_penalty;           ///< the penalty for curves
	uint32 npf_crossing_penalty;             ///< the penalty for level crossings
	uint32 npf_road_drive_through_penalty;   ///< the penalty for going through a drive-through road stop
};

/** Settings related to the yet another pathfinder. */
struct YAPFSettings {
	bool   disable_node_optimization;        ///< whether to use exit-dir instead of trackdir in node key
	uint32 max_search_nodes;                 ///< stop path-finding when this number of nodes visited
	bool   ship_use_yapf;                    ///< use YAPF for ships
	bool   road_use_yapf;                    ///< use YAPF for road
	bool   rail_use_yapf;                    ///< use YAPF for rail
	uint32 road_slope_penalty;               ///< penalty for up-hill slope
	uint32 road_curve_penalty;               ///< penalty for curves
	uint32 road_crossing_penalty;            ///< penalty for level crossing
	uint32 road_stop_penalty;                ///< penalty for going through a drive-through road stop
	bool   rail_firstred_twoway_eol;         ///< treat first red two-way signal as dead end
	uint32 rail_firstred_penalty;            ///< penalty for first red signal
	uint32 rail_firstred_exit_penalty;       ///< penalty for first red exit signal
	uint32 rail_lastred_penalty;             ///< penalty for last red signal
	uint32 rail_lastred_exit_penalty;        ///< penalty for last red exit signal
	uint32 rail_station_penalty;             ///< penalty for non-target station tile
	uint32 rail_slope_penalty;               ///< penalty for up-hill slope
	uint32 rail_curve45_penalty;             ///< penalty for curve
	uint32 rail_curve90_penalty;             ///< penalty for 90-deg curve
	uint32 rail_depot_reverse_penalty;       ///< penalty for reversing in the depot
	uint32 rail_crossing_penalty;            ///< penalty for level crossing
	uint32 rail_look_ahead_max_signals;      ///< max. number of signals taken into consideration in look-ahead load balancer
	int32  rail_look_ahead_signal_p0;        ///< constant in polynomial penalty function
	int32  rail_look_ahead_signal_p1;        ///< constant in polynomial penalty function
	int32  rail_look_ahead_signal_p2;        ///< constant in polynomial penalty function

	uint32 rail_longer_platform_penalty;           ///< penalty for longer  station platform than train
	uint32 rail_longer_platform_per_tile_penalty;  ///< penalty for longer  station platform than train (per tile)
	uint32 rail_shorter_platform_penalty;          ///< penalty for shorter station platform than train
	uint32 rail_shorter_platform_per_tile_penalty; ///< penalty for shorter station platform than train (per tile)
};

/** Settings related to all pathfinders. */
struct PathfinderSettings {
	uint8  pathfinder_for_trains;            ///< the pathfinder to use for trains
	uint8  pathfinder_for_roadvehs;          ///< the pathfinder to use for roadvehicles
	uint8  pathfinder_for_ships;             ///< the pathfinder to use for ships
	bool   new_pathfinding_all;              ///< use the newest pathfinding algorithm for all

	bool   roadveh_queue;                    ///< buggy road vehicle queueing
	bool   forbid_90_deg;                    ///< forbid trains to make 90 deg turns

	byte   wait_oneway_signal;               ///< waitingtime in days before a oneway signal
	byte   wait_twoway_signal;               ///< waitingtime in days before a twoway signal

	OPFSettings  opf;                        ///< pathfinder settings for the old pathfinder
	NPFSettings  npf;                        ///< pathfinder settings for the new pathfinder
	YAPFSettings yapf;                       ///< pathfinder settings for the yet another pathfinder
};

/** Settings related to orders. */
struct OrderSettings {
	bool   improved_load;                    ///< improved loading algorithm
	bool   gradual_loading;                  ///< load vehicles gradually
	bool   selectgoods;                      ///< only send the goods to station if a train has been there
	bool   gotodepot;                        ///< allow goto depot in orders
	bool   no_servicing_if_no_breakdowns;    ///< dont send vehicles to depot when breakdowns are disabled
	bool   timetabling;                      ///< whether to allow timetabling
	bool   serviceathelipad;                 ///< service helicopters at helipads automatically (no need to send to depot)
};

/** Settings related to vehicles. */
struct VehicleSettings {
	bool   mammoth_trains;                   ///< allow very long trains
	bool   realistic_acceleration;           ///< realistic acceleration for trains
	bool   wagon_speed_limits;               ///< enable wagon speed limits
	bool   disable_elrails;                  ///< when true, the elrails are disabled
	UnitID max_trains;                       ///< max trains in game per player
	UnitID max_roadveh;                      ///< max trucks in game per player
	UnitID max_aircraft;                     ///< max planes in game per player
	UnitID max_ships;                        ///< max ships in game per player
	bool   servint_ispercent;                ///< service intervals are in percents
	uint16 servint_trains;                   ///< service interval for trains
	uint16 servint_roadveh;                  ///< service interval for road vehicles
	uint16 servint_aircraft;                 ///< service interval for aircraft
	uint16 servint_ships;                    ///< service interval for ships
	uint8  plane_speed;                      ///< divisor for speed of aircraft
	uint8  freight_trains;                   ///< value to multiply the weight of cargo by
	bool   dynamic_engines;                  ///< enable dynamic allocation of engine data
	bool   never_expire_vehicles;            ///< never expire vehicles
	byte   extend_vehicle_life;              ///< extend vehicle life by this many years
};

/** Settings related to the economy. */
struct EconomySettings {
	bool   inflation;                        ///< disable inflation
	bool   bribe;                            ///< enable bribing the local authority
	bool   smooth_economy;                   ///< smooth economy
	bool   allow_shares;                     ///< allow the buying/selling of shares
	byte   dist_local_authority;             ///< distance for town local authority, default 20
	bool   exclusive_rights;                 ///< allow buying exclusive rights
	bool   give_money;                       ///< allow giving other players money
	bool   mod_road_rebuild;                 ///< roadworks remove unneccesary RoadBits
	bool   multiple_industry_per_town;       ///< allow many industries of the same type per town
	bool   same_industry_close;              ///< allow same type industries to be built close to each other
	uint8  town_growth_rate;                 ///< town growth rate
	uint8  larger_towns;                     ///< the number of cities to build. These start off larger and grow twice as fast
	uint8  initial_city_size;                ///< multiplier for the initial size of the cities compared to towns
	TownLayoutByte town_layout;              ///< select town layout
	bool   station_noise_level;              ///< build new airports when the town noise level is still within accepted limits
	uint16 town_noise_population[3];         ///< population to base decision on noise evaluation (@see town_council_tolerance)
};

/** Settings related to stations. */
struct StationSettings {
	bool   modified_catchment;               ///< different-size catchment areas
	bool   join_stations;                    ///< allow joining of train stations
	bool   nonuniform_stations;              ///< allow nonuniform train stations
	bool   adjacent_stations;                ///< allow stations to be built directly adjacent to other stations
	bool   always_small_airport;             ///< always allow small airports
	byte   station_spread;                   ///< amount a station may spread
};

/** All settings together. */
struct Settings {
	GUISettings          gui;                ///< settings related to the GUI
	GameCreationSettings game_creation;      ///< settings used during the creation of a game (map)
	ConstructionSettings construction;       ///< construction of things in-game
	AISettings           ai;                 ///< what may the AI do?
	PathfinderSettings   pf;                 ///< settings for all pathfinders
	OrderSettings        order;              ///< settings related to orders
	VehicleSettings      vehicle;            ///< options for vehicles
	EconomySettings      economy;            ///< settings to change the economy
	StationSettings      station;            ///< settings related to station management
};

extern Settings _settings;

/** The patch values that are used for new games and/or modified in config file */
extern Settings _settings_newgame;

#endif /* SETTINGS_TYPE_H */
