/* $Id$ */

#ifndef NEWGRF_CALLBACKS_H
#define NEWGRF_CALLBACKS_H

/** @file newgrf_callbacks.h
 */

/**
 * List of implemented NewGRF callbacks.
 * Names are formatted as CBID_<CLASS>_<CALLBACK>
 */
enum CallbackID {
	// Powered wagons, if the result is lower as 0x40 then the wagon is powered
	// TODO: interpret the rest of the result, aka "visual effects"
	CBID_TRAIN_WAGON_POWER          = 0x10,

	// Vehicle length, returns the amount of 1/8's the vehicle is shorter
	// only for train vehicles
	CBID_TRAIN_VEHICLE_LENGTH       = 0x11,

	// Refit capacity, the passed vehicle needs to have its ->cargo_type set to
	// the cargo we are refitting to, returns the new cargo capacity
	CBID_VEHICLE_REFIT_CAPACITY     = 0x15,

	CBID_TRAIN_ARTIC_ENGINE         = 0x16,
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
 * Result of a failed callback.
 */
enum {
	CALLBACK_FAILED = 0xFFFF
};

#endif /* NEWGRF_CALLBACKS_H */
