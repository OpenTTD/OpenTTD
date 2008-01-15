/* $Id$ */

/** @file newgrf_string_type.h */

#ifndef NEWGRF_STRING_TYPE_H
#define NEWGRF_STRING_TYPE_H

#include "strings_type.h"

/**
 * A string with the required information to perform a GRF string remapping.
 */
struct GRFMappedStringID
{
private:
	/** The GRF ID associated to the to-be-remapped string */
	uint32 grfid;
	/** The string; when grfid != 0 it should be remapped */
	StringID string;

public:
	/**
	 * Create the struct.
	 * @param str    the string to store (or remap)
	 * @param grf_id the GRF to remap it with
	 */
	GRFMappedStringID(StringID str, uint32 grf_id) : grfid(grf_id), string(str) {}

	/**
	 * An empty string.
	 */
	GRFMappedStringID() {}

	/** Cast operator, returns the string */
	inline operator StringID() const
	{
		return string;
	}

	/** Assigns the string and resets the GRF ID. */
	GRFMappedStringID& operator = (StringID str)
	{
		string = str;
		grfid = 0;
		return *this;
	}

	/**
	 * Map the string.
	 */
	void MapString();
};

#endif /* NEWGRF_STRING_TYPE_H */
