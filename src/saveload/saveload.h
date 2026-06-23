/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file saveload.h Functions/types related to saving and loading games. */

#ifndef SAVELOAD_H
#define SAVELOAD_H

#include "saveload_error.hpp"
#include "../fileio_type.h"
#include "../fios.h"

/** SaveLoad versions
 * Previous savegame versions, the trunk revision where they were
 * introduced and the released version that had that particular
 * savegame version.
 * Up to savegame version 18 there is a minor version as well.
 *
 * Older entries keep their original numbering.
 *
 * Newer entries should use a descriptive labels, numeric version
 * and PR can be added to comment.
 *
 * Note that this list must not be reordered.
 */
enum SaveLoadVersion : uint16_t {
	SL_MIN_VERSION,                         ///< First savegame version

	SLV_BIG_CURRENCY,                       ///<   1.0         0.1.x, 0.2.x Change currency from 32 to 64 bits
	SLV_VEHICLE_CURRENCY_STATION_CHANGES,   /**<   2.0         0.3.0        Adding vehicle state, larger currency size for statistics, station size revamped.
	                                         *     2.1         0.3.1, 0.3.2 Unify way of storing town owner
	                                         *     2.2         lost         New airports */
	SLV_BIGGER_STATION_VARIABLES,           ///<   3.x         lost         Increase size of airport blocks/station build date
	SLV_TOWN_TOLERANCE_PAUSE_MODE,          /**<   4.0     1                Town council tolerance and pause mode
	                                         *     4.1   122   0.3.3, 0.3.4 Store exclusive rights in towns
	                                         *     4.2  1222   0.3.5        Currencies are reordered
	                                         *     4.3  1417                Make water owned by OWNER_NONE
	                                         *     4.4  1426                Make vehicle references same as other references */

	SLV_BIG_MAP,                            /**<   5.0  1429 Making maps a different size than 256x256
	                                         *     5.1  1440 Flexible airport layouts
	                                         *     5.2  1525   0.3.6 Dynamic order array */
	SLV_MULTIPLE_ROAD_STOPS,                /**<   6.0  1721 Multi tile road stops, and some map size related fixes
	                                         *     6.1  1768 Town index in m2 */
	SLV_LARGER_CARGO_SOURCE,                ///<   7.0  1770 With more stations, the size of the cargo source needed to be increased
	SLV_LARGER_UNIT_NUMBER,                 ///<   8.0  1786 Increase size of (vehicle) unit numbers
	SLV_LARGER_TOWN_CARGO_STATISTICS,       ///<   9.0  1909 Increase size of passenger/mail production of this and previous months

	SLV_LARGER_TOWN_COUNTER,                ///<  10.0  2030 Increase size of the town counter
	SLV_LARGER_TOWN_ITERATOR,               /**<  11.0  2033 Increase size of the town iterator
	                                         *    11.1  2041 Fix vehicle counters */
	SLV_LINK_WAYPOINT_TO_TOWN,              ///<  12.1  2046 Link waypoints to towns and remove some bit stuffing
	SLV_LARGER_AI_STATE_COUNTER,            ///<  13.1  2080   0.4.0, 0.4.0.1 AI state counter increased due it storing tile indices
	SLV_TRANSFER_ORDER,                     ///<  14.0  2441 Transfer orders for feeder systems

	SLV_MOVE_SEMAPHORE_BITS,                ///<  15.0  2499 Move rail signal bit for semaphores
	SLV_ENGINE_RENEW,                       /**<  16.0  2817 Automatic replacing/renewing of vehicles
	                                         *    16.1  3155 Keep vehicle length during autoreplace */
	SLV_STORE_WAYPOINT_ID_IN_MAP,           /**<  17.0  3212 Store the ID of waypoints in m2 of the map
	                                         *    17.1  3218 Make train subtype a bitmask */
	SLV_REMOVE_MINOR_VERSION,               ///<  18    3227 Remove the minor versions from savegames
	SLV_ENGINE_RENEW_POOL,                  ///<  19    3396 Engine renews are now stored in a pool

	SLV_NO_MULTIHEAD_REFERENCE,             ///<  20    3403 Remove reference from one multihead to the other one
	SLV_REMOVE_OLD_PBS,                     ///<  21    3472   0.4.x Remove old implementation of path based signals
	SLV_SAVE_PATCHES,                       ///<  22    3726 Save state of patches (precursor of settings) in the savegame
	SLV_REMOVE_AUTOSAVE_INTERVAL,           ///<  23    3915 Store autosave interval locally, instead of in savegame
	SLV_ELRAIL,                             ///<  24    4150 Electrified railways

	SLV_IMPROVE_MULTISTOP,                  ///<  25    4259 Improve the behaviour of RVs going to road stops
	SLV_LAST_VEHICLE_TYPE,                  ///<  26    4466 Store the last vehicle type at stations instead of the vehicle ID
	SLV_NEWGRF_STATIONS,                    ///<  27    4757 NewGRF graphics for stations
	SLV_YAPF,                               ///<  28    4987 Yet another path finder
	SLV_MORE_UNDER_BRIDGES,                 ///<  29    5070 Support crossings, fields and bridge/tunnel heads under bridges

	SLV_TGP,                                ///<  30    5946 TerraGenesis Perlin
	SLV_BIG_DATES,                          ///<  31    5999 Change date from 1920 - 2090 to 0 - 5 000 000
	SLV_LINK_FARM_FIELD_TO_INDUSTRY,        ///<  32    6001 Link farm fields to the industry, so it gets removed when the industry goes away
	SLV_SAVE_YAPF_SETTINGS,                 ///<  33    6440 Some YAPF settings were not saved properly
	SLV_LIVERIES,                           ///<  34    6455 Liveries and two company colours (2cc)

	SLV_LIVERY_REFIT,                       ///<  35    6602 NewGRF livery refits
	SLV_REFIT_ORDERS,                       ///<  36    6624 Vehicles can be refitted as part of an order
	SLV_UTF8,                               ///<  37    7182 UTF-8 strings
	SLV_DISABLE_ELRAIL_SETTING,             ///<  38    7195 Add setting to disable electrified rails
	SLV_FREIGHT_WEIGHT,                     ///<  39    7269 Setting to increase the weight of cargo on freight trains

	SLV_GRADUAL_LOADING,                    ///<  40    7326 Gradual (un)loading of cargo
	SLV_NEWGRF_SETTINGS,                    ///<  41    7348   0.5.x Save what NewGRFs are used in the game and their settings
	SLV_BRIDGE_WORMHOLE,                    ///<  42    7573 Bridges become wormholes, so more things can be built under them (e.g. signals)
	SLV_UNIFY_ANIMATION_STATE,              ///<  43    7642 Put all animation state information in same map bits
	SLV_CARGO_SOURCE_TILE,                  ///<  44    8144 Store the source tile of the cargo, so accurate payment can happen when the source station is removed

	SLV_COUNT_PAID_FOR_CARGO,               ///<  45    8501 Count the amount of cargo that was paid for
	SLV_MORE_AIRPORT_BLOCKS,                ///<  46    8705 Increase number of blocks an airport can have
	SLV_DRIVE_THROUGH_ROAD_STOPS,           ///<  47    8735 Drive through road stops
	SLV_RAIL_TRACK_TYPE_UNIFICATION,        ///<  48    8935 Put all the rail track type information in same map bits
	SLV_SIMPLIFY_PLAYER_FACE,               ///<  49    8969 Simplify the storage of player face information

	SLV_AIRCRAFT_SPEED_HOLDING,             ///<  50    8973 Aircraft speed in km-ish/h and reduced speed in holding patterns
	SLV_FEEDER_SHARE,                       ///<  51    8978 Rewrite of transfers to retain knowledge about the already paid amount for transfered cargo
	SLV_STATUE_OWNER,                       ///<  52    9066 Store the owner of the statue, so the town can be informed of their removal
	SLV_NEWGRF_HOUSES,                      ///<  53    9316 NewGRF controlled houses
	SLV_TOWN_GROWTH_CONTROL,                ///<  54    9613 Give the player control over the town growth

	SLV_NEWGRF_CARGO,                       ///<  55    9638 Increase number of cargos and NewGRF control of cargos
	SLV_CITIES,                             ///<  56    9667 Cities that start bigger and grow faster
	SLV_FIFO_LOADING,                       ///<  57    9691 First-in-first-out loading of vehicles
	SLV_VERY_LOW_TOWN_INDUSTRY_NUMBER,      ///<  58    9762 Difficulty settings for very low number of industries and towns
	SLV_TOWN_LAYOUT,                        ///<  59    9779 More layout options for towns

	SLV_VEHICLE_GROUPS,                     ///<  60    9874 Arbitrary grouping, by the player, of vehicles
	SLV_MULTIPLE_ROAD_TYPES,                ///<  61    9892 Multiple road types for the same tile
	SLV_ADJACENT_STATIONS,                  ///<  62    9905 Allow building multiple stations directly next to eachother
	SLV_TRAM_LIVERY,                        ///<  63    9956 Add separate livery for trams
	SLV_MULTIPLE_SIGNAL_TYPES,              ///<  64   10006 Multiple different signal types on the same (diagonal) tile, instead of the same for both directions

