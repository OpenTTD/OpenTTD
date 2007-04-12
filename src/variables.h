/* $Id$ */

/** @file variables.h */

#ifndef VARIABLES_H
#define VARIABLES_H

#include "yapf/yapf_settings.h"

/* ********* START OF SAVE REGION */
#if !defined(MAX_PATH)
# define MAX_PATH 260
#endif

#include "gfx.h"

/* Prices and also the fractional part. */
VARDEF Prices _price;
VARDEF uint16 _price_frac[NUM_PRICES];

VARDEF uint32 _cargo_payment_rates[NUM_CARGO];
VARDEF uint16 _cargo_payment_rates_frac[NUM_CARGO];

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
VARDEF GameOptions _opt;

/* These are the default options for a new game */
VARDEF GameOptions _opt_newgame;

/* Pointer to one of the two _opt OR _opt_newgame structs */
VARDEF GameOptions *_opt_ptr;

/* Amount of game ticks */
VARDEF uint16 _tick_counter;

/* This one is not used anymore. */
VARDEF VehicleID _vehicle_id_ctr_day;

/* Skip aging of cargo? */
VARDEF byte _age_cargo_skip_counter;

/* Position in tile loop */
VARDEF TileIndex _cur_tileloop_tile;

/* Also save scrollpos_x, scrollpos_y and zoom */
VARDEF uint16 _disaster_delay;

/* Determines what station to operate on in the
 *  tick handler. */
VARDEF uint16 _station_tick_ctr;

VARDEF uint32 _random_seeds[2][2];

/* Iterator through all towns in OnTick_Town */
VARDEF uint32 _cur_town_ctr;
/* Frequency iterator at the same place */
VARDEF uint32 _cur_town_iter;

VARDEF uint _cur_player_tick_index;
VARDEF uint _next_competitor_start;

/* Determines how often to run the tree loop */
VARDEF byte _trees_tick_ctr;

/* Keep track of current game position */
VARDEF int _saved_scrollpos_x;
VARDEF int _saved_scrollpos_y;
VARDEF byte _saved_scrollpos_zoom;

/* ********* END OF SAVE REGION */

struct Patches {
	bool modified_catchment;            // different-size catchment areas
	bool vehicle_speed;                 // show vehicle speed
	bool build_on_slopes;               // allow building on slopes
	bool mammoth_trains;                // allow very long trains
	bool join_stations;                 // allow joining of train stations
	bool full_load_any;                 // new full load calculation, any cargo must be full
	bool improved_load;                 // improved loading algorithm
	bool gradual_loading;               // load vehicles gradually
	byte station_spread;                // amount a station may spread
	bool inflation;                     // disable inflation
	bool selectgoods;                   // only send the goods to station if a train has been there
	bool longbridges;                   // allow 100 tile long bridges
	bool gotodepot;                     // allow goto depot in orders
	bool build_rawmaterial_ind;         // allow building raw material industries
	bool multiple_industry_per_town;    // allow many industries of the same type per town
	bool same_industry_close;           // allow same type industries to be built close to each other
	bool lost_train_warn;               // if a train can't find its destination, show a warning
	uint8 order_review_system;
	bool train_income_warn;             // if train is generating little income, show a warning
	bool status_long_date;              // always show long date in status bar
	bool signal_side;                   // show signals on right side
	bool show_finances;                 // show finances at end of year
	bool new_nonstop;                   // ttdpatch compatible nonstop handling
	bool roadveh_queue;                 // buggy road vehicle queueing
	bool autoscroll;                    // scroll when moving mouse to the edge.
	byte errmsg_duration;               // duration of error message
	byte land_generator;                // the landscape generator
	byte oil_refinery_limit;            // distance oil refineries allowed from map edge
	byte snow_line_height;              // a number 0-15 that configured snow line height
	byte tgen_smoothness;               // how rough is the terrain from 0-3
	uint32 generation_seed;             // noise seed for world generation
	byte tree_placer;                   // the tree placer algorithm
	byte heightmap_rotation;            // rotation director for the heightmap
	byte se_flat_world_height;          // land height a flat world gets in SE
	bool bribe;                         // enable bribing the local authority
	bool nonuniform_stations;           // allow nonuniform train stations
	bool always_small_airport;          // always allow small airports
	bool realistic_acceleration;        // realistic acceleration for trains
	bool wagon_speed_limits;            // enable wagon speed limits
	bool forbid_90_deg;                 // forbid trains to make 90 deg turns
	bool invisible_trees;               // don't show trees when buildings are transparent
	bool no_servicing_if_no_breakdowns; // dont send vehicles to depot when breakdowns are disabled
	bool link_terraform_toolbar;        // display terraform toolbar when displaying rail, road, water and airport toolbars
	bool reverse_scroll;                // Right-Click-Scrolling scrolls in the opposite direction
	bool disable_elrails;               // when true, the elrails are disabled
	bool measure_tooltip;               // Show a permanent tooltip when dragging tools
	byte liveries;                      // Options for displaying company liveries, 0=none, 1=self, 2=all
	bool prefer_teamchat;               // Choose the chat message target with <ENTER>, true=all players, false=your team

