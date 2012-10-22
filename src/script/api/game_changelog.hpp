/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file game_changelog.hpp Lists all changes / additions to the API.
 *
 * Only new / renamed / deleted api functions will be listed here. A list of
 * bug fixes can be found in the normal changelog. Note that removed API
 * functions may still be available if you return an older API version
 * in GetAPIVersion() in info.nut.
 *
 * \b 1.3.0
 *
 * 1.3.0 is not yet released. The following changes are not set in stone yet.
 *
 * API additions:
 * \li GSCargoMonitor
 * \li GSEngine::IsValidEngine and GSEngine::IsBuildable when outside GSCompanyMode scope
 * \li GSEventExclusiveTransportRights
 * \li GSEventRoadReconstruction
 * \li GSNews::NT_ACCIDENT, GSNews::NT_COMPANY_INFO, GSNews::NT_ADVICE, GSNews::NT_ACCEPTANCE
 * \li GSIndustryType::IsProcessingIndustry
 * \li GSStation::IsAirportClosed
 * \li GSStation::OpenCloseAirport
 * \li GSController::Break
 * \li GSIndustryType::BuildIndustry, GSIndustryType::CanBuildIndustry, GSIndustryType::ProspectIndustry and GSIndustryType::CanProspectIndustry when outside GSCompanyMode scope
 *
 * \b 1.2.2
 *
 * No changes
 *
 * \b 1.2.1
 *
 * No changes
 *
 * \b 1.2.0
 * \li First stable release with the NoGo framework.
 */