	SLV_UNIFY_CURRENCY,                     ///<  65   10210 Make all variables related to currency 64 bits
	SLV_NEWGRF_TOWN_NAMES,                  ///<  66   10211 NewGRF provided town names
	SLV_TIMETABLES,                         ///<  67   10236 Introduce timetables for vehicles
	SLV_CARGO_PACKETS,                      ///<  68   10266 Account for individual units of cargo, i.e. there can be cargo from multiple sources/ages in one vehicle
	SLV_MORE_CARGO_PACKETS,                 ///<  69   10319 Allow more than ~65k cargo packets

	SLV_CARGO_PAYMENT_OVERFLOW,             ///<  70   10541 Fix overflow of cargo payment rates, plus preparation for player founded industries
	SLV_UNGROUPED_VEHICLES,                 ///<  71   10567 Add a group with vehicles that aren't in any other group
	SLV_SPLIT_STATION_TYPE_FROM_GFXID,      ///<  72   10601 Splits the encoding of station type from the graphics identifer
	SLV_NEWGRF_INDUSTRY_LAYOUT,             ///<  73   10903 NewGRF provided layouts for industries
	SLV_FIX_STATION_PICKUP_ACCOUNTING,      ///<  74   11030 Accounting of which cargos a station would pick up was done incorrectly

	SLV_AUTOSLOPE,                          ///<  75   11107 Terraforming under buildings/track/anything that supports foundations
	SLV_NEWGRF_PERSISTENT_STORAGE,          ///<  76   11139 Persistently store some state of NewGRF objects/entities
	SLV_CLEANUP_UNCONNECTED_ROADS,          ///<  77   11172 Option to remove unconnected roads during a town's road reconstruction
	SLV_STORE_INDUSTRY_CARGO,               ///<  78   11176 Store an industry's cargo, so it can be customised upon building
	SLV_FAIR_PLAY_SETTINGS,                 ///<  79   11188 Add setting to disable exclusive rights in a town and giving money

	SLV_NEWGRF_MORE_ANIMATION,              ///<  80   11228 Support more types of animation for NewGRF industries
	SLV_FIX_TREE_GROUND,                    ///<  81   11244 Various fixes to improve the visuals of the ground under trees
	SLV_NEWGRF_INDUSTRY_RANDOM_TRIGGERS,    ///<  82   11410 NewGRF random triggers for industries
	SLV_DEPOT_WATER_OWNERS,                 ///<  83   11589 Store the owner of the water under depots, so removing of the depot doesn't disown the original owner
	SLV_REPLACE_CUSTOM_NAME_ARRAY,          ///<  84   11822 Replace single fixed size array of custom names, by moving the name into the appropriate objects

	SLV_MAGLEV_MONORAIL_PAX_WAGON_LIVERY,   ///<  85   11874 Add livery for maglev/monorail passenger wagons
	SLV_WATER_CLASS,                        ///<  86   12042 Store the type of water (sea/ocean, canal, river) for buoys, docks, locks and depots
	SLV_SIMPLIFY_PATHFINDER_SETTINGS,       ///<  87   12129 Make it easier to select the pathfinder to use
	SLV_FRACTION_PROFIT_RUNNING_TICKS,      ///<  88   12134 Store vehicle profits as a (fixed point) fraction, and store the number of ticks a vehicle ran in a day
	SLV_MORE_WAYPOINTS_PER_TOWN,            ///<  89   12160 Support more than 64 waypoints per town

	SLV_PLANE_SPEED_FACTOR,                 ///<  90   12293 Setting to increase aircraft speed to be on par with the other vehicles
	SLV_MORE_HOUSE_ANIMATION_FRAMES,        ///<  91   12347 Increase number of animation frames for NewGRF houses
	SLV_REMOVE_HOUSE_COUNT,                 ///<  92   12381   0.6.x Remove number of houses in a town from the save
	SLV_IMPROVED_ORDERS,                    ///<  93   12648 Orders support all full load/non stop types at the same time now
	SLV_FIX_COMPANY_CARGO_TYPES,            ///<  94   12816 The company's cargo types should have increased in since with SLV_NEWGRF_CARGO

	SLV_MORE_ENGINE_TYPES,                  ///<  95   12924 Allow more than the original 255 engine types
	SLV_AIRPORT_NOISE,                      ///<  96   13226 Introduce noise for airports, to allow more than two airports per town
	SLV_MERGE_OPTS_PATS,                    ///<  97   13256 Merge the OPTS and PATS chunks, i.e. all settings in one chunk
	SLV_GAMELOG,                            ///<  98   13375 Logging of important actions/situations in the save
	SLV_INDUSTRY_TILE_WATER_CLASS,          ///<  99   13838 Add water classes to industry tiles

	SLV_YAPP,                               ///< 100   13952 New version of path based signals
	SLV_NEWGRF_PALETTE,                     ///< 101   14233 Store palette used by each of the NewGRFs
	SLV_SPREAD_INDUSTRY_PRODUCTION_CHANGES, ///< 102   14332 Spread the industry production changes over the month, instead of doing all on the same day
	SLV_NEWGRF_SUPPLIED_STATION_NAME,       ///< 103   14598 NewGRF industry supplying default names for nearby stations
	SLV_MORE_COMPANIES,                     ///< 104   14735 Increase maximum number of companies to 15

	SLV_ORDER_LIST,                         ///< 105   14803 Create separate order list objects for maintaining orders
	SLV_DISTANT_STATION_JOINING,            ///< 106   14919 Distant joining of stations
	SLV_NOAI,                               ///< 107   15027 Replace built in cheating AI with framework for externally developed (scripted) AIs
	SLV_STORE_AI_VERSION,                   ///< 108   15045 Store the version of the AI script
	SLV_NEXT_COMPETITOR_START_OVERFLOW,     ///< 109   15075 Prevent overflow in the next competitor start counter

	SLV_REMOVE_OLD_AI_SETTINGS,             ///< 110   15148 Remove remnants of the old AI's configuration
	SLV_FREEFORM_EDGES,                     ///< 111   15190 Allow terraforming along the edge of the map
	SLV_SPLIT_HQ,                           ///< 112   15290 Split the behaviour of headquarters from the other unmovable objects
	SLV_ROAD_LAYOUT_PER_TOWN,               ///< 113   15340 Allow for different road layouts for each of the towns
	SLV_SEPARATE_ROAD_OWNERS,               ///< 114   15601 Separate owners for road bits, tram bits and the road stop

	SLV_CUSTOM_TOWN_NUMBER,                 ///< 115   15695 Configuration for specific number of towns to build
	SLV_GAMELOG_EMERGENCY,                  ///< 116   15893   0.7.x Gamelog event for emergency/crash saves
	SLV_PLATFORM_STOP_LOCATION,             ///< 117   16037 Set the platform stop location via train orders
	SLV_DIGIT_GROUP_SEPARATOR,              ///< 118   16129 Configurable digit group separator
	SLV_PAUSE_MODES,                        ///< 119   16242 Use bitmask of reason to pause, so manual/auto pausing do not conflict

	SLV_COMPANY_SERVICE_INTERVALS,          ///< 120   16439 Make service intervals configurable per company
	SLV_CARGO_PAYMENTS,                     ///< 121   16694 Perform payment of cargo after unloading
	SLV_WAYPOINT_MORE_LIKE_STATION,         ///< 122   16855 Make waypoint data look more like stations
	SLV_UNIFY_WAYPOINT_AND_STATION,         ///< 123   16909 Unify stations and waypoints
	SLV_MULTI_TILE_WAYPOINTS,               ///< 124   16993 Waypoints can be bigger than a single tile

	SLV_REMOVE_SUBSIDY_STATION_BINDING,     ///< 125   17113 Awarded subsidies are not bound to stations, but to their actual source/destination
	SLV_CUMULATED_INFLATION,                ///< 126   17433 Store cumulated inflation, and recalculate prices/payments upon load
	SLV_TOWN_ACCEPTANCE,                    ///< 127   17439 Store mask of cargos accepted by town houses and head quarters
	SLV_FOUND_TOWN,                         ///< 128   18281 Founding of new towns
	SLV_TIMETABLE_START,                    ///< 129   18292 Allow setting the start date of a timetable

	SLV_ROAD_STOP_OCCUPANCY_PENALTY,        ///< 130   18404 Add configurable pathfinder penalty for an occupied road stop
	SLV_MAXIMUM_DEPOT_PENALTY,              ///< 131   18481 Add configurable maximum pathfinder penalty for finding a depot
	SLV_DISALLOW_TREE_BUILDING,             ///< 132   18522 Setting to partially disable building of trees
	SLV_TRAIN_SLOPE_STEEPNESS,              ///< 133   18674 Setting to increase steepness of slopes for trains under realistic acceleration
	SLV_VIRTUAL_FEEDER_SHARE_PAYMENT,       ///< 134   18703 Pay a part of the virtual profit during a transfer to the intermediate vehicle

	SLV_ROCKS_STAY_UNDER_SNOW,              ///< 135   18719 Rocks stay under snow, i.e. they return when the snow goes away
	SLV_SPLIT_LOAD_WAIT_COUNTERS,           ///< 136   18764 Split counters for (un)loading and signal waiting/turning as otherwise they interfere
	SLV_AIRPORT_ANIMATION_FRAMES,           ///< 137   18912 Use animation frames instead of many airport tile ids for animation
	SLV_REDUCE_PLANE_CRASHES,               ///< 138   18942   1.0.x Setting to reduce/disable crashing of planes
	SLV_RV_REALISTIC_ACCELERATION,          ///< 139   19346 Realistic acceleration of road vehicles

