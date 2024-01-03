/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_callbacks.h Callbacks that NewGRFs could implement. */

#ifndef NEWGRF_CALLBACKS_H
#define NEWGRF_CALLBACKS_H

/**
 * List of implemented NewGRF callbacks.
 * Most of these callbacks are only triggered when the corresponding
 * bit is set in the callback flags/trigger for a vehicle, house,
 * industry, etc.
 * Names are formatted as CBID_<CLASS>_<CALLBACK>
 */
enum CallbackID {
	/** Set when using the callback resolve system, but not to resolve a callback. */
	CBID_NO_CALLBACK                     = 0x00,

	/** Set when calling a randomizing trigger (almost undocumented). */
	CBID_RANDOM_TRIGGER                  = 0x01,

	/* There are no callbacks 0x02 - 0x0F. */

	/** Visual effects and wagon power. */
	CBID_VEHICLE_VISUAL_EFFECT           = 0x10, // 8 bit callback

	/** Vehicle length, returns the amount of 1/8's the vehicle is shorter for trains and RVs. */
	CBID_VEHICLE_LENGTH                  = 0x11,

	/** Determine the amount of cargo to load per unit of time when using gradual loading. */
	CBID_VEHICLE_LOAD_AMOUNT             = 0x12, // 8 bit callback

	/** Determine whether a newstation should be made available to build. */
	CBID_STATION_AVAILABILITY            = 0x13, // 8 bit callback

	/** Choose a sprite layout to draw, instead of the standard 0-7 range. */
	CBID_STATION_SPRITE_LAYOUT           = 0x14,

	/**
	 * Refit capacity, the passed vehicle needs to have its ->cargo_type set to
	 * the cargo we are refitting to, returns the new cargo capacity.
	 */
	CBID_VEHICLE_REFIT_CAPACITY          = 0x15, // 15 bit callback

	/** Builds articulated engines for trains and RVs. */
	CBID_VEHICLE_ARTIC_ENGINE            = 0x16, // 8 bit callback for grf version < 8

	/** Determine whether the house can be built on the specified tile. */
	CBID_HOUSE_ALLOW_CONSTRUCTION        = 0x17, // 8 bit callback

	/** AI construction/purchase selection */
	CBID_GENERIC_AI_PURCHASE_SELECTION   = 0x18, // 8 bit callback, implemented for stations only

	/** Determine the cargo "suffixes" for each refit possibility of a cargo. */
	CBID_VEHICLE_CARGO_SUFFIX            = 0x19,

	/** Determine the next animation frame for a house. */
	CBID_HOUSE_ANIMATION_NEXT_FRAME      = 0x1A, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_HOUSE_ANIMATION_START_STOP      = 0x1B, // 15 bit callback

	/** Called whenever the construction state of a house changes. */
	CBID_HOUSE_CONSTRUCTION_STATE_CHANGE = 0x1C, // 15 bit callback

	/** Determine whether a wagon can be attached to an already existing train. */
	CBID_TRAIN_ALLOW_WAGON_ATTACH        = 0x1D,

	/** Called to determine the colour of a town building. */
	CBID_HOUSE_COLOUR                    = 0x1E, // 15 bit callback

	/** Called to decide how much cargo a town building can accept. */
	CBID_HOUSE_CARGO_ACCEPTANCE          = 0x1F, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_HOUSE_ANIMATION_SPEED           = 0x20, // 8 bit callback

	/** Called periodically to determine if a house should be destroyed. */
	CBID_HOUSE_DESTRUCTION               = 0x21, // 8 bit callback

	/** Called to determine if the given industry type is available. For grf version >= 8 also a probability can be returned. */
	CBID_INDUSTRY_PROBABILITY            = 0x22, // 15 bit callback

	/**
	 * This callback is called from vehicle purchase lists. It returns a value to be
	 * used as a custom string ID in the 0xD000 range.
	 */
	CBID_VEHICLE_ADDITIONAL_TEXT         = 0x23,

	/** Called when building a station to customize the tile layout */
	CBID_STATION_TILE_LAYOUT             = 0x24, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_INDTILE_ANIM_START_STOP         = 0x25, // 15 bit callback

