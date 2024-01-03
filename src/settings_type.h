/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_type.h Types related to global configuration settings. */

#ifndef SETTINGS_TYPE_H
#define SETTINGS_TYPE_H

#include "timer/timer_game_calendar.h"
#include "economy_type.h"
#include "town_type.h"
#include "transport_type.h"
#include "network/network_type.h"
#include "company_type.h"
#include "cargotype.h"
#include "linkgraph/linkgraph_type.h"
#include "zoom_type.h"
#include "openttd.h"
#include "rail_gui.h"
#include "signal_type.h"
#include "timetable.h"

/* Used to validate sizes of "max" value in settings. */
const size_t MAX_SLE_UINT8 = UINT8_MAX;
const size_t MAX_SLE_UINT16 = UINT16_MAX;
const size_t MAX_SLE_UINT32 = UINT32_MAX;
const size_t MAX_SLE_UINT = UINT_MAX;
const size_t MAX_SLE_INT8 = INT8_MAX;
const size_t MAX_SLE_INT16 = INT16_MAX;
const size_t MAX_SLE_INT32 = INT32_MAX;
const size_t MAX_SLE_INT = INT_MAX;

/** Settings profiles and highscore tables. */
enum SettingsProfile {
	SP_BEGIN = 0,
	SP_EASY = SP_BEGIN,                       ///< Easy difficulty.
	SP_MEDIUM,                                ///< Medium difficulty.
	SP_HARD,                                  ///< Hard difficulty.

	SP_END,                                   ///< End of setting profiles.

	SP_CUSTOM = SP_END,                       ///< No profile, special "custom" highscore.
	SP_SAVED_HIGHSCORE_END,                   ///< End of saved highscore tables.

	SP_MULTIPLAYER = SP_SAVED_HIGHSCORE_END,  ///< Special "multiplayer" highscore. Not saved, always specific to the current game.
	SP_HIGHSCORE_END,                         ///< End of highscore tables.
};

/** Available industry map generation densities. */
enum IndustryDensity {
	ID_FUND_ONLY, ///< The game does not build industries.
	ID_MINIMAL,   ///< Start with just the industries that must be present.
	ID_VERY_LOW,  ///< Very few industries at game start.
	ID_LOW,       ///< Few industries at game start.
	ID_NORMAL,    ///< Normal amount of industries at game start.
	ID_HIGH,      ///< Many industries at game start.

	ID_CUSTOM,    ///< Custom number of industries.

	ID_END,       ///< Number of industry density settings.
};

/** Possible values for "use_relay_service" setting. */
enum UseRelayService : uint8_t {
	URS_NEVER = 0,
	URS_ASK,
	URS_ALLOW,
};

/** Possible values for "participate_survey" setting. */
enum ParticipateSurvey : uint8_t {
	PS_ASK = 0,
	PS_NO,
	PS_YES,
};

/** Right-click to close window actions. */
enum RightClickClose : uint8_t {
	RCC_NO = 0,
	RCC_YES,
	RCC_YES_EXCEPT_STICKY,
};

/** Settings related to the difficulty of the game */
struct DifficultySettings {
	byte   competitor_start_time;            ///< Unused value, used to load old savegames.
	byte   competitor_intelligence;          ///< Unused value, used to load old savegames.

	byte   max_no_competitors;               ///< the number of competitors (AIs)
	uint16_t competitors_interval;             ///< the interval (in minutes) between adding competitors
	byte   number_towns;                     ///< the amount of towns
	byte   industry_density;                 ///< The industry density. @see IndustryDensity
	uint32_t max_loan;                         ///< the maximum initial loan
	byte   initial_interest;                 ///< amount of interest (to pay over the loan)
	byte   vehicle_costs;                    ///< amount of money spent on vehicle running cost
	byte   competitor_speed;                 ///< the speed at which the AI builds
	byte   vehicle_breakdowns;               ///< likelihood of vehicles breaking down
	byte   subsidy_multiplier;               ///< payment multiplier for subsidized deliveries
	uint16_t subsidy_duration;                 ///< duration of subsidies
	byte   construction_cost;                ///< how expensive is building
	byte   terrain_type;                     ///< the mountainousness of the landscape
	byte   quantity_sea_lakes;               ///< the amount of seas/lakes
	bool   economy;                          ///< how volatile is the economy
	bool   line_reverse_mode;                ///< reversing at stations or not
	bool   disasters;                        ///< are disasters enabled
	byte   town_council_tolerance;           ///< minimum required town ratings to be allowed to demolish stuff
};

/** Settings relating to viewport/smallmap scrolling. */
enum ViewportScrollMode {
	VSM_VIEWPORT_RMB_FIXED, ///< Viewport moves with mouse movement on holding right mouse button, cursor position is fixed.
	VSM_MAP_RMB_FIXED,      ///< Map moves with mouse movement on holding right mouse button, cursor position is fixed.
	VSM_MAP_RMB,            ///< Map moves with mouse movement on holding right mouse button, cursor moves.
	VSM_MAP_LMB,            ///< Map moves with mouse movement on holding left mouse button, cursor moves.
	VSM_END,                ///< Number of scroll mode settings.
};