	SLV_STORE_AIRPORT_SIZE,                 ///< 140   19382 Store the size of the airport in the station
	SLV_UNIQUE_DEPOT_NAMES,                 ///< 141   19799 Give depots unique names
	SLV_NEWGRF_DEPOT_BUILD_DATE,            ///< 142   20003 Depot build date for NewGRFs
	SLV_DISABLE_TOWN_LEVEL_CROSSING,        ///< 143   20048 Setting to be able to disable building rail/road crossings by towns
	SLV_REORDER_UNMOVABLE_REMOVE_RESERVED,  ///< 144   20334 Reorder map bits of unmovable tiles and remove unused reserved zero bytes

	SLV_NEWGRF_AIRPORT_SMOKE,               ///< 145   20376 NewGRF support for airport and configurable amount of smoke for vehicles
	SLV_UNIFY_WATER_CLASS,                  ///< 146   20446 Unify location for storing water class in the map
	SLV_UNIFY_ANIMATION_FRAME,              ///< 147   20621 Unify location of animation frame
	SLV_INDUSTRY_PLATFORM,                  ///< 148   20659 Setting to make a flat area around (new) industries
	SLV_CUSTOM_SEA_LEVEL,                   ///< 149   20832 Setting to influence the sea level (amount of water)

	SLV_FRACTIONAL_CARGO_DELIVERY,          ///< 150   20857 When spreading cargo over stations, spread fractional amounts for fairness
	SLV_STORE_NEWGRF_VERSION,               ///< 151   20918 Store the version of the used NewGRFs
	SLV_INDUSTRY_MANAGEMENT,                ///< 152   21171 Manage the amount of industries that ought to be spawned per type
	SLV_LEAVE_ROAD_STOP_SEPARATELY,         ///< 153   21263 Fix issue where multiple vehicles could leave a road stop at the same time
	SLV_PAUSE_LEVEL,                        ///< 154   21426 Setting to determine what commands are allowed when paused

	SLV_NEWGRF_OBJECT_VIEW,                 ///< 155   21453 Support for views in NewGRF objects
	SLV_TERRAFORM_LIMITS,                   ///< 156   21728 Introduce limits for terraforming and clearing times
	SLV_UNIFY_GROUND_VEHICLES,              ///< 157   21862 Unify the way ground vehicles are handled (articulated parts, etc)
	SLV_TRACK_REAL_AND_AUTO_ORDERS,         ///< 158   21933 Track which real and auto order is the current order
	SLV_MAX_LENGTH_AND_REVERSE_SIGNALS,     ///< 159   21962 Settings for reversing at signals, and maximum train, bridge and tunnel length

	SLV_DISALLOW_ROAD_RECONSTRUCTION,       ///< 160   21974   1.1.x Setting to disallow road reconstruction
	SLV_PERSISTENT_STORAGE_POOL,            ///< 161   22567 Store persistent storage in a pool
	SLV_NEWGRF_CUSTOM_CARGO_AGING,          ///< 162   22713 NewGRF influence on aging of cargo in vehicles
	SLV_RIVERS,                             ///< 163   22767 Rivers
	SLV_VEHICLE_CENTRE_AND_Z_POS,           ///< 164   23290 Vehicle centres are not fixed at 4/8 of the vehicle; change type of z-positions to prepare for higher maps

	SLV_SCRIPT_TOWN_GROWTH,                 ///< 165   23304 Storage of cargo statistics for use by game scripts
	SLV_INFRASTRUCTURE_MAINTENANCE_COSTS,   ///< 166   23415 Infrastructure can now cost some periodic fee
	SLV_NEWGRF_AIRCRAFT_RANGE,              ///< 167   23504 NewGRF provided maximum aircraft range
	SLV_SCRIPT_TOWN_TEXT,                   ///< 168   23637 Game scripts can put a text in the town window
	SLV_MOVE_SCC_ENCODED,                   ///< 169   23816 Move SCC_ENCODED to the first StringControlCode

	SLV_COUNT_INDIVIDUAL_CARGOES,           ///< 170   23826 Store the count of individual cargo delivery for a period
	SLV_SCENARIO_DEITY_SIGNS,               ///< 171   23835 Signs made in scenarios become of OWNER_DEITY, so they are always shown
	SLV_ORDER_MAX_SPEED,                    ///< 172   23947 Set maximum speed for orders
	SLV_FIX_ROAD_OWNERSHIP,                 ///< 173   23967   1.2.0-RC1 Seemingly unneeded bump supposed to fix something with road ownership
	SLV_CURRENT_ORDER_MAX_SPEED,            ///< 174   23973   1.2.x     Save maximum speed of current order

	SLV_AUTOREPLACE_WHEN_OLD_TREE_LIMIT,    ///< 175   24136 Autoreplace vehicle only when they are old, and putting limit on amount of trees to build (at once)
	SLV_BACKUP_ORDER_STATE,                 ///< 176   24446 Put more of the state of a vehicle's orders (like lateness, start point) in the order backup
	SLV_MONTHLY_BANKRUPTCY_CHECK,           ///< 177   24619 Check for bankruptcy on a monthly cycle
	SLV_SCRIPT_SETTINGS_PROFILE,            ///< 178   24789 Setting for the difficulty profile of AIs
	SLV_ROBUST_ENGINE_PREVIEW,              ///< 179   24810 Make engine preview offers robust when company ranking changes

	SLV_SERVICE_INTERVAL_PERCENT,           ///< 180   24998   1.3.x Service interval in percent or days stored per vehicle
	SLV_CARGO_RESERVATION,                  ///< 181   25012 Persist the reservation of cargo for vehicles instead of recalculating it each time
	SLV_GOAL_PROGRESS_PLANE_ACCELERATION,   ///< 182   25115 FS#5492, r25259, r25296 Goal status and plane acceleration fixes
	SLV_CARGODIST,                          ///< 183   25363 Cargodist
	SLV_SEPARATE_LOCALE_UNITS,              ///< 184   25508 Unit localisation split

	SLV_STORYBOOKS,                         ///< 185   25620 Storybooks
	SLV_OBJECT_TYPE_TO_POOL,                ///< 186   25833 Move object type from map to pool object
	SLV_LINKGRAPH_RESTRICTED_FLOW,          ///< 187   25899 Linkgraph - restricted flows
	SLV_UNIFY_RV_TRAVEL_TIME,               ///< 188   26169 v1.4  FS#5831 Unify RV travel time
	SLV_GROUP_HIERARCHY,                    ///< 189   26450 Hierarchical vehicle subgroups

	SLV_SEPARATE_ORDER_TRAVEL_WAIT_TIME,    ///< 190   26547 Separate order travel and wait times
	SLV_LINKGRAPH_LOCATION_DISASTER_STORE,  ///< 191   26636 FS#6026 Fix disaster vehicle storage (No bump)
	                                        ///< 191   26646 FS#6041 Linkgraph - store locations
	SLV_FIX_ORDER_BACKUP,                   ///< 192   26700 FS#6066 Fix saving of order backups
	SLV_HIDE_ENGINES_FOR_COMPANY,           ///< 193   26802 Hiding of engines for a company
	SLV_MAX_BRIDGE_MAP_HEIGHT,              ///< 194   26881 v1.5 Setting for maximum bridge and map height

	SLV_DISTINGUISH_1_6,                    ///< 195   27572 v1.6.1 Convenience bump to distinguish 1.6 from 1.5 saves
	SLV_DISTINGUISH_1_7,                    ///< 196   27778 v1.7 Convenience bump to distinguish 1.7 from 1.6 saves
	SLV_STORE_MAP_VARIETY,                  ///< 197   27978 v1.8 Store map variety
	SLV_TOWN_GROWTH_IN_GAME_TICKS,          ///< 198  PR#6763 Switch town growth rate and counter to actual game ticks
	SLV_EXTEND_CARGOTYPES,                  ///< 199  PR#6802 Extend cargotypes to 64

	SLV_EXTEND_RAILTYPES,                   ///< 200  PR#6805 Extend railtypes to 64, adding uint16_t to map array.
	SLV_EXTEND_PERSISTENT_STORAGE,          ///< 201  PR#6885 Extend NewGRF persistent storages.
	SLV_EXTEND_INDUSTRY_CARGO_SLOTS,        ///< 202  PR#6867 Increase industry cargo slots to 16 in, 16 out
	SLV_SHIP_PATH_CACHE,                    ///< 203  PR#7072 Add path cache for ships
	SLV_SHIP_ROTATION,                      ///< 204  PR#7065 Add extra rotation stages for ships.

	SLV_GROUP_LIVERIES,                     ///< 205  PR#7108 Livery storage change and group liveries.
	SLV_SHIPS_STOP_IN_LOCKS,                ///< 206  PR#7150 Ship/lock movement changes.
	SLV_FIX_CARGO_MONITOR,                  ///< 207  PR#7175 v1.9  Cargo monitor data packing fix to support 64 cargotypes.
	SLV_TOWN_CARGOGEN,                      ///< 208  PR#6965 New algorithms for town building cargo generation.
	SLV_SHIP_CURVE_PENALTY,                 ///< 209  PR#7289 Configurable ship curve penalties.

	SLV_SERVE_NEUTRAL_INDUSTRIES,           ///< 210  PR#7234 Company stations can serve industries with attached neutral stations.
	SLV_ROADVEH_PATH_CACHE,                 ///< 211  PR#7261 Add path cache for road vehicles.
	SLV_REMOVE_OPF,                         ///< 212  PR#7245 Remove OPF.
	SLV_TREES_WATER_CLASS,                  ///< 213  PR#7405 WaterClass update for tree tiles.
	SLV_ROAD_TYPES,                         ///< 214  PR#6811 NewGRF road types.

