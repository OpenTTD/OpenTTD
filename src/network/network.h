/* $Id$ */

#ifndef NETWORK_H
#define NETWORK_H

#include "../player_type.h"

#ifdef ENABLE_NETWORK

void NetworkStartUp();
void NetworkShutDown();

extern bool _networking;         ///< are we in networking mode?
extern bool _network_server;     ///< network-server is active
extern bool _network_available;  ///< is network mode available?
extern bool _network_dedicated;  ///< are we a dedicated server?
extern bool _network_advertise;  ///< is the server advertising to the master server?
extern bool _network_reload_cfg; ///< will we reload the entire config for the next game?

#else /* ENABLE_NETWORK */
/* Network function stubs when networking is disabled */

static inline void NetworkStartUp() {}
static inline void NetworkShutDown() {}

#define _networking 0
#define _network_server 0
#define _network_available 0
#define _network_dedicated 0
#define _network_advertise 0

#endif /* ENABLE_NETWORK */

/** As which player do we play? */
extern PlayerID _network_playas;

#endif /* NETWORK_H */
