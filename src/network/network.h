/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network.h Basic functions/variables used all over the place. */

#ifndef NETWORK_H
#define NETWORK_H


#ifdef ENABLE_NETWORK

void NetworkStartUp();
void NetworkShutDown();
void NetworkDrawChatMessage();
bool HasClients();

extern bool _networking;         ///< are we in networking mode?
extern bool _network_server;     ///< network-server is active
extern bool _network_available;  ///< is network mode available?
extern bool _network_dedicated;  ///< are we a dedicated server?
extern bool _is_network_server;  ///< Does this client wants to be a network-server?

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkStartUp() {}
static inline void NetworkShutDown() {}
static inline void NetworkDrawChatMessage() {}
static inline bool HasClients() { return false; }

#define _networking 0
#define _network_server 0
#define _network_available 0
#define _network_dedicated 0
#define _is_network_server 0

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_H */