/** Settings related to the GUI and other stuff that is not saved in the savegame. */
struct GUISettings {
	bool   sg_full_load_any;                 ///< new full load calculation, any cargo must be full read from pre v93 savegames
	bool   lost_vehicle_warn;                ///< if a vehicle can't find its destination, show a warning
	uint8_t  order_review_system;              ///< perform order reviews on vehicles
	bool   vehicle_income_warn;              ///< if a vehicle isn't generating income, show a warning
	bool   show_finances;                    ///< show finances at end of year
	bool   sg_new_nonstop;                   ///< ttdpatch compatible nonstop handling read from pre v93 savegames
	bool   new_nonstop;                      ///< ttdpatch compatible nonstop handling
	uint8_t  stop_location;                    ///< what is the default stop location of trains?
	uint8_t  auto_scrolling;                   ///< scroll when moving mouse to the edge (see #ViewportAutoscrolling)
	byte   errmsg_duration;                  ///< duration of error message
	uint16_t hover_delay_ms;                   ///< time required to activate a hover event, in milliseconds
	bool   link_terraform_toolbar;           ///< display terraform toolbar when displaying rail, road, water and airport toolbars
	uint8_t  smallmap_land_colour;             ///< colour used for land and heightmap at the smallmap
	uint8_t  linkgraph_colours;                ///< linkgraph overlay colours
	uint8_t  scroll_mode;                      ///< viewport scroll mode
	bool   smooth_scroll;                    ///< smooth scroll viewports
	bool   measure_tooltip;                  ///< show a permanent tooltip when dragging tools
	byte   liveries;                         ///< options for displaying company liveries, 0=none, 1=self, 2=all
	bool   prefer_teamchat;                  ///< choose the chat message target with \<ENTER\>, true=all clients, false=your team
	uint8_t  advanced_vehicle_list;            ///< use the "advanced" vehicle list
	uint8_t  loading_indicators;               ///< show loading indicators
	uint8_t  default_rail_type;                ///< the default rail type for the rail GUI
	uint8_t  toolbar_pos;                      ///< position of toolbars, 0=left, 1=center, 2=right
	uint8_t  statusbar_pos;                    ///< position of statusbar, 0=left, 1=center, 2=right
	uint8_t  window_snap_radius;               ///< windows snap at each other if closer than this
	uint8_t  window_soft_limit;                ///< soft limit of maximum number of non-stickied non-vital windows (0 = no limit)
	ZoomLevel zoom_min;                      ///< minimum zoom out level
	ZoomLevel zoom_max;                      ///< maximum zoom out level
	ZoomLevel sprite_zoom_min;               ///< maximum zoom level at which higher-resolution alternative sprites will be used (if available) instead of scaling a lower resolution sprite
	uint32_t autosave_interval;              ///< how often should we do autosaves?
	bool   threaded_saves;                   ///< should we do threaded saves?
	bool   keep_all_autosave;                ///< name the autosave in a different way
	bool   autosave_on_exit;                 ///< save an autosave when you quit the game, but do not ask "Do you really want to quit?"
	bool   autosave_on_network_disconnect;   ///< save an autosave when you get disconnected from a network game with an error?
	uint8_t  date_format_in_default_names;     ///< should the default savegame/screenshot name use long dates (31th Dec 2008), short dates (31-12-2008) or ISO dates (2008-12-31)
	byte   max_num_autosaves;                ///< controls how many autosavegames are made before the game starts to overwrite (names them 0 to max_num_autosaves - 1)
	bool   population_in_label;              ///< show the population of a town in its label?
	uint8_t  right_mouse_btn_emulation;        ///< should we emulate right mouse clicking?
	uint8_t  scrollwheel_scrolling;            ///< scrolling using the scroll wheel?
	uint8_t  scrollwheel_multiplier;           ///< how much 'wheel' per incoming event from the OS?
	bool   timetable_arrival_departure;      ///< show arrivals and departures in vehicle timetables
	RightClickClose  right_click_wnd_close;  ///< close window with right click
	bool   pause_on_newgame;                 ///< whether to start new games paused or not
	SignalGUISettings signal_gui_mode;       ///< select which signal types are shown in the signal GUI
	SignalCycleSettings cycle_signal_types;  ///< Which signal types to cycle with the build signal tool.
	SignalType default_signal_type;          ///< The default signal type, which is set automatically by the last signal used. Not available in Settings.
	TimerGameCalendar::Year coloured_news_year; ///< when does newspaper become coloured?
	TimetableMode timetable_mode;            ///< Time units for timetables: days, seconds, or ticks
	bool   quick_goto;                       ///< Allow quick access to 'goto button' in vehicle orders window
	bool   auto_euro;                        ///< automatically switch to euro in 2002
	byte   drag_signals_density;             ///< many signals density
	bool   drag_signals_fixed_distance;      ///< keep fixed distance between signals when dragging
	TimerGameCalendar::Year semaphore_build_before; ///< build semaphore signals automatically before this year
	byte   news_message_timeout;             ///< how much longer than the news message "age" should we keep the message in the history
	bool   show_track_reservation;           ///< highlight reserved tracks.
	byte   station_numtracks;                ///< the number of platforms to default on for rail stations
	byte   station_platlength;               ///< the platform length, in tiles, for rail stations
	bool   station_dragdrop;                 ///< whether drag and drop is enabled for stations
	bool   station_show_coverage;            ///< whether to highlight coverage area
	bool   persistent_buildingtools;         ///< keep the building tools active after usage
	bool   expenses_layout;                  ///< layout of expenses window
	uint32_t last_newgrf_count;                ///< the numbers of NewGRFs we found during the last scan
	byte   missing_strings_threshold;        ///< the number of missing strings before showing the warning
	uint8_t  graph_line_thickness;             ///< the thickness of the lines in the various graph guis
	uint8_t  osk_activation;                   ///< Mouse gesture to trigger the OSK.
	byte   starting_colour;                  ///< default color scheme for the company to start a new game with
	byte   starting_colour_secondary;        ///< default secondary color scheme for the company to start a new game with
	bool   show_newgrf_name;                 ///< Show the name of the NewGRF in the build vehicle window
	bool   show_cargo_in_vehicle_lists;      ///< Show the cargoes the vehicles can carry in the list windows
	bool   auto_remove_signals;              ///< automatically remove signals when in the way during rail construction
	uint16_t refresh_rate;                     ///< How often we refresh the screen (time between draw-ticks).
	uint16_t fast_forward_speed_limit;         ///< Game speed to use when fast-forward is enabled.