	/** Called to determine industry tile next animation frame. */
	CBID_INDTILE_ANIM_NEXT_FRAME         = 0x26, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_INDTILE_ANIMATION_SPEED         = 0x27, // 8 bit callback

	/** Called to determine if the given industry can be built on specific area. */
	CBID_INDUSTRY_LOCATION               = 0x28, // 15 bit callback

	/** Called on production changes, so it can be adjusted. */
	CBID_INDUSTRY_PRODUCTION_CHANGE      = 0x29, // 15 bit callback

	/** Called to determine which cargoes a town building should accept. */
	CBID_HOUSE_ACCEPT_CARGO              = 0x2A, // 15 bit callback

	/** Called to query the cargo acceptance of the industry tile */
	CBID_INDTILE_CARGO_ACCEPTANCE        = 0x2B, // 15 bit callback

	/** Called to determine which cargoes an industry should accept. */
	CBID_INDTILE_ACCEPT_CARGO            = 0x2C, // 15 bit callback

	/**
	 * Called to determine if a specific colour map should be used for a vehicle
	 * instead of the default livery.
	 */
	CBID_VEHICLE_COLOUR_MAPPING          = 0x2D, // 15 bit callback

	/** Called to determine how much cargo a town building produces. */
	CBID_HOUSE_PRODUCE_CARGO             = 0x2E, // 15 bit callback

	/** Called to determine if the given industry tile can be built on specific tile. */
	CBID_INDTILE_SHAPE_CHECK             = 0x2F, // 15 bit callback

	/** Called to determine the type (if any) of foundation to draw for industry tile. */
	CBID_INDTILE_DRAW_FOUNDATIONS        = 0x30, // 15 bit callback

	/**
	 * Called when the company (or AI) tries to start or stop a vehicle. Mainly
	 * used for preventing a vehicle from leaving the depot.
	 */
	CBID_VEHICLE_START_STOP_CHECK        = 0x31, // 15 bit callback, but 0xFF test is done with 8 bit

	/** Called for every vehicle every 32 days (not all on same date though). */
	CBID_VEHICLE_32DAY_CALLBACK          = 0x32, // 2 bit callback

	/** Called to play a special sound effect */
	CBID_VEHICLE_SOUND_EFFECT            = 0x33, // 15 bit callback

	/** Return the vehicles this given vehicle can be "upgraded" to. */
	CBID_VEHICLE_AUTOREPLACE_SELECTION   = 0x34, // 15 bit callback, not implemented

	/** Called monthly on production changes, so it can be adjusted more frequently */
	CBID_INDUSTRY_MONTHLYPROD_CHANGE     = 0x35, // 15 bit callback

	/**
	 * Called to modify various vehicle properties. Callback parameter 1
	 * specifies the property index, as used in Action 0, to change.
	 */
	CBID_VEHICLE_MODIFY_PROPERTY         = 0x36, // 8/15 bit depends on queried property

	/** Called to determine text to display after cargo name */
	CBID_INDUSTRY_CARGO_SUFFIX           = 0x37, // 15 bit callback, but 0xFF test is done with 8 bit

	/** Called to determine more text in the fund industry window */
	CBID_INDUSTRY_FUND_MORE_TEXT         = 0x38, // 15 bit callback

	/** Called to calculate the income of delivered cargo */
	CBID_CARGO_PROFIT_CALC               = 0x39, // 15 bit callback

	/** Called to determine more text in the industry window */
	CBID_INDUSTRY_WINDOW_MORE_TEXT       = 0x3A, // 15 bit callback

	/** Called to determine industry special effects */
	CBID_INDUSTRY_SPECIAL_EFFECT         = 0x3B, // 15 bit callback

	/** Called to determine if industry can alter the ground below industry tile */
	CBID_INDTILE_AUTOSLOPE               = 0x3C, // 15 bit callback

	/** Called to determine if the industry can still accept or refuse more cargo arrival */
	CBID_INDUSTRY_REFUSE_CARGO           = 0x3D, // 15 bit callback

	/* There are no callbacks 0x3E - 0x13F */

	/** Called for periodically starting or stopping the animation. */
	CBID_STATION_ANIM_START_STOP         = 0x140, // 15 bit callback

	/** Called to determine station tile next animation frame. */
	CBID_STATION_ANIM_NEXT_FRAME         = 0x141, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_STATION_ANIMATION_SPEED         = 0x142, // 8 bit callback

