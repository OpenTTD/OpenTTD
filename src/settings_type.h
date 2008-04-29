/* $Id$ */

/** @file settings_type.h Types related to global configuration settings. */

#ifndef SETTINGS_TYPE_H
#define SETTINGS_TYPE_H

#include "yapf/yapf_settings.h"
#include "date_type.h"
#include "town_type.h"

#define GAME_DIFFICULTY_NUM 18

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
	GDType town_council_tolerance; ///< minimum required town ratings to be allowed to demolish stuff
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

/* Pointer to one of the two _opt OR _opt_newgame structs */
extern GameOptions *_opt_ptr;

struct Patches {
	bool modified_catchment;            ///< different-size catchment areas
	bool vehicle_speed;                 ///< show vehicle speed
	bool build_on_slopes;               ///< allow building on slopes
	bool mammoth_trains;                ///< allow very long trains
	bool join_stations;                 ///< allow joining of train stations
	bool sg_full_load_any;              ///< new full load calculation, any cargo must be full read from pre v93 savegames
	bool improved_load;                 ///< improved loading algorithm
	bool gradual_loading;               ///< load vehicles gradually
	byte station_spread;                ///< amount a station may spread
	bool inflation;                     ///< disable inflation
	bool selectgoods;                   ///< only send the goods to station if a train has been there
	bool longbridges;                   ///< allow 100 tile long bridges
	bool gotodepot;                     ///< allow goto depot in orders
	uint8 raw_industry_construction;    ///< Type of (raw) industry construction (none, "normal", prospecting)
	bool multiple_industry_per_town;    ///< allow many industries of the same type per town
	bool same_industry_close;           ///< allow same type industries to be built close to each other
	bool lost_train_warn;               ///< if a train can't find its destination, show a warning
	uint8 order_review_system;
	bool train_income_warn;             ///< if train is generating little income, show a warning
	bool status_long_date;              ///< always show long date in status bar
	bool signal_side;                   ///< show signals on right side
	bool show_finances;                 ///< show finances at end of year
	bool sg_new_nonstop;                ///< ttdpatch compatible nonstop handling read from pre v93 savegames
	bool new_nonstop;                   ///< ttdpatch compatible nonstop handling
	bool roadveh_queue;                 ///< buggy road vehicle queueing
	bool autoscroll;                    ///< scroll when moving mouse to the edge.
	byte errmsg_duration;               ///< duration of error message
	byte land_generator;                ///< the landscape generator
	byte oil_refinery_limit;            ///< distance oil refineries allowed from map edge
	byte snow_line_height;              ///< a number 0-15 that configured snow line height
	byte tgen_smoothness;               ///< how rough is the terrain from 0-3
	uint32 generation_seed;             ///< noise seed for world generation
	byte tree_placer;                   ///< the tree placer algorithm
	byte heightmap_rotation;            ///< rotation director for the heightmap
	byte se_flat_world_height;          ///< land height a flat world gets in SE
	bool bribe;                         ///< enable bribing the local authority
	bool nonuniform_stations;           ///< allow nonuniform train stations
	bool adjacent_stations;             ///< allow stations to be built directly adjacent to other stations
	bool always_small_airport;          ///< always allow small airports
	bool realistic_acceleration;        ///< realistic acceleration for trains
	bool wagon_speed_limits;            ///< enable wagon speed limits
	bool forbid_90_deg;                 ///< forbid trains to make 90 deg turns
	bool no_servicing_if_no_breakdowns; ///< dont send vehicles to depot when breakdowns are disabled
	bool link_terraform_toolbar;        ///< display terraform toolbar when displaying rail, road, water and airport toolbars
	bool reverse_scroll;                ///< Right-Click-Scrolling scrolls in the opposite direction
	bool smooth_scroll;                 ///< Smooth scroll viewports
	bool disable_elrails;               ///< when true, the elrails are disabled
	bool measure_tooltip;               ///< Show a permanent tooltip when dragging tools
	byte liveries;                      ///< Options for displaying company liveries, 0=none, 1=self, 2=all
	bool prefer_teamchat;               ///< Choose the chat message target with <ENTER>, true=all players, false=your team
	uint8 advanced_vehicle_list;        ///< Use the "advanced" vehicle list
	uint8 loading_indicators;           ///< Show loading indicators
	uint8 default_rail_type;            ///< The default rail type for the rail GUI

	uint8 toolbar_pos;                  ///< position of toolbars, 0=left, 1=center, 2=right
	uint8 window_snap_radius;           ///< Windows snap at each other if closer than this

	bool always_build_infrastructure;   ///< Always allow building of infrastructure, even when you do not have the vehicles for it
	UnitID max_trains;                  ///< max trains in game per player (these are 16bit because the unitnumber field can't hold more)
	UnitID max_roadveh;                 ///< max trucks in game per player
	UnitID max_aircraft;                ///< max planes in game per player
	UnitID max_ships;                   ///< max ships in game per player

	bool servint_ispercent;             ///< service intervals are in percents
	uint16 servint_trains;              ///< service interval for trains
	uint16 servint_roadveh;             ///< service interval for road vehicles
	uint16 servint_aircraft;            ///< service interval for aircraft
	uint16 servint_ships;               ///< service interval for ships

	uint8 pathfinder_for_trains;        ///< the pathfinder to use for trains
	uint8 pathfinder_for_roadvehs;      ///< the pathfinder to use for roadvehicles
	uint8 pathfinder_for_ships;         ///< the pathfinder to use for ships