	uint16_t console_backlog_timeout;          ///< the minimum amount of time items should be in the console backlog before they will be removed in ~3 seconds granularity.
	uint16_t console_backlog_length;           ///< the minimum amount of items in the console backlog before items will be removed.

	uint8_t  station_gui_group_order;          ///< the order of grouping cargo entries in the station gui
	uint8_t  station_gui_sort_by;              ///< sort cargo entries in the station gui by station name or amount
	uint8_t  station_gui_sort_order;           ///< the sort order of entries in the station gui - ascending or descending
	uint16_t network_chat_box_width_pct;       ///< width of the chat box in percent
	uint8_t  network_chat_box_height;          ///< height of the chat box in lines
	uint16_t network_chat_timeout;             ///< timeout of chat messages in seconds

	uint8_t  developer;                        ///< print non-fatal warnings in console (>= 1), copy debug output to console (== 2)
	bool   show_date_in_logs;                ///< whether to show dates in console logs
	bool   newgrf_developer_tools;           ///< activate NewGRF developer tools and allow modifying NewGRFs in an existing game
	bool   ai_developer_tools;               ///< activate AI/GS developer tools
	bool   scenario_developer;               ///< activate scenario developer: allow modifying NewGRFs in an existing game
	uint8_t  settings_restriction_mode;        ///< selected restriction mode in adv. settings GUI. @see RestrictionMode
	bool   newgrf_show_old_versions;         ///< whether to show old versions in the NewGRF list
	uint8_t  newgrf_default_palette;           ///< default palette to use for NewGRFs without action 14 palette information

	bool   scale_bevels;                     ///< bevels are scaled with GUI scale.

	/**
	 * Returns true when the user has sufficient privileges to edit newgrfs on a running game
	 * @return whether the user has sufficient privileges to edit newgrfs in an existing game
	 */
	bool UserIsAllowedToChangeNewGRFs() const
	{
		return this->scenario_developer || this->newgrf_developer_tools;
	}
};

/** Settings related to sound effects. */
struct SoundSettings {
	bool   news_ticker;                      ///< Play a ticker sound when a news item is published.
	bool   news_full;                        ///< Play sound effects associated to certain news types.
	bool   new_year;                         ///< Play sound on new year, summarising the performance during the last year.
	bool   confirm;                          ///< Play sound effect on successful constructions or other actions.
	bool   click_beep;                       ///< Beep on a random selection of buttons.
	bool   disaster;                         ///< Play disaster and accident sounds.
	bool   vehicle;                          ///< Play vehicle sound effects.
	bool   ambient;                          ///< Play ambient, industry and town sounds.
};

/** Settings related to music. */
struct MusicSettings {
	byte playlist;     ///< The playlist (number) to play
	byte music_vol;    ///< The requested music volume
	byte effect_vol;   ///< The requested effects volume
	byte custom_1[33]; ///< The order of the first custom playlist
	byte custom_2[33]; ///< The order of the second custom playlist
	bool playing;      ///< Whether music is playing
	bool shuffle;      ///< Whether to shuffle the music
};

