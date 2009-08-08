/* $Id$ */

/** @file ai_changelog.hpp Lists all changes / additions to the API.
 *
 * Only new / renamed / deleted api functions will be listed here. A list of
 * bug fixes can be found in the normal changelog.
 *
 * \b 0.9.0
 *
 * Scheduled API removals:
 * \li AIOrder::ChangeOrder
 * \li AISign::GetMaxSignID
 * \li AITile::GetHeight
 * \li AIBaseStation::WAYPOINT_INVALID
 *
 * \b 0.8.0
 *
 * API additions:
 * \li AIBaseStation
 * \li AIBuoyList
 * \li AIRail::RemoveRailWaypointTileRect
 *
 * Other changes:
 * \li The GetName / SetName / GetLocation functions were moved from AIStation
 *     and AIWaypoint to AIBaseStation, but you can still use AIStation.GetName
 *     as before
 * \li The GetConstructionDate function was moved from AIStation to
 *     AIBaseStation, but can still be used as AIStation.GetConstructionDate
 * \li WaypointID was replaced by StationID. All WaypointIDs from previous
 *     savegames are invalid
 * \li WAYPOINT_INVALID is now deprecated, use STATION_INVALID instead
 * \li AISubsidy::GetSource and AISubsidy::GetDestination return STATION_INVALID
 *     for awarded subsidies
 * \li AIs can create subclasses of API classes and use API constants as part
 *     of their own constants
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
