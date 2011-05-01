/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_error.hpp Everything to query errors. */

#ifndef AI_ERROR_HPP
#define AI_ERROR_HPP

#include "ai_object.hpp"
#include <map>

/**
 * Helper to write precondition enforcers for the AI API in an abbreviated manner.
 * @param returnval The value to return on failure.
 * @param condition The condition that must be obeyed.
 */
#define EnforcePrecondition(returnval, condition)               \
	if (!(condition)) {                                           \
		AIObject::SetLastError(AIError::ERR_PRECONDITION_FAILED);   \
		return returnval;                                           \
	}

/**
 * Helper to write precondition enforcers for the AI API in an abbreviated manner.
 * @param returnval The value to return on failure.
 * @param condition The condition that must be obeyed.
 * @param error_code The error code passed to AIObject::SetLastError.
 */
#define EnforcePreconditionCustomError(returnval, condition, error_code)   \
	if (!(condition)) {                                                      \
		AIObject::SetLastError(error_code);                                    \
		return returnval;                                                      \
	}

/**
 * Class that handles all error related functions.
 */
class AIError : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIError"; }

	/**
	 * All categories errors can be divided in.
	 */
	enum ErrorCategories {
		ERR_CAT_NONE = 0, ///< Error messages not related to any category.
		ERR_CAT_GENERAL,  ///< Error messages related to general things.
		ERR_CAT_VEHICLE,  ///< Error messages related to building / maintaining vehicles.
		ERR_CAT_STATION,  ///< Error messages related to building / maintaining stations.
		ERR_CAT_BRIDGE,   ///< Error messages related to building / removing bridges.
		ERR_CAT_TUNNEL,   ///< Error messages related to building / removing tunnels.
		ERR_CAT_TILE,     ///< Error messages related to raising / lowering and demolishing tiles.
		ERR_CAT_SIGN,     ///< Error messages related to building / removing signs.
		ERR_CAT_RAIL,     ///< Error messages related to building / maintaining rails.
		ERR_CAT_ROAD,     ///< Error messages related to building / maintaining roads.
		ERR_CAT_ORDER,    ///< Error messages related to managing orders.
		ERR_CAT_MARINE,   ///< Error messages related to building / removing ships, docks and channels.
		ERR_CAT_WAYPOINT, ///< Error messages related to building / maintaining waypoints.

		/**
		 * DO NOT USE! The error bitsize determines how many errors can be stored in
		 *  a category and what the offsets are of all categories.
		 */
		ERR_CAT_BIT_SIZE = 8,
	};

	/**
	 * All general related error messages.
	 */
	enum ErrorMessages {
		/** Initial error value */
		ERR_NONE = ERR_CAT_NONE << ERR_CAT_BIT_SIZE,  // []
		/** If an error occurred and the error wasn't mapped */
		ERR_UNKNOWN,                                  // []
		/** If a precondition is not met */
		ERR_PRECONDITION_FAILED,                      // []
		/** A string supplied was too long */
		ERR_PRECONDITION_STRING_TOO_LONG,             // []
		/** An error returned by a NewGRF. No possibility to get the exact error in an AI readable format */
		ERR_NEWGRF_SUPPLIED_ERROR,                    // []

		/** Base for general errors */
		ERR_GENERAL_BASE = ERR_CAT_GENERAL << ERR_CAT_BIT_SIZE,

		/** Not enough cash to perform the previous action */
		ERR_NOT_ENOUGH_CASH,                          // [STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY]

		/** Local authority won't allow the previous action */
		ERR_LOCAL_AUTHORITY_REFUSES,                  // [STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS]

		/** The piece of infrastructure you tried to build is already in place */
		ERR_ALREADY_BUILT,                            // [STR_ERROR_ALREADY_BUILT, STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST]

		/** Area isn't clear, try to demolish the building on it */
		ERR_AREA_NOT_CLEAR,                           // [STR_ERROR_BUILDING_MUST_BE_DEMOLISHED, STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST, STR_ERROR_MUST_DEMOLISH_RAILROAD, STR_ERROR_MUST_DEMOLISH_AIRPORT_FIRST, STR_ERROR_MUST_DEMOLISH_CARGO_TRAM_STATION_FIRST, STR_ERROR_MUST_DEMOLISH_TRUCK_STATION_FIRST, STR_ERROR_MUST_DEMOLISH_PASSENGER_TRAM_STATION_FIRST, STR_ERROR_MUST_DEMOLISH_BUS_STATION_FIRST, STR_ERROR_BUOY_IN_THE_WAY, STR_ERROR_MUST_DEMOLISH_DOCK_FIRST, STR_ERROR_GENERIC_OBJECT_IN_THE_WAY, STR_ERROR_COMPANY_HEADQUARTERS_IN, STR_ERROR_OBJECT_IN_THE_WAY, STR_ERROR_MUST_REMOVE_ROAD_FIRST, STR_ERROR_MUST_REMOVE_RAILROAD_TRACK, STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST, STR_ERROR_MUST_DEMOLISH_TUNNEL_FIRST, STR_ERROR_EXCAVATION_WOULD_DAMAGE]

		/** Area / property is owned by another company */
		ERR_OWNED_BY_ANOTHER_COMPANY,                 // [STR_ERROR_AREA_IS_OWNED_BY_ANOTHER, STR_ERROR_OWNED_BY]

		/** The name given is not unique for the object type */
		ERR_NAME_IS_NOT_UNIQUE,                       // [STR_ERROR_NAME_MUST_BE_UNIQUE]

		/** The building you want to build requires flat land */
		ERR_FLAT_LAND_REQUIRED,                       // [STR_ERROR_FLAT_LAND_REQUIRED]

		/** Land is sloped in the wrong direction for this build action */
		ERR_LAND_SLOPED_WRONG,                        // [STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION]

		/** A vehicle is in the way */
		ERR_VEHICLE_IN_THE_WAY,                       // [STR_ERROR_TRAIN_IN_THE_WAY, STR_ERROR_ROAD_VEHICLE_IN_THE_WAY, STR_ERROR_SHIP_IN_THE_WAY, STR_ERROR_AIRCRAFT_IN_THE_WAY]

		/** Site is unsuitable */
		ERR_SITE_UNSUITABLE,                          // [STR_ERROR_SITE_UNSUITABLE]

		/** Too close to the edge of the map */
		ERR_TOO_CLOSE_TO_EDGE,                        // [STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP]

		/** Station is too spread out */
		ERR_STATION_TOO_SPREAD_OUT,                   // [STR_ERROR_STATION_TOO_SPREAD_OUT]
	};

	/**
	 * Check the membership of the last thrown error.
	 * @return The category the error belongs to.
	 * @note The last throw error can be aquired by calling GetLastError().
	 */
	static ErrorCategories GetErrorCategory();

	/**
	 * Get the last error.
	 * @return An ErrorMessages enum value.
	 */
	static AIErrorType GetLastError();

	/**
	 * Get the last error in string format (for human readability).
	 * @return An ErrorMessage enum item, as string.
	 */
	static char *GetLastErrorString();