/** Settings related to currency/unit systems. */
struct LocaleSettings {
	byte        currency;                         ///< currency we currently use
	byte        units_velocity;                   ///< unit system for velocity of trains and road vehicles
	byte        units_velocity_nautical;          ///< unit system for velocity of ships and aircraft
	byte        units_power;                      ///< unit system for power
	byte        units_weight;                     ///< unit system for weight
	byte        units_volume;                     ///< unit system for volume
	byte        units_force;                      ///< unit system for force
	byte        units_height;                     ///< unit system for height
	std::string digit_group_separator;            ///< thousand separator for non-currencies
	std::string digit_group_separator_currency;   ///< thousand separator for currencies
	std::string digit_decimal_separator;          ///< decimal separator
};

/** Settings related to news */
struct NewsSettings {
	uint8_t arrival_player;                                 ///< NewsDisplay of vehicles arriving at new stations of current player
	uint8_t arrival_other;                                  ///< NewsDisplay of vehicles arriving at new stations of other players
	uint8_t accident;                                       ///< NewsDisplay of accidents that occur
	uint8_t accident_other;                                 ///< NewsDisplay if a vehicle from another company is involved in an accident
	uint8_t company_info;                                   ///< NewsDisplay of general company information
	uint8_t open;                                           ///< NewsDisplay on new industry constructions
	uint8_t close;                                          ///< NewsDisplay about closing industries
	uint8_t economy;                                        ///< NewsDisplay on economical changes
	uint8_t production_player;                              ///< NewsDisplay of production changes of industries affecting current player
	uint8_t production_other;                               ///< NewsDisplay of production changes of industries affecting competitors
	uint8_t production_nobody;                              ///< NewsDisplay of production changes of industries affecting no one
	uint8_t advice;                                         ///< NewsDisplay on advice affecting the player's vehicles
	uint8_t new_vehicles;                                   ///< NewsDisplay of new vehicles becoming available
	uint8_t acceptance;                                     ///< NewsDisplay on changes affecting the acceptance of cargo at stations
	uint8_t subsidies;                                      ///< NewsDisplay of changes on subsidies
	uint8_t general;                                        ///< NewsDisplay of other topics
};

/** All settings related to the network. */
struct NetworkSettings {
	uint16_t      sync_freq;                                ///< how often do we check whether we are still in-sync
	uint8_t       frame_freq;                               ///< how often do we send commands to the clients
	uint16_t      commands_per_frame;                       ///< how many commands may be sent each frame_freq frames?
	uint16_t      commands_per_frame_server;                ///< how many commands may be sent each frame_freq frames? (server-originating commands)
	uint16_t      max_commands_in_queue;                    ///< how many commands may there be in the incoming queue before dropping the connection?
	uint16_t      bytes_per_frame;                          ///< how many bytes may, over a long period, be received per frame?
	uint16_t      bytes_per_frame_burst;                    ///< how many bytes may, over a short period, be received?
	uint16_t      max_init_time;                            ///< maximum amount of time, in game ticks, a client may take to initiate joining
	uint16_t      max_join_time;                            ///< maximum amount of time, in game ticks, a client may take to sync up during joining
	uint16_t      max_download_time;                        ///< maximum amount of time, in game ticks, a client may take to download the map
	uint16_t      max_password_time;                        ///< maximum amount of time, in game ticks, a client may take to enter the password
	uint16_t      max_lag_time;                             ///< maximum amount of time, in game ticks, a client may be lagging behind the server
	bool        pause_on_join;                            ///< pause the game when people join
	uint16_t      server_port;                              ///< port the server listens on
	uint16_t      server_admin_port;                        ///< port the server listens on for the admin network
	bool        server_admin_chat;                        ///< allow private chat for the server to be distributed to the admin network
	ServerGameType server_game_type;                      ///< Server type: local / public / invite-only.
	std::string server_invite_code;                       ///< Invite code to use when registering as server.
	std::string server_invite_code_secret;                ///< Secret to proof we got this invite code from the Game Coordinator.
	std::string server_name;                              ///< name of the server
	std::string server_password;                          ///< password for joining this server
	std::string rcon_password;                            ///< password for rconsole (server side)
	std::string admin_password;                           ///< password for the admin network
	std::string client_name;                              ///< name of the player (as client)
	std::string default_company_pass;                     ///< default password for new companies in encrypted form
	std::string connect_to_ip;                            ///< default for the "Add server" query
	std::string network_id;                               ///< network ID for servers
	bool        autoclean_companies;                      ///< automatically remove companies that are not in use
	uint8_t       autoclean_unprotected;                    ///< remove passwordless companies after this many months
	uint8_t       autoclean_protected;                      ///< remove the password from passworded companies after this many months
	uint8_t       autoclean_novehicles;                     ///< remove companies with no vehicles after this many months
	uint8_t       max_companies;                            ///< maximum amount of companies
	uint8_t       max_clients;                              ///< maximum amount of clients
	TimerGameCalendar::Year restart_game_year;            ///< year the server restarts
	uint8_t       min_active_clients;                       ///< minimum amount of active clients to unpause the game
	bool        reload_cfg;                               ///< reload the config file before restarting
	std::string last_joined;                              ///< Last joined server
	bool        no_http_content_downloads;                ///< do not do content downloads over HTTP
	UseRelayService use_relay_service;                    ///< Use relay service?
	ParticipateSurvey participate_survey;                 ///< Participate in the automated survey
};

