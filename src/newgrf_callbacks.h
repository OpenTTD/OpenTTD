/* $Id$ */

/** @file newgrf_callbacks.h
 */

#ifndef NEWGRF_CALLBACKS_H
#define NEWGRF_CALLBACKS_H


/**
 * List of implemented NewGRF callbacks.
 * Names are formatted as CBID_<CLASS>_<CALLBACK>
 */
enum CallbackID {
	/* Set when calling a randomizing trigger (almost undocumented) */
	CBID_RANDOM_TRIGGER             = 0x01,

	/* Powered wagons, if the result is lower as 0x40 then the wagon is powered
	 * @todo : interpret the rest of the result, aka "visual effects" */
	CBID_TRAIN_WAGON_POWER          = 0x10,

	/* Vehicle length, returns the amount of 1/8's the vehicle is shorter
	 * only for train vehicles */
	CBID_TRAIN_VEHICLE_LENGTH       = 0x11,

	/* Called (if appropriate bit in callback mask is set) to determine the
	 * amount of cargo to load per unit of time when using gradual loading. */
	CBID_VEHICLE_LOAD_AMOUNT        = 0x12,

	/* Called (if appropriate bit in callback mask is set) to determine if a
	 * newstation should be made available to build */
	CBID_STATION_AVAILABILITY       = 0x13,

	/* Called (if appropriate bit in callback mask is set) when drawing a tile
	 * to choose a sprite layout to draw, instead of the standard 0-7 range */
	CBID_STATION_SPRITE_LAYOUT      = 0x14,

	/* Refit capacity, the passed vehicle needs to have its ->cargo_type set to
	 * the cargo we are refitting to, returns the new cargo capacity */
	CBID_VEHICLE_REFIT_CAPACITY     = 0x15,

	CBID_TRAIN_ARTIC_ENGINE         = 0x16,

	/* Called (if appropriate bit in callback mask is set) to determine whether
	 * the house can be built on the specified tile. */
	CBID_HOUSE_ALLOW_CONSTRUCTION   = 0x17,

	CBID_VEHICLE_CARGO_SUFFIX       = 0x19,

	/* Called (if appropriate bit in callback mask is set) to determine
	 * the next animation frame. */
	CBID_HOUSE_ANIMATION_NEXT_FRAME = 0x1A,

	/* Called (if appropriate bit in callback mask is set) for periodically
	 * starting or stopping the animation. */
	CBID_HOUSE_ANIMATION_START_STOP = 0x1B,

	/* Called (if appropriate bit in callback mask is set) whenever the
	 * construction state of a house changes. */
	CBID_CONSTRUCTION_STATE_CHANGE  = 0x1C,

	CBID_TRAIN_ALLOW_WAGON_ATTACH   = 0x1D,

	/* Called (if appropriate bit in callback mask is set) to determine the
	 * colour of a town building. */
	CBID_BUILDING_COLOUR            = 0x1E,

	/* Called (if appropriate bit in callback mask is set) to decide how much
	 * cargo a town building can accept. */
	CBID_HOUSE_CARGO_ACCEPTANCE     = 0x1F,

	/* Called (if appropriate bit in callback mask is set) to indicate
	 * how long the current animation frame should last. */
	CBID_HOUSE_ANIMATION_SPEED      = 0x20,

	/* Called (if appropriate bit in callback mask is set) periodically to
	 * determine if a house should be destroyed. */
	CBID_HOUSE_DESTRUCTION          = 0x21,

	/* Called to determine if the given industry type is available */
	CBID_INDUSTRY_AVAILABLE         = 0x22, // not yet implemented

	/* This callback is called from vehicle purchase lists. It returns a value to be
	 * used as a custom string ID in the 0xD000 range. */
	CBID_VEHICLE_ADDITIONAL_TEXT    = 0x23,

	/* Called when building a station to customize the tile layout */
	CBID_STATION_TILE_LAYOUT        = 0x24,

	/* Called for periodically starting or stopping the animation. */
	CBID_INDTILE_ANIM_START_STOP    = 0x25, // not yet implemented

