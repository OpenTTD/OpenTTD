/* $Id$ */

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
	static const char *GetClassName() { return "AIError"; }

	/**
	 * All categories errors can be divided in.
	 */
	enum ErrorCategories {
		ERR_CAT_NONE = 0, //!< Error messages not related to any category.
		ERR_CAT_GENERAL,  //!< Error messages related to general things.
		ERR_CAT_VEHICLE,  //!< Error messages related to building / maintaining vehicles.
		ERR_CAT_STATION,  //!< Error messages related to building / maintaining stations.
		ERR_CAT_BRIDGE,   //!< Error messages related to building / removing bridges.
		ERR_CAT_TUNNEL,   //!< Error messages related to building / removing tunnels.
		ERR_CAT_TILE,     //!< Error messages related to raising / lowering and demolishing tiles.
		ERR_CAT_SIGN,     //!< Error messages related to building / removing signs.
		ERR_CAT_RAIL,     //!< Error messages related to building / maintaining rails.
		ERR_CAT_ROAD,     //!< Error messages related to building / maintaining roads.
		ERR_CAT_ORDER,    //!< Error messages related to managing orders.
		ERR_CAT_MARINE,   //!< Error messages related to building / removing ships, docks and channels.

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
		/** If an error occured and the error wasn't mapped */
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
		ERR_NOT_ENOUGH_CASH,                          // [STR_0003_NOT_ENOUGH_CASH_REQUIRES]

		/** Local authority won't allow the previous action */
		ERR_LOCAL_AUTHORITY_REFUSES,                  // [STR_2009_LOCAL_AUTHORITY_REFUSES]

		/** The piece of infrastructure you tried to build is already in place */
		ERR_ALREADY_BUILT,                            // [STR_1007_ALREADY_BUILT, STR_5007_MUST_DEMOLISH_BRIDGE_FIRST]

		/** Area isn't clear, try to demolish the building on it */
		ERR_AREA_NOT_CLEAR,                           // [STR_2004_BUILDING_MUST_BE_DEMOLISHED, STR_5007_MUST_DEMOLISH_BRIDGE_FIRST, STR_300B_MUST_DEMOLISH_RAILROAD, STR_300E_MUST_DEMOLISH_AIRPORT_FIRST, STR_MUST_DEMOLISH_CARGO_TRAM_STATION, STR_3047_MUST_DEMOLISH_TRUCK_STATION, STR_MUST_DEMOLISH_PASSENGER_TRAM_STATION, STR_3046_MUST_DEMOLISH_BUS_STATION, STR_306A_BUOY_IN_THE_WAY, STR_304D_MUST_DEMOLISH_DOCK_FIRST, STR_4800_IN_THE_WAY, STR_5804_COMPANY_HEADQUARTERS_IN, STR_5800_OBJECT_IN_THE_WAY, STR_1801_MUST_REMOVE_ROAD_FIRST, STR_1008_MUST_REMOVE_RAILROAD_TRACK, STR_5007_MUST_DEMOLISH_BRIDGE_FIRST, STR_5006_MUST_DEMOLISH_TUNNEL_FIRST, STR_1002_EXCAVATION_WOULD_DAMAGE]

		/** Area / property is owned by another company */
		ERR_OWNED_BY_ANOTHER_COMPANY,                 // [STR_1024_AREA_IS_OWNED_BY_ANOTHER, STR_013B_OWNED_BY]

		/** The name given is not unique for the object type */
		ERR_NAME_IS_NOT_UNIQUE,                       // [STR_NAME_MUST_BE_UNIQUE]

		/** The building you want to build requires flat land */
		ERR_FLAT_LAND_REQUIRED,                       // [STR_0007_FLAT_LAND_REQUIRED]

		/** Land is sloped in the wrong direction for this build action */
		ERR_LAND_SLOPED_WRONG,                        // [STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION]

		/** A vehicle is in the way */
		ERR_VEHICLE_IN_THE_WAY,                       // [STR_8803_TRAIN_IN_THE_WAY, STR_9000_ROAD_VEHICLE_IN_THE_WAY, STR_980E_SHIP_IN_THE_WAY, STR_A015_AIRCRAFT_IN_THE_WAY]

		/** Site is unsuitable */
		ERR_SITE_UNSUITABLE,                          // [STR_0239_SITE_UNSUITABLE, STR_304B_SITE_UNSUITABLE]

		/** Too close to the edge of the map */
		ERR_TOO_CLOSE_TO_EDGE,                        // [STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP]

		/** Station is too spread out */
		ERR_STATION_TOO_SPREAD_OUT,                   // [STR_306C_STATION_TOO_SPREAD_OUT]
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
	 * Map an internal OpenTTD error message to it's NoAI equivalent.
	 * @note DO NOT INVOKE THIS METHOD YOURSELF! The calls are autogenerated.
	 * @param internal_string_id The OpenTTD StringID used for an error.
	 * @param ai_error_msg The NoAI equivalent error message.
	 */
	static void RegisterErrorMap(StringID internal_string_id, AIErrorType ai_error_msg);

	/**
	 * Map an internal OpenTTD error message to it's NoAI equivalent.
	 * @note DO NOT INVOKE THIS METHOD YOURSELF! The calls are autogenerated.
	 * @param ai_error_msg The NoAI error message representation.
	 * @param message The string representation of this error message, used for debug purposes.
	 */
	static void RegisterErrorMapString(AIErrorType ai_error_msg, const char *message);
#endif /* EXPORT_SKIP */

private:
	typedef std::map<StringID, AIErrorType> AIErrorMap;
	typedef std::map<AIErrorType, const char *> AIErrorMapString;

	static AIErrorMap error_map;
	static AIErrorMapString error_map_string;
};

#endif /* AI_ERROR_HPP */
