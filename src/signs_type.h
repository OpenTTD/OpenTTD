/* $Id$ */

/** @file signs_type.h Types related to signs */

#ifndef SIGNS_TYPE_H
#define SIGNS_TYPE_H

typedef uint16 SignID;
struct Sign;

enum {
	INVALID_SIGN = 0xFFFF,

	MAX_LENGTH_SIGN_NAME_BYTES  =  31, ///< The maximum length of a sign name in bytes including '\0'
	MAX_LENGTH_SIGN_NAME_PIXELS = 255, ///< The maximum length of a sign name in pixels
};

#endif /* SIGNS_TYPE_H */