	SLV_SCRIPT_MEMLIMIT,                    ///< 215  PR#7516 Limit on AI/GS memory consumption.
	SLV_MULTITILE_DOCKS,                    ///< 216  PR#7380 Multiple docks per station.
	SLV_TRADING_AGE,                        ///< 217  PR#7780 Configurable company trading age.
	SLV_ENDING_YEAR,                        ///< 218  PR#7747 v1.10  Configurable ending year.
	SLV_REMOVE_TOWN_CARGO_CACHE,            ///< 219  PR#8258 Remove town cargo acceptance and production caches.

	/* Patchpacks for a while considered it a good idea to jump a few versions
	 * above our version for their savegames. But as time continued, this gap
	 * has been closing, up to the point we would start to reuse versions from
	 * their patchpacks. This is not a problem from our perspective: the
	 * savegame will simply fail to load because they all contain chunks we
	 * cannot digest. But, this gives for ugly errors. As we have plenty of
	 * versions anyway, we simply skip the versions we know belong to
	 * patchpacks. This way we can present the user with a clean error
	 * indicate they are loading a savegame from a patchpack.
	 * For future patchpack creators: please follow a system like JGRPP, where
	 * the version is masked with 0x8000, and the true version is stored in
	 * its own chunk with feature toggles.
	 */
	SLV_START_PATCHPACKS,                   ///< 220  First known patchpack to use a version just above ours.
	SLV_END_PATCHPACKS = 286,               ///< 286  Last known patchpack to use a version just above ours.

	SLV_GS_INDUSTRY_CONTROL,                ///< 287  PR#7912 and PR#8115 GS industry control.
	SLV_VEH_MOTION_COUNTER,                 ///< 288  PR#8591 Desync safe motion counter
	SLV_INDUSTRY_TEXT,                      ///< 289  PR#8576 v1.11.0-RC1  Additional GS text for industries.

	SLV_MAPGEN_SETTINGS_REVAMP,             ///< 290  PR#8891 v1.11  Revamp of some mapgen settings (snow coverage, desert coverage, heightmap height, custom terrain type).
	SLV_GROUP_REPLACE_WAGON_REMOVAL,        ///< 291  PR#7441 Per-group wagon removal flag.
	SLV_CUSTOM_SUBSIDY_DURATION,            ///< 292  PR#9081 Configurable subsidy duration.
	SLV_SAVELOAD_LIST_LENGTH,               ///< 293  PR#9374 Consistency in list length with SL_STRUCT / SL_STRUCTLIST / SL_DEQUE / SL_REFLIST.
	SLV_RIFF_TO_ARRAY,                      ///< 294  PR#9375 Changed many CH_RIFF chunks to CH_ARRAY chunks.

	SLV_TABLE_CHUNKS,                       ///< 295  PR#9322 Introduction of CH_TABLE and CH_SPARSE_TABLE.
	SLV_SCRIPT_INT64,                       ///< 296  PR#9415 SQInteger is 64bit but was saved as 32bit.
	SLV_LINKGRAPH_TRAVEL_TIME,              ///< 297  PR#9457 v12.0-RC1  Store travel time in the linkgraph.
	SLV_DOCK_DOCKINGTILES,                  ///< 298  PR#9578 All tiles around docks may be docking tiles.
	SLV_REPAIR_OBJECT_DOCKING_TILES,        ///< 299  PR#9594 v12.0  Fixing issue with docking tiles overlapping objects.

	SLV_U64_TICK_COUNTER,                   ///< 300  PR#10035 Make tick counter 64bit to avoid wrapping.
	SLV_LAST_LOADING_TICK,                  ///< 301  PR#9693 Store tick of last loading for vehicles.
	SLV_MULTITRACK_LEVEL_CROSSINGS,         ///< 302  PR#9931 v13.0  Multi-track level crossings.
	SLV_NEWGRF_ROAD_STOPS,                  ///< 303  PR#10144 NewGRF road stops.
	SLV_LINKGRAPH_EDGES,                    ///< 304  PR#10314 Explicitly store link graph edges destination, PR#10471 int64_t instead of uint64_t league rating

	SLV_VELOCITY_NAUTICAL,                  ///< 305  PR#10594 Separation of land and nautical velocity (knots!)
	SLV_CONSISTENT_PARTIAL_Z,               ///< 306  PR#10570 Conversion from an inconsistent partial Z calculation for slopes, to one that is (more) consistent.
	SLV_MORE_CARGO_AGE,                     ///< 307  PR#10596 Track cargo age for a longer period.
	SLV_LINKGRAPH_SECONDS,                  ///< 308  PR#10610 Store linkgraph update intervals in seconds instead of days.
	SLV_AI_START_DATE,                      ///< 309  PR#10653 Removal of individual AI start dates and added a generic one.

	SLV_EXTEND_VEHICLE_RANDOM,              ///< 310  PR#10701 Extend vehicle random bits.
	SLV_EXTEND_ENTITY_MAPPING,              ///< 311  PR#10672 Extend entity mapping range.
	SLV_DISASTER_VEH_STATE,                 ///< 312  PR#10798 Explicit storage of disaster vehicle state.
	SLV_SAVEGAME_ID,                        ///< 313  PR#10719 Add an unique ID to every savegame (used to deduplicate surveys).
	SLV_STRING_GAMELOG,                     ///< 314  PR#10801 Use std::string in gamelog.

	SLV_INDUSTRY_CARGO_REORGANISE,          ///< 315  PR#10853 Industry accepts/produced data reorganised.
	SLV_PERIODS_IN_TRANSIT_RENAME,          ///< 316  PR#11112 Rename days in transit to (cargo) periods in transit.
	SLV_NEWGRF_LAST_SERVICE,                ///< 317  PR#11124 Added stable date_of_last_service to avoid NewGRF trouble.
	SLV_REMOVE_LOADED_AT_XY,                ///< 318  PR#11276 Remove loaded_at_xy variable from CargoPacket.
	SLV_CARGO_TRAVELLED,                    ///< 319  PR#11283 CargoPacket now tracks how far it travelled inside a vehicle.

	SLV_STATION_RATING_CHEAT,               ///< 320  PR#11346 Add cheat to fix station ratings at 100%.
	SLV_TIMETABLE_START_TICKS,              ///< 321  PR#11468 Convert timetable start from a date to ticks.
	SLV_TIMETABLE_START_TICKS_FIX,          ///< 322  PR#11557 Fix for missing convert timetable start from a date to ticks.
	SLV_TIMETABLE_TICKS_TYPE,               ///< 323  PR#11435 Convert timetable current order time to ticks.
	SLV_WATER_REGIONS,                      ///< 324  PR#10543 Water Regions for ship pathfinder.

	SLV_WATER_REGION_EVAL_SIMPLIFIED,       ///< 325  PR#11750 Simplified Water Region evaluation.
	SLV_ECONOMY_DATE,                       ///< 326  PR#10700 Split calendar and economy timers and dates.
	SLV_ECONOMY_MODE_TIMEKEEPING_UNITS,     ///< 327  PR#11341 Mode to display economy measurements in wallclock units.
	SLV_CALENDAR_SUB_DATE_FRACT,            ///< 328  PR#11428 Add sub_date_fract to measure calendar days.
	SLV_SHIP_ACCELERATION,                  ///< 329  PR#10734 Start using Vehicle's acceleration field for ships too.

	SLV_MAX_LOAN_FOR_COMPANY,               ///< 330  PR#11224 Separate max loan for each company.
	SLV_DEPOT_UNBUNCHING,                   ///< 331  PR#11945 Allow unbunching shared order vehicles at a depot.
	SLV_AI_LOCAL_CONFIG,                    ///< 332  PR#12003 Config of running AI is stored inside Company.
	SLV_SCRIPT_RANDOMIZER,                  ///< 333  PR#12063 v14.0-RC1 Save script randomizers.
	SLV_VEHICLE_ECONOMY_AGE,                ///< 334  PR#12141 v14.0 Add vehicle age in economy year, for profit stats minimum age

	SLV_COMPANY_ALLOW_LIST,                 ///< 335  PR#12337 Saving of list of client keys that are allowed to join this company.
	SLV_GROUP_NUMBERS,                      ///< 336  PR#12297 Add per-company group numbers.
	SLV_INCREASE_STATION_TYPE_FIELD_SIZE,   ///< 337  PR#12572 Increase size of StationType field in map array
	SLV_ROAD_WAYPOINTS,                     ///< 338  PR#12572 Road waypoints
	SLV_COMPANY_INAUGURATED_PERIOD,         ///< 339  PR#12798 Companies show the period inaugurated in wallclock mode.

	SLV_ROAD_STOP_TILE_DATA,                ///< 340  PR#12883 Move storage of road stop tile data, also save for road waypoints.
	SLV_COMPANY_ALLOW_LIST_V2,              ///< 341  PR#12908 Fixed savegame format for saving of list of client keys that are allowed to join this company.
	SLV_WATER_TILE_TYPE,                    ///< 342  PR#13030 Simplify water tile type.
	SLV_PRODUCTION_HISTORY,                 ///< 343  PR#10541 Industry production history.
	SLV_ROAD_TYPE_LABEL_MAP,                ///< 344  PR#13021 Add road type label map to allow upgrade/conversion of road types.

