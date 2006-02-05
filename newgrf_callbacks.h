/* $Id$ */

#ifndef NEWGRF_CALLBACKS_H
#define NEWGRF_CALLBACKS_H

/** @file newgrf_callbacks.h
 */

// This enum lists the implemented callbacks
// Use as argument for the GetCallBackResult function (see comments there)
enum CallbackID {
	// Powered wagons, if the result is lower as 0x40 then the wagon is powered
	// TODO: interpret the rest of the result, aka "visual effects"
	CBID_WAGON_POWER = 0x10,

	// Vehicle length, returns the amount of 1/8's the vehicle is shorter
	// only for train vehicles
	CBID_VEH_LENGTH = 0x11,

	// Refit capacity, the passed vehicle needs to have its ->cargo_type set to
	// the cargo we are refitting to, returns the new cargo capacity
	CBID_REFIT_CAP = 0x15,

	CBID_ARTIC_ENGINE = 0x16,
};

// bit positions for rvi->callbackmask, indicates which callbacks are used by an engine
// (some callbacks are always used, and dont appear here)
enum CallbackMask {
	CBM_WAGON_POWER = 0,
	CBM_VEH_LENGTH = 1,
	CBM_REFIT_CAP = 3,
	CBM_ARTIC_ENGINE = 4,
};

enum {
	CALLBACK_FAILED = 0xFFFF
};

#endif /* NEWGRF_CALLBACKS_H */