	/* Called to determine industry tile next animation frame. */
	CBID_INDTILE_ANIM_NEXT_FRAME    = 0x26, // not yet implemented

	/* Called to indicate how long the current animation frame should last. */
	CBID_INDTILE_ANIMATION_SPEED    = 0x27, // not yet implemented

	/* Called to determine if the given industry can be built on specific area */
	CBID_INDUSTRY_LOCATION          = 0x28, // not yet implemented

	/* Called on production changes, so it can be adjusted */
	CBID_INDUSTRY_PRODUCTION_CHANGE = 0x29, // not yet implemented

	/* Called (if appropriate bit in callback mask is set) to determine which
	 * cargoes a town building should accept. */
	CBID_HOUSE_ACCEPT_CARGO         = 0x2A,

	/* Called to query the cargo acceptance of the industry tile */
	CBID_INDTILE_ACCEPT_CARGO       = 0x2B, // not yet implemented

	/* Called to determine which cargoes an industry should accept. */
	CBID_INDUSTRY_ACCEPT_CARGO      = 0x2C, // not yet implemented

	/* Called to determine if a specific colour map should be used for a vehicle
	 * instead of the default livery */
	CBID_VEHICLE_COLOUR_MAPPING     = 0x2D,

	/* Called (if appropriate bit in callback mask is set) to determine how much
	 * cargo a town building produces. */
	CBID_HOUSE_PRODUCE_CARGO        = 0x2E, // not yet implemented

	/* Called to determine if the given industry tile can be built on specific tile */
	CBID_INDTILE_SHAPE_CHECK        = 0x2F, // not yet implemented

	/* Called to determine the type (if any) of foundation to draw for industry tile */
	CBID_INDUSTRY_DRAW_FOUNDATIONS  = 0x30, // not yet implemented

	/* Called when the player (or AI) tries to start or stop a vehicle. Mainly
	 * used for preventing a vehicle from leaving the depot. */
	CBID_VEHICLE_START_STOP_CHECK   = 0x31,

	/* Called to play a special sound effect */
	CBID_VEHICLE_SOUND_EFFECT       = 0x33,

	/* Called monthly on production changes, so it can be adjusted more frequently */
	CBID_INDUSTRY_MONTHLYPROD_CHANGE= 0x35, // not yet implemented

	/* Called to modify various vehicle properties. Callback parameter 1
	 * specifies the property index, as used in Action 0, to change. */
	CBID_VEHICLE_MODIFY_PROPERTY    = 0x36,

	/* Called to determine text to display after cargo name */
	CBID_INDUSTRY_CARGO_SUFFIX      = 0x37, // not yet implemented

	/* Called to determine more text in the fund industry window */
	CBID_INDUSTRY_FUND_MORE_TEXT    = 0x38, // not yet implemented

	/* Called to calculate the income of delivered cargo */
	CBID_CARGO_PROFIT_CALC          = 0x39,

	/* Called to determine more text in the industry window */
	CBID_INDUSTRY_WINDOW_MORE_TEXT  = 0x3A,

	/* Called to determine industry special effects */
	CBID_INDUSTRY_SPECIAL_EFFECT    = 0x3B, // not yet implemented

	/* Called to determine if industry can alter the ground below industry tile */
	CBID_INDUSTRY_AUTOSLOPE         = 0x3C, // not yet implemented

	/* Called to determine if the industry can still accept or refuse  more cargo arrival */
	CBID_INDUSTRY_REFUSE_CARGO      = 0x3D, // not yet implemented

	/* Called (if appropriate bit in callback mask set) to determine whether a
	 * town building can be destroyed. */
	CBID_HOUSE_DENY_DESTRUCTION     = 0x143,

	/* Called to calculate part of a station rating */
	CBID_CARGO_STATION_RATING_CALC  = 0x145,
};

/**
 * Callback masks for vehicles, indicates which callbacks are used by a vehicle.
 * Some callbacks are always used and don't have a mask.
 */
