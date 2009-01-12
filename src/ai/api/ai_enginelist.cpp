/* $Id$ */

/** @file ai_enginelist.cpp Implementation of AIEngineList and friends. */

#include "ai_enginelist.hpp"
#include "../../company_func.h"
#include "../../engine_base.h"
#include "../../core/bitmath_func.hpp"

AIEngineList::AIEngineList(AIVehicle::VehicleType vehicle_type)
{
	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, (::VehicleType)vehicle_type) {
		if (HasBit(e->company_avail, _current_company)) this->AddItem(e->index);
	}
}
