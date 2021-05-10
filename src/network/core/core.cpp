/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file core.cpp Functions used to initialize/shut down the core network
 */

#include "../../stdafx.h"
#include "../../debug.h"
#include "os_abstraction.h"
#include "packet.h"

#include "../../safeguards.h"


/**
 * Initializes the network core (as that is needed for some platforms
 * @return true if the core has been initialized, false otherwise
 */
bool NetworkCoreInitialize()
{
/* Let's load the network in windows */
#ifdef _WIN32
	{
		WSADATA wsa;
		DEBUG(net, 3, "[core] loading windows socket library");
		if (WSAStartup(MAKEWORD(2, 0), &wsa) != 0) {
			DEBUG(net, 0, "[core] WSAStartup failed, network unavailable");
			return false;
		}
	}
#endif /* _WIN32 */

	return true;
}

/**
 * Shuts down the network core (as that is needed for some platforms
 */
void NetworkCoreShutdown()
{
#if defined(_WIN32)
	WSACleanup();
#endif
}