enum VehicleCallbackMask {
	CBM_WAGON_POWER    = 0, ///< Powered wagons (trains only)
	CBM_VEHICLE_LENGTH = 1, ///< Vehicle length (trains only)
	CBM_LOAD_AMOUNT    = 2, ///< Load amount
	CBM_REFIT_CAPACITY = 3, ///< Cargo capacity after refit
	CBM_ARTIC_ENGINE   = 4, ///< Add articulated engines (trains only)
	CBM_CARGO_SUFFIX   = 5, ///< Show suffix after cargo name
	CBM_COLOUR_REMAP   = 6, ///< Change colour mapping of vehicle
	CBM_SOUND_EFFECT   = 7, ///< Vehicle uses custom sound effects
};

/**
 * Callback masks for stations.
 */
enum StationCallbackMask {
	CBM_STATION_AVAIL = 0, ///< Availability of station in construction window
	CBM_CUSTOM_LAYOUT = 1, ///< Use callback to select a tile layout to use
};

/**
 * Callback masks for houses.
 */
enum HouseCallbackMask {
	CBM_HOUSE_ALLOW_CONSTRUCTION  = 0,
	CBM_ANIMATION_NEXT_FRAME      = 1,
	CBM_ANIMATION_START_STOP      = 2,
	CBM_CONSTRUCTION_STATE_CHANGE = 3,
	CBM_BUILDING_COLOUR           = 4,
	CBM_CARGO_ACCEPTANCE          = 5,
	CBM_ANIMATION_SPEED           = 6,
	CBM_HOUSE_DESTRUCTION         = 7,
	CBM_HOUSE_ACCEPT_CARGO        = 8,
	CBM_HOUSE_PRODUCE_CARGO       = 9,
	CBM_HOUSE_DENY_DESTRUCTION    = 10,
};

/**
 * Callback masks for cargos.
 */
enum CargoCallbackMask {
	CBM_CARGO_PROFIT_CALC         = 0,
	CBM_CARGO_STATION_RATING_CALC = 1,
};

/**
 * Callback masks for Industries
 */
enum IndustryCallbackMask {
	CBM_IND_AVAILABLE                 = 0,  ///< industry availability callback
	CBM_IND_PRODUCTION_CARGO_ARRIVAL  = 1,  ///< call production callback when cargo arrives at the industry
	CBM_IND_PRODUCTION_256_TICKS      = 2,  ///< call production callback every 256 ticks
	CBM_IND_LOCATION                  = 3,  ///< check industry construction on given area
	CBM_IND_PRODUCTION_CHANGE         = 4,  ///< controls random production change
	CBM_IND_MONTHLYPROD_CHANGE        = 5,  ///< controls monthly random production change
	CBM_IND_CARGO_SUFFIX              = 6,  ///< cargo sub-type display
	CBM_IND_FUND_MORE_TEXT            = 7,  ///< additional text in fund window
	CBM_IND_WINDOW_MORE_TEXT          = 8,  ///< additional text in industry window
	CBM_IND_SPECIAL_EFFECT            = 9,  ///< control special effects
	CBM_IND_REFUSE_CARGO              = 10, ///< option out of accepting cargo
};

/**
 * Callback masks for industry tiles
 */
enum IndustryTileCallbackMask {
	CBM_INDT_ANIM_NEXT_FRAME          = 0,  ///< decides next animation frame
	CBM_INDT_ANIM_SPEED               = 1,  ///< decides animation speed
	CBM_INDT_ACCEPTANCE_CARGO         = 2,  ///< decides amount of cargo acceptance
	CBM_INDT_ACCEPT_CARGO             = 3,  ///< decides accepted types
	CBM_INDT_SHAPE_CHECK              = 4,  ///< decides slope suitability
	CBM_INDT_DRAW_FOUNDATIONS         = 5,  ///< decides if default foundations need to be drawn
	CBM_INDT_AUTOSLOPE                = 6,  ///< decides allowance of autosloping
};

/**
 * Result of a failed callback.
 */
enum {
	CALLBACK_FAILED = 0xFFFF
};

#endif /* NEWGRF_CALLBACKS_H */
