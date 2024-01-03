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
#include "../../3rdparty/nlohmann/json.hpp"
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

	auto it = std::max_element(std::cbegin(cap), std::cend(cap));
	if (*it == 0) return CT_INVALID;

	return CargoID(std::distance(std::cbegin(cap), it));
}

int32_t ScriptEventEnginePreview::GetCapacity()
{
	if (!this->IsEngineValid()) return -1;
	const Engine *e = ::Engine::Get(this->engine);
	switch (e->type) {
		case VEH_ROAD:
		case VEH_TRAIN: {
			CargoArray capacities = GetCapacityOfArticulatedParts(this->engine);
			for (uint &cap : capacities) {
				if (cap != 0) return cap;
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
/**
 * Convert a JSON part fo Squirrel.
 * @param vm The VM used.
 * @param json The JSON part to convert to Squirrel.
 */
static bool ScriptEventAdminPortReadValue(HSQUIRRELVM vm, nlohmann::json &json)
{
	switch (json.type()) {
		case nlohmann::json::value_t::null:
			sq_pushnull(vm);
			break;

		case nlohmann::json::value_t::boolean:
			sq_pushbool(vm, json.get<bool>() ? 1 : 0);
			break;

		case nlohmann::json::value_t::string: {
			auto value = json.get<std::string>();
			sq_pushstring(vm, value.data(), value.size());
			break;
		}

		case nlohmann::json::value_t::number_integer:
		case nlohmann::json::value_t::number_unsigned:
			sq_pushinteger(vm, json.get<int64_t>());
			break;

		case nlohmann::json::value_t::object:
			sq_newtable(vm);

			for (auto &[key, value] : json.items()) {
				sq_pushstring(vm, key.data(), key.size());

				if (!ScriptEventAdminPortReadValue(vm, value)) {
					return false;
				}

				sq_rawset(vm, -3);
			}
			break;

		case nlohmann::json::value_t::array:
			sq_newarray(vm, 0);

			for (auto &value : json) {
				if (!ScriptEventAdminPortReadValue(vm, value)) {
					return false;
				}

				sq_arrayappend(vm, -2);
			}
			break;

		/* These types are not supported by Squirrel. */
		case nlohmann::json::value_t::number_float:
		default:
			return false;
	}

	return true;
}

SQInteger ScriptEventAdminPort::GetObject(HSQUIRRELVM vm)
{
	auto json = nlohmann::json::parse(this->json, nullptr, false);

	if (!json.is_object()) {
		ScriptLog::Error("The root element in the JSON data from AdminPort has to be an object.");

		sq_pushnull(vm);
		return 1;
	}

	auto top = sq_gettop(vm);
	if (!ScriptEventAdminPortReadValue(vm, json)) {
		/* Rewind the stack, removing anything that might be left on top. */
		sq_settop(vm, top);

		ScriptLog::Error("Received invalid JSON data from AdminPort.");

		sq_pushnull(vm);
		return 1;
	}

	return 1;
}