	uint8 plane_speed;                  ///< divisor for speed of aircraft

	bool autorenew;
	int16 autorenew_months;
	int32 autorenew_money;

	byte pf_maxdepth;                      ///< maximum recursion depth when searching for a train route for new pathfinder
	uint16 pf_maxlength;                   ///< maximum length when searching for a train route for new pathfinder

	bool bridge_pillars;                   ///< show bridge pillars for high bridges

	bool ai_disable_veh_train;             ///< disable types for AI
	bool ai_disable_veh_roadveh;           ///< disable types for AI
	bool ai_disable_veh_aircraft;          ///< disable types for AI
	bool ai_disable_veh_ship;              ///< disable types for AI
	Year starting_year;                    ///< starting date
	Year ending_year;                      ///< end of the game (just show highscore)
	Year colored_news_year;                ///< when does newspaper become colored?

	bool keep_all_autosave;                ///< name the autosave in a different way.
	bool autosave_on_exit;                 ///< save an autosave when you quit the game, but do not ask "Do you really want to quit?"
	byte max_num_autosaves;                ///< controls how many autosavegames are made before the game starts to overwrite (names them 0 to max_num_autosaves - 1)
	bool extra_dynamite;                   ///< extra dynamite
	bool road_stop_on_town_road;           ///< allow building of drive-through road stops on town owned roads

	bool never_expire_vehicles;            ///< never expire vehicles
	byte extend_vehicle_life;              ///< extend vehicle life by this many years

	bool auto_euro;                        ///< automatically switch to euro in 2002
	bool serviceathelipad;                 ///< service helicopters at helipads automatically (no need to send to depot)
	bool smooth_economy;                   ///< smooth economy
	bool allow_shares;                     ///< allow the buying/selling of shares
	byte dist_local_authority;             ///< distance for town local authority, default 20

	byte wait_oneway_signal;               ///< waitingtime in days before a oneway signal
	byte wait_twoway_signal;               ///< waitingtime in days before a twoway signal

	uint8 map_x;                           ///< Size of map
	uint8 map_y;

	byte drag_signals_density;             ///< many signals density
	Year semaphore_build_before;           ///< Build semaphore signals automatically before this year
	bool ainew_active;                     ///< Is the new AI active?
	bool ai_in_multiplayer;                ///< Do we allow AIs in multiplayer

	/*
	 * New Path Finding
	 */
	bool new_pathfinding_all;              ///< Use the newest pathfinding algorithm for all

	/**
	 * The maximum amount of search nodes a single NPF run should take. This
	 * limit should make sure performance stays at acceptable levels at the cost
	 * of not being perfect anymore. This will probably be fixed in a more
	 * sophisticated way sometime soon
	 */
	uint32 npf_max_search_nodes;

	uint32 npf_rail_firstred_penalty;      ///< The penalty for when the first signal is red (and it is not an exit or combo signal)
	uint32 npf_rail_firstred_exit_penalty; ///< The penalty for when the first signal is red (and it is an exit or combo signal)
	uint32 npf_rail_lastred_penalty;       ///< The penalty for when the last signal is red
	uint32 npf_rail_station_penalty;       ///< The penalty for station tiles
	uint32 npf_rail_slope_penalty;         ///< The penalty for sloping upwards
	uint32 npf_rail_curve_penalty;         ///< The penalty for curves
	uint32 npf_rail_depot_reverse_penalty; ///< The penalty for reversing in depots
	uint32 npf_buoy_penalty;               ///< The penalty for going over (through) a buoy
	uint32 npf_water_curve_penalty;        ///< The penalty for curves
	uint32 npf_road_curve_penalty;         ///< The penalty for curves
	uint32 npf_crossing_penalty;           ///< The penalty for level crossings
	uint32 npf_road_drive_through_penalty; ///< The penalty for going through a drive-through road stop

	bool population_in_label;              ///< Show the population of a town in his label?

	uint8 freight_trains;                  ///< Value to multiply the weight of cargo by

	/** YAPF settings */
	YapfSettings  yapf;

	uint8 right_mouse_btn_emulation;

	uint8 scrollwheel_scrolling;
	uint8 scrollwheel_multiplier;

	uint8 town_growth_rate;      ///< Town growth rate
	uint8 larger_towns;          ///< The number of cities to build. These start off larger and grow twice as fast
	uint8 initial_city_size;     ///< Multiplier for the initial size of the cities compared to towns

	bool pause_on_newgame;       ///< Whether to start new games paused or not.

	TownLayoutByte town_layout;  ///< Select town layout

	bool timetabling;            ///< Whether to allow timetabling.
	bool timetable_in_ticks;     ///< Whether to show the timetable in ticks rather than days.

	bool autoslope;              ///< Allow terraforming under things.

	bool mod_road_rebuild;       ///< Roadworks remove unneccesary RoadBits

	bool exclusive_rights;       ///< allow buying exclusive rights
	bool give_money;             ///< allow giving other players money

	bool enable_signal_gui;      ///< Show the signal GUI when the signal button is pressed

	bool dynamic_engines;    ///< Enable dynamic allocation of engine data
};

extern Patches _patches;

/** The patch values that are used for new games and/or modified in config file */
extern Patches _patches_newgame;

#endif /* SETTINGS_TYPE_H */