	SLV_NONFLOODING_WATER_TILES,            ///< 345  PR#13013 Store water tile non-flooding state.
	SLV_PATH_CACHE_FORMAT,                  ///< 346  PR#12345 Vehicle path cache format changed.
	SLV_ANIMATED_TILE_STATE_IN_MAP,         ///< 347  PR#13082 Animated tile state saved for improved performance.
	SLV_INCREASE_HOUSE_LIMIT,               ///< 348  PR#12288 Increase house limit to 4096.
	SLV_COMPANY_INAUGURATED_PERIOD_V2,      ///< 349  PR#13448 Fix savegame storage for company inaugurated year in wallclock mode.

	SLV_ENCODED_STRING_FORMAT,              ///< 350  PR#13499 Encoded String format changed.
	SLV_PROTECT_PLACED_HOUSES,              ///< 351  PR#13270 Houses individually placed by players can be protected from town/AI removal.
	SLV_SCRIPT_SAVE_INSTANCES,              ///< 352  PR#13556 Scripts are allowed to save instances.
	SLV_FIX_SCC_ENCODED_NEGATIVE,           ///< 353  PR#14049 Fix encoding of negative parameters.
	SLV_ORDERS_OWNED_BY_ORDERLIST,          ///< 354  PR#13948 Orders stored in OrderList, pool removed.

	SLV_FACE_STYLES,                        ///< 355  PR#14319 Addition of face styles, replacing gender and ethnicity.
	SLV_INDUSTRY_NUM_VALID_HISTORY,         ///< 356  PR#14416 Store number of valid history records for industries.
	SLV_INDUSTRY_ACCEPTED_HISTORY,          ///< 357  PR#14321 Add per-industry history of cargo delivered and waiting.
	SLV_TOWN_SUPPLY_HISTORY,                ///< 358  PR#14461 Town supply history.
	SLV_STATIONS_UNDER_BRIDGES,             ///< 359  PR#14477 Allow stations under bridges.

	SLV_DOCKS_UNDER_BRIDGES,                ///< 360  PR#14594 Allow docks under bridges.
	SLV_LOCKS_UNDER_BRIDGES,                ///< 361  PR#14595 Allow locks under bridges.
	SLV_ENGINE_MULTI_RAILTYPE,              ///< 362  PR#14357 v15.0 Train engines can have multiple railtypes.
	SLV_SIGN_TEXT_COLOURS,                  ///< 363  PR#14743 Configurable sign text colors in scenario editor.
	SLV_BUOYS_AT_0_0,                       ///< 364  PR#14983 Allow to build buoys at (0x0).

	SLV_DRIVE_BACKWARDS,                    ///< 365  PR#15379 Trains can drive backwards.

	SL_MAX_VERSION,                         ///< Highest possible saveload version
};

/** Save or load result codes. */
enum class SaveLoadResult : uint8_t {
	Ok, ///< completed successfully
	Error, ///< error that was caught before internal structures were modified
	ReInit, ///< error that was caught in the middle of updating game state, need to clear it. (can only happen during load)
};

/** Deals with the type of the savegame, independent of extension */
struct FileToSaveLoad {
	SaveLoadOperation file_op;       ///< File operation to perform.
	FiosType ftype;                  ///< File type.
	std::string name;                ///< Name of the file.
	EncodedString title;             ///< Internal name of the game.

	void SetMode(const FiosType &ft, SaveLoadOperation fop = SaveLoadOperation::Load);
	void Set(const FiosItem &item);
};

/** Types of save games. */
enum SavegameType : uint8_t {
	SGT_TTD,    ///< TTD  savegame (can be detected incorrectly)
	SGT_TTDP1,  ///< TTDP savegame ( -//- ) (data at NW border)
	SGT_TTDP2,  ///< TTDP savegame in new format (data at SE border)
	SGT_OTTD,   ///< OTTD savegame
	SGT_TTO,    ///< TTO savegame
	SGT_INVALID = 0xFF, ///< broken savegame (used internally)
};

extern FileToSaveLoad _file_to_saveload;

std::string GenerateDefaultSaveName();
void SetSaveLoadError(StringID str);
EncodedString GetSaveLoadErrorType();
EncodedString GetSaveLoadErrorMessage();
SaveLoadResult SaveOrLoad(std::string_view filename, SaveLoadOperation fop, DetailedFileType dft, Subdirectory sb, bool threaded = true);
void WaitTillSaved();
void ProcessAsyncSaveFinish();
void DoExitSave();

void DoAutoOrNetsave(FiosNumberedSaveName &counter);

SaveLoadResult SaveWithFilter(std::shared_ptr<struct SaveFilter> writer, bool threaded);
SaveLoadResult LoadWithFilter(std::shared_ptr<struct LoadFilter> reader);

typedef void AutolengthProc(int);

/** Type of a chunk. */
enum ChunkType : uint8_t {
	CH_RIFF = 0,
	CH_ARRAY = 1,
	CH_SPARSE_ARRAY = 2,
	CH_TABLE = 3,
	CH_SPARSE_TABLE = 4,

	CH_TYPE_MASK = 0xf, ///< All ChunkType values have to be within this mask.
	CH_READONLY, ///< Chunk is never saved.
};

/** Handlers and description of chunk. */
struct ChunkHandler {
	uint32_t id;                          ///< Unique ID (4 letters).
	ChunkType type;                     ///< Type of the chunk. @see ChunkType

	ChunkHandler(uint32_t id, ChunkType type) : id(id), type(type) {}

	/** Ensure the destructor of the sub classes are called as well. */
	virtual ~ChunkHandler() = default;

	/**
	 * Save the chunk.
	 * Must be overridden, unless Chunk type is CH_READONLY.
	 */
	virtual void Save() const { NOT_REACHED(); }

	/**
	 * Load the chunk.
	 * Must be overridden.
	 */
	virtual void Load() const = 0;

	/**
	 * Fix the pointers.
	 * Pointers are saved using the index of the pointed object.
	 * On load, pointers are filled with indices and need to be fixed to point to the real object.
	 * Must be overridden if the chunk saves any pointer.
	 */
	virtual void FixPointers() const {}

	/**
	 * Load the chunk for game preview.
	 * Default implementation just skips the data.
	 * @param len Number of bytes to skip.
	 */
	virtual void LoadCheck(size_t len = 0) const;

	std::string GetName() const
	{
		return std::string()
			+ static_cast<char>(this->id >> 24)
			+ static_cast<char>(this->id >> 16)
			+ static_cast<char>(this->id >> 8)
			+ static_cast<char>(this->id);
	}
};

/** A reference to ChunkHandler. */
using ChunkHandlerRef = std::reference_wrapper<const ChunkHandler>;

/** A table of ChunkHandler entries. */
using ChunkHandlerTable = std::span<const ChunkHandlerRef>;

/** A table of SaveLoad entries. */
using SaveLoadTable = std::span<const struct SaveLoad>;

/** A table of SaveLoadCompat entries. */
using SaveLoadCompatTable = std::span<const struct SaveLoadCompat>;

/** Handler for saving/loading an object to/from disk. */
class SaveLoadHandler {
public:
	std::optional<std::vector<SaveLoad>> load_description; ///< Description derived from savegame being loaded.

	/** Ensure the destructor of the sub classes are called as well. */
	virtual ~SaveLoadHandler() = default;

	/**
	 * Save the object to disk.
	 * @param object The object to store.
	 */
	virtual void Save([[maybe_unused]] void *object) const {}

	/**
	 * Load the object from disk.
	 * @param object The object to load.
	 */
	virtual void Load([[maybe_unused]] void *object) const {}

	/**
	 * Similar to load, but used only to validate savegames.
	 * @param object The object to load.
	 */
	virtual void LoadCheck([[maybe_unused]] void *object) const {}

	/**
	 * A post-load callback to fix #SL_REF integers into pointers.
	 * @param object The object to fix.
	 */
	virtual void FixPointers([[maybe_unused]] void *object) const {}

	/**
	 * Get the description of the fields in the savegame.
	 * @return Save load description.
	 */
	virtual SaveLoadTable GetDescription() const = 0;

	/**
	 * Get the pre-header description of the fields in the savegame.
	 * @return Compatibility save load description.
	 */
	virtual SaveLoadCompatTable GetCompatDescription() const = 0;

	/**
	 * Get the description for how to load the chunk. Depending on the
	 * savegame version this can either use the headers in the savegame or
	 * fall back to backwards compatibility and uses hard-coded headers.
	 * @return The description to load the complete chunk.
	 */
	SaveLoadTable GetLoadDescription() const;
};

/**
 * Default handler for saving/loading an object to/from disk.
 *
 * This handles a few common things for handlers, meaning the actual handler
 * needs less code.
 *
 * Usage: class SlMine : public DefaultSaveLoadHandler<SlMine, MyObject> {}
 *
 * @tparam TImpl The class initializing this template.
 * @tparam TObject The class of the object using this SaveLoadHandler.
 */
template <class TImpl, class TObject>
class DefaultSaveLoadHandler : public SaveLoadHandler {
public:
	SaveLoadTable GetDescription() const override { return static_cast<const TImpl *>(this)->description; }

	SaveLoadCompatTable GetCompatDescription() const override { return static_cast<const TImpl *>(this)->compat_description; }

	/**
	 * Save the object to disk.
	 * @param object The object to store.
	 */
	virtual void Save([[maybe_unused]] TObject *object) const {}
	void Save(void *object) const override { this->Save(static_cast<TObject *>(object)); }