/** Settings related to the creation of games. */
struct GameCreationSettings {
	uint32_t generation_seed;                  ///< noise seed for world generation
	TimerGameCalendar::Year starting_year;   ///< starting date
	TimerGameCalendar::Year ending_year;     ///< scoring end date
	uint8_t  map_x;                            ///< X size of map
	uint8_t  map_y;                            ///< Y size of map
	byte   land_generator;                   ///< the landscape generator
	byte   oil_refinery_limit;               ///< distance oil refineries allowed from map edge
	byte   snow_line_height;                 ///< the configured snow line height (deduced from "snow_coverage")
	byte   snow_coverage;                    ///< the amount of snow coverage on the map
	byte   desert_coverage;                  ///< the amount of desert coverage on the map
	byte   heightmap_height;                 ///< highest mountain for heightmap (towards what it scales)
	byte   tgen_smoothness;                  ///< how rough is the terrain from 0-3
	byte   tree_placer;                      ///< the tree placer algorithm
	byte   heightmap_rotation;               ///< rotation director for the heightmap
	byte   se_flat_world_height;             ///< land height a flat world gets in SE
	byte   town_name;                        ///< the town name generator used for town names
	byte   landscape;                        ///< the landscape we're currently in
	byte   water_borders;                    ///< bitset of the borders that are water
	uint16_t custom_town_number;               ///< manually entered number of towns
	uint16_t custom_industry_number;           ///< manually entered number of industries
	byte   variety;                          ///< variety level applied to TGP
	byte   custom_terrain_type;              ///< manually entered height for TGP to aim for
	byte   custom_sea_level;                 ///< manually entered percentage of water in the map
	byte   min_river_length;                 ///< the minimum river length
	byte   river_route_random;               ///< the amount of randomicity for the route finding
	byte   amount_of_rivers;                 ///< the amount of rivers
};

/** Settings related to construction in-game */
struct ConstructionSettings {
	uint8_t  map_height_limit;                 ///< the maximum allowed heightlevel
	bool   build_on_slopes;                  ///< allow building on slopes
	bool   autoslope;                        ///< allow terraforming under things
	uint16_t max_bridge_length;                ///< maximum length of bridges
	byte   max_bridge_height;                ///< maximum height of bridges
	uint16_t max_tunnel_length;                ///< maximum length of tunnels
	byte   train_signal_side;                ///< show signals on left / driving / right side
	bool   extra_dynamite;                   ///< extra dynamite
	bool   road_stop_on_town_road;           ///< allow building of drive-through road stops on town owned roads
	bool   road_stop_on_competitor_road;     ///< allow building of drive-through road stops on roads owned by competitors
	bool   crossing_with_competitor;         ///< allow building of level crossings with competitor roads or rails
	uint8_t  raw_industry_construction;        ///< type of (raw) industry construction (none, "normal", prospecting)
	uint8_t  industry_platform;                ///< the amount of flat land around an industry
	bool   freeform_edges;                   ///< allow terraforming the tiles at the map edges
	uint8_t  extra_tree_placement;             ///< (dis)allow building extra trees in-game
	uint8_t  command_pause_level;              ///< level/amount of commands that can't be executed while paused

	uint32_t terraform_per_64k_frames;         ///< how many tile heights may, over a long period, be terraformed per 65536 frames?
	uint16_t terraform_frame_burst;            ///< how many tile heights may, over a short period, be terraformed?
	uint32_t clear_per_64k_frames;             ///< how many tiles may, over a long period, be cleared per 65536 frames?
	uint16_t clear_frame_burst;                ///< how many tiles may, over a short period, be cleared?
	uint32_t tree_per_64k_frames;              ///< how many trees may, over a long period, be planted per 65536 frames?
	uint16_t tree_frame_burst;                 ///< how many trees may, over a short period, be planted?
	uint32_t build_object_per_64k_frames;      ///< how many tiles may, over a long period, be purchased or have objects built on them per 65536 frames?
	uint16_t build_object_frame_burst;         ///< how many tiles may, over a short period, be purchased or have objects built on them?
};

/** Settings related to the AI. */
struct AISettings {
	bool   ai_in_multiplayer;                ///< so we allow AIs in multiplayer
	bool   ai_disable_veh_train;             ///< disable types for AI
	bool   ai_disable_veh_roadveh;           ///< disable types for AI
	bool   ai_disable_veh_aircraft;          ///< disable types for AI
	bool   ai_disable_veh_ship;              ///< disable types for AI
};

/** Settings related to scripts. */
struct ScriptSettings {
	uint8_t  settings_profile;                 ///< difficulty profile to set initial settings of scripts, esp. random AIs
	uint32_t script_max_opcode_till_suspend;   ///< max opcode calls till scripts will suspend
	uint32_t script_max_memory_megabytes;      ///< limit on memory a single script instance may have allocated
};