	uint8 toolbar_pos;                  // position of toolbars, 0=left, 1=center, 2=right
	uint8 window_snap_radius;           // Windows snap at each other if closer than this

	UnitID max_trains;                  // max trains in game per player (these are 16bit because the unitnumber field can't hold more)
	UnitID max_roadveh;                 // max trucks in game per player
	UnitID max_aircraft;                // max planes in game per player
	UnitID max_ships;                   // max ships in game per player

	bool servint_ispercent;             // service intervals are in percents
	uint16 servint_trains;              // service interval for trains
	uint16 servint_roadveh;             // service interval for road vehicles
	uint16 servint_aircraft;            // service interval for aircraft
	uint16 servint_ships;               // service interval for ships

	bool autorenew;
	int16 autorenew_months;
	int32 autorenew_money;

	byte pf_maxdepth;                   // maximum recursion depth when searching for a train route for new pathfinder
	uint16 pf_maxlength;                // maximum length when searching for a train route for new pathfinder


	bool bridge_pillars;                // show bridge pillars for high bridges

	bool ai_disable_veh_train;          // disable types for AI
	bool ai_disable_veh_roadveh;        // disable types for AI
	bool ai_disable_veh_aircraft;       // disable types for AI
	bool ai_disable_veh_ship;           // disable types for AI
	Year starting_year;                 // starting date
	Year ending_year;                   // end of the game (just show highscore)
	Year colored_news_year;             // when does newspaper become colored?

	bool keep_all_autosave;             // name the autosave in a different way.
	bool autosave_on_exit;              // save an autosave when you quit the game, but do not ask "Do you really want to quit?"
	byte max_num_autosaves;             // controls how many autosavegames are made before the game starts to overwrite (names them 0 to max_num_autosaves - 1)
	bool extra_dynamite;                // extra dynamite
	bool road_stop_on_town_road;        // allow building of drive-through road stops on town owned roads

	bool never_expire_vehicles;         // never expire vehicles
	byte extend_vehicle_life;           // extend vehicle life by this many years

	bool auto_euro;                     // automatically switch to euro in 2002
	bool serviceathelipad;              // service helicopters at helipads automatically (no need to send to depot)
	bool smooth_economy;                // smooth economy
	bool allow_shares;                  // allow the buying/selling of shares
	byte dist_local_authority;          // distance for town local authority, default 20

	byte wait_oneway_signal;            // waitingtime in days before a oneway signal
	byte wait_twoway_signal;            // waitingtime in days before a twoway signal

	uint8 map_x;                        // Size of map
	uint8 map_y;

	byte drag_signals_density;          // many signals density
	Year semaphore_build_before;        // Build semaphore signals automatically before this year
	bool ainew_active;                  // Is the new AI active?
	bool ai_in_multiplayer;             // Do we allow AIs in multiplayer

	/*
	 * New Path Finding
	 */
	bool new_pathfinding_all; /* Use the newest pathfinding algorithm for all */

	/**
	 * The maximum amount of search nodes a single NPF run should take. This
	 * limit should make sure performance stays at acceptable levels at the cost
	 * of not being perfect anymore. This will probably be fixed in a more
	 * sophisticated way sometime soon
	 */
	uint32 npf_max_search_nodes;

	uint32 npf_rail_firstred_penalty;      // The penalty for when the first signal is red (and it is not an exit or combo signal)
	uint32 npf_rail_firstred_exit_penalty; // The penalty for when the first signal is red (and it is an exit or combo signal)
	uint32 npf_rail_lastred_penalty;       // The penalty for when the last signal is red
	uint32 npf_rail_station_penalty;       // The penalty for station tiles
	uint32 npf_rail_slope_penalty;         // The penalty for sloping upwards
	uint32 npf_rail_curve_penalty;         // The penalty for curves
	uint32 npf_rail_depot_reverse_penalty; // The penalty for reversing in depots
	uint32 npf_buoy_penalty;               // The penalty for going over (through) a buoy
	uint32 npf_water_curve_penalty;        // The penalty for curves
	uint32 npf_road_curve_penalty;         // The penalty for curves
	uint32 npf_crossing_penalty;           // The penalty for level crossings
	uint32 npf_road_drive_through_penalty; // The penalty for going through a drive-through road stop

	bool population_in_label; // Show the population of a town in his label?

	uint8 freight_trains; // Value to multiply the weight of cargo by

	/** YAPF settings */
	YapfSettings  yapf;

	uint8 scrollwheel_scrolling;
	uint8 scrollwheel_multiplier;

	uint8 town_growth_rate; ///< Town growth rate
	uint8 larger_towns;     ///< 1 in the specified number of towns will grow twice as fast
};

VARDEF Patches _patches;


struct Cheat {
	bool been_used; // has this cheat been used before?
	bool value;     // tells if the bool cheat is active or not
};


/* WARNING! Do _not_ remove entries in Cheats struct or change the order
 * of the existing ones! Would break downward compatibility.
 * Only add new entries at the end of the struct! */

