/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file config.cpp Configuration of the connection strings for network stuff using environment variables.
 */

#include "../../stdafx.h"

#include "../../string_func.h"

#include "../../safeguards.h"

/**
 * Get the connection string for the game coordinator from the environment variable OTTD_COORDINATOR_CS,
 * or when it has not been set a hard coded default DNS hostname of the production server.
 * @return The game coordinator's connection string.
 */
std::string_view NetworkCoordinatorConnectionString()
{
	return GetEnv("OTTD_COORDINATOR_CS").value_or("coordinator.openttd.org");
}

/**
 * Get the connection string for the STUN server from the environment variable OTTD_STUN_CS,
 * or when it has not been set a hard coded default DNS hostname of the production server.
 * @return The STUN server's connection string.
 */
std::string_view NetworkStunConnectionString()
{
	return GetEnv("OTTD_STUN_CS").value_or("stun.openttd.org");
}

/**
 * Get the connection string for the content server from the environment variable OTTD_CONTENT_SERVER_CS,
 * or when it has not been set a hard coded default DNS hostname of the production server.
 * @return The content server's connection string.
 */
std::string_view NetworkContentServerConnectionString()
{
	return GetEnv("OTTD_CONTENT_SERVER_CS").value_or("content.openttd.org");
}

/**
 * Get the URI string for the content mirror from the environment variable OTTD_CONTENT_MIRROR_URI,
 * or when it has not been set a hard coded URI of the production server.
 * @return The content mirror's URI string.
 */
std::string_view NetworkContentMirrorUriString()
{
	return GetEnv("OTTD_CONTENT_MIRROR_URI").value_or("https://binaries.openttd.org/bananas");
}

/**
 * Get the URI string for the survey from the environment variable OTTD_SURVEY_URI,
 * or when it has not been set a hard coded URI of the production server.
 * @return The survey's URI string.
 */
std::string_view NetworkSurveyUriString()
{
	return GetEnv("OTTD_SURVEY_URI").value_or("https://survey-participate.openttd.org/");
}
