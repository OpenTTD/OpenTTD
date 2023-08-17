/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file config.cpp Configuration of the connection strings for network stuff using environment variables.
 */

#include "../../stdafx.h"

#include "../../string_func.h"

#include "../../safeguards.h"

/**
 * Get the environment variable using std::getenv and when it is an empty string (or nullptr), return a fallback value instead.
 * @param variable The environment variable to read from.
 * @param fallback The fallback in case the environment variable is not set.
 * @return The environment value, or when that does not exist the given fallback value.
 */
static const char *GetEnv(const char *variable, const char *fallback)
{
	const char *value = std::getenv(variable);
	return StrEmpty(value) ? fallback : value;
}

/**
 * Get the connection string for the game coordinator from the environment variable OTTD_COORDINATOR_CS,
 * or when it has not been set a hard coded default DNS hostname of the production server.
 * @return The game coordinator's connection string.
 */
const char *NetworkCoordinatorConnectionString()
{
	return GetEnv("OTTD_COORDINATOR_CS", "coordinator.openttd.org");
}

/**
 * Get the connection string for the STUN server from the environment variable OTTD_STUN_CS,
 * or when it has not been set a hard coded default DNS hostname of the production server.
 * @return The STUN server's connection string.
 */
const char *NetworkStunConnectionString()
{
	return GetEnv("OTTD_STUN_CS", "stun.openttd.org");
}

/**
 * Get the connection string for the content server from the environment variable OTTD_CONTENT_SERVER_CS,
 * or when it has not been set a hard coded default DNS hostname of the production server.
 * @return The content server's connection string.
 */
const char *NetworkContentServerConnectionString()
{
	return GetEnv("OTTD_CONTENT_SERVER_CS", "content.openttd.org");
}

/**
 * Get the URI string for the content mirror from the environment variable OTTD_CONTENT_MIRROR_URI,
 * or when it has not been set a hard coded URI of the production server.
 * @return The content mirror's URI string.
 */
const char *NetworkContentMirrorUriString()
{
	return GetEnv("OTTD_CONTENT_MIRROR_URI", "https://binaries.openttd.org/bananas");
}

/**
 * Get the URI string for the survey from the environment variable OTTD_SURVEY_URI,
 * or when it has not been set a hard coded URI of the production server.
 * @return The survey's URI string.
 */
const char *NetworkSurveyUriString()
{
	return GetEnv("OTTD_SURVEY_URI", "https://survey-participate.openttd.org/");
}