	/** Called to determine whether a town building can be destroyed. */
	CBID_HOUSE_DENY_DESTRUCTION          = 0x143, // 15 bit callback

	/** Select an ambient sound to play for a given type of tile. */
	CBID_SOUNDS_AMBIENT_EFFECT           = 0x144, // 15 bit callback

	/** Called to calculate part of a station rating. */
	CBID_CARGO_STATION_RATING_CALC       = 0x145, // 15 bit callback

	/** Allow signal sprites to be replaced dynamically. */
	CBID_NEW_SIGNALS_SPRITE_DRAW         = 0x146, // 15 bit callback, not implemented

	/** Add an offset to the default sprite numbers to show another sprite. */
	CBID_CANALS_SPRITE_OFFSET            = 0x147, // 15 bit callback

	/** Called when a cargo type specified in property 20 is accepted. */
	CBID_HOUSE_WATCHED_CARGO_ACCEPTED    = 0x148, // 15 bit callback

	/** Callback done for each tile of a station to check the slope. */
	CBID_STATION_LAND_SLOPE_CHECK        = 0x149, // 15 bit callback

	/** Called to determine the colour of an industry. */
	CBID_INDUSTRY_DECIDE_COLOUR          = 0x14A, // 4 bit callback

	/** Customize the input cargo types of a newly build industry. */
	CBID_INDUSTRY_INPUT_CARGO_TYPES      = 0x14B, // 8 bit callback

	/** Customize the output cargo types of a newly build industry. */
	CBID_INDUSTRY_OUTPUT_CARGO_TYPES     = 0x14C, // 8 bit callback

	/** Called on the Get Tile Description for an house tile. */
	CBID_HOUSE_CUSTOM_NAME               = 0x14D, // 15 bit callback

	/** Called to determine the type (if any) of foundation to draw for house tile. */
	CBID_HOUSE_DRAW_FOUNDATIONS          = 0x14E, // 15 bit callback

	/** Called to determine if one can alter the ground below a house tile */
	CBID_HOUSE_AUTOSLOPE                 = 0x14F, // 15 bit callback

	/** Called to determine the type (if any) of foundation to draw for an airport tile. */
	CBID_AIRPTILE_DRAW_FOUNDATIONS       = 0x150, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_AIRPTILE_ANIM_START_STOP        = 0x152, // 15 bit callback

	/** Called to determine airport tile next animation frame. */
	CBID_AIRPTILE_ANIM_NEXT_FRAME        = 0x153, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_AIRPTILE_ANIMATION_SPEED        = 0x154, // 8 bit callback

	/**
	 * This callback is called from airport list. It returns a value to be
	 * used as a custom string ID in the 0xD000 range.
	 */
	CBID_AIRPORT_ADDITIONAL_TEXT         = 0x155, // 15 bit callback

	/** Called to determine text to show as airport layout name. */
	CBID_AIRPORT_LAYOUT_NAME             = 0x156, // 15 bit callback

	/** Callback done for each tile of an object to check the slope. */
	CBID_OBJECT_LAND_SLOPE_CHECK         = 0x157, // 15 bit callback

	/** Determine the next animation frame for a house. */
	CBID_OBJECT_ANIMATION_NEXT_FRAME     = 0x158, // 15 bit callback

	/** Called for periodically starting or stopping the animation. */
	CBID_OBJECT_ANIMATION_START_STOP     = 0x159, // 15 bit callback

	/** Called to indicate how long the current animation frame should last. */
	CBID_OBJECT_ANIMATION_SPEED          = 0x15A, // 8 bit callback

	/** Called to determine the colour of a town building. */
	CBID_OBJECT_COLOUR                   = 0x15B, // 15 bit callback

	/** Called to determine more text in the fund object window */
	CBID_OBJECT_FUND_MORE_TEXT           = 0x15C, // 15 bit callback

	/** Called to determine if one can alter the ground below an object tile */
	CBID_OBJECT_AUTOSLOPE                = 0x15D, // 15 bit callback

	/** Called to determine the cost factor for refitting a vehicle. */
	CBID_VEHICLE_REFIT_COST              = 0x15E, // 15 bit callback

