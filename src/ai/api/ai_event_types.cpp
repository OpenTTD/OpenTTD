/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_event_types.cpp Implementation of all EventTypes. */

#include "../../stdafx.h"
#include "ai_event_types.hpp"
#include "ai_vehicle.hpp"
#include "../../command_type.h"
#include "../../strings_func.h"
#include "../../settings_type.h"
#include "../../engine_base.h"
#include "../../articulated_vehicles.h"
#include "table/strings.h"

bool AIEventEnginePreview::IsEngineValid() const
{
	const Engine *e = ::Engine::GetIfValid(this->engine);
	return e != NULL && e->IsEnabled();
}

char *AIEventEnginePreview::GetName()
{
	if (!this->IsEngineValid()) return NULL;
	static const int len = 64;
	char *engine_name = MallocT<char>(len);

	::SetDParam(0, this->engine);
	::GetString(engine_name, STR_ENGINE_NAME, &engine_name[len - 1]);
	return engine_name;
}

CargoID AIEventEnginePreview::GetCargoType()
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

int32 AIEventEnginePreview::GetCapacity()
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

int32 AIEventEnginePreview::GetMaxSpeed()
{
	if (!this->IsEngineValid()) return -1;
	const Engine *e = ::Engine::Get(this->engine);
	int32 max_speed = e->GetDisplayMaxSpeed(); // km-ish/h
	if (e->type == VEH_AIRCRAFT) max_speed /= _settings_game.vehicle.plane_speed;
	return max_speed;
}

Money AIEventEnginePreview::GetPrice()
{
	if (!this->IsEngineValid()) return -1;
	return ::Engine::Get(this->engine)->GetCost();
}

Money AIEventEnginePreview::GetRunningCost()
{
	if (!this->IsEngineValid()) return -1;
	return ::Engine::Get(this->engine)->GetRunningCost();
}

int32 AIEventEnginePreview::GetVehicleType()
{
	if (!this->IsEngineValid()) return AIVehicle::VT_INVALID;
	switch (::Engine::Get(this->engine)->type) {
		case VEH_ROAD:     return AIVehicle::VT_ROAD;
		case VEH_TRAIN:    return AIVehicle::VT_RAIL;
		case VEH_SHIP:     return AIVehicle::VT_WATER;
		case VEH_AIRCRAFT: return AIVehicle::VT_AIR;
		default: NOT_REACHED();
	}
}

bool AIEventEnginePreview::AcceptPreview()
{
	if (!this->IsEngineValid()) return false;
	return AIObject::DoCommand(0, this->engine, 0, CMD_WANT_ENGINE_PREVIEW);
}

bool AIEventCompanyAskMerger::AcceptMerger()
{
	return AIObject::DoCommand(0, this->owner, 0, CMD_BUY_COMPANY);
}
