#ifndef VARIABLES_H
#define VARIABLES_H

#include "player.h"
//enum { DPARAM_SIZE = 32 };


// ********* START OF SAVE REGION

#if !defined(MAX_PATH)
# define MAX_PATH 260
#endif

// Prices and also the fractional part.
VARDEF Prices _price;
VARDEF uint16 _price_frac[NUM_PRICES];

VARDEF uint32 _cargo_payment_rates[NUM_CARGO];
VARDEF uint16 _cargo_payment_rates_frac[NUM_CARGO];

typedef struct {
	GameDifficulty diff;
	byte diff_level;
	byte currency;
	bool kilometers;
	byte town_name;
	byte landscape;
	byte snow_line;
	byte autosave;
	byte road_side;
} GameOptions;

// These are the options for the current game
VARDEF GameOptions _opt;

// These are the options for the new game
VARDEF GameOptions _new_opt;

// Current date
VARDEF uint16 _date;
VARDEF uint16 _date_fract;

// Amount of game ticks
VARDEF uint16 _tick_counter;

// Used when calling OnNewDay
VARDEF VehicleID _vehicle_id_ctr_day;

// Skip aging of cargo?
VARDEF byte _age_cargo_skip_counter;

// Available aircraft types
VARDEF byte _avail_aircraft;

// Position in tile loop
VARDEF TileIndex _cur_tileloop_tile;

// Also save scrollpos_x, scrollpos_y and zoom
VARDEF uint16 _disaster_delay;

// Determines what station to operate on in the
//  tick handler.
VARDEF uint16 _station_tick_ctr;

VARDEF uint32 _random_seeds[2][2];
VARDEF uint32 _player_seeds[MAX_PLAYERS][2];

// Iterator through all towns in OnTick_Town
VARDEF byte _cur_town_ctr;

VARDEF uint _cur_player_tick_index;
VARDEF uint _next_competitor_start;

// Determines how often to run the tree loop
VARDEF byte _trees_tick_ctr;

// Keep track of current game position
VARDEF int _saved_scrollpos_x;
VARDEF int _saved_scrollpos_y;
VARDEF byte _saved_scrollpos_zoom;

// ********* END OF SAVE REGION

typedef struct Patches {
	bool vehicle_speed;			// show vehicle speed
	bool build_on_slopes;		// allow building on slopes
	bool mammoth_trains;		// allow very long trains
	bool join_stations;			// allow joining of train stations
	bool full_load_any;			// new full load calculation, any cargo must be full
	byte station_spread;		// amount a station may spread
	bool inflation;					// disable inflation
	bool selectgoods;       // only send the goods to station if a train has been there
	bool longbridges;				// allow 100 tile long bridges
	bool gotodepot;					// allow goto depot in orders
	bool build_rawmaterial_ind;	 // allow building raw material industries
	bool multiple_industry_per_town;	// allow many industries of the same type per town
	bool same_industry_close;	// allow same type industries to be built close to each other
	uint16 lost_train_days;	// if a train doesn't switch order in this amount of days, a train is lost warning is shown
	uint8 order_review_system;
	bool train_income_warn; // if train is generating little income, show a warning
	bool status_long_date;		// always show long date in status bar
	bool signal_side;				// show signals on right side
	bool show_finances;			// show finances at end of year
	bool new_nonstop;				// ttdpatch compatible nonstop handling
	bool roadveh_queue;			// buggy road vehicle queueing
	bool autoscroll;				// scroll when moving mouse to the edge.
	byte errmsg_duration;		// duration of error message
	byte snow_line_height;	// a number 0-15 that configured snow line height
	bool bribe;							// enable bribing the local authority
	bool new_depot_finding;	// use new algorithm to find a depot.
	bool nonuniform_stations;// allow nonuniform train stations
	bool always_small_airport; // always allow small airports
	bool realistic_acceleration; // realistic acceleration for trains
	bool invisible_trees; // don't show trees when buildings are transparent

	uint8 toolbar_pos;			// position of toolbars, 0=left, 1=center, 2=right

	byte max_trains;				//max trains in game per player (these are 8bit because the unitnumber field can't hold more)
	byte max_roadveh;				//max trucks in game per player
	byte max_aircraft;			//max planes in game per player
	byte max_ships;					//max ships in game per player

	bool servint_ispercent;	// service intervals are in percents
	uint16 servint_trains;	// service interval for trains
	uint16 servint_roadveh;	// service interval for road vehicles
	uint16 servint_aircraft;// service interval for aircraft
	uint16 servint_ships;		// service interval for ships

	bool autorenew;
	int16 autorenew_months;
	int32 autorenew_money;

	bool new_pathfinding;		// use optimized pathfinding algoritm for trains
	byte pf_maxdepth;				// maximum recursion depth when searching for a train route for new pathfinder
	uint16 pf_maxlength;		// maximum length when searching for a train route for new pathfinder


	bool bridge_pillars;		// show bridge pillars for high bridges

	bool ai_disable_veh_train;		// disable types for AI
	bool ai_disable_veh_roadveh;		// disable types for AI
	bool ai_disable_veh_aircraft;		// disable types for AI
	bool ai_disable_veh_ship;		// disable types for AI
	uint32 starting_date;		// starting date
	uint32 colored_news_date; // when does newspaper become colored?

	bool keep_all_autosave;		// name the autosave in a different way.
	bool extra_dynamite;			// extra dynamite

	bool never_expire_vehicles; // never expire vehicles
	byte extend_vehicle_life;	// extend vehicle life by this many years

	bool auto_euro;						// automatically switch to euro in 2002
	bool serviceathelipad;	// service helicopters at helipads automatically (no need to send to depot)
	bool smooth_economy;		// smooth economy
	byte dist_local_authority;		// distance for town local authority, default 20

	byte wait_oneway_signal;	//waitingtime in days before a oneway signal
	byte wait_twoway_signal;	//waitingtime in days before a twoway signal

	byte drag_signals_density; // many signals density
	bool ainew_active;  // Is the new AI active?
} Patches;