	/** Called when industry is built to set initial production level. */
	CBID_INDUSTRY_PROD_CHANGE_BUILD      = 0x15F, // 15 bit callback

	/** Called to spawn visual effects for vehicles. */
	CBID_VEHICLE_SPAWN_VISUAL_EFFECT     = 0x160, // 15 bit callback

	/** Called to determine the engine name to show. */
	CBID_VEHICLE_NAME                    = 0x161, // 15 bit callback

	/** Called to determine probability during build. */
	CBID_VEHICLE_BUILD_PROBABILITY       = 0x162, // 15 bit callback
};

/**
 * Callback masks for vehicles, indicates which callbacks are used by a vehicle.
 * Some callbacks are always used and don't have a mask.
 */
enum VehicleCallbackMask {
	CBM_VEHICLE_VISUAL_EFFECT  = 0, ///< Visual effects and wagon power (trains, road vehicles and ships)
	CBM_VEHICLE_LENGTH         = 1, ///< Vehicle length (trains and road vehicles)
	CBM_VEHICLE_LOAD_AMOUNT    = 2, ///< Load amount
	CBM_VEHICLE_REFIT_CAPACITY = 3, ///< Cargo capacity after refit
	CBM_VEHICLE_ARTIC_ENGINE   = 4, ///< Add articulated engines (trains and road vehicles)
	CBM_VEHICLE_CARGO_SUFFIX   = 5, ///< Show suffix after cargo name
	CBM_VEHICLE_COLOUR_REMAP   = 6, ///< Change colour mapping of vehicle
	CBM_VEHICLE_SOUND_EFFECT   = 7, ///< Vehicle uses custom sound effects
	CBM_VEHICLE_NAME           = 8, ///< Engine name
};

/**
 * Callback masks for stations.
 */
enum StationCallbackMask {
	CBM_STATION_AVAIL                = 0, ///< Availability of station in construction window
	CBM_STATION_SPRITE_LAYOUT        = 1, ///< Use callback to select a sprite layout to use
	CBM_STATION_ANIMATION_NEXT_FRAME = 2, ///< Use a custom next frame callback
	CBM_STATION_ANIMATION_SPEED      = 3, ///< Customize the animation speed of the station
	CBM_STATION_SLOPE_CHECK          = 4, ///< Check slope of new station tiles
};

/**
 * Callback masks for road stops.
 */
enum RoadStopCallbackMask {
	CBM_ROAD_STOP_AVAIL                = 0, ///< Availability of road stop in construction window
	CBM_ROAD_STOP_ANIMATION_NEXT_FRAME = 1, ///< Use a custom next frame callback
	CBM_ROAD_STOP_ANIMATION_SPEED      = 2, ///< Customize the animation speed of the road stop
};

/**
 * Callback masks for houses.
 */
enum HouseCallbackMask {
	CBM_HOUSE_ALLOW_CONSTRUCTION        =  0, ///< decide whether the house can be built on a given tile
	CBM_HOUSE_ANIMATION_NEXT_FRAME      =  1, ///< decides next animation frame
	CBM_HOUSE_ANIMATION_START_STOP      =  2, ///< periodically start/stop the animation
	CBM_HOUSE_CONSTRUCTION_STATE_CHANGE =  3, ///< change animation when construction state changes
	CBM_HOUSE_COLOUR                    =  4, ///< decide the colour of the building
	CBM_HOUSE_CARGO_ACCEPTANCE          =  5, ///< decides amount of cargo acceptance
	CBM_HOUSE_ANIMATION_SPEED           =  6, ///< decides animation speed
	CBM_HOUSE_DESTRUCTION               =  7, ///< trigger destruction of building
	CBM_HOUSE_ACCEPT_CARGO              =  8, ///< decides accepted types
	CBM_HOUSE_PRODUCE_CARGO             =  9, ///< custom cargo production
	CBM_HOUSE_DENY_DESTRUCTION          = 10, ///< conditional protection
	CBM_HOUSE_DRAW_FOUNDATIONS          = 11, ///< decides if default foundations need to be drawn
	CBM_HOUSE_AUTOSLOPE                 = 12, ///< decides allowance of autosloping
};

/**
 * Callback masks for canals.
 */
enum CanalCallbackMask {
	CBM_CANAL_SPRITE_OFFSET       = 0, ///< Enable add sprite offset callback
};

