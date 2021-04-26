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
#include "../../string_func.h"

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

#if defined(_WIN32)
/**
 * Return the string representation of the given error from the OS's network functions.
 * @param error The error number (from \c NetworkGetLastError()).
 * @return The error message, potentially an empty string but never \c nullptr.
 */
const char *NetworkGetErrorString(int error)
{
	static char buffer[512];
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, sizeof(buffer), NULL) == 0) {
		seprintf(buffer, lastof(buffer), "Unknown error %d", error);
	}
	return buffer;
}
#endif /* defined(_WIN32) */