/** Settings related to the new pathfinder. */
struct NPFSettings {
	/**
	 * The maximum amount of search nodes a single NPF run should take. This
	 * limit should make sure performance stays at acceptable levels at the cost
	 * of not being perfect anymore.
	 */
	uint32_t npf_max_search_nodes;
	uint32_t maximum_go_to_depot_penalty;      ///< What is the maximum penalty that may be endured for going to a depot

	uint32_t npf_rail_firstred_penalty;        ///< the penalty for when the first signal is red (and it is not an exit or combo signal)
	uint32_t npf_rail_firstred_exit_penalty;   ///< the penalty for when the first signal is red (and it is an exit or combo signal)
	uint32_t npf_rail_lastred_penalty;         ///< the penalty for when the last signal is red
	uint32_t npf_rail_station_penalty;         ///< the penalty for station tiles
	uint32_t npf_rail_slope_penalty;           ///< the penalty for sloping upwards
	uint32_t npf_rail_curve_penalty;           ///< the penalty for curves
	uint32_t npf_rail_depot_reverse_penalty;   ///< the penalty for reversing in depots
	uint32_t npf_rail_pbs_cross_penalty;       ///< the penalty for crossing a reserved rail track
	uint32_t npf_rail_pbs_signal_back_penalty; ///< the penalty for passing a pbs signal from the backside
	uint32_t npf_buoy_penalty;                 ///< the penalty for going over (through) a buoy
	uint32_t npf_water_curve_penalty;          ///< the penalty for curves
	uint32_t npf_road_curve_penalty;           ///< the penalty for curves
	uint32_t npf_crossing_penalty;             ///< the penalty for level crossings
	uint32_t npf_road_drive_through_penalty;   ///< the penalty for going through a drive-through road stop
	uint32_t npf_road_dt_occupied_penalty;     ///< the penalty multiplied by the fill percentage of a drive-through road stop
	uint32_t npf_road_bay_occupied_penalty;    ///< the penalty multiplied by the fill percentage of a road bay
};

/** Settings related to the yet another pathfinder. */
struct YAPFSettings {
	bool   disable_node_optimization;        ///< whether to use exit-dir instead of trackdir in node key
	uint32_t max_search_nodes;                 ///< stop path-finding when this number of nodes visited
	uint32_t maximum_go_to_depot_penalty;      ///< What is the maximum penalty that may be endured for going to a depot
	bool   ship_use_yapf;                    ///< use YAPF for ships
	bool   road_use_yapf;                    ///< use YAPF for road
	bool   rail_use_yapf;                    ///< use YAPF for rail
	uint32_t road_slope_penalty;               ///< penalty for up-hill slope
	uint32_t road_curve_penalty;               ///< penalty for curves
	uint32_t road_crossing_penalty;            ///< penalty for level crossing
	uint32_t road_stop_penalty;                ///< penalty for going through a drive-through road stop
	uint32_t road_stop_occupied_penalty;       ///< penalty multiplied by the fill percentage of a drive-through road stop
	uint32_t road_stop_bay_occupied_penalty;   ///< penalty multiplied by the fill percentage of a road bay
	bool   rail_firstred_twoway_eol;         ///< treat first red two-way signal as dead end
	uint32_t rail_firstred_penalty;            ///< penalty for first red signal
	uint32_t rail_firstred_exit_penalty;       ///< penalty for first red exit signal
	uint32_t rail_lastred_penalty;             ///< penalty for last red signal
	uint32_t rail_lastred_exit_penalty;        ///< penalty for last red exit signal
	uint32_t rail_station_penalty;             ///< penalty for non-target station tile
	uint32_t rail_slope_penalty;               ///< penalty for up-hill slope
	uint32_t rail_curve45_penalty;             ///< penalty for curve
	uint32_t rail_curve90_penalty;             ///< penalty for 90-deg curve
	uint32_t rail_depot_reverse_penalty;       ///< penalty for reversing in the depot
	uint32_t rail_crossing_penalty;            ///< penalty for level crossing
	uint32_t rail_look_ahead_max_signals;      ///< max. number of signals taken into consideration in look-ahead load balancer
	int32_t  rail_look_ahead_signal_p0;        ///< constant in polynomial penalty function
	int32_t  rail_look_ahead_signal_p1;        ///< constant in polynomial penalty function
	int32_t  rail_look_ahead_signal_p2;        ///< constant in polynomial penalty function
	uint32_t rail_pbs_cross_penalty;           ///< penalty for crossing a reserved tile
	uint32_t rail_pbs_station_penalty;         ///< penalty for crossing a reserved station tile
	uint32_t rail_pbs_signal_back_penalty;     ///< penalty for passing a pbs signal from the backside
	uint32_t rail_doubleslip_penalty;          ///< penalty for passing a double slip switch

	uint32_t rail_longer_platform_penalty;           ///< penalty for longer  station platform than train
	uint32_t rail_longer_platform_per_tile_penalty;  ///< penalty for longer  station platform than train (per tile)
	uint32_t rail_shorter_platform_penalty;          ///< penalty for shorter station platform than train
	uint32_t rail_shorter_platform_per_tile_penalty; ///< penalty for shorter station platform than train (per tile)
	uint32_t ship_curve45_penalty;                   ///< penalty for 45-deg curve for ships
	uint32_t ship_curve90_penalty;                   ///< penalty for 90-deg curve for ships
};

