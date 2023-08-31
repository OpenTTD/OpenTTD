/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_event_types.cpp Implementation of all EventTypes. */

#include "../../stdafx.h"
#include "script_event_types.hpp"
#include "script_vehicle.hpp"
#include "script_log.hpp"
#include "../../strings_func.h"
#include "../../settings_type.h"
#include "../../engine_base.h"
#include "../../articulated_vehicles.h"
#include "../../string_func.h"
#include "../../economy_cmd.h"
#include "../../engine_cmd.h"
#include "table/strings.h"

#include "../../safeguards.h"

bool ScriptEventEnginePreview::IsEngineValid() const
{
	const Engine *e = ::Engine::GetIfValid(this->engine);
	return e != nullptr && e->IsEnabled();
}

std::optional<std::string> ScriptEventEnginePreview::GetName()
{
	if (!this->IsEngineValid()) return std::nullopt;

	::SetDParam(0, this->engine);
	return GetString(STR_ENGINE_NAME);
}

CargoID ScriptEventEnginePreview::GetCargoType()
{
	if (!this->IsEngineValid()) return CT_INVALID;
	CargoArray cap = ::GetCapacityOfArticulatedParts(this->engine);

	CargoID most_cargo = CT_INVALID;
	uint amount = 0;
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		if (cap[cid] > amount) {
			amount = cap[cid];
			most_cargo = cid;
		}
	}

	return most_cargo;
}

int32_t ScriptEventEnginePreview::GetCapacity()
{
	if (!this->IsEngineValid()) return -1;
	const Engine *e = ::Engine::Get(this->engine);
	switch (e->type) {
		case VEH_ROAD:
		case VEH_TRAIN: {
			CargoArray capacities = GetCapacityOfArticulatedParts(this->engine);
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				if (capacities[c] == 0) continue;
				return capacities[c];
			}
			return -1;
		}

		case VEH_SHIP:
		case VEH_AIRCRAFT:
			return e->GetDisplayDefaultCapacity();

		default: NOT_REACHED();
	}
}

int32_t ScriptEventEnginePreview::GetMaxSpeed()
{
	if (!this->IsEngineValid()) return -1;
	const Engine *e = ::Engine::Get(this->engine);
	int32_t max_speed = e->GetDisplayMaxSpeed(); // km-ish/h
	if (e->type == VEH_AIRCRAFT) max_speed /= _settings_game.vehicle.plane_speed;
	return max_speed;
}

Money ScriptEventEnginePreview::GetPrice()
{
	if (!this->IsEngineValid()) return -1;
	return ::Engine::Get(this->engine)->GetCost();
}

Money ScriptEventEnginePreview::GetRunningCost()
{
	if (!this->IsEngineValid()) return -1;
	return ::Engine::Get(this->engine)->GetRunningCost();
}

int32_t ScriptEventEnginePreview::GetVehicleType()
{
	if (!this->IsEngineValid()) return ScriptVehicle::VT_INVALID;
	switch (::Engine::Get(this->engine)->type) {
		case VEH_ROAD:     return ScriptVehicle::VT_ROAD;
		case VEH_TRAIN:    return ScriptVehicle::VT_RAIL;
		case VEH_SHIP:     return ScriptVehicle::VT_WATER;
		case VEH_AIRCRAFT: return ScriptVehicle::VT_AIR;
		default: NOT_REACHED();
	}
}

bool ScriptEventEnginePreview::AcceptPreview()
{
	EnforceCompanyModeValid(false);
	if (!this->IsEngineValid()) return false;
	return ScriptObject::Command<CMD_WANT_ENGINE_PREVIEW>::Do(this->engine);
}

bool ScriptEventCompanyAskMerger::AcceptMerger()
{
	EnforceCompanyModeValid(false);
	return ScriptObject::Command<CMD_BUY_COMPANY>::Do((::CompanyID)this->owner, false);
}

ScriptEventAdminPort::ScriptEventAdminPort(const std::string &json) :
		ScriptEvent(ET_ADMIN_PORT),
		json(json)
{
}