	/**
	 * Load the object from disk.
	 * @param object The object to load.
	 */
	virtual void Load([[maybe_unused]] TObject *object) const {}
	void Load(void *object) const override { this->Load(static_cast<TObject *>(object)); }

	/**
	 * Similar to load, but used only to validate savegames.
	 * @param object The object to load.
	 */
	virtual void LoadCheck([[maybe_unused]] TObject *object) const {}
	void LoadCheck(void *object) const override { this->LoadCheck(static_cast<TObject *>(object)); }

	/**
	 * A post-load callback to fix #SL_REF integers into pointers.
	 * @param object The object to fix.
	 */
	virtual void FixPointers([[maybe_unused]] TObject *object) const {}
	void FixPointers(void *object) const override { this->FixPointers(static_cast<TObject *>(object)); }
};

/** Type of reference (#SLE_REF, #SLE_CONDREF). */
enum SLRefType : uint8_t {
	REF_VEHICLE        =  1, ///< Load/save a reference to a vehicle.
	REF_STATION        =  2, ///< Load/save a reference to a station.
	REF_TOWN           =  3, ///< Load/save a reference to a town.
	REF_VEHICLE_OLD    =  4, ///< Load/save an old-style reference to a vehicle (for pre-4.4 savegames).
	REF_ROADSTOPS      =  5, ///< Load/save a reference to a bus/truck stop.
	REF_ENGINE_RENEWS  =  6, ///< Load/save a reference to an engine renewal (autoreplace).
	REF_CARGO_PACKET   =  7, ///< Load/save a reference to a cargo packet.
	REF_ORDERLIST      =  8, ///< Load/save a reference to an orderlist.
	REF_STORAGE        =  9, ///< Load/save a reference to a persistent storage.
	REF_LINK_GRAPH     = 10, ///< Load/save a reference to a link graph.
	REF_LINK_GRAPH_JOB = 11, ///< Load/save a reference to a link graph job.
};

/**
 * VarTypes is the general bitmasked magic type that tells us
 * certain characteristics about the variable it refers to. For example
 * SLE_FILE_* gives the size(type) as it would be in the savegame and
 * SLE_VAR_* the size(type) as it is in memory during runtime. These are
 * the first 8 bits (0-3 SLE_FILE, 4-7 SLE_VAR).
 * Bits 8-15 are reserved for various flags as explained below
 */
enum VarTypes : uint16_t {
	/* 4 bits allocated a maximum of 16 types for NumberType.
	 * NOTE: the SLE_FILE_NNN values are stored in the savegame! */
	SLE_FILE_END      =  0, ///< Used to mark end-of-header in tables.
	SLE_FILE_I8       =  1,
	SLE_FILE_U8       =  2,
	SLE_FILE_I16      =  3,
	SLE_FILE_U16      =  4,
	SLE_FILE_I32      =  5,
	SLE_FILE_U32      =  6,
	SLE_FILE_I64      =  7,
	SLE_FILE_U64      =  8,
	SLE_FILE_STRINGID =  9, ///< StringID offset into strings-array
	SLE_FILE_STRING   = 10,
	SLE_FILE_STRUCT   = 11,
	/* 4 more possible file-primitives */

	SLE_FILE_TYPE_MASK = 0xf, ///< Mask to get the file-type (and not any flags).
	SLE_FILE_HAS_LENGTH_FIELD = 1 << 4, ///< Bit stored in savegame to indicate field has a length field for each entry.

	/* 4 bits allocated a maximum of 16 types for NumberType */
	SLE_VAR_BL    =  0 << 4,
	SLE_VAR_I8    =  1 << 4,
	SLE_VAR_U8    =  2 << 4,
	SLE_VAR_I16   =  3 << 4,
	SLE_VAR_U16   =  4 << 4,
	SLE_VAR_I32   =  5 << 4,
	SLE_VAR_U32   =  6 << 4,
	SLE_VAR_I64   =  7 << 4,
	SLE_VAR_U64   =  8 << 4,
	SLE_VAR_NULL  =  9 << 4, ///< useful to write zeros in savegame.
	SLE_VAR_STR   = 12 << 4, ///< string pointer
	SLE_VAR_STRQ  = 13 << 4, ///< string pointer enclosed in quotes
	SLE_VAR_NAME  = 14 << 4, ///< old custom name to be converted to a string pointer
	/* 1 more possible memory-primitives */

	/* Shortcut values */
	SLE_VAR_CHAR = SLE_VAR_I8,

	/* Default combinations of variables. As savegames change, so can variables
	 * and thus it is possible that the saved value and internal size do not
	 * match and you need to specify custom combo. The defaults are listed here */
	SLE_BOOL         = SLE_FILE_I8  | SLE_VAR_BL,
	SLE_INT8         = SLE_FILE_I8  | SLE_VAR_I8,
	SLE_UINT8        = SLE_FILE_U8  | SLE_VAR_U8,
	SLE_INT16        = SLE_FILE_I16 | SLE_VAR_I16,
	SLE_UINT16       = SLE_FILE_U16 | SLE_VAR_U16,
	SLE_INT32        = SLE_FILE_I32 | SLE_VAR_I32,
	SLE_UINT32       = SLE_FILE_U32 | SLE_VAR_U32,
	SLE_INT64        = SLE_FILE_I64 | SLE_VAR_I64,
	SLE_UINT64       = SLE_FILE_U64 | SLE_VAR_U64,
	SLE_CHAR         = SLE_FILE_I8  | SLE_VAR_CHAR,
	SLE_STRINGID     = SLE_FILE_STRINGID | SLE_VAR_U32,
	SLE_STRING       = SLE_FILE_STRING   | SLE_VAR_STR,
	SLE_STRINGQUOTE  = SLE_FILE_STRING   | SLE_VAR_STRQ,
	SLE_NAME         = SLE_FILE_STRINGID | SLE_VAR_NAME,

	/* Shortcut values */
	SLE_UINT  = SLE_UINT32,
	SLE_INT   = SLE_INT32,
	SLE_STR   = SLE_STRING,
	SLE_STRQ  = SLE_STRINGQUOTE,

	/* 8 bits allocated for a maximum of 8 flags
	 * Flags directing saving/loading of a variable */
	SLF_ALLOW_CONTROL   = 1 << 8, ///< Allow control codes in the strings.
	SLF_ALLOW_NEWLINE   = 1 << 9, ///< Allow new lines in the strings.
	SLF_REPLACE_TABCRLF = 1 << 10, ///< Replace tabs, cr and lf in the string with spaces.
};

typedef uint32_t VarType;

/** Type of data saved. */
enum SaveLoadType : uint8_t {
	SL_VAR         =  0, ///< Save/load a variable.
	SL_REF         =  1, ///< Save/load a reference.
	SL_STRUCT      =  2, ///< Save/load a struct.

	SL_STDSTR      =  4, ///< Save/load a \c std::string.

	SL_ARR         =  5, ///< Save/load a fixed-size array of #SL_VAR elements.
	SL_VECTOR      =  7, ///< Save/load a vector of #SL_VAR elements.
	SL_REFLIST     =  8, ///< Save/load a list of #SL_REF elements.
	SL_STRUCTLIST  =  9, ///< Save/load a list of structs.

	SL_SAVEBYTE    = 10, ///< Save (but not load) a byte.
	SL_NULL        = 11, ///< Save null-bytes and load to nowhere.

	SL_REFVECTOR   = 12, ///< Save/load a vector of #SL_REF elements.
};

typedef void *SaveLoadAddrProc(void *base, size_t extra);

/** SaveLoad type struct. Do NOT use this directly but use the SLE_ macros defined just below! */
struct SaveLoad {
	std::string name;    ///< Name of this field (optional, used for tables).
	SaveLoadType cmd;    ///< The action to take with the saved/loaded type, All types need different action.
	VarType conv;        ///< Type of the variable to be saved; this field combines both FileVarType and MemVarType.
	uint16_t length;       ///< (Conditional) length of the variable (eg. arrays) (max array size is 65536 elements).
	SaveLoadVersion version_from;   ///< Save/load the variable starting from this savegame version.
	SaveLoadVersion version_to;     ///< Save/load the variable before this savegame version.
	SaveLoadAddrProc *address_proc; ///< Callback proc the get the actual variable address in memory.
	size_t extra_data;              ///< Extra data for the callback proc.
	std::shared_ptr<SaveLoadHandler> handler; ///< Custom handler for Save/Load procs.
};

/**
 * SaveLoad information for backwards compatibility.
 *
 * At SLV_SETTINGS_NAME a new method of keeping track of fields in a savegame
 * was added, where the order of fields is no longer important. For older
 * savegames we still need to know the correct order. This struct is the glue
 * to make that happen.
 */
struct SaveLoadCompat {
	std::string name; ///< Name of the field.
	VarTypes null_type; ///< The type associated with the NULL field; defaults to SLE_FILE_U8 to just count bytes.
	uint16_t null_length; ///< Length of the NULL field.
	SaveLoadVersion version_from; ///< Save/load the variable starting from this savegame version.
	SaveLoadVersion version_to; ///< Save/load the variable before this savegame version.
};

/**
 * Get the NumberType of a setting. This describes the integer type
 * as it is represented in memory
 * @param type VarType holding information about the variable-type
 * @return the SLE_VAR_* part of a variable-type description
 */
inline constexpr VarType GetVarMemType(VarType type)
{
	return GB(type, 4, 4) << 4;
}