/** Settings related to all pathfinders. */
struct PathfinderSettings {
	uint8_t  pathfinder_for_trains;            ///< the pathfinder to use for trains
	uint8_t  pathfinder_for_roadvehs;          ///< the pathfinder to use for roadvehicles
	uint8_t  pathfinder_for_ships;             ///< the pathfinder to use for ships
	bool   new_pathfinding_all;              ///< use the newest pathfinding algorithm for all

	bool   roadveh_queue;                    ///< buggy road vehicle queueing
	bool   forbid_90_deg;                    ///< forbid trains to make 90 deg turns

	bool   reverse_at_signals;               ///< whether to reverse at signals at all
	byte   wait_oneway_signal;               ///< waitingtime in days before a oneway signal
	byte   wait_twoway_signal;               ///< waitingtime in days before a twoway signal

	bool   reserve_paths;                    ///< always reserve paths regardless of signal type.
	byte   wait_for_pbs_path;                ///< how long to wait for a path reservation.
	byte   path_backoff_interval;            ///< ticks between checks for a free path.

	NPFSettings  npf;                        ///< pathfinder settings for the new pathfinder
	YAPFSettings yapf;                       ///< pathfinder settings for the yet another pathfinder
};

/** Settings related to orders. */
struct OrderSettings {
	bool   improved_load;                    ///< improved loading algorithm
	bool   gradual_loading;                  ///< load vehicles gradually
	bool   selectgoods;                      ///< only send the goods to station if a train has been there
	bool   no_servicing_if_no_breakdowns;    ///< don't send vehicles to depot when breakdowns are disabled
	bool   serviceathelipad;                 ///< service helicopters at helipads automatically (no need to send to depot)
};

/** Settings related to vehicles. */
struct VehicleSettings {
	uint8_t  max_train_length;                 ///< maximum length for trains
	uint8_t  smoke_amount;                     ///< amount of smoke/sparks locomotives produce
	uint8_t  train_acceleration_model;         ///< realistic acceleration for trains
	uint8_t  roadveh_acceleration_model;       ///< realistic acceleration for road vehicles
	uint8_t  train_slope_steepness;            ///< Steepness of hills for trains when using realistic acceleration
	uint8_t  roadveh_slope_steepness;          ///< Steepness of hills for road vehicles when using realistic acceleration
	bool   wagon_speed_limits;               ///< enable wagon speed limits
	bool   disable_elrails;                  ///< when true, the elrails are disabled
	UnitID max_trains;                       ///< max trains in game per company
	UnitID max_roadveh;                      ///< max trucks in game per company
	UnitID max_aircraft;                     ///< max planes in game per company
	UnitID max_ships;                        ///< max ships in game per company
	uint8_t  plane_speed;                      ///< divisor for speed of aircraft
	uint8_t  freight_trains;                   ///< value to multiply the weight of cargo by
	bool   dynamic_engines;                  ///< enable dynamic allocation of engine data
	bool   never_expire_vehicles;            ///< never expire vehicles
	byte   extend_vehicle_life;              ///< extend vehicle life by this many years
	byte   road_side;                        ///< the side of the road vehicles drive on
	uint8_t  plane_crashes;                    ///< number of plane crashes, 0 = none, 1 = reduced, 2 = normal
};

/** Settings related to the economy. */
struct EconomySettings {
	bool   inflation;                        ///< disable inflation
	bool   bribe;                            ///< enable bribing the local authority
	EconomyType type;                        ///< economy type (original/smooth/frozen)
	uint8_t  feeder_payment_share;             ///< percentage of leg payment to virtually pay in feeder systems
	byte   dist_local_authority;             ///< distance for town local authority, default 20
	bool   exclusive_rights;                 ///< allow buying exclusive rights
	bool   fund_buildings;                   ///< allow funding new buildings
	bool   fund_roads;                       ///< allow funding local road reconstruction
	bool   give_money;                       ///< allow giving other companies money
	bool   mod_road_rebuild;                 ///< roadworks remove unnecessary RoadBits
	bool   multiple_industry_per_town;       ///< allow many industries of the same type per town
	uint8_t  town_growth_rate;                 ///< town growth rate
	uint8_t  larger_towns;                     ///< the number of cities to build. These start off larger and grow twice as fast
	uint8_t  initial_city_size;                ///< multiplier for the initial size of the cities compared to towns
	TownLayout town_layout;                  ///< select town layout, @see TownLayout
	TownCargoGenMode town_cargogen_mode;     ///< algorithm for generating cargo from houses, @see TownCargoGenMode
	bool   allow_town_roads;                 ///< towns are allowed to build roads (always allowed when generating world / in SE)
	TownFounding found_town;                 ///< town founding.
	bool   station_noise_level;              ///< build new airports when the town noise level is still within accepted limits
	uint16_t town_noise_population[4];         ///< population to base decision on noise evaluation (@see town_council_tolerance)
	bool   allow_town_level_crossings;       ///< towns are allowed to build level crossings
	bool   infrastructure_maintenance;       ///< enable monthly maintenance fee for owner infrastructure
};

