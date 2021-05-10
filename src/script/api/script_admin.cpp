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

#include "../../safeguards.h"

/* static */ bool ScriptAdmin::MakeJSON(HSQUIRRELVM vm, SQInteger index, int max_depth, std::string &data)
{
	if (max_depth == 0) {
		ScriptLog::Error("Send parameters can only be nested to 25 deep. No data sent."); // SQUIRREL_MAX_DEPTH = 25
		return false;
	}

	switch (sq_gettype(vm, index)) {
		case OT_INTEGER: {
			SQInteger res;
			sq_getinteger(vm, index, &res);

			char buf[10];
			seprintf(buf, lastof(buf), "%d", (int32)res);
			data = buf;
			return true;
		}

		case OT_STRING: {
			const SQChar *buf;
			sq_getstring(vm, index, &buf);

			size_t len = strlen(buf) + 1;
			if (len >= 255) {
				ScriptLog::Error("Maximum string length is 254 chars. No data sent.");
				return false;
			}

			data = std::string("\"") + buf + "\"";
			return true;
		}

		case OT_ARRAY: {
			data = "[ ";

			bool first = true;
			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, index - 1))) {
				if (!first) data += ", ";
				if (first) first = false;

				std::string tmp;

				bool res = MakeJSON(vm, -1, max_depth - 1, tmp);
				sq_pop(vm, 2);
				if (!res) {
					sq_pop(vm, 1);
					return false;
				}
				data += tmp;
			}
			sq_pop(vm, 1);
			data += " ]";
			return true;
		}

		case OT_TABLE: {
			data = "{ ";

			bool first = true;
			sq_pushnull(vm);
			while (SQ_SUCCEEDED(sq_next(vm, index - 1))) {
				if (!first) data += ", ";
				if (first) first = false;

				std::string key;
				std::string value;

				/* Store the key + value */
				bool res = MakeJSON(vm, -2, max_depth - 1, key) && MakeJSON(vm, -1, max_depth - 1, value);
				sq_pop(vm, 2);
				if (!res) {
					sq_pop(vm, 1);
					return false;
				}
				data += key + ": " + value;
			}
			sq_pop(vm, 1);
			data += " }";
			return true;
		}

		case OT_BOOL: {
			SQBool res;
			sq_getbool(vm, index, &res);

			if (res) {
				data = "true";
				return true;
			}

			data = "false";
			return true;
		}

		case OT_NULL: {
			data = "null";
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

	std::string json;
	ScriptAdmin::MakeJSON(vm, -1, SQUIRREL_MAX_DEPTH, json);

	if (json.length() > NETWORK_GAMESCRIPT_JSON_LENGTH) {
		ScriptLog::Error("You are trying to send a table that is too large to the AdminPort. No data sent.");
		sq_pushinteger(vm, 0);
		return 1;
	}

	NetworkAdminGameScript(json.c_str());

	sq_pushinteger(vm, 1);
	return 1;
}
