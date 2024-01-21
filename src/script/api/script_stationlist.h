/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_stationlist.h List all the stations (you own). */

#ifndef SCRIPT_STATIONLIST_HPP
#define SCRIPT_STATIONLIST_HPP

#include "script_list.h"
#include "script_station.h"

/**
 * Creates a list of stations of which you are the owner.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList : public ScriptList {
public:
	/**
	 * @param station_type The type of station to make a list of stations for.
	 */
	ScriptStationList(ScriptStation::StationType station_type);
};

/**
 * Creates a list of stations associated with cargo at a station. This is very generic. Use the
 * subclasses for all practical purposes.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_Cargo : public ScriptList {
public:
	/**
	 * Criteria of selecting and grouping cargo at a station.
	 */
	enum CargoSelector {
		CS_BY_FROM,     ///< Group by origin station.
		CS_VIA_BY_FROM, ///< Select by next hop and group by origin station.
		CS_BY_VIA,      ///< Group by next hop.
		CS_FROM_BY_VIA  ///< Select by origin station and group by next hop.
	};

	/**
	 * Ways of associating cargo to stations.
	 */
	enum CargoMode {
		CM_WAITING,     ///< Waiting cargo.
		CM_PLANNED      ///< Planned cargo.
	};

	/**
	 * Creates a list of stations associated with cargo in the specified way, selected and grouped
	 * by the chosen criteria.
	 * @param mode Mode of association, either waiting cargo or planned cargo.
	 * @param selector Mode of grouping and selecting to be applied.
	 * @param station_id Station to be queried.
	 * @param cargo Cargo type to query for.
	 * @param other_station Other station to restrict the query with.
	 */
	ScriptStationList_Cargo(ScriptStationList_Cargo::CargoMode mode, ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoID cargo, StationID other_station);

protected:

	/**
	 * Creates an empty list.
	 */
	ScriptStationList_Cargo() {}
};

/**
 * Creates a list of stations associated with cargo waiting at a station. This is very generic. Use
 * the subclasses for all practical purposes.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoWaiting : public ScriptStationList_Cargo {
protected:
	friend class ScriptStationList_Cargo;

	/**
	 * Creates an empty list.
	 */
	ScriptStationList_CargoWaiting() {}

	/**
	 * Add waiting cargo to the list.
	 * @param station_id Station to query for waiting cargo.
	 * @param cargo Cargo type to query for.
	 * @param other_station Other station to restrict the query with.
	 */
	template<CargoSelector Tselector>
	void Add(StationID station_id, CargoID cargo, StationID other_station = INVALID_STATION);

public:

	/**
	 * Creates a list of stations associated with waiting cargo, selected and grouped by the chosen
	 * criteria.
	 * @param selector Mode of grouping and selecting to be applied.
	 * @param station_id Station to be queried.
	 * @param cargo Cargo type to query for.
	 * @param other_station Other station to restrict the query with.
	 */
	ScriptStationList_CargoWaiting(ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoID cargo, StationID other_station);
};

/**
 * Creates a list of stations associated with cargo planned to pass a station. This is very
 * generic. Use the subclasses for all practical purposes.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoPlanned : public ScriptStationList_Cargo {
protected:
	friend class ScriptStationList_Cargo;

	/**
	 * Creates an empty list.
	 */
	ScriptStationList_CargoPlanned() {}

	/**
	 * Add planned cargo to the list.
	 * @param station_id Station to query for waiting cargo.
	 * @param cargo Cargo type to query for.
	 * @param other_station Other station to restrict the query with.
	 */
	template<CargoSelector Tselector>
	void Add(StationID station_id, CargoID cargo, StationID other_station = INVALID_STATION);

public:

	/**
	 * Creates a list of stations associated with cargo planned to pass the station, selected and
	 * grouped by the chosen criteria.
	 * @param selector Mode of grouping and selecting to be applied.
	 * @param station_id Station to be queried.
	 * @param cargo Cargo type to query for.
	 * @param other_station Other station to restrict the query with.
	 */
	ScriptStationList_CargoPlanned(ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoID cargo, StationID other_station);
};