#ifndef EXPORT_SKIP
	/**
	 * Get the error based on the OpenTTD StringID.
	 * @note DO NOT INVOKE THIS METHOD YOURSELF!
	 * @param internal_string_id The string to convert.
	 * @return The NoAI equivalent error message.
	 */
	static AIErrorType StringToError(StringID internal_string_id);

	/**
	 * Map an internal OpenTTD error message to its NoAI equivalent.
	 * @note DO NOT INVOKE THIS METHOD YOURSELF! The calls are autogenerated.
	 * @param internal_string_id The OpenTTD StringID used for an error.
	 * @param ai_error_msg The NoAI equivalent error message.
	 */
	static void RegisterErrorMap(StringID internal_string_id, AIErrorType ai_error_msg);

	/**
	 * Map an internal OpenTTD error message to its NoAI equivalent.
	 * @note DO NOT INVOKE THIS METHOD YOURSELF! The calls are autogenerated.
	 * @param ai_error_msg The NoAI error message representation.
	 * @param message The string representation of this error message, used for debug purposes.
	 */
	static void RegisterErrorMapString(AIErrorType ai_error_msg, const char *message);
#endif /* EXPORT_SKIP */

private:
	typedef std::map<StringID, AIErrorType> AIErrorMap;           ///< The type for mapping between error (internal OpenTTD) StringID to the AI error type.
	typedef std::map<AIErrorType, const char *> AIErrorMapString; ///< The type for mapping between error type and textual representation.

	static AIErrorMap error_map;              ///< The mapping between error (internal OpenTTD) StringID to the AI error type.
	static AIErrorMapString error_map_string; ///< The mapping between error type and textual representation.
};

#endif /* AI_ERROR_HPP */
