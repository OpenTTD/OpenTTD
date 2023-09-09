/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_station.cpp Implementation of ScriptStation. */

#include "../../stdafx.h"
#include "script_station.hpp"
#include "script_map.hpp"
#include "script_town.hpp"
#include "script_cargo.hpp"
#include "../../station_base.h"
#include "../../roadstop_base.h"
#include "../../town.h"
#include "../../station_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptStation::IsValidStation(StationID station_id)
{
	EnforceDeityOrCompanyModeValid(false);
	const Station *st = ::Station::GetIfValid(station_id);
	return st != nullptr && (st->owner == ScriptObject::GetCompany() || ScriptCompanyMode::IsDeity() || st->owner == OWNER_NONE);
}

/* static */ ScriptCompany::CompanyID ScriptStation::GetOwner(StationID station_id)
{
	if (!IsValidStation(station_id)) return ScriptCompany::COMPANY_INVALID;

	return static_cast<ScriptCompany::CompanyID>((int)::Station::Get(station_id)->owner);
}

/* static */ StationID ScriptStation::GetStationID(TileIndex tile)
{
	if (!::IsValidTile(tile) || !::IsTileType(tile, MP_STATION)) return INVALID_STATION;
	return ::GetStationIndex(tile);
}

template<bool Tfrom, bool Tvia>
/* static */ bool ScriptStation::IsCargoRequestValid(StationID station_id,
		StationID from_station_id, StationID via_station_id, CargoID cargo_id)
{
	if (!IsValidStation(station_id)) return false;
	if (Tfrom && !IsValidStation(from_station_id) && from_station_id != STATION_INVALID) return false;
	if (Tvia && !IsValidStation(via_station_id) && via_station_id != STATION_INVALID) return false;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return false;
	return true;
}

template<bool Tfrom, bool Tvia>
/* static */ SQInteger ScriptStation::CountCargoWaiting(StationID station_id,
		StationID from_station_id, StationID via_station_id, CargoID cargo_id)
{
	if (!ScriptStation::IsCargoRequestValid<Tfrom, Tvia>(station_id, from_station_id,
			via_station_id, cargo_id)) {
		return -1;
	}

	const StationCargoList &cargo_list = ::Station::Get(station_id)->goods[cargo_id].cargo;
	if (!Tfrom && !Tvia) return cargo_list.TotalCount();

	uint16_t cargo_count = 0;
	std::pair<StationCargoList::ConstIterator, StationCargoList::ConstIterator> range = Tvia ?
				cargo_list.Packets()->equal_range(via_station_id) :
				std::make_pair(StationCargoList::ConstIterator(cargo_list.Packets()->begin()),
						StationCargoList::ConstIterator(cargo_list.Packets()->end()));
	for (StationCargoList::ConstIterator it = range.first; it != range.second; it++) {
		const CargoPacket *cp = *it;
		if (!Tfrom || cp->GetFirstStation() == from_station_id) cargo_count += cp->Count();
	}

	return cargo_count;
}

/* static */ SQInteger ScriptStation::GetCargoWaiting(StationID station_id, CargoID cargo_id)
{
	return CountCargoWaiting<false, false>(station_id, STATION_INVALID, STATION_INVALID, cargo_id);
}

/* static */ SQInteger ScriptStation::GetCargoWaitingFrom(StationID station_id,
		StationID from_station_id, CargoID cargo_id)
{
	return CountCargoWaiting<true, false>(station_id, from_station_id, STATION_INVALID, cargo_id);
}

/* static */ SQInteger ScriptStation::GetCargoWaitingVia(StationID station_id,
		StationID via_station_id, CargoID cargo_id)
{
	return CountCargoWaiting<false, true>(station_id, STATION_INVALID, via_station_id, cargo_id);
}

/* static */ SQInteger ScriptStation::GetCargoWaitingFromVia(StationID station_id,
		StationID from_station_id, StationID via_station_id, CargoID cargo_id)
{
	return CountCargoWaiting<true, true>(station_id, from_station_id, via_station_id, cargo_id);
}

template<bool Tfrom, bool Tvia>
/* static */ SQInteger ScriptStation::CountCargoPlanned(StationID station_id,
		StationID from_station_id, StationID via_station_id, CargoID cargo_id)
{
	if (!ScriptStation::IsCargoRequestValid<Tfrom, Tvia>(station_id, from_station_id,
			via_station_id, cargo_id)) {
		return -1;
	}

	const FlowStatMap &flows = ::Station::Get(station_id)->goods[cargo_id].flows;
	if (Tfrom) {
		return Tvia ? flows.GetFlowFromVia(from_station_id, via_station_id) :
					  flows.GetFlowFrom(from_station_id);
	} else {
		return Tvia ? flows.GetFlowVia(via_station_id) : flows.GetFlow();
	}
}

/* static */ SQInteger ScriptStation::GetCargoPlanned(StationID station_id, CargoID cargo_id)
{
	return CountCargoPlanned<false, false>(station_id, STATION_INVALID, STATION_INVALID, cargo_id);
}