struct LinkGraphSettings {
	uint16_t recalc_time;                     ///< time (in days) for recalculating each link graph component.
	uint16_t recalc_interval;                 ///< time (in days) between subsequent checks for link graphs to be calculated.
	DistributionType distribution_pax;      ///< distribution type for passengers
	DistributionType distribution_mail;     ///< distribution type for mail
	DistributionType distribution_armoured; ///< distribution type for armoured cargo class
	DistributionType distribution_default;  ///< distribution type for all other goods
	uint8_t accuracy;                         ///< accuracy when calculating things on the link graph. low accuracy => low running time
	uint8_t demand_size;                      ///< influence of supply ("station size") on the demand function
	uint8_t demand_distance;                  ///< influence of distance between stations on the demand function
	uint8_t short_path_saturation;            ///< percentage up to which short paths are saturated before saturating most capacious paths

	inline DistributionType GetDistributionType(CargoID cargo) const
	{
		if (IsCargoInClass(cargo, CC_PASSENGERS)) return this->distribution_pax;
		if (IsCargoInClass(cargo, CC_MAIL)) return this->distribution_mail;
		if (IsCargoInClass(cargo, CC_ARMOURED)) return this->distribution_armoured;
		return this->distribution_default;
	}
};

/** Settings related to stations. */
struct StationSettings {
	bool   modified_catchment;               ///< different-size catchment areas
	bool   serve_neutral_industries;         ///< company stations can serve industries with attached neutral stations
	bool   adjacent_stations;                ///< allow stations to be built directly adjacent to other stations
	bool   distant_join_stations;            ///< allow to join non-adjacent stations
	bool   never_expire_airports;            ///< never expire airports
	byte   station_spread;                   ///< amount a station may spread
};

/** Default settings for vehicles. */
struct VehicleDefaultSettings {
	bool   servint_ispercent;                ///< service intervals are in percents
	uint16_t servint_trains;                   ///< service interval for trains
	uint16_t servint_roadveh;                  ///< service interval for road vehicles
	uint16_t servint_aircraft;                 ///< service interval for aircraft
	uint16_t servint_ships;                    ///< service interval for ships
};

/** Settings that can be set per company. */
struct CompanySettings {
	bool engine_renew;                       ///< is autorenew enabled
	int16_t engine_renew_months;               ///< months before/after the maximum vehicle age a vehicle should be renewed
	uint32_t engine_renew_money;               ///< minimum amount of money before autorenew is used
	bool renew_keep_length;                  ///< sell some wagons if after autoreplace the train is longer than before
	VehicleDefaultSettings vehicle;          ///< default settings for vehicles
};

/** All settings together for the game. */
struct GameSettings {
	DifficultySettings   difficulty;         ///< settings related to the difficulty
	GameCreationSettings game_creation;      ///< settings used during the creation of a game (map)
	ConstructionSettings construction;       ///< construction of things in-game
	AISettings           ai;                 ///< what may the AI do?
	ScriptSettings       script;             ///< settings for scripts
	class AIConfig      *ai_config[MAX_COMPANIES]; ///< settings per company
	class GameConfig    *game_config;        ///< settings for gamescript
	PathfinderSettings   pf;                 ///< settings for all pathfinders
	OrderSettings        order;              ///< settings related to orders
	VehicleSettings      vehicle;            ///< options for vehicles
	EconomySettings      economy;            ///< settings to change the economy
	LinkGraphSettings    linkgraph;          ///< settings for link graph calculations
	StationSettings      station;            ///< settings related to station management
	LocaleSettings       locale;             ///< settings related to used currency/unit system in the current game
};

/** All settings that are only important for the local client. */
struct ClientSettings {
	GUISettings          gui;                ///< settings related to the GUI
	NetworkSettings      network;            ///< settings related to the network
	CompanySettings      company;            ///< default values for per-company settings
	SoundSettings        sound;              ///< sound effect settings
	MusicSettings        music;              ///< settings related to music/sound
	NewsSettings         news_display;       ///< news display settings.
};

/** The current settings for this game. */
extern ClientSettings _settings_client;

/** The current settings for this game. */
extern GameSettings _settings_game;

/** The settings values that are used for new games and/or modified in config file. */
extern GameSettings _settings_newgame;

/** Old vehicle settings, which were game settings before, and are company settings now. (Needed for savegame conversion) */
extern VehicleDefaultSettings _old_vds;

/**
 * Get the settings-object applicable for the current situation: the newgame settings
 * when we're in the main menu and otherwise the settings of the current game.
 */
static inline GameSettings &GetGameSettings()
{
	return (_game_mode == GM_MENU) ? _settings_newgame : _settings_game;
}

#endif /* SETTINGS_TYPE_H */
