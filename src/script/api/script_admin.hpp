/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_admin.hpp Everything to communicate with the AdminPort. */

#ifndef SCRIPT_ADMIN_HPP
#define SCRIPT_ADMIN_HPP

#include "script_object.hpp"
#include <nlohmann/json.hpp>

/**
 * Class that handles communication with the AdminPort.
 * @api game
 */
class ScriptAdmin : public ScriptObject {
public:
#ifndef DOXYGEN_API
	/**
	 * Internal representation of the Send function.
	 */
	static SQInteger Send(HSQUIRRELVM vm);
#else
	/**
	 * Send information to the AdminPort. The information can be anything
	 *  as long as it isn't a class or instance thereof.
	 * @param table The information to send, in a table. For example: { param = "param" }.
	 * @return True if and only if the data was successfully converted to JSON
	 *  and send to the AdminPort.
	 * @note If the resulting JSON of your table is larger than 1450 bytes,
	 *   nothing will be sent (and false will be returned).
	 */
	static bool Send(void *table);
#endif /* DOXYGEN_API */

protected:
	/**
	 * Convert a Squirrel structure into a JSON object.
	 * @param json The resulting JSON object.
	 * @param vm The VM to operate on.
	 * @param index The index we are currently working for.
	 * @param depth The current depth in the squirrel struct.
	 * @return True iff the conversion was successful.
	 */
	static bool MakeJSON(nlohmann::json &data, HSQUIRRELVM vm, SQInteger index, int depth = 0);
};

#endif /* SCRIPT_ADMIN_HPP */