VARDEF Patches _patches;


typedef struct Cheat {
	bool been_used;	// has this cheat been used before?
	byte value;			// active?
} Cheat;


// WARNING! Do _not_ remove entries in Cheats struct or change the order
// of the existing ones! Would break downward compatibility.
// Only add new entries at the end of the struct!

typedef struct Cheats {
	Cheat magic_bulldozer;		// dynamite industries, unmovables
	Cheat switch_player;			// change to another player
	Cheat money;							// get rich
	Cheat crossing_tunnels;		// allow tunnels that cross each other
	Cheat build_in_pause;			// build while in pause mode
	Cheat no_jetcrash;				// no jet will crash on small airports anymore
	Cheat switch_climate;
	Cheat change_date;				//changes date ingame
} Cheats;

VARDEF Cheats _cheats;

typedef struct Paths {
	char *personal_dir;  // includes cfg file and save folder
	char *game_data_dir; // includes data, gm, lang
	char *data_dir;
	char *gm_dir;
	char *lang_dir;
	char *save_dir;
	char *autosave_dir;
	char *scenario_dir;
} Paths;

VARDEF Paths _path;

// Which options struct does options modify?
VARDEF GameOptions *_opt_mod_ptr;

// NOSAVE: Used in palette animations only, not really important.
VARDEF int _timer_counter;


// NOSAVE: can be determined from _date
VARDEF byte _cur_year;
VARDEF byte _cur_month;

// NOSAVE: can be determined from player structs
VARDEF byte _player_colors[MAX_PLAYERS];

VARDEF bool _in_state_game_loop;
VARDEF uint32 _frame_counter;

VARDEF uint32 _frame_counter_max; // for networking, this is the frame that we are not allowed to execute yet.
VARDEF uint32 _frame_counter_srv; // for networking, this is the last known framecounter of the server. it is always less than frame_counter_max.

// networking settings
VARDEF bool _network_available;  // is network mode available?
VARDEF uint32 _network_ip_list[10]; // Network IPs
VARDEF uint16 _network_game_count;

VARDEF uint _network_client_port;
VARDEF uint _network_server_port;

VARDEF uint16 _network_sync_freq;
VARDEF uint16 _network_ahead_frames;

VARDEF uint32 _sync_seed_1, _sync_seed_2;

VARDEF bool _is_ai_player; // current player is an AI player? - Can be removed if new AI is done

VARDEF bool _do_autosave;
VARDEF int _autosave_ctr;

