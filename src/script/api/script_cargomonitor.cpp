/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_cargomonitor.cpp Code to monitor cargo pickup and deliveries by companies. */

#include "../../stdafx.h"
#include "script_cargomonitor.hpp"

/* static */ uint32 ScriptCargoMonitor::GetTownDeliveryAmount(ScriptCompany::CompanyID company, CargoID cargo, TownID town_id, bool keep_monitoring)
{
	CargoMonitorID monitor = EncodeCargoTownMonitor(static_cast<CompanyID>(company), cargo, town_id);
	return GetDeliveryAmount(monitor, keep_monitoring);
}

/* static */ uint32 ScriptCargoMonitor::GetIndustryDeliveryAmount(ScriptCompany::CompanyID company, CargoID cargo, IndustryID industry_id, bool keep_monitoring)
{
	CargoMonitorID monitor = EncodeCargoIndustryMonitor(static_cast<CompanyID>(company), cargo, industry_id);
	return GetDeliveryAmount(monitor, keep_monitoring);
}

/* static */ uint32 ScriptCargoMonitor::GetTownPickupAmount(ScriptCompany::CompanyID company, CargoID cargo, TownID town_id, bool keep_monitoring)
{
	CargoMonitorID monitor = EncodeCargoTownMonitor(static_cast<CompanyID>(company), cargo, town_id);
	return GetPickupAmount(monitor, keep_monitoring);
}

/* static */ uint32 ScriptCargoMonitor::GetIndustryPickupAmount(ScriptCompany::CompanyID company, CargoID cargo, IndustryID industry_id, bool keep_monitoring)
{
	CargoMonitorID monitor = EncodeCargoIndustryMonitor(static_cast<CompanyID>(company), cargo, industry_id);
	return GetPickupAmount(monitor, keep_monitoring);
}

/* static */ void ScriptCargoMonitor::StopAllMonitoring()
{
	ClearCargoPickupMonitoring();
	ClearCargoDeliveryMonitoring();
}

