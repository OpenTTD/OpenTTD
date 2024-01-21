/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_integration.h Interface definitions for game to report/respond to social integration. */

#ifndef SOCIAL_INTEGRATION_H
#define SOCIAL_INTEGRATION_H

class SocialIntegration {
public:
	/**
	 * Initialize the social integration system, loading any social integration plugins that are available.
	 */
	static void Initialize();
};

#endif /* SOCIAL_INTEGRATION_H */