/**
 * Get the FileType of a setting. This describes the integer type
 * as it is represented in a savegame/file
 * @param type VarType holding information about the file-type
 * @return the SLE_FILE_* part of a variable-type description
 */
inline constexpr VarType GetVarFileType(VarType type)
{
	return GB(type, 0, 4);
}

/**
 * Check if the given saveload type is a numeric type.
 * @param conv the type to check
 * @return True if it's a numeric type.
 */
inline constexpr bool IsNumericType(VarType conv)
{
	return GetVarMemType(conv) <= SLE_VAR_U64;
}

/**
 * Return expect size in bytes of a VarType
 * @param type VarType to get size of.
 * @return size of type in bytes.
 */
inline constexpr size_t SlVarSize(VarType type)
{
	switch (GetVarMemType(type)) {
		case SLE_VAR_BL: return sizeof(bool);
		case SLE_VAR_I8: return sizeof(int8_t);
		case SLE_VAR_U8: return sizeof(uint8_t);
		case SLE_VAR_I16: return sizeof(int16_t);
		case SLE_VAR_U16: return sizeof(uint16_t);
		case SLE_VAR_I32: return sizeof(int32_t);
		case SLE_VAR_U32: return sizeof(uint32_t);
		case SLE_VAR_I64: return sizeof(int64_t);
		case SLE_VAR_U64: return sizeof(uint64_t);
		case SLE_VAR_NULL: return sizeof(void *);
		case SLE_VAR_STR: return sizeof(std::string);
		case SLE_VAR_STRQ: return sizeof(std::string);
		case SLE_VAR_NAME: return sizeof(std::string);
		default: NOT_REACHED();
	}
}

/**
 * Check if a saveload cmd/type/length entry matches the size of the variable.
 * @param cmd SaveLoadType of entry.
 * @param type VarType of entry.
 * @param length Array length of entry.
 * @param size Actual size of variable.
 * @return true iff the sizes match.
 */
inline constexpr bool SlCheckVarSize(SaveLoadType cmd, VarType type, size_t length, size_t size)
{
	switch (cmd) {
		case SL_VAR: return SlVarSize(type) == size;
		case SL_REF: return sizeof(void *) == size;
		case SL_STDSTR: return SlVarSize(type) == size;
		case SL_ARR: return SlVarSize(type) * length <= size; // Partial load of array is permitted.
		case SL_VECTOR: return sizeof(std::vector<void *>) == size;
		case SL_REFLIST: return sizeof(std::list<void *>) == size;
		case SL_REFVECTOR: return sizeof(std::vector<void *>) == size;
		case SL_SAVEBYTE: return true;
		default: NOT_REACHED();
	}
}

/**
 * Storage of simple variables, references (pointers), and arrays.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param name     Field name for table chunks.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @param extra    Extra data to pass to the address callback function.
 * @note In general, it is better to use one of the SLE_* macros below.
 */
#define SLE_GENERAL_NAME(cmd, name, base, variable, type, length, from, to, extra) \
	SaveLoad {name, cmd, type, length, from, to, [] (void *b, size_t) -> void * { \
		static_assert(SlCheckVarSize(cmd, type, length, sizeof(static_cast<base *>(b)->variable))); \
		assert(b != nullptr); \
		return const_cast<void *>(static_cast<const void *>(std::addressof(static_cast<base *>(b)->variable))); \
	}, extra, nullptr}

/**
 * Storage of simple variables, references (pointers), and arrays with a custom name.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @param extra    Extra data to pass to the address callback function.
 * @note In general, it is better to use one of the SLE_* macros below.
 */
#define SLE_GENERAL(cmd, base, variable, type, length, from, to, extra) SLE_GENERAL_NAME(cmd, #variable, base, variable, type, length, from, to, extra)

/**
 * Storage of a variable in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDVAR(base, variable, type, from, to) SLE_GENERAL(SL_VAR, base, variable, type, 0, from, to, 0)

/**
 * Storage of a variable in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDVARNAME(base, variable, name, type, from, to) SLE_GENERAL_NAME(SL_VAR, name, base, variable, type, 0, from, to, 0)

/**
 * Storage of a reference in some savegame versions.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Type of the reference, a value from #SLRefType.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLE_CONDREF(base, variable, type, from, to) SLE_GENERAL(SL_REF, base, variable, type, 0, from, to, 0)

/**
 * Storage of a fixed-size array of #SL_VAR elements in some savegame versions.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLE_CONDARR(base, variable, type, length, from, to) SLE_GENERAL(SL_ARR, base, variable, type, length, from, to, 0)

/**
 * Storage of a fixed-size array of #SL_VAR elements in some savegame versions.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLE_CONDARRNAME(base, variable, name, type, length, from, to) SLE_GENERAL_NAME(SL_ARR, name, base, variable, type, length, from, to, 0)

/**
 * Storage of a string in some savegame versions.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the string (only used for fixed size buffers).
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLE_CONDSTR(base, variable, type, length, from, to) SLE_GENERAL(SL_STR, base, variable, type, length, from, to, 0)

/**
 * Storage of a \c std::string in some savegame versions.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLE_CONDSSTR(base, variable, type, from, to) SLE_GENERAL(SL_STDSTR, base, variable, type, 0, from, to, 0)

/**
 * Storage of a \c std::string in some savegame versions.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLE_CONDSSTRNAME(base, variable, name, type, from, to) SLE_GENERAL_NAME(SL_STDSTR, name, base, variable, type, 0, from, to, 0)

/**
 * Storage of a list of #SL_REF elements in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDREFLIST(base, variable, type, from, to) SLE_GENERAL(SL_REFLIST, base, variable, type, 0, from, to, 0)

/**
 * Storage of a vector of #SL_REF elements in some savegame versions.
 * @param base     Name of the class or struct containing the vector.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the vector.
 * @param to       Last savegame version that has the vector.
 */
#define SLE_CONDREFVECTOR(base, variable, type, from, to) SLE_GENERAL(SL_REFVECTOR, base, variable, type, 0, from, to, 0)

/**
 * Storage of a vector of #SL_VAR elements in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDVECTOR(base, variable, type, from, to) SLE_GENERAL(SL_VECTOR, base, variable, type, 0, from, to, 0)

/**
 * Storage of a vector of #SL_VAR elements in some savegame versions.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLE_CONDVECTOR(base, variable, type, from, to) SLE_GENERAL(SL_VECTOR, base, variable, type, 0, from, to, 0)

/**
 * Storage of a variable in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_VAR(base, variable, type) SLE_CONDVAR(base, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a variable in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_VARNAME(base, variable, name, type) SLE_CONDVARNAME(base, variable, name, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a reference in every version of a savegame.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Type of the reference, a value from #SLRefType.
 */