struct Cheats {
	Cheat magic_bulldozer;  // dynamite industries, unmovables
	Cheat switch_player;    // change to another player
	Cheat money;            // get rich
	Cheat crossing_tunnels; // allow tunnels that cross each other
	Cheat build_in_pause;   // build while in pause mode
	Cheat no_jetcrash;      // no jet will crash on small airports anymore
	Cheat switch_climate;
	Cheat change_date;      // changes date ingame
	Cheat setup_prod;       // setup raw-material production in game
	Cheat dummy;            // empty cheat (enable running el-engines on normal rail)
};

VARDEF Cheats _cheats;

struct Paths {
	char *personal_dir;  // includes cfg file and save folder
	char *game_data_dir; // includes data, gm, lang
	char *data_dir;
	char *gm_dir;
	char *lang_dir;
	char *save_dir;
	char *autosave_dir;
	char *scenario_dir;
	char *heightmap_dir;
	char *second_data_dir;
};

VARDEF Paths _paths;

/* NOSAVE: Used in palette animations only, not really important. */
VARDEF int _timer_counter;


VARDEF uint32 _frame_counter;

VARDEF bool _is_old_ai_player; // current player is an oldAI player? (enables a lot of cheats..)

VARDEF bool _do_autosave;
VARDEF int _autosave_ctr;

VARDEF byte _display_opt;
VARDEF byte _transparent_opt;
VARDEF int _caret_timer;
VARDEF uint32 _news_display_opt;
VARDEF bool _news_ticker_sound;

VARDEF StringID _error_message;
VARDEF int32 _additional_cash_required;

VARDEF uint32 _decode_parameters[20];

VARDEF bool _rightclick_emulate;

/* IN/OUT parameters to commands */
VARDEF byte _yearly_expenses_type;
VARDEF TileIndex _terraform_err_tile;
VARDEF TileIndex _build_tunnel_endtile;
VARDEF bool _generating_world;

/* Deals with the type of the savegame, independent of extension */
struct SmallFiosItem {
	int mode;             // savegame/scenario type (old, new)
	char name[MAX_PATH];  // name
	char title[255];      // internal name of the game
};

/* Used when switching from the intro menu. */
VARDEF byte _switch_mode;
VARDEF StringID _switch_mode_errorstr;
VARDEF SmallFiosItem _file_to_saveload;



VARDEF Vehicle *_place_clicked_vehicle;

VARDEF char _ini_videodriver[32], _ini_musicdriver[32], _ini_sounddriver[32];

/** Information about a language */
struct Language {
	char *name; ///< The internal name of the language
	char *file; ///< The name of the language as it appears on disk
};

/** Used for dynamic language support */
struct DynamicLanguages {
	int num;                         ///< Number of languages
	int curr;                        ///< Currently selected language index
	char curr_file[MAX_PATH];        ///< Currently selected language file name without path (needed for saving the filename of the loaded language).
	StringID dropdown[MAX_LANG + 1]; ///< List of languages in the settings gui
	Language ent[MAX_LANG];          ///< Information about the languages
};

extern DynamicLanguages _dynlang; // defined in strings.cpp

VARDEF int _num_resolutions;
VARDEF uint16 _resolutions[32][2];
VARDEF uint16 _cur_resolution[2];

VARDEF char _savegame_format[8];

VARDEF char *_config_file;
VARDEF char *_highscore_file;
VARDEF char *_log_file;


static inline void SetDParamX(uint32 *s, uint n, uint32 v)
{
	s[n] = v;
}

static inline uint32 GetDParamX(const uint32 *s, uint n)
{
	return s[n];
}

static inline void SetDParam(uint n, uint32 v)
{
	assert(n < lengthof(_decode_parameters));
	_decode_parameters[n] = v;
}

static inline void SetDParam64(uint n, uint64 v)
{
	assert(n + 1 < lengthof(_decode_parameters));
	_decode_parameters[n + 0] = v & 0xffffffff;
	_decode_parameters[n + 1] = v >> 32;
}

static inline uint32 GetDParam(uint n)
{
	assert(n < lengthof(_decode_parameters));
	return _decode_parameters[n];
}

/* Used to bind a C string name to a dparam number.
 * NOTE: This has a short lifetime. You can't
 *       use this string much later or it will be gone. */
void SetDParamStr(uint n, const char *str);

/** This function takes a C-string and allocates a temporary string ID.
 * The duration of the bound string is valid only until the next acll to GetString,
 * so be careful. */
StringID BindCString(const char *str);


#define COPY_IN_DPARAM(offs,src,num) memcpy(_decode_parameters + offs, src, sizeof(uint32) * (num))
#define COPY_OUT_DPARAM(dst,offs,num) memcpy(dst,_decode_parameters + offs, sizeof(uint32) * (num))


#define SET_EXPENSES_TYPE(x) _yearly_expenses_type = x;

/* landscape.cpp */
extern const byte _tileh_to_sprite[32];
extern const Slope _inclined_tileh[16];

extern const TileTypeProcs * const _tile_type_procs[16];

/* misc */
VARDEF char _screenshot_name[128];
VARDEF byte _vehicle_design_names;

/* Forking stuff */
VARDEF bool _dedicated_forks;

#endif /* VARIABLES_H */