/**
 * Creates a list of origin stations of waiting cargo at a station, with the amounts of cargo
 * waiting from each of those origin stations as values.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoWaitingByFrom : public ScriptStationList_CargoWaiting {
public:
	/**
	 * @param station_id Station to query for waiting cargo.
	 * @param cargo Cargo type to query for.
	 */
	ScriptStationList_CargoWaitingByFrom(StationID station_id, CargoID cargo);
};

/**
 * Creates a list of origin stations of cargo waiting at a station for a transfer via another
 * station, with the amounts of cargo waiting from each of those origin stations as values.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoWaitingViaByFrom : public ScriptStationList_CargoWaiting {
public:
	/**
	 * @param station_id Station to query for waiting cargo.
	 * @param cargo Cargo type to query for.
	 * @param via Next hop to restrict the query with.
	 */
	ScriptStationList_CargoWaitingViaByFrom(StationID station_id, CargoID cargo, StationID via);
};

/**
 * Creates a list of next hops of waiting cargo at a station, with the amounts of cargo waiting for
 * each of those next hops as values.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoWaitingByVia : public ScriptStationList_CargoWaiting {
public:
	/**
	 * @param station_id Station to query for waiting cargo.
	 * @param cargo Cargo type to query for.
	 */
	ScriptStationList_CargoWaitingByVia(StationID station_id, CargoID cargo);
};

/**
 * Creates a list of next hops of waiting cargo from a specific station at another station, with
 * the amounts of cargo waiting for each of those next hops as values.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoWaitingFromByVia : public ScriptStationList_CargoWaiting {
public:
	/**
	 * @param station_id Station to query for waiting cargo.
	 * @param cargo Cargo type to query for.
	 * @param from Origin station to restrict the query with.
	 */
	ScriptStationList_CargoWaitingFromByVia(StationID station_id, CargoID cargo, StationID from);
};

/**
 * Creates a list of origin stations of cargo planned to pass a station, with the monthly amounts
 * of cargo planned for each of those origin stations as values.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoPlannedByFrom : public ScriptStationList_CargoPlanned {
public:
	/**
	 * @param station_id Station to query for planned flows.
	 * @param cargo Cargo type to query for.
	 */
	ScriptStationList_CargoPlannedByFrom(StationID station_id, CargoID cargo);
};

/**
 * Creates a list of origin stations of cargo planned to pass a station going via another station,
 * with the monthly amounts of cargo planned for each of those origin stations as values.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoPlannedViaByFrom : public ScriptStationList_CargoPlanned {
public:
	/**
	 * @param station_id Station to query for planned flows.
	 * @param cargo Cargo type to query for.
	 * @param via Next hop to restrict the query with.
	 */
	ScriptStationList_CargoPlannedViaByFrom(StationID station_id, CargoID cargo, StationID via);
};

/**
 * Creates a list of next hops of cargo planned to pass a station, with the monthly amounts of
 * cargo planned for each of those next hops as values.
 * Cargo planned to go "via" the station being queried will actually be delivered there.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoPlannedByVia : public ScriptStationList_CargoPlanned {
public:
	/**
	 * @param station_id Station to query for planned flows.
	 * @param cargo Cargo type to query for.
	 */
	ScriptStationList_CargoPlannedByVia(StationID station_id, CargoID cargo);
};

/**
 * Creates a list of next hops of cargo planned to pass a station and originating from another
 * station, with the monthly amounts of cargo planned for each of those next hops as values.
 * Cargo planned to go "via" the station being queried will actually be delivered there.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_CargoPlannedFromByVia : public ScriptStationList_CargoPlanned {
public:
	/**
	 * @param station_id Station to query for planned flows.
	 * @param cargo Cargo type to query for.
	 * @param from Origin station to restrict the query with.
	 */
	ScriptStationList_CargoPlannedFromByVia(StationID station_id, CargoID cargo, StationID from);
};

/**
 * Creates a list of stations which the vehicle has in its orders.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_Vehicle : public ScriptList {
public:
	/**
	 * @param vehicle_id The vehicle to get the list of stations it has in its orders from.
	 */
	ScriptStationList_Vehicle(VehicleID vehicle_id);
};

#endif /* SCRIPT_STATIONLIST_HPP */
