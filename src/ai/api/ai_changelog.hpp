/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_changelog.hpp Lists all changes / additions to the API.
 *
 * Only new / renamed / deleted api functions will be listed here. A list of
 * bug fixes can be found in the normal changelog. Note that removed API
 * functions may still be available if you return an older API version
 * in GetAPIVersion() in info.nut.
 *
 * \b 0.8.0
 *
 * API additions:
 * \li AIBaseStation
 * \li AIBuoyList
 * \li AIEventCompanyAskMerger
 * \li AIRail::RemoveRailWaypointTileRect
 * \li AISubsidy::SubsidyParticipantType
 * \li AISubsidy::GetSourceType
 * \li AISubsidy::GetSourceIndex
 * \li AISubsidy::GetDestinationType
 * \li AISubsidy::GetDestinationIndex
 * \li AITown::GetLastMonthTransportedPercentage
 * \li AIIndustry::GetLastMonthTransportedPercentage
 *
 * API removals:
 * \li AIOrder::ChangeOrder
 * \li AISign::GetMaxSignID
 * \li AITile::GetHeight
 * \li AIBaseStation::WAYPOINT_INVALID
 * \li AISubsidy::SourceIsTown
 * \li AISubsidy::GetSource
 * \li AISubsidy::DestinationIsTown
 * \li AISubsidy::GetDestination
 * \li AITown::GetMaxProduction
 * \li AIWaypoint::WAYPOINT_INVALID
 *
 * Other changes:
 * \li The GetName / SetName / GetLocation functions were moved from AIStation
 *     and AIWaypoint to AIBaseStation, but you can still use AIStation.GetName
 *     as before
 * \li The GetConstructionDate function was moved from AIStation to
 *     AIBaseStation, but can still be used as AIStation.GetConstructionDate
 * \li WaypointID was replaced by StationID. All WaypointIDs from previous
 *     savegames are invalid. Use STATION_INVALID instead of WAYPOINT_INVALID
 * \li AIVehicleList_Station now also works for waypoints
 *
 * \b 0.7.3
 *
 * API additions:
 * \li AIAbstractList::SORT_ASCENDING
 * \li AIAbstractList::SORT_DESCENDING
 * \li AICompany::GetPresidentGender
 * \li AICompany::SetPresidentGender
 * \li AIEngine::GetDesignDate
 * \li AIStation::GetConstructionDate
 *
 * Other changes:
 * \li AIs are now killed when they execute a DoCommand or Sleep at a time
 *     they are not allowed to do so.
 * \li When the API requests a string as parameter you can give ever squirrel
 *     type and it'll be converted to a string
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
