/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_admin.cpp Implementation of ScriptAdmin. */

#include "../../stdafx.h"
#include "script_admin.hpp"
#include "script_log.hpp"
#include "../../network/network_admin.h"
#include "../script_instance.hpp"
#include "../../string_func.h"
#include "../../3rdparty/nlohmann/json.hpp"

#include "../../safeguards.h"

/**
 * Convert a Squirrel structure into a JSON object.
 *
 * This function is not "static", so it can be tested in unittests.
 *
 * @param json The resulting JSON object.
 * @param vm The VM to operate on.
 * @param index The index we are currently working for.
 * @param depth The current depth in the squirrel struct.
 * @return True iff the conversion was successful.
 */
bool ScriptAdminMakeJSON(nlohmann::json &json, HSQUIRRELVM vm, SQInteger index, int depth = 0)
{
	if (depth == SQUIRREL_MAX_DEPTH) {
		ScriptLog::Error("Send parameters can only be nested to 25 deep. No data sent."); // SQUIRREL_MAX_DEPTH = 25
		return false;
	}

	switch (sq_gettype(vm, index)) {
		case OT_INTEGER: {
			SQInteger res;
			sq_getinteger(vm, index, &res);

			json = res;
			return true;
		}

		case OT_STRING: {
			const SQChar *buf;
			sq_getstring(vm, index, &buf);

			json = std::string(buf);
			return true;
		}

		case OT_ARRAY: {
			json = nlohmann::json::array();

			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, index - 1))) {
				nlohmann::json tmp;

				bool res = ScriptAdminMakeJSON(tmp, vm, -1, depth + 1);
				sq_pop(vm, 2);
				if (!res) {
					sq_pop(vm, 1);
					return false;
				}

				json.push_back(tmp);
			}
			sq_pop(vm, 1);
			return true;
		}

		case OT_TABLE: {
			json = nlohmann::json::object();

			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, index - 1))) {
				/* Squirrel ensure the key is a string. */
				assert(sq_gettype(vm, -2) == OT_STRING);
				const SQChar *buf;
				sq_getstring(vm, -2, &buf);
				std::string key = std::string(buf);

				nlohmann::json value;
				bool res = ScriptAdminMakeJSON(value, vm, -1, depth + 1);
				sq_pop(vm, 2);
				if (!res) {
					sq_pop(vm, 1);
					return false;
				}

				json[key] = value;
			}
			sq_pop(vm, 1);
			return true;
		}

		case OT_BOOL: {
			SQBool res;
			sq_getbool(vm, index, &res);

			json = res ? true : false;
			return true;
		}

		case OT_NULL: {
			json = nullptr;
			return true;
		}

		default:
			ScriptLog::Error("You tried to send an unsupported type. No data sent.");
			return false;
	}
}

/* static */ SQInteger ScriptAdmin::Send(HSQUIRRELVM vm)
{
	if (sq_gettop(vm) - 1 != 1) return sq_throwerror(vm, "wrong number of parameters");

	if (sq_gettype(vm, 2) != OT_TABLE) {
		return sq_throwerror(vm, "ScriptAdmin::Send requires a table as first parameter. No data sent.");
	}

	nlohmann::json json;
	if (!ScriptAdminMakeJSON(json, vm, -1)) {
		sq_pushinteger(vm, 0);
		return 1;
	}

	NetworkAdminGameScript(json.dump());

	sq_pushinteger(vm, 1);
	return 1;
}