VARDEF byte _local_player;
VARDEF byte _display_opt;
VARDEF byte _pause;
VARDEF int _caret_timer;
VARDEF uint16 _news_display_opt;
VARDEF byte _game_mode;

VARDEF StringID _error_message;
VARDEF StringID _error_message_2;
VARDEF int32 _additional_cash_required;

VARDEF uint32 _decode_parameters[10];
VARDEF byte _current_player;

VARDEF int _docommand_recursive;

VARDEF uint32 _pressed_key; // Low 8 bits = ASCII, High 16 bits = keycode
VARDEF bool _ctrl_pressed;  // Is Ctrl pressed?
VARDEF bool _shift_pressed;  // Is Alt pressed?
VARDEF byte _dirkeys;				// 1=left, 2=up, 4=right, 8=down

VARDEF bool _fullscreen;
VARDEF bool _double_size;
VARDEF uint _display_hz;
VARDEF bool _force_full_redraw;
VARDEF uint _fullscreen_bpp;
VARDEF bool _fast_forward;
VARDEF bool _rightclick_emulate;

// IN/OUT parameters to commands
VARDEF byte _yearly_expenses_type;
VARDEF TileIndex _terraform_err_tile;
VARDEF uint _build_tunnel_endtile;
VARDEF bool _generating_world;
VARDEF int _new_town_size;
VARDEF uint _returned_refit_amount;

// Deals with the type of the savegame, independent of extension
typedef struct {
	int mode;						// savegame/scenario type (old, new)
	byte name[MAX_PATH];	// name
} SmallFiosItem;

// Used when switching from the intro menu.
VARDEF byte _switch_mode;
VARDEF StringID _switch_mode_errorstr;
VARDEF bool _exit_game;
VARDEF SmallFiosItem _file_to_saveload;
VARDEF byte _make_screenshot;

VARDEF bool _networking;
VARDEF bool _networking_override; // dont shutdown network core when the GameMenu appears.

VARDEF bool _networking_sync; // if we use network mode and the games must stay in sync.
VARDEF bool _networking_server;
VARDEF bool _networking_queuing; // queueing only?

VARDEF byte _network_playas; // an id to play as..

VARDEF byte _get_z_hint; // used as a hint to getslopez to return the right height at a bridge.

VARDEF char *_newgrf_files[32];


VARDEF Vehicle *_place_clicked_vehicle;

VARDEF char _ini_videodriver[16], _ini_musicdriver[16], _ini_sounddriver[16];

VARDEF bool _cache_sprites;

// debug features
VARDEF char _savedump_path[64];
VARDEF uint _savedump_first, _savedump_freq, _savedump_last;
// end of debug features


typedef struct {
	char *name;
	char *file;
} DynLangEnt;

// Used for dynamic language support
typedef struct {
	int num; // number of languages
	int curr; // currently selected language index
	char curr_file[32]; // currently selected language file
	StringID dropdown[32 + 1]; // used in settings dialog
	DynLangEnt ent[32];
} DynamicLanguages;

VARDEF DynamicLanguages _dynlang;

VARDEF int _num_resolutions;
VARDEF uint16 _resolutions[32][2];
VARDEF uint16 _cur_resolution[2];

VARDEF char _screenshot_format_name[8];
VARDEF int _num_screenshot_formats, _cur_screenshot_format;

VARDEF char _savegame_format[8];

VARDEF char *_config_file;

// NOSAVE: These can be recalculated from InitializeLandscapeVariables
typedef struct {
	StringID names_s[NUM_CARGO];
	StringID names_p[NUM_CARGO];
	StringID names_long_s[NUM_CARGO];
	StringID names_long_p[NUM_CARGO];
	StringID names_short[NUM_CARGO];
	byte weights[NUM_CARGO];
	SpriteID sprites[NUM_CARGO];
	byte transit_days_1[NUM_CARGO];
	byte transit_days_2[NUM_CARGO];
	byte ai_railwagon[3][NUM_CARGO];
	byte ai_roadveh_start[NUM_CARGO];
	byte ai_roadveh_count[NUM_CARGO];
} CargoConst;

VARDEF CargoConst _cargoc;

typedef byte TownNameGenerator(byte *buf, uint32 seed);
extern TownNameGenerator * const _town_name_generators[];