/**
 * Callback masks for cargoes.
 */
enum CargoCallbackMask {
	CBM_CARGO_PROFIT_CALC         = 0, ///< custom profit calculation
	CBM_CARGO_STATION_RATING_CALC = 1, ///< custom station rating for this cargo type
};

/**
 * Callback masks for Industries
 */
enum IndustryCallbackMask {
	CBM_IND_PROBABILITY               =  0, ///< industry availability/probability callback
	CBM_IND_PRODUCTION_CARGO_ARRIVAL  =  1, ///< call production callback when cargo arrives at the industry
	CBM_IND_PRODUCTION_256_TICKS      =  2, ///< call production callback every 256 ticks
	CBM_IND_LOCATION                  =  3, ///< check industry construction on given area
	CBM_IND_PRODUCTION_CHANGE         =  4, ///< controls random production change
	CBM_IND_MONTHLYPROD_CHANGE        =  5, ///< controls monthly random production change
	CBM_IND_CARGO_SUFFIX              =  6, ///< cargo sub-type display
	CBM_IND_FUND_MORE_TEXT            =  7, ///< additional text in fund window
	CBM_IND_WINDOW_MORE_TEXT          =  8, ///< additional text in industry window
	CBM_IND_SPECIAL_EFFECT            =  9, ///< control special effects
	CBM_IND_REFUSE_CARGO              = 10, ///< option out of accepting cargo
	CBM_IND_DECIDE_COLOUR             = 11, ///< give a custom colour to newly build industries
	CBM_IND_INPUT_CARGO_TYPES         = 12, ///< customize the cargoes the industry requires
	CBM_IND_OUTPUT_CARGO_TYPES        = 13, ///< customize the cargoes the industry produces
	CBM_IND_PROD_CHANGE_BUILD         = 14, ///< initialise production level on construction
};

/**
 * Callback masks for industry tiles
 */
enum IndustryTileCallbackMask {
	CBM_INDT_ANIM_NEXT_FRAME          = 0,  ///< decides next animation frame
	CBM_INDT_ANIM_SPEED               = 1,  ///< decides animation speed
	CBM_INDT_CARGO_ACCEPTANCE         = 2,  ///< decides amount of cargo acceptance
	CBM_INDT_ACCEPT_CARGO             = 3,  ///< decides accepted types
	CBM_INDT_SHAPE_CHECK              = 4,  ///< decides slope suitability
	CBM_INDT_DRAW_FOUNDATIONS         = 5,  ///< decides if default foundations need to be drawn
	CBM_INDT_AUTOSLOPE                = 6,  ///< decides allowance of autosloping
};

/**
 * Callback masks for objects
 */
enum ObjectCallbackMask {
	CBM_OBJ_SLOPE_CHECK               =  0, ///< decides slope suitability
	CBM_OBJ_ANIMATION_NEXT_FRAME      =  1, ///< decides next animation frame
	CBM_OBJ_ANIMATION_SPEED           =  2, ///< decides animation speed
	CBM_OBJ_COLOUR                    =  3, ///< decide the colour of the building
	CBM_OBJ_FUND_MORE_TEXT            =  4, ///< additional text in fund window
	CBM_OBJ_AUTOSLOPE                 =  5, ///< decides allowance of autosloping
};

/**
 * Callback masks for airport tiles
 */
enum AirportTileCallbackMask {
	CBM_AIRT_ANIM_NEXT_FRAME          = 0,  ///< decides next animation frame
	CBM_AIRT_ANIM_SPEED               = 1,  ///< decides animation speed
	CBM_AIRT_SHAPE_CHECK              = 4,  ///< decides slope suitability
	CBM_AIRT_DRAW_FOUNDATIONS         = 5,  ///< decides if default foundations need to be drawn
	CBM_AIRT_AUTOSLOPE                = 6,  ///< decides allowance of autosloping
};

/**
 * Different values for Callback result evaluations
 */
static const uint CALLBACK_FAILED              = 0xFFFF; ///< Result of a failed callback.
static const uint CALLBACK_HOUSEPRODCARGO_END  = 0x20FF; ///< Sentinel indicating that the loop for CBID_HOUSE_PRODUCE_CARGO has ended

#endif /* NEWGRF_CALLBACKS_H */
