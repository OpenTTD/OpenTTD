/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_stationlist.cpp Implementation of ScriptStationList and friends. */

#include "../../stdafx.h"
#include "script_stationlist.hpp"
#include "script_vehicle.hpp"
#include "script_cargo.hpp"
#include "../../station_base.h"
#include "../../vehicle_base.h"

#include "../../safeguards.h"

ScriptStationList::ScriptStationList(ScriptStation::StationType station_type)
{
	EnforceDeityOrCompanyModeValid_Void();
	bool is_deity = ScriptCompanyMode::IsDeity();
	CompanyID owner = ScriptObject::GetCompany();
	for (Station *st : Station::Iterate()) {
		if ((is_deity || st->owner == owner) && (st->facilities & station_type) != 0) this->AddItem(st->index);
	}
}

ScriptStationList_Vehicle::ScriptStationList_Vehicle(VehicleID vehicle_id)
{
	if (!ScriptVehicle::IsPrimaryVehicle(vehicle_id)) return;

	Vehicle *v = ::Vehicle::Get(vehicle_id);

	for (Order *o = v->GetFirstOrder(); o != nullptr; o = o->next) {
		if (o->IsType(OT_GOTO_STATION)) this->AddItem(o->GetDestination());
	}
}

ScriptStationList_Cargo::ScriptStationList_Cargo(ScriptStationList_Cargo::CargoMode mode,
		ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoID cargo,
		StationID other_station)
{
	switch (mode) {
		case CM_WAITING:
			ScriptStationList_CargoWaiting(selector, station_id, cargo, other_station).SwapList(this);
			break;
		case CM_PLANNED:
			ScriptStationList_CargoPlanned(selector, station_id, cargo, other_station).SwapList(this);
			break;
		default:
			NOT_REACHED();
	}
}

ScriptStationList_CargoWaiting::ScriptStationList_CargoWaiting(
		ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoID cargo,
		StationID other_station)
{
	switch (selector) {
		case CS_BY_FROM:
			ScriptStationList_CargoWaitingByFrom(station_id, cargo).SwapList(this);
			break;
		case CS_VIA_BY_FROM:
			ScriptStationList_CargoWaitingViaByFrom(station_id, cargo, other_station).SwapList(this);
			break;
		case CS_BY_VIA:
			ScriptStationList_CargoWaitingByVia(station_id, cargo).SwapList(this);
			break;
		case CS_FROM_BY_VIA:
			ScriptStationList_CargoWaitingFromByVia(station_id, cargo, other_station).SwapList(this);
			break;
		default:
			NOT_REACHED();
	}
}

ScriptStationList_CargoPlanned::ScriptStationList_CargoPlanned(
		ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoID cargo,
		StationID other_station)
{
	switch (selector) {
		case CS_BY_FROM:
			ScriptStationList_CargoPlannedByFrom(station_id, cargo).SwapList(this);
			break;
		case CS_VIA_BY_FROM:
			ScriptStationList_CargoPlannedViaByFrom(station_id, cargo, other_station).SwapList(this);
			break;
		case CS_BY_VIA:
			ScriptStationList_CargoPlannedByVia(station_id, cargo).SwapList(this);
			break;
		case CS_FROM_BY_VIA:
			ScriptStationList_CargoPlannedFromByVia(station_id, cargo, other_station).SwapList(this);
			break;
		default:
			NOT_REACHED();
	}
}

class CargoCollector {
public:
	CargoCollector(ScriptStationList_Cargo *parent, StationID station_id, CargoID cargo,
			StationID other);
	~CargoCollector() ;

	template<ScriptStationList_Cargo::CargoSelector Tselector>
	void Update(StationID from, StationID via, uint amount);
	const GoodsEntry *GE() const { return ge; }

private:
	void SetValue();

	ScriptStationList_Cargo *list;
	const GoodsEntry *ge;
	StationID other_station;

	StationID last_key;
	uint amount;
};

CargoCollector::CargoCollector(ScriptStationList_Cargo *parent,
		StationID station_id, CargoID cargo, StationID other) :
	list(parent), ge(nullptr), other_station(other), last_key(INVALID_STATION), amount(0)
{
	if (!ScriptStation::IsValidStation(station_id)) return;
	if (!ScriptCargo::IsValidCargo(cargo)) return;
	this->ge = &(Station::Get(station_id)->goods[cargo]);
}

CargoCollector::~CargoCollector()
{
	this->SetValue();
}

void CargoCollector::SetValue()
{
	if (this->amount > 0) {
		if (this->list->HasItem(this->last_key)) {
			this->list->SetValue(this->last_key,
					this->list->GetValue(this->last_key) + this->amount);
		} else {
			this->list->AddItem(this->last_key, this->amount);
		}
	}
}