/* static */ SQInteger ScriptStation::GetCargoPlannedFrom(StationID station_id,
		StationID from_station_id, CargoID cargo_id)
{
	return CountCargoPlanned<true, false>(station_id, from_station_id, STATION_INVALID, cargo_id);
}

/* static */ SQInteger ScriptStation::GetCargoPlannedVia(StationID station_id,
		StationID via_station_id, CargoID cargo_id)
{
	return CountCargoPlanned<false, true>(station_id, STATION_INVALID, via_station_id, cargo_id);
}

/* static */ SQInteger ScriptStation::GetCargoPlannedFromVia(StationID station_id,
		StationID from_station_id, StationID via_station_id, CargoID cargo_id)
{
	return CountCargoPlanned<true, true>(station_id, from_station_id, via_station_id, cargo_id);
}

/* static */ bool ScriptStation::HasCargoRating(StationID station_id, CargoID cargo_id)
{
	if (!IsValidStation(station_id)) return false;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return false;

	return ::Station::Get(station_id)->goods[cargo_id].HasRating();
}

/* static */ SQInteger ScriptStation::GetCargoRating(StationID station_id, CargoID cargo_id)
{
	if (!ScriptStation::HasCargoRating(station_id, cargo_id)) return -1;

	return ::ToPercent8(::Station::Get(station_id)->goods[cargo_id].rating);
}

/* static */ SQInteger ScriptStation::GetCoverageRadius(ScriptStation::StationType station_type)
{
	if (station_type == STATION_AIRPORT) return -1;
	if (!HasExactlyOneBit(station_type)) return -1;

	if (!_settings_game.station.modified_catchment) return CA_UNMODIFIED;

	switch (station_type) {
		case STATION_TRAIN:      return CA_TRAIN;
		case STATION_TRUCK_STOP: return CA_TRUCK;
		case STATION_BUS_STOP:   return CA_BUS;
		case STATION_DOCK:       return CA_DOCK;
		default:                 return CA_NONE;
	}
}

/* static */ SQInteger ScriptStation::GetStationCoverageRadius(StationID station_id)
{
	if (!IsValidStation(station_id)) return -1;

	return Station::Get(station_id)->GetCatchmentRadius();
}

/* static */ SQInteger ScriptStation::GetDistanceManhattanToTile(StationID station_id, TileIndex tile)
{
	if (!IsValidStation(station_id)) return -1;

	return ScriptMap::DistanceManhattan(tile, GetLocation(station_id));
}

/* static */ SQInteger ScriptStation::GetDistanceSquareToTile(StationID station_id, TileIndex tile)
{
	if (!IsValidStation(station_id)) return -1;

	return ScriptMap::DistanceSquare(tile, GetLocation(station_id));
}

/* static */ bool ScriptStation::IsWithinTownInfluence(StationID station_id, TownID town_id)
{
	if (!IsValidStation(station_id)) return false;

	return ScriptTown::IsWithinTownInfluence(town_id, GetLocation(station_id));
}

/* static */ bool ScriptStation::HasStationType(StationID station_id, StationType station_type)
{
	if (!IsValidStation(station_id)) return false;
	if (!HasExactlyOneBit(station_type)) return false;

	return (::Station::Get(station_id)->facilities & station_type) != 0;
}

/* static */ bool ScriptStation::HasRoadType(StationID station_id, ScriptRoad::RoadType road_type)
{
	if (!IsValidStation(station_id)) return false;
	if (!ScriptRoad::IsRoadTypeAvailable(road_type)) return false;

	for (const RoadStop *rs = ::Station::Get(station_id)->GetPrimaryRoadStop(ROADSTOP_BUS); rs != nullptr; rs = rs->next) {
		if (HasBit(::GetPresentRoadTypes(rs->xy), (::RoadType)road_type)) return true;
	}
	for (const RoadStop *rs = ::Station::Get(station_id)->GetPrimaryRoadStop(ROADSTOP_TRUCK); rs != nullptr; rs = rs->next) {
		if (HasBit(::GetPresentRoadTypes(rs->xy), (::RoadType)road_type)) return true;
	}

	return false;
}

/* static */ TownID ScriptStation::GetNearestTown(StationID station_id)
{
	if (!IsValidStation(station_id)) return INVALID_TOWN;

	return ::Station::Get(station_id)->town->index;
}

/* static */ bool ScriptStation::IsAirportClosed(StationID station_id)
{
	EnforcePrecondition(false, IsValidStation(station_id));
	EnforcePrecondition(false, HasStationType(station_id, STATION_AIRPORT));

	return (::Station::Get(station_id)->airport.flags & AIRPORT_CLOSED_block) != 0;
}

/* static */ bool ScriptStation::OpenCloseAirport(StationID station_id)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsValidStation(station_id));
	EnforcePrecondition(false, HasStationType(station_id, STATION_AIRPORT));

	return ScriptObject::Command<CMD_OPEN_CLOSE_AIRPORT>::Do(station_id);
}
