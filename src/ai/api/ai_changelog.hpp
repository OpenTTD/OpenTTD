/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file ai_changelog.hpp Lists all changes / additions to the API.
 *
 * Only new / renamed / deleted api functions will be listed here. A list of
 * bug fixes can be found in the normal changelog. Note that removed API
 * functions may still be available if you return an older API version
 * in GetAPIVersion() in info.nut.
 *
 * \b 1.1.0
 *
 * 1.1.0 is not yet released. The following changes are not set in stone yet.
 *
 * API additions:
 * \li IsEnd for all lists.
 * \li AIEventTownFounded
 * \li AIIndustry::GetIndustryID
 * \li AIIndustryType::INDUSTRYTYPE_TOWN
 * \li AIIndustryType::INDUSTRYTYPE_UNKNOWN
 * \li AIOrder::IsVoidOrder
 * \li AIRail::GetName
 * \li AITown::IsCity
 *
 * API removals:
 * \li HasNext for all lists.
 * \li AIAbstractList, use AIList instead.
 * \li AIList::ChangeItem, use AIList::SetValue instead.
 *
 * Other changes:
 * \li AIEngine::GetMaxTractiveEffort can be used for road vehicles.
 * \li AIEngine::GetPower can be used for road vehicles.
 * \li AIEngine::GetWeight can be used for road vehicles.
 * \li AIIndustry::IsCargoAccepted now returns CargoAcceptState instead of a boolean.
 * \li AIOrder::GetOrderFlags returns AIOrder::AIOF_INVALID for void orders as well.
 * \li AIRoad::BuildDriveThroughRoadStation now allows overbuilding.
 * \li AIRoad::BuildRoadStation now allows overbuilding.
 *
 * \b 1.0.5
 *
 * No changes
 *
 * \b 1.0.4
 *
 * No changes
 *
 * \b 1.0.3
 *
 * API additions:
 * \li AIRail::ERR_RAILTYPE_DISALLOWS_CROSSING
 *
 * \b 1.0.2
 *
 * Other changes:
 * \li AIBridge::GetPrice now returns the price of the bridge without the cost for the rail or road.
 *
 * \b 1.0.1
 *
 * API additions:
 * \li AIRail::GetMaxSpeed
 *
 * \b 1.0.0
 *
 * API additions:
 * \li AIBaseStation
 * \li AIEngine::IsBuildable
 * \li AIEventCompanyAskMerger
 * \li AIIndustry::GetLastMonthTransportedPercentage
 * \li AIInfo::AICONFIG_INGAME
 * \li AIMarine::GetBuildCost
 * \li AIOrder::AIOF_GOTO_NEAREST_DEPOT
 * \li AIOrder::GetStopLocation
 * \li AIOrder::SetStopLocation
 * \li AIRail::RemoveRailStationTileRectangle
 * \li AIRail::RemoveRailWaypointTileRectangle
 * \li AIRail::GetBuildCost
 * \li AIRoad::GetBuildCost
 * \li AISubsidy::SubsidyParticipantType
 * \li AISubsidy::GetSourceType
 * \li AISubsidy::GetSourceIndex
 * \li AISubsidy::GetDestinationType
 * \li AISubsidy::GetDestinationIndex
 * \li AITile::GetBuildCost
 * \li AITown::GetLastMonthTransportedPercentage
 * \li AIVehicleList_Depot
 * \li AIWaypoint::WaypointType
 * \li AIWaypoint::HasWaypointType
 * \li Some error messages to AIWaypoint
 *
 * API removals:
 * \li AIOrder::ChangeOrder, use AIOrder::SetOrderFlags instead
 * \li AIRail::RemoveRailStationTileRect, use AIRail::RemoveRailStationTileRectangle instead
 * \li AIRail::RemoveRailWaypoint, use AIRail::RemoveRailWaypointTileRectangle instead
 * \li AISign::GetMaxSignID, use AISignList instead
 * \li AIStation::ERR_STATION_TOO_LARGE, use AIError::ERR_STATION_TOO_SPREAD_OUT instead
 * \li AISubsidy::SourceIsTown, use AISubsidy::GetSourceType instead
 * \li AISubsidy::GetSource, use AISubsidy::GetSourceIndex instead
 * \li AISubsidy::DestinationIsTown, use AISubsidy::GetDestinationType instead
 * \li AISubsidy::GetDestination, use AISubsidy::GetDestinationIndex instead
 * \li AITile::GetHeight, use AITile::GetMinHeight/GetMaxHeight/GetCornerHeight instead
 * \li AITown::GetMaxProduction, use AITown::GetLastMonthProduction instead
 * \li AIVehicle::SkipToVehicleOrder, use AIOrder::SkipToOrder instead
 * \li AIWaypoint::WAYPOINT_INVALID, use AIBaseStation::STATION_INVALID instead
 *
 * Other changes:
 * \li The GetName / SetName / GetLocation functions were moved from AIStation
 *     and AIWaypoint to AIBaseStation, but you can still use AIStation.GetName
 *     as before
 * \li The GetConstructionDate function was moved from AIStation to
 *     AIBaseStation, but can still be used as AIStation.GetConstructionDate
 * \li WaypointID was replaced by StationID. All WaypointIDs from previous
 *     savegames are invalid. Use STATION_INVALID instead of WAYPOINT_INVALID
 * \li AIWaypointList constructor now needs a WaypointType similiar to AIStationList,
 *     it can also handle buoys.
 * \li AIVehicleList_Station now also works for waypoints
 * \li Stations can be build over rail without signals that is in the right
 *     direction for the to-be built station. It will also convert the rail if
 *     the station's rail type supports the old type.
 * \li GetAPIVersion() was added as function to info.nut. If it does not exist
 *     API version 0.7 is assumed. This function should return the major and
 *     minor number of the stable version of the API the AI is written against.
 *     For 0.7.2 that would be 0.7, for 1.1.3 it would be 1.1, etc.
 * \li The subsidy logic has changed. Subsidy is now awarded when cargo
 *     originating from subsidy source is delivered to station that has subsidy
 *     destination it its catchment area. One industry tile or one town house
 *     is enough as long as station accepts the cargo. Awarded subsidies are no
 *     longer bound to stations used for first delivery, any station can be
 *     used for loading and unloading as long as cargo is transfered from
 *     source to destination.
 * \li Make AIEngine:CanRefitCargo() not report refittability to mail by
 *     default for aircraft. It is not necessarily true. This means that even
 *     if the aircraft can carry mail (as secondary cargo) it does not return
 *     true if the aircraft cannot carry it as its only cargo.
 * \li Improve behaviour of (AIEngine|AIEventEnginePreview)::GetCargoType()
 *     and AIEngine::CanRefitCargo() for articulated vehicles. For
 *     CanRefitCargo true is returned if at least one part can be refitted.
 *     For GetCargoType the first most used cargo type is returned.
 * \li AIIndustryType::GetConstructionCost() now returns -1 if the industry is
 *     neither buildable nor prospectable.
 * \li AIEngine::IsValidEngine will now return true if you have at least one
 *     vehicle of that type in your company, regardless if it's still buildable
 *     or not. AIEngine::IsBuildable returns only true when you can actually
 *     build an engine.
 * \li AITile::GetCargoProduction will now return the number of producers,
 *     including houses instead the number of producing tiles. This means that
 *     also industries that do not have a tile within the radius, but where
 *     the search bounding box and the industry's bounding box intersect, are
 *     counted. Previously these industries (and their cargos), although they
 *     produced cargo for a station at the given location, were not returned.
 * \li AIRail::BuildRail will now fail completely if there is an obstacle
 *     between the begin and end, instead of building up to the obstacle and
 *     returning that everything went okay.
 * \li Orders for buoys are now waypoint orders, i.e. instead of using the
 *     station orders for buoys one has to use waypoint orders.
 * \li Autoreplaces can now also be set for the default group via AIGroup.
 *
 * \b 0.7.5
 *
 * No changes
 *
 * \b 0.7.4
 *
 * No changes
 *
 * \b 0.7.3
 *
 * API additions:
 * \li AIAbstractList::SORT_ASCENDING
 * \li AIAbstractList::SORT_DESCENDING
 * \li AIAirport::IsAirportInformationAvailable
 * \li AICompany::GetPresidentGender
 * \li AICompany::SetPresidentGender
 * \li AIEngine::GetDesignDate
 * \li AIStation::GetConstructionDate
 *
 * Other changes:
 * \li AIs are now killed when they execute a DoCommand or Sleep at a time
 *     they are not allowed to do so.
 * \li When the API requests a string as parameter you can give every squirrel
 *     type and it will be converted to a string
 * \li AIs can create subclasses of API classes and use API constants as part
 *     of their own constants
 *
 * \b 0.7.2
 *
 * API additions:
 * \li AIVehicle::GetReliability
 *
 * Other changes:
 * \li DoCommands and sleeps in call, acall, pcall and valuators are disallowed
 *
 * \b 0.7.1
 *
 * API additions:
 * \li AIAirport::GetPrice
 * \li AIController::GetVersion
 * \li AIOrder::AIOF_DEPOT_FLAGS
 * \li AIOrder::AIOF_STOP_IN_DEPOT
 * \li AIOrder::IsCurrentOrderPartOfOrderList
 * \li AIOrder::IsGotoDepotOrder
 * \li AIOrder::IsGotoStationOrder
 * \li AIOrder::IsGotoWaypointOrder
 * \li AISignList
 * \li AITile::CORNER_[WSEN]
 * \li AITile::ERR_AREA_ALREADY_FLAT
 * \li AITile::ERR_EXCAVATION_WOULD_DAMAGE
 * \li AITile::GetCornerHeight
 * \li AITile::GetMaxHeight
 * \li AITile::GetMinHeight
 * \li AIVehicle::SendVehicleToDepotForServicing
 *
 * Other changes:
 * \li GetURL() was added as optional function to info.nut
 * \li UseAsRandomAI() was added as optional function to info.nut
 * \li A limit was introduced on the time the AI spends in the constructor and Load function
 *
 * \b 0.7.0
 * \li First stable release with the NoAI framework.
 */