template<ScriptStationList_Cargo::CargoSelector Tselector>
void CargoCollector::Update(StationID from, StationID via, uint amount)
{
	StationID key = INVALID_STATION;
	switch (Tselector) {
		case ScriptStationList_Cargo::CS_VIA_BY_FROM:
			if (via != this->other_station) return;
			FALLTHROUGH;
		case ScriptStationList_Cargo::CS_BY_FROM:
			key = from;
			break;
		case ScriptStationList_Cargo::CS_FROM_BY_VIA:
			if (from != this->other_station) return;
			FALLTHROUGH;
		case ScriptStationList_Cargo::CS_BY_VIA:
			key = via;
			break;
	}
	if (key == this->last_key) {
		this->amount += amount;
	} else {
		this->SetValue();
		this->amount = amount;
		this->last_key = key;
	}
}


template<ScriptStationList_Cargo::CargoSelector Tselector>
void ScriptStationList_CargoWaiting::Add(StationID station_id, CargoID cargo, StationID other_station)
{
	CargoCollector collector(this, station_id, cargo, other_station);
	if (collector.GE() == nullptr) return;

	StationCargoList::ConstIterator iter = collector.GE()->cargo.Packets()->begin();
	StationCargoList::ConstIterator end = collector.GE()->cargo.Packets()->end();
	for (; iter != end; ++iter) {
		collector.Update<Tselector>((*iter)->GetFirstStation(), iter.GetKey(), (*iter)->Count());
	}
}


template<ScriptStationList_Cargo::CargoSelector Tselector>
void ScriptStationList_CargoPlanned::Add(StationID station_id, CargoID cargo, StationID other_station)
{
	CargoCollector collector(this, station_id, cargo, other_station);
	if (collector.GE() == nullptr) return;

	FlowStatMap::const_iterator iter = collector.GE()->flows.begin();
	FlowStatMap::const_iterator end = collector.GE()->flows.end();
	for (; iter != end; ++iter) {
		const FlowStat::SharesMap *shares = iter->second.GetShares();
		uint prev = 0;
		for (FlowStat::SharesMap::const_iterator flow_iter = shares->begin();
				flow_iter != shares->end(); ++flow_iter) {
			collector.Update<Tselector>(iter->first, flow_iter->second, flow_iter->first - prev);
			prev = flow_iter->first;
		}
	}
}

ScriptStationList_CargoWaitingByFrom::ScriptStationList_CargoWaitingByFrom(StationID station_id,
		CargoID cargo)
{
	this->Add<CS_BY_FROM>(station_id, cargo);
}

ScriptStationList_CargoWaitingViaByFrom::ScriptStationList_CargoWaitingViaByFrom(
		StationID station_id, CargoID cargo, StationID via)
{
	CargoCollector collector(this, station_id, cargo, via);
	if (collector.GE() == nullptr) return;

	std::pair<StationCargoList::ConstIterator, StationCargoList::ConstIterator> range =
			collector.GE()->cargo.Packets()->equal_range(via);
	for (StationCargoList::ConstIterator iter = range.first; iter != range.second; ++iter) {
		collector.Update<CS_VIA_BY_FROM>((*iter)->GetFirstStation(), iter.GetKey(), (*iter)->Count());
	}
}


ScriptStationList_CargoWaitingByVia::ScriptStationList_CargoWaitingByVia(StationID station_id,
		CargoID cargo)
{
	this->Add<CS_BY_VIA>(station_id, cargo);
}

ScriptStationList_CargoWaitingFromByVia::ScriptStationList_CargoWaitingFromByVia(
		StationID station_id, CargoID cargo, StationID from)
{
	this->Add<CS_FROM_BY_VIA>(station_id, cargo, from);
}

ScriptStationList_CargoPlannedByFrom::ScriptStationList_CargoPlannedByFrom(StationID station_id,
		CargoID cargo)
{
	this->Add<CS_BY_FROM>(station_id, cargo);
}

ScriptStationList_CargoPlannedViaByFrom::ScriptStationList_CargoPlannedViaByFrom(
		StationID station_id, CargoID cargo, StationID via)
{
	this->Add<CS_VIA_BY_FROM>(station_id, cargo, via);
}


ScriptStationList_CargoPlannedByVia::ScriptStationList_CargoPlannedByVia(StationID station_id,
		CargoID cargo)
{
	this->Add<CS_BY_VIA>(station_id, cargo);
}


ScriptStationList_CargoPlannedFromByVia::ScriptStationList_CargoPlannedFromByVia(
		StationID station_id, CargoID cargo, StationID from)
{
	CargoCollector collector(this, station_id, cargo, from);
	if (collector.GE() == nullptr) return;

	FlowStatMap::const_iterator iter = collector.GE()->flows.find(from);
	if (iter == collector.GE()->flows.end()) return;
	const FlowStat::SharesMap *shares = iter->second.GetShares();
	uint prev = 0;
	for (FlowStat::SharesMap::const_iterator flow_iter = shares->begin();
			flow_iter != shares->end(); ++flow_iter) {
		collector.Update<CS_FROM_BY_VIA>(iter->first, flow_iter->second, flow_iter->first - prev);
		prev = flow_iter->first;
	}
}