#define SET_DPARAM32(n, v) (_decode_parameters[n] = (v))
#define SET_DPARAMX32(s, n, v) ((s)[n] = (v))
#define GET_DPARAM32(n) (_decode_parameters[n])

#define SET_DPARAM(n, v) (_decode_parameters[n] = (v))
#define SET_DPARAMX(s, n, v) ((s)[n] = (v))
#define GET_DPARAM(n) (_decode_parameters[n])

static void FORCEINLINE SET_DPARAM64(int n, int64 v)
{
	_decode_parameters[n] = (uint32)v;
	_decode_parameters[n+1] = (uint32)((uint64)v >> 32);
}

#if defined(TTD_LITTLE_ENDIAN)
#define SET_DPARAMX16(s, n, v) ( ((uint16*)(s+n))[0] = (v))
#define SET_DPARAMX8(s, n, v) ( ((uint8*)(s+n))[0] = (v))
#define GET_DPARAMX16(s, n) ( ((uint16*)(s+n))[0])
#define GET_DPARAMX8(s, n) ( ((uint8*)(s+n))[0])
#elif defined(TTD_BIG_ENDIAN)
#define SET_DPARAMX16(s, n, v) ( ((uint16*)(s+n))[1] = (v))
#define SET_DPARAMX8(s, n, v) ( ((uint8*)(s+n))[3] = (v))
#define GET_DPARAMX16(s, n) ( ((uint16*)(s+n))[1])
#define GET_DPARAMX8(s, n) ( ((uint8*)(s+n))[3])
#endif

#define SET_DPARAM16(n, v) SET_DPARAMX16(_decode_parameters, n, v)
#define SET_DPARAM8(n, v)  SET_DPARAMX8(_decode_parameters, n, v)
#define GET_DPARAM16(n)    GET_DPARAMX16(_decode_parameters, n)
#define GET_DPARAM8(n)     GET_DPARAMX8(_decode_parameters, n)

#define COPY_IN_DPARAM(offs,src,num) memcpy(_decode_parameters + offs, src, sizeof(uint32) * (num))
#define COPY_OUT_DPARAM(dst,offs,num) memcpy(dst,_decode_parameters + offs, sizeof(uint32) * (num))

#define INJECT_DPARAM(n) InjectDparam(n);

#define SET_EXPENSES_TYPE(x) if (x) _yearly_expenses_type=x;

/* landscape.c */
extern const byte _tileh_to_sprite[32];
extern byte _map_type_and_height[TILES_X * TILES_Y];
extern byte _map5[TILES_X * TILES_Y];
extern byte _map3_lo[TILES_X * TILES_Y];
extern byte _map3_hi[TILES_X * TILES_Y];
extern byte _map_owner[TILES_X * TILES_Y];
extern byte _map2[TILES_X * TILES_Y];
extern byte _map_extra_bits[TILES_X * TILES_Y/4];

static const byte _inclined_tileh[] = {
	3,9,3,6,12,6,12,9,
};

extern const TileTypeProcs * const _tile_type_procs[16];

/* station_cmd.c */
// there are 5 types of airport (Country (3x4) , City(6x6), Metropolitan(6x6), International(7x7), Heliport(1x1)
// will become obsolete once airports are loaded from seperate file
extern const byte _airport_size_x[5];
extern const byte _airport_size_y[5];

extern const TileIndexDiff _tileoffs_by_dir[4];

/* misc */
VARDEF byte str_buffr[512];
VARDEF char _screenshot_name[128];
VARDEF char _userstring[128];
VARDEF byte _vehicle_design_names;

VARDEF SignStruct _sign_list[40];
VARDEF SignStruct *_new_sign_struct;

VARDEF bool _ignore_wrong_grf;

/* tunnelbridge */
#define MAX_BRIDGES 13

/* Debugging levels */
VARDEF int _debug_spritecache_level;
VARDEF int _debug_misc_level;
VARDEF int _debug_grf_level;
VARDEF int _debug_ai_level;
VARDEF int _debug_net_level;

void CDECL debug(const char *s, ...);
#ifdef NO_DEBUG_MESSAGES
	#define DEBUG(name, level)
#else
	#define DEBUG(name, level) if (level == 0 || _debug_ ## name ## _level >= level) debug
#endif

#endif /* VARIABLES_H */
