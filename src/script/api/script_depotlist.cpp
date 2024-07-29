/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_depotlist.cpp Implementation of ScriptDepotList and friends. */

#include "../../stdafx.h"
#include "script_depotlist.hpp"
#include "../../depot_base.h"
#include "../../station_base.h"

#include "../../safeguards.h"

ScriptDepotList::ScriptDepotList(ScriptTile::TransportType transport_type)
{
	EnforceDeityOrCompanyModeValid_Void();
	static_assert(VEH_TRAIN == (int)ScriptTile::TRANSPORT_RAIL);
	static_assert(VEH_ROAD  == (int)ScriptTile::TRANSPORT_ROAD);
	static_assert(VEH_SHIP  == (int)ScriptTile::TRANSPORT_WATER);

	bool is_deity = ScriptCompanyMode::IsDeity();
	CompanyID owner = ScriptObject::GetCompany();
	for (const Depot *depot : Depot::Iterate()) {
		if (depot->veh_type != (VehicleType)transport_type ||
			(!is_deity && ::GetTileOwner(depot->xy) != owner)) continue;

		this->AddItem(depot->xy.base());
	}
}