#define SLE_REF(base, variable, type) SLE_CONDREF(base, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of fixed-size array of #SL_VAR elements in every version of a savegame.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLE_ARR(base, variable, type, length) SLE_CONDARR(base, variable, type, length, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of fixed-size array of #SL_VAR elements in every version of a savegame.
 * @param base     Name of the class or struct containing the array.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLE_ARRNAME(base, variable, name, type, length) SLE_CONDARRNAME(base, variable, name, type, length, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a \c std::string in every savegame version.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_SSTR(base, variable, type) SLE_CONDSSTR(base, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a \c std::string in every savegame version.
 * @param base     Name of the class or struct containing the string.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param name     Field name for table chunks.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_SSTRNAME(base, variable, name, type) SLE_CONDSSTRNAME(base, variable, name, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a list of #SL_REF elements in every savegame version.
 * @param base     Name of the class or struct containing the list.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_REFLIST(base, variable, type) SLE_CONDREFLIST(base, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a vector of #SL_REF elements in every savegame version.
 * @param base     Name of the class or struct containing the vector.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLE_REFVECTOR(base, variable, type) SLE_CONDREFVECTOR(base, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Only write byte during saving; never read it during loading.
 * When using SLE_SAVEBYTE you will have to read this byte before the table
 * this is in is read. This also means SLE_SAVEBYTE can only be used at the
 * top of a chunk.
 * This is intended to be used to indicate what type of entry this is in a
 * list of entries.
 * @param base     Name of the class or struct containing the variable.
 * @param variable Name of the variable in the class or struct referenced by \a base.
 */
#define SLE_SAVEBYTE(base, variable) SLE_GENERAL(SL_SAVEBYTE, base, variable, 0, 0, SL_MIN_VERSION, SL_MAX_VERSION, 0)

/**
 * Storage of global simple variables, references (pointers), and arrays.
 * @param name     The name of the field.
 * @param cmd      Load/save type. @see SaveLoadType
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 * @param extra    Extra data to pass to the address callback function.
 * @note In general, it is better to use one of the SLEG_* macros below.
 */
#define SLEG_GENERAL(name, cmd, variable, type, length, from, to, extra) \
	SaveLoad {name, cmd, type, length, from, to, [] (void *, size_t) -> void * { \
		static_assert(SlCheckVarSize(cmd, type, length, sizeof(variable))); \
		return static_cast<void *>(std::addressof(variable)); }, extra, nullptr}

/**
 * Storage of a global variable in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLEG_CONDVAR(name, variable, type, from, to) SLEG_GENERAL(name, SL_VAR, variable, type, 0, from, to, 0)

/**
 * Storage of a global reference in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the field.
 * @param to       Last savegame version that has the field.
 */
#define SLEG_CONDREF(name, variable, type, from, to) SLEG_GENERAL(name, SL_REF, variable, type, 0, from, to, 0)

/**
 * Storage of a global fixed-size array of #SL_VAR elements in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 * @param from     First savegame version that has the array.
 * @param to       Last savegame version that has the array.
 */
#define SLEG_CONDARR(name, variable, type, length, from, to) SLEG_GENERAL(name, SL_ARR, variable, type, length, from, to, 0)

/**
 * Storage of a global \c std::string in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the string.
 * @param to       Last savegame version that has the string.
 */
#define SLEG_CONDSSTR(name, variable, type, from, to) SLEG_GENERAL(name, SL_STDSTR, variable, type, 0, from, to, 0)

/**
 * Storage of a structs in some savegame versions.
 * @param name     The name of the field.
 * @param handler  SaveLoadHandler for the structs.
 * @param from     First savegame version that has the struct.
 * @param to       Last savegame version that has the struct.
 */
#define SLEG_CONDSTRUCT(name, handler, from, to) SaveLoad {name, SL_STRUCT, 0, 0, from, to, nullptr, 0, std::make_shared<handler>()}

/**
 * Storage of a global reference list in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDREFLIST(name, variable, type, from, to) SLEG_GENERAL(name, SL_REFLIST, variable, type, 0, from, to, 0)

/**
 * Storage of a global vector of #SL_VAR elements in some savegame versions.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDVECTOR(name, variable, type, from, to) SLEG_GENERAL(name, SL_VECTOR, variable, type, 0, from, to, 0)

/**
 * Storage of a list of structs in some savegame versions.
 * @param name     The name of the field.
 * @param handler  SaveLoadHandler for the list of structs.
 * @param from     First savegame version that has the list.
 * @param to       Last savegame version that has the list.
 */
#define SLEG_CONDSTRUCTLIST(name, handler, from, to) SaveLoad {name, SL_STRUCTLIST, 0, 0, from, to, nullptr, 0, std::make_shared<handler>()}

/**
 * Storage of a global variable in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_VAR(name, variable, type) SLEG_CONDVAR(name, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a global reference in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_REF(name, variable, type) SLEG_CONDREF(name, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a global fixed-size array of #SL_VAR elements in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 * @param length   Number of elements in the array.
 */
#define SLEG_ARR(name, variable, type, length) SLEG_CONDARR(name, variable, type, length, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a global \c std::string in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_SSTR(name, variable, type) SLEG_CONDSSTR(name, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a structs in every savegame version.
 * @param name     The name of the field.
 * @param handler SaveLoadHandler for the structs.
 */
#define SLEG_STRUCT(name, handler) SLEG_CONDSTRUCT(name, handler, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a global reference list in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_REFLIST(name, variable, type) SLEG_CONDREFLIST(name, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a global vector of #SL_VAR elements in every savegame version.
 * @param name     The name of the field.
 * @param variable Name of the global variable.
 * @param type     Storage of the data in memory and in the savegame.
 */
#define SLEG_VECTOR(name, variable, type) SLEG_CONDVECTOR(name, variable, type, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Storage of a list of structs in every savegame version.
 * @param name    The name of the field.
 * @param handler SaveLoadHandler for the list of structs.
 */
#define SLEG_STRUCTLIST(name, handler) SLEG_CONDSTRUCTLIST(name, handler, SL_MIN_VERSION, SL_MAX_VERSION)

/**
 * Field name where the real SaveLoad can be located.
 * @param name The name of the field.
 */
#define SLC_VAR(name) {name, SLE_FILE_U8, 0, SL_MIN_VERSION, SL_MAX_VERSION}

/**
 * Empty space in every savegame version.
 * @param length Length of the empty space in bytes.
 * @param from   First savegame version that has the empty space.
 * @param to     Last savegame version that has the empty space.
 */
#define SLC_NULL(length, from, to) {{}, SLE_FILE_U8, length, from, to}

/**
 * Checks whether the savegame is below \a major.\a minor.
 * @param major Major number of the version to check against.
 * @param minor Minor number of the version to check against. If \a minor is 0 or not specified, only the major number is checked.
 * @return Savegame version is earlier than the specified version.
 */
inline bool IsSavegameVersionBefore(SaveLoadVersion major, uint8_t minor = 0)
{
	extern SaveLoadVersion _sl_version;
	extern uint8_t            _sl_minor_version;
	return _sl_version < major || (minor > 0 && _sl_version == major && _sl_minor_version < minor);
}

/**
 * Checks whether the savegame is below or at \a major. This should be used to repair data from existing
 * savegames which is no longer corrupted in new savegames, but for which otherwise no savegame
 * bump is required.
 * @param major Major number of the version to check against.
 * @return Savegame version is at most the specified version.
 */
inline bool IsSavegameVersionBeforeOrAt(SaveLoadVersion major)
{
	extern SaveLoadVersion _sl_version;
	return _sl_version <= major;
}

/**
 * Checks if some version from/to combination falls within the range of the
 * active savegame version.
 * @param version_from Inclusive savegame version lower bound.
 * @param version_to   Exclusive savegame version upper bound. SL_MAX_VERSION if no upper bound.
 * @return Active savegame version falls within the given range.
 */
inline bool SlIsObjectCurrentlyValid(SaveLoadVersion version_from, SaveLoadVersion version_to)
{
	extern const SaveLoadVersion SAVEGAME_VERSION;
	return version_from <= SAVEGAME_VERSION && SAVEGAME_VERSION < version_to;
}

/**
 * Get the address of the variable. Null-variables don't have an address,
 * everything else has a callback function that returns the address based
 * on the saveload data and the current object for non-globals.
 * @param object The object to get a relative address from, or \c nullptr for global objects.
 * @param sld The save-load configuration for a single variable.
 * @return The address where to store the given variable into.
 */
inline void *GetVariableAddress(const void *object, const SaveLoad &sld)
{
	/* Entry is a null-variable, mostly used to read old savegames etc. */
	if (GetVarMemType(sld.conv) == SLE_VAR_NULL) {
		assert(sld.address_proc == nullptr);
		return nullptr;
	}

	/* Everything else should be a non-null pointer. */
	assert(sld.address_proc != nullptr);
	return sld.address_proc(const_cast<void *>(object), sld.extra_data);
}

int64_t ReadValue(const void *ptr, VarType conv);
void WriteValue(void *ptr, VarType conv, int64_t val);

void SlSetArrayIndex(uint index);
static void SlSetArrayIndex(const ConvertibleThroughBase auto &index) { SlSetArrayIndex(index.base()); }
int SlIterateArray();

void SlSetStructListLength(size_t length);
size_t SlGetStructListLength(size_t limit);

void SlAutolength(AutolengthProc *proc, int arg);
size_t SlGetFieldLength();
void SlSetLength(size_t length);
size_t SlCalcObjMemberLength(const void *object, const SaveLoad &sld);
size_t SlCalcObjLength(const void *object, const SaveLoadTable &slt);

uint8_t SlReadByte();
void SlReadString(std::string &str, size_t length);
void SlWriteByte(uint8_t b);

void SlGlobList(const SaveLoadTable &slt);
void SlCopy(void *object, size_t length, VarType conv);
std::vector<SaveLoad> SlTableHeader(const SaveLoadTable &slt);
std::vector<SaveLoad> SlCompatTableHeader(const SaveLoadTable &slt, const SaveLoadCompatTable &slct);
void SlObject(void *object, const SaveLoadTable &slt);

bool SaveloadCrashWithMissingNewGRFs();

/**
 * Read in bytes from the file/data structure but don't do
 * anything with them, discarding them in effect
 * @param length The amount of bytes that is being treated this way
 */
inline void SlSkipBytes(size_t length)
{
	for (; length != 0; length--) SlReadByte();
}

extern std::string _savegame_format;
extern bool _do_autosave;

/**
 * Default handler for saving/loading a vector to/from disk.
 *
 * This handles a few common things for handlers, meaning the actual handler
 * needs less code.
 *
 * @tparam TImpl The class initializing this template.
 * @tparam TObject The class of the object using this SaveLoadHandler.
 * @tparam TElementType The type of the elements contained within the vector.
 * @tparam MAX_LENGTH maximum number of elements to load.
 */
template <class TImpl, class TObject, class TElementType, size_t MAX_LENGTH = UINT32_MAX>
class VectorSaveLoadHandler : public DefaultSaveLoadHandler<TImpl, TObject> {
public:
	/**
	 * Get instance of vector to load/save.
	 * @param object Object containing vector.
	 * @returns Vector to load/save.
	 */
	virtual std::vector<TElementType> &GetVector(TObject *object) const = 0;

	/**
	 * Get number of elements to load into vector.
	 * @returns Number of elements to load into the vector.
	 * @note This is only overridden if the number of elements comes from a different location due to savegame changes.
	 */
	virtual size_t GetLength() const { return SlGetStructListLength(MAX_LENGTH); }

	void Save(TObject *object) const override
	{
		auto &vector = this->GetVector(object);
		SlSetStructListLength(vector.size());

		for (auto &item : vector) {
			SlObject(&item, this->GetDescription());
		}
	}

	void Load(TObject *object) const override
	{
		auto &vector = this->GetVector(object);
		size_t count = this->GetLength();

		vector.reserve(count);
		while (count-- > 0) {
			auto &item = vector.emplace_back();
			SlObject(&item, this->GetLoadDescription());
		}
	}
};

#endif /* SAVELOAD_H */
