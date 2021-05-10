/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargomonitor.cpp Code to monitor cargo pickup and deliveries by companies. */

#include "../../stdafx.h"
#include "script_cargo.hpp"
#include "script_cargomonitor.hpp"
#include "../../town.h"
#include "../../industry.h"

#include "../../safeguards.h"

/* static */ int32 ScriptCargoMonitor::GetTownDeliveryAmount(ScriptCompany::CompanyID company, CargoID cargo, TownID town_id, bool keep_monitoring)
{
	CompanyID cid = static_cast<CompanyID>(company);
	if (cid >= MAX_COMPANIES) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;
	if (!::Town::IsValidID(town_id)) return -1;

	CargoMonitorID monitor = EncodeCargoTownMonitor(cid, cargo, town_id);
	return GetDeliveryAmount(monitor, keep_monitoring);
}

/* static */ int32 ScriptCargoMonitor::GetIndustryDeliveryAmount(ScriptCompany::CompanyID company, CargoID cargo, IndustryID industry_id, bool keep_monitoring)
{
	CompanyID cid = static_cast<CompanyID>(company);
	if (cid >= MAX_COMPANIES) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;
	if (!::Industry::IsValidID(industry_id)) return -1;

	CargoMonitorID monitor = EncodeCargoIndustryMonitor(cid, cargo, industry_id);
	return GetDeliveryAmount(monitor, keep_monitoring);
}

/* static */ int32 ScriptCargoMonitor::GetTownPickupAmount(ScriptCompany::CompanyID company, CargoID cargo, TownID town_id, bool keep_monitoring)
{
	CompanyID cid = static_cast<CompanyID>(company);
	if (cid >= MAX_COMPANIES) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;
	if (!::Town::IsValidID(town_id)) return -1;

	CargoMonitorID monitor = EncodeCargoTownMonitor(cid, cargo, town_id);
	return GetPickupAmount(monitor, keep_monitoring);
}

/* static */ int32 ScriptCargoMonitor::GetIndustryPickupAmount(ScriptCompany::CompanyID company, CargoID cargo, IndustryID industry_id, bool keep_monitoring)
{
	CompanyID cid = static_cast<CompanyID>(company);
	if (cid >= MAX_COMPANIES) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;
	if (!::Industry::IsValidID(industry_id)) return -1;

	CargoMonitorID monitor = EncodeCargoIndustryMonitor(cid, cargo, industry_id);
	return GetPickupAmount(monitor, keep_monitoring);
}

/* static */ void ScriptCargoMonitor::StopAllMonitoring()
{
	ClearCargoPickupMonitoring();
	ClearCargoDeliveryMonitoring();
}