#define SKIP_EMPTY(p) while (*(p) == ' ' || *(p) == '\n' || *(p) == '\r') (p)++;
#define RETURN_ERROR(stack) { ScriptLog::Error("Received invalid JSON data from AdminPort."); if (stack != 0) sq_pop(vm, stack); return nullptr; }

SQInteger ScriptEventAdminPort::GetObject(HSQUIRRELVM vm)
{
	const char *p = this->json.c_str();

	if (this->ReadTable(vm, p) == nullptr) {
		sq_pushnull(vm);
		return 1;
	}

	return 1;
}

const char *ScriptEventAdminPort::ReadString(HSQUIRRELVM vm, const char *p)
{
	const char *value = p;

	bool escape = false;
	for (;;) {
		if (*p == '\\') {
			escape = true;
			p++;
			continue;
		}
		if (*p == '"' && escape) {
			escape = false;
			p++;
			continue;
		}
		escape = false;

		if (*p == '"') break;
		if (*p == '\0') RETURN_ERROR(0);

		p++;
	}

	size_t len = p - value;
	sq_pushstring(vm, value, len);
	p++; // Step past the end-of-string marker (")

	return p;
}

const char *ScriptEventAdminPort::ReadTable(HSQUIRRELVM vm, const char *p)
{
	sq_newtable(vm);

	SKIP_EMPTY(p);
	if (*p++ != '{') RETURN_ERROR(1);

	for (;;) {
		SKIP_EMPTY(p);
		if (*p++ != '"') RETURN_ERROR(1);

		p = ReadString(vm, p);
		if (p == nullptr) {
			sq_pop(vm, 1);
			return nullptr;
		}

		SKIP_EMPTY(p);
		if (*p++ != ':') RETURN_ERROR(2);

		p = this->ReadValue(vm, p);
		if (p == nullptr) {
			sq_pop(vm, 2);
			return nullptr;
		}

		sq_rawset(vm, -3);
		/* The key (-2) and value (-1) are popped from the stack by squirrel. */

		SKIP_EMPTY(p);
		if (*p == ',') {
			p++;
			continue;
		}
		break;
	}

	SKIP_EMPTY(p);
	if (*p++ != '}') RETURN_ERROR(1);

	return p;
}

const char *ScriptEventAdminPort::ReadValue(HSQUIRRELVM vm, const char *p)
{
	SKIP_EMPTY(p);

	if (strncmp(p, "false", 5) == 0) {
		sq_pushbool(vm, 0);
		return p + 5;
	}
	if (strncmp(p, "true", 4) == 0) {
		sq_pushbool(vm, 1);
		return p + 4;
	}
	if (strncmp(p, "null", 4) == 0) {
		sq_pushnull(vm);
		return p + 4;
	}

	switch (*p) {
		case '"': {
			/* String */
			p = ReadString(vm, ++p);
			if (p == nullptr) return nullptr;

			break;
		}

		case '{': {
			/* Table */
			p = this->ReadTable(vm, p);
			if (p == nullptr) return nullptr;

			break;
		}

		case '[': {
			/* Array */
			sq_newarray(vm, 0);

			/* Empty array? */
			const char *p2 = p + 1;
			SKIP_EMPTY(p2);
			if (*p2 == ']') {
				p = p2 + 1;
				break;
			}

			while (*p++ != ']') {
				p = this->ReadValue(vm, p);
				if (p == nullptr) {
					sq_pop(vm, 1);
					return nullptr;
				}
				sq_arrayappend(vm, -2);

				SKIP_EMPTY(p);
				if (*p == ',') continue;
				if (*p == ']') break;
				RETURN_ERROR(1);
			}

			p++;

			break;
		}

		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '0':
		case '-': {
			/* Integer */

			const char *value = p++;
			for (;;) {
				switch (*p++) {
					case '1': case '2': case '3': case '4': case '5':
					case '6': case '7': case '8': case '9': case '0':
						continue;

					default:
						break;
				}

				p--;
				break;
			}

			int res = atoi(value);
			sq_pushinteger(vm, (SQInteger)res);

			break;
		}

		default:
			RETURN_ERROR(0);
	}

	return p;
}

#undef SKIP_EMPTY
#undef RETURN_ERROR
