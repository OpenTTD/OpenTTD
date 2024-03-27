/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_error.hpp Everything to query errors. */

#ifndef SCRIPT_ERROR_HPP
#define SCRIPT_ERROR_HPP

#include "script_object.hpp"
#include "script_companymode.hpp"

/**
 * Helper to write precondition enforcers for the script API in an abbreviated manner.
 * @param returnval The value to return on failure.
 * @param condition The condition that must be obeyed.
 */
#define EnforcePrecondition(returnval, condition)               \
	if (!(condition)) {                                           \
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_FAILED);   \
		return returnval;                                           \
	}

/**
 * Helper to write precondition enforcers for the script API in an abbreviated manner.
 * @param returnval The value to return on failure.
 * @param condition The condition that must be obeyed.
 * @param error_code The error code passed to ScriptObject::SetLastError.
 */
#define EnforcePreconditionCustomError(returnval, condition, error_code)   \
	if (!(condition)) {                                                      \
		ScriptObject::SetLastError(error_code);                                    \
		return returnval;                                                      \
	}

/**
 * Helper to write precondition enforcers for the script API in an abbreviated manner for encoded texts.
 * @param returnval The value to return on failure.
 * @param string The string that is checked.
 */
#define EnforcePreconditionEncodedText(returnval, string)   \
	if (string.empty()) { \
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_FAILED); \
		return returnval; \
	}

/**
 * Helper to enforce the precondition that the company mode is valid.
 * @param returnval The value to return on failure.
 */
#define EnforceCompanyModeValid(returnval) \
	EnforcePreconditionCustomError(returnval, ScriptCompanyMode::IsValid(), ScriptError::ERR_PRECONDITION_INVALID_COMPANY)

/**
 * Helper to enforce the precondition that the company mode is valid.
 */
#define EnforceCompanyModeValid_Void() \
	if (!ScriptCompanyMode::IsValid()) { \
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_INVALID_COMPANY); \
		return; \
	}

/**
 * Helper to enforce the precondition that we are in a deity mode.
 * @param returnval The value to return on failure.
 */
#define EnforceDeityMode(returnval) \
	EnforcePreconditionCustomError(returnval, ScriptCompanyMode::IsDeity(), ScriptError::ERR_PRECONDITION_INVALID_COMPANY)

/**
 * Helper to enforce the precondition that the company mode is valid or that we are a deity.
 * @param returnval The value to return on failure.
 */
#define EnforceDeityOrCompanyModeValid(returnval) \
	EnforcePreconditionCustomError(returnval, ScriptCompanyMode::IsDeity() || ScriptCompanyMode::IsValid(), ScriptError::ERR_PRECONDITION_INVALID_COMPANY)

/**
 * Helper to enforce the precondition that the company mode is valid or that we are a deity.
 */
#define EnforceDeityOrCompanyModeValid_Void() \
	if (!(ScriptCompanyMode::IsDeity() || ScriptCompanyMode::IsValid())) { \
		ScriptObject::SetLastError(ScriptError::ERR_PRECONDITION_INVALID_COMPANY); \
		return; \
	}


/**
 * Class that handles all error related functions.
 * @api ai game
 */
class ScriptError : public ScriptObject {
public:
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
	 *
	 * @see ScriptErrorType
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
		/** The company you use is invalid */
		ERR_PRECONDITION_INVALID_COMPANY,             // []
		/** An error returned by a NewGRF. No possibility to get the exact error in an script readable format */
		ERR_NEWGRF_SUPPLIED_ERROR,                    // []

		/** Base for general errors */
		ERR_GENERAL_BASE = ERR_CAT_GENERAL << ERR_CAT_BIT_SIZE,

		/** Not enough cash to perform the previous action */
		ERR_NOT_ENOUGH_CASH,                          // [STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY]

		/** Local authority won't allow the previous action */
		ERR_LOCAL_AUTHORITY_REFUSES,                  // [STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS, STR_ERROR_LOCAL_AUTHORITY_REFUSES_NOISE]

		/** The piece of infrastructure you tried to build is already in place */
		ERR_ALREADY_BUILT,                            // [STR_ERROR_ALREADY_BUILT, STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST, STR_ERROR_TREE_ALREADY_HERE]

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
		ERR_SITE_UNSUITABLE,                          // [STR_ERROR_SITE_UNSUITABLE, STR_ERROR_TREE_WRONG_TERRAIN_FOR_TREE_TYPE]

		/** Too close to the edge of the map */
		ERR_TOO_CLOSE_TO_EDGE,                        // [STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP]

		/** Station is too spread out */
		ERR_STATION_TOO_SPREAD_OUT,                   // [STR_ERROR_STATION_TOO_SPREAD_OUT]
	};

	/**
	 * Check the membership of the last thrown error.
	 * @return The category the error belongs to.
	 * @note The last throw error can be acquired by calling GetLastError().
	 */
	static ErrorCategories GetErrorCategory();

	/**
	 * Get the last error.
	 * @return An ErrorMessages enum value.
	 */
	static ScriptErrorType GetLastError();

	/**
	 * Get the last error in string format (for human readability).
	 * @return An ErrorMessage enum item, as string.
	 */
	static std::optional<std::string> GetLastErrorString();

	/**
	 * Get the error based on the OpenTTD StringID.
	 * @api -all
	 * @param internal_string_id The string to convert.
	 * @return The script equivalent error message.
	 */
	static ScriptErrorType StringToError(StringID internal_string_id);

	/**
	 * Map an internal OpenTTD error message to its script equivalent.
	 * @api -all
	 * @param internal_string_id The OpenTTD StringID used for an error.
	 * @param ai_error_msg The script equivalent error message.
	 */
	static void RegisterErrorMap(StringID internal_string_id, ScriptErrorType ai_error_msg);

	/**
	 * Map an internal OpenTTD error message to its script equivalent.
	 * @api -all
	 * @param ai_error_msg The script error message representation.
	 * @param message The string representation of this error message, used for debug purposes.
	 */
	static void RegisterErrorMapString(ScriptErrorType ai_error_msg, const char *message);

private:
	typedef std::map<StringID, ScriptErrorType> ScriptErrorMap;           ///< The type for mapping between error (internal OpenTTD) StringID to the script error type.
	typedef std::map<ScriptErrorType, const char *> ScriptErrorMapString; ///< The type for mapping between error type and textual representation.

	static ScriptErrorMap error_map;              ///< The mapping between error (internal OpenTTD) StringID to the script error type.
	static ScriptErrorMapString error_map_string; ///< The mapping between error type and textual representation.
};

#endif /* SCRIPT_ERROR_HPP */
